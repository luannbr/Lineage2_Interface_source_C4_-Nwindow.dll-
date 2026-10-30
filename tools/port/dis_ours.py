#!/usr/bin/env python3
"""Disassemble OUR compiled version of a hooked function next to the stock one.

usage: python tools/port/dis_ours.py <c2_va> [--root DIR] [--stock]   (c2_va hex, e.g. 10025e20)

Our function is found through the smoke log (build/smoke_system/NWindowDecomp.log: `<va> <name> ours=<rva>`)
of the tree given by --root (default: this repo) and read from build/bin/Release/NWindowDecomp.dll. --stock also
prints the stock function from analysis/ghidra/asm/<va>.asm. The listing follows the first `ret` only when the
function is short; use --max to print more instructions."""
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

import pefile
from capstone import CS_ARCH_X86, CS_MODE_32, Cs

ROOT = Path(__file__).resolve().parents[2]


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("va")
    ap.add_argument("--root", type=Path, default=ROOT)
    ap.add_argument("--stock", action="store_true")
    ap.add_argument("--max", type=int, default=400)
    a = ap.parse_args()
    va = int(a.va, 16)
    log = (a.root / "build" / "smoke_system" / "NWindowDecomp.log").read_text(encoding="latin-1")
    m = re.search(r"^\S+\s+%08x\s+(\S.*?)\s+ours=([0-9a-f]+)" % va, log, re.M | re.I) or \
        re.search(r"%08x\s+(\S.*?)\s+ours=([0-9a-f]+)" % va, log, re.I)
    if not m:
        print("no hook at %08x in the smoke log" % va)
        return 1
    name, rva = m.group(1), int(m.group(2), 16)
    pe = pefile.PE(str(a.root / "build" / "bin" / "Release" / "NWindowDecomp.dll"))
    base = pe.OPTIONAL_HEADER.ImageBase
    mem = pe.get_memory_mapped_image()
    md = Cs(CS_ARCH_X86, CS_MODE_32)
    print("== ours: %s (rva %x)" % (name, rva))
    n = 0
    depth_ret = 0
    for ins in md.disasm(bytes(mem[rva:rva + 0x4000]), base + rva):
        print("%08x  %-6s %s" % (ins.address, ins.mnemonic, ins.op_str))
        n += 1
        if ins.mnemonic == "ret":
            depth_ret += 1
            if n < 60 or depth_ret >= 3:
                break
        if n >= a.max:
            break
    if a.stock:
        p = a.root / "analysis" / "ghidra" / "asm" / ("%08x.asm" % va)
        print("== stock: %s" % p.name)
        print(p.read_text(encoding="utf-8", errors="replace"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
