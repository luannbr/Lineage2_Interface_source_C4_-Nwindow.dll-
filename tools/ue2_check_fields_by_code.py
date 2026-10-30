#!/usr/bin/env python3
"""The NW_ASSERT_OFFSET field offsets of include/ue2/UE2Engine.h against the CODE of the Engine.dll of the previous client.

tools/ue2_check_headers.py proves the offsets of the script classes from Engine.u. The classes with no script (USkeletalMesh,
UViewport, UClient, the data records...) and the native tails cannot be proved that way; their offsets were carried from the
previous client by evidence (tools/ue2_disp_diff.py). This tool repeats that evidence for EVERY asserted field at once:

  * the old offset of a field is the one the header had at the base revision (default: the tag c3-base, the finished C3 repo);
  * the exported methods of the field's class and of its base classes are paired between the two Engine.dlls (the same decorated
    name), instruction by instruction (only functions whose mnemonic sequences are identical: strict), and every
    `[reg + OLD]` that became `[reg + X]` is a vote old -> X;
  * AGREE: the new offset of the header has votes; DIFFER: the old offset has votes, none for the header's (the field needs a look);
    NONE: no exported method of the class touches the field at its old offset (not provable by this method).

  python tools/ue2_check_fields_by_code.py [--old <C3 orig dir>] [--rev c3-base] [--class NAME ...] [--all]
Exit 1 when a field DIFFERs.
"""
from __future__ import annotations

import argparse
import collections
import csv
import re
import subprocess
import os
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import engine_vtables as ev  # noqa: E402
import ue2_disp_diff as dd  # noqa: E402

ROOT = HERE.parent
ASSERT = re.compile(r"NW_ASSERT_OFFSET\(\s*(\w+)\s*,\s*(\w+)\s*,\s*(0x[0-9A-Fa-f]+|\d+)\s*\)")
CLASS = re.compile(r"^(?:class|struct)\s+(?:\w+_API\s+)?(\w+)\s*:\s*public\s+(\w+)", re.M)
# NWD_C3_ROOT: a checkout of the C3 project (its orig/ holds the C3 client binaries)
OLD_DEFAULT = (Path(os.environ["NWD_C3_ROOT"]) / "orig" if os.environ.get("NWD_C3_ROOT") else None)
NOTES = ROOT / "tools" / "ue2_data" / "field_code_notes.csv"      # class,field,why: a DIFFER that is not a problem


def strict_pairs(a, b, lo, hi):
    """dd.pair_numbers for functions whose mnemonic sequences are identical (same length, same order)."""
    if len(a) != len(b) or [m for m, _ in a] != [m for m, _ in b]:
        return []
    return dd.pair_numbers(a, b, lo, hi)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--old", type=Path, default=OLD_DEFAULT, required=OLD_DEFAULT is None, help="default: $NWD_C3_ROOT/orig")
    ap.add_argument("--rev", default="c3-base", help="git revision of the previous header")
    ap.add_argument("--class", dest="classes", action="append", default=[])
    ap.add_argument("--all", action="store_true", help="also print the AGREE rows")
    ap.add_argument("--weak", action="store_true", help="when the class has no vote, try every exported function (small offsets are ambiguous)")
    a = ap.parse_args()

    cur = (ROOT / "include/ue2/UE2Engine.h").read_text(encoding="utf-8")
    old = subprocess.run(["git", "show", f"{a.rev}:include/ue2/UE2Engine.h"], cwd=ROOT, capture_output=True, text=True,
                         encoding="utf-8").stdout
    new_off = {(c, f): int(o, 0) for c, f, o in ASSERT.findall(cur)}
    old_off = {(c, f): int(o, 0) for c, f, o in ASSERT.findall(old)}
    bases = dict(CLASS.findall(cur))

    d_old, d_new = ev.dll("Engine", a.old), ev.dll("Engine")
    names = sorted(set(d_old.exp) & set(d_new.exp))
    by_class: dict[str, list[str]] = collections.defaultdict(list)
    for n in names:
        if n.startswith(("??_7", "?exec")) or "@@3" in n or "@@2" in n:
            continue
        by_class[dd.klass(n)].append(n)

    cache: dict[str, list] = {}

    def pairs_of(n: str):
        if n not in cache:
            ba, bb = dd.body(d_old, n), dd.body(d_new, n)
            cache[n] = strict_pairs(ba, bb, 4, 0x200000) if ba and bb else []
        return cache[n]

    def chain(c: str):
        out = [c]
        while out[-1] in bases:
            out.append(bases[out[-1]])
        return out

    rows = []
    for (c, f), noff in sorted(new_off.items()):
        if a.classes and c not in a.classes:
            continue
        ooff = old_off.get((c, f))
        if ooff is None:
            rows.append((c, f, None, noff, "NEW", ""))
            continue
        votes = collections.Counter()
        for k in chain(c):
            for n in by_class.get(k, ()):
                for x, y, *_ in pairs_of(n):
                    if x == ooff:
                        votes[y] += 1
        scope = "class"
        if not votes and a.weak:
            scope = "any"
            for n in names:
                if n.startswith(("??_7", "?exec")) or "@@3" in n or "@@2" in n:
                    continue
                for x, y, *_ in pairs_of(n):
                    if x == ooff:
                        votes[y] += 1
        if not votes:
            rows.append((c, f, ooff, noff, "NONE", ""))
        elif votes.get(noff):
            rows.append((c, f, ooff, noff, "AGREE", f"{votes[noff]} votes ({scope})"))
        else:
            rows.append((c, f, ooff, noff, "DIFFER", "; ".join(f"{y:#x} x{n}" for y, n in votes.most_common(3)) + f" ({scope})"))
    explained = {}
    if NOTES.exists():
        for r in csv.DictReader(l for l in NOTES.open(encoding="utf-8") if not l.startswith("#")):
            explained[(r["class"], r["field"])] = r["why"]
    rows = [(c, f, o, n, "EXPLAINED" if v == "DIFFER" and (c, f) in explained else v,
             note + ("  -- " + explained[(c, f)] if v == "DIFFER" and (c, f) in explained else ""))
            for c, f, o, n, v, note in rows]
    cnt = collections.Counter(r[4] for r in rows)
    for c, f, o, n, v, note in rows:
        if v == "AGREE" and not a.all:
            continue
        oo = "-" if o is None else f"{o:#x}"
        print(f"{v:6s} {c}.{f}: old {oo} -> header {n:#x}  {note}")
    print(f"{len(rows)} asserted fields: {cnt['AGREE']} AGREE with code votes, {cnt['DIFFER']} DIFFER, {cnt['EXPLAINED']} DIFFER but explained "
          f"(tools/ue2_data/field_code_notes.csv), {cnt['NONE']} NONE (no exported method of the class reads the old offset), "
          f"{cnt['NEW']} new since {a.rev}")
    return 1 if cnt["DIFFER"] else 0


if __name__ == "__main__":
    sys.exit(main())
