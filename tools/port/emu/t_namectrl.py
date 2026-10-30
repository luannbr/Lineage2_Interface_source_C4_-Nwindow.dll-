"""NCNameCtrl's constructor, stock (0x1002D260) against ours, on dirty objects: the bytes of the object must come out the same.

The stock builds NControl's fields inline (ID, DisableTexture, Caption[0], NotifyID) where ours calls NControl's constructor; the
NToolTipInfo at +0x220 is built by the stock constructor 0x10002D90 on both sides (ours reaches it with StockCallAt through
g_nwBase, which this harness has no stock image behind: the call is a `ret` and the 0x80 bytes of the tool tip are not compared).
The vtable pointer (+0) is not compared either.

usage: python tools/port/emu/t_namectrl.py [cases] [seed]   (needs build/bin/Release/NWindowDecomp.dll + .map, or NWD_SA=1 for the standalone)"""
import sys, os, random
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import t_trace as T
from emu import *

SIZE = 0x2A4
cases = int(sys.argv[1]) if len(sys.argv) > 1 else 30
bad = 0
r = random.Random(int(sys.argv[2]) if len(sys.argv) > 2 else 7)
# one emulator per side for all the cases (each case gets a fresh object in the emulated heap)
emus = []
for kind, key in (('stock', 0x1002D260), ('ours', '??0NCNameCtrl@@QAE@XZ')):
    e = T.TraceEmu(kind, key, SIZE)
    if kind == 'ours':
        # the stock NToolTipInfo constructor (0x10002D90) is a `ret` for ours (hook build: through g_nwBase; standalone: the function itself,
        # when it is not inlined, and then the 0x80 bytes of the tool tip are skipped below anyway)
        e.uc.mem_write(NWBASE + 0x2D90, bytes([0xC3]))
    emus.append(e)
for c in range(cases):
    data = T.dirty(r, SIZE)
    res = []
    for e in emus:
        e.obj = e.alloc(SIZE)
        e.other = e.alloc(SIZE)
        e.uc.mem_write(e.obj, data)
        e.call(e.fn, this=e.obj, args=[])
        res.append(bytes(e.uc.mem_read(e.obj, SIZE)))
    a, b = res
    offs = [i for i in range(SIZE) if a[i] != b[i] and not (0 <= i < 4) and not (0x220 <= i < 0x2A0)]
    if offs:
        bad += 1
        print('DIFF case', c, ' '.join('+0x%X' % (o & ~3) for o in sorted(set(offs))[:12]))
print('t_namectrl cases %d  differ %d  raised 0' % (cases, bad))
sys.exit(1 if bad else 0)
