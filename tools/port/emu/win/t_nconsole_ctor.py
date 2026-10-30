# Differential test of NConsoleWnd's constructor body 0x100824F0 (ConstructConsole) and destructor body 0x10082900 (DestroyConsole)
# (stock vs ours under unicorn). The object is filled with a poison pattern; the constructor runs and the WHOLE object is compared
# (the pointers into the stock image of our side are brought back to the stock's numbering), together with the order of the engine
# imports called (UL2ConsoleWnd's constructor, FArray::Realloc, ...). The destructor runs on a console whose arrays hold random
# counts and element data; the object after it and the imports called (ConditionalDestroy, FArray::Remove, ~FArray,
# ~UL2ConsoleWnd, the FString destructors of the manor entries) are compared.
#   python t_nconsole_ctor.py [cases] [seed]
import sys, os, random, struct
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from gw import *
import sa

SIZE = 0x3CA4                       # NW_ASSERT_SIZE(NConsoleWnd)
FUNCS = {'ctor': 0x100824f0, 'dtor': 0x10082900}
ARRAYS = {'sysmsg': (0x3C10, 8), 'manor': (0x3C20, 0x10), 'timers': (0x3C30, 0x14),
          'mode0': (0x3BB8, 4), 'mode1': (0x3BC4, 4), 'mode2': (0x3BD0, 4)}   # (offset, element size)
stubs.IMPORT_POP.update({'??0UL2ConsoleWnd@@QAE@XZ': 0, '??1UL2ConsoleWnd@@UAE@XZ': 0, '?ConditionalDestroy@UObject@@QAEHXZ': 0})


# the stock's vtables of the console (the object's +0 and the FExec one at +0x34) and the standalone symbols of its own
STOCK_VT = {'??_7NConsoleWnd@@6BUObject@@@': 0x101B2428, '??_7NConsoleWnd@@6BFExec@@@': 0x101B2424}


class CW(GW):
    def __init__(self, kind):
        GW.__init__(self, kind, {}, {})
        # standalone: the vtable pointers the real constructor stores are the standalone's own objects, named by the stock vtable they stand for
        self.sa_vt = {self.symbols[s]: va for s, va in STOCK_VT.items()} if (kind == 'ours' and sa.ENABLED) else {}

    def norm(self, v):
        if v is None:
            return v
        if v in self.sa_vt:
            return self.sa_vt[v]
        if self.kind == 'ours' and NWBASE <= v < NWBASE + 0x400000:
            return v - NWBASE + 0x10000000      # a pointer into the stock image, in the stock's numbering
        return v

    def do_import(self, nm):
        t = self.this()
        self.log.append(('imp', nm.split('@')[0], 'this' if t == self.obj else self.norm(t)))
        if nm == '??0UL2ConsoleWnd@@QAE@XZ':
            return t
        if nm in ('??1UL2ConsoleWnd@@UAE@XZ',):
            return None
        if nm == '?ConditionalDestroy@UObject@@QAEHXZ':
            return 1
        return GW.do_import(self, nm)


def snapshot(e, obj, size):
    mem = bytes(e.uc.mem_read(obj, size))
    words = struct.unpack('<%dI' % (size // 4), mem)
    return [e.norm(w) for w in words]


def build_dtor_world(e, obj, rnd):
    e.uc.mem_write(obj, bytes([0xCD]) * SIZE)
    e.uc.mem_write(obj, bytes(0x3BA8))            # the engine's part: zero
    for nm, (off, esz) in ARRAYS.items():
        n = rnd.randint(0, 3)
        data = e.alloc(max(16, n * esz + 16)) if n else 0
        if n:
            e.uc.mem_write(data, bytes(n * esz))
            if nm == 'manor':
                for i in range(n):          # id, then the name's FString: empty or with a buffer
                    if rnd.randint(0, 1):
                        d = e.alloc(32)
                        e.uc.mem_write(d, 'ab\0'.encode('utf-16-le'))
                        e.write32(data + i * esz + 4, d)
                        e.write32(data + i * esz + 8, 3)
                        e.write32(data + i * esz + 12, 3)
        e.write32(obj + off, data)
        e.write32(obj + off + 4, n)
        e.write32(obj + off + 8, n)
    for off in (0x3BA8, 0x3BAC, 0x3BB0, 0x3BB4, 0x3C3C, 0x3C44, 0x3C64):
        e.write32(obj + off, rnd.randint(0, 9))
    e.write32(obj, 0x12345678)
    e.write32(obj + 0x34, 0x23456789)


def run(e, A, c):
    mark = len(e.log)
    obj = e.alloc(SIZE + 16)
    e.obj = obj
    if c['f'] == 'ctor':
        e.uc.mem_write(obj, bytes([0xCD]) * (SIZE + 16))
    else:
        build_dtor_world(e, obj, random.Random(c['seed']))
        e.uc.mem_write(obj + SIZE, bytes([0xCD]) * 16)
    ret = e.call(A[c['f']], this=obj, args=[])
    snap = snapshot(e, obj, SIZE + 16)
    return dict(log=e.log[mark:], obj=snap, ret=('this' if ret == obj else ret) if c['f'] == 'ctor' else 0)


if __name__ == '__main__':
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 20
    seed = int(sys.argv[2]) if len(sys.argv) > 2 else 1
    if sa.ENABLED:
        # the standalone has no ConstructConsole / DestroyConsole (NConsoleWnd.cpp): the constructor is the real NConsoleWnd::NConsoleWnd and the
        # destructor body is what the scalar deleting destructor runs with flags = 0 (the trampoline must exist before the emulators are made)
        ssym = sa.parse_map(sa.SA_MAP)[0]
        SA_FUNCS = {'ctor': ssym['??0NConsoleWnd@@QAE@XZ'], 'dtor': sa.tramp_for(ssym['??_GNConsoleWnd@@UAEPAXI@Z'])}
    es = CW('stock'); eo = CW('ours')
    lm = log_map()
    es.heap_mark, eo.heap_mark = es.heap_ptr, eo.heap_ptr
    As = dict(FUNCS)
    Ao = dict(SA_FUNCS) if sa.ENABLED else {k: lm[v] for k, v in FUNCS.items()}
    r = random.Random(seed)
    total = compare('ctor', es, eo, run, [dict(f='ctor', seed=i) for i in range(2)], As, Ao)
    total += compare('dtor', es, eo, run, [dict(f='dtor', seed=r.randint(0, 1 << 30)) for _ in range(n)], As, Ao)
    print('TOTAL problems', total)
