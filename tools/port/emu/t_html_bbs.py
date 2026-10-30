# Differential test of the HTML page tree and the board viewer of the C2 NWindow.dll (written by the w2html lane): the stock function and ours run on identical object graphs under unicorn (tools/port/emu); the graphs, the globals and the device call logs are compared. Needs build/bin/Release/NWindowDecomp.dll (+ .map) and `pip install unicorn`.
# python tools/port/emu/t_html_bbs.py <n> <seed> {pieces|post|form|getvalue|action|button}
import os, sys, random, re
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from html_emu import *

stubs.IMPORT_POP['WideCharToMultiByte'] = 0x20
for k, v in (('LoadHtml', 4), ('ShowSysMsg', 8), ('HasInvalid', 4), ('PlaySound', 4), ('SetMultiText', 4), ('GetValue', 0),
             ('RunCmd', 8), ('HideWindow', 0), ('OwnerProc', 12)):
    stubs.FN_POP[k] = v
# C4 addresses (C2: 0x10036130, 0x1006A8C0, 0x100563D0, 0x10022230, 0x10061D20): NCHtmlViewer::LoadHtml, NConsoleWnd::ShowSystemMessage / HasInvalidChars /
# PlayInterfaceSound, NCMultiEdit::SetText
stubs.STOCK_FN.update({0x100419D0: 'LoadHtml', 0x1007D380: 'ShowSysMsg', 0x1006A570: 'HasInvalid', 0x10028450: 'SetMultiText', 0x10074E60: 'PlaySound'})
stubs.OURS_FN.update({'LoadHtml': ('@NCHtmlViewer@@', '?LoadHtml@'), 'ShowSysMsg': ('@NConsoleWnd@@', '?ShowSystemMessage@'),
                      'HasInvalid': ('@NConsoleWnd@@', '?HasInvalidChars@'), 'SetMultiText': ('@NCMultiEdit@@', '?SetText@'),
                      'PlaySound': ('@NConsoleWnd@@', '?PlayInterfaceSound@')})

STOCK_B = {'addpiece': 0x100421D0, 'postpage': 0x10041B30, 'formpage': 0x10041DC0, 'parts': 0x10042140,
           'getvalue': 0x1003C5C0, 'action': 0x1003B4A0, 'button': 0x1003B550, 'showpage': 0x10041B20,
           'wproc': 0x1003B390}
OURS_B = {'addpiece': '?AddPiece@NCBBSHtmlViewer@@', 'postpage': '?BuildPostPage@NCBBSHtmlViewer@@',
          'formpage': '?BuildFormPage@NCBBSHtmlViewer@@', 'parts': '?ShowPageParts@NCBBSHtmlViewer@@',
          'getvalue': '?GetControlValue@NCBBSHtmlViewer@@', 'action': '?OnActionLink@NCBBSHtmlViewer@@',
          'button': '?OnButton@NCBBSHtmlViewer@@', 'showpage': '?ShowPage@NCBBSHtmlViewer@@'}


class BEmu(HEmu):
    def __init__(self, kind):
        super().__init__(kind)
        self.cmd_results = []
        # the viewer's fake vtable (slot 49 HideWindow; C2: 44) and an owner window's (slot 25 WindowProc)
        self.vvt = self.newobj(0x400)
        self.write32(self.vvt + 49 * 4, self.mkstub('HideWindow', 0))
        self.ovt = self.newobj(0x400)
        self.write32(self.ovt + 25 * 4, self.mkstub('OwnerProc', 1))
        self.cvt = self.newobj(0x400)
        self.write32(self.cvt + 74 * 4, self.mkstub('GetValue', 2))        # NControl::GetValue is vslot 74 (C2: 67)
        # the console's RunHtmlCommand (slot 438; C2: 332)
        self.convt = self.newobj(0x800)
        self.write32(self.convt + 438 * 4, self.mkstub('RunCmd', 3))
        self.write32(self.console, self.convt)
        if kind == 'ours' and not sa.ENABLED:
            # GBoardMarks (20 pointers at 0x10235340; C2: 19 at 0x10160358) and their strings, from the stock image: the hook build reads them there
            # (nw::Addr); the standalone has its own table. (PlayInterfaceSound is patched through OURS_FN now)
            from emu import Emu
            st = Emu('stock')
            for i in range(20):
                ptr = st.read32(0x10235340 + 4 * i)
                self.write32(NWBASE + 0x235340 + 4 * i, self.put_wstr(st.wstr(ptr) if ptr else ''))
            del st
        else:
            pass
        self.stock_marks = True

    def mkstub(self, nm, i):
        a = STUB_BASE + 0x9400 + 16 * (len(self.stubs) % 200) + 0
        while a in self.stubs:
            a += 16
        self.uc.mem_write(a, b'\xC3')
        self.stubs[a] = ('fn:' + nm, None)
        return a

    def do_fn(self, nm):
        t = self.this()
        a = self.arg
        if nm == 'LoadHtml':
            self.log.append(('LoadHtml', self.wstr(a(0))))
            return 1
        if nm == 'ShowSysMsg':
            self.log.append(('SysMsg', a(0), a(1)))
            return None
        if nm == 'HasInvalid':
            s = self.wstr(a(0)) or ''
            r = 1 if any(c in s for c in '#$') else 0
            self.log.append(('HasInvalid', s[:20], r))
            return r
        if nm == 'SetMultiText':
            self.log.append(('SetMultiText', self.wstr(a(0))[:20]))
            return None
        if nm == 'GetValue':
            return self.read32(t + 0x300)
        if nm == 'RunCmd':
            self.log.append(('RunCmd', self.wstr(a(0))))
            return self.cmd_results.pop(0) if self.cmd_results else 0
        if nm == 'HideWindow':
            self.log.append(('HideWindow',))
            return None
        if nm == 'OwnerProc':
            self.log.append(('OwnerProc', a(0), a(1), a(2)))
            return 1
        if nm == 'PlaySound':
            self.log.append(('PlaySound', a(0)))
            return None
        return super().do_fn(nm)

    def do_import(self, nm):
        if nm == 'WideCharToMultiByte':
            s = self.wstr(self.stack_arg(2)) or ''
            out = ''.join(c if ord(c) < 128 else '?' for c in s).encode('ascii')
            dst, n = self.stack_arg(4), self.stack_arg(5)
            self.uc.mem_write(dst, out[:n - 1] + b'\0')
            return len(out) + 1
        return super().do_import(nm)

    def fnb(self, name):
        if self.kind == 'stock':
            return STOCK_B[name]
        c = [v for k, v in self.symbols.items() if k.startswith(OURS_B[name])]
        assert len(c) == 1, (name, c)
        return c[0]


stubs_orig = None
BS = 0x24190        # C4 (C2: 0x24134)


def masked(b):
    b = bytearray(b)
    b[0:4] = bytes(4)
    b[0x144:0x150] = bytes(12)          # Controls (C2: +0xF4)
    b[0x164:0x168] = bytes(4)           # Owner (C2: +0x114)
    return bytes(b)


def new_viewer(e, controls, owner):
    v = e.newobj(BS)
    e.write32(v, e.vvt)
    e.write_tarray(v + 0x144, controls)
    e.write32(v + 0x164, owner)
    return v


def mk_control(e, caption, value, multi):
    c = e.newobj(0x400)
    e.write32(c, e.cvt)
    e.uc.mem_write(c + 0xF4, caption.encode('utf-16-le') + b'\0\0')
    e.write32(c + 0x300, e.put_wstr(value) if value is not None else 0)
    e.write32(c + 0x24, e.gptr(0x102CC318) if multi else e.gptr(0x102CCD78))
    return c


def rand_piece(r):
    k = r.choice(['<', '1', '2', '3', '4', '1001', '1002', '101', '102', '103', 'x', ''])
    if k == '<':
        return '<html>' + r.choice(['hi', '', 'A' * 30])
    body = '\x08'.join(r.choice(['f%d' % i, ' ', 'Title text', 'T' * 20, '<!--X-->', '']) for i in range(r.randint(0, 6)))
    return k + '\x08' + body if r.random() < 0.9 else k


def run(e, case):
    e.heap_ptr = e.heap_mark
    e.log.clear()
    out = {}
    try:
        kind = case[0]
        if kind == 'pieces':
            v = new_viewer(e, [], 0)
            seq = []
            for text in case[1]:
                p = e.put_wstr(text)
                e.call(e.fnb('addpiece'), this=v, args=[p])
                seq.append(bytes(e.uc.mem_read(v + 0x178, 0x20)))
            out['seq'] = seq
            out['mem'] = masked(e.uc.mem_read(v, BS))
        elif kind == 'form':
            # BuildFormPage reads its three 16 KB locals (writer, title, content) uninitialised when no field of the list names them
            # (src/nwindow/NCBoardPage.cpp: "tests stack garbage; empty here"): the stack below the call is zeroed, so that the stock's
            # garbage is a defined "empty" on both sides (the stack history of the two emulators differs)
            e.uc.mem_write(STACK_TOP - 0x40000, bytes(0x3F000))
            ctrls = []
            for cs, val, multi in case[2]:
                ctrls.append(mk_control(e, cs, val, multi))
            v = new_viewer(e, ctrls, 0)
            e.uc.mem_write(v + 0x10190, case[1][0].encode('utf-16-le') + b'\0\0')
            e.uc.mem_write(v + 0x14190, case[1][1].encode('utf-16-le') + b'\0\0')
            e.write32(v + 0x184, 2)
            e.call(e.fnb('formpage'), this=v)
            out['mem'] = masked(e.uc.mem_read(v, 0x1A000))
            out['fs'] = [e.fs_get(c + 0x258) for c in ctrls]
        elif kind == 'post':
            v = new_viewer(e, [], 0)
            for off, s in zip((0x190, 0x4190, 0x8190, 0xC190), case[1]):
                e.uc.mem_write(v + off, s.encode('utf-16-le') + b'\0\0')
            e.write32(v + 0x180, 4)
            e.call(e.fnb('postpage'), this=v)
            out['mem'] = masked(e.uc.mem_read(v, 0x10200))
        elif kind == 'getvalue':
            ctrls = [mk_control(e, cs, val, multi) for cs, val, multi in case[1]]
            v = new_viewer(e, ctrls, 0)
            name = e.put_wstr(case[2]) if case[2] is not None else 0
            out['ret'] = e.call(e.fnb('getvalue'), this=v, args=[name])
            out['ret'] = e.wstr(out['ret'])[:12] if out['ret'] else 0
        elif kind in ('action', 'button'):
            e.cmd_results = list(case[3])
            owner = 0
            if case[2]:
                ow = e.newobj(0x40)
                e.write32(ow, e.ovt)
                owner = e.newobj(0x10)
                e.write32(owner + 4, ow)
            v = new_viewer(e, [], owner)
            e.write32(v + 0x17C, case[4])
            e.write32(v + 0x180, 5); e.write32(v + 0x184, 6); e.write32(v + 0x188, 7)
            p = e.put_wstr(case[1])
            out['ret'] = e.call(e.fnb(kind), this=v, args=[p])
            out['mem'] = [e.read32(v + o) for o in (0x17C, 0x180, 0x184, 0x188)]
            out['tgt'] = e.wstr(e.G(0x102C8098), 256)
        elif kind == 'showpage':
            v = new_viewer(e, [], 0)
            e.write32(v + 0x17C, 9)
            out['ret'] = e.call(e.fnb('showpage'), this=v, args=[e.put_wstr('page')])
            out['lock'] = e.read32(v + 0x17C)
        out['log'] = [x for x in e.log if x[0] != 'Malloc' and x[0] != 'Free']
    except Exception as ex:
        out['exc'] = re.sub(r'eip=[0-9a-f]+', 'eip=X', str(ex))[:100]
    return out


def gen(r, kind):
    if kind == 'pieces':
        return ('pieces', [rand_piece(r) for _ in range(r.randint(1, 9))])
    if kind == 'form':
        fields = '\x08'.join(r.choice(['t1', ' ', 'MyTitle', '', 'Body text', 'x']) for _ in range(r.randint(0, 19)))
        tmpl = ''.join(r.choice(['<html>', '<!--ROOT_BID-->', '<!--TITLE-->', '<!--CONTENT-->', '<b>', '<!--IP-->', 'xx']) for _ in range(r.randint(1, 8)))
        ctrls = [(r.choice(['', 'a']), None, r.random() < 0.5) for _ in range(r.randint(0, 3))]
        return ('form', (tmpl, '\x08' + fields if r.random() < 0.7 else fields), ctrls)
    if kind == 'post':
        tmpl = ''.join(r.choice(['<html>', '<!--ROOT_BID-->', '<!--COMMENT-->', '<!--THREAD-->', '<!--IP-->', 'xx']) for _ in range(r.randint(1, 8)))
        fields = '\x08'.join(r.choice(['t1', ' ', 'MyTitle', '', 'x']) for _ in range(r.randint(0, 19)))
        return ('post', (tmpl, fields, r.choice(['cmt', '']), r.choice(['thr', ''])))
    if kind == 'getvalue':
        vals = [r.choice([None, '', 'a', 'ab', 'a#', 'x' * 3000, 'x' * 3001, 'x' * 17, 'y' * 100]) for _ in range(5)]
        ctrls = [(r.choice(['n1', 'n2', '']), vals[i], r.random() < 0.5) for i in range(r.randint(0, 4))]
        return ('getvalue', ctrls, r.choice(['n1', 'n2', 'zz', None]))
    if kind in ('action', 'button'):
        return (kind, r.choice(['', 'bypass x', 'a' * 50]), r.random() < 0.5, [r.choice([0, 1, 2]) for _ in range(2)], r.choice([0, 0, 1]))
    return ('showpage',)


def main(n, seed, kind):
    # ShowPage (15 bytes: bLocked = 0; return LoadHtml(Text)) is inlined in ours, in both builds: it has no function of its own to call
    probe = BEmu('ours')
    for needed in {'showpage': ['showpage'], 'pieces': ['addpiece'], 'form': ['formpage'], 'post': ['postpage'], 'getvalue': ['getvalue'],
                   'action': ['action'], 'button': ['button']}.get(kind, []):
        if not [v for k, v in probe.symbols.items() if k.startswith(OURS_B[needed])]:
            sa.skip('NCBBSHtmlViewer ' + kind, 'no function of its own in ours (inlined)')
            return 0
    del probe
    r = random.Random(seed)
    cases = [gen(r, kind) for _ in range(n)]
    res = {}
    for k in ('stock', 'ours'):
        e = BEmu(k)
        e.heap_mark = e.heap_ptr
        res[k] = [run(e, c) for c in cases]
    bad = exc = 0
    for i, (a, b) in enumerate(zip(res['stock'], res['ours'])):
        if 'exc' in a or 'exc' in b:
            exc += 1
        if a != b:
            bad += 1
            if bad <= 2:
                print('case', i, str(cases[i])[:300])
                for kk in a:
                    if a.get(kk) != b.get(kk):
                        print('  key', kk)
                        print('   stock', str(a.get(kk))[:700])
                        print('   ours ', str(b.get(kk))[:700])
    print('t_html_bbs', kind, 'cases', n, 'differ', bad, 'exceptions', exc)
    return 1 if bad or exc else 0


if __name__ == '__main__':
    sys.exit(main(int(sys.argv[1]), int(sys.argv[2]), sys.argv[3]))
