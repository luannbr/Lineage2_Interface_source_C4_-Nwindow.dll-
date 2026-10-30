#!/usr/bin/env python3
"""Exports that kept their decorated name between the C1 and the C2 Engine/Core but pop another number of
argument bytes (`ret N`): a by-value parameter or a hidden pointer changed its size, i.e. a STRUCT CHANGED
SIZE although the mangled name (which has no sizes) did not (User, ItemInfo, PledgeInfo, FL2... by value).

  python tools/ue2_ret_diff.py [--c1 <C1 orig dir>]
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

# NWD_C1_ROOT: a checkout of the C1 project
C1_ORIG = (Path(os.environ["NWD_C1_ROOT"]) / "orig" if os.environ.get("NWD_C1_ROOT") else None)
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)


def rets(d: ev.Dll, name: str) -> set[int] | None:
    va = d.exp[name]
    if not d.is_code(va):
        return None
    b = d.mem[va - d.base:va - d.base + 5]
    if b[:1] == b"\xe9":
        va = va + 5 + struct.unpack("<i", b[1:5])[0]
    out: set[int] = set()
    for i in md.disasm(bytes(d.mem[va - d.base:va - d.base + 4000]), va):
        if i.mnemonic == "int3":
            break
        if i.address != va and i.address in d.names and not d.names[i.address][0].startswith("??_7"):
            break
        if i.mnemonic == "ret":
            out.add(int(i.op_str, 16) if i.op_str else 0)
    return out


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--c1", default=str(C1_ORIG) if C1_ORIG else None, required=C1_ORIG is None, help="default: $NWD_C1_ROOT/orig")
    a = ap.parse_args()
    n = 0
    for dn in ("Core", "Engine"):
        d1, d2 = ev.dll(dn, Path(a.c1)), ev.dll(dn)
        diff = []
        for name in sorted(set(d1.exp) & set(d2.exp)):
            if name.startswith(("??_7", "?exec")) or "@@3" in name or "@@2" in name or "@@0" in name:
                continue
            r1, r2 = rets(d1, name), rets(d2, name)
            if r1 is None or r2 is None or not r1 or not r2 or r1 == r2:
                continue
            diff.append((name, r1, r2))
        und = ev.undname([x[0] for x in diff])
        print(f"== {dn}: {len(diff)} exports with the same name and another `ret N`")
        for (name, r1, r2), u in zip(diff, und):
            print(f"  C1 ret {sorted(r1)} -> C2 ret {sorted(r2)}   {u}")
            n += 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
