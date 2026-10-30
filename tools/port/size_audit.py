#!/usr/bin/env python3
"""A struct / class whose constructor in this client writes past the size the headers assert.

The headers come from the base (C2) tree: `NW_ASSERT_SIZE(NItemInfo, 0x17A8)`. A client that grew the struct by a
field keeps every offset the votes explain and nothing else notices: the judges compare the code of a function,
not the size of the object it puts on the stack. The first game run that got into the world died on it:
NConsoleWnd::ReceiveSkillList keeps an NItemInfo local, the stock functions it calls fill 0x17AC bytes, our local
is 0x17A8 and the 4 bytes land on the /GS cookie (fastfail 0xc0000409 in __report_gsfailure).

For every class that has an NW_ASSERT_SIZE and a constructor in db/functions.csv this reads the constructor of the
stock image and takes the highest `[this + disp]` it stores to (this = ecx at entry, followed through `mov reg, ecx`,
`push ecx` / `pop reg` and `lea`); when that is at or past the asserted size the class is listed. A constructor
that stores the new field only when asked is invisible here: a lower bound, not a proof.

usage: python tools/port/size_audit.py"""
from __future__ import annotations

import csv
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from nwd.image import Image  # noqa: E402
from capstone import CS_ARCH_X86, CS_MODE_32, CS_OP_MEM, CS_OP_REG, Cs  # noqa: E402
from capstone.x86 import X86_REG_ECX  # noqa: E402

ASSERT = re.compile(r"^NW_ASSERT_SIZE\(\s*(\w+)\s*,\s*(0x[0-9A-Fa-f]+|\d+)\s*\)", re.M)


def asserted_sizes():
    out = {}
    for h in sorted((ROOT / "include" / "nwindow").glob("*.h")):
        for m in ASSERT.finditer(h.read_text(encoding="utf-8", errors="replace")):
            out[m.group(1)] = (int(m.group(2), 0), h.name)
    return out


def ctor_vas():
    """class -> [va of every constructor the database names]"""
    out = {}
    for r in csv.DictReader((ROOT / "db" / "functions.csv").open(encoding="utf-8")):
        name = r["name"]
        if "::" not in name:
            continue
        cls, _, meth = name.rpartition("::")
        cls = cls.split("::")[-1]
        if meth == cls or meth.startswith(cls + "("):
            out.setdefault(cls, []).append(int(r["va"], 16))
    return out


def max_store(img, md, va, limit=900):
    """highest displacement of a write through `this` in the first instructions of the function at va"""
    regs = {X86_REG_ECX}
    top = 0
    n = 0
    data = img.read(va, 0x600)
    for ins in md.disasm(data, va):
        n += 1
        if n > limit:
            break
        if ins.mnemonic in ("ret", "retn"):
            break
        ops = ins.operands
        if ins.mnemonic == "mov" and len(ops) == 2 and ops[0].type == CS_OP_REG and ops[1].type == CS_OP_REG \
                and ops[1].reg in regs:
            regs.add(ops[0].reg)
            continue
        if ins.mnemonic == "lea" and len(ops) == 2 and ops[1].type == CS_OP_MEM and ops[1].mem.base in regs \
                and ops[1].mem.index == 0:
            # lea reg, [this + disp]: a pointer into the object, followed one level (the member's own constructor
            # call writes through it; only the pointer itself is of interest here)
            continue
        if ins.mnemonic == "call":
            continue
        if ops and ops[0].type == CS_OP_MEM and ops[0].mem.base in regs and ops[0].mem.index == 0 \
                and ins.mnemonic in ("mov", "and", "or", "xor", "add", "sub", "movzx", "movsx", "stosd", "inc", "dec",
                                     "fstp", "fst", "fistp"):
            if ins.mnemonic in ("mov", "and", "or", "xor", "add", "sub", "inc", "dec", "fstp", "fst", "fistp"):
                size = ops[0].size or 4
                top = max(top, ops[0].mem.disp + size)
        # a register that is overwritten stops being `this`
        try:
            _, wr = ins.regs_access()
        except Exception:
            wr = []
        for r in wr:
            if r in regs and not (ins.mnemonic == "mov" and len(ops) == 2 and ops[1].type == CS_OP_REG and ops[1].reg in regs):
                if r != X86_REG_ECX or ins.mnemonic not in ("push", "pop"):
                    regs.discard(r)
    return top


def main() -> int:
    img = Image()
    md = Cs(CS_ARCH_X86, CS_MODE_32)
    md.detail = True
    sizes = asserted_sizes()
    ctors = ctor_vas()
    rows = []
    for cls, (size, hdr) in sorted(sizes.items()):
        for va in ctors.get(cls, []):
            try:
                top = max_store(img, md, va)
            except Exception:
                continue
            if top > size:
                rows.append((cls, hdr, size, top, va))
    print("%d classes with an asserted size, %d with a constructor in the database" %
          (len(sizes), sum(1 for c in sizes if c in ctors)))
    seen = set()
    for cls, hdr, size, top, va in rows:
        if cls in seen:
            continue
        seen.add(cls)
        print("  %-28s %-24s asserted 0x%x, constructor %08x writes up to 0x%x (+0x%x)" %
              (cls, hdr, size, va, top, top - size))
    print("%d class(es) to look at" % len(seen))
    return 1 if seen else 0


if __name__ == "__main__":
    raise SystemExit(main())
