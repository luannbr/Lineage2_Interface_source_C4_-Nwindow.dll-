#!/usr/bin/env python3
"""OPCODE -> FORMAT table for every server->client packet the C3 client's Engine.dll can decode.

How the Chronicle 3 Engine.dll dispatches a game-server packet (all read from the disassembly, see
docs/notas/C3_Pacotes_T1.md):

* UNetworkHandler::Tick (VA 0x10405b10) pops a packet (opcode byte at +0, word sub-opcode at +2, payload pointer at
  +8) and, in the game state (engine+0x44 == 2), calls
      primary  table  VA 0x108e6318 + opcode * 0x104        (opcode byte 0x00..0xFE)
      extended table  VA 0x108f6618 + subopcode * 0x104     (opcode 0xFE, then the word sub-opcode)
  Each 0x104-byte entry is {dword handler; wchar name[]}. The tables are filled at startup by code that stores
  `mov [table + n*0x104], thunk` after copying the packet NAME, so this tool recovers handler + the client's own
  packet name for every opcode by scanning the code for those stores. (Entry 0 is statically initialised.) While
  the engine state is 1 (login server) a third table of 13 pointers at 0x108e62c4 is used instead (keys 'L:xx').
* handler(this, payload) decodes the payload with the format interpreter (VA 0x103ebff0, thunk 0x1030537b):
  reader(unused, cursor, format, out...) -> new cursor. The opcode (and sub-opcode) is NOT part of the format.
  Alphabet: c = 1 byte, h = 2, d = 4, f = 8 (double), S = NUL terminated UTF-16 string (2*(len+1) bytes),
  s = NUL terminated 8 bit string, b = raw block of N bytes where N is given by the CALLER (not in the packet).
  There is no array construct: lists are loops in the handler that call the reader again.
* the handler then calls a virtual of FL2NetNotify ([[engine+0x48]]; the object is the UGameEngine, whose vtable
  has the same 403 slots). Slot -> name is read from the real FL2NetNotify vtable (RVA 0x467308) through the DLL's
  export table; slot -> NConsoleWnd method is read from the first call/jmp of UGameEngine::OnX.

The structure of each handler (which format, how often, under which condition) is recovered by instrumented
emulation (unicorn): the real handler code runs with a fake engine, the reader is re-implemented, and one run per
perturbed packet field shows which fields are loop counts or switches. Without unicorn only the static pass runs.

fmt notation:   letters = fields;  [#n:body] = body repeated N times, N = value of the n-th letter of the same
nesting level;  {#n=a,b:body} / {#n!=a:body} = body present only when that field has / has not the listed value.
min_bytes = payload bytes read when every loop runs 0 times and every condition is false (a shorter packet makes
the interpreter read past the buffer: the Engine.dll+0EC053 crash).

usage:
    python tools/c3/s2c_formats.py                       # table of every opcode (about 4 minutes with emulation)
    python tools/c3/s2c_formats.py --opcode 9A           # one opcode (hex), FE:0003 (extended) or L:03 (login state)
    python tools/c3/s2c_formats.py --opcode 9A --full    # every field of the entry as JSON
    python tools/c3/s2c_formats.py --json analysis/c3_s2c_formats.json
    python tools/c3/s2c_formats.py --no-emu              # static pass only (fast; loops not resolved)
    python tools/c3/s2c_formats.py --protocol 530        # protocol version the emulated client "sent" (default 530)
"""
from __future__ import annotations

import argparse
import json
import re
import struct
import os
import sys
from pathlib import Path

import pefile
from capstone import CS_ARCH_X86, CS_MODE_32, Cs
from capstone.x86 import X86_OP_IMM, X86_OP_MEM, X86_OP_REG

ROOT = Path(__file__).resolve().parents[2]
# NWD_C3_SYSTEM: the C3 client's System folder
DEFAULT_ENGINE = (Path(os.environ["NWD_C3_SYSTEM"]) / "Engine.dll" if os.environ.get("NWD_C3_SYSTEM") else None)
# NWD_C3_SPEC: the C3 packet spec page ("C3 Rise Of Darkness.html")
DEFAULT_SPEC = (Path(os.environ["NWD_C3_SPEC"]) if os.environ.get("NWD_C3_SPEC") else None)

PRIMARY_TABLE = 0x108E6318
EXT_TABLE = 0x108F6618
ENTRY = 0x104
READER_THUNK = 0x1030537B          # jmp 0x103ebff0, the format interpreter
READER_REAL = 0x103EBFF0
WRITER_THUNK = 0x10309043          # jmp 0x103ebb20, the c2s packer (not used here)
G_PROTOCOL = 0x108E6290            # protocol version the client sent in its first packet (cdb); CharInfo/UserInfo/NpcInfo
PROTOCOL_VERSION = 530             #   read extra trailing fields when it is > 420 (C3 'Rise of Darkness' = 530)
G_ENGINE = 0x1097B408              # *(G_ENGINE) = engine object; +0x48 = FL2NetNotify instance
NOTIFY_VT_RVA = 0x467308           # ??_7FL2NetNotify@@6B@ (403 slots)
FMT_OK = re.compile(r"^[cdhfsSb]+$")


class Image:
    def __init__(self, path: Path):
        self.pe = pefile.PE(str(path))
        self.base = self.pe.OPTIONAL_HEADER.ImageBase
        self.mem = bytes(self.pe.get_memory_mapped_image())
        text = next(s for s in self.pe.sections if s.Name.startswith(b".text"))
        self.t0 = text.VirtualAddress
        self.t1 = text.VirtualAddress + text.Misc_VirtualSize
        self.md = Cs(CS_ARCH_X86, CS_MODE_32)
        self.md.detail = True
        self.exports = {}
        if hasattr(self.pe, "DIRECTORY_ENTRY_EXPORT"):
            for s in self.pe.DIRECTORY_ENTRY_EXPORT.symbols:
                if s.name:
                    self.exports.setdefault(self.base + s.address, s.name.decode("latin1"))
        self._code = self.mem[self.t0:self.t1]

    # ---- raw access -------------------------------------------------------------------------------------------
    def u32(self, va: int) -> int:
        o = va - self.base
        return struct.unpack_from("<I", self.mem, o)[0]

    def cstr(self, va: int, mx: int = 300) -> str:
        a = va - self.base
        e = a
        while e < len(self.mem) and self.mem[e] != 0 and e - a < mx:
            e += 1
        return self.mem[a:e].decode("latin1")

    def wstr(self, va: int, mx: int = 300) -> str:
        a = va - self.base
        e = a
        while e + 1 < len(self.mem) and self.mem[e:e + 2] != b"\0\0" and e - a < mx * 2:
            e += 2
        return self.mem[a:e].decode("utf-16le", "replace")

    def follow_thunk(self, va: int) -> int:
        for _ in range(6):
            o = va - self.base
            if self.mem[o] == 0xE9:
                va = (va + 5 + struct.unpack_from("<i", self.mem, o + 1)[0]) & 0xFFFFFFFF
            else:
                break
        return va

    def in_text(self, va: int) -> bool:
        return self.base + self.t0 <= va < self.base + self.t1

    def disasm(self, va: int, size: int):
        o = va - self.base
        return list(self.md.disasm(self.mem[o:o + size], va))

    # ---- tables -----------------------------------------------------------------------------------------------
    def find_table(self, table: int, count: int):
        """{index: (thunk, real, name, store_va)} from `mov dword ptr [table+i*0x104], thunk` stores."""
        res = {}
        code = self._code
        for m in re.finditer(rb"\xc7\x05(....)(....)", code, re.S):
            ad = struct.unpack("<I", m.group(1))[0]
            if table <= ad < table + count * ENTRY + 4 and (ad - table) % ENTRY == 0:
                i = (ad - table) // ENTRY
                win = code[max(0, m.start() - 48):m.start() + 48]
                pushes = [struct.unpack("<I", x.group(1))[0] for x in re.finditer(rb"\x68(....)", win, re.S)]
                name = None
                for k, p in enumerate(pushes):
                    if p == ad + 4 and k > 0:
                        name = self.wstr(pushes[k - 1])
                h = struct.unpack("<I", m.group(2))[0]
                res[i] = (h, self.follow_thunk(h), name, self.base + self.t0 + m.start())
        return res

    def notify_vtable(self):
        """{slot: (func_va, export_name)} of FL2NetNotify in this Engine.dll."""
        va = self.base + NOTIFY_VT_RVA
        out = {}
        for i in range(403):
            f = self.u32(va + 4 * i)
            out[i] = (f, self.exports.get(f))
        return out


def demangle_short(mangled: str | None) -> str | None:
    """?OnChangeWaitType@FL2NetNotify@@UAEHPAUUser@@HVFVector@@@Z -> OnChangeWaitType"""
    if not mangled:
        return None
    m = re.match(r"\?([^@]+)@FL2NetNotify@@", mangled)
    return m.group(1) if m else mangled


# --------------------------------------------------------------------------------------------------------------
class Handler:
    """Static facts about one packet handler function."""

    def __init__(self, img: Image, start: int):
        self.img = img
        self.start = start
        self.end = start
        self.readers = []     # {"va","fmt","loop":(lo,hi)|None, "bsizes":[...]}
        self.notifies = []    # {"va","slot"}
        self.backedges = []   # (from_va, to_va)
        self.size_known = True
        self._scan()

    def _scan(self):
        img = self.img
        ins = []
        pos = self.start
        lo = self.start
        max_t = self.start
        tables = []
        limit = self.start + 0x6000
        while pos < limit:
            chunk = img.disasm(pos, 2048)
            if not chunk:
                pos += 1
                continue
            stop = False
            for i in chunk:
                ins.append(i)
                pos = i.address + i.size
                # forward / backward jump bookkeeping
                if i.mnemonic == "jmp" and i.op_str.startswith("dword ptr [") and "*4" in i.op_str:
                    mm = re.search(r"\+ (0x[0-9a-f]+)\]", i.op_str)
                    if mm:
                        t = int(mm.group(1), 16)
                        k = 0
                        while k < 256:
                            e = img.u32(t + 4 * k)
                            if lo <= e < self.start + 0x6000 and img.in_text(e):
                                max_t = max(max_t, e)
                                k += 1
                            else:
                                break
                        tables.append((t, k))
                elif i.mnemonic.startswith("j") and i.operands and i.operands[0].type == X86_OP_IMM:
                    t = i.operands[0].imm
                    if t > i.address:
                        max_t = max(max_t, t)
                if i.mnemonic in ("ret",) or (i.mnemonic == "jmp" and i.operands and i.operands[0].type == X86_OP_IMM):
                    nxt = i.address + i.size
                    if nxt > max_t:
                        b = img.mem[nxt - img.base:nxt - img.base + 1]
                        if b in (b"\xcc", b"\x90") or tables or True:
                            # end of function: the next byte is padding or a jump table
                            if b in (b"\xcc", b"\x90") or (tables and i.mnemonic == "ret"):
                                stop = True
                                break
                if i.mnemonic == "int3":
                    stop = True
                    break
            if stop:
                break
        self.ins = ins
        self.end = ins[-1].address + ins[-1].size if ins else self.start
        self.consts = set()
        self.has_switch = False
        for i in ins:
            if i.mnemonic == "cmp" and len(i.operands) == 2 and i.operands[1].type == X86_OP_IMM:
                v = i.operands[1].imm & 0xFFFFFFFF
                if v >= 0xFFFFFF00:
                    v = (-(1 << 32) + v)
                if 0 <= v <= 64:
                    self.consts.add(v)
            if i.mnemonic == "jmp" and i.operands and i.operands[0].type == X86_OP_MEM and i.operands[0].mem.index:
                self.has_switch = True
        self._analyse()

    def _analyse(self):
        img = self.img
        ins = self.ins
        # back edges (loops)
        for i in ins:
            if i.mnemonic.startswith("j") and i.operands and i.operands[0].type == X86_OP_IMM:
                t = i.operands[0].imm
                if t <= i.address and self.start <= t:
                    self.backedges.append((i.address, t))
        loops = sorted(self.backedges, key=lambda e: e[0] - e[1])
        # reader calls + their format
        tags = {}
        pushes = []   # (va, kind, value)
        for idx, i in enumerate(ins):
            if i.mnemonic == "push" and i.operands and i.operands[0].type == X86_OP_IMM:
                pushes.append((idx, i.operands[0].imm & 0xFFFFFFFF))
            if i.mnemonic == "call" and i.operands and i.operands[0].type == X86_OP_IMM and \
                    i.operands[0].imm in (READER_THUNK, READER_REAL):
                fmt = None
                fidx = None
                for (pidx, val) in reversed(pushes):
                    if pidx >= idx:
                        continue
                    if idx - pidx > 40:
                        break
                    if img.base <= val < img.base + len(img.mem):
                        s = img.cstr(val)
                        if s and FMT_OK.match(s):
                            fmt = s
                            fidx = pidx
                            break
                loop = None
                for (frm, to) in loops:
                    if to <= i.address <= frm:
                        loop = (to, frm)
                        break
                self.readers.append({"va": i.address, "fmt": fmt, "loop": loop, "ins_idx": idx, "fmt_idx": fidx})
        # notify calls: mov r1,[x+0x48] ; mov r2,[r1] ; call [r2+off]
        ntag = {}
        for i in ins:
            if i.mnemonic == "mov" and len(i.operands) == 2 and i.operands[0].type == X86_OP_REG and \
                    i.operands[1].type == X86_OP_MEM:
                dst = i.reg_name(i.operands[0].reg)
                m = i.operands[1].mem
                base = i.reg_name(m.base) if m.base else None
                if m.index == 0 and base and m.disp == 0x48 and True:
                    ntag[dst] = "N"
                    continue
                if m.index == 0 and base and m.disp == 0 and ntag.get(base) == "N":
                    ntag[dst] = "V"
                    continue
                ntag.pop(dst, None)
            elif i.mnemonic == "call" and i.operands and i.operands[0].type == X86_OP_MEM:
                m = i.operands[0].mem
                base = i.reg_name(m.base) if m.base else None
                if base and m.index == 0 and ntag.get(base) == "V":
                    self.notifies.append({"va": i.address, "slot": m.disp // 4, "off": m.disp})
            else:
                # other register writers invalidate their tag
                try:
                    _, w = i.regs_access()
                    for r in w:
                        ntag.pop(i.reg_name(r), None)
                except Exception:
                    pass


def load_vt_inc():
    """{slot: (name, 'args')} from include/ue2/gen/FL2NetNotify.vt.inc (403 slots; verified equal to the C3 Engine.dll
    export names for 402 of them, see the doc)."""
    out = {}
    p = ROOT / "include" / "ue2" / "gen" / "FL2NetNotify.vt.inc"
    if p.exists():
        for line in p.read_text(encoding="utf-8").splitlines():
            m = re.match(r"\s*virtual\s+(\S.*?)\s+(\w+)\((.*?)\)\s*;\s*//\s*(\d+)\s+\+0x([0-9a-fA-F]+)", line)
            if m:
                out[int(m.group(4))] = (m.group(2), m.group(3))
    return out


def slot_name(nvt, inc, slot):
    ex = nvt.get(slot, (0, None))[1]
    nm = demangle_short(ex)
    if nm is None and slot in inc:
        nm = inc[slot][0]
    return nm


def build(img: Image):
    prim = img.find_table(PRIMARY_TABLE, 256)
    ext = img.find_table(EXT_TABLE, 256)
    return prim, ext


# --------------------------------------------------------------------------------------------------------------
# Instrumented emulation of a handler (unicorn). Optional: without unicorn only the static pass is available.
try:
    from unicorn import (Uc, UcError, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE, UC_HOOK_MEM_READ_UNMAPPED,
                         UC_HOOK_MEM_WRITE_UNMAPPED, UC_MEM_READ_UNMAPPED, UC_MEM_WRITE_UNMAPPED)
    from unicorn.x86_const import (UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_EDX, UC_X86_REG_ESI,
                                   UC_X86_REG_EDI, UC_X86_REG_EBP, UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_EFLAGS)
    HAVE_UNICORN = True
except Exception:  # pragma: no cover
    HAVE_UNICORN = False

IMPORT_POP = {"??0FString@@QAE@PBG@Z": 4, "??4FStringNoInit": 4, "?Realloc@FArray": 4, "RotateAngleAxis": 12,
              "?Rotation@FVector": 4, "DeleteCriticalSection": 4, "closesocket": 4}
STACK_BASE, STACK_SIZE = 0x00200000, 0x00100000
THIS, ENGOBJ, NOTIFY_OBJ, VT = 0x01000000, 0x01100000, 0x01200000, 0x01300000
BUF = 0x03000000
RAND, RAND_SIZE = 0x04000000, 0x00400000
STUB_BASE, SENTINEL = 0x7E000000, 0x7F000000
SIZES = {"c": 1, "h": 2, "d": 4, "f": 8}


class Emu:
    """Runs one handler with a fake engine: the reader (c/h/d/f/S/s/b) is re-implemented, every other call outside
    the handler is stubbed, FL2NetNotify virtuals are recorded. Values for the packet fields come from value_fn."""

    def __init__(self, img: Image):
        self.img = img
        self.nvt = img.notify_vtable()
        self.cache_ret = {}
        self.imports = {}
        self.dirty = False
        self._setup()

    def _setup(self):
        img = self.img
        uc = Uc(UC_ARCH_X86, UC_MODE_32)
        self.uc = uc
        size = (len(img.mem) + 0xFFF) & ~0xFFF
        uc.mem_map(img.base, size)
        uc.mem_write(img.base, img.mem)
        uc.mem_map(0, 0x10000)
        uc.mem_map(STACK_BASE, STACK_SIZE)
        uc.mem_map(THIS, 0x400000)
        uc.mem_map(BUF, 0x10000)
        uc.mem_map(RAND, RAND_SIZE)
        uc.mem_map(STUB_BASE, 0x10000)
        uc.mem_map(SENTINEL, 0x1000)
        P = RAND + 0x200000
        blob = bytearray()
        for k in range(RAND_SIZE // 0x100):
            blk = bytearray(struct.pack("<I", P) * 0x40)
            struct.pack_into("<II", blk, 0, 1, RAND + 0x100 * (k + 1) if k + 1 < RAND_SIZE // 0x100 else P)
            blob += blk
        uc.mem_write(RAND, bytes(blob))
        uc.mem_write(G_ENGINE, struct.pack("<I", ENGOBJ))
        uc.mem_write(ENGOBJ + 0x48, struct.pack("<I", NOTIFY_OBJ))
        uc.mem_write(ENGOBJ + 0x44, struct.pack("<I", 2))
        uc.mem_write(NOTIFY_OBJ, struct.pack("<I", VT))
        uc.mem_write(VT, b"".join(struct.pack("<I", STUB_BASE + 16 * i) for i in range(410)))
        # TMap at ENGOBJ+0x80 with one pair {next=-1, key=2, value=user object}
        MAP, PAIRS, HASH = ENGOBJ + 0x80, ENGOBJ + 0x1000, ENGOBJ + 0x2000
        uc.mem_write(MAP, struct.pack("<IIIII", PAIRS, 1, 1, HASH, 1))
        uc.mem_write(PAIRS, struct.pack("<III", 0xFFFFFFFF, 2, RAND + 0x1000))
        uc.mem_write(HASH, struct.pack("<I", 0))
        uc.mem_write(THIS + 0x3C, struct.pack("<I", RAND))
        uc.mem_write(THIS + 0x2C, struct.pack("<I", RAND))
        dsec = next(x for x in img.pe.sections if x.Name.startswith(b".data"))
        data_lo = img.base + dsec.VirtualAddress
        self.snap = [(data_lo, bytes(uc.mem_read(data_lo, dsec.SizeOfRawData))),
                     (THIS, bytes(uc.mem_read(THIS, 0x400000))),
                     (RAND, bytes(uc.mem_read(RAND, RAND_SIZE))),
                     (SENTINEL, bytes(0x1000))]
        self.automapped = []
        self.imports = {}
        if hasattr(img.pe, "DIRECTORY_ENTRY_IMPORT"):
            for d in img.pe.DIRECTORY_ENTRY_IMPORT:
                for imp in d.imports:
                    if imp.name:
                        self.imports[imp.address] = d.dll.decode().lower() + "!" + imp.name.decode()
                    uc.mem_write(imp.address, struct.pack("<I", RAND + 0x300))

    def ret_bytes(self, va):
        if va in self.cache_ret:
            return self.cache_ret[va]
        img = self.img
        n = None
        cur = img.follow_thunk(va)
        if img.in_text(cur):
            for ins in img.disasm(cur, 1200)[:400]:
                if ins.mnemonic == "ret":
                    n = ins.operands[0].imm if ins.operands else 0
                    break
        self.cache_ret[va] = n
        return n

    def run(self, h: Handler, value_fn, max_insns=60000, watch=None):
        if self.dirty:
            self._setup()
            self.dirty = False
        uc, img, ins = self.uc, self.img, h.ins
        for a, b in self.snap:
            uc.mem_write(a, b)
        uc.mem_write(G_PROTOCOL, struct.pack("<I", PROTOCOL_VERSION))
        for a in self.automapped:
            try:
                uc.mem_unmap(a, 0x2000)
            except UcError:
                pass
        self.automapped = []
        trace, notifies = [], []
        state = {"callno": 0, "err": None, "unk": []}
        sp = STACK_BASE + STACK_SIZE - 0x1000 - 12
        uc.mem_write(sp, struct.pack("<III", SENTINEL, THIS, BUF))
        uc.reg_write(UC_X86_REG_ESP, sp)
        entry_esp = sp
        for r in (UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_EDX, UC_X86_REG_ESI, UC_X86_REG_EDI,
                  UC_X86_REG_EBP):
            uc.reg_write(r, 0)
        uc.reg_write(UC_X86_REG_EFLAGS, 2)
        uc.mem_write(BUF, bytes(0x100))
        regmap = {"eax": UC_X86_REG_EAX, "ebx": UC_X86_REG_EBX, "ecx": UC_X86_REG_ECX, "edx": UC_X86_REG_EDX,
                  "esi": UC_X86_REG_ESI, "edi": UC_X86_REG_EDI, "ebp": UC_X86_REG_EBP, "esp": UC_X86_REG_ESP}

        def rd32(a):
            return struct.unpack("<I", bytes(uc.mem_read(a, 4)))[0]

        def target_of(i):
            op = i.operands[0]
            if op.type == X86_OP_IMM:
                return op.imm & 0xFFFFFFFF, None
            if op.type == X86_OP_REG:
                return uc.reg_read(regmap[i.reg_name(op.reg)]), None
            m = op.mem
            a = m.disp
            if m.base:
                a += uc.reg_read(regmap[i.reg_name(m.base)])
            if m.index:
                a += uc.reg_read(regmap[i.reg_name(m.index)]) * m.scale
            a &= 0xFFFFFFFF
            try:
                return rd32(a), a
            except UcError:
                return 0, a

        def handle_reader(i):
            esp = uc.reg_read(UC_X86_REG_ESP)
            buf = rd32(esp + 4)
            fmt_va = rd32(esp + 8)
            fmt = img.cstr(fmt_va) if img.base <= fmt_va < img.base + len(img.mem) else "?"
            args = esp + 12
            cur = buf
            fields = []
            dsts = []
            call = state["callno"]
            state["callno"] += 1
            ai = 0
            for fno, ch in enumerate(fmt):
                v = value_fn(call, fno, ch, i.address)
                if ch in "chdf":
                    dst = rd32(args + 4 * ai)
                    dsts.append(dst)
                    ai += 1
                    uc.mem_write(dst, struct.pack({"c": "<B", "h": "<H", "d": "<I", "f": "<d"}[ch],
                                                  float(v) if ch == "f" else v & ((1 << (8 * SIZES[ch])) - 1)))
                    cur += SIZES[ch]
                elif ch in "Ss":
                    mx = rd32(args + 4 * ai)
                    dst = rd32(args + 4 * ai + 4)
                    ai += 2
                    if ch == "S":
                        uc.mem_write(dst, "A\0".encode("utf-16le"))
                        cur += 4
                    else:
                        uc.mem_write(dst, b"A\0")
                        cur += 2
                    v = (ch, mx)
                elif ch == "b":
                    sz = rd32(args + 4 * ai)
                    ai += 2
                    cur += sz
                    v = ("b", sz)
                fields.append((ch, v))
            trace.append({"va": i.address, "fmt": fmt, "fields": fields, "call": call, "bytes": cur - buf, "dsts": dsts})
            uc.reg_write(UC_X86_REG_EAX, cur)
            uc.reg_write(UC_X86_REG_EIP, i.address + i.size)

        addr_k = {i.address: k for k, i in enumerate(ins)}

        watched = {}

        def hook(uc_, address, size, user):
            if watch and address in watch and address not in watched:
                watched[address] = uc.reg_read(UC_X86_REG_ESP)
            k = addr_k.get(address)
            if k is None:
                return
            i = ins[k]
            if i.mnemonic != "call":
                return
            tgt, memaddr = target_of(i)
            nxt = i.address + i.size
            if tgt in (READER_THUNK, READER_REAL):
                handle_reader(i)
                return
            if STUB_BASE <= tgt < STUB_BASE + 16 * 410:
                slot = (tgt - STUB_BASE) // 16
                esp = uc.reg_read(UC_X86_REG_ESP)
                nb = self.ret_bytes(self.nvt.get(slot, (0, None))[0]) or 0
                notifies.append({"va": i.address, "slot": slot, "args": [rd32(esp + 4 * a) for a in range(min(nb // 4, 12))]})
                uc.reg_write(UC_X86_REG_ESP, esp + nb)
                uc.reg_write(UC_X86_REG_EAX, 1)
                uc.reg_write(UC_X86_REG_EIP, nxt)
                return
            if tgt == 0x10714EE0:   # __chkstk
                uc.reg_write(UC_X86_REG_ESP, uc.reg_read(UC_X86_REG_ESP) - uc.reg_read(UC_X86_REG_EAX))
                uc.reg_write(UC_X86_REG_EIP, nxt)
                return
            if h.start <= tgt < h.end and tgt != h.start:
                return   # call inside the handler: run for real
            esp = uc.reg_read(UC_X86_REG_ESP)
            nb = None
            if i.operands[0].type == X86_OP_IMM:
                nb = self.ret_bytes(tgt)
                th = img.follow_thunk(tgt)
                if nb is not None and img.mem[th - img.base:th - img.base + 2] == b"\xff\x25":
                    nb = None
            if nb is None:
                nb = 0
                if memaddr is not None and memaddr in self.imports:
                    for key, val in IMPORT_POP.items():
                        if key in self.imports[memaddr]:
                            nb = val
                            break
            if nb == 0 and i.operands[0].type == X86_OP_MEM and not (memaddr is not None and memaddr in self.imports):
                disp = i.operands[0].mem.disp
                if i.operands[0].mem.index == 0 and disp == 0:
                    nb = 8          # GMalloc->Malloc(size, tag)
                elif i.operands[0].mem.index == 0 and disp == 8:
                    nb = 4          # GMalloc->Free(ptr)
                elif disp != 0x2BC:  # 0x2bc = FOutputDevice::Logf (cdecl); anything else: callee pops what was pushed
                    pend = 0
                    j = k - 1
                    while j >= 0:
                        mm = ins[j].mnemonic
                        if mm == "push":
                            pend += 1
                        elif mm in ("call", "jmp", "ret") or mm.startswith("j") or                                 (mm in ("add", "sub") and ins[j].op_str.startswith("esp")):
                            break
                        j -= 1
                    cleaned = False
                    for x in ins[k + 1:k + 3]:
                        if x.mnemonic == "add" and x.op_str.startswith("esp"):
                            cleaned = True
                    nb = 0 if cleaned else 4 * pend
            state["unk"].append((i.address, tgt, nb))
            uc.reg_write(UC_X86_REG_ESP, esp + nb)
            uc.reg_write(UC_X86_REG_EAX, RAND)
            uc.reg_write(UC_X86_REG_EIP, nxt)

        def unmapped(uc_, access, address, size, value, user):
            if access in (UC_MEM_READ_UNMAPPED, UC_MEM_WRITE_UNMAPPED):
                try:
                    b = address & ~0xFFF
                    uc.mem_map(b, 0x2000)
                    self.automapped.append(b)
                    return True
                except UcError:
                    return False
            return False

        hm = uc.hook_add(UC_HOOK_MEM_READ_UNMAPPED | UC_HOOK_MEM_WRITE_UNMAPPED, unmapped)
        hh = uc.hook_add(UC_HOOK_CODE, hook, begin=h.start, end=h.end)
        try:
            uc.emu_start(h.start, SENTINEL, count=max_insns)
        except UcError as e:
            state["err"] = str(e)
            if "EXCEPTION" in state["err"] or "INSN_INVALID" in state["err"]:
                self.dirty = True
        finally:
            uc.hook_del(hh)
            uc.hook_del(hm)
        state["ended"] = uc.reg_read(UC_X86_REG_EIP) == SENTINEL
        state["esp_ok"] = uc.reg_read(UC_X86_REG_ESP) == entry_esp + 4
        state["eip"] = uc.reg_read(UC_X86_REG_EIP)
        return {"trace": trace, "notifies": notifies, "state": state, "watched": watched}


# --------------------------------------------------------------------------------------------------------------
# Structure recovery: baseline run (every numeric field = 2) + one run per perturbed field.
TEST1 = (0, 1, 3)
TEST2 = (4, 5, 6, 7, 8, 9, 10, 11, 12)


def _cnt(trace):
    c = {}
    for t in trace:
        c[t["va"]] = c.get(t["va"], 0) + 1
    return c


def _merge_order(order, new_order):
    """insert the sites of new_order that are missing in order, right after their predecessor."""
    for idx, va in enumerate(new_order):
        if va in order:
            continue
        pos = 0
        for prev in reversed(new_order[:idx]):
            if prev in order:
                pos = order.index(prev) + 1
                break
        order.insert(pos, va)
    return order


def _first_order(trace):
    seen = []
    for t in trace:
        if t["va"] not in seen:
            seen.append(t["va"])
    return seen


def test_values(h: Handler):
    tv = set(TEST1)
    for c in h.consts:
        if c >= 2:
            tv.add(c)
            tv.add(c + 1)
    if h.has_switch:
        tv |= set(range(0, 13))
    return tuple(sorted(tv))


def analyse_handler(em: Emu, h: Handler):
    """-> dict(base=..., order=[va], site=[va->info], ctrl={...}, partial=bool)."""
    TV1 = test_values(h)
    def run(over):
        return em.run(h, lambda c, f, l, va: over.get((va, f), 2))

    base = run({})
    bc = _cnt(base["trace"])
    order = _first_order(base["trace"])
    site_info = {}
    for t in base["trace"]:
        site_info.setdefault(t["va"], t)
    ctrl = {}      # (call, field) -> {tv: counts}
    queue = [t for t in site_info.values()]
    done_sites = set()
    while queue:
        t = queue.pop(0)
        if t["va"] in done_sites:
            continue
        done_sites.add(t["va"])
        for fno, (ch, v) in enumerate(t["fields"]):
            if ch not in "chd":
                continue
            res = {}
            changed = False
            for tv in TV1:
                r = run({(t["va"], fno): tv})
                res[tv] = _cnt(r["trace"])
                if res[tv] != bc:
                    changed = True
                for tt in r["trace"]:
                    if tt["va"] not in site_info:
                        site_info[tt["va"]] = tt
                        queue.append(tt)
                _merge_order(order, _first_order(r["trace"]))
            if changed:
                ctrl[(t["va"], fno)] = {"res": res, "site": t["va"], "ch": ch}
    # keys that behave like a type/switch: look at more values
    for key, info in list(ctrl.items()):
        res = info["res"]
        info["wide"] = False
        sites_dep = [va for va in set(site_info) if len({res[tv].get(va, 0) for tv in res} | {bc.get(va, 0)}) > 1]
        info["dep"] = sites_dep
        kinds = set()
        for va in sites_dep:
            b = bc.get(va, 0)
            if b > 0 and all(res[tv].get(va, 0) * 2 == b * tv for tv in TEST1):
                kinds.add("loop")
            else:
                kinds.add("cond")
        if "cond" in kinds:
            wide = {}
            for tv in sorted(set(TEST2) | set(TV1)):
                if tv in res:
                    continue
                r = run({key: tv})
                wide[tv] = _cnt(r["trace"])
                for tt in r["trace"]:
                    if tt["va"] not in site_info:
                        site_info[tt["va"]] = tt
                _merge_order(order, _first_order(r["trace"]))
            res.update(wide)
            info["wide"] = True
            info["dep"] = [va for va in set(site_info) if len({res[tv].get(va, 0) for tv in res} | {bc.get(va, 0)}) > 1]
    return {"base": base, "bc": bc, "order": order, "site": site_info, "ctrl": ctrl}


def build_tree(an):
    """Turn the analysis into a nested structure: list of nodes
       {"t":"f","va":va,"fmt":str} | {"t":"loop","key":k,"body":[...]} | {"t":"cond","key":k,"vals":[...],"body":[...]}"""
    bc, order, site_info, ctrl = an["bc"], an["order"], an["site"], an["ctrl"]
    sig = {va: [] for va in order}
    for key, info in ctrl.items():
        res = info["res"]
        for va in info["dep"]:
            if va not in sig:
                continue
            b = bc.get(va, 0)
            tvs = sorted(res)
            cs = {tv: res[tv].get(va, 0) for tv in tvs}
            if b > 0 and all(cs[tv] * 2 == b * tv for tv in TEST1):
                sig[va].append((key, "loop", ()))
            else:
                vals = sorted({tv for tv in tvs if cs[tv] > 0} | ({2} if b > 0 else set()))
                sig[va].append((key, "cond", tuple(vals), tuple(sorted(set(tvs) | {2}))))

    # a condition keyed on a field that lives inside a loop body belongs to that loop (per item switch)
    key_site = {k: info["site"] for k, info in ctrl.items()}
    changed = True
    while changed:
        changed = False
        for va in order:
            for e in list(sig[va]):
                if e[1] != "cond":
                    continue
                ks = key_site.get(e[0])
                if ks in sig:
                    for le in sig[ks]:
                        if le[1] == "loop" and le not in sig[va]:
                            sig[va].append(le)
                            changed = True

    def key_ord(entry):
        return (0 if entry[1] == "loop" else 1, entry[0])

    def emit(sites, outer):
        out = []
        i = 0
        while i < len(sites):
            s = sites[i]
            rem = [e for e in sig[s] if e[:2] not in outer]
            if not rem:
                out.append({"t": "f", "va": s, "fmt": site_info[s]["fmt"], "fields": site_info[s]["fields"]})
                i += 1
                continue
            e = min(rem, key=key_ord)
            j = i
            while j < len(sites) and e in sig[sites[j]]:
                j += 1
            body = emit(sites[i:j], outer | {e[:2]})
            if e[1] == "loop":
                out.append({"t": "loop", "key": e[0], "body": body})
            else:
                out.append({"t": "cond", "key": e[0], "vals": list(e[2]), "dom": list(e[3]), "body": body})
            i = j
        return out

    return emit(order, frozenset())


def _field_sizes(fields):
    n = 0
    for ch, v in fields:
        if ch in SIZES:
            n += SIZES[ch]
        elif ch == "S":
            n += 2
        elif ch == "s":
            n += 1
        elif ch == "b":
            n += v[1]
    return n


def render_tree(tree, site_info):
    """-> (fmt string, min_bytes, loops list). #n = n-th letter of the same nesting level (1-based)."""
    loops = []

    def letters_of(nodes):
        # (call, field) -> position within this level
        pos = {}
        n = 0
        for nd in nodes:
            if nd["t"] == "f":
                t = site_info[nd["va"]]
                for fno in range(len(t["fmt"])):
                    n += 1
                    pos[(t["va"], fno)] = n
        return pos

    def rend(nodes, outer_pos, depth):
        pos = letters_of(nodes)
        allpos = dict(outer_pos)
        for k, v in pos.items():
            allpos[k] = (v, depth)
        s = []
        minb = 0
        for nd in nodes:
            if nd["t"] == "f":
                s.append(nd["fmt"])
                minb += _field_sizes(nd["fields"])
            else:
                body, _ = rend(nd["body"], {k: v for k, v in allpos.items()}, depth + 1)
                ref = allpos.get(nd["key"])
                if ref is None:
                    r = "#?"
                else:
                    r = "#%d" % ref[0] if ref[1] == depth else "#%s%d" % ("^" * (depth - ref[1]), ref[0])
                if nd["t"] == "loop":
                    s.append("[%s:%s]" % (r, body))
                    loops.append({"count": r, "body": body})
                else:
                    vals = nd["vals"]
                    dom = nd.get("dom") or vals
                    absent = [v for v in dom if v not in vals]
                    if not absent:
                        vs = " any"
                    elif len(vals) > 3 and len(absent) < len(vals):
                        vs = "!=" + ",".join(str(v) for v in absent)
                    elif vals and 0 not in vals and vals == [v for v in dom if v != 0]:
                        vs = "!=0"
                    elif len(vals) > 3 and vals == list(range(vals[0], vals[-1] + 1)):
                        vs = "=%d..%d" % (vals[0], vals[-1])
                    else:
                        vs = "=" + ",".join(str(v) for v in vals)
                    s.append(("{%s%s:%s}" % (r, vs, body)))
        return "".join(s), minb

    fmt, minb = rend(tree, {}, 0)
    return fmt, minb, loops


def static_fmt(h: Handler) -> str:
    """Fallback: reader formats in address order; [..] = reader call inside a loop (count unknown)."""
    out, open_loop = [], None
    for r in h.readers:
        f = r["fmt"] or "?"
        if r["loop"] and r["loop"] != open_loop:
            if open_loop:
                out.append("]")
            out.append("[")
            open_loop = r["loop"]
        elif not r["loop"] and open_loop:
            out.append("]")
            open_loop = None
        out.append(f)
    if open_loop:
        out.append("]")
    return "".join(out)


# --------------------------------------------------------------------------------------------------------------
def load_spec(path: Path):
    """{ '9A': 'PrivateStoreManageList', 'FE:0001': 'ExRegenMax' } from the netpro C3 page (server list only)."""
    if not path.exists():
        return {}
    s = path.read_text(encoding="utf-8", errors="replace")
    i = s.find("<h2>Server</h2>")
    out = {}
    for op, name in re.findall(r"<li>([0-9A-Fa-f]{2}(?::[0-9A-Fa-f]{2,4})?)\s*<a[^>]*>([^<]+)</a>", s[i:]):
        if ":" in op:
            a, b = op.split(":")
            op = "%s:%04X" % (a.upper(), int(b, 16))
        else:
            op = op.upper()
        out[op] = name.strip()
    return out


def norm(n):
    if not n:
        return ""
    n = re.sub(r"Packet$", "", n)
    return re.sub(r"[^a-z0-9]", "", n.lower())


def load_vslot_names():
    """{vtable byte offset: [function names]} of the console window, from db/functions.csv ('vslot N (+0xOFF)')."""
    import csv
    out = {}
    p = ROOT / "db" / "functions.csv"
    if p.exists():
        with p.open(encoding="utf-8", errors="replace", newline="") as f:
            for row in csv.reader(f):
                if len(row) > 7 and row[0] != "va":
                    for m in re.finditer(r"vslot (\d+) \(\+0x([0-9a-f]+)\)", row[7]):
                        out.setdefault(int(m.group(2), 16), []).append(row[4])
    return out


def console_target(img: Image, slot: int, vslots):
    """FL2NetNotify slot -> NConsoleWnd method. UGameEngine implements FL2NetNotify (its vtable has the same 403 slots);
    the override UGameEngine::OnX forwards to the console window with its first call/jmp [reg+off]; off is looked up
    in the console's vtable. Returns (offset, name) or (None, None) when the override does not forward."""
    ge = None
    for va, nm in img.exports.items():
        if nm == "??_7UGameEngine@@6B@":
            ge = va
    if ge is None:
        return None, None
    real = img.follow_thunk(img.u32(ge + 4 * slot))
    for n, x in enumerate(img.disasm(real, 200)[:14]):
        if x.mnemonic in ("call", "jmp") and x.operands:
            op = x.operands[0]
            if op.type == X86_OP_MEM and op.mem.base and not op.mem.index and op.mem.disp >= 0x100 and op.mem.disp != 0x2BC:
                names = [n_ for n_ in vslots.get(op.mem.disp, []) if n_.startswith("NConsoleWnd::")]
                return op.mem.disp, (names[0] if names else None)
            return None, None   # first call is something else (world logic, not the console)
        if x.mnemonic == "ret":
            break
    return None, None


def login_table(img: Image):
    """The 13 login-state handlers (table at 0x108e62c4, used while the engine state is 1)."""
    out = {}
    for i in range(13):
        out["L:%02X" % i] = img.follow_thunk(img.u32(0x108E62C4 + 4 * i))
    return out



# Entries whose structure the emulation cannot recover alone (a conjunction of two fields, a lookup that decides
# whether a field is read, ...). Each one was read in the disassembly; the reason is in 'notes'.
MANUAL = {
    "4E": {"fmt": "ddd[#3:dSdddddddd]", "certainty": "high",
           "notes": "emulation stops inside the member loop (stack discipline of an unknown virtual); the single reader "
                    "site 0x1041b37e is inside the loop whose count is the 3rd d (leader, loot, count); read from the code"},
    "5F": {"fmt": "dS", "certainty": "high",
           "notes": "one reader site (dS); the loop after it only searches the engine's own table (no packet reads)"},
    "A7": {"fmt": "d[#1:d{objectId known to the client:ddd}]", "certainty": "medium",
           "notes": "QUIRK: per entry the client reads the object id and ONLY IF that object is in its world map it reads "
                    "x,y,z (3 dwords); unknown objects do not consume the 12 bytes, so a server sending 16 bytes per "
                    "entry desyncs the following entries. min_bytes counts the count only"},
    "C9": {"fmt": "dddSSdSdd{#2=1&#9=0:d[#1:d]}", "certainty": "medium",
           "notes": "the optional tail (count + count dwords of selectable siege times) is read only when the 2nd d == 1 "
                    "and the 9th letter (last d) == 0 (cmp at 0x103ffe34/0x103ffe3a); two-field condition, not found by "
                    "the one-field perturbation"},
    "DD": {"fmt": "ddd[#3:ddddddddffd[20:c]d]", "certainty": "low",
           "notes": "monster race (Derby) packet, never used by a normal server; per runner: 8d + 2f + d + 20 x c + d; the "
                    "count field is one of the first two d (cmp [esp+0x30] at 0x104135d0), not pinned down"},
}


def notify_by_value(em, h, nvt, inc, limit=48):
    """For a handler that dispatches on a value read from the packet (StatusUpdate attribute id): {value: [notify names]}"""
    base = em.run(h, lambda c, f, l, va: 2)
    out = {}
    if not base["trace"]:
        return out
    # the attribute id is the first field of the loop-body site (the second reader site)
    seen_sites = set()
    for t in base["trace"]:
        if t["va"] in seen_sites:
            continue
        seen_sites.add(t["va"])
        for fno, (ch, v) in enumerate(t["fields"]):
            if ch != "d":
                continue
            res = {}
            for tv in range(0, limit):
                r = em.run(h, lambda c, f, l, va, _t=t["va"], _f=fno, _v=tv: _v if (va == _t and f == _f) else 2)
                names = []
                for n in r["notifies"]:
                    nm = slot_name(nvt, inc, n["slot"])
                    if nm not in names:
                        names.append(nm)
                res[tv] = names
            distinct = {n for names in res.values() for n in names}
            if len(distinct) >= 5:
                return {"site": "0x%x" % t["va"], "field": fno + 1, "map": {tv: names for tv, names in res.items() if names}}
    return out



def annotate_blocks(em, h, an, tree, fmt):
    """'b' = raw block whose byte count is passed by the handler; find the packet field it comes from (the
    one whose value change changes the block size) and write it as b<#n> (n-th top-level letter)."""
    pos = {}
    n = 0
    for nd in tree:
        if nd["t"] == "f":
            t = an["site"][nd["va"]]
            for fno in range(len(t["fmt"])):
                n += 1
                pos[(t["va"], fno)] = n
    base = an["base"]["trace"]

    def bsizes(over):
        r = em.run(h, lambda c, f, l, va: over.get((va, f), 2))
        return [v[1] for t in r["trace"] for (ch, v) in t["fields"] if ch == "b"]

    ref = bsizes({})
    out = fmt
    delta = 0
    for k in range(len(ref)):
        src = None
        for (va, fno), p in pos.items():
            ch = an["site"][va]["fmt"][fno]
            if ch not in "chd":
                continue
            r = bsizes({(va, fno): 3})
            if len(r) == len(ref) and r[k] != ref[k]:
                src = p
                break
        if src is not None:
            out = out.replace("b", "b<#%d>" % src, 1)
            delta -= ref[k]
        else:
            out = out.replace("b", "b<%d>" % ref[k], 1)
        break    # only the first block is annotated (no handler has more)
    return out, delta


def make_table(img: Image, spec: dict, use_emu=True, only=None, verbose=False):
    prim, ext = build(img)
    nvt = img.notify_vtable()
    inc = load_vt_inc()
    vslots = load_vslot_names()
    em = Emu(img) if (use_emu and HAVE_UNICORN) else None
    entries = []   # (key, name, real, state)
    entries.append(("00", None, img.follow_thunk(img.u32(PRIMARY_TABLE)), "game"))   # statically initialised entry
    for i, (th, real, name, st) in sorted(prim.items()):
        entries.append(("%02X" % i, name, real, "game"))
    for i, (th, real, name, st) in sorted(ext.items()):
        entries.append(("FE:%04X" % i, name, real, "game"))
    for k, real in login_table(img).items():
        entries.append((k, "LoginState%s" % k[2:], real, "login"))
    table = {}
    for key, name, real, state in entries:
        if only and key != only:
            continue
        h = Handler(img, real)
        nots_static = []
        for n in h.notifies:
            coff, cname = console_target(img, n["slot"], vslots)
            nots_static.append({"slot": n["slot"], "off": "0x%x" % n["off"], "name": slot_name(nvt, inc, n["slot"]),
                                "sig": inc.get(n["slot"], (None, ""))[1], "call_va": "0x%x" % n["va"],
                                "console_vslot_off": ("0x%x" % coff) if coff else None, "nwindow": cname})
        ent = {
            "engine_name": name if state == "game" else None,
            "spec_name": spec.get(key),
            "fmt": None,
            "prefix": "ch" if key.startswith("FE:") else "c",
            "handler_rva": "0x%x" % (real - img.base),
            "handler_va": "0x%x" % real,
            "handler_size": h.end - h.start,
            "notify": None,
            "notifies": nots_static,
            "nwindow": None,
            "certainty": "low",
            "notes": "",
            "state": state,
        }
        notes = []
        fmt = None
        if em is not None:
            try:
                an = analyse_handler(em, h)
                base = an["base"]
                tree = build_tree(an)
                fmt, minb, loops = render_tree(tree, an["site"])
                if "b" in fmt:
                    fmt, dmin = annotate_blocks(em, h, an, tree, fmt)
                    minb += dmin
                ent["min_bytes"] = minb
                ent["loops"] = loops
                ex = {t["va"] for t in base["trace"]} | set(an["site"])
                missing = [r for r in h.readers if r["va"] not in ex]
                clean = base["state"]["ended"] and base["state"]["esp_ok"]
                if missing:
                    notes.append("static reader sites never reached by the emulation: %s (format may be incomplete)" %
                                 ", ".join("0x%x:%s" % (r["va"], r["fmt"]) for r in missing))
                if not clean:
                    notes.append("emulation ended early (%s) after %d reader calls: tail of the handler not executed" %
                                 (base["state"]["err"] or "stack mismatch", len(base["trace"])))
                # dynamic notify order
                dyn = []
                for n in base["notifies"]:
                    nm = slot_name(nvt, inc, n["slot"])
                    if nm and nm not in dyn:
                        dyn.append(nm)
                ent["notify"] = dyn[0] if dyn else (nots_static[0]["name"] if nots_static else None)
                ent["certainty"] = "high" if (not missing and clean) else ("medium" if not missing else "low")
                ent["emulated"] = True
            except Exception as e:  # keep going
                notes.append("emulation failed: %r" % (e,))
                fmt = None
        if fmt is None:
            fmt = static_fmt(h)
            ent["emulated"] = False
            ent["notify"] = nots_static[0]["name"] if nots_static else None
            ent["certainty"] = "low" if any(r["loop"] for r in h.readers) else "medium"
            notes.append("static pass only: [..] = reader call inside a loop (count not identified)")
        if key in MANUAL and state == "game":
            fmt = MANUAL[key]["fmt"]
            ent["certainty"] = MANUAL[key]["certainty"]
            notes.append("MANUAL: " + MANUAL[key]["notes"])
            ent.pop("loops", None)
        ent["fmt"] = fmt
        ent["fmt_has_opcode"] = False
        ent["wire_fmt"] = ent["prefix"] + fmt
        ent["nwindow"] = None
        for n in nots_static:
            if n["name"] == ent["notify"]:
                ent["nwindow"] = n["nwindow"]
                break
        if em is not None and len({n["slot"] for n in h.notifies}) >= 8:
            sw = notify_by_value(em, h, nvt, inc)
            if sw:
                ent["switch_notify"] = sw
                notes.append("dispatches on the value of field %d of site %s: see switch_notify (value -> notify)" %
                             (sw["field"], sw["site"]))
        if not h.readers:
            notes.append("handler reads nothing from the packet payload (any payload is ignored)")
        if ent["spec_name"] and name and norm(ent["spec_name"]) != norm(name):
            notes.append("SPEC NAME DIFFERS: engine '%s' vs spec '%s'" % (name, ent["spec_name"]))
        ent["notes"] = "; ".join(notes)
        table[key] = ent
        if verbose:
            print(key, name, fmt, file=sys.stderr)
    return table


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--engine", type=Path, default=DEFAULT_ENGINE, required=DEFAULT_ENGINE is None, help="default: $NWD_C3_SYSTEM/Engine.dll")
    ap.add_argument("--spec", type=Path, default=DEFAULT_SPEC, help="default: $NWD_C3_SPEC")
    ap.add_argument("--opcode", help="only this opcode, hex: 9A, or FE:0003 for an extended one, L:03 for a login-state one")
    ap.add_argument("--json", type=Path, help="write the table as JSON")
    ap.add_argument("--no-emu", action="store_true", help="static pass only (no unicorn)")
    ap.add_argument("--full", action="store_true", help="print every field of each entry")
    ap.add_argument("--protocol", type=int, default=PROTOCOL_VERSION,
                    help="protocol version the emulated client sent in its first packet (CharInfo/UserInfo/NpcInfo read "
                         "extra trailing fields when it is > 420; C3 = 530)")
    a = ap.parse_args()
    globals()["PROTOCOL_VERSION"] = a.protocol
    img = Image(a.engine)
    spec = load_spec(a.spec)
    only = None
    if a.opcode:
        only = a.opcode.upper()
        if only.startswith("FE:"):
            only = "FE:%04X" % int(only[3:], 16)
        elif not only.startswith("L:"):
            only = "%02X" % int(only, 16)
    table = make_table(img, spec, use_emu=not a.no_emu, only=only)
    if a.json:
        a.json.parent.mkdir(parents=True, exist_ok=True)
        a.json.write_text(json.dumps(table, indent=1, ensure_ascii=False), encoding="utf-8")
        print("wrote", a.json, len(table), "entries", file=sys.stderr)
    for k, e in table.items():
        if a.full:
            print(k, json.dumps(e, indent=1, ensure_ascii=False))
        else:
            print("%-7s %-34s %-24s %-10s %-4s %-62s %s" % (k, e["engine_name"] or "-", e["spec_name"] or "-", e["handler_rva"],
                                                           e["certainty"], e["fmt"], e["notify"] or "-"))
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except BrokenPipeError:
        pass
