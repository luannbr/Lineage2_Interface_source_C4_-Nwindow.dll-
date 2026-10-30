#!/usr/bin/env python3
"""A call into the stock image by address must push as many argument bytes as the stock function pops.

The first world-entry run of the stage 4 build died in NCGMSnoopWnd::OnCreate: `NewStock<NCCheckBox>(0x100058C0, id, 0, 0)` gave the
stock constructor three arguments, it ends with `ret 0x10` (four), so the frame came back four bytes short and the next store went to a
NULL. No judge looks at this: verify_hooks compares calls by address, not by arity, and the compiler cannot know the stock function's
pop. This reads every

    StockCallAt<R>(0xVA, self, a, b ...)   StockCall<R>(0xVA, self, ...)   NewStock<T>(0xVA, a, b ...)   NewStockObj<T>(Class, 0xVA, a, b ...)

in src/ (not Prologues.gen.cpp), counts the arguments after `self` and compares 4 bytes each with the `ret N` of the stock function at
0xVA (db/functions.csv for its size, orig/NWindow.dll for the bytes). A call whose arguments may be wider than a dword (a FString, a
rectangle, a struct, a double) is skipped: the sum is not known from the text. A function with no `ret N` of its own (a tail jump) is
skipped. What is left is a mismatch to look at: fix the call, or list the line in db/stockargs_ok.txt (`file|line without blanks`)
with a reason in the commit.

usage: python tools/port/check_stockargs.py            exit 1 when a call is not in the list and does not match
       python tools/port/check_stockargs.py --update   rewrite db/stockargs_ok.txt from the tree (after you looked at the lines)"""
from __future__ import annotations

import csv
import re
import sys
from pathlib import Path

import pefile
from capstone import CS_ARCH_X86, CS_MODE_32, Cs

ROOT = Path(__file__).resolve().parents[2]
LIST = ROOT / "db" / "stockargs_ok.txt"
WIDE = re.compile(r"FString|Rect|Info|Vector|FVector|DOUBLE|double|\bstruct\b|NString|FName|L2FName|Param\b|\*\s*\w+\s*\)\s*\*|\bTArray\b|Array")
CALL = re.compile(r"\b(StockCallAt|StockCall|NewStockObj|NewStock|NChatNewStockObj)\s*(<(?:[^<>]|<[^<>]*>)*>)?\s*\(")


def split_args(text: str, start: int):
    """The top-level arguments of the call whose '(' is just before `start`, and the index after its ')'."""
    depth, i, cur, args = 0, start, [], []
    while i < len(text):
        c = text[i]
        if c in "([{<" and not (c == "<" and not re.match(r"[A-Za-z_]", text[i + 1:i + 2] or " ")):
            depth += 1
        elif c in ")]}":
            if depth == 0:
                if cur or args:
                    args.append("".join(cur).strip())
                return args, i + 1
            depth -= 1
        elif c == ">" and depth > 0 and "<" in "".join(cur):
            depth -= 1
        if c == "," and depth == 0:
            args.append("".join(cur).strip())
            cur = []
        else:
            cur.append(c)
        i += 1
    return args, i


def stock_rets():
    pe = pefile.PE(str(ROOT / "orig" / "NWindow.dll"), fast_load=True)
    base = pe.OPTIONAL_HEADER.ImageBase
    md = Cs(CS_ARCH_X86, CS_MODE_32)
    funcs = {}
    for r in csv.DictReader(open(ROOT / "db" / "functions.csv", newline="", encoding="utf-8", errors="replace")):
        funcs[int(r["va"], 16)] = int(r["size"])

    def rets(va):
        size = funcs.get(va)
        if not size:
            return None
        data = pe.get_data(va - base, size)
        out = set()
        for ins in md.disasm(data, va):
            if ins.mnemonic == "ret":
                out.add(int(ins.op_str, 16) if ins.op_str else 0)
        return out
    return rets, funcs


def main() -> int:
    rets, funcs = stock_rets()
    found, bad = [], []
    cache = {}
    for f in sorted((ROOT / "src").rglob("*.cpp")):
        if f.name == "Prologues.gen.cpp":
            continue
        rel = f.relative_to(ROOT).as_posix()
        text = f.read_text(encoding="utf-8", errors="replace")
        for m in CALL.finditer(text):
            kind = m.group(1)
            args, end = split_args(text, m.end())
            # the helper definitions themselves (`template <...> T* NewStock(DWORD Ctor, A... a)`) have no literal address
            if kind in ("StockCallAt", "StockCall"):
                if len(args) < 2 or not re.match(r"0x1[0-9A-Fa-f]{7}$", args[0]):
                    continue
                va, rest = int(args[0], 16), args[2 - 0:] if len(args) >= 2 else []
                rest = args[2:]
            elif kind in ("NewStock",):
                if not args or not re.match(r"0x1[0-9A-Fa-f]{7}$", args[0]):
                    continue
                va, rest = int(args[0], 16), args[1:]
            else:   # NewStockObj / NChatNewStockObj(Class, Ctor, ...)
                if len(args) < 2 or not re.match(r"0x1[0-9A-Fa-f]{7}$", args[1]):
                    continue
                va, rest = int(args[1], 16), args[2:]
            if va not in cache:
                cache[va] = rets(va)
            rs = cache[va]
            line = text.count("\n", 0, m.start()) + 1
            key = rel + "|" + re.sub(r"\s+", "", text.splitlines()[line - 1].split("//", 1)[0])
            found.append(key)
            if rs is None or not rs:
                continue
            if any(WIDE.search(a) for a in rest):
                continue
            if 4 * len(rest) not in rs:
                bad.append((rel, line, key, va, len(rest), sorted(rs)))
    if "--update" in sys.argv:
        LIST.write_text("".join(sorted({b[2] for b in bad}) and [k + "\n" for k in sorted({b[2] for b in bad})]), encoding="utf-8")
        print("check_stockargs: %d call(s) read, %d listed in db/stockargs_ok.txt" % (len(found), len({b[2] for b in bad})))
        return 0
    ok = set(LIST.read_text(encoding="utf-8").splitlines()) if LIST.exists() else set()
    new = [b for b in bad if b[2] not in ok]
    for rel, line, key, va, n, rs in new:
        print("  %s:%d  0x%08X gets %d argument dword(s) (%d bytes), the stock pops %s" % (
            rel, line, va, n, 4 * n, "/".join("0x%X" % r for r in rs)))
    print("check_stockargs: %d call(s) with an address read, %d mismatch(es), %d new" % (len(found), len(bad), len(new)))
    return 1 if new else 0


if __name__ == "__main__":
    sys.exit(main())
