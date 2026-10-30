#!/usr/bin/env python3
"""Compare every reimplemented function with the stock one it replaces.

Not a byte comparison (different compiler): the things that must match for the replacement to
be ABI-correct, taken from the disassembly of both sides:
  ret      stack bytes popped on return (parameter count / calling convention)
  vcalls   virtual call offsets, `call [reg+X]` or `mov r,[reg+X] ... call r` (vtable slots used)
  imports  Core/Engine/Win32 functions called (appUnwindf, from guard/unguard funclets, ignored)
Sets are compared, not counts (one call site duplicated per branch is fine). Functions that are
only a call/jmp to another function of the same module are followed, on both sides, so inlining
differences between the two compilers do not show.

Every remaining difference must be explained before the function counts as verified: write the
explanation in db/verify_notes.csv (va,explanation) and it is reported as NOTE instead of DIFF.

Our side: "ours=" RVAs in NWindowDecomp.log (written when the DLL loads) + function boundaries
from the PDB through dumpbin /disasm.
  python tools/verify_hooks.py [--log FILE] [--dll FILE]
"""
from __future__ import annotations

import argparse
import bisect
import csv
import os
import re
import subprocess
import sys
from pathlib import Path

import pefile
from capstone import CS_ARCH_X86, CS_MODE_32, Cs

sys.path.insert(0, str(Path(__file__).resolve().parent))
from nwd.image import ROOT, Image  # noqa: E402

LOG_RE = re.compile(r"^\S+\s+([0-9a-f]{8}) (.+?)\s+ours=([0-9a-f]+) (.*)$")
VT_RE = re.compile(r"^\S+\s+vt (\S+) (\d+) stock=([0-9a-f]{8}) ours=([0-9a-f]+)")
MEMREG_RE = re.compile(r"^dword ptr \[(e[a-ds][xip]|e[sd]i|ebp)(?: \+ (0x[0-9a-f]+))?\]$")
MEMABS_RE = re.compile(r"^dword ptr \[(0x[0-9a-f]+)\]$")
STACKSLOT_RE = re.compile(r"^dword ptr \[e[bs]p [+-] (?:0x[0-9a-f]+|\d)\]$")  # capstone: < 10 decimal
IGNORED_IMPORTS = {"?appUnwindf@@YAXPBGZZ"}
STOCK_PURECALL = 0x10171E13  # _purecall of the stock static CRT (C4 image; C3: 0x1014D4CA, C2: 0x100E84EA, C1: 0x100BE4FC)


def dumpbin_path() -> str:
    hits = sorted(Path("C:/Program Files/Microsoft Visual Studio").glob(
        "2022/*/VC/Tools/MSVC/*/bin/Hostx64/x86/dumpbin.exe"))
    if not hits:
        raise SystemExit("dumpbin.exe not found (Visual Studio 2022 C++ tools)")
    return str(hits[-1])


def our_symbols(dll: Path) -> list[tuple[int, str]]:
    """Symbol labels of our DLL (from its PDB, through dumpbin) -> sorted (address, label)."""
    out = subprocess.run([dumpbin_path(), "/nologo", "/disasm:nobytes", str(dll)],
                         capture_output=True, text=True, errors="replace").stdout
    syms: dict[int, str] = {}
    label = None
    for line in out.splitlines():
        if line and not line.startswith(" ") and line.endswith(":"):
            label = line[:-1]
            continue
        if label:
            m = re.match(r"\s+([0-9A-F]{8}):", line)
            if m:
                syms.setdefault(int(m.group(1), 16), label)
            label = None
    return sorted(syms.items())


def method_of(label: str) -> str:
    """Method part of a decorated or pretty label: ?Foo@NCWnd@@... -> Foo, ??1NCWnd@@ -> ~NCWnd."""
    if label.startswith("??_G"):
        return "`scalar deleting destructor'"
    if label.startswith("??_E"):
        return "`vector deleting destructor'"
    if label.startswith("??1"):
        return "~" + label[3:].split("@")[0]
    if label.startswith("??0"):
        return label[3:].split("@")[0]
    if label.startswith("?"):
        return label[1:].split("@")[0]
    return label.split("::")[-1].split("<")[0]


class Code:
    """A module to disassemble: read(va, n), imports {iat: name}, function extent."""

    def __init__(self, read, imports, extent, internal):
        self.read, self.imports, self.extent, self.internal = read, imports, extent, internal


# The stock image's Code, set by main(): a stub of ours (NW_STOCK: mov eax, [g_nwBase]; add eax, imm; jmp eax) stands for
# the stock function it jumps to, which `features` follows into this module
STOCK_CODE = None
STUB_BASE = 0x10000000

REGS = {"eax", "ebx", "ecx", "edx", "esi", "edi", "ebp"}
NO_WRITE = {"cmp", "test", "push", "call", "jmp", "ret", "retn"}


JUMPTABLE_RE = re.compile(r"^dword ptr \[e[a-ds][xip]\*4 \+ (0x[0-9a-f]+)\]$")
BYTETABLE_RE = re.compile(r"^byte ptr \[e[a-ds][xip] \+ (0x[0-9a-f]+)\]$")


def decode_skipping_tables(md, code: Code, a: int, b: int):
    """Instructions of [a, b) without the switch tables the compiler put in the range
    (`jmp [reg*4 + table]` dword tables; `movzx reg, byte ptr [reg + table]` index tables,
    which run to the end of the range): decoded as code they give bogus rets and calls."""
    tables: dict[int, int] = {}
    va = a
    data = code.read(a, b - a)
    while va < b:
        if va in tables:
            va = max(tables[va], va + 1)
            continue
        ins = next(md.disasm(data[va - a:va - a + 16], va), None)
        if ins is None:
            va += 1
            continue
        yield ins
        m = JUMPTABLE_RE.search(ins.op_str) if ins.mnemonic == "jmp" else None
        if m:
            t = int(m.group(1), 16)
            if a <= t < b:
                end = t
                while end + 4 <= b and a <= int.from_bytes(data[end - a:end - a + 4], "little") < b:
                    end += 4
                tables[t] = end
        m = BYTETABLE_RE.search(ins.op_str.split(",")[-1].strip()) if ins.mnemonic == "movzx" else None
        if m:
            t = int(m.group(1), 16)
            if a <= t < b:
                tables[t] = b
        va += ins.size


def features(md, code: Code, start: int, depth: int = 0):
    ret, vcalls, imports, calls = set(), set(), set(), []
    # what a register holds when it is called through: ("vt", offset) after
    # `mov r, [reg+X]`, ("imp", name) after `mov r, [IAT]`
    held: dict[str, tuple] = {}
    # stack slots holding an import (`mov dword ptr [ebp - X], reg` after `mov reg, [IAT]`)
    # or a vtable slot (after `mov reg, [reg2 + X]`): the current compiler caches often-called
    # imports there and calls through them, and spills a virtual's address before a call
    held_mem: dict[str, tuple] = {}
    extent = code.extent(start)
    for a, b in extent:
        for ins in decode_skipping_tables(md, code, a, b):
            op = ins.op_str
            mn = ins.mnemonic
            if mn in ("ret", "retn"):
                ret.add(int(op, 16) if op else 0)
            parts = [p.strip() for p in op.split(",", 1)] if op else []
            if mn == "mov" and len(parts) == 2 and STACKSLOT_RE.match(parts[0]):
                src = held.get(parts[1])
                if src and src[0] in ("imp", "vt"):
                    held_mem[parts[0]] = src
                else:
                    held_mem.pop(parts[0], None)
            if mn == "call" and op in held_mem:
                kind, v = held_mem[op]
                (vcalls if kind == "vt" else imports).add(v)
                continue
            # `lea r, [r]` is an alignment filler before a loop head: r keeps what it held
            if mn == "lea" and len(parts) == 2 and parts[1] == f"[{parts[0]}]":
                continue
            # so is `mov r, r` (the two-byte nop the compiler puts before a loop head)
            if mn == "mov" and len(parts) == 2 and parts[0] == parts[1] and parts[0] in REGS:
                continue
            if parts and parts[0] in REGS and mn not in NO_WRITE and not mn.startswith("j"):
                held.pop(parts[0], None)
                if mn == "mov" and len(parts) == 2:
                    m = MEMREG_RE.match(parts[1])
                    m2 = MEMABS_RE.match(parts[1])
                    if m:
                        held[parts[0]] = ("vt", int(m.group(2), 16) if m.group(2) else 0)
                    elif m2 and int(m2.group(1), 16) in code.imports:
                        held[parts[0]] = ("imp", code.imports[int(m2.group(1), 16)])
            if mn in ("call", "jmp"):
                # the current compiler merges tails: `mov eax, [eax + X]; jmp common` where the
                # common tail does `call eax` - follow the jump a few instructions
                if mn == "jmp" and op.startswith("0x") and any(x <= int(op, 16) < y for x, y in extent):
                    t = int(op, 16)
                    for reg, (kind, v) in list(held.items()):
                        if kind != "vt":
                            continue
                        for ins2 in list(md.disasm(code.read(t, 64), t))[:16]:
                            p2 = [q.strip() for q in ins2.op_str.split(",", 1)] if ins2.op_str else []
                            if ins2.mnemonic == "call" and ins2.op_str == reg:
                                vcalls.add(v)
                                break
                            if ins2.mnemonic in ("ret", "retn", "jmp") or \
                                    (p2 and p2[0] == reg and ins2.mnemonic not in NO_WRITE):
                                break
                m = MEMREG_RE.match(op)
                if m:
                    vcalls.add(int(m.group(2), 16) if m.group(2) else 0)
                elif op in held:
                    kind, v = held[op]
                    (vcalls if kind == "vt" else imports).add(v)
                m2 = MEMABS_RE.match(op)
                if m2 and int(m2.group(1), 16) in code.imports:
                    imports.add(code.imports[int(m2.group(1), 16)])
                # jumps inside the function itself are control flow, not calls
                if op.startswith("0x") and code.internal(int(op, 16)) and                         not any(x <= int(op, 16) < y for x, y in extent):
                    t = int(op, 16)
                    # a `jmp [IAT]` thunk (the stock linker's stubs for imports called without
                    # dllimport) is the import itself
                    stub = code.read(t, 6)
                    if stub[:2] == bytes((0xFF, 0x25)) and int.from_bytes(stub[2:6], "little") in code.imports:
                        imports.add(code.imports[int.from_bytes(stub[2:6], "little")])
                        continue
                    if STOCK_CODE is not None and code is not STOCK_CODE:
                        b = code.read(t, 12)
                        # mov eax, [g_nwBase] (A1 abs32); add eax, imm32 (05) or imm8 (83 C0); jmp eax (FF E0)
                        if len(b) == 12 and b[0] == 0xA1 and b[5] == 0x05 and tuple(b[10:12]) == (0xFF, 0xE0):
                            calls.append((mn, ("stock", STUB_BASE + int.from_bytes(b[6:10], "little"))))
                            continue
                        if len(b) == 12 and b[0] == 0xA1 and tuple(b[5:7]) == (0x83, 0xC0) and tuple(b[8:10]) == (0xFF, 0xE0):
                            calls.append((mn, ("stock", STUB_BASE + int.from_bytes(b[7:8], "little", signed=True))))
                            continue
                    calls.append((mn, t))
    imports -= IGNORED_IMPORTS
    # a forwarder (one call/jmp to a sibling, no other call) stands for its target
    def follow(target, d):
        if isinstance(target, tuple):
            return features(md, STOCK_CODE, target[1], d)
        return features(md, code, target, d)

    if depth < 3 and len(calls) == 1 and not vcalls and not imports:
        r2, v2, i2 = follow(calls[0][1], depth + 1)
        return (ret or r2) if calls[0][0] == "call" else r2, v2, i2
    # no ret of its own: every exit is a tail jump, which returns with the target's ret
    if not ret and depth < 3:
        for mn, target in calls:
            if mn == "jmp":
                ret |= follow(target, depth + 1)[0]
    return ret, vcalls, imports


def main() -> int:
    from install_test import DEFAULT_SYSTEM

    ap = argparse.ArgumentParser()
    ap.add_argument("--log", type=Path, default=DEFAULT_SYSTEM / "NWindowDecomp.log" if DEFAULT_SYSTEM else None)
    ap.add_argument("--dll", type=Path, default=ROOT / "build" / "bin" / "Release" / "NWindowDecomp.dll")
    args = ap.parse_args()
    if args.log is None:
        ap.error("pass --log FILE or set NWD_CLIENT_SYSTEM (the test client's System folder, where NWindowDecomp.log is)")

    md = Cs(CS_ARCH_X86, CS_MODE_32)

    # stock side: Ghidra ranges (main body only; catch/unwind funclets are separate functions)
    stock = Image()
    granges = {}
    for r in csv.DictReader((ROOT / "analysis/ghidra/functions.tsv").open(encoding="utf-8"), delimiter="\t"):
        granges[int(r["va"], 16)] = [tuple(int(x, 16) for x in rg.split("-")) for rg in r["ranges"].split(",")]
    # code the analysis split off a function (db/not_functions.csv) belongs to its owner
    for r in csv.DictReader(l for l in (ROOT / "db/not_functions.csv").open(encoding="utf-8") if not l.startswith("#")):
        owner, frag = int(r["part_of"], 16), int(r["va"], 16)
        if owner in granges and frag in granges:
            granges[owner] = granges[owner] + granges[frag]
    for r in csv.DictReader(l for l in (ROOT / "db/extra_functions.csv").open(encoding="utf-8") if not l.startswith("#")):
        va = int(r["va"], 16)
        granges[va] = [(va, va + int(r["size"]) - 1)]
    s_lo, s_hi = stock.sect_range(".text")
    stock_code = Code(stock.read, {va: n for va, (_, n) in stock.imports.items()},
                      lambda va: [(a, b + 1) for a, b in granges.get(va, [(va, va + 0x3F)])],
                      lambda va: s_lo <= va < s_hi)

    # our side: PDB function boundaries
    pe = pefile.PE(str(args.dll))
    base = pe.OPTIONAL_HEADER.ImageBase
    mem = pe.get_memory_mapped_image()
    o_imports = {i.address: (i.name or b"").decode() for e in pe.DIRECTORY_ENTRY_IMPORT for i in e.imports}
    symbols = our_symbols(args.dll)
    starts = [a for a, _ in symbols]
    label_at = dict(symbols)
    o_text = next(s for s in pe.sections if s.Name.startswith(b".text"))
    o_lo, o_hi = base + o_text.VirtualAddress, base + o_text.VirtualAddress + o_text.Misc_VirtualSize

    def o_extent(va):
        i = bisect.bisect_right(starts, va)
        end = starts[i] if i < len(starts) else o_hi
        return [(va, end)]

    ours_code = Code(lambda va, n: mem[va - base:va - base + n], o_imports, o_extent,
                     lambda va: o_lo <= va < o_hi)
    global STOCK_CODE
    STOCK_CODE = stock_code

    notes = {}
    notes_file = ROOT / "db" / "verify_notes.csv"
    if notes_file.exists():
        for r in csv.DictReader(l for l in notes_file.open(encoding="utf-8") if not l.startswith("#")):
            notes[int(r["va"], 16)] = r["explanation"]

    rows = []
    for line in args.log.read_text(encoding="latin-1").splitlines():
        m = LOG_RE.match(line)
        if m:
            rows.append((int(m.group(1), 16), m.group(2), int(m.group(3), 16), m.group(4)))
    if not rows:
        text = args.log.read_text(encoding="latin-1") if args.log.exists() else ""
        # the loader ran our DllMain and found nothing registered (the infrastructure build, with an
        # empty game source): zero hooks, nothing to compare, and it is a pass
        if re.search(r"hooks: 0 installed, 0 registered, 0 errors", text):
            print(f"0 hooks registered in {args.log.name}: nothing to compare")
            print()
            print("0 ok, 0 explained, 0 unexplained")
            return 0
        print(f"no hook lines in {args.log} (run the game or tools/smoke first)")
        return 1

    n_diff = n_note = 0
    for va, name, rva, state in sorted(rows):
        sr, sv, si = features(md, stock_code, va)
        orr, ov, oi = features(md, ours_code, base + rva)
        # slot 0 of an allocator: the stock build inlines `new` (GMalloc->Malloc(size, L"new")), ours calls the
        # out-of-line operator new of NwMemory.cpp: the one virtual call is the same allocation
        sv, ov = sv - {0}, ov - {0}
        diffs = []
        if sr != orr:
            diffs.append(f"ret     stock {sorted(sr)} ours {sorted(orr)}")
        if sv != ov:
            diffs.append("vcalls  stock {%s} ours {%s}" % (", ".join(f"+{x:#x}" for x in sorted(sv)),
                                                          ", ".join(f"+{x:#x}" for x in sorted(ov))))
        if si != oi:
            diffs.append(f"imports stock {sorted(si)} ours {sorted(oi)}")
        if not diffs:
            tag = "OK  "
        elif va in notes:
            tag, n_note = "NOTE", n_note + 1
        else:
            tag, n_diff = "DIFF", n_diff + 1
        print(f"{tag} {va:08x} {name} [{state}]")
        for d in diffs:
            print(f"       {d}")
        if diffs and va in notes:
            print(f"       -> {notes[va]}")
    print(f"\n{len(rows) - n_diff - n_note} ok, {n_note} explained, {n_diff} unexplained")

    # ---- vtables: our compiled one vs the stock one, slot by slot ----------------------------
    # (NW_CHECK_VTABLE logs "vt Class slot stock=VA ours=RVA"). Import slots must reach the same
    # import; code slots must pop the same stack bytes and, when the stock function has a real
    # name, carry the same method name.
    db_rows = list(csv.DictReader((ROOT / "db/functions.csv").open(encoding="utf-8")))
    db_names = {int(r["va"], 16): r["name"] for r in db_rows}
    # stock bodies the linker folded: one address serving several slot numbers
    db_folded = {int(r["va"], 16) for r in db_rows if r.get("notes", "").count("vslot") > 1}
    vts: dict[str, list] = {}
    for line in args.log.read_text(encoding="latin-1").splitlines():
        m = VT_RE.match(line)
        if m:
            vts.setdefault(m.group(1), []).append((int(m.group(2)), int(m.group(3), 16), int(m.group(4), 16)))

    def import_thunk(read, imports, va):
        b = read(va, 6)
        if b[:2] == b"\xff\x25":
            return imports.get(int.from_bytes(b[2:6], "little"))
        return None

    # classes whose constructor we hook: their objects carry OUR vtable (since wave 1), so a slot that reaches another
    # function than the stock slot does is a live bug, not an informational diff
    ctor_hooked = set()
    for _va, _name, _rva, _state in rows:
        m = re.match(r"^(\w+)::(\w+)(?:\(|$)", _name)
        if m and m.group(1) == m.group(2):
            ctor_hooked.add(m.group(1))
    VT_IGNORE = {"NCCmdTarget"}      # C3 vtable has 25 slots; classes.json counts the next class' vtable too (39)
    n_vt_bad = n_vt_real = 0
    for cls, slots in sorted(vts.items()):
        bad = []
        uses = {}
        for _, sva, _ in slots:
            uses[sva] = uses.get(sva, 0) + 1
        for k, sva, orva in sorted(slots):
            ova = base + orva
            s_imp = import_thunk(stock.read, stock_code.imports, sva)
            o_imp = import_thunk(ours_code.read, ours_code.imports, ova)
            if s_imp or o_imp:
                if s_imp != o_imp:
                    bad.append(f"slot {k}: stock {s_imp or db_names.get(sva)} ours {o_imp or label_at.get(ova)}")
                continue
            o_label = label_at.get(ova, "?")
            if sva == STOCK_PURECALL and "purecall" in o_label:
                continue  # pure virtual: both runtimes' _purecall
            # NW_STOCK stub (declared, not decompiled yet): mov eax, [g_nwBase]; add eax, va -
            # 0x10000000; jmp eax - fine when it jumps to the stock slot's own function
            stub = ours_code.read(ova, 12)
            if len(stub) < 12:
                bad.append(f"slot {k}: our vtable entry {orva:08x} is outside our code (stock slot {sva:08x})")
                continue
            if stub[0] == 0xA1 and stub[5] == 0x05 and stub[10:12] == bytes((0xFF, 0xE0)):
                if int.from_bytes(stub[6:10], "little") + 0x10000000 == sva:
                    continue
                bad.append(f"slot {k}: NW_STOCK stub jumps to "
                           f"{int.from_bytes(stub[6:10], 'little') + 0x10000000:08x}, stock slot {sva:08x}")
                continue
            sr, _, _ = features(md, stock_code, sva)
            orr, _, _ = features(md, ours_code, ova)
            s_name = db_names.get(sva, f"{sva:08x}").split("::")[-1]
            o_name = method_of(o_label)
            # a stock body shared by several slots (linker-folded defaults) has one db name only
            generic = s_name.startswith(("vfn", "sub_")) or uses[sva] > 1 or sva in db_folded
            # MSVC 14 puts the vector deleting destructor in slot 3 where VC7.0 had the scalar
            # one; called with flag 1 (delete p) both run ~T() + operator delete: same ABI
            if "deleting destructor" in s_name and "deleting destructor" in o_name:
                generic = True
            if sr != orr:
                bad.append(f"slot {k}: ret stock {sorted(sr)} ({s_name}) ours {sorted(orr)} ({o_name})")
            elif not generic and s_name != o_name and s_name not in o_label:
                bad.append(f"slot {k}: name stock {s_name} ours {o_name} (same ret)")
        n_vt_bad += bool(bad)
        # a diff of the name only is benign; a `ret` with an unknown stock side (`[]`: a tail jump) is not decidable
        real = [b for b in bad if ": name stock " not in b and ": ret stock [] " not in b]
        is_real = bool(real) and cls in ctor_hooked and cls not in VT_IGNORE
        n_vt_real += is_real
        print(f"{'VT BAD ' if is_real else 'VT OK  ' if not bad else 'VT DIFF'} {cls} ({len(slots)} slots)")
        for b in bad:
            print(f"       {b}")
    # C2 (docs/notas/C2_Porte.md): in hook mode our compiled vtables are never used (the objects are
    # built by the stock code), the slot numbers our code calls are checked per function above
    # ("vcalls"), and the C2 headers still carry placeholders for the virtuals only C2 has. The
    # vtable diffs are informational until the standalone phase (NWD_VT_STRICT=1 makes them fail).
    if n_vt_bad:
        print(f"note: {n_vt_bad} vtables differ ({n_vt_real} of them belong to a class whose constructor is hooked and "
              f"reach another function than the stock slot: those fail; NWD_VT_STRICT=1 makes every diff fail)")
    return 1 if (n_diff or n_vt_real or (n_vt_bad and os.environ.get("NWD_VT_STRICT"))) else 0


if __name__ == "__main__":
    os.environ.setdefault("MSYS_NO_PATHCONV", "1")
    sys.exit(main())
