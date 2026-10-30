#!/usr/bin/env python3
"""Turn compile errors of copied bodies into demotions: db/demote.csv (c2_va, reason).

A copied C1 body that does not compile against the C2 headers (a field that is gone, a changed
signature, an API the C2 Engine does not have) is not the same code: gen.py makes it a stub.
Reads the cl log, finds the generated top-level construct around each error line and records
what it is:

  * a unit (a function that ends at its NW_HOOK_ line): its C2 address            `10036ce0`
  * a helper of the text between the units (static / inline / template function):  `helper:Name@File.cpp`
  * a local struct of that text:                                                   `helper:struct:Name@File.cpp`
  * a definition of the text between the units (no hook of its own, qualified):    `helper:fn:Class::Method@File.cpp`
  * a `naked` stub the compiler refused (C3068, a by-value parameter that needs unwinding): `naked:Class::Method`

The helper keys are per file (the same helper name is defined in many files of the base, each its own copy).
Errors outside a construct of a generated source (headers, stubs) are listed, not demoted.

usage: python tools/port/demote.py <cl log> [--stage build/gen_stage]
"""
from __future__ import annotations

import argparse
import csv
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
# cl prints `file(line): error Cnnnn: text`; MSBuild (the cmake VS generator) adds the column, indents the line
# and appends the project in brackets
ERR = re.compile(r"^\s*(?P<file>[^(\r\n]+)\((?P<line>\d+)(?:,\d+)?\): (?:fatal )?error (?P<code>C\d+): "
                 r"(?P<msg>.*?)(?: \[[A-Za-z]:.*\])?(?: \(compilando.*)?$")
HOOK = re.compile(r"^NW_HOOK_\w+\(\s*0x([0-9A-Fa-f]{8})")
HELPER = re.compile(r"^(?:template\s*<[^>]*>\s*)?(?:static|inline|FORCEINLINE|__forceinline|template)\b[^;{]*?\b(\w+)\s*\(")
STRUCT = re.compile(r"^(?:struct|class)\s+(\w+)\s*$")
QUAL = re.compile(r"^[A-Za-z_][\w:<>\*&~ ,]*?\b(\w+::~?\w+)\s*\(")
STATEMENT = re.compile(r"^(return|else|if|for|while|do|switch|case|goto|delete|new|throw)\b")


def read_lines(path: Path):
    out = []
    for b in path.read_bytes().split(b"\n"):
        try:
            out.append(b.decode("utf-8").rstrip("\r"))
        except UnicodeDecodeError:
            out.append(b.decode("cp1252", errors="replace").rstrip("\r"))
    return out


def depth_after(lines):
    """brace + paren depth after each line (comments, strings and character literals do not count)"""
    res, depth, in_c, in_pp = [], 0, False, False
    for l in lines:
        i, n = 0, len(l)
        if not in_c and (l.startswith("#") or in_pp):      # a directive, with its continuation lines (`\` at the end)
            res.append(depth)
            in_pp = l.rstrip().endswith(chr(92))
            continue
        while i < n:
            c = l[i]
            if in_c:
                e = l.find("*/", i)
                if e < 0:
                    i = n
                else:
                    i, in_c = e + 2, False
                continue
            if c == "/" and l[i:i + 2] == "//":
                break
            if c == "/" and l[i:i + 2] == "/*":
                in_c = True
                i += 2
                continue
            if c in "\"'":
                j = i + 1
                while j < n and l[j] != c:
                    j += 2 if l[j] == "\\" else 1
                i = j + 1
                continue
            if c in "{(":
                depth += 1
            elif c in "})":
                depth -= 1
            i += 1
        res.append(depth)
    return res


def classify(lines, dep, n):
    """What the top-level construct around line n (0-based) is: ('unit', va) | ('fn', name) | ('helper', name) |
    ('struct', name) | None."""
    nl = len(lines)
    n = min(n, nl - 1)
    # the head of the construct: the nearest column-0 code line at depth 0 above (or at) n
    h = n
    while h >= 0:
        l = lines[h]
        before = dep[h - 1] if h > 0 else 0
        if before == 0 and l and l[0] not in " \t{}:,/#)" and not l.startswith("NW_HOOK_"):
            break
        if before == 0 and l.startswith("NW_HOOK_") and h < n:
            return None          # n is below a hook line, outside the construct
        h -= 1
    if h < 0:
        return None
    head = lines[h]
    # its end: the line where the depth is back to 0 and nothing continues it
    e = max(h, n)
    while e < nl:
        if dep[e] == 0:
            nxt = e + 1
            while nxt < nl and not lines[nxt].strip():
                nxt += 1
            if nxt < nl and lines[nxt] and lines[nxt][0] in " \t{:," and dep[e] == 0 and e < n + 1000 \
                    and not lines[nxt].lstrip().startswith("//"):
                e = nxt
                continue
            break
        e += 1
    if head.startswith("NW_NAKED") or head.startswith("// "):
        return None          # a stub is not a copied body
    nxt = e + 1
    while nxt < nl and not lines[nxt].strip():
        nxt += 1
    hm = HOOK.match(lines[nxt]) if nxt < nl else None
    if hm:
        return ("unit", hm.group(1).lower())
    sm = STRUCT.match(head)
    if sm:
        return ("struct", sm.group(1))
    m = HELPER.match(head)
    if m:
        return ("helper", m.group(1))
    q = QUAL.match(head)
    if q and not STATEMENT.match(head) and not head.startswith(("NW_", "#")):
        return ("fn", q.group(1))
    return None


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("log")
    ap.add_argument("--stage", default=str(ROOT / "build" / "gen_stage"))
    a = ap.parse_args()
    stage = Path(a.stage)
    path = ROOT / "db" / "demote.csv"
    have = {}
    if path.exists():
        for r in csv.DictReader(path.open(encoding="utf-8")):
            have[r["c2_va"]] = r["reason"]
    new, loose = {}, []
    texts = {}
    deps = {}
    for raw_b in open(a.log, "rb").read().split(b"\n"):
        try:
            raw = raw_b.decode("utf-8")
        except UnicodeDecodeError:
            raw = raw_b.decode("cp1252", errors="replace")
        m = ERR.match(raw.rstrip())
        if not m:
            continue
        f = Path(m.group("file").strip())
        why = "%s: %s" % (m.group("code"), m.group("msg")[:100])
        if m.group("code") == "C3068":
            # a `naked` stub with a by-value parameter that needs unwinding: gen.py makes it a breaking definition
            nm = re.match(r"'([^']+)'", m.group("msg"))
            if nm:
                key = "naked:" + nm.group(1)
                if key not in have:
                    new.setdefault(key, "C3068: a naked stub with a parameter that needs unwinding")
                continue
        if f.suffix != ".cpp":
            loose.append(raw.strip())
            continue
        name = f.name
        if name not in texts:
            p = stage / "src" / "nwindow" / name
            texts[name] = read_lines(p) if p.exists() else []
            deps[name] = depth_after(texts[name]) if texts[name] else []
        lines = texts[name]
        if not lines:        # the file is gone (a stale project entry, C1083) or unreadable
            loose.append(raw.strip())
            continue
        c = classify(lines, deps[name], int(m.group("line")) - 1)
        if c is None:
            loose.append(raw.strip())
            continue
        kind, val = c
        if kind == "unit":
            key = val
        elif kind == "fn":
            key = "helper:fn:%s@%s" % (val, name)
        elif kind == "struct":
            key = "helper:struct:%s@%s" % (val, name)
        else:
            key = "helper:%s@%s" % (val, name)
        if key not in have:
            new.setdefault(key, why)
    with path.open("a" if have else "w", encoding="utf-8", newline="") as fh:
        w = csv.writer(fh)
        if not have:
            w.writerow(["c2_va", "reason"])
        for va, why in sorted(new.items()):
            w.writerow([va, why])
    print("demoted %d new units (%d known); %d errors outside a copied body" % (len(new), len(have), len(loose)))
    for l in sorted(set(loose))[:15]:
        print("  ", l[:170])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
