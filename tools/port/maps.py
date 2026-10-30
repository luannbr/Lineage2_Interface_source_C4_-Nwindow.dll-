#!/usr/bin/env python3
"""Operand-level maps C1 -> C2, read off the aligned instructions of the matched functions.

db/fmap.csv says which C2 function is the rebuilt C1 function. Here each pair is walked
instruction by instruction (same shape: 1:1; different shape: only the equal blocks of a diff on
the loose tokens) and every pair of operands that differs is recorded:
  addresses  call targets, global variables, tables, strings, vtables: c1 VA -> c2 VA, by votes
  field offsets  [reg+disp] with reg not esp/ebp: (c1 disp, c2 disp), by votes per class of the
                 function (tools/port/layout.py turns them into a class layout map)
  constants  an immediate that differs in an aligned pair: reported per function (the port of
             category C functions has to change these by hand)
Writes db/amap.csv (kind,c1,c2,votes,share,alts), analysis/port/votes.json and
analysis/port/constants.csv.

usage: python tools/port/maps.py"""
from __future__ import annotations

import collections
import csv
import difflib
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import common  # noqa: E402
from common import ROOT  # noqa: E402

D_MIN_SIM = 0.6   # a different-shape pair is walked only above this similarity


def raw(img, ranges):
    """Per instruction (after the same filter as common.norm): (mnemonic, [operand tuples])."""
    out = []
    for lo, hi in ranges:
        for i in img.md.disasm(img.read(lo, hi - lo + 1), lo):
            m = i.mnemonic
            if m in ("int3", "nop"):
                continue
            ops = []
            for op in i.operands:
                if op.type == 1:      # X86_OP_REG
                    ops.append(("r", i.reg_name(op.reg)))
                elif op.type == 2:    # X86_OP_IMM
                    ops.append(("i", op.imm & 0xFFFFFFFF))
                elif op.type == 3:    # X86_OP_MEM
                    mm = op.mem
                    ops.append(("m", i.reg_name(mm.base) if mm.base else "",
                                i.reg_name(mm.index) if mm.index else "", mm.scale, mm.disp & 0xFFFFFFFF, op.size))
            out.append((m, ops))
    return out


WRITES = {"mov", "lea", "xor", "add", "sub", "and", "or", "shl", "shr", "sar", "inc", "dec", "imul", "movzx",
          "movsx", "neg", "not", "xchg", "adc", "sbb", "rol", "ror", "pop", "xadd", "bsf", "bsr", "mul", "div", "idiv"}


def this_info(ins):
    """Which registers hold `this` in a thiscall method, flow-insensitively:
    ecx until the first instruction that overwrites it, and a callee-saved register that is
    written exactly once, by an early `mov reg, ecx`. Returns (ecx_until, set of regs)."""
    ecx_until = len(ins)
    for k, (m, ops) in enumerate(ins):
        if m in ("call", "rep movsd", "rep movsb", "rep stosd", "rep stosb") or m.startswith("rep") or                 (ops and ops[0] == ("r", "ecx") and m in WRITES and m != "pop"):
            ecx_until = k
            break
        if m in ("mul", "div", "idiv", "cdq") and False:
            pass
    regs = set()
    for reg in ("esi", "edi", "ebx"):
        writes = [k for k, (m, ops) in enumerate(ins) if ops and ops[0] == ("r", reg) and m in WRITES and m != "pop"]
        if len(writes) == 1:
            k = writes[0]
            m, ops = ins[k]
            if m == "mov" and ops[1] == ("r", "ecx") and k <= ecx_until:
                regs.add(reg)
    return ecx_until, regs


def is_this(info, k, base):
    ecx_until, regs = info
    return (base == "ecx" and k <= ecx_until) or base in regs


def section_kind(img, va):
    s = img.section(va)
    if s == ".text":
        return "code"
    if s in (".rdata", ".data"):
        return "data"
    return None


def main() -> int:
    m1, m2 = common.c1_model(), common.c2_model()
    i1, i2 = m1.img, m2.img
    hi1 = i1.base + i1.pe.OPTIONAL_HEADER.SizeOfImage
    hi2 = i2.base + i2.pe.OPTIONAL_HEADER.SizeOfImage
    rows = list(csv.DictReader((ROOT / "db" / "fmap.csv").open(encoding="utf-8")))

    addr_votes = collections.defaultdict(collections.Counter)    # (kind, c1) -> Counter(c2)
    disp_votes = collections.defaultdict(collections.Counter)    # class -> Counter("d1:d2")
    consts = []
    walked = 0
    for r in rows:
        if r["kind"] != "game":
            continue
        a, b = int(r["c1_va"], 16), int(r["c2_va"], 16)
        x, y = m1.by_va[a], m2.by_va[b]
        cat, sim = r["cat"], float(r["sim"])
        if cat == "D" and sim < D_MIN_SIM:
            continue
        ra, rb = raw(i1, x["ranges"]), raw(i2, y["ranges"])
        if len(ra) != len(x["l"]) or len(rb) != len(y["l"]):
            continue     # the decoders disagree on this function: skip rather than misalign
        if x["l"] == y["l"]:
            blocks = [(0, len(ra), 0)]
        else:
            sm = difflib.SequenceMatcher(None, x["l"], y["l"], autojunk=False)
            blocks = [(i1_, i2_, j1_) for tag, i1_, i2_, j1_, j2_ in sm.get_opcodes() if tag == "equal"]
        walked += 1
        ta, tb = this_info(ra), this_info(rb)
        cls = r["class"] or "?"
        w = 1.0 if cat in "ABC" else 0.5
        for lo, hi, j0 in blocks:
            for k in range(lo, hi):
                (ma, oa), (mb, ob) = ra[k], rb[j0 + k - lo]
                if ma != mb or len(oa) != len(ob):
                    continue
                for p, q in zip(oa, ob):
                    if p[0] != q[0]:
                        continue
                    if p[0] == "i":
                        v1, v2 = p[1], q[1]
                        in1 = i1.base <= v1 < hi1
                        in2 = i2.base <= v2 < hi2
                        if in1 and in2:
                            kd = section_kind(i1, v1)
                            if kd:
                                addr_votes[(kd, v1)][v2] += 1
                        elif v1 != v2 and not in1 and not in2 and not ma.startswith("j") and ma != "call":
                            consts.append((r["c1_va"], r["c2_va"], k, ma, v1, v2))
                    elif p[0] == "m":
                        base1, idx1, d1 = p[1], p[2], p[4]
                        base2, d2 = q[1], q[4]
                        if base1 != base2 and base1 and base2 and not (base1 in ("esp", "ebp") or base2 in ("esp", "ebp")):
                            pass   # a register renamed: the displacement still pairs
                        if i1.base <= d1 < hi1 and i2.base <= d2 < hi2:
                            kd = section_kind(i1, d1)
                            if kd:
                                addr_votes[(kd, d1)][d2] += 1
                        elif is_this(ta, k, base1) and is_this(tb, j0 + k - lo, base2):
                            if d1 < 0x10000 and d2 < 0x10000:
                                disp_votes[cls]["%x:%x" % (d1, d2)] += w
    print("walked %d pairs" % walked)

    # function targets from the function map itself, as the strongest votes
    for r in rows:
        addr_votes[("code", int(r["c1_va"], 16))][int(r["c2_va"], 16)] += 1000

    out = ROOT / "db" / "amap.csv"
    n = amb = 0
    with out.open("w", encoding="utf-8", newline="") as f:
        w = csv.writer(f)
        w.writerow(["kind", "c1", "c2", "votes", "share", "alts"])
        for (kd, v1), c in sorted(addr_votes.items(), key=lambda kv: (kv[0][0], kv[0][1])):
            tot = sum(c.values())
            (v2, nn), *rest = c.most_common()
            share = nn / tot
            amb += share < 0.9
            n += 1
            w.writerow([kd, "%08x" % v1, "%08x" % v2, nn, "%.2f" % share,
                        " ".join("%08x:%d" % (a, b) for a, b in rest[:3])])
    print("-> %s: %d addresses, %d ambiguous (<90%% share)" % (out, n, amb))

    pdir = ROOT / "analysis" / "port"
    pdir.mkdir(parents=True, exist_ok=True)
    (pdir / "votes.json").write_text(json.dumps({k: dict(v) for k, v in disp_votes.items()}, indent=0),
                                     encoding="utf-8")
    with (pdir / "constants.csv").open("w", encoding="utf-8", newline="") as f:
        w = csv.writer(f)
        w.writerow(["c1_va", "c2_va", "insn", "mnemonic", "c1_value", "c2_value"])
        for c in consts:
            w.writerow([c[0], c[1], c[2], c[3], hex(c[4]), hex(c[5])])
    print("-> analysis/port/votes.json (%d classes), constants.csv (%d differing immediates)" % (
        len(disp_votes), len(consts)))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
