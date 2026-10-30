#!/usr/bin/env python3
"""Client -> server packets of the C3 client, read from Engine.dll (pefile + capstone, nothing else).

How a packet leaves the client
  NWindow  : fills an L2ParamStack (PushBack, first pushed = first popped) and calls a virtual of
             Engine->NetHandler (a UL2NetHandler; the real class is UNetworkHandler, whose vtable
             Engine.dll exports with every slot named: ??_7UNetworkHandler@@6BUObject@@@).
  Engine   : UNetworkHandler::Request*(L2ParamStack&) pops the values (L2ParamStack::Top) and calls
             `Socket->Send(Socket, "fmt", opcode, args...)`, vtable slot +0x64 of the object at
             UNetworkHandler+0x3C (cdecl, varargs). Send() runs the packet writer (Format, rva 0x3EBB20 of
             the C3 Engine) which appends the fields to a 0x1FFE byte buffer, puts the 16 bit total length
             in front of it, encrypts and calls WSOCK32!send. Variable length packets build their list in a
             local buffer first (FormatTo, rva 0x3EBF50, same writer, no header) and hand it to Send() as a
             block (`b`: int length, void* pointer).
  Writer alphabet (jump table at 0x103ebcec, index table at 0x103ebd0c, indexed by letter - 0x53):
             c  1 byte  (low byte of an int argument)          h  2 bytes     d  4 bytes
             f  8 bytes (a double: two stack slots)
             S  UTF-16 string, NUL terminated (NULL pointer -> 00 00)
             s  8 bit string, NUL terminated (NULL pointer -> 00)
             b  and  x : raw block, two arguments (int length, void* pointer): `length` bytes copied
             any other letter is skipped without consuming an argument.
             The first letter of a Send() format is always `c` and is the OPCODE (the first value after the
             format). Extended packets are `cd..`/`ch..` (opcode, then the sub-opcode as a field).
             FVector arguments (floats) are converted with _ftol before they are pushed: they are dwords.

What this tool does
  1. finds the UNetworkHandler vtable by its export and reads every slot (names come from the exports);
  2. walks each slot's code, tracking the values of registers / stack slots: `Pn` is the n-th
     L2ParamStack::Top() result (P0 = the first value NWindow pushed), `argN` is the N-th stack argument of
     the virtual itself (the ones that are not an L2ParamStack&), `ftol(..)` a float converted to int;
  3. records each Send() call (format, opcode, values) and each FormatTo() call (the per-item format of lists);
  4. looks for the NWindow.dll functions that call each slot (--nwindow, uses the Ghidra export of the stock
     NWindow in analysis/ghidra/asm) and the values they push;
  5. cross-checks with the Lucera server's GamePacketHandler (--server) and the netpro spec (--spec).

  The Java server is read while other lanes edit it: the server_* fields are the state at the time of the run
  (--handler points at an older GamePacketHandler.java, e.g. java_backup_pre_audit, to see the table as it was).

usage: python tools/c3/c2s_formats.py [--engine PATH] [--json analysis/c3_c2s_formats.json] [--md rows.md]
                                      [--name SUBSTR] [--trace] [--list] [--nwindow] [--server DIR] [--spec HTML]
                                      [--handler GamePacketHandler.java]
  (no arguments: one line per Send() in the terminal; --json also needs analysis/ghidra for the NWindow callers)
"""
from __future__ import annotations

import argparse
import html
import json
import re
import struct
import os
import sys
from pathlib import Path

import pefile
from capstone import CS_ARCH_X86, CS_MODE_32, Cs

ROOT = Path(__file__).resolve().parents[2]
# NWD_C3_SYSTEM: the C3 client's System folder
DEFAULT_ENGINE = (Path(os.environ["NWD_C3_SYSTEM"]) / "Engine.dll" if os.environ.get("NWD_C3_SYSTEM") else None)
# NWD_C3_SERVER_SRC: the source tree of the C3 test server (java/, dist/)
DEFAULT_SERVER = (Path(os.environ["NWD_C3_SERVER_SRC"]) if os.environ.get("NWD_C3_SERVER_SRC") else None)
# NWD_C3_SPEC: the C3 packet spec page ("C3 Rise Of Darkness.html")
DEFAULT_SPEC = (Path(os.environ["NWD_C3_SPEC"]) if os.environ.get("NWD_C3_SPEC") else None)
VTABLE_SYM = "??_7UNetworkHandler@@6BUObject@@@"
SEND_SLOT = 0x64          # vtable offset of the socket object's Send(this, fmt, opcode, ...) (cdecl, varargs)
SOCKET_FIELD = 0x3C       # UNetworkHandler::Socket
TOP_SYM = "?Top@L2ParamStack@@QAEPAXXZ"
FIRST_SENDER_SLOT = 52    # slots below are Tick/Init/getters (checked with --list)
FORMAT_TO = 0x103EBF50    # FormatTo(socket, buf, size, fmt, ...): the writer into a local buffer
FTOL = 0x10714D84         # CRT _ftol
STRUCT_SIZES = {"FVector": 12, "ItemInfo": 0x30, "CharacterInfo": 0x60, "FName": 8}

REG32 = ("eax", "ebx", "ecx", "edx", "esi", "edi", "ebp")
SUBREG = {"al": "eax", "bl": "ebx", "cl": "ecx", "dl": "edx", "ax": "eax", "bx": "ebx", "cx": "ecx", "dx": "edx"}


class Engine:
    def __init__(self, path: Path):
        self.pe = pefile.PE(str(path))
        self.base = self.pe.OPTIONAL_HEADER.ImageBase
        self.mem = bytes(self.pe.get_memory_mapped_image())
        self.md = Cs(CS_ARCH_X86, CS_MODE_32)
        self.md.detail = False
        self.exp = self._exports()
        self.by_va: dict[int, list[str]] = {}
        for n, va in self.exp.items():
            self.by_va.setdefault(va, []).append(n)
        self.imports = {}
        for e in getattr(self.pe, "DIRECTORY_ENTRY_IMPORT", []):
            for i in e.imports:
                self.imports[i.address] = e.dll.decode() + "!" + (i.name or b"").decode()
        self.text = [(self.base + s.VirtualAddress, self.base + s.VirtualAddress + s.Misc_VirtualSize)
                     for s in self.pe.sections if s.Characteristics & 0x20000000]

    def _exports(self):
        # pefile names only the first 8192 exports: read the table by hand
        dd = self.pe.OPTIONAL_HEADER.DATA_DIRECTORY[0]
        hdr = self.pe.get_data(dd.VirtualAddress, 40)
        (_, _, _, _, _, _b, nf, nn, af, an, ao) = struct.unpack("<IIHHIIIIIII", hdr)
        funcs = struct.unpack("<%dI" % nf, self.pe.get_data(af, 4 * nf))
        names = struct.unpack("<%dI" % nn, self.pe.get_data(an, 4 * nn))
        ords = struct.unpack("<%dH" % nn, self.pe.get_data(ao, 2 * nn))
        return {self.pe.get_string_at_rva(n).decode(): self.base + funcs[o] for n, o in zip(names, ords)}

    def u32(self, va):
        return struct.unpack_from("<I", self.mem, va - self.base)[0]

    def is_code(self, va):
        return any(a <= va < b for a, b in self.text)

    def resolve(self, va):
        """Follow an incremental-link `jmp rel32` thunk."""
        b = self.mem[va - self.base:va - self.base + 5]
        if b[:1] == b"\xe9":
            return va + 5 + struct.unpack("<i", b[1:5])[0]
        return va

    def cstr(self, va):
        off = va - self.base
        if off < 0 or off + 4 > len(self.mem):
            return None
        raw = self.mem[off:off + 96].split(b"\x00")[0]
        if raw and all(32 <= c < 127 for c in raw):
            return raw.decode()
        return None

    def name_at(self, va):
        for c in (va, self.resolve(va)):
            if c in self.by_va:
                return self.by_va[c][0]
        return None

    def vtable(self, sym):
        va = self.exp[sym]
        out = []
        for k in range(1000):
            t = self.u32(va + 4 * k)
            if k and (va + 4 * k) in self.by_va and any(n.startswith("??_7") for n in self.by_va[va + 4 * k]):
                break
            if not self.is_code(t) and t not in self.imports and not (self.base <= t < self.base + len(self.mem)):
                break
            out.append(t)
        return out

    def sweep(self, va, limit=0x3000):
        """All instructions from va to the first int3 padding (the functions are contiguous)."""
        va = self.resolve(va)
        code = self.mem[va - self.base:va - self.base + limit]
        res = []
        for ins in self.md.disasm(code, va):
            if ins.mnemonic == "int3":
                break
            res.append(ins)
        return res

    def ret_bytes(self, va):
        """Bytes the callee pops (`ret N`), looking at the first ret of the function."""
        for ins in self.sweep(va, 0x400):
            if ins.mnemonic == "ret":
                return int(ins.op_str, 16) if ins.op_str else 0
        return 0


# ---------------------------------------------------------------------------------------------------
# signatures (MSVC decorated names)
# ---------------------------------------------------------------------------------------------------
def short_name(sym: str) -> str:
    m = re.match(r"\?([^@]+)@([^@]+)@@", sym)
    return m.group(1) if m else sym


def parse_params(sym: str):
    """[(type text, byte size)] of a decorated member function name (only the types the NetHandler uses)."""
    m = re.match(r"\?[^@]+@[^@]+@@[A-Z][A-Z][A-Z](.*)", sym)
    if not m:
        return []
    s = m.group(1)
    # return type
    if s.startswith("?A"):
        s = s[2:]
        s = re.sub(r"^[UV][^@]+@@", "", s)
    else:
        s = s[1:] if not s.startswith(("U", "V")) else re.sub(r"^[UV][^@]+@@", "", s)
    out = []
    while s and not s.startswith(("@Z", "XZ", "Z")):
        t = None
        for pat, name, size in ((r"^H", "INT", 4), (r"^K", "DWORD", 4), (r"^M", "FLOAT", 4), (r"^_N", "UBOOL8", 4),
                                (r"^PAG", "TCHAR*", 4), (r"^PAD", "char*", 4), (r"^PAE", "BYTE*", 4),
                                (r"^PAX", "void*", 4), (r"^PAV([^@]+)@@", None, 4), (r"^PAU([^@]+)@@", None, 4),
                                (r"^AAV([^@]+)@@", None, 4), (r"^AAU([^@]+)@@", None, 4),
                                (r"^V([^@]+)@@", None, None), (r"^U([^@]+)@@", None, None)):
            mm = re.match(pat, s)
            if mm:
                if name is None:
                    cls = mm.group(1)
                    if pat.startswith(("^V", "^U")):
                        size = STRUCT_SIZES.get(cls, 4)
                        name = cls + " (by value)"
                    else:
                        name = cls + ("&" if pat.startswith("^AA") else "*")
                t = (name, size)
                s = s[mm.end():]
                break
        if t is None:
            # a '0' / '1' back-reference to an earlier type: same type as that parameter (single digit index)
            if s[0].isdigit():
                idx = int(s[0])
                s = s[1:]
                refs = [o for o in out]
                t = refs[idx] if idx < len(refs) else ("?", 4)
            else:
                t = ("?" + s[:6], 4)
                break
        out.append(t)
    return out


def arg_layout(params):
    """{entry-relative byte offset: label} for the stack arguments of a thiscall virtual."""
    lab = {}
    off = 4
    for i, (t, size) in enumerate(params, 1):
        if t.startswith("FVector"):
            for j, c in enumerate("xyz"):
                lab[off + 4 * j] = "arg%d.%s" % (i, c)
        elif size and size > 4:
            for j in range(size // 4):
                lab[off + 4 * j] = "arg%d+0x%x" % (i, 4 * j)
        else:
            lab[off] = "arg%d" % i
        off += size or 4
    return lab


# ---------------------------------------------------------------------------------------------------
# value tracking
# ---------------------------------------------------------------------------------------------------
MEM_RE = re.compile(r"^(?:(dword|word|byte|qword) ptr )?\[(.*)\]$")


class Tracker:
    """Straight-line walk of one function (branches are not followed: the code is read in address order).
    Registers, frame slots and pushed arguments are small expression strings."""

    def __init__(self, eng: Engine, top_va: int):
        self.eng = eng
        self.top = {top_va, eng.resolve(top_va)}
        self.regs: dict[str, str] = {"ecx": "this"}
        self.stack: list[str] = []
        self.d = 0                 # esp - esp at entry
        self.ebp_base = None       # ebp - esp at entry once `mov ebp, esp` is seen
        self.frame: dict[int, str] = {}
        self.fpu: list[str] = []
        self.ntop = 0
        self.sends: list[dict] = []
        self.fmts: list[dict] = []
        self.loops = 0
        self.drift = False

    # -- operands ----------------------------------------------------------------------------
    def slot(self, inner: str):
        """Entry-relative frame offset of `esp + X` / `ebp + X` / `ebp - X`, or None."""
        m = re.fullmatch(r"(esp|ebp)(?:\s*([+-])\s*(0x[0-9a-f]+|\d+))?", inner.strip())
        if not m:
            return None
        disp = int(m.group(3), 0) if m.group(3) else 0
        if m.group(2) == "-":
            disp = -disp
        if m.group(1) == "esp":
            return self.d + disp
        if self.ebp_base is None:
            return None
        return self.ebp_base + disp

    def addr_text(self, inner: str):
        def rep(m):
            v = self.regs.get(m.group(0))
            return v if v and len(v) < 30 and not v.startswith("#") else m.group(0)
        return re.sub(r"\be[a-d]x\b|\be[sd]i\b|\bebp\b", rep, inner)

    def val(self, op: str) -> str:
        op = op.strip()
        if op in REG32:
            return self.regs.get(op, op)
        if op in SUBREG:
            v = self.regs.get(SUBREG[op], SUBREG[op])
            return ("byte(%s)" if op.endswith("l") else "word(%s)") % v
        if re.fullmatch(r"-?0x[0-9a-f]+|-?\d+", op):
            return op
        m = MEM_RE.match(op)
        if m:
            kind, inner = m.group(1), m.group(2)
            sl = self.slot(inner)
            if sl is not None:
                if sl in self.frame:
                    return self.frame[sl]
                if sl >= 4:
                    return "A%x" % sl           # a stack argument of the virtual (entry offset)
                return "L%d" % (-sl)            # an unset local
            txt = "[%s]" % self.addr_text(inner)
            if kind in ("byte", "word") and not txt.startswith("[this"):
                return "%s(%s)" % (kind, txt)
            return txt
        return op

    def set_reg(self, r, v):
        self.regs[r] = v

    # -- instruction step --------------------------------------------------------------------
    def step(self, ins):
        m, ops = ins.mnemonic, ins.op_str
        parts = [p.strip() for p in re.split(r",(?![^\[]*\])", ops)] if ops else []
        if m == "push":
            self.stack.append(self.val(ops))
            self.d -= 4
        elif m == "pop":
            if self.stack:
                self.stack.pop()
            self.d += 4
            self.set_reg(ops, "#")
        elif m in ("add", "sub") and parts and parts[0] == "esp":
            n = int(parts[1], 16) if parts[1].startswith("0x") else int(parts[1])
            if m == "add":
                self.d += n
                for _ in range(n // 4):
                    if self.stack:
                        self.stack.pop()
            else:
                self.d -= n
        elif m == "mov" and len(parts) == 2:
            dst, src = parts
            if dst == "ebp" and src == "esp":
                self.ebp_base = self.d
            elif dst == "esp":
                m2 = MEM_RE.match(src)
                self.drift = True
            elif dst in REG32:
                self.set_reg(dst, self.val(src))
            elif dst in SUBREG:
                full = SUBREG[dst]
                self.set_reg(full, self.val(src) if dst.endswith("l") else "#")
            else:
                mm = MEM_RE.match(dst)
                if mm:
                    sl = self.slot(mm.group(2))
                    if sl is not None and (mm.group(1) in (None, "dword")):
                        self.frame[sl] = self.val(src)
        elif m.startswith("set") and parts and parts[0] in SUBREG:
            self.set_reg(SUBREG[parts[0]], "flag(%s)" % m[3:])
        elif m in ("movzx", "movsx") and len(parts) == 2:
            v = self.val(parts[1])
            self.set_reg(parts[0], v)
        elif m == "xor" and len(parts) == 2 and parts[0] == parts[1]:
            self.set_reg(parts[0], "0")
        elif m == "lea" and len(parts) == 2:
            inner = MEM_RE.match(parts[1]).group(2) if MEM_RE.match(parts[1]) else parts[1]
            sl = self.slot(inner)
            self.set_reg(parts[0], ("&frame%+d" % sl) if sl is not None else "&[%s]" % self.addr_text(inner))
        elif m == "fld" and parts:
            self.fpu.append("float(%s)" % self.val(parts[0]))
        elif m in ("inc", "dec", "add", "sub", "imul", "shl", "shr", "sar", "and", "or", "neg", "not") and parts \
                and parts[0] in REG32:
            self.set_reg(parts[0], "#(%s %s)" % (self.regs.get(parts[0], parts[0]), m + " " + ",".join(parts[1:])))
        elif m in ("jl", "jle", "jg", "jge", "jne", "je", "jmp", "jb", "ja", "jbe", "jae", "js", "jns"):
            mm = re.fullmatch(r"0x([0-9a-f]+)", ops)
            if mm and int(mm.group(1), 16) <= ins.address:
                self.loops += 1
        elif m == "call":
            self.do_call(ins, ops)

    def do_call(self, ins, ops):
        mm = re.fullmatch(r"0x([0-9a-f]+)", ops)
        tgt = int(mm.group(1), 16) if mm else None
        if tgt is not None and (tgt in self.top or self.eng.resolve(tgt) in self.top):
            self.set_reg("eax", "P%d" % self.ntop)
            self.ntop += 1
            for r in ("ecx", "edx"):
                self.set_reg(r, "#")
            return
        if tgt is not None and self.eng.resolve(tgt) == FTOL:
            self.set_reg("eax", "ftol(%s)" % (self.fpu.pop() if self.fpu else "?"))
            return
        if tgt is not None and self.eng.resolve(tgt) == FORMAT_TO:
            items = list(reversed(self.stack))     # [sock, buf, size, fmt, args...]
            fmt = fmt_of(self.eng, items[3]) if len(items) > 3 else None
            need = arg_count(fmt, 0) if fmt else 0
            self.fmts.append({"va": ins.address, "fmt": fmt, "args": items[4:4 + need]})
            self.cleanup_cdecl(None)
            self.set_reg("eax", "#")
            return
        mm = re.fullmatch(r"dword ptr \[(e[a-d]x|e[sd]i|ebp)\s*\+\s*(0x[0-9a-f]+)\]", ops)
        if mm and int(mm.group(2), 16) == SEND_SLOT:
            items = list(reversed(self.stack))     # [this, fmt, opcode, args...] (top of stack first)
            self.sends.append({"va": ins.address, "items": items})
            self.set_reg("eax", "#")
            return
        # any other call: assume the callee pops its own stack arguments if it is a known `ret N` function
        if tgt is not None:
            n = self.eng.ret_bytes(tgt)
            for _ in range(n // 4):
                if self.stack:
                    self.stack.pop()
            self.d += n
        for r in ("eax", "ecx", "edx"):
            self.set_reg(r, "#")

    def cleanup_cdecl(self, n):
        pass


def fmt_of(eng: Engine, s: str):
    if s and re.fullmatch(r"0x[0-9a-f]+", s):
        c = eng.cstr(int(s, 16))
        if c and re.fullmatch(r"[A-Za-z]+", c):
            return c
    return None


def arg_count(fmt: str, skip_opcode: int = 1):
    """Number of stack values the writer consumes for the fields of `fmt` (b/x/f take two)."""
    n = 0
    for ch in fmt:
        n += {"b": 2, "x": 2, "f": 2}.get(ch, 1) if ch in "cdhfSsbx" else 0
    return n - skip_opcode


def label_args(args, layout):
    out = []
    for a in args:
        mm = re.fullmatch(r"A([0-9a-f]+)", a)
        if mm:
            out.append(layout.get(int(mm.group(1), 16), a))
            continue
        out.append(re.sub(r"\bA([0-9a-f]+)\b", lambda m: layout.get(int(m.group(1), 16), m.group(0)), a))
    return out


def analyse(eng: Engine, top_va: int, va: int, sym: str):
    ins_list = eng.sweep(va)
    tr = Tracker(eng, top_va)
    for ins in ins_list:
        tr.step(ins)
    layout = arg_layout(parse_params(sym))
    sends = []
    for s in tr.sends:
        items = s["items"]
        if len(items) < 3:
            continue
        this, fmt_s, opc = items[0], items[1], items[2]
        fmt = fmt_of(eng, fmt_s)
        if fmt is None:
            sends.append({"va": s["va"], "error": "no format string at %s" % fmt_s})
            continue
        need = arg_count(fmt)
        args = label_args(items[3:3 + need], layout)
        # the receiver of Send(): UNetworkHandler+0x3C is the game server socket, +0x40 the login server's
        channel = "login" if "0x40]" in this else "game"
        sends.append({"va": s["va"], "fmt": fmt, "opcode": label_args([opc], layout)[0], "args": args,
                      "short": len(items[3:3 + need]) < need, "channel": channel})
    fmts = [{"va": f["va"], "fmt": f["fmt"], "args": label_args(f["args"], layout)} for f in tr.fmts]
    return ins_list, tr, sends, fmts


def op_key(opcode: str, fmt: str, args: list[str]):
    """'45', 'CF:0001' (opcode + h sub-opcode), '39:00' (opcode + c sub-opcode)."""
    try:
        o = int(opcode, 0)
    except ValueError:
        return None
    key = "%02X" % o
    if len(fmt) > 1 and fmt[1] in "ch" and args:
        try:
            sub = int(args[0], 0)
        except ValueError:
            return key
        if o in (0xCF, 0x39):
            return "%s:%0*X" % (key, 4 if fmt[1] == "h" else 2, sub)
    return key


def collect(eng: Engine, name_filter="", verbose=False):
    vt = eng.vtable(VTABLE_SYM)
    top_va = eng.exp[TOP_SYM]
    out = {}
    for k, t in enumerate(vt):
        if k < FIRST_SENDER_SLOT:
            continue
        sym = eng.name_at(t) or "sub_%08x" % t
        nm = short_name(sym)
        if name_filter and name_filter.lower() not in nm.lower():
            continue
        ins_list, tr, sends, fmts = analyse(eng, top_va, t, sym)
        out[nm] = {"slot": k, "vofs": 4 * k, "va": "0x%x" % eng.resolve(t), "rva": "0x%x" % (eng.resolve(t) - eng.base),
                   "sym": sym, "params": [p[0] for p in parse_params(sym)], "sizes": [p[1] or 4 for p in parse_params(sym)], "tops": tr.ntop, "loops": tr.loops,
                   "sends": sends, "item_fmts": fmts, "ninsn": len(ins_list)}
    return vt, out


# ---------------------------------------------------------------------------------------------------
# the Lucera server and the netpro spec
# ---------------------------------------------------------------------------------------------------
def server_handler(root: Path, handler: Path | None = None):
    """{(opcode, sub): [(state, class)]} from java/l2/gameserver/network/l2/GamePacketHandler.java.
    `sub` is the word that follows an extended prefix (0xD0, and 0xCF once the server handles the C3 one) or None.
    The labels of a nested switch are indented deeper than the state's own switch."""
    path = handler or root / "java" / "l2" / "gameserver" / "network" / "l2" / "GamePacketHandler.java"
    state = None
    out: dict = {}
    case = None
    top = None
    for l in path.read_text(encoding="utf-8").splitlines():
        m = re.match(r"\s*case (CONNECTED|AUTHED|IN_GAME):", l)
        if m:
            state, case, top = m.group(1), None, None
            continue
        if state is None:
            continue
        indent = len(l) - len(l.lstrip())
        m = re.match(r"\s*case (\d+)(?::| ->)", l)
        if m:
            case = (int(m.group(1)), indent)
            if indent <= 28:
                top = int(m.group(1))
        m = re.search(r"var3 = new (\w+)\(", l)
        if m and case is not None:
            op, ind = case
            key = (op, None) if ind <= 28 else (top, op)
            out.setdefault(key, []).append((state, m.group(1)))
            case = None
    return out


def server_reads(root: Path, cls: str):
    """The read* calls of readImpl() in source order, and whether the body has loops / conditions."""
    path = root / "java" / "l2" / "gameserver" / "network" / "l2" / "c2s" / (cls + ".java")
    if not path.exists():
        return None, False
    txt = path.read_text(encoding="utf-8")
    m = re.search(r"void readImpl\(\)[^{]*\{", txt)
    if not m:
        return "", False
    i, depth = m.end(), 1
    while i < len(txt) and depth:
        depth += {"{": 1, "}": -1}.get(txt[i], 0)
        i += 1
    body = txt[m.end():i - 1]
    seq = "".join(x.group(1).upper() for x in re.finditer(r"\bread([DCHSBQF])\(", body))
    variable = bool(re.search(r"\b(for|while|if)\b|\?", body))
    return seq, variable


def spec_names(path: Path):
    """{opcode: name} of the client section of the netpro HTML (only the names, no field list)."""
    t = path.read_text(encoding="utf-8", errors="replace")
    t = re.sub(r"<script.*?</script>|<style.*?</style>", "", t, flags=re.S)
    lines = [x.strip() for x in html.unescape(re.sub(r"<[^>]+>", "\n", t)).splitlines() if x.strip()]
    out = {}
    if "Client" not in lines:
        return out
    i = lines.index("Client") + 1
    while i < len(lines) - 1 and lines[i] != "Server":
        if re.fullmatch(r"[0-9A-F]{2}", lines[i]):
            out[lines[i]] = lines[i + 1]
            i += 2
        else:
            i += 1
    return out


def norm(n: str):
    n = re.sub(r"[^a-z0-9]", "", n.lower())
    for p in ("request", "send", "reply"):
        if n.startswith(p) and len(n) > len(p) + 2:
            n = n[len(p):]
    return n.replace("aquire", "acquire").replace("withdrawal", "withdraw").replace("withdrawl", "withdraw")


# client name / spec name -> server classes that do the same job under another name (reviewed in the Java source;
# a name that is not here and does not normalise to the server's is reported as an OPCODE MISMATCH)
ALIAS = {
    "SendLogOutPacket": ["Logout"], "SendLogOut": ["Logout"],
    "RequestGameStart": ["CharacterSelected"], "RequestCharacterCreate": ["CharacterCreate"],
    "RequestCharacterDelete": ["CharacterDelete"], "RequestCharacterRestore": ["CharacterRestore"],
    "RequestNewCharacter": ["NewCharacter"], "RequestEnterWorldPacket": ["EnterWorld"], "RequestEnterWorld": ["EnterWorld"],
    "RequestAttack": ["AttackRequest"], "RequestUseItem": ["UseItem"], "RequestTrade": ["TradeRequest"],
    "SendAppearingPacket": ["Appearing"], "SendAppearing": ["Appearing"],
    "CanNotMoveAnymore": ["CannotMoveAnymore"], "CanNotMoveAnymore(Vehicle)": ["CannotMoveAnymoreInVehicle"],
    "StartRotating": ["StartRotatingC"], "FinishRotating": ["FinishRotatingC"], "Say2": ["Say2C"],
    "RequestTargetCancel": ["RequestTargetCanceld"], "RequestSkillList": ["RequestSkillList"],
    "RequestShowboard": ["RequestShowBoard"], "RequestBBSWrite": ["RequestBBSwrite"],
    "RequestAcquireSkillInfo": ["RequestAquireSkillInfo"], "RequestAcquireSkill": ["RequestAquireSkill"],
    "RequestDestroyQuest": ["RequestQuestAbort"], "RequestGMList": ["RequestGmList"],
    "GetOnVehicle": ["RequestGetOnVehicle"], "GetOffVehicle": ["RequestGetOffVehicle"],
    "RequestOpenMinimap": ["RequestShowMiniMap"], "RequestReviveReply": ["RequestRestartPoint"],
    "RequestObserverEndPacket": ["RequestObserverEnd"], "GMSnoopEnd": ["SnoopQuit"],
    "RequestTutorialQuestionMarkPressed": ["RequestTutorialQuestionMark"],
    "RequestWithDrawalPledge": ["RequestWithdrawalPledge"], "MoveToLocationInVehicle": ["RequestMoveToLocationInVehicle"],
    "RequestPartyMatchConfig": ["RequestPartyMatchConfig"], "RequestPartyMatchList": ["RequestPartyMatchList"],
    "RequestPartyMatchDetail": ["RequestPartyMatchDetail"], "SendProtocolVersion": ["ProtocolVersion"],
    "RequestLogin": ["AuthLogin"], "RequestFriendInfoList": ["RequestFriendList"],
    "RequestVoteSociality": ["RequestVoteNew"], "VoteSociality": ["RequestVoteNew"],
    # private stores: the Lucera sources name the Sell / Buy families the other way round at 0x79 / 0x90, the code
    # behind each opcode is the C3 meaning (checked in runImpl: 0x73 -> PrivateStoreManageListSell, 0x79 -> the purchase
    # from a sell store, 0x90 -> PrivateStoreManageListBuy, 0x96 -> the sale to a buy store)
    "RequestPrivateStoreManage": ["RequestPrivateStoreSell"], "RequestPrivateStoreSellManageList": ["RequestPrivateStoreSell"],
    "RequestPrivateStoreQuit": ["RequestPrivateStoreQuitSell"], "RequestPrivateStoreSellQuit": ["RequestPrivateStoreQuitSell"],
    "SetPrivateStoreMsg": ["SetPrivateStoreMsgSell"], "SetPrivateStoreSellMsg": ["SetPrivateStoreMsgSell"],
    "SetPrivateStoreList": ["SetPrivateStoreSellList"], "SetPrivateStoreSellList": ["SetPrivateStoreSellList"],
    "SendPrivateStoreBuyList": ["RequestPrivateStoreManageBuy"],
    "RequestPrivateStoreBuyManage": ["RequestPrivateStoreBuy"], "RequestPrivateStoreBuyManageList": ["RequestPrivateStoreBuy"],
    "RequestPrivateStoreBuyQuit": ["RequestPrivateStoreQuitBuy"], "RequestPrivateStoreBuyManageQuit": ["RequestPrivateStoreQuitBuy"],
    "SetPrivateStoreBuyMsg": ["SetPrivateStoreMsgBuy"],
    "SendPrivateStoreBuyBuyList": ["RequestPrivateStoreBuySellList"], "SendPrivateStoreSellList": ["RequestPrivateStoreBuySellList"],
    "RequestHennaItemList": ["RequestHennaList"], "RequestHennaUnequipList": ["RequestHennaUnequipList"],
}


def match_server(name: str, spec: str | None, classes: list[str]):
    """True when one server class at the opcode is the client's packet (same normalised name, same spec name or a
    reviewed alias)."""
    for c in classes:
        for cand in (name, spec):
            if cand and (norm(c) == norm(cand) or c in ALIAS.get(cand, [])):
                return True
    return False


def compare_layout(fmt: str, key: str, server_seq: str | None, variable: bool):
    """Client fields after the opcode (and sub-opcode) against the server's readImpl() reads."""
    body = fmt[1:]
    if key.startswith("CF:") or key.startswith("39:"):
        body = body[1:]
    cl = "".join({"c": "C", "h": "H", "d": "D", "f": "F", "S": "S", "s": "S", "b": "B", "x": "B"}.get(ch, "?") for ch in body)
    head = cl.split("B")[0] if "B" in cl else cl
    sv = server_seq or ""
    if sv == cl and not variable:
        return "equal"
    if variable:
        return "server has loops/conditions (client %s, server reads %s): manual" % (cl, sv)
    if sv.startswith(cl):
        return "server reads MORE than the client sends (client %s, server %s)" % (cl, sv)
    if cl.startswith(sv):
        return "client sends more than the server reads (tolerated) (client %s, server %s)" % (cl, sv)
    return "DIFFERENT (client %s, server %s)" % (cl, sv)


# what each wire field is (after the opcode), where the Engine code and the server's readers make it clear. Keys are
# the UNetworkHandler slot names. P0, P1.. are the L2ParamStack values in the order NWindow pushes them.
FIELD_NAMES = {
    "RequestActionUse": ["actionId (P0)", "ctrl / force (P1)", "shift / dont-move (P2, byte)"],
    "ChangeMoveType": ["isRun: 0 walk, 1 run (arg1)"],
    "ChangeWaitType": ["isStand: 0 sit, 1 stand (arg1)"],
    "RequestTargetCancel": [],
    "Action": ["objectId (arg1)", "originX", "originY", "originZ (arg2, floats -> int)", "actionId: 0 click, 1 shift-click (arg3, byte)"],
    "Attack": ["objectId (arg1)", "originX", "originY", "originZ (arg2)", "shift (arg3, byte)"],
    "MoveBackwardToLocation": ["targetX", "targetY", "targetZ (arg1)", "originX", "originY", "originZ (arg2)"],
    "MoveToLocation": ["targetX", "targetY", "targetZ (arg2)", "originX", "originY", "originZ (arg3)",
                       "moveMovement: 1 when arg5 == 0 (the 7th dword is flag(e) = sete)"],
    "ValidatePosition": ["x", "y", "z (arg1)", "heading (arg2)", "vehicleId (arg3)"],
    "Say2": ["text (P1)", "chatType (P0)"],
    "RequestItemList": [],
    "RequestUseItem": ["objectId (P0)", "ctrl (P1)"],
    "RequestSkillItem": [],
    "RequestMagicItem": [],
    "RequestMagicSkillUse": ["skillId (P0)", "ctrl (P1)", "shift (P2, byte)"],
    "RequestShortCutReg": ["type (P0)", "slot (P1)", "id (P2)", "level (P3)"],
    "RequestPrivateStoreManage": [],
    "SetPrivateStoreList": ["count (P0)", "[count x ddd: objectId, count, price]"],
    "RequestPrivateStoreManageCancel": [],
    "RequestPrivateStoreQuit": [],
    "SetPrivateStoreMsg": ["message"],
    "SendPrivateStoreBuyList": ["storeOwnerId (P0)", "count (P1)", "[count x ddd: objectId, count, price]"],
    "RequestPrivateStoreBuyManage": [],
    "SetPrivateStoreBuyList": ["count (P0)", "[count x dhdd: itemId, enchant(h), count, price]"],
    "RequestPrivateStoreBuyManageCancel": [],
    "RequestPrivateStoreBuyQuit": [],
    "SetPrivateStoreBuyMsg": ["message"],
    "SendPrivateStoreBuyBuyList": ["storeOwnerId (P0)", "count (P1)", "[count x ddhdd: objectId, itemId, enchant(h), count, price]"],
    "RequestPrivateStoreList": ["storeOwnerId"],
    "RequestPrivateStoreBuyList": ["storeOwnerId"],
    "RequestSellItem": ["listId (P0)", "count (P1)", "[count x ddd: objectId, itemId, count]"],
    "RequestBuyItem": ["listId (P0)", "count (P1)", "[count x dd: itemId, count]"],
    "RequestBypassToServer": ["command"],
    "RequestLinkHtml": ["link"],
    "RequestAcquireSkillInfo": ["skillId (P0)", "level (P1)"],
    "RequestAcquireSkill": ["skillId (P0)", "level (P1)"],
    "RequestJoinPledge": ["targetObjectId (P0)"],
    "RequestCreatePledge": ["clanName"],
    "RequestJoinParty": ["targetName (P0)", "lootRule (P1)"],
    "RequestRestartPoint": ["point (P0)"],
    "ReviveReply": ["answer (P0)"],
    "SendLogOutPacket": [],
    "RequestRestart": [],
    "RequestGameStart": ["slot (arg1)"],
    "RequestEnterWorldPacket": ["5 dwords (arg1..arg5)"],
}
# human notes that a table cannot say
NOTES = {
    "RequestPetitionCancel": "format 'cS' but no value is pushed: Send() reads a stack slot past its arguments, so a garbage wide string follows the opcode (harmless: the server reads nothing)",
    "FinishRotating": "format 'cdd' but only one value is pushed: the second dword is the caller's saved ESI (garbage, unused by the server)",
    "RequestHennaItemList": "format 'cd' with no value pushed: the dword is garbage (the server ignores it)",
    "RequestHennaUnequipList": "format 'cd' with no value pushed: the dword is garbage (the server ignores it)",
    "RequestDismissPartyRoom": "format 'chdd': the last dword is the saved ESI (garbage)",
    "RequestWithdrawPartyRoom": "format 'chdd': the last dword is the saved ESI (garbage)",
    "RequestMagicSkillList": "the format string is 'ddd' (not 'cdd'): the OPCODE IS SENT AS A DWORD, 2E 00 00 00, then two dwords; the spec calls 0x2E RequestMagicSkillList, no NWindow caller",
    "RequestSkillItem": "this is the packet the spec calls RequestSkillList (0x3F): the skill window opening asks for the skill list",
    "RequestMagicItem": "the spec calls 0x3E RequestMagicList; no NWindow caller",
    "RequestAttack": "sends opcode 0x04 (Action) as 'cdddd', WITHOUT the final action byte the other Action senders write; no NWindow caller",
    "RequestEquipItem": "no NWindow caller (items are equipped with UseItem 0x14)",
    "MoveToLocation": "two senders of the same shapes: opcode 0x01 (ground click, 7 dwords) and, when the pawn is on a vehicle, opcode 0x5C MoveToLocationInVehicle (vehicle id + 6 dwords)",
    "MoveBackwardToLocation": "opcode 0x01 with 6 dwords (no moveMovement), the same opcode MoveToLocation writes with 7; no NWindow caller (the Engine's own controller calls both)",
    "CanNotMoveAnymore": "two senders: opcode 0x36 (x,y,z,heading) and, on a vehicle, 0x5D (vehicle id, x,y,z, heading)",
    "Say2": "two shapes: 'cSd' (text, type) and, for a whisper (type 2), 'cSdS' (text, 2, target); nothing is sent when the text is empty",
    "RequestBlock": "P0 = type 0..4: types 0 (block) and 1 (unblock) send 'cdS' (type, name = P1); types 2, 3, 4 send 'cd' (type only)",
    "RequestPledgePower": "two shapes: 'cddb' with a 0x20 byte privilege block and 'cdd'",
    "RequestSetPledgeCrest": "two shapes: 'cd' (size 0, remove) and 'cdb' (size, crest bitmap)",
    "RequestMakeMacro": "header cdSSSccb, then the entries as 'ccdcS' (entry no., type, d1, d2/shortcut, command)",
    "RequestTeleport": "two shapes (2 or 3 values); no NWindow caller",
    "RequestMultiSellChoose": "listId, entryId only: C3 has no amount field",
    "RequestJoinPartyRoom": "C3 sends only the room id",
}
# Send() sites that are not UNetworkHandler slots (found by scanning for `push fmt` + `call [reg+0x64]`)
EXTRA_SENDS = [
    {"name": "(connect) ProtocolVersion", "fmt": "cdb", "opcode": "0x0", "engine_va": "0x103eb877",
     "channel": "game", "fields": ["protocolVersion (arg)", "length of the block", "pointer to the block (XOR-scrambled string)"],
     "note": "sent once after the connection to the game server is made"},
    {"name": "(connect) AuthLogin", "fmt": "cSddddd", "opcode": "0x8", "engine_va": "0x103f6526", "channel": "game",
     "fields": ["account name (global 0x1097c8cc)", "[0x1097c8c4]", "[0x1097c8c8]", "[0x1097c8ac]", "[0x1097c8b0]", "[[0x1423a8d4]]"],
     "note": "session keys of the login server; the account name is a wide string"},
]
TYPE_OF = {"d": "INT", "c": "INT (byte)", "h": "INT (word)", "S": "TCHAR*", "s": "char*", "f": "double"}


def describe_params(r: dict, s: dict, names: list[str]):
    """['P0 INT actionId', ...]: the L2ParamStack contents (or the stack arguments) in the order they are pushed."""
    out = {}
    fmt = s["fmt"][1:]
    ai = 0
    for ch in fmt:
        if ch in "bx":
            ai += 2
            continue
        a = s["args"][ai] if ai < len(s["args"]) else None
        ai += 1
        m = re.fullmatch(r"(?:byte\()?(P\d+)\)?", a or "")
        if m:
            nm = names[len(out)] if len(out) < len(names) else ""
            out[m.group(1)] = "%s %s %s" % (m.group(1), TYPE_OF.get(ch, "?"), nm)
    for f in r.get("item_fmts", []):
        for a in f["args"]:
            if re.fullmatch(r"P\d+", a):
                out.setdefault(a, "%s INT (list item field)" % a)
    return [out[k].strip() for k in sorted(out, key=lambda x: int(x[1:]))]


def certainty_of(r: dict, s: dict):
    """high: format, opcode and every value traced to a stack argument, a constant or a Top() result. The two values of
    a block (`b`: length, pointer) are the list the loop built, described by the item format, not traced."""
    args = []
    i = 0
    for ch in s["fmt"][1:]:
        if ch in "bxf":
            i += 2
            continue
        if i < len(s["args"]):
            args.append(s["args"][i])
        i += 1
    bad = s.get("short") or any(re.search(r"^#|^L-?\d|^e[a-d]x$|^e[sd]i$|^ebp$|^\[", a) for a in args)
    return "medium" if bad else "high"


def attach_server(entry: dict, key: str, nm: str, fmt: str, srv, root):
    """server_class / server_opcode_matches / server_reads of the Lucera handler for this opcode. The C3 client's
    extended packets are opcode 0xCF + word; the Lucera (Interlude) server's are 0xD0 + word, and its 0xCF is
    RequestReload: `server_d0_class` tells what the same sub-opcode would be under 0xD0."""
    op = int(key.split(":")[0], 16)
    sub = int(key.split(":")[1], 16) if ":" in key and op == 0xCF else None
    if op == 0xCF:
        hits = srv.get((0xCF, sub), []) or []          # a server that already knows the C3 prefix
        d0 = srv.get((208, sub), [])
        entry["server_d0_class"] = d0[0][1] if d0 else None
    else:
        hits = srv.get((op, None), []) or []
    classes = [c for _, c in hits]
    entry["server_class"] = classes[0] if len(classes) == 1 else (classes or None)
    entry["server_states"] = [st for st, _ in hits]
    entry["server_opcode_matches"] = (match_server(nm, entry.get("spec_name"), classes) if classes else None)
    if op == 0xCF and not classes:
        entry["server_opcode_matches"] = False         # the C3 client's 0xCF + word has no handler (Interlude: 0xD0)
    if classes and root is not None:
        seq, var = server_reads(root, classes[0])
        entry["server_reads"] = seq
        entry["server_reads_variable"] = var
        entry["layout_check"] = compare_layout(fmt, key, seq, var)


def build_table(res: dict, srv, spec, root: Path | None):
    """analysis/c3_c2s_formats.json: {"45": {...}} (extended: "CF:0001", "39:00"; the login server's packets are
    "LOGIN:00" ...); a second variant with the same key goes in "variants" of the first entry."""
    table: dict = {}
    for nm, r in res.items():
        for s in r["sends"]:
            if "fmt" not in s:
                continue
            key = op_key(s["opcode"], s["fmt"], s["args"])
            if key is None:
                continue
            if s.get("channel") == "login":
                key = "LOGIN:" + key
            names = FIELD_NAMES.get(nm, [])
            entry = {"name": nm, "fmt": s["fmt"], "channel": s.get("channel", "game"),
                     "netHandler_vslot": r["slot"], "netHandler_vofs": "0x%x" % r["vofs"],
                     "engine_rva": r["rva"], "send_site": "0x%x" % s["va"],
                     "signature": r["params"], "params": describe_params(r, s, [n.split(" (")[0] for n in names if "(P" in n]),
                     "fields": s["args"], "field_names": names,
                     "item_fmt": r["item_fmts"][0]["fmt"] if r["item_fmts"] else None,
                     "item_args": r["item_fmts"][0]["args"] if r["item_fmts"] else None,
                     "nwindow_stub": r.get("nwindow_stub"), "nwindow_callers": r.get("nwindow_callers", []),
                     "nwindow_detail": r.get("nwindow_detail", []), "certainty": certainty_of(r, s)}
            if nm in NOTES:
                entry["notes"] = NOTES[nm]
            if spec is not None and not key.startswith("LOGIN:"):
                entry["spec_name"] = spec.get(key.split(":")[0])
            if key.startswith("LOGIN:"):
                entry["server_class"] = None
                entry["server_opcode_matches"] = None
                entry["notes"] = (entry.get("notes", "") + " login server protocol, not the game server's").strip()
            elif srv is not None:
                attach_server(entry, key, nm, s["fmt"], srv, root)
            if key in table:
                table[key].setdefault("variants", []).append(entry)
            else:
                table[key] = entry
    # Send() calls outside the vtable (the connect sequence)
    for x in EXTRA_SENDS:
        key = "%02X" % int(x["opcode"], 16)
        entry = {"name": x["name"], "fmt": x["fmt"], "channel": x["channel"], "netHandler_vslot": None,
                 "engine_va": x["engine_va"], "fields": x["fields"], "notes": x["note"], "nwindow_callers": [],
                 "certainty": "high", "spec_name": (spec or {}).get(key)}
        if srv is not None:
            attach_server(entry, key, x["name"], x["fmt"], srv, root)
        if key in table:
            table[key].setdefault("variants", []).append(entry)
        else:
            table[key] = entry
    return dict(sorted(table.items()))


# ---------------------------------------------------------------------------------------------------
# the stock NWindow.dll (Ghidra export): who calls each NetHandler slot and what it pushes
# ---------------------------------------------------------------------------------------------------
REGS = ("EAX", "EBX", "ECX", "EDX", "ESI", "EDI", "EBP")
ASM_LINE = re.compile(r"^([0-9a-f]{8})\s+([0-9a-f]+)\s+([A-Z][A-Z0-9]*)\s*(.*?)(?:\s*;\s*(.*))?$")


def load_asm(path: Path):
    ins = []
    for l in path.read_text(encoding="utf-8", errors="replace").splitlines():
        m = ASM_LINE.match(l)
        if m:
            ins.append((int(m.group(1), 16), m.group(3), m.group(4).strip(), (m.group(5) or "").strip()))
    return ins


def split_ops(ops: str):
    return [p.strip() for p in re.split(r",(?![^\[]*\])", ops)] if ops else []


def nwindow_names(root: Path):
    names = {}
    import csv
    for f in ("functions.csv", "names.csv"):
        p = root / "db" / f
        if not p.exists():
            continue
        for row in csv.reader(l for l in p.read_text(encoding="utf-8").splitlines() if l and not l.startswith("#")):
            if len(row) > 4 and re.fullmatch(r"[0-9a-f]{8}", row[0]):
                names.setdefault(int(row[0], 16), row[4] if f == "functions.csv" else row[1])
    return names


def scan_nwindow(root: Path, sender_ofs: dict[int, str], sigs: dict[int, tuple] | None = None):
    """{vofs: [caller dicts]} from the Ghidra export of the stock NWindow (analysis/ghidra/asm/*.asm)."""
    asm_dir = root / "analysis" / "ghidra" / "asm"
    names = nwindow_names(root)
    funcs = {}
    for p in sorted(asm_dir.glob("*.asm")):
        funcs[int(p.stem, 16)] = load_asm(p)
    # 1. the 14 byte stubs  mov eax,[ecx+0x48] / mov ecx,[eax+0x58] / mov edx,[ecx] / jmp [edx+ofs]
    stubs = {}
    for va, ins in funcs.items():
        if 3 <= len(ins) <= 5:
            t = [(m, o) for _, m, o, _ in ins]
            if (len(t) >= 4 and t[0][0] == "MOV" and "[ECX + 0x48]" in t[0][1] and "[EAX + 0x58]" in t[1][1]
                    and t[3][0] in ("JMP", "CALL")):
                mm = re.search(r"\[EDX \+ (0x[0-9a-f]+)\]", t[3][1])
                if mm and int(mm.group(1), 16) in sender_ofs:
                    stubs[va] = int(mm.group(1), 16)
    out: dict[int, list] = {}
    for va, ins in funcs.items():
        fname = names.get(va, "FUN_%08x" % va)
        cur: list | None = None        # the pushes of the L2ParamStack constructed last (code order)
        pb_regs: set[str] = set()      # registers that hold L2ParamStack::PushBack
        nethandler_regs: dict[str, int] = {}   # registers that hold Engine->NetHandler -> instruction index
        vtable_of: dict[str, str] = {}         # register -> the object register whose vtable it holds
        for i, (addr, m, ops, cm) in enumerate(ins):
            parts = split_ops(ops)
            if m == "MOV" and len(parts) == 2 and "PTR_PushBack" in cm:
                pb_regs.add(parts[0])
            # `mov reg,[x + 0x58]` right after `mov x,[y + 0x48]`: reg is Engine->NetHandler; `mov rv,[reg]` its vtable
            if m == "MOV" and len(parts) == 2 and parts[0] in REGS:
                if (re.fullmatch(r"dword ptr \[E[A-Z]{2} \+ 0x58\]", parts[1]) and i > 0
                        and any(ins[k][1] == "MOV" and "+ 0x48]" in ins[k][2] for k in range(max(0, i - 8), i))):
                    nethandler_regs[parts[0]] = i
                else:
                    nethandler_regs.pop(parts[0], None)
                mv = re.fullmatch(r"dword ptr \[(E[A-Z]{2})\]", parts[1])
                if mv:
                    vtable_of[parts[0]] = mv.group(1)
                else:
                    vtable_of.pop(parts[0], None)
            if m == "CALL" and "PTR_L2ParamStack" in cm:
                cur = []
            site_ofs = None
            via = None
            if m in ("CALL", "JMP"):
                mm = re.fullmatch(r"0x([0-9a-f]{8})", ops)
                if mm and int(mm.group(1), 16) in stubs:
                    site_ofs, via = stubs[int(mm.group(1), 16)], "stub %s" % names.get(int(mm.group(1), 16), "FUN_" + mm.group(1))
                else:
                    mm = re.search(r"\[E[A-Z]{2} \+ (0x[0-9a-f]+)\]", ops)
                    if mm and "ptr" in ops and int(mm.group(1), 16) in sender_ofs and va not in stubs:
                        reg = re.search(r"\[(E[A-Z]{2}) \+ 0x[0-9a-f]+\]", ops).group(1)
                        if vtable_of.get(reg) in nethandler_regs:
                            site_ofs, via = int(mm.group(1), 16), "inline"
            if m == "CALL" and ("PTR_PushBack" in cm or ops in pb_regs):
                # the value pushed just before (the stack object is ECX)
                for j in range(i - 1, max(-1, i - 5), -1):
                    if ins[j][1] == "PUSH":
                        if cur is not None:
                            cur.append("%08x: %s" % (ins[j][0], describe_push(ins, j, ins[j][2])))
                        break
            if site_ofs is not None:
                uses_stack, ndw = (sigs or {}).get(site_ofs, (True, 0))
                if uses_stack:
                    pushes = list(cur) if cur is not None else None
                else:
                    # plain stack arguments: the PUSHes right before the call, first argument first
                    pushes = []
                    for j in range(i - 1, max(-1, i - 14), -1):
                        if len(pushes) >= ndw:
                            break
                        if ins[j][1] == "PUSH":
                            pushes.append("%08x: %s" % (ins[j][0], describe_push(ins, j, ins[j][2])))
                        elif ins[j][1] in ("CALL", "JMP", "RET"):
                            break
                out.setdefault(site_ofs, []).append({
                    "function": fname, "va": "0x%08x" % va, "call_site": "0x%08x" % addr, "via": via,
                    "pushes": pushes})
    return out, stubs, names


def describe_push(ins, j, val):
    """The operand pushed at ins[j], with a register resolved to the last MOV into it."""
    if re.fullmatch(r"E[A-Z]{2}", val):
        for k in range(j - 1, max(-1, j - 8), -1):
            mk, ok = ins[k][1], split_ops(ins[k][2])
            if mk in ("MOV", "MOVZX", "MOVSX") and len(ok) == 2 and ok[0] == val:
                return "%s <- %s" % (val, ok[1])
            if mk in ("XOR",) and len(ok) == 2 and ok[0] == ok[1] == val:
                return "0"
            if mk == "CALL" or ok and ok[0] == val:
                break
    return val


def md_rows(table: dict):
    """Markdown rows: opcode | name | client format | NWindow callers (+ pushes) | server | certainty."""
    def callers(e):
        out = []
        for c in e.get("nwindow_detail", [])[:3]:
            pushes = c.get("pushes")
            if pushes is None:
                p = ""
            else:
                p = " [" + ", ".join(x.split(": ", 1)[1] if ": " in x else x for x in pushes) + "]"
            out.append("%s%s" % (c["function"], p))
        more = len(e.get("nwindow_detail", [])) - 3
        s = "; ".join(out) + (" (+%d)" % more if more > 0 else "")
        return s or "sem chamador no NWindow"
    rows = []
    for key, v in table.items():
        for e in [v] + v.get("variants", []):
            fm = e["fmt"] + ("  itens: " + e["item_fmt"] if e.get("item_fmt") else "")
            if e.get("channel") == "login":
                srv = "(login server)"
            elif e.get("server_class") is None:
                srv = "**sem handler**"
            else:
                sc = e["server_class"] if isinstance(e["server_class"], str) else "/".join(e["server_class"])
                srv = "%s lê `%s`%s%s" % (sc, e.get("server_reads", ""), " (condicional)" if e.get("server_reads_variable") else "",
                                          "" if e.get("server_opcode_matches") else " **OPCODE DIFERENTE**")
            rows.append("| %s | %s | `%s` | %s | %s | %s |" % (
                key, e["name"], fm, callers(e).replace("|", "/"), srv.replace("|", "/"), e.get("certainty", "")))
    return rows


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--engine", type=Path, default=DEFAULT_ENGINE, required=DEFAULT_ENGINE is None, help="the C3 client's Engine.dll (default: $NWD_C3_SYSTEM/Engine.dll)")
    ap.add_argument("--json", type=Path, help="write the packet table here (analysis/c3_c2s_formats.json)")
    ap.add_argument("--name", default="", help="only slots whose name contains this (case-insensitive)")
    ap.add_argument("--trace", action="store_true", help="print the tracked values of every send site")
    ap.add_argument("--list", action="store_true", help="list the UNetworkHandler vtable and exit")
    ap.add_argument("--server", type=Path, nargs="?", const=DEFAULT_SERVER, help="cross-check with the Java server")
    ap.add_argument("--spec", type=Path, nargs="?", const=DEFAULT_SPEC, help="cross-check with the netpro spec")
    ap.add_argument("--nwindow", action="store_true", help="find the NWindow callers (needs analysis/ghidra)")
    ap.add_argument("--handler", type=Path, help="read the opcode table from this GamePacketHandler.java instead (e.g. the pre-audit backup)")
    ap.add_argument("--md", type=Path, help="with --json: also write the markdown table rows here")
    a = ap.parse_args()

    eng = Engine(a.engine)
    if a.list:
        for k, t in enumerate(eng.vtable(VTABLE_SYM)):
            print("%3d +0x%03x %08x %s" % (k, 4 * k, eng.resolve(t), eng.name_at(t) or "sub_%08x" % t))
        return 0
    vt, res = collect(eng, a.name)
    if a.nwindow or a.json:
        ofs_map = {r["vofs"]: nm for nm, r in res.items()}
        sigs = {r["vofs"]: ("L2ParamStack&" in r["params"], sum((r.get("sizes") or [0])) // 4) for r in res.values()}
        calls, stubs, names = scan_nwindow(ROOT, ofs_map, sigs)
        stub_of = {ofs: names.get(va, "FUN_%08x" % va) + "@0x%08x" % va for va, ofs in stubs.items()}
        for ofs, lst in calls.items():
            r = res[ofs_map[ofs]]
            r["nwindow_stub"] = stub_of.get(ofs)
            r["nwindow_detail"] = lst
            r["nwindow_callers"] = sorted({"%s@%s" % (c["function"], c["va"]) for c in lst})
        for ofs, nm in ofs_map.items():
            res[nm].setdefault("nwindow_stub", stub_of.get(ofs))
            res[nm].setdefault("nwindow_detail", [])
    for nm, r in res.items():
        if a.trace or a.name:
            print("== slot %d +0x%x %s @%s  params=%s Top()=%d loops=%d" % (r["slot"], r["vofs"], nm, r["va"], r["params"], r["tops"], r["loops"]))
            for s in r["sends"]:
                print("   send @%08x %s" % (s["va"], {k: v for k, v in s.items() if k != "va"}))
            for f in r["item_fmts"]:
                print("   item fmt @%08x %s" % (f["va"], {k: v for k, v in f.items() if k != "va"}))
        else:
            for s in r["sends"]:
                print("%3d %-34s %-14s %-5s %s" % (r["slot"], nm, s.get("fmt"), s.get("opcode"), s.get("args")))
    if a.json:
        srv = spec = None
        root = a.server or DEFAULT_SERVER
        if root and root.exists():
            srv = server_handler(root, a.handler)
        if (a.spec or DEFAULT_SPEC) and (a.spec or DEFAULT_SPEC).exists():
            spec = spec_names(a.spec or DEFAULT_SPEC)
        table = build_table(res, srv, spec, root if root and root.exists() else None)
        a.json.write_text(json.dumps(table, indent=1, ensure_ascii=False), encoding="utf-8")
        print("wrote", a.json, len(table), "keys")
        if a.md:
            a.md.write_text(chr(10).join(md_rows(table)) + chr(10), encoding="utf-8")
            print("wrote", a.md)
    return 0


if __name__ == "__main__":
    sys.exit(main())
