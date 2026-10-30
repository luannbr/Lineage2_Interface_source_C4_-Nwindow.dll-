#!/usr/bin/env python3
"""Every virtual call with a literal slot number (`nw::VirtualCall<R>(obj, 45, ...)`, `VirtualCall<void>(Engine->NetHandler, 0x264 / 4, ...)`) of a hooked
body must be a slot that the stock function calls too (`call [reg + N * 4]`).

Why: a slot written by hand is read off the stock call site, or it is wrong. NCGDevice::DrawPledgeCrest / DrawAllyCrest called the NetHandler slots 43 / 44
where the stock calls 45 / 46 (GetPledgeCrestTex / GetAllyCrestTex): verify_hooks compares the SETS of virtual offsets of the two functions, but 316 hooks
carry a note that explains a difference away (an inlined callee, a tail forward, a guard's unwind path), and a wrong literal slot hides behind such a note
as well as anywhere else. This scan reads the source, not the compiled code: for each literal slot it asks whether the stock function (its Ghidra ranges and
EH funclets, the pieces the analysis cut off, and the functions it calls directly, one level: our body may inline a callee) has a virtual call at slot * 4.

For a hook unit the slots of its own body and of the inline helpers its closure runs (the same resolution as check_literals) are compared with the
stock function's. A literal slot the stock does not use is printed with the slots it does use; a VirtualCall in a definition that no hooked body runs is
listed as unchecked (`?`). Exit 1 with --strict when a slot of a hook unit is not in the stock function.

usage: python tools/port/check_vslots.py [--strict] [--all]"""
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
sys.path.insert(0, str(ROOT / "tools" / "port"))
import check_literals as cl  # noqa: E402

VCALL = re.compile(r"VirtualCall\s*<[^;{}()]*?>\s*\(")
MEMOFF = re.compile(r"\[(e[a-ds][xip]|e[sd]i|ebp)(?: \+ (0x[0-9a-f]+|\d+))?\]$")


def eval_slot(expr: str):
    e = expr.strip()
    e = re.sub(r"\(\s*(?:INT|int|DWORD|UINT)\s*\)", "", e)
    if not re.fullmatch(r"[0-9a-fA-FxX+\-*/() ]+", e):
        return None
    try:
        v = eval(re.sub(r"\b(0[xX][0-9a-fA-F]+)\b", lambda m: str(int(m.group(1), 16)), e).replace("/", "//"), {"__builtins__": {}}, {})
        return int(v)
    except Exception:
        return None


class StockSlots:
    def __init__(self, stock):
        self.stock = stock
        self.cache: dict[int, set[int]] = {}
        self.callees: dict[int, set[int]] = {}

    def own(self, va: int) -> tuple[set[int], set[int]]:
        """(virtual offsets, direct callees) of one stock function"""
        if va in self.cache:
            return self.cache[va], self.callees[va]
        st = self.stock
        offs: set[int] = set()
        calls: set[int] = set()
        rng = list(st.ranges.get(va) or [(va, va + (int(st.rows[va]["size"]) if va in st.rows else 64) - 1)])
        # the analysis cuts a function at a loop head: the rest is the anonymous function right after it (nobody calls it)
        a0, b0 = rng[0]
        nxt = b0 + 1
        extra = []
        while st.nocallers.get(nxt) and nxt in st.ranges and len(extra) < 4:
            extra.append(nxt)
            nxt = max(y for x, y in st.ranges[nxt]) + 1
        for e in extra:
            rng += st.ranges[e]
        held: dict[str, int] = {}
        for a, b in rng:
            held.clear()
            for ins in st.decode(a, b + 1):
                mn, op = ins.mnemonic, ins.op_str
                parts = [p.strip() for p in op.split(",", 1)] if op else []
                if mn in ("call", "jmp"):
                    m = MEMOFF.search(op) if op.startswith("dword ptr") else None
                    if m:
                        offs.add(int(m.group(2), 0) if m.group(2) else 0)
                    elif op in held:
                        offs.add(held[op])
                    else:
                        t = re.match(r"0x([0-9a-f]+)$", op)
                        if t:
                            calls.add(int(t.group(1), 16))
                    continue
                if parts and parts[0] in ("eax", "ecx", "edx", "ebx", "esi", "edi", "ebp"):
                    held.pop(parts[0], None)
                    if mn == "mov" and len(parts) == 2:
                        m = MEMOFF.search(parts[1]) if parts[1].startswith("dword ptr") else None
                        if m:
                            held[parts[0]] = int(m.group(2), 0) if m.group(2) else 0
        self.cache[va], self.callees[va] = offs, calls
        return offs, calls

    def slots(self, va: int) -> set[int]:
        offs, calls = self.own(va)
        out = set(offs)
        lo, hi = 0x10001000, 0x1019C810
        for c in calls:
            if lo <= c < hi and c in self.stock.rows:
                o2, calls2 = self.own(c)
                out |= o2
                for c2 in calls2:       # a forwarder's target
                    if lo <= c2 < hi and c2 in self.stock.rows and int(self.stock.rows[c]["size"]) <= 40:
                        out |= self.own(c2)[0]
        return out


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--strict", action="store_true")
    ap.add_argument("--all", action="store_true", help="also list the slots that match")
    a = ap.parse_args()
    tree = cl.Tree()
    stock = cl.Stock()
    ss = StockSlots(stock)
    # the literal slots of every definition
    sites: dict[int, list[tuple[int, int, str]]] = {}        # id(def) -> [(line, slot, expr)]
    for d in tree.alldefs:
        if d.kind not in ("fn", "macro"):
            continue
        body = d.body
        for m in VCALL.finditer(body):
            op = body.index("(", m.end() - 2) if body[m.end() - 1] != "(" else m.end() - 1
            end = cl.balanced(body, op)
            args = cl.split_args(body[op + 1:end - 1])
            if len(args) < 2:
                continue
            slot = eval_slot(args[1])
            if slot is None:
                continue
            sites.setdefault(id(d), []).append((d.line + body[:m.start()].count("\n"), slot, args[1].strip()))
    defs_by_id = {id(d): d for d in tree.alldefs}
    # the units that run each definition
    owners: dict[int, set[int]] = {}

    def walk(d, depth, ctx, root, stack):
        if id(d) in sites:
            owners.setdefault(id(d), set()).add(root)
        if depth >= cl.MAX_DEPTH:
            return
        owner = d.owner or ctx
        ctor = bool(owner) and d.name in (owner, "~" + owner)
        for call in tree.calls_of(d):
            for c in tree.candidates(call, owner, d.file, ctor):
                if id(c) not in stack:
                    walk(c, depth + 1, owner, root, stack + (id(d),))
    for u in tree.units:
        if u["def"] is not None:
            walk(u["def"], 0, u["cls"], u["va"], ())
    nchecked = nbad = nunchecked = 0
    for did, lst in sorted(sites.items(), key=lambda kv: (defs_by_id[kv[0]].file, defs_by_id[kv[0]].line)):
        d = defs_by_id[did]
        units = set(owners.get(did, set()))
        if d.unit:
            units |= {u["va"] for u in tree.units if u["def"] is d}
        for line, slot, expr in lst:
            if not units:
                nunchecked += 1
                if a.all:
                    print("?%s:%d %s::%s slot %d (+0x%X) [%s]: no hooked body runs it" % (d.file, line, d.owner, d.name, slot, slot * 4, expr))
                continue
            ok = False
            union: set[int] = set()
            for va in units:
                if va not in stock.rows:
                    continue
                sl = ss.slots(va)
                union |= sl
                if slot * 4 in sl:
                    ok = True
            nchecked += 1
            if not ok:
                nbad += 1
                near = sorted(x for x in union if abs(x - slot * 4) <= 0x20)
                print("%s:%d %s::%s slot %d (+0x%X) [%s]: the stock function(s) %s call no virtual at +0x%X; nearby: %s; all: %s" % (
                    d.file, line, d.owner, d.name, slot, slot * 4, expr, " ".join("%08x" % v for v in sorted(units)[:4]), slot * 4,
                    " ".join("+0x%X" % x for x in near) or "-", " ".join("+0x%X" % x for x in sorted(union))[:160]))
            elif a.all:
                print(" %s:%d %s::%s slot %d (+0x%X) ok" % (d.file, line, d.owner, d.name, slot, slot * 4))
    print("check_vslots: %d literal virtual slot(s) of hooked code checked, %d not used by the stock function(s), %d in code no hooked body runs" % (
        nchecked, nbad, nunchecked))
    return 1 if (a.strict and nbad) else 0


if __name__ == "__main__":
    sys.exit(main())
