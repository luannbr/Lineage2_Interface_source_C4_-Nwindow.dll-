#!/usr/bin/env python3
"""Differences of tools/port/verify_data.py -> notes and demotions.

verify_data compares, per hooked function, the constants, the field displacements and the strings the stock code
uses with what ours compiled to. A function whose sets differ is not trusted:

  * the only difference is a string of ours equal to the function's own name (the guard() frame the stock build
    of this client no longer has there): explained, one line in db/verify_data_notes.csv;
  * anything else (a constant, a displacement, another string): back to a stub, one line in db/demote.csv.

Run it after the cycle (db/verify_data.csv is the cycle's output), regenerate with tools/port/gen.py and repeat
until verify_data says "0 differ". Functions ported by hand later are verified the same way.

usage: python tools/port/demote_data.py [db/verify_data.csv]"""
from __future__ import annotations

import csv
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def main() -> int:
    src = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "db" / "verify_data.csv"
    # a candidate (a base body that is not the same code as the stock function: tools/port/gen.py db/candidates.csv) is kept
    # only when EVERY set equals the stock function's: no note and no guard-name excuse applies to it
    cands = set()
    cp = ROOT / "db" / "candidates.csv"
    if cp.exists():
        cands = {r["c2_va"].lower() for r in csv.DictReader(cp.open(encoding="utf-8"))}
    rows = [r for r in csv.DictReader(src.open(encoding="utf-8"))
            if r["status"] not in ("ok", "note") or (r["status"] == "note" and r["c2_va"].lower() in cands)]
    notes_path = ROOT / "db" / "verify_data_notes.csv"
    dem_path = ROOT / "db" / "demote.csv"
    have_notes = set()
    if notes_path.exists():
        have_notes = {l.split(",", 1)[0].strip().lower() for l in notes_path.open(encoding="utf-8")
                      if l.strip() and not l.startswith("#") and not l.startswith("va,")}
    have_dem = set()
    if dem_path.exists():
        have_dem = {r["c2_va"] for r in csv.DictReader(dem_path.open(encoding="utf-8"))}
    notes, dem = [], []
    for r in rows:
        va = r["c2_va"].lower()
        if va in cands:
            if va not in have_dem:
                dem.append((va, "candidate: " + r["detail"].strip()[:400]))
            continue
        if va in have_notes:        # noted after verify_data ran (tools/port/carry_notes.py --data)
            continue
        name = r["name"]
        d = r["detail"].strip()
        own = name.split("::")[-1]
        m = re.fullmatch(r"strs stock-only \{\} ours-only \{([^{}]*)\}", d)
        # the guard() label of the C3 source: the function's own name, or the name the source copied from a sibling
        # (`guard(NCWnd::OnLButtonDown)` in NCItemWnd::OnLButtonUp): a `Class::Method` string that only ours has
        guard_like = re.compile(r"[A-Za-z_]\w*::~?[A-Za-z_]\w*")
        if m and all(x.strip() == name or x.strip().endswith("::" + own) or guard_like.fullmatch(x.strip())
                     for x in m.group(1).split(",")):
            if va not in have_notes:
                notes.append((va, "ours-only string is a guard() label of the C3 source: the C4 build has no guard frame here"))
        elif va not in have_dem:
            dem.append((va, "verify_data: " + d[:400]))
    if notes:
        new = not notes_path.exists()
        with notes_path.open("a", encoding="utf-8", newline="") as f:
            w = csv.writer(f)
            if new:
                w.writerow(["va", "explanation"])
            w.writerows(notes)
    if dem:
        new = not dem_path.exists()
        with dem_path.open("a", encoding="utf-8", newline="") as f:
            w = csv.writer(f)
            if new:
                w.writerow(["c2_va", "reason"])
            w.writerows(dem)
    print(f"{len(rows)} differ: {len(notes)} explained (guard name), {len(dem)} demoted")
    return 0


if __name__ == "__main__":
    sys.exit(main())
