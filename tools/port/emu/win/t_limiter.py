# Differential tests of the NChatLimiter functions (the chat flood control): Clear, Tick, AddLine, SetLimits (stock vs ours under unicorn)
#   python t_limiter.py [cases] [seed]
import sys, os, random, struct
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from gw import *
import gw
import stubs
from unicorn.x86_const import *

FUNCS = {'clear': (0x10056350, 0), 'tick': (0x10053e50, 0), 'addline': (0x100563b0, 2), 'setlimits': (0x10058210, 8)}
CALLEES = {0x1002d660: ('NStringCtor', 4, 1), 0x1002d690: ('NStringDtor', 0, 0), 0x1002d4b0: ('NStringGetAt', 4, 0),
           0x1007d380: ('ShowSystemMessage', 8, 0)}
OURS_SYMS = {0x1002d660: '??0NString@@QAE@PBG@Z', 0x1002d690: '??1NString@@QAE@XZ', 0x1002d4b0: '?GetAt@NString@@QAEPAGI@Z'}
stubs.IMPORT_POP.update({'?appStrcmp@@YAHPBG0@Z': 0})
TEXTS = ['a', 'b', 'hello', 'hello!', '']


class LW(GW):
    def __init__(self, kind):
        GW.__init__(self, kind, CALLEES, OURS_SYMS)
        self.case = None
        self.rets['NStringCtor'] = self.nctor
        self.rets['NStringGetAt'] = lambda e, t, a: e.read32(t) + 2 * a[0]

    def nctor(self, e, t, a):
        s = e.wstr(a[0]) if a[0] else ''
        e.write32(t, e.put_wstr(s))
        e.write32(t + 4, len(s))
        e.log.append(('NStringCtor', s))
        return t

    def do_import(self, nm):
        if nm == '?appStrcmp@@YAHPBG0@Z':
            x, y = self.wstr(self.arg(0)), self.wstr(self.arg(1))
            return 0 if x == y else (1 if x > y else 0xFFFFFFFF)
        return GW.do_import(self, nm)


def world(e, c):
    e.case = c
    e.objs = {}
    obj = e.alloc(0x40)
    e.uc.mem_write(obj, bytes([0xCD]) * 0x40)
    e.objs[obj] = 'limiter'
    lim = c['limits']
    for off, v in zip((0x10, 0x14, 0x18, 0x1C, 0x20, 0x24, 0x28), lim[:7]):
        e.write32(obj + off, v)
    e.write32(obj + 0x2C, f32(lim[7]))
    e.write32(obj, c['points'])
    e.uc.mem_write(obj + 0x30, bytes([c['blimited']]))
    n = len(c['items'])
    data = e.alloc(8 * (n + 4))
    e.uc.mem_write(data, bytes(8 * (n + 4)))
    for k, (txt, pts) in enumerate(c['items']):
        s = e.alloc(0x14)
        e.uc.mem_write(s, bytes(0x14))
        e.write32(s, e.put_wstr(txt))
        e.write32(s + 4, len(txt))
        e.objs[s] = 'text%d' % k
        e.write32(data + 8 * k, s)
        e.write32(data + 8 * k + 4, pts)
    e.write32(obj + 4, data); e.write32(obj + 8, n); e.write32(obj + 12, n + 4)
    return obj


def run(e, A, c):
    mark = len(e.log)
    obj = world(e, c)
    va, nargs = A[c['f']]
    args = []
    if c['f'] == 'addline':
        txt = e.put_wstr(c['text']) if c['text'] is not None else 0
        args = [c['length'], txt]
    elif c['f'] == 'setlimits':
        l = c['newlim']
        args = [l[0], l[1], l[2], l[3], l[4], l[5], l[6], f32(l[7])]
    e.call(va, this=obj, args=args)
    n = e.read32(obj + 8)
    data = e.read32(obj + 4)
    items = []
    for k in range(min(n, 16)):
        p = e.read32(data + 8 * k)
        items.append((e.objs.get(p, 'new:' + (e.wstr(e.read32(p)) if p > 0x60000000 else '?')), e.rds32(data + 8 * k + 4)))
    words = [e.rds32(obj + o) for o in (0, 8, 0x10, 0x14, 0x18, 0x1C, 0x20, 0x24, 0x28, 0x2C)]
    return dict(log=e.log[mark:], words=words, blimited=bytes(e.uc.mem_read(obj + 0x30, 1)), items=items)


def rand(r, f):
    c = dict(f=f, seed=r.randint(0, 10 ** 6), points=r.choice([0, 100, 5000, 9999, 10000, 10001, 20000, 25000, 30000]),
             blimited=r.randint(0, 1), text=r.choice(TEXTS + [None]), length=r.choice([0, 1, 5, 20, 60, 100, 300]),
             limits=[r.choice([20000, 10000, 5000]), r.choice([10000, 5000, 3000]), 100000, r.choice([200, 100, 5000]), r.choice([60, 10, 200]),
                     r.choice([100, 0, 50]), r.choice([2000, 500, 20000]), r.choice([0.1, 0.5, 0.0, 1.0, 2.0])],
             newlim=[r.randint(0, 99999) for _ in range(7)] + [r.choice([0.1, 0.25, 3.0])])
    c['items'] = [(r.choice(TEXTS), r.choice([0, 100, 10000, 10001, 15000, 30000])) for _ in range(r.choice([0, 1, 2, 3, 4]))]
    return c


if __name__ == '__main__':
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 40
    seed = int(sys.argv[2]) if len(sys.argv) > 2 else 1
    es = LW('stock'); eo = LW('ours')
    lm = log_map()
    es.heap_mark, eo.heap_mark = es.heap_ptr, eo.heap_ptr
    As = {k: (v[0], v[1]) for k, v in FUNCS.items()}
    Ao = {k: (lm[v[0]], v[1]) for k, v in FUNCS.items()}
    r = random.Random(seed)
    total = 0
    for f in FUNCS:
        total += compare(f, es, eo, run, [rand(r, f) for _ in range(n)], As, Ao)
    print('TOTAL problems', total)
