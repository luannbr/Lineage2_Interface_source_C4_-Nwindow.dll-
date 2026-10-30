#!/usr/bin/env python3
"""Virtual-function declarations of engine classes, generated from Engine.dll's exported vtables.

Each target below becomes a fragment (include/ue2/gen/<Class>.vt.inc) meant to be #included
inside the hand-written class body; the signatures come from the decorated names of the slot
targets (undname), so parameter and return types are exact. Types that no header defines get a
forward declaration in include/ue2/gen/EngineFwd.gen.h (enums as opaque `enum E : INT;`).

MSVC puts overloaded virtuals in reverse declaration order, so each run of same-named new slots
is emitted last-slot first (a name in two separate runs would be grouped by MSVC: an error here).
Slots whose target is _purecall carry no name: they are emitted as
`virtual void PureSlotNNN() = 0;` unless a table names them after the implementing class
(tools/ue2_data/l2consolewnd_slots_c4.csv, from the C3 table by tools/ue2_console_slots_c4.py; the C3 one came from the C2 table by
tools/ue2_remap_console_slots.py and that one from tools/ue2_console_slots.py over the C1 names: UL2ConsoleWnd's, as NConsoleWnd
implements them, with the evidence of each row in its `source` column)
or the vtable of an exported implementation names them (C2: UL2NetHandler's slots are
UNetworkHandler's, exported by Engine.dll itself, the C1 client had a Network.dll; the audio,
render device and viewport ones are in ALAudio.dll, D3DDrv.dll and WinDrv.dll, copies of those go
in orig/).

C2: the first slot to emit is the slot count of the PARENT's vtable, read from the DLL (it was a
hand-kept number in the C1 build and moves by 1 or more in the C2 one: UObject got ClearL2Game).

  python tools/gen_engine_decls.py
"""
from __future__ import annotations

import re
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import engine_vtables as ev  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
GEN = ROOT / "include" / "ue2" / "gen"

# (class, dll, vtable symbol, the parent's (dll, vtable symbol) whose slot count is the first slot to
#  emit (None: slot 0), prefix of the exported slots, pure slot table, the implementation's (dll,
#  vtable) that names the pure slots)
E = "ENGINE_API "
OBJ = ("Core", "??_7UObject@@6B@")
SUBSYS = ("Core", "??_7USubsystem@@6BUObject@@@")
TABLE = "tools/ue2_data/l2consolewnd_slots_c4.csv"
TARGETS = [
    ("FL2NetNotify", "Engine", "??_7FL2NetNotify@@6B@", None, "", None, None),
    ("UL2ConsoleWnd", "Engine", "??_7UL2ConsoleWnd@@6BUObject@@@", SUBSYS, E, TABLE, None),
    ("UPrimitive", "Engine", "??_7UPrimitive@@6B@", OBJ, E, None, None),
    ("UMesh", "Engine", "??_7UMesh@@6B@", ("Engine", "??_7UPrimitive@@6B@"), E, None, None),
    ("AActor", "Engine", "??_7AActor@@6B@", OBJ, E, None, None),
    ("APawn", "Engine", "??_7APawn@@6B@", ("Engine", "??_7AActor@@6B@"), E, None, None),
    ("AController", "Engine", "??_7AController@@6B@", ("Engine", "??_7AActor@@6B@"), E, None, None),
    ("UPlayer", "Engine", "??_7UPlayer@@6BUObject@@@", OBJ, E, None,
     ("Engine", "??_7UViewport@@6BUObject@@@")),
    ("UViewport", "Engine", "??_7UViewport@@6BUObject@@@", ("Engine", "??_7UPlayer@@6BUObject@@@"), E, None,
     ("WinDrv", "??_7UWindowsViewport@@6BUObject@@@")),
    ("UAudioSubsystem", "Engine", "??_7UAudioSubsystem@@6BUObject@@@", SUBSYS, E, None,
     ("ALAudio", "??_7UALAudioSubsystem@@6BUObject@@@")),
    ("URenderDevice", "Engine", "??_7URenderDevice@@6BUObject@@@", SUBSYS, E, None,
     ("D3DDrv", "??_7UD3DRenderDevice@@6BUObject@@@")),
    ("UEngine", "Engine", "??_7UEngine@@6BUObject@@@", SUBSYS, E, None,
     ("Engine", "??_7UGameEngine@@6BUObject@@@")),
    ("UGameEngine", "Engine", "??_7UGameEngine@@6BUObject@@@", ("Engine", "??_7UEngine@@6BUObject@@@"), E, None, None),
    ("UL2NetHandler", "Engine", "??_7UL2NetHandler@@6BUObject@@@", SUBSYS, E, None,
     ("Engine", "??_7UNetworkHandler@@6BUObject@@@")),
]


def pure_names(path: str | None) -> tuple[dict[int, str], dict[int, str]]:
    """slot -> declaration (no `virtual`, no `= 0`) of the pure slots a table names, and slot ->
    comment (the evidence column of tools/ue2_console_slots.py's table, or what is known about an
    unnamed slot)."""
    if not path:
        return {}, {}
    import csv
    rows = csv.DictReader(l for l in (ROOT / path).open(encoding="utf-8") if not l.startswith("#"))
    decl, note = {}, {}
    for r in rows:
        slot = int(r["slot"])
        if r["declaration"]:
            decl[slot] = r["declaration"]
            if r.get("source") and r["source"] not in ("layout", "handler", "guard", "forward", "names.csv"):
                note[slot] = ("base name, " if r.get("c1_slot") else "") + r["source"]
        if r.get("note"):
            note[slot] = (note[slot] + "; " if slot in note else "") + r["note"]
    return decl, note


BASIC = {
    "void": "void", "bool": "bool", "int": "INT", "unsigned int": "UINT", "long": "LONG",
    "unsigned long": "DWORD", "short": "SWORD", "unsigned short": "_WORD", "char": "ANSICHAR",
    "signed char": "SBYTE", "unsigned char": "BYTE", "float": "FLOAT", "double": "DOUBLE",
    "__int64": "SQWORD", "unsigned __int64": "QWORD",
}

SIG_RE = re.compile(r"^(?:public|protected|private): virtual (?P<ret>.+?) (?P<cc>__thiscall|__cdecl|__stdcall) "
                    r"(?P<cls>[\w:<>]+)::(?P<name>~?\w+)\((?P<args>.*)\)(?P<const>const)?\s*$")


def undname_all(names: list[str]) -> list[str]:
    ev.undname([])  # resolve the tool path
    out = []
    for i in range(0, len(names), 40):
        chunk = names[i:i + 40]
        r = subprocess.run([ev.UNDNAME, *chunk], capture_output=True, text=True)
        got = [x[7:-1] for x in r.stdout.splitlines() if x.startswith("is :- ")]
        if len(got) != len(chunk):
            got = ev.undname(chunk)
        out.extend(got)
    return out


def split_args(s: str) -> list[str]:
    out, depth, cur = [], 0, ""
    for ch in s:
        if ch == "<":
            depth += 1
        elif ch == ">":
            depth -= 1
        if ch == "," and depth == 0:
            out.append(cur.strip())
            cur = ""
        else:
            cur += ch
    if cur.strip():
        out.append(cur.strip())
    return out


class TypeMapper:
    def __init__(self, known: set[str]):
        self.known = known
        self.fwd: dict[str, str] = {}   # name -> class/struct/enum

    def base(self, words: str, pointer: bool) -> str:
        words = words.strip()
        m = re.match(r"^(class|struct|enum|union) (.+)$", words)
        if m:
            tag, name = m.group(1), m.group(2)
            t = re.match(r"^(\w+)<(.*)>$", name)
            if t:
                inner = ", ".join(self.type(a) for a in split_args(t.group(2)))
                return f"{t.group(1)}<{inner}>"
            # classes and structs are forward-declared even when a header defines them (the
            # fragments are included before some definitions); an enum only when none does
            if tag != "enum" or name not in self.known:
                self.fwd.setdefault(name, tag)
            return name
        if words == "unsigned short" and pointer:
            return "TCHAR"
        if words in BASIC:
            return BASIC[words]
        raise ValueError(f"unknown type {words!r}")

    def type(self, s: str) -> str:
        toks = re.findall(r"<[^>]*(?:<[^>]*>[^>]*)*>|[\w]+|\*|&|const|volatile", s)
        # base = words up to the first qualifier
        words, i = [], 0
        while i < len(toks) and toks[i] not in ("*", "&", "const", "volatile"):
            if toks[i].startswith("<"):
                words[-1] += toks[i]
            else:
                words.append(toks[i])
            i += 1
        rest = toks[i:]
        base = self.base(" ".join(words), pointer="*" in rest)
        out = base
        first = True
        for t in rest:
            if t == "const":
                out = f"const {out}" if first else f"{out} const"
            elif t in ("*", "&"):
                out += t
                first = False
        return out


def known_types() -> set[str]:
    known = set()
    for h in (ROOT / "include").rglob("*.h"):
        if h.parent == GEN:
            continue
        txt = h.read_text(encoding="utf-8", errors="replace")
        known.update(re.findall(r"^\s*(?:class|struct)\s+(?:\w+_API\s+)?(\w+)\s*(?::[^;{]*)?\{", txt, re.M))
        known.update(re.findall(r"^\s*enum\s+(\w+)\s*(?::\s*\w+\s*)?\{", txt, re.M))
    return known


def slot_sigs(dll: str, sym: str) -> list[str]:
    """The undecorated signature of each slot of an exported vtable (the label when none)."""
    labels = [lab for _, _, lab in ev.dll(dll).vtable(sym)]
    pretty = undname_all([l for l in labels if l.startswith("?")])
    it = iter(pretty)
    return [next(it) if l.startswith("?") else l for l in labels]


def emit_class(cls: str, dll: str, sym: str, first: int, tm: TypeMapper, api: str = "",
               pure: dict[int, str] | None = None, impl: tuple[str, str] | None = None,
               notes: dict[int, str] | None = None) -> list[str]:
    notes = notes or {}
    vt = ev.dll(dll).vtable(sym)
    sigs = slot_sigs(dll, sym)
    impl_sigs = slot_sigs(*impl) if impl else []

    def declaration(slot: int, sig: str, prefix: str, suffix: str) -> tuple[str, str]:
        m = SIG_RE.match(sig)
        if not m:
            raise ValueError(f"{cls} slot {slot}: cannot parse {sig!r}")
        ret = tm.type(m["ret"])
        args = [] if m["args"] in ("void", "") else split_args(m["args"])
        params = ", ".join("..." if a == "..." else tm.type(a) for a in args)
        cc = "VARARGS " if m["cc"] == "__cdecl" else ("STDCALL " if m["cc"] == "__stdcall" else "")
        const = " const" if m["const"] else ""
        return m["name"], f"{prefix}virtual {ret} {cc}{m['name']}({params}){const}{suffix};"

    decls = []  # (slot, name, text)
    for (slot, _, lab), sig in zip(vt, sigs):
        if slot < first:
            continue
        if "_purecall" in lab:
            if pure and slot in pure:
                name = re.search(r"(\w+)\(", pure[slot].replace("VARARGS ", "")).group(1)
                decls.append((slot, name, f"virtual {pure[slot]} = 0;"))
            elif slot < len(impl_sigs) and SIG_RE.match(impl_sigs[slot]):
                decls.append((slot, *declaration(slot, impl_sigs[slot], "", " = 0")))
            else:
                # C4 port: named like the placeholder NWindow's generated headers declare for the slot
                # (`virtual void NewVirtual<slot>();`, defined as a jump to the stock function), so the class
                # that implements the slot overrides this one - a name of its own would append a new slot at the
                # end of its vtable and every virtual call of ours past it would go to the wrong place
                decls.append((slot, f"NewVirtual{slot}", f"virtual void NewVirtual{slot}() = 0;"))
            continue
        decls.append((slot, *declaration(slot, sig, api, "")))
    # MSVC groups every overload of a name at its first declaration: a name split over two runs
    # of slots cannot be declared in this order
    seen = set()
    for k, (slot, name, _) in enumerate(decls):
        if name in seen and decls[k - 1][1] != name:
            raise ValueError(f"{cls} slot {slot}: {name} also declared in an earlier, separate slot")
        seen.add(name)
    # reverse runs of adjacent overloads (same name)
    out, i = [], 0
    while i < len(decls):
        j = i
        while j + 1 < len(decls) and decls[j + 1][1] == decls[i][1] and not decls[i][1].startswith("NewVirtual"):
            j += 1
        out.extend(reversed(decls[i:j + 1]))
        i = j + 1
    width = max(len(t) for _, _, t in out) + 2
    lines = [f"// {cls}: {len(vt)} slots from {dll}.dll {sym} (tools/gen_engine_decls.py, do not edit)"]
    for slot, _, text in out:
        extra = f"  [{notes[slot]}]" if slot in notes else ""
        lines.append(f"{text:<{width}}// {slot:3} +0x{slot * 4:03x}{extra}")
    return lines


def main() -> int:
    GEN.mkdir(parents=True, exist_ok=True)
    tm = TypeMapper(known_types())
    for cls, dll, sym, parent, api, table, impl in TARGETS:
        first = len(ev.dll(*parent[:1]).vtable(parent[1])) if parent else 0
        pure, notes = pure_names(table)
        lines = emit_class(cls, dll, sym, first, tm, api, pure, impl, notes)
        (GEN / f"{cls}.vt.inc").write_text("\n".join(lines) + "\n", encoding="utf-8")
        print(f"{cls}: {len(lines) - 1} declarations")
    fwd = ["// Forward declarations for the generated engine vtables (tools/gen_engine_decls.py, do not edit)",
           "#pragma once", ""]
    for name, tag in sorted(tm.fwd.items()):
        fwd.append(f"enum {name} : INT;" if tag == "enum" else f"{tag} {name};")
    (GEN / "EngineFwd.gen.h").write_text("\n".join(fwd) + "\n", encoding="utf-8")
    print(f"forward declarations: {len(tm.fwd)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
