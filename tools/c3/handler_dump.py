#!/usr/bin/env python3
"""Disassemble the s2c packet handler(s) of the C3 Engine.dll for given opcodes (lane A2 helper).

Why: analysis/c3_s2c_formats.json is an early static pass ("reader formats in address order"). It misses what only the
disassembly shows: the opcode-dependent branches (ShortCutRegister switch on the type), the loop counters, handlers that
read nothing (ChooseInventoryItem), and stub handlers (`xor al,al ; ret`, 3 bytes: MagicList, friend packets...).
This tool prints every instruction of a handler and annotates
    * `push <fmt string>`            -> ; FMT 'dSdddd'   (the format given to the reader at 0x1030537b / 0x103ebff0)
    * `call reader`                  -> ; <<READER>>
    * `call [vtable slot]`           -> ; notify slot N OnXxx   (FL2NetNotify method named through the export table)

usage:
    python tools/c3/handler_dump.py 53 6E            # full listing of opcodes 0x53 and 0x6E
    python tools/c3/handler_dump.py --compact 44     # only readers, jumps, compares, calls, ret (the shape of the format)
    python tools/c3/handler_dump.py --engine PATH 41
Opcodes are hex (as keys of analysis/c3_s2c_formats.json). Extended packets (FE:xxxx) are not covered.
"""
from __future__ import annotations

import argparse
import importlib.util
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("opcodes", nargs="+", help="hex opcodes, e.g. 41 53 6E")
    ap.add_argument("--compact", action="store_true", help="print only readers/jumps/compares/calls/ret")
    ap.add_argument("--engine", help="path of the C3 Engine.dll (default: the one of tools/c3/s2c_formats.py)")
    ap.add_argument("--json", default=str(ROOT / "analysis" / "c3_s2c_formats.json"), help="table giving the handler VAs")
    args = ap.parse_args()

    spec = importlib.util.spec_from_file_location("s2c_formats", ROOT / "tools" / "c3" / "s2c_formats.py")
    m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)
    img = m.Image(Path(args.engine) if args.engine else m.DEFAULT_ENGINE)
    table = json.load(open(args.json))
    nvt = img.notify_vtable()

    for arg in args.opcodes:
        key = arg.upper()
        if key not in table:
            print("===== %s: not in %s" % (key, args.json))
            continue
        e = table[key]
        h = m.Handler(img, int(e["handler_va"], 16))
        print("=====", key, e["engine_name"], e["handler_va"], "size", h.end - h.start)
        for i in h.ins:
            s = "%x: %s %s" % (i.address, i.mnemonic, i.op_str)
            if i.mnemonic == "push" and i.operands and i.operands[0].type == 2:
                v = i.operands[0].imm & 0xFFFFFFFF
                if img.base <= v < img.base + len(img.mem):
                    st = img.cstr(v)
                    if st and m.FMT_OK.match(st):
                        s += "    ; FMT '%s'" % st
                    else:
                        ws = img.wstr(v, 40)
                        if ws and ws.isprintable() and len(ws) > 1:
                            s += "    ; W'%s'" % ws
            if i.mnemonic == "call" and i.operands and i.operands[0].type == 2:
                if i.operands[0].imm in (m.READER_THUNK, m.READER_REAL):
                    s += "    ; <<READER>>"
            if i.mnemonic == "call" and i.operands and i.operands[0].type == 3:
                mm = i.operands[0].mem
                if mm.disp and mm.disp % 4 == 0 and mm.disp // 4 in nvt:
                    s += "    ; notify slot %d %s" % (mm.disp // 4, m.demangle_short(nvt[mm.disp // 4][1]))
            if args.compact and not (i.mnemonic.startswith("j") or i.mnemonic in ("cmp", "test", "ret", "call") or "FMT" in s):
                continue
            print(s)


if __name__ == "__main__":
    main()
