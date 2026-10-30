#!/usr/bin/env python3
"""Hooked functions whose stack frame is smaller than the stock one by what looks like a grown struct.

A local struct is the one thing no judge compares: verify_data looks at constants and displacements, and the frame
is whatever each compiler makes of it. When the client grew the struct (NItemInfo 0x17A8 -> 0x17AC) our local is
too small for the stock functions that fill it and the overflow lands on the /GS cookie or the saved registers
(the second game run: NConsoleWnd::ReceiveSkillList, fastfail 0xc0000409).

For every hooked function the frame size is read from both prologues (`sub esp, N` or `mov eax, N / call
__chkstk`). Our frame carries the /GS cookie (4 bytes) when the function has a buffer, the stock one never does,
so with the same locals ours is the stock frame + 4: a function with a cookie whose frame is below that, or one
without a cookie whose frame is below the stock one, has a local that is smaller here than there. The compilers
differ in the small (spills, temporaries), so only frames of 0x100 bytes and more count, and a hit is a lead, not
a verdict: look at the locals of the function.

usage: python tools/port/check_frames.py [--log build/smoke_system/NWindowDecomp.log] [--min 0x200]"""
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

import pefile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from nwd.image import Image  # noqa: E402
from capstone import CS_ARCH_X86, CS_MODE_32, Cs  # noqa: E402

CALL_RE = re.escape(bytes([0xE8])) + b"(....)"
LOG = re.compile(r"^\S+\s+([0-9a-f]{8}) (\S+)\s+ours=([0-9a-f]+) ok")


def frame(md, code, va):
    """stack bytes reserved by the prologue (None when there is no recognisable one)"""
    total = 0
    last_mov = None
    n = 0
    for ins in md.disasm(code, va):
        n += 1
        if n > 40:
            break
        if ins.mnemonic == "sub" and ins.op_str.startswith("esp, "):
            try:
                total += int(ins.op_str.split(",")[1], 0)
            except ValueError:
                pass
            continue
        if ins.mnemonic == "mov" and ins.op_str.startswith("eax, 0x"):
            last_mov = int(ins.op_str.split(",")[1], 0)
            continue
        # a frame probed with __chkstk is at most the stack reserve (1 MB). NWndUtil::MakeTimeStr 0x10037BC0 does `mov eax, 0x88888889`
        # (the reciprocal of the division by 60) before its first import call: that is not a frame (it was read as 0x410 +
        # 0x88888889 = 0x88888c99 for a function whose frame is its `sub esp, 0x410`, and the hook was left off for it)
        if ins.mnemonic == "call" and last_mov is not None and 0x1000 <= last_mov <= 0x100000:
            return total + last_mov
        if ins.mnemonic in ("ret", "jmp"):
            break
        if ins.mnemonic in ("push", "mov", "lea", "xor", "and", "sub", "or"):
            continue
        if ins.mnemonic == "call":
            break
    return total or None


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--log", type=Path, default=ROOT / "build" / "smoke_system" / "NWindowDecomp.log")
    ap.add_argument("--dll", type=Path, default=ROOT / "build" / "bin" / "Release" / "NWindowDecomp.dll")
    ap.add_argument("--min", type=lambda x: int(x, 0), default=0x100)
    a = ap.parse_args()
    img = Image()
    md = Cs(CS_ARCH_X86, CS_MODE_32)
    pe = pefile.PE(str(a.dll))
    base = pe.OPTIONAL_HEADER.ImageBase
    mem = pe.get_memory_mapped_image()
    # the functions of ours that check the /GS cookie: a call to __security_check_cookie
    gs_funcs = set()
    mp = a.dll.with_suffix(".map")
    syms, secchk = [], None
    for ln in mp.read_text(encoding="latin-1").splitlines():
        m = re.match(r"^\s*0001:[0-9a-f]{8}\s+(\S+)\s+([0-9a-f]{8})\s+f", ln)
        if m:
            syms.append(int(m.group(2), 16))
            if m.group(1) == "@__security_check_cookie@4":
                secchk = int(m.group(2), 16)
    syms.sort()
    text = next(s for s in pe.sections if s.Name.startswith(b".text"))
    tlo = base + text.VirtualAddress
    tdata = text.get_data()[:text.Misc_VirtualSize]
    import bisect
    for m in re.finditer(CALL_RE, tdata, re.S):
        if tlo + m.start() + 5 + int.from_bytes(m.group(1), "little", signed=True) == secchk:
            gs_funcs.add(syms[bisect.bisect_right(syms, tlo + m.start()) - 1])
    rows = []
    for ln in a.log.read_text(encoding="latin-1").splitlines():
        m = LOG.match(ln)
        if not m:
            continue
        sva, name, ours = int(m.group(1), 16), m.group(2), int(m.group(3), 16)
        fs = frame(md, img.read(sva, 0x80), sva)
        fo = frame(md, bytes(mem[ours:ours + 0x80]), base + ours)
        if fs is None or fs < a.min:
            continue
        if fo is None:
            continue
        need = fs + (4 if (base + ours) in gs_funcs else 0)
        if fo < need:
            rows.append((fs, fo, name, sva, (base + ours) in gs_funcs))
    rows.sort(reverse=True)
    print("%d hooked function(s) with a stock frame of 0x%x bytes or more and ours smaller than stock (+4 with a cookie)" %
          (len(rows), a.min))
    for fs, fo, name, sva, gs in rows:
        print("  %08x %-52s stock 0x%x ours 0x%x%s" % (sva, name[:52], fs, fo, " (/GS)" if gs else ""))
    return 1 if rows else 0


if __name__ == "__main__":
    raise SystemExit(main())
