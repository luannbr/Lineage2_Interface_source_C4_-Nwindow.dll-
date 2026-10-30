#!/usr/bin/env python3
"""The InternalConstructor of the standalone NWindow.dll against the stock one (phase 5, p5ctor).

In the standalone every class registers `static void InternalConstructor(void* X) { new ((EInternal*)X) TClass; }`
(include/nwindow/NWindow.h, NW_DECLARE_CLASS, NW_HOOK_MODE=0): the constructor that runs is the one
callable with NO arguments, with the DEFAULT argument values our header declares.  The stock
InternalConstructor (one per class, analysis/classes.json "internal_ctor"; the three abstract-ish
NCObject/NCCmdTarget/NCAbstractMain share one) is

    mov ecx,[esp+4]; test ecx,ecx; jz end; push <args>...; call <ctor>; ret      (or jmp <ctor>, no args)

and the constants it pushes are the arguments the engine's object construction really uses.  In hook
mode a mismatch is silent (the stock code constructs); in the standalone it is wrong, and a class with
only an argument-taking constructor does not even compile (error C2512).

Part 1: for each of the classes in analysis/classes.json
  stock   the InternalConstructor read from orig/NWindow.dll (every instruction is modelled: mov/xor
          into registers, push imm / push reg, float constants from memory, wide-string pointers resolved to
          the literal), the constructor it calls and that constructor's `ret N`
  ours    the constructors the class declares in include/nwindow/<Class>.h (comments stripped, default
          arguments evaluated), the one the standalone would call (the one callable with no arguments)
  verdict OK, or MISMATCH (a value differs), ARITY (our constructor takes more/fewer arguments than the
          stock one), NO-DEFAULT-CTOR (C2512 in the standalone), AMBIGUOUS (C2668), UNCHECKED (a default
          value or a stock constant could not be evaluated).
Part 1b (needs the standalone link, see below): the InternalConstructor the COMPILER emitted for each class
is disassembled and compared with the stock one - the pushed constants (strings resolved to the literal) and the
`ret N` of the constructor it calls. This is the proof that does not depend on the header parser.
Part 2: the registration data the generator cannot see - header NW_DECLARE_CLASS arguments (parent,
flags, StaticClassVA, size) against the stock registration (classes.json), and what
src/standalone/Classes.gen.cpp emits (parent UClass, class flags, size, order, Within).
Part 2b (needs the standalone link): the UClass constructor call of every compiled `PrivateStaticClass`
initializer is read back (extract_classes.py's emulator) and compared with classes.json: size, class flags,
parent UClass, Within, GUID, name, package, config, object flags, InternalConstructor, static constructor.

  python tools/port/check_internal_ctors.py [--all] [--no-registration] [--no-compiled] [--probe DLL] [--map MAP]
    --all          list every class (default: those with stock arguments and every problem)
    --probe/--map  the standalone DLL and its map parts 1b and 2b read (default: build/standalone/Release/NWindow.dll
                   once the target NWindowStandalone links, else build/standalone_probe/Release/NWindow_probe.dll,
                   from `cmake --build build --config Release --target NWindowStandaloneProbe` with
                   -DNWD_GAME_SOURCES=ON: the probe links with /FORCE:UNRESOLVED and is only read, never loaded).
                   Skipped with a note when the DLL is not there.
    --no-compiled  parts 1 and 2 only (headers and the generated file against classes.json and the stock image)
Exit code 0 when everything is OK, 1 otherwise.
"""
from __future__ import annotations

import argparse
import csv
import json
import re
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))

from nwd.image import Image  # noqa: E402

INCLUDE = ROOT / "include"
HDR_DIR = INCLUDE / "nwindow"
GEN_CPP = ROOT / "src" / "standalone" / "Classes.gen.cpp"
CLASSES_JSON = ROOT / "analysis" / "classes.json"
FUNCS_CSV = ROOT / "db" / "functions.csv"

CLASS_FLAG_VALUES = {"CLASS_Abstract": 1, "CLASS_Config": 4}
REGS32 = ("eax", "ebx", "ecx", "edx", "esi", "edi", "ebp")


# ------------------------------------------------------------------------------------------------
# stock side: the InternalConstructor in the image
# ------------------------------------------------------------------------------------------------
class Stock:
    """What one stock InternalConstructor does."""

    def __init__(self, va):
        self.va = va
        self.pushed = []          # raw values in push order (the last pushed is the first C++ argument)
        self.call = None          # constructor VA
        self.is_jmp = False
        self.notes = []           # anything the model could not follow
        self.ctor_ret = None      # `ret N` of the constructor (bytes the callee pops)

    @property
    def args(self):
        """The C++ argument list, first argument first."""
        return list(reversed(self.pushed))


def signed32(v):
    v &= 0xFFFFFFFF
    return v - 0x100000000 if v & 0x80000000 else v


def model_internal_ctor(img: Image, va: int) -> Stock:
    st = Stock(va)
    regs = {}                     # register -> value (int) or None (unknown)
    sp_pending = 0                # `sub esp,4` reserved for a float store
    float_pending = None
    first = True
    for ins in img.disasm(va, 0x80, stop_at_ret=True):
        m, ops = ins.mnemonic, ins.op_str
        was_first, first = first, False
        if was_first and m == "push" and ops == "ebp":
            continue                                   # our compiled code: push ebp; mov ebp, esp ... pop ebp
        if m == "mov" and ops == "ebp, esp" or m == "pop" and ops == "ebp":
            continue
        if m == "mov" and re.fullmatch(r"ecx, dword ptr \[(?:esp \+ 4|ebp \+ 8)\]", ops):
            regs["ecx"] = "this"
        elif m == "test" and ops in ("ecx, ecx",):
            pass
        elif m in ("je", "jz", "jne", "jnz"):
            pass
        elif m == "xor":
            a, b = [x.strip() for x in ops.split(",")]
            if a == b:
                regs[a] = 0
            else:
                st.notes.append(f"unmodelled: {m} {ops}")
        elif m == "mov":
            a, b = [x.strip() for x in ops.split(",", 1)]
            if a in REGS32 and re.fullmatch(r"-?(0x[0-9a-f]+|\d+)", b):
                regs[a] = int(b, 16) if "0x" in b else int(b)
            elif a in REGS32 and b in REGS32:
                regs[a] = regs.get(b)
            elif a in REGS32:
                regs[a] = None
                st.notes.append(f"register {a} loaded from memory: {ops}")
            else:
                st.notes.append(f"unmodelled: {m} {ops}")
        elif m == "push":
            if re.fullmatch(r"-?(0x[0-9a-f]+|\d+)", ops):
                st.pushed.append(signed32(int(ops, 16) if "0x" in ops else int(ops)))
            elif ops in REGS32:
                v = regs.get(ops)
                if v is None:
                    st.pushed.append(("?", f"push {ops} (unknown)"))
                else:
                    st.pushed.append(v)
            elif ops.startswith("dword ptr ["):
                st.pushed.append(("?", f"push {ops}"))
            else:
                st.pushed.append(("?", f"push {ops}"))
        elif m == "sub" and ops == "esp, 4":
            sp_pending += 1
        elif m == "fld" and ops.startswith("dword ptr [0x"):
            addr = int(re.search(r"0x[0-9a-f]+", ops).group(0), 16)
            float_pending = struct.unpack("<I", img.read(addr, 4))[0]
        elif m == "fstp" and ops == "dword ptr [esp]" and float_pending is not None and sp_pending:
            st.pushed.append(float_pending)         # a float constant: the bits, decoded by the parameter type
            float_pending, sp_pending = None, sp_pending - 1
        elif m in ("call", "jmp"):
            st.call = int(ops, 16) if re.fullmatch(r"0x[0-9a-f]+", ops) else None
            st.is_jmp = m == "jmp"
            if st.call is None:
                st.notes.append(f"indirect {m} {ops}")
            break
        elif m in ("ret", "retn"):
            break
        else:
            st.notes.append(f"unmodelled: {m} {ops}")
    if st.call is None:
        st.notes.append("no call/jmp to a constructor found")
    else:
        st.ctor_ret = ret_bytes(img, st.call)
    return st


def ret_bytes(img: Image, va: int, depth: int = 0):
    """`ret N` of the function at va (the first ret reached; a plain jmp thunk is followed)."""
    for ins in img.disasm(va, 0x2000, stop_at_ret=True):
        if ins.mnemonic in ("ret", "retn"):
            return int(ins.op_str, 16) if ins.op_str else 0
        if ins.mnemonic == "jmp" and depth < 4 and re.fullmatch(r"0x[0-9a-f]+", ins.op_str) and ins.address == va:
            return ret_bytes(img, int(ins.op_str, 16), depth + 1)
    return None


def resolve_stock_value(img: Image, v):
    """('int', n) | ('ptr', va) | ('str', text) | ('bits', n) | ('?', why) for one pushed value."""
    if isinstance(v, tuple):
        return v
    u = v & 0xFFFFFFFF
    lo, hi = img.base, img.base + img.pe.OPTIONAL_HEADER.SizeOfImage
    if lo <= u < hi and not img.in_text(u):
        s = img.wstr(u, 256)
        if s is not None and s and all(32 <= ord(c) < 0x2FFF for c in s):
            return ("str", s)
        return ("ptr", u)
    return ("int", signed32(v))


# ------------------------------------------------------------------------------------------------
# our side: the constructors a header declares
# ------------------------------------------------------------------------------------------------
def strip_comments(text: str) -> str:
    """Comments replaced by spaces (newlines kept, so line numbers survive); string literals intact."""
    out, i, n = [], 0, len(text)
    while i < n:
        c = text[i]
        if c in "\"'":
            j = i + 1
            while j < n and text[j] != c:
                j += 2 if text[j] == "\\" else 1
            out.append(text[i:j + 1])
            i = j + 1
        elif text.startswith("//", i):
            j = text.find("\n", i)
            j = n if j < 0 else j
            out.append(" " * (j - i))
            i = j
        elif text.startswith("/*", i):
            j = text.find("*/", i + 2)
            j = n if j < 0 else j + 2
            out.append(re.sub(r"[^\n]", " ", text[i:j]))
            i = j
        else:
            out.append(c)
            i += 1
    return "".join(out)


def match_close(text: str, i: int, open_c: str, close_c: str) -> int:
    """Index of the bracket closing the one at text[i] (string literals skipped)."""
    depth, n = 0, len(text)
    while i < n:
        c = text[i]
        if c in "\"'":
            j = i + 1
            while j < n and text[j] != c:
                j += 2 if text[j] == "\\" else 1
            i = j
        elif c == open_c:
            depth += 1
        elif c == close_c:
            depth -= 1
            if depth == 0:
                return i
        i += 1
    return -1


def split_top(s: str, sep: str = ","):
    """Split at top-level `sep` (outside (), <>, [], {} and string literals)."""
    parts, depth, cur, i, n = [], 0, [], 0, len(s)
    while i < n:
        c = s[i]
        if c in "\"'":
            j = i + 1
            while j < n and s[j] != c:
                j += 2 if s[j] == "\\" else 1
            cur.append(s[i:j + 1])
            i = j + 1
            continue
        if c in "([{":
            depth += 1
        elif c in ")]}":
            depth -= 1
        elif c == "<" and depth >= 0 and re.match(r"<\s*\w", s[i:i + 3]) and re.search(r"\w\s*$", "".join(cur)):
            depth += 1                                 # a template argument list (TArray<INT>)
        elif c == ">" and depth > 0 and s[i - 1:i] != "-":
            depth -= 1
        if c == sep and depth == 0:
            parts.append("".join(cur))
            cur = []
        else:
            cur.append(c)
        i += 1
    parts.append("".join(cur))
    return parts


class Param:
    def __init__(self, decl: str, default):
        self.decl = " ".join(decl.split())
        self.default = None if default is None else default.strip()

    @property
    def is_float(self):
        return bool(re.search(r"\b(FLOAT|float)\b", self.decl)) and "*" not in self.decl and "&" not in self.decl

    @property
    def slots(self):
        d = self.decl
        if "*" in d or "&" in d:
            return 1
        if re.search(r"\b(DOUBLE|double|INT64|QWORD|SQWORD|__int64|long long)\b", d):
            return 2
        if re.search(r"\bFString\b", d):
            return 3
        return 1


class Ctor:
    def __init__(self, header, line, text, params, variadic=False):
        self.header, self.line, self.text, self.params, self.variadic = header, line, text, params, variadic

    @property
    def callable_without_args(self):
        return all(p.default is not None for p in self.params)

    @property
    def total_slots(self):
        return sum(p.slots for p in self.params)

    def __str__(self):
        return f"{self.header}:{self.line}: {self.text}"


def find_class_body(clean: str, name: str):
    """(start of body after '{', end index of the closing '}', bases text) of `class name ... {`."""
    for m in re.finditer(r"\b(?:class|struct)\s+(?:\w+\s+)?" + re.escape(name) + r"\b\s*(?::([^{;]*))?\{", clean):
        end = match_close(clean, m.end() - 1, "{", "}")
        if end > 0:
            return m.end(), end, (m.group(1) or "").strip()
    return None


def parse_ctors(path: Path, name: str):
    """(constructors declared in the class body of `name`, bases text) or (None, None)."""
    raw = path.read_text(encoding="utf-8", errors="replace")
    clean = strip_comments(raw)
    found = find_class_body(clean, name)
    if not found:
        return None, None
    start, end, bases = found
    body = clean[start:end]
    # brace depth at each position of the body (member bodies and nested types are deeper than 0)
    depth, d = [0] * (len(body) + 1), 0
    i, n = 0, len(body)
    while i < n:
        c = body[i]
        if c in "\"'":
            j = i + 1
            while j < n and body[j] != c:
                j += 2 if body[j] == "\\" else 1
            for k in range(i, min(j + 1, n)):
                depth[k] = d
            i = j + 1
            continue
        if c == "{":
            d += 1
        depth[i] = d
        if c == "}":
            d -= 1
        i += 1
    ctors = []
    for m in re.finditer(r"(?<![\w~:>.])" + re.escape(name) + r"\s*\(", body):
        if depth[m.start()] != 0:
            continue
        # not a member access / a macro argument: the statement must start here (optionally `explicit`/`inline`)
        k = m.start() - 1
        while k >= 0 and body[k] in " \t\r\n":
            k -= 1
        prev = body[max(0, k - 8):k + 1]
        # (`)` closes the NW_DECLARE_CLASS(...) macro, which has no `;`)
        if k >= 0 and body[k] not in ";{}:)" and not re.search(r"(explicit|inline|public:|private:|protected:)$", prev.strip()):
            continue
        close = match_close(body, m.end() - 1, "(", ")")
        if close < 0:
            continue
        ptxt = body[m.end():close]
        params, variadic = [], False
        if ptxt.strip() not in ("", "void"):
            for part in split_top(ptxt):
                part = part.strip()
                if part == "...":
                    variadic = True
                    continue
                eq = split_top(part, "=")
                decl = eq[0]
                default = "=".join(eq[1:]) if len(eq) > 1 else None
                params.append(Param(decl, default))
        # a copy constructor is no default constructor
        if len(params) == 1 and re.search(r"\b" + re.escape(name) + r"\s*&", params[0].decl):
            continue
        line = raw.count("\n", 0, start + m.start()) + 1
        text = " ".join(body[m.start():close + 1].split())
        ctors.append(Ctor(path.name, line, text, params, variadic))
    return ctors, bases


# ---- default argument values -------------------------------------------------------------------
_SYMS = None


def symbols():
    """#define NAME <int> and enumerators with an integer value found in include/ (for defaults like CLASS_Config)."""
    global _SYMS
    if _SYMS is None:
        _SYMS = {"TRUE": 1, "FALSE": 0, "NULL": 0, "true": 1, "false": 0}
        for p in list(INCLUDE.rglob("*.h")):
            t = strip_comments(p.read_text(encoding="utf-8", errors="replace"))
            for m in re.finditer(r"^\s*#\s*define\s+(\w+)\s+\(?\s*(-?(?:0x[0-9a-fA-F]+|\d+))[uUlL]*\s*\)?\s*$", t, re.M):
                _SYMS.setdefault(m.group(1), int(m.group(2), 0))
            for m in re.finditer(r"\b(\w+)\s*=\s*(-?(?:0x[0-9a-fA-F]+|\d+))[uUlL]*\s*[,}\n]", t):
                _SYMS.setdefault(m.group(1), int(m.group(2), 0))
    return _SYMS


def eval_default(expr: str, is_float: bool = False):
    """('int', n) | ('float', f) | ('str', s) | ('null',) | ('?', expr) for a default argument expression."""
    e = expr.strip()
    m = re.fullmatch(r'(?:TEXT\(\s*)?L?"((?:[^"\\]|\\.)*)"\s*\)?', e)
    if m:
        return ("str", bytes(m.group(1), "utf-8").decode("unicode_escape"))
    e = re.sub(r"\(\s*(?:const\s+)?(?:INT|UBOOL|DWORD|BYTE|WORD|SWORD|SQWORD|UINT|FLOAT|float|int|unsigned|TCHAR\s*\*|void\s*\*)\s*\)", "", e)
    e = re.sub(r"\b(?:INT|DWORD|UBOOL|BYTE|WORD|FLOAT|float|int)\s*\(\s*([^()]*)\)", r"(\1)", e)   # functional cast
    e = re.sub(r"(?<=[0-9a-fA-F.])[uUlL]+\b", "", e)
    e = re.sub(r"(\d+\.\d*|\.\d+|\d+)[fF]\b", r"\1", e)
    tokens = re.findall(r"[A-Za-z_]\w*", re.sub(r"0[xX][0-9a-fA-F]+", "", e))
    syms = symbols()
    for t in set(tokens):
        if t in syms:
            e = re.sub(r"\b" + t + r"\b", str(syms[t]), e)
        elif not re.fullmatch(r"[eE]", t):
            return ("?", expr.strip())
    if not re.fullmatch(r"[\d\s+\-*/()<>|&~^.xXa-fA-F%]*", e) or not e.strip():
        return ("?", expr.strip())
    try:
        v = eval(e, {"__builtins__": {}}, {})
    except Exception:
        return ("?", expr.strip())
    if isinstance(v, float):
        return ("float", v)
    return ("int", int(v))


def values_equal(stock, ours, param: Param):
    """True/False, or None when the comparison cannot be made."""
    sk, ok = stock[0], ours[0]
    if sk == "?" or ok == "?":
        return None
    if ok == "str":
        return sk == "str" and stock[1] == ours[1]
    if sk == "str" or sk == "ptr":
        return False                                   # a stock pointer to data against a number/NULL
    if ok == "float":
        if sk == "int":
            f = struct.unpack("<f", struct.pack("<I", stock[1] & 0xFFFFFFFF))[0]
            return abs(f - ours[1]) < 1e-6 or (f == 0 and ours[1] == 0)
        return None
    if param.is_float and sk == "int":
        f = struct.unpack("<f", struct.pack("<I", stock[1] & 0xFFFFFFFF))[0]
        return abs(f - float(ours[1])) < 1e-6
    return (stock[1] & 0xFFFFFFFF) == (ours[1] & 0xFFFFFFFF)


def show_stock(v):
    k = v[0]
    if k == "int":
        return str(v[1]) if -1000 < v[1] < 1000 else hex(v[1] & 0xFFFFFFFF)
    if k == "str":
        return 'L"' + v[1] + '"'
    if k == "ptr":
        return f"&{v[1]:#x}"
    return f"?({v[1]})"


def show_ours(v):
    k = v[0]
    if k == "str":
        return 'L"' + v[1] + '"'
    if k == "int":
        return str(v[1]) if -1000 < v[1] < 1000 else hex(v[1] & 0xFFFFFFFF)
    if k == "float":
        return repr(v[1])
    return f"?({v[1]})"


# ------------------------------------------------------------------------------------------------
# part 1
# ------------------------------------------------------------------------------------------------
def class_headers():
    sys.path.insert(0, str(ROOT / "tools"))
    from gen_implement_class import class_headers as ch
    return ch()


def check_ctors(classes, img, headers):
    rows, stocks = [], {}
    for c in classes:
        va = c["internal_ctor"]
        if va not in stocks:
            stocks[va] = model_internal_ctor(img, va)
        st = stocks[va]
        name = c["cpp"]
        row = {"class": name, "stock": st, "problems": [], "verdict": "OK", "ours": None, "decl": "-"}
        rows.append(row)
        sargs = [resolve_stock_value(img, v) for v in st.args]
        row["sargs"] = sargs
        # the stock call must be consistent: pushed words == ret N of the constructor it calls
        if st.notes:
            row["problems"].append("stock model: " + "; ".join(st.notes))
        if st.ctor_ret is not None and st.ctor_ret != 4 * len(st.pushed):
            row["problems"].append(f"stock pushes {len(st.pushed)} word(s) but the constructor {st.call:#x} pops {st.ctor_ret}")
        if st.call != c["ctor"]:
            row["problems"].append(f"registration says constructor {c['ctor']:#x}, the InternalConstructor calls {st.call:#x}"
                                   if st.call is not None else "no constructor call")
        hdr = headers.get(name)
        if not hdr:
            row["verdict"] = "NO-HEADER"
            continue
        ctors, bases = parse_ctors(ROOT / "include" / hdr, name)
        if ctors is None:
            row["verdict"] = "NO-CLASS-BODY"
            continue
        row["bases"] = bases
        if not ctors:                                   # no constructor declared: the implicit one
            row["decl"] = "(implicit)"
            row["ours"] = []
            if sargs:
                row["verdict"] = "NO-DEFAULT-CTOR"
                row["problems"].append("the class declares no constructor; the stock one takes arguments")
            continue
        cands = [k for k in ctors if k.callable_without_args and not k.variadic]
        if not cands:
            row["verdict"] = "NO-DEFAULT-CTOR"
            row["decl"] = " | ".join(k.text for k in ctors)
            row["problems"].append("no constructor callable without arguments: " + "; ".join(str(k) for k in ctors))
            continue
        if len(cands) > 1:
            row["verdict"] = "AMBIGUOUS"
            row["decl"] = " | ".join(k.text for k in cands)
            row["problems"].append("several constructors callable without arguments: " + "; ".join(str(k) for k in cands))
            continue
        k = cands[0]
        row["decl"] = f"{k.text}   [{k.header}:{k.line}]"
        ours = [eval_default(p.default, p.is_float) for p in k.params]
        row["ours"] = ours
        # compare slot by slot (a 64-bit parameter takes two pushed words: only counted)
        if k.total_slots != len(sargs):
            row["verdict"] = "ARITY"
            row["problems"].append(f"our constructor takes {k.total_slots} argument word(s), the stock InternalConstructor pushes {len(sargs)}")
            continue
        if any(p.slots != 1 for p in k.params):
            row["verdict"] = "UNCHECKED"
            row["problems"].append("a multi-word parameter: compare by hand")
            continue
        bad, unk = [], []
        for idx, (p, s, o) in enumerate(zip(k.params, sargs, ours)):
            eq = values_equal(s, o, p)
            if eq is None:
                unk.append(f"arg{idx + 1}: stock {show_stock(s)} / ours {show_ours(o)}")
            elif not eq:
                bad.append(f"arg{idx + 1}: stock {show_stock(s)}, ours {show_ours(o)}")
        if bad:
            row["verdict"] = "MISMATCH"
            row["problems"] += bad
        elif unk:
            row["verdict"] = "UNCHECKED"
            row["problems"] += unk
    return rows


def print_ctor_table(rows, show_all):
    print("== InternalConstructor: stock arguments against our default constructor")
    print(f"{'class':28} {'stock IC':10} {'stock ctor':10} {'stock args':34} verdict   our declaration")
    for r in rows:
        st = r["stock"]
        interesting = show_all or r["verdict"] != "OK" or r["sargs"] or r["problems"]
        if not interesting:
            continue
        args = ", ".join(show_stock(v) for v in r["sargs"]) or "-"
        ctor = f"{st.call:#x}" if st.call else "?"
        print(f"{r['class']:28} {st.va:#010x} {ctor:10} {args:34} {r['verdict']:8}  {r['decl']}")
        for p in r["problems"]:
            print(f"{'':28}   ! {p}")
    from collections import Counter
    cnt = Counter(r["verdict"] for r in rows)
    shared = Counter(r["stock"].va for r in rows)
    with_args = sum(1 for r in rows if r["sargs"])
    print(f"{len(rows)} classes, {len({r['stock'].va for r in rows})} distinct stock InternalConstructors "
          f"({sum(1 for v in shared.values() if v > 1)} shared), {with_args} push arguments; verdicts: "
          + ", ".join(f"{k} {v}" for k, v in sorted(cnt.items())))
    return all(r["verdict"] == "OK" and not r["problems"] for r in rows)


def check_db_names(classes):
    """Informational: how many stock InternalConstructors db/functions.csv names (Ghidra did not split all)."""
    names = {}
    if FUNCS_CSV.exists():
        for r in csv.DictReader(FUNCS_CSV.open(encoding="utf-8")):
            if r["name"].endswith("::InternalConstructor"):
                names[int(r["va"], 16)] = r["name"].split("::")[0]
    bad, unnamed = [], 0
    for c in classes:
        n = names.get(c["internal_ctor"])
        if n is None:
            unnamed += 1
        elif n != c["cpp"] and c["cpp"] not in ("NCAbstractMain", "NCCmdTarget"):
            bad.append(f"{c['cpp']}: classes.json InternalConstructor {c['internal_ctor']:#x} is named {n}::InternalConstructor in the db")
    print(f"db/functions.csv names {len(names)} InternalConstructor functions; {unnamed} classes have one the db does not name "
          f"(read from the image anyway); naming conflicts: {len(bad)}")
    for b in bad:
        print("  !", b)
    return not bad


# ------------------------------------------------------------------------------------------------
# part 1b: the InternalConstructor the standalone COMPILER emitted, against the stock one
# ------------------------------------------------------------------------------------------------
DEFAULT_PROBE = ROOT / "build" / "standalone_probe" / "Release" / "NWindow_probe.dll"


def compiled_symbols(map_path: Path) -> dict:
    """{class: VA} of every ?InternalConstructor@<Class>@@SAXPAX@Z in a link map."""
    syms = {}
    for line in map_path.read_text(encoding="latin-1").splitlines():
        m = re.match(r"\s*[0-9a-f]{4}:[0-9a-f]{8}\s+\?InternalConstructor@(\w+)@@SAXPAX@Z\s+([0-9a-f]{8})", line)
        if m:
            syms[m.group(1)] = int(m.group(2), 16)
    return syms


def check_compiled(rows, dll: Path, map_path: Path) -> bool:
    """Disassemble every `Class::InternalConstructor` in the standalone DLL (the probe link of
    `cmake --build build --config Release --target NWindowStandaloneProbe`) and compare what it pushes
    and the `ret N` of the constructor it calls with the stock ones: the proof that the default
    arguments the compiler used are the stock constants."""
    print("== compiled InternalConstructor (standalone) against the stock one")
    if not dll.exists() or not map_path.exists():
        print(f"{dll} / {map_path} not found: build the target NWindowStandaloneProbe first (not checked)")
        return True
    newest_src = max(p.stat().st_mtime for p in list(HDR_DIR.glob("*.h")) + [GEN_CPP])
    if dll.stat().st_mtime < newest_src:
        print(f"WARNING: {dll.name} is older than the newest header: rebuild NWindowStandaloneProbe first")
    pimg = Image(dll)
    syms = compiled_symbols(map_path)
    bad, checked, same_args, same_ret = [], 0, 0, 0
    for r in rows:
        name = r["class"]
        stock = r["stock"]
        # NCCmdTarget and NCAbstractMain declare no InternalConstructor of their own: the parent's one is registered
        sym = syms.get(name)
        if sym is None:
            if r["bases"] is not None and name in ("NCCmdTarget", "NCAbstractMain"):
                continue
            bad.append(f"{name}: no ?InternalConstructor@{name}@@SAXPAX@Z in the map")
            continue
        cs = model_internal_ctor(pimg, sym)
        checked += 1
        cargs = [resolve_stock_value(pimg, v) for v in cs.args]
        sargs = r["sargs"]
        problems = []
        if cs.notes:
            problems.append("compiled code not modelled: " + "; ".join(cs.notes))
        if len(cargs) != len(sargs):
            problems.append(f"the compiled InternalConstructor pushes {len(cargs)} argument(s) {[show_stock(v) for v in cargs]}, the stock one {len(sargs)} {[show_stock(v) for v in sargs]}")
        else:
            diffs = []
            for i, (a, b) in enumerate(zip(sargs, cargs)):
                if a[0] == "ptr" and b[0] == "ptr":
                    continue
                if a != b:
                    diffs.append(f"arg{i + 1}: stock {show_stock(a)}, compiled {show_stock(b)}")
            if diffs:
                problems.append("compiled InternalConstructor passes different constants: " + "; ".join(diffs))
            else:
                same_args += 1
        if cs.ctor_ret is not None and stock.ctor_ret is not None:
            if cs.ctor_ret != stock.ctor_ret:
                problems.append(f"the constructor the standalone calls pops {cs.ctor_ret} bytes, the stock one {stock.ctor_ret}")
            else:
                same_ret += 1
        if problems:
            r["verdict"] = "COMPILED-DIFF" if r["verdict"] == "OK" else r["verdict"]
            r["problems"] += problems
            bad += [f"{name}: {p}" for p in problems]
    print(f"{checked} compiled InternalConstructors read from {dll.name}: {same_args} push exactly the stock constants, "
          f"{same_ret} call a constructor with the stock `ret N`; differences: {len(bad)}")
    for b in bad:
        print("  !", b)
    return not bad


# ------------------------------------------------------------------------------------------------
# part 2: the registration
# ------------------------------------------------------------------------------------------------
DECL_FULL = re.compile(r"NW_DECLARE_(ABSTRACT_)?CLASS\(\s*(\w+)\s*,\s*(\w+)\s*,\s*([^,]+?)\s*,\s*(0x[0-9a-fA-F]+|\d+)\s*\)")
IMPL_RE = re.compile(r"^NW_IMPLEMENT_CLASS\((\w+),\s*(\w+),\s*([^)]*)\)\s*//\s*stock UClass (0x[0-9a-fA-F]+)", re.M)
SIZE_RE = re.compile(r"^NW_ASSERT_SIZE\((\w+),\s*(0x[0-9a-fA-F]+)\);", re.M)


def flags_value(expr: str):
    v = 0
    for part in expr.split("|"):
        part = part.strip().strip("()")
        if part in CLASS_FLAG_VALUES:
            v |= CLASS_FLAG_VALUES[part]
        elif re.fullmatch(r"(0x[0-9a-fA-F]+|\d+)", part):
            v |= int(part, 0)
        else:
            return None
    return v


def check_stock_registration(classes, img: Image) -> bool:
    """analysis/classes.json is what every other check trusts: read the stock initializers again and confirm
    the facts this tool and Classes.gen.cpp take from it that gen_implement_class.py --check does not
    re-read - the parent UClass (its PrivateStaticClass, or the engine's), the PrivateStaticClass address
    (the `mov ecx, <this>` of the initializer) and the InternalConstructor registered."""
    from extract_classes import PARAMS, UCLASS_CTOR, emulate_to

    print("== stock initializers read again (classes.json is only trusted where re-read here)")
    call = b"\xff\x15" + img.import_slot(UCLASS_CTOR).to_bytes(4, "little")
    by_static = {c["static_class"]: c["cpp"] for c in classes}
    bad = []
    for c in classes:
        code = img.read(c["initializer"], 0x100)
        site = code.find(call)
        if site < 0:
            bad.append(f"{c['cpp']}: no UClass constructor call in the initializer {c['initializer']:#x}")
            continue
        regs, args = emulate_to(img, c["initializer"], c["initializer"] + site)
        d = {k: v.v for k, v in zip(PARAMS, args)}
        sup = d["Super"]
        if isinstance(sup, int):
            sup_name = by_static.get(sup)
        else:
            m = re.fullmatch(r"&\?PrivateStaticClass@(\w+)@@0VUClass@@A", str(sup))
            sup_name = m.group(1) if m else None
        if sup_name != c["super"]:
            bad.append(f"{c['cpp']}: the stock initializer registers parent {sup!r} ({sup_name}), classes.json says {c['super']}")
        if d["InternalConstructor"] != c["internal_ctor"]:
            bad.append(f"{c['cpp']}: the stock initializer registers InternalConstructor {d['InternalConstructor']!r}, classes.json {c['internal_ctor']:#x}")
        this = regs.get("ecx")
        if this is not None and this.v != c["static_class"]:
            bad.append(f"{c['cpp']}: the initializer constructs the UClass at {this.v!r}, classes.json static_class {c['static_class']:#x}")
    print(f"{len(classes)} stock initializers: parent UClass, InternalConstructor and UClass address agree with "
          f"classes.json; differences: {len(bad)}")
    for b in bad:
        print("  !", b)
    return not bad


def check_registration(classes, headers):
    problems = []
    by_name = {c["cpp"]: c for c in classes}
    ext_parents = {"UObject", "UL2ConsoleWnd", "UL2Font"}

    def bad(msg):
        problems.append(msg)

    # --- stock registration data shared by every class (what NW_IMPLEMENT_CLASS hardcodes)
    for c in classes:
        if c["within"] != "UObject" or c["within_raw"] != "&?PrivateStaticClass@UObject@@0VUClass@@A":
            bad(f"{c['cpp']}: Within is {c['within']}, NW_IMPLEMENT_CLASS passes UObject")
        if c["package"] != "NWindow" or c["guid"] != [0, 0, 0, 0]:
            bad(f"{c['cpp']}: package/GUID differ from the NW_IMPLEMENT_CLASS ones")
        if c["flags"] != 0x4084004:
            bad(f"{c['cpp']}: object flags {c['flags']:#x}, NW_IMPLEMENT_CLASS passes 0x4084004")
        if c["super"] not in by_name and c["super"] not in ext_parents:
            bad(f"{c['cpp']}: registered parent {c['super']} is neither a class of the package nor UObject/UL2ConsoleWnd/UL2Font")
        if c["class_flags"] & ~5:
            bad(f"{c['cpp']}: class flags {c['class_flags']:#x} have bits NW_IMPLEMENT_CLASS cannot name")

    # --- the generated file
    text = GEN_CPP.read_text(encoding="utf-8")
    impls = {m.group(1): m for m in IMPL_RE.finditer(text)}
    sizes = {m.group(1): int(m.group(2), 16) for m in SIZE_RE.finditer(text)}
    order = [m.group(1) for m in IMPL_RE.finditer(text)]
    if len(impls) != len(classes) or len(order) != len(impls):
        bad(f"Classes.gen.cpp has {len(order)} NW_IMPLEMENT_CLASS lines for {len(classes)} classes")
    if order != [c["cpp"] for c in sorted(classes, key=lambda c: c["initializer"])]:
        bad("Classes.gen.cpp is not in the order of the stock initializers")
    for c in classes:
        m = impls.get(c["cpp"])
        if m is None:
            bad(f"{c['cpp']}: no NW_IMPLEMENT_CLASS in Classes.gen.cpp")
            continue
        if m.group(2) != c["super"]:
            bad(f"{c['cpp']}: Classes.gen.cpp registers parent {m.group(2)}, stock {c['super']}")
        if flags_value(m.group(3)) != c["class_flags"]:
            bad(f"{c['cpp']}: Classes.gen.cpp registers class flags {m.group(3)}, stock {c['class_flags']:#x}")
        if int(m.group(4), 16) != c["static_class"]:
            bad(f"{c['cpp']}: Classes.gen.cpp names stock UClass {m.group(4)}, stock {c['static_class']:#x}")
        if sizes.get(c["cpp"]) != c["size"]:
            bad(f"{c['cpp']}: Classes.gen.cpp asserts sizeof {sizes.get(c['cpp'])}, stock {c['size']:#x}")

    # --- the NW_DECLARE_CLASS of each header
    diverging = []
    hdr_text = {}
    for c in classes:
        name = c["cpp"]
        hdr = headers.get(name)
        if not hdr:
            bad(f"{name}: no NW_DECLARE_CLASS in include/nwindow")
            continue
        raw = hdr_text.setdefault(hdr, (INCLUDE / hdr).read_text(encoding="utf-8", errors="replace"))
        clean = strip_comments(raw)
        ms = [m for m in DECL_FULL.finditer(clean) if m.group(2) == name]
        if len(ms) != 1:
            bad(f"{name}: {len(ms)} NW_DECLARE_CLASS declarations in {hdr}")
            continue
        m = ms[0]
        is_abs, sup, flg, va = bool(m.group(1)), m.group(3), m.group(4), int(m.group(5), 0)
        if va != c["static_class"]:
            bad(f"{name}: NW_DECLARE_CLASS StaticClassVA {va:#x}, stock UClass {c['static_class']:#x} (hook mode's StaticClass() would be wrong)")
        fv = flags_value(flg)
        if fv is None:
            bad(f"{name}: NW_DECLARE_CLASS flags '{flg}' not evaluated")
        elif (fv | (1 if is_abs else 0)) != c["class_flags"]:
            bad(f"{name}: header StaticClassFlags {fv | (1 if is_abs else 0):#x}, stock class flags {c['class_flags']:#x}")
        if bool(c["class_flags"] & 1) != is_abs:
            bad(f"{name}: {'NW_DECLARE_ABSTRACT_CLASS' if is_abs else 'NW_DECLARE_CLASS'} but stock CLASS_Abstract is {c['class_flags'] & 1}")
        fb = find_class_body(clean, name)
        base = None
        if fb and fb[2]:
            mb = re.search(r"public\s+(\w+)", fb[2])
            base = mb.group(1) if mb else None
        if base is not None and base != sup:
            bad(f"{name}: C++ base {base} but NW_DECLARE_CLASS Super {sup}")
        if sup != c["super"]:
            diverging.append((name, sup, c["super"]))
        # size asserted in the header
        mz = re.search(r"NW_ASSERT_SIZE\(\s*" + re.escape(name) + r"\s*,\s*(0x[0-9a-fA-F]+|\d+)\s*\)", clean)
        if mz and int(mz.group(1), 0) != c["size"]:
            bad(f"{name}: header asserts size {mz.group(1)}, stock {c['size']:#x}")
        elif not mz:
            bad(f"{name}: no NW_ASSERT_SIZE in {hdr} (Classes.gen.cpp still asserts it)")

    # --- the abstract classes share the parent's InternalConstructor (no InternalConstructor of their own)
    for c in classes:
        if c["class_flags"] & 1:
            parent = by_name.get(c["super"])
            if parent is None or parent["internal_ctor"] != c["internal_ctor"]:
                bad(f"{c['cpp']}: abstract but the stock registers its own InternalConstructor {c['internal_ctor']:#x}")

    print("== registration: NW_DECLARE_CLASS / NW_IMPLEMENT_CLASS against the stock UClass data")
    print(f"{len(classes)} classes: Within UObject, package NWindow, GUID 0, object flags 0x4084004 in all; "
          f"class flags {sum(1 for c in classes if c['class_flags'] == 0)}x0, "
          f"{sum(1 for c in classes if c['class_flags'] == 1)}x CLASS_Abstract, "
          f"{sum(1 for c in classes if c['class_flags'] == 4)}x CLASS_Config; "
          f"Classes.gen.cpp: {len(order)} registrations, sizes asserted {len(sizes)}")
    print(f"C++ base (Super) differs from the registered parent in {len(diverging)} class(es) "
          f"(Classes.gen.cpp registers the stock parent, the header keeps the C++ base):")
    for name, sup, reg in diverging:
        print(f"  {name}: header Super {sup}, registered parent {reg}")
    print(f"registration problems: {len(problems)}")
    for p in problems:
        print("  !", p)
    return not problems


def map_symbols(map_path: Path) -> dict:
    """{decorated name: VA} of every public symbol in a link map."""
    syms = {}
    for line in map_path.read_text(encoding="latin-1").splitlines():
        m = re.match(r"\s*[0-9a-f]{4}:[0-9a-f]{8}\s+(\S+)\s+([0-9a-f]{8})", line)
        if m:
            syms.setdefault(m.group(1), int(m.group(2), 16))
    return syms


def check_compiled_registration(classes, dll: Path, map_path: Path) -> bool:
    """Read the UClass constructor call of every compiled `Class::PrivateStaticClass` initializer in the
    standalone DLL (extract_classes.py's emulator, the one gen_implement_class.py uses on the stock image)
    and compare it with the stock registration in analysis/classes.json: size, class flags, the parent
    UClass (its PrivateStaticClass in this DLL, or the engine's), Within, GUID, name, package, config,
    object flags, the InternalConstructor and the static constructor."""
    from extract_classes import PARAMS, UCLASS_CTOR, emulate_to

    print("== compiled registration (standalone) against the stock UClass data")
    if not dll.exists() or not map_path.exists():
        print(f"{dll} / {map_path} not found: build the target NWindowStandaloneProbe first (not checked)")
        return True
    img = Image(dll)
    syms = map_symbols(map_path)
    gpackage = img.exports.get("GPackage")
    call = b"\xff\x15" + img.import_slot(UCLASS_CTOR).to_bytes(4, "little")   # call dword ptr [slot]
    by_name = {c["cpp"]: c for c in classes}
    bad = []
    n_ok = 0
    for c in classes:
        name = c["cpp"]
        init = syms.get(f"??__E?PrivateStaticClass@{name}@@0VUClass@@A@@YAXXZ")
        if init is None:
            bad.append(f"{name}: no PrivateStaticClass initializer in the map")
            continue
        site = img.read(init, 0x100).find(call)
        if site < 0:
            bad.append(f"{name}: no UClass constructor call in the initializer {init:#x}")
            continue
        _, args = emulate_to(img, init, init + site)
        d = {k: v.v for k, v in zip(PARAMS, args)}
        if c["super"] in by_name:
            want_super = syms.get(f"?PrivateStaticClass@{c['super']}@@0VUClass@@A")
        else:
            want_super = f"&?PrivateStaticClass@{c['super']}@@0VUClass@@A"
        # the InternalConstructor registered: the class's own, or the parent's for the abstract ones
        owner = name
        while f"?InternalConstructor@{owner}@@SAXPAX@Z" not in syms and owner in by_name and by_name[owner]["super"] in by_name:
            owner = by_name[owner]["super"]
        want = {"ENative": 0, "Size": c["size"], "ClassFlags": c["class_flags"], "Super": want_super,
                "Within": "&?PrivateStaticClass@UObject@@0VUClass@@A", "Guid0": 0, "Guid1": 0, "Guid2": 0, "Guid3": 0,
                "Package": gpackage, "Config": "ret:?StaticConfigName@UObject@@SAPBGXZ", "Flags": 0x4084004,
                "InternalConstructor": syms.get(f"?InternalConstructor@{owner}@@SAXPAX@Z"),
                "StaticConstructor": "&?StaticConstructor@UObject@@QAEXXZ"}
        errs = [f"{k} is {d[k]!r}, stock registration {v!r}" for k, v in want.items() if d[k] != v]
        nm = img.wstr(d["Name"]) if isinstance(d["Name"], int) else None
        if nm != name[1:] or (isinstance(d["Name"], int) and img.read(d["Name"] - 2, 2) != name[0].encode("utf-16-le")):
            errs.append(f"name {nm!r} is not TEXT(\"{name}\")+1")
        if errs:
            bad += [f"{name}: {e}" for e in errs]
        else:
            n_ok += 1
    print(f"{len(classes)} compiled UClass registrations read from {dll.name}: {n_ok} identical to the stock one "
          f"(size, class flags, parent UClass, Within, GUID, name, package, config, object flags, InternalConstructor, "
          f"static constructor); differences: {len(bad)}")
    for b in bad:
        print("  !", b)
    return not bad


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--all", action="store_true", help="list every class")
    ap.add_argument("--no-registration", action="store_true")
    ap.add_argument("--probe", type=Path, default=None,
                    help="the standalone DLL to read the compiled code from (default: build/standalone/Release/NWindow.dll "
                         "when it links, else the probe link)")
    ap.add_argument("--map", type=Path, default=None, help="its link map (default: next to the DLL, .map)")
    ap.add_argument("--no-compiled", action="store_true", help="skip the check of the compiled code")
    args = ap.parse_args()
    if args.probe is None:
        real = ROOT / "build" / "standalone" / "Release" / "NWindow.dll"
        args.probe = real if real.exists() and real.with_suffix(".map").exists() else DEFAULT_PROBE
    classes = json.loads(CLASSES_JSON.read_text(encoding="utf-8"))
    img = Image()
    headers = class_headers()
    rows = check_ctors(classes, img, headers)
    ok = True
    if not args.no_compiled:
        ok &= check_compiled(rows, args.probe, args.map or args.probe.with_suffix(".map"))
        print()
    ok &= print_ctor_table(rows, args.all)
    ok &= check_db_names(classes)
    if not args.no_registration:
        print()
        ok &= check_stock_registration(classes, img)
        print()
        ok &= check_registration(classes, headers)
        if not args.no_compiled:
            print()
            ok &= check_compiled_registration(classes, args.probe, args.map or args.probe.with_suffix(".map"))
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
