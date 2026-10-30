#!/usr/bin/env python3
"""Every tiny (<= 20 bytes) stock function that the source names by address: what the stock bytes do against what the call assumes.

Why: the linker folds the C4 getters, setters and forwarders that have the same bytes into one address (and the db names the folded address after
one of the classes), and the C3 -> C4 address map pairs a C3 function with whichever neighbour carries those bytes. A call by address of such a
function is wrong when the bytes do something else on another field: NCDeliverWnd::SetTargetID jumped to 0x10084BD0 (`mov [ecx+0x168], eax`, the
grid's target) and stored past the end of the 0x114-byte window (the stock calls 0x10084BF0, `mov [ecx+0x110], eax`); the buying window's owner and
adena setters were 0x101029D0 / 0x100E6B80 (store +0x114 / +0x120) where the stock calls 0x101029E0 / 0x100E3370 (+0x118 / +0x11C); the macro
delete asked for 0x10068020 (+0x264, the editor's save) where the stock asks for 0x10068040 (+0x268). check_literals sees those only in a hooked
body whose stock function does not use the literal; this scan looks at the tiny functions themselves, wherever they are named (hooked bodies,
helpers, headers, NW_STOCK stubs).

For each such function it prints the stock bytes in words (getter / setter / forwarder / import thunk / virtual forward) and, for each place that
names it, the call and what could be told about its receiver: the class (typed through the same rules as check_literals: casts, locals, fields,
return types) and, for a getter or a setter of [ecx + N], the field the class' header puts at N (comments `// +0xNN`) - or that N is past the
object (classes.json size / NW_ASSERT_SIZE), which is a store or a read outside the window. Each place is marked `*` when a hooked body runs
it (check_literals compares the address with the stock function that body replaces) and `?` when none does (stubs of virtuals, constructors, headers:
only this scan and a reader look at it). Flags:
  BEYOND   [ecx + N] with N >= the receiver class' size (a store or a read outside the object): the only flag --strict fails on (KNOWN_BEYOND
           lists the ones read against the stock: the tab bar's buttons are NCTabButton, 0x2D8 bytes, declared as NCRadioButton here)
  NOFIELD  the receiver class and its ancestors have no field at N in the headers (a layout hole or a field of another class): a hint
  RET      the call reads a result (`StockCallAt<non-void>`) of a setter, whose eax is its argument: a hint
  ARGS     the arguments passed differ from the stock `ret N` (4 bytes each; check_stockargs does it too): a hint

usage: python tools/port/check_tiny.py [--strict] [--flags-only] [--unchecked]"""
from __future__ import annotations

import argparse
import csv
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
sys.path.insert(0, str(ROOT / "tools" / "port"))
import check_literals as cl  # noqa: E402

CALL = re.compile(r"(?:StockCallAt|StockCall|NewStock|NewStockObj)\s*(?:<([^<>]*(?:<[^<>]*>)?[^<>]*)>)?\s*\(\s*0[xX](10[0-2][0-9A-Fa-f]{5})\s*(?:,|\))")
ADDR = re.compile(r"Addr\s*\(\s*0[xX](10[0-2][0-9A-Fa-f]{5})\s*\)")
STUB = re.compile(r"__stock\(0[xX](10[0-2][0-9A-Fa-f]{5})\)")
KNOWN_BEYOND = {("NCRadioButton", "src/nwindow/NCTabControl.cpp")}     # the tab buttons (NCTabButton, 0x2D8 bytes) seen as their base
GENERIC = {"NCWnd", "NCObject", "NCCmdTarget", "NControl", "UObject", "BYTE", "void", "INT", "DWORD", "FString"}   # the real class is a derived one
FIELD_LINE = re.compile(r"//\s*\+0x([0-9A-Fa-f]+)")
CLASS_START = re.compile(r"^\s*(?:class|struct)\s+(?:\w+\s+)*?(\w+)\s*(?::[^{;]*)?\{?\s*$")


def describe(img, va: int, size: int) -> tuple[str, str, int | None, str]:
    """(kind, one-line text, N of [ecx + N] or None, op) for the stock bytes"""
    ins = list(img.disasm(va, size + 4, stop_at_ret=True))
    text = "; ".join("%s %s" % (i.mnemonic, i.op_str) for i in ins)
    ret = 0
    for i in ins:
        if i.mnemonic in ("ret", "retn"):
            ret = int(i.op_str, 16) if i.op_str else 0
    mem = [re.search(r"\[ecx(?: \+ (0x[0-9a-f]+))?\]", i.op_str) for i in ins if i.mnemonic in ("mov", "movzx", "movsx", "lea", "add", "cmp", "test", "fld", "movss")]
    mem = [m for m in mem if m]
    n = int(mem[0].group(1), 16) if mem and mem[0].group(1) else (0 if mem else None)
    if len(ins) == 1 and ins[0].mnemonic == "jmp":
        t = ins[0].op_str
        if t.startswith("dword ptr"):
            return "import", text, None, ""
        return "forward", text, None, t
    if ins and ins[-1].mnemonic in ("ret", "retn") and len(mem) >= 1 and len(ins) <= 4:
        stores = [i for i in ins if i.mnemonic == "mov" and re.match(r"dword ptr \[ecx", i.op_str)]
        if stores:
            return "setter", text, n, "ret %d" % ret
        if ins[0].mnemonic in ("mov", "movzx", "movsx", "lea") and "[ecx" in ins[0].op_str.split(",")[-1]:
            return "getter", text, n, "ret %d" % ret
    if any(i.mnemonic == "jmp" and "[edx +" in i.op_str for i in ins):
        return "vforward", text, None, ""
    return "other", text, n, "ret %d" % ret


def load_class_sizes(tree) -> dict[str, int]:
    import json
    out = {}
    for c in json.load(open(ROOT / "analysis" / "classes.json", encoding="utf-8")):
        out[c["cpp"]] = int(c["size"])
    for p in sorted((ROOT / "include").rglob("*.h")):
        for m in re.finditer(r"NW_ASSERT_SIZE\s*\(\s*(\w+)\s*,\s*(0[xX][0-9A-Fa-f]+|\d+)\s*\)", p.read_text(encoding="utf-8", errors="replace")):
            out.setdefault(m.group(1), int(m.group(2), 0))
    return out


def load_fields() -> dict[tuple[str, int], str]:
    """(class, offset) -> field name, from the `// +0xNN` comments of the headers"""
    out: dict[tuple[str, int], str] = {}
    for p in sorted((ROOT / "include").rglob("*.h")):
        cur = None
        for line in p.read_text(encoding="utf-8", errors="replace").split("\n"):
            m = CLASS_START.match(line)
            if m and ";" not in line.split("//")[0]:
                cur = m.group(1)
            fm = FIELD_LINE.search(line)
            if cur and fm and not line.lstrip().startswith("//"):
                code = line.split("//", 1)[0]
                nm = re.findall(r"[A-Za-z_]\w*", re.sub(r"\[[^\]]*\]", "", code))
                if nm:
                    out.setdefault((cur, int(fm.group(1), 16)), nm[-1])
    return out


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--strict", action="store_true", help="exit 1 on a flag")
    ap.add_argument("--flags-only", action="store_true")
    ap.add_argument("--unchecked", action="store_true", help="only the sites that no hooked body reaches (check_literals does not look at them)")
    a = ap.parse_args()
    tree = cl.Tree()
    stock = cl.Stock()
    sizes = load_class_sizes(tree)
    fields = load_fields()
    rows = stock.rows
    tiny = {va for va, r in rows.items() if int(r["size"]) <= 20 and r["kind"] in ("game", "thunk", "crt", "init")}
    desc: dict[int, tuple] = {}

    def info(va):
        if va not in desc:
            desc[va] = describe(stock.img, va, int(rows[va]["size"]))
        return desc[va]

    reached = tree.reached()

    def mark(d) -> str:
        """`*` the definition is a hook unit or runs from one (check_literals compares what it names with the stock function the unit
        replaces); `?` nothing of the hooked code runs it, so only this scan (and a reader) looks at the address"""
        return "*" if (d.unit or id(d) in reached) else "?"

    sites: dict[int, list[str]] = {}
    nflags = hard = 0
    for d in tree.alldefs:
        if d.kind != "fn" and d.kind != "macro":
            continue
        body = d.body
        loc = cl.local_types(body)
        for m in CALL.finditer(body):
            va = int(m.group(2), 16)
            if va not in tiny:
                continue
            rtype_decl = (m.group(1) or "").strip()
            # the receiver: the first argument after the address
            end = cl.balanced(body, body.index("(", m.start()))
            args = cl.split_args(body[body.index("(", m.start()) + 1:end - 1])
            recv = args[1].strip() if len(args) > 1 else ""
            nargs = max(0, len(args) - 2)
            t = None
            if recv:
                pos = body.index("(", m.start()) + 1 + len(args[0]) + 1 + len(args[1])
                t = tree.receiver_type(body + "", pos, d, loc) if recv else None
                if t is None and recv == "this":
                    t = d.owner
                if t is None and re.fullmatch(r"[A-Za-z_]\w*", recv):
                    t = loc.get(recv) or next((tree.fields.get((c, recv)) for c in tree.chain(d.owner) if tree.fields.get((c, recv))), None)
            kind, text, n, op = info(va)
            flags = []
            if n is not None and t and t not in GENERIC:
                sz = sizes.get(t)
                if sz is not None and n >= sz:
                    flags.append("BEYOND(%s is 0x%X bytes, +0x%X)" % (t, sz, n))
                    if (t, d.file) not in KNOWN_BEYOND:
                        hard += 1
                else:
                    fld = next((fields.get((c, n)) for c in tree.chain(t) if fields.get((c, n))), None)
                    if fld is None and kind in ("getter", "setter"):
                        flags.append("NOFIELD(%s has no field at +0x%X)" % (t, n))
            if kind == "setter" and rtype_decl and rtype_decl != "void":
                flags.append("RET(a setter's eax is the argument, the call reads %s)" % rtype_decl)
            mm = re.search(r"ret (\d+)", op)
            if mm and nargs * 4 != int(mm.group(1)) and kind in ("getter", "setter"):
                flags.append("ARGS(%d passed, stock ret %s)" % (nargs, mm.group(1)))
            line = d.line + body[:m.start()].count("\n")
            fieldname = ""
            if n is not None and t:
                fieldname = next((fields.get((c, n)) for c in tree.chain(t) if fields.get((c, n))), "")
            sites.setdefault(va, []).append("    %s%s:%d %s%s(%s)%s%s" % (
                mark(d), d.file, line, (d.owner + "::" if d.owner else ""), d.name, recv[:40], ("  this=%s" % t) if t else "",
                ("  field=%s" % fieldname if fieldname else "") + ("  [%s]" % "; ".join(flags) if flags else "")))
            nflags += bool(flags)
    # NW_STOCK stubs and Addr() literals
    for d in tree.alldefs:
        if d.kind != "fn":
            continue
        for m in STUB.finditer(d.body):
            va = int(m.group(1), 16)
            if va not in tiny:
                continue
            kind, text, n, op = info(va)
            flags = []
            if n is not None and d.owner and d.owner not in GENERIC and kind in ("getter", "setter"):
                sz = sizes.get(d.owner)
                fld = next((fields.get((c, n)) for c in tree.chain(d.owner) if fields.get((c, n))), None)
                if sz is not None and n >= sz:
                    flags.append("BEYOND(%s is 0x%X bytes, +0x%X)" % (d.owner, sz, n))
                    if (d.owner, d.file) not in KNOWN_BEYOND:
                        hard += 1
                elif fld is None and kind in ("getter", "setter"):
                    flags.append("NOFIELD(%s has no field at +0x%X)" % (d.owner, n))
            sites.setdefault(va, []).append("    %s%s:%d stub %s::%s  (NW_STOCK)%s%s" % (
                mark(d), d.file, d.line, d.owner, d.name, ("  field=%s" % next((fields.get((c, n)) for c in tree.chain(d.owner) if fields.get((c, n))), "")) if n is not None and d.owner else "",
                "  [%s]" % "; ".join(flags) if flags else ""))
            nflags += bool(flags)
        for m in ADDR.finditer(d.body):
            va = int(m.group(1), 16)
            if va in tiny and not CALL.search(d.body[max(0, m.start() - 60):m.end() + 5]):
                sites.setdefault(va, []).append("    %s%s:%d Addr(): %s::%s" % (mark(d), d.file, d.line, d.owner, d.name))
    shown = 0
    for va in sorted(sites):
        kind, text, n, op = info(va)
        lines = sites[va]
        if a.unchecked:
            lines = [l for l in lines if l.startswith("    ?")]
            if not lines:
                continue
        flagged = [l for l in lines if "[" in l.split("  ")[-1] and re.search(r"\[(BEYOND|NOFIELD|RET|ARGS)", l)]
        if a.flags_only and not flagged:
            continue
        shown += 1
        print("%08x %-34s %-8s %s" % (va, rows[va]["name"][:34], kind, text))
        for l in (flagged if a.flags_only else lines):
            print(l)
    nchk = sum(1 for v in sites.values() for l in v if l.startswith("    *"))
    print("check_tiny: %d tiny stock function(s) named by address, %d place(s) (%d run from a hooked body), %d hint(s), %d outside their object" % (
        len(sites), sum(len(v) for v in sites.values()), nchk, nflags, hard))
    return 1 if (a.strict and hard) else 0


if __name__ == "__main__":
    sys.exit(main())
