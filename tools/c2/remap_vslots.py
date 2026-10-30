#!/usr/bin/env python3
"""db/vslots_c1.csv (the C1 hand-named virtual slots: class,slot,name) -> db/vslots.csv with the
slot numbers of the C2 vtables.

Every C2 vtable is shifted against the C1 one (UObject got ClearL2Game at slot 22, UL2ConsoleWnd
many virtuals more...). db/vslots.json (tools/port/vslots.py) holds, per class, {map: {c1slot:
c2slot}}. A named slot is (owner class, slot): the owner is the class that introduced the slot, so
its own map is the one to use. A slot that has no C2 counterpart (the map has no entry, or the
class is gone) is not written: it goes to stdout.

usage: python tools/c2/remap_vslots.py        (reads db/vslots_c1.csv and db/vslots.json)
"""
from __future__ import annotations

import csv
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def main() -> int:
    maps = json.loads((ROOT / "db" / "vslots.json").read_text(encoding="utf-8"))
    rows = [r for r in csv.DictReader(l for l in (ROOT / "db" / "vslots_c1.csv").open(encoding="utf-8")
                                       if not l.startswith("#"))]
    out, dropped = [], []
    for r in rows:
        cls, slot, name = r["class"], int(r["slot"]), r["name"]
        m = maps.get(cls, {}).get("map", {})
        if str(slot) in m:
            out.append((cls, m[str(slot)], name))
        else:
            dropped.append(f"{cls},{slot},{name}")
    out.sort(key=lambda x: (x[0], x[1]))
    with (ROOT / "db" / "vslots.csv").open("w", encoding="utf-8", newline="") as fh:
        fh.write("# Hand-named virtual slots of the C2 vtables (class,slot,name): named once, inherited by every\n"
                 "# override in the subclasses (tools/build_db.py). Remapped from db/vslots_c1.csv with\n"
                 "# db/vslots.json by tools/c2/remap_vslots.py; add C2-only names here by hand.\n")
        w = csv.writer(fh, lineterminator="\n")
        w.writerow(["class", "slot", "name"])
        for row in out:
            w.writerow(row)
    print(f"{len(rows)} C1 named slots -> {len(out)} remapped, {len(dropped)} without a C2 slot"
          + ("".join(f"\n  dropped: {d}" for d in dropped)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
