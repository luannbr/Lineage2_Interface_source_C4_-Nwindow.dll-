#!/usr/bin/env python3
"""A class's vtable next to its parent's: which slots are inherited, overridden or new, with
the database name of each target (analysis/classes.json + db/functions.csv).

  python tools/vtable.py NCFrameCtrl            # slots from 25 (UObject's 25 are all inherited; C1 had 24)
  python tools/vtable.py NCIcon --all           # every slot
"""
from __future__ import annotations

import csv
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
UOBJECT_SLOTS = 25   # C2 UObject (C1: 24; ClearL2Game is slot 22)


def main(argv):
    if not argv:
        print(__doc__)
        return 1
    show_all = "--all" in argv
    names = [a for a in argv if not a.startswith("--")]
    classes = {c["cpp"]: c for c in json.loads((ROOT / "analysis" / "classes.json").read_text())}
    db = {}
    with open(ROOT / "db" / "functions.csv", newline="", encoding="utf-8") as f:
        for r in csv.DictReader(f):
            db[int(r["va"], 16)] = r
    for name in names:
        c = classes[name]
        parent = classes.get(c.get("super") or "")
        pslots = {s["slot"]: s["va"] for s in parent["vtable_slots"]} if parent else {}
        print(f"== {name} (UClass {c['name']}, size 0x{c['size']:X}, "
              f"{len(c['vtable_slots'])} slots, vtable {c['vtable']:08x}, parent {c.get('super')})")
        for s in c["vtable_slots"]:
            slot, va = s["slot"], s["va"]
            if slot < UOBJECT_SLOTS and not show_all:
                continue
            if slot not in pslots:
                tag = "new"
            elif pslots[slot] == va:
                tag = "   "
            else:
                tag = "ovr"
            r = db.get(va)
            label = s.get("import") or (r["name"] if r else "?")
            status = f"[{r['status']}]" if r else ""
            print(f"  {slot:3} +0x{slot * 4:03x} {tag} {va:08x} {label} {status}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
