#!/usr/bin/env python3
"""The UE2 guard() name of every guarded function of a NWindow.dll (same method as tools/eh_scan.py,
but for any image and function list, and nothing is written): used by tools/ue2_console_slots.py to
name NConsoleWnd's C2 vtable slots.

  python tools/ue2_guard_names.py [NWindow.dll [functions.tsv]]    # prints VA<TAB>guard name
default: this repo's orig/NWindow.dll (C2) with analysis/c2/funcs_c2_nwindow.tsv
"""
from __future__ import annotations

import bisect
import csv
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from nwd.image import ROOT, Image  # noqa: E402

EH_MAGIC = (0x19930520, 0x19930521, 0x19930522)


def guard_names(img: Image, funcs_tsv: Path) -> dict[int, str]:
    funcs = []
    for r in csv.DictReader(funcs_tsv.open(encoding="utf-8"), delimiter="\t"):
        funcs.append((int(r["va"], 16), [tuple(int(x, 16) for x in rg.split("-")) for rg in r["ranges"].split(",")]))
    spans = sorted((a, b, va) for va, ranges in funcs for a, b in ranges)
    starts = [s[0] for s in spans]
    fbody = dict(funcs)

    def containing(va):
        i = bisect.bisect_right(starts, va) - 1
        if i >= 0 and spans[i][0] <= va <= spans[i][1]:
            return spans[i][2]
        return None

    lo, hi = img.sect_range(".text")
    code = img.read(lo, hi - lo)
    # the __ehhandler$ stubs: mov eax, FuncInfo ; jmp ___CxxFrameHandler (the handler is the
    # target most such stubs jump to)
    cand = []
    i = code.find(b"\xb8")
    while i >= 0 and i + 10 <= len(code):
        if code[i + 5] == 0xE9:
            fi = struct.unpack_from("<I", code, i + 1)[0]
            if img.section(fi) == ".rdata" and img.u32(fi) in EH_MAGIC:
                cand.append((lo + i, fi, lo + i + 10 + struct.unpack_from("<i", code, i + 6)[0]))
        i = code.find(b"\xb8", i + 1)
    counts = {}
    for _, _, t in cand:
        counts[t] = counts.get(t, 0) + 1
    handler = max(counts, key=counts.get)
    stubs = {s: fi for s, fi, t in cand if t == handler}

    by_value = {}
    for r in img.relocs:
        if img.in_text(r):
            by_value.setdefault(img.u32(r), []).append(r)

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
    for stub, fi in sorted(stubs.items()):
        _magic, _max_state, _p_unwind, n_try, p_try = struct.unpack("<5I", img.read(fi, 20))
        catches = []
        for t in range(n_try):
            _lo, _hi, _ch, n_catch, p_handlers = struct.unpack("<iiiiI", img.read(p_try + 20 * t, 20))
            for h in range(n_catch):
                _adj, p_type, _disp, handler_va = struct.unpack("<IIiI", img.read(p_handlers + 16 * h, 16))
                catches.append((handler_va, p_type))
        parents = {containing(r) for r in by_value.get(stub, [])}
        parents.discard(None)
        if len(parents) != 1:
            continue
        parent = parents.pop()
        for handler_va, p_type in catches:
            if p_type == 0:  # catch(...)
                cf = containing(handler_va)
                for s in wstrings_in(fbody.get(cf, [(handler_va, handler_va + 0x40)])):
                    if s != "%s":
                        result.setdefault(parent, s)
    return result


def main(argv) -> int:
    img = Image(argv[0]) if argv else Image()
    tsv = Path(argv[1]) if len(argv) > 1 else ROOT / "analysis" / "c2" / "funcs_c2_nwindow.tsv"
    for va, name in sorted(guard_names(img, tsv).items()):
        print(f"{va:08x}\t{name}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
