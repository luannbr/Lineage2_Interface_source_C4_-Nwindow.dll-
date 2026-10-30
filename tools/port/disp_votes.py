#!/usr/bin/env python3
"""Where a field really is: the displacement pairs of our compiled copy and the stock function it replaces.

tools/port/verify_data.py says THAT a function's field displacements differ; this says WHICH field moved where.
Each hooked function whose displacement sets differ (db/verify_data.csv, status diff) is aligned instruction by
instruction with the stock function (the copy has the same shape: that is what made it eligible); every pair of
memory operands `[reg + disp]` on `this` (a register that holds ecx of the entry) with two different
displacements is a vote  class: our disp -> stock disp. Our disp is where our header put the field (a gap-rule
guess), the stock one is where the C3 code reads it.

Output: a table per class (ours -> stock, votes, functions, the header field at our displacement) and
db/disp_votes.csv. A pair seen in two or more functions and in no contradiction is a safe line for
db/layout_manual.csv.

usage: python tools/port/disp_votes.py [--log build/smoke_system/NWindowDecomp.log] [--dll build/bin/Release/NWindowDecomp.dll]
       [--all]        every hooked function, not only the ones verify_data flags"""
from __future__ import annotations

import argparse
import bisect
import collections
import csv
import difflib
import json
import re
import sys
from pathlib import Path

import pefile
from capstone import CS_ARCH_X86, CS_MODE_32, Cs
from capstone.x86 import X86_OP_IMM, X86_OP_MEM, X86_OP_REG

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import verify_hooks as vh  # noqa: E402
from nwd.image import Image  # noqa: E402

BASES = ("ecx", "esi", "edi", "ebx", "edx", "eax", "ebp")


def instructions(md, read, extents):
    out = []
    for a, b in extents:
        for ins in vh.decode_skipping_tables(md, vh.Code(read, {}, lambda v: extents, lambda v: True), a, b):
            out.append(ins)
    return out


def this_regs(ins_list):
    """For every instruction: the set of registers that hold `this` before it (ecx at entry, copies of it)."""
    cur = {"ecx"}
    seq = []
    for ins in ins_list:
        seq.append(frozenset(cur))
        ops = ins.operands
        mn = ins.mnemonic
        if mn == "mov" and len(ops) == 2 and ops[0].type == X86_OP_REG and ops[1].type == X86_OP_REG:
            dst, src = ins.reg_name(ops[0].reg), ins.reg_name(ops[1].reg)
            if src in cur:
                cur.add(dst)
            else:
                cur.discard(dst)
        elif ops and ops[0].type == X86_OP_REG and mn in ("mov", "xor", "pop", "lea", "movzx", "movsx", "imul", "sub", "add",
                                                           "and", "or", "shl", "shr", "sar", "neg", "not", "inc", "dec",
                                                           "setne", "sete", "cmovne"):
            r = ins.reg_name(ops[0].reg)
            if mn in ("add", "sub", "and", "or", "shl", "shr", "sar", "inc", "dec") and r in cur:
                pass            # this + something: no longer this
            if r in cur and not (mn == "mov" and len(ops) == 2 and ops[1].type == X86_OP_REG and ins.reg_name(ops[1].reg) in cur):
                cur.discard(r)
        elif mn == "call":
            cur.discard("eax")
            cur.discard("edx")
            cur.discard("ecx")
    return seq


def tok(ins):
    """alignment token: mnemonic and operand kinds (registers and displacements are what differs)"""
    parts = [ins.mnemonic]
    for op in ins.operands:
        if op.type == X86_OP_REG:
            parts.append("r")
        elif op.type == X86_OP_IMM:
            parts.append("i")
        elif op.type == X86_OP_MEM:
            parts.append("m")
    return " ".join(parts)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--log", type=Path, default=ROOT / "build" / "smoke_system" / "NWindowDecomp.log")
    ap.add_argument("--dll", type=Path, default=ROOT / "build" / "bin" / "Release" / "NWindowDecomp.dll")
    ap.add_argument("--all", action="store_true")
    a = ap.parse_args()
    md = Cs(CS_ARCH_X86, CS_MODE_32)
    md.detail = True
    stock = Image()
    granges = {}
    for r in csv.DictReader((ROOT / "analysis/ghidra/functions.tsv").open(encoding="utf-8"), delimiter="\t"):
        granges[int(r["va"], 16)] = [tuple(int(x, 16) for x in rg.split("-")) for rg in r["ranges"].split(",")]
    for r in csv.DictReader(l for l in (ROOT / "db/not_functions.csv").open(encoding="utf-8") if not l.startswith("#")):
        owner, frag = int(r["part_of"], 16), int(r["va"], 16)
        if owner in granges and frag in granges:
            granges[owner] = granges[owner] + granges[frag]

    pe = pefile.PE(str(a.dll))
    base = pe.OPTIONAL_HEADER.ImageBase
    mem = pe.get_memory_mapped_image()
    symbols = vh.our_symbols(a.dll)
    starts = [x for x, _ in symbols]
    o_text = next(s for s in pe.sections if s.Name.startswith(b".text"))
    o_hi = base + o_text.VirtualAddress + o_text.Misc_VirtualSize

    def o_read(va, n):
        return mem[va - base:va - base + n]

    def o_extent(va):
        i = bisect.bisect_right(starts, va)
        return [(va, starts[i] if i < len(starts) else o_hi)]

    flagged = None
    if not a.all:
        flagged = set()
        for r in csv.DictReader((ROOT / "db" / "verify_data.csv").open(encoding="utf-8")):
            if r["status"] == "diff" and "disps" in r["detail"]:
                flagged.add(r["c2_va"])
    fn = {r["va"]: r for r in csv.DictReader((ROOT / "db" / "functions.csv").open(encoding="utf-8"))}
    layout = json.load(open(ROOT / "db" / "layout.json", encoding="utf-8"))

    votes = collections.defaultdict(lambda: collections.defaultdict(set))   # class -> (ours, stock) -> {function}
    nfun = 0
    for line in a.log.read_text(encoding="latin-1").splitlines():
        m = vh.LOG_RE.match(line)
        if not m:
            continue
        va, name, rva = int(m.group(1), 16), m.group(2), int(m.group(3), 16)
        if flagged is not None and "%08x" % va not in flagged:
            continue
        ext_s = [(x, y + 1) for x, y in granges.get(va, [(va, va + 0x3F)])]
        si = instructions(md, stock.read, ext_s)
        oi = instructions(md, o_read, o_extent(base + rva))
        if not si or not oi:
            continue
        ts, to = this_regs(si), this_regs(oi)
        sm = difflib.SequenceMatcher(None, [tok(i) for i in si], [tok(i) for i in oi], autojunk=False)
        nfun += 1
        cls = (fn.get("%08x" % va) or {}).get("class") or name.split("::")[0]
        for blk in sm.get_matching_blocks():
            for k in range(blk.size):
                x, y = si[blk.a + k], oi[blk.b + k]
                for p, q in zip(x.operands, y.operands):
                    if p.type != X86_OP_MEM or q.type != X86_OP_MEM:
                        continue
                    bp = x.reg_name(p.mem.base) if p.mem.base else ""
                    bq = y.reg_name(q.mem.base) if q.mem.base else ""
                    if not bp or not bq or bp in ("esp", "ebp") or bq in ("esp", "ebp"):
                        continue
                    if p.mem.index or q.mem.index:
                        continue
                    ds, do = p.mem.disp & 0xFFFFFFFF, q.mem.disp & 0xFFFFFFFF
                    if ds >= 0x20 and do >= 0x20 and ds < 0x10000 and do < 0x10000 and ds != do:
                        on_this = bp in ts[blk.a + k] and bq in to[blk.b + k]
                        votes[cls][(do, ds, on_this)].add("%s" % name.split("::")[-1])

    def field_at(cls, off):
        out = []
        n = cls
        seen = 0
        while n in layout and seen < 12:
            for r in layout[n]["fields"]:
                if r["c2"] is not None and r["c2"] <= off < r["c2"] + max(r["ext"], 1):
                    out.append("%s.%s%s" % (n, r["name"], "" if r["c2"] == off else "+%x" % (off - r["c2"])))
            n = layout[n].get("parent")
            seen += 1
        return out

    # the fields the generated headers declare at an offset: `Type Name;  // +0xOFF`
    decl = collections.defaultdict(list)
    cur = None
    fld = re.compile(r"^\s+([A-Za-z_][\w:<>\*, ]*?[\s\*])(\w+)(\[[^\]]*\])?;\s*//\s*\+0x([0-9A-Fa-f]+)")
    for hp in sorted((ROOT / "include" / "nwindow").glob("*.h")) + [ROOT / "include" / "ue2" / "UE2Engine.h"]:
        for ln in hp.read_text(encoding="utf-8", errors="replace").splitlines():
            m = re.match(r"^(?:class|struct)\s+(?:\w+_API\s+)?(\w+)", ln)
            if m:
                cur = m.group(1)
            m = fld.match(ln)
            if m and cur:
                decl[int(m.group(4), 16)].append("%s.%s (%s)" % (cur, m.group(2), m.group(1).strip()))

    def decl_at(off):
        return decl.get(off, [])

    rows = []
    for cls in sorted(votes):
        by_ours = collections.defaultdict(dict)
        for (do, ds, on_this), who in votes[cls].items():
            by_ours[do][(ds, on_this)] = who
        for do in sorted(by_ours):
            targets = by_ours[do]
            for key in sorted(targets, key=lambda t: -len(targets[t])):
                ds, on_this = key
                who = sorted(targets[key])
                where = "|".join(field_at(cls, do)) if on_this else "(other object) " + "|".join(decl_at(do)[:4])
                rows.append((cls, do, ds, len(who), where, ",".join(who[:4])))
    with (ROOT / "db" / "disp_votes.csv").open("w", encoding="utf-8", newline="") as f:
        w = csv.writer(f)
        w.writerow(["class", "ours", "stock", "functions", "header_field", "examples"])
        for r in rows:
            w.writerow(["%s" % r[0], "%x" % r[1], "%x" % r[2], r[3], r[4], r[5]])
    print("%d functions aligned, %d (class, ours -> stock) pairs -> db/disp_votes.csv" % (nfun, len(rows)))
    for cls in sorted(votes, key=lambda c: -sum(len(w) for w in votes[c].values()))[:12]:
        print(cls, sum(len(w) for w in votes[cls].values()))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
