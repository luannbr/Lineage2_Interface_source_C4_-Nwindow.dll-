#!/usr/bin/env python3
"""C1 addresses still sitting in the C2 source as literals (globals, StockCall targets, Addr()).

gen.py rewrote the literals inside the bodies it copied and the hook/vtable/class lines, but not the
ones in headers (`#define GConsole NW_GLOBAL(NConsoleWnd*, 0x10122894)`) or in the intro code of a
source file: in hook mode such a global reads the stock image at the C1 address, which in C2 is
something else (a foundation agent found GConsole reading read-only data; the cycle cannot see it).
Method: every `0x10xxxxxx` the C1 tree has outside comments and outside the NW_HOOK_*/NW_CHECK_VTABLE/
NW_DECLARE_CLASS lines is a C1 address; if the same literal is still in the C2 tree (outside the same
places), it was never converted. It is mapped through db/fmap.csv (functions) and db/amap.csv (globals,
tables, strings, call targets) and rewritten in place; the ones without a C2 counterpart are listed.

usage: python tools/port/fix_literals.py [--apply]     (writes db/literal_fix_report.csv)"""
from __future__ import annotations

import argparse
import csv
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from common import ROOT, C1_ROOT  # noqa: E402

LIT = re.compile(r"\b0x(10[0-9A-Fa-f]{6})\b")
SKIP = re.compile(r"^\s*(NW_HOOK_|NW_CHECK_VTABLE|NW_DECLARE_|NW_IMPLEMENT_|NW_ASSERT)")


def code_part(line: str) -> str:
    return line.split("//", 1)[0]


def literals(path: Path):
    out = []
    in_block = False
    for n, line in enumerate(path.read_text(encoding="utf-8", errors="replace").split("\n")):
        s = line
        if in_block:
            if "*/" in s:
                s = s.split("*/", 1)[1]
                in_block = False
            else:
                continue
        while "/*" in s:
            a, b = s.split("/*", 1)
            if "*/" in b:
                s = a + b.split("*/", 1)[1]
            else:
                s = a
                in_block = True
        if SKIP.match(s):
            continue
        for m in LIT.finditer(code_part(s)):
            out.append((n, m.group(1).lower()))
    return out


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--apply", action="store_true")
    a = ap.parse_args()
    fmap = {}
    for r in csv.DictReader((ROOT / "db" / "fmap.csv").open(encoding="utf-8")):
        fmap[int(r["c1_va"], 16)] = int(r["c2_va"], 16)
    amap = {}
    for r in csv.DictReader((ROOT / "db" / "amap.csv").open(encoding="utf-8")):
        if float(r["share"]) >= 0.9:
            amap[int(r["c1"], 16)] = int(r["c2"], 16)

    c1lits = set()
    for sub, pat in (("include/nwindow", "*.h"), ("src/nwindow", "*.cpp")):
        for p in (C1_ROOT / sub).glob(pat):
            for _, v in literals(p):
                c1lits.add(v)

    rows = []
    n_fix = 0
    for sub, pat in (("include/nwindow", "*.h"), ("src/nwindow", "*.cpp")):
        for p in sorted((ROOT / sub).glob(pat)):
            hits = literals(p)
            stale = [(n, v) for n, v in hits if v in c1lits]
            if not stale:
                continue
            lines = p.read_text(encoding="utf-8", errors="replace").split("\n")
            for n, v in stale:
                va = int(v, 16)
                new = fmap.get(va) or amap.get(va)
                rows.append((p.relative_to(ROOT).as_posix(), n + 1, v, ("%08x" % new) if new else "", lines[n].strip()[:110]))
                if new and new != va and a.apply:
                    head, sep, tail = lines[n].partition("//")
                    lines[n] = re.sub(r"\b0x%s\b" % v, "0x%08x" % new, head, flags=re.I) + sep + tail
                    n_fix += 1
            if a.apply:
                p.write_text("\n".join(lines), encoding="utf-8", newline="\n")
    with (ROOT / "db" / "literal_fix_report.csv").open("w", encoding="utf-8", newline="") as f:
        w = csv.writer(f)
        w.writerow(["file", "line", "c1_literal", "c2_address", "text"])
        w.writerows(rows)
    unmapped = [r for r in rows if not r[3]]
    print("%d stale literals (%d without a C2 counterpart), %d rewritten" % (len(rows), len(unmapped), n_fix))
    for r in unmapped[:20]:
        print("  UNMAPPED %s:%s 0x%s  %s" % (r[0], r[1], r[2], r[4]))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
