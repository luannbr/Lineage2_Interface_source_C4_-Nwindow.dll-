#!/usr/bin/env python3
"""Where did a class's fields go between two builds of the Engine? Votes from the exported methods themselves.

Engine.dll exports almost every method of its classes (`?GetName@User@@QAEPAGXZ`), and a decorated name is the same in
two clients. This tool pairs, for every name present in both DLLs, the instructions of the two function bodies
(difflib over the mnemonic sequence, then operand by operand) and collects the numbers that changed:

  displacement   `[reg + 0xE8]` -> `[reg + 0xE4]`   a field that moved
  immediate      `imul ecx, ecx, 0x1F8` -> `0x230`  a record size, a count, a stride, a memset length

It is the evidence the earlier steps of the project took by hand ("one-instruction getters", "the Engine's own
Clear/Load/Get methods pair every displacement of the old build with the new one"), made repeatable.

  python tools/ue2_disp_diff.py --old <orig dir of the previous client> CLASS [CLASS ...]
        per exported method of the classes: every operand number that differs, then a vote table old -> new
  python tools/ue2_disp_diff.py --old <dir> --disp 0xE8 [--class User]
        every method that touches displacement 0xE8 in the old build and what it is in the new one
  options: --min / --max bound the numbers looked at (default 0x4 .. 0x100000), --dll Engine|Core, --all-functions
        (default: only the methods of the classes named; --all-functions pairs every export, useful with --disp)

The default --old is the C3 repo's orig/ ($NWD_C3_ROOT\\orig).
"""
from __future__ import annotations

import argparse
import collections
import difflib
import re
import struct
import os
import sys
from pathlib import Path

import capstone

sys.path.insert(0, str(Path(__file__).resolve().parent))
import engine_vtables as ev  # noqa: E402

OLD_DEFAULT = (Path(os.environ["NWD_C3_ROOT"]) / "orig" if os.environ.get("NWD_C3_ROOT") else None)
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
HEX = re.compile(r"0x[0-9a-f]+")
REG = re.compile(r"\be(?:[abcd]x|[sd]i|[sb]p)\b|\b[abcd][lh]\b")


def body(d: ev.Dll, name: str, limit: int = 0x600):
    """[(mnemonic, op_str)] of an exported function (incremental-link thunks followed), up to the first int3,
    the start of another export, or `limit` bytes."""
    va = d.exp[name]
    if not d.is_code(va):
        return None
    b = d.mem[va - d.base:va - d.base + 5]
    if b[:1] == b"\xe9":
        va = va + 5 + struct.unpack("<i", b[1:5])[0]
    out = []
    for i in md.disasm(bytes(d.mem[va - d.base:va - d.base + limit]), va):
        if i.mnemonic == "int3":
            break
        if i.address != va and i.address in d.names:
            break
        out.append((i.mnemonic, i.op_str))
    return out


STACK = re.compile(r"\[[^\]]*(?:\bebp -|\besp)[^\]]*\]")      # [ebp - 0x1c], [esp + 0x20]: locals and outgoing arguments, not fields


def numbers(op: str) -> list[int]:
    """The hex numbers of an operand, the ones inside a stack slot left out (their frame layout moves with every recompilation)."""
    return [int(x, 16) for x in HEX.findall(STACK.sub("[stack]", op))]


def skeleton(op: str) -> str:
    return REG.sub("R", HEX.sub("#", STACK.sub("[stack]", op)))


def pair_numbers(a, b, lo: int, hi: int):
    """[(old number, new number, mnemonic, old op, new op)] of the instruction pairs whose numbers differ."""
    out = []
    sm = difflib.SequenceMatcher(None, [m for m, _ in a], [m for m, _ in b], autojunk=False)
    for tag, i1, i2, j1, j2 in sm.get_opcodes():
        if tag == "equal" or (tag == "replace" and i2 - i1 == j2 - j1):
            for (ma, oa), (mb, ob) in zip(a[i1:i2], b[j1:j2]):
                if ma != mb or skeleton(oa) != skeleton(ob):
                    continue
                na, nb = numbers(oa), numbers(ob)
                if len(na) != len(nb):
                    continue
                for x, y in zip(na, nb):
                    if x != y and lo <= x <= hi and x < 0x10000000 and y < 0x10000000:
                        out.append((x, y, ma, oa, ob))
    return out


def klass(name: str) -> str:
    m = re.match(r"^\?[~\w]+@(\w+)@@", name)
    return m.group(1) if m else ""


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("classes", nargs="*")
    ap.add_argument("--old", type=Path, default=OLD_DEFAULT, required=OLD_DEFAULT is None, help="default: $NWD_C3_ROOT/orig")
    ap.add_argument("--dll", default="Engine")
    ap.add_argument("--min", type=lambda x: int(x, 0), default=0x4)
    ap.add_argument("--max", type=lambda x: int(x, 0), default=0x100000)
    ap.add_argument("--disp", type=lambda x: int(x, 0), default=None, help="only numbers whose OLD value is this")
    ap.add_argument("--class", dest="cls", default=None, help="with --disp: only the methods of this class")
    ap.add_argument("--all-functions", action="store_true")
    ap.add_argument("--quiet", action="store_true", help="only the vote table")
    a = ap.parse_args()

    d_old, d_new = ev.dll(a.dll, a.old), ev.dll(a.dll)
    names = sorted(set(d_old.exp) & set(d_new.exp))
    wanted = set(a.classes)
    if a.cls:
        wanted.add(a.cls)
    sel = []
    for n in names:
        if n.startswith(("??_7", "?exec")) or "@@3" in n or "@@2" in n:
            continue
        if wanted and not a.all_functions and klass(n) not in wanted:
            continue
        sel.append(n)
    if a.disp is not None and a.cls is None and not a.classes:
        sel = [n for n in names if not n.startswith(("??_7", "?exec")) and "@@3" not in n and "@@2" not in n]
    votes: dict[int, collections.Counter] = collections.defaultdict(collections.Counter)
    funcs: dict[tuple[int, int], list[str]] = collections.defaultdict(list)
    shown = 0
    und = dict(zip(sel, ev.undname(sel))) if (not a.quiet and len(sel) < 400) else {}
    for n in sel:
        ba, bb = body(d_old, n), body(d_new, n)
        if not ba or not bb:
            continue
        pairs = pair_numbers(ba, bb, a.min, a.max)
        if a.disp is not None:
            pairs = [p for p in pairs if p[0] == a.disp]
        if not pairs:
            continue
        for x, y, m, oa, ob in pairs:
            votes[x][y] += 1
            funcs[(x, y)].append(n)
        if not a.quiet:
            print(f"== {und.get(n, n)}   ({len(ba)} -> {len(bb)} instructions)")
            for x, y, m, oa, ob in pairs:
                print(f"     {x:#7x} -> {y:#7x}   {m} {oa}   =>   {ob}")
            shown += 1
    print(f"-- vote table ({len(sel)} functions looked at, {shown} with changed numbers): old -> new x methods")
    for x in sorted(votes):
        row = ", ".join(f"{y:#x} x{n}" for y, n in votes[x].most_common(5))
        print(f"  {x:#7x}: {row}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
