#!/usr/bin/env python3
"""The NWindow -> Engine boundary, stock against ours: the L2ParamStack contents and the NetHandler slot.

The other judges compare the SHAPE of a hooked function (calls, constants, vtable displacements); none of them reads
WHICH VALUES are pushed onto an L2ParamStack, in WHICH ORDER, or which Engine->NetHandler virtual gets them. This tool
runs a symbolic forward dataflow over both the stock function and our compiled one (capstone, same CFG machinery as
tools/port/verify_args.py) and builds, per function:

  * the L2ParamStack records: constructor argument, the ordered list of PushBack() values (symbolic: `a1`, `[this+0x39c8]`,
    `[[a0+0x10]+0x1c]`, `0x5`, `ret(1006a2b0)`...), and the call(s) that take the stack by reference (the consumers);
  * every call: callee key (stock VA, import, vtable offset), `this` expression and the pushed arguments;
  * the Engine->NetHandler virtual calls (`this` expression ends in +0x58]): the vtable offset.

and reports, for every hooked function that touches the boundary on either side:
  PUSH  the pushed values/order of a record differ (or a record exists on one side only)
  VSLOT a NetHandler offset is called by one side only
  ARG   a call both sides make has different symbolic arguments (wildcard `?` matches anything)

Expressions are text; the two compilers allocate registers differently but the loads from `this` / arguments / the
engine are the same, so a faithful port gives equal text. A DIFF is a reason to read both disassemblies, not a verdict.
`--vtable` also lists every NetHandler slot the STOCK image calls with the UNetworkHandler method name Engine.dll
gives that slot (C3 Engine.dll, read only).

  python tools/c3/net_boundary.py [--only HEXVA ...] [--all] [--vtable] [--md docs/notas/C3_Pacotes_N.md] [-v]
Output: analysis/c3_net_boundary.json (per function records of both sides), the table in --md.
"""
from __future__ import annotations

import argparse
import bisect
import csv
import ctypes
import json
import re
import os
import sys
from collections import Counter, defaultdict
from pathlib import Path

import pefile
from capstone import CS_ARCH_X86, CS_MODE_32, Cs

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import verify_hooks as vh  # noqa: E402
from nwd.image import Image  # noqa: E402

# NWD_C3_SYSTEM: the C3 client's System folder
ENGINE_DLL = (Path(os.environ["NWD_C3_SYSTEM"]) / "Engine.dll" if os.environ.get("NWD_C3_SYSTEM") else None)
CTOR = "??0L2ParamStack@@QAE@H@Z"
DTOR = "??1L2ParamStack@@QAE@XZ"
PUSHBACK = "?PushBack@L2ParamStack@@QAEHPAX@Z"
NH_RE = re.compile(r"\+0x58\]$")          # Engine->NetHandler: [engine + 0x58]

REG32 = ("eax", "ebx", "ecx", "edx", "esi", "edi", "ebp")
PARENT = {"al": "eax", "ah": "eax", "ax": "eax", "bl": "ebx", "bh": "ebx", "bx": "ebx", "cl": "ecx", "ch": "ecx",
          "cx": "ecx", "dl": "edx", "dh": "edx", "dx": "edx", "si": "esi", "di": "edi", "bp": "ebp"}
PARENT.update({r: r for r in REG32})
PARENT["esp"] = "esp"
NOWRITE = {"cmp", "test", "nop", "jmp", "ret", "retn", "fcom", "fcomp", "fucom", "fucomp", "fnstsw", "fnstcw",
           "fldcw", "fxch", "fucompp", "fcompp", "int3"}
MEMOP = re.compile(r"^(?:(byte|word|dword|qword) ptr )?\[(.*)\]$")
TERM = re.compile(r"([+-])\s*([^+-]+)")


def h(v: int) -> str:
    return "%d" % v if 0 <= v < 10 else ("0x%x" % v if v >= 0 else "-0x%x" % -v)


def imm(s: str):
    s = s.strip()
    try:
        return int(s, 0) & 0xFFFFFFFF
    except ValueError:
        return None


# ---------------------------------------------------------------------------------------------------------------
class Machine:
    """one image: how to read it, its imports, how to resolve a direct call target to a key"""

    def __init__(self, code, resolve, ret_n, side):
        self.code, self.resolve, self.ret_n, self.side = code, resolve, ret_n, side


def render_key(k, names):
    if k is None:
        return "?"
    if k[0] == "va":
        return names.get(k[1], "%08x" % k[1])
    if k[0] == "imp":
        return "import " + str(k[1])
    if k[0] == "vt":
        return "vt+0x%x" % k[1]
    return str(k)


class State:
    __slots__ = ("r", "sp", "mem", "run", "fp")

    def __init__(self):
        self.r = {"ecx": "this"}
        self.sp = 0
        self.mem = {}
        self.run = ()
        self.fp = None

    def copy(self):
        s = State()
        s.r, s.sp, s.mem, s.run, s.fp = dict(self.r), self.sp, dict(self.mem), self.run, self.fp
        return s


def phi(x, y):
    if x == y:
        return x
    if x is None or y is None:
        return None
    alts = set()
    for v in (x, y):
        if v.startswith("phi("):
            alts.update(v[4:-1].split("|"))
        else:
            alts.add(v)
    allptr = all(re.match(r"^0x1[0-9a-f]{7}$", a) for a in alts)       # merged tails call one of several functions
    if len(alts) > (16 if allptr else 3):
        return None
    return "phi(%s)" % "|".join(sorted(alts))


def join(a: State, b: State) -> State:
    s = State()
    for k in set(a.r) & set(b.r):
        v = phi(a.r[k], b.r[k])
        if v is not None:
            s.r[k] = v
    s.sp = a.sp
    for k in set(a.mem) & set(b.mem):
        v = phi(a.mem[k], b.mem[k])
        if v is not None:
            s.mem[k] = v
    n = min(len(a.run), len(b.run))
    s.run = tuple(phi(x, y) for x, y in zip(a.run[:n], b.run[:n]))
    s.fp = a.fp if a.fp == b.fp else None
    return s


def same(a: State, b: State) -> bool:
    return a.r == b.r and a.mem == b.mem and a.run == b.run and a.fp == b.fp


def cap(v):
    return v if v is None or len(v) < 120 else None


def eval_addr(inner: str, st: State):
    """-> ('stack', absolute address) | ('mem', text) | None for the inside of a memory operand"""
    terms = []
    sign = "+"
    s = inner.replace(" ", "")
    if not s.startswith(("+", "-")):
        s = "+" + s
    for m in re.finditer(r"([+-])([^+-]+)", s):
        terms.append((m.group(1), m.group(2)))
    base = None
    disp = 0
    parts = []
    for sg, t in terms:
        k = 1 if sg == "+" else -1
        if "*" in t:
            reg, sc = t.split("*")
            v = st.r.get(reg)
            if v is None:
                return None
            parts.append("%s*%s" % (v, sc))
            continue
        n = imm(t)
        if n is not None:
            disp += k * (n if n < 0x80000000 else n - (1 << 32))
            continue
        if t == "esp":
            v = "SP:%d" % st.sp
        else:
            v = st.r.get(PARENT.get(t, t)) if t in PARENT else None
        if v is None:
            return None
        if k < 0:
            return None
        if base is None:
            base = v
        else:
            parts.append(v)
    if base is None and not parts:
        return ("abs", disp)
    if base == "NWB" and not parts:
        return ("sabs", 0x10000000 + disp)
    if base is not None and base.startswith("SP:") and not parts:
        return ("stack", int(base[3:]) + disp)
    txt = base if base is not None else ""
    for p in parts:
        txt += ("+" if txt else "") + p
    if disp:
        txt += ("+" if disp > 0 else "-") + h(abs(disp))
    return ("mem", txt)


def load(op: str, st: State, pfx: str = ""):
    m = MEMOP.match(op)
    if not m:
        return None
    sz = m.group(1) or "dword"
    a = eval_addr(m.group(2), st)
    p = {"byte": "b", "word": "w", "qword": "q", "dword": ""}[sz] + pfx
    if a is None:
        return None
    if a[0] == "stack":
        v = st.mem.get(a[1])
        if v is not None and not p:
            return v
        if v is not None:
            return "%s(%s)" % (p, v)
        if a[1] >= 4 and (a[1] - 4) % 4 == 0 and not p:
            return "a%d" % ((a[1] - 4) // 4)
        return "L%d" % a[1] if not p else None
    if a[0] == "sabs":
        return "%s[G@%08x]" % (p, a[1] & 0xFFFFFFFF)
    if a[0] == "abs":
        ab = a[1] & 0xFFFFFFFF
        if SIDE[0] == "ours":
            return "NWB" if ab == NWBASE[0] else "%s[G]" % p
        if 0x10000000 <= ab < 0x20000000:
            return "%s[G@%08x]" % (p, ab)
        return "%s[G]" % p
    return cap("%s[%s]" % (p, a[1]))


def store(op: str, val, st: State):
    m = MEMOP.match(op)
    if not m:
        return
    a = eval_addr(m.group(2), st)
    if a and a[0] == "stack" and (m.group(1) or "dword") != "dword":
        st.mem.pop(a[1], None)
    elif a and a[0] == "stack":
        if val is None:
            st.mem.pop(a[1], None)
        else:
            st.mem[a[1]] = val


def setreg(st, dst, v):
    r = PARENT.get(dst)
    if r is None:
        return
    if dst != r or v is None:
        st.r.pop(r, None)
        return
    st.r[r] = cap(v)


def regval(st, op):
    """value of an operand: imm, 32-bit reg, memory"""
    n = imm(op)
    if n is not None:
        return h(n)
    if op in REG32:
        return st.r.get(op)
    if op == "esp":
        return "SP:%d" % st.sp
    return load(op, st)


def binop(mn, a, b):
    if a is None or b is None:
        return None
    sym = {"add": "+", "sub": "-", "imul": "*", "and": "&", "or": "|", "xor": "^", "shl": "<<", "shr": ">>",
           "sar": ">>"}.get(mn)
    if sym is None:
        return None
    if a.startswith("SP:") and b.lstrip("-").startswith(("0x", "1", "2", "3", "4", "5", "6", "7", "8", "9", "0")):
        n = imm(b)
        if n is not None and mn in ("add", "sub"):
            return "SP:%d" % (int(a[3:]) + (n if mn == "add" else -n))
    if mn == "add":
        # flatten and sort the terms (the two compilers add in different orders), folding the constants
        ts = sum_terms(a) + sum_terms(b)
        k = 0
        rest = []
        for t in ts:
            n = imm(t)
            if n is not None:
                k += n if n < 0x80000000 else n - (1 << 32)
            else:
                rest.append(t)
        rest.sort()
        if k:
            rest.append(h(k) if k > 0 else "-" + h(-k))
        if len(rest) == 1:
            return rest[0]
        if not rest:
            return "0"
        return "(%s)" % "+".join(rest)
    if mn in ("imul", "and", "or", "xor") and a > b:
        a, b = b, a
    return "(%s%s%s)" % (a, sym, b)


def sum_terms(v):
    """top-level terms of a `(a+b+c)` sum made by binop, else the value itself"""
    if v.startswith("(") and v.endswith(")"):
        inner = v[1:-1]
        depth = 0
        terms, cur = [], ""
        for ch in inner:
            if ch in "([":
                depth += 1
            elif ch in ")]":
                depth -= 1
            if ch == "+" and depth == 0:
                terms.append(cur)
                cur = ""
            else:
                cur += ch
        terms.append(cur)
        if len(terms) > 1 and all(t for t in terms):
            return terms
    return [v]


class Fn:
    """analysis result of one function: events in address order"""

    def __init__(self):
        self.events = []          # dicts
        self.drift = 0
        self.unresolved = 0
        self.ninstr = 0


def analyze(md, mach: Machine, start: int, names, depth=0):
    code = mach.code
    SIDE[0] = mach.side
    extent = code.extent(start)
    ins_list = []
    for a, b in extent:
        ins_list.extend(vh.decode_skipping_tables(md, code, a, b))
    fn = Fn()
    if not ins_list:
        return fn
    fn.ninstr = len(ins_list)
    by_addr = {i.address: n for n, i in enumerate(ins_list)}

    def inside(t):
        return any(a <= t < b for a, b in extent)

    succ = {}
    for n, ins in enumerate(ins_list):
        nxt = [ins_list[n + 1].address] if n + 1 < len(ins_list) else []
        mn, op = ins.mnemonic, ins.op_str
        if mn in ("ret", "retn", "int3", "ud2"):
            nxt = []
        elif mn == "jmp":
            if op.startswith("0x"):
                t = int(op, 16)
                nxt = [t] if inside(t) else []
            else:
                m = re.match(r"^dword ptr \[e[a-ds][xip]\*4 \+ (0x[0-9a-f]+)\]$", op)
                nxt = []
                if m:
                    t = int(m.group(1), 16)
                    for _ in range(300):
                        v = int.from_bytes(code.read(t, 4), "little")
                        if not inside(v):
                            break
                        nxt.append(v)
                        t += 4
        elif mn.startswith("j") and op.startswith("0x"):
            t = int(op, 16)
            if inside(t):
                nxt.append(t)
        succ[ins.address] = nxt

    def callkey(ins, st):
        mn, op = ins.mnemonic, ins.op_str
        if op.startswith("0x"):
            return mach.resolve(int(op, 16)), None
        m = MEMOP.match(op)
        if m:
            a = eval_addr(m.group(2), st)
            if a and a[0] == "abs":
                ab = a[1] & 0xFFFFFFFF
                if ab in code.imports:
                    return ("imp", code.imports[ab]), None
            if a and a[0] == "stack":
                v = st.mem.get(a[1])               # a virtual's address spilled to a stack slot earlier
                if v and v.startswith("[") and v.endswith("]"):
                    mv = re.match(r"^\[(.*)\+(0x[0-9a-f]+|\d+)\]$", v)
                    if mv:
                        return ("vt", int(mv.group(2), 0)), st.r.get("ecx")
                if v and v.startswith("IMP:"):
                    return ("imp", v[4:]), None
            mm = re.match(r"^dword ptr \[(e[a-ds][xip]|e[sd]i|ebp)(?: \+ (0x[0-9a-f]+))?\]$", op)
            if mm:
                return ("vt", int(mm.group(2), 16) if mm.group(2) else 0), st.r.get("ecx")
            return None, None
        if op in REG32:
            v = st.r.get(op)
            mb = re.match(r"^\(NWB\+(0x[0-9a-f]+)\)$", v or "")
            if mb:                                   # StockCallAt: g_nwBase + (va - 0x10000000)
                return ("va", 0x10000000 + int(mb.group(1), 16)), None
            if v and v.startswith("phi(") and SIDE[0] == "ours":
                alts = v[4:-1].split("|")
                if all(re.match(r"^0x1[0-9a-f]{7}$", x) for x in alts):
                    return ("multi", tuple(int(x, 16) for x in alts)), None     # merged tails: one call, several targets
            if v and re.match(r"^0x1[0-9a-f]{7}$", v) and SIDE[0] == "ours":
                return ("va", int(v, 16)), None      # StockCallAt(va, ...) after the compiler folded base + offset
            if v and v.startswith("IMP:"):
                return ("imp", v[4:]), None
            if v and v.startswith("[") and v.endswith("]"):
                # a vtable slot loaded earlier: [[X+0]+0x298]
                mv = re.match(r"^\[(.*)\+(0x[0-9a-f]+|\d+)\]$", v)
                if mv:
                    inner = mv.group(1)
                    mi = re.match(r"^\[(.*)\+0\]$", inner) or re.match(r"^\[(.*)\]$", inner)
                    return ("vt", int(mv.group(2), 0)), (mi.group(1) if mi else None)
        return None, None

    def step(ins, st: State, record=None):
        mn, op = ins.mnemonic, ins.op_str
        parts = [p.strip() for p in op.split(",", 1)] if op else []
        if mn == "push":
            v = regval(st, op) if (op in REG32 or imm(op) is not None or MEMOP.match(op)) else None
            if v is not None and v.startswith("SP:"):
                pass
            st.sp -= 4
            st.mem[st.sp] = v if v is not None else None
            if v is None:
                st.mem.pop(st.sp, None)
            if by_addr[ins.address] < 16 and op in ("ebx", "ebp", "esi", "edi"):
                return                      # a callee-saved register saved in the prologue
            st.run = st.run + (v,)
            return
        if mn == "pop":
            v = st.mem.get(st.sp)
            st.sp += 4
            st.run = st.run[:-1]
            if parts and parts[0] in PARENT:
                setreg(st, parts[0], v)
            return
        if mn in ("call",):
            key, thisx = callkey(ins, st)
            nxt = ins_list[by_addr[ins.address] + 1] if by_addr[ins.address] + 1 < len(ins_list) else None
            cdecl = bool(nxt and nxt.mnemonic == "add" and nxt.op_str.startswith("esp, "))
            run = st.run
            ea = st.r.get("eax")
            if ins.op_str.startswith("0x") and ea and re.match(r"^0x[0-9a-f]{4,}$", ea) and int(ea, 16) >= 0x1000                     and not run and (key is None or key[0] in ("our", "va")) and nxt is not None:
                st.sp -= int(ea, 16)         # __chkstk / alloca_probe: it lowers esp by eax itself
                st.r.pop("eax", None)
                return
            if record:
                record(ins, key, thisx, run, st)
            npop = None
            if key and key[0] == "va":
                nb = mach.ret_n(key[1])
                npop = nb // 4 if nb is not None else None
            elif key and key[0] == "imp":
                npop = IMP_POP.get(key[1])
            if cdecl:
                pass
            else:
                n = len(run) if npop is None else npop
                st.sp += 4 * n
                for _ in range(n):
                    pass
            if any(x and x.startswith("SP:") for x in run) or (st.r.get("ecx") or "").startswith("SP:"):
                st.mem = {k: v for k, v in st.mem.items() if k >= 0}      # the callee may have written through it
            st.run = ()
            ret = None
            if key is not None and key[0] != "multi":
                ret = "ret(%s)" % (("%08x" % key[1]) if key[0] == "va" else ("vt%x" % key[1] if key[0] == "vt" else
                                    (key[1][:24] if isinstance(key[1], str) else "our%x" % key[1])))
            for r in ("eax", "ecx", "edx"):
                st.r.pop(r, None)
            if ret:
                st.r["eax"] = ret
            st.fp = None
            return
        if mn == "ret" or mn == "retn":
            if st.sp != 0:
                fn.drift += 1
            return
        if mn == "leave":
            b = st.r.get("ebp")
            if b and b.startswith("SP:"):
                st.sp = int(b[3:])
            v = st.mem.get(st.sp)
            st.sp += 4
            setreg(st, "ebp", v)
            return
        if mn == "sub" and op.startswith("esp, "):
            n = imm(op.split(",")[1])
            if n is not None:
                st.sp -= n
            return
        if mn == "add" and op.startswith("esp, "):
            n = imm(op.split(",")[1])
            if n is not None:
                st.sp += n
                st.run = st.run[:max(0, len(st.run) - n // 4)]
            return
        if mn == "mov" and parts and parts[0] == "esp":
            v = regval(st, parts[1])
            if v and v.startswith("SP:"):
                st.sp = int(v[3:])
            return
        # ---- fpu: only enough to carry a float argument to the stack ----
        if mn == "fld":
            st.fp = load(op, st, "f") if MEMOP.match(op) else None
            return
        if mn == "fld1":
            st.fp = "1.0"
            return
        if mn == "fldz":
            st.fp = "0.0"
            return
        if mn in ("fstp", "fst"):
            if MEMOP.match(op):
                store(op, st.fp, st)
            if mn == "fstp":
                st.fp = None
            return
        if mn.startswith("f"):
            if mn not in ("fcom", "fucom", "fnstsw", "fnstcw", "fldcw", "fxch"):
                st.fp = None
            return
        if mn.startswith("rep") or mn in ("stosd", "stosb", "movsd", "movsb", "movsw", "scasb", "lodsd", "cmpsb"):
            for r in ("eax", "ecx", "esi", "edi"):
                st.r.pop(r, None)
            return
        if mn in ("cdq", "cwde", "cwd", "cbw") or (mn in ("mul", "div", "idiv", "imul") and len(parts) <= 1):
            st.r.pop("eax", None)
            st.r.pop("edx", None)
            return
        if mn in NOWRITE or mn.startswith("j") or not parts:
            return
        dst = parts[0]
        if mn == "mov" and dst.endswith("fs:[0]") or "fs:[0x0]" in dst:
            st.run = ()                     # the SEH frame is linked: everything pushed so far is the prologue's
            return
        if MEMOP.match(dst):
            if mn == "mov" and len(parts) == 2:
                store(dst, regval(st, parts[1]), st)
            else:
                m = MEMOP.match(dst)
                a = eval_addr(m.group(2), st)
                if a and a[0] == "stack":
                    st.mem.pop(a[1], None)
            return
        if dst not in PARENT:
            return
        if mn == "xor" and len(parts) == 2 and parts[1] == dst:
            setreg(st, dst, "0")
            return
        if mn == "mov" and len(parts) == 2:
            src = parts[1]
            if dst in REG32:
                m = MEMOP.match(src)
                if m:
                    a = eval_addr(m.group(2), st)
                    if a and a[0] == "abs" and (a[1] & 0xFFFFFFFF) in code.imports:
                        st.r[dst] = "IMP:" + code.imports[a[1] & 0xFFFFFFFF]
                        return
                setreg(st, dst, regval(st, src))
            else:
                setreg(st, dst, None)
            return
        if mn in ("movzx", "movsx") and len(parts) == 2:
            if MEMOP.match(parts[1]):
                v = load(parts[1], st, "")
            else:
                v = st.r.get(PARENT.get(parts[1], "")) if parts[1] in PARENT else None
            setreg(st, dst, v)
            return
        if mn == "lea" and len(parts) == 2:
            m = MEMOP.match(parts[1])
            a = eval_addr(m.group(2), st) if m else None
            if a is None:
                setreg(st, dst, None)
            elif a[0] == "stack":
                setreg(st, dst, "SP:%d" % a[1])
            elif a[0] in ("abs", "sabs"):
                setreg(st, dst, h(a[1] & 0xFFFFFFFF))
            else:
                mm = re.match(r"^(.*)\+(0x[0-9a-f]+)$", a[1])
                if mm and "*" not in mm.group(1):
                    setreg(st, dst, binop("add", mm.group(1), mm.group(2)))
                else:
                    setreg(st, dst, "(%s)" % a[1])
            return
        if mn in ("add", "sub", "and", "or", "xor", "imul", "shl", "shr", "sar") and len(parts) == 2 and dst in REG32:
            setreg(st, dst, binop(mn, st.r.get(dst), regval(st, parts[1])))
            return
        if mn in ("inc", "dec") and dst in REG32:
            setreg(st, dst, binop("add" if mn == "inc" else "sub", st.r.get(dst), "1"))
            return
        if mn == "xchg":
            setreg(st, dst, None)
            if len(parts) == 2 and parts[1] in PARENT:
                setreg(st, parts[1], None)
            return
        setreg(st, dst, None)

    # ---- dataflow ----
    state_in = {ins_list[0].address: State()}
    work = [ins_list[0].address]
    budget = 80 * len(ins_list)
    while work and budget > 0:
        budget -= 1
        a = work.pop()
        n = by_addr.get(a)
        if n is None:
            continue
        st = state_in[a].copy()
        step(ins_list[n], st)
        for s in succ.get(a, ()):
            if s not in by_addr:
                continue
            if s not in state_in:
                state_in[s] = st.copy()
                work.append(s)
            else:
                j = join(state_in[s], st)
                if not same(j, state_in[s]):
                    state_in[s] = j
                    work.append(s)

    # ---- events, in address order ----
    for n, ins in enumerate(ins_list):
        st = state_in.get(ins.address)
        if st is None:
            continue
        if ins.mnemonic not in ("call", "jmp"):
            continue
        if ins.mnemonic == "jmp" and ins.op_str.startswith("0x") and inside(int(ins.op_str, 16)):
            continue
        st = st.copy()

        def rec(i, key, thisx, run, s):
            ec = s.r.get("ecx")
            if key is not None and key[0] == "vt":
                ec = thisx if thisx is not None else ec
            if key is not None and key[0] == "multi":
                for v_ in key[1]:
                    fn.events.append({"addr": i.address, "key": ("va", v_), "ecx": s.r.get("ecx"), "this": thisx,
                                      "args": list(reversed(run))})
                return
            fn.events.append({"addr": i.address, "key": key, "ecx": s.r.get("ecx"), "this": thisx,
                              "args": list(reversed(run))})

        if ins.mnemonic == "call":
            step(ins, st, rec)
        else:
            key, thisx = callkey(ins, st)
            if key is None and not ins.op_str.startswith(("0x", "dword ptr")):
                fn.unresolved += 1
            if key is not None:
                fn.events.append({"addr": ins.address, "key": key, "ecx": st.r.get("ecx"), "this": thisx,
                                  "args": list(reversed(st.run)), "tail": True})
        if fn.events and fn.events[-1]["addr"] == ins.address and fn.events[-1]["key"] is None:
            fn.unresolved += 1
    fn.events = [e for e in fn.events if e["key"] is not None]
    for e in fn.events:
        if e["key"][0] == "va" and e["key"][1] in THUNKS:
            e["key"] = ("va", THUNKS[e["key"][1]])
    for e in fn.events:
        # a virtual call on the console global (GConsole->vfnN) is the console method of that slot
        if e["key"][0] == "vt" and e["ecx"] in ("[G@1026e44c]", "[G@%08x]" % 0x1026e44c) and e["key"][1] // 4 in CONSOLE_VT:
            e["key"] = ("va", CONSOLE_VT[e["key"][1] // 4])
    # our own helpers (outlined by the compiler, never hooked): their boundary calls count, their values do not
    if depth < 3 and mach.side == "ours":
        out = []
        for e in fn.events:
            out.append(e)
            k = e["key"]
            if k[0] == "our":
                try:
                    sub = analyze(md, mach, k[1], names, depth + 1)
                except Exception:  # noqa: BLE001
                    continue
                for x in sub.events:
                    if x["key"][0] == "our":
                        continue
                    y = dict(x)
                    y["args"] = [None] * len(x["args"])
                    y["ecx"] = None if x["key"][0] != "imp" else x["ecx"]
                    y["this"] = None
                    y["nested"] = True
                    out.append(y)
        fn.events = out
    return fn


IMP_POP: dict[str, int] = {}
SIDE = ["stock"]
THUNKS = {}              # stock `jmp rel32` thunks -> their target
CONSOLE_VT = {}          # NConsoleWnd vtable slot -> stock function (analysis/classes.json)
NH_OFFS = set()          # NetHandler offsets the stock images calls (filled in main)
NWBASE = [None]      # address of our g_nwBase variable (the stock image's base lives there)


def build_records(fn: Fn):
    """-> (records, nh_calls, calls): L2ParamStack records, NetHandler vcalls, all other keyed calls"""
    recs, nh, calls = [], [], []
    cur = None
    for e in fn.events:
        k = e["key"]
        if k == ("imp", CTOR):
            cur = {"ctor": e["args"][0] if e["args"] else None, "obj": e["ecx"], "pushes": [], "consumers": [],
                   "addr": e["addr"]}
            recs.append(cur)
            continue
        if k == ("imp", PUSHBACK):
            tgt = cur
            if cur is not None and e["ecx"] is not None and cur["obj"] is not None and e["ecx"] != cur["obj"]:
                # another object? look for the record that owns this address
                for r in reversed(recs):
                    if r["obj"] == e["ecx"]:
                        tgt = r
                        break
            if tgt is not None:
                tgt["pushes"].append(e["args"][0] if e["args"] else None)
            continue
        if k == ("imp", DTOR):
            cur = None if (cur and e["ecx"] == cur["obj"]) else cur
            continue
        is_nh = k[0] == "vt" and e["this"] is not None and bool(NH_RE.search(e["this"]))

        if is_nh:
            nh.append({"off": k[1], "args": e["args"], "addr": e["addr"]})
        # a consumer takes the stack object by reference
        taken = None
        for a in e["args"]:
            if a and a.startswith("SP:"):
                taken = a
        if taken is not None:
            for r in reversed(recs):
                if r["obj"] == taken:
                    r["consumers"].append((k, e["this"] if k[0] == "vt" else None))
                    break
        calls.append({"key": k, "this": e["this"] if k[0] == "vt" else e["ecx"], "args": e["args"], "nh": is_nh,
                      "addr": e["addr"]})
    return recs, nh, calls


# ---------------------------------------------------------------------------------------------------------------
def weq(a, b):
    """equal with `?`/None wildcards, stack addresses, locals and image pointers generic; two phis agree when
    they share an alternative"""
    if a is None or b is None or a == "?" or b == "?":
        return True
    a, b = norm(a), norm(b)
    if a == b:
        return True
    sa = set(a[4:-1].split("|")) if a.startswith("phi(") else {a}
    sb = set(b[4:-1].split("|")) if b.startswith("phi(") else {b}
    return bool(sa & sb)


def norm(v):
    v = re.sub(r"SP:-?\d+", "&loc", v)
    v = re.sub(r"\bL-?\d+\b", "loc", v)
    v = re.sub(r"\b0x1[0-9a-f]{7}\b",
               lambda m: "ptr" if 0x10000000 <= int(m.group(0), 16) < 0x20000000 else m.group(0), v)
    v = re.sub(r"\bour[0-9a-f]{6,8}\b", "ourfn", v)
    v = v.replace("0xffffffe2", "-0x1e")
    return v


def seq_eq(a, b):
    return len(a) == len(b) and all(weq(x, y) for x, y in zip(a, b))


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--only", nargs="*", help="stock VAs (hex) to analyse")
    ap.add_argument("--dll", type=Path, default=ROOT / "build" / "bin" / "Release" / "NWindowDecomp.dll")
    ap.add_argument("--log", type=Path, default=ROOT / "build" / "smoke_system" / "NWindowDecomp.log")
    ap.add_argument("--all", action="store_true", help="also compare every call's arguments of functions with no L2ParamStack")
    ap.add_argument("--vtable", action="store_true")
    ap.add_argument("--wrappers", action="store_true", help="only list the tiny NetHandler wrappers: our name vs the Engine's slot name")
    ap.add_argument("--md", type=Path)
    ap.add_argument("-v", action="store_true")
    ap.add_argument("--dump", help="print the events of one function (stock va hex)")
    args = ap.parse_args()

    md = Cs(CS_ARCH_X86, CS_MODE_32)
    stock = Image()
    granges = {}
    for r in csv.DictReader((ROOT / "analysis/ghidra/functions.tsv").open(encoding="utf-8"), delimiter="\t"):
        granges[int(r["va"], 16)] = [tuple(int(x, 16) for x in rg.split("-")) for rg in r["ranges"].split(",")]
    for r in csv.DictReader(l for l in (ROOT / "db/not_functions.csv").open(encoding="utf-8") if not l.startswith("#")):
        owner, frag = int(r["part_of"], 16), int(r["va"], 16)
        if owner in granges and frag in granges:
            granges[owner] = granges[owner] + granges[frag]
    for r in csv.DictReader(l for l in (ROOT / "db/extra_functions.csv").open(encoding="utf-8") if not l.startswith("#")):
        va = int(r["va"], 16)
        granges[va] = [(va, va + int(r["size"]) - 1)]
    s_lo, s_hi = stock.sect_range(".text")
    stock_code = vh.Code(stock.read, {va: n for va, (_, n) in stock.imports.items()},
                         lambda va: [(a, b + 1) for a, b in granges.get(va, [(va, va + 0x3F)])],
                         lambda va: s_lo <= va < s_hi)
    stock_code.nwbase = None

    pe = pefile.PE(str(args.dll))
    base = pe.OPTIONAL_HEADER.ImageBase
    mem = pe.get_memory_mapped_image()
    o_imports = {i.address: (i.name or b"").decode() for e in pe.DIRECTORY_ENTRY_IMPORT for i in e.imports}
    symbols = vh.our_symbols(args.dll)
    starts = [a for a, _ in symbols]
    o_text = next(s for s in pe.sections if s.Name.startswith(b".text"))
    o_lo, o_hi = base + o_text.VirtualAddress, base + o_text.VirtualAddress + o_text.Misc_VirtualSize

    def o_extent(va):
        i = bisect.bisect_right(starts, va)
        end = starts[i] if i < len(starts) else o_hi
        return [(va, end)]

    ours_code = vh.Code(lambda va, n: mem[va - base:va - base + n], o_imports, o_extent, lambda va: o_lo <= va < o_hi)
    ours_code.nwbase = None
    mp = args.dll.with_suffix(".map")
    if mp.exists():
        for ln in mp.read_text(encoding="latin-1").splitlines():
            m = re.match(r"^\s*0003:[0-9a-f]{8}\s+_g_nwBase\s+([0-9a-f]{8})\s", ln)
            if m:
                NWBASE[0] = int(m.group(1), 16)
                break

    rows = []
    for line in args.log.read_text(encoding="latin-1").splitlines():
        m = vh.LOG_RE.match(line)
        if m:
            rows.append((int(m.group(1), 16), m.group(2), int(m.group(3), 16)))
    names = {}
    for r in csv.DictReader((ROOT / "db/functions.csv").open(encoding="utf-8")):
        names[int(r["va"], 16)] = r["name"]
    for va, nm, _ in rows:
        names.setdefault(va, nm)

    alias: dict[int, int] = {}

    def follow_forwarder(a):
        data = ours_code.read(a, 24)
        for ins in md.disasm(bytes(data), a):
            if ins.mnemonic in ("push", "mov", "pop", "nop"):
                continue
            if ins.mnemonic == "jmp" and ins.op_str.startswith("0x"):
                return int(ins.op_str, 16)
            break
        return None

    for va, _, rva in rows:
        alias.setdefault(base + rva, va)
        t = follow_forwarder(base + rva)
        if t is not None and o_lo <= t < o_hi:
            alias.setdefault(t, va)

    def resolve_ours(t):
        if t in alias:
            return ("va", alias[t])
        if not (o_lo <= t < o_hi):
            return None
        b = ours_code.read(t, 12)
        if len(b) == 12 and b[0] == 0xA1 and b[5] == 0x05 and tuple(b[10:12]) == (0xFF, 0xE0):
            return ("va", vh.STUB_BASE + int.from_bytes(b[6:10], "little"))
        if len(b) >= 6 and b[:2] == b"\xff\x25":
            name = o_imports.get(int.from_bytes(b[2:6], "little"))
            return ("imp", name) if name else None
        t2 = follow_forwarder(t)
        if t2 is not None and t2 != t and t2 in alias:
            return ("va", alias[t2])
        return ("our", t)

    def resolve_stock(t):
        b = stock.read(t, 6)
        if b[:2] == b"\xff\x25":
            name = stock_code.imports.get(int.from_bytes(b[2:6], "little"))
            if name:
                return ("imp", name)
        return ("va", t)

    ret_cache = {}

    def ret_n(va):
        if va not in ret_cache:
            try:
                r = vh.features(md, stock_code, va)[0]
            except Exception:  # noqa: BLE001
                r = set()
            ret_cache[va] = max(r) if len(r) >= 1 else None
        return ret_cache[va]

    # imports: pops of the known L2ParamStack members, the rest by the decorated name's arity (thiscall/stdcall)
    IMP_POP[CTOR] = 1
    IMP_POP[DTOR] = 0
    IMP_POP[PUSHBACK] = 1
    IMP_POP["?Top@L2ParamStack@@QAEPAXXZ"] = 0

    for c in json.load((ROOT / "analysis" / "classes.json").open(encoding="utf-8")):
        if c.get("cpp") == "NConsoleWnd":
            for sl in c["vtable_slots"]:
                if sl.get("va"):
                    CONSOLE_VT[sl["slot"]] = sl["va"]
    for r in csv.DictReader((ROOT / "db/functions.csv").open(encoding="utf-8")):
        if r["kind"] == "thunk" or int(r["size"]) == 5:
            va_ = int(r["va"], 16)
            b_ = stock.read(va_, 5)
            if len(b_) == 5 and b_[0] == 0xE9:
                THUNKS[va_] = (va_ + 5 + int.from_bytes(b_[1:5], "little", signed=True)) & 0xFFFFFFFF
    mach_s = Machine(stock_code, resolve_stock, ret_n, "stock")
    mach_o = Machine(ours_code, resolve_ours, ret_n, "ours")

    if args.only:
        sel = {int(x, 16) for x in args.only}
        rows = [r for r in rows if r[0] in sel]
    if args.dump:
        va = int(args.dump, 16)
        rva = next(r[2] for r in rows if r[0] == va)
        ostart = base + rva
        tf = follow_forwarder(ostart)
        if tf is not None and o_lo <= tf < o_hi:
            ostart = tf
        for lab, mach, st in (("stock", mach_s, va), ("ours", mach_o, ostart)):
            fn = analyze(md, mach, st, names)
            print("== %s drift=%d unresolved=%d" % (lab, fn.drift, fn.unresolved))
            for e in fn.events:
                print("  %08x %-34s this=%s ecx=%s args=%s" % (e["addr"], render_key(e["key"], names), e["this"], e["ecx"], e["args"]))
        return 0

    results = []
    n_touch = n_ok = 0
    out_json = {}
    cache = {}
    touch_set = set()
    for va, name, rva in sorted(rows):
        try:
            ostart = base + rva
            tf = follow_forwarder(ostart)
            if tf is not None and o_lo <= tf < o_hi:
                ostart = tf
            fs = analyze(md, mach_s, va, names)
            fo = analyze(md, mach_o, ostart, names)
        except Exception as e:  # noqa: BLE001
            print("SKIP %08x %s: %s" % (va, name, e))
            continue
        cache[va] = (name, fs, fo, build_records(fs), None)
        for x in cache[va][3][1]:
            NH_OFFS.add(x["off"])
    for va in list(cache):
        name, fs, fo, bs, _ = cache[va]
        cache[va] = (name, fs, fo, bs, build_records(fo))
        if any(x[0] or x[1] for x in (cache[va][3], cache[va][4])):
            touch_set.add(va)
    clo_cache = {}

    def closure(v, depth):
        """NetHandler offsets the STOCK function v reaches (itself and the stock callees, a few levels down)"""
        if v in clo_cache:
            return clo_cache[v]
        clo_cache[v] = set()
        if v in cache:
            f = cache[v][1]
            nhs_ = cache[v][3][1]
            cl_ = cache[v][3][2]
        else:
            try:
                f = analyze(md, mach_s, v, names)
            except Exception:  # noqa: BLE001
                return set()
            r_, nhs_, cl_ = build_records(f)
        out = {x["off"] for x in nhs_}
        if depth < 3:
            for c in cl_:
                if c["key"][0] == "va" and c["key"][1] != v:
                    out |= closure(c["key"][1], depth + 1)
        clo_cache[v] = out
        return out

    wrapper_off = {}
    for va, v in cache.items():
        offs = {x["off"] for x in v[3][1]}
        if offs and len(v[1].events) <= 3:
            wrapper_off[va] = offs
    slots_c3 = engine_vtable_names()

    def slot_name(off):
        return slots_c3.get(off // 4, "?")

    # ---- the wrappers (NConsoleWnd::Request*): our name against the Engine's name of the slot they jump to ----
    wrap_rows = []
    for va, offs in sorted(wrapper_off.items()):
        name = cache[va][0]
        if len(offs) != 1:
            continue
        off = next(iter(offs))
        en = slot_name(off)
        nm = name.split("::")[-1]
        ok = (nm.lower() == en.lower()) or nm.lower().replace("request", "") == en.lower().replace("request", "")
        wrap_rows.append((va, name, off, en, ok))
    args.wrap_rows = wrap_rows
    if args.wrappers:
        for va, name, off, en, ok in wrap_rows:
            print("%s %08x %-42s +0x%03x  engine: %s" % ("  " if ok else "!!", va, name, off, en))
        return 0

    for va in sorted(cache):
        name, fs, fo, (rs, nhs, cs), (ro, nho, co) = cache[va]
        touch = bool(rs or ro or nhs or nho)
        if not touch and not args.all:
            continue
        n_touch += 1
        probs = []
        # ---- NetHandler slots: direct vcalls, plus the slot of every boundary wrapper called (by its STOCK body) ----
        def eff(nh_list, call_list):
            e = Counter(x["off"] for x in nh_list)
            for c in call_list:
                k = c["key"]
                if k[0] == "va":
                    for off in closure(k[1], 0):
                        e[off] += 1
            return e
        offs_s, offs_o = eff(nhs, cs), eff(nho, co)
        for off in sorted(set(offs_s) | set(offs_o)):
            if (off in offs_s) != (off in offs_o):
                probs.append(("VSLOT", "NetHandler +0x%x (%s): stock %s, ours %s" % (
                    off, slot_name(off), "yes" if off in offs_s else "NO", "yes" if off in offs_o else "NO")))
        # ---- L2ParamStack records ----
        used = set()
        for r in rs:
            best = None
            for j, q in enumerate(ro):
                if j in used:
                    continue
                if seq_eq(r["pushes"], q["pushes"]) and weq(r["ctor"], q["ctor"]):
                    best = j
                    break
            if best is not None:
                used.add(best)
                continue
            # closest by the same consumer keys, else by count
            cand = [j for j, q in enumerate(ro) if j not in used]
            sk = sorted(map(repr, (c[0] for c in r["consumers"])))
            cand.sort(key=lambda j: (sorted(map(repr, (c[0] for c in ro[j]["consumers"]))) != sk,
                                     abs(len(ro[j]["pushes"]) - len(r["pushes"]))))
            q = ro[cand[0]] if cand else None
            if q is not None:
                used.add(ro.index(q))
            probs.append(("PUSH", "stock %s pushes [%s] -> %s | ours %s" % (
                "Params(%s)" % r["ctor"], ", ".join(map(str, r["pushes"])),
                ",".join(sorted(render_key(c[0], names) for c in r["consumers"])) or "(no consumer seen)",
                ("Params(%s) pushes [%s] -> %s" % (q["ctor"], ", ".join(map(str, q["pushes"])),
                                                   ",".join(sorted(render_key(c[0], names) for c in q["consumers"])) or "(none)"))
                if q is not None else "NO MATCHING RECORD")))
        for j, q in enumerate(ro):
            if j not in used and len(rs) != len(ro):
                probs.append(("PUSH", "ours has an extra record Params(%s) [%s] -> %s" % (
                    q["ctor"], ", ".join(map(str, q["pushes"])), ",".join(sorted(render_key(c[0], names) for c in q["consumers"])))))
        # ---- call arguments ----
        bykey_o = defaultdict(list)
        for c in co:
            bykey_o[c["key"]].append(c)
        for c in cs:
            if c["key"][0] == "our":
                continue
            lst = bykey_o.get(c["key"])
            if not lst:
                continue
            kn = c["key"]
            if kn[0] == "imp" and kn[1] in (CTOR, PUSHBACK, DTOR):
                continue
            nmax = None
            if kn[0] == "va":
                nb = ret_n(kn[1])
                nmax = nb // 4 if nb is not None else None
            sa = c["args"][:nmax] if nmax is not None else c["args"]
            ok = False
            for d in lst:
                oa = d["args"][:nmax] if nmax is not None else d["args"]
                if len(sa) == len(oa) and all(weq(x, y) for x, y in zip(sa, oa)) and weq(c["this"], d["this"]):
                    ok = True
                    break
            if not ok and (c["nh"] or (kn[0] == "va" and kn[1] in touch_set)):
                d = lst[0]
                oa = d["args"][:nmax] if nmax is not None else d["args"]
                probs.append(("ARG", "%s: stock this=%s (%s) | ours this=%s (%s)" % (
                    render_key(kn, names), c["this"], ", ".join(map(str, sa)), d["this"], ", ".join(map(str, oa)))))
        rec = {"va": "%08x" % va, "name": name, "stock": {"records": rs, "nh": nhs}, "ours": {"records": ro, "nh": nho},
               "drift": [fs.drift, fo.drift], "unresolved": [fs.unresolved, fo.unresolved], "problems": probs}
        out_json["%08x" % va] = rec
        # keep only unique problem lines
        seen = set()
        probs2 = []
        for p in probs:
            if p not in seen:
                seen.add(p)
                probs2.append(p)
        rec["problems"] = probs2
        if probs2:
            results.append((va, name, probs2, fs, fo))
        else:
            n_ok += 1
        if args.v or probs2:
            for kind, txt in probs2:
                print("%-5s %08x %s: %s" % (kind, va, name, txt))
    (ROOT / "analysis").mkdir(exist_ok=True)
    (ROOT / "analysis" / "c3_net_boundary.json").write_text(json.dumps(out_json, indent=1, default=str), encoding="utf-8")
    print("\nnet_boundary: %d functions touch the boundary, %d match, %d differ (%d with esp drift on a side)" % (
        n_touch, n_ok, len(results), sum(1 for r in out_json.values() if any(r["drift"]))))
    if args.vtable or args.md:
        write_vtable_and_md(args, rows, names, out_json, results, stock, md)
    return 0


def engine_vtable_names():
    """slot -> method name of UNetworkHandler in the C3 Engine.dll (exports + the vtable)"""
    sys.path.insert(0, str(ROOT / "tools"))
    import engine_vtables as ev
    if ENGINE_DLL is None:
        sys.exit("net_boundary: set NWD_C3_SYSTEM (the C3 client's System folder, for its Engine.dll)")
    pe = pefile.PE(str(ENGINE_DLL))
    base = pe.OPTIONAL_HEADER.ImageBase
    mem = pe.get_memory_mapped_image()
    exp = {}
    byva = defaultdict(list)
    for n, rva in ev.read_exports(pe):
        exp[n] = base + rva
        byva[base + rva].append(n)
    vt = None
    for n in exp:
        if n.startswith("??_7UNetworkHandler@@6B") and "UObject" in n or n == "??_7UNetworkHandler@@6BUObject@@@":
            vt = exp[n]
            break
    if vt is None:
        for n in exp:
            if n.startswith("??_7UNetworkHandler@@"):
                vt = exp[n]
                break
    out = {}
    if vt is None:
        return out
    # thunks: Engine is an incremental build; a slot may point at `jmp body`
    def names_at(a):
        res = list(byva.get(a, []))
        b = mem[a - base:a - base + 5]
        if b[0] == 0xE9:
            t = (a + 5 + int.from_bytes(b[1:5], "little", signed=True)) & 0xFFFFFFFF
            res += byva.get(t, [])
        return res
    for i in range(0, 320):
        a = int.from_bytes(mem[vt - base + 4 * i:vt - base + 4 * i + 4], "little")
        if not (base <= a < base + len(mem)):
            break
        nm = [x for x in names_at(a) if "UNetworkHandler" in x or "UL2NetHandler" in x]
        out[i] = nm[0].split("@")[0].lstrip("?") if nm else ("<purecall>" if mem[a - base:a - base + 1] else "?")
    return out


FINDINGS = """
## 1. Veredito sobre sentar/levantar e cancelar alvo (lado NWindow)

O lado NWindow e IDENTICO ao stock nesses caminhos (certeza alta, lido do disassembly e do dataflow simbolico):

* `NConsoleWnd::UseAction` (0x10074ab0), ramo `default`: stock e nosso fazem `IsCtrlDown()` (virtual 297, +0x4a4),
  `UInput::KeyDown(0x10)`, `L2ParamStack Params(10)`, `PushBack(actionId)`, `PushBack(ctrl ? IsCtrlDown() : bForce)`,
  `PushBack(KeyDown(0x10) ? KeyDown(0x10) : bShift)`, depois `NConsoleWnd::RequestActionUse` (0x1005d3a0), que salta para
  `NetHandler->vtable+0x298` (slot 166, `RequestActionUse` no Engine C3). Ordem, contagem (3) e argumento do construtor (10) batem.
* `NConsoleWnd::RequestActionUse` (0x1005d3a0) e `RequestTargetCancel` (0x1005d390, +0x260 = slot 152, `RequestTargetCancel` no Engine C3):
  stock e nosso saltam para o mesmo offset. Ha uma unica sobrecarga de `L2ParamStack::PushBack` no Engine C3, `PushBack(void*)`
  (nao existe tag de tipo: o Engine le a pilha por posicao), entao "INT contra ponteiro" nao muda nada.
* Cancelar alvo: `NCTargetStatusWnd::OnClose` (0x100fa9f0, botao X), ESC (`NConsoleWnd::OnKeyEvent`) e `ShowRestartMenu` constroem uma
  `L2ParamStack Params(10)` VAZIA e chamam `RequestTargetCancel`, exatamente como o stock. O que sai no fio depende do Engine
  (formato do pacote no `Engine.dll` C3) e do servidor, nao do NWindow.
* Conclusao: se sentar/levantar e cancelar alvo nao funcionam, a causa NAO esta no que o NWindow empurra nem no slot do NetHandler.

## 2. Defeitos reais encontrados e corrigidos (todos do mesmo tipo: wrapper errado, "nomes deslocados")

O `NConsoleWnd` tem dezenas de wrappers minusculos (`0x1005d020..0x1005dxxx`: `mov eax,[ecx+0x48]; mov ecx,[eax+0x58]; mov edx,[ecx];
jmp [edx+X]`). Os nomes `Request*` vieram do mapa do C2; no C3 a ORDEM dos wrappers mudou em varias familias, entao o wrapper
que o NOSSO codigo chama pelo nome nao e o que o stock chama: o corpo do hook esta certo (usa o offset do stock), mas o ponto de
chamada manda outro pacote. A tabela da secao 4 lista os wrappers cujo nome difere do metodo que o Engine C3 da ao slot.

| funcao (VA) | antes (ponto de chamada) | agora | pacote |
|---|---|---|---|
| `NConsoleWnd::ReceiveAskJoinPledge` 0x10070570 (recusa automatica do convite de clan) | `RequestAnswerJoinPledge` = 0x1005d4d0 (+0x1c8) | wrapper 0x1005d4c0 (+0x1c4) | saia `RequestOustPledgeMember`, passa a `RequestAnswerJoinPledge` |
| `NConsoleWnd::ReceiveStartPledgeWar` 0x10070680 (recusa da guerra) | `RequestReplyStartPledgeWar` = 0x1005d510 (+0x1e0) | 0x1005d500 (+0x1dc) | saia `RequestStopPledgeWar`, passa a `ReplyStartPledgeWar` |
| `NConsoleWnd::ReceiveStopPledgeWar` 0x100707a0 (recusa do fim da guerra) | `RequestReplyStopPledgeWar` = 0x1005d530 (+0x1e8) | 0x1005d520 (+0x1e4) | saia `RequestSurrenderPledgeWar`, passa a `ReplyStopPledgeWar` |
| `NCPledgeWnd::OnInvite` 0x10047350 (botao Convidar do clan) | `RequestJoinPledge` = 0x1005d570 (+0x1d4) | 0x1005d560 (+0x1d0) | saia `RequestWithDrawalPledge`, passa a `RequestJoinPledge` |
| `NCMacroArrayWnd::OnDialogResult` 0x1009d090 (apagar macro) | `RequestDeleteMacro` = 0x1005d020 (+0x25c) | 0x1005d030 (+0x258) | saia `RequestOpenMinimap`, passa a `RequestDeleteMacro` |

Os valores empurrados (e a ordem) desses pontos de chamada ja eram iguais aos do stock; so o wrapper estava trocado. Os pontos de
chamada corrigidos usam `nw::StockCallAt<void>(VA, ...)` (o mesmo padrao dos macros `*_AT` de `NCGaraDialogBox.cpp`, que o porte
anterior ja tinha descoberto para os dialogos de clan).
"""

UNPROVEN = """
## 5. Nao provado / limites da ferramenta

* `NCChatWnd::ProcessChatLine` (0x10050360) e `NCConsole::OnHtmlCommand` (0x10054540): muitos `Params` num corpo so; a ferramenta junta
  os blocos por construtor e nao e sensivel ao caminho, entao o relatorio e ruidoso. O conjunto de slots/wrappers chamados bate
  (nenhum VSLOT), mas os pushes individuais nao foram provados um a um. Revisar a mao se um `/comando` de chat se comportar mal.
* `NCEquipItemWnd::OnLButtonUp/OnRButtonDown/OnLButtonDblClick`, `NCShopWnd::OnOK`, `NCTrainListWnd::OnListCommand`, `FormatDateTime`:
  aparecem como PUSH mas foram lidos a mao (decompile do stock x fonte) e sao iguais (varios ramos reusando o mesmo construtor,
  lacos, e campos de um struct preenchido por chamada). `NCPetitionFeedBackWnd::OnSend`: o stock chama `NCMultiEdit::GetText`
  (0x100251b0) e nos `NCHtmlMultiEdit::GetValue` (0x100415a0), que e um `jmp 0x100251b0`: igual.
* `NMacroEntry::Tick` (0x1009e8f0) e o ramo Attack de `NConsoleWnd::UseAction`: o nosso chama `nw::VirtualCall<>` / `RequestAttack`
  por funcoes auxiliares fora de linha que a ferramenta nao atravessa com valores; os offsets (38 -> +0x98, 241 -> +0x3c4,
  +0xdc/+0xe0) foram conferidos na fonte contra o stock.
* Chamadas `nw::VirtualCall<>(NetHandler, N, ...)` com N fixo (varias em `NConsoleWndNet.cpp`) so sao conferidas pelo offset final
  (verify_hooks) e pelo conjunto de slots desta ferramenta; o FORMATO que o Engine le em cada slot nao e verificavel daqui.
* O que o `Engine.dll` C3 faz com a `L2ParamStack` de cada slot (formato do pacote c2s) nao foi lido nesta pista.
"""


def write_vtable_and_md(args, rows, names, out_json, results, stock, md):
    slots = engine_vtable_names()
    use = defaultdict(set)
    for va, rec in out_json.items():
        for x in rec["stock"]["nh"]:
            use[x["off"]].add(rec["name"])
    tab = []
    for off in sorted(use):
        tab.append((off, off // 4, slots.get(off // 4, "?"), sorted(use[off])))
    lines = []
    lines.append("# C3 pacotes, pista N: a fronteira NWindow -> Engine (NetHandler) do codigo portado")
    lines.append("")
    lines.append("Gerado por `python tools/c3/net_boundary.py --md docs/notas/C3_Pacotes_N.md` (bruto em `analysis/c3_net_boundary.json`).")
    lines.append("A ferramenta executa simbolicamente o stock e a nossa DLL e compara, por funcao hookeada: o `vcall` no `Engine->NetHandler`,")
    lines.append("a sequencia ORDENADA de valores de cada `L2ParamStack` (`PushBack`), o argumento do construtor e os argumentos das chamadas.")
    lines.append(FINDINGS.rstrip("\n"))
    lines.append("")
    lines.append("## 3. Resultado da comparacao automatica")
    lines.append("")
    lines.append("%d funcoes tocam a fronteira (L2ParamStack ou NetHandler); %d ainda aparecem com diferenca simbolica (a secao 5 diz o que foi lido a mao)." % (len(out_json), len(results)))
    lines.append("")
    if results:
        lines.append("| VA | funcao | tipo | stock x nosso |")
        lines.append("|---|---|---|---|")
        for va, name, probs, fs, fo in results:
            for kind, txt in probs:
                if kind == "ARG":
                    continue
                lines.append("| %08x | %s | %s | %s |" % (va, name, kind, txt.replace("|", "/")[:300]))
        lines.append("")
    lines.append("## 4. Wrappers `NConsoleWnd` cujo nome difere do metodo que o Engine C3 da ao slot")
    lines.append("")
    lines.append("Cada linha: VA do wrapper, nosso nome (do mapa do C2), offset do NetHandler que o stock chama, nome do slot no `UNetworkHandler` do Engine.dll C3.")
    lines.append("O corpo do hook usa o offset do stock (correto). O RISCO e um ponto de chamada que escolhe o wrapper pelo NOME (a secao 2 lista os 5 que erravam).")
    lines.append("")
    lines.append("| VA | nome nosso | offset | slot no Engine C3 |")
    lines.append("|---|---|---|---|")
    for va, name, off, en, ok in getattr(args, "wrap_rows", []):
        if not ok:
            lines.append("| %08x | %s | +0x%x | %s |" % (va, name, off, en))
    lines.append("")
    lines.append("## 4b. Slots do NetHandler chamados pelo stock (offset -> metodo do UNetworkHandler no Engine.dll C3)")
    lines.append("")
    lines.append("| offset | slot | metodo no Engine C3 | funcoes NWindow que chamam |")
    lines.append("|---|---|---|---|")
    for off, slot, eng, fns in tab:
        lines.append("| +0x%x | %d | %s | %s |" % (off, slot, eng, ", ".join(fns[:6]) + (" ..." if len(fns) > 6 else "")))
    lines.append("")
    lines.append(UNPROVEN.rstrip("\n"))
    text = "\n".join(lines) + "\n"
    if args.md:
        args.md.write_text(text, encoding="utf-8")
        print("wrote", args.md)
    else:
        print(text)


if __name__ == "__main__":
    sys.exit(main())
