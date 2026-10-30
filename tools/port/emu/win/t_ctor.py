# Differential test of the constructors NCEditBox, NCChatWnd and NCConsole (x2) (stock vs ours under unicorn): the object is
# filled with a poison pattern, the constructor runs, the whole object is compared (the vtable pointer excepted)
import sys, os, random, struct
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from gw import *

# name: (stock va, ctor arg count, object size)
FUNCS = {'edit': (0x100112c0, 5, 0x2D4), 'chat': (0x1005a670, 0, 0x628), 'console': (0x1005fef0, 0, 0x2A0),
         'console2': (0x100600e0, 1, 0x2A0)}
CALLEES = {
    0x10036ce0: ('NCWndCtor', 0, 1), 0x1002b450: ('FindTexture', 8, 0), 0x1004bb70: ('ParserCtor', 0, 1), 0x10056350: ('LimiterClear', 0, 0),
}
OURS_SYMS = {0x10036ce0: '??0NCWnd@@QAE@XZ', 0x1002b450: '?FindTexture@NCObject@@QAEPAVUTexture@@PBGH@Z',
             0x1004bb70: '??0NCommandParser@@QAE@XZ',
             0x10056350: '?Clear@NChatLimiter@@QAEXXZ'}


class CW(GW):
    def __init__(self, kind):
        syms = {}
        for va, nm in OURS_SYMS.items():
            syms[va] = nm
        GW.__init__(self, kind, CALLEES, syms)
        self.rets['FindTexture'] = lambda e, t, a: 0x7777 + (a[0] and e.wstr(a[0]) and len(e.wstr(a[0])) or 0)
        self.lang = self.alloc(4)


def run(e, A, c):
    mark = len(e.log)
    va, nargs, size = A[c['f']]
    obj = e.alloc(size + 16)
    e.uc.mem_write(obj, bytes([0xCD]) * (size + 16))
    e.objs[obj] = 'obj'
    e.objs[obj + 0x210] = 'parser'
    e.objs[obj + 0x1DC] = 'limiter'
    for nm, v in e.gvars.items():
        pass
    # GLanguageType
    e.set_lang(c['lang'])
    args = []
    if c['f'] == 'edit':
        name = e.put_wstr(c['name']) if c['name'] is not None else 0
        args = [c['type'], name, c['ime'], c['extra'], c['u1c8']]
    elif c['f'] == 'console2':
        args = [0x5151]
    e.call(va, this=obj, args=args)
    mem = bytes(e.uc.mem_read(obj, size + 16))
    words = struct.unpack('<%dI' % ((size + 16) // 4), mem)
    snap = [('vt' if i == 0 else w) for i, w in enumerate(words)]
    return dict(log=e.log[mark:], obj=snap)


def rand(r, f):
    c = dict(f=f, lang=r.choice([0, 1, 2, 3, 4, 5]))
    if f == 'edit':
        c.update(type=r.randint(0, 4), name=r.choice([None, '', 'L2UI.back', 'x']), ime=r.randint(0, 1),
                 extra=r.randint(0, 40), u1c8=r.randint(0, 3))
    return c


if __name__ == '__main__':
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 20
    seed = int(sys.argv[2]) if len(sys.argv) > 2 else 1
    which = sys.argv[3].split(',') if len(sys.argv) > 3 else list(FUNCS)
    es = CW('stock'); eo = CW('ours')
    lm = log_map()
    es.heap_mark, eo.heap_mark = es.heap_ptr, eo.heap_ptr
    As = {k: (v[0], v[1], v[2]) for k, v in FUNCS.items()}
    Ao = {k: (lm[v[0]], v[1], v[2]) for k, v in FUNCS.items()}
    r = random.Random(seed)
    total = 0
    for f in which:
        cases = [rand(r, f) for _ in range(n)]
        total += compare(f, es, eo, run, cases, As, Ao)
    print('TOTAL problems', total)
