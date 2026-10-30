# Differential test of NToolTipInfo::FillMatchRow (stock vs ours under unicorn): a list row (NListRow, C4 layout: the cells' pointer, the
# text, three parameters; the cells are 0x1A64 bytes) made by the test; the console's class names are logging stubs. Compared: the
# calls with their arguments and the tip after the call. The unused word of a line (+0x408) is not compared.
#   python t_tipmatch.py [cases] [seed]
import sys, os, random, struct
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from tip4 import *
from t_tipdraw import DESCS
from t_tiplayout import rand_case as layout_case
from t_tipfill import mask_info

VA = 0x10031B10
CELL = 0x1A64
STYLES = [0x80000, 0x400000, 0x20000, 0x40000, 0x100000, 0x200000, 0x8000, 0x800000, 0x10000, 0, 0x8001, 1, 2, 0x4000000, 0x1000000,
          0x10000, 0x8000, 0x800000, 0x100000, 0x200000]
TEXTS = ['', '12', '345', 'abc', '-7', ' 5x', 'Cell text', '0', '99', '1', '2', '56 and more', 'A longer text of a cell']


def put_row(e, r):
    cells = e.alloc(CELL * 12 + 16)
    e.uc.mem_write(cells, bytes(CELL * 12 + 16))
    for k in range(12):
        base = cells + CELL * k
        e.write_wstr_at(base, r['cells'][k])
        e.write32(base + 0x1A48, r['p1'][k] & 0xFFFFFFFF)
        e.write32(base + 0x1A4C, r['p2'][k] & 0xFFFFFFFF)
    row = e.alloc(0x890)
    e.uc.mem_write(row, bytes(0x890))
    e.write32(row, cells)
    e.write_wstr_at(row + 4, r['text'])
    e.write32(row + 0x804, r['rp1'] & 0xFFFFFFFF)
    e.write32(row + 0x808, r['rp2'] & 0xFFFFFFFF)
    e.write32(row + 0x80C, r['rp3'] & 0xFFFFFFFF)
    return row


def rand_row(r):
    return dict(cells=[r.choice(TEXTS) for _ in range(12)], p1=[r.choice([-1, 0, 1, 2, 3, 4]) for _ in range(12)],
                p2=[r.choice([-1, 0, 5, 24, 88]) for _ in range(12)], text=r.choice(DESCS),
                rp1=r.choice([0, 1, 5, -1]), rp2=r.choice([0, 1, 7, -1]), rp3=r.choice([0, 1, 2, 3, 4, 5, -1]))


def run(e, A, c):
    mark = len(e.log)
    info = e.alloc(0x80)
    put_info(e, info, c['old'])
    row = put_row(e, c['row'])
    va, nargs = A
    e.call(va, this=info, args=[row, c['style']])
    return dict(log=e.log[mark:], info=mask_info(dump_info(e, info)))


def main():
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 200
    seed = int(sys.argv[2]) if len(sys.argv) > 2 else 1
    es = TW('stock')
    eo = TW('ours')
    lm = log_map()
    es.heap_mark, eo.heap_mark = es.heap_ptr, eo.heap_ptr
    r = random.Random(seed)
    cases = []
    for _ in range(n):
        old = layout_case(r)
        old['lines'] = old['lines'][:3]
        cases.append(dict(old=old, row=rand_row(r), style=r.choice(STYLES)))
    total = compare('matchrow', es, eo, run, cases, (VA, 2), (lm[VA], 2))
    print('TOTAL problems', total)


if __name__ == '__main__':
    main()
