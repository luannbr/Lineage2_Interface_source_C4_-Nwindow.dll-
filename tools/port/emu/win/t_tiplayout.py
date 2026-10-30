# Differential test of NToolTipInfo::Layout (stock vs ours under unicorn); the tip (NToolTipInfo, C4 layout) is built by the test, the
# console's text getters and the device's GetTextExtent / GetCharSize are logging stubs. The compared result: the calls (arguments
# included) and the tip after the call (FullBox, TitleBox...).
#   python t_tiplayout.py [cases] [seed]
import sys, os, random
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from tip4 import *
from t_tipdraw import NAMES, NAMES2, LABELS, VALUES, DESCS, rand_lines

VA = 0x1002ea90
STYLES = [0, 1, 2, 4, 8, 0x10, 0x20, 0x40, 0x80, 0x100, 0x200, 0x400, 0x800, 0x802, 0x1000, 0x1002, 0x2000, 0x8000, 0x10000, 0x20000,
          0x40000, 0x800000, 0x2, 0x2, 0x2, 0x20, 0x20, 0x14, 0x4, 0x10]


def rand_case(r):
    style = r.choice(STYLES)
    c = dict(name=r.choice(NAMES), name2=r.choice(NAMES2), u18=r.choice([-1, -1, -1, 0, 5, 140, 563]),
             enchant=r.choice([0, 0, 1, 3, 12, 20]), grade=r.choice([0, 1, 2, 3, 4, 5, 9, -1]),
             count=r.choice([0, 1, 2, 999, 1234, 56789, 1234567, 2000000000, -5]), extra=r.choice([0, 0, 7, 100]),
             show=r.choice([0, 1, 2, 3, 4, 3]), type=r.choice([-1, 0, 1, 2, 3, 4, 5, 6]), level=r.choice([0, 1, 40, 70]),
             itemid=r.choice([0, 57, 1000]), sctype=r.choice([0, 1, 2, 3, 4, 5, 6]), style=style,
             opts=r.choice([0, 0, 1, 2, 3, 4, 5, 8, 0xC, 0x20, 0x24, 0x2F]), desc=r.choice(DESCS),
             lines=rand_lines(r, False))
    # the stock reads the first two lines of a 0x10000 tip and the price line's extent of an uninitialized stack slot without
    # lines: keep the tests to the defined cases
    if style == 0x10000:
        while len(c['lines']) < 2:
            c['lines'].append(dict(label=r.choice(LABELS), value=r.choice(VALUES), blabel=1, bcolon=1))
    if (style & 2) and (c['opts'] & 0xC) and not c['lines']:
        c['lines'] = rand_lines(random.Random(r.random()), False) or [dict(label='Price', value='1,000', blabel=1, bcolon=1)]
    c['fw'] = r.choice([0, 10, 100.5, 250, 300.25])
    c['fh'] = r.choice([0, 13, 90, 240.5])
    c['tw'] = r.choice([0, 20, 120.5, 300])
    c['th'] = r.choice([0, 13, 26.25])
    return c


def run(e, A, c):
    mark = len(e.log)
    info = e.alloc(0x80)
    put_info(e, info, c)
    va, nargs = A
    e.call(va, this=info, args=[])
    return dict(log=e.log[mark:], info=dump_info(e, info))


def main():
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 200
    seed = int(sys.argv[2]) if len(sys.argv) > 2 else 1
    es = TW('stock')
    eo = TW('ours')
    lm = log_map()
    es.heap_mark, eo.heap_mark = es.heap_ptr, eo.heap_ptr
    r = random.Random(seed)
    cases = [rand_case(r) for _ in range(n)]
    total = compare('layout', es, eo, run, cases, (VA, 0), (lm[VA], 0))
    print('TOTAL problems', total)


if __name__ == '__main__':
    main()
