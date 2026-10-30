# Differential test of C2-only functions: the stock function and ours run side by side under unicorn with the engine imports stubbed in Python (tools/port/emu: emu.py, stubs.py, t_fill.py, t_equip.py, t_num.py). Written by the foundation A2b lane for the tool tip subsystem; see docs/notas/C2_Porte.md.
"""Tiny x86 emulation harness: run one function of the stock NWindow.dll (orig) or of our hook DLL under unicorn
with the engine imports stubbed in Python, and log what it does (stub calls and the memory it writes)."""
import struct, re, sys
import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE, UC_HOOK_MEM_UNMAPPED, UcError
from unicorn.x86_const import *

ROOT = str(__import__('pathlib').Path(__file__).resolve().parents[3])
sys.path.insert(0, __import__('os').path.dirname(__import__('os').path.abspath(__file__)))
import sa
STOCK = ROOT + r'\orig\NWindow.dll'
# `ours` is the hook build by default; NWD_SA=1 (run_suite.py --mode sa) makes it the standalone NWindow.dll (see sa.py)
OURS = sa.SA_DLL if sa.ENABLED else ROOT + r'\build\bin\Release\NWindowDecomp.dll'
OURS_MAP = sa.SA_MAP if sa.ENABLED else ROOT + r'\build\bin\Release\NWindowDecomp.map'
TRAMP_BASE = sa.TRAMP_BASE   # sa mode: `push 0; call ??_G<Class>; ret` stubs that run a destructor body through the scalar deleting destructor

STUB_BASE = 0x7E000000
SENTINEL = 0x7D000000
STACK_TOP = 0x2F100000
HEAP = 0x60000000
FS_BASE = 0x70000000
NWBASE = 0x50000000          # where our DLL's g_nwBase points (stand-in for the stock image's globals)


def u32(x):
    return x & 0xFFFFFFFF


class Emu:
    def __init__(self, kind):
        self.kind = kind
        path = STOCK if kind == 'stock' else OURS
        self.pe = pefile.PE(path)
        self.base = self.pe.OPTIONAL_HEADER.ImageBase
        self.uc = Uc(UC_ARCH_X86, UC_MODE_32)
        self.log = []
        self.stubs = {}        # address -> (name, handler)
        self.heap_ptr = HEAP + 0x01000000
        self.map_image()
        self.uc.mem_map(STACK_TOP - 0x100000, 0x100000)
        self.uc.mem_map(0, 0x1000)
        self.setup_fs()
        self.uc.mem_map(HEAP, 0x04000000)
        self.uc.mem_map(STUB_BASE, 0x10000)
        self.uc.mem_map(SENTINEL, 0x1000)
        if kind == 'ours':
            self.symbols = self.read_map()
            if sa.ENABLED:
                # the standalone has no stock image to stand in for: an access through the hook mode's stand-in region (NWBASE + rva) is
                # translated to the standalone object that holds the same stock address, or fails (sa.SaUc)
                self.uc = sa.SaUc(self.uc, NWBASE)
                self.uc.mem_map(TRAMP_BASE, 0x10000)
                for ta, ga in sa.tramps():
                    # push 0; call ??_G<Class>; xor eax, eax; ret   (a destructor's eax is no result: the scalar deleting destructor returns `this`)
                    self.uc.mem_write(ta, b'\x6A\x00\xE8' + struct.pack('<i', ga - (ta + 7)) + b'\x31\xC0\xC3')
            else:
                self.uc.mem_map(NWBASE, 0x00400000)
                # g_nwBase
                self.write32(self.symbols['_g_nwBase'], NWBASE)
        self.install_imports()
        self.pending = None
        self.uc.hook_add(UC_HOOK_CODE, self.on_code, None, STUB_BASE, STUB_BASE + 0x10000)

    def gptr(self, va):
        """the pointer value a test gives ours for the stock address va (a UClass, a table ...): the stock's own address on the stock side, the
        stand-in region (NWBASE + rva) in the hook build, the object itself in the standalone (sa.gptr)"""
        if self.kind == 'stock':
            return va
        if sa.ENABLED:
            return sa.gptr(self, va)
        return NWBASE + (va - 0x10000000)

    def stockptr(self, v):
        """the stock address that the pointer v (made by ours) stands for: a stand-in address of the hook build, or an object of the standalone"""
        if self.kind == 'ours':
            if sa.ENABLED:
                r = sa.inv(v)
                return r[0] if r else v
            if NWBASE <= v < NWBASE + 0x400000:
                return v - NWBASE + 0x10000000
        return v

    def setup_fs(self):
        # fs has base 0 in unicorn: fs:[0] is linear address 0 (the end of the SEH chain)
        pass

    # ---- image -------------------------------------------------------------------------------
    def map_image(self):
        pe = self.pe
        size = (pe.OPTIONAL_HEADER.SizeOfImage + 0xFFF) & ~0xFFF
        self.uc.mem_map(self.base, size)
        self.uc.mem_write(self.base, pe.header[:pe.OPTIONAL_HEADER.SizeOfHeaders])
        for s in pe.sections:
            data = s.get_data()
            self.uc.mem_write(self.base + s.VirtualAddress, data)

    def read_map(self):
        syms = {}
        txt = open(OURS_MAP, encoding='latin-1').read().split('\n')
        for l in txt:
            m = re.match(r'\s*[0-9a-f]{4}:[0-9a-f]{8}\s+(\S+)\s+([0-9a-f]{8})\s', l)
            if m:
                syms.setdefault(m.group(1), int(m.group(2), 16))
        return syms

    def install_imports(self):
        self.import_names = {}
        n = 0
        for e in self.pe.DIRECTORY_ENTRY_IMPORT:
            for imp in e.imports:
                name = imp.name.decode() if imp.name else 'ord%d' % imp.ordinal
                addr = STUB_BASE + 16 * n
                n += 1
                self.uc.mem_write(addr, b'\xC3')
                self.write32(imp.address, addr)
                self.stubs[addr] = ('imp:' + name, None)
                self.import_names[addr] = name

    def register_function_stub(self, va, name):
        """Replace a function of the image by a stub (the stock code at va / our symbol)."""
        self.stubs[va] = ('fn:' + name, None)

    # ---- memory helpers -----------------------------------------------------------------------
    def read32(self, a):
        return struct.unpack('<I', bytes(self.uc.mem_read(a, 4)))[0]

    def write32(self, a, v):
        self.uc.mem_write(a, struct.pack('<I', u32(v)))

    def rds32(self, a):
        return struct.unpack('<i', bytes(self.uc.mem_read(a, 4)))[0]

    def alloc(self, n):
        a = self.heap_ptr
        self.heap_ptr += (n + 15) & ~15
        return a

    def wstr(self, a, maxn=100000):
        if a == 0:
            return None
        out = []
        for i in range(maxn):
            c = struct.unpack('<H', bytes(self.uc.mem_read(a + 2 * i, 2)))[0]
            if c == 0:
                break
            out.append(chr(c))
        return ''.join(out)

    def put_wstr(self, s):
        a = self.alloc(2 * len(s) + 2)
        self.uc.mem_write(a, s.encode('utf-16-le') + b'\0\0')
        return a

    def write_wstr_at(self, a, s):
        self.uc.mem_write(a, s.encode('utf-16-le') + b'\0\0')

    # FString: {Data, Num, Max}
    def fs_get(self, a):
        d = self.read32(a)
        n = self.read32(a + 4)
        if n == 0:
            return ''
        return self.wstr(d)

    def fs_set(self, a, s):
        if s is None or s == '':
            self.write32(a, 0)
            self.write32(a + 4, 0)
            self.write32(a + 8, 0)
            return
        d = self.put_wstr(s)
        self.write32(a, d)
        self.write32(a + 4, len(s) + 1)
        self.write32(a + 8, len(s) + 1)

    # ---- running --------------------------------------------------------------------------------
    def on_code(self, uc, address, size, user):
        if address in self.stubs:
            self.pending = address
            uc.emu_stop()

    def reg(self, r):
        return self.uc.reg_read(r)

    def stack_arg(self, i):
        return self.read32(self.reg(UC_X86_REG_ESP) + 4 + 4 * i)

    def call(self, va, this=None, args=(), conv='thiscall', retpop=None):
        """Run the function at va (thiscall with `this` in ecx, or cdecl) until it returns."""
        uc = self.uc
        esp = STACK_TOP - 0x1000
        for a in reversed(args):
            esp -= 4
            self.write32(esp, a)
        esp -= 4
        self.write32(esp, SENTINEL)
        uc.reg_write(UC_X86_REG_ESP, esp)
        uc.reg_write(UC_X86_REG_EBP, 0)
        uc.reg_write(UC_X86_REG_ECX, this or 0)
        uc.reg_write(UC_X86_REG_EDX, 0)
        uc.reg_write(UC_X86_REG_EAX, 0)
        uc.reg_write(UC_X86_REG_EBX, 0)
        uc.reg_write(UC_X86_REG_ESI, 0)
        uc.reg_write(UC_X86_REG_EDI, 0)
        self.write32(0, 0xFFFFFFFF)   # fs:[0] = end of the SEH chain
        pc = va
        steps = 0
        while True:
            self.pending = None
            try:
                uc.emu_start(pc, SENTINEL, timeout=0, count=0)
            except UcError as e:
                raise RuntimeError('unicorn error %s at eip=%08x (last stub %s)' % (e, uc.reg_read(UC_X86_REG_EIP), self.log[-3:]))
            if self.pending is None:
                break
            steps += 1
            if steps > 200000:
                raise RuntimeError('too many stub calls')
            addr = self.pending
            name, _ = self.stubs[addr]
            ret = self.dispatch(name)
            # return from the stub: eax = ret; pop the return address and the callee-popped bytes
            esp = self.reg(UC_X86_REG_ESP)
            retaddr = self.read32(esp)
            pop = self.last_pop
            uc.reg_write(UC_X86_REG_ESP, esp + 4 + pop)
            if ret is not None:
                uc.reg_write(UC_X86_REG_EAX, u32(ret))
            pc = retaddr
            if pc == SENTINEL:
                break
        return self.reg(UC_X86_REG_EAX)

    def dispatch(self, name):
        raise NotImplementedError(name)
