#!/usr/bin/env python3
"""Name the function that holds each RVA of the standalone NWindow.dll (from its link map).

usage: python tools/port/map_addr.py [--map FILE] <rva-hex>...
       python tools/port/map_addr.py --log NWindow_crash.log    (every `NWindow.dll+RVA` in the crash log)

The map (build/standalone/Release/NWindow.map by default) lists each public symbol as
`<section>:<offset> <name> <Rva+Base> ...`; an address is named by the last symbol at or before it."""
from __future__ import annotations

import argparse
import bisect
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def load(map_path: Path):
    base = None
    syms = []
    for line in map_path.read_text(encoding="latin-1").splitlines():
        m = re.match(r"\s*Preferred load address is ([0-9a-fA-F]+)", line)
        if m:
            base = int(m.group(1), 16)
        m = re.match(r"\s+[0-9a-f]{4}:[0-9a-f]{8}\s+(\S+)\s+([0-9a-f]{8})\s", line)
        if m and base is not None:
            syms.append((int(m.group(2), 16) - base, m.group(1)))
    syms.sort()
    return syms


def name(syms, starts, rva):
    i = bisect.bisect_right(starts, rva) - 1
    if i < 0:
        return "?"
    return "%s+0x%X" % (syms[i][1], rva - syms[i][0])


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--map", type=Path, default=ROOT / "build" / "standalone" / "Release" / "NWindow.map")
    ap.add_argument("--log", type=Path)
    ap.add_argument("rvas", nargs="*")
    a = ap.parse_args()
    syms = load(a.map)
    starts = [s[0] for s in syms]
    if a.log:
        for line in a.log.read_text(encoding="latin-1").splitlines():
            m = re.search(r"NWindow\.dll\+([0-9A-Fa-f]+)", line)
            if m:
                print("%s    -> %s" % (line.rstrip(), name(syms, starts, int(m.group(1), 16))))
            else:
                print(line.rstrip())
        return 0
    for r in a.rvas:
        print("%s -> %s" % (r, name(syms, starts, int(r, 16))))
    return 0


if __name__ == "__main__":
    sys.exit(main())
