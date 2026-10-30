#!/usr/bin/env python3
"""Who owns the anonymous (`sub_XXXXXXXX`) functions: every call / jump / absolute reference to each one is
mapped to the function that contains it; a function whose callers all belong to one class is attributed to that
class (propagated through anonymous callers until nothing changes). Writes db/anon_owner.csv:

  va,size,owner_class,callers,why

  python tools/port/anon_owner.py            write the csv and print the summary
  python tools/port/anon_owner.py 10094030   the callers of one function

`owner_class` is empty when the callers belong to several classes (a shared helper) or none (a data reference).
Used to cut the anonymous functions into lanes that do not touch each other's files.
"""
from __future__ import annotations

import bisect
import csv
import struct
import sys
from collections import Counter, defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from nwd.image import Image  # noqa: E402


def main(argv):
    rows = list(csv.DictReader(open(ROOT / "db" / "functions.csv", newline="", encoding="utf-8")))
    srt = sorted(rows, key=lambda r: int(r["va"], 16))
    starts = [int(r["va"], 16) for r in srt]
    sizes = [max(int(r["size"]), 1) for r in srt]

    def owner(va):
        i = bisect.bisect_right(starts, va) - 1
        for j in range(i, max(i - 8, -1), -1):
            if va < starts[j] + sizes[j]:
                return srt[j]
        return None

    todo = {int(r["va"], 16): r for r in rows if r["name"].startswith("sub_") and r["status"] == "todo"}
    img = Image()
    lo, hi = img.sect_range(".text")
    text = img.read(lo, hi - lo)
    callers = defaultdict(set)            # target va -> {(caller row va, kind)}
    for op, kind in ((0xE8, "call"), (0xE9, "jmp")):
        pos = text.find(bytes([op]))
        while pos != -1 and pos + 5 <= len(text):
            va = lo + pos
            rel = struct.unpack_from("<i", text, pos + 1)[0]
            t = va + 5 + rel
            if t in todo:
                o = owner(va)
                if o is not None and int(o["va"], 16) != t:
                    callers[t].add((o["va"], o["name"], o["class"], kind))
            pos = text.find(bytes([op]), pos + 1)
    for r in img.relocs:
        v = img.u32(r)
        if v in todo:
            o = owner(r) if lo <= r < hi else None
            callers[v].add((o["va"], o["name"], o["class"], "abs") if o else ("data", "data", "", "abs"))

    if argv:
        for a in argv:
            for c in sorted(callers.get(int(a, 16), ())):
                print("  %s %-5s %s [%s]" % (c[0], c[3], c[1], c[2]))
        return 0

    owner_class = {}
    for _ in range(12):
        changed = False
        for va in todo:
            if va in owner_class:
                continue
            cls = set()
            for cva, cname, ccls, kind in callers.get(va, ()):
                if cva == "data":
                    cls.add("<data>")
                elif cname.startswith("sub_"):
                    cls.add(owner_class.get(int(cva, 16), "<anon>"))
                else:
                    cls.add(ccls or "<free>")
            cls.discard("<anon>")
            if len(cls) == 1 and "<data>" not in cls and "<free>" not in cls:
                owner_class[va] = next(iter(cls))
                changed = True
        if not changed:
            break

    out = ROOT / "db" / "anon_owner.csv"
    n_own = 0
    by = Counter()
    with open(out, "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["va", "size", "owner_class", "callers", "why"])
        for va in sorted(todo):
            r = todo[va]
            cs = sorted({c[1] for c in callers.get(va, ())})
            oc = owner_class.get(va, "")
            why = "" if oc else ("no caller (data)" if not cs else "several classes")
            w.writerow([r["va"], r["size"], oc, ";".join(cs[:6]), why])
            if oc:
                n_own += 1
                by[oc] += int(r["size"])
    print("%d anonymous todo, %d attributed to one class -> %s" % (len(todo), n_own, out))
    for c, b in by.most_common(40):
        print("  %-34s %7d B" % (c, b))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
