#!/usr/bin/env python3
"""Class layout map C1 -> C2: where each field the C1 headers declare sits in the C2 object.

Evidence = analysis/port/votes.json (tools/port/maps.py): for every aligned instruction pair of
the matched functions, the displacement a [reg+disp] operand had in C1 and has in C2, counted per
class of the function. The C1 headers (tools/port/hdr.py) give the fields, their offsets and
extents. Per class, parents first:
  * the candidates for a field are the votes whose C1 displacement falls inside the field's extent
    (start = c2 displacement - the distance into the field), counted over the class and all its
    descendants (their methods touch the inherited fields too);
  * the fields are then chosen as the heaviest chain with strictly increasing C2 offsets (fields
    keep their declaration order; the compiler lays them out in it);
  * a field with no evidence between two mapped neighbours whose gaps are the same size in both
    builds is placed by that gap.
status: mapped (votes) | derived (gap rule) | unmapped (no evidence: a function that uses it
cannot be ported automatically) | conflict (extent overlaps the next mapped field in C2).
Writes db/layout.json and prints a summary.

usage: python tools/port/layout.py"""
from __future__ import annotations

import collections
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import hdr  # noqa: E402
import common  # noqa: E402
from common import ROOT, C1_ROOT  # noqa: E402


def load_classes(root: Path):
    p = root / "analysis" / "classes.json"
    if not p.exists():
        p = root / "analysis" / "c2" / "analysis" / "classes.json"
    out = {c["cpp"]: c for c in json.load(open(p, encoding="utf-8"))}
    # the object layout and the vtable follow the C++ base class, which the registered UClass
    # super does not always name (NCTradeItemWnd is registered under NCWnd, derives from NCItemWnd)
    for n, par in cpp_parents().items():
        if n in out and par in out and out[n]["super"] != par:
            out[n]["uclass_super"] = out[n]["super"]
            out[n]["super"] = par
    return out


_CPP_PARENTS = None


def cpp_parents():
    global _CPP_PARENTS
    if _CPP_PARENTS is None:
        _CPP_PARENTS = {}
        for hp in sorted((C1_ROOT / "include" / "nwindow").glob("*.h")):
            _, cds = hdr.parse(hp)
            for cd in cds:
                if cd.parent:
                    _CPP_PARENTS[cd.name] = cd.parent
    return _CPP_PARENTS


def heaviest_chain(items):
    """items: [(order, c2, weight, label)] sorted by order; strictly increasing c2, max weight."""
    n = len(items)
    best = [0.0] * n
    prev = [-1] * n
    for i in range(n):
        best[i] = items[i][2]
        for j in range(i):
            if items[j][1] < items[i][1] and best[j] + items[i][2] > best[i]:
                best[i] = best[j] + items[i][2]
                prev[i] = j
    if not n:
        return []
    k = max(range(n), key=lambda i: best[i])
    out = []
    while k >= 0:
        out.append(items[k])
        k = prev[k]
    return out[::-1]


def main() -> int:
    c1 = load_classes(C1_ROOT)
    c2 = load_classes(ROOT)
    votes = json.load(open(ROOT / "analysis" / "port" / "votes.json", encoding="utf-8"))
    MANUAL_LAYOUT = collections.defaultdict(list)
    mpath = ROOT / "db" / "layout_manual.csv"
    if mpath.exists():
        import csv
        for mr in csv.DictReader(l for l in mpath.open(encoding="utf-8") if not l.startswith("#")):
            MANUAL_LAYOUT[mr["class"]].append(mr)

    # C1 headers
    decls = {}
    for p in sorted((C1_ROOT / "include" / "nwindow").glob("*.h")):
        _, cds = hdr.parse(p)
        for cd in cds:
            if cd.name in c1:
                decls[cd.name] = cd

    kids = collections.defaultdict(list)
    for n, c in c1.items():
        kids[c["super"]].append(n)

    def descendants(n):
        out, st = [], [n]
        while st:
            x = st.pop()
            out.append(x)
            st.extend(kids.get(x, []))
        return out

    order = []
    seen = set()

    def visit(n):
        if n in seen or n not in c1:
            return
        seen.add(n)
        visit(c1[n]["super"])
        order.append(n)

    for n in c1:
        visit(n)

    result = {}
    stats = collections.Counter()
    for n in order:
        cd = decls.get(n)
        if cd is None or n not in c2:
            continue
        flds = sorted(cd.fields, key=lambda f: f.off)
        size1, size2 = c1[n]["size"], c2[n]["size"]
        ends = [flds[i + 1].off if i + 1 < len(flds) else size1 for i in range(len(flds))]
        agg = collections.defaultdict(collections.Counter)   # field index -> Counter(c2 start)
        vt = collections.Counter()
        for d in descendants(n):
            for k, w in votes.get(d, {}).items():
                a, b = k.split(":")
                vt[(int(a, 16), int(b, 16))] += w
        for (d1, d2), w in vt.items():
            if d2 >= size2 or d1 >= size1:
                continue
            for i, f in enumerate(flds):
                if f.off <= d1 < ends[i]:
                    start = d2 - (d1 - f.off)
                    if 0 <= start < size2:
                        agg[i][start] += w
                    break
        # piecewise-constant delta (c2 - c1) over the fields in declaration order: a change of delta
        # means fields were inserted or removed there; a change costs LAMBDA votes. Fields without
        # evidence continue the delta of their neighbours (status "derived").
        nf = len(flds)
        wd = [collections.Counter() for _ in range(nf)]       # per field: delta -> votes
        for i, c in agg.items():
            for start, w in c.items():
                wd[i][start - flds[i].off] += w
        tot = collections.Counter()
        for c in wd:
            for d, w in c.items():
                tot[d] += w
        pn = c1[n]["super"]
        prior = (c2[pn]["size"] - c1[pn]["size"]) if pn in c1 and pn in c2 else None
        LAMBDA = 4.0       # a change of delta inside the fields
        L_BUF = 1.0        # ... right after a big buffer, whose type may have changed (TCHAR[128] -> FString)
        L_END = 3.0        # ending at a delta other than the sizeof's
        L_OVER = 8.0       # a small field placed over the previous one
        extents = [ends[i] - flds[i].off for i in range(nf)]
        size_delta = size2 - size1
        cand = {d for d, _ in tot.most_common(14)} | {0, size_delta}
        if prior is not None:
            cand.add(prior)
        for i in range(nf):                       # what a replaced buffer leaves behind
            if extents[i] >= 0x20:
                for dd in list(cand):
                    for keep in (0, 4, 8, 0xC, 0x10):
                        cand.add(dd - (extents[i] - keep))
        deltas = sorted(cand)
        INF = 1e18
        cost = [[INF] * len(deltas) for _ in range(nf)]
        back = [[-1] * len(deltas) for _ in range(nf)]
        for i in range(nf):
            for a, d in enumerate(deltas):
                e = -wd[i].get(d, 0.0)
                if i == 0:
                    pr = 0.0 if (prior is None or d == prior) else LAMBDA
                    cost[i][a] = e + pr
                else:
                    best, bb = INF, -1
                    lam = L_BUF if extents[i - 1] >= 0x20 else LAMBDA
                    for b, d0 in enumerate(deltas):
                        c = cost[i - 1][b] + (0.0 if a == b else lam)
                        if extents[i - 1] < 0x20 and flds[i].off + d < flds[i - 1].off + d0 + extents[i - 1]:
                            c += L_OVER
                        if c < best:
                            best, bb = c, b
                    cost[i][a] = best + e
                    back[i][a] = bb
                if i == nf - 1:
                    # the last field ends at the sizeof, give or take what C2 appended
                    if d != size_delta:
                        cost[i][a] += L_END
                    if flds[i].off + d + (extents[i] if extents[i] < 0x20 else 0) > size2:
                        cost[i][a] += L_OVER
        placed = {}
        if nf:
            a = min(range(len(deltas)), key=lambda x: cost[nf - 1][x])
            seq = [0] * nf
            for i in range(nf - 1, -1, -1):
                seq[i] = a
                a = back[i][a]
            for i in range(nf):
                d = deltas[seq[i]]
                w = wd[i].get(d, 0.0)
                share = (w / sum(wd[i].values())) if wd[i] else 0.0
                placed[i] = (flds[i].off + d, "mapped" if w > 0 else "derived", w, share if w > 0 else 1.0)
        # a placement outside this class's own bytes in C2 is no placement: the class was rewritten
        # beyond what the evidence can follow (NCDialogBox lost two 512-byte buffers)
        lo2 = c2[pn]["size"] if pn in c2 else 0x28
        for i in list(placed):
            c2off = placed[i][0]
            if c2off < lo2 or c2off + min(extents[i], 4) > size2:
                del placed[i]
        # two fields cannot share bytes: of an overlapping pair the one with less evidence goes
        while True:
            order_ = sorted(placed, key=lambda i: (placed[i][0], i))
            hit = None
            for a_, b_ in zip(order_, order_[1:]):
                if placed[a_][0] + min(extents[a_], 4) > placed[b_][0]:
                    hit = (a_, b_)
                    break
            if hit is None:
                break
            a_, b_ = hit
            loser = a_ if placed[a_][2] < placed[b_][2] or (placed[a_][2] == placed[b_][2] and placed[a_][1] == "derived") else b_
            del placed[loser]
        rec = []
        for i, f in enumerate(flds):
            p = placed.get(i)
            rec.append(dict(name=f.name, typ=f.typ, arr=f.arr, c1=f.off, ext=ends[i] - f.off,
                            c2=p[0] if p else None, status=p[1] if p else "unmapped",
                            votes=round(p[2], 1) if p else 0, share=round(p[3], 2) if p else 0))
        # hand-made placements (db/layout_manual.csv): where the code of this client puts a field the votes
        # could not place (or placed wrong), a field that is gone, and the new type of a field that changed it
        for mr in MANUAL_LAYOUT.get(n, ()):
            for r in rec:
                if r["name"] == mr["field"]:
                    if mr["c2"] in ("gone", ""):
                        r["c2"], r["status"] = None, "manual-gone"
                    else:
                        r["c2"], r["status"] = int(mr["c2"], 16), "manual"
                    if mr.get("retype"):
                        r["retype"] = mr["retype"]
                    break
            else:
                print("layout_manual.csv: %s has no field %s" % (n, mr["field"]), file=sys.stderr)
        # the hand-placed fields are anchors: a field after one (in the base layout) that the votes put where the
        # anchor's own shift says it cannot be (before it, or inside it) is moved by the anchor's shift instead
        anchors = sorted((r for r in rec if r["status"] == "manual" and r["c2"] is not None), key=lambda r: r["c1"])
        if anchors:
            for r in sorted(rec, key=lambda r: r["c1"]):
                if r["status"].startswith("manual") or r["c2"] is None and r["status"] != "unmapped":
                    continue
                below = [x for x in anchors if x["c1"] <= r["c1"]]
                if not below:
                    continue
                a_ = below[-1]
                want = r["c1"] + (a_["c2"] - a_["c1"])
                if r["c2"] is None:
                    continue
                inside = a_["c2"] <= r["c2"] < a_["c2"] + max(a_["ext"], 4) and r["c1"] != a_["c1"]
                before = r["c2"] < a_["c2"] and r["c1"] > a_["c1"]
                if inside or before:
                    r["c2"], r["status"] = want, "derived"
        # a field the anchors' shift pushes past the end of the object is not placed
        if anchors:
            for r_ in rec:
                if r_["c2"] is not None and not r_["status"].startswith("manual") and r_["c2"] + min(r_["ext"], 4) > size2:
                    r_["c2"], r_["status"] = None, "unmapped"
        # two fields cannot share bytes: of an overlapping pair the one with less standing goes (derived, then votes;
        # a hand placement stays)
        rank = {"derived": 0, "mapped": 1, "manual": 2}

        def standing(r_):
            return rank.get(r_["status"].split("+")[0], 0)
        while anchors:
            mp0 = sorted((r for r in rec if r["c2"] is not None), key=lambda r: (r["c2"], r["c1"]))
            hit = None
            for r_, n_ in zip(mp0, mp0[1:]):
                if r_["c2"] + min(r_["ext"], 4) > n_["c2"]:
                    hit = (r_, n_)
                    break
            if hit is None:
                break
            a_, b_ = hit
            if standing(a_) == standing(b_) == 2:
                break
            loser = a_ if standing(a_) < standing(b_) or (standing(a_) == standing(b_) and a_["c1"] > b_["c1"]) else b_
            loser["c2"], loser["status"] = None, "unmapped"
        # overlap check in C2
        mp = sorted((r for r in rec if r["c2"] is not None), key=lambda r: r["c2"])
        for r, nx in zip(mp, mp[1:]):
            if r["c2"] + r["ext"] > nx["c2"]:
                r["status"] += "+overlap"
        for r in rec:
            stats[r["status"].split("+")[0]] += 1
        result[n] = dict(size1=size1, size2=size2, parent=c1[n]["super"], fields=rec)

    (ROOT / "db" / "layout.json").write_text(json.dumps(result, indent=1), encoding="utf-8")
    print("classes %d; fields %s" % (len(result), dict(stats)))
    nover = sum(1 for c in result.values() for r in c["fields"] if "overlap" in r["status"])
    print("fields whose C1 extent overlaps the next mapped field in C2:", nover)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
