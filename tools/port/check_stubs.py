#!/usr/bin/env python3
"""A stub that jumps into the stock code must declare the arguments the stock function pops.

`NW_NAKED ... Class::Method(args) NW_STOCK(0xVA)` is what ported code calls to run a function nobody has ported
yet: the compiler pushes what the declaration says and the stock function pops what ITS `ret N` says. When
the two differ (the client changed the signature: an argument more, a FLOAT for an INT, a struct by value) the
stack of the caller is wrong from that call on. Neither verify_hooks (it looks at the hooked function's own ret)
nor the smoke test see it, because they never run the caller.

This reads every stub of src/nwindow (and src/re), takes the stack bytes its parameter list pushes (a thiscall
member: the parameters; a static / free function: the parameters, with `ret 0` expected when the stock one is
cdecl) and compares with the `ret N` of the stock function. Output: db/stub_check.csv and a table of the
mismatches; exit code 1 when there is one.

usage: python tools/port/check_stubs.py [--all]"""
from __future__ import annotations

import argparse
import csv
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from nwd.image import Image  # noqa: E402
from capstone import CS_ARCH_X86, CS_MODE_32, Cs  # noqa: E402

STUB = re.compile(r"^NW_NAKED\s+(?P<head>.+?)\s*\((?P<params>[^)]*)\)\s*(?P<const>const)?\s*NW_STOCK\(\s*0x(?P<va>[0-9A-Fa-f]{8})\s*\)",
                  re.M)
SIZES = {
    "INT": 4, "int": 4, "UINT": 4, "DWORD": 4, "UBOOL": 4, "BOOL": 4, "FLOAT": 4, "float": 4, "SWORD": 4, "WORD": 4,
    "BYTE": 4, "bool": 4, "char": 4, "TCHAR": 4, "long": 4, "ULONG": 4, "LONG": 4, "unsigned": 4, "short": 4,
    "DOUBLE": 8, "double": 8, "SQWORD": 8, "QWORD": 8, "__int64": 8, "FName": 4, "FVector": 12, "FRotator": 12,
    "FColor": 4, "WPARAM": 4, "LPARAM": 4, "HWND": 4, "HIMC": 4, "SIZE": 8, "POINT": 8, "RECT": 16, "FLOAT": 4,
    "NRectF": 16, "FBox": 28, "FPlane": 16,
}


STRUCT_SIZE = {}
STATIC_NAMES = set()


def scan_headers():
    """struct sizes (NW_ASSERT_SIZE) and the names declared `static` in the headers"""
    asz = re.compile(r"^NW_ASSERT_SIZE\(\s*(\w+)\s*,\s*(0x[0-9A-Fa-f]+|\d+)\s*\)", re.M)
    st = re.compile(r"\bstatic\b(?:[^;{(]|__declspec\(\w+\))*?\b(?!__declspec)(\w+)\s*\(")
    for h in list((ROOT / "include" / "nwindow").glob("*.h")) + list((ROOT / "include" / "ue2").glob("*.h")):
        t = h.read_text(encoding="utf-8", errors="replace")
        for m in asz.finditer(t):
            STRUCT_SIZE[m.group(1)] = int(m.group(2), 0)
        for ln in t.split("\n"):
            code = ln.split("//")[0]
            m = st.search(code)
            if m and "static_assert" not in code and "static_cast" not in code:
                STATIC_NAMES.add(m.group(1))


def param_bytes(params: str):
    """(bytes, unknown types) of a parameter list"""
    params = params.strip()
    if not params or params == "void":
        return 0, []
    total, unknown = 0, []
    depth, cur, items = 0, "", []
    for ch in params:
        if ch in "<(":
            depth += 1
        elif ch in ">)":
            depth -= 1
        if ch == "," and depth == 0:
            items.append(cur.strip())
            cur = ""
        else:
            cur += ch
    if cur.strip():
        items.append(cur.strip())
    for it in items:
        if it == "...":
            return None, ["..."]
        it = re.sub(r"\s*=\s*.*$", "", it)               # default argument
        it = re.sub(r"\b(const|struct|class|enum|volatile|UObject::)\b\s*", "", it).strip()
        if "*" in it or "&" in it:
            total += 4
            continue
        # strip the parameter name
        m = re.match(r"^(.*?)(?:\s+\w+)?(\[[^\]]*\])?$", it)
        typ = (m.group(1) if m else it).strip()
        typ = typ.split()[-1] if typ.split() else typ
        if m and m.group(2):
            total += 4
            continue
        if typ in SIZES:
            total += SIZES[typ]
        elif typ in STRUCT_SIZE:
            total += (STRUCT_SIZE[typ] + 3) // 4 * 4        # a struct by value: its size, to a dword
        elif typ.startswith("L2Font") or typ.endswith("Type") or typ.startswith("E") and typ[1:2].isupper():
            total += 4                                     # an enum
        else:
            unknown.append(typ)
            total += 4
    return total, unknown


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--all", action="store_true", help="also list the stubs that agree")
    a = ap.parse_args()
    scan_headers()
    img = Image()
    md = Cs(CS_ARCH_X86, CS_MODE_32)
    granges = {}
    for r in csv.DictReader((ROOT / "analysis/ghidra/functions.tsv").open(encoding="utf-8"), delimiter="\t"):
        granges[int(r["va"], 16)] = [tuple(int(x, 16) for x in rg.split("-")) for rg in r["ranges"].split(",")]

    def stock_rets(va):
        rets = set()
        seen = set()
        todo = [va]
        depth = 0
        while todo and depth < 3:
            v = todo.pop()
            if v in seen:
                continue
            seen.add(v)
            for lo, hi in granges.get(v, [(v, v + 0x3F)]):
                for ins in md.disasm(img.read(lo, hi - lo + 1), lo):
                    if ins.mnemonic in ("ret", "retn"):
                        rets.add(int(ins.op_str, 0) if ins.op_str else 0)
            depth += 1
        return rets

    rows = []
    bad = 0
    for p in sorted((ROOT / "src" / "nwindow").glob("*.cpp")) + sorted((ROOT / "src" / "re").glob("*.cpp")):
        txt = p.read_text(encoding="utf-8", errors="replace")
        for m in STUB.finditer(txt):
            head = m.group("head")
            va = int(m.group("va"), 16)
            params = m.group("params")
            n, unknown = param_bytes(params)
            if n is not None and "__fastcall" in head and n >= 8:
                n -= 8          # the first two arguments travel in ecx / edx
            name = re.sub(r"\s+", " ", head).split()[-1]
            # a class returned by value (FString, NString, a struct that is not 1, 2, 4 or 8 bytes) comes back through a
            # hidden pointer the callee pops (thiscall / cdecl alike: one more dword of `ret N`)
            rtype = re.sub(r"\b(static|inline|virtual|const|__declspec\(\w+\)|NW_NAKED)\b\s*", "", head).strip().split(" ")
            if n is not None and len(rtype) >= 2 and "*" not in rtype[0] and "&" not in rtype[0]:
                rt = rtype[0]
                if rt in ("FString", "NString", "NStdString", "NStlWString") or (
                        rt in STRUCT_SIZE and STRUCT_SIZE[rt] not in (1, 2, 4, 8)) or (rt in SIZES and SIZES[rt] > 8):
                    n += 4
            is_member = "::" in name and name.split("::")[-1] not in STATIC_NAMES
            rets = stock_rets(va)
            line = txt[:m.start()].count("\n") + 1
            if n is None:
                status = "varargs"
            elif not rets:
                status = "no ret found"
            elif is_member:
                ok = rets == {n} or (n in rets and len(rets) > 1 and 0 not in rets)
                status = "ok" if ok else "MISMATCH"
            else:
                # a free / static function: cdecl pops nothing, stdcall pops the arguments
                ok = rets <= {0, n} and (n in rets or 0 in rets)
                status = "ok" if ok else "MISMATCH"
            if unknown and status == "MISMATCH" and any(r >= n and (r - n) % 4 == 0 for r in rets):
                status = "ok (struct by value of unknown size: %s)" % ",".join(unknown)       # the stock pops more: it may be it
            if unknown and status == "ok":
                status = "ok (struct by value unknown: %s)" % ",".join(unknown)
            if status == "MISMATCH":
                bad += 1
            rows.append((p.name, line, name, "%08x" % va, n, sorted(rets), status, params.strip()[:120]))
    with (ROOT / "db" / "stub_check.csv").open("w", encoding="utf-8", newline="") as f:
        w = csv.writer(f)
        w.writerow(["file", "line", "name", "va", "declared_bytes", "stock_ret", "status", "params"])
        for r in rows:
            w.writerow(r)
    print("%d stubs, %d mismatch" % (len(rows), bad))
    for r in rows:
        if r[6] == "MISMATCH" or (a.all and r[6] != "ok"):
            print("  %-26s:%-5d %-48s %s declared %s stock ret %s" % (r[0], r[1], r[2][:48], r[3], r[4], r[5]))
    # A mismatching stub is harmless while nothing compiled calls it (the generator demotes every copied function
    # that names one): look for the direct calls in the DLL that was built
    callers = compiled_callers([r for r in rows if r[6] == "MISMATCH"])
    if callers is not None:
        print("%d compiled call site(s) of a mismatching stub" % len(callers))
        for site, fn, stub in callers[:40]:
            print("  %s calls %s" % (fn, stub))
        bad += len(callers)
    return 1 if bad else 0


def compiled_callers(mismatches):
    """(call site, calling function, stub) for every direct call/jmp in build/bin/Release/NWindowDecomp.dll that
    lands on the compiled body of a mismatching stub; None when there is no map or DLL to read."""
    import bisect
    import pefile
    dll = ROOT / "build" / "bin" / "Release" / "NWindowDecomp.dll"
    mp = dll.with_suffix(".map")
    if not dll.exists() or not mp.exists():
        return None
    syms = []
    for ln in mp.read_text(encoding="latin-1").splitlines():
        m = re.match(r"^\s*0001:[0-9a-f]{8}\s+(\S+)\s+([0-9a-f]{8})\s+f", ln)
        if m:
            syms.append((int(m.group(2), 16), m.group(1)))
    syms.sort()
    starts = [s[0] for s in syms]
    want = {}
    for r in mismatches:
        cls, _, meth = r[2].rpartition("::")
        cls = cls.split("::")[-1]
        for va, nm in syms:
            if nm.startswith("?%s@%s@@" % (meth, cls)):
                want[va] = r[2]
    pe = pefile.PE(str(dll))
    base = pe.OPTIONAL_HEADER.ImageBase
    text = next(s for s in pe.sections if s.Name.startswith(b".text"))
    lo = base + text.VirtualAddress
    data = text.get_data()[:text.Misc_VirtualSize]
    # only the stubs themselves (mov eax, [g_nwBase] / add eax, imm / jmp eax): the name also matches the
    # overloads of the same method that are ported
    for va in list(want):
        body = data[va - lo:va - lo + 12]
        if not (body[:1] == b"\xa1" and body[5:6] == b"\x05" and body[10:12] == b"\xff\xe0"):
            del want[va]
    out = []
    for m in re.finditer(rb"[\xe8\xe9](....)", data, re.S):
        tgt = lo + m.start() + 5 + int.from_bytes(m.group(1), "little", signed=True)
        if tgt in want:
            site = lo + m.start()
            i = bisect.bisect_right(starts, site) - 1
            fn = syms[i][1] if i >= 0 else "?"
            if starts[i] != tgt:
                out.append((site, fn, want[tgt]))
    return out


if __name__ == "__main__":
    raise SystemExit(main())
