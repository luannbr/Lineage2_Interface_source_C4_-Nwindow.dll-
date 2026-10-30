#!/usr/bin/env python3
"""Every `Addr(0xVA)` / `StockCallAt<R>(0xVA, ...)` / `StockCall` / `NewStock` literal in src/ that points into .text must be the entry of a function of db/functions.csv.

The C4 build ran for the first time in the game and died in FL2FontPage at engine start: `nw::StockFread` still called the
C3 address of `_fread` (0x1014DAC0, the middle of another function in the C4 image). The generator remaps the addresses it
writes, but a literal in a hand-written file (src/re/NwHook.h, helpers in a class file) is not remapped by anyone. This judge
reads the literals and compares them with the function table of the image the tree is built for.

usage: python tools/port/check_addrs.py      exit 1 when a literal in .text is not a function entry"""
from __future__ import annotations

import csv
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
TEXT_LO, TEXT_HI = 0x10001000, 0x1019C810       # .text of the C4 image (3,112,980-byte NWindow.dll); data addresses are skipped
LIT = re.compile(r"(?:Addr\(|(?:StockCallAt|StockCall|NewStock)\s*(?:<[^()]*>)?\(\s*)0x(1[0-9a-fA-F]{7})")


def main() -> int:
    funcs = set()
    for r in csv.DictReader(open(ROOT / "db" / "functions.csv", newline="", encoding="utf-8", errors="replace")):
        funcs.add(int(r["va"], 16))
    bad = []
    n = 0
    for f in sorted((ROOT / "src").rglob("*")):
        if f.suffix not in (".cpp", ".h") or f.name == "Prologues.gen.cpp":
            continue
        text = f.read_text(encoding="utf-8", errors="replace")
        for m in LIT.finditer(text):
            va = int(m.group(1), 16)
            if not TEXT_LO <= va < TEXT_HI:
                continue
            n += 1
            if va not in funcs:
                bad.append("%s:%d  0x%08X is not a function entry of db/functions.csv" % (
                    f.relative_to(ROOT).as_posix(), text.count("\n", 0, m.start()) + 1, va))
    for b in bad:
        print("  " + b)
    print("check_addrs: %d Addr()/StockCall literal(s) in .text checked, %d not a function entry" % (n, len(bad)))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
