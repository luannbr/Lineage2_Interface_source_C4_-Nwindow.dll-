#!/usr/bin/env python3
"""The checks no judge of the cycle does, in one run (what the game runs found, one by one):

  check_breakers   no hooked function reaches an NW_UNPORTED() int3 (also behind a prologue, also in vtables)
  check_retval     a hook that returns nothing / a byte / not a float while a stock caller reads eax / st0
  check_stubs      a compiled call of a stub that declares other arguments than the stock `ret N` pops
  check_frames     a local struct smaller here than in the stock frame (the /GS cookie dies first)
  size_audit       the stock constructor writes past the size the header asserts
  verify_args      a constant argument of a call that the stock passes (also through a register: `push edi`, edi = 0)
                   and no call of ours matches (NewFrameCtrl(1, 0, 0) where the stock builds (0, 0, 0): a second X)
  check_crt        our code calls a stateful entry point of OUR static C runtime (fread, fopen, ...) on what the
                   stock image's runtime owns (a FILE* from the stock NL2Font::Load faulted the first wave-1 run)
  check_addrs      every Addr(0xVA) literal of src/ that points into .text is a function entry of this image (the first C4 game
                   run died in FL2FontPage: nw::StockFread still had the C3 address of _fread)
  check_rawoffs    a raw byte offset (`nw::Field<T>(x, 0xNN)`, `(BYTE*)x + 0xNN`) in a body is in the list of the ones read against the
                   C4 stock (db/rawoffs_ok.txt): the C3 numbers of ReceiveMacroListInfo zeroed other windows of the heap
  check_stockargs  a call into the stock by address (StockCallAt / StockCall / NewStock literal) pushes as many argument dwords as the
                   stock function pops with its `ret N` (NewStock<NCCheckBox>(0x100058C0, id, 0, 0) left the frame 4 bytes short and
                   crashed the first stage 4 world entry); wide arguments are skipped, the reviewed leftovers are in db/stockargs_ok.txt
  check_literals   every stock address a hooked body names (directly, or through a header helper / stub / macro its closure runs) is one the stock
                   function it replaces uses too; a flag that is not in db/literals_ok.txt (read against the stock) fails. Found the folded-getter / setter
                   bugs of wave C (the buying window's owner and adena setters, NCDeliverWnd::SetTargetID, the macro delete's request, the item use's
                   IsDwarf) that no other judge sees: the C3 -> C4 address map pairs a folded tiny function with a neighbour
  check_vslots     every literal virtual slot (`nw::VirtualCall<R>(obj, N, ...)`) of hooked code is a `call [reg + N * 4]` of the stock function
  check_tiny       a tiny stock function named by address is never read or written outside the object its receiver class says (--strict: BEYOND only)
  vt_audit         the vtable the compiler builds from our headers is the one the headers say (slot by slot): a
                   virtual that is not an override of the ancestor's declaration becomes a new slot at the end
  vt_slots         every slot of OUR vtable of a class whose constructor is hooked reaches the stock slot's function (a hook, an NW_STOCK stub, the
                   compiled method of the hooked function, a forwarder chain) or is in db/vtslots_ok.txt (read against the stock): a compiled copy of a
                   shared / folded stock body (the C3 WindowProc with the enabled test, the manor shop's buttons, a missing closing sound) fails

Prints one line per check and `CHECKS: ALL OK` (exit 0) or the failures (exit 1). Run after tools/cycle.py: it reads
build/bin/Release/NWindowDecomp.{dll,map} and build/smoke_system/NWindowDecomp.log of this tree.

Known and accepted (the compilers differ in the small, text buffers that only get short strings): the four frames
in KNOWN_FRAMES. A new one is a lead to look at, not noise.

usage: python tools/port/checks.py"""
from __future__ import annotations

import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
KNOWN_FRAMES = {"10008d40", "10016b20", "100170c0", "1011eb40",    # the C3 ones (100095d0, 10014870, 10015040, 10109b10) at their C4 addresses
                "10094ed0"}                                       # NCInvenItemWnd::OnLButtonDblClick: the stock frame 0x1C0 = two NDialogInfo (0xA0 each, one
#                                                                   per branch) + TCHAR Name[64] (0x80); ours 0x124 = one 0xA0 slot both branches share + Name[64]
#                                                                   + the cookie (the audit lane's fix of UseItem let the compiler overlap the two disjoint locals)


def run(script: str, *args: str) -> tuple[int, str]:
    p = subprocess.run([sys.executable, str(ROOT / "tools" / "port" / script), *args], cwd=ROOT,
                       capture_output=True, text=True, errors="replace")
    return p.returncode, p.stdout + p.stderr


def main() -> int:
    fails = []

    rc, out = run("check_breakers.py")
    line = out.strip().splitlines()[-1] if out.strip() else "(no output)"
    print("check_breakers:", line)
    if rc:
        fails.append(("check_breakers", out))

    rc, out = run("check_retval.py")
    print("check_retval:  ", out.strip().splitlines()[-1] if out.strip() else "(no output)")
    if rc:
        fails.append(("check_retval", out))

    rc, out = run("check_stubs.py")
    m = re.search(r"^(\d+) compiled call site", out, re.M)
    calls = int(m.group(1)) if m else -1
    print("check_stubs:   ", [l for l in out.splitlines() if "stubs," in l][:1], "compiled calls of a mismatching stub:", calls)
    if calls != 0:
        fails.append(("check_stubs", out))

    rc, out = run("check_frames.py")
    rows = re.findall(r"^\s+([0-9a-f]{8}) (\S+)", out, re.M)
    new = [r for r in rows if r[0] not in KNOWN_FRAMES]
    print("check_frames:  ", "%d flagged, %d new" % (len(rows), len(new)))
    if new:
        fails.append(("check_frames", "\n".join(l for l in out.splitlines() if any(l.strip().startswith(n[0]) for n in new))))

    rc, out = run("size_audit.py")
    print("size_audit:    ", out.strip().splitlines()[-1] if out.strip() else "(no output)")
    if rc:
        fails.append(("size_audit", out))

    rc, out = run("check_crt.py")
    print("check_crt:     ", out.strip().splitlines()[-1] if out.strip() else "(no output)")
    if rc:
        fails.append(("check_crt", out))

    rc, out = run("check_addrs.py")
    print("check_addrs:   ", out.strip().splitlines()[-1] if out.strip() else "(no output)")
    if rc:
        fails.append(("check_addrs", out))

    rc, out = run("check_rawoffs.py")
    print("check_rawoffs: ", out.strip().splitlines()[-1] if out.strip() else "(no output)")
    if rc:
        fails.append(("check_rawoffs", out))

    rc, out = run("check_stockargs.py")
    print("check_stockargs:", out.strip().splitlines()[-1] if out.strip() else "(no output)")
    if rc:
        fails.append(("check_stockargs", out))

    rc, out = run("check_literals.py", "--all", "--quiet")
    print("check_literals:", out.strip().splitlines()[-1] if out.strip() else "(no output)")
    if rc:
        fails.append(("check_literals", out))

    rc, out = run("check_vslots.py", "--strict")
    print("check_vslots:  ", out.strip().splitlines()[-1] if out.strip() else "(no output)")
    if rc:
        fails.append(("check_vslots", out))

    rc, out = run("check_tiny.py", "--strict")
    print("check_tiny:    ", out.strip().splitlines()[-1] if out.strip() else "(no output)")
    if rc:
        fails.append(("check_tiny", out))

    rc, out = run("vt_audit.py", "--quiet")
    print("vt_audit:      ", out.strip().splitlines()[-1] if out.strip() else "(no output)")
    if rc:
        fails.append(("vt_audit", out))

    rc, out = run("vt_slots.py")
    print("vt_slots:      ", out.strip().splitlines()[-1] if out.strip() else "(no output)")
    if rc:
        fails.append(("vt_slots", out))

    rc, out = run("verify_args.py", "--smoke", "--quiet")
    print("verify_args:   ", out.strip().splitlines()[-1] if out.strip() else "(no output)")
    if rc:
        diff_lines = [l for l in out.splitlines() if l.startswith("DIFF")]
        fails.append(("verify_args", chr(10).join(diff_lines) or out))

    if fails:
        print("CHECKS: %d FAILED" % len(fails))
        for name, text in fails:
            print("--- %s" % name)
            print("\n".join(text.strip().splitlines()[:30]))
        return 1
    print("CHECKS: ALL OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
