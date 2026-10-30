#!/usr/bin/env python3
"""The C2 client's server-packet formats, read from the client's Engine.dll.

Engine.dll decodes each server packet with a small format interpreter (rva 0xE0240: c = byte, h = word,
d = dword, f = double, S = wide string, ...) and then calls a virtual of FL2NetNotify (On<Packet>(...)).
For every format string literal this finds the code that pushes it, the notify call that follows in the same
function and prints `On<Name>(args)  <format>`: the layout a server has to send to a C2 client. A packet
shorter than its format makes the interpreter read past the buffer (a GPF in Engine.dll+E028F).

usage: python tools/c2/packet_formats.py [--engine PATH] [--name SUBSTR]"""
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
# NWD_C2_SYSTEM: the C2 client's System folder
DEFAULT_ENGINE = (Path(os.environ["NWD_C2_SYSTEM"]) / "Engine.dll" if os.environ.get("NWD_C2_SYSTEM") else None)
FMT_RE = re.compile(rb"(?<![A-Za-z0-9_])([cdhqfsSbBwxQHD\[\]\(\)0-9]{2,120})\x00")


def notify_slots():
    """{vtable byte offset: 'OnName(args)'} from include/ue2/gen/FL2NetNotify.vt.inc."""
    out = {}
    for line in (ROOT / "include/ue2/gen/FL2NetNotify.vt.inc").read_text(encoding="utf-8").splitlines():
        m = re.match(r"\s*virtual\s+\S.*?\s+(On\w+)\((.*?)\)\s*(?:=\s*0)?;\s*//\s*(\d+)\s+\+0x([0-9a-fA-F]+)", line)
        if m:
            out[int(m.group(4), 16)] = "%s(%s)" % (m.group(1), m.group(2))
    return out


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--engine", type=Path, default=DEFAULT_ENGINE, required=DEFAULT_ENGINE is None, help="default: $NWD_C2_SYSTEM/Engine.dll")
    ap.add_argument("--name", default="")
    a = ap.parse_args()
    pe = pefile.PE(str(a.engine))
    base = pe.OPTIONAL_HEADER.ImageBase
    mem = bytes(pe.get_memory_mapped_image())
    text = next(s for s in pe.sections if s.Name.startswith(b".text"))
    t0, t1 = text.VirtualAddress, text.VirtualAddress + text.Misc_VirtualSize
    slots = notify_slots()
    md = Cs(CS_ARCH_X86, CS_MODE_32)
    md.detail = False

    # format-like strings in the data sections: letters of the packet alphabet, with at least one c/d/h/S
    fmts = {}
    for sec in pe.sections:
        if sec.Name.startswith(b".text"):
            continue
        s0 = sec.VirtualAddress
        blob = mem[s0:s0 + sec.Misc_VirtualSize]
        for m in FMT_RE.finditer(blob):
            s = m.group(1).decode()
            if set(s) & set("cdhSf") and not re.fullmatch(r"[A-Za-z]{2,}", s.replace("d", "").replace("c", "") or "x") is None:
                pass
            if re.fullmatch(r"[cdhqfSbB\[\]\(\)0-9]+", s) and any(ch in s for ch in "dhS") and len(s) >= 3:
                fmts[base + s0 + m.start()] = s

    # push imm32 of each such string, then the next FL2NetNotify call in the following 700 bytes
    rows = []
    for m in re.finditer(rb"\x68(....)", mem[t0:t1], re.S):
        va = struct.unpack("<I", m.group(1))[0]
        if va not in fmts:
            continue
        at = t0 + m.start()
        region = mem[at:at + 700]
        call = None
        for ins in md.disasm(region, base + at):
            if ins.mnemonic == "call" and "+" in ins.op_str and "ptr [" in ins.op_str:
                mm = re.search(r"\[(?:e[a-d]x|e[sd]i|ebp)\s*\+\s*(0x[0-9a-f]+)\]", ins.op_str)
                if mm and int(mm.group(1), 16) in slots:
                    call = slots[int(mm.group(1), 16)]
                    break
            if ins.mnemonic in ("ret", "int3"):
                break
        rows.append((at, fmts[va], call))
    seen = set()
    print("%-9s %-34s %s" % ("code", "format", "FL2NetNotify call that follows (same function)"))
    for at, f, call in sorted(rows):
        if a.name and (not call or a.name.lower() not in call.lower()):
            continue
        key = (f, call)
        if key in seen:
            continue
        seen.add(key)
        print("%08x  %-34s %s" % (base + at, f, call or "-"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
