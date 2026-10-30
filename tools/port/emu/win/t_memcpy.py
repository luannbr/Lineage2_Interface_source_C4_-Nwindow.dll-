# Differential test of appMemcpy (stock 0x10001CB0, cdecl: Dest, Src, Count) against ours (NAppMemcpy: the C runtime's memcpy), stock vs ours
# under unicorn. The stock one has two paths (the global GIsSSE the image imports: 1 = MMX blocks with a prefetching loop, a non-temporal loop for
# more than 32 KB and a movsb sled for the tail; 0 = rep movsd / movsb): both are run, over the sizes around every threshold of the code
# (0x40, the sled's 8-byte alignment, 0x8000, 0x10000, the 0x400 / 0xC50 block counts) and over the 8 alignments of source and destination.
# Compared: the destination, the bytes around it (a guard of 0x20 on both sides must stay as it was), the callee-saved registers (ebx, esi, edi,
# ebp: set to values that must come back) and esp (cdecl: the caller pops).
#   python t_memcpy.py [random_cases] [seed]
import sys, os, random, struct
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
EMU = os.path.dirname(HERE)
sys.path.insert(0, EMU)
from emu import *
import e4
from e4 import log_map
import sa
from unicorn.x86_const import *

VA = 0x10001CB0
GUARD = 0x40
BIG = 0x30000


class MC(Emu):
    def __init__(self, kind):
        Emu.__init__(self, kind)
        self.last_pop = 0
        self.sse = self.alloc(16)
        for ee in self.pe.DIRECTORY_ENTRY_IMPORT:
            for imp in ee.imports:
                if imp.name and imp.name.decode() == '?GIsSSE@@3HA':
                    self.write32(imp.address, self.sse)
        self.src = self.alloc(BIG + 64)
        self.dst = self.alloc(BIG + 64 + 2 * GUARD)
        self.dispatch_calls = 0

    def dispatch(self, name):
        raise RuntimeError('unexpected stub call %s' % name)

    def copy(self, va, sse, sbase, dbase, n, data, guard):
        self.write32(self.sse, sse)
        self.uc.mem_write(sbase, data)
        self.uc.mem_write(self.dst, guard)
        uc = self.uc
        esp = STACK_TOP - 0x1000
        for a in (n, sbase, dbase):
            esp -= 4
            self.write32(esp, a)
        esp -= 4
        self.write32(esp, SENTINEL)
        uc.reg_write(UC_X86_REG_ESP, esp)
        for reg, v in ((UC_X86_REG_EBX, 0x11111111), (UC_X86_REG_ESI, 0x22222222), (UC_X86_REG_EDI, 0x33333333), (UC_X86_REG_EBP, 0x44444444),
                       (UC_X86_REG_ECX, 0), (UC_X86_REG_EDX, 0), (UC_X86_REG_EAX, 0)):
            uc.reg_write(reg, v)
        self.write32(0, 0xFFFFFFFF)
        uc.emu_start(va, SENTINEL, timeout=0, count=0)
        regs = tuple(uc.reg_read(r) for r in (UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP, UC_X86_REG_ESP))
        return bytes(uc.mem_read(self.dst, BIG + 64 + 2 * GUARD)), regs


def sizes(r, extra):
    base = [0, 1, 2, 3, 4, 7, 8, 9, 15, 16, 31, 32, 33, 63, 64, 65, 66, 100, 127, 128, 129, 255, 256, 1000, 4095, 4096, 4097, 0x7FFF, 0x8000, 0x8001,
            0xBFFF, 0xC000, 0xFFFF, 0x10000, 0x10001, 0x12345, 0x1FFFF, 0x20000, 0x2FFF0]
    return base + [r.randint(0, 0x20000) for _ in range(extra)] + [r.randint(0, 300) for _ in range(extra)]


def main():
    extra = int(sys.argv[1]) if len(sys.argv) > 1 else 20
    seed = int(sys.argv[2]) if len(sys.argv) > 2 else 1
    es = MC('stock')
    eo = MC('ours')
    try:
        va_o = log_map()[VA]
    except sa.NoStandaloneFunction as ex:
        # the standalone has no appMemcpy of its own: every caller copies with the C runtime's memcpy (NLeftHelpers.cpp NAppMemcpy is that call
        # in the hook build; NCPawnViewrWnd.cpp StockMemcpy). The stock appMemcpy is therefore compared with the memcpy the standalone links.
        va_o = eo.symbols['_memcpy']
        print('NOTE %s; comparing with the linked C runtime memcpy at %08x' % (ex, va_o))
    r = random.Random(seed)
    bad = 0
    total = 0
    for n in sizes(r, extra):
        for sse in (1, 0):
            so, do = r.randint(0, 7), r.randint(0, 7)
            data = r.randbytes(n + 16)
            guard = r.randbytes(BIG + 64 + 2 * GUARD)
            # the destination block sits GUARD bytes into the buffer, at a chosen misalignment (the stock and ours have the same heap layout)
            res = []
            for e, va in ((es, VA), (eo, va_o)):
                sbase = e.src + so
                dbase = e.dst + GUARD + do
                mem, regs = e.copy(va, sse, sbase, dbase, n, data, guard)
                res.append((mem, regs, dbase - e.dst))
            total += 1
            (ma, ra, da), (mb, rb, db) = res
            want = bytearray(guard)
            want[da:da + n] = data[:n]
            ok = ma == mb == bytes(want) and ra == rb and ra[:4] == (0x11111111, 0x22222222, 0x33333333, 0x44444444)
            if not ok:
                bad += 1
                if bad <= 5:
                    first = next((k for k in range(len(ma)) if ma[k] != mb[k]), None)
                    print('--- n=%d sse=%d so=%d do=%d: stock==want %s ours==want %s regs %s %s first diff %s' % (
                        n, sse, so, do, ma == bytes(want), mb == bytes(want), ra, rb, first))
    print('appMemcpy: %d runs, %d differ' % (total, bad))
    print('appMemcpy cases %d differ %d raised 0' % (total, bad))      # the line the suite runner counts
    sys.exit(1 if bad else 0)


if __name__ == '__main__':
    main()
