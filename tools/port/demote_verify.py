#!/usr/bin/env python3
"""Unexplained differences of tools/verify_hooks.py -> demotions (db/demote.csv).

A ported body whose compiled form disagrees with the stock C2 function (the pop on return, the
virtual slots it calls, the imports it calls) and whose difference nobody explained is not
trusted: it goes back to a stub. Explaining it (db/verify_notes.csv) or fixing the header keeps it.

usage: python tools/port/demote_verify.py <cycle log with the verify section>"""
from __future__ import annotations

import csv
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def main() -> int:
    log = Path(sys.argv[1]).read_text(encoding="utf-8", errors="replace").split("\n")
    try:
        i = next(k for k, l in enumerate(log) if l.startswith("=== verify"))
    except StopIteration:
        print("no verify section in the log")
        return 1
    path = ROOT / "db" / "demote.csv"
    have = {}
    if path.exists():
        for r in csv.DictReader(path.open(encoding="utf-8")):
            have[r["c2_va"]] = r["reason"]
    new = {}
    # differences noted after the log was written (tools/port/carry_notes.py) are explained, not demoted
    noted = set()
    npath = ROOT / "db" / "verify_notes.csv"
    if npath.exists():
        noted = {l.split(",", 1)[0].strip().lower() for l in npath.open(encoding="utf-8")
                 if l.strip() and not l.startswith("#") and not l.startswith("va,")}
    k = i + 1
    while k < len(log):
        l = log[k]
        if l.startswith("=== "):
            break
        if l.startswith("DIFF"):
            va = l.split()[1].lower()
            det = []
            k += 1
            while k < len(log) and log[k].startswith("       "):
                det.append(log[k].strip())
                k += 1
            if va not in have and va not in noted:
                new[va] = "verify: " + "; ".join(det)[:140].replace("\n", " ")
            continue
        k += 1
    # a class whose constructor is hooked carries OUR vtable (the objects the hooked constructor builds), so every slot
    # must reach what the stock slot reaches (`VT BAD` lines of verify_hooks): the class goes back to the stock
    # constructors (the stubs forward to them; the classes derived from it keep theirs)
    rep_ctor = {}
    rp0 = ROOT / "db" / "gen_report.csv"
    if rp0.exists():
        for r in csv.DictReader(rp0.open(encoding="utf-8")):
            if r["action"] in ("ported", "candidate") and r["c2_va"] and r["name"] == "%s::%s" % (r["class"], r["class"]):
                rep_ctor.setdefault(r["class"], []).append(r["c2_va"])
    nvt = 0
    for idx, l in enumerate(log):
        if l.startswith("VT BAD"):
            cls = l.split()[2]
            slot = log[idx + 1].strip()[:90] if idx + 1 < len(log) else ""
            for va in rep_ctor.get(cls, []):
                if va not in have and va not in new:
                    new[va] = "vtable: %s %s" % (cls, slot)
                    nvt += 1
    import re
    # tools/port/verify_data.py: the same field is read at another offset by the stock function and by
    # ours (both sides have displacements the other lacks): a struct layout the headers got wrong. Its output is
    # only the current one when it is newer than the cycle log (verify_data runs after a cycle that passes)
    dp = ROOT / "db" / "verify_data.csv"
    if dp.exists() and dp.stat().st_mtime < Path(sys.argv[1]).stat().st_mtime:
        dp = ROOT / "db" / "no_such_file.csv"
    if dp.exists():
        for r in csv.DictReader(dp.open(encoding="utf-8")):
            if r["status"] != "diff":
                continue
            for part in r["detail"].split("; "):
                m = re.match(r"disps stock-only \{(.*?)\} ours-only \{(.*?)\}", part)
                if m and m.group(1) and m.group(2) and r["c2_va"] not in have:
                    new[r["c2_va"]] = "verify_data: " + part[:130]
    # candidates (a C1 body tried on a C2 function that is not the same code) are kept only when EVERY
    # set verify_data compares is equal to the stock function's
    cp = ROOT / "db" / "candidates.csv"
    if cp.exists() and dp.exists():
        cands = {r["c2_va"] for r in csv.DictReader(cp.open(encoding="utf-8"))}
        for r in csv.DictReader(dp.open(encoding="utf-8")):
            if r["c2_va"] in cands and r["status"] != "ok" and r["c2_va"] not in have:
                new.setdefault(r["c2_va"], "candidate: " + r["detail"][:130])
    # tools/regcheck.py: a stock caller keeps using a register after calling a function we hooked
    # (the original was built with whole-program optimization and left it alone): the callee goes
    # back to the stock code, as in C1, until its callers are ported with it
    rep = {}
    rp = ROOT / "db" / "gen_report.csv"
    if rp.exists():
        for r in csv.DictReader(rp.open(encoding="utf-8")):
            if r["action"] in ("ported", "candidate"):
                rep.setdefault(r["name"], []).append(r["c2_va"])
    nreg = 0
    for l in log:
        if l.startswith("BREAKS") and " after call to " in l:
            callee = l.split(" after call to ", 1)[1].split(" reads", 1)[0].strip()
            caller = l.split(" in ", 1)[1].split(":", 1)[0]
            for va in rep.get(callee, []):
                if va not in have and va not in new:
                    new[va] = "regcheck: %s reads a register after calling it" % caller
                    nreg += 1
    with path.open("a", encoding="utf-8", newline="") as fh:
        w = csv.writer(fh)
        for va, why in sorted(new.items()):
            w.writerow([va, why])
    print("demoted %d functions the verifier could not explain (%d known)" % (len(new), len(have)))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
