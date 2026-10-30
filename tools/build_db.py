#!/usr/bin/env python3
"""Master function table: db/functions.csv (+ docs/PROGRESSO.md).

Merges every source of knowledge about the stock NWindow.dll (the C2 client's, orig/NWindow.dll):
  analysis/ghidra/functions.tsv  every function Ghidra knows (after tools/gen_seeds.py pass)
  analysis/classes.json          IMPLEMENT_CLASS table: ctors, vtables (tools/extract_classes.py)
  analysis/eh.json               EH funclets and guard() names (tools/eh_scan.py)
  db/names.csv                   names given by hand (va,name[,note]) - highest priority
  db/morto.csv                   stock functions no path of the game reaches in hook mode
  db/vslots.csv                  virtual slot names (class,slot,name): named once, inherited by
                                 every override in the subclasses (C1's, remapped to the C2 slots)
  db/fmap.csv                    the C1 function each C2 function was rebuilt from (tools/port/
                                 match.py): its C1 name is inherited, at the lowest priority
  src/nwindow/**                 NW_HOOK_* macros: which functions are already reimplemented
                                 (only with --game-src: until src/nwindow is regenerated for C2 it
                                 still holds the C1 copy, whose hook addresses mean nothing here)

Where a name comes from (column name_src of db/functions.csv), highest priority first:
  manual   db/names.csv
  source   an NW_HOOK_* of the reimplementation
  guard    the UE2 guard() string of the function (eh_scan.py)
  export   exported by the image (DllMain)
  class    the IMPLEMENT_CLASS machinery: initializer, autoclass, atexit, InternalConstructor, ctor, dtor
  vslot    a virtual slot whose name is known in C2: UObject's, the engine base class's (read from the
           C2 Engine.dll / Core.dll vtables), hand-named (db/vslots.csv) or taken from a guard() string
  ghidra   a library name Ghidra's FID matched (CRT)
  fmap     inherited from the C1 function the fmap says this one was rebuilt from; fmap:A/B/C/D is
           the match category (strict-equal, equal after masking offsets, same shape, different shape)
  fmap-slot a virtual slot named by what the C1 names of its overrides agree on
  vfn      placeholder Class::vfnNNN of a virtual slot nobody named

kind:   game | init (dynamic initializers / atexit) | eh (catch/unwind funclet or the handler
        stub, goes with its parent) | crt (static MSVC 7.0 runtime, replaced by ours) | thunk
        (import jump)
status: todo | done (reimplemented + hooked) | verified (done + tested in game) | conv (never
        hooked by convention) | fase5 (static initialization: the standalone build writes it) |
        morto (db/morto.csv: every caller is ours) | lib | part; the reason for conv, fase5 and
        morto goes to notes
"""
from __future__ import annotations

import argparse
import csv
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

# Layout of the C3 image's .text (orig/NWindow.dll), found like the C1/C2 ones were (the linker's
# `jmp [IAT]` stubs, then the first libcmt 7.0 object) and checked against the Ghidra list:
#   0x10001000..          the game objects (Catch@ funclets sit inside their functions)
#   0x10144A60-0x10144B67 stubs of Core/Engine virtuals the vtables point at (44 x `jmp [IAT]`)
#   0x10144B70-0x1014C0BF 84 functions of an object linked in between: the nProtect GameGuard client
#                         library (29,318 bytes; C2: 83 functions, 28,658 bytes)
#   0x1014C0C0-0x1014C18B IMM32 / VERSION / WS2_32 / WININET stubs (34 x `jmp [IAT]`)
#   0x1014C18C..          libcmt: the first one is a 28-byte function followed by 11, 63, 24 and 63
#                         bytes, exactly as the C1 and C2 CRTs begin after their stubs
#   0x1015C240-0x10168595 the Unwind@ funclets and the __ehhandler stubs of the C++ EH (2,214 funclets)
#   0x101685B0..          the static initializers (IMPLEMENT_CLASS, atexit stubs, TArray globals)
# C2 (for the record): thunks 0x100DFDD0-0x100DFED7 and 0x100E7180-0x100E7239, CRT_LO 0x100E723A.
# C1: thunks 0x100BD360-0x100BD4D3, CRT_LO 0x100BD4D4.
THUNK_BLOCKS = [(0x10169420, 0x10169527), (0x10170A90, 0x10170B49)]
CRT_LO = 0x10170B4A

# the 24 virtuals of C1's UObject plus ClearL2Game (slot 22) that the C2 Core added
UOBJECT_SLOTS = ["QueryInterface", "AddRef", "Release", "`scalar deleting destructor'",
                 "ProcessEvent", "ProcessDelegate", "ProcessState", "ProcessRemoteFunction",
                 "Modify", "PostLoad", "Destroy", "Serialize", "IsPendingKill", "GotoState",
                 "GotoLabel", "InitExecution", "ShutdownAfterError", "PostEditChange",
                 "CallFunction", "ScriptConsoleExec", "Register", "LanguageChange", "ClearL2Game",
                 "Rename", "NetDirty"]
# (at the start of a line: a hook switched off with `// NW_HOOK_...` is not a hook; the count of the progress file was 13 too high)
HOOK_RE = re.compile(r"^[ \t]*NW_HOOK_(\w+)\(\s*(0x[0-9A-Fa-f]{8})\s*,\s*([^\n]*)", re.M)
# the stock globals our headers name: #define GName NW_GLOBAL(Type, 0x1012xxxx)
GLOBAL_RE = re.compile(r"#define\s+(G\w+)\s+NW_GLOBAL\(.*?,\s*(0x[0-9A-Fa-f]{8})\s*\)")
# the stock vtables the source checks ours against: NW_CHECK_VTABLE(Class, 0x100dxxxx, N)
VTCHECK_RE = re.compile(r"NW_CHECK_VTABLE(?:_FOR)?\(\s*(\w+)\s*,(?:\s*\w+\s*,)?\s*(0x[0-9A-Fa-f]{8})")


def hook_name(kind: str, args: str) -> str:
    """Name a NW_HOOK_* registration gives the function it replaces."""
    args = args.split("//")[0]
    parts = [p.strip().rstrip(")").strip() for p in args.split(",")]
    if kind in ("CTOR", "CTOR_ARGS"):
        return f"{parts[0]}::{parts[0]}"
    if kind == "DTOR":
        return f"{parts[0]}::~{parts[0]}"
    if kind == "FUNC":
        return parts[0]
    return f"{parts[0]}::{parts[1]}"  # METHOD, METHOD_SIG, VARARGS


def read_csv(path: Path) -> list[dict]:
    if not path.exists():
        return []
    return list(csv.DictReader((l for l in path.open(encoding="utf-8") if not l.startswith("#"))))


def import_jump(img, va: int, size: int) -> bool:
    """A 6-byte `jmp dword ptr [IAT slot]`: the linker's stub for an import (also found inside
    the game objects, for the functions of npkcrypt.dll and a few Engine ones)."""
    b = img.read(va, 6)
    return size == 6 and b[:2] == b"\xff\x25" and int.from_bytes(b[2:6], "little") in img.imports


def engine_slot_names(cls: str):
    """Method name of every slot of an engine class's vtable (UObject, UL2ConsoleWnd, UL2Font),
    read from the C2 Core.dll / Engine.dll the way tools/engine_vtables.py does: slot -> name,
    or None for a slot with no name there (pure virtual, the deleting destructor)."""
    import engine_vtables as ev

    for d in ("Engine", "Core"):
        dll = ev.dll(d)
        syms = [n for n in dll.exp if n.startswith(f"??_7{cls}@@6B")]
        if not syms:
            continue
        sym = sorted(syms, key=lambda n: (n != f"??_7{cls}@@6B@", n))[0]
        out = []
        for _, _, label in dll.vtable(sym):
            m = re.match(r"^(?:import:)?\?(\w+)@\w+@@", label)
            out.append(m.group(1) if m else None)
        return out
    return []


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--game-src", action="store_true",
                    help="read the NW_HOOK_*/NW_CHECK_VTABLE macros of src/nwindow (only once that "
                         "directory has been regenerated for the C2 image)")
    args = ap.parse_args()
    sys.path.insert(0, str(ROOT / "tools"))
    from nwd.image import Image

    funcs = list(csv.DictReader((ROOT / "analysis/ghidra/functions.tsv").open(encoding="utf-8"),
                                delimiter="\t"))
    classes = json.loads((ROOT / "analysis/classes.json").read_text(encoding="utf-8"))
    eh = json.loads((ROOT / "analysis/eh.json").read_text(encoding="utf-8"))
    cpp = {c["cpp"]: c for c in classes}
    img = Image()
    src_dirs = [ROOT / "src" / "nwindow"] if args.game_src else []

    by_va = {int(f["va"], 16): f for f in funcs}
    # entries Ghidra missed (db/extra_functions.csv)
    for r in read_csv(ROOT / "db/extra_functions.csv"):
        va = int(r["va"], 16)
        if va not in by_va:
            by_va[va] = {"va": r["va"], "size": r["size"], "name": f"FUN_{r['va']}",
                         "ranges": f"{va:x}-{va + int(r['size']) - 1:x}"}
    rows = {}
    for va, f in by_va.items():
        rows[va] = {"va": va, "size": int(f["size"]), "kind": "game", "name": "", "class": "",
                    "status": "todo", "src": "", "notes": "", "name_src": ""}

    # ---- kinds -----------------------------------------------------------------------------
    funclet_parent = {}
    stub_parent = {}   # `mov eax, FuncInfo; jmp __CxxFrameHandler`, filed as functions
    for pva, e in eh.items():
        for c in e["catches"]:
            funclet_parent[c["va"]] = int(pva, 16)
        for u in e["unwinds"]:
            funclet_parent[u] = int(pva, 16)
        if e.get("stub"):
            stub_parent[e["stub"]] = int(pva, 16)
    frame_handler = next(va for va, f in by_va.items() if f["name"] == "___CxxFrameHandler")

    def handler_stub(va: int) -> bool:
        """`mov eax, FuncInfo; jmp ___CxxFrameHandler` (a stub shared by several functions has no
        single parent, so eh_scan.py does not list it)."""
        b = img.read(va, 10)
        return (b[0] == 0xB8 and b[5] == 0xE9
                and va + 10 + int.from_bytes(b[6:10], "little", signed=True) == frame_handler)

    eh_region = min(va for va, f in by_va.items()
                    if va > CRT_LO and f["name"].startswith(("Unwind@", "Catch@")))
    init_lo = min(c["initializer"] for c in classes)
    for va, r in rows.items():
        name = by_va[va]["name"]
        if name.startswith(("Catch@", "Unwind@")):
            r["kind"], r["status"] = "eh", "part"
            p = funclet_parent.get(va)
            r["notes"] = f"parent {p:08x}" if p else ""
        elif va in stub_parent:
            r["kind"], r["status"] = "eh", "part"
            r["notes"] = f"parent {stub_parent[va]:08x} (handler stub)"
        elif any(a <= va <= b for a, b in THUNK_BLOCKS) or import_jump(img, va, r["size"]):
            r["kind"], r["status"], r["name"] = "thunk", "lib", name.split("::", 1)[-1]
            r["name_src"] = "ghidra"
        elif eh_region <= va < init_lo and handler_stub(va):
            r["kind"], r["status"] = "eh", "part"
            r["notes"] = "handler stub shared by several functions"
        elif CRT_LO <= va < eh_region:
            r["kind"], r["status"] = "crt", "lib"
            if not name.startswith(("FUN_", "FID_conflict:")):
                r["name"], r["name_src"] = name, "ghidra"
        elif va >= init_lo:
            r["kind"] = "init"
    for r in read_csv(ROOT / "db/not_functions.csv"):
        va = int(r["va"], 16)
        if va in rows:
            rows[va]["kind"], rows[va]["status"] = "fragment", "part"
            rows[va]["notes"] = f"part of {r['part_of']}: {r['reason']}"

    # names: va -> (priority, name, class, source); the lowest priority number wins, the first
    # one given wins a tie
    PRIO = {"manual": 0, "source": 1, "guard": 2, "export": 3, "class": 4, "global": 4, "dtor": 4,
            "vslot": 5, "ghidra": 6, "fmap": 7, "fmap-slot": 8, "vfn": 9}
    names: dict[int, tuple[int, str, str, str]] = {}

    def give(va, name, cls, src, prio=None):
        if va not in rows or rows[va]["kind"] in ("thunk", "eh", "fragment"):
            return  # an import stub is named after its import, a funclet goes with its parent
        p = PRIO[src.split(" ")[0].split(":")[0]] if prio is None else prio
        if va not in names or p < names[va][0]:
            names[va] = (p, name, cls, src)

    # the library names Ghidra's FID gave the CRT stand at their own priority (an ambiguous
    # FID_conflict name is worth less than a C1 name)
    for va, r in rows.items():
        if r["kind"] == "crt" and not r["name"]:
            n = by_va[va]["name"]
            if n.startswith("FID_conflict:"):
                give(va, n, "", "ghidra", PRIO["fmap"] + 1)
        elif r["kind"] == "crt":
            give(va, r["name"], "", "ghidra")
            r["name"] = ""

    # ---- IMPLEMENT_CLASS: initializers, ctors, InternalConstructor -------------------------
    text_lo, text_hi = img.sect_range(".text")
    text = img.read(text_lo, text_hi - text_lo)

    def body_at(pattern: bytes):
        """Address of the only place in .text holding these bytes (give() keeps function starts)."""
        i = text.find(pattern)
        return text_lo + i if i >= 0 and text.find(pattern, i + 1) < 0 else None

    def le(v: int) -> bytes:
        return v.to_bytes(4, "little")

    uclass_dtor = le(img.import_slot("??1UClass@@UAE@XZ"))
    ctor_users: dict[int, list[str]] = {}
    for c in classes:
        give(c["initializer"], f"{c['cpp']}::PrivateStaticClass`dynamic initializer'", c["cpp"], "class")
        # the export's: mov [autoclassX], &X::PrivateStaticClass; ret
        va = body_at(b"\xc7\x05" + le(c["autoclass_va"]) + le(c["static_class"]) + b"\xc3")
        if va:
            give(va, f"autoclass{c['cpp']}`dynamic initializer'", c["cpp"], "class")
        # what the initializer registers with atexit: mov ecx, &X::PrivateStaticClass; jmp [~UClass]
        va = body_at(b"\xb9" + le(c["static_class"]) + b"\xff\x25" + uclass_dtor)
        if va:
            give(va, f"{c['cpp']}::PrivateStaticClass`dynamic atexit destructor'", c["cpp"], "class")
        give(c["internal_ctor"], f"{c['cpp']}::InternalConstructor", c["cpp"], "class")
        if c["ctor"]:
            ctor_users.setdefault(c["ctor"], []).append(c["cpp"])

    # the other static objects (TArray globals): the initializer constructs the object and
    # registers its destructor with atexit (or only registers it), the atexit destructor is
    # `mov ecx, &G; jmp ~T`; named after the global our headers declare at that address. The
    # headers are read only with --game-src: include/nwindow is the C1 copy until regenerated
    # (C1 addresses). Until then the fmap gives these initializers the C1 global's name.
    gname = {}
    if args.game_src:
        for path in sorted((ROOT / "include").rglob("*.h")):
            for m in GLOBAL_RE.finditer(path.read_text(encoding="utf-8", errors="replace")):
                gname[int(m.group(2), 16)] = m.group(1)

    def this_global(va):
        b = img.read(va, 5)
        return gname.get(int.from_bytes(b[1:], "little")) if b[0] == 0xB9 else None

    for va, r in rows.items():
        if r["kind"] != "init" or va in names or not gname:
            continue
        ins = list(img.disasm(va, r["size"], stop_at_ret=True))
        g = this_global(va)
        if g and ins[-1].mnemonic == "jmp":
            give(va, f"{g}`dynamic atexit destructor'", "", "global")
            continue
        for i in ins:
            if not g and i.mnemonic == "push" and i.op_str.startswith("0x") and img.in_text(int(i.op_str, 16)):
                g = this_global(int(i.op_str, 16))
        if g:
            give(va, f"{g}`dynamic initializer'", "", "global")

    # code the image exports by name: the package's DllMain (_DllMain@12)
    for exp, va in img.exports.items():
        if va in rows:
            give(va, exp.lstrip("_").split("@")[0], "", "export")
    for va, users in ctor_users.items():
        users.sort(key=lambda n: cpp[n]["depth"])
        give(va, f"{users[0]}::{users[0]}", users[0], "class")
        if len(users) > 1:
            rows[va]["notes"] = "ICF: tambem " + ", ".join(f"{u}::{u}" for u in users[1:])

    # ---- names inherited from the C1 project (db/fmap.csv) ----------------------------------
    fmap_name: dict[int, tuple[str, str]] = {}   # c2 va -> (C1 name, match category)
    for r in read_csv(ROOT / "db/fmap.csv"):
        n = r["name"]
        meth = n.split("::")[-1]
        if (not n or n.startswith(("sub_", "FUN_")) or re.fullmatch(r"vfn\d+", meth)
                or r["kind"] in ("eh", "thunk", "fragment")):
            continue  # an unnamed C1 function, or a C1 slot number that means nothing in C2
        fmap_name[int(r["c2_va"], 16)] = (n, r["cat"])
    for va, (n, cat) in fmap_name.items():
        give(va, n, n.split("::")[0] if "::" in n else "", f"fmap:{cat}")

    # ---- virtual slots ---------------------------------------------------------------------
    def slots(c):
        return [s["va"] for s in c.get("vtable_slots", [])]

    def slot_owner(c, k):
        """Highest ancestor of c that already has slot k (the class that introduced it)."""
        owner = c["cpp"]
        s = c.get("super")
        while s in cpp and k < len(slots(cpp[s])):
            owner = s
            s = cpp[s].get("super")
        return owner

    vslot_names = {}
    for r in read_csv(ROOT / "db/vslots.csv"):
        vslot_names[(r["class"], int(r["slot"]))] = (r["name"], "vslot")

    # the engine base class's own virtuals, named by the C2 Core.dll / Engine.dll
    engine = {b: engine_slot_names(b) for b in {c["engine_base"] for c in classes}}
    for c in classes:
        for k, nm in enumerate(engine.get(c["engine_base"], [])):
            if nm and k < len(slots(c)) and k >= len(UOBJECT_SLOTS):
                vslot_names.setdefault((slot_owner(c, k), k), (nm, "vslot"))

    guard_name = {}
    for pva, e in eh.items():
        g = e["guard"]
        if g:
            base = [n for n in g if all(o.startswith(n) for o in g)]
            guard_name[int(pva, 16)] = base[0] if base else g[-1]

    # a guard name on an override names the whole slot
    for c in classes:
        for k, t in enumerate(slots(c)):
            if t in guard_name and "::" in guard_name[t]:
                cls, meth = guard_name[t].split("::", 1)
                if cls == c["cpp"] and "::" not in meth:
                    vslot_names.setdefault((slot_owner(c, k), k), (meth, "vslot"))

    # and so does what the C1 names of the overrides agree on (a vote: the most common method
    # name wins), when no other source names the slot
    votes: dict[tuple[str, int], dict[str, int]] = {}
    for c in classes:
        sup = cpp.get(c.get("super"))
        inherited = slots(sup) if sup else []
        for k, t in enumerate(slots(c)):
            if (k < len(inherited) and inherited[k] == t) or t not in fmap_name:
                continue
            n = fmap_name[t][0]
            if "::" not in n or k < len(UOBJECT_SLOTS):
                continue
            meth = n.split("::", 1)[1]
            if meth.startswith(("~", "`")) or not re.fullmatch(r"\w+", meth):
                continue
            v = votes.setdefault((slot_owner(c, k), k), {})
            v[meth] = v.get(meth, 0) + 1
    n_split = 0
    for key, v in votes.items():
        if key in vslot_names:
            continue
        best = sorted(v.items(), key=lambda kv: (-kv[1], kv[0]))
        vslot_names[key] = (best[0][0], "fmap-slot")
        n_split += len(v) > 1

    for c in sorted(classes, key=lambda c: c["depth"]):
        sup = cpp.get(c.get("super"))
        inherited = slots(sup) if sup else []
        for k, t in enumerate(slots(c)):
            if k < len(inherited) and inherited[k] == t:
                continue  # not overridden here
            if rows.get(t, {}).get("kind") == "thunk":
                continue
            if k < len(UOBJECT_SLOTS):
                meth, how = UOBJECT_SLOTS[k], "vslot"
            elif (slot_owner(c, k), k) in vslot_names:
                meth, how = vslot_names[(slot_owner(c, k), k)]
            else:
                meth, how = f"vfn{k:03d}", "vfn"
            give(t, f"{c['cpp']}::{meth}", c["cpp"], how)
            note = f"vslot {k} (+0x{4 * k:x})"
            if note not in rows[t]["notes"]:
                rows[t]["notes"] = (rows[t]["notes"] + "; " if rows[t]["notes"] else "") + note

    # the real destructor: first direct call of the `scalar deleting destructor' (vslot 3)
    for c in sorted(classes, key=lambda c: c["depth"]):
        s = slots(c)
        if len(s) > 3:
            for ins in img.disasm(s[3], 0x40, stop_at_ret=True):
                if ins.mnemonic == "call" and ins.op_str.startswith("0x"):
                    give(int(ins.op_str, 16), f"{c['cpp']}::~{c['cpp']}", c["cpp"], "dtor")
                    break

    for va, g in guard_name.items():
        give(va, g, g.split("::")[0] if "::" in g else "", "guard")

    # names carried by the reimplementation itself (NW_HOOK_* in src/nwindow)
    hooks = []
    for d in src_dirs:
        for path in sorted(d.rglob("*.cpp")):
            for m in HOOK_RE.finditer(path.read_text(encoding="utf-8", errors="replace")):
                hooks.append((int(m.group(2), 16), m.group(1), m.group(3), path))
    for va, kind, hargs, path in hooks:
        n = hook_name(kind, hargs)
        # a stock guard copied from another function is reproduced on purpose: no warning when
        # the source file uses that guard name
        if (va in guard_name and guard_name[va] != n
                and f"guard({guard_name[va]})" not in path.read_text(encoding="utf-8", errors="replace")):
            print(f"WARN {va:08x}: source says {n}, guard() string says {guard_name[va]}")
        give(va, n, n.split("::")[0] if "::" in n else "", "source")

    for r in read_csv(ROOT / "db/names.csv"):
        va = int(r["va"], 16)
        give(va, r["name"], r["name"].split("::")[0] if "::" in r["name"] else "", "manual")
        if r.get("note") and va in rows:
            rows[va]["notes"] = r["note"]

    for va, (_, n, cls, src) in names.items():
        rows[va]["name"], rows[va]["class"] = n, cls
        rows[va]["name_src"] = src if src.startswith("fmap:") else src.split(" ")[0]
    for va, r in rows.items():
        if not r["name"]:
            r["name"] = f"sub_{va:08x}"
        if r["kind"] == "eh" and r["notes"].startswith("parent "):
            p = int(r["notes"][7:15], 16)
            r["class"] = rows.get(p, {}).get("class", "")

    # ---- status from the source tree ---------------------------------------------------------
    verified = {int(r["va"], 16) for r in read_csv(ROOT / "db/verified.csv")}
    for va, kind, hargs, path in hooks:
        if va not in rows:
            print(f"WARN {path.name}: hook {va:#x} is not a known function start")
            continue
        rows[va]["status"] = "verified" if va in verified else "done"
        rows[va]["src"] = path.relative_to(ROOT).as_posix()

    set_left_status(rows, by_va, classes, img, src_dirs)

    # ---- write -------------------------------------------------------------------------------
    out = ROOT / "db" / "functions.csv"
    cols = ["va", "size", "kind", "status", "name", "class", "src", "notes", "name_src"]
    with out.open("w", encoding="utf-8", newline="") as fh:
        w = csv.DictWriter(fh, fieldnames=cols)
        w.writeheader()
        for va in sorted(rows):
            r = dict(rows[va])
            r["va"] = f"{va:08x}"
            w.writerow(r)

    write_progress(rows, classes)
    name_stats(rows, n_split)
    return 0


def name_stats(rows, n_split):
    """Which sources the names of db/functions.csv come from (printed, not stored)."""
    from collections import Counter

    named, unnamed = Counter(), Counter()
    for r in rows.values():
        if r["name_src"] and r["name_src"] != "vfn":
            named[(r["kind"], r["name_src"])] += 1
        else:
            unnamed[r["kind"]] += 1
    kinds = sorted({k for k, _ in named} | set(unnamed))
    srcs = sorted({s for _, s in named})
    total = sum(named.values())
    print(f"names: {total} of {len(rows)} functions have a real name; {len(rows) - total} do not "
          "(sub_xxxxxxxx, or Class::vfnNNN for a virtual slot nobody named)")
    print("  " + "kind".ljust(9) + "".join(s.rjust(10) for s in srcs) + "unnamed".rjust(10))
    for k in kinds:
        print("  " + k.ljust(9) + "".join(str(named.get((k, s), 0)).rjust(10) for s in srcs)
              + str(unnamed.get(k, 0)).rjust(10))
    print("  total     " + "".join(str(sum(v for (k, s2), v in named.items() if s2 == s)).rjust(10) for s in srcs)
          + str(sum(unnamed.values())).rjust(10))
    print(f"  ({n_split} virtual slots had disagreeing C1 names for their overrides: the majority won)")


FINISHED = ("done", "verified")
LEFT = ("conv", "fase5", "morto")


def set_left_status(rows, by_va, classes, img, src_dirs):
    """Status for what has no hook on purpose, with the reason in notes (docs/notas/Restante.md):
    conv   never hooked by convention: the deleting destructors the compiler writes for every
           class with a virtual destructor and IMPLEMENT_CLASS's InternalConstructor; the stock
           one only calls a destructor / constructor that has a hook
    fase5  the image's static initialization (IMPLEMENT_CLASS, the autoclass exports, the
           atexit destructors, the TArray globals) and DllMain: the standalone build writes them
    morto  db/morto.csv: every caller is ours; checked here against the stock code
    """
    import bisect

    uobject_delete = f"[{img.import_slot('??3UObject@@SAXPAXI@Z'):#x}]"
    gmalloc = f"[{img.import_slot('?GMalloc@@3PAVFMalloc@@A'):#x}]"
    ictors = {c["internal_ctor"] for c in classes}
    exported = set(img.exports.values())
    morto = {int(r["va"], 16): r["reason"] for r in read_csv(ROOT / "db/morto.csv")}
    vt_class = {}  # stock vtable -> class, from the NW_CHECK_VTABLEs of the source
    for d in src_dirs:
        for path in sorted(d.rglob("*.cpp")):
            for m in VTCHECK_RE.finditer(path.read_text(encoding="utf-8", errors="replace")):
                vt_class[int(m.group(2), 16)] = m.group(1)

    def mark(r, status, why):
        r["status"] = status
        r["notes"] = f"{r['notes']}; {why}" if r["notes"] else why

    def deleting_dtor(va, size):
        """For `~T(); if (flags & 1) delete this; return this;`: (True, the ~T it calls or None
        when it is inline); (False, None) for anything else."""
        ins = list(img.disasm(va, size, stop_at_ret=True))
        if not ins or ins[-1].mnemonic != "ret" or ins[-1].op_str != "4":
            return False, None
        body = " ; ".join(f"{i.mnemonic} {i.op_str}" for i in ins)
        if not re.search(r"test byte ptr \[esp \+ [48]\], 1", body):
            return False, None
        if uobject_delete not in body and not (gmalloc in body and "+ 8]" in body):
            return False, None
        calls = [int(i.op_str, 16) for i in ins if i.mnemonic == "call" and i.op_str.startswith("0x")]
        return True, calls[0] if calls else None

    # who calls what (rel32 call/jmp), and which function holds an address (Ghidra's ranges)
    lo, hi = img.sect_range(".text")
    text = img.read(lo, hi - lo)
    sites: dict[int, list[int]] = {}
    for m in re.finditer(rb"[\xe8\xe9]", text):
        p = m.start()
        if p + 5 <= len(text):
            t = lo + p + 5 + int.from_bytes(text[p + 1:p + 5], "little", signed=True)
            sites.setdefault(t, []).append(lo + p)
    spans = sorted((int(a, 16), int(b, 16), va) for va, f in by_va.items()
                   for a, b in (rg.split("-") for rg in f["ranges"].split(",")))
    starts = [s[0] for s in spans]

    def ours(site):
        i = bisect.bisect_right(starts, site) - 1
        while i >= 0 and spans[i][0] <= site:
            if site <= spans[i][1]:
                r = rows[spans[i][2]]
                if r["kind"] == "eh" and r["notes"].startswith("parent "):
                    r = rows.get(int(r["notes"][7:15], 16), r)
                return r["status"] in FINISHED
            i -= 1
        return False

    for va in sorted(rows):
        r = rows[va]
        if r["kind"] not in ("game", "init") or r["status"] != "todo":
            continue
        n = r["name"]
        if r["kind"] == "init" or va in exported:
            if "::PrivateStaticClass`dynamic initializer'" in n:
                why = "IMPLEMENT_CLASS: construtor nativo da UClass + atexit"
            elif n.startswith("autoclass"):
                why = "inicializador do export " + n.split("`")[0]
            elif "::PrivateStaticClass`dynamic atexit destructor'" in n:
                why = "~UClass registrado no atexit"
            elif n.startswith("G") and "`dynamic initializer'" in n:
                calls = sum(1 for i in img.disasm(va, r["size"]) if i.mnemonic == "call")
                why = "global " + n.split("`")[0] + (" (TArray): construtor e registro no atexit" if calls > 1
                                                     else " (TArray zerado): só o registro no atexit")
            elif n.startswith("G") and "`dynamic atexit destructor'" in n:
                why = "global " + n.split("`")[0] + " (TArray): destrutor registrado no atexit"
            elif n == "DllMain":
                why = "DllMain do IMPLEMENT_PACKAGE; sem gancho: o regcheck acusa o chamador do CRT"
            else:
                why = "inicialização estática"
            mark(r, "fase5", why + " (fase 5)")
            continue
        if va in ictors:
            t = next((int(i.op_str, 16) for i in img.disasm(va, r["size"], stop_at_ret=True)
                      if i.mnemonic in ("call", "jmp") and i.op_str.startswith("0x")), None)
            if t in rows and rows[t]["status"] in FINISHED:
                mark(r, "conv", f"InternalConstructor do IMPLEMENT_CLASS: chama {rows[t]['name']}, que tem gancho")
            continue
        is_sdd, dtor = deleting_dtor(va, r["size"])
        if is_sdd:
            if dtor is None:
                vt = next((int(i.op_str.split(", ")[1], 16) for i in img.disasm(va, r["size"], stop_at_ret=True)
                           if i.mnemonic == "mov" and re.fullmatch(r"dword ptr \[e\w\w\], 0x[0-9a-f]+", i.op_str)), None)
                cls, why = vt_class.get(vt, ""), "o destrutor é inline"
            elif dtor in rows and rows[dtor]["status"] in FINISHED:
                cls, why = rows[dtor]["class"], f"chama {rows[dtor]['name']}, que tem gancho"
            else:
                continue
            if n.startswith("sub_") and cls:
                r["name"], r["class"] = f"{cls}::`scalar deleting destructor'", cls
            mark(r, "conv", "deleting destructor do compilador: " + why)
            continue
        if va in morto:
            stock = [s for s in sites.get(va, []) if not ours(s)]
            if stock:
                print(f"WARN morto {va:08x}: still called by the original at "
                      + ", ".join(f"{s:08x}" for s in stock))
            else:
                mark(r, "morto", morto[va])


def write_progress(rows, classes):
    from collections import Counter

    work = [r for r in rows.values() if r["kind"] in ("game", "init")]
    done = [r for r in work if r["status"] in FINISHED]
    ver = [r for r in work if r["status"] == "verified"]
    left = {s: [r for r in work if r["status"] == s] for s in LEFT + ("todo",)}
    tot_b = sum(r["size"] for r in work)
    done_b = sum(r["size"] for r in done)
    kinds = Counter(r["kind"] for r in rows.values())

    def count(rs):
        return f"**{len(rs)}** ({sum(r['size'] for r in rs):,} bytes)"

    lines = [
        "# Progresso da decompilação", "",
        "Gerado por `tools/build_db.py` — não editar à mão.", "",
        f"- Funções a decompilar (game + init): **{len(work)}** ({tot_b:,} bytes)",
        f"- Reimplementadas (hook ativo): **{len(done)}** ({done_b:,} bytes, "
        f"{100.0 * done_b / tot_b:.2f}% do código)",
        f"- Verificadas no jogo: **{len(ver)}**",
        f"- Sem gancho, com motivo na coluna notes do banco (docs/notas/Restante.md):",
        f"  - convenção (deleting destructors, InternalConstructor): {count(left['conv'])}",
        f"  - só na fase 5 (inicialização estática, DllMain): {count(left['fase5'])}",
        f"  - mortas no modo gancho (db/morto.csv): {count(left['morto'])}",
        f"- Sem gancho e sem motivo (todo): {count(left['todo'])}", "",
        "| kind | funções |", "|---|---:|",
    ]
    for k, n in sorted(kinds.items()):
        lines.append(f"| {k} | {n} |")
    lines += ["", "## Por classe", "", "| classe | funções | feitas | sem gancho | todo | bytes | % bytes |",
              "|---|---:|---:|---:|---:|---:|---:|"]
    by_cls: dict[str, list] = {}
    for r in work:
        by_cls.setdefault(r["class"] or "(sem classe)", []).append(r)
    for cls in sorted(by_cls, key=lambda c: (c == "(sem classe)", c.lower())):
        rs = by_cls[cls]
        b = sum(r["size"] for r in rs)
        db = sum(r["size"] for r in rs if r["status"] in FINISHED)
        nd = sum(1 for r in rs if r["status"] in FINISHED)
        nl = sum(1 for r in rs if r["status"] in LEFT)
        nt = sum(1 for r in rs if r["status"] == "todo")
        lines.append(f"| {cls} | {len(rs)} | {nd} | {nl} | {nt} | {b:,} | {100.0 * db / b if b else 0:.1f}% |")
    (ROOT / "docs" / "PROGRESSO.md").write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"db: {len(rows)} functions; work {len(work)} ({tot_b:,} B): done {len(done)} "
          f"({100.0 * done_b / tot_b:.2f}%), " + ", ".join(f"{s} {len(left[s])}" for s in left)
          + f"; kinds {dict(kinds)}")


if __name__ == "__main__":
    sys.exit(main())
