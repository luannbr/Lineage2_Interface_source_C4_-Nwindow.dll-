#!/usr/bin/env python3
"""UL2ConsoleWnd's pure slots in the C2 Engine: which C1 name (db/l2consolewnd_slots.csv, the
C1 repo's table) belongs to which C2 slot.

The 228 pure slots of the C1 UL2ConsoleWnd are named after what NConsoleWnd implements; in C2 there are
275 of them (the vtable has 320 slots instead of 272) and every number after the first insertion
moved. A C1 slot is placed on a C2 slot only with evidence, by tiers (the "source" column):
  layout   the non-pure slots (exported by name) and the pure runs between them: a run that kept its
           length keeps its order
  handler  UGameEngine::OnXxx (Engine.dll, both builds) forwards to the console: the one console slot
           it calls is "Xxx" in both builds
  guard    the guard("NConsoleWnd::Xxx") string of the slot's NConsoleWnd function, in both builds
  gap      between two anchors that are the same distance apart in C1 and C2, the slots are the same
  align    inside a window between two anchors that are NOT the same distance apart (new slots were
           inserted): an order-preserving alignment of the NConsoleWnd functions of the two builds
           (same `ret N`, similar size, similar instruction shapes), a pair is taken only when EVERY
           best alignment contains it (identical stubs, folded by the linker, never qualify)
Everything else stays unnamed (PureSlotNNN in the generated list, with the candidate names we know
from new UGameEngine handlers / guard strings as a note) until the C2 NConsoleWnd code names it.
A placed row also gets a flag when the `ret N` of the implementing functions differs between the
builds (the C2 signature changed: the C1 declaration is wrong for it).

  python tools/ue2_console_slots.py [--c1 DIR]     # writes tools/ue2_data/l2consolewnd_slots_c2.csv
"""
from __future__ import annotations

import argparse
import collections
import csv
import json
import re
import struct
import os
import sys
from pathlib import Path

import difflib

import capstone
from capstone.x86 import X86_OP_IMM, X86_OP_MEM, X86_OP_REG

sys.path.insert(0, str(Path(__file__).resolve().parent))
import engine_vtables as ev  # noqa: E402
from nwd.image import Image  # noqa: E402
import ue2_guard_names as gn  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
# NWD_C1_ROOT: a checkout of the C1 project
C1_ROOT = (Path(os.environ["NWD_C1_ROOT"]) if os.environ.get("NWD_C1_ROOT") else None)
SYM = "??_7UL2ConsoleWnd@@6BUObject@@@"
OUT = ROOT / "tools" / "ue2_data" / "l2consolewnd_slots_c2.csv"

md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
md.detail = True


def read_table(path: Path) -> dict[int, str]:
    rows = csv.DictReader(l for l in path.open(encoding="utf-8") if not l.startswith("#"))
    return {int(r["slot"]): r["declaration"] for r in rows}


def name_of(decl: str) -> str:
    return re.search(r"(\w+)\(", decl.replace("VARARGS ", "")).group(1)


def handler_slots(d: ev.Dll, pure: set[int]) -> dict[str, set[int]]:
    """UGameEngine::OnXxx -> the pure console slots its body calls through a vtable."""
    out: dict[str, set[int]] = {}
    for _, t, lab in d.vtable("??_7UGameEngine@@6B@"):
        m = re.match(r"\?(On\w+)@UGameEngine", lab)
        if not m:
            continue
        va = t
        b = d.mem[va - d.base:va - d.base + 5]
        if b[:1] == b"\xe9":
            va = va + 5 + struct.unpack("<i", b[1:5])[0]
        code = bytes(d.mem[va - d.base:va - d.base + 1200])
        for i in md.disasm(code, va):
            if i.mnemonic == "int3":
                break
            if i.address != va and i.address in d.names and not d.names[i.address][0].startswith("??_7"):
                break
            if i.mnemonic == "call" and i.operands[0].type == X86_OP_MEM:
                mem = i.operands[0].mem
                if mem.base and not mem.index and mem.disp % 4 == 0 and mem.disp // 4 in pure:
                    out.setdefault(m.group(1), set()).add(mem.disp // 4)
        out.setdefault(m.group(1), set())
    return out


def features(img: Image, funcs_tsv: Path, vas: set[int]) -> dict[int, dict]:
    """VA -> {size, rets (the `ret N` operands), toks (mnemonic + operand kinds, call targets and
    field offsets masked)} of the functions at `vas`."""
    out: dict[int, dict] = {}
    for r in csv.DictReader(funcs_tsv.open(encoding="utf-8"), delimiter="	"):
        va = int(r["va"], 16)
        if va not in vas:
            continue
        rets: set[int] = set()
        toks: list[str] = []
        for rg in r["ranges"].split(","):
            lo, hi = (int(x, 16) for x in rg.split("-"))
            for i in md.disasm(img.read(lo, hi - lo + 1), lo):
                if i.mnemonic in ("int3", "nop"):
                    continue
                if i.mnemonic == "ret":
                    rets.add(int(i.op_str, 16) if i.op_str else 0)
                kinds = "".join("r" if o.type == X86_OP_REG else "i" if o.type == X86_OP_IMM else "m"
                                for o in i.operands)
                toks.append(i.mnemonic + ":" + kinds)
        out[va] = {"size": int(r["size"]), "rets": rets, "toks": toks}
    return out


def align_window(a_slots: list[int], b_slots: list[int], f1: dict, f2: dict, va1: dict, va2: dict) -> dict[int, int]:
    """Order-preserving alignment of C1 slots a_slots with C2 slots b_slots; returns the pairs that
    are in every best alignment."""
    n, m = len(a_slots), len(b_slots)
    G1, G2, MIN = 600, 200, 600     # gap costs (a C1 slot should find a partner; C2 has new slots)

    def score(i: int, j: int):
        x, y = f1.get(va1[a_slots[i]]), f2.get(va2[b_slots[j]])
        if not x or not y or x["rets"] != y["rets"]:
            return None
        if min(x["size"], y["size"]) < 24:      # stubs look alike, and the linker folds them: no evidence
            return None
        if min(x["size"], y["size"]) * 2 < max(x["size"], y["size"]):
            return None
        sm = difflib.SequenceMatcher(None, x["toks"], y["toks"], autojunk=False).ratio()
        v = int(round(sm * 1000))
        return v if v >= MIN else None

    sc = [[score(i, j) for j in range(m)] for i in range(n)]
    NEG = -10 ** 9
    # forward value and number of best paths
    F = [[NEG] * (m + 1) for _ in range(n + 1)]
    NF = [[0] * (m + 1) for _ in range(n + 1)]
    F[0][0], NF[0][0] = 0, 1

    def relax(tbl, cnt, i, j, v, c):
        if v > tbl[i][j]:
            tbl[i][j], cnt[i][j] = v, c
        elif v == tbl[i][j]:
            cnt[i][j] += c

    for i in range(n + 1):
        for j in range(m + 1):
            if F[i][j] == NEG:
                continue
            if i < n:
                relax(F, NF, i + 1, j, F[i][j] - G1, NF[i][j])
            if j < m:
                relax(F, NF, i, j + 1, F[i][j] - G2, NF[i][j])
            if i < n and j < m and sc[i][j] is not None:
                relax(F, NF, i + 1, j + 1, F[i][j] + sc[i][j], NF[i][j])
    B = [[NEG] * (m + 1) for _ in range(n + 1)]
    NB = [[0] * (m + 1) for _ in range(n + 1)]
    B[n][m], NB[n][m] = 0, 1
    for i in range(n, -1, -1):
        for j in range(m, -1, -1):
            if (i, j) == (n, m):
                continue
            if i < n:
                relax(B, NB, i, j, B[i + 1][j] - G1, NB[i + 1][j]) if B[i + 1][j] != NEG else None
            if j < m:
                relax(B, NB, i, j, B[i][j + 1] - G2, NB[i][j + 1]) if B[i][j + 1] != NEG else None
            if i < n and j < m and sc[i][j] is not None and B[i + 1][j + 1] != NEG:
                relax(B, NB, i, j, sc[i][j] + B[i + 1][j + 1], NB[i + 1][j + 1])
    best, total = F[n][m], NF[n][m]
    out = {}
    for i in range(n):
        for j in range(m):
            if sc[i][j] is None or F[i][j] == NEG or B[i + 1][j + 1] == NEG:
                continue
            if F[i][j] + sc[i][j] + B[i + 1][j + 1] == best and NF[i][j] * NB[i + 1][j + 1] == total:
                out[a_slots[i]] = b_slots[j]
    return out


def runs(vt) -> list[tuple[str, int, int]]:
    out: list[list] = []
    for k, _, lab in vt:
        kind = "P" if "purecall" in lab else "N"
        if out and out[-1][0] == kind:
            out[-1][2] = k
        else:
            out.append([kind, k, k])
    return [tuple(x) for x in out]


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--c1", default=str(C1_ROOT) if C1_ROOT else None, required=C1_ROOT is None, help="the C1 project (default: $NWD_C1_ROOT)")
    c1root = Path(ap.parse_args().c1)

    t1 = read_table(c1root / "db" / "l2consolewnd_slots.csv")
    d1, d2 = ev.dll("Engine", c1root / "orig"), ev.dll("Engine")
    vt1, vt2 = d1.vtable(SYM), d2.vtable(SYM)
    pure1 = {k for k, _, l in vt1 if "purecall" in l}
    pure2 = {k for k, _, l in vt2 if "purecall" in l}
    lab1, lab2 = [l for _, _, l in vt1], [l for _, _, l in vt2]

    # ---- anchors: (c1 slot) -> (c2 slot, source) ---------------------------------------------
    anch: dict[int, tuple[int, str]] = {}
    for k, l in enumerate(lab1):          # layout: the non-pure slots, matched by their label
        if k >= 25 and "purecall" not in l and not l.startswith("sub_") and l in lab2:
            anch[k] = (lab2.index(l), "layout")
    r1 = [r for r in runs(vt1) if r[0] == "P"]
    r2 = [r for r in runs(vt2) if r[0] == "P"]
    assert len(r1) == len(r2), "the pure runs do not pair up"
    for (_, a1, b1), (_, a2, b2) in zip(r1, r2):
        if b1 - a1 == b2 - a2:            # a run that kept its length keeps its order
            for k in range(b1 - a1 + 1):
                anch[a1 + k] = (a2 + k, "layout")

    names1 = {s: name_of(d) for s, d in t1.items()}
    h1, h2 = handler_slots(d1, pure1), handler_slots(d2, pure2)
    for s, n in sorted(names1.items()):
        if h1.get("On" + n) == {s} and len(h2.get("On" + n, ())) == 1:
            anch.setdefault(s, (next(iter(h2["On" + n])), "handler"))

    cls1 = {c["cpp"]: c for c in json.load((c1root / "analysis" / "classes.json").open(encoding="utf-8"))}["NConsoleWnd"]
    cls2 = {c["cpp"]: c for c in json.load((ROOT / "analysis" / "c2" / "analysis" / "classes.json").open(encoding="utf-8"))}["NConsoleWnd"]
    eh1 = json.load((c1root / "analysis" / "eh.json").open(encoding="utf-8"))
    img2 = Image()
    g2 = gn.guard_names(img2, ROOT / "analysis" / "c2" / "funcs_c2_nwindow.tsv")
    gc1: dict[str, list[int]] = collections.defaultdict(list)
    gc2: dict[str, list[int]] = collections.defaultdict(list)
    for s in cls1["vtable_slots"]:
        e = eh1.get("%08x" % s["va"])
        if e and e["guard"] and e["guard"][0].startswith("NConsoleWnd::"):
            gc1[e["guard"][0].split("::")[1]].append(s["slot"])
    for s in cls2["vtable_slots"]:
        n = g2.get(s["va"])
        if n and n.startswith("NConsoleWnd::"):
            gc2[n.split("::")[1]].append(s["slot"])
    for n, sl in gc1.items():
        if len(sl) == 1 and len(gc2.get(n, ())) == 1 and sl[0] >= 26 and sl[0] in pure1 and gc2[n][0] in pure2:
            anch.setdefault(sl[0], (gc2[n][0], "guard"))

    def monotone(a: dict[int, tuple[int, str]]) -> list[str]:
        bad = []
        items = sorted(a.items())
        for (s1, (j1, _)), (s2, (j2, _)) in zip(items, items[1:]):
            if j2 <= j1:
                bad.append(f"C1 {s1}->{j1} / {s2}->{j2}")
        return bad

    bad = monotone(anch)
    if bad:
        sys.exit("anchors out of order: " + "; ".join(bad))

    def fill(a: dict[int, tuple[int, str]]) -> dict[int, tuple[int, str]]:
        """gap rule: two anchors the same distance apart in both builds -> 1:1 in between."""
        out = dict(a)
        items = sorted(a.items())
        for (s1, (j1, _)), (s2, (j2, _)) in zip(items, items[1:]):
            if s2 - s1 == j2 - j1:
                for k in range(s1 + 1, s2):
                    out.setdefault(k, (j1 + k - s1, "gap"))
        return out

    placed = fill(anch)

    # ---- tier 2: the windows between anchors that are not the same distance apart -------------
    cls1_slots = {x["slot"]: x["va"] for x in cls1["vtable_slots"]}
    cls2_slots = {x["slot"]: x["va"] for x in cls2["vtable_slots"]}
    f1 = features(Image(c1root / "orig" / "NWindow.dll"), c1root / "analysis" / "ghidra" / "functions.tsv",
                  set(cls1_slots.values()))
    f2 = features(img2, ROOT / "analysis" / "c2" / "funcs_c2_nwindow.tsv", set(cls2_slots.values()))
    items = sorted(placed.items())
    n_align = 0
    for (s1, (j1, _)), (s2, (j2, _)) in zip(items, items[1:]):
        a = [k for k in range(s1 + 1, s2) if k in pure1 and k not in placed]
        b = [k for k in range(j1 + 1, j2) if k in pure2]
        if not a or not b:
            continue
        for s, j in align_window(a, b, f1, f2, cls1_slots, cls2_slots).items():
            placed[s] = (j, "align")
            n_align += 1
    bad = monotone(placed)
    if bad:
        sys.exit("placement out of order: " + "; ".join(bad))

    # ---- signature check: `ret N` of the implementing functions --------------------------------
    rs1 = {va: f["rets"] for va, f in f1.items()}
    rs2 = {va: f["rets"] for va, f in f2.items()}
    va1 = {s["slot"]: s["va"] for s in cls1["vtable_slots"]}
    va2 = {s["slot"]: s["va"] for s in cls2["vtable_slots"]}

    # ---- the table -----------------------------------------------------------------------------
    by_c2 = {j: (s, src) for s, (j, src) in placed.items() if s in t1}
    # the unplaced C1 rows of a window may be any of the unnamed C2 slots of the same window
    pitems = sorted((s, j) for s, (j, _) in placed.items())
    window_names: dict[int, list[str]] = collections.defaultdict(list)
    for s in sorted(t1):
        if s in placed:
            continue
        lo = max((x for x in pitems if x[0] < s), default=(25, 26))
        hi = min((x for x in pitems if x[0] > s), default=(272, 320))
        for j in range(lo[1] + 1, hi[1]):
            if j in pure2 and j not in by_c2:
                window_names[j].append(names1[s])
    new_handlers = {n: s for n, s in h2.items() if n not in h1}
    out_rows = []
    n_named = n_ret = 0
    for j in sorted(pure2):
        if j < 26:
            continue
        if j in by_c2:
            s, src = by_c2[j]
            note = ""
            a, b = rs1.get(va1.get(s), set()), rs2.get(va2.get(j), set())
            if a and b and a != b:
                # the C1 arguments are wrong for C2: the slot stays unnamed, the C1 row is the hint
                note = "C1 slot %d was `%s` (%s) but its ret is %s in C1 and %s in C2: the signature changed" % (
                    s, t1[s], src, "/".join("0x%x" % x for x in sorted(a)), "/".join("0x%x" % x for x in sorted(b)))
                n_ret += 1
                out_rows.append((j, "", "", note, ""))
                continue
            out_rows.append((j, t1[s], src, note, s))
            n_named += 1
        else:
            notes = []
            for n, sl in new_handlers.items():
                if j in sl and len(sl) <= 2:
                    notes.append("UGameEngine::%s calls it" % n)
            for n, sl in gc2.items():
                if j in sl and n not in gc1:
                    notes.append("guard NConsoleWnd::%s" % n)
            if window_names.get(j):
                notes.append("one of the unplaced C1 rows " + "/".join(window_names[j]))
            out_rows.append((j, "", "", "; ".join(notes), ""))
    dropped = [(s, names1[s]) for s in sorted(t1) if s not in placed]
    with OUT.open("w", encoding="utf-8", newline="\n") as f:
        f.write("# UL2ConsoleWnd's pure slots in the C2 Engine (tools/ue2_console_slots.py, do not edit by hand).\n"
                "# declaration: the C1 name and signature of the slot, carried over only where the 'source' column says\n"
                "# how the C2 slot was found (layout, handler, guard, gap, align: see the script's docstring);\n"
                "# only the placed rows carry a name, the rest (declaration empty) are generated as PureSlotNNN; the note lists\n"
                "# what is known about the slot. The `c1_slot` column is the C1 slot of the row.\n")
        w = csv.writer(f, lineterminator="\n")
        w.writerow(["slot", "declaration", "source", "note", "c1_slot"])
        for r in out_rows:
            w.writerow(r)
        f.write("# C1 rows with no C2 slot yet (%d): %s\n" % (len(dropped), ", ".join(f"{n}(C1 {s})" for s, n in dropped)))
    src_n = collections.Counter(r[2] for r in out_rows if r[1])
    print(f"{len(pure2)} pure slots in C2, {n_named} named from C1 {dict(src_n)}, "
          f"{len(out_rows) - n_named} placeholders, {len(dropped)} C1 rows unplaced, {n_ret} with a changed `ret N`")
    print(f"wrote {OUT}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
