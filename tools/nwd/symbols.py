"""Symbols of a linked image: the linker map (link /MAP) of our DLLs, or the same view of the
stock NWindow.dll built from what the project knows about it (db/functions.csv,
analysis/classes.json, the exports). tools/verify_standalone.py and tools/difftest.py use it
to find our functions, vtables and UClass objects by name.

A symbol is (decorated, pretty): the map gives decorated names, undecorated here with
dbghelp's UnDecorateSymbolName (name only: "NCWnd::OnPaint", "NCButton::`vftable'"); the stock
view has pretty names from the database and decorated ones only where they are certain
(PrivateStaticClass, InternalConstructor, vtables, exports).
"""
from __future__ import annotations

import bisect
import csv
import ctypes
import json
import re
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

# " 0001:00018a50       ?OnPaint@NCWnd@@UAEHPAVNCGDevice@@@Z 10019a50 f i NCAbnormalStatusWnd.obj"
MAP_LINE = re.compile(r"^\s*([0-9a-f]{4}):([0-9a-f]{8})\s+(\S+)\s+([0-9a-f]{8})((?:\s+[fi](?=\s))*)\s+(.*)$")
MAP_SECTION = re.compile(r"^\s*([0-9a-f]{4}):([0-9a-f]{8})\s+([0-9a-f]{8})H\s+(\S+)\s+(CODE|DATA)\s*$")
MAP_BASE = re.compile(r"^\s*Preferred load address is ([0-9a-f]+)")
FUNCLET = re.compile(r"__(catch|unwindfunclet)\$")

UNDNAME_NAME_ONLY = 0x1000
_undname = None


def undecorate(name: str) -> str:
    """Name only, as the database writes it: ?OnPaint@NCWnd@@UAEHPAVNCGDevice@@@Z -> NCWnd::OnPaint.
    C names lose the leading underscore and the @N of stdcall (_DllMain@12 -> DllMain)."""
    global _undname
    if not name.startswith("?"):
        m = re.fullmatch(r"[_@]?(\w+?)(?:@\d+)?", name)
        return m.group(1) if m else name
    if _undname is None:
        dbghelp = ctypes.WinDLL("dbghelp")
        _undname = dbghelp.UnDecorateSymbolName
        _undname.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_uint32, ctypes.c_uint32]
        _undname.restype = ctypes.c_uint32
    buf = ctypes.create_string_buffer(2048)
    n = _undname(name.encode("latin-1"), buf, len(buf), UNDNAME_NAME_ONLY)
    return buf.value[:n].decode("latin-1") if n else name


class Symbols:
    def __init__(self, base: int):
        self.base = base
        self.by_addr: dict[int, list[tuple[str | None, str]]] = defaultdict(list)
        self.by_name: dict[str, int] = {}
        self.func_starts: list[int] = []
        self.sections: dict[str, tuple[int, int]] = {}  # map section contribution -> (va, size)
        self.entry: int | None = None
        self._sorted: list[int] | None = None

    def add(self, addr: int, decorated: str | None, pretty: str | None = None, func: bool = False):
        if pretty is None:
            pretty = undecorate(decorated) if decorated else "?"
        if (decorated, pretty) not in self.by_addr[addr]:
            self.by_addr[addr].append((decorated, pretty))
        if decorated:
            self.by_name.setdefault(decorated, addr)
        self.by_name.setdefault(pretty, addr)
        if func:
            self.func_starts.append(addr)
        self._sorted = None

    def finish(self):
        self.func_starts = sorted(set(self.func_starts))
        self._sorted = sorted(self.by_addr)
        self._pretty = None

    def find_pretty(self, pretty: str) -> set[int]:
        """Every address whose undecorated name is pretty (overloads give several)."""
        if getattr(self, "_pretty", None) is None:
            self._pretty = defaultdict(set)
            for a, lst in self.by_addr.items():
                for _, p in lst:
                    self._pretty[p].add(a)
        return self._pretty.get(pretty, set())

    # ---- queries ----------------------------------------------------------------------------
    def at(self, addr: int) -> list[tuple[str | None, str]]:
        return self.by_addr.get(addr, [])

    def pretty_at(self, addr: int) -> list[str]:
        return list(dict.fromkeys(p for _, p in self.at(addr)))

    def find(self, name: str) -> int | None:
        """Address of a decorated (or pretty) name."""
        return self.by_name.get(name)

    def with_prefix(self, prefix: str) -> list[tuple[str, int]]:
        return sorted((n, a) for n, a in self.by_name.items() if n.startswith(prefix))

    def func_containing(self, addr: int) -> int | None:
        i = bisect.bisect_right(self.func_starts, addr)
        return self.func_starts[i - 1] if i else None

    def func_end(self, start: int, limit: int) -> int:
        i = bisect.bisect_right(self.func_starts, start)
        return self.func_starts[i] if i < len(self.func_starts) else limit

    def next_symbol(self, addr: int) -> int | None:
        """The first symbol address after addr (the end of a vtable, a table...)."""
        if self._sorted is None:
            self._sorted = sorted(self.by_addr)
        i = bisect.bisect_right(self._sorted, addr)
        return self._sorted[i] if i < len(self._sorted) else None


def load_map(path: Path) -> Symbols:
    """Publics and static symbols of an MSVC linker map (link /MAP). The map lists every symbol
    the linker kept, including the functions /OPT:ICF folded onto one body (same address)."""
    text = path.read_text(encoding="latin-1").splitlines()
    base = 0x10000000
    for line in text[:20]:
        m = MAP_BASE.match(line)
        if m:
            base = int(m.group(1), 16)
    syms = Symbols(base)
    for line in text:
        m = MAP_SECTION.match(line)
        if m:
            sect, off, size, name = int(m.group(1), 16), int(m.group(2), 16), int(m.group(3), 16), m.group(4)
            syms.sections.setdefault(name, (sect, off, size))
            continue
        m = MAP_LINE.match(line)
        if m:
            if m.group(1) == "0000":
                continue  # absolute symbols and the debug sections
            va = int(m.group(4), 16)
            flags = m.group(5).split()
            # catch blocks and unwind funclets sit inside their function's code (the PDB counts
            # them in it): they do not end the function
            syms.add(va, m.group(3), func="f" in flags and not FUNCLET.match(m.group(3)))
            continue
        if line.strip().startswith("entry point at"):
            sect, off = line.split()[-1].split(":")
            syms.entry = (int(sect, 16), int(off, 16))
    syms.finish()
    return syms


def stock_symbols(img, classes: list[dict] | None = None) -> Symbols:
    """The stock image seen like a map: functions from db/functions.csv (pretty names), and the
    decorated names every IMPLEMENT_CLASS object has, from analysis/classes.json."""
    syms = Symbols(img.base)
    for r in csv.DictReader((ROOT / "db" / "functions.csv").open(encoding="utf-8")):
        if r["kind"] in ("game", "init", "crt", "thunk", "lib"):
            syms.add(int(r["va"], 16), None, r["name"], func=True)
    if classes is None:
        classes = json.loads((ROOT / "analysis" / "classes.json").read_text(encoding="utf-8"))
    for c in classes:
        cpp = c["cpp"]
        syms.add(c["static_class"], f"?PrivateStaticClass@{cpp}@@0VUClass@@A")
        syms.add(c["internal_ctor"], f"?InternalConstructor@{cpp}@@SAXPAX@Z", func=True)
        if c.get("secondary_vtables"):
            syms.add(c["vtable"], f"??_7{cpp}@@6BUObject@@@")
            for sv in c["secondary_vtables"]:
                syms.add(sv["vtable"], f"??_7{cpp}@@6BFExec@@@")
        elif c.get("vtable"):
            syms.add(c["vtable"], f"??_7{cpp}@@6B@")
    for name, va in img.exports.items():
        syms.add(va, name, func=img.in_text(va))
    # _purecall of the stock static CRT: 0x100BE4FC in the C1 image, 0x100E84EA in the C2 one,
    # 0x1014D4CA in the C3 one
    # (db/names.csv names it; it is the target of the pure virtual slots)
    for r in csv.DictReader((ROOT / "db" / "functions.csv").open(encoding="utf-8")):
        if r["name"] == "_purecall":
            syms.add(int(r["va"], 16), "__purecall", "_purecall", func=True)
            break
    for va in (int(x, 16) for x in (ROOT / "analysis" / "vtable_starts.txt").read_text().split()):
        if not syms.at(va):
            syms.add(va, None, f"vtable_{va:08x}")
    syms.finish()
    return syms


# ---- full signatures -------------------------------------------------------------------------

CALLING = (" __thiscall ", " __cdecl ", " __stdcall ", " __fastcall ")
_full_cache: dict[str, str | None] = {}


def full_signature(decorated: str) -> str | None:
    """A function's qualified name, parameters and const-ness, from its decorated name with
    dbghelp's complete undecoration, without access, virtual, static, calling convention and
    return type: ?What@NStdLogicError@@UBEPBDXZ -> "NStdLogicError::What()const". Two builds
    of the same declaration give the same text, whether or not it is virtual."""
    if decorated in _full_cache:
        return _full_cache[decorated]
    out = None
    if decorated.startswith("?"):
        undecorate("?x@@YAXXZ")  # loads dbghelp
        buf = ctypes.create_string_buffer(4096)
        n = _undname(decorated.encode("latin-1"), buf, len(buf), 0)
        text = buf.value[:n].decode("latin-1").rstrip() if n else ""
        hits = [text.find(c) for c in CALLING if c in text]
        if hits and text.endswith((")", ")const")):
            out = text[min(hits):].split(" ", 2)[2].replace("(void)", "()")
    _full_cache[decorated] = out
    return out


def split_signature(sig: str) -> tuple[str, list[str], bool]:
    """"A::B(int,char const *)const" -> ("A::B", ["int", "char const *"], True)."""
    const = sig.endswith("const")
    body = sig[:-5] if const else sig
    depth, start = 0, 0
    for k in range(len(body) - 1, -1, -1):
        c = body[k]
        if c == ")":
            depth += 1
        elif c == "(":
            depth -= 1
            if depth == 0:
                start = k
                break
    name, params = body[:start], body[start + 1:-1]
    out, depth, cur = [], 0, ""
    for c in params:
        if c in "<(":
            depth += 1
        elif c in ">)":
            depth -= 1
        if c == "," and depth == 0:
            out.append(cur)
            cur = ""
        else:
            cur += c
    if cur:
        out.append(cur)
    return name, out, const


def join_signature(name: str, params: list[str], const: bool) -> str:
    return f"{name}({','.join(params)})" + ("const" if const else "")


def signature_index(syms: Symbols) -> dict[str, set[int]]:
    """Full signature -> addresses, for every function symbol of a map."""
    out: dict[str, set[int]] = defaultdict(set)
    starts = set(syms.func_starts)
    for addr, lst in syms.by_addr.items():
        if addr not in starts:
            continue
        for dec, _ in lst:
            sig = full_signature(dec) if dec else None
            if sig:
                out[sig].add(addr)
    return out


# ---- what the hook-mode build knows: stock address -> our function --------------------------------

LOG_RE = re.compile(r"^\S+\s+([0-9a-f]{8}) (.+?)\s+ours=([0-9a-f]+) (.*)$")
FWD_RE = re.compile(r"^`anonymous namespace'::NwFwd_\d+::(Fwd|FwdC)<")
RUN_RE = re.compile(r"^`anonymous namespace'::Nw(Ctor|Dtor)_\d+::Run$")


def class_of_pointer(param: str) -> str:
    """"class TArray<class NCWnd *> *" -> "TArray<class NCWnd *>"."""
    p = param.strip()
    if p.endswith("*"):
        p = p[:-1].rstrip()
    for kw in ("class ", "struct ", "union "):
        if p.startswith(kw):
            p = p[len(kw):]
    return p


def hook_signature(syms: Symbols, mem, md, name: str, va: int) -> str | None:
    """The full signature of the function a hook installs, from the hook DLL's map: the
    forwarder NW_HOOK_METHOD registers (its parameters are the method's), the wrapper
    NW_HOOK_CTOR/DTOR registers (its first parameter is the class), or the function itself
    (NW_HOOK_FUNC, NW_HOOK_VARARGS). The class of a method is the hook's name, except when the
    forwarder calls the method itself: then the callee's own name (the hook may spell the class
    through a typedef, TTimerArray::Remove for TArray<NTimer>::Remove)."""
    pretty = name.split("(")[0]
    method = pretty.split("::")[-1]
    own = [(d, p) for d, p in syms.at(va) if d]
    for d, p in own:
        m = FWD_RE.match(p)
        if m:
            sig = full_signature(d)
            if not sig:
                return None
            _, params, _ = split_signature(sig)
            const = m.group(1) == "FwdC"
            # the forwarder's callee, when it is that method: its exact signature
            rva = va - syms.base
            for ins in md.disasm(mem[rva:rva + 96], va):
                if ins.mnemonic in ("call", "jmp") and ins.op_str.startswith("0x"):
                    for d2, _ in syms.at(int(ins.op_str, 16)):
                        s2 = full_signature(d2) if d2 else None
                        if s2:
                            n2, p2, c2 = split_signature(s2)
                            if n2.split("::")[-1] == method and p2 == params and c2 == const:
                                return s2
                    break
                if ins.mnemonic in ("ret", "retn"):
                    break
            return join_signature(pretty, params, const)
        m = RUN_RE.match(p)
        if m:
            sig = full_signature(d)
            if not sig:
                return None
            _, params, _ = split_signature(sig)
            cls = class_of_pointer(params[0])
            short = cls if "<" in cls else cls.split("::")[-1]
            if m.group(1) == "Dtor":
                return f"{cls}::~{short}()"
            rest = params[1:]
            if "(" in name and rest and rest[0] == "void *":  # NW_HOOK_CTOR_ARGS: Run(self, void*, ...)
                rest = rest[1:]
            return join_signature(f"{cls}::{short}", rest, False)
    # NW_HOOK_FUNC / NW_HOOK_VARARGS: the function itself
    sigs = {x for x in (full_signature(d) for d, p in own if p == pretty or p.split("::")[-1] == method) if x}
    if not sigs:
        sigs = {x for x in (full_signature(d) for d, _ in own) if x}
    return sigs.pop() if len(sigs) == 1 else None


def hook_functions(log: Path, hook_map: Path) -> tuple[dict[int, str], list[str]]:
    """Stock va -> full signature of the function of ours that replaces it, from a hook-mode
    run (NWindowDecomp.log: va, name, the registered code's rva) and the hook DLL's linker map
    (the DLL itself next to it). The same signature names that function in the standalone
    build (same source). Returns (table, the hooks whose function could not be named)."""
    from capstone import CS_ARCH_X86, CS_MODE_32, Cs
    import pefile
    syms = load_map(hook_map)
    mem = pefile.PE(str(hook_map.with_suffix(".dll")), fast_load=True).get_memory_mapped_image()
    md = Cs(CS_ARCH_X86, CS_MODE_32)
    out, unresolved = {}, []
    for line in log.read_text(encoding="latin-1").splitlines():
        m = LOG_RE.match(line)
        if not m:
            continue
        va, name, rva = int(m.group(1), 16), m.group(2).strip(), int(m.group(3), 16)
        sig = hook_signature(syms, mem, md, name, syms.base + rva)
        if sig:
            out[va] = sig
        else:
            unresolved.append(f"{va:08x} {name}")
    return out, unresolved
