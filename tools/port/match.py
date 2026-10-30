#!/usr/bin/env python3
"""Function map C1 -> C2: which C2 function is the rebuilt version of each C1 function.

Both builds come from the same source tree at two dates, and the linker lays the object files out
in the same order, so the functions of the two .text sections are two mostly-aligned sequences.
Method (the order is what makes it cheap and safe):
  1. anchors: functions that are unique in BOTH images and identical under a fingerprint that
     survives a class layout change (the wide strings they refer to, the strict token hash, the
     offset-masked hash, the loose hash, in that order of confidence);
  2. the longest increasing chain of anchors (an anchor that crosses the order is dropped);
  3. between two consecutive anchors, a monotone alignment (DP) of the functions in between, by
     instruction similarity (>= SIM_MIN);
  4. the vtable pass: inside a class, a slot between two aligned slots with the same distance on
     both sides is the same method (a body that changed too much to match by similarity).
Output: db/fmap.csv (c1_va,c2_va,how,sim,cat,size1,size2,kind,class,name) and a report.
cat: A strict-equal | B equal after masking field offsets | C same shape (constants differ) |
     D different shape (needs a diff-assisted port)

usage: python tools/port/match.py [--c1 DIR]"""
from __future__ import annotations

import argparse
import bisect
import collections
import csv
import difflib
import json
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import common  # noqa: E402
from common import ROOT  # noqa: E402

K = 4
SIM_MIN = 0.55
DP_CELLS = 1_500_000


def shingles(toks):
    return {tuple(toks[i:i + K]) for i in range(max(1, len(toks) - K + 1))}


def sim(x, y):
    cx, cy = x.get("vcls"), y.get("vcls")
    if cx and cy and not (cx & cy):
        return 0.0      # vtable members of unrelated classes are never the same method
    a, b = x["sh"], y["sh"]
    inter = len(a & b)
    if not inter:
        return 0.0
    jac = inter / len(a | b)
    if jac < 0.12:
        return jac * 0.5
    if len(x["l"]) * len(y["l"]) < 160000:
        return difflib.SequenceMatcher(None, x["l"], y["l"], autojunk=False).ratio()
    return jac


def lis(pairs):
    """Longest strictly increasing chain of (i, j) pairs, i already distinct."""
    pairs = sorted(pairs)
    tails, idx, prev = [], [], [-1] * len(pairs)
    for k, (_, j) in enumerate(pairs):
        p = bisect.bisect_left(tails, j)
        if p == len(tails):
            tails.append(j)
            idx.append(k)
        else:
            tails[p] = j
            idx[p] = k
        prev[k] = idx[p - 1] if p else -1
    out, k = [], idx[-1] if idx else -1
    while k >= 0:
        out.append(pairs[k])
        k = prev[k]
    return out[::-1]


def unique_pairs(f1, f2, key, minlen, tok="d"):
    d1, d2 = collections.defaultdict(list), collections.defaultdict(list)
    for i, x in enumerate(f1):
        k = key(x)
        if k is not None:
            d1[k].append(i)
    for j, y in enumerate(f2):
        k = key(y)
        if k is not None:
            d2[k].append(j)
    out = []
    for k, a in d1.items():
        b = d2.get(k)
        if b and len(a) == 1 and len(b) == 1 and len(f1[a[0]][tok]) >= minlen:
            out.append((a[0], b[0]))
    return out


def gap_dp(f1, f2, g1, g2, match, how):
    n, m = len(g1), len(g2)
    dp = [[0.0] * (m + 1) for _ in range(n + 1)]
    bt = [[0] * (m + 1) for _ in range(n + 1)]
    cache = {}
    for p in range(1, n + 1):
        x = f1[g1[p - 1]]
        for q in range(1, m + 1):
            y = f2[g2[q - 1]]
            s = sim(x, y)
            cache[(p, q)] = s
            s = s if s >= SIM_MIN else 0.0
            best, h = dp[p - 1][q], 1
            if dp[p][q - 1] > best:
                best, h = dp[p][q - 1], 2
            if s and dp[p - 1][q - 1] + s > best:
                best, h = dp[p - 1][q - 1] + s, 3
            dp[p][q] = best
            bt[p][q] = h
    p, q = n, m
    while p > 0 and q > 0:
        h = bt[p][q]
        if h == 3:
            match[g1[p - 1]] = g2[q - 1]
            how[g1[p - 1]] = "dp"
            p -= 1
            q -= 1
        elif h == 1:
            p -= 1
        else:
            q -= 1


def align(f1, f2, match, how):
    """DP-fill every gap between consecutive matched pairs. Big gaps are split on the best
    >= 0.8 pairs found inside them (recursion on the anchors)."""
    pairs = sorted(match.items())
    bounds = [(-1, -1)] + pairs + [(len(f1), len(f2))]
    big = []
    for (a1, a2), (b1, b2) in zip(bounds, bounds[1:]):
        g1, g2 = list(range(a1 + 1, b1)), list(range(a2 + 1, b2))
        if not g1 or not g2:
            continue
        if len(g1) * len(g2) > DP_CELLS:
            big.append((g1, g2))
            continue
        gap_dp(f1, f2, g1, g2, match, how)
    return big


def split_big(f1, f2, g1, g2, match, how):
    """A gap too big for the DP: candidates by shared shingles, chain of the good ones, recurse."""
    post = collections.defaultdict(list)
    for j in g2:
        for s in f2[j]["sh"]:
            post[s].append(j)
    cand = []
    for i in g1:
        cnt = collections.Counter()
        for s in f1[i]["sh"]:
            for j in post.get(s, ()):
                cnt[j] += 1
        for j, c in cnt.most_common(2):
            if c >= 0.5 * len(f1[i]["sh"]):
                cand.append((i, j))
    ch = lis(cand)
    good = [(i, j) for i, j in ch if sim(f1[i], f2[j]) >= 0.8]
    for i, j in good:
        match[i] = j
        how[i] = "gap-anchor"
    return len(good)


def attach_classes(m1, m2, c1cls, c2cls):
    for m, cl in ((m1, c1cls), (m2, c2cls)):
        for c in cl.values():
            for sl in c.get("vtable_slots", []):
                if sl.get("import"):
                    continue
                x = m.by_va.get(sl["va"])
                if x is not None:
                    x.setdefault("vcls", set()).add(c["cpp"])


def drop_inconsistent(f1, f2, match, how):
    bad = [i for i, j in match.items() if not _compatible(f1[i], f2[j])]
    for i in bad:
        del match[i]
        del how[i]
    return len(bad)


def _compatible(x, y):
    cx, cy = x.get("vcls"), y.get("vcls")
    return not (cx and cy and not (cx & cy))


def classify(x, y):
    if x["s"] == y["s"]:
        return "A"
    if x["d"] == y["d"]:
        return "B"
    if x["l"] == y["l"]:
        return "C"
    return "D"


def vtable_pass(m1, m2, c1cls, c2cls, match, how):
    """Inside each class, pair the vtable slots between two aligned slots at the same distance."""
    f1, f2 = m1.funcs, m2.funcs
    added = 0
    for cpp, a in c1cls.items():
        b = c2cls.get(cpp)
        if not b:
            continue
        s1 = [(s["slot"], s["va"]) for s in a.get("vtable_slots", []) if not s.get("import")]
        s2 = [(s["slot"], s["va"]) for s in b.get("vtable_slots", []) if not s.get("import")]
        pos2 = {va: sl for sl, va in s2}
        anchors = []  # (slot1, slot2)
        for sl, va in s1:
            i = m1.idx.get(va)
            j = match.get(i) if i is not None else None
            if j is not None and f2[j]["va"] in pos2:
                anchors.append((sl, pos2[f2[j]["va"]]))
        anchors = sorted(set(anchors))
        # keep a monotone chain only
        anchors = lis(anchors)
        by1 = dict(s1)
        by2 = dict(s2)
        for (p1, p2), (q1, q2) in zip(anchors, anchors[1:]):
            if q1 - p1 == q2 - p2 and q1 - p1 > 1:
                for d in range(1, q1 - p1):
                    v1, v2 = by1.get(p1 + d), by2.get(p2 + d)
                    if v1 is None or v2 is None:
                        continue
                    i, j = m1.idx.get(v1), m2.idx.get(v2)
                    if i is None or j is None or i in match or j in match.values():
                        continue
                    match[i] = j
                    how[i] = "vslot"
                    added += 1
    return added


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--c1", default=None)
    a = ap.parse_args()
    if a.c1:
        common.C1_ROOT = Path(a.c1)
    t0 = time.time()
    m1, m2 = common.c1_model(), common.c2_model()
    f1, f2 = m1.funcs, m2.funcs
    print("C1 %d functions, C2 %d functions (%.0fs)" % (len(f1), len(f2), time.time() - t0))
    for x in f1 + f2:
        x["sh"] = shingles(x["l"])

    c1cls = {c["cpp"]: c for c in json.load(open(common.C1_ROOT / "analysis" / "classes.json", encoding="utf-8"))}
    c2p = ROOT / "analysis" / "classes.json"
    if not c2p.exists():
        c2p = ROOT / "analysis" / "c2" / "analysis" / "classes.json"
    c2cls = {c["cpp"]: c for c in json.load(open(c2p, encoding="utf-8"))}
    attach_classes(m1, m2, c1cls, c2cls)

    match, how = {}, {}
    # 1. anchors, most reliable first
    def strkey(x):
        s = sorted(t for t in x["str"] if len(t) >= 6)
        return tuple(s) if s else None
    tiers = [
        ("str", unique_pairs(f1, f2, strkey, 6)),
        ("strict", unique_pairs(f1, f2, lambda x: x["hs"], 5, "s")),
        ("dmask", unique_pairs(f1, f2, lambda x: x["hd"], 8)),
        ("loose", unique_pairs(f1, f2, lambda x: x["hl"], 14, "l")),
    ]
    cand = {}
    for name, ps in tiers:
        print("  anchors %-7s %5d" % (name, len(ps)))
        for i, j in ps:
            if i not in cand:
                cand[i] = (j, name)
    for i, j in lis([(i, j) for i, (j, _) in cand.items()]):
        match[i] = j
        how[i] = cand[i][1]
    print("  chain %d of %d anchors" % (len(match), len(cand)))
    print("  dropped %d anchors of unrelated classes" % drop_inconsistent(f1, f2, match, how))

    # 2-3. alignment of the gaps, then again inside the big ones
    for rnd in range(3):
        big = align(f1, f2, match, how)
        n = sum(split_big(f1, f2, g1, g2, match, how) for g1, g2 in big)
        print("  round %d: %d matched, %d big gaps, %d new anchors" % (rnd, len(match), len(big), n))
        if not big or not n:
            break

    # 4. vtable pass
    for _ in range(2):
        n = vtable_pass(m1, m2, c1cls, c2cls, match, how)
        print("  vtable pass: +%d" % n)

    # 5. manual pairs (db/fmap_manual.csv: c1_va,c2_va,...; tools/port/suggest_fmap.py proposes them from the call
    # sites of matched callers): both ends must still be free
    mpath = ROOT / "db" / "fmap_manual.csv"
    if mpath.exists():
        idx1 = {int(x["va"]): i for i, x in enumerate(f1)}
        idx2 = {int(y["va"]): j for j, y in enumerate(f2)}
        used2 = set(match.values())
        added = 0
        for r in csv.DictReader(l for l in mpath.open(encoding="utf-8") if not l.startswith("#")):
            i = idx1.get(int(r["c1_va"], 16))
            j = idx2.get(int(r["c2_va"], 16))
            if i is None or j is None or i in match or j in used2:
                continue
            match[i] = j
            how[i] = "manual"
            used2.add(j)
            added += 1
        print("  manual pairs: +%d" % added)

    # output
    out = ROOT / "db" / "fmap.csv"
    rows = []
    for i, j in sorted(match.items()):
        x, y = f1[i], f2[j]
        s = 1.0 if x["s"] == y["s"] else difflib.SequenceMatcher(None, x["d"], y["d"], autojunk=False).ratio()             if len(x["d"]) * len(y["d"]) < 160000 else sim(x, y)
        rows.append([("%08x" % x["va"]), ("%08x" % y["va"]), how[i], "%.3f" % s, classify(x, y),
                     x["size"], y["size"], x.get("kind", ""), x.get("cls", ""), x.get("nm", "")])
    with out.open("w", encoding="utf-8", newline="") as f:
        w = csv.writer(f)
        w.writerow(["c1_va", "c2_va", "how", "sim", "cat", "size1", "size2", "kind", "class", "name"])
        w.writerows(rows)
    print("-> %s (%d pairs, %.0fs)" % (out, len(rows), time.time() - t0))

    # report
    game = [i for i, x in enumerate(f1) if x.get("kind") == "game"]
    mg = [i for i in game if i in match]
    tot = sum(f1[i]["size"] for i in game)
    cat = collections.Counter()
    catb = collections.Counter()
    for r in rows:
        if r[7] == "game":
            cat[r[4]] += 1
            catb[r[4]] += r[5]
    print("C1 game functions %d, matched %d; bytes %d of %d (%.1f%%)" % (
        len(game), len(mg), sum(f1[i]["size"] for i in mg), tot, 100.0 * sum(f1[i]["size"] for i in mg) / tot))
    for c, d in (("A", "strict equal"), ("B", "equal after masking field offsets"),
                 ("C", "same shape, constants differ"), ("D", "different shape")):
        print("  %s %-36s %5d funcs %8d bytes" % (c, d, cat[c], catb[c]))
    inv = set(match.values())
    new = [y for j, y in enumerate(f2) if j not in inv]
    print("C2 functions without a C1 counterpart: %d (%d bytes)" % (len(new), sum(y["size"] for y in new)))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
