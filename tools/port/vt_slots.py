#!/usr/bin/env python3
"""Does every slot of OUR vtable behave like the stock slot it replaces?  (a judge of tools/port/checks.py)

When the constructor of a class is hooked, the objects carry the vtable the compiler builds from our headers (not the stock
one). verify_hooks / vt_audit prove the SHAPE of that vtable (slot count, names, `ret N`) and prove every HOOKED stock address
against our body; nothing proved that a slot whose stock function is shared / folded / a thunk / unported behaves like the
compiled method that sits in our vtable. The first one found (wave C, lane misc): 30 classes had the C3 `WindowProc` body
(`Msg != WM_PAINT && !IsEnabled()` first) in slot 25 while the C4 stock slot is NControl's `call NCWnd::WindowProc; neg/sbb/neg`
(no enabled check): a disabled window swallowed the messages the stock one handles.

For every class whose constructor is hooked (`Class::Class` in the smoke log) and every slot of its compiled vtable (the smoke
log lines `vt Class slot stock=VA ours=RVA`; a class with no NW_CHECK_VTABLE, so no such lines, is read from the image at its
`??_7Class@@6B@` symbol against analysis/classes.json), this follows what the slot reaches in OUR dll and sorts it:

  hook      a hooked function (verify_hooks compared it with the stock one) whose stock address is the stock slot's function, or
            what the plain `jmp` thunk the stock slot starts with reaches                              proven
  stub      an NW_STOCK stub that jumps to the stock slot's function                                   proven
  import    both sides are `jmp [IAT]` of the same import / both are _purecall                          proven
  same      a COMPILED function that is the stock function anyway:
              - the stock slot's function is hooked and the compiled function is that very method (the hook is
                `NW_HOOK_METHOD(va, Class, Method)` and the slot holds `Class::Method`, whose body the hook's forwarder inlines;
                the linker folds identical functions: every name at the address counts) or compiles to the same code
                (instructions, callees, imports and data compared by name)
              - a forwarder chain: the stock slot is `return Parent::M(a, b, c) != 0` (29 bytes, any number of them in a row, the
                last calling a real function F) and ours is a hook / a stub / a compiled `return G(a, b, c) != 0` that ends in the
                same F (the normalisation is the identity on F when every path of F returns 0 or 1: BOOL_VALUED)
              - a virtual thunk: the stock slot is `mov eax,[ecx]; jmp [eax+4N]` (this->slot N(...)) and ours is the same thunk, or
                reaches what the stock's slot N is
            proven by the hook
  dtor      the compiler's deleting destructor of a class whose destructor is hooked, where the stock slot is the stock
            deleting destructor that calls that hooked destructor                                      proven by the hook
  DIFF      a compiled function whose stock function is hooked, with other code than the hooked method  candidate
  UNPROVEN  a compiled function and the stock slot's function is not hooked at all (shared, folded, a thunk, unported)  candidate
  OTHER     the slot reaches a hook / a stub of a function that is not the stock slot's one (a missing override: the slot is the
            parent's, the stock class has its own; check_vtlive, which this replaces, skipped every slot that is a hook's address) candidate

A candidate is a bug until somebody read the stock function (python tools/disasm.py <va>, analysis/ghidra/asm/<va>.asm) and our
body and decided: fix the body (make it the stock's: see `NCTrainWnd::WindowProc`), or remove our override so the slot
inherits a proven parent body, or leave the slot a NW_STOCK stub (it jumps to the stock address, which a hook may own); or
write it in db/vtslots_ok.txt: `Class|slot reason hash` (one-word reason; the hash is the one of the compiled code the review
looked at, so a body edited later is a candidate again). The checks.py judge fails on a candidate that is not in the list.

  python tools/port/vt_slots.py                 summary + every candidate not in db/vtslots_ok.txt (exit 1 when there is one)
  python tools/port/vt_slots.py --all           every non-proven slot, reviewed ones included
  python tools/port/vt_slots.py --class NCButton [--class ...]     every slot of these classes, proven ones too
  python tools/port/vt_slots.py --dtors         the classes whose destructor (not the constructor) is hooked as well
  python tools/port/vt_slots.py --update --reason word    rewrite db/vtslots_ok.txt from the tree: reviewed entries keep their
                                                reason and get the new hash, new candidates get --reason (read them first!),
                                                stale ones go

Reads build/bin/Release/NWindowDecomp.{dll,map} and build/smoke_system/NWindowDecomp.log (tools/cycle.py makes them) and the
stock image orig/NWindow.dll."""
from __future__ import annotations

import argparse
import bisect
import csv
import hashlib
import json
import re
import struct
import sys
from pathlib import Path

import pefile
from capstone import CS_ARCH_X86, CS_MODE_32, Cs

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from nwd.image import Image  # noqa: E402

DLL = ROOT / "build" / "bin" / "Release" / "NWindowDecomp.dll"
MAP = ROOT / "build" / "bin" / "Release" / "NWindowDecomp.map"
LOG = ROOT / "build" / "smoke_system" / "NWindowDecomp.log"
LIST = ROOT / "db" / "vtslots_ok.txt"
FUNCS = ROOT / "db" / "functions.csv"
CLASSES = ROOT / "analysis" / "classes.json"

LOG_RE = re.compile(r"^\S+\s+([0-9a-f]{8}) (.+?)\s+ours=([0-9a-f]+) (.*)$")
VT_RE = re.compile(r"^\S+\s+vt (\S+) (\d+) stock=([0-9a-f]{8}) ours=([0-9a-f]+)")
MAP_RE = re.compile(r"^\s*([0-9a-f]{4}):([0-9a-f]{8})\s+(\S+)\s+([0-9a-f]{8})\s+(f?)\s*(i?)\s+\S+")
STOCK_PURECALL = 0x10171E13      # _purecall of the stock static CRT (C4 image)
HEXADDR = re.compile(r"0x[0-9a-f]{8}\b")
CANDIDATE = ("DIFF", "UNPROVEN", "OTHER")


def demangle(sym: str) -> str:
    """`?WindowProc@NCActionWnd@@UAEHIIJ@Z` -> `NCActionWnd::WindowProc` (enough to read; anything else is kept as it is)"""
    sym = re.sub(r"\?A0x[0-9a-f]+", "", sym)
    m = re.match(r"^\?\?_([EG])(\w+)@@", sym)
    if m:
        return "%s::`%s deleting destructor`" % (m.group(2), "vector" if m.group(1) == "E" else "scalar")
    m = re.match(r"^\?\?1(\w+)@@", sym)
    if m:
        return "%s::~%s" % (m.group(1), m.group(1))
    m = re.match(r"^\?\?0(\w+)@@", sym)
    if m:
        return "%s::%s" % (m.group(1), m.group(1))
    m = re.match(r"^\?(\w+)@(\w+)@@", sym)
    if m:
        return "%s::%s" % (m.group(2), m.group(1))
    m = re.match(r"^\?(\w+)@@", sym)
    if m:
        return m.group(1)
    return sym


class Ours:
    """our compiled DLL: bytes, symbols (the linker map), hooks and vtable lines (the smoke log)"""

    def __init__(self):
        self.pe = pefile.PE(str(DLL))
        self.base = self.pe.OPTIONAL_HEADER.ImageBase
        self.mem = self.pe.get_memory_mapped_image()
        self.size = len(self.mem)
        self.md = Cs(CS_ARCH_X86, CS_MODE_32)
        self.md.detail = False
        self.imports = {}
        for e in getattr(self.pe, "DIRECTORY_ENTRY_IMPORT", []):
            for i in e.imports:
                self.imports[i.address] = (i.name or b"#%d" % i.ordinal).decode()
        self.sym = {}          # code address -> raw name (the first wins, the non-`__` ones first)
        self.dsym = {}         # every other symbol: data, strings, vtables
        self.alias = {}        # code address -> every name there (the linker folds identical functions: one address, several names)
        for line in MAP.read_text(encoding="latin-1").splitlines():
            m = MAP_RE.match(line)
            if not m:
                continue
            a, n = int(m.group(4), 16), m.group(3)
            if m.group(1) == "0001":
                self.alias.setdefault(a, []).append(n)
                if a not in self.sym or (self.sym[a].startswith("_") and not n.startswith("_")):
                    self.sym[a] = n
            else:
                self.dsym.setdefault(a, n)
        self.starts = sorted(self.sym)
        self.dstarts = sorted(self.dsym)
        self.by_name = {}      # demangled `Class::Method` -> [address]: the compiled methods (the hooks' forwarders left out)
        for a, ns in self.alias.items():
            for n in ns:
                if n.startswith("?") and "$Fwd@" not in n and "NwCtor_" not in n and "NwDtor_" not in n:
                    self.by_name.setdefault(demangle(n), []).append(a)
        self.by_name = {k: sorted(set(v)) for k, v in self.by_name.items()}
        self.hook_at = {}      # our address -> [(stock va, name)]
        self.hook_va = {}      # stock va -> [(our address, name)]
        self.hook_by_name = {}  # `Class::Method` (no arguments) -> {stock va}
        self.vt = {}           # class -> [(slot, stock va, our address)]
        self.ctor_hooked = set()
        self.dtor_hooked = set()
        for line in LOG.read_text(encoding="latin-1").splitlines():
            m = VT_RE.match(line)
            if m:
                self.vt.setdefault(m.group(1), []).append((int(m.group(2)), int(m.group(3), 16), self.base + int(m.group(4), 16)))
                continue
            m = LOG_RE.match(line)
            if not m or not m.group(4).startswith("ok"):
                continue
            va, name, a = int(m.group(1), 16), m.group(2), self.base + int(m.group(3), 16)
            self.hook_at.setdefault(a, []).append((va, name))
            self.hook_va.setdefault(va, []).append((a, name))
            self.hook_by_name.setdefault(name.split("(")[0], set()).add(va)
            mm = re.match(r"^(\w+)::(~?)(\w+)(?:\(|$)", name)
            if mm and mm.group(1) == mm.group(3):
                (self.dtor_hooked if mm.group(2) else self.ctor_hooked).add(mm.group(1))

    def read(self, a: int, n: int) -> bytes:
        o = a - self.base
        return bytes(self.mem[o:o + n]) if 0 <= o < self.size else b""

    def in_code(self, a: int) -> bool:
        return bool(self.starts) and self.starts[0] <= a < self.end(self.starts[-1])

    def name(self, a: int) -> str:
        """the function at / containing a (demangled)"""
        i = bisect.bisect_right(self.starts, a) - 1
        if i < 0:
            return "?%08x" % a
        s = self.starts[i]
        n = demangle(self.sym[s])
        return n if s == a else "%s+%#x" % (n, a - s)

    def dname(self, a: int) -> str:
        """a data address by the symbol it is at (or inside)"""
        if a in self.dsym:
            return self.dsym[a]
        if a in self.sym:
            return demangle(self.sym[a])
        i = bisect.bisect_right(self.dstarts, a) - 1
        if i >= 0 and a - self.dstarts[i] < 0x4000:
            return "%s+%#x" % (self.dsym[self.dstarts[i]], a - self.dstarts[i])
        return "A"

    def end(self, a: int) -> int:
        i = bisect.bisect_right(self.starts, a)
        return min(self.starts[i] if i < len(self.starts) else a + 0x400, a + 0x3000)

    def chain(self, a: int) -> list[int]:
        """a, then what a plain thunk reaches: `push ebp; mov ebp,esp; pop ebp; jmp X` or `jmp X`"""
        out = [a]
        for _ in range(6):
            b = self.read(a, 9)
            if b[:5] == b"\x55\x8b\xec\x5d\xe9":
                a = (a + 9 + struct.unpack("<i", b[5:9])[0]) & 0xFFFFFFFF
            elif b[:1] == b"\xe9":
                a = (a + 5 + struct.unpack("<i", b[1:5])[0]) & 0xFFFFFFFF
            else:
                break
            out.append(a)
        return out

    def stub_target(self, a: int):
        """the stock va an NW_STOCK stub jumps to (`mov eax,[g_nwBase]; add eax,va-0x10000000; jmp eax`), else None"""
        b = self.read(a, 12)
        if len(b) == 12 and b[0] == 0xA1 and b[5] == 0x05 and b[10:12] == b"\xff\xe0":
            return struct.unpack("<I", b[6:10])[0] + 0x10000000
        return None

    def import_of(self, a: int):
        b = self.read(a, 6)
        if b[:2] == b"\xff\x25":
            return self.imports.get(struct.unpack("<I", b[2:6])[0])
        return None

    def norm(self, a: int) -> list[str]:
        """the instructions of the function at a with every address named: a call / jump to another function by its name (a plain
        thunk followed to its body), a jump inside the function by its offset, an import or a data / string / vtable address by the
        symbol of the map. Two functions with the same list are the same code."""
        out = []
        code = self.read(a, self.end(a) - a)
        fend = self.end(a)
        for i in self.md.disasm(code, a):
            if i.mnemonic in ("int3", "nop"):
                continue
            op = i.op_str
            if (i.mnemonic == "call" or i.mnemonic.startswith("j")) and re.fullmatch(r"0x[0-9a-f]+", op):
                t = int(op, 16)
                if i.mnemonic != "call" and a <= t < fend:
                    op = "L%+d" % (t - a)
                elif self.in_code(t):
                    op = "<%s>" % self.name(self.chain(t)[-1])
                else:
                    op = self.dname(t)
            else:
                def sub(m):
                    v = int(m.group(0), 16)
                    if v in self.imports:
                        return "import:" + self.imports[v]
                    if not self.base <= v < self.base + self.size:
                        return m.group(0)
                    n = self.name(v) if self.in_code(v) else self.dname(v)
                    return re.sub(r"(__ehhandler|__unwindfunclet|__catch|__sehtable)\$.*", r"\1", n)
                op = HEXADDR.sub(sub, op)
            out.append("%s %s" % (i.mnemonic, op))
        return out


def stock_chain(stock: Image, va: int) -> list[int]:
    out = [va]
    for _ in range(6):
        b = stock.read(va, 5)
        if b[:1] == b"\xe9":
            va = (va + 5 + struct.unpack("<i", b[1:5])[0]) & 0xFFFFFFFF
            out.append(va)
        else:
            break
    return out


def stock_import(stock: Image, va: int):
    b = stock.read(va, 6)
    if b[:2] == b"\xff\x25":
        imp = stock.imports.get(struct.unpack("<I", b[2:6])[0])
        return imp[1] if imp else None
    return None


def stock_first_call(stock: Image, va: int):
    """the target of the first direct call of the stock function (its first 24 instructions)"""
    for n, i in enumerate(stock.md.disasm(stock.read(va, 120), va)):
        if n > 24:
            break
        if i.mnemonic == "call" and re.fullmatch(r"0x[0-9a-f]+", i.op_str):
            return int(i.op_str, 16)
        if i.mnemonic in ("ret", "jmp"):
            break
    return None


def db_names() -> dict[int, tuple[str, int]]:
    out = {}
    if FUNCS.exists():
        for r in csv.DictReader(FUNCS.open(encoding="utf-8")):
            out[int(r["va"], 16)] = (r["name"], int(r["size"] or 0))
    return out


def class_info() -> dict[str, dict]:
    """analysis/classes.json by C++ name: ancestors and the stock vtable (slot -> va)"""
    out = {}
    if CLASSES.exists():
        for c in json.loads(CLASSES.read_text(encoding="utf-8")):
            out[c.get("cpp") or c.get("name")] = {"ancestors": c.get("ancestors", []),
                                                  "slots": {s["slot"]: s["va"] for s in c.get("vtable_slots", [])}}
    return out


def vtables_from_map(o: Ours, info: dict[str, dict]) -> list[str]:
    """a class whose constructor is hooked but that has no NW_CHECK_VTABLE (so no `vt` lines in the smoke log) still carries our vtable:
    read it from the image at the symbol `??_7Class@@6B@` and take the stock slots from classes.json. -> the classes added"""
    rev = {n: a for a, n in o.dsym.items()}
    added = []
    for cls in sorted((o.ctor_hooked | o.dtor_hooked) - set(o.vt)):
        a = rev.get("??_7%s@@6B@" % cls)
        slots = info.get(cls, {}).get("slots")
        if a is None or not slots:
            continue
        rows = []
        for k, sva in sorted(slots.items()):
            b = o.read(a + 4 * k, 4)
            if len(b) == 4:
                rows.append((k, sva, struct.unpack("<I", b)[0]))
        o.vt[cls] = rows
        added.append(cls)
    return added


# The stock's plain forwarders: `return Parent::Method(a, b, c) != 0` as the C4 compiler wrote it (29 bytes: the three arguments pushed again,
# the call, neg / sbb / neg). A function of a class that adds nothing is one of these, calling its parent's: a chain of them is one
# normalisation of the function at the end of it.
FWD_BYTES = re.compile(re.escape(b"\x8b\x44\x24\x0c\x8b\x54\x24\x08\x50\x8b\x44\x24\x08\x52\x50\xe8") + b"(....)" +
                       re.escape(b"\xf7\xd8\x1b\xc0\xf7\xd8\xc2\x0c\x00"), re.S)
# `mov eax,[ecx]; jmp [eax+N]`: a virtual function that only calls another slot of the same object (the stock's `this->slot N(...)`)
VTHUNK_BYTES = re.compile(re.escape(b"\x8b\x01\xff") + b"(?:" + re.escape(b"\xa0") + b"(....)|" + re.escape(b"\x60") + b"(.))", re.S)
# stock functions whose every return path gives 0 or 1 (mov eax,1 / xor eax,eax / neg-sbb-neg): the forwarders' normalisation is the
# identity on them, so a slot that is the function itself and one that forwards to it are the same function
BOOL_VALUED = {0x10034390}      # NCWnd::WindowProc: read instruction by instruction (wave C, vtslots lane): every `ret 0xc` follows one of the three


def stock_value(stock: Image, va: int):
    """(final va, layers): plain `jmp` thunks are transparent, every 29-byte normalising forwarder is a layer"""
    layers = 0
    for _ in range(16):
        b = stock.read(va, 29)
        m = FWD_BYTES.fullmatch(b)
        if m:
            layers += 1
            va = (va + 20 + struct.unpack("<i", m.group(1))[0]) & 0xFFFFFFFF
        elif b[:1] == b"\xe9":
            va = (va + 5 + struct.unpack("<i", b[1:5])[0]) & 0xFFFFFFFF
        else:
            break
    return va, layers


def value_eq(a, b) -> bool:
    (fa, la), (fb, lb) = a, b
    return fa == fb and ((la >= 1) == (lb >= 1) or fa in BOOL_VALUED)


def vthunk_slot(read, va: int):
    """N when the function at va is `mov eax,[ecx]; jmp [eax + 4*N]`, else None"""
    m = VTHUNK_BYTES.match(read(va, 8))
    if not m:
        return None
    d = struct.unpack("<I", m.group(1))[0] if m.group(1) is not None else m.group(2)[0]
    return d // 4


def compiled_forward(o: Ours, a: int):
    """the call target when the compiled function at a is `return F(Msg, wParam, lParam) != 0` (frame, three pushes, call, neg/sbb/neg), else None"""
    code = o.read(a, o.end(a) - a)
    ins = [i for i in o.md.disasm(code, a) if i.mnemonic not in ("int3", "nop")]
    shape = ["push ebp", "mov ebp, esp", "push dword ptr [ebp + 0x10]", "push dword ptr [ebp + 0xc]", "push dword ptr [ebp + 8]", "call",
             "neg eax", "sbb eax, eax", "neg eax", "pop ebp", "ret 0xc"]
    if len(ins) != len(shape):
        return None
    for i, s in zip(ins, shape):
        if s == "call":
            if i.mnemonic != "call" or not re.fullmatch(r"0x[0-9a-f]+", i.op_str):
                return None
        elif ("%s %s" % (i.mnemonic, i.op_str)).strip() != s:
            return None
    return int(ins[5].op_str, 16)


def ours_value(o: Ours, stock: Image, a: int, depth: int = 0):
    """the (final stock va, layers) of the function our address a stands for: a hook, a stub, the compiled method of a hooked one, or a
    compiled plain forwarder to one of those; None when it is something else"""
    if depth > 8:
        return None
    ch = o.chain(a)
    for x in ch:
        for va, _n in o.hook_at.get(x, []):
            return stock_value(stock, va)
    st = o.stub_target(ch[-1])
    if st is not None:
        return stock_value(stock, st)
    names = {demangle(n) for n in o.alias.get(ch[-1], [])}
    for n in names:
        vas = o.hook_by_name.get(n, set())
        if len(vas) == 1 and len(o.by_name.get(n, [])) == 1:
            return stock_value(stock, next(iter(vas)))
    t = compiled_forward(o, ch[-1])
    if t is not None:
        v = ours_value(o, stock, t, depth + 1)
        if v is not None:
            return v[0], v[1] + 1
    return None


def scan(o: Ours, stock: Image, classes=None, dtors=False):
    """-> rows (class, slot, kind, stock va, our address, detail, hash of the compiled code)"""
    rows = []
    scope = set(o.ctor_hooked) | (set(o.dtor_hooked) if dtors else set())
    info = class_info()
    anc = {c: v["ancestors"] for c, v in info.items()}
    extra = vtables_from_map(o, info)
    cache = {}
    stock_slot = {c: {s: va for s, va, _ in v} for c, v in o.vt.items()}
    ours_slot = {c: {s: a for s, _, a in v} for c, v in o.vt.items()}

    def normc(a):
        if a not in cache:
            cache[a] = o.norm(a)
        return cache[a]

    for cls in sorted(o.vt):
        if classes is not None:
            if cls not in classes:
                continue
        elif cls not in scope:
            continue
        for slot, sva, ova in sorted(o.vt[cls]):
            sch = stock_chain(stock, sva)
            och = o.chain(ova)
            body = och[-1]
            h, detail = "", ""
            simp, oimp = stock_import(stock, sva), o.import_of(ova)
            hooks = [(va, n) for x in och for (va, n) in o.hook_at.get(x, [])]
            stub = o.stub_target(body)
            if simp or oimp:
                kind = "import" if simp == oimp else "OTHER"
                detail = "stock import %s ours import %s" % (simp, oimp)
            elif sva == STOCK_PURECALL and "purecall" in o.name(ova).lower():
                kind, detail = "import", "_purecall"
            elif any(va in sch for va, _ in hooks):
                kind, detail = "hook", next(n for va, n in hooks if va in sch)
            elif stub is not None and stub in sch:
                kind, detail = "stub", "stub -> %08x" % stub
            else:
                kind = None
                # the stock slot only calls another slot of the object (`mov eax,[ecx]; jmp [eax+4N]`): ours is that call too, or reaches what the
                # stock's slot N is
                n = vthunk_slot(stock.read, sch[-1])
                if n is not None:
                    if vthunk_slot(o.read, body) == n:
                        kind, detail = "same", "the same virtual thunk (slot %d)" % n
                    elif n in stock_slot[cls]:
                        ov = ours_value(o, stock, ova)
                        if ov is not None and ov[0] in stock_chain(stock, stock_slot[cls][n]) and ov[1] == 0:
                            kind, detail = "same", "stock thunk to slot %d (%08x), ours reaches that function" % (n, ov[0])
                        elif ours_slot[cls].get(n) is not None and o.chain(ours_slot[cls][n])[-1] == body:
                            kind, detail = "same", "stock thunk to slot %d, ours is that slot's function" % n
                if kind is None:
                    sv, ov = stock_value(stock, sva), ours_value(o, stock, ova)
                    if sv[1] >= 1 and ov is not None and value_eq(sv, ov):
                        kind, detail = "same", "forwarder chain to %08x (%d layer(s) stock, %d ours)" % (sv[0], sv[1], ov[1])
                if kind is not None:
                    pass
                elif hooks:
                    kind = "OTHER"
                    detail = "reaches the hook of %s (%08x), the stock slot is %08x" % (hooks[0][1], hooks[0][0], sva)
                elif stub is not None:
                    kind, detail = "OTHER", "stub jumps to %08x, the stock slot is %08x" % (stub, sva)
                else:
                    cname = o.name(body)
                    code = normc(body)
                    h = hashlib.md5("\n".join(code).encode()).hexdigest()[:8]
                    hv = [va for va in sch if va in o.hook_va]
                    m = re.match(r"^(\w+)::`(?:vector|scalar) deleting destructor`$", cname)
                    if m and not hv:
                        # the compiler's deleting destructor: the stock one is `call ~Y; (delete)` and ~Y is hooked, Y the class or an ancestor
                        # that has the destructor (a class without its own shares the parent's)
                        tgt = stock_first_call(stock, sch[-1])
                        tg = stock_chain(stock, tgt) if tgt else []
                        dh = [nm for va in tg for (_a, nm) in o.hook_va.get(va, [])]
                        y = re.match(r"^(\w+)::~(\w+)$", dh[0]) if dh else None
                        if y and (y.group(1) == m.group(1) or y.group(1) in anc.get(m.group(1), [])):
                            kind, detail = "dtor", "calls the hooked " + dh[0]
                        else:
                            kind = "UNPROVEN"
                            detail = "%s; the stock one calls %s" % (cname, ("%08x%s" % (tgt, " (hooked as %s)" % dh[0] if dh else " (not hooked)")) if tgt else "nothing")
                    elif hv:
                        hnames = [nm.split("(")[0] for va in hv for (_a, nm) in o.hook_va[va]]
                        mine = {demangle(nm) for nm in o.alias.get(body, [])}
                        hit = [hn for hn in hnames if hn in mine and len(o.by_name.get(hn, [])) == 1]
                        if hit:
                            kind, detail = "same", "is " + hit[0]
                        else:
                            for hn in hnames:
                                for ca in o.by_name.get(hn, []):
                                    if normc(ca) == code:
                                        kind, detail = "same", "same code as the hooked " + hn
                                        break
                                if kind:
                                    break
                        if not kind:
                            kind = "DIFF"
                            detail = "compiled %s; the stock function (%08x) is hooked as %s" % (cname, hv[0], hnames[0])
                    else:
                        kind, detail = "UNPROVEN", "compiled %s; the stock function is not hooked" % cname
            rows.append((cls, slot, kind, sva, ova, detail, h))
    scan.extra = extra
    return rows


def load_list() -> dict[str, tuple[str, str]]:
    """key `Class|slot` -> (reason, hash)"""
    out = {}
    if LIST.exists():
        for line in LIST.read_text(encoding="utf-8").splitlines():
            line = line.split("#", 1)[0].strip()
            if line:
                f = line.split()
                out[f[0]] = (f[1] if len(f) > 1 else "", f[2] if len(f) > 2 else "")
    return out


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--class", dest="classes", action="append", default=[])
    ap.add_argument("--all", action="store_true", help="every non-proven slot, reviewed ones included")
    ap.add_argument("--dtors", action="store_true", help="also the classes whose destructor (not the constructor) is hooked")
    ap.add_argument("--update", action="store_true")
    ap.add_argument("--reason", default="")
    a = ap.parse_args()
    for p in (DLL, MAP, LOG):
        if not p.exists():
            print("vt_slots: %s is missing (run tools/cycle.py first)" % p.relative_to(ROOT))
            return 1
    o = Ours()
    stock = Image()
    names = db_names()
    rows = scan(o, stock, set(a.classes) if a.classes else None, a.dtors)
    listed = load_list()
    cand = [r for r in rows if r[2] in CANDIDATE]

    def status(r):
        key = "%s|%d" % (r[0], r[1])
        if key not in listed:
            return "new"
        return "ok" if (not listed[key][1] or not r[6] or listed[key][1] == r[6]) else "changed"

    if a.update:
        new = [r for r in cand if status(r) == "new"]
        if new and not a.reason:
            print("vt_slots --update: %d new candidate(s): read them (run without --update), then --update --reason <word>" % len(new))
            return 1
        out = []
        for r in cand:
            key = "%s|%d" % (r[0], r[1])
            reason = listed[key][0] if key in listed and listed[key][0] else a.reason
            out.append("%s %s%s" % (key, reason, (" " + r[6]) if r[6] else ""))
        head = [ln for ln in LIST.read_text(encoding="utf-8").splitlines() if ln.startswith("#")] if LIST.exists() else []
        if not head:
            head = ["# Class|slot reason hash: a slot of the compiled vtable of a class whose constructor is hooked that reaches a compiled",
                    "# function the judges cannot tie to the stock one (tools/port/vt_slots.py); each was read against the C4 stock function.",
                    "# The hash is that of the compiled code that was read: a body edited since is a candidate again."]
        LIST.write_text("".join(x + "\n" for x in head) + "".join(x + "\n" for x in sorted(out)), encoding="utf-8")
        print("vt_slots: %d reviewed slot(s) written to db/vtslots_ok.txt" % len(out))
        return 0

    def fname(va):
        n = names.get(va)
        return ("%s (%d B)" % (n[0], n[1])) if n else ""

    count = {}
    for r in rows:
        count[r[2]] = count.get(r[2], 0) + 1
    if a.classes:
        for r in rows:
            print("  %-26s %3d %-8s stock %08x %-52s %s" % (r[0], r[1], r[2], r[3], fname(r[3])[:52], r[5]))
    else:
        for r in rows:
            if r[2] not in CANDIDATE and not (a.all and r[2] not in ("hook", "stub", "import")):
                continue
            st = status(r) if r[2] in CANDIDATE else "-"
            if st == "ok" and not a.all:
                continue
            print("%-7s %-26s slot %3d %-8s stock %08x %-52s %s%s" % (
                st.upper() if st != "ok" else "ok", r[0], r[1], r[2], r[3], fname(r[3])[:52], r[5],
                ("  [hash %s]" % r[6]) if r[6] and st in ("new", "changed") else ""))
    bad = [r for r in cand if status(r) != "ok"]
    summary = ", ".join("%d %s" % (v, k) for k, v in sorted(count.items()))
    print("vt_slots: %d classes, %d slots (%s); %d candidate(s), %d not reviewed in db/vtslots_ok.txt" % (
        len({r[0] for r in rows}), len(rows), summary, len(cand), len(bad)))
    return 1 if bad and not a.classes else 0


if __name__ == "__main__":
    sys.exit(main())
