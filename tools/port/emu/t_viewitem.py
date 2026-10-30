"""Differential test of the viewers' FillItemInfo (C4 stock 0x100D0E40 of NCPawnViewrWnd and 0x100BEE80 of NCNpcViewrWnd against ours; the C2
script had 0x100aafe0 / 0x1009c530): random item records of the three classes (weapon 0, armor 1, etc 2, and an unknown class) are turned
into an NItemInfo by both; the whole 0x19B4 bytes of the NItemInfo, the calls of the stubbed services (FindTexture) and the name
(L2FName) / string functions' results must be equal. (The C4 record, NViewItemDataC4 in include/nwindow/NConsoleWndC4.h, is read in place:
no virtual getters any more; the NPC viewer has no armor branch.)
usage: python tools/port/emu/t_viewitem.py <cases> <seed>   (needs build/bin/Release/NWindowDecomp.dll and its .map, or NWD_SA=1 for the standalone)"""
import sys, random, os, struct
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import emu as _emu
import stubs
from stubs import *
import sa

stubs.IMPORT_POP.update({'??DL2FName@@QBEPBGXZ': 0, '??DFName@@QBEPBGXZ': 0})
stubs.FN_POP.update({'FindTexture': 8})
stubs.STOCK_FN.clear()
stubs.OURS_FN.clear()
TipEmu.CONSOLE_VA = 0x102C39F4
stubs.STOCK_FN.update({0x1002B450: 'FindTexture'})                                      # NCObject::FindTexture(Name, bFallback)
stubs.OURS_FN.update({'FindTexture': ('@NCObject@@', '?FindTexture@NCObject@@')})

INFO_SIZE = 0x19B4
REC_SIZE = 0x5A0


def sym(e, pre):
    c = [v for s, v in e.symbols.items() if s.startswith(pre)]
    assert len(c) >= 1, pre
    return c[0]


class ViewItemEmu(TipEmu):
    def name_of(self, idx):
        return 'N%d' % idx

    def do_import(self, nm):
        if nm in ('??DL2FName@@QBEPBGXZ', '??DFName@@QBEPBGXZ'):
            idx = self.read32(self.this())
            self.log.append((nm, idx))
            return self.static_str(self.name_of(idx))
        return super().do_import(nm)

    def do_fn(self, nm):
        if nm == 'FindTexture':
            name = self.wstr(self.arg(0))
            self.log.append(('FindTexture', name, self.arg(1)))
            return 0 if name.startswith('N1') else 0x5000 + (sum(ord(c) for c in name) & 0xFFF)      # some names do not load
        return super().do_fn(nm)


def rand_record(r):
    klass = r.choice([0, 1, 2, 2, 0, 1, 7])
    ints = {}
    for off in range(0x08, REC_SIZE, 4):
        ints[off] = r.randint(-5, 400)
    for off in (0x3C, 0x40, 0x44, 0x48, 0x4C, 0x50, 0x54):         # the L2FName indexes (0 = no name; N1x = a texture that does not load)
        ints[off] = r.choice([0, 1, 10, 11, 17, r.randint(0, 900)])
    ints[0x04] = klass
    ints[0xA8] = r.choice([0, 1, 2, 5, 5, 2, 16, 8])               # the etc type
    desc = r.choice([None, '', 'A plain item.', 'Line one.\\nLine two', 'x' * 60])
    return {'ints': ints, 'desc': desc}


def make_record(e, c):
    p = e.alloc(REC_SIZE)
    e.uc.mem_write(p, bytes(REC_SIZE))
    for off, v in c['ints'].items():
        e.write32(p + off, v)
    if c['desc'] is None:
        e.write32(p + 0x58, 0); e.write32(p + 0x5C, 0); e.write32(p + 0x60, 0)
    else:
        e.fs_set(p + 0x58, c['desc'])
    return p


def run(kind, which, cases):
    e = ViewItemEmu(kind)
    if kind == 'stock':
        fn = {'pawn': 0x100D0E40, 'npc': 0x100BEE80}[which]
    else:
        cls = {'pawn': 'NCPawnViewrWnd', 'npc': 'NCNpcViewrWnd'}[which]
        fn = sym(e, '?FillItemInfo@%s@@' % cls)
    e.heap_mark = e.heap_ptr
    outs = []
    for c in cases:
        e.heap_ptr = e.heap_mark
        e.log.clear()
        wnd = e.alloc(0x400)
        e.uc.mem_write(wnd, bytes(0x400))
        info = e.alloc(INFO_SIZE)
        e.uc.mem_write(info, bytes(INFO_SIZE))
        rec = make_record(e, c)
        e.call(fn, this=wnd, args=[rec, info])
        outs.append((bytes(e.uc.mem_read(info, INFO_SIZE)), list(e.log)))
    return outs


def main(n, seed):
    r = random.Random(seed)
    cases = [rand_record(r) for _ in range(n)]
    tot = 0
    for which in ('pawn', 'npc'):
        a = run('stock', which, cases)
        b = run('ours', which, cases)
        bad = 0
        for i, (x, y) in enumerate(zip(a, b)):
            if x != y:
                bad += 1
                if bad <= 3:
                    print(which, 'case', i, 'class', cases[i]['ints'][4], 'desc', cases[i]['desc'])
                    if x[0] != y[0]:
                        for off in range(0, INFO_SIZE, 2):
                            if x[0][off:off + 2] != y[0][off:off + 2]:
                                print('  info differs at 0x%x: stock %s ours %s' % (off, x[0][off:off + 8].hex(), y[0][off:off + 8].hex()))
                                break
                    if x[1] != y[1]:
                        for j, (p, q) in enumerate(zip(x[1], y[1])):
                            if p != q:
                                print('  log differs at', j, '\n   stock', p, '\n   ours ', q)
                                break
                        else:
                            print('  log lengths', len(x[1]), len(y[1]))
        print('t_viewitem', which, 'cases %d  differ %d  raised 0' % (n, bad))
        tot += bad
    return 1 if tot else 0


if __name__ == '__main__':
    sys.exit(main(int(sys.argv[1]), int(sys.argv[2])))
