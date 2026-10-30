# Differential test of NWndUtil::MakeTimeStr (0x10037BC0, cdecl: the FString comes back through the hidden first argument, then the seconds)
# against the stock (stock vs ours under unicorn): "<days><day> <hours><hour> <minutes><minute>" with the three system strings 1109 /
# 1110 / 1111 (the tip4.py world fills every system string with "S<id>"). Compared: the returned string (and eax), the sprintf call (format and
# result), every import call. The seconds are random over the whole range of an INT (negative ones too: the divisions are signed).
#   python t_timestr.py [cases] [seed]
import sys, os, random
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from tip4 import *

VA = 0x10037BC0


def rand_case(r):
    kind = r.randrange(8)
    if kind == 0:
        s = r.randrange(-100000, 100000)
    elif kind == 1:
        s = r.randrange(0, 5 * 86400)
    elif kind == 2:
        s = r.choice([0, 59, 60, 61, 3599, 3600, 3601, 86399, 86400, 86401, -1, -60, -3600, -86400, 0x7FFFFFFF, -0x80000000])
    elif kind == 3:
        s = r.randrange(-0x80000000, 0x7FFFFFFF)
    else:
        s = r.randrange(0, 40 * 86400)
    return dict(seconds=s)


def run(e, A, c):
    mark = len(e.log)
    ret = e.alloc(16)
    e.uc.mem_write(ret, bytes(16))
    r = e.call(A[0], args=[ret, c['seconds'] & 0xFFFFFFFF], conv='cdecl')
    return dict(ret=(r == ret), text=e.fs_get(ret), log=e.log[mark:])


def main():
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 400
    seed = int(sys.argv[2]) if len(sys.argv) > 2 else 1
    es = TW('stock')
    eo = TW('ours')
    lm = log_map()
    es.heap_mark, eo.heap_mark = es.heap_ptr, eo.heap_ptr
    r = random.Random(seed)
    cases = [rand_case(r) for _ in range(n)]
    total = compare('MakeTimeStr', es, eo, run, cases, (VA,), (lm[VA],))
    print('TOTAL problems', total)


if __name__ == '__main__':
    main()
