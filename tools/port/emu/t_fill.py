# STALE (C4, sa-emu agent `old`): covered by win/t_tipfill.py (Fill), t_tiplayout.py (Layout), t_tipdraw.py / t_tipdraw2.py (NToolTipSkin), t_tipmatch.py (FillMatchRow); C2 addresses / the C2 item layout (0x1A28), does not run.
import sys, random
import os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from stubs import *

STOCK_ADDR = {
    'ctor': 0x10002230, 'fill': 0x10028B20, 'layout': 0x10027500, 'assign': 0x1001ACA0, 'skinctor': 0x10029F00,
    'title': 0x10028760, 'fulltext': 0x10027EB0, 'draw': 0x1002A230, 'drawfull': 0x10029FA0,
    'setclear': 0x10028AC0, 'settext': 0x10028AA0, 'macro': 0x10029BA0, 'match': 0x10029BF0,
}
OURS_SYM = {
    'ctor': '??0NToolTipInfo@@QAE@XZ', 'fill': '?Fill@NToolTipInfo@@', 'layout': '?Layout@NToolTipInfo@@',
    'assign': '??4NToolTipInfo@@QAEAAU0@ABU0@@Z', 'skinctor': '??0NToolTipSkin@@QAE@XZ',
    'title': '?DrawTitleText@NToolTipSkin@@', 'fulltext': '?DrawFullText@NToolTipSkin@@', 'draw': '?Draw@NToolTipSkin@@',
    'drawfull': '?DrawFull@NToolTipSkin@@', 'setclear': '?Clear@NToolTipInfo@@', 'settext': '?SetText@NToolTipInfo@@',
    'macro': '?FillMacro@NToolTipInfo@@', 'match': '?FillMatchRow@NToolTipInfo@@',
}


def addrs(e):
    if e.kind == 'stock':
        return STOCK_ADDR
    out = {}
    for k, pre in OURS_SYM.items():
        c = [v for s, v in e.symbols.items() if s.startswith(pre)]
        assert len(c) == 1, (k, c)
        out[k] = c[0]
    return out


def make_item(e, it):
    p = e.alloc(0x1B00)
    e.uc.mem_write(p, bytes(0x1B00))
    e.write_wstr_at(p + 0xC, it['name'])
    e.write_wstr_at(p + 0x20C, it['name2'])
    e.write_wstr_at(p + 0x70C, it['desc'])
    for off, v in it['ints'].items():
        e.write32(p + off, v)
    return p


def wcut(b):
    out = []
    for i in range(0, len(b), 2):
        c = b[i] | (b[i + 1] << 8)
        if c == 0:
            break
        out.append(chr(c))
    return ''.join(out)


def dump_info(e, p):
    d = {}
    d['name'] = e.fs_get(p)
    d['name2'] = e.fs_get(p + 0xC)
    d['ints'] = [e.rds32(p + o) for o in range(0x18, 0x38, 4)]
    data, num = e.read32(p + 0x38), e.read32(p + 0x3C)
    lines = []
    for i in range(num):
        b = bytes(e.uc.mem_read(data + 0x414 * i, 0x414))
        lab = wcut(b[:0x200])
        val = wcut(b[0x200:0x400])
        lines.append((lab, val) + struct.unpack('<IIiii', b[0x400:0x414])[:2] + struct.unpack('<ii', b[0x40C:0x414]))
    d['lines'] = lines
    d['desc'] = e.fs_get(p + 0x44)
    d['tail'] = [e.rds32(p + o) for o in range(0x50, 0x78, 4)]
    return d


def new_info(e, A):
    p = e.alloc(0x78)
    e.uc.mem_write(p, bytes(0x78))
    e.call(A['ctor'], this=p)
    return p


def rand_item(r):
    names = ['', 'Sword', 'Dagger of Doom', 'Zubei\'s Boots', 'A' * 40]
    descs = ['', 'A plain item.', 'Line one.\\nLine two.\\nLine three that is a bit longer than the others.', 'x\\n']
    ints = {}
    def s(off, v): ints[off] = v & 0xFFFFFFFF
    s(0x190C, r.choice([-1, 0, 0, 1, 1, 2, 3, 4, 5, 5, 6]))
    s(0x1910, r.choice([-1, 7, 0xC, 0xD, 0, 1]))
    s(0x1914, r.randint(1, 99999))
    s(0x1918, r.randint(0, 30))
    s(0x191C, r.choice([-1, 0, 1, 5, 9]))
    s(0x1920, r.choice([0, 1, 2, 999, 1234, 56789, 1234567, 2000000000]))
    s(0x1924, r.choice([0, 7, 1500, 123456, 98765432, 2000000000]))
    s(0x1928, r.choice([0, 1, 2, 0x40, 0x80, 0x100, 0x180, 0x400, 0x2000, 0x4000, 0x8000]))
    s(0x192C, r.randint(0, 3000))
    s(0x1934, r.choice([-1, 0, 1, 2, 3, 9]))
    s(0x1938, r.choice([-1, 0, 5, 77]))
    s(0x193C, r.choice([0, 0, 1, 3, 4, 16, 0x8001, 0xABCD]))
    s(0x1940, r.choice([0, 1, 0x10, 0x23, 0xFFFF, 0x80F0]))
    s(0x1944, r.choice([0, 1]))
    s(0x194C, r.choice([0, 0, 3, 40]))
    s(0x1958, r.choice([0, 0, 1, 2, 3, 4, 5]))
    s(0x1968, r.randint(0, 8))
    s(0x1970, r.choice([-1, 0, 3]))
    s(0x1974, r.randint(0, 20))
    s(0x1978, r.choice([0, 5, 0x1234, 0x80000000]))
    for off in (0x197C, 0x1980, 0x1984, 0x1988, 0x198C, 0x1990, 0x199C, 0x19A0, 0x19A4, 0x1A74):
        s(off, r.randint(0, 500))
    s(0x1A74, r.choice([0, 0, 3, 8]))
    return {'name': r.choice(names), 'name2': r.choice(['', '', 'Of the Ages', 'x' * 30]), 'desc': r.choice(descs), 'ints': ints}


def run_case(e, A, case):
    start = len(e.log)
    out = {}
    mode = case.get('mode', 'fill')
    if mode == 'fill':
        item = make_item(e, case['item'])
        info = new_info(e, A)
        e.call(A['fill'], this=info, args=[item, case['style'], case['opts']])
    elif mode == 'text':
        info = new_info(e, A)
        e.write32(info + 0x54, case['opts'])
        e.call(A['settext'], this=info, args=[e.put_wstr(case['text']), case['style']])
    elif mode == 'macro':
        info = e.alloc(0xB00)
        e.uc.mem_write(info, bytes(0xB00))
        e.call(A['ctor'], this=info)
        e.write_wstr_at(info + 0x9EC, case['mname'])
        e.write_wstr_at(info + 0xA06, case['mdesc'])
        e.call(A['macro'], this=info, args=[info, case['style']])
    else:
        row = e.alloc(0x888)
        e.uc.mem_write(row, bytes(0x888))
        cells = e.alloc(0x1820 * 3)
        e.uc.mem_write(cells, bytes(0x1820 * 3))
        e.write32(row, cells)
        e.write_wstr_at(row + 4, case['mdesc'])
        e.write32(row + 0x804, case['p'][0]); e.write32(row + 0x808, case['p'][1]); e.write32(row + 0x80C, case['p'][2])
        e.write32(cells + 0x180C, case['c'][0]); e.write32(cells + 0x1820 + 0x180C, case['c'][1])
        info = row + 0x810
        e.call(A['ctor'], this=info)
        e.call(A['match'], this=info, args=[row, case['style']])
    out['info'] = dump_info(e, info)
    out['log_fill'] = e.log[start:]
    # skin draws
    skin = e.alloc(0x9C)
    e.uc.mem_write(skin, bytes(0x9C))
    e.call(A['skinctor'], this=skin)
    for i in range(9):
        e.write32(skin + 4 * i, (0x1000 + i) if case['tex'][i] else 0)
    e.call(A['assign'], this=skin + 0x24, args=[info])
    mark = len(e.log)
    e.call(A['title'], this=skin, args=[0x1234, 11, 22])
    out['title'] = e.log[mark:]
    mark = len(e.log)
    e.call(A['fulltext'], this=skin, args=[0x1234, 11, 22])
    out['fulltext'] = e.log[mark:]
    # by-value draws
    for nm in ('draw', 'drawfull'):
        mark = len(e.log)
        px = e.alloc(8)
        e.write32(px, case['ox'])
        e.write32(px + 4, case['oy'])
        words = [e.read32(info + 4 * i) for i in range(0x78 // 4)]
        e.call(A[nm], this=skin, args=[0x1234] + words + [px, px + 4] + case['wnd'])
        out[nm] = e.log[mark:] + [('out', e.rds32(px), e.rds32(px + 4))]
    return out


def main(n, seed):
    r = random.Random(seed)
    cases = []
    styles = [1, 2, 4, 8, 0x10, 0x20, 0x40, 0x80, 0x802, 0x88, 0x14]
    optss = [0, 0, 1, 2, 3, 4, 7, 8, 0x20, 0x24]
    for _ in range(n):
        cases.append({'item': rand_item(r), 'style': r.choice(styles), 'opts': r.choice(optss),
                      'tex': [r.random() < 0.8 for _ in range(9)], 'ox': r.randint(-50, 1100), 'oy': r.randint(0, 300),
                      'wnd': [r.randint(-20, 900), r.randint(-20, 700), r.randint(10, 300), r.randint(10, 200)]})
    for _ in range(n // 3):
        cases.append({'mode': 'text', 'text': r.choice(['', 'Hello', 'A rather long tool tip text for a button', 'x\ny']), 'style': 0x400,
                      'opts': r.choice([0, 1, 2, 7]), 'item': None,
                      'tex': [r.random() < 0.8 for _ in range(9)], 'ox': r.randint(-50, 1100), 'oy': r.randint(0, 300),
                      'wnd': [r.randint(-20, 900), r.randint(-20, 700), r.randint(10, 300), r.randint(10, 200)]})
        cases.append({'mode': 'macro', 'mname': r.choice(['', 'Macro1', 'Heal all']), 'mdesc': r.choice(['', 'does a thing', 'a\nb\nc']), 'style': 0x100,
                      'opts': 0, 'item': None,
                      'tex': [r.random() < 0.8 for _ in range(9)], 'ox': r.randint(-50, 1100), 'oy': r.randint(0, 300),
                      'wnd': [r.randint(-20, 900), r.randint(-20, 700), r.randint(10, 300), r.randint(10, 200)]})
        cases.append({'mode': 'match', 'mdesc': r.choice(['', 'Looking for party\nLv 40+', 'x' * 60]), 'style': 0x200, 'opts': 0, 'item': None,
                      'p': [r.randint(0, 1), r.randint(0, 1), r.randint(0, 5)], 'c': [r.randint(0, 80), r.randint(1, 80)],
                      'tex': [r.random() < 0.8 for _ in range(9)], 'ox': r.randint(-50, 1100), 'oy': r.randint(0, 300),
                      'wnd': [r.randint(-20, 900), r.randint(-20, 700), r.randint(10, 300), r.randint(10, 200)]})
    es = TipEmu('stock')
    eo = TipEmu('ours')
    As, Ao = addrs(es), addrs(eo)
    bad = 0
    es.heap_mark = es.heap_ptr
    eo.heap_mark = eo.heap_ptr
    for i, c in enumerate(cases):
        es.heap_ptr = es.heap_mark
        eo.heap_ptr = eo.heap_mark
        a = run_case(es, As, c)
        b = run_case(eo, Ao, c)
        if a != b:
            bad += 1
            if bad <= 6:
                print('--- case', i, 'style %x opts %x' % (c['style'], c['opts']), c['item']['ints'].get(0x190C), 'K', c['item']['ints'].get(0x1910))
                for k in a:
                    if a[k] != b[k]:
                        print(' differs:', k)
                        print('  stock:', a[k] if len(str(a[k])) < 1500 else str(a[k])[:1500])
                        print('  ours :', b[k] if len(str(b[k])) < 1500 else str(b[k])[:1500])
        es.log.clear(); eo.log.clear()
    print('cases', n, 'differ', bad)


if __name__ == '__main__':
    main(int(sys.argv[1]), int(sys.argv[2]))
