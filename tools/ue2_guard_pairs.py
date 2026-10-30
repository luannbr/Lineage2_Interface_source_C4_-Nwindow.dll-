#!/usr/bin/env python3
"""Displacement changes between the NWindow.dll of two clients, function by function, paired by the guard() name.

Every guarded function of the NWindow carries its UE2 guard name ("NCDetailStatusWnd::OnPaint", tools/ue2_guard_names.py); the same name in the
previous client's image (analysis/<prev>/eh.json of this repo, made by tools/eh_scan.py) and in this one pairs the two bodies without any function
map. The pair is walked instruction by instruction (difflib over the mnemonics; operands compared with the registers and the numbers masked) and the
numbers that changed are printed. It is how the fields of the Engine's records that the NWindow READS were followed from the C3 to the C4
(User: `Load 0x158 -> 0x164`, FL2GameData: `0x3F63C -> 0x722CC`...), before tools/port/match.py has a function map of the new image.

  python tools/ue2_guard_pairs.py NCDetailStatusWnd::OnPaint --min 0x140 --max 0x240     # every changed number in [min, max]
  python tools/ue2_guard_pairs.py "" --min 0x3000 --max 0x80000 --votes                     # all functions, only the old -> new vote table
  python tools/ue2_guard_pairs.py NCDetailStatusWnd::OnPaint --same 0x34 0x64 0x80          # where those offsets are read unchanged too

Options: --old-nwindow <the previous client's NWindow.dll> (default: the C3 repo's orig/), --old-eh <eh.json of the previous client> (default
analysis/c3/eh.json), --census <this image's Ghidra functions.tsv> (default analysis/ghidra/functions.tsv). Stack slots ([ebp - x], [esp + x]) are ignored.
"""
from __future__ import annotations

import argparse
import collections
import difflib
import json
import re
import os
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
sys.path.insert(0, str(HERE))
from nwd.image import Image  # noqa: E402
import ue2_guard_names as gn  # noqa: E402

# NWD_C3_ROOT: a checkout of the C3 project
OLD_NWINDOW = (Path(os.environ["NWD_C3_ROOT"]) / "orig" / "NWindow.dll" if os.environ.get("NWD_C3_ROOT") else None)
HEX = re.compile(r"0x[0-9a-f]+")
STACK = re.compile(r"\[[^\]]*(?:\bebp -|\besp)[^\]]*\]")
REG = re.compile(r"\be(?:[abcd]x|[sd]i|[sb]p)\b")


def nums(op: str) -> list[int]:
    return [int(x, 16) for x in HEX.findall(STACK.sub("[s]", op))]


def skel(op: str) -> str:
    return REG.sub("R", HEX.sub("#", STACK.sub("[s]", op)))


def body(img: Image, va: int, n: int = 0x2400):
    out = []
    for ins in img.md.disasm(img.read(va, n), va):
        out.append((ins.mnemonic, ins.op_str))
        if ins.mnemonic == "ret" and len(out) > 4:
            break
    return out


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("name", help="substring of the guard name ('' = every paired function)")
    ap.add_argument("--min", type=lambda x: int(x, 0), default=0)
    ap.add_argument("--max", type=lambda x: int(x, 0), default=0x400)
    ap.add_argument("--same", type=lambda x: int(x, 0), nargs="*", default=None, help="offsets whose unchanged and changed reads are counted")
    ap.add_argument("--votes", action="store_true", help="only the old -> new vote table")
    ap.add_argument("--old-nwindow", type=Path, default=OLD_NWINDOW, required=OLD_NWINDOW is None, help="default: $NWD_C3_ROOT/orig/NWindow.dll")
    ap.add_argument("--old-eh", type=Path, default=ROOT / "analysis" / "c3" / "eh.json")
    ap.add_argument("--census", type=Path, default=ROOT / "analysis" / "ghidra" / "functions.tsv")
    a = ap.parse_args()

    old, new = Image(a.old_nwindow), Image()
    eh = json.loads(a.old_eh.read_text(encoding="utf-8"))
    g_old = {int(k, 16): v["guard"][0] for k, v in eh.items() if v.get("guard")}
    g_new = gn.guard_names(new, a.census)
    by_old: dict[str, list[int]] = collections.defaultdict(list)
    by_new: dict[str, list[int]] = collections.defaultdict(list)
    for va, n in g_old.items():
        by_old[n].append(va)
    for va, n in g_new.items():
        by_new[n].append(va)
    votes: dict[int, collections.Counter] = collections.defaultdict(collections.Counter)
    same: dict[int, collections.Counter] = collections.defaultdict(collections.Counter)
    npairs = 0
    for n in sorted(set(by_old) & set(by_new)):
        if a.name not in n or len(by_old[n]) != 1 or len(by_new[n]) != 1:
            continue
        x, y = body(old, by_old[n][0]), body(new, by_new[n][0])
        npairs += 1
        sm = difflib.SequenceMatcher(None, [m for m, _ in x], [m for m, _ in y], autojunk=False)
        changed = []
        for tag, i1, i2, j1, j2 in sm.get_opcodes():
            if tag == "equal" or (tag == "replace" and i2 - i1 == j2 - j1):
                for (ma, oa), (mb, ob) in zip(x[i1:i2], y[j1:j2]):
                    if ma != mb or skel(oa) != skel(ob):
                        continue
                    na, nb = nums(oa), nums(ob)
                    if len(na) != len(nb):
                        continue
                    for p, q in zip(na, nb):
                        if a.same is not None and p in a.same:
                            same[p][(q, n)] += 1
                        if p != q and a.min <= p <= a.max:
                            votes[p][q] += 1
                            changed.append((p, q, ma, oa, ob))
        if a.same is None and not a.votes and changed:
            print(f"== {n}  old {by_old[n][0]:#x} ({len(x)} insns)  new {by_new[n][0]:#x} ({len(y)} insns)")
            for p, q, ma, oa, ob in changed:
                print(f"     {p:#x} -> {q:#x}   {ma} {oa}   =>   {ob}")
    if a.same is not None:
        for o in a.same:
            print(f"{o:#x}: " + ", ".join(f"{q:#x} in {n} x{c}" for (q, n), c in same[o].most_common(8)))
    if a.votes:
        print(f"-- {npairs} function pairs; old -> new x functions")
        for p in sorted(votes):
            print(f"  {p:#x}: " + ", ".join(f"{q:#x} x{c}" for q, c in votes[p].most_common(3)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
