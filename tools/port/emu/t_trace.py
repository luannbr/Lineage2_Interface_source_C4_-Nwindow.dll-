"""Generic effect comparison of one function, stock against ours, on a dirty object: both are run on the same random bytes and the
object (plus the imports the function calls, with `this` and the arguments that point into the object made relative) must come out the same.
For constructors, destructors and small field copiers; imports are no-ops that return `this`/0 (nothing they would do is compared).

usage: python tools/port/emu/t_trace.py <stock va> <ours symbol prefix> <object size> [--args N] [--vtable] [--cases K] [--seed S]
         [--ptrs i,j] [--skip lo-hi,...]
  --args N     the function takes N stack arguments (after `this` in ecx); each is a random dword, or with --ptrs a pointer to a second
               random object of the same size (the listed argument indexes, e.g. 0 for a copy source)
  --vtable     the first dword of the object is a vtable pointer: not compared
  --skip       byte ranges (hex offsets) not compared (e.g. a sub-object built by a stubbed constructor)
needs build/bin/Release/NWindowDecomp.dll + .map (or NWD_SA=1 for the standalone). Prints DIFF lines and a result line `t_trace <prefix> cases N  differ D  raised 0`."""
import sys, os, random, struct, argparse
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from emu import *
from stubs import IMPORT_POP


class TraceEmu(Emu):
    def __init__(self, kind, va_or_prefix, size):
        super().__init__(kind)
        self.last_pop = 0
        self.size = size
        self.calls = []
        if kind == 'stock':
            self.fn = va_or_prefix
        else:
            c = [v for s, v in self.symbols.items() if s.startswith(va_or_prefix)]
            if len(c) != 1:
                raise SystemExit('ours symbol %s: %s' % (va_or_prefix, c))
            self.fn = c[0]
        self.obj = 0
        self.other = 0

    def rel(self, v):
        for base in (self.obj, self.other):
            if base and base <= v < base + self.size:
                return ('obj' if base == self.obj else 'other', v - base)
        return v

    def dispatch(self, name):
        kind, nm = name.split(':', 1)
        assert kind == 'imp', name
        self.last_pop = IMPORT_POP.get(nm, 0)
        arg0 = self.rel(self.read32(self.reg(UC_X86_REG_ESP) + 4)) if self.last_pop >= 4 else None
        self.calls.append((nm, self.rel(self.reg(UC_X86_REG_ECX)), arg0))
        return self.reg(UC_X86_REG_ECX)


def dirty(r, n):
    return bytes(r.getrandbits(8) for _ in range(n))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('va')
    ap.add_argument('prefix')
    ap.add_argument('size')
    ap.add_argument('--args', type=int, default=0)
    ap.add_argument('--vtable', action='store_true')
    ap.add_argument('--cases', type=int, default=20)
    ap.add_argument('--seed', type=int, default=1)
    ap.add_argument('--ptrs', default='')
    ap.add_argument('--skip', default='')
    a = ap.parse_args()
    size = int(a.size, 0)
    ptrs = set(int(x) for x in a.ptrs.split(',') if x != '')
    skip = []
    for part in a.skip.split(','):
        if part:
            lo, hi = part.split('-')
            skip.append((int(lo, 16), int(hi, 16)))
    if a.vtable:
        skip.append((0, 4))
    S, O = TraceEmu('stock', int(a.va, 16), size), TraceEmu('ours', a.prefix, size)
    r = random.Random(a.seed)
    bad = 0
    for c in range(a.cases):
        data = dirty(r, size)
        other = dirty(r, size)
        argv = [r.getrandbits(32) & 0x7FFFFFFF for _ in range(a.args)]
        res = []
        for e in (S, O):
            e.calls = []
            e.obj = e.alloc(size)
            e.other = e.alloc(size)
            e.uc.mem_write(e.obj, data)
            e.uc.mem_write(e.other, other)
            args = [e.other if i in ptrs else argv[i] for i in range(a.args)]
            ret = e.call(e.fn, this=e.obj, args=args)
            final = bytes(e.uc.mem_read(e.obj, size))
            res.append((final, e.calls, e.rel(ret), bytes(e.uc.mem_read(e.other, size))))
        fa, fb = res[0][0], res[1][0]
        offs = [i for i in range(size) if fa[i] != fb[i] and not any(lo <= i < hi for lo, hi in skip)]
        if offs:
            bad += 1
            ranges = sorted(set(i & ~3 for i in offs))
            print('DIFF case %d: object bytes differ at %s' % (c, ' '.join('+0x%X' % o for o in ranges[:12])))
        if res[0][1] != res[1][1]:
            bad += 1
            print('DIFF case %d: import calls differ:\n  stock %s\n  ours  %s' % (c, res[0][1][:6], res[1][1][:6]))
        if res[0][2] != res[1][2]:
            bad += 1
            print('DIFF case %d: return value stock %s ours %s' % (c, res[0][2], res[1][2]))
        if res[0][3] != res[1][3]:
            bad += 1
            print('DIFF case %d: the second object (an argument) differs' % c)
        if bad > 5:
            break
    print('t_trace %s cases %d  differ %d  raised 0' % (a.prefix, a.cases, bad))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
