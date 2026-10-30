#!/usr/bin/env python3
"""UL2ConsoleWnd's pure slots in the C4 Engine.dll: which C3 row (tools/ue2_data/l2consolewnd_slots_c3.csv) belongs to
which C4 slot. The same method as tools/ue2_console_slots.py (C1 -> C2), over the C3 and C4 NWindow images; this tool
imports its pieces.

The C3 and C4 UL2ConsoleWnd vtables (Engine.dll) differ in two pure runs only: 272 -> 294 slots at 27.. (22 new slots)
and 60 -> 73 at the end (13 new slots: 390 -> 425 in all); every other run keeps its length and its order, so the
named slots (exported by Engine.dll) and the equal-length runs give the layout anchors. Inside the two windows that
grew a C3 row is placed on a C4 slot only with evidence, by tiers (the "source" column of the new table):
  layout   the non-pure slots and the pure runs that kept their length
  handler  UGameEngine::OnXxx (Engine.dll, both builds) forwards to the console: the one console slot it calls
           is "Xxx" in both builds
  guard    the guard("NConsoleWnd::Xxx") string of the slot's NConsoleWnd function, in both builds
  gap      between two anchors that are the same distance apart in C3 and C4, the slots are the same
  align    inside a window between two anchors that are NOT the same distance apart (new slots were inserted):
           an order-preserving alignment of the NConsoleWnd functions of the two builds (same `ret N`, similar
           size, similar instruction shapes), a pair is taken only when EVERY best alignment contains it
Everything else stays unnamed (PureSlotNNN in the generated list) with what is known about it as a note: the new
UGameEngine handlers that call it, the guard names of the C4 NWindow that the C3 one does not have, the unplaced C3
rows of its window. A placed row whose `ret N` differs between the builds (the signature changed) loses its name and
keeps the C3 row in the note.

  python tools/ue2_console_slots_c4.py [--c3 <C3 repo root>]     # writes tools/ue2_data/l2consolewnd_slots_c4.csv
"""
from __future__ import annotations

import argparse
import collections
import csv
import json
import os
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import engine_vtables as ev  # noqa: E402
from nwd.image import Image  # noqa: E402
import ue2_guard_names as gn  # noqa: E402
import ue2_console_slots as cs  # noqa: E402

ROOT = HERE.parent
# NWD_C3_ROOT: a checkout of the C3 project
C3_ROOT = (Path(os.environ["NWD_C3_ROOT"]) if os.environ.get("NWD_C3_ROOT") else None)
SYM = cs.SYM
OUT = ROOT / "tools" / "ue2_data" / "l2consolewnd_slots_c4.csv"
OLD_TABLE = ROOT / "tools" / "ue2_data" / "l2consolewnd_slots_c3.csv"


def read_rows(path: Path) -> dict[int, dict]:
    rows = csv.DictReader(l for l in path.open(encoding="utf-8") if not l.startswith("#"))
    return {int(r["slot"]): r for r in rows}


def c3_functions_tsv(dst: Path) -> None:
    """The C3 data has no Ghidra census here; its function database (db/c3/functions.csv: va, size) is enough for the shape features."""
    with dst.open("w", encoding="utf-8", newline="") as f:
        f.write("va\tsize\tranges\n")
        for r in csv.DictReader((ROOT / "db" / "c3" / "functions.csv").open(encoding="utf-8")):
            va, size = int(r["va"], 16), int(r["size"])
            if size > 0:
                f.write("%x\t%d\t%x-%x\n" % (va, size, va, va + size - 1))


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--c3", default=str(C3_ROOT) if C3_ROOT else None, required=C3_ROOT is None, help="the finished C3 repo (read only): orig/, Engine/NWindow of the C3 client (default: $NWD_C3_ROOT)")
    a = ap.parse_args()
    c3root = Path(a.c3)

    t3 = read_rows(OLD_TABLE)
    names3 = {s: cs.name_of(r["declaration"]) for s, r in t3.items() if r["declaration"]}
    d3, d4 = ev.dll("Engine", c3root / "orig"), ev.dll("Engine")
    vt3, vt4 = d3.vtable(SYM), d4.vtable(SYM)
    pure3 = {k for k, _, l in vt3 if "purecall" in l}
    pure4 = {k for k, _, l in vt4 if "purecall" in l}
    lab3, lab4 = [l for _, _, l in vt3], [l for _, _, l in vt4]

    # ---- anchors: (c3 slot) -> (c4 slot, source) ---------------------------------------------
    anch: dict[int, tuple[int, str]] = {}
    for k, l in enumerate(lab3):
        if k >= 25 and "purecall" not in l and not l.startswith("sub_") and l in lab4:
            anch[k] = (lab4.index(l), "layout")
    r3 = [r for r in cs.runs(vt3) if r[0] == "P"]
    r4 = [r for r in cs.runs(vt4) if r[0] == "P"]
    assert len(r3) == len(r4), "the pure runs do not pair up"
    grown = []
    for (_, a1, b1), (_, a2, b2) in zip(r3, r4):
        if b1 - a1 == b2 - a2:
            for k in range(b1 - a1 + 1):
                anch[a1 + k] = (a2 + k, "layout")
        else:
            grown.append((a1, b1, a2, b2))
    print("pure runs that grew (C3 first..last -> C4 first..last):",
          ", ".join(f"{a1}..{b1} ({b1 - a1 + 1}) -> {a2}..{b2} ({b2 - a2 + 1})" for a1, b1, a2, b2 in grown))

    h3, h4 = cs.handler_slots(d3, pure3), cs.handler_slots(d4, pure4)
    n_handler = 0
    for s, n in sorted(names3.items()):
        if h3.get("On" + n) == {s} and len(h4.get("On" + n, ())) == 1:
            if s not in anch:
                anch[s] = (next(iter(h4["On" + n])), "handler")
                n_handler += 1

    cls3 = {c["cpp"]: c for c in json.load((ROOT / "analysis" / "c3" / "classes.json").open(encoding="utf-8"))}["NConsoleWnd"]
    cls4 = {c["cpp"]: c for c in json.load((ROOT / "analysis" / "classes.json").open(encoding="utf-8"))}["NConsoleWnd"]
    eh3 = json.load((ROOT / "analysis" / "c3" / "eh.json").open(encoding="utf-8"))
    img4 = Image()
    census4 = ROOT / "analysis" / "ghidra" / "functions.tsv"
    g4 = gn.guard_names(img4, census4)
    gc3: dict[str, list[int]] = collections.defaultdict(list)
    gc4: dict[str, list[int]] = collections.defaultdict(list)
    for s in cls3["vtable_slots"]:
        e = eh3.get("%08x" % s["va"])
        if e and e["guard"] and e["guard"][0].startswith("NConsoleWnd::"):
            gc3[e["guard"][0].split("::")[1]].append(s["slot"])
    for s in cls4["vtable_slots"]:
        n = g4.get(s["va"])
        if n and n.startswith("NConsoleWnd::"):
            gc4[n.split("::")[1]].append(s["slot"])
    n_guard = 0
    for n, sl in gc3.items():
        if len(sl) == 1 and len(gc4.get(n, ())) == 1 and sl[0] >= 26 and sl[0] in pure3 and gc4[n][0] in pure4:
            if sl[0] not in anch:
                anch[sl[0]] = (gc4[n][0], "guard")
                n_guard += 1

    def monotone(x: dict[int, tuple[int, str]]) -> list[str]:
        bad = []
        items = sorted(x.items())
        for (s1, (j1, _)), (s2, (j2, _)) in zip(items, items[1:]):
            if j2 <= j1:
                bad.append(f"C3 {s1}->{j1} / {s2}->{j2}")
        return bad

    bad = monotone(anch)
    if bad:
        # a handler/guard anchor that contradicts the layout: drop it and say so (the layout rows are facts)
        print("anchors out of order, dropping the handler/guard ones that disagree:", "; ".join(bad))
        keep = {s: v for s, v in anch.items() if v[1] == "layout"}
        for s, v in sorted(anch.items()):
            if v[1] != "layout":
                trial = dict(keep)
                trial[s] = v
                if not monotone(trial):
                    keep[s] = v
        anch = keep

    def fill(x):
        out = dict(x)
        items = sorted(x.items())
        for (s1, (j1, _)), (s2, (j2, _)) in zip(items, items[1:]):
            if s2 - s1 == j2 - j1:
                for k in range(s1 + 1, s2):
                    out.setdefault(k, (j1 + k - s1, "gap"))
        return out

    placed = fill(anch)

    # ---- tier 2: the windows between anchors that are not the same distance apart -------------
    cls3_slots = {x["slot"]: x["va"] for x in cls3["vtable_slots"]}
    cls4_slots = {x["slot"]: x["va"] for x in cls4["vtable_slots"]}
    with tempfile.TemporaryDirectory() as td:
        tsv3 = Path(td) / "f3.tsv"
        c3_functions_tsv(tsv3)
        f3 = cs.features(Image(c3root / "orig" / "NWindow.dll"), tsv3, set(cls3_slots.values()))
    f4 = cs.features(img4, census4, set(cls4_slots.values()))
    items = sorted(placed.items())
    n_align = 0
    for (s1, (j1, _)), (s2, (j2, _)) in zip(items, items[1:]):
        x = [k for k in range(s1 + 1, s2) if k in pure3 and k not in placed]
        y = [k for k in range(j1 + 1, j2) if k in pure4]
        if not x or not y:
            continue
        for s, j in cs.align_window(x, y, f3, f4, cls3_slots, cls4_slots).items():
            placed[s] = (j, "align")
            n_align += 1
    bad = monotone(placed)
    if bad:
        sys.exit("placement out of order: " + "; ".join(bad))

    rs3 = {va: f["rets"] for va, f in f3.items()}
    rs4 = {va: f["rets"] for va, f in f4.items()}

    # ---- the table -----------------------------------------------------------------------------
    by_c4 = {j: (s, src) for s, (j, src) in placed.items() if s in t3 and t3[s]["declaration"]}
    pitems = sorted((s, j) for s, (j, _) in placed.items())
    window_names: dict[int, list[str]] = collections.defaultdict(list)
    for s in sorted(names3):
        if s in placed:
            continue
        lo = max((x for x in pitems if x[0] < s), default=(25, 26))
        hi = min((x for x in pitems if x[0] > s), default=(389, 424))
        for j in range(lo[1] + 1, hi[1]):
            if j in pure4 and j not in by_c4:
                window_names[j].append(names3[s])
    new_handlers = {n: sl for n, sl in h4.items() if n not in h3}
    out_rows = []
    n_named = n_ret = 0
    for j in sorted(pure4):
        if j < 26:
            continue
        if j in by_c4:
            s, src = by_c4[j]
            r = t3[s]
            note = r["note"]
            x, y = rs3.get(cls3_slots.get(s), set()), rs4.get(cls4_slots.get(j), set())
            if x and y and x != y:
                note = "C3 slot %d was `%s` (%s) but its ret is %s in C3 and %s in C4: the signature changed" % (
                    s, r["declaration"], src, "/".join("0x%x" % v for v in sorted(x)), "/".join("0x%x" % v for v in sorted(y)))
                n_ret += 1
                out_rows.append((j, "", "", note, ""))
                continue
            out_rows.append((j, r["declaration"], src, note, s))
            n_named += 1
        else:
            notes = []
            # what the C3 table already said about the slot it replaces, when it was an unnamed one
            for s, (jj, src) in placed.items():
                if jj == j and s in t3 and not t3[s]["declaration"]:
                    notes.append("C3 slot %d (%s): %s" % (s, src, t3[s]["note"]))
            for n, sl in new_handlers.items():
                if j in sl and len(sl) <= 2:
                    notes.append("UGameEngine::%s calls it" % n)
            for n, sl in gc4.items():
                if j in sl and n not in gc3:
                    notes.append("guard NConsoleWnd::%s" % n)
            va = cls4_slots.get(j)
            if va in rs4 and rs4[va]:
                notes.append("NConsoleWnd function %08x, ret %s" % (va, "/".join("0x%x" % v for v in sorted(rs4[va]))))
            if window_names.get(j):
                notes.append("one of the unplaced C3 rows " + "/".join(window_names[j]))
            out_rows.append((j, "", "", "; ".join(notes), ""))
    dropped = [(s, names3[s]) for s in sorted(names3) if s not in placed]
    with OUT.open("w", encoding="utf-8", newline="\n") as f:
        f.write("# UL2ConsoleWnd's pure slots in the C4 Engine.dll (tools/ue2_console_slots_c4.py over the C3 table: do not edit by hand).\n"
                "# declaration: the name and signature of the slot's NConsoleWnd implementation, carried over only where the\n"
                "# 'source' column says how the C4 slot was found (layout, handler, guard, gap, align: see the script's docstring);\n"
                "# the rest (declaration empty) are generated as PureSlotNNN; the note lists what is known about the slot.\n"
                "# The `c1_slot` column is the C3 slot of the row (the base client's).\n")
        w = csv.writer(f, lineterminator="\n")
        w.writerow(["slot", "declaration", "source", "note", "c1_slot"])
        for r in out_rows:
            w.writerow(r)
        f.write("# C3 rows with no C4 slot (%d): %s\n" % (len(dropped), ", ".join(f"{n}(C3 {s})" for s, n in dropped)))
    src_n = collections.Counter(r[2] for r in out_rows if r[1])
    print(f"{len(pure4)} pure slots in C4, {n_named} named from the C3 table {dict(src_n)} ({n_handler} handler and "
          f"{n_guard} guard anchors, {n_align} aligned), {len(out_rows) - n_named} placeholders, {len(dropped)} C3 rows unplaced, "
          f"{n_ret} with a changed `ret N`")
    print(f"wrote {OUT}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
