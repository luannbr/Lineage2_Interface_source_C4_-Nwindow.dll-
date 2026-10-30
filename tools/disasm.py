#!/usr/bin/env python3
"""Disassemble stock functions with names from db/functions.csv and the imports.

  python tools/disasm.py 10023160 10022b60     one or more functions (entry VAs)
  python tools/disasm.py NCWnd::OnCreate        by name (exact match in the db)
  python tools/disasm.py --class NCWnd          every game function of a class

Annotations: call/jmp targets by db name, IAT slots by import name, relocated constants that
point at UTF-16 strings, and vtable-slot offsets as `vslot N`.
"""
from __future__ import annotations

import csv
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from nwd.image import ROOT, Image  # noqa: E402

HEX = re.compile(r"0x[0-9a-f]+")


def main(argv: list[str]) -> int:
    img = Image()
    rows = list(csv.DictReader((ROOT / "db" / "functions.csv").open(encoding="utf-8")))
    by_va = {int(r["va"], 16): r for r in rows}
    by_name = {}
    for r in rows:
        by_name.setdefault(r["name"], []).append(int(r["va"], 16))

    targets: list[int] = []
    if argv and argv[0] == "--class":
        targets = [int(r["va"], 16) for r in rows if r["class"] == argv[1] and r["kind"] in ("game", "init")]
    else:
        for a in argv:
            if re.fullmatch(r"(0x)?[0-9a-fA-F]{8}", a):
                targets.append(int(a, 16))
            else:
                targets += by_name.get(a, [])

    def label(v: int) -> str | None:
        if v in by_va:
            return by_va[v]["name"]
        if v in img.imports:
            return img.imports[v][1]
        sec = img.section(v)
        if sec == ".rdata" and v in img.reloc_set:
            return None
        if sec in (".rdata", ".data"):
            s = img.wstr(v, 80)
            if s and len(s) >= 2 and all(32 <= ord(c) < 127 for c in s):
                return 'L"' + s + '"'
        return None

    for va in targets:
        r = by_va.get(va)
        if not r:
            print(f"; {va:08x}: not a function entry")
            continue
        size = int(r["size"])
        print(f"; ===== {va:08x} {r['name']}  ({size} bytes)  {r['notes']}")
        end = va + size
        for ins in img.disasm(va, size + 16, stop_at_ret=False):
            if ins.address >= end:
                break
            notes = []
            for m in HEX.findall(ins.op_str):
                v = int(m, 16)
                lab = label(v)
                if lab:
                    notes.append(lab)
            m = re.search(r"\[e\w\w \+ (0x[0-9a-f]+)\]", ins.op_str)
            if ins.mnemonic in ("call", "jmp") and m:
                off = int(m.group(1), 16)
                if off % 4 == 0 and off < 0x800:
                    notes.append(f"vslot {off // 4}")
            n = ("  ; " + ", ".join(notes)) if notes else ""
            print(f"  {ins.address:08x}  {ins.bytes.hex():<20} {ins.mnemonic} {ins.op_str}{n}")
        print()
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
