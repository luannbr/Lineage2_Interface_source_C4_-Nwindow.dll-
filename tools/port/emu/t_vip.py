# STALE (C4, sa-emu agent `old`): covered by win/t_vipwnd.py (draw: NCVIPShopItemInfoWnd::DrawItemInfo 0x100A0160); C2 model, does not run.
"""Differential test of NCVIPShopItemInfoWnd::DrawItemInfo (stock 0x10084840 against ours): random item entries,
the calls logged by the stubs (DrawText, DrawTexture, GetTextExtent...) and the returned Y must be equal.
usage: python tools/port/emu/t_vip.py <cases> <seed>"""
import sys, random
import os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from t_fill import *

FN_POP['HasAddName'] = 4
FN_POP['GetAddName'] = 4


class VipEmu(TipEmu):
    def __init__(self, kind):
        super().__init__(kind)
        stub = STUB_BASE + 0x9100
        self.uc.mem_write(stub, b'\xC3')
        self.stubs[stub] = ('fn:HasAddName', None)
        stub2 = STUB_BASE + 0x9110
        self.uc.mem_write(stub2, b'\xC3')
        self.stubs[stub2] = ('fn:GetAddName', None)
        for va, st in ((0x10057730, stub), (0x100578a0, stub2)):
            code = b'\x68' + struct.pack('<I', st) + b'\xC3'
            if kind == 'stock':
                self.uc.mem_write(va, code)
            else:
                self.uc.mem_write(NWBASE + (va - 0x10000000), code)

    def do_fn(self, nm):
        if nm == 'HasAddName':
            v = self.arg(0)
            self.log.append(('HasAddName', v))
            return 1 if v % 3 != 0 else 0
        if nm == 'GetAddName':
            v = self.arg(0)
            self.log.append(('GetAddName', v))
            return self.static_str('add%d' % v)
        if nm == 'ConvertNewLines' and self.arg(0) == 0:
            self.fs_set(self.arg(1), '')
            return None
        return super().do_fn(nm)


def main(n, seed):
    r = random.Random(seed)
    cases = []
    for _ in range(n):
        it = rand_item(r)
        it['ints'][0x1918] = r.randint(0, 40)
        if r.random() < 0.15:
            it = None
        cases.append({'item': it, 'x': r.choice([16, 0, 5]), 'y': r.choice([15, 0, 100]),
                      'icon': r.randint(1, 1 << 20)})
    res = {}
    for kind in ('stock', 'ours'):
        e = VipEmu(kind)
        if kind == 'stock':
            fn = 0x10084840
        else:
            fn = [v for s, v in e.symbols.items() if s.startswith('?DrawItemInfo@NCVIPShopItemInfoWnd@@')][0]
        e.heap_mark = e.heap_ptr
        outs = []
        for c in cases:
            e.heap_ptr = e.heap_mark
            e.log.clear()
            wnd = e.alloc(0x2000)
            e.uc.mem_write(wnd, bytes(0x2000))
            dev = e.alloc(0x80)
            e.uc.mem_write(dev, bytes(0x80))
            item = 0
            if c['item'] is not None:
                item = make_item(e, c['item'])
                e.write32(item, c['icon'])
                # the three icon names / the unknown strings are not read by DrawItemInfo
            ret = e.call(fn, this=wnd, args=[dev, item, c['x'], c['y']])
            outs.append((ret, list(e.log)))
        res[kind] = outs
    bad = 0
    for i, (a, b) in enumerate(zip(res['stock'], res['ours'])):
        if a != b:
            bad += 1
            if bad <= 3:
                print('case', i, cases[i]['item']['ints'] if cases[i]['item'] else None)
                print(' stock', a[0], len(a[1]))
                print(' ours ', b[0], len(b[1]))
                for j, (x, y) in enumerate(zip(a[1], b[1])):
                    if x != y:
                        print('  first diff at log', j, '\n   stock', x, '\n   ours ', y)
                        break
                else:
                    print('  lengths differ', len(a[1]), len(b[1]))
    print('vip cases', n, 'differ', bad)


if __name__ == '__main__':
    main(int(sys.argv[1]), int(sys.argv[2]))
