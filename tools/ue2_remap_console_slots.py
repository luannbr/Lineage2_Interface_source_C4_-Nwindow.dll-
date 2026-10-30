#!/usr/bin/env python3
"""Carry the UL2ConsoleWnd pure-slot name table (slot -> declaration) to the next client's slot numbers.

The declarations of UL2ConsoleWnd's pure virtuals are NConsoleWnd's own (the engine class only has
`_purecall` there; tools/ue2_console_slots.py made the C2 table from the C1 names with the evidence of every
row). A new client renumbers the slots (C2 320 -> C3 390 virtuals) and adds callbacks. This tool

  * moves every row of the old table to its new slot through the NConsoleWnd slot map of tools/port/vslots.py
    (db/vslots.json: monotone alignment of the two vtables), checking that the new slot is a pure one in the
    new Engine.dll's UL2ConsoleWnd vtable (a row whose slot is no longer pure is reported and dropped);
  * adds a row for every pure slot of the new vtable that no old row reaches: no declaration (the generator
    emits PureSlotNNN), with what the NWindow image says about the function there (db/functions.csv name,
    its `ret N`) as the note.

  python tools/ue2_remap_console_slots.py [--from OLD.csv] [--to NEW.csv]

The result is the input of tools/gen_engine_decls.py (TABLE). Re-running it over a table that was
revised by hand loses the revisions: keep the output under version control and edit it there.
"""
from __future__ import annotations

import argparse
import csv
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from nwd.image import Image  # noqa: E402


def vtable_pure(path: Path, cls: str) -> dict[int, bool]:
    """slot -> is it `_purecall`, for the (`cls`, UObject) vtable of engine_vtables.txt."""
    out: dict[int, bool] = {}
    inside = False
    head = re.compile(r"^== \w+ \?\?_7%s@@6BUObject@@@ \((\d+) slots\)" % re.escape(cls))
    for ln in path.open(encoding="utf-8", errors="replace"):
        if ln.startswith("== "):
            inside = bool(head.match(ln))
            continue
        if inside:
            m = re.match(r"^\s+(\d+) \+0x[0-9a-f]+ [0-9a-f]+ (.*)$", ln)
            if m:
                out[int(m.group(1))] = "_purecall" in m.group(2)
    return out


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--from", dest="old", type=Path, default=ROOT / "tools/ue2_data/l2consolewnd_slots_c2.csv")
    ap.add_argument("--to", dest="new", type=Path, default=ROOT / "tools/ue2_data/l2consolewnd_slots_c3.csv")
    ap.add_argument("--vslots", type=Path, default=ROOT / "db/vslots.json")
    ap.add_argument("--cls", default="NConsoleWnd")
    ap.add_argument("--vtables-new", type=Path, default=ROOT / "analysis/engine_vtables.txt")
    ap.add_argument("--classes", type=Path, default=ROOT / "analysis/classes.json")
    a = ap.parse_args()

    mp = {int(k): v for k, v in json.loads(a.vslots.read_text(encoding="utf-8"))[a.cls]["map"].items()}
    pure_new = vtable_pure(a.vtables_new, "UL2ConsoleWnd")
    img = Image()
    funcs = {int(r["va"], 16): r for r in csv.DictReader((ROOT / "db/functions.csv").open(encoding="utf-8"))}
    cls = {c["cpp"]: c for c in json.loads(a.classes.read_text(encoding="utf-8"))}[a.cls]
    slot_va = {s["slot"]: s["va"] for s in cls["vtable_slots"]}

    old_rows = list(csv.DictReader(l for l in a.old.open(encoding="utf-8") if not l.startswith("#")))
    out: dict[int, dict] = {}
    dropped = []
    for r in old_rows:
        s = int(r["slot"])
        t = mp.get(s)
        if t is None:
            dropped.append((s, r["declaration"], "no slot in the new vtable"))
            continue
        if not pure_new.get(t):
            dropped.append((s, r["declaration"], f"slot {t} is not pure in the new Engine.dll"))
            continue
        if t in out:
            dropped.append((s, r["declaration"], f"slot {t} already taken by slot {out[t]['c1_slot']}"))
            continue
        out[t] = dict(r, slot=str(t), c1_slot=str(s))
    # a dropped row whose method name is the name the function map gave to a function sitting in a pure
    # slot nobody claimed: the slot moved where the monotone alignment did not follow it
    by_name: dict[str, list[int]] = {}
    for t in sorted(pure_new):
        va = slot_va.get(t)
        if pure_new[t] and t not in out and va in funcs:
            nm = funcs[va]["name"]
            if nm.startswith(a.cls + "::") and "vfn" not in nm:
                by_name.setdefault(nm.split("::", 1)[1], []).append(t)
    still = []
    for s_, decl, why in dropped:
        m = re.search(r"(\w+)\(", decl or "")
        slots = by_name.get(m.group(1), []) if m else []
        row = next((r for r in old_rows if int(r["slot"]) == s_), None)
        if len(slots) == 1 and row is not None and slots[0] not in out:
            out[slots[0]] = dict(row, slot=str(slots[0]), c1_slot=str(s_),
                                 note=(row["note"] + "; " if row["note"] else "") + "moved by name (the monotone slot map lost it)")
            print(f"  C2 slot {s_} {m.group(1)} -> slot {slots[0]} (by the name the function map gave)")
        else:
            still.append((s_, decl, why))
    dropped = still
    added = 0
    for t in sorted(pure_new):
        if not pure_new[t] or t in out:
            continue
        va = slot_va.get(t)
        note = ""
        if va and va in funcs:
            f = funcs[va]
            nm = f["name"]
            ret = ""
            for i in img.disasm(va, 0x400, stop_at_ret=True, limit=400):
                if i.mnemonic in ("ret", "retn"):
                    ret = i.op_str or "0"
                    break
            note = f"{a.cls} function {va:08x} {nm}" + (f", ret {ret}" if ret else "") + f" (name from {f['name_src'] or 'none'})"
        out[t] = {"slot": str(t), "declaration": "", "source": "new", "note": note, "c1_slot": ""}
        added += 1

    cols = ["slot", "declaration", "source", "note", "c1_slot"]
    with a.new.open("w", encoding="utf-8", newline="") as f:
        f.write("# UL2ConsoleWnd's pure slots in the C3 Engine.dll (tools/ue2_remap_console_slots.py over the C2 table:\n")
        f.write("# every row moved through the NConsoleWnd slot map of db/vslots.json; the C2 revisions by hand come along).\n")
        f.write("# declaration: the name and signature of the slot's NConsoleWnd implementation; empty = PureSlotNNN in the\n")
        f.write("# generated list. source: how the C2 slot was found (layout, handler, guard, gap, align, content, map, forward,\n")
        f.write("# names.csv) or `new`: a C3-only slot (note: what the C3 NWindow image has there). The `c1_slot` column is the\n")
        f.write("# BASE client's slot (C2) of the row; empty: no counterpart.\n")
        for s, decl, why in dropped:
            f.write(f"# dropped C2 slot {s}: {decl or '(unnamed)'}: {why}\n")
        w = csv.DictWriter(f, fieldnames=cols, lineterminator="\n")
        w.writeheader()
        for t in sorted(out):
            w.writerow({k: out[t].get(k, "") for k in cols})
    named = sum(1 for r in out.values() if r["declaration"])
    print(f"{len(old_rows)} old rows -> {len(out) - added} moved, {len(dropped)} dropped; {added} new pure slots; "
          f"{named} of {len(out)} rows have a declaration; written {a.new}")
    for s, decl, why in dropped:
        print(f"  dropped C2 slot {s}: {decl or '(unnamed)'}: {why}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
