"""Differential test of NToolTipInfo::FormatNumber (stock 0x1002E910, "%d" with a comma before each group of three digits, into a static
buffer) and NToolTipInfo::NumberColor (0x1002E9C0, the colour of the text by its digit count), stock against ours, on the edges of the
groups of three and on negative numbers. C4 addresses (the C2 script had 0x100273C0 / 0x10027470) and no engine service is needed, so
the C2 service tables of stubs.py are emptied.
usage: python tools/port/emu/t_num.py [cases] [seed]   (cases = random numbers added to the fixed list; build/bin/Release/NWindowDecomp.dll + .map,
       or NWD_SA=1 for the standalone)"""
import sys
import os
import random
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from stubs import *
import stubs

stubs.STOCK_FN.clear()          # the C2 services of stubs.py are not used by these two functions (and sit at other C4 addresses)
stubs.OURS_FN.clear()
TipEmu.CONSOLE_VA = 0x102C39F4


def run(kind, extra):
    e = TipEmu(kind)
    if kind == 'stock':
        fmt, col = 0x1002E910, 0x1002E9C0
    else:
        fmt = [v for k, v in e.symbols.items() if k.startswith('?FormatNumber@NToolTipInfo@@')][0]
        col = [v for k, v in e.symbols.items() if k.startswith('?NumberColor@NToolTipInfo@@')][0]
    out = []
    for n in [0, 5, 12, 123, 999, 1000, 1234, 12345, 123456, 1234567, 12345678, 123456789, 2147483647, -1, -12, -123, -1234, -123456,
              -2147483648] + extra:
        p = e.call(fmt, args=[n & 0xFFFFFFFF])
        s = e.wstr(p)
        c = e.call(col, args=[p])
        out.append((n, s, hex(c)))
    # NumberColor alone, on texts with any mix of digits and commas (the digit count is the length without the commas)
    rnd = random.Random(7)
    for _ in range(len(extra) + 24):
        t = ''.join(rnd.choice('0123456789,,') for _ in range(rnd.randint(0, 16)))
        c = e.call(col, args=[e.put_wstr(t)])
        out.append((t, '', hex(c)))
    return out


def main():
    cases = int(sys.argv[1]) if len(sys.argv) > 1 else 0
    seed = int(sys.argv[2]) if len(sys.argv) > 2 else 1
    rnd = random.Random(seed)
    extra = []
    for _ in range(cases):
        k = rnd.randint(1, 9)
        extra.append(rnd.randint(-10 ** k, 10 ** k))
    a = run('stock', extra)
    b = run('ours', extra)
    bad = 0
    for x, y in zip(a, b):
        ok = x == y
        if not ok:
            bad += 1
        if not ok or '-v' in sys.argv:
            print(x, 'OK' if ok else 'DIFF %s' % (y,))
    print('FormatNumber+NumberColor cases %d  differ %d  raised 0' % (len(a), bad))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
