#!/usr/bin/env python3
"""The regions of FL2GameData (the global GL2GameData: the game-data tables) from its COPY constructor,
for both builds: the constructor copies the object piece by piece, so every member region, its element
size and count, and the total size fall out of its code (the same method the C1 project's
tools/engine/fl2gamedata_map.py used on the decompiler's output; here straight from the disassembly).

  rep movsd      `mov ecx, N ; lea esi,[src+OFF] ; lea edi,[dst+OFF] ; rep movsd`   N dwords at OFF
  vector copy    `push dtor ; push ctor ; push COUNT ; push SIZE ; lea ..[OFF] ; call vec_copy`   COUNT x SIZE
  one object     `lea ecx,[this+OFF] ; lea edx,[other+OFF] ; call <copy constructor>`            at OFF

  python tools/ue2_gamedata_regions.py [--c1 <C1 orig dir>]
"""
from __future__ import annotations

import argparse
import struct
import os
import sys
from pathlib import Path

import capstone

sys.path.insert(0, str(Path(__file__).resolve().parent))
import engine_vtables as ev  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
# NWD_C1_ROOT: a checkout of the C1 project
C1_ORIG = (Path(os.environ["NWD_C1_ROOT"]) / "orig" if os.environ.get("NWD_C1_ROOT") else None)
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)


def regions(d: ev.Dll):
    va = d.exp["??0FL2GameData@@QAE@ABV0@@Z"]
    if d.mem[va - d.base:va - d.base + 1] == b"\xe9":
        va = va + 5 + struct.unpack("<i", d.mem[va - d.base + 1:va - d.base + 5])[0]
    ins = []
    for i in md.disasm(bytes(d.mem[va - d.base:va - d.base + 8000]), va):
        ins.append((i.mnemonic, i.op_str))
        if i.mnemonic == "ret":
            break
    out = []
    pushes: list[int] = []
    last_ecx = None
    seen = set()
    for k, (m, o) in enumerate(ins):
        if m == "push" and o.startswith("0x"):
            pushes.append(int(o, 16))
            pushes = pushes[-4:]
        elif m == "mov" and o.startswith("ecx, 0x"):
            last_ecx = int(o.split(", ")[1], 16)
        elif m == "rep movsd":
            # the offset is in the preceding `lea esi, [ebp + OFF]`
            for p in range(k - 1, max(k - 6, 0), -1):
                if ins[p][0] == "lea" and ins[p][1].startswith("esi, [ebp + "):
                    off = int(ins[p][1].split("+")[1].strip(" ]"), 16)
                    if (off, "dwords") not in seen:
                        seen.add((off, "dwords"))
                        out.append((off, 4 * last_ecx, "dwords", last_ecx, 4))
                    break
        elif m == "call" and len(pushes) >= 2:
            # vector copy: the pushes before the lea's are SIZE then COUNT (last two of the four)
            for p in range(k - 1, max(k - 6, 0), -1):
                if ins[p][0] == "lea" and ins[p][1].startswith(("ecx, [ebp + ", "edx, [ebp + ", "eax, [ebp + ")):
                    off = int(ins[p][1].split("+")[1].strip(" ]"), 16)
                    if (off, "x") not in seen:
                        seen.add((off, "x"))
                        out.append((off, None, "call", pushes[-1], pushes[-2]))
                    break
            pushes = []
    return sorted(out)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--c1", default=str(C1_ORIG) if C1_ORIG else None, required=C1_ORIG is None, help="default: $NWD_C1_ROOT/orig")
    a = ap.parse_args()
    for tag, d in (("C1", ev.dll("Engine", Path(a.c1))), ("C2", ev.dll("Engine"))):
        print(f"== {tag}: FL2GameData copy constructor regions")
        for off, size, kind, count, elem in regions(d):
            if kind == "dwords":
                print(f"  +0x{off:05X}  {size:#8x} bytes  {count} dwords")
            else:
                print(f"  +0x{off:05X}  {elem} x {count:#x} (vector copy: count x element size)")
        r = [x for x in regions(d) if x[2] == "dwords"]
        last = max(r)
        print(f"  last block ends at +0x{last[0] + last[1]:X}: the object is that big")
    return 0


if __name__ == "__main__":
    sys.exit(main())
