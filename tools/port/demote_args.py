#!/usr/bin/env python3
"""Unexplained differences of tools/port/verify_args.py -> demotions (db/demote.csv).

verify_args compares the constant arguments of every call (a register that holds a constant counts) between the stock
function and the copy we compiled: where a call of the stock passes values that no call of ours passes, the copy is not
the same code (the defaults of a declaration changed with the client, a flag that moved). Its DIFF lines (those not
explained in db/verify_args_notes.csv) name the function; it goes back to the stock code.

usage: python tools/port/demote_args.py <verify_args output>"""
from __future__ import annotations

import csv
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def main() -> int:
    text = Path(sys.argv[1]).read_text(encoding="utf-8", errors="replace").splitlines()
    path = ROOT / "db" / "demote.csv"
    have = {r["c2_va"] for r in csv.DictReader(path.open(encoding="utf-8"))} if path.exists() else set()
    new = {}
    for l in text:
        if l.startswith("DIFF"):
            parts = l.split(None, 2)
            if len(parts) >= 3 and parts[1] not in have:
                new[parts[1].lower()] = "verify_args: " + parts[2][:150]
    with path.open("a", encoding="utf-8", newline="") as f:
        w = csv.writer(f)
        for va, why in sorted(new.items()):
            w.writerow([va, why])
    print("demoted %d functions whose constant call arguments differ from the stock's" % len(new))
    return 0


if __name__ == "__main__":
    sys.exit(main())
