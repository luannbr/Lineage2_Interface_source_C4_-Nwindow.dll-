#!/usr/bin/env python3
"""C++ exception-handling tables -> parent function of every Catch@/Unwind@ funclet, and the
UE2 guard() name of every guarded function.

MSVC 7.0 x86 EH: a function with EH pushes the address of its `__ehhandler$` stub
(`mov eax, FuncInfo; jmp ___CxxFrameHandler`). FuncInfo (magic 0x19930520) lists the unwind
actions and try blocks; each HandlerType points at a catch funclet. UE2's
  guard(Name) ... unguard  ==  try { ... } catch(TCHAR*) {throw;} catch(...) {appUnwindf(L"%s", L"Name"); throw;}
so the catch(...) funclet references the UTF-16 function name.

Writes analysis/eh.json: { parent_va: {stub, funcinfo, catches, unwinds, guard} }.
"""
from __future__ import annotations

import bisect
import csv
import json
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from nwd.image import ROOT, Image  # noqa: E402

EH_MAGIC = (0x19930520, 0x19930521, 0x19930522)


def load_functions():
    rows = list(csv.DictReader((ROOT / "analysis/ghidra/functions.tsv").open(encoding="utf-8"), delimiter="\t"))
    funcs = []
    for r in rows:
        ranges = [tuple(int(x, 16) for x in rg.split("-")) for rg in r["ranges"].split(",")]
        funcs.append((int(r["va"], 16), r["name"], ranges))
    return funcs


class FuncIndex:
    def __init__(self, funcs):
        self.spans = sorted((a, b, va) for va, _, ranges in funcs for a, b in ranges)
        self.starts = [s[0] for s in self.spans]

    def containing(self, va):
        i = bisect.bisect_right(self.starts, va) - 1
        if i >= 0:
            a, b, f = self.spans[i]
            if a <= va <= b:
                return f
        return None


def main() -> int:
    img = Image()
    funcs = load_functions()
    idx = FuncIndex(funcs)
    fbody = {va: ranges for va, _, ranges in funcs}
    frame_handler = next(va for va, name, _ in funcs if name == "___CxxFrameHandler")

    lo, hi = img.sect_range(".text")
    code = img.read(lo, hi - lo)
    stubs = {}
    i = code.find(b"\xb8")
    while i >= 0 and i + 10 <= len(code):
        if code[i + 5] == 0xE9:
            tgt = lo + i + 10 + struct.unpack_from("<i", code, i + 6)[0]
            if tgt == frame_handler:
                fi = struct.unpack_from("<I", code, i + 1)[0]
                if img.section(fi) == ".rdata" and img.u32(fi) in EH_MAGIC:
                    stubs[lo + i] = fi
        i = code.find(b"\xb8", i + 1)

    # who pushes each stub: relocated dwords in .text holding the stub address
    by_value = {}
    for r in img.relocs:
        if img.in_text(r):
            by_value.setdefault(img.u32(r), []).append(r)

    # UTF-16 string references inside a function body
    def wstrings_in(ranges):
        out = []
        for a, b in ranges:
            j = bisect.bisect_left(img.relocs, a)
            while j < len(img.relocs) and img.relocs[j] <= b:
                v = img.u32(img.relocs[j])
                if img.section(v) == ".rdata":
                    s = img.wstr(v, 200)
                    if s and len(s) >= 3 and all(32 <= ord(c) < 127 for c in s):
                        out.append(s)
                j += 1
        return out

    result = {}
    orphans = 0
    for stub, fi in sorted(stubs.items()):
        magic, max_state, p_unwind, n_try, p_try = struct.unpack("<5I", img.read(fi, 20))
        unwinds = []
        for k in range(max_state):
            _to, action = struct.unpack("<iI", img.read(p_unwind + 8 * k, 8))
            if action:
                unwinds.append(action)
        catches = []
        for t in range(n_try):
            _lo, _hi, _ch, n_catch, p_handlers = struct.unpack("<iiiiI", img.read(p_try + 20 * t, 20))
            for h in range(n_catch):
                adj, p_type, disp, handler = struct.unpack("<IIiI", img.read(p_handlers + 16 * h, 16))
                catches.append({"va": handler, "type": p_type, "adjectives": adj})
        parents = {idx.containing(r) for r in by_value.get(stub, [])}
        parents.discard(None)
        if len(parents) != 1:
            orphans += 1
            continue
        parent = parents.pop()
        guard = []
        for c in catches:
            if c["type"] == 0:  # catch(...)
                cf = idx.containing(c["va"])
                for s in wstrings_in(fbody.get(cf, [(c["va"], c["va"] + 0x40)])):
                    if s != "%s":
                        guard.append(s)
        result[f"{parent:08x}"] = {
            "stub": stub, "funcinfo": fi, "max_state": max_state,
            "catches": catches, "unwinds": sorted(set(unwinds)), "guard": guard,
        }

    out = ROOT / "analysis" / "eh.json"
    out.write_text(json.dumps(result, indent=1), encoding="utf-8")
    n_guard = sum(1 for v in result.values() if v["guard"])
    print(f"{len(stubs)} EH stubs, {len(result)} parents, {n_guard} guarded, {orphans} orphan stubs -> {out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
