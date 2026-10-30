#!/usr/bin/env python3
"""Our code must not hand the stock image's C runtime objects to OUR C runtime (or the other way round).

NWindowDecomp.dll links its C runtime statically and so does the stock NWindow.dll: two copies, two FILE tables,
two sets of locks, two locale states. Heap is bridged (operator new/delete go through GMalloc), but a FILE*
that stock code opened is only valid for the stock fread/fseek/fclose. The first game run of wave 1 died on
exactly that: a ported FL2FontPage::Load called OUR fread on the stock NL2Font::Load's stream and faulted in
ntdll (the lock of a stream our runtime had never seen).

This lists every direct call from our own object code into one of the C runtime's stateful entry points
(files, descriptors, the process environment, thread start, atexit, locale). Anything it finds needs either
the stock function through `nw::StockFread`-style calls, or proof that the object lives and dies inside our
runtime (a file we opened ourselves and close ourselves).

usage: python tools/port/check_crt.py [--all]     (reads build/bin/Release/NWindowDecomp.{dll,map}); exit 1 on a hit"""
from __future__ import annotations

import argparse
import bisect
import re
import sys
from pathlib import Path

import pefile
from capstone import CS_ARCH_X86, CS_MODE_32, Cs

ROOT = Path(__file__).resolve().parents[2]
MAP = ROOT / "build" / "bin" / "Release" / "NWindowDecomp.map"
DLL = ROOT / "build" / "bin" / "Release" / "NWindowDecomp.dll"

# stateful: the stream / descriptor / process state of a runtime. (A name with a leading underscore as the map writes it.)
STATEFUL = re.compile(
    r"^_?_?(f(read|write|open|close|seek|tell|gets|getc|getwc|puts|putc|putwc|printf|wprintf|scanf|flush|eof|error|"
    r"getpos|setpos|reopen|read|write|setvbuf)|_wfopen|_wfreopen|_fsopen|_wfsopen|_fdopen|_fileno|_open|_wopen|_sopen|"
    r"_close|_read|_write|_lseek|_chsize|_commit|_dup2?|_pipe|_popen|_wpopen|rewind|tmpfile|_tempnam|getenv|_wgetenv|"
    r"_putenv|_wputenv|_beginthread|_beginthreadex|_endthread|atexit|_onexit|setlocale|_wsetlocale|signal|"
    r"strtok|wcstok|_mbstok|setvbuf|setbuf|ungetc|_setmode|_get_osfhandle|_open_osfhandle|__acrt_iob_func|"
    r"__iob_func|exit|_exit|abort|_cexit|_c_exit|system|_wsystem|_stat|_wstat|_access|_waccess|_chdir|_getcwd|"
    r"_mkdir|_rmdir|_unlink|remove|rename|setjmp|longjmp)$")


def load_map():
    """(rva, name, is_crt, obj) of every public symbol in section 1 (.text), sorted."""
    rows = []
    rx = re.compile(r"^\s*0001:([0-9a-f]{8})\s+(\S+)\s+([0-9a-f]{8})\s+(?:[fi]\s+)*(\S+)\s*$")
    for ln in MAP.read_text(encoding="latin-1").splitlines():
        m = rx.match(ln)
        if m:
            rows.append((int(m.group(3), 16) - 0x10000000, m.group(2), ":" in m.group(4), m.group(4)))
    rows.sort()
    return rows


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--all", action="store_true", help="list every CRT entry point called from our code")
    a = ap.parse_args()
    rows = load_map()
    keys = [r[0] for r in rows]
    pe = pefile.PE(str(DLL), fast_load=True)
    text = next(s for s in pe.sections if s.Name.rstrip(b"\0") == b".text")
    base_rva = text.VirtualAddress
    data = text.get_data()
    md = Cs(CS_ARCH_X86, CS_MODE_32)
    md.detail = False

    hits = {}      # crt name -> {caller name: count}
    for i, (rva, name, is_crt, obj) in enumerate(rows):
        if is_crt:
            continue
        end = rows[i + 1][0] if i + 1 < len(rows) else base_rva + len(data)
        if rva < base_rva or end > base_rva + len(data) or end - rva > 0x20000:
            continue
        code = data[rva - base_rva:end - base_rva]
        for ins in md.disasm(code, 0x10000000 + rva):
            if ins.mnemonic not in ("call", "jmp") or not ins.op_str.startswith("0x"):
                continue
            tgt = int(ins.op_str, 16) - 0x10000000
            j = bisect.bisect_right(keys, tgt) - 1
            if j < 0 or rows[j][0] != tgt:
                continue
            t = rows[j]
            if not t[2]:
                continue
            nm = t[1].lstrip("_") if t[1].startswith("_") and not t[1].startswith("__") else t[1]
            if a.all or STATEFUL.match(t[1]) or STATEFUL.match(nm):
                hits.setdefault(t[1], {}).setdefault(name, 0)
                hits[t[1]][name] += 1

    if not hits:
        print("check_crt: no call from our code into a stateful C runtime entry point")
        return 0
    n = 0
    for crt in sorted(hits):
        callers = hits[crt]
        print("  %-22s %3d caller(s): %s" % (crt, len(callers), ", ".join(sorted(callers)[:4])))
        n += len(callers)
    print("check_crt: %d stateful CRT entry point(s) called from our code (%d callers)" % (len(hits), n))
    return 0 if a.all else 1


if __name__ == "__main__":
    sys.exit(main())
