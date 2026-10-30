#!/usr/bin/env python3
"""Who references a stock address: direct call/jmp (E8/E9 rel32) and absolute references
(relocated dwords: vtable slots, pushed function pointers, instruction operands such as
[0x1012d7bc]). Each hit is shown with the database function that contains it.

  python tools/xrefs.py 10020dd0 10020de0 1012d7bc
"""
from __future__ import annotations

import bisect
import csv
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from nwd.image import Image  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]


def load_functions():
    starts, rows = [], []
    with open(ROOT / "db" / "functions.csv", newline="", encoding="utf-8") as f:
        for r in csv.DictReader(f):
            starts.append(int(r["va"], 16))
            rows.append(r)
    order = sorted(range(len(starts)), key=starts.__getitem__)
    return [starts[i] for i in order], [rows[i] for i in order]


def owner(starts, rows, va):
    i = bisect.bisect_right(starts, va) - 1
    if i < 0:
        return "?"
    # EH stubs and fragments sit inside their parent's range: look back for a function that
    # actually contains va before settling for the nearest start
    for j in range(i, max(i - 8, -1), -1):
        if va < starts[j] + max(int(rows[j]["size"]), 1):
            return f"{rows[j]['name']}+{va - starts[j]:x}"
    return f"after {rows[i]['name']}+{va - starts[i]:x}"


def main(argv):
    targets = [int(a, 16) for a in argv]
    if not targets:
        print(__doc__)
        return 1
    img = Image()
    starts, rows = load_functions()
    text_lo, text_hi = img.sect_range(".text")
    text = img.read(text_lo, text_hi - text_lo)
    relocs = img.relocs
    for t in targets:
        print(f"== {t:08x} {owner(starts, rows, t)}")
        hits = []
        # direct rel32 call / jmp
        for op, kind in ((0xE8, "call"), (0xE9, "jmp")):
            pos = text.find(bytes([op]))
            while pos != -1 and pos + 5 <= len(text):
                va = text_lo + pos
                rel = struct.unpack_from("<i", text, pos + 1)[0]
                if va + 5 + rel == t:
                    hits.append((va, kind))
                pos = text.find(bytes([op]), pos + 1)
        # absolute references through relocated dwords
        for r in relocs:
            if img.u32(r) == t:
                hits.append((r, "abs"))
        for va, kind in sorted(set(hits)):
            where = owner(starts, rows, va) if text_lo <= va < text_hi else "data"
            print(f"  {va:08x} {kind:5} {where}")
        if not hits:
            print("  (none)")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
