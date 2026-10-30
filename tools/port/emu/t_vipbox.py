# STALE (C4, sa-emu agent `old`): covered by win/t_vipwnd.py (gridpaint / wndpaint); the C2 list box / list window classes do not exist in C4; C2 model, does not run.
"""Differential test of the VIP shop's painters that need no children: NCVIPShopItemListBox::OnPaint (stock
0x100861a0), NCVIPShopItemListWnd::OnPaint (0x10086690), NCVIPShopWnd::OnPaint (0x10086b10), against ours.
usage: python tools/port/emu/t_vipbox.py <cases> <seed>"""
import sys, random
import os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from t_vip import *


def build_box(e, c):
    wnd = e.alloc(0x400)
    e.uc.mem_write(wnd, bytes(0x400))
    for off, v in ((0x44, c['sx']), (0x48, c['sy']), (0x4C, c['w']), (0x50, c['h']),
                   (0x1A8, c['hl']), (0x1C4, c['vis']), (0x1CC, c['top']), (0x1D0, c['num']), (0x1D4, c['sel']),
                   (0x1DC, c['tx']), (0x1E0, c['ty']), (0x1E4, c['gap'])):
        e.write32(wnd + off, v)
    ptrs = []
    for en in c['entries']:
        p = e.alloc(0x520)
        e.uc.mem_write(p, bytes(0x520))
        e.write32(p + 0, 1)
        e.write32(p + 4, en['id'])
        e.write_wstr_at(p + 8, en['name'])
        e.write_wstr_at(p + 0x208, en['desc'])
        e.write32(p + 0x408, en['icon'])
        e.write32(p + 0x510, en['type'])
        e.write32(p + 0x514, en['count'])
        ptrs.append(p)
    d = e.alloc(4 * max(1, len(ptrs)))
    for i, v in enumerate(ptrs):
        e.write32(d + 4 * i, v)
    e.write32(wnd + 0x1EC, d)
    e.write32(wnd + 0x1F0, len(ptrs))
    e.write32(wnd + 0x1F4, len(ptrs))
    return wnd


def main(n, seed):
    r = random.Random(seed)
    cases = []
    for _ in range(n):
        ne = r.randint(1, 6)
        ents = [{'id': r.randint(0, 30), 'name': r.choice(['', 'Sword', 'Zubei\'s Boots', 'A' * 40]),
                 'desc': r.choice(['', 'Desc', 'x' * 30]), 'icon': r.choice([0, 0x1234]),
                 'type': r.choice([0, 1, 5, 5]), 'count': r.choice([0, 3, 1000])} for _ in range(ne)]
        cases.append({'sx': r.randint(0, 600), 'sy': r.randint(0, 400), 'w': r.randint(100, 300), 'h': r.randint(100, 300),
                      'hl': r.choice([0, 0x77]), 'vis': r.randint(1, 8), 'top': r.randint(0, 2), 'num': r.randint(0, ne + 1),
                      'sel': r.randint(-1, ne), 'tx': r.randint(0, 5), 'ty': r.randint(0, 5), 'gap': r.choice([4, 4, 0]),
                      'entries': ents, 'back': r.choice([0, 0x55]), 'page': r.choice([0, 1]),
                      'ss': r.choice([0, 1])})
    res = {}
    for kind in ('stock', 'ours'):
        e = VipEmu(kind)
        e.heap_mark = e.heap_ptr
        syms = e.symbols if kind == 'ours' else None
        fl = 0x100861A0 if kind == 'stock' else [v for s, v in syms.items() if s.startswith('?OnPaint@NCVIPShopItemListBox@@')][0]
        fw = 0x10086690 if kind == 'stock' else [v for s, v in syms.items() if s.startswith('?OnPaint@NCVIPShopItemListWnd@@')][0]
        fs = 0x10086B10 if kind == 'stock' else [v for s, v in syms.items() if s.startswith('?OnPaint@NCVIPShopWnd@@')][0]
        outs = []
        for c in cases:
            e.heap_ptr = e.heap_mark
            e.log.clear()
            wnd = build_box(e, c)
            dev = e.alloc(0x80)
            e.uc.mem_write(dev, bytes(0x80))
            r1 = e.call(fl, this=wnd, args=[dev])
            dv = [e.read32(dev + o) for o in (0x2C, 0x30, 0x48, 0x4C, 0x5C)]
            # list page / shop page
            w2 = e.alloc(0x400)
            e.uc.mem_write(w2, bytes(0x400))
            for off, v in ((0x44, c['sx']), (0x48, c['sy']), (0x4C, c['w']), (0x50, c['h']), (0x9C, c['back'])):
                e.write32(w2 + off, v)
            e.write32(w2 + 0xB8, c['page'])
            e.write32(w2 + 0xAC, c['back'])
            r2 = e.call(fw, this=w2, args=[dev])
            r3 = e.call(fs, this=w2, args=[dev])
            outs.append((r1, r2, r3, dv, list(e.log)))
        res[kind] = outs
    bad = 0
    for i, (a, b) in enumerate(zip(res['stock'], res['ours'])):
        if a != b:
            bad += 1
            if bad <= 3:
                print('case', i, cases[i])
                for j, (x, y) in enumerate(zip(a[4], b[4])):
                    if x != y:
                        print('  first diff at log', j, '\n   stock', x, '\n   ours ', y)
                        break
                print(a[:4], b[:4], len(a[4]), len(b[4]))
    print('vipbox cases', n, 'differ', bad)


if __name__ == '__main__':
    main(int(sys.argv[1]), int(sys.argv[2]))
