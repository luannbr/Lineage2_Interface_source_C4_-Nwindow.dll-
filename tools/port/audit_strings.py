#!/usr/bin/env python3
"""Static audit of the CONTENT of the string literals (pass 1) and of the virtual slot of the calls through the engine interface objects (pass 2)
of every hooked function, stock against ours.

Why: the judges of the project (verify_hooks.py, verify_data.py, check_literals.py ...) compare call targets, vcall slot SETS (verify_hooks drops slot 0 on both
sides), imports and addresses, but never what a string literal says. The differential execution under unicorn (docs/notas/C4_EmuSA.md) found two real bugs that all of
them had missed: NCMultiEdit.cpp VK_RETURN pushed L"\\n" where the stock pushes L"\\r\\n" (0x101A6288), and ChatWnd::ChatWnd called FConfigCache vslot 1 (GetInt)
where the stock calls vslot 0 (GetBool). This tool looks for that class of bug statically, in every hook, in about a minute.

Input: build/smoke_system/NWindowDecomp.log (`<stock va> <name> ours=<rva> ok`: the hook registrations), build/bin/Release/NWindowDecomp.dll + .map (ours; both are
copied to a temp file first, a build may replace them), orig/NWindow.dll with its Ghidra ranges (the same function extents as tools/verify_hooks.py).

Pass 1 (strings). On each side, every immediate / absolute operand (push imm, mov r,imm, lea, mov/cmp with an absolute address) that points into a data section and decodes as
a NUL-terminated string (ANSI: >= 2 printable characters; UTF-16LE: ASCII, Latin, Hangul, CJK, punctuation) is a string of the function; a pointer to a table of string
pointers counts the entries, a global that holds the pointer of a string (`mov edx,[g_pszFoo]`) counts the string. The body of a hooked function on our side is the compiled
function, with the hook-only wrapper (`??$Fwd`, `?Run@NwCtor_N`, `?Run@NwDtor_N`) followed to the method it calls. The two SETS of distinct strings are compared, and the
number of sites of the strings both sides have. A difference is a candidate unless a rule explains it:
  helper       the string is in the other side's closure (own body + the internal functions it calls, 2 deep; the NW_STOCK stubs of ours resolve to the stock): an inlined
               versus an out-of-line helper. Not applied when the function also has a stock-only and an ours-only string that look alike (a changed literal)
  stock-data   the stock reads the string through a table / pointer variable / buffer of its data and ours reads the stock image there (g_nwBase): the data is the business
               of the data judges (verify_data.py, tools/gen_globals.py --check)
  table-run    a run of consecutive string pointers: the stock run goes on into the neighbouring table; within the common length the two runs must be equal, in order
  spelling     the same text, ANSI in one side (FString(const char*)) and wide in the other
  codegen      a string the stock uses at more sites than ours (MSVC 14 shares loaded pointers and equal tails)
  noise        hook-only trace strings of ours
What is left is read against the stock asm and the source by hand; the verdict is in REVIEWED below (REAL = a bug of the port, with the stock address, the source line and
the fix; benign; unclear). A candidate nobody reviewed is NEW.

Pass 2 (virtual slots of the interface objects). A forward dataflow over the basic blocks follows what a register / an ebp stack slot holds: `this`, an argument, the value of an
imported data symbol (`mov r,[IAT]`: GConfig, GMalloc, GFileManager ...), a stock global (in ours `g_nwBase + rva` is the same address), the value stored at a symbolic address;
`call [vt+N]` / `mov r,[vt+N]; call r` with vt the value stored at the object's address is a virtual call of (object, N), slot 0 included. Per object, the slot sets of the two
sides are compared. An object reached from an imported symbol is strict (a slot only one side calls on it, the callees of the other side as the fallback, is reported); any other
object (named by its chain of members from the root, `this` / a global / an argument alike) is reported only as SUBST, when BOTH sides call it and each has a slot the other
lacks (the GetBool / GetInt model). A call that only one side makes (a getter inlined or devirtualized right after a `new`, an esp-relative spill) is counted as informational.

usage: python tools/port/audit_strings.py [--all] [--func <stock va hex>] [--no-slots] [--selftest] [--log FILE] [--dll FILE] [--map FILE]
       --all       also print the benign lines, the rule-explained differences with their evidence and the informational slot lines
       --func      audit only this stock function and print everything about it (both string sets and closures, the virtual calls found)
       --selftest  run the two known bugs on hand-made code (must be flagged) and exit
exit status 1 when a REAL / unclear / NEW candidate is open (the REAL ones disappear when the source is fixed and the hook dll rebuilt)."""
from __future__ import annotations

import argparse
import bisect
import csv
import difflib
import os
import re
import shutil
import struct
import sys
import tempfile
import textwrap
import time
from collections import Counter, defaultdict, namedtuple
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))

import pefile  # noqa: E402
from capstone import CS_ARCH_X86, CS_MODE_32, Cs  # noqa: E402
from capstone.x86 import X86_OP_IMM, X86_OP_MEM, X86_OP_REG  # noqa: E402

import verify_hooks as vh  # noqa: E402  (decode_skipping_tables, Code)
from nwd.image import Image  # noqa: E402

LOG_RE = re.compile(r"^\S+\s+([0-9a-f]{8}) (.+?)\s+ours=([0-9a-f]+) (.*)$")
MAP_RE = re.compile(r"^\s*([0-9a-f]{4}):([0-9a-f]{8})\s+(\S+)\s+([0-9a-f]{8})\s+(f?)\s*(i?)\s+(\S.*)$")
STUB_BASE = 0x10000000
HOOK_PREFIX = "sa-emu_strings_"
WS_CHARS = "\t\r\n"


def WIDE_EXTRA(c):
    """characters other than ASCII/Latin that a UI string may hold: Hangul (syllables, jamo, compatibility jamo), CJK, CJK punctuation, full-width forms"""
    return (0x1100 <= c <= 0x11FF or 0x3000 <= c <= 0x30FF or 0x3130 <= c <= 0x318F or 0x4E00 <= c <= 0x9FFF or 0xAC00 <= c <= 0xD7A3 or 0xFF00 <= c <= 0xFFEF
            or 0x2010 <= c <= 0x206F)


FILE_RE = re.compile(r"\.(cpp|c|h|inl|hpp)$", re.I)
I = namedtuple("I", "addr size mn ops text wregs")          # ops: ('i', imm) | ('r', reg) | ('m', base, index, scale, disp, size)


# ------------------------------------------------------------------------------------------------------------------------------------
#   a module (the stock image or our dll): bytes, sections, function extents, decoded instructions, strings
# ------------------------------------------------------------------------------------------------------------------------------------
class Mod:
    def __init__(self, tag, pe, imports, extent, labels=None):
        self.tag = tag
        self.base = pe.OPTIONAL_HEADER.ImageBase
        self.mem = pe.get_memory_mapped_image()
        self.imports = imports                      # IAT slot va -> name
        self.extent = extent                        # fn start -> [(a, b)]
        self.labels = labels or {}                  # va -> symbol (ours)
        self.data = []                              # every data section (strings referenced by `push offset`)
        self.rodata = []                            # the read-only ones (strings referenced through a memory operand)
        self.text = (0, 0)
        for s in pe.sections:
            nm = s.Name.rstrip(b"\0").decode()
            lo = self.base + s.VirtualAddress
            hi = lo + max(s.Misc_VirtualSize, s.SizeOfRawData)
            if nm.startswith(".text"):
                self.text = (lo, hi)
            elif not nm.startswith((".reloc", ".rsrc", ".edata", ".idata")):
                self.data.append((lo, hi))
                if not (s.Characteristics & 0x80000000):        # IMAGE_SCN_MEM_WRITE
                    self.rodata.append((lo, hi))
        self.iat = (min(imports), max(imports) + 4) if imports else (0, 0)      # the import address table holds pointers/hints, not strings
        self.md = Cs(CS_ARCH_X86, CS_MODE_32)
        self.md.detail = True
        self.code = vh.Code(self.read, imports, extent, self.is_text)
        self._str = {}
        self._fn = {}
        self.g_nwbase = None                        # ours: the address of the g_nwBase variable

    def read(self, va, n):
        o = va - self.base
        return bytes(self.mem[o:o + n])

    def is_text(self, va):
        return self.text[0] <= va < self.text[1]

    def in_data(self, va):
        for lo, hi in self.data:
            if lo <= va < hi:
                return True
        return False

    def in_rodata(self, va):
        for lo, hi in self.rodata:
            if lo <= va < hi:
                return True
        return False

    # ---- strings ---------------------------------------------------------------------------------------------------------------------
    def string_at(self, va):
        """('w', text) | ('a', text) | None for the NUL-terminated string at va."""
        r = self._str.get(va, 0)
        if r != 0:
            return r
        r = None
        if self.in_data(va) and not (self.iat[0] <= va < self.iat[1]):
            o = va - self.base
            m = self.mem
            # ANSI first: printable ASCII, >= 2 characters (a wide ASCII string has a NUL at its second byte, so it never passes); then UTF-16LE
            e = bytes(m[o:o + 1025]).find(bytes(1))
            if 2 <= e <= 1024:
                raw = bytes(m[o:o + e])
                if all(0x20 <= c <= 0x7E or c in (9, 10, 13) for c in raw):
                    r = ("a", raw.decode("latin-1"))
            if r is None and va % 2 == 0:
                out = []
                for i in range(1024):
                    c = m[o + 2 * i] | (m[o + 2 * i + 1] << 8)
                    if c == 0:
                        break
                    if not (0x20 <= c <= 0x7E or c in (9, 10, 13) or 0xA0 <= c <= 0x24F or WIDE_EXTRA(c)):
                        out = None
                        break
                    out.append(chr(c))
                else:
                    out = None
                if out:
                    r = ("w", "".join(out))
        self._str[va] = r
        return r

    def strings_at(self, va, direct=True, table=True):
        """the strings a pointer operand stands for: the string itself (`direct`), or the entries of a table of string pointers
        (consecutive dwords that each point at a string; NULL entries are skipped; >= 1 entry)."""
        if direct:
            s = self.string_at(va)
            if s:
                return [(s, va)]
        if not table or not self.in_data(va) or va % 4:
            return []
        out = []
        o = va - self.base
        for i in range(256):
            if o + 4 * i + 4 > len(self.mem):
                break
            p = struct.unpack_from("<I", self.mem, o + 4 * i)[0]
            if p == 0:
                if i == 0:
                    return []
                continue
            t = self.string_at(p)
            if not t:
                break
            out.append((t, p))
        return out

    # ---- decoded functions ------------------------------------------------------------------------------------------------------------
    def instrs(self, fs):
        r = self._fn.get(fs)
        if r is not None:
            return r
        ins_l = []
        for a, b in self.extent(fs):
            for ins in vh.decode_skipping_tables(self.md, self.code, a, b):
                ops = []
                for o in ins.operands:
                    if o.type == X86_OP_IMM:
                        ops.append(("i", o.imm & 0xFFFFFFFF))
                    elif o.type == X86_OP_REG:
                        ops.append(("r", ins.reg_name(o.reg)))
                    elif o.type == X86_OP_MEM:
                        ops.append(("m", ins.reg_name(o.mem.base) if o.mem.base else None, ins.reg_name(o.mem.index) if o.mem.index else None,
                                    o.mem.scale, o.mem.disp, o.size))
                    else:
                        ops.append(("x",))
                ins_l.append(I(ins.address, ins.size, ins.mnemonic, tuple(ops), ins.op_str, tuple(ins.reg_name(r) for r in ins.regs_access()[1])))
        self._fn[fs] = ins_l
        return ins_l

    def vcalls(self, fs):
        """pass 2: [(object key, vtable byte offset, call address)] of the virtual calls of one function body (a linear register tracker, see SlotTracker)"""
        ckey = ("vcalls", fs)
        r = self._fn.get(ckey)
        if r is None:
            r = SlotTracker(self).run(fs)
            self._fn[ckey] = r
        return r

    def stub_target(self, va):
        """ours: the NW_STOCK stub `mov eax,[g_nwBase]; add eax,imm; jmp eax` -> stock va it stands for"""
        b = self.read(va, 12)
        if len(b) == 12 and b[0] == 0xA1 and b[5] == 0x05 and tuple(b[10:12]) == (0xFF, 0xE0):
            return STUB_BASE + int.from_bytes(b[6:10], "little")
        if len(b) == 12 and b[0] == 0xA1 and tuple(b[5:7]) == (0x83, 0xC0) and tuple(b[8:10]) == (0xFF, 0xE0):
            return STUB_BASE + int.from_bytes(b[7:8], "little", signed=True)
        return None

    def scan(self, fs):
        """-> dict: strs {(kind,text): [site va]}, calls [(mn, target)] (internal functions outside the extent; ('stock', va) for an NW_STOCK stub), n (instructions)"""
        ckey = ("scan", fs)
        r = self._fn.get(ckey)
        if r is not None:
            return r
        ext = self.extent(fs)
        nwb = 0                                     # ours: reads of the g_nwBase variable (the hook build reaching the stock image's data)
        refs = []                                   # (string address, key, instruction address, operand type, via)
        opv = defaultdict(set)                      # instruction address -> the data addresses its operands name (the string, the table, the variable)
        cmps = set()                                # strings compared by address (`cmp reg, offset s`)
        tbls = []                                   # the runs of a table of string pointers, in table order
        calls = []
        ins_l = self.instrs(fs)
        for ins in ins_l:
            is_flow = ins.mn in ("call", "jmp") or ins.mn.startswith("j")
            if self.g_nwbase is not None and any(o[0] == "m" and (o[4] & 0xFFFFFFFF) == self.g_nwbase for o in ins.ops):
                nwb += 1
            for op in ins.ops:
                # `push offset s` / `mov r, offset s` may name writable data; a memory operand (the /GS cookie, a global) only counts as a string in
                # read-only data, but may be a table of string pointers (`mov eax, [reg*4 + table]`) anywhere in the data, or a global variable that
                # holds the pointer of a string (`mov edx, [g_pszPrefix]`)
                if op[0] == "i" and not is_flow and self.in_data(op[1]):
                    v, direct, table = op[1], True, self.in_rodata(op[1])
                    if ins.mn == "cmp":
                        # a pointer compare: against a literal (`Name == HtmlTransparent`) or against the end of a table (the next thing in the pool
                        # is a string by chance): never a use of the string, only evidence for the other side
                        s_ = self.string_at(v)
                        if s_:
                            cmps.add(s_)
                        continue
                    ptrvar = False
                elif op[0] == "m" and self.in_data(op[4] & 0xFFFFFFFF):
                    v = op[4] & 0xFFFFFFFF
                    direct, table = self.in_rodata(v), op[2] is not None and op[3] == 4       # [reg*4 + table]
                    ptrvar = op[5] == 4 and not self.in_rodata(v) and not (self.iat[0] <= v < self.iat[1])
                else:
                    continue
                run = self.strings_at(v, direct, table)
                is_direct = bool(direct and self.string_at(v))
                if run:
                    opv[ins.addr].add(v)
                if is_direct:
                    k0, t0 = run[0][0]
                    # an integer table read through a memory operand often starts with a small value that decodes as a one- or two-character string;
                    # a single character must not be a Latin-1 / Latin-A letter (a Hangul or CJK one is a real string)
                    if (op[0] == "m" and len(t0) < 3) or (len(t0) == 1 and 0x7E < ord(t0) <= 0x24F):
                        continue
                for (k, t), p in run:
                    refs.append((p, (k, t), ins.addr, op[0], ("w" if not self.in_rodata(p) else "d") if is_direct else "t"))
                if run and not is_direct:
                    tbls.append([kt for kt, _ in run])
                if ptrvar and not self.string_at(v) and v % 4 == 0 and v - self.base + 4 <= len(self.mem):
                    pv = struct.unpack_from("<I", self.mem, v - self.base)[0]
                    if self.in_data(pv):
                        sv = self.string_at(pv)
                        if sv:
                            refs.append((pv, sv, ins.addr, "p", "v"))
                            opv[ins.addr].add(v)
            if ins.mn in ("call", "jmp") and ins.ops and ins.ops[0][0] == "i":
                t = ins.ops[0][1]
                if self.is_text(t) and not any(a <= t < b for a, b in ext):
                    if self.tag == "stock":
                        stub = self.read(t, 6)
                        if stub[:2] == bytes((0xFF, 0x25)) and int.from_bytes(stub[2:6], "little") in self.imports:
                            continue                              # a jmp [IAT] thunk is the import itself
                    else:
                        st = self.stub_target(t)
                        if st is not None:
                            calls.append((ins.mn, ("stock", st)))
                            continue
                    calls.append((ins.mn, t))
        # a load through a memory operand from the inside of a string that the function references from its start is an inline copy of the
        # literal (`wcscpy(buf, L"0xff")` as `mov ecx,[s]; mov ecx,[s+4]; mov cx,[s+8]`): the tail pieces are not strings of their own
        spans = [(p, p + (2 if k == "w" else 1) * (len(t) + 1)) for p, (k, t), _, _, _ in refs]
        strs = defaultdict(list)
        via = defaultdict(set)
        addrs = defaultdict(set)
        for p, key, ad, ot, vi in refs:
            if ot == "m" and any(lo < p < hi for lo, hi in spans):
                continue
            strs[key].append(ad)
            via[key].add(vi)
            addrs[key] |= opv[ad]
        tbl = {k for k, v in via.items() if v <= {"t", "v", "w"}}       # reached only through a table of pointers / a global pointer variable / a buffer of the writable data: data
        # the arguments of appFailAssert(expr, file, line) / the check macros: an ANSI __FILE__ string and what is pushed next to it (the expression
        # text, ANSI in the stock and wide in ours) are kept apart
        files = [ad for (k, t), sites in strs.items() if k == "a" and FILE_RE.search(t) for ad in sites]
        asserts = {}
        for sk in list(strs):
            if (sk[0] == "a" and FILE_RE.search(sk[1])) or any(abs(ad - f) <= 24 for ad in strs[sk] for f in files):
                asserts[sk] = strs.pop(sk)
        r = {"strs": dict(strs), "asserts": asserts, "calls": calls, "n": len(ins_l), "tbl": tbl, "cmps": cmps, "tbls": tbls, "nwb": nwb, "addrs": dict(addrs)}
        self._fn[ckey] = r
        return r


# ------------------------------------------------------------------------------------------------------------------------------------
#   pass 2: the virtual calls through the engine interface objects
# ------------------------------------------------------------------------------------------------------------------------------------
SUB = {"al": "eax", "ah": "eax", "ax": "eax", "bl": "ebx", "bh": "ebx", "bx": "ebx", "cl": "ecx", "ch": "ecx", "cx": "ecx", "dl": "edx", "dh": "edx",
       "dx": "edx", "si": "esi", "di": "edi", "sp": "esp", "bp": "ebp"}
GPR = ("eax", "ebx", "ecx", "edx", "esi", "edi")
ABS = ("abs",)


def preg(r):
    return SUB.get(r, r)


def vfmt(v):
    """printable key of a symbolic value (root, offset)"""
    if v is None:
        return "?"
    root, off = v
    return rfmt(root) + ("%+#x" % off if off else "")


def rfmt(root):
    k = root[0]
    if k == "this":
        return "this"
    if k == "arg":
        return "arg%d" % root[1]
    if k == "imp":
        m = re.match(r"^\??([^@]+)@", root[1])
        return "@" + (m.group(1) if m else root[1])
    if k == "abs":
        return "G"
    if k == "mem":
        return "[" + rfmt(root[1]) + ("%+#x" % root[2] if root[2] else "") + "]"
    return k


class SlotTracker:
    """A forward dataflow over the basic blocks of one function that follows what a register (and an ebp-relative stack slot) holds, as a symbolic value
    (root, offset): `this` (ecx at entry), an argument, an imported data symbol (`mov r,[IAT]`: the address of GConfig ...), a constant / absolute address
    (the stock's globals; in ours `g_nwBase + rva` is the same stock address), and the value stored at a symbolic address (`mov r,[r2+d]`). At a join the
    states are merged (a value the paths do not agree on is forgotten). `call [vt+N]` / `mov r,[vt+N]; call r` is a virtual call when vt is the value stored
    at the object's address: it is recorded as (key of the object, N, address)."""

    MAXDEPTH = 7

    def __init__(self, mod):
        self.mod = mod
        self.gpr = GPR
        self.frame = True

    @staticmethod
    def sdisp(d):
        d &= 0xFFFFFFFF
        return d - 0x100000000 if d >= 0x80000000 else d

    @classmethod
    def depth(cls, v):
        d = 0
        r = v[0]
        while r[0] == "mem":
            d += 1
            r = r[1]
        return d

    def addr_of(self, regs, op):
        """the symbolic address (root, offset) of a memory operand, or None"""
        _, base, index, scale, disp, _sz = op
        if index is not None or base == "esp" or (base == "ebp" and self.frame):
            return None
        if base is None:
            return (ABS, disp & 0xFFFFFFFF)
        bv = regs.get(preg(base))
        if bv is None:
            return None
        return (bv[0], bv[1] + self.sdisp(disp))

    def load(self, regs, slots, op):
        mod = self.mod
        _, base, index, scale, disp, _sz = op
        if base == "ebp" and index is None and self.frame:
            d = self.sdisp(disp)
            if d >= 8:
                return (("arg", d), 0)
            return slots.get(d) if d < 0 else None
        if base is None and index is None:
            a = disp & 0xFFFFFFFF
            if a in mod.imports:
                return (("imp", mod.imports[a]), 0)
            if mod.g_nwbase is not None and a == mod.g_nwbase:
                return (ABS, STUB_BASE)
            return (("mem", ABS, a), 0)
        ad = self.addr_of(regs, op)
        if ad is None:
            return None
        v = (("mem", ad[0], ad[1]), 0)
        return v if self.depth(v) <= self.MAXDEPTH else None

    @staticmethod
    def vcall_of(callee):
        """callee value -> (object key, N) when it is [[obj] + N]"""
        if callee is None:
            return None
        root, off = callee
        if root[0] == "mem" and root[1][0] == "mem":
            vt = root[1]                        # the vtable pointer, stored at (vt[1] + vt[2]) = the object's address
            return (vfmt((vt[1], vt[2])), root[2] + off)
        return None

    def step(self, ins, regs, slots, out):
        """the effect of one instruction on (regs, slots); a virtual call is appended to `out` when it is not None"""
        mn, ops = ins.mn, ins.ops
        if mn == "call":
            callee = None
            if ops and ops[0][0] == "m":
                callee = self.load(regs, slots, ops[0])
            elif ops and ops[0][0] == "r":
                callee = regs.get(preg(ops[0][1]))
            vc = self.vcall_of(callee)
            if vc is not None and out is not None:
                out.append((vc[0], vc[1], ins.addr))
            for r in ("eax", "ecx", "edx"):
                regs[r] = None
            return
        if mn == "mov" and len(ops) == 2 and ops[0][0] == "r" and ops[0][1] in self.gpr:
            d = ops[0][1]
            if ops[1][0] == "r":
                regs[d] = regs.get(preg(ops[1][1]))
            elif ops[1][0] == "i":
                regs[d] = (ABS, ops[1][1])
            elif ops[1][0] == "m":
                regs[d] = self.load(regs, slots, ops[1])
            else:
                regs[d] = None
            return
        if mn == "mov" and len(ops) == 2 and ops[0][0] == "m":
            _, base, index, scale, disp, sz = ops[0]
            if base == "ebp" and index is None and self.frame:
                d = self.sdisp(disp)
                if ops[1][0] == "r" and sz == 4:
                    slots[d] = regs.get(preg(ops[1][1]))
                elif ops[1][0] == "i" and sz == 4:
                    slots[d] = (ABS, ops[1][1])
                else:
                    slots[d] = None
            return
        if mn == "lea" and len(ops) == 2 and ops[0][0] == "r" and ops[0][1] in self.gpr and ops[1][0] == "m":
            regs[ops[0][1]] = self.addr_of(regs, ops[1])
            return
        if mn in ("add", "sub") and len(ops) == 2 and ops[0][0] == "r" and ops[0][1] in self.gpr and ops[1][0] == "i":
            v = regs.get(ops[0][1])
            imm = self.sdisp(ops[1][1])
            regs[ops[0][1]] = (v[0], v[1] + (imm if mn == "add" else -imm)) if v is not None else None
            return
        # anything else: whatever it writes is forgotten
        if mn not in ("cmp", "test", "push", "jmp", "ret", "retn", "nop") and not mn.startswith("j"):
            for w in ins.wregs:
                w = preg(w)
                if w in regs:
                    regs[w] = None
            if self.frame and ops and ops[0][0] == "m" and ops[0][1] == "ebp" and ops[0][2] is None:
                slots[self.sdisp(ops[0][4])] = None

    @staticmethod
    def meet(a, b):
        """the merge of two states (regs, slots): what both agree on"""
        if a is None:
            return b
        if b is None:
            return a
        regs = {k: (a[0][k] if a[0].get(k) == b[0].get(k) else None) for k in a[0]}
        slots = {}
        for k in set(a[1]) | set(b[1]):
            va, vb = a[1].get(k), b[1].get(k)
            slots[k] = va if va == vb else None
        return regs, slots

    def run(self, fs):
        ins_l = self.mod.instrs(fs)
        if not ins_l:
            return []
        # ebp is the frame pointer when the function sets it up (`mov ebp, esp` in the first instructions), a general register otherwise
        self.frame = any(i.mn == "mov" and i.ops[:2] == (("r", "ebp"), ("r", "esp")) for i in ins_l[:8])
        self.gpr = GPR if self.frame else GPR + ("ebp",)
        addrs = [i.addr for i in ins_l]
        index = {a: n for n, a in enumerate(addrs)}
        leaders = {0}
        for n, ins in enumerate(ins_l):
            if ins.mn.startswith("j") and ins.ops and ins.ops[0][0] == "i":
                t = ins.ops[0][1]
                if t in index:
                    leaders.add(index[t])
            if (ins.mn.startswith("j") or ins.mn in ("ret", "retn")) and n + 1 < len(ins_l):
                leaders.add(n + 1)
        starts = sorted(leaders)
        blocks = []                                  # (first, last+1)
        for k, s0 in enumerate(starts):
            blocks.append((s0, starts[k + 1] if k + 1 < len(starts) else len(ins_l)))
        bidx = {s0: k for k, (s0, _) in enumerate(blocks)}
        succ = []
        for k, (s0, e0) in enumerate(blocks):
            last = ins_l[e0 - 1]
            ss = []
            if last.mn.startswith("j") and last.ops and last.ops[0][0] == "i" and last.ops[0][1] in index:
                ss.append(bidx[index[last.ops[0][1]]])
            if last.mn not in ("jmp", "ret", "retn") and e0 < len(ins_l):
                ss.append(bidx[e0])
            succ.append(ss)
        # a block nothing jumps to (the cases of a switch: `jmp [reg*4 + table]`) is entered from the nearest indirect jump above it
        has_pred = {t for ss in succ for t in ss}
        ind = [k for k, (s0, e0) in enumerate(blocks) if ins_l[e0 - 1].mn == "jmp" and ins_l[e0 - 1].ops and ins_l[e0 - 1].ops[0][0] == "m"]
        for k in range(1, len(blocks)):
            if k not in has_pred:
                above = [j for j in ind if j < k]
                if above:
                    succ[above[-1]].append(k)
        entry_regs = {r: None for r in self.gpr}
        entry_regs["ecx"] = (("this",), 0)
        states = [None] * len(blocks)
        states[0] = (entry_regs, {})
        work = [0]
        guard_n = 0
        while work and guard_n < 20000:
            guard_n += 1
            k = work.pop()
            regs, slots = dict(states[k][0]), dict(states[k][1])
            for n in range(*blocks[k]):
                self.step(ins_l[n], regs, slots, None)
            for t in succ[k]:
                new = self.meet(states[t], (regs, slots)) if states[t] is not None else (dict(regs), dict(slots))
                if states[t] is None or new != states[t]:
                    states[t] = new
                    work.append(t)
        out = []
        for k, (s0, e0) in enumerate(blocks):
            if states[k] is None:
                regs, slots = {r: None for r in self.gpr}, {}
            else:
                regs, slots = dict(states[k][0]), dict(states[k][1])
            for n in range(s0, e0):
                self.step(ins_l[n], regs, slots, out)
        return out


# ------------------------------------------------------------------------------------------------------------------------------------
#   building the two modules
# ------------------------------------------------------------------------------------------------------------------------------------
def build_stock():
    stock = Image()
    granges = {}
    for r in csv.DictReader((ROOT / "analysis/ghidra/functions.tsv").open(encoding="utf-8"), delimiter="\t"):
        granges[int(r["va"], 16)] = [tuple(int(x, 16) for x in rg.split("-")) for rg in r["ranges"].split(",")]
    for r in csv.DictReader(l for l in (ROOT / "db/not_functions.csv").open(encoding="utf-8") if not l.startswith("#")):
        owner, frag = int(r["part_of"], 16), int(r["va"], 16)
        if owner in granges and frag in granges:
            granges[owner] = granges[owner] + granges[frag]
    for r in csv.DictReader(l for l in (ROOT / "db/extra_functions.csv").open(encoding="utf-8") if not l.startswith("#")):
        va = int(r["va"], 16)
        granges[va] = [(va, va + int(r["size"]) - 1)]
    names = {}
    for r in csv.DictReader((ROOT / "db/functions.csv").open(encoding="utf-8")):
        names[int(r["va"], 16)] = r["name"]
    m = Mod("stock", stock.pe, {va: n for va, (_, n) in stock.imports.items()},
            lambda va: [(a, b + 1) for a, b in granges.get(va, [(va, va + 0x3F)])])
    m.granges = granges
    m.names = names
    return m


def parse_map(path):
    """-> (code symbols {va: name}, data symbols {name: va})"""
    syms, data = {}, {}
    for ln in open(path, encoding="latin-1"):
        m = MAP_RE.match(ln)
        if m:
            if m.group(1) == "0001":
                syms.setdefault(int(m.group(4), 16), m.group(3))
            else:
                data.setdefault(m.group(3), int(m.group(4), 16))
    return syms, data


def build_ours(dll, mapf, tmpdir):
    d2 = Path(tmpdir) / (HOOK_PREFIX + "ours.dll")
    m2 = Path(tmpdir) / (HOOK_PREFIX + "ours.map")
    shutil.copy2(dll, d2)
    shutil.copy2(mapf, m2)
    pe = pefile.PE(str(d2))
    syms, data_syms = parse_map(m2)
    starts = sorted(syms)
    base = pe.OPTIONAL_HEADER.ImageBase
    t = next(s for s in pe.sections if s.Name.startswith(b".text"))
    t_hi = base + t.VirtualAddress + t.Misc_VirtualSize
    o_imports = {i.address: (i.name or b"").decode() for e in pe.DIRECTORY_ENTRY_IMPORT for i in e.imports}

    def extent(va):
        i = bisect.bisect_right(starts, va)
        end = starts[i] if i < len(starts) else t_hi
        return [(va, end)]

    m = Mod("ours", pe, o_imports, extent, syms)
    m.g_nwbase = data_syms.get("_g_nwBase")
    return m


# ------------------------------------------------------------------------------------------------------------------------------------
#   one hooked function on one side: its body (wrapper followed), its strings, its closure
# ------------------------------------------------------------------------------------------------------------------------------------
WRAP_NAME = re.compile(r"\$Fwd@|\$FwdC@|\?Run@NwCtor_|\?Run@NwDtor_")


class Body:
    """the functions that make up a hooked function: the entry, and the target of a wrapper"""

    def __init__(self, mod: Mod, fs: int, hookname: str = ""):
        self.mod = mod
        self.fs = fs
        self.hookname = hookname.split("(")[0].strip()
        self.parts = [fs]
        self.stub = None                    # ours: the whole thing is an NW_STOCK stub
        self.stock_calls = []               # va of stock functions reached through NW_STOCK stubs (ours)
        self._resolve(fs, 0)

    def _resolve(self, fs, depth):
        mod = self.mod
        if mod.tag == "ours" and depth == 0:
            st = mod.stub_target(fs)
            if st is not None:
                self.stub = st
                return
        sc = mod.scan(fs)
        tg = {t for _, t in sc["calls"]}
        if depth >= 3 or not tg or sc["strs"] or sc["asserts"]:
            return
        # a hook-only wrapper of ours (`??$Fwd`, `?Run@NwCtor_N`, `?Run@NwDtor_N`) stands for the method it calls: the target whose symbol is the hooked method
        # (`?Method@Class@@...`, `??0Class@@...` for a constructor, `??1Class@@...` for a destructor) or the only target of a short wrapper (the by-value
        # parameters are copied in the wrapper); a bare `jmp` thunk (either side) stands for its target; anything else is a function with callees,
        # whose strings are its own business
        ins_l = mod.instrs(fs)
        follow = []
        if mod.tag == "ours" and WRAP_NAME.search(mod.labels.get(fs, "")):
            follow = [t for t in tg if not isinstance(t, tuple) and self._is_method(mod.labels.get(t, ""))]
            if not follow and len(tg) == 1 and sc["n"] <= 30:
                follow = list(tg)
        elif sc["n"] <= 6 and ins_l and ins_l[-1].mn == "jmp" and len(tg) == 1:
            follow = list(tg)
        for t in sorted(follow, key=lambda x: (isinstance(x, tuple), x)):
            if isinstance(t, tuple):
                self.stock_calls.append(t[1])
                if depth == 0 and len(follow) == 1:
                    self.stub = t[1]
                continue
            if t not in self.parts:
                self.parts.append(t)
                self._resolve(t, depth + 1)

    def _is_method(self, label):
        """does the decorated symbol `label` name the hooked method (hook name `Class::Method`)?"""
        if not self.hookname or not label:
            return False
        parts = self.hookname.split("::")
        if len(parts) < 2:
            return label.startswith("?" + parts[0] + "@@")
        cls, meth = parts[-2], parts[-1]
        if meth == cls:
            return label.startswith("??0" + cls + "@@")
        if meth == "~" + cls:
            return label.startswith(("??1" + cls + "@@", "??_G" + cls + "@@", "??_E" + cls + "@@"))
        return label.startswith("?" + meth + "@" + cls + "@@")

    def own(self):
        """({(kind,text): [sites]}, data-derived keys, compared-by-address keys, table runs) of every part (assert strings apart, see Mod.scan)"""
        out = defaultdict(list)
        tbl, cmps, tbls = set(), set(), []
        for p in self.parts:
            sc = self.mod.scan(p)
            for k, v in sc["strs"].items():
                out[k].extend(v)
            tbl |= sc["tbl"]
            cmps |= sc["cmps"]
            tbls.extend(sc["tbls"])
        tbl = {k for k in tbl if k in out}
        return out, tbl, cmps, tbls


def closure(mod: Mod, parts, stock: Mod, depth=2):
    """{(kind,text): (module tag, function va)} of the parts and everything they call, `depth` levels deep (breadth first); the NW_STOCK stubs of ours
    resolve into the stock. The strings that are only compared by address count too (evidence, not a use)."""
    seen = {}
    todo = [(p, mod, 0) for p in parts]
    done = set()
    while todo:
        nxt = []
        for fs, m, d in todo:
            if (m.tag, fs) in done:
                continue
            done.add((m.tag, fs))
            sc = m.scan(fs)
            for k in sc["strs"]:
                seen.setdefault(k, (m.tag, fs))
            for k in sc["cmps"]:
                seen.setdefault(k, (m.tag, fs))
            if d < depth:
                for _, t in sc["calls"]:
                    if isinstance(t, tuple):
                        nxt.append((t[1], stock, d))      # a stub is the stock function itself: no depth consumed
                    else:
                        nxt.append((t, m, d + 1))
        todo = nxt
    return seen


# ------------------------------------------------------------------------------------------------------------------------------------
#   rule filters
# ------------------------------------------------------------------------------------------------------------------------------------
def noise_reason(side, key):
    """rule-based filters: -> reason or None"""
    kind, text = key
    if side == "ours" and kind == "a" and (text.startswith("[trace]") or re.match(r"^~?\w+::~?\w+$", text)):
        return "hook-only trace string"
    return None


# ------------------------------------------------------------------------------------------------------------------------------------
#   the reviewed list: every candidate that survives the rules was read against the stock asm and the source
# ------------------------------------------------------------------------------------------------------------------------------------
# REVIEWED[(stock va, side, kind, text)] = (class, reason)   side: S stock-only string, O ours-only string, C a string that ours uses at MORE sites than the stock
#   class: REAL (a bug of the port, listed until the source is fixed), benign, unclear.  kind: w wide, a ANSI.
# SLOT_REVIEWED[(stock va, object key)] = (class, reason)
REVIEWED = {}
SLOT_REVIEWED = {}


def _r(va, cls, why, *keys):
    for side, kind, text in keys:
        REVIEWED[(va, side, kind, text)] = (cls, why)


_r(0x1000f9c0, "benign",
   "documented deviation (src/nwindow/NCEditBox.cpp:464): the stock formats the typed code with swprintf(Temp, L\"%c\", wParam); the UCRT's wide %c keeps "
   "only the low byte, so ours stores Temp[0] = wParam, Temp[1] = 0 (the same string)", ("S", "w", "%c"))

_r(0x10026830, "REAL",
   "NCMultiEdit::OnCreate measures the sample text of the box with NCGDevice::GetTextExtent. The stock (0x100268de: push 0x101A60D4; call 0x10012960) passes "
   "L\"\\uAC00\" (the Hangul syllable GA, bytes 00 AC 00 00); ours passes L\"\\u00B0\\u00A1\" (the two bytes B0 A1 of its CP949 encoding read as two Latin-1 "
   "characters), so CharW (NCMultiEdit +0x2AC, `width of a Hangul character` in include/nwindow/NCMultiEdit.h:126) is the width of \"\\u00B0\\u00A1\", not of a "
   "Hangul syllable. src/nwindow/NCMultiEdit.cpp:1109 (the C3 comment above it describes the CP949 bytes B0 A1, which the C4 image no longer uses). "
   "FIX: TEXT(\"\\xAC00\")", ("S", "w", "\uac00"), ("O", "w", "\xb0\xa1"))

_r(0x100c27f0, "REAL",
   "NCNpcView_CharacterSelectWnd::OnCreate: the stock calls NCButton::SetText(L\"\\uC18C \\uD658\") (0x100c2cf0 on the button at +0x134, 0x100c349e on +0x138; "
   "the string with a space at 0x101BF694); ours has no space. src/nwindow/NCNpcViewrWnd.cpp:597 and :655 (NCPawnViewrWnd.cpp:829 is another window and "
   "matches its stock). FIX: TEXT(\"\\xC18C \\xD658\") at both lines", ("S", "w", "\uc18c \ud658"), ("O", "w", "\uc18c\ud658"))

_r(0x100f7c70, "REAL",
   "NCReplayListWnd::Refresh: the stock cuts the name of each file at the FIRST dot (0x100f7cf2 and 0x100f7d05: appStrstr(Name, L\".\") twice, then "
   "*p = 0); ours looks for the extension \".L2R\" (src/nwindow/NCReplayListWnd.cpp:186-187), so a file name with another dot (\"a.b.L2R\" -> stock \"a\", ours "
   "\"a.b\") or a lower-case extension (appStrstr is case sensitive: ours keeps \".l2r\") gives another row text. FIX: TEXT(\".\") in both appStrstr calls",
   ("S", "w", "."), ("O", "w", ".L2R"))

# text folded into immediates (inline_text): numeric constants and the copy of a literal in pieces
_r(0x1002ded0, "benign", "not text: the stock pushes the style flags 0x400032 (`bDragable ? NWS_DRAGABLE | 0x32 : 0x22`, src/nwindow/NCTabControl.cpp:309); the "
   "compiler of ours builds the same value without that immediate", ("S", "iw", "2@"))
_r(0x1005d580, "benign", "ours stores the three dots of the ellipsis as one dword immediate, the stock one character at a time", ("O", "iw", ".."))
_r(0x10168a10, "benign", "ours stores the three dots of the ellipsis as one dword immediate, the stock one character at a time", ("O", "iw", ".."))
_r(0x100d8260, "benign", "not text: 0x55555556, the multiplier of a division by 3 that the compiler of ours emits", ("O", "ia", "VUUU"))

# strings that ours uses at more sites than the stock: the MSVC 14 compiler duplicates a tail the VC 7.0 one shares
_r(0x1004c080, "benign", "documented (src/nwindow/NKeyCrypt.cpp:207-212): without npkcrypt.dll ours logs the same line as a driver that did not start; the "
   "stock links npkcrypt statically", ("C", "w", "Open KeyCrypt Driver Failed : %x"))
_r(0x100c5e70, "benign", "the two branches (true / false) of the trilinear switch each call GConfig->SetString in ours; the stock shares one call and "
   "selects the value string", ("C", "w", "UseTrilinear"), ("C", "w", "Video"))
_r(0x100c97d0, "benign", "ours duplicates the `push L\"Korean\"` of the shared tail into two branches", ("C", "w", "Korean"))
_r(0x10125be0, "benign", "ours calls Printf in both branches; the stock selects the argument first and shares the call", ("C", "w", "eventmatch fence %s %d"))
_r(0x1010e560, "benign", "ours repeats the appSprintf(L\"%d\") of a shared tail in its branches (4 sites against 2); same string, same calls", ("C", "w", "%d"))
_r(0x10165440, "benign", "'FLN' is ANSI in the stock (FString(const char*)) and wide in ours, see the spelling rule; the count is of the wide one",
   ("C", "w", "FLN"))

# the interface slots
SLOT_REVIEWED[(0x10029f30, "[@GMalloc]")] = (
    "benign", "NLineList::~NLineList: the stock ends with `if (Head) delete Head` (0x10029f45: GMalloc->Free, slot 8) but its own Clear() (0x10028000) "
    "has just set Head = 0, so the Free never runs; the compiler of ours removes the dead branch (src/nwindow/NCMultiEdit.cpp:87-88)")


# ------------------------------------------------------------------------------------------------------------------------------------
#   pass 1: one hooked function
# ------------------------------------------------------------------------------------------------------------------------------------
def load_hooks(log):
    rows = []
    for line in Path(log).read_text(encoding="latin-1").splitlines():
        m = LOG_RE.match(line)
        if m:
            rows.append((int(m.group(1), 16), m.group(2), int(m.group(3), 16), m.group(4)))
    return rows


def show(key):
    k, t = key
    return "%s%s" % ({"w": "L", "ia": "imm:", "iw": "imm:L"}.get(k, ""), ascii(t))


def where(ev, stock, ours):
    tag, fs = ev
    return ("stock " + stock.names.get(fs, "%08x" % fs)) if tag == "stock" else ("ours " + ours.labels.get(fs, "%08x" % fs))


def table_consistency(S_tbls, O_tbls):
    """-> (keys of the stock runs that agree with a run of ours, keys of ours that agree with a stock run, [(stock run, ours run, index)] that disagree).
    A run of the stock that goes on past the end of ours is the neighbouring table (the runs are consecutive pointers); within the common length the two
    must be equal, in order."""
    cs, co, bad = set(), set(), []
    for Lo in O_tbls:
        best, ov = None, 0
        for Ls in S_tbls:
            n = len(set(Ls) & set(Lo))
            if n > ov:
                best, ov = Ls, n
        if best is None:
            continue
        m = min(len(best), len(Lo))
        if best[:m] == Lo[:m]:
            cs |= set(best)
            co |= set(Lo)
        else:
            i = next(i for i in range(m) if best[i] != Lo[i])
            bad.append((best, Lo, i))
    return cs, co, bad


Diff = namedtuple("Diff", "va name side key sites why ev")      # side: S stock-only, O ours-only, C more sites in ours, T table disagreement; why: None or a rule


def inline_text(mod, parts):
    """text folded into immediates: an imm32 whose four bytes are printable ASCII (`cmp dword ptr [eax], 'v1.0'`) or two printable UTF-16 characters
    (`mov dword ptr [esi+0x24], 0x2e002e` = L".."). Keys ('ia', text) / ('iw', text). Numeric constants that happen to look like text show up too."""
    out = set()
    for p in parts:
        for ins in mod.instrs(p):
            if ins.mn in ("call", "jmp") or ins.mn.startswith("j"):
                continue
            for op in ins.ops:
                if op[0] == "i":
                    b = struct.pack("<I", op[1])
                    if all(0x20 <= c <= 0x7E for c in b) and len(set(b)) > 1:
                        out.add(("ia", b.decode()))
                    elif b[1] == 0 and b[3] == 0 and 0x20 <= b[0] <= 0x7E and 0x20 <= b[2] <= 0x7E:
                        out.add(("iw", chr(b[0]) + chr(b[2])))
    return out


def near(consts, rva, span=0x400):
    """is there a constant of ours (an immediate or a displacement, sorted) equal to `rva` or at most `span` below it: the base of the table the stock
    indexes (`Addr(0x1023DBB8)` for the element at 0x1023DBD0)?"""
    i = bisect.bisect_right(consts, rva)
    return i > 0 and rva - consts[i - 1] <= span and consts[i - 1] >= 0x1000


def audit_hook(va, name, rva, stock, ours):
    ofs = ours.base + rva
    so = Body(stock, va, name)
    oo = Body(ours, ofs, name)
    if oo.stub is not None:
        return None, so, oo, None
    S, S_tbl, S_cmp, S_tbls = so.own()
    O, O_tbl, O_cmp, O_tbls = oo.own()
    Sc = closure(stock, so.parts, stock)
    Oc = closure(ours, oo.parts, stock)
    cs, co, bad = table_consistency(S_tbls, O_tbls)
    oconst = set()
    for f in slot_closure(ours, oo.parts):
        for ins in ours.instrs(f):
            for op in ins.ops:
                if op[0] == "i":
                    oconst.add(op[1])
                elif op[0] == "m":
                    oconst.add(op[4] & 0xFFFFFFFF)
    oconst_sorted = sorted(oconst)
    s_addrs = defaultdict(set)
    for p in so.parts:
        for k, v in stock.scan(p)["addrs"].items():
            s_addrs[k] |= v
    o_nwb = sum(ours.scan(p)["nwb"] for p in oo.parts) + sum(ours.scan(t)["nwb"] for p in oo.parts for _, t in ours.scan(p)["calls"] if not isinstance(t, tuple))
    diffs = []
    for key in sorted(set(S) - set(O)):
        why, ev = None, None
        if key in Oc:
            why, ev = "helper", where(Oc[key], stock, ours)
        elif key in cs:
            why, ev = "table-run", "the stock run goes on past the end of ours (neighbouring table)"
        elif key in S_tbl and not O_tbls and o_nwb and any(near(oconst_sorted, x - STUB_BASE) for x in s_addrs[key]):
            why, ev = "stock-data", "reached through the table / pointer variable / buffer at %s of the stock data; ours has that address (or the base of the table) as a constant (g_nwBase + %s)" % (
                " ".join("%08x" % x for x in sorted(s_addrs[key])[:2]), " ".join("%x" % (x - STUB_BASE) for x in sorted(s_addrs[key])[:2]))
        elif ("w" if key[0] == "a" else "a", key[1]) in O:
            why, ev = "spelling", "the same text as an %s string of ours (FString(const char*) vs FString(const TCHAR*))" % ("wide" if key[0] == "a" else "ANSI")
        elif noise_reason("stock", key):
            why, ev = "noise", noise_reason("stock", key)
        diffs.append(Diff(va, name, "S", key, S[key], why, ev))
    for key in sorted(set(O) - set(S)):
        why, ev = None, None
        if key in Sc:
            why, ev = "helper", where(Sc[key], stock, ours)
        elif key in co:
            why, ev = "table-run", "the run of ours is a prefix of a stock run"
        elif key in O_tbl and not S_tbls:
            why, ev = "stock-data", "reached through a table of ours, the stock has none in this function"
        elif ("w" if key[0] == "a" else "a", key[1]) in S:
            why, ev = "spelling", "the same text as an %s string of the stock" % ("wide" if key[0] == "a" else "ANSI")
        elif noise_reason("ours", key):
            why, ev = "noise", noise_reason("ours", key)
        diffs.append(Diff(va, name, "O", key, O[key], why, ev))
    # a stock-only and an ours-only string that look alike (a changed literal) stay candidates even when each is found in the other side's helpers
    sd = [i for i, d in enumerate(diffs) if d.side == "S" and d.why == "helper"]
    od = [i for i, d in enumerate(diffs) if d.side == "O" and d.why == "helper"]
    for i in sd:
        for j in od:
            a_, b_ = diffs[i].key[1], diffs[j].key[1]
            if a_ in b_ or b_ in a_ or difflib.SequenceMatcher(None, a_, b_).ratio() >= 0.6:
                diffs[i] = diffs[i]._replace(why=None, ev=None)
                diffs[j] = diffs[j]._replace(why=None, ev=None)
    for key in sorted(set(S) & set(O)):
        if key in S_tbl or key in O_tbl:
            continue
        ns, no = len(S[key]), len(O[key])
        if ns != no:
            if ns > no:
                diffs.append(Diff(va, name, "C", key, (ns, no), "codegen", "the stock uses it at %d sites, ours at %d: MSVC 14 shares the loaded pointer / merges equal tails" % (ns, no)))
            else:
                diffs.append(Diff(va, name, "C", key, (ns, no), None, None))
    sI, oI = inline_text(stock, so.parts), inline_text(ours, oo.parts)
    sIc = set().union(*[inline_text(stock, [f]) for f in slot_closure(stock, so.parts)]) if sI != oI else set()
    oIc = set().union(*[inline_text(ours, [f]) for f in slot_closure(ours, oo.parts)]) if sI != oI else set()
    for key in sorted(sI - oI):
        covered = key in oIc
        diffs.append(Diff(va, name, "S", key, [], "helper" if covered else None, "folded into an immediate in a callee of ours" if covered else None))
    for key in sorted(oI - sI):
        covered = key in sIc
        diffs.append(Diff(va, name, "O", key, [], "helper" if covered else None, "folded into an immediate in a callee of the stock" if covered else None))
    for Ls, Lo, i in bad:
        diffs.append(Diff(va, name, "T", ("t", "table entry %d: stock %s ours %s" % (i, show(Ls[i]), show(Lo[i]))), [], None, None))
    info = dict(S=S, O=O, Sc=Sc, Oc=Oc, S_tbls=S_tbls, O_tbls=O_tbls)
    return diffs, so, oo, info


# ------------------------------------------------------------------------------------------------------------------------------------
#   pass 2: the virtual slots of one hooked function
# ------------------------------------------------------------------------------------------------------------------------------------
LOOSE_RE = re.compile(r"\[G\+0x[0-9a-f]+\]|this|arg\d+")


def loose_key(key):
    """the object key without the kind of its root: the stock reaches the console as `[G+0x102c39f4]`, a method of the console as `this`, a callee as `arg8`;
    the chain of members after the root is what says which object it is"""
    return LOOSE_RE.sub("R", key)


SlotDiff = namedtuple("SlotDiff", "va name cat key stock ours")     # cat: SUBST | MISSING | EXTRA (strict objects), INFO-* (the others); stock/ours: slot offsets


def slot_closure(mod, parts, depth=2):
    """the parts and the internal functions they call, `depth` levels deep (the stubs of ours are the stock: not followed)"""
    seen = list(parts)
    todo = list(parts)
    for _ in range(depth):
        nxt = []
        for p in todo:
            for _, t in mod.scan(p)["calls"]:
                if not isinstance(t, tuple) and t not in seen:
                    seen.append(t)
                    nxt.append(t)
        todo = nxt
    return seen


def slot_map(mod, parts):
    d = defaultdict(set)
    for p in parts:
        for key, n, ad in mod.vcalls(p):
            d[key if key.startswith("[@") else loose_key(key)].add(n)
    return d


def slot_compare(va, name, S, O, Sc, Oc):
    """S, O: {object key: {slot offsets}} of the two bodies, Sc / Oc: the same over the callees too. An object reached from an imported data symbol
    (`[@GConfig]`, `[@GMalloc]` ...) is compared strictly: a slot of one side that the other side (callees included) never calls on it is reported
    (MISSING = the stock calls it, EXTRA = ours does). Any other object, by the chain of members from its root, is reported only when BOTH sides call it
    and a slot of each side is not called by the other (SUBST, the GetBool / GetInt model); a call that one side makes and the other does not (a getter
    inlined or devirtualized, an esp-relative spill the tracker cannot follow) is only counted as INFO."""
    res = []
    for key in sorted(set(S) | set(O)):
        s, o = S.get(key, set()), O.get(key, set())
        if key.startswith("[@"):
            ms, mo = s - Oc.get(key, set()), o - Sc.get(key, set())
            strict = True
        else:
            if key not in S or key not in O:
                continue
            ms, mo = s - o, o - s
            strict = False
        if not (ms or mo):
            continue
        cat = "SUBST" if (ms and mo) else "MISSING" if ms else "EXTRA"
        if not strict and cat != "SUBST":
            cat = "INFO-" + cat
        res.append(SlotDiff(va, name, cat, key, sorted(s), sorted(o)))
    return res


def audit_slots(va, name, so, oo, stock, ours):
    return slot_compare(va, name, slot_map(stock, so.parts), slot_map(ours, oo.parts), slot_map(stock, slot_closure(stock, so.parts)),
                        slot_map(ours, slot_closure(ours, oo.parts)))


# ------------------------------------------------------------------------------------------------------------------------------------
#   the report
# ------------------------------------------------------------------------------------------------------------------------------------
def sites_text(d):
    if d.side == "C":
        return "stock %d sites, ours %d" % d.sites
    return " ".join("%08x" % x for x in d.sites[:3])


SIDE_WORD = {"S": "stock-only", "O": "ours-only", "C": "more sites", "T": "table"}


def _wrap(prefix, text, width=150):
    return textwrap.fill(text, width=width, initial_indent=prefix, subsequent_indent=" " * len(prefix))


def _print_groups(groups, a, line_of, head_of):
    """groups: {class: [(item, reason)]}; items of one function are printed together, each distinct reason once"""
    for title in ("REAL", "unclear", "NEW", "benign"):
        items = groups[title]
        if not items:
            continue
        print()
        print("  [%s] %d function(s)" % (title, len({head_of(d) for d, _ in items})))
        order = []
        by = defaultdict(list)
        for d, why in items:
            h = head_of(d)
            if h not in by:
                order.append(h)
            by[h].append((d, why))
        for h in order:
            print("    %s" % h)
            reasons = []
            for d, why in by[h]:
                print("        %s" % line_of(d))
                if why and why not in reasons:
                    reasons.append(why)
            for why in reasons:
                print(_wrap("        -> ", why))


def report(a, alld, slotd, stats, stock, ours):
    n_hooks, n_stub, n_cmp, n_strs, tot = stats
    rules = Counter(d.why for d in alld if d.why)
    cand = [d for d in alld if d.why is None]
    groups = {"REAL": [], "unclear": [], "NEW": [], "benign": []}
    used = set()
    for d in cand:
        if d.side == "T":
            cls, why = "NEW", None
        else:
            cls, why = REVIEWED.get((d.va, d.side, d.key[0], d.key[1]), ("NEW", None))
            used.add((d.va, d.side, d.key[0], d.key[1]))
        groups[cls].append((d, why))
    nf = {k: len({d.va for d, _ in v}) for k, v in groups.items()}
    print("audit_strings: %d hooks scanned (%d NW_STOCK stubs skipped), %d compared" % (n_hooks, n_stub, n_cmp))
    print("  strings compared (distinct per hook, summed): stock %d (%d reference sites), ours %d (%d sites); virtual calls tracked: stock %d, ours %d"
          % (tot["s_str"], tot["s_site"], tot["o_str"], tot["o_site"], tot["s_vc"], tot["o_vc"]))
    print()
    print("PASS 1 - string literals: %d differences between the stock and ours, %d explained by a rule %s" % (len(alld), len(alld) - len(cand), dict(rules)))
    print("  candidates left: %d strings in %d functions; by function: REAL %d, unclear %d, benign (reviewed) %d, NEW (nobody has looked) %d"
          % (len(cand), len({d.va for d in cand}), nf["REAL"], nf["unclear"], nf["benign"], nf["NEW"]))
    _print_groups(groups, a, lambda d: "%-11s %-40s %s" % (SIDE_WORD[d.side], show(d.key)[:40], sites_text(d)), lambda d: "%08x %s" % (d.va, d.name))
    if a.all:
        print()
        print("  [explained by a rule]")
        for d in sorted(alld, key=lambda x: (x.why or "", x.va)):
            if d.why:
                print("    %-9s %08x %-36s %-10s %-34s %s" % (d.why, d.va, d.name[:36], SIDE_WORD[d.side], show(d.key)[:34], (d.ev or "")[:100]))
    stale = sorted(k for k in REVIEWED if k not in used) if not a.func else []
    if stale:
        print()
        print("  reviewed entries that match nothing now (fixed, or the string changed): %d" % len(stale))
        for k in stale:
            print("    %08x %s %s   [%s]" % (k[0], k[1], show((k[2], k[3])), REVIEWED[k][0]))
    print()
    sgroups = {"REAL": [], "unclear": [], "NEW": [], "benign": []}
    if slotd is None:
        print("PASS 2 - virtual slots: skipped (--no-slots)")
    else:
        sinfo = Counter()
        for d in slotd:
            if d.cat.startswith("INFO"):
                sinfo[d.cat] += 1
                continue
            cls, why = SLOT_REVIEWED.get((d.va, d.key), ("NEW", None))
            sgroups[cls].append((d, why))
        snf = {k: len({d.va for d, _ in v}) for k, v in sgroups.items()}
        print("PASS 2 - virtual slots of the interface objects: %d strict differences (by function: REAL %d, unclear %d, benign (reviewed) %d, NEW %d); "
              "%d informational %s" % (sum(len(v) for v in sgroups.values()), snf["REAL"], snf["unclear"], snf["benign"], snf["NEW"], sum(sinfo.values()), dict(sinfo)))
        _print_groups(sgroups, a, lambda d: "%-8s object %-24s stock %s ours %s" % (d.cat, d.key[:24], [hex(x) for x in d.stock], [hex(x) for x in d.ours]),
                      lambda d: "%08x %s" % (d.va, d.name))
        if a.all:
            print()
            print("  [informational: a call that only one side makes on an object; inlined or devirtualized getters, spills the tracker cannot follow]")
            for d in slotd:
                if d.cat.startswith("INFO"):
                    print("    %08x %-40s %-13s %-32s stock %s ours %s" % (d.va, d.name[:40], d.cat, d.key[:32], [hex(x) for x in d.stock], [hex(x) for x in d.ours]))
    bad = nf["REAL"] + nf["NEW"] + nf["unclear"] + (sum(len({d.va for d, _ in sgroups[k]}) for k in ("REAL", "NEW", "unclear")) if slotd is not None else 0)
    print()
    print("RESULT: %s" % ("%d function(s) with an open candidate (REAL / unclear / NEW)" % bad if bad else "no open candidate"))
    return 1 if bad else 0


def run(a, rows, stock, ours):
    want = int(a.func, 16) if a.func else None
    n_hooks = n_stub = n_cmp = n_strs = 0
    alld, slotd = [], ([] if not a.no_slots else None)
    tot = Counter()
    for va, name, rva, state in sorted(rows):
        if want is not None and va != want:
            continue
        n_hooks += 1
        diffs, so, oo, info = audit_hook(va, name, rva, stock, ours)
        if diffs is None:
            n_stub += 1
            continue
        n_cmp += 1
        n_strs += len(info["S"]) + len(info["O"])
        tot["s_str"] += len(info["S"])
        tot["o_str"] += len(info["O"])
        tot["s_site"] += sum(len(v) for v in info["S"].values())
        tot["o_site"] += sum(len(v) for v in info["O"].values())
        tot["s_vc"] += sum(len(stock.vcalls(p)) for p in so.parts)
        tot["o_vc"] += sum(len(ours.vcalls(p)) for p in oo.parts) if not a.no_slots else 0
        alld.extend(diffs)
        sd = []
        if slotd is not None:
            sd = audit_slots(va, name, so, oo, stock, ours)
            slotd.extend(sd)
        if want is not None:
            print("== %08x %s  ours=%x  [%s]" % (va, name, ours.base + rva, ours.labels.get(ours.base + rva, "?")))
            print("   stock parts %s   ours parts %s" % ([hex(p) for p in so.parts], [hex(p) for p in oo.parts]))
            print("   stock own  :", sorted(show(k) for k in info["S"]))
            print("   ours  own  :", sorted(show(k) for k in info["O"]))
            print("   stock close:", sorted(show(k) for k in info["Sc"]))
            print("   ours  close:", sorted(show(k) for k in info["Oc"]))
            for tag, mod, b in (("stock", stock, so), ("ours", ours, oo)):
                for p in b.parts:
                    for key, n, ad in mod.vcalls(p):
                        print("   %s vcall %-40s +%#x @%08x" % (tag, key[:40], n, ad))
            print()
    return report(a, alld, slotd, (n_hooks, n_stub, n_cmp, n_strs, tot), stock, ours)


# ------------------------------------------------------------------------------------------------------------------------------------
#   self test: the two bugs the differential run found, on hand-made code
# ------------------------------------------------------------------------------------------------------------------------------------
class _Sec:
    def __init__(self, name, va, size, chars):
        self.Name, self.VirtualAddress, self.Misc_VirtualSize, self.SizeOfRawData, self.Characteristics = name, va, size, size, chars


class _Opt:
    ImageBase = 0x10000000


class _FakePE:
    """just enough of a pefile.PE for Mod: .text at 0x1000, .rdata (read-only) at 0x2000, .data at 0x3000"""
    OPTIONAL_HEADER = _Opt()

    def __init__(self, text, rdata):
        self.img = bytearray(0x4000)
        self.img[0x1000:0x1000 + len(text)] = text
        self.img[0x2000:0x2000 + len(rdata)] = rdata
        self.sections = [_Sec(b".text", 0x1000, 0x1000, 0x60000020), _Sec(b".rdata", 0x2000, 0x1000, 0x40000040), _Sec(b".data", 0x3000, 0x1000, 0xC0000040)]

    def get_memory_mapped_image(self):
        return bytes(self.img)


def _fake(tag, text, rdata, imports):
    pe = _FakePE(text, rdata)
    return Mod(tag, pe, imports, lambda va: [(va, 0x10001000 + len(text))])


def selftest():
    """1. VK_RETURN: the stock pushes L"\\r\\n" (0D 00 0A 00), ours pushes L"\\n"; must come out as stock-only + ours-only, unexplained.
    2. ChatWnd: the stock calls GConfig vslot 0 (GetBool: `mov eax,[IAT]; mov ecx,[eax]; mov edx,[ecx]; call [edx]`), ours vslot 1 (`call [edx+4]`);
       must come out as SUBST on [@GConfig]."""
    iat = 0x10002100
    imports = {iat: "?GConfig@@3PAVFConfigCache@@A"}
    # push offset s; ret
    def push_str(va):
        return b"\x68" + struct.pack("<I", va) + b"\xc3"
    rd_stock = bytearray(0x200)
    rd_stock[0:6] = "\r\n".encode("utf-16le") + b"\0\0"
    rd_ours = bytearray(0x200)
    rd_ours[0:4] = "\n".encode("utf-16le") + b"\0\0"
    stock = _fake("stock", push_str(0x10002000), bytes(rd_stock), imports)
    ours = _fake("ours", push_str(0x10002000), bytes(rd_ours), imports)
    stock.names = {}
    ours.labels = {}
    diffs, so, oo, info = audit_hook(0x10001000, "NCMultiEdit::OnKeyDown", 0x1000, stock, ours)
    open_ = sorted((d.side, show(d.key)) for d in diffs if d.why is None)
    ok1 = open_ == [("O", "L'\\n'"), ("S", "L'\\r\\n'")]
    # mov eax,[iat]; mov ecx,[eax]; mov edx,[ecx]; push 0; push 0; push 0; push 0; call [edx(+4)]; ret
    def getcfg(slot):
        call = b"\xff\x12" if slot == 0 else b"\xff\x52" + bytes((slot,))
        return b"\xa1" + struct.pack("<I", iat) + b"\x8b\x08\x8b\x11" + b"\x6a\x00" * 4 + call + b"\xc3"
    stock = _fake("stock", getcfg(0), b"", imports)
    ours = _fake("ours", getcfg(4), b"", imports)
    stock.names = {}
    ours.labels = {}
    so, oo = Body(stock, 0x10001000, "ChatWnd::ChatWnd"), Body(ours, 0x10001000, "ChatWnd::ChatWnd")
    sd = audit_slots(0x10001000, "ChatWnd::ChatWnd", so, oo, stock, ours)
    ok2 = [(d.cat, d.key, d.stock, d.ours) for d in sd] == [("SUBST", "[@GConfig]", [0], [4])]
    same = audit_slots(0x10001000, "x", so, so, stock, stock)
    ok3 = not same
    print("selftest: VK_RETURN string (L\"\\r\\n\" vs L\"\\n\") flagged: %s" % ("ok" if ok1 else "FAILED %s" % open_))
    print("selftest: GetBool / GetInt slot (vslot 0 vs 1 on GConfig) flagged as SUBST: %s" % ("ok" if ok2 else "FAILED %s" % [(d.cat, d.key, d.stock, d.ours) for d in sd]))
    print("selftest: identical bodies give no slot difference: %s" % ("ok" if ok3 else "FAILED"))
    return 0 if (ok1 and ok2 and ok3) else 1


def main():
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--all", action="store_true", help="also print the benign / rule-explained lines with their evidence")
    ap.add_argument("--func", help="audit only this stock function (hex va) and print everything about it")
    ap.add_argument("--no-slots", action="store_true", help="skip pass 2 (the virtual slots)")
    ap.add_argument("--selftest", action="store_true", help="run the two known bugs on hand-made code and exit")
    ap.add_argument("--log", type=Path, default=ROOT / "build" / "smoke_system" / "NWindowDecomp.log")
    ap.add_argument("--dll", type=Path, default=ROOT / "build" / "bin" / "Release" / "NWindowDecomp.dll")
    ap.add_argument("--map", type=Path, help="default: next to --dll")
    a = ap.parse_args()
    if a.selftest:
        return selftest()
    mapf = a.map or a.dll.with_suffix(".map")
    for f in (a.log, a.dll, mapf):
        if not Path(f).exists():
            print("missing input: %s (run the hook build and cycle first)" % f)
            return 2
    t0 = time.time()
    rows = load_hooks(a.log)
    if not rows:
        print("no hook lines in %s (run the hook cycle first)" % a.log)
        return 1
    tmp = tempfile.mkdtemp(prefix=HOOK_PREFIX)
    try:
        stock = build_stock()
        ours = build_ours(a.dll, mapf, tmp)
        res = run(a, rows, stock, ours)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    print("(%.0f s)" % (time.time() - t0))
    return res


if __name__ == "__main__":
    sys.exit(main())
