#!/usr/bin/env python3
"""Constructor pairs the class tables give and the function map lacks: db/fmap.csv rows with how=ctor.

analysis/classes.json holds, per class, the address of its constructor (`ctor`, and `internal_ctor` for the ones with
a second entry); both builds have the same classes, so the pair (base address, this client's address) is known
without looking at the code. tools/port/match.py leaves a part of them unpaired (a constructor is one of many
near-identical functions: no unique anchor), and tools/port/gen.py then finds no row in db/eligible.csv for them and
stubs the constructor, and with it the constructors of every class derived from it.

This appends a row for each such pair whose two ends are both free in db/fmap.csv, with the category the matcher's
own classify() gives (A strict-equal, B equal after masking field offsets, C same shape, D different shape).
Run after match.py and before eligible.py:   python tools/port/match.py; python tools/port/ctor_pairs.py

usage: python tools/port/ctor_pairs.py"""
from __future__ import annotations

import csv
import difflib
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import common  # noqa: E402
import match  # noqa: E402
from common import ROOT, C1_ROOT  # noqa: E402


def addr(v):
    if v is None:
        return None
    return v if isinstance(v, int) else int(v, 16)


def main() -> int:
    c1 = {c["cpp"]: c for c in json.load(open(C1_ROOT / "analysis" / "classes.json", encoding="utf-8"))}
    c2 = {c["cpp"]: c for c in json.load(open(ROOT / "analysis" / "classes.json", encoding="utf-8"))}
    path = ROOT / "db" / "fmap.csv"
    rows = list(csv.reader(path.open(encoding="utf-8")))
    head, body = rows[0], rows[1:]
    used1 = {r[0] for r in body}
    used2 = {r[1] for r in body}
    m1, m2 = common.c1_model(), common.c2_model()
    added = []
    for n, a in sorted(c1.items()):
        b = c2.get(n)
        if not b:
            continue
        for key in ("ctor", "internal_ctor"):
            v1, v2 = addr(a.get(key)), addr(b.get(key))
            if not v1 or not v2:
                continue
            k1, k2 = "%08x" % v1, "%08x" % v2
            if k1 in used1 or k2 in used2:
                continue
            x, y = m1.by_va.get(v1), m2.by_va.get(v2)
            if x is None or y is None:
                continue
            if x["s"] == y["s"]:
                s = 1.0
            elif len(x["d"]) * len(y["d"]) < 160000:
                s = difflib.SequenceMatcher(None, x["d"], y["d"], autojunk=False).ratio()
            else:
                s = 0.0
            added.append([k1, k2, "ctor", "%.3f" % s, match.classify(x, y), x["size"], y["size"],
                          x.get("kind", ""), x.get("cls", n), x.get("nm", "") or "%s::%s" % (n, n)])
            used1.add(k1)
            used2.add(k2)
    body.extend(added)
    body.sort(key=lambda r: r[0])
    with path.open("w", encoding="utf-8", newline="") as f:
        w = csv.writer(f)
        w.writerow(head)
        w.writerows(body)
    import collections
    print("%d constructor pairs added to db/fmap.csv (%s)" % (
        len(added), dict(collections.Counter(r[4] for r in added))))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
