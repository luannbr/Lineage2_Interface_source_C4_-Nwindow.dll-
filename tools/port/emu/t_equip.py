# STALE (C4, sa-emu agent `old`): no C4 successor for NCEquipItemWnd::OnDrawToolTip (needs the C4 tip4.py world and NItemEntry 0x1C40); C2 addresses, does not run.
import sys, random
import os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from t_fill import *

def setup(e, A, case, ds):
    # item entries
    items = []
    for it in case['items']:
        p = make_item(e, it['item'])
        info = p + 0x19B0
        e.call(A['ctor'], this=info)
        e.call(A['fill'], this=info, args=[p, 2, 0])
        e.write32(p + 0x1950, it['sel'])
        items.append(p)
    hennas = []
    for h in case['hennas']:
        if h is None:
            hennas.append(0)
        else:
            p = make_item(e, h)
            info = p + 0x19B0
            e.call(A['ctor'], this=info)
            e.call(A['fill'], this=info, args=[p, 2, 0])
            hennas.append(p)
    def arr(ptrs):
        d = e.alloc(4 * max(1, len(ptrs)))
        for i, v in enumerate(ptrs):
            e.write32(d + 4 * i, v)
        return d
    wnd = e.alloc(0x260)
    e.uc.mem_write(wnd, bytes(0x260))
    vt = e.alloc(0x400)
    for i in range(0x100):
        e.write32(vt + 4 * i, 0)
    e.write32(vt + 0x10C, ds['getequip'])
    e.write32(wnd, vt)
    for off, v in ((0x44, case['sx']), (0x48, case['sy']), (0x4C, case['w']), (0x50, case['h'])):
        e.write32(wnd + off, v)
    skin = e.alloc(0x9C)
    e.uc.mem_write(skin, bytes(0x9C))
    e.call(A['skinctor'], this=skin)
    for i in range(9):
        e.write32(skin + 4 * i, (0x1000 + i) if case['tex'][i] else 0)
    e.write32(wnd + 0x70, skin if case['skin'] else 0)
    e.write32(wnd + 0xA0, arr(items)); e.write32(wnd + 0xA4, len(items)); e.write32(wnd + 0xA8, len(items))
    e.write32(wnd + 0xAC, arr(hennas)); e.write32(wnd + 0xB0, len(hennas)); e.write32(wnd + 0xB4, len(hennas))
    e.write32(wnd + 0x120, case['cell'])
    e.write32(wnd + 0x128, case['hover'])
    e.write32(wnd + 0x24C, case['hh'])
    return wnd

def main(n, seed):
    r = random.Random(seed)
    cases = []
    for _ in range(n):
        items = [{'item': rand_item(r), 'sel': r.choice([0, 1])} for _ in range(r.randint(0, 6))]
        for it in items:
            it['item']['ints'][0x1914] = r.randint(1, 99)
        hen = [None if r.random() < 0.3 else rand_item(r) for _ in range(r.randint(0, 4))]
        cases.append({'items': items, 'hennas': hen, 'sx': r.randint(0, 600), 'sy': r.randint(0, 400), 'w': r.randint(100, 300), 'h': r.randint(100, 300),
                      'cell': r.choice([32, 32, 34]), 'hover': r.randint(-1, 15), 'hh': r.randint(-1, 5), 'skin': r.random() < 0.9,
                      'tex': [r.random() < 0.8 for _ in range(9)]})
    res = {}
    for kind in ('stock', 'ours'):
        e = TipEmu(kind)
        A = addrs(e) if kind == 'ours' else STOCK_ADDR
        if kind == 'stock':
            fn = 0x10080480
            ge_target = 0x10080480
        else:
            fn = [v for s, v in e.symbols.items() if s.startswith('?OnDrawToolTip@NCEquipItemWnd@@')][0]
        # a stub address for GetEquipSlot
        stub = STUB_BASE + 0x9000
        e.uc.mem_write(stub, b'\xC3')
        e.stubs[stub] = ('fn:GetEquipSlot', None)
        ds = {'getequip': stub}
        e.heap_mark = e.heap_ptr
        outs = []
        for c in cases:
            e.heap_ptr = e.heap_mark
            e.log.clear()
            wnd = setup(e, A, c, ds)
            dev = e.alloc(0x80)
            e.uc.mem_write(dev, bytes(0x80))
            e.log.clear()
            ret = e.call(fn, this=wnd, args=[dev])
            dv = [e.read32(dev + o) for o in (0x2C, 0x30, 0x48, 0x4C, 0x5C)]
            outs.append((ret, dv, list(e.log)))
        res[kind] = outs
    bad = 0
    for i, (a, b) in enumerate(zip(res['stock'], res['ours'])):
        if a != b:
            bad += 1
            if bad <= 3:
                print('case', i, cases[i]['hover'], cases[i]['hh'])
                print(' stock', str(a)[:900])
                print(' ours ', str(b)[:900])
    print('equip cases', n, 'differ', bad)

if __name__ == '__main__':
    main(int(sys.argv[1]), int(sys.argv[2]))
