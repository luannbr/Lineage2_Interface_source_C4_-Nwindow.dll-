#!/usr/bin/env python3
"""Carry the verify notes of the base client (db/c3/verify_notes.csv: a difference of tools/verify_hooks.py that was
checked by hand and is not a bug) over to the functions this client ported from the same body: same source, same
compiler difference. The note text is prefixed `C3:` so a reader knows it was inherited.

Only the DIFFs of a cycle log are looked at (--log build/judge_cy_N.log, the `=== verify` section), and a note is carried
only when the new difference is of the kind the old note speaks of:

  * a `vcalls` difference (another virtual slot is called) is a layout / slot bug until proved otherwise: it is carried
    only when the note itself talks about virtual calls / slots / offsets;
  * a `ret` difference (another stack pop) likewise needs the note to talk about the return / pop;
  * `imports` differences (the other compiler inlines or calls something) are what most notes are about: carried.

Everything else stays DIFF and the loop demotes it (tools/port/demote_verify.py skips the noted ones).

usage: python tools/port/carry_notes.py --log <cycle log>"""
from __future__ import annotations

import argparse
import csv
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

VCALL_WORDS = re.compile(r"virtual|vcall|vtable|slot|\+0x[0-9a-f]+|offset", re.I)
RET_WORDS = re.compile(r"\bret\b|\bpop|return", re.I)


def rows(p: Path):
    return [r for r in csv.DictReader(l for l in p.open(encoding="utf-8") if not l.startswith("#"))]


def parse_detail(d: str):
    """'consts stock-only {a, b} ours-only {c}; strs ...' -> {label: (stock items, ours items)}"""
    out = {}
    for part in d.split("; "):
        m = re.match(r"(\w+) stock-only \{(.*?)\} ours-only \{(.*?)\}$", part.strip())
        if m:
            out[m.group(1)] = ([x.strip() for x in m.group(2).split(",") if x.strip()],
                               [x.strip() for x in m.group(3).split(",") if x.strip()])
    return out


def same_kind(new: str, old: str) -> bool:
    """The new verify_data difference is of the kind the base client's (noted) one was: every label of the new one is
    in the old one, with the same strings and the same number of constants / displacements / globals on each side."""
    a, b = parse_detail(new), parse_detail(old)
    if not a or any(k not in b for k in a):
        return False
    for k, (s, o) in a.items():
        s0, o0 = b[k]
        if k == "strs":
            if sorted(s) != sorted(s0) or sorted(o) != sorted(o0):
                return False
        elif len(s) != len(s0) or len(o) != len(o0):
            return False
    return True


def carry_data() -> int:
    """db/verify_data.csv (this cycle) against db/c3/verify_data.csv + verify_data_notes.csv: a `diff` row whose
    function was ported from a body the base client had explained, with the same kind of difference, gets the note."""
    base_notes = {r["va"].lower(): r["explanation"] for r in rows(ROOT / "db" / "c3" / "verify_data_notes.csv")}
    base_detail = {r["c2_va"].lower(): r["detail"] for r in rows(ROOT / "db" / "c3" / "verify_data.csv")
                   if r["status"] == "note"}
    ported = {}
    for r in rows(ROOT / "db" / "gen_report.csv"):
        if r["action"] == "ported" and r["c2_va"]:
            ported.setdefault(r["c2_va"].lower(), []).append(r["c1_va"].lower().replace("0x", ""))
    path = ROOT / "db" / "verify_data_notes.csv"
    have = {r["va"].lower(): r["explanation"] for r in rows(path)} if path.exists() else {}
    added = refused = 0
    for r in rows(ROOT / "db" / "verify_data.csv"):
        va = r["c2_va"].lower()
        if r["status"] != "diff" or va in have:
            continue
        for b in ported.get(va, []):
            if b in base_notes and b in base_detail:
                if same_kind(r["detail"], base_detail[b]):
                    have[va] = "C3: " + base_notes[b]
                    added += 1
                else:
                    refused += 1
                break
    with path.open("w", encoding="utf-8", newline="") as f:
        w = csv.writer(f)
        w.writerow(["va", "explanation"])
        for va in sorted(have):
            w.writerow([va, have[va]])
    print("%d verify_data notes carried over (%d refused: another kind of difference), %d in db/verify_data_notes.csv"
          % (added, refused, len(have)))
    return 0


def carry_args() -> int:
    """db/c3/verify_args_notes.csv (va,callee,explanation) at the C4 address of the paired function (same shape:
    function map category A, B or C)."""
    fmap = {r["c1_va"].lower(): r for r in rows(ROOT / "db" / "fmap.csv")}
    path = ROOT / "db" / "verify_args_notes.csv"
    head = [l.rstrip("\n") for l in path.open(encoding="utf-8") if l.startswith("#")] if path.exists() else []
    have = {(r["va"].lower(), r["callee"]): r["explanation"] for r in rows(path)} if path.exists() else {}
    added = 0
    for r in rows(ROOT / "db" / "c3" / "verify_args_notes.csv"):
        m = fmap.get(r["va"].lower())
        if m and m["cat"] in ("A", "B", "C") and (m["c2_va"].lower(), r["callee"]) not in have:
            have[(m["c2_va"].lower(), r["callee"])] = "C3: " + r["explanation"]
            added += 1
    with path.open("w", encoding="utf-8", newline="") as f:
        for l in head or ["# va,callee,explanation (tools/port/verify_args.py: a stock call whose constant arguments no call of ours "
                          "matches; \"*\" = any callee). 'C3:' marks a note inherited from the C3 project (same body)."]:
            f.write(l + "\n")
        w = csv.writer(f)
        w.writerow(["va", "callee", "explanation"])
        for (va, callee), why in sorted(have.items()):
            w.writerow([va, callee, why])
    print("%d verify_args notes carried over, %d in db/verify_args_notes.csv" % (added, len(have)))
    return 0


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--log")
    ap.add_argument("--data", action="store_true", help="carry the verify_data notes (db/verify_data.csv) instead")
    ap.add_argument("--args", action="store_true", help="carry the verify_args notes instead")
    a = ap.parse_args()
    if a.args:
        return carry_args()
    if a.data:
        return carry_data()
    if not a.log:
        ap.error("--log or --data")
    base = {r["va"].lower(): r["explanation"] for r in rows(ROOT / "db" / "c3" / "verify_notes.csv")}
    ported = {}
    for r in rows(ROOT / "db" / "gen_report.csv"):
        if r["action"] == "ported" and r["c2_va"]:
            ported.setdefault(r["c2_va"].lower(), []).append(r["c1_va"].lower().replace("0x", ""))
    path = ROOT / "db" / "verify_notes.csv"
    have, head = {}, []
    if path.exists():
        for l in path.open(encoding="utf-8"):
            if l.startswith("#"):
                head.append(l.rstrip("\n"))
        have = {r["va"].lower(): r["explanation"] for r in rows(path)}
    log = Path(a.log).read_text(encoding="utf-8", errors="replace").split("\n")
    try:
        i = next(k for k, l in enumerate(log) if l.startswith("=== verify"))
    except StopIteration:
        print("no verify section in the log")
        return 1
    added = skipped = 0
    k = i + 1
    while k < len(log):
        l = log[k]
        if l.startswith("=== "):
            break
        if l.startswith("DIFF"):
            va = l.split()[1].lower()
            kinds = set()
            k += 1
            while k < len(log) and log[k].startswith("       "):
                kinds.add(log[k].split()[0])
                k += 1
            if va in have:
                continue
            note = next((base[x] for x in ported.get(va, []) if x in base), None)
            if note is None:
                continue
            if "vcalls" in kinds and not VCALL_WORDS.search(note):
                skipped += 1
                continue
            if "ret" in kinds and not RET_WORDS.search(note):
                skipped += 1
                continue
            have[va] = "C3: " + note
            added += 1
            continue
        k += 1
    with path.open("w", encoding="utf-8", newline="") as f:
        for l in head or ["# Differences tools/verify_hooks.py reports that were checked by hand and are not bugs.",
                          "# One line per stock address; 'C3:' marks a note inherited from the C3 project (same body)."]:
            f.write(l + "\n")
        w = csv.writer(f)
        w.writerow(["va", "explanation"])
        for va in sorted(have):
            w.writerow([va, have[va]])
    print("%d notes carried over (%d refused: the new difference is not what the note speaks of), %d in db/verify_notes.csv"
          % (added, skipped, len(have)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
