"""Read-only model of the reference image (orig/NWindow.dll): sections, imports, exports,
relocations, strings and a capstone disassembler. Every analysis tool builds on this."""
from __future__ import annotations

import struct
from functools import cached_property
from pathlib import Path

import pefile
from capstone import CS_ARCH_X86, CS_MODE_32, Cs

ROOT = Path(__file__).resolve().parents[2]
ORIG = ROOT / "orig" / "NWindow.dll"

# md5 of the stock C4 NWindow.dll this project decompiles (TimeDateStamp 0x43C73652 = 13/01/2006,
# 2,732,052 bytes, ImageBase 0x10000000; the "updated" Oct/2005 client's system folder). The C2 build
# (md5 0111f657..., 0x4227239B), the April C3 build (3df64c9d..., 0x42723245) and the C4 one are not it.
STOCK_MD5 = "afab3ae3fa4f85f7f4c323715e5155e3"
STOCK_TIMEDATESTAMP = 0x43C73652


class Image:
    def __init__(self, path: Path | str = ORIG):
        self.path = Path(path)
        self.pe = pefile.PE(str(self.path))
        self.base = self.pe.OPTIONAL_HEADER.ImageBase
        self.mem = self.pe.get_memory_mapped_image()
        self.md = Cs(CS_ARCH_X86, CS_MODE_32)
        self.md.detail = False

    # ---- sections -------------------------------------------------------------------------
    def section(self, va: int):
        rva = va - self.base
        for s in self.pe.sections:
            if s.VirtualAddress <= rva < s.VirtualAddress + max(s.Misc_VirtualSize, s.SizeOfRawData):
                return s.Name.rstrip(b"\0").decode()
        return None

    def sect_range(self, name: str) -> tuple[int, int]:
        for s in self.pe.sections:
            if s.Name.rstrip(b"\0").decode() == name:
                return self.base + s.VirtualAddress, self.base + s.VirtualAddress + s.Misc_VirtualSize
        raise KeyError(name)

    def in_text(self, va: int) -> bool:
        lo, hi = self.sect_range(".text")
        return lo <= va < hi

    # ---- raw reads ------------------------------------------------------------------------
    def read(self, va: int, n: int) -> bytes:
        o = va - self.base
        return self.mem[o:o + n]

    def u32(self, va: int) -> int:
        return struct.unpack_from("<I", self.mem, va - self.base)[0]

    def wstr(self, va: int, maxlen: int = 512) -> str | None:
        o = va - self.base
        out = []
        for i in range(maxlen):
            c = struct.unpack_from("<H", self.mem, o + 2 * i)[0]
            if c == 0:
                return "".join(out)
            out.append(chr(c))
        return None

    def astr(self, va: int, maxlen: int = 512) -> str | None:
        o = va - self.base
        e = self.mem.find(b"\0", o, o + maxlen)
        return None if e < 0 else self.mem[o:e].decode("latin-1")

    # ---- imports / exports ----------------------------------------------------------------
    @cached_property
    def imports(self) -> dict[int, tuple[str, str]]:
        """IAT slot VA -> (dll, decorated name)."""
        out = {}
        for e in self.pe.DIRECTORY_ENTRY_IMPORT:
            for i in e.imports:
                out[i.address] = (e.dll.decode(), (i.name or b"#%d" % i.ordinal).decode())
        return out

    def import_slot(self, name: str) -> int:
        for va, (_, n) in self.imports.items():
            if n == name:
                return va
        raise KeyError(name)

    @cached_property
    def exports(self) -> dict[str, int]:
        """export name -> VA."""
        return {s.name.decode(): self.base + s.address for s in self.pe.DIRECTORY_ENTRY_EXPORT.symbols}

    # ---- relocations ----------------------------------------------------------------------
    @cached_property
    def relocs(self) -> list[int]:
        """VAs of every HIGHLOW fixup (each one holds an absolute address)."""
        out = []
        for blk in self.pe.DIRECTORY_ENTRY_BASERELOC:
            for e in blk.entries:
                if e.type == 3:
                    out.append(self.base + e.rva)
        out.sort()
        return out

    @cached_property
    def reloc_set(self) -> set[int]:
        return set(self.relocs)

    # ---- disassembly ----------------------------------------------------------------------
    def disasm(self, va: int, size: int = 0x400, stop_at_ret: bool = True, limit: int = 4000):
        n = 0
        for ins in self.md.disasm(self.read(va, size), va):
            yield ins
            n += 1
            if n >= limit or (stop_at_ret and ins.mnemonic in ("ret", "retn")):
                return

    def import_name_in(self, op_str: str) -> str | None:
        """Name of the import an operand like 'dword ptr [0x100d71c4]' refers to."""
        import re

        for m in re.findall(r"0x[0-9a-f]{7,8}", op_str):
            v = int(m, 16)
            if v in self.imports:
                return self.imports[v][1]
        return None


# md5 of the C1 NWindow.dll (TimeDateStamp 0x40DBD452): the image the hand-written tables of
# tools/gen_globals.py (SPEC) and tools/difftest/ (cases that call stock functions by address) and
# tests/golden/ were made for. They mean nothing on the C2 image: those tools refuse to run on it
# (require_c1_tables) until they are redone for C2 (docs/PLANO_C2.md, phase 5).
C1_MD5 = "2dacb6ebcbb3bbe22cec2b296231dc08"


def require_c1_tables(tool: str, force: bool = False) -> None:
    """Exit 2 with an explanation when orig/NWindow.dll is not the C1 build."""
    import hashlib
    import sys

    md5 = hashlib.md5(ORIG.read_bytes()).hexdigest() if ORIG.exists() else None
    if md5 == C1_MD5 or force:
        return
    print(f"{tool}: its tables are written for the C1 image (md5 {C1_MD5}); orig/NWindow.dll is "
          f"{md5} ({'the C2 build' if md5 == STOCK_MD5 else 'unknown'}). They have to be redone for C2 "
          "first (docs/PLANO_C2.md, phase 5); --force runs it anyway.")
    sys.exit(2)
