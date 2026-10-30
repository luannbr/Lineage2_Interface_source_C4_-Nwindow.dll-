#!/usr/bin/env python3
"""Where did a struct's fields go in the next client? Votes from the matched NWindow functions.

For every function pair of db/fmap.csv with the same shape (categories A, B, C: same instruction
sequence), the instructions are paired one to one; when both carry a `[reg + disp]` (or
`[reg + index*scale + disp]`) operand and the OLD displacement is one of the offsets the header
asserts for the struct (NW_ASSERT_OFFSET(Struct, Field, 0xOFF), or any offset inside the struct up to
--span bytes past a field), the pair (old disp -> new disp) is a vote for that field. A struct whose
offsets are large and distinctive (FL2GameData, the data tables) votes cleanly; a small struct (User:
offsets below 0x200) is polluted by `this+off` accesses of the window classes that moved by their own
base-class growth, so read those votes with the field's meaning in mind and prefer the Engine's own
one-instruction getters.

  python tools/ue2_struct_votes.py FL2GameData [--rev HEAD] [--min-disp 0x1000] [--all]

--rev    read the asserted OLD offsets from that revision of include/ue2/UE2Engine.h (default: the file
         as it is; after the header was moved to the new layout use the revision of the old one)
--base   the base tree whose NWindow.dll the old displacements come from (default: tools/port/common.py)
"""
from __future__ import annotations

import argparse
import collections
import csv
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
sys.path.insert(0, str(ROOT / "tools" / "port"))
from nwd.image import Image  # noqa: E402
import common  # noqa: E402

DISP = re.compile(r"\[(e[a-ds][xip]|e[sd]i|ebp)(?: \+ (e[a-ds][xip]|e[sd]i)(?:\*\d)?)?(?: \+ (0x[0-9a-f]+))?\]")
ASSERT = re.compile(r"NW_ASSERT_OFFSET\(\s*(\w+)\s*,\s*(\w+)\s*,\s*(0x[0-9A-Fa-f]+|\d+)\s*\)")


def extents(path: Path):
    d = {}
    for r in csv.DictReader(path.open(encoding="utf-8"), delimiter="\t"):
        d[int(r["va"], 16)] = [tuple(int(x, 16) for x in p.split("-")) for p in r["ranges"].split(",")]
    return d


def insns(img: Image, ranges):
    out = []
    for lo, hi in ranges:
        out += [i for i in img.md.disasm(img.read(lo, hi - lo + 1), lo) if i.mnemonic not in ("int3", "nop")]
    return out


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("struct")
    ap.add_argument("--rev", default=None)
    ap.add_argument("--min-disp", type=lambda x: int(x, 0), default=0)
    ap.add_argument("--span", type=lambda x: int(x, 0), default=0, help="also vote for offsets this far past a field")
    ap.add_argument("--all", action="store_true", help="print the fields without votes too")
    a = ap.parse_args()

    if a.rev:
        hdr = subprocess.run(["git", "show", f"{a.rev}:include/ue2/UE2Engine.h"], cwd=ROOT, capture_output=True,
                             text=True, encoding="utf-8").stdout
    else:
        hdr = (ROOT / "include/ue2/UE2Engine.h").read_text(encoding="utf-8")
    fields = {int(m.group(3), 0): m.group(2) for m in ASSERT.finditer(hdr) if m.group(1) == a.struct}
    if not fields:
        print(f"no NW_ASSERT_OFFSET for {a.struct}", file=sys.stderr)
        return 2
    base = common.C1_ROOT
    img_old = Image(base / "orig" / "NWindow.dll")
    img_new = Image(ROOT / "orig" / "NWindow.dll")
    f_old = extents(base / "analysis" / "ghidra" / "functions.tsv")
    f_new = extents(ROOT / "analysis" / "ghidra" / "functions.tsv")
    votes: dict[int, collections.Counter] = collections.defaultdict(collections.Counter)
    funcs: dict[tuple[int, int], set] = collections.defaultdict(set)
    nfun = 0
    keys = sorted(fields)
    for r in csv.DictReader((ROOT / "db" / "fmap.csv").open(encoding="utf-8")):
        if r["cat"] not in ("A", "B", "C"):
            continue
        va_o, va_n = int(r["c1_va"], 16), int(r["c2_va"], 16)
        if va_o not in f_old or va_n not in f_new:
            continue
        io, in_ = insns(img_old, f_old[va_o]), insns(img_new, f_new[va_n])
        if len(io) != len(in_):
            continue
        nfun += 1
        for x, y in zip(io, in_):
            if x.mnemonic != y.mnemonic:
                break
            mx, my = DISP.search(x.op_str), DISP.search(y.op_str)
            if not (mx and my and mx.group(3) and my.group(3)):
                continue
            if mx.group(1) in ("esp", "ebp") and not mx.group(2):
                continue
            do, dn = int(mx.group(3), 16), int(my.group(3), 16)
            if do < a.min_disp:
                continue
            # the field this old displacement belongs to
            f = None
            for k in keys:
                if k <= do <= k + a.span:
                    f = k
            if f is None:
                continue
            votes[f][(do, dn)] += 1
            funcs[(do, dn)].add(va_n)
    print(f"{a.struct}: {nfun} function pairs compared, {len(fields)} fields asserted")
    print(f"{'old':>9}  {'field':26s} votes (old disp -> new disp x functions)")
    for k in keys:
        c = votes.get(k)
        if not c:
            if a.all:
                print(f"{k:#9x}  {fields[k]:26s} (no votes)")
            continue
        top = c.most_common(4)
        print(f"{k:#9x}  {fields[k]:26s} " + ", ".join(f"{do:#x}->{dn:#x} x{n}" for (do, dn), n in top))
    return 0


if __name__ == "__main__":
    sys.exit(main())
