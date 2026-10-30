#!/usr/bin/env python3
"""Every stock address literal a hooked body uses must be one the stock function it replaces uses too.

The C3 -> C4 address map is ambiguous for tiny functions that the linker folded (the `jmp [IAT]` winsock thunks, the `mov eax, [ecx + N]; ret`
getters): a literal remapped by its neighbours can name a perfectly valid function that is the wrong one. check_addrs.py only asks that the
literal is a function entry; the verify judges follow imports and virtual slots, not direct-call targets. Found with it in lane msn2: the
winsock thunks of NMsnProtocol.h / MSNSocket.cpp / MsnSend.cpp (WSAGetLastError, recv, send, socket, connect, htons ...) all name a neighbour of
the right thunk, GiveListsToTree stored the allow list and the groups where the stock stores the forward and block lists
(MsnListFriends / MsnListBlocked of MsnWnd.h), SendBlock asked the allow list for the forward list's contact.

How it works (wave C audit lane; the first version matched helpers by bare name and flagged 527 of 5,394 units, 98 % of them name collisions):
  * every source file (include/nwindow/*.h, include/ue2, src/nwindow/*.cpp, src/re/NwHook.h) is parsed into definitions (functions, methods,
    NW_STOCK stubs, `#define` macros) with owner class, parameter counts, the `0x10xxxxxx` literals of the body (code, not comments) and the
    calls it makes by name;
  * a hook unit (an `NW_HOOK_*` line and the definition right above it) has its own literals and, for each call by name, the definitions the call
    can resolve to: same arity; owner = the unit's class or one of its ancestors (analysis/classes.json) for a bare call, the named class for
    `X::f()`, any owner for `obj->f()` (the receiver's type is unknown: the call is accepted when ONE candidate is clean). The functions that
    are hook units themselves are not helpers (our B is called directly: B's literals are B's business). The closure is followed 6 deep;
  * the stock function = its Ghidra ranges + the EH funclets / fragments that belong to it, a tail `jmp` of a forwarder followed; its literals
    are the call/jmp targets, immediates and absolute operands >= 0x10000000 of its instructions;
  * a literal of the unit that is not there is a flag, printed with the chain of helpers that brought it and the addresses the stock does use.
    `ORDER` lines: the direct calls the stock makes in another order than the literals appear in the source (a hint, not an error).

db/literals_ok.txt: one line per reviewed flag, `file|va name|literals|reason`; a flag that is not in the list fails (exit 1, and checks.py).
A changed literal set is a new line, so a literal edited after the review is looked at again. The reasons are one word (see the head of the file).

usage: python tools/port/check_literals.py [--lane NAME | --all] [--quiet] [--list] [--update --reason WORD] [file.cpp ...]
       --update   rewrite db/literals_ok.txt from the flags of the tree (existing reasons kept; a new line needs --reason: READ the stock first)
       --list     print every flagged unit, the reviewed ones too, with their reason"""
from __future__ import annotations

import argparse
import bisect
import csv
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from nwd.image import Image  # noqa: E402

LIST = ROOT / "db" / "literals_ok.txt"
HOOK = re.compile(r"^(?:NW_HOOK|MSN2_AFTER_MSN1)_(\w+)\(\s*(0x[0-9A-Fa-f]{8})\s*,\s*([\w:]+)?(?:\s*,\s*(\w+))?")
LIT = re.compile(r"\b0[xX](10[0-2][0-9A-Fa-f]{5})\b")
STOCK_MACRO = re.compile(r"\bNW_STOCK(?:_AT)?\s*\(\s*0[xX](10[0-2][0-9A-Fa-f]{5})\s*\)")
IDENT_CALL = re.compile(r"(~?[A-Za-z_]\w*)\s*(?:<[^<>;(){}]*>)?\s*\(")
IDENT = re.compile(r"[A-Za-z_]\w*")
SKIP_NAMES = {"if", "for", "while", "switch", "catch", "return", "sizeof", "do", "else", "guard", "unguard", "__declspec", "alignas",
              "decltype", "static_assert", "NW_ASSERT_OFFSET", "NW_ASSERT_SIZE", "__stock", "template", "typedef", "operator",
              "__attribute__", "defined"}
MAX_DEPTH = 6
STR_OR_COMMENT = re.compile(r'"(?:\\.|[^"\\\n])*"|\'(?:\\.|[^\'\\\n])*\'|//[^\n]*|/\*.*?\*/', re.S)
MACRO = re.compile(r"^[ \t]*#[ \t]*define[ \t]+(\w+)(\([^)\n]*\))?[ \t]*((?:[^\n]*\\\n)*[^\n]*)", re.M)
DECL = re.compile(r"((?:~?[A-Za-z_]\w*\s*(?:<[^<>;(){}]*>)?\s*::\s*)*)(~?[A-Za-z_]\w*|operator\s*(?:\(\s*\)|[^\s(A-Za-z_]+|new|delete))\s*\(")
REG_MACRO = re.compile(r"^[ \t]*(?:NW_HOOK_\w+|MSN2_\w+|NW_CHECK_VTABLE\w*|NW_ASSERT_\w+|NW_IMPLEMENT_\w+|NW_DECLARE_\w+|IMPLEMENT_\w+|DECLARE_\w+)\s*\(", re.M)
MACRO_CALL = re.compile(r"\s*[A-Z][A-Z0-9_]{2,}\s*\(")
CLASS_HDR = re.compile(r"\b(?:struct|class|union)\s+(?:[A-Za-z_]\w*\s+)*?([A-Za-z_]\w*)\s*(?::[^{(]*)?$")


# ----------------------------------------------------------------------------------------------------------------------------------
#   source model
# ----------------------------------------------------------------------------------------------------------------------------------
class Def:
    __slots__ = ("name", "owner", "pmin", "pmax", "lits", "body", "calls", "file", "line", "end_line", "local", "unit", "kind", "objlike")

    def __init__(self, name, owner, pmin, pmax, lits, body, file, line, end_line, local=False, kind="fn", objlike=False):
        self.name, self.owner, self.pmin, self.pmax, self.lits, self.body = name, owner, pmin, pmax, lits, body
        self.file, self.line, self.end_line, self.local, self.kind, self.objlike = file, line, end_line, local, kind, objlike
        self.calls: list[tuple[str, str, int]] | None = None
        self.unit = False

    def where(self) -> str:
        return "%s:%d" % (self.file, self.line)


def clean(text: str) -> str:
    """comments and string contents blanked, the line structure kept"""
    def sub(m):
        s = m.group(0)
        if s[0] in "\"'":
            return s[0] + s[0]
        return "".join("\n" if c == "\n" else " " for c in s)
    return STR_OR_COMMENT.sub(sub, text)


def balanced(text: str, i: int) -> int:
    """index just after the parenthesis group that opens at text[i]"""
    depth = 0
    n = len(text)
    while i < n:
        c = text[i]
        if c == "(":
            depth += 1
        elif c == ")":
            depth -= 1
            if depth == 0:
                return i + 1
        i += 1
    return n


def split_args(inner: str) -> list[str]:
    out, depth, cur = [], 0, []
    for c in inner:
        if c in "([{":
            depth += 1
        elif c in ")]}":
            depth -= 1
        if c == "," and depth == 0:
            out.append("".join(cur))
            cur = []
        else:
            cur.append(c)
    if "".join(cur).strip() or out:
        out.append("".join(cur))
    return out


def param_range(inner: str) -> tuple[int, int]:
    s = inner.strip()
    if not s or s == "void":
        return 0, 0
    if "..." in s:
        return 0, 999
    parts = split_args(s)
    opt = sum(1 for p in parts if "=" in p)
    return len(parts) - opt, len(parts)


def parse_header(header: str):
    """(owner from the qualifier, name, pmin, pmax) of a function definition header, or None"""
    for m in DECL.finditer(header):
        name = re.sub(r"\s+", "", m.group(2))
        if name in SKIP_NAMES:
            continue
        qual = re.sub(r"\s+", "", m.group(1))
        owner = None
        if qual:
            parts = [re.sub(r"<.*", "", p) for p in qual.split("::") if p]
            owner = parts[-1] if parts else None
        end = balanced(header, m.end() - 1)
        pmin, pmax = param_range(header[m.end():end - 1])
        return owner, name, pmin, pmax, header[:m.start()]
    return None


COND = re.compile(r"^[ \t]*#[ \t]*(ifdef|ifndef|if|elif|else|endif)\b(.*)$")
# conditions with a known value in the game build (CMakeLists: NW_HOOK_MODE=1; NW_FEATURES and MSN2_HOOK_DUPLICATES are not defined)
KNOWN_COND = {"NW_HOOK_MODE": True, "!NW_HOOK_MODE": False, "NW_HOOK_MODE==1": True, "NW_HOOK_MODE==0": False, "NW_HOOK_MODE!=0": True,
              "0": False, "1": True, "NW_FEATURES": False, "!NW_FEATURES": True, "MSN2_HOOK_DUPLICATES": False}


def apply_conditionals(text: str) -> str:
    """blank the lines of the branches that the hook build (NW_HOOK_MODE=1, no NW_FEATURES) does not compile; an unknown condition keeps
    both branches"""
    out = []
    stack: list[list] = []     # [parent_active, cond (True/False/None), in_else]
    active = True
    for line in text.split("\n"):
        m = COND.match(line)
        if m:
            kw, arg = m.group(1), re.sub(r"\s+", "", m.group(2))
            if kw in ("if", "ifdef", "ifndef"):
                if kw == "if":
                    val = KNOWN_COND.get(arg)
                elif kw == "ifdef":
                    val = False if arg == "NW_FEATURES" else None
                else:
                    val = True if arg == "NW_FEATURES" else None
                stack.append([active, val, False])
                active = active and (val is not False)
            elif kw == "elif" and stack:
                stack[-1][1] = None
                active = stack[-1][0]
            elif kw == "else" and stack:
                top = stack[-1]
                top[2] = True
                active = top[0] and (top[1] is not True)
            elif kw == "endif" and stack:
                active = stack.pop()[0]
            out.append("")
            continue
        out.append(line if active else "")
    return "\n".join(out)


def scan_file(path: Path, rel: str, is_cpp: bool):
    """-> (defs, cleaned text, virtual declarations). Macros and function definitions with the stock literals of their bodies."""
    raw = path.read_text(encoding="utf-8", errors="replace")
    text = apply_conditionals(clean(raw))
    hooks = [(n, HOOK.match(line)) for n, line in enumerate(text.split("\n")) if HOOK.match(line)]
    text = STOCK_MACRO.sub(lambda m: "{ __stock(0x%s); }" % m.group(1), text)
    defs: list[Def] = []
    virt: set[tuple[str | None, str]] = set()
    fields: dict[tuple[str, str], str] = {}
    bases: dict[str, list[str]] = {}
    rets: dict[tuple[str | None, str], str] = {}

    def take_macro(m):
        name, params, body = m.group(1), m.group(2), m.group(3).replace("\\\n", " ")
        lits = {int(x, 16) for x in LIT.findall(body)}
        if lits or IDENT.search(body):
            pmin, pmax = param_range(params[1:-1]) if params else (0, 0)
            ln = text[:m.start()].count("\n") + 1
            defs.append(Def(name, None, pmin, pmax, lits, body, rel, ln, ln + m.group(0).count("\n"), local=is_cpp, kind="macro",
                            objlike=params is None))
        return "".join("\n" if c == "\n" else " " for c in m.group(0))
    text = MACRO.sub(take_macro, text)
    text = re.sub(r"^[ \t]*#[^\n]*", "", text, flags=re.M)
    # the registration macros of a file (hook lines, vtable and layout checks) are statements without a `;`: blank them, or the next
    # definition's header would swallow them (and their addresses)
    out_chars = list(text)
    for m in REG_MACRO.finditer(text):
        end = balanced(text, m.end() - 1)
        for k in range(m.start(), end):
            if out_chars[k] != "\n":
                out_chars[k] = " "
    text = "".join(out_chars)

    # brace pairing
    stack: list[int] = []
    match: dict[int, int] = {}
    for m in re.finditer(r"[{}]", text):
        if m.group(0) == "{":
            stack.append(m.start())
        elif stack:
            match[stack.pop()] = m.start()
    line_of = None
    # line numbers: cumulative newline offsets
    nl = [i for i, c in enumerate(text) if c == "\n"] if False else [m.start() for m in re.finditer(r"\n", text)]

    def ln(i: int) -> int:
        return bisect.bisect_right(nl, i - 1) + 1

    scopes: list[tuple[str | None, int]] = []
    seg = 0
    pos = 0
    tok = re.compile(r"[{};}]")
    while True:
        m = tok.search(text, pos)
        if not m:
            break
        i = m.start()
        c = m.group(0)
        if c == ";":
            stmt = text[seg:i]
            if "(" in stmt:
                ph = parse_header(stmt)
                if ph:
                    howner = ph[0] or next((s for s, _ in reversed(scopes) if s), None)
                    if re.search(r"\bvirtual\b", stmt):
                        virt.add((howner, ph[1]))
                    rt = ret_of(ph[4])
                    if rt:
                        rets.setdefault((howner, ph[1]), rt)
            elif scopes and scopes[-1][0] and "(" not in stmt:
                for fname, ftype in parse_fields(stmt):
                    fields[(scopes[-1][0], fname)] = ftype
            seg = pos = i + 1
            continue
        if c == "}":
            while scopes and scopes[-1][1] <= i:
                scopes.pop()
            seg = pos = i + 1
            continue
        close = match.get(i, len(text) - 1)
        header = text[seg:i]
        # macro invocations in front of the definition (statements without a `;`: NCCONSOLE_GM_TOGGLE(...)) are not part of its header
        while True:
            mm = MACRO_CALL.match(header)
            if not mm:
                break
            cut = balanced(header, mm.end() - 1)
            seg += cut
            header = header[cut:]
        cls =CLASS_HDR.search(header.strip()) if "(" not in header and not re.search(r"\benum\b", header) else None
        if cls:
            scopes.append((cls.group(1), close))
            hs = header.strip()
            tail = hs[hs.rfind(cls.group(1)) + len(cls.group(1)):]
            if ":" in tail:
                bases[cls.group(1)] = [b for b in re.findall(r"[A-Za-z_]\w*", re.sub(r"<[^<>]*>", "", tail))
                                       if b not in ("public", "private", "protected", "virtual")]
            seg = pos = i + 1
            continue
        if "(" not in header and re.search(r"\bnamespace\b|\bextern\b", header):
            scopes.append((None, close))
            seg = pos = i + 1
            continue
        ph = parse_header(header) if "(" in header else None
        if ph:
            owner, name, pmin, pmax, prefix = ph
            if owner is None:
                owner = next((s for s, _ in reversed(scopes) if s), None)
            rt = ret_of(prefix)
            if rt:
                rets.setdefault((owner, name), rt)
            if re.search(r"\bvirtual\b", header):
                virt.add((owner, name))
            body = text[i + 1:close]
            lits = {int(x, 16) for x in LIT.findall(body)} | {int(x, 16) for x in LIT.findall(header)}
            defs.append(Def(name, owner, pmin, pmax, lits, header + " { " + body, rel, ln(seg + len(header) - len(header.lstrip())), ln(close),
                            local=is_cpp and bool(re.search(r"\bstatic\b", header))))
        seg = pos = close + 1
    return defs, text, virt, fields, bases, hooks, rets


TYPE_QUAL = {"static", "const", "volatile", "mutable", "struct", "class", "enum", "unsigned", "signed", "register", "extern", "inline",
             "typedef", "public", "private", "protected", "virtual", "union", "__declspec"}


NOT_A_TYPE = TYPE_QUAL | {"FORCEINLINE", "NW_NAKED", "noinline", "__forceinline", "__cdecl", "__stdcall", "__thiscall", "__fastcall", "operator"}


def ret_of(prefix: str) -> str | None:
    """the return type named in front of a function's name (the last type word), None for a constructor"""
    toks = [t for t in re.findall(r"[A-Za-z_]\w*", re.sub(r"<[^<>]*>", "", prefix)) if t not in NOT_A_TYPE]
    return toks[-1] if toks else None


def parse_fields(stmt: str) -> list[tuple[str, str]]:
    """a member declaration `Type* A, B[4];` -> [(A, Type), (B, Type)]"""
    s = re.sub(r"\b(?:public|private|protected)\s*:", " ", stmt)
    s = re.sub(r"<[^<>]*>", "", s)
    s = re.sub(r"\[[^\]]*\]", "", s)
    parts = s.split(",")
    m = re.match(r"\s*((?:[A-Za-z_]\w*(?:::)?\s*)+?)[*&\s]*([A-Za-z_]\w*)\s*(?:=[^,]*)?$", parts[0])
    if not m:
        return []
    toks = [t for t in re.findall(r"[A-Za-z_]\w*", m.group(1)) if t not in TYPE_QUAL]
    if not toks:
        return []
    ftype = toks[-1]
    out = [(m.group(2), ftype)]
    for p in parts[1:]:
        mm = re.match(r"\s*[*&\s]*([A-Za-z_]\w*)\s*(?:=[^,]*)?$", p)
        if mm:
            out.append((mm.group(1), ftype))
    return out


LOCAL_PTR = re.compile(r"(?<![\w.>:])((?:const\s+)?[A-Z]\w*)\s*[*&]\s*(\w+)\s*(?==|;|,|\)|\[)")
LOCAL_VAL = re.compile(r"(?<![\w.>:])([A-Z]\w*)\s+(\w+)\s*(?==|;|\(|,|\))")
STATIC_CAST = re.compile(r"(?:static|reinterpret|const|dynamic)_cast\s*<\s*(?:const\s+)?([A-Za-z_]\w*)\s*[*&]*\s*>\s*$")


def local_types(body: str) -> dict[str, str]:
    out: dict[str, str] = {}
    for rx in (LOCAL_VAL, LOCAL_PTR):
        for m in rx.finditer(body):
            t = m.group(1).replace("const", "").strip()
            if t and t not in TYPE_QUAL:
                out[m.group(2)] = t
    return out


# ----------------------------------------------------------------------------------------------------------------------------------
#   the model of the tree: every definition, the hook units, the resolution of the calls
# ----------------------------------------------------------------------------------------------------------------------------------
class Tree:
    def __init__(self, cpp_files: list[Path] | None = None):
        self.by_name: dict[str, list[Def]] = {}
        self.units: list[dict] = []
        self.ancestors: dict[str, set[str]] = {}
        for c in json.load(open(ROOT / "analysis" / "classes.json", encoding="utf-8")):
            self.ancestors[c["cpp"]] = set(c.get("ancestors") or [])
        files: list[tuple[Path, str, bool]] = []
        for p in sorted((ROOT / "include").rglob("*.h")) + sorted((ROOT / "include").rglob("*.inc")):
            files.append((p, p.relative_to(ROOT).as_posix(), False))
        files.append((ROOT / "src" / "re" / "NwHook.h", "src/re/NwHook.h", False))
        for p in sorted((ROOT / "src" / "nwindow").glob("*.cpp")):
            files.append((p, p.relative_to(ROOT).as_posix(), True))
        self.cpp_text: dict[str, str] = {}
        alldefs: list[Def] = []
        parsed = []
        self.virt: set[tuple[str | None, str]] = set()
        self.fields: dict[tuple[str, str], str] = {}
        self.rets: dict[tuple[str | None, str], str] = {}
        self.bases: dict[str, list[str]] = {}
        self.anc_list: dict[str, list[str]] = {}
        for c in json.load(open(ROOT / "analysis" / "classes.json", encoding="utf-8")):
            self.anc_list[c["cpp"]] = list(c.get("ancestors") or [])
        for p, rel, is_cpp in files:
            defs, text, virt, fields, bases, hooks, rets = scan_file(p, rel, is_cpp)
            for k, v in rets.items():
                self.rets.setdefault(k, v)
            alldefs.extend(defs)
            self.virt |= virt
            self.fields.update(fields)
            for k, v in bases.items():
                self.bases.setdefault(k, v)
            parsed.append((p, rel, is_cpp, defs, text, hooks))
        self.virt_any = {n for o, n in self.virt if o is None}
        self.alldefs = alldefs
        for p, rel, is_cpp, defs, text, hooks in parsed:
            if is_cpp:
                self.cpp_text[rel] = text
                self.find_units(rel, defs, text, hooks)
        for d in alldefs:
            if not d.unit:
                self.by_name.setdefault(d.name, []).append(d)
        self.chains: dict[str, list[str]] = {}
        self.fields_by_name: dict[str, set[str]] = {}
        for (_c, fname), ftype in self.fields.items():
            self.fields_by_name.setdefault(fname, set()).add(ftype)
        self.by_name_all: dict[str, list[Def]] = {}
        for d in alldefs:
            self.by_name_all.setdefault(d.name, []).append(d)
        self.ancestors = {c: set(self.chain(c)[1:]) for c in set(self.anc_list) | set(self.bases)}
        self.close_names()

    # hook lines -> the definition right above
    def find_units(self, rel: str, defs: list[Def], text: str, hooks: list):
        by_end: dict[int, Def] = {}
        for d in defs:
            if d.kind == "fn":
                by_end[d.end_line] = d
        clines = text.split("\n")
        for i, m in hooks:
            macro_kind, va, cls, meth = m.group(1), int(m.group(2), 16), m.group(3), m.group(4)
            # the name the definition must have (a hook of a compiler-generated destructor has no definition of its own: the one right
            # above belongs to another function and must not be taken for it)
            if macro_kind in ("CTOR", "CTOR_ARGS"):
                want = cls
            elif macro_kind == "DTOR":
                want = "~" + (cls or "")
            elif macro_kind == "FUNC":
                want = cls.split("::")[-1] if cls else None
                cls = cls.split("::")[0] if cls and "::" in cls else None
            else:
                want = meth
            # 1. the definition that ends right above the hook line (typedef lines and blanks in between)
            j = i - 1
            while j >= 0 and (not clines[j].strip() or clines[j].lstrip().startswith("typedef")):
                j -= 1
            d = by_end.get(j + 1)
            if d is not None and d.name != want:
                d = None
            if d is None and j >= 0 and by_end.get(j + 1) is None and clines[j].strip() \
                    and not clines[j].rstrip().endswith((";", "{", "}")) and want is not None:
                # the definition is a macro invocation (NCCONSOLE_GM_TOGGLE(...)): its text stands for the body
                d = Def("<macro>", None, 0, 0, {int(x, 16) for x in LIT.findall(clines[j])}, clines[j], rel, j + 1, j + 1)
                d.unit = True
            if d is None and want is not None:
                # 2. the nearest definition above by name and owner (a method defined inside a local struct, a stacked hook)
                near = [e for e in defs if e.kind == "fn" and e.name == want and e.end_line <= i + 1 and
                        (cls is None or e.owner == cls or e.owner in self.ancestors.get(cls, ()))]
                d = max(near, key=lambda e: e.end_line) if near else None
            if d is None and cls and want is not None:
                # 3. the body is inline in a header (an explicit instantiation of a template, a struct's own constructor)
                near = [e for e in self.alldefs if e.kind == "fn" and e.name == want and e.owner == cls and not e.file.endswith(".cpp")]
                d = near[0] if len(near) == 1 else None
            if d is not None and not d.unit:
                d.unit = True
            self.units.append({"file": rel, "va": va, "kind": macro_kind, "cls": cls, "def": d, "line": i + 1, "want": want})

    def close_names(self):
        """names (and macros) worth following: they carry a literal or call a name that does"""
        alldefs = [d for v in self.by_name.values() for d in v]
        interesting = {d.name for d in alldefs if d.lits}
        idents = {id(d): set(IDENT.findall(d.body)) for d in alldefs}
        changed = True
        while changed:
            changed = False
            for d in alldefs:
                if d.name not in interesting and idents[id(d)] & interesting:
                    interesting.add(d.name)
                    changed = True
        self.interesting = interesting
        self.objlike = {d.name: d for d in alldefs if d.objlike and d.name in interesting}
        self.objlike_re = re.compile(r"\b(" + "|".join(sorted(self.objlike, key=len, reverse=True)) + r")\b") if self.objlike else None

    def receiver_type(self, body: str, k: int, d: Def, loc: dict[str, str]) -> str | None:
        """the static type of the expression that ends at body[:k] (just before `->` / `.`), when it can be read off the text"""
        j = k
        while j > 0 and body[j - 1] in " \t\n":
            j -= 1
        if j > 0 and body[j - 1] == ")":
            depth, q = 0, j - 1
            while q >= 0:
                if body[q] == ")":
                    depth += 1
                elif body[q] == "(":
                    depth -= 1
                    if depth == 0:
                        break
                q -= 1
            m = STATIC_CAST.search(body[:q].rstrip())
            if m:
                return m.group(1)
            m2 = re.match(r"\s*\(\s*(?:const\s+)?([A-Za-z_]\w*)\s*[*&]\s*\)", body[q + 1:j - 1])
            if m2:
                return m2.group(1)
            # the result of a call `A->Name(...)` / `Name(...)`: the declared return type of Name in the class of A (or the free function)
            pre2 = body[:q].rstrip()
            m3 = re.search(r"([A-Za-z_]\w*)$", pre2)
            if not m3:
                return None
            before = pre2[:m3.start()].rstrip()
            if before.endswith("->") or before.endswith("."):
                classes = self.chain(self.receiver_type(body, len(before) - (2 if before.endswith("->") else 1), d, loc))
            elif before.endswith("::"):
                mq = re.search(r"([A-Za-z_]\w*)$", before[:-2].rstrip())
                classes = self.chain(mq.group(1)) if mq else []
            else:
                classes = self.chain(d.owner) + [None]
            for c in classes:
                t = self.rets.get((c, m3.group(1)))
                if t:
                    return t
            return None
        if j > 0 and body[j - 1] == "]":
            depth, q = 0, j - 1
            while q >= 0:
                if body[q] == "]":
                    depth += 1
                elif body[q] == "[":
                    depth -= 1
                    if depth == 0:
                        break
                q -= 1
            j = q
        m = re.search(r"([A-Za-z_]\w*)$", body[:j])
        if not m:
            return None
        r = m.group(1)
        pre = body[:m.start()].rstrip()
        if pre.endswith("->") or pre.endswith("."):
            # a member of another object (`((NCRecipeShopWnd*)this)->ShopFrame->`): the type of the base says which class' field it is
            base = self.receiver_type(body, len(pre) - (2 if pre.endswith("->") else 1), d, loc)
            for c in self.chain(base):
                t = self.fields.get((c, r))
                if t:
                    return t
        else:
            if r == "this":
                return d.owner
            if r in loc:
                return loc[r]
            if r in self.objlike:
                mm = re.search(r"NW_GLOBAL\s*\(\s*([A-Za-z_]\w*)", self.objlike[r].body)
                return mm.group(1) if mm else None
            for c in self.chain(d.owner):
                t = self.fields.get((c, r))
                if t:
                    return t
        # a field of an object of unknown type (`GetDesktop()->TrainWnd->`): when every declaration of that name has one type, that is it
        ts = self.fields_by_name.get(r)
        if ts and len(ts) == 1:
            return next(iter(ts))
        return None

    def calls_of(self, d: Def) -> list[tuple[str, str, int, str | None]]:
        if d.calls is not None:
            return d.calls
        out = []
        body = d.body
        loc = local_types(body)
        for m in IDENT_CALL.finditer(body):
            name = m.group(1)
            if name not in self.interesting or name in SKIP_NAMES:
                continue
            i = m.start()
            k = i
            while k > 0 and body[k - 1] in " \t\n":
                k -= 1
            kind = "bare"
            rtype = None
            if k >= 2 and body[k - 2:k] == "::":
                q = k - 2
                while q > 0 and body[q - 1] in " \t\n":
                    q -= 1
                e = q
                while q > 0 and (body[q - 1].isalnum() or body[q - 1] == "_"):
                    q -= 1
                kind = "scope:" + body[q:e]
            elif k >= 2 and body[k - 2:k] == "->":
                kind = "member"
                rtype = self.receiver_type(body, k - 2, d, loc)
            elif k >= 1 and body[k - 1] == ".":
                kind = "member"
                rtype = self.receiver_type(body, k - 1, d, loc)
            end = balanced(body, m.end() - 1)
            out.append((kind, name, len(split_args(body[m.end():end - 1])), rtype))
        if self.objlike_re:
            for m in self.objlike_re.finditer(body):
                if m.group(1) != d.name:
                    out.append(("obj", m.group(1), -1, None))
        d.calls = out
        return out

    def chain(self, c: str | None) -> list[str]:
        """the class and its ancestors, nearest first (classes.json, else the base lists of the headers)"""
        if not c:
            return []
        if c in self.chains:
            return self.chains[c]
        if c in self.anc_list:
            out = [c] + self.anc_list[c]
        else:
            out = [c]
            for b in self.bases.get(c, []):
                for x in self.chain(b):
                    if x not in out:
                        out.append(x)
        self.chains[c] = out
        return out

    def is_virtual(self, owner: str, name: str) -> bool:
        return name in self.virt_any or any((c, name) in self.virt for c in self.chain(owner))

    def candidates(self, call: tuple[str, str, int, str | None], ctx_cls: str | None, file: str, static_ctor: bool = False) -> list[Def]:
        """the non-hooked definitions the call can run (a call that binds to a hooked function, or to a virtual dispatched through the
        vtable, runs nothing that this check looks at). C++ lookup: the nearest class of the chain that has the name wins."""
        kind, name, nargs, rtype = call
        pool = [d for d in self.by_name_all.get(name, ())
                if not (d.local and d.file != file) and d.objlike == (kind == "obj") and (kind == "obj" or d.pmin <= nargs <= d.pmax)]
        if kind == "obj":
            return [d for d in pool if not d.unit]
        if kind == "bare":
            start = ctx_cls
        elif kind.startswith("scope:"):
            start = kind[6:] or None
        else:
            start = rtype
        if start is None and kind == "member":
            # the receiver's type is not readable: every class that has the name (the caller keeps the best one)
            return [d for d in pool if not d.unit and not (d.owner and self.is_virtual(d.owner, d.name))]
        for c in self.chain(start):
            here = [d for d in pool if d.owner == c]
            if here:
                if any(d.unit for d in here):
                    return []
                if kind in ("bare", "member") and self.is_virtual(c, name) and not (static_ctor and kind == "bare" and c == ctx_cls):
                    return []
                return here
        if kind == "member":
            return []
        free = [d for d in pool if d.owner is None]
        return [] if any(d.unit for d in free) else free

    def reached(self) -> set[int]:
        """ids of the definitions a hook unit's closure runs (the ones whose literals this check looks at)"""
        seen: set[int] = set()

        def walk(d: Def, depth: int, ctx: str | None):
            if id(d) in seen and depth > 0:
                return
            seen.add(id(d))
            if depth >= MAX_DEPTH:
                return
            owner = d.owner or ctx
            ctor = bool(owner) and d.name in (owner, "~" + owner)
            for call in self.calls_of(d):
                for c in self.candidates(call, owner, d.file, ctor):
                    walk(c, depth + 1, owner)
        for u in self.units:
            if u["def"] is not None:
                walk(u["def"], 0, u["cls"])
        return seen

    def miss(self, d: Def, seen: set[int], depth: int, stack: tuple, ctx_cls: str | None, memo: dict) -> dict[int, str]:
        """literals of d (and of the helpers it calls) that are not in `seen`: {literal: chain}"""
        key = id(d)
        if key in memo:
            return memo[key]
        res = {v: d.where() for v in d.lits if v not in seen}
        memo[key] = res           # cycles end here
        if depth < MAX_DEPTH:
            owner = d.owner or ctx_cls
            ctor = bool(owner) and d.name in (owner, "~" + owner)
            for call in self.calls_of(d):
                cands = self.candidates(call, owner, d.file, ctor)
                best = None
                for c in cands:
                    if id(c) in stack:
                        continue
                    r = self.miss(c, seen, depth + 1, stack + (id(d),), owner, memo)
                    if best is None or len(r) < len(best):
                        best = r
                    if not r:
                        break
                if best:
                    for v, w in best.items():
                        res.setdefault(v, "%s -> %s" % (call[1], w))
        return res


# ----------------------------------------------------------------------------------------------------------------------------------
#   the stock side
# ----------------------------------------------------------------------------------------------------------------------------------
class Stock:
    def __init__(self):
        self.img = Image()
        self.ranges: dict[int, list[tuple[int, int]]] = {}
        self.nocallers: dict[int, bool] = {}      # anonymous and never called: a piece of the function in front of it
        for r in csv.DictReader((ROOT / "analysis" / "ghidra" / "functions.tsv").open(encoding="utf-8"), delimiter="\t"):
            self.ranges[int(r["va"], 16)] = [tuple(int(x, 16) for x in rg.split("-")) for rg in r["ranges"].split(",")]
            self.nocallers[int(r["va"], 16)] = r["callers"] == "0" and r["name"].startswith(("FUN_", "sub_"))
        for r in csv.DictReader(l for l in (ROOT / "db" / "not_functions.csv").open(encoding="utf-8") if not l.startswith("#")):
            owner, frag = int(r["part_of"], 16), int(r["va"], 16)
            if owner in self.ranges and frag in self.ranges:
                self.ranges[owner] = self.ranges[owner] + self.ranges[frag]
        for r in csv.DictReader(l for l in (ROOT / "db" / "extra_functions.csv").open(encoding="utf-8") if not l.startswith("#")):
            va = int(r["va"], 16)
            self.ranges[va] = [(va, va + int(r["size"]) - 1)]
        self.rows: dict[int, dict] = {}
        for r in csv.DictReader((ROOT / "db" / "functions.csv").open(encoding="utf-8", errors="replace")):
            va = int(r["va"], 16)
            self.rows[va] = r
            m = re.match(r"parent ([0-9a-f]{8})", r.get("notes") or "")
            if m and r["kind"] in ("eh", "fragment"):
                parent = int(m.group(1), 16)
                self.ranges.setdefault(parent, [])
                if (va, va + int(r["size"]) - 1) not in self.ranges[parent]:
                    self.ranges[parent] = self.ranges[parent] + [(va, va + int(r["size"]) - 1)]
        self.cache: dict[int, tuple[set[int], list[int]]] = {}

    def decode(self, a: int, b: int):
        md = self.img.md
        data = self.img.read(a, b - a + 16)
        va = a
        while va < b:
            got = False
            for ins in md.disasm(data[va - a:], va):
                if ins.address >= b:
                    return
                got = True
                yield ins
                va = ins.address + ins.size
            if not got or va < b:
                va += 1       # bad byte (a table in the code): step over it

    def seen(self, va: int) -> tuple[set[int], list[int]]:
        if va in self.cache:
            return self.cache[va]
        seen: set[int] = set()
        order: list[int] = []
        rng = self.ranges.get(va)
        size = None
        if not rng:
            r = self.rows.get(va)
            size = int(r["size"]) if r else 64
            rng = [(va, va + size - 1)]
        total = sum(b - a + 1 for a, b in rng)
        tail = None
        last = None
        for k, (a, b) in enumerate(rng):
            for ins in self.decode(a, b + 1):
                if k == 0:
                    last = ins
                if ins.op_str and "0x" in ins.op_str:
                    for m in re.findall(r"0x[0-9a-f]+", ins.op_str):
                        v = int(m, 16)
                        if v >= 0x10000000:
                            seen.add(v)
                if ins.mnemonic in ("call", "jmp"):
                    m = re.match(r"0x([0-9a-f]+)$", ins.op_str)
                    if m:
                        t = int(m.group(1), 16)
                        if t not in order:
                            order.append(t)
                        if ins.mnemonic == "jmp" and not any(x <= t <= y for x, y in rng):
                            tail = t
        # the analysis cuts a function in two at a loop head after an alignment (AddQuestID 0x10067390 / 0x100673C3): when the last
        # instruction of the function falls through into an anonymous function nobody calls, that one is the rest of it
        if last is not None and last.mnemonic not in ("ret", "retn", "jmp", "int3", "ud2", "hlt") and not (
                last.mnemonic == "call" and last.op_str == "0x10171169"):
            nxt = rng[0][1] + 1
            if self.nocallers.get(nxt):
                s2, o2 = self.seen(nxt)
                seen |= s2
                for t in o2:
                    if t not in order:
                        order.append(t)
        if tail is not None and total <= 40 and tail in self.rows and self.rows[tail]["kind"] != "eh":
            s2, o2 = self.seen(tail)           # a forwarder stands for its target
            seen |= s2
            for t in o2:
                if t not in order:
                    order.append(t)
        self.cache[va] = (seen, order)
        return seen, order


# ----------------------------------------------------------------------------------------------------------------------------------
def load_list() -> dict[str, str]:
    out: dict[str, str] = {}
    if LIST.exists():
        for line in LIST.read_text(encoding="utf-8").splitlines():
            if not line.strip() or line.startswith("#"):
                continue
            key, _, reason = line.rpartition("|")
            out[key] = reason
    return out


def flag_key(rel: str, va: int, name: str, lits: list[int]) -> str:
    return "%s|%08x %s|%s" % (rel, va, name, " ".join("%08x" % x for x in sorted(lits)))


def analyse(tree: Tree, stock: Stock, only_files: set[str] | None):
    """-> flags [(rel, va, name, {lit: chain}, seen, order, unit)], nunits, order hints
    An order hint is a unit whose own `0x10xxxxxx` calls come in another order than the stock makes them: a hint only (a body may reorder
    independent calls), but a stock call made before another one that reads what it writes is a bug the judges do not see."""
    flags = []
    hints = []
    n = 0
    for u in tree.units:
        if only_files is not None and u["file"] not in only_files:
            continue
        n += 1
        va = u["va"]
        if va not in stock.rows:
            continue
        d: Def | None = u["def"]
        if d is None:
            continue
        seen, order = stock.seen(va)
        seen = seen | {va}
        ctx = d.owner or u["cls"]
        res = tree.miss(d, seen, 0, (), ctx, {})
        res.pop(va, None)
        if res:
            flags.append((u["file"], va, stock.rows[va]["name"], res, seen, order, u))
        src_order = list(dict.fromkeys(int(x, 16) for x in LIT.findall(d.body) if int(x, 16) != va))
        common = [v for v in src_order if v in order]
        stock_common = [v for v in order if v in common]
        if common != stock_common:
            hints.append((u["file"], va, stock.rows[va]["name"], common, stock_common))
    return flags, n, hints


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--lane")
    ap.add_argument("--all", action="store_true")
    ap.add_argument("--quiet", action="store_true")
    ap.add_argument("--list", action="store_true")
    ap.add_argument("--update", action="store_true")
    ap.add_argument("--order", action="store_true", help="also print the units whose calls come in another order than the stock's")
    ap.add_argument("--reason")
    ap.add_argument("files", nargs="*")
    a = ap.parse_args()
    only: set[str] | None = None
    if a.files:
        only = {("src/nwindow/" + Path(f).name) for f in a.files}
    elif a.lane:
        lanes = json.load(open(ROOT / "db" / "lanes_s4.json", encoding="utf-8"))
        only = {f for f in lanes[a.lane]["files"] if f.startswith("src/")}
    tree = Tree()
    stock = Stock()
    flags, nunits, hints = analyse(tree, stock, None if (a.all or a.update or a.list) else only)
    ok = load_list()
    new = []
    keys_now: dict[str, str] = {}
    for rel, va, name, res, seen, order, u in flags:
        key = flag_key(rel, va, name, list(res))
        keys_now[key] = ""
        reviewed = key in ok
        if not reviewed:
            new.append(key)
        if not reviewed and a.quiet and (only is None or rel in only):
            print("  not in db/literals_ok.txt (read the stock function, then --update --reason WORD): %s" % key)
        if a.list or (not reviewed and not a.quiet):
            print("%s %08x %s: literal(s) not used by the stock function: %s%s" % (
                rel.split("/")[-1], va, name, " ".join("%08x" % x for x in sorted(res)), ("  [%s]" % ok[key]) if reviewed else ""))
            for v in sorted(res)[:6]:
                print("     %08x via %s" % (v, res[v]))
            if not reviewed:
                print("     the stock calls: " + " ".join("%08x" % x for x in sorted(seen) if 0x10001000 <= x < 0x1019C810))
                for v in sorted(res):
                    if v not in stock.rows:
                        near = [x for x in sorted(seen) if v - 0x100 <= x <= v + 0x100 and x != v]
                        if near:
                            print("     data %08x: the stock uses addresses next to it: %s (a table or a structure read from its base?)" % (
                                v, " ".join("%08x" % x for x in near)))
    if a.update:
        if new and not a.reason:
            print("check_literals: %d new flag(s): read the stock functions, then --update --reason WORD" % len(new))
            return 1
        head = [l for l in LIST.read_text(encoding="utf-8").splitlines() if l.startswith("#")] if LIST.exists() else []
        lines = ["%s|%s" % (k, ok.get(k) or a.reason) for k in sorted(keys_now)]
        LIST.write_text("\n".join(head + lines) + "\n", encoding="utf-8")
        print("check_literals: %d line(s) written to db/literals_ok.txt (%d new, %d dropped)" % (
            len(lines), len(new), len([k for k in ok if k not in keys_now])))
        return 0
    if a.order:
        for rel, va, name, common, stock_common in hints:
            print("ORDER %s %08x %s: source %s | stock %s" % (rel.split("/")[-1], va, name,
                  " ".join("%08x" % x for x in common), " ".join("%08x" % x for x in stock_common)))
    sel = [k for k in new if only is None or k.split("|")[0] in only]
    stale = [k for k in ok if k not in keys_now and (only is None or k.split("|")[0] in only)]
    print("check_literals: %d hook unit(s), %d flagged, %d reviewed in db/literals_ok.txt, %d not reviewed%s" % (
        nunits, len(flags), len(flags) - len(new), len(sel), ("  (%d stale list line(s): --update drops them)" % len(stale)) if stale else ""))
    return 1 if sel else 0


if __name__ == "__main__":
    sys.exit(main())
