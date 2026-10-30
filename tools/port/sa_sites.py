#!/usr/bin/env python3
"""The work list of stage 5 (standalone): every place the standalone compile still reaches the stock image.

A standalone compile (NW_HOOK_MODE=0) sees the source without the `#if NW_HOOK_MODE` branches. What is left of these constructs in
that view is what the standalone DLL cannot link (each one is an unresolved symbol), so this is the list a lane has to bring to zero:

  StockCallAt<R>(va, this, ...)   thiscall into the stock image  -> a typed call of OUR function that holds that address
  nw::StockCall<R>(va, ...)       same (older spelling)
  NW_STOCK(va) / NW_STOCK_AT(va)  a body that jumps into the stock image -> a real body
  nw::Addr(va)                    a stock table or a stock function pointer -> our table / our function
  NW_GLOBAL(T, va)                a stock global variable -> our global (src/standalone/NwGlobals.cpp)
  NW_UNPORTED / NW_UNPORTED_NORET a debug break where a function was never ported

Each site is shown with what the database knows about the target address (db/functions.csv: status, name, source file), so a
`done` target is a function we already have (the call becomes a normal call) and a `todo`/`lib` target is real work.

    python tools/port/sa_sites.py                         # one line per file with the counts
    python tools/port/sa_sites.py --list                  # every site: file:line kind target (status name) text
    python tools/port/sa_sites.py --files "src/nwindow/NCConsole.cpp,src/nwindow/NConsoleWnd*.cpp"   # comma separated globs
    python tools/port/sa_sites.py --files ... --total     # only the total (exit code 1 while sites are left: the lane's finish line)
    python tools/port/sa_sites.py --kind NW_STOCK --list

Run it from the root of your worktree (it reads that tree's src/, include/ and db/functions.csv).
"""
from __future__ import annotations

import argparse
import bisect
import csv
import fnmatch
import re
import sys
from collections import Counter, defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

KINDS = {
    "StockCallAt": re.compile(r"StockCallAt\s*<[^()]*?>\s*\(\s*(0x[0-9A-Fa-f]+|[A-Za-z_][\w:]*)"),
    "StockCall": re.compile(r"nw::StockCall\s*<[^()]*?>\s*\(\s*(0x[0-9A-Fa-f]+|[A-Za-z_][\w:]*)"),
    "NW_STOCK": re.compile(r"NW_STOCK(?:_AT)?\(\s*(0x[0-9A-Fa-f]+)"),
    "Addr": re.compile(r"nw::Addr\(\s*(0x[0-9A-Fa-f]+|[A-Za-z_][\w:]*)"),
    "NW_GLOBAL": re.compile(r"NW_GLOBAL\(\s*[^,()]+(?:<[^>]*>)?[^,()]*,\s*(0x[0-9A-Fa-f]+)"),
    "NW_UNPORTED": re.compile(r"\b(NW_UNPORTED(?:_NORET)?)\s*\("),
}


def standalone_lines(path: Path):
    """(line number, text) of what a standalone compile sees: `#if NW_HOOK_MODE` blocks are skipped (their #else is kept),
    any other conditional is kept whole."""
    stack: list[list] = []
    for n, line in enumerate(path.read_text(encoding="utf-8", errors="replace").splitlines(), 1):
        s = line.strip()
        m = re.match(r"#\s*(if|ifdef|ifndef|elif|else|endif)\b(.*)", s)
        if m:
            d, cond = m.group(1), m.group(2).strip()
            if d in ("if", "ifdef", "ifndef"):
                if d == "if" and cond == "NW_HOOK_MODE":
                    stack.append(["hook", False])
                elif d == "if" and cond in ("!NW_HOOK_MODE", "NW_HOOK_MODE == 0", "NW_HOOK_MODE==0"):
                    stack.append(["hook", True])
                else:
                    stack.append(["other", True])
            elif d == "else" and stack and stack[-1][0] == "hook":
                stack[-1][1] = not stack[-1][1]
            elif d == "endif" and stack:
                stack.pop()
            continue
        if all(active for _, active in stack):
            yield n, line


def load_db():
    rows = list(csv.DictReader(open(ROOT / "db" / "functions.csv", encoding="utf-8")))
    fn = {int(r["va"], 16): r for r in rows}
    return fn, sorted(fn)


def lookup(fn, starts, va):
    if va in fn:
        return fn[va], True
    i = bisect.bisect_right(starts, va) - 1
    if i >= 0:
        r = fn[starts[i]]
        if starts[i] <= va < starts[i] + int(r["size"]):
            return r, False
    return None, False


def describe(fn, starts, target):
    if not target.startswith("0x"):
        return "dynamic"
    va = int(target, 16)
    r, exact = lookup(fn, starts, va)
    if r is None:
        return "data/none"
    return "%s%s %s" % (r["status"], "" if exact else " (inside)", r["name"])


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--files", help="comma separated globs relative to the root (default: every src/nwindow/*.cpp and include/**/*.h)")
    ap.add_argument("--kind", help="only this kind (StockCallAt, StockCall, NW_STOCK, Addr, NW_GLOBAL, NW_UNPORTED)")
    ap.add_argument("--list", action="store_true", help="one line per site")
    ap.add_argument("--total", action="store_true", help="only the total; exit code 1 when sites are left")
    args = ap.parse_args()

    fn, starts = load_db()
    all_files = sorted((ROOT / "src" / "nwindow").rglob("*.cpp")) + sorted((ROOT / "include").rglob("*.h"))
    if args.files:
        pats = [p.strip().replace("\\", "/") for p in args.files.split(",") if p.strip()]
        sel = []
        for f in all_files:
            rel = f.relative_to(ROOT).as_posix()
            if any(fnmatch.fnmatch(rel, p) for p in pats):
                sel.append(f)
        all_files = sel
    kinds = {args.kind: KINDS[args.kind]} if args.kind else KINDS

    per_file: dict[str, Counter] = defaultdict(Counter)
    sites = []
    for f in all_files:
        rel = f.relative_to(ROOT).as_posix()
        for n, line in standalone_lines(f):
            code = line.split("//")[0]
            if code.lstrip().startswith("#define "):
                continue  # the macros themselves
            for kind, rx in kinds.items():
                for m in rx.finditer(code):
                    target = m.group(1)
                    per_file[rel][kind] += 1
                    sites.append((rel, n, kind, target, describe(fn, starts, target), line.strip()[:110]))

    total = len(sites)
    if args.total:
        print(total)
        return 1 if total else 0
    if args.list:
        for rel, n, kind, target, d, text in sites:
            print("%s:%d  %-11s %-12s (%s)  %s" % (rel, n, kind, target, d, text))
    else:
        for rel, c in sorted(per_file.items(), key=lambda x: -sum(x[1].values())):
            print("%4d  %-52s %s" % (sum(c.values()), rel, dict(c)))
    kc = Counter(s[2] for s in sites)
    print("TOTAL %d sites in %d files %s" % (total, len(per_file), dict(kc)), file=sys.stderr if args.list else sys.stdout)
    return 1 if total else 0


if __name__ == "__main__":
    sys.exit(main())
