#!/usr/bin/env python3
"""Field offsets from what OnCreate makes: the base source says which field gets which object, the C3 code says at
which offset.

A window's OnCreate stores what it makes into its fields: `Child = new NCButton;`, `BackTex = FindTexture(TEXT("L2UI.x"))`.
The same statements are in the C3 function (the decompile of analysis/ghidra/decomp/<va>.c: a StaticAllocateObject of the
class's UClass then `*(this + OFF) = object`, or `*(this + OFF) = FindTexture(L"name")`). Pair them:

  * a texture field by its string (unique in both: one pair; repeated: by order);
  * an object field by its class (the same number of fields of the class on both sides: by order).

The pairs say where the field is in this client, with no vote evidence needed. Output: db/layout_create.csv
(class,field,c1,c3,how,evidence); with --apply the lines are appended to db/layout_manual.csv (a field the manual file
already places is left alone). Then run tools/port/layout.py.

usage: python tools/port/layout_from_create.py [--apply] [--class NCConsole]"""
from __future__ import annotations

import argparse
import collections
import csv
import json
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from common import ROOT, C1_ROOT  # noqa: E402

STORE = re.compile(r"\*\([^)]*\)\s*\(\s*(?:in_ECX|this|param_1)\s*\+\s*(0x[0-9a-fA-F]+|\d+)\s*\)\s*=\s*([^;]+);")
ALLOC = re.compile(r"StaticAllocateObject\s*\(\s*&DAT_([0-9a-f]{8})")
TEXCALL = re.compile(r"FUN_10028970\s*\(\s*L\"([^\"]+)\"")
VARTEX = re.compile(r"(\w+)\s*=\s*FUN_10028970\s*\(\s*L\"([^\"]+)\"")


def c3_stores(text, cmap):
    """[(offset, 'new', class) | (offset, 'tex', string)] in the order of the code"""
    out = []
    pend_cls = None
    vtex = {}
    for ln in text.split("\n"):
        m = ALLOC.search(ln)
        if m:
            pend_cls = cmap.get(int(m.group(1), 16))
        for vm in VARTEX.finditer(ln):
            vtex[vm.group(1)] = vm.group(2)
        m = STORE.search(ln)
        if not m:
            continue
        off = int(m.group(1), 16) if m.group(1).startswith("0x") else int(m.group(1))
        rhs = m.group(2).strip()
        tm = TEXCALL.search(rhs)
        if tm:
            out.append((off, "tex", tm.group(1)))
        elif rhs in vtex:
            out.append((off, "tex", vtex[rhs]))
        elif pend_cls and re.fullmatch(r"\w+", rhs):
            out.append((off, "new", pend_cls))
            pend_cls = None
    return out


def base_stores(text):
    """[(field, 'new', class) | (field, 'tex', string)] from a function body of the base source"""
    out = []
    for m in re.finditer(r"^\s*(\w+)\s*=\s*new\s+(\w+)", text, re.M):
        out.append((m.group(1), "new", m.group(2)))
    for m in re.finditer(r"^\s*(\w+)\s*=\s*FindTexture\(\s*TEXT\(\"([^\"]+)\"\)\s*\)", text, re.M):
        out.append((m.group(1), "tex", m.group(2)))
    return out


def function_body(src, cls, meth):
    m = re.search(r"^[^\n;{]*\b" + re.escape(cls) + r"::" + re.escape(meth) + r"\s*\([^)]*\)\s*\n\{", src, re.M)
    if not m:
        return None
    i = src.index("{", m.start())
    depth = 0
    for j in range(i, len(src)):
        if src[j] == "{":
            depth += 1
        elif src[j] == "}":
            depth -= 1
            if depth == 0:
                return src[i:j + 1]
    return None


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--apply", action="store_true")
    ap.add_argument("--class", dest="only", default="")
    a = ap.parse_args()
    cmap = {x["static_class"]: x["cpp"] for x in json.load(open(ROOT / "analysis" / "classes.json", encoding="utf-8"))}
    layout = json.load(open(ROOT / "db" / "layout.json", encoding="utf-8"))
    fn3 = collections.defaultdict(dict)       # class -> method -> va
    for r in csv.DictReader((ROOT / "db" / "functions.csv").open(encoding="utf-8")):
        if r["class"] and "::" in r["name"]:
            fn3[r["class"]].setdefault(r["name"].split("::")[-1], r["va"])
    manual = set()
    mp = ROOT / "db" / "layout_manual.csv"
    if mp.exists():
        for r in csv.DictReader(l for l in mp.open(encoding="utf-8") if not l.startswith("#")):
            manual.add((r["class"], r["field"]))
    srcs = {}
    for p in (C1_ROOT / "src" / "nwindow").glob("*.cpp"):
        srcs[p.name] = p.read_text(encoding="utf-8", errors="replace")
    rows = []
    for cls, info in sorted(layout.items()):
        if a.only and cls != a.only:
            continue
        va = fn3.get(cls, {}).get("OnCreate")
        if not va:
            continue
        dp = ROOT / "analysis" / "ghidra" / "decomp" / ("%s.c" % va)
        if not dp.exists():
            continue
        body = None
        for name, s in srcs.items():
            body = function_body(s, cls, "OnCreate")
            if body:
                break
        if not body:
            continue
        b = base_stores(body)
        c = c3_stores(dp.read_text(encoding="utf-8", errors="replace"), cmap)
        fields = {f["name"]: f for f in info["fields"]}
        # textures by string, objects by class
        for kind in ("tex", "new"):
            bb = [(f, v) for f, k, v in b if k == kind and f in fields]
            cc = [(o, v) for o, k, v in c if k == kind]
            byv_b = collections.defaultdict(list)
            for f, v in bb:
                byv_b[v].append(f)
            byv_c = collections.defaultdict(list)
            for o, v in cc:
                byv_c[v].append(o)
            for v, fs in byv_b.items():
                os_ = byv_c.get(v, [])
                fs = sorted(fs, key=lambda f: fields[f]["c1"])
                os_ = sorted(os_)
                if len(fs) == len(os_):
                    for f, o in zip(fs, os_):
                        rows.append((cls, f, fields[f]["c1"], o, kind, v))
    with (ROOT / "db" / "layout_create.csv").open("w", encoding="utf-8", newline="") as f:
        w = csv.writer(f)
        w.writerow(["class", "field", "c1", "c3", "how", "evidence"])
        for r in rows:
            w.writerow([r[0], r[1], "%x" % r[2], "%x" % r[3], r[4], r[5]])
    n = collections.Counter(r[0] for r in rows)
    print("%d pairs in %d classes -> db/layout_create.csv" % (len(rows), len(n)))
    if a.apply:
        # the votes of the instruction pairs outweigh this pairing: only fields they did not place are taken
        lay = json.load(open(ROOT / "db" / "layout.json", encoding="utf-8"))
        status = {(c, f["name"]): f["status"] for c, i in lay.items() for f in i["fields"]}
        add = [r for r in rows if (r[0], r[1]) not in manual and status.get((r[0], r[1]), "").split("+")[0] != "mapped"]
        with mp.open("a", encoding="utf-8", newline="\n") as f:
            f.write("# from what OnCreate makes (tools/port/layout_from_create.py)\n")
            for cls, field, c1, c3, kind, v in add:
                ev = v if kind == "new" else '"%s"' % v
                f.write('%s,%s,%x,,"OnCreate %s %s at +0x%x"\n' % (
                    cls, field, c3, "makes" if kind == "new" else "loads", ev.replace('"', "'"), c3))
        print("appended %d lines to db/layout_manual.csv" % len(add))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
