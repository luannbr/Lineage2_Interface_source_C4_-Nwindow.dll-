# Differential test of NToolTipSkin::DrawTitleText / DrawFullText (stock vs ours under unicorn); the tip (NToolTipInfo, C4 layout) is
# built by the test, the device and the console's text getters are logging stubs.
#   python t_tipdraw.py [cases] [seed] [title|full]
import sys, os, random, struct
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from tip4 import *

FUNCS = {'title': 0x100301f0, 'full': 0x1002f750}
SKIN = 0xA4
NAMES = ['', 'Sword', 'Dagger of Doom', "Zubei's Boots", 'A' * 30]
NAMES2 = ['', '', 'Of the Ages', 'x' * 20, 'y']
LABELS = ['', 'P.Atk', 'Weight', 'Soulshot', 'A long label that goes on', 'Range']
VALUES = ['', '1', '123', 'wt3 / slot', '1,234,567', 'X 5', 'more text here']
DESCS = ['', 'A plain item.', 'Line one.\r\nLine two.\r\nLine three that is a bit longer than the others, sic.', 'x\r\n', '\r\n\r\nz']


def rand_lines(r, icons):
    out = []
    for _ in range(r.choice([0, 0, 1, 2, 3, 5, 8])):
        ln = dict(label=r.choice(LABELS), value=r.choice(VALUES), lcol=r.choice([0xFFA3A3A3, 0xFF112233]),
                  vcol=r.choice([0xFFB09B79, 0xFFDCDCDC, 0xFF00FF00, 0xFFFFFF00]), blabel=r.choice([1, 1, 0]), bcolon=r.choice([1, 0]),
                  u408=r.choice([0, 7]))
        if icons:
            ln['tw'] = r.choice([16, 32, 24])
            ln['th'] = r.choice([16, 32, 12])
        out.append(ln)
    return out


def rand_case(r):
    style = r.choice([0, 1, 2, 4, 8, 0x10, 0x20, 0x40, 0x80, 0x100, 0x200, 0x400, 0x800, 0x802, 0x1000, 0x1002, 0x2000, 0x8000])
    c = dict(name=r.choice(NAMES), name2=r.choice(NAMES2), u18=r.choice([-1, -1, -1, 0, 5, 140, 563]),
             enchant=r.choice([0, 0, 1, 3, 12, 20]), grade=r.choice([0, 1, 2, 3, 4, 5, 9, -1]),
             count=r.choice([0, 1, 2, 999, 1234, 56789, 1234567, 2000000000, -5]), extra=r.choice([0, 0, 7, 100]),
             show=r.choice([0, 1, 2, 3, 4, 3]), type=r.choice([-1, 0, 1, 2, 3, 4, 5, 6]), level=r.choice([0, 1, 40, 70]),
             itemid=r.choice([0, 57, 1000]), sctype=r.choice([0, 1, 2, 3, 4, 5]), style=style,
             opts=r.choice([0, 0, 1, 2, 3, 4, 5, 8, 0xC, 0x20, 0x24, 0x2F]), desc=r.choice(DESCS),
             lines=rand_lines(r, bool(style & 0x1000)))
    c['fw'] = r.choice([0, 10, 100.5, 250, 300.25])
    c['fh'] = r.choice([0, 13, 90, 240.5])
    c['tw'] = r.choice([0, 20, 120.5, 300])
    c['th'] = r.choice([0, 13, 26.25])
    c['x'] = r.choice([0, 5, 100, 1000, -20])
    c['y'] = r.choice([0, 5, 50, 700, -3])
    c['tex'] = [r.random() < 0.8 for _ in range(9)]
    return c


def run(e, A, c):
    mark = len(e.log)
    skin = e.alloc(SKIN)
    e.uc.mem_write(skin, bytes(SKIN))
    for i in range(9):
        e.write32(skin + 4 * i, (0x1000 + i) if c['tex'][i] else 0)
    put_info(e, skin + 0x24, c)
    dev = 0x31337000
    e.objs[dev] = 'device'
    va, nargs = A
    e.call(va, this=skin, args=[dev, c['x'] & 0xFFFFFFFF, c['y'] & 0xFFFFFFFF])
    return dict(log=e.log[mark:], info=dump_info(e, skin + 0x24))


def main():
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 200
    seed = int(sys.argv[2]) if len(sys.argv) > 2 else 1
    which = sys.argv[3].split(',') if len(sys.argv) > 3 else ['title', 'full']
    es = TW('stock')
    eo = TW('ours')
    lm = log_map()
    es.heap_mark, eo.heap_mark = es.heap_ptr, eo.heap_ptr
    r = random.Random(seed)
    total = 0
    for f in which:
        cases = [rand_case(r) for _ in range(n)]
        va = FUNCS[f]
        As, Ao = (va, 3), (lm[va], 3)
        total += compare(f, es, eo, run, cases, As, Ao)
    print('TOTAL problems', total)


if __name__ == '__main__':
    main()
