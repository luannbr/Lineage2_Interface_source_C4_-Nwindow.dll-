# Differential tests of the small anonymous helpers of the items3 lane (stock against ours under unicorn): std::uninitialized_fill_n for dwords
# (0x10043e60), std::deque<POINT>::_Tidy (0x10037eb0), TArray<INT>::Empty (0x1002ea50), NCShopItemWnd::SetSide (0x101028e0) and the title
# setters of the trade and recipe shop windows (0x10119d10, 0x100efc60).
#   python t_vipwnd4.py [cases] [seed] [fill,tidy,emptyint,setside,tradetitle,recipetitle]
import sys, os, random, struct
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import tip4
from tip4 import *
import gw
from t_vipwnd3 import V3

tip4.OURS_SYMS.update({0x10011810: '?SetTitleID@NCFrameCtrl@@QAEXH@Z'})


def fill_case(r):
    return dict(count=r.choice([0, 1, 2, 5, 17]), value=r.randint(0, 0xFFFFFFFF), nullfirst=r.random() < 0.3, pre=r.randint(0, 255))


def run_fill(e, A, c):
    mark = len(e.log)
    buf = e.alloc(0x100)
    e.uc.mem_write(buf, bytes([c['pre']]) * 0x100)
    val = e.alloc(16)
    e.write32(val, c['value'])
    first = 0 if c['nullfirst'] else buf + 8
    e.call(A[0], args=[first, c['count'], val])
    return dict(log=e.log[mark:], mem=bytes(e.uc.mem_read(buf, 0x100)))


def tidy_case(r):
    return dict(blocks=r.choice([0, 1, 3, 8]), size=r.choice([0, 1, 7, 30]), off=r.choice([0, 1, 5]), nullmap=r.random() < 0.15)


def run_tidy(e, A, c):
    mark = len(e.log)
    d = e.alloc(0x20)
    e.uc.mem_write(d, bytes(0x20))
    n = c['blocks']
    mp = 0 if (n == 0 or c['nullmap']) else e.alloc(4 * n + 16)
    for k in range(n):
        if mp:
            blk = e.alloc(32)
            e.objs[blk] = 'blk%d' % k
            e.write32(mp + 4 * k, blk)
    if mp:
        e.objs[mp] = 'map'
    e.write32(d + 4, mp)
    e.write32(d + 8, n)
    e.write32(d + 0xC, c['off'])
    e.write32(d + 0x10, c['size'])
    e.call(A[0], this=d, args=[])
    return dict(log=e.log[mark:], mem=e.snapshot(d, 0x14))


def empty_case(r):
    return dict(n=r.choice([0, 3, 9]), slack=r.choice([0, 0, 4, 16]))


def run_empty(e, A, c):
    mark = len(e.log)
    arr = e.alloc(0x20)
    e.uc.mem_write(arr, bytes(0x20))
    data = e.alloc(0x80) if c['n'] else 0
    e.write32(arr, data)
    e.write32(arr + 4, c['n'])
    e.write32(arr + 8, c['n'] + 2)
    e.call(A[0], this=arr, args=[c['slack']])
    return dict(log=e.log[mark:], hdr=[e.read32(arr + 4), e.read32(arr + 8)])


def side_case(r):
    return dict(v=r.choice([0, 1, 2, -1, 77]))


def run_side(e, A, c):
    mark = len(e.log)
    w = e.window('grid', size=0x400)
    e.write32(w + 0x170, 99)
    e.call(A[0], this=w, args=[c['v'] & 0xFFFFFFFF])
    return dict(log=e.log[mark:], f=e.read32(w + 0x170))


def title_case(r):
    return dict(v=r.choice([0, 136, 561, 77, -1]), has=r.random() < 0.9)


def make_run_title(off):
    def run(e, A, c):
        mark = len(e.log)
        w = e.window('shopwnd', size=0x400)
        fr = e.window('frame', size=0x300)
        e.write32(w + off, fr)
        e.call(A[0], this=w, args=[c['v'] & 0xFFFFFFFF])
        return dict(log=e.log[mark:])
    return run


TESTS = {
    'fill': (0x10043e60, 3, fill_case, run_fill, None),
    'tidy': (0x10037eb0, 0, tidy_case, run_tidy, None),
    'emptyint': (0x1002ea50, 1, empty_case, run_empty, None),
    'setside': (0x101028e0, 1, side_case, run_side, None),
    'tradetitle': (0x10119d10, 1, title_case, make_run_title(0x11C), {0x10011810: ('SetTitleID', 4, 0)}),
    'recipetitle': (0x100efc60, 1, title_case, make_run_title(0x124), {0x10011810: ('SetTitleID', 4, 0)}),
}


# standalone mode: the stand-in `NPointDequeTidyExt::TidyBlocks` is the standalone's NPointDeque::Tidy (one source function); TArray<INT>::Empty
# (`emptyint`) is a header inline that the standalone expands in its callers: no function, SKIP
SA_ALT = {'tidy': '?Tidy@NPointDeque@@QAEXXZ'}


def main():
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 100
    seed = int(sys.argv[2]) if len(sys.argv) > 2 else 1
    which = sys.argv[3].split(',') if len(sys.argv) > 3 else list(TESTS)
    lm = log_map()
    total = 0
    for name in which:
        va, nargs, mk, run, svc = TESTS[name]
        ova = sa_pick(lm, va, name, SA_ALT.get(name))      # (standalone mode: None = no standalone function, SKIP line printed)
        if ova is None:
            continue
        r = random.Random(seed)
        es = V3('stock', svc)
        eo = V3('ours', svc)
        es.heap_mark, eo.heap_mark = es.heap_ptr, eo.heap_ptr
        cases = [mk(r) for _ in range(n)]

        def runner(e, A, c, run=run):
            e.case = c
            e.nobj = 0
            e.params = {}
            return run(e, A, c)
        total += compare(name, es, eo, runner, cases, (va, nargs), (ova, nargs))
    print('TOTAL problems', total)


if __name__ == '__main__':
    main()
