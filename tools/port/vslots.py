#!/usr/bin/env python3
"""Virtual slot map C1 -> C2 per class: which C2 vtable slot each C1 slot became.

The C2 engine classes grew virtuals (UObject got ClearL2Game at slot 22, UL2ConsoleWnd 48 more...),
so every class's vtable shifts, and slots appear inside the vtable of a class. Per class, a
monotone alignment of the C1 slots with the C2 slots, scored by
  imports      the same decorated name (an engine function the vtable points at)   10
  functions    db/fmap.csv says the two slot functions are the same function         10
  inherited    the slot is the parent's, and the parent's map pairs the two          6
  gap          between two aligned slots with the same distance on both sides         1
Writes db/vslots.json: {class: {map: {c1slot: c2slot}, new: [{slot, va}], gone: [c1 slot],
                                 n1, n2, parent}}.

usage: python tools/port/vslots.py   (after tools/port/match.py)"""
from __future__ import annotations

import csv
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import common  # noqa: E402
from common import ROOT, C1_ROOT  # noqa: E402
from layout import load_classes  # noqa: E402


def align(s1, s2, score):
    n, m = len(s1), len(s2)
    dp = [[0] * (m + 1) for _ in range(n + 1)]
    bt = [[0] * (m + 1) for _ in range(n + 1)]
    for i in range(1, n + 1):
        for j in range(1, m + 1):
            best, h = dp[i - 1][j], 1
            if dp[i][j - 1] > best:
                best, h = dp[i][j - 1], 2
            sc = score(s1[i - 1], s2[j - 1])
            if sc and dp[i - 1][j - 1] + sc > best:
                best, h = dp[i - 1][j - 1] + sc, 3
            dp[i][j], bt[i][j] = best, h
    pairs = []
    i, j = n, m
    while i > 0 and j > 0:
        h = bt[i][j]
        if h == 3:
            pairs.append((i - 1, j - 1))
            i -= 1
            j -= 1
        elif h == 1:
            i -= 1
        else:
            j -= 1
    return pairs[::-1]


def main() -> int:
    c1 = load_classes(C1_ROOT)
    c2 = load_classes(ROOT)
    fmap = {}
    for r in csv.DictReader((ROOT / "db" / "fmap.csv").open(encoding="utf-8")):
        fmap[int(r["c1_va"], 16)] = int(r["c2_va"], 16)

    done = {}
    out = {}
    MANUAL = []
    mp_path = ROOT / "db" / "vslots_manual.csv"
    if mp_path.exists():
        for r in csv.DictReader(l for l in mp_path.open(encoding="utf-8") if not l.startswith("#")):
            MANUAL.append((r["class"], int(r["c1_slot"]), int(r["c2_slot"])))

    def vt(c):
        return [(s["slot"], s["va"], s.get("import")) for s in c.get("vtable_slots", [])]

    def run(n):
        if n in done:
            return done[n]
        a, b = c1[n], c2.get(n)
        if b is None:
            done[n] = None
            return None
        pm = None
        if a["super"] in c1:
            pm = run(a["super"])
        s1, s2 = vt(a), vt(b)
        par1 = {s: va for s, va, _ in vt(c1[a["super"]])} if a["super"] in c1 else {}
        par2 = {s: va for s, va, _ in vt(c2[a["super"]])} if a["super"] in c2 else {}

        def score(x, y):
            if x[2] and y[2]:
                return 10 if x[2] == y[2] else 0
            if not x[2] and not y[2] and fmap.get(x[1]) == y[1]:
                return 10
            if pm is not None and pm["map"].get(x[0]) == y[0] and par1.get(x[0]) == x[1] and par2.get(y[0]) == y[1]:
                return 6
            # an inherited slot keeps its identity when the class overrides it: the parent's map pairs the slots
            if pm is not None and x[0] in par1 and y[0] in par2 and pm["map"].get(x[0]) == y[0]:
                return 5
            return 0

        pairs = align(s1, s2, score)
        mp = {s1[i][0]: s2[j][0] for i, j in pairs}
        how = {s1[i][0]: "score" for i, j in pairs}
        # gaps of the same size between two aligned slots
        keys = sorted(mp)
        for p, q in zip(keys, keys[1:]):
            g1, g2 = q - p, mp[q] - mp[p]
            if g1 == g2 and g1 > 1:
                for d in range(1, g1):
                    mp[p + d] = mp[p] + d
                    how[p + d] = "gap"
        # an inherited slot is the parent's slot whatever the class does to it (an override or not): the alignment can pair
        # two overrides of different slots by their (shared) bodies; the parent's map is the authority
        if pm is not None:
            s2set = {x[0] for x in s2}
            for x in sorted(par1):
                t = pm["map"].get(x)
                if t is None or t not in s2set or mp.get(x) == t or x not in {y[0] for y in s1}:
                    continue
                for k in [k for k, v in mp.items() if v == t and k != x]:
                    del mp[k]
                    how.pop(k, None)
                mp[x] = t
                how[x] = "inherited"
        # the class's own virtuals (past the parent's table) that nothing paired: when both builds have as many,
        # they are the same virtuals in the same order
        if pm is not None:
            n1p, n2p = len(par1), len(par2)
            own1 = [x[0] for x in s1 if x[0] >= n1p and x[0] not in mp]
            own2 = [x[0] for x in s2 if x[0] >= n2p and x[0] not in set(mp.values())]
            if own1 and len(own1) == len(own2) and len([x for x in s1 if x[0] >= n1p]) == len(own1):
                for a_, b_ in zip(own1, own2):
                    mp[a_] = b_
                    how[a_] = "own"
            else:
                # the other build has more (C4 added virtuals to the class): a slot of this class's own that no function
                # paired takes the first free slot of the other build between its neighbours' (the order is kept). Wrong
                # guesses cost nothing: the callers that read another slot fail verify_hooks (`vcalls`) either way
                free2 = [x for x in own2]
                for a_ in own1:
                    lo = max([v for k, v in mp.items() if k < a_], default=-1)
                    hi = min([v for k, v in mp.items() if k > a_], default=10 ** 9)
                    cand = [b_ for b_ in free2 if lo < b_ < hi]
                    if cand:
                        mp[a_] = cand[0]
                        how[a_] = "order"
                        free2.remove(cand[0])
        # hand-made corrections (db/vslots_manual.csv): a slot that moved out of order (the monotone alignment
        # cannot say that) or that the function map could not pair; the old target of a slot is released
        chain, c = [], n
        while c is not None:           # the class and its ancestors: a descendant inherits the moved slot
            chain.append(c)
            c = c1[c]["super"] if c in c1 else None
        for (mc, a1, a2) in MANUAL:
            if mc not in chain:
                continue
            for k in [k for k, v in mp.items() if v == a2 and k != a1]:
                del mp[k]
                how.pop(k, None)
            mp[a1] = a2
            how[a1] = "manual"
        used = set(mp.values())
        new = [dict(slot=s, va=va) for s, va, imp in s2 if s not in used]
        gone = [s for s, _, _ in s1 if s not in mp]
        res = dict(map=mp, how=how, new=new, gone=gone, n1=len(s1), n2=len(s2), parent=a["super"])
        done[n] = res
        out[n] = res
        return res

    for n in c1:
        run(n)
    serial = {n: dict(map={str(k): v for k, v in r["map"].items()},
                      how={str(k): v for k, v in r["how"].items()},
                      new=r["new"], gone=r["gone"], n1=r["n1"], n2=r["n2"], parent=r["parent"])
              for n, r in out.items() if r}
    (ROOT / "db" / "vslots.json").write_text(json.dumps(serial, indent=0), encoding="utf-8")
    nn = sum(len(r["new"]) for r in out.values() if r)
    ng = sum(len(r["gone"]) for r in out.values() if r)
    print("classes %d; slots without a C1 counterpart %d, C1 slots gone %d" % (len(serial), nn, ng))
    for n in ("NCObject", "NCCmdTarget", "NCWnd", "NControl", "NCButton", "NConsoleWnd"):
        r = out.get(n)
        if r:
            sh = sorted(set(v - int(k) for k, v in r["map"].items()))
            print("  %-12s C1 %3d slots, C2 %3d; shifts %s; new %s; gone %s" % (
                n, r["n1"], r["n2"], sh, [x["slot"] for x in r["new"]][:12], r["gone"][:8]))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
