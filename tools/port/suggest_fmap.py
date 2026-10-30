#!/usr/bin/env python3
"""Proposes db/fmap_manual.csv: function pairs the matcher missed that the call sites of matched callers name.

tools/port/eligible.py rejects a pair when a call goes to X in the base and to Y in this client and (X, Y) is not
a pair of db/fmap.csv. When the two callers have the same shape (that is what category A/B/C says) the callee at
the same place is, in practice, the same function: this takes every such (X, Y) whose two functions are
instruction-similar (same loose token stream, ratio >= --min-sim) and neither of which is in db/fmap.csv, and
writes them. Run tools/port/match.py afterwards (it reads db/fmap_manual.csv), then maps.py, layout.py, vslots.py,
eligible.py.

usage: python tools/port/suggest_fmap.py [--min-sim 0.75]"""
from __future__ import annotations

import argparse
import collections
import csv
import difflib
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import common  # noqa: E402
from common import ROOT  # noqa: E402


# pairs read by hand: the same function under the renamed / rewritten code
KNOWN = {
    (0x1000e8a0, 0x1000f8d0),   # NCFrameCtrl::SetTitleText
}


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--min-sim", type=float, default=0.75)
    a = ap.parse_args()
    m1, m2 = common.c1_model(), common.c2_model()
    fm = list(csv.DictReader((ROOT / "db" / "fmap.csv").open(encoding="utf-8")))
    used1 = {int(r["c1_va"], 16) for r in fm}
    used2 = {int(r["c2_va"], 16) for r in fm}
    callers = collections.defaultdict(list)
    for r in csv.DictReader((ROOT / "db" / "eligible.csv").open(encoding="utf-8")):
        m = re.match(r"insn \d+ calls ([0-9a-f]+), C2 calls ([0-9a-f]+)", r["reason"])
        if m:
            callers[(int(m.group(1), 16), int(m.group(2), 16))].append(r["name"])
    out = []
    for (v1, v2), who in sorted(callers.items()):
        x, y = m1.by_va.get(v1), m2.by_va.get(v2)
        if x is None or y is None or v1 in used1 or v2 in used2:
            continue
        sim = difflib.SequenceMatcher(None, x["l"], y["l"], autojunk=False).ratio()
        ratio = (len(x["l"]) + 1) / (len(y["l"]) + 1)
        # several callers of the same shape agree: the call site names the pair even when the code was rewritten
        ok = (sim >= a.min_sim and 0.5 <= ratio <= 2.0) or (len(who) >= 2 and sim >= 0.3) or (v1, v2) in KNOWN
        print("%s %08x -> %08x sim %.2f (%d callers: %s)%s" % ("+" if ok else "-", v1, v2, sim, len(who), who[0],
                                                                "" if ok else "  rejected"))
        if ok:
            out.append((v1, v2, sim, len(who), who[0]))
    p = ROOT / "db" / "fmap_manual.csv"
    with p.open("w", encoding="utf-8", newline="") as f:
        f.write("# Function pairs the matcher missed; the call sites of matched callers name them (tools/port/suggest_fmap.py)\n")
        w = csv.writer(f)
        w.writerow(["c1_va", "c2_va", "sim", "callers", "example"])
        for v1, v2, sim, n, ex in out:
            w.writerow(["%08x" % v1, "%08x" % v2, "%.2f" % sim, n, ex])
    print("%d pairs -> %s" % (len(out), p))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
