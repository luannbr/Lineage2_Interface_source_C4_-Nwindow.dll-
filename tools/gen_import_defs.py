#!/usr/bin/env python3
"""Module-definition files for the engine DLLs NWindow links against.

Writes third_party/ue_import/<Dll>.def with every named export of orig/<Dll>.dll; exports that
live outside executable sections are marked DATA (GMalloc, GLog, PrivateStaticClass, vtables...).
CMake turns each .def into an import library with lib.exe, so a wrong prototype in our headers
fails at link time instead of calling the wrong function at run time.
"""
from __future__ import annotations

import struct
import sys
from pathlib import Path

import pefile

ROOT = Path(__file__).resolve().parents[1]
DLLS = ["Core", "Engine"]


def read_exports(pe):
    """[(name bytes, rva)] of the whole named export table. pefile stops at 8,192 names (its
    MAX_SYMBOL_EXPORT_COUNT-like cap in the name loop): the Engine has 8,249, so the last 57 names were
    missing from Engine.def until this was read by hand."""
    dd = pe.OPTIONAL_HEADER.DATA_DIRECTORY[0]
    hdr = pe.get_data(dd.VirtualAddress, 40)
    (_, _, _, _, _, _base, n_funcs, n_names, a_funcs, a_names, a_ords) = struct.unpack("<IIHHIIIIIII", hdr)
    funcs = struct.unpack("<%dI" % n_funcs, pe.get_data(a_funcs, 4 * n_funcs))
    names = struct.unpack("<%dI" % n_names, pe.get_data(a_names, 4 * n_names))
    ords = struct.unpack("<%dH" % n_names, pe.get_data(a_ords, 2 * n_names))
    return [(pe.get_string_at_rva(n), funcs[o]) for n, o in zip(names, ords)]


def main() -> int:
    out_dir = ROOT / "third_party" / "ue_import"
    out_dir.mkdir(parents=True, exist_ok=True)
    for name in DLLS:
        pe = pefile.PE(str(ROOT / "orig" / f"{name}.dll"))
        exe = [(s.VirtualAddress, s.VirtualAddress + max(s.Misc_VirtualSize, s.SizeOfRawData))
               for s in pe.sections if s.Characteristics & 0x20000000]
        lines = [f"LIBRARY {name}.dll", "EXPORTS"]
        n_data = 0
        for sym_name, rva in sorted(read_exports(pe)):
            is_code = any(a <= rva < b for a, b in exe)
            n_data += not is_code
            lines.append(f"  {sym_name.decode()}{'' if is_code else ' DATA'}")
        (out_dir / f"{name}.def").write_text("\n".join(lines) + "\n", encoding="ascii")
        print(f"{name}.def: {len(lines) - 2} exports ({n_data} DATA)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
