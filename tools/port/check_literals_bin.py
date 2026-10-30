#!/usr/bin/env python3
"""check_literals on the COMPILED code: every stock function our compiled hooks call by address must be one the stock function uses too.

tools/port/check_literals.py reads the source (definitions, calls by name, headers) and has to guess what a name means. This one reads what the
compiler made of it: for each hook (the `ours=` RVAs of build/smoke_system/NWindowDecomp.log, the function extents of the PDB) it decodes our
function and every function of ours it calls that is not a hook itself (a header helper, an NW_STOCK stub, a static function; a hooked method is
recognised by its decorated name: its literals are its own business), and collects the stock addresses it puts in a register or an operand:
  * a full stock VA that is NOT relocated (`push 0x101029E0`: StockCallAt(va, ...) passes the VA; our own addresses carry a relocation);
  * g_nwBase + RVA (`mov eax, [g_nwBase]; add eax, imm` / `lea ecx, [eax + imm]` / `call [eax + imm]`: what Addr(va) and NW_STOCK compile to),
    tracked through the registers that hold g_nwBase (found in the .map).
A literal that is a stock function entry and that the stock function (its Ghidra ranges, EH funclets, loop-head pieces) does not use is printed.
The compiler inlines hooked methods into each other and folds identical functions, so a hit is a lead: wave C read all of them (the ones left are an
inlined callee, a by-value forwarder's copy constructor, an inlined `_Xlen`); an independent look at what check_literals reads from the source.

usage: python tools/port/check_literals_bin.py [--data] [--why VA LITERAL]     (after tools/cycle.py: it reads build/)"""
from __future__ import annotations

import argparse
import bisect
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
sys.path.insert(0, str(ROOT / "tools" / "port"))
import pefile  # noqa: E402
from capstone import CS_ARCH_X86, CS_MODE_32, Cs  # noqa: E402
from capstone.x86 import X86_OP_IMM, X86_OP_MEM  # noqa: E402

import check_literals as cl  # noqa: E402
import verify_hooks as vh  # noqa: E402

S_LO, S_HI = 0x10001000, 0x10330000
BUILD = ROOT / "build"


def demangle(lbl: str):
    m = re.match(r"^\?\?0(\w+)@@", lbl)
    if m:
        return m.group(1) + "::" + m.group(1)
    m = re.match(r"^\?\?1(\w+)@@", lbl)
    if m:
        return m.group(1) + "::~" + m.group(1)
    m = re.match(r"^\?(\w+)@(\w+)@@", lbl)
    if m:
        return m.group(2) + "::" + m.group(1)
    return None


class Compiled:
    def __init__(self, dll: Path, log: Path, map_file: Path):
        self.pe = pefile.PE(str(dll))
        self.base = self.pe.OPTIONAL_HEADER.ImageBase
        self.mem = self.pe.get_memory_mapped_image()
        self.relocs = {self.base + e.rva for blk in self.pe.DIRECTORY_ENTRY_BASERELOC for e in blk.entries if e.type == 3}
        text = next(s for s in self.pe.sections if s.Name.startswith(b".text"))
        self.t_lo, self.t_hi = self.base + text.VirtualAddress, self.base + text.VirtualAddress + text.Misc_VirtualSize
        self.syms = vh.our_symbols(dll)
        self.starts = [a for a, _ in self.syms]
        self.label = dict(self.syms)
        m = re.search(r"^\s*\d+:[0-9a-f]+\s+_g_nwBase\s+([0-9a-f]{8})", map_file.read_text(encoding="latin-1"), re.M)
        if not m:
            raise SystemExit("g_nwBase not found in %s" % map_file)
        self.gbase = int(m.group(1), 16)
        self.md = Cs(CS_ARCH_X86, CS_MODE_32)
        self.md.detail = True
        self.hooks = []
        for line in log.read_text(encoding="latin-1").splitlines():
            mm = re.match(r"^\S+\s+([0-9a-f]{8}) (.+?)\s+ours=([0-9a-f]+) ", line)
            if mm:
                self.hooks.append((int(mm.group(1), 16), mm.group(2), self.base + int(mm.group(3), 16)))
        self.hook_ours = {o for _, _, o in self.hooks}
        self.hooked_names = {re.sub(r"\(.*$", "", n).strip() for _, n, _ in self.hooks}
        self.cache: dict[int, tuple[set[int], set[int]]] = {}
        self.funcs: set[int] = set()

    def is_hook(self, addr: int) -> bool:
        return addr in self.hook_ours or demangle(self.label.get(addr, "")) in self.hooked_names

    def scan(self, va: int):
        if va in self.cache:
            return self.cache[va]
        i = bisect.bisect_right(self.starts, va)
        end = self.starts[i] if i < len(self.starts) else self.t_hi
        lits: set[int] = set()
        calls: set[int] = set()
        data = self.mem[va - self.base:end - self.base]
        addr = va
        taint: set[str] = set()
        while addr < end:
            got = False
            for ins in self.md.disasm(data[addr - va:], addr):
                if ins.address >= end:
                    break
                got = True
                addr = ins.address + ins.size
                mn, ops = ins.mnemonic, ins.operands
                for op in ops:
                    v = offs = None
                    if op.type == X86_OP_IMM:
                        v, offs = op.imm & 0xFFFFFFFF, ins.imm_offset
                    elif op.type == X86_OP_MEM and op.mem.disp:
                        v, offs = op.mem.disp & 0xFFFFFFFF, ins.disp_offset
                    if v is None:
                        continue
                    if mn == "call" and op.type == X86_OP_IMM and self.t_lo <= v < self.t_hi:
                        calls.add(v)
                        continue
                    if mn.startswith("j") and op.type == X86_OP_IMM:
                        continue
                    if offs and (ins.address + offs) in self.relocs:
                        if v == self.gbase and mn == "mov" and len(ops) == 2 and ops[0].type == 1:      # mov reg, [g_nwBase]
                            taint.add(ins.reg_name(ops[0].reg))
                        continue
                    if S_LO <= v < S_HI:
                        lits.add(v)
                if mn == "add" and len(ops) == 2 and ops[0].type == 1 and ops[1].type == X86_OP_IMM and ins.reg_name(ops[0].reg) in taint:
                    lits.add(0x10000000 + (ops[1].imm & 0xFFFFFFFF))
                elif mn in ("lea", "mov", "call", "jmp", "push", "cmp", "fld", "fild", "movzx", "movsx", "test"):
                    for op in ops:
                        if op.type == X86_OP_MEM and op.mem.base and ins.reg_name(op.mem.base) in taint and op.mem.disp and op.mem.index == 0:
                            d = op.mem.disp & 0xFFFFFFFF
                            if d >= 0x1000:
                                lits.add(0x10000000 + d)
                            if mn == "lea" and ops[0].type == 1:
                                taint.add(ins.reg_name(ops[0].reg))
                if mn == "call":
                    taint -= {"eax", "ecx", "edx"}
                elif ops and ops[0].type == 1 and mn not in ("cmp", "test", "push", "add") and \
                        not (mn == "mov" and len(ops) == 2 and ops[1].type == X86_OP_MEM and ops[1].mem.disp == self.gbase):
                    r0 = ins.reg_name(ops[0].reg)
                    if mn == "mov" and len(ops) == 2 and ops[1].type == 1 and ins.reg_name(ops[1].reg) in taint:
                        taint.add(r0)
                    elif not (mn == "lea" and r0 in taint):
                        taint.discard(r0)
            if not got or addr < end:
                addr += 1
            if got and addr >= end:
                break
        self.cache[va] = (lits, calls)
        return self.cache[va]

    def closure(self, start: int, depth: int = 0, seen: set | None = None, want: int | None = None, path: tuple = ()):
        """the literals of start and of the non-hook functions it calls; with `want`, the call path to the first function that has it"""
        seen = seen if seen is not None else set()
        if start in seen:
            return set() if want is None else None
        seen.add(start)
        lits, calls = self.scan(start)
        if want is not None:
            if want in lits:
                return path + (start,)
        out = set(lits)
        if depth < 6:
            for c in calls:
                if self.is_hook(c):
                    continue
                r = self.closure(c, depth + 1, seen, want, path + (start,))
                if want is not None:
                    if r:
                        return r
                else:
                    out |= r
        return out if want is None else None


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--data", action="store_true", help="also the data addresses (UClass objects, tables, strings of the stock image)")
    ap.add_argument("--why", nargs=2, metavar=("VA", "LITERAL"), help="the call path from the hook at VA to the function that names LITERAL")
    ap.add_argument("--dll", type=Path, default=BUILD / "bin" / "Release" / "NWindowDecomp.dll")
    ap.add_argument("--log", type=Path, default=BUILD / "smoke_system" / "NWindowDecomp.log")
    ap.add_argument("--map", type=Path, default=BUILD / "bin" / "Release" / "NWindowDecomp.map")
    a = ap.parse_args()
    st = cl.Stock()
    cc = Compiled(a.dll, a.log, a.map)
    cc.funcs = set(st.rows)
    if a.why:
        va, lit = int(a.why[0], 16), int(a.why[1], 16)
        ours = next(o for v, n, o in cc.hooks if v == va)
        p = cc.closure(ours, want=lit)
        print("path:", [(hex(x), cc.label.get(x, "?")[:70]) for x in (p or ())])
        return 0
    res = []
    for va, name, ours in sorted(set(cc.hooks)):
        if va not in st.rows:
            continue
        seen, _ = st.seen(va)
        seen = seen | {va}
        miss = sorted(x for x in cc.closure(ours) if x not in seen and x != va and (a.data or x in st.rows))
        miss = [x for x in miss if x & 0xFFFFFF00 != 0x10000000 or x in st.rows]
        if miss:
            res.append((va, name, miss))
    for va, name, miss in res:
        print("%08x %s: %s" % (va, name, " ".join("%08x%s" % (x, ("(" + st.rows[x]["name"][:26] + ")") if x in st.rows else "") for x in miss[:6])))
    print("check_literals_bin: %d compiled hook(s), %d name a stock %s the stock function does not use" % (
        len(cc.hooks), len(res), "address" if a.data else "function"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
