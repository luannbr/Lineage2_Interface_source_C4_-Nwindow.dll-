#!/usr/bin/env python3
"""Function starts the auto-analysis missed, for ANY PE (tools/gen_seeds.py is tied to orig/NWindow.dll).
Same two rules: (1) targets of relocations outside .text that point into uncovered .text (vtables,
message maps, EH tables); (2) the first 16-aligned byte after int3/nop padding in each uncovered gap.
usage: gen_seeds.py <dll> <funcs.tsv> <out seeds.txt>      (funcs.tsv from tools/ghidra/NwdFuncs.java)"""
import csv
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from nwd.image import Image  # noqa: E402

dll, tsv, out = sys.argv[1:4]
img = Image(dll)
lo, hi = img.sect_range(".text")
n = hi - lo
cov = bytearray(n)
for r in csv.DictReader(open(tsv, encoding="utf-8"), delimiter="\t"):
    for rg in r["ranges"].split(","):
        a, b = (int(x, 16) for x in rg.split("-"))
        if lo <= a < hi:
            b = min(b, hi - 1)
            cov[a - lo:b - lo + 1] = b"\1" * (b - a + 1)
seeds = set()
for r in img.relocs:
    if img.in_text(r):
        continue
    t = img.u32(r)
    if lo <= t < hi and not cov[t - lo]:
        seeds.add(t)
i = 0
while i < n:
    if cov[i]:
        i += 1
        continue
    j = i
    while j < n and not cov[j]:
        j += 1
    k = i
    while k < j and img.mem[lo - img.base + k] in (0xCC, 0x90):
        k += 1
    if k < j and (lo + k) % 16 == 0 and (lo + k) not in img.reloc_set:
        seeds.add(lo + k)
    i = j
Path(out).write_text("".join("%08x\n" % v for v in sorted(seeds)), encoding="utf-8")
print("%d seeds; uncovered %d of %d bytes of .text" % (len(seeds), cov.count(0), n))
