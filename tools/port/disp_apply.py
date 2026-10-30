#!/usr/bin/env python3
"""db/disp_votes.csv -> lines of db/layout_manual.csv.

tools/port/verify_data.py says that a hooked function reads a field at another displacement than the stock function
(`disps stock-only {20c} ours-only {208}`); tools/port/disp_votes.py aligns the two functions and says WHICH header
field sits at our displacement (`NCButton.Text`) and where the stock code reads it. That is one vote for
db/layout_manual.csv:  class,field,<stock displacement>.

This reads the votes and appends a line for every field that
  * sits exactly AT our displacement (not inside it: a vote on the middle of an array says little),
  * is voted to one stock displacement only (a field with two different targets is a conflict: left alone),
  * has no line in db/layout_manual.csv yet.
The field is named by the class that declares it (the votes name the class the field was found in). A vote on "another
object" (the displacement is on a pointer other than this) is not about this class's layout and is skipped.
Run tools/port/layout.py afterwards (the manual placements are anchors there), then eligible.py.

usage: python tools/port/disp_apply.py [--min-functions N] [--dry-run]"""
from __future__ import annotations

import argparse
import collections
import csv
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--min-functions", type=int, default=1)
    ap.add_argument("--dry-run", action="store_true")
    a = ap.parse_args()
    votes = collections.defaultdict(lambda: collections.defaultdict(set))    # (class, field) -> stock disp -> functions
    for r in csv.DictReader((ROOT / "db" / "disp_votes.csv").open(encoding="utf-8")):
        hf = r["header_field"]
        if not hf or hf.startswith("(other object)"):
            continue
        ours = int(r["ours"], 16)
        cands = []
        for part in hf.split("|"):
            m = re.fullmatch(r"(\w+)\.(\w+)", part)         # exactly at the start of the field
            if m:
                cands.append((m.group(1), m.group(2)))
        if len(cands) != 1:
            continue          # inside a field, or two fields claim the displacement: nothing to say
        votes[cands[0]][int(r["stock"], 16)].update(r["examples"].split(",") if r["examples"] else [r["functions"]])
        votes[cands[0]]["_ours"] = {ours}
    path = ROOT / "db" / "layout_manual.csv"
    head = [l.rstrip("\n") for l in path.open(encoding="utf-8") if l.startswith("#")] if path.exists() else []
    have = {}
    if path.exists():
        for r in csv.DictReader(l for l in path.open(encoding="utf-8") if not l.startswith("#")):
            have[(r["class"], r["field"])] = r
    new, conflicts = [], 0
    for (cls, fld), tg in sorted(votes.items()):
        targets = {k: v for k, v in tg.items() if k != "_ours"}
        if (cls, fld) in have:
            continue
        if len(targets) != 1:
            conflicts += 1
            continue
        (stock, who), = targets.items()
        if len(who) < a.min_functions:
            continue
        new.append((cls, fld, "%x" % stock, "", "disp_votes: ours 0x%x -> stock 0x%x in %s" % (
            next(iter(tg["_ours"])), stock, ",".join(sorted(who))[:80])))
    print("%d new placements, %d fields with conflicting votes" % (len(new), conflicts))
    if a.dry_run or not new:
        return 0
    with path.open("w", encoding="utf-8", newline="") as f:
        for l in head:
            f.write(l + "\n")
        w = csv.writer(f)
        w.writerow(["class", "field", "c2", "retype", "note"])
        for r in have.values():
            w.writerow([r["class"], r["field"], r["c2"], r.get("retype", ""), r.get("note", "")])
        for r in new:
            w.writerow(r)
    return 0


if __name__ == "__main__":
    sys.exit(main())
