#!/usr/bin/env python3
"""Which Core.dll / Engine.dll imports of orig/NWindow.dll no header of include/ue2 declares.

Every name NWindow imports is undecorated (undname) into (class, member name) / (function name) and
looked up among the ENGINE_API / CORE_API declarations of UE2Core.h, UE2Engine.h and the generated
fragments (include/ue2/gen, which UE2Engine.h includes inside the class bodies). Matching is by
class + name (overloads are not told apart here: the link test, tools/ue2_check/, does that).
Virtual functions of an exported class count too: a class derived from UObject imports the base
virtuals it does not override (they sit in its vtable).

  python tools/ue2_check_imports.py [--dll NWindow.dll]      # exit 1 when something is missing
"""
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

import pefile

sys.path.insert(0, str(Path(__file__).resolve().parent))
import engine_vtables as ev  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
INC = ROOT / "include" / "ue2"


def strip_comments(t: str) -> str:
    t = re.sub(r"/\*.*?\*/", "", t, flags=re.S)
    return re.sub(r"//[^\n]*", "", t)


def with_includes(path: Path) -> str:
    """The header text with its `#include "gen/X.vt.inc"` lines replaced by the fragment."""
    out = []
    for line in path.read_text(encoding="utf-8").split("\n"):
        m = re.match(r'\s*#include\s+"(gen/[\w.]+\.inc)"', line)
        out.append((INC / m.group(1)).read_text(encoding="utf-8") if m else line)
    return "\n".join(out)


def declared(paths: list[Path]) -> set[tuple[str, str]]:
    """{(class or '', name)} of every API declaration (member functions, data, free functions)."""
    out: set[tuple[str, str]] = set()
    for path in paths:
        text = strip_comments(with_includes(path))
        stack: list[str] = []         # class name per open brace ('' for other braces)
        pending = ""
        buf = ""
        for ch in text:
            if ch == "{":
                m = re.search(r"\b(?:class|struct)\s+(?:\w+_API\s+)?(\w+)\s*(?::[^{;]*)?$", buf)
                stack.append(m.group(1) if m else "")
                buf = ""
            elif ch == "}":
                if stack:
                    stack.pop()
                buf = ""
            elif ch == ";":
                stmt = buf.strip()
                buf = ""
                cls = next((c for c in reversed(stack) if c), "")
                if re.search(r"\b(?:ENGINE_API|CORE_API)\b", stmt) and not stmt.startswith("#"):
                    m = re.search(r"((?:operator\s*(?:\(\)|\[\]|[^\s(]+))|~?\w+)\s*\(", stmt)
                    if m:
                        out.add((cls, re.sub(r"\s+", "", m.group(1))))
                    else:
                        m = re.search(r"(\w+)\s*(?:\[[^\]]*\])?\s*$", stmt)
                        if m:
                            out.add((cls, m.group(1)))
            elif ch == "\n" and buf.lstrip().startswith("#"):
                buf = ""              # a preprocessor line
            else:
                buf += ch
        # `class CORE_API Name { ... }` marks every member exported: nothing to add here
    return out


def split_decl(und: str):
    """undname text -> (class, name) or ('', name) for a free function / variable."""
    s = und
    s = re.sub(r"^(?:public|protected|private):\s*", "", s)
    s = re.sub(r"\b(?:static|virtual)\s+", "", s)
    m = re.search(r"(?:__thiscall|__cdecl|__stdcall)\s+([\w:<>~]+(?:::~?\w+|::operator\S*)?)\s*\(", s)
    if m:
        full = m.group(1)
    else:
        m = re.search(r"([\w:<>~]+(?:::operator\S+)?)\s*$", s)           # a variable
        full = m.group(1) if m else s
    parts = full.split("::")
    return ("::".join(parts[:-1]), parts[-1]) if len(parts) > 1 else ("", parts[0])


def operator_name(cls: str, name: str) -> str:
    return name.replace("operator ", "operator")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--dll", default=str(ROOT / "orig" / "NWindow.dll"))
    a = ap.parse_args()
    decl = declared([INC / "UE2Core.h", INC / "UE2Engine.h"])
    pe = pefile.PE(a.dll)
    missing = 0
    for e in pe.DIRECTORY_ENTRY_IMPORT:
        dn = e.dll.decode().lower()
        if dn not in ("core.dll", "engine.dll"):
            continue
        names = [i.name.decode() for i in e.imports if i.name]
        und = ev.undname(names)
        miss = []
        for n, u in zip(names, und):
            cls, name = split_decl(u)
            name = operator_name(cls, name)
            cls_short = cls.split("::")[-1] if cls else ""
            # constructors / destructors / data / globals: same lookup; FStringNoInit & co. are classes
            ok = (cls_short, name) in decl or (cls_short, re.sub(r"\s+", "", name)) in decl
            if not ok and name.startswith("operator"):
                ok = any(c == cls_short and d.startswith("operator") and d[8:] == name[8:] for c, d in decl)
            if not ok and not cls_short:
                ok = ("", name) in decl
            if not ok:
                miss.append((n, u))
        print(f"{dn}: {len(names)} imports, {len(miss)} not declared in include/ue2")
        for n, u in miss:
            print(f"   {u}\n      {n}")
        missing += len(miss)
    return 1 if missing else 0


if __name__ == "__main__":
    sys.exit(main())
