# 0x100684b0 (request through the net handler slot 130) and 0x100bd7c0 (map cover reset) - stock vs ours
import sys, os, random, struct
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from gw import *
import gw
gw.VPOP[130] = 8
FUNCS = {'req': 0x100684b0, 'mapreset': 0x100bd7c0}
CALLEES = {0x100bd3d0: ('MapChildReset', 0, 0)}


class CW(GW):
    def __init__(self, kind):
        GW.__init__(self, kind, CALLEES, {})


def run(e, A, c):
    mark = len(e.log)
    rnd = random.Random(c['seed'])
    obj = e.window('obj', size=0x300, rnd=rnd)
    if c['f'] == 'req':
        h1 = e.window('h1', size=0x100, rnd=rnd)
        h2 = e.window('h2', size=0x100, rnd=rnd)
        e.write32(obj + 0x54, h1)
        e.write32(h1 + 0x64, h2)
    else:
        e.write32(obj + 0xE8, e.window('child', size=0x100, rnd=rnd) if c['child'] else 0)
    e.call(A[c['f']], this=obj, args=[])
    return dict(log=e.log[mark:], obj=e.snapshot(obj, 0x300))


if __name__ == '__main__':
    es = CW('stock'); eo = CW('ours')
    lm = log_map()
    es.heap_mark, eo.heap_mark = es.heap_ptr, eo.heap_ptr
    As = dict(FUNCS)
    Ao = {k: lm[v] for k, v in FUNCS.items()}
    r = random.Random(5)
    for f in FUNCS:
        compare(f, es, eo, run, [dict(f=f, seed=r.randint(0, 10 ** 6), child=r.randint(0, 1)) for _ in range(20)], As, Ao)
