#!/usr/bin/env python3
"""The C1 NWindow.dll (the C1 project, --c1) against another build (this tree's orig/, the C2 client's): what
can our C1 source give it.

Needs the function lists of both (tools/ghidra/NwdFuncs.java: the C1 one is <c1>/analysis/ghidra/functions.tsv,
the other from tools/c2/ghidra_census.sh / tools/ghidra/run_full.sh) and the class tables (tools/extract_classes.py
of each tree). This tree is the C2 one, so the three arguments default to its own files. Functions are compared after normalisation: every
address (calls, globals, strings, vtables) becomes a token, so a rebuilt function with the same
code matches even when everything around it moved.
  strict  the same instructions, registers, small constants and field offsets
  loose   the same instruction shapes (mnemonic + operand kinds): field offsets, registers and
          constants may differ (a class layout that moved, a register allocated differently)
  fuzzy   the best of the candidates that share 4-instruction shingles, by difflib ratio over the loose tokens
usage: compare_nwindow.py [<other NWindow.dll> <other funcs.tsv> <other classes.json>] [--c1 DIR] [--out analysis/c2]
Writes <out>/compare_summary.txt and <out>/class_delta.csv."""
import argparse
import collections
import csv
import difflib
import hashlib
import json
import os
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from nwd.image import ROOT, Image  # noqa: E402
from capstone.x86 import X86_OP_IMM, X86_OP_MEM, X86_OP_REG  # noqa: E402

K = 4


def norm(img, ranges):
    """(strict tokens, loose tokens) of one function (all its ranges)."""
    strict, loose = [], []
    fstart, fend = ranges[0][0], max(r[1] for r in ranges) + 1
    hi_addr = img.base + img.pe.OPTIONAL_HEADER.SizeOfImage

    def isaddr(v):
        return img.base <= v < hi_addr

    for lo, hi in ranges:
        for i in img.md.disasm(img.read(lo, hi - lo + 1), lo):
            m = i.mnemonic
            if m in ("int3", "nop"):
                continue
            ops, lops = [], []
            for op in i.operands:
                if op.type == X86_OP_REG:
                    ops.append(i.reg_name(op.reg))
                    lops.append("r")
                elif op.type == X86_OP_IMM:
                    v = op.imm & 0xFFFFFFFF
                    if m == "call":
                        ops.append("T")
                    elif m.startswith("j"):
                        ops.append(("J%d" % (v - fstart)) if fstart <= v < fend else "T")
                    elif isaddr(v):
                        ops.append("A")
                    else:
                        ops.append("%x" % v)
                    lops.append("i")
                elif op.type == X86_OP_MEM:
                    mm = op.mem
                    d = mm.disp & 0xFFFFFFFF
                    dd = "A" if isaddr(d) else "%x" % d
                    ops.append("[%s+%s*%d+%s]" % (i.reg_name(mm.base) if mm.base else "",
                                                  i.reg_name(mm.index) if mm.index else "", mm.scale, dd))
                    lops.append("m")
            strict.append(m + " " + ",".join(ops))
            loose.append(m + " " + ",".join(lops))
    return strict, loose


def h(toks):
    return hashlib.md5("\n".join(toks).encode()).hexdigest()


def shingles(toks):
    return {tuple(toks[i:i + K]) for i in range(max(1, len(toks) - K + 1))}


def load_funcs(tsv, img):
    out = []
    for r in csv.DictReader(open(tsv, encoding="utf-8"), delimiter="\t"):
        rg = [tuple(int(x, 16) for x in p.split("-")) for p in r["ranges"].split(",")]
        out.append(dict(va=int(r["va"], 16), size=int(r["size"]), ranges=rg, name=r["name"]))
    for x in out:
        x["s"], x["l"] = norm(img, x["ranges"])
        x["hs"], x["hl"] = h(x["s"]), h(x["l"])
    return out


def best_match(x, pool, post):
    sh = shingles(x["l"])
    cnt = collections.Counter()
    for s in sh:
        for j in post.get(s, ()):
            cnt[j] += 1
    best = 0.0
    for j, c in cnt.most_common(4):
        y = pool[j]
        if len(x["l"]) * len(y["l"]) < 4e6:
            r = difflib.SequenceMatcher(None, x["l"], y["l"], autojunk=False).ratio()
        else:
            r = c / (len(sh) + len(y["sh"]) - c)
        best = max(best, r)
    return best


def index(funcs):
    post = collections.defaultdict(list)
    for j, x in enumerate(funcs):
        x["sh"] = shingles(x["l"])
        for s in x["sh"]:
            post[s].append(j)
    return post


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("dll", nargs="?", default=str(ROOT / "orig" / "NWindow.dll"))
    ap.add_argument("funcs", nargs="?", default=str(ROOT / "analysis" / "ghidra" / "functions.tsv"))
    ap.add_argument("classes", nargs="?", default=str(ROOT / "analysis" / "classes.json"))
    ap.add_argument("--c1", default=os.environ.get("NWD_C1_ROOT"), required=not os.environ.get("NWD_C1_ROOT"),
                    help="the C1 project (read only): its orig/, db/, analysis/ (default: $NWD_C1_ROOT)")
    ap.add_argument("--out", default=str(ROOT / "analysis" / "c2"))
    a = ap.parse_args()
    out = Path(a.out)
    out.mkdir(parents=True, exist_ok=True)

    c1root = Path(a.c1)
    i1, i2 = Image(c1root / "orig" / "NWindow.dll"), Image(a.dll)
    i1.md.detail = i2.md.detail = True   # operands
    kinds = {int(r["va"], 16): r for r in csv.DictReader(open(c1root / "db" / "functions.csv", encoding="utf-8"))}
    f1 = load_funcs(c1root / "analysis" / "ghidra" / "functions.tsv", i1)
    f2 = load_funcs(a.funcs, i2)
    for x in f1:
        k = kinds.get(x["va"])
        x["kind"], x["cls"] = (k["kind"], k["class"] or k["name"].split("::")[0]) if k else ("?", "")
    lines = []

    def P(s=""):
        print(s)
        lines.append(s)

    def text_size(img):
        return [s for s in img.pe.sections if s.Name.startswith(b".text")][0].Misc_VirtualSize

    P("C1 NWindow: %d functions, %d bytes of %d in .text" % (len(f1), sum(x["size"] for x in f1), text_size(i1)))
    P("other     : %d functions, %d bytes of %d in .text" % (len(f2), sum(x["size"] for x in f2), text_size(i2)))

    g1 = [x for x in f1 if x["kind"] == "game"]
    gs, gl = collections.Counter(x["hs"] for x in g1), collections.Counter(x["hl"] for x in g1)
    post1 = index(g1)
    infra_s = {x["hs"] for x in f1 if x["kind"] != "game"}
    infra_l = {x["hl"] for x in f1 if x["kind"] != "game"}

    # the other build, by what C1 has for it
    cat = collections.defaultdict(list)
    for y in f2:
        if y["name"].startswith(("Catch@", "Unwind@")):
            cat["0 eh funclets"].append(y)
        elif (y["hs"] in infra_s and y["hs"] not in gs) or (y["hl"] in infra_l and y["hl"] not in gl):
            cat["0 crt / static init / thunks"].append(y)
        elif gs.get(y["hs"]):
            cat["1 identical to a C1 function"].append(y)
        elif gl.get(y["hl"]):
            cat["2 same shape (offsets/registers differ)"].append(y)
        else:
            y["sh"] = shingles(y["l"])
            b = best_match(y, g1, post1)
            cat["3 >=90% similar" if b >= .9 else "4 75-90% similar" if b >= .75 else
                "5 50-75% similar" if b >= .5 else "6 <50% / no counterpart (new)"].append(y)
    tot = sum(y["size"] for y in f2)
    P()
    P("Functions of the other build, by what our C1 source has for them:")
    for k, L in sorted(cat.items()):
        P("  %-42s %5d funcs %8d bytes %5.1f%%" % (k, len(L), sum(y["size"] for y in L),
                                                   100.0 * sum(y["size"] for y in L) / tot))

    # C1 game functions, by what the other build has
    h2s, h2l = collections.Counter(x["hs"] for x in f2), collections.Counter(x["hl"] for x in f2)
    post2 = index(f2)
    byc = collections.defaultdict(lambda: [0, 0, 0, 0, 0])
    tot1 = collections.Counter()
    for x in g1:
        if h2s.get(x["hs"]):
            k = 0
        elif h2l.get(x["hl"]):
            k = 1
        else:
            b = best_match(x, f2, post2)
            k = 2 if b >= .9 else 3 if b >= .75 else 4
        byc[x["cls"]][k] += x["size"]
        tot1[k] += x["size"]
    t1 = sum(tot1.values())
    P()
    P("C1 game functions (%d bytes), by what the other build has for them:" % t1)
    for k, n in enumerate(["identical", "same shape", ">=90% similar", "75-90% similar", "<75% / gone"]):
        P("  %-16s %8d bytes %5.1f%%" % (n, tot1[k], 100.0 * tot1[k] / t1))

    # classes
    c1 = {r["cpp"]: r for r in json.load(open(c1root / "analysis" / "classes.json"))}
    c2 = {r["cpp"]: r for r in json.load(open(a.classes))}

    def slots(r):
        return len(r.get("vtable_slots", []))

    common = sorted(set(c1) & set(c2))
    P()
    P("Classes: C1 %d, other %d, in common %d, new %d, gone %d" % (
        len(c1), len(c2), len(common), len(set(c2) - set(c1)), len(set(c1) - set(c2))))
    P("  sizeof changed: %d of %d   vtable slot count changed: %d   parent changed: %d" % (
        sum(c1[c]["size"] != c2[c]["size"] for c in common), len(common),
        sum(slots(c1[c]) != slots(c2[c]) for c in common), sum(c1[c]["super"] != c2[c]["super"] for c in common)))
    P("  new: " + ", ".join(sorted(set(c2) - set(c1))))
    with open(out / "class_delta.csv", "w", encoding="utf-8", newline="") as f:
        w = csv.writer(f)
        w.writerow(["class", "sizeof_c1", "sizeof_other", "vslots_c1", "vslots_other", "b_identical",
                    "b_same_shape", "b_ge90", "b_75_90", "b_lt75_or_gone"])
        for c in common:
            w.writerow([c, hex(c1[c]["size"]), hex(c2[c]["size"]), slots(c1[c]), slots(c2[c])] + byc.get(c, [0] * 5))
        for c in sorted(set(c2) - set(c1)):
            w.writerow([c, "", hex(c2[c]["size"]), "", slots(c2[c])] + [""] * 5)
    (out / "compare_summary.txt").write_text("\n".join(lines) + "\n", encoding="utf-8")
    P("-> %s" % out)


if __name__ == "__main__":
    main()
