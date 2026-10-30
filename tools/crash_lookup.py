#!/usr/bin/env python3
"""Which function is NWindowDecomp.dll+RVA (or a stock NWindow.dll+RVA) from a `[exception]` line of NWindowDecomp.log.

  python tools/crash_lookup.py 5f130 1002fa20        RVAs of NWindowDecomp.dll (hex), or stock VAs
  python tools/crash_lookup.py --log <path to NWindowDecomp.log>   every module+RVA of the [exception] lines

NWindowDecomp.dll: the linker map of the build that was installed (build/bin/Release/NWindowDecomp.map);
NWindow.dll: db/functions.csv (the stock VA is 0x10000000 + RVA)."""
import bisect
import csv
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def load_map():
    items = []
    p = ROOT / "build" / "bin" / "Release" / "NWindowDecomp.map"
    base = 0x10000000
    for ln in p.read_text(encoding="latin-1").splitlines():
        m = re.match(r"^\s*0001:([0-9a-f]{8})\s+(\S+)\s+([0-9a-f]{8})", ln)
        if m:
            items.append((int(m.group(3), 16) - base, m.group(2)))
    items.sort()
    return items


def load_stock():
    items = []
    for r in csv.DictReader((ROOT / "db" / "functions.csv").open(encoding="utf-8")):
        items.append((int(r["va"], 16) - 0x10000000, r["name"], int(r["size"])))
    items.sort()
    return items


def lookup(items, rva, stock=False):
    keys = [x[0] for x in items]
    i = bisect.bisect_right(keys, rva) - 1
    if i < 0:
        return "?"
    x = items[i]
    return "%s+%x" % (x[1], rva - x[0])


def main():
    args = sys.argv[1:]
    ours, stock = load_map(), load_stock()
    if args and args[0] == "--log":
        for ln in Path(args[1]).read_text(encoding="latin-1").splitlines():
            for m in re.finditer(r"(NWindowDecomp|NWindow)\.dll\+([0-9a-f]{6})", ln, re.I):
                rva = int(m.group(2), 16)
                print(ln[:60].strip(), "->", lookup(ours if m.group(1).lower() == "nwindowdecomp" else stock, rva))
        return
    for a in args:
        v = int(a, 16)
        if v >= 0x10000000:
            print("%s: stock %s" % (a, lookup(stock, v - 0x10000000)))
        else:
            print("%s: ours  %s | stock %s" % (a, lookup(ours, v), lookup(stock, v)))


if __name__ == "__main__":
    main()
