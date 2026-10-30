#!/usr/bin/env python3
"""The work packet of one function: everything needed to port it by hand (or by an agent).

  python tools/port/packet.py NCButton::OnPaint        (a name: C1 or C2 style, class::method)
  python tools/port/packet.py 10001b30                 (a C1 address, hex, with --c1; default: C2 address)
  python tools/port/packet.py --list NCButton          (every function of a class: status and why)

Prints, in this order:
  1. the map row (C1 va, C2 va, similarity, category, how it was matched) and why the generator
     did not port it (db/gen_report.csv)
  2. the C1 source of the function (the unit in the C1 tree: signature, body, hook line)
  3. the instruction diff C1 -> C2 (the C2 side annotated with the C2 field names: `this+0x254`
     shows as `TextID`), '=' same, '~' same shape with other operands, '-' only C1, '+' only C2
  4. the Ghidra decompile of the C2 function (analysis/ghidra/decomp), when it exists
"""
from __future__ import annotations

import argparse
import csv
import difflib
import json
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import common  # noqa: E402
import gen  # noqa: E402
import maps  # noqa: E402
from common import ROOT, C1_ROOT  # noqa: E402


def disasm_lines(img, ranges):
    out = []
    for lo, hi in ranges:
        for i in img.md.disasm(img.read(lo, hi - lo + 1), lo):
            if i.mnemonic in ("int3", "nop"):
                continue
            out.append((i.address, "%s %s" % (i.mnemonic, i.op_str)))
    return out


def field_names(layout, cls, c2=True):
    """C2 offset -> field name along the ancestor chain of cls."""
    names = {}
    chain = []
    c1 = gen.load_classes(C1_ROOT)
    n = cls
    while n in layout:
        chain.append(n)
        n = c1[n]["super"] if n in c1 else None
        if n is None:
            break
    for k in reversed(chain):
        for r in layout[k]["fields"]:
            if r["c2"] is not None:
                for d in range(max(r["ext"], 1)):
                    names.setdefault(r["c2"] + d, (k, r["name"], d))
    return names


def annotate(text, names, cls, this_ok=True):
    if not this_ok:
        return text
    m = re.search(r"\[(e[a-ds][xip]|e[sd]i|ebx)(?: \+ (0x[0-9a-f]+))?\]", text)
    if not m or not m.group(2):
        return text
    d = int(m.group(2), 16)
    if d in names:
        k, n, off = names[d]
        return "%-44s ; %s.%s%s" % (text, k, n, ("+%d" % off) if off else "")
    return text


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("what")
    ap.add_argument("--c1", action="store_true", help="the hex address is a C1 address")
    ap.add_argument("--list", action="store_true")
    ap.add_argument("--decomp-lines", type=int, default=140)
    a = ap.parse_args()

    rep = list(csv.DictReader((ROOT / "db" / "gen_report.csv").open(encoding="utf-8")))
    fmap = list(csv.DictReader((ROOT / "db" / "fmap.csv").open(encoding="utf-8")))

    if a.list:
        for r in rep:
            if r["class"] == a.what:
                print("%-10s %s -> %s  %-9s %-34s %s" % (r["c1_va"], "", r["c2_va"], r["action"], r["name"], r["reason"]))
        return 0

    row = None
    if re.fullmatch(r"[0-9a-fA-F]{8}", a.what):
        key = "c1_va" if a.c1 else "c2_va"
        row = next((r for r in fmap if r[key] == a.what.lower()), None)
    else:
        want = a.what.replace("::", "::")
        row = next((r for r in fmap if r["name"] == want), None)
    if row is None:
        print("no map row for", a.what)
        return 1
    m1, m2 = common.c1_model(), common.c2_model()
    va1, va2 = int(row["c1_va"], 16), int(row["c2_va"], 16)
    print("== map: %s  C1 %08x (%d bytes) -> C2 %08x (%d bytes)  sim %s cat %s via %s  class %s" % (
        row["name"], va1, int(row["size1"]), va2, int(row["size2"]), row["sim"], row["cat"], row["how"], row["class"]))
    for r in rep:
        if r["c1_va"] == row["c1_va"]:
            print("== generator: %s %s" % (r["action"], r["reason"]))

    # C1 source unit
    found = False
    for p in sorted((C1_ROOT / "src" / "nwindow").glob("*.cpp")):
        lines = p.read_text(encoding="utf-8", errors="replace").split("\n")
        units, _ = gen.split_units(lines)
        start = 0
        for intro, fn, hm in units:
            if int(hm.group(2), 16) == va1:
                print("== C1 source (%s)" % p.name)
                print("\n".join(fn))
                found = True
        if found:
            break
    if not found:
        print("== C1 source: not found in the C1 tree")

    # instruction diff
    x, y = m1.by_va[va1], m2.by_va[va2]
    la, lb = disasm_lines(m1.img, x["ranges"]), disasm_lines(m2.img, y["ranges"])
    rb = maps.raw(m2.img, y["ranges"])
    tb = maps.this_info(rb)

    def this_at(k):
        """is the memory operand of C2 instruction k based on `this`?"""
        for op in rb[k][1]:
            if op[0] == "m" and maps.is_this(tb, k, op[1]):
                return True
        return False
    layout = json.load(open(ROOT / "db" / "layout.json", encoding="utf-8"))
    names = field_names(layout, row["class"]) if row["class"] in layout else {}
    sm = difflib.SequenceMatcher(None, x["l"], y["l"], autojunk=False)
    print("== instruction diff C1 -> C2 (%d / %d instructions)" % (len(la), len(lb)))
    for tag, i1, i2, j1, j2 in sm.get_opcodes():
        if tag == "equal":
            for k in range(i1, i2):
                a_, b_ = la[k][1], lb[j1 + k - i1][1]
                if x["s"][k] == y["s"][j1 + k - i1]:
                    print("  = %08x %s" % (lb[j1 + k - i1][0], annotate(b_, names, row["class"], this_at(j1 + k - i1))))
                else:
                    print("  ~ %08x %-40s | %s" % (lb[j1 + k - i1][0], annotate(b_, names, row["class"], this_at(j1 + k - i1)), a_))
        else:
            for k in range(i1, i2):
                print("  - %08x %s" % (la[k][0], la[k][1]))
            for k in range(j1, j2):
                print("  + %08x %s" % (lb[k][0], annotate(lb[k][1], names, row["class"], this_at(k))))

    d = ROOT / "analysis" / "ghidra" / "decomp" / ("%08x.c" % va2)
    if d.exists():
        txt = d.read_text(encoding="utf-8", errors="replace").split("\n")
        print("== Ghidra decompile of C2 %08x (%d lines)" % (va2, len(txt)))
        print("\n".join(txt[:a.decomp_lines]))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
