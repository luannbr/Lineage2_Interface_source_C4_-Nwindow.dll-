# Differential test of NCStatusBarCtrl::StartAnim (0x1010E3E0, thiscall, ret 0x10: Value, Max, Time as a double) against the stock (stock vs
# ours under unicorn): the x87 arithmetic of the strip's scale / amount / step / rate and the time stamp. The control is a 0x264-byte
# buffer filled with a marker; every byte of it must match except the time stamp (+0x238: appSeconds, which reads the time stamp counter
# of the host), which is only checked to be 2^24 plus a small number of seconds (GSecondsPerCycle is 1e-12 here).
#   python t_statusbar.py [cases] [seed]
import sys, os, random, struct
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
EMU = os.path.dirname(HERE)
sys.path.insert(0, EMU)
from emu import *
import e4
from e4 import log_map

VA = 0x1010E3E0
SIZE = 0x264
STAMP = 0x238


class SB(Emu):
    def __init__(self, kind):
        Emu.__init__(self, kind)
        self.last_pop = 0
        self.spc = self.alloc(16)
        self.uc.mem_write(self.spc, struct.pack('<d', 1e-12))
        for ee in self.pe.DIRECTORY_ENTRY_IMPORT:
            for imp in ee.imports:
                if imp.name and imp.name.decode() == '?GSecondsPerCycle@@3NA':
                    self.write32(imp.address, self.spc)

    def dispatch(self, name):
        raise RuntimeError('unexpected stub call %s' % name)


def rand_case(r):
    mx = r.choice([0, 1, 2, 3, 100, 999, 1500, 4321, 99999, 1234567, -7, -2, 0x7FFFFFFF])
    val = r.choice([0, 1, mx, mx // 2, mx // 3, mx * 2, -5, 12345, r.randrange(-100000, 100000), r.randrange(0, 0x7FFFFFFF)])
    t = r.choice([0.0, 0.5, 1.0, 2.0, 0.333, 3.75, 1e-3, 120.0, 1e6, -1.5, r.random() * 10, r.uniform(-50, 50)])
    return dict(val=val, mx=mx, time=t)


def run(e, c):
    obj = e.alloc(SIZE)
    e.uc.mem_write(obj, bytes([0xA5]) * SIZE)
    lo, hi = struct.unpack('<II', struct.pack('<d', c['time']))
    e.call(e.va, this=obj, args=[c['val'] & 0xFFFFFFFF, c['mx'] & 0xFFFFFFFF, lo, hi])
    data = bytes(e.uc.mem_read(obj, SIZE))
    stamp = struct.unpack('<d', data[STAMP:STAMP + 8])[0]
    return data[:STAMP] + data[STAMP + 8:], stamp


def main():
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 300
    seed = int(sys.argv[2]) if len(sys.argv) > 2 else 1
    es = SB('stock')
    eo = SB('ours')
    es.va = VA
    eo.va = log_map()[VA]
    r = random.Random(seed)
    bad = 0
    for i in range(n):
        c = rand_case(r)
        a, sa = run(es, c)
        b, sb = run(eo, c)
        stamp_ok = all(16777216.0 <= s < 16777216.0 + 1e9 for s in (sa, sb)) and abs(sa - sb) < 1e6
        if a != b or not stamp_ok:
            bad += 1
            if bad <= 5:
                diff = [(k, a[k], b[k]) for k in range(len(a)) if a[k] != b[k]][:6]
                print('--- case', i, c, 'stamps', sa, sb, 'diff bytes', diff)
    print('StartAnim: %d cases, %d differ' % (n, bad))
    print('StartAnim cases %d differ %d raised 0' % (n, bad))      # the line the suite runner counts
    sys.exit(1 if bad else 0)


if __name__ == '__main__':
    main()
