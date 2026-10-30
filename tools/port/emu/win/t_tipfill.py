# Differential test of NToolTipInfo::Fill (stock vs ours under unicorn): an item entry (NItemEntry, C4 layout, 0x1CA0 bytes with
# the shortcut's tail) made by the test; the console's getters and the number/time helpers are logging stubs. Compared: the calls
# with their arguments, and the tip after the call (strings, numbers, lines, boxes). The unused word of a line (+0x408, the stack's
# garbage in the stock) is not compared.
#   python t_tipfill.py [cases] [seed]
import sys, os, random, struct
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import tip4
from tip4 import *
from t_tipdraw import NAMES, NAMES2, DESCS
from t_tiplayout import rand_case as layout_case

VA = 0x10030770
ENTRY = 0x1CA0
STYLES = [1, 2, 2, 2, 2, 2, 2, 4, 4, 8, 0x10, 0x20, 0x20, 0x40, 0x80, 2 | 0x800, 2 | 0x800, 2 | 0x800, 4 | 0x10, 2 | 0x10, 1 | 2,
          0x20 | 0x40, 0, 0x100, 0x1000, 2 | 4, 0x80 | 8, 1 | 0x10, 0x40 | 4]
OPTS = [0, 0, 0, 4, 8, 0x20, 4 | 0x20, 8 | 0x20, 4 | 8, 1, 2, 3, 0x24, 0x2C]


def u32c(v):
    return v & 0xFFFFFFFF


def rand_item(r):
    it = dict(
        name=r.choice(NAMES), name2=r.choice(NAMES2), desc=r.choice(DESCS),
        type=r.choice([-1, 0, 0, 1, 1, 1, 2, 2, 3, 4, 5, 5, 5, 6]), kind=r.choice([-1, 0, 3, 7, 7, 0xD, 0xD, 0xE, 0xE, 0xF, 0xF, 9]),
        oid=r.choice([0, 77]), id=r.choice([0, 57, 1001, 3, 5, 10, 20]), lvl=r.choice([-1, 0, 1, 40, 70]),
        count=r.choice([0, 1, 2, 999, 1234, 56789, 1234567, 2000000000, -5]),
        price=r.choice([0, 5, 999, 1234, 56789, 12345678, 2000000000, 1234567890, -5]),
        body=r.choice([0, 1, 2, 0x40, 0x80, 0x100, 0x180, 0x8000, 0x4000, 0x20, 6]),
        weight=r.choice([0, 1, 30, 1500, 99999]), wtype=r.choice([-1, 0, 1, 2, 6, 9, 11, 12]),
        wanted=r.choice([-1, 0, 1, 25, 3600, 86399, 100000]),
        ench=r.choice([0, 0, 1, 3, 12, 20, 0x1234, 0xFFFF, 0x8001, 0x10]),
        c3c=r.choice([0, 1, 0x12, 0x3F, 0xABCD, 0x1F2, 0x10, 0xF0, 0x0F]),
        stack=r.choice([0, 1, 1, 2, 3]),
        trade=r.choice([0, 0, 5, -3, 1 << 32, (1 << 40) + 7, 123456, -(1 << 33), 2000000000]),
        grade=r.choice([0, 1, 2, 3, 4, 5, -1]), aspd=r.choice([0, 300, 500, 700]),
        c70=r.choice([0, 0, 4, 33, -2]), sub=r.choice([-1, 0, 1, 2, 3]), c78=r.choice([0, 5, 20]),
        c7c=r.choice([0, 1, 2, 0x5555, 0xFFFF, 0x10001, 7]), p_atk=r.choice([0, 100, 5000]), m_atk=r.choice([0, 50, 4000]),
        p_def=r.choice([0, 80, 900]), m_def=r.choice([0, 30, 700]), sh_def=r.choice([0, 15, 200]), sh_rate=r.choice([0, 20, 35]),
        mpb=r.choice([0, 25, 90]), soul=r.choice([0, 1, 5, 100, -2]), spirit=r.choice([0, 1, 7, 100, -2]),
        sc_extra=r.choice([0, 3, 99, -1]))
    return it


def put_item(e, it, p=None):
    if p is None:
        p = e.alloc(ENTRY + 16)
    e.uc.mem_write(p, bytes(ENTRY if p is not None else ENTRY + 16))
    e.write_wstr_at(p + 0x14, it['name'])
    e.write_wstr_at(p + 0x214, it['name2'])
    e.write_wstr_at(p + 0x914, it['desc'])
    for off, key in ((0x1B14, 'type'), (0x1B18, 'kind'), (0x1B1C, 'oid'), (0x1B20, 'id'), (0x1B24, 'lvl'), (0x1B28, 'count'),
                     (0x1B2C, 'price'), (0x1B30, 'body'), (0x1B34, 'weight'), (0x1B3C, 'wtype'), (0x1B40, 'wanted'),
                     (0x1B44, 'ench'), (0x1B4C, 'stack'), (0x1B64, 'grade'), (0x1B74, 'aspd'), (0x1B78, 'c70'), (0x1B7C, 'sub'),
                     (0x1B80, 'c78'), (0x1B84, 'c7c'), (0x1B88, 'p_atk'), (0x1B8C, 'm_atk'), (0x1B90, 'p_def'), (0x1B94, 'm_def'),
                     (0x1B98, 'sh_def'), (0x1B9C, 'sh_rate'), (0x1BA8, 'mpb'), (0x1BAC, 'soul'), (0x1BB0, 'spirit'),
                     (0x1C9C, 'sc_extra')):
        e.write32(p + off, u32c(it[key]))
    e.uc.mem_write(p + 0x1B48, struct.pack('<H', it['c3c'] & 0xFFFF))
    e.uc.mem_write(p + 0x1B54, struct.pack('<q', it['trade']))
    return p


def mask_info(d):
    d = dict(d)
    d['lines'] = [(a, b, (c[0], c[1], 0, c[3], c[4]), t) for (a, b, c, t) in d['lines']]
    return d


def run(e, A, c):
    mark = len(e.log)
    info = e.alloc(0x80)
    put_info(e, info, c['old'])
    item = put_item(e, c['item'])
    va, nargs = A
    e.call(va, this=info, args=[item, c['style'], c['opts']])
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
        cases.append(dict(old=old, item=rand_item(r), style=r.choice(STYLES), opts=r.choice(OPTS)))
    total = compare('fill', es, eo, run, cases, (VA, 3), (lm[VA], 3))
    print('TOTAL problems', total)


if __name__ == '__main__':
    main()
