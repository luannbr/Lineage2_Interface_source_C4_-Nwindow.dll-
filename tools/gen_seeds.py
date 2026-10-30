#!/usr/bin/env python3
"""Function starts Ghidra's auto-analysis misses.

Ghidra only finds code reached by direct calls; methods reached only through vtables, message
maps or other function-pointer tables stay undisassembled (~290 KB of .text on the first run).
Seeds, in order of confidence:
  1. targets of relocations that live OUTSIDE .text (vtables, message maps, EH tables, data)
     and point into .text - jump tables live inside .text and are excluded on purpose;
  2. the first byte after int3/nop padding in every uncovered .text gap (16-aligned).
Writes analysis/seeds.txt (one hex VA per line) for tools/ghidra/NwdSeed.java.
"""
from __future__ import annotations

import csv
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from nwd.image import ROOT, Image  # noqa: E402


def main() -> int:
    img = Image()
    lo, hi = img.sect_range(".text")
    tsv = ROOT / "analysis" / "ghidra" / "functions.tsv"
    covered = bytearray(hi - lo)
    entries = set()
    if tsv.exists():
        for r in csv.DictReader(tsv.open(encoding="utf-8"), delimiter="\t"):
            entries.add(int(r["va"], 16))
            for rg in r["ranges"].split(","):
                a, b = (int(x, 16) for x in rg.split("-"))
                covered[a - lo:b - lo + 1] = b"\1" * (b - a + 1)

    seeds = set()
    for r in img.relocs:
        if img.in_text(r):
            continue
        t = img.u32(r)
        if lo <= t < hi and not covered[t - lo]:
            seeds.add(t)

    # first instruction after padding in uncovered gaps
    i = 0
    n = hi - lo
    while i < n:
        if covered[i]:
            i += 1
            continue
        j = i
        while j < n and not covered[j]:
            j += 1
        k = i
        while k < j and img.mem[lo - img.base + k] in (0xCC, 0x90):
            k += 1
        # a relocated dword right after the padding is a switch jump table, not code
        if k < j and (lo + k) % 16 == 0 and (lo + k) not in img.reloc_set:
            seeds.add(lo + k)
        i = j

    out = ROOT / "analysis" / "seeds.txt"
    out.write_text("".join(f"{v:08x}\n" for v in sorted(seeds)), encoding="utf-8")
    print(f"{len(seeds)} seeds -> {out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
