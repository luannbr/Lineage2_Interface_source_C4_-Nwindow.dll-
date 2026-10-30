#!/usr/bin/env python3
"""Packet formats of the C3 client's shop / store family, read from Engine.dll (lane P of the C3 packet audit).

How the C3 Engine.dll handles the packets (all verified in the disassembly):

 * s2c: UNetworkHandler::Tick (rva 0x105b10) calls  table[opcode].fn(netHandler, data)  with
   table = 0x108e6318 + opcode * 0x104 (extended 0xFE packets: 0x108f6618 + word * 0x104). Every entry is
   {fn pointer, 0x100 bytes name}; the init code fills them with `mov [table+op*0x104], fn` (+ a copy of the name).
   Each fn is a thunk (jmp) to the handler. The handler reads the packet with the FORMAT INTERPRETER (thunk at
   0x1030537b -> 0x103ebff0: interp(ctx, data, fmt, out pointers...) -> data after the fields) and then calls a
   FL2NetNotify virtual. Format characters: c byte, h word, d dword, f double, S wide string (2 outs: size, dest),
   s ansi string (2 outs), b block (2 outs). A packet shorter than its format reads past the buffer (GPF at the
   `d` case, Engine.dll+0EC053). Loops are code loops around an interpreter call.
 * c2s: UNetworkHandler::<Request>(...) calls  engine->vtable[0x64](engine, fmt, args...)  with the opcode as the
   first value ('c') and the fields after it; lists are built with `b` blocks (count, then N records formatted by an
   inner interpreter call).

usage: python tools/c3/p_family_formats.py [--engine PATH] [--s2c] [--c2s] [--only REGEX]
"""
from __future__ import annotations

import argparse
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
INTERP_THUNK = 0x1030537B
NORMAL_TABLE, EXT_TABLE, STRIDE = 0x108E6318, 0x108F6618, 0x104
# s2c opcodes of the shop / store family (name, opcode)
FAMILY_S2C = ["10", "11", "1E", "1F", "20", "21", "22", "5E", "74", "75", "7C", "9A", "9B", "9C", "B7", "B8", "B9", "C2", "C3", "D0",
              "D6", "D7", "D8", "D9", "DA", "DB", "E8", "E9", "EF", "F0"]
FAMILY_C2S = r"RequestTrade|RequestAddTradeItem|RequestTradeDone|AnswerTradeRequest|RequestSellItem|RequestBuyItem|RequestPreviewItem|" \
             r"RequestMultiSellChoose|RequestPackage.*|RequestRecipe.*|.*PrivateStore.*"


class Eng:
    def __init__(self, path: Path):
        self.pe = pefile.PE(str(path))
        self.base = self.pe.OPTIONAL_HEADER.ImageBase
        self.mem = bytes(self.pe.get_memory_mapped_image())
        self.md = Cs(CS_ARCH_X86, CS_MODE_32)
        text = next(s for s in self.pe.sections if s.Name.startswith(b".text"))
        self.t0, self.t1 = text.VirtualAddress, text.VirtualAddress + text.Misc_VirtualSize

    def cstr(self, va):
        r = va - self.base
        if not 0 <= r < len(self.mem):
            return None
        s = self.mem[r:r + 80]
        i = s.find(b"\0")
        if i > 0 and all(32 <= c < 127 for c in s[:i]):
            return s[:i].decode()
        return None

    def wstr(self, va):
        r = va - self.base
        out = ""
        for i in range(0, 200, 2):
            c = self.mem[r + i:r + i + 2]
            if c == b"\0\0":
                break
            out += c.decode("utf-16-le", "replace")
        return out

    def thunk(self, va):
        r = va - self.base
        if self.mem[r] == 0xE9:
            return va + 5 + struct.unpack("<i", self.mem[r + 1:r + 5])[0]
        return va

    def func_end(self, start):
        for ins in self.md.disasm(self.mem[start - self.base:start - self.base + 0x1800], start):
            if ins.mnemonic == "int3":
                return ins.address
        return start + 0x1800

    def s2c_table(self):
        tab, names = {}, {}
        for m in re.finditer(rb"\xc7\x05(....)(....)", self.mem[self.t0:self.t1], re.S):
            addr, val = struct.unpack("<II", m.group(1) + m.group(2))
            if NORMAL_TABLE <= addr < NORMAL_TABLE + 256 * STRIDE and (addr - NORMAL_TABLE) % STRIDE == 0:
                tab[(addr - NORMAL_TABLE) // STRIDE] = val
        for m in re.finditer(rb"\x68(....)\x68(....)", self.mem[self.t0:self.t1], re.S):
            nm, dst = struct.unpack("<II", m.group(1) + m.group(2))
            if NORMAL_TABLE + 4 <= dst < NORMAL_TABLE + 256 * STRIDE and (dst - NORMAL_TABLE - 4) % STRIDE == 0:
                names[(dst - NORMAL_TABLE - 4) // STRIDE] = self.wstr(nm)
        return tab, names

    def s2c_events(self, fn):
        """formats (in code order, loops marked by LOOP) and notify slots of one handler"""
        end = self.func_end(fn)
        ev, pend = [], None
        for ins in self.md.disasm(self.mem[fn - self.base:end - self.base], fn):
            if ins.mnemonic == "push" and ins.op_str.startswith("0x"):
                s = self.cstr(int(ins.op_str, 16))
                if s and re.fullmatch(r"[cdhfSsb]+", s):
                    pend = s
            elif ins.mnemonic == "call":
                if ins.op_str.startswith("0x"):
                    if int(ins.op_str, 16) == INTERP_THUNK and pend:
                        ev.append(pend)
                        pend = None
                else:
                    m = re.search(r"\[e..\s*\+\s*(0x[0-9a-f]+)\]", ins.op_str)
                    if m and int(m.group(1), 16) >= 0x400:
                        ev.append("notify+" + m.group(1))
            elif ins.mnemonic in ("jl", "jle", "jg", "jne", "jb", "ja"):
                try:
                    if int(ins.op_str, 16) < ins.address:
                        ev.append("LOOP")
                except ValueError:
                    pass
        return ev

    def c2s_requests(self, pattern):
        """UNetworkHandler request methods: (name, opcode, format) from the exported vtable listing"""
        lines = (ROOT / "analysis" / "engine_vtables.txt").read_text(encoding="utf-8").splitlines()
        rows = []
        for ln in lines:
            m = re.match(r"\s*\d+ \+0x([0-9a-f]+) ([0-9a-f]{8}) \?(\w+)@UNetworkHandler@@", ln)
            if not m or not re.fullmatch(pattern, m.group(3)):
                continue
            fn = self.thunk(int(m.group(2), 16))
            fmts, ops, call_fmt = [], None, None
            end = self.func_end(fn)
            code = list(self.md.disasm(self.mem[fn - self.base:end - self.base], fn))
            for i, ins in enumerate(code):
                if ins.mnemonic == "call" and "+ 0x64]" in ins.op_str:
                    # the pushes just before: ..., opcode, fmt, engine
                    pushes = [c for c in code[max(0, i - 14):i] if c.mnemonic == "push"]
                    for j in range(len(pushes) - 1):
                        s = self.cstr(int(pushes[j + 1].op_str, 16)) if pushes[j + 1].op_str.startswith("0x") else None
                        if s and re.fullmatch(r"c[cdhfSsb]*", s) and pushes[j].op_str.startswith("0x"):
                            ops, call_fmt = int(pushes[j].op_str, 16), s
                if ins.mnemonic == "push" and ins.op_str.startswith("0x"):
                    s = self.cstr(int(ins.op_str, 16))
                    if s and re.fullmatch(r"[dhc]{1,6}", s) and s != call_fmt:
                        fmts.append(s)
            rows.append((m.group(3), ops, call_fmt, [f for f in fmts if f != call_fmt] if call_fmt and "b" in call_fmt else []))
        return rows


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--engine", type=Path, default=DEFAULT_ENGINE, required=DEFAULT_ENGINE is None, help="default: $NWD_C3_SYSTEM/Engine.dll")
    ap.add_argument("--s2c", action="store_true", help="only the server -> client handlers")
    ap.add_argument("--c2s", action="store_true", help="only the client -> server requests")
    ap.add_argument("--only", default="", help="regex on the packet name")
    a = ap.parse_args()
    e = Eng(a.engine)
    if not a.c2s:
        tab, names = e.s2c_table()
        print("== s2c: opcode  name  handler  formats (in code order; 'LOOP' = loop back edge, notify+N = FL2NetNotify slot)")
        for op in FAMILY_S2C:
            n = int(op, 16)
            if n not in tab:
                continue
            nm = names.get(n) or "?"
            if a.only and not re.search(a.only, nm, re.I):
                continue
            fn = e.thunk(tab[n])
            print("  %s %-28s 0x%08x  %s" % (op, nm, fn, " ".join(e.s2c_events(fn))))
    if not a.s2c:
        print("== c2s: request  opcode  format  (record formats of the 'b' blocks after)")
        for nm, op, fmt, recs in e.c2s_requests(FAMILY_C2S):
            if a.only and not re.search(a.only, nm, re.I):
                continue
            print("  %-34s 0x%02x  %-6s %s" % (nm, op if op is not None else -1, fmt, " ".join(recs)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
