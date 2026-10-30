"""A small parser for the declarations of include/ue2 (UE2Core.h, UE2Engine.h and the generated fragments):
statements of every class body, with the class, the access level and the text, enough to generate the
check code of tools/ue2_check/gen_checks.py. Not a C++ parser: the headers are regular (one declaration
per statement, no nested templates in member declarations that matter here)."""
from __future__ import annotations

import re
from dataclasses import dataclass, field
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
INC = ROOT / "include" / "ue2"

API_RE = re.compile(r"\b(?:ENGINE_API|CORE_API)\b")
SKIP_WORDS = {"ENGINE_API", "CORE_API", "virtual", "inline", "FORCEINLINE", "static", "extern", "friend"}
CONV = {"STDCALL": "__stdcall", "VARARGS": "__cdecl"}


def strip_comments(t: str) -> str:
    t = re.sub(r"/\*.*?\*/", lambda m: "\n" * m.group(0).count("\n"), t, flags=re.S)
    return re.sub(r"//[^\n]*", "", t)


def with_includes(path: Path) -> str:
    out = []
    for line in path.read_text(encoding="utf-8").split("\n"):
        m = re.match(r'\s*#include\s+"(gen/[\w.]+\.inc)"', line)
        out.append((INC / m.group(1)).read_text(encoding="utf-8") if m else line)
    return "\n".join(out)


@dataclass
class Decl:
    cls: str                 # '' for a free declaration
    access: str              # public / protected / private
    text: str                # the statement without the trailing ';'
    api: bool = False        # ENGINE_API / CORE_API on the statement (or on the whole class)
    virtual: bool = False
    static: bool = False
    pure: bool = False
    const: bool = False
    extern: bool = False
    name: str = ""
    ret: str = ""
    params: list[str] = field(default_factory=list)   # parameter types, names and defaults stripped
    raw_params: str = ""
    conv: str = ""           # '' | '__stdcall' | '__cdecl'
    varargs: bool = False
    is_func: bool = False
    is_ctor: bool = False
    is_dtor: bool = False
    base: str = ""           # class base list text


def split_top(s: str) -> list[str]:
    out, depth, cur = [], 0, ""
    for ch in s:
        if ch in "<([":
            depth += 1
        elif ch in ">)]":
            depth -= 1
        if ch == "," and depth == 0:
            out.append(cur.strip())
            cur = ""
        else:
            cur += ch
    if cur.strip():
        out.append(cur.strip())
    return out


KEYWORDS = {"unsigned", "signed", "long", "short", "int", "char", "float", "double", "void", "const", "struct",
            "class", "enum", "volatile", "bool"}


def param_type(p: str) -> str:
    """The type of a parameter declaration: default argument and parameter name removed."""
    p = p.split("=")[0].strip() if "=" in p and "operator" not in p else p
    # 'const TCHAR* Key', 'FName Name', 'UObject*& Res', 'INT'
    m = re.match(r"^(.*?)(?:\s+|(?<=[*&]))([A-Za-z_]\w*)$", p)
    if m and m.group(1).strip() and m.group(2) not in KEYWORDS:
        head = m.group(1).strip()
        if head not in ("const", "unsigned", "signed", "struct", "class", "enum", "volatile"):
            return head
    return p


def parse_function(d: Decl) -> None:
    t = re.sub(r"\s+", " ", d.text).strip()
    t = re.sub(r"^(?:ENGINE_API|CORE_API)\s+", "", t)
    d.virtual = bool(re.search(r"\bvirtual\b", t))
    d.static = bool(re.search(r"\bstatic\b", t))
    d.extern = bool(re.search(r"\bextern\b", t))
    m = re.search(r"(?:\s*=\s*0)?\s*$", t)
    if re.search(r"=\s*0\s*$", t):
        d.pure = True
        t = re.sub(r"\s*=\s*0\s*$", "", t)
    if "(" not in t or t.rstrip().endswith(")") is False and not re.search(r"\)\s*const\s*$", t):
        return
    # name: identifier (or operator...) right before the first '(' that is not part of a function-pointer type
    m = re.search(r"((?:operator\s*(?:\(\)|\[\]|new|delete|[^\s(\w]+|\w+))|~?\w+)\s*\(", t)
    if not m:
        return
    d.name = re.sub(r"\s+", "", m.group(1)) if m.group(1).startswith("operator") else m.group(1)
    if re.fullmatch(r"operator(new|delete)(\[\])?", d.name):
        d.name = d.name.replace("operator", "operator ", 1)
    head = t[:m.start()]
    rest = t[m.end():]
    # params up to the matching ')'
    depth, i = 1, 0
    while i < len(rest) and depth:
        depth += {"(": 1, ")": -1}.get(rest[i], 0)
        i += 1
    d.raw_params = rest[:i - 1]
    tail = rest[i:]
    d.const = bool(re.search(r"\bconst\b", tail))
    for tok, cc in CONV.items():
        if re.search(r"\b%s\b" % tok, head):
            d.conv = cc
    ps = split_top(d.raw_params)
    if ps and ps[-1] == "...":
        d.varargs = True
        ps = ps[:-1]
    if ps == ["void"]:
        ps = []
    d.params = [param_type(p) for p in ps]
    ret = re.sub(r"\b(?:ENGINE_API|CORE_API|virtual|inline|FORCEINLINE|static|STDCALL|VARARGS|friend)\b", " ", head)
    d.ret = re.sub(r"\s+", " ", ret).strip()
    d.is_func = True
    d.is_dtor = d.name.startswith("~")
    d.is_ctor = (not d.is_dtor) and d.ret == "" and d.name == d.cls


BASES: dict[str, list[str]] = {}      # class -> its base classes (filled by parse)


def parse(paths: list[Path]) -> list[Decl]:
    decls: list[Decl] = []
    for path in paths:
        text = strip_comments(with_includes(path))
        stack: list[tuple[str, str, bool]] = []     # (class name or '', access, class has API)
        buf = ""
        for ch in text:
            if ch == "{":
                m = re.search(r"\b(class|struct)\s+((?:ENGINE_API|CORE_API)\s+)?(\w+)\s*(?::([^{;]*))?$", buf)
                if m:
                    stack.append((m.group(3), "private" if m.group(1) == "class" else "public", bool(m.group(2))))
                    BASES[m.group(3)] = [b.strip().split()[-1] for b in (m.group(4) or "").split(",") if b.strip()]
                else:
                    stack.append(("", "public", False))
                buf = ""
            elif ch == "}":
                if stack:
                    stack.pop()
                buf = ""
            elif ch == ";":
                stmt = re.sub(r"\s+", " ", buf).strip()
                buf = ""
                cls, acc, cls_api = next(((c, a, k) for c, a, k in reversed(stack) if c), ("", "public", False))
                if not stmt or stmt.startswith("#") or stmt.startswith(("typedef", "using", "enum", "friend", "template", "static_assert")):
                    continue
                if stmt.startswith(("class ", "struct ")) and "(" not in stmt:
                    continue
                d = Decl(cls=cls, access=acc, text=stmt)
                d.api = bool(API_RE.search(stmt)) or (cls_api and "(" in stmt)
                if "(" in stmt and not re.match(r"^(?:extern )?\"C\"", stmt):
                    parse_function(d)
                if not d.is_func:
                    d.extern = bool(re.search(r"\bextern\b", stmt))
                    m = re.search(r"(\w+)\s*(?:\[[^\]]*\])?\s*$", re.sub(r"\s*=.*$", "", stmt))
                    d.name = m.group(1) if m else ""
                    d.static = bool(re.search(r"\bstatic\b", stmt))
                decls.append(d)
            elif ch == "\n" and buf.strip().startswith("#"):
                buf = ""
            elif ch == ":" and re.search(r"(?:^|\s)(public|protected|private)\s*$", buf):
                m = re.search(r"(public|protected|private)\s*$", buf)
                if stack:
                    stack[-1] = (stack[-1][0], m.group(1), stack[-1][2])
                buf = ""
            else:
                buf += ch
    return decls
