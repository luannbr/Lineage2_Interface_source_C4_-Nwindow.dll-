"""Differential test of the GM snoop code (stock against ours): NCGMSnoopWnd::AddLine (C4 0x100E0790), NCGMSnoopManager::AddLine
(0x100E0960) and NCGMSnoopManager::OpenSnoop (0x100E09C0). The manager and its four windows are models (the manager's windows at
+0xE8, in a window: the list at +0xEC, the seven check boxes at +0xF4.. with their flag at +0x294, the snoop id at +0x110); the
virtual calls (AddString slot 76, ShowWindow slot 48, SetTitle slot 57) and the dialog functions are stubs that log what they get.
Random snoop ids, filters, chat types and texts are run by both. (C2 addresses were 0x100B7260 / 0x100B74F0 / 0x100B7550.)
usage: python tools/port/emu/t_snoop.py <cases> <seed>   (needs build/bin/Release/NWindowDecomp.dll and its .map, or NWD_SA=1 for the standalone)"""
import sys, random, os, struct
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import emu as _emu
import stubs
from stubs import *
import sa

stubs.FN_POP.update({'WAddString': 0xC, 'WShow': 0, 'WSetTitle': 4, 'DlgCtor': 0, 'DlgSetMsg': 4, 'DlgShow': 4, 'DlgDtor': 0})
# the stock functions stubbed and the same ones in ours (a substring of the decorated name)
stubs.STOCK_FN.clear()
stubs.OURS_FN.clear()
TipEmu.CONSOLE_VA = 0x102C39F4
stubs.STOCK_FN.update({0x100085C0: 'DlgCtor', 0x100079B0: 'DlgSetMsg', 0x10072E20: 'DlgShow', 0x10007D50: 'DlgDtor'})
stubs.OURS_FN.update({'DlgCtor': ('NDialogInfo@@', '??0NDialogInfo@@QAE@XZ'), 'DlgSetMsg': ('@NDialogInfo@@', '?SetSystemMessage@NDialogInfo@@'),
                      'DlgShow': ('@NConsoleWnd@@', '?ShowDialog@NConsoleWnd@@'), 'DlgDtor': ('NDialogInfo@@', '??1NDialogInfo@@')})


def sym(e, pre):
    c = [v for s, v in e.symbols.items() if s.startswith(pre)]
    assert len(c) >= 1, pre
    return c[0]


class SnoopEmu(TipEmu):
    def __init__(self, kind):
        super().__init__(kind)
        # C4: FL2GameData's SysString table is at +0x9A798 (TipEmu's block is smaller): a block of its own
        self.gdata = self.alloc(0x100000)
        for e in self.pe.DIRECTORY_ENTRY_IMPORT:
            for imp in e.imports:
                if imp.name and b'GL2GameData' in imp.name:
                    self.write32(imp.address, self.gdata)
        self.wvt = self.alloc(0x200)
        self.lvt = self.alloc(0x200)
        for vt, slot, name, off in ((self.wvt, 48, 'WShow', 0xC000), (self.wvt, 57, 'WSetTitle', 0xC010),
                                    (self.lvt, 76, 'WAddString', 0xC020)):
            addr = STUB_BASE + off
            self.uc.mem_write(addr, bytes([0xC3]))
            self.stubs[addr] = ('fn:' + name, None)
            self.write32(vt + 4 * slot, addr)

    def do_fn(self, nm):
        t = self.this()
        esp = self.reg(UC_X86_REG_ESP)
        a = lambda i: self.read32(esp + 4 + 4 * i)
        if nm == 'WAddString':
            self.log.append(('AddString', t, self.wstr(a(0)), a(1), self.rds32(esp + 12)))
            return None
        if nm == 'WShow':
            self.log.append(('ShowWindow', t))
            return None
        if nm == 'WSetTitle':
            self.log.append(('SetTitle', t, self.wstr(a(0))))
            return None
        if nm == 'DlgCtor':
            self.uc.mem_write(t, bytes(0xA0))
            self.write32(t, 0xFFFFFFFF)
            self.log.append(('DlgCtor',))
            return t
        if nm == 'DlgSetMsg':
            self.log.append(('DlgSetMsg', a(0)))
            return None
        if nm == 'DlgShow':
            self.log.append(('DlgShow', [self.rds32(a(0) + o) for o in range(0, 0x20, 4)]))
            return None
        if nm == 'DlgDtor':
            return None
        return super().do_fn(nm)


def make_manager(e, c):
    mgr = e.alloc(0xF8)
    e.uc.mem_write(mgr, bytes(0xF8))
    wins = []
    for i in range(4):
        w = e.alloc(0x118)
        e.uc.mem_write(w, bytes(0x118))
        e.write32(w, e.wvt)
        lst = e.alloc(0x20)
        e.write32(lst, e.lvt)
        e.write32(w + 0xEC, lst)
        for k in range(7):
            cb = e.alloc(0x2C4)
            e.uc.mem_write(cb, bytes(0x2C4))
            e.write32(cb + 0x294, c['checks'][i][k])
            e.write32(w + 0xF4 + 4 * k, cb)
        e.write32(w + 0x110, c['ids'][i] & 0xFFFFFFFF)
        e.write32(mgr + 0xE8 + 4 * i, w)
        wins.append(w)
    return mgr, wins


def rand_case(r):
    return {
        'ids': [r.choice([-1, -1, 7, 8, 9, 10, 11]) for _ in range(4)],
        'checks': [[r.choice([0, 1, 1, 2]) for _ in range(7)] for _ in range(4)],
        'id': r.choice([7, 8, 9, 10, 11, 12, 13, -1]),
        'type': r.choice([0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, -1]),
        'color': r.choice([0xFFFFFFFF, 0xFFFF7200, 0xFF00FF00]),
        'text': r.choice(['hello', 'a: b', '']),
        'name': r.choice(['Zubei', 'Somebody Long Name']),
        'extra': r.choice([0, 0, 5]),
    }


def run(kind, which, cases):
    e = SnoopEmu(kind)
    if kind == 'stock':
        fn = {'wline': 0x100E0790, 'mline': 0x100E0960, 'open': 0x100E09C0}[which]
    else:
        fn = sym(e, {'wline': '?AddLine@NCGMSnoopWnd@@', 'mline': '?AddLine@NCGMSnoopManager@@', 'open': '?OpenSnoop@NCGMSnoopManager@@'}[which])
    e.heap_mark = e.heap_ptr
    outs = []
    for c in cases:
        e.heap_ptr = e.heap_mark
        e.log.clear()
        e.fs_set(e.gdata + 0x9A798, 'Snoop title')          # SysString[693]
        mgr, wins = make_manager(e, c)
        text = e.put_wstr(c['text'])
        name = e.put_wstr(c['name'])
        buf = e.alloc(0x200)
        if which == 'wline':
            e.call(fn, this=wins[0], args=[text, c['color'], c['type'] & 0xFFFFFFFF, c['extra']])
        elif which == 'mline':
            e.call(fn, this=mgr, args=[c['id'] & 0xFFFFFFFF, text, c['color'], c['type'] & 0xFFFFFFFF, c['extra']])
        else:
            e.call(fn, this=mgr, args=[c['id'] & 0xFFFFFFFF, name])
        ids = [e.rds32(w + 0x110) for w in wins]
        outs.append((list(e.log), ids))
    return outs


def main(n, seed):
    r = random.Random(seed)
    cases = [rand_case(r) for _ in range(n)]
    tot = 0
    for which in ('wline', 'mline', 'open'):
        a = run('stock', which, cases)
        b = run('ours', which, cases)
        bad = sum(1 for x, y in zip(a, b) if x != y)
        for i, (x, y) in enumerate(zip(a, b)):
            if x != y:
                print(which, 'case', i, cases[i], '\n stock', x, '\n ours ', y)
                break
        print('t_snoop %s (%d with a log) cases %d  differ %d  raised 0' % (which, sum(1 for x in a if x[0]), n, bad))
        tot += bad
    return 1 if tot else 0


if __name__ == '__main__':
    sys.exit(main(int(sys.argv[1]), int(sys.argv[2])))
