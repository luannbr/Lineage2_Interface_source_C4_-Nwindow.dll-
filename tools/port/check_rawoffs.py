#!/usr/bin/env python3
"""A raw byte offset in a function body (`nw::Field<T>(obj, 0xNN)`, `(BYTE*)obj + 0xNN`) is a number copied from the C3 body.

The first C4 game run that reached the world died because of one: NConsoleWnd::ReceiveMacroListInfo wrote the macro window's slots at
the C3 offsets (desktop +0x12C, slots +0x11C; C4: +0x150 / +0x13C) and zeroed other objects of the heap. tools/verify_data.py
could not see it (the by-value MacroInfo makes the hook a forwarder and the body stays behind it) and the inherited `C3:` note
excused the rest. Two more were found by reading all of them: the pawn's store message in NCPrivateBuyWnd / NCPrivateShopWnd
(C3 +0x1628 / +0x161C, C4 +0x16A8 / +0x169C).

Every raw-offset line of src/ (not Prologues.gen.cpp) must be in db/rawoffs_ok.txt, a line per site: `file|the line without
its blanks`. A line that is not there is new: check the number against the C4 stock function (tools/disasm.py <va>: the stock
has the same displacement, or the constant is an offset of a struct that did not move), then add it with --update. A line
that was edited since (a changed number) is new again, so a fixed offset has to be looked at once more.

usage: python tools/port/check_rawoffs.py            exit 1 when a raw-offset line is not in the list
       python tools/port/check_rawoffs.py --update   rewrite db/rawoffs_ok.txt from the tree (after you looked at the new lines)"""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
LIST = ROOT / "db" / "rawoffs_ok.txt"
PAT = re.compile(r"\(\s*(?:const\s+)?BYTE\s*\*\s*\)\s*[A-Za-z_][A-Za-z_0-9>()\-\.]*\s*\+\s*0x[0-9A-Fa-f]{2,}"
                 r"|BYTE\*>\([^)]*\)\s*\+\s*0x[0-9A-Fa-f]{2,}"
                 r"|Field<[^>]*>\([^;]*,\s*0x[0-9A-Fa-f]+\)")


def sites() -> list[tuple[str, int, str]]:
    out = []
    for f in sorted((ROOT / "src").rglob("*.cpp")):
        if f.name == "Prologues.gen.cpp":
            continue
        rel = f.relative_to(ROOT).as_posix()
        for i, line in enumerate(f.read_text(encoding="utf-8", errors="replace").splitlines(), 1):
            code = line.split("//", 1)[0]
            if PAT.search(code):
                out.append((rel, i, rel + "|" + re.sub(r"\s+", "", code)))
    return out


def main() -> int:
    found = sites()
    if "--update" in sys.argv:
        LIST.write_text("".join(k + "\n" for _, _, k in found), encoding="utf-8")
        print("check_rawoffs: %d raw-offset line(s) written to db/rawoffs_ok.txt" % len(found))
        return 0
    ok = set(LIST.read_text(encoding="utf-8").splitlines()) if LIST.exists() else set()
    new = [(f, n, k) for f, n, k in found if k not in ok]
    for f, n, k in new:
        print("  %s:%d  raw offset not in db/rawoffs_ok.txt: %s" % (f, n, k.split("|", 1)[1][:100]))
    if new:
        print("  (compare each number with the C4 stock function, then python tools/port/check_rawoffs.py --update)")
    print("check_rawoffs: %d raw-offset line(s), %d not reviewed" % (len(found), len(new)))
    return 1 if new else 0


if __name__ == "__main__":
    sys.exit(main())
