#!/usr/bin/env python3
"""Compares the exported vtables of the C1 Core/Engine with the C2 ones, slot by slot, by the
decorated name each slot points at (pure slots: `_purecall`; a slot nobody exports: `sub_`).

The C1 binaries are read from the sibling C1 project (read-only reference):
  --c1 <C1 project>\\orig   (default: $NWD_C1_ROOT\\orig)

  python tools/ue2_vtable_diff.py                 # one line per class: slots C1 -> C2, first difference
  python tools/ue2_vtable_diff.py UCanvas AActor  # the whole alignment of those classes
"""
from __future__ import annotations

import argparse
import difflib
import os
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import engine_vtables as ev  # noqa: E402

# NWD_C1_ROOT: a checkout of the C1 project
C1_ORIG = (Path(os.environ["NWD_C1_ROOT"]) / "orig" if os.environ.get("NWD_C1_ROOT") else None)


def labels(d: ev.Dll, sym: str) -> list[str]:
    out = []
    for _, _, lab in d.vtable(sym):
        out.append(lab)
    return out


def norm(lab: str) -> str:
    # unexported bodies (sub_xxxx) differ in address only: compare as an unknown body
    return "sub_" if lab.startswith("sub_") else lab


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("classes", nargs="*")
    ap.add_argument("--c1", default=str(C1_ORIG) if C1_ORIG else None, required=C1_ORIG is None, help="default: $NWD_C1_ROOT/orig")
    a = ap.parse_args()
    c1 = Path(a.c1)
    syms: dict[tuple[str, str], tuple] = {}
    for dn in ("Core", "Engine"):
        d1, d2 = ev.dll(dn, c1), ev.dll(dn)
        for s in sorted(x for x in d2.exp if x.startswith("??_7")):
            syms[(dn, s)] = (d1, d2, s in d1.exp)
    n_same = 0
    for (dn, s), (d1, d2, in1) in syms.items():
        cls = s[4:].split("@@")[0]
        if a.classes and cls not in a.classes:
            continue
        l2 = [norm(x) for x in labels(d2, s)]
        if not in1:
            print(f"{dn}.{s}: NEW in C2 ({len(l2)} slots)")
            continue
        l1 = [norm(x) for x in labels(d1, s)]
        if l1 == l2 and not a.classes:
            n_same += 1
            continue
        first = next((i for i, (x, y) in enumerate(zip(l1, l2)) if x != y), min(len(l1), len(l2)))
        print(f"{dn}.{s}: {len(l1)} -> {len(l2)} slots, first difference at slot {first}")
        if a.classes:
            sm = difflib.SequenceMatcher(None, l1, l2, autojunk=False)
            for op, i1, i2, j1, j2 in sm.get_opcodes():
                if op == "equal":
                    continue
                for i in range(i1, i2):
                    print(f"    - C1 {i:3d} {l1[i]}")
                for j in range(j1, j2):
                    print(f"    + C2 {j:3d} {l2[j]}")
    if not a.classes:
        print(f"{n_same} vtables identical")
        gone = [s for s in ev.dll("Engine", c1).exp if s.startswith("??_7") and s not in ev.dll("Engine").exp]
        gone += [s for s in ev.dll("Core", c1).exp if s.startswith("??_7") and s not in ev.dll("Core").exp]
        print(f"{len(gone)} C1 vtables not exported by C2: {', '.join(gone[:40])}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
