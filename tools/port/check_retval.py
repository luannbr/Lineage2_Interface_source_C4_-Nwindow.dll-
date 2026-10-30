#!/usr/bin/env python3
"""A hooked function whose return value does not reach the stock callers the way they read it.

verify_hooks compares what a function pops on `ret`, the vtable slots it calls and its imports; it does not
look at the return value. Three ways it goes wrong, all invisible until the game runs:

  void    the hook is declared `void` and leaves whatever eax held, while a stock caller reads eax right after
          the call (`mov edx, [eax]`, `test eax, eax`, `push eax` ...). That is how NCWnd::GetLocalPos /
          GetLocalRect (they return the pointer they were given) broke NCHtmlViewer::ShowWindow at the first
          run in the game.
  narrow  the hook returns bool / BYTE / WORD: only al / ax is set, a stock caller that reads eax gets garbage
          in the upper bits.
  x87     the hook is not declared FLOAT / DOUBLE and the first x87 instruction after a stock call consumes st0.

The stock image is scanned for direct calls (`E8 rel32`) to every hook of src/nwindow. Virtual functions are also
called through the vtable, which is not seen here: a quiet result is not a proof for them.

usage: python tools/port/check_retval.py"""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from nwd.image import Image  # noqa: E402
from capstone import CS_ARCH_X86, CS_MODE_32, Cs  # noqa: E402
from capstone.x86 import X86_REG_AH, X86_REG_AL, X86_REG_AX, X86_REG_EAX  # noqa: E402

EAX = {X86_REG_EAX, X86_REG_AX, X86_REG_AL, X86_REG_AH}
EAX_WIDE = {X86_REG_EAX, X86_REG_AX, X86_REG_AH}
HOOK = re.compile(r"^NW_HOOK_(METHOD|FUNC|VARARGS|METHOD_SIG)\(\s*0x([0-9A-Fa-f]{8})\s*,\s*([^)]*)\)", re.M)
CALL_OP = bytes([0xE8])
DROP = r"\b(virtual|static|inline|FORCEINLINE|__declspec\(\w+\)|NW_NAKED|VARARGS)\b"
NARROW = re.compile(r"^(const\s+)?(bool|BYTE|UBYTE|char|SBYTE|WORD|SWORD|short|unsigned\s+char|unsigned\s+short)$")
FLOATY = re.compile(r"\b(FLOAT|float|DOUBLE|double)\b")
X87_LOAD = ("fld", "fild", "fnstsw", "fnstcw", "fldcw", "fninit", "fnclex", "fnsave", "frstor", "fnop")


def hooks_with_types():
    """va -> (name, file, line, return type) of the hooks whose definition was found"""
    out = {}
    for p in sorted((ROOT / "src" / "nwindow").glob("*.cpp")):
        txt = p.read_text(encoding="utf-8", errors="replace")
        for m in HOOK.finditer(txt):
            va = int(m.group(2), 16)
            args = [a.strip() for a in m.group(3).split(",")]
            if m.group(1) in ("METHOD", "METHOD_SIG", "VARARGS") and len(args) >= 2:
                full = "%s::%s" % (args[0], args[1])
            elif m.group(1) == "FUNC":
                full = args[0]
            else:
                continue
            pat = re.compile(r"^([^\n;{}#]*?)\b" + re.escape(full) + r"\s*\(", re.M)
            found = None
            for d in pat.finditer(txt, 0, m.start()):
                found = d
            if not found:
                continue
            ret = re.sub(DROP, "", found.group(1)).strip()
            out[va] = (full, p.name, txt[:m.start()].count("\n") + 1, ret)
    return out


def main() -> int:
    hooks = hooks_with_types()
    img = Image()
    md = Cs(CS_ARCH_X86, CS_MODE_32)
    md.detail = True
    t_lo, t_hi = img.sect_range(".text")
    code = img.read(t_lo, t_hi - t_lo)
    found = {"void": {}, "narrow": {}, "x87": {}}
    pos = code.find(CALL_OP)
    while pos != -1:
        addr = t_lo + pos
        if pos + 5 <= len(code):
            rel = int.from_bytes(code[pos + 1:pos + 5], "little", signed=True)
            tv = (addr + 5 + rel) & 0xFFFFFFFF
            if tv in hooks:
                ret = hooks[tv][3]
                kind = "void" if ret == "void" else ("narrow" if NARROW.match(ret) else None)
                tail = list(md.disasm(code[pos + 5:pos + 5 + 64], addr + 5))
                if kind:
                    watch = EAX if kind == "void" else EAX_WIDE
                    for y in tail:
                        if y.mnemonic in ("ret", "retn", "jmp", "call") or y.mnemonic.startswith("j"):
                            break
                        try:
                            rd, wr = y.regs_access()
                        except Exception:
                            break
                        if y.mnemonic in ("xor", "sub") and len(y.operands) == 2 and \
                                y.operands[0].type == 1 and y.operands[1].type == 1 and \
                                y.operands[0].reg == y.operands[1].reg:
                            rd = []         # `xor eax, eax` only writes
                        if any(r in watch for r in rd):
                            found[kind].setdefault(tv, []).append(addr)
                            break
                        if any(r in EAX for r in wr):
                            break
                if not FLOATY.search(ret):
                    for y in tail:
                        if y.mnemonic in ("ret", "retn", "jmp", "call") or y.mnemonic.startswith("j"):
                            break
                        if y.mnemonic.startswith("f"):
                            if not y.mnemonic.startswith(X87_LOAD):
                                found["x87"].setdefault(tv, []).append(addr)
                            break
        pos = code.find(CALL_OP, pos + 1)
    total = 0
    for kind, title in (("void", "declared void, a stock caller reads eax after the call"),
                        ("narrow", "returns a byte / word, a stock caller reads eax / ax / ah after the call"),
                        ("x87", "not declared FLOAT / DOUBLE, a stock caller's first x87 instruction uses st0")):
        print("%s:" % title)
        for va, cs in sorted(found[kind].items()):
            name, f, line, ret = hooks[va]
            total += 1
            print("  %08x %-44s %s:%d  returns '%s'  callers %s" %
                  (va, name, f, line, ret, ", ".join("%08x" % c for c in cs[:4])))
    print("%d hooks scanned, %d to look at" % (len(hooks), total))
    return 1 if total else 0


if __name__ == "__main__":
    raise SystemExit(main())
