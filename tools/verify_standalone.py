#!/usr/bin/env python3
"""Check a standalone NWindow.dll (phase 5, docs/notas/Standalone.md) against the stock one,
without running the game: what the engine relies on when it loads the package.

  pe         PE header: ImageBase 0x10000000, no ASLR, GUI subsystem, OS version, relocations,
             entry point; imports: Core/Engine names that exist in orig/Core.dll/Engine.dll,
             no VC++ runtime DLL, no NWindowDecomp.dll
  imports    the import table against the stock's (docs/notas/C2_Fase5.md, "nProtect e NKeyCrypt no
             standalone"): only DLLs the stock imports, none of them npkcrypt.dll (NKeyCrypt loads it
             at run time by ordinal), and every import the stock's GAME code uses is imported too (what
             only the statically linked nProtect library or the C runtime uses is not needed); and the
             CMake link line of the standalone target names a library for each DLL needed
  resources  the 7 colour cursors (RT_GROUP_CURSOR 101..131), image bytes equal to the stock
  exports    exactly the stock's (ordinal, name) pairs (180 in C1, 227 in C2, 343 in C4); _DllMain@12 is code, GPackage is
             L"NWindow", every autoclassX holds (or is set by an initializer to) X's UClass
  classes    the IMPLEMENT_CLASS registrations (178 in C1, 225 in C2, 341 in C4): the arguments of every call to Core's
             UClass native constructor (the stack emulator of tools/extract_classes.py), class
             by class against analysis/classes.json - name, size, class flags, super, within,
             GUID, package, config, object flags, InternalConstructor, StaticConstructor - and
             the order the static initializers run in
  vtables    every class's vtable (primary and FExec) through the map: slot count, and slot by
             slot the function it points to against the stock slot (analysis/classes.json
             vtable_slots): the same import, _purecall, the same function name
             (db/functions.csv, the names our NW_HOOK_* gave), the same bytes popped on return;
             plus the 6 non-UObject vtables of the NW_CHECK_VTABLE list
  source     no DLL needed: NW_DECLARE_CLASS (flags, super, UClass address) and NW_CHECK_VTABLE
             (vtable address, slot count) in our headers against analysis/classes.json; the
             phase 6 classes (NW_DECLARE_L2_CLASS / NW_IMPLEMENT_L2_CLASS, include/features and
             src/features) against tests/standalone/l2_extras.csv

  python tools/verify_standalone.py [--dll NWindow.dll] [--map NWindow.map] [--checks a,b] [--only Class]
  python tools/verify_standalone.py --stock          every check on orig/NWindow.dll itself (must pass)
  python tools/verify_standalone.py --dll build/standalone_probe/Release/NWindow_probe.dll   (the /FORCE:UNRESOLVED probe: before the
                                                     link closes; its dll name and the functions /OPT:REF dropped show as failures)
  python tools/verify_standalone.py --fixture        a stand-in NWindow.dll built here by the current
                                                     compiler (build/standalone_fixture, see below)
  python tools/verify_standalone.py --dll build/bin/Release/NWindowDecomp.dll --checks vtables
                                                     the vtables of the hook-mode build (linked /MAP too)
  python tools/verify_standalone.py --cursors-res FILE   the stock cursors as a .res for the link
  python tools/verify_standalone.py --dll build/l2/Release/NWindow.dll --l2
                                                     the phase 6 build (NWindowL2): the stock checks,
                                                     plus exactly the L2 extras of
                                                     tests/standalone/l2_extras.csv (exports: their
                                                     autoclass after the stock ordinals; classes: their
                                                     registration against the list). Without --l2 a
                                                     class or export the stock lacks fails, as for the
                                                     faithful build.
Symbols come from the linker map (/MAP, next to the DLL by default). Known differences that
are not bugs go to tests/standalone/notes.csv (check,key,explanation) and show as NOTE.
The run-time side (Core's UClass objects after loading our DLL) is the difftest case "classes":
python tools/difftest.py --standalone.
Exit code: 0 all ok, 1 a check failed, 2 nothing to check (no DLL, no map).
"""
from __future__ import annotations

import argparse
import csv
import json
import os
import re
import struct
import sys
from collections import Counter, defaultdict
from pathlib import Path

import pefile
from capstone import CS_ARCH_X86, CS_MODE_32, Cs

sys.path.insert(0, str(Path(__file__).resolve().parent))
from nwd.image import ROOT, Image  # noqa: E402
from nwd.symbols import Symbols, load_map, stock_symbols  # noqa: E402
from extract_classes import PARAMS, UCLASS_CTOR, config_expr, emulate_to, func_start, thunk_import  # noqa: E402

STOCK = ROOT / "orig" / "NWindow.dll"
# where the standalone target may put NWindow.dll (the first one found is checked)
CANDIDATES = [ROOT / "build" / "standalone" / "Release" / "NWindow.dll",
              ROOT / "build" / "bin" / "Release" / "NWindow.dll",
              ROOT / "build" / "Release" / "NWindow.dll"]
NOTES = ROOT / "tests" / "standalone" / "notes.csv"
L2_EXTRAS = ROOT / "tests" / "standalone" / "l2_extras.csv"
CHECKS = ["pe", "imports", "resources", "exports", "classes", "vtables", "source"]
STOCK_PURECALL = 0x10171E13   # the C4 image's _purecall (C3: 0x1014D4CA, C2: 0x100E84EA, C1: 0x100BE4FC)
# The game source the "source" check and the NW_CHECK_VTABLE list read: src/ and include/ of the
# C4 tree (read unless --no-game-src; --game-src is kept for the old command lines).
SRC_CPP: list[Path] = []
SRC_H: list[Path] = []
# The statically linked nProtect GameGuard client library of the stock image (C4: 86 functions, 28,322 bytes, no game code;
# C2: 83, 0x100DFEE0-0x100E717F; C4: 0x10169530-0x10170A8F, NPGTempDll .. NPGCrc32): its imports are not needed by the standalone build, which does not link it
# (docs/notas/C2_Fase5.md)
NPROTECT_LIB = (0x10169530, 0x10170A90)
# What a standalone NWindow.dll may import: the DLLs the stock image does, but npkcrypt.dll (NKeyCrypt loads it with
# LoadLibrary and resolves its eight ordinals with GetProcAddress, src/nwindow/NKeyCrypt.cpp)
RUNTIME_LOADED_DLLS = {"npkcrypt.dll"}
# the library each DLL of the standalone link comes from, as CMakeLists.txt names it (Core/Engine: the import
# libraries made from third_party/ue_import/*.def; kernel32 is a default library of the linker)
IMPORT_LIBS = {"core.dll": "UE_IMPORT_LIBS", "engine.dll": "UE_IMPORT_LIBS", "kernel32.dll": None,
               "user32.dll": "user32", "imm32.dll": "imm32", "version.dll": "version", "advapi32.dll": "advapi32",
               "ws2_32.dll": "ws2_32", "winmm.dll": "winmm", "gdi32.dll": "gdi32", "shlwapi.dll": "shlwapi",
               "wininet.dll": "wininet"}
VC_RUNTIME = ("vcruntime", "msvcp", "api-ms-win-crt", "ucrtbase", "msvcr1", "concrt")
IMAGE_FILE_DLL = 0x2000
DYNAMIC_BASE = 0x40
DLLCHAR_NAMES = {0x20: "HIGH_ENTROPY_VA", 0x40: "DYNAMIC_BASE", 0x80: "FORCE_INTEGRITY", 0x100: "NX_COMPAT",
                 0x200: "NO_ISOLATION", 0x400: "NO_SEH", 0x800: "NO_BIND", 0x1000: "APPCONTAINER",
                 0x2000: "WDM_DRIVER", 0x4000: "GUARD_CF", 0x8000: "TERMINAL_SERVER_AWARE"}


# ---- report ------------------------------------------------------------------------------------

class Report:
    def __init__(self, notes: dict[tuple[str, str], str], stock: bool = False):
        self.notes = notes
        self.stock = stock
        self.used_notes: set[tuple[str, str]] = set()
        self.counts: dict[str, Counter] = defaultdict(Counter)
        self.group = ""

    def section(self, name: str, title: str):
        self.group = name
        print(f"\n=== {name}: {title}")

    def ok(self, key: str, msg: str = "", quiet: bool = False):
        self.counts[self.group]["ok"] += 1
        if not quiet:
            print(f"  ok    {key}" + (f": {msg}" if msg else ""))

    def fail(self, key: str, msg: str):
        note = self.notes.get((self.group, key))
        if note is not None:
            self.used_notes.add((self.group, key))
            self.counts[self.group]["note"] += 1
            print(f"  NOTE  {key}: {msg}\n        -> {note}")
        else:
            self.counts[self.group]["fail"] += 1
            print(f"  FAIL  {key}: {msg}")

    def info(self, msg: str):
        print(f"  info  {msg}")

    def summary(self) -> int:
        print("\n=== summary")
        bad = 0
        for g, c in self.counts.items():
            print(f"  {g:10} {c['ok']:5} ok, {c['note']:3} explained, {c['fail']:3} failed")
            bad += c["fail"]
        for k in sorted(set(self.notes) - self.used_notes):
            if k[0] in self.counts and not (self.stock and k[0] != "source"):
                print(f"  (note not used: {k[0]},{k[1]})")
        print(f"verify_standalone: {'OK' if not bad else f'{bad} failed'}")
        return 1 if bad else 0


def read_l2_extras() -> dict[str, dict]:
    """tests/standalone/l2_extras.csv: the classes only NWindowL2 registers (phase 6), by C++ name:
    super, class_flags (int), feature."""
    out = {}
    for r in csv.DictReader(l for l in L2_EXTRAS.open(encoding="utf-8") if not l.startswith("#")):
        out[r["class"]] = dict(r, class_flags=int(r["class_flags"], 0))
    return out


def l2_ancestors(cpp: str, extras: dict[str, dict], classes: dict[str, dict]) -> list[str]:
    """An L2 extra's parents up to UObject, through the extras and then the stock classes."""
    out = []
    sup = extras[cpp]["super"]
    while sup in extras:
        out.append(sup)
        sup = extras[sup]["super"]
    if sup in classes:
        out += [sup] + list(classes[sup]["ancestors"])
    return out


def read_notes() -> dict[tuple[str, str], str]:
    if not NOTES.exists():
        return {}
    out = {}
    for r in csv.DictReader(l for l in NOTES.open(encoding="utf-8") if not l.startswith("#")):
        out[(r["check"], r["key"])] = r["explanation"]
    return out


# ---- the DLL under test -------------------------------------------------------------------------

class Target:
    """The DLL checked (ours, or the stock one for --stock) with its symbols."""

    def __init__(self, dll: Path, syms: Symbols | None, stock: bool):
        self.dll = dll
        self.img = Image(dll)
        self.syms = syms
        self.stock = stock

    def section_of(self, va: int):
        rva = va - self.img.base
        for s in self.img.pe.sections:
            if s.VirtualAddress <= rva < s.VirtualAddress + max(s.Misc_VirtualSize, s.SizeOfRawData):
                return s
        return None

    def executable(self, va: int) -> bool:
        s = self.section_of(va)
        return bool(s and s.Characteristics & 0x20000000)

    def writable(self, va: int) -> bool:
        s = self.section_of(va)
        return bool(s and s.Characteristics & 0x80000000)

    def import_thunk(self, va: int):
        """The import a `jmp dword ptr [IAT]` at va reaches (a linker import thunk)."""
        return thunk_import(self.img, va)

    def func_start(self, va: int) -> int:
        if self.syms:
            f = self.syms.func_containing(va)
            if f is not None and va - f < 0x10000:
                return f
        return func_start(self.img, va)

    def classes_at(self, va: int, what: str) -> list[str]:
        """Classes whose `what` (PrivateStaticClass, InternalConstructor...) sits at va."""
        out = []
        for dec, pretty in (self.syms.at(va) if self.syms else []):
            if pretty.endswith("::" + what):
                out.append(pretty[: -len(what) - 2])
        return out


# ---- pe -----------------------------------------------------------------------------------------

def check_pe(rep: Report, t: Target, stock: Image):
    rep.section("pe", "header, relocations, imports")
    oh, fh = t.img.pe.OPTIONAL_HEADER, t.img.pe.FILE_HEADER
    soh = stock.pe.OPTIONAL_HEADER
    (rep.ok if fh.Machine == 0x14C else rep.fail)("machine", f"{fh.Machine:#x} (i386 = 0x14c)")
    (rep.ok if fh.Characteristics & IMAGE_FILE_DLL else rep.fail)("dll", f"characteristics {fh.Characteristics:#x}")
    (rep.ok if oh.ImageBase == soh.ImageBase else rep.fail)(
        "imagebase", f"{oh.ImageBase:#x}, stock {soh.ImageBase:#x} (link /BASE:0x10000000)")
    dc = oh.DllCharacteristics
    names = [n for b, n in DLLCHAR_NAMES.items() if dc & b]
    if dc & DYNAMIC_BASE:
        rep.fail("aslr", f"DllCharacteristics {dc:#x} has DYNAMIC_BASE: link /DYNAMICBASE:NO (the stock has 0, "
                         "crash addresses stay comparable)")
    else:
        rep.ok("aslr", f"no DYNAMIC_BASE (DllCharacteristics {dc:#x} {' '.join(names)}; stock {soh.DllCharacteristics:#x})")
    if dc & 0x100:
        rep.info("NX_COMPAT set: ignored for a DLL (DEP is decided by L2.exe)")
    (rep.ok if oh.Subsystem == soh.Subsystem else rep.fail)("subsystem", f"{oh.Subsystem} (stock {soh.Subsystem}, 2 = GUI)")
    osv = (oh.MajorOperatingSystemVersion, oh.MinorOperatingSystemVersion)
    ssv = (oh.MajorSubsystemVersion, oh.MinorSubsystemVersion)
    stock_v = (soh.MajorSubsystemVersion, soh.MinorSubsystemVersion)
    if ssv > (6, 1):
        rep.fail("osversion", f"subsystem version {ssv[0]}.{ssv[1]}: a DLL above 6.1 does not load on Windows 7 "
                              "(link /SUBSYSTEM:WINDOWS,6.00)")
    else:
        rep.ok("osversion", f"OS {osv[0]}.{osv[1]}, subsystem {ssv[0]}.{ssv[1]} (stock {stock_v[0]}.{stock_v[1]}; "
                            "VS2022 goes no lower than 5.01)")
    relocs = oh.DATA_DIRECTORY[pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_BASERELOC"]].Size
    (rep.ok if relocs else rep.fail)("relocations", f"{relocs} bytes of base relocations (the DLL can be rebased)"
                                     if relocs else "none: the DLL cannot load if 0x10000000 is taken (no /FIXED)")
    (rep.ok if oh.AddressOfEntryPoint else rep.fail)(
        "entry", f"entry point {t.img.base + oh.AddressOfEntryPoint:#x}"
        + (f" {t.syms.pretty_at(t.img.base + oh.AddressOfEntryPoint)}" if t.syms else "")
        if oh.AddressOfEntryPoint else "no entry point: the static initializers (IMPLEMENT_CLASS) would never run")
    tls = oh.DATA_DIRECTORY[pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_TLS"]].Size
    if tls:
        rep.info("TLS directory present (thread-safe statics): fine on Vista and later for a LoadLibrary'd DLL")

    dlls = {e.dll.decode().lower(): e for e in t.img.pe.DIRECTORY_ENTRY_IMPORT}
    stock_dlls = {e.dll.decode().lower(): e for e in stock.pe.DIRECTORY_ENTRY_IMPORT}
    bad = [d for d in dlls if d.startswith(VC_RUNTIME)]
    (rep.ok if not bad else rep.fail)("crt", "static CRT (no VC++ runtime DLL)" if not bad else
                                      f"imports {bad}: the clients have no VC++ redist, build with /MT")
    (rep.ok if "nwindowdecomp.dll" not in dlls else rep.fail)(
        "hookdll", "does not import NWindowDecomp.dll" if "nwindowdecomp.dll" not in dlls else
        "imports NWindowDecomp.dll: that is the hook-mode build")
    for engine in ("core.dll", "engine.dll"):
        if engine not in dlls:
            rep.fail(f"imports:{engine}", "not imported")
            continue
        ref = pefile.PE(str(ROOT / "orig" / (engine[0].upper() + engine[1:])), fast_load=True)
        ref.parse_data_directories([pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_EXPORT"]])
        # pefile names only the first 8,192 exports (the C3 Engine.dll has 9,670): read the table by hand
        import engine_vtables
        have = {n for n, _ in engine_vtables.read_exports(ref)}
        names = [(i.name or b"").decode() for i in dlls[engine].imports]
        missing = [n for n in names if n not in have]
        if missing:
            rep.fail(f"imports:{engine}", f"{len(missing)} not exported by orig/{engine}: {missing[:5]}")
        else:
            n_stock = len(stock_dlls[engine].imports) if engine in stock_dlls else 0
            rep.ok(f"imports:{engine}", f"{len(names)} names, all exported by the original (stock imports {n_stock})")
    rep.info("imports: " + ", ".join(f"{e.dll.decode()} {len(e.imports)}" for e in dlls.values()))


# ---- imports ------------------------------------------------------------------------------------

def stock_import_users(stock: Image) -> dict[tuple[str, str], dict[str, set[str]]]:
    """Who in the stock image calls each import: (dll, name) -> {"game": functions, "nprot": functions, "crt": functions}.
    From a disassembly of every function of db/functions.csv; a `jmp [IAT]` linker thunk stands for its import and is
    followed to its callers; a reference that is only data (`mov r, [IAT]`) counts as a call."""
    rows = list(csv.DictReader((ROOT / "db" / "functions.csv").open(encoding="utf-8")))
    imports = stock.imports
    thunks: dict[int, int] = {}   # va of a `jmp dword ptr [slot]` stub -> slot
    for r in rows:
        va, size = int(r["va"], 16), int(r["size"])
        if size == 6 and r["kind"] == "thunk":
            b = stock.read(va, 6)
            slot = int.from_bytes(b[2:], "little")
            if b[:2] == b"\xff\x25" and slot in imports:
                thunks[va] = slot
    users: dict[tuple[str, str], dict[str, set[str]]] = defaultdict(lambda: defaultdict(set))
    imports = {slot: (dll.lower(), name) for slot, (dll, name) in imports.items()}
    md = Cs(CS_ARCH_X86, CS_MODE_32)
    for r in rows:
        va, size, kind = int(r["va"], 16), int(r["size"]), r["kind"]
        if kind in ("thunk", "fragment") or size <= 0:
            continue
        cat = "nprot" if NPROTECT_LIB[0] <= va < NPROTECT_LIB[1] else ("crt" if kind == "crt" else "game")
        who = r["name"] or f"sub_{va:08x}"
        for ins in md.disasm(stock.read(va, size), va):
            op = ins.op_str
            if ins.mnemonic in ("call", "jmp") and re.fullmatch(r"0x[0-9a-f]+", op):
                slot = thunks.get(int(op, 16))
                if slot is not None:
                    users[imports[slot]][cat].add(who)
                continue
            for m in re.findall(r"0x[0-9a-f]{7,8}", op):
                if int(m, 16) in imports:
                    users[imports[int(m, 16)]][cat].add(who)
    return users


def import_table(img: Image) -> dict[str, set[str]]:
    """DLL (lower case) -> imported names ("#n" for an ordinal)."""
    return {e.dll.decode().lower(): {(i.name or b"#%d" % i.ordinal).decode() for i in e.imports}
            for e in img.pe.DIRECTORY_ENTRY_IMPORT}


def check_imports(rep: Report, t: Target, stock: Image):
    rep.section("imports", "the import table against the stock's: libraries, what the game code needs, the link line")
    ours, theirs = import_table(t.img), import_table(stock)
    users = stock_import_users(stock)

    # the libraries: only the stock's, and none of the ones NKeyCrypt loads at run time
    bad = sorted(d for d in ours if d not in theirs)
    if bad:
        rep.fail("libraries", f"imports {bad}: the stock image does not (a standalone NWindow.dll needs only what the "
                              "stock one does)")
    else:
        rep.ok("libraries", f"{len(ours)} DLLs ({', '.join(sorted(ours))}), all imported by the stock too")
    for dll in sorted(RUNTIME_LOADED_DLLS):
        if t.stock:
            rep.info(f"{dll}: imported (the stock; NKeyCrypt loads it with LoadLibrary and GetProcAddress in ours)")
        elif dll in ours:
            rep.fail(dll, "imported: the standalone build resolves it at run time (src/nwindow/NKeyCrypt.cpp) so that "
                          "it loads where the driver package is not installed")
        else:
            rep.ok(dll, "not imported (NKeyCrypt: LoadLibrary + GetProcAddress by ordinal, silent without it)")

    # what the stock's game code calls must be imported
    link_libs = []
    for dll in sorted(theirs):
        if dll in RUNTIME_LOADED_DLLS:
            continue
        need = {n for n in theirs[dll] if users.get((dll, n), {}).get("game")}
        lib_only = sorted(theirs[dll] - need)
        have = ours.get(dll, set())
        missing = sorted(need - have)
        for name in missing:
            who = sorted(users[(dll, name)]["game"])
            rep.fail(f"{dll}!{name}", "the stock's game code calls it (" + ", ".join(who[:3])
                     + (f" and {len(who) - 3} more" if len(who) > 3 else "") + ") and the standalone does not import it")
        if need:
            link_libs.append(dll)
        extra = sorted(have - theirs[dll])
        if not missing:
            rep.ok(dll, f"stock {len(theirs[dll])} ({len(need)} used by game code), ours {len(have)}: every name the game "
                        f"code needs; the other {len(lib_only)} are only the nProtect library's or the C runtime's"
                        + (f"; {len(extra)} names only ours (the new C runtime)" if extra and dll == "kernel32.dll"
                           else (f"; names only ours: {', '.join(extra[:6])}" if extra else "")))

    # the link line of the standalone target provides a library for each DLL the game code needs
    cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8", errors="replace")
    m = re.search(r"function\(nwd_standalone_dll_of.*?target_link_libraries\(\$\{target\} PRIVATE ([^)]*)\)", cmake, re.S)
    if not m:
        rep.fail("cmake", "no target_link_libraries of nwd_standalone_dll_of in CMakeLists.txt")
        return
    line = [w.lower() for w in m.group(1).split()]
    lacking = []
    for dll in link_libs:
        lib = IMPORT_LIBS.get(dll)
        if dll not in IMPORT_LIBS:
            lacking.append(f"{dll} (no library known: add it to IMPORT_LIBS)")
        elif lib is not None and lib.lower() not in line and f"${{{lib.lower()}}}" not in line:
            lacking.append(f"{dll} ({lib})")
    if lacking:
        rep.fail("cmake", "the link line of the standalone target lacks " + ", ".join(lacking))
    else:
        rep.ok("cmake", "link line: " + " ".join(m.group(1).split()))
    for dll in sorted(theirs):
        if dll in RUNTIME_LOADED_DLLS or dll in link_libs or IMPORT_LIBS.get(dll) is None:
            continue
        rep.info(f"{dll}: nothing the game code calls (only the nProtect library or the C runtime), "
                 + ("so the standalone does not import it" if dll not in ours else "yet ours imports it")
                 + ("; its library is on the link line, unused" if IMPORT_LIBS.get(dll, "").lower() in line else ""))


# ---- resources ----------------------------------------------------------------------------------

def group_cursors(img: Image) -> dict[int, list]:
    """RT_GROUP_CURSOR id -> [(language, [(entry header, cursor image bytes)])]."""
    pe = img.pe
    if not hasattr(pe, "DIRECTORY_ENTRY_RESOURCE"):
        return {}
    by_type = {e.id: e for e in pe.DIRECTORY_ENTRY_RESOURCE.entries}

    def data(entry):
        return pe.get_data(entry.data.struct.OffsetToData, entry.data.struct.Size)

    cursors = {}
    if 1 in by_type:  # RT_CURSOR
        for e in by_type[1].directory.entries:
            cursors[e.id] = data(e.directory.entries[0])
    out = {}
    if 12 in by_type:  # RT_GROUP_CURSOR
        for e in by_type[12].directory.entries:
            for lang in e.directory.entries:
                d = data(lang)
                count = struct.unpack_from("<H", d, 4)[0]
                items = []
                for k in range(count):
                    ent = d[6 + 14 * k: 6 + 14 * (k + 1)]
                    cid = struct.unpack_from("<H", ent, 12)[0]
                    items.append((ent[:12], cursors.get(cid)))
                out.setdefault(e.id, []).append((lang.id, items))
    return out


def check_resources(rep: Report, t: Target, stock: Image):
    rep.section("resources", "the colour cursors (NCVirtualWndMain::LoadCursors)")
    ref, ours = group_cursors(stock), group_cursors(t.img)
    for gid, ((lang, items),) in sorted(ref.items()):
        if gid not in ours:
            rep.fail(f"cursor {gid}", "RT_GROUP_CURSOR missing")
            continue
        olang, oitems = ours[gid][0]
        if [(h, d) for h, d in oitems] != [(h, d) for h, d in items]:
            rep.fail(f"cursor {gid}", "cursor header or image bytes differ from the stock")
        else:
            rep.ok(f"cursor {gid}", f"{len(items)} image(s), {sum(len(d or b'') for _, d in items)} bytes"
                   + (f"; language {olang}, stock {lang} (LoadCursor finds either)" if olang != lang else ""), quiet=True)
    extra = sorted(set(ours) - set(ref))
    if extra:
        rep.info(f"cursor groups the stock does not have: {extra}")
    n = sum(1 for g in ref if g in ours)
    rep.info(f"{n}/{len(ref)} cursor groups present")


# ---- exports ------------------------------------------------------------------------------------

def export_table(img: Image) -> tuple[str, list[tuple[int, str, int, object]]]:
    e = img.pe.DIRECTORY_ENTRY_EXPORT
    name = img.pe.get_string_at_rva(e.struct.Name).decode("latin-1")
    return name, [(s.ordinal, (s.name or b"").decode(), img.base + s.address, s.forwarder) for s in e.symbols]


def check_exports(rep: Report, t: Target, stock: Image, classes: dict[str, dict],
                  extras: dict[str, dict] | None = None):
    rep.section("exports", "the names the engine looks up (GetProcAddress): 180 in C1, 227 in C2, 343 in C4"
                + (f", + the autoclass of the {len(extras)} L2 extras" if extras else ""))
    if not hasattr(t.img.pe, "DIRECTORY_ENTRY_EXPORT"):
        rep.fail("table", "no export table")
        return
    sname, sexp = export_table(stock)
    oname, oexp = export_table(t.img)
    # (the /FORCE:UNRESOLVED probe of tools/standalone_report.py is linked under another name)
    (rep.ok if oname.lower() in (sname.lower(), "nwindow_probe.dll") else rep.fail)("dllname", f"{oname} (stock {sname})")
    want = {n: o for o, n, _, _ in sexp}
    have = {n: o for o, n, _, _ in oexp}
    if extras:
        # the L2 extras' autoclass: all there, past the stock ordinals; the rest must be the stock's
        l2 = {f"autoclass{c}" for c in extras}
        missing = sorted(l2 - set(have))
        low = sorted(n for n in l2 & set(have) if have[n] <= max(want.values()))
        if missing or low:
            rep.fail("l2", f"L2 extras not exported {missing}, among the stock ordinals {low}")
        else:
            rep.ok("l2", f"{len(l2)} autoclass of the L2 extras, ordinals "
                         f"{min(have[n] for n in l2)}..{max(have[n] for n in l2)} after the stock ones")
        have = {n: o for n, o in have.items() if n not in l2}
    if set(want) != set(have):
        missing, extra = sorted(set(want) - set(have)), sorted(set(have) - set(want))
        rep.fail("names", f"{len(missing)} missing {missing[:6]}, {len(extra)} extra {extra[:6]}")
    else:
        rep.ok("names", f"the stock's {len(want)} names")
        moved = sorted((want[n], n, have[n]) for n in want if want[n] != have[n])
        if moved:
            rep.fail("ordinals", f"{len(moved)} names with another ordinal: "
                                 + ", ".join(f"{n} {o} (stock {so})" for so, n, o in moved[:6])
                                 + " (__declspec(dllexport) without a .def gives the stock ones)")
        else:
            rep.ok("ordinals", "every name at the stock ordinal (1 _DllMain@12, 2 GPackage, 3.. autoclass)")
    fwd = [n for _, n, _, f in oexp if f]
    if fwd:
        rep.fail("forwarders", f"{fwd}")
    by_name = {n: va for _, n, va, _ in oexp}

    va = by_name.get("_DllMain@12")
    if va is None:
        rep.fail("_DllMain@12", "not exported")
    elif not t.executable(va):
        rep.fail("_DllMain@12", f"{va:#x} is not code")
    else:
        rep.ok("_DllMain@12", f"{va:#x} code" + (f" {t.syms.pretty_at(va)}" if t.syms else ""))
    va = by_name.get("GPackage")
    if va is None:
        rep.fail("GPackage", "not exported")
    else:
        s = t.img.wstr(va, 64)
        (rep.ok if s == "NWindow" else rep.fail)("GPackage", f"{va:#x} = L\"{s}\" (Core names the package by it)")

    bad = 0
    for cpp in sorted(classes) + sorted(extras or {}):
        key = f"autoclass{cpp}"
        va = by_name.get(key)
        if va is None:
            bad += 1
            rep.fail(key, "not exported")
            continue
        if not t.writable(va):
            bad += 1
            rep.fail(key, f"{va:#x} is not in a writable data section")
            continue
        psc = psc_of(t, cpp)
        v = t.img.u32(va)
        if v:
            if va not in t.img.reloc_set:
                bad += 1
                rep.fail(key, f"holds {v:#x} without a relocation (wrong once rebased)")
            elif psc is not None and v != psc:
                bad += 1
                rep.fail(key, f"holds {v:#x}, {cpp}'s UClass is at {psc:#x} {t.syms.pretty_at(v) if t.syms else ''}")
            else:
                rep.ok(key, f"= {v:#x} in the file", quiet=True)
        else:
            # set at startup, like the stock's 11-byte initializers: mov dword ptr [va], psc
            setters = find_store(t.img, va, psc)
            if not setters:
                bad += 1
                rep.fail(key, f"0 in the file and no `mov [{va:#x}], {psc and hex(psc)}` in the code")
            else:
                rep.ok(key, f"set by the initializer at {setters[0]:#x}", quiet=True)
    if not bad:
        rep.ok("autoclass", f"{len(classes) + len(extras or {})} autoclass exports hold (or get) their "
                            "class's UClass")


def psc_of(t: Target, cpp: str) -> int | None:
    """Address of cpp::PrivateStaticClass (public or private member)."""
    if not t.syms:
        return None
    for mangled in (f"?PrivateStaticClass@{cpp}@@0VUClass@@A", f"?PrivateStaticClass@{cpp}@@2VUClass@@A"):
        a = t.syms.find(mangled)
        if a is not None:
            return a
    return t.syms.find(f"{cpp}::PrivateStaticClass")


def find_store(img: Image, va: int, value: int | None) -> list[int]:
    """Code addresses of `mov dword ptr [va], imm` (C7 05 va imm), imm == value when given."""
    lo, hi = img.sect_range(".text")
    code = img.read(lo, hi - lo)
    pat = b"\xc7\x05" + va.to_bytes(4, "little")
    out = []
    i = code.find(pat)
    while i >= 0:
        if value is None or int.from_bytes(code[i + 6:i + 10], "little") == value:
            out.append(lo + i)
        i = code.find(pat, i + 1)
    return out


# ---- classes ------------------------------------------------------------------------------------

def ctor_call_sites(img: Image) -> list[int]:
    """Calls of Core's UClass native constructor: `call [IAT]`, or `call thunk` to a `jmp [IAT]`."""
    try:
        slot = img.import_slot(UCLASS_CTOR)
    except KeyError:
        return []
    lo, hi = img.sect_range(".text")
    code = img.read(lo, hi - lo)
    sites = []
    pat = b"\xff\x15" + slot.to_bytes(4, "little")
    i = code.find(pat)
    while i >= 0:
        sites.append(lo + i)
        i = code.find(pat, i + 1)
    thunks = set()
    pat = b"\xff\x25" + slot.to_bytes(4, "little")
    i = code.find(pat)
    while i >= 0:
        thunks.add(lo + i)
        i = code.find(pat, i + 1)
    if thunks:
        for off in range(len(code) - 5):
            if code[off] == 0xE8:
                target = lo + off + 5 + int.from_bytes(code[off + 1:off + 5], "little", signed=True)
                if target in thunks:
                    sites.append(lo + off)
    return sorted(sites)


def l2_record(cpp: str, extras: dict[str, dict], classes: dict[str, dict]) -> dict:
    """What an L2 extra's IMPLEMENT_CLASS must pass: its list row, the rest as every stock class
    (object flags, within, GUID, config, static constructor); no stock size."""
    any_stock = next(iter(classes.values()))
    e = extras[cpp]
    return {"cpp": cpp, "name": cpp[1:], "name_literal": cpp, "size": None, "class_flags": e["class_flags"],
            "flags": any_stock["flags"], "super": e["super"], "within": "UObject", "guid": [0, 0, 0, 0],
            "config": any_stock["config"], "static_ctor_raw": any_stock["static_ctor_raw"],
            "internal_ctor": None, "l2": True}


def check_classes(rep: Report, t: Target, classes: dict[str, dict], stock_order: list[str],
                  extras: dict[str, dict] | None = None):
    rep.section("classes", "the UClass registrations (IMPLEMENT_CLASS)"
                + (f", + the {len(extras)} L2 extras" if extras else ""))
    img = t.img
    sites = ctor_call_sites(img)
    if not sites:
        rep.fail("ctor", f"no call to Core's {UCLASS_CTOR}")
        return
    gpackage = img.exports.get("GPackage")
    # stock InternalConstructor bodies the linker folded: any class of the group is fine
    folded = defaultdict(set)
    for r in classes.values():
        folded[r["internal_ctor"]].add(r["cpp"])
    seen: dict[str, int] = {}
    order: list[tuple[int, str]] = []
    n_ok = 0
    for site in sites:
        start = t.func_start(site)
        regs, args = emulate_to(img, start, site)
        d = dict(zip(PARAMS, args))
        this = regs.get("ecx")
        owners = t.classes_at(this.v, "PrivateStaticClass") if this is not None and isinstance(this.v, int) else []
        if len(owners) != 1:
            rep.fail(f"site {site:#x}", f"this = {this!r}, not one class's PrivateStaticClass {owners}")
            continue
        cpp = owners[0]
        if cpp in seen:
            rep.fail(cpp, f"registered twice ({seen[cpp]:#x}, {site:#x})")
            continue
        seen[cpp] = site
        order.append((initializer_slot(t, start), cpp))
        r = classes.get(cpp)
        if r is None and extras and cpp in extras:
            r = l2_record(cpp, extras, classes)
        if r is None:
            rep.fail(cpp, "registered, but the stock has no such class"
                          + (" and it is not an L2 extra" if extras is not None else ""))
            continue
        bad = []

        def expect(field, got, want):
            if got != want:
                show = (lambda v: hex(v) if isinstance(v, int) else v)
                bad.append(f"{field} {show(got)} (stock {show(want)})")

        name_ptr = d["Name"].v
        if isinstance(name_ptr, int) and t.section_of(name_ptr - 2) is not None:
            expect("name", img.wstr(name_ptr, 128), r["name"])
            expect("literal", img.wstr(name_ptr - 2, 128), r["name_literal"])
        else:
            bad.append(f"name {d['Name']!r} (stock TEXT(\"{r['name_literal']}\")+1)")
        if r["size"] is not None:
            expect("size", d["Size"].v, r["size"])
        expect("class_flags", d["ClassFlags"].v, r["class_flags"])
        expect("flags", d["Flags"].v, r["flags"])
        expect("super", class_ref(t, d["Super"]), r["super"])
        expect("within", class_ref(t, d["Within"]), r["within"])
        expect("guid", [d[f"Guid{k}"].v for k in range(4)], r["guid"])
        expect("package", d["Package"].v, gpackage)
        expect("config", config_expr(img, d["Config"]), r["config"])
        ic = d["InternalConstructor"].v
        if isinstance(ic, int) and r.get("l2"):
            # its own; an abstract one registers its parent's (NW_DECLARE_ABSTRACT_CLASS)
            got = t.classes_at(ic, "InternalConstructor")
            ok = cpp in got or (r["class_flags"] & 1 and set(got) & set(l2_ancestors(cpp, extras, classes)))
            if not ok:
                bad.append(f"InternalConstructor {ic:#x} is {got or t.syms.pretty_at(ic) if t.syms else '?'}")
        elif isinstance(ic, int):
            got = t.classes_at(ic, "InternalConstructor")
            if cpp not in got and not (set(got) & folded[r["internal_ctor"]]):
                bad.append(f"InternalConstructor {ic:#x} is {got or t.syms.pretty_at(ic) if t.syms else '?'}")
        else:
            bad.append(f"InternalConstructor {d['InternalConstructor']!r}")
        expect("StaticConstructor", repr(d["StaticConstructor"]), r["static_ctor_raw"])
        if bad:
            rep.fail(cpp, "; ".join(bad))
        elif r.get("l2"):
            rep.ok(cpp, f"L2 extra: super {r['super']}, class flags {r['class_flags']:#x}, size "
                        f"{d['Size'].v:#x}" if isinstance(d["Size"].v, int) else "L2 extra")
        else:
            n_ok += 1
            rep.ok(cpp, quiet=True)
    missing = sorted((set(classes) | set(extras or {})) - set(seen))
    for cpp in missing:
        rep.fail(cpp, "never registered (no UClass native constructor call for it)")
    print(f"  {n_ok}/{len(classes)} classes registered exactly like the stock ({len(sites)} constructor calls"
          + (f", {len(set(extras) & set(seen))} of them the L2 extras" if extras else "") + ")")
    # the order the initializers run in: the CRT table order (stock: the .obj link order); the L2
    # extras are registered after the stock classes and are not in it
    stock_set = set(stock_order)
    ours = [c for _, c in sorted(o for o in order if o[0] is not None) if c in stock_set]
    if len(ours) != len([o for o in order if o[1] in stock_set]):
        rep.info("order: some initializers not found in the CRT table (not compared)")
    elif ours == [c for c in stock_order if c in seen]:
        rep.ok("order", "the static initializers run in the stock order")
    else:
        k = next(i for i, (a, b) in enumerate(zip(ours, stock_order)) if a != b)
        rep.fail("order", f"initializer {k} registers {ours[k]}, the stock's registers {stock_order[k]} "
                          "(one file with the IMPLEMENT_CLASS in the stock order reproduces it)")


def class_ref(t: Target, v) -> str:
    """The class a UClass* argument names: our PrivateStaticClass (a symbol), an imported one
    (&?PrivateStaticClass@UObject@@...) or the result of an imported StaticClass() call."""
    if isinstance(v.v, int):
        return ",".join(t.classes_at(v.v, "PrivateStaticClass")) or repr(v)
    m = re.fullmatch(r"&\?PrivateStaticClass@(\w+)@@[02]VUClass@@A|ret:\?StaticClass@(\w+)@@SAPAVUClass@@XZ", repr(v))
    return (m.group(1) or m.group(2)) if m else repr(v)


def initializer_slot(t: Target, func: int) -> int | None:
    """Where the CRT initializer table points at func (its place in the run order)."""
    if not hasattr(t, "_xc"):
        t._xc = {}
        for s in t.img.pe.sections:
            if not s.Name.startswith((b".rdata", b".data", b".CRT")):
                continue
            base = t.img.base + s.VirtualAddress
            data = s.get_data()
            for off in range(0, len(data) - 3, 4):
                va = base + off
                if va in t.img.reloc_set:
                    t._xc.setdefault(int.from_bytes(data[off:off + 4], "little"), va)
    return t._xc.get(func)


# ---- vtables ------------------------------------------------------------------------------------

def stock_code_model(stock: Image):
    """verify_hooks' view of the stock code: Ghidra function ranges for the ret comparison."""
    from verify_hooks import Code
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
    lo, hi = stock.sect_range(".text")
    return Code(stock.read, {va: n for va, (_, n) in stock.imports.items()},
                lambda va: [(a, b + 1) for a, b in granges.get(va, [(va, va + 0x3F)])],
                lambda va: lo <= va < hi)


def target_code_model(t: Target, stock_model):
    from verify_hooks import Code
    if t.stock:
        return stock_model
    lo, hi = t.img.sect_range(".text")
    return Code(t.img.read, {va: n for va, (_, n) in t.img.imports.items()},
                lambda va: [(va, t.syms.func_end(va, hi))],
                lambda va: lo <= va < hi)


def source_vtable_checks() -> dict[str, list[tuple[str | None, int, int]]]:
    """NW_CHECK_VTABLE(Class, StockVA, Slots) / NW_CHECK_VTABLE_FOR(Class, Base, ...) in our source."""
    out: dict[str, list] = {}
    rx = re.compile(r"NW_CHECK_VTABLE(?:_FOR)?\(\s*(\w+)\s*,\s*(?:(\w+)\s*,\s*)?(0x[0-9a-fA-F]+)\s*,\s*(\d+)\s*\)")
    for p in sorted(SRC_CPP + SRC_H):
        # a line comment is not a check ("// C2: no vtable for NW_CHECK_VTABLE(NHtmlParser, ...)")
        text = re.sub(r"^[ \t]*//.*$", "", p.read_text(encoding="utf-8", errors="replace"), flags=re.M)
        for m in rx.finditer(text):
            out.setdefault(m.group(1), []).append((m.group(2), int(m.group(3), 16), int(m.group(4))))
    return out


def vtable_candidates(t: Target, cpp: str) -> dict[str, int]:
    """Our vtables of cpp: {"" (single inheritance) or base name: address}."""
    out = {}
    for name, addr in t.syms.with_prefix(f"??_7{cpp}@@6B"):
        rest = name[len(f"??_7{cpp}@@6B"):]
        out["" if rest == "@" else rest.rstrip("@")] = addr
    return out


def our_slots(t: Target, vt: int, limit: int) -> list[int]:
    """Consecutive relocated code pointers from vt, up to the next symbol (the next vtable)."""
    end = t.syms.next_symbol(vt) if t.syms else None
    out = []
    p = vt
    while (end is None or p < end) and p in t.img.reloc_set and len(out) < limit:
        v = t.img.u32(p)
        if not t.executable(v):
            break
        out.append(v)
        p += 4
    return out


def check_vtables(rep: Report, t: Target, stock: Image, classes: dict[str, dict], all_classes: set[str],
                  only: list[str]):
    rep.section("vtables", "slot by slot against the stock (names: db/functions.csv)")
    md = Cs(CS_ARCH_X86, CS_MODE_32)
    from verify_hooks import features
    db = {int(r["va"], 16): r for r in csv.DictReader((ROOT / "db" / "functions.csv").open(encoding="utf-8"))}
    stock_model = stock_code_model(stock)
    our_model = target_code_model(t, stock_model)
    todo: list[tuple[str, str, int, list[dict]]] = []  # (key, cpp, our vt symbol base, stock slots)
    for cpp, r in sorted(classes.items()):
        todo.append((cpp, cpp, "UObject" if r.get("secondary_vtables") else "", r.get("vtable_slots", [])))
        for sv in r.get("secondary_vtables", []):
            todo.append((f"{cpp}/FExec", cpp, "FExec", sv["vtable_slots"]))
    for cpp, checks in sorted(source_vtable_checks().items()):
        if cpp in all_classes or (only and cpp not in only):
            continue
        for base, va, n in checks:
            slots = [{"slot": k, "va": stock.u32(va + 4 * k), "import": thunk_import(stock, stock.u32(va + 4 * k))}
                     for k in range(n)]
            todo.append((cpp, cpp, base or "", slots))
    # a stock body serving several slot numbers (a linker-folded default, e.g. `return 0`) has
    # one name in the database for all of them; the same body at the same slot in the vtables of
    # a class and its children is plain inheritance
    slot_numbers = defaultdict(set)
    for _, _, _, slots in todo:
        for s in slots:
            slot_numbers[s["va"]].add(s["slot"])
    ancestors = {r["cpp"]: r["ancestors"] for r in classes.values()}
    n_ok = n_slots = n_stub_classes = 0
    kinds = Counter()
    for key, cpp, base, slots in todo:
        cands = vtable_candidates(t, cpp)
        vt = cands.get(base)
        if vt is None:
            rep.fail(key, f"no vtable symbol ??_7{cpp}@@6B{base + '@@' if base else ''}@ in the map "
                          f"(found {sorted(cands) or 'none'})")
            continue
        ours = our_slots(t, vt, 4096)
        bad = []
        stubs = []   # slots whose body is an int3 stub: a NW_STOCK / NW_UNPORTED placeholder nobody replaced yet
        if len(ours) != len(slots):
            rep.fail(f"{key}:count", f"{len(ours)} slots at {vt:#x}, the stock vtable has {len(slots)}"
                                     " (the common ones are compared)")
        for s, ova in zip(slots, ours):
            k, sva = s["slot"], s["va"]
            s_imp = s["import"]
            o_imp = t.import_thunk(ova)
            if not o_imp and t.img.read(ova, 1) == b"\xcc":
                # (the stock never starts a function with int3; the linker folds all such stubs into one body,
                # so the name the map gives it, and its `ret`, say nothing)
                stubs.append(f"{k} {s_imp or db.get(sva, {}).get('name', f'sub_{sva:08x}')}")
                kinds["int3 stub"] += 1
                continue
            onames = t.syms.pretty_at(ova)
            if s_imp or o_imp:
                if s_imp != o_imp:
                    bad.append(f"slot {k}: stock {s_imp or db.get(sva, {}).get('name')} ours {o_imp or onames}")
                kinds["import"] += 1
                continue
            if sva == STOCK_PURECALL:
                if not any("purecall" in n for n in onames):
                    bad.append(f"slot {k}: stock _purecall ours {onames}")
                kinds["_purecall"] += 1
                continue
            if not onames:
                bad.append(f"slot {k}: points to {ova:#x}, no symbol there")
                continue
            row = db.get(sva, {})
            sname = row.get("name", f"sub_{sva:08x}")
            sr = features(md, stock_model, sva)[0]
            orr = features(md, our_model, ova)[0]
            if sr != orr:
                # a function whose exits are all tail jumps (`jmp [vtable slot]`, a shared epilogue) has no `ret` of
                # its own, so nothing to compare: when the names agree the slot holds the right function (the hook
                # judges of tools/port/checks.py compare its body with the stock one)
                if (not sr or not orr) and sname in onames:
                    kinds["same name, one side ends in a tail jump (ret not comparable)"] += 1
                    continue
                bad.append(f"slot {k}: ret stock {sorted(sr)} ({sname}) ours {sorted(orr)} ({onames[0]})")
                continue
            # deleting destructors: VC7.0 put the scalar one in the slot, the current compiler
            # the vector one; called with flag 1 both run ~T() + operator delete
            if "deleting destructor" in sname:
                cls = sname.split("::")[0]
                if not any(n.startswith(cls + "::") and "deleting destructor" in n for n in onames):
                    bad.append(f"slot {k}: stock {sname} ours {onames}")
                kinds["deleting destructor"] += 1
                continue
            method = sname.split("::")[-1]
            # a stock body several slots share (linker-folded defaults, vfnNNN names) has one
            # name only: the ret comparison is what can be said
            generic = (method.startswith(("vfn", "sub_")) or len(slot_numbers[sva]) > 1
                       or row.get("notes", "").count("vslot") > 1)
            if sname in onames:
                kinds["function name"] += 1
                continue
            if generic:
                kinds["body the stock shares between slots (ret only)"] += 1
                continue
            # the stock vtable of cpp points to a body named after a class that is neither cpp
            # nor an ancestor: a class only inherits its ancestors' bodies, so that body is also
            # cpp's own (or an ancestor's) override, folded by the linker with the other class's.
            # Ours must then be that override: the same method of cpp or of an ancestor.
            owner = sname.split("::")[0]
            family = [cpp] + list(ancestors.get(cpp, []))
            if owner not in family and any(n.split("::")[-1] == method and n.split("::")[0] in family
                                           for n in onames):
                kinds["method name, the stock linker folded two classes' overrides"] += 1
                continue
            bad.append(f"slot {k}: stock {sname} ours {onames}")
        if stubs:
            n_stub_classes += 1
            rep.fail(f"{key}:stubs", f"{len(stubs)} slot(s) point to an int3 stub (stock body in brackets): "
                                     + "; ".join(stubs))
        if bad:
            cap = 10**6 if os.environ.get("NW_VERIFY_ALL") else 30   # NW_VERIFY_ALL=1: every differing slot, not the first 30
            rep.fail(key, "; ".join(bad[:cap]) + (f"; ... {len(bad) - cap} more" if len(bad) > cap else ""))
        if not bad and not stubs:
            n_ok += 1
            rep.ok(key, f"{len(ours)} slots", quiet=True)
        n_slots += min(len(ours), len(slots))
    print(f"  {n_ok}/{len(todo)} vtables with every common slot as the stock; {n_slots} slots matched by "
          + ", ".join(f"{k} {v}" for k, v in kinds.most_common()) + " (code slots: same ret too)")
    if kinds["int3 stub"]:
        print(f"  {kinds['int3 stub']} slots in {n_stub_classes} vtables still point to an int3 stub (NW_STOCK / "
              "NW_UNPORTED placeholder): each is reported on its own `<Class>:stubs` line, apart from the other differences")


# ---- source -------------------------------------------------------------------------------------

def check_source(rep: Report, classes: dict[str, dict]):
    if not SRC_H:
        rep.section("source", "no game source: skipped (--no-game-src)")
        return
    rep.section("source", "NW_DECLARE_CLASS and NW_CHECK_VTABLE against analysis/classes.json")
    flags = {"0": 0, "CLASS_Abstract": 1, "CLASS_Config": 4, "CLASS_Abstract | CLASS_Config": 5}
    # NW_DECLARE_ABSTRACT_CLASS adds CLASS_Abstract to the flags it is given (NWindow.h)
    rx = re.compile(r"NW_DECLARE_(ABSTRACT_)?CLASS\(\s*(\w+)\s*,\s*(\w+)\s*,\s*([^,)]+?)\s*(?:,\s*(0x[0-9a-fA-F]+)\s*)?\)")
    # the macros' own #define lines (and their continuations) are not declarations
    define_rx = re.compile(r"^[ \t]*#[ \t]*define\b(?:.*\\\n)*.*$", re.M)
    decl = {}
    for p in sorted(SRC_H):
        # a whole-line comment is not a declaration ("// C3: class NCTVItem gone (no counterpart):  NW_DECLARE_CLASS(NCTVItem, ...)")
        text = re.sub(r"^[ \t]*//.*$", "", p.read_text(encoding="utf-8", errors="replace"), flags=re.M)
        text = define_rx.sub("", text)
        for m in rx.finditer(text):
            decl[m.group(2)] = (m.group(3), m.group(4), int(m.group(5), 16) if m.group(5) else None,
                                p.relative_to(ROOT).as_posix(), bool(m.group(1)))
    bad = 0
    for cpp, r in sorted(classes.items()):
        if cpp not in decl:
            rep.fail(cpp, "no NW_DECLARE_CLASS")
            bad += 1
            continue
        sup, fl, va, path, abstract = decl[cpp]
        f = flags.get(fl)
        if f is not None and abstract:
            f |= 1
        if f != r["class_flags"]:
            rep.fail(f"{cpp}:flags", f"{fl} in {path}, the stock registers {r['class_flags']:#x}")
            bad += 1
        if va is not None and va != r["static_class"]:
            rep.fail(f"{cpp}:address", f"{va:#x} in {path}, stock UClass at {r['static_class']:#x}")
            bad += 1
        if sup != r["super"]:
            rep.fail(f"{cpp}:super", f"C++ base {sup} in {path}, the stock registers the UClass with super {r['super']}")
            bad += 1
    for cpp in sorted(set(decl) - set(classes)):
        rep.fail(cpp, "NW_DECLARE_CLASS of a class the stock does not register")
        bad += 1
    if not bad:
        rep.ok("NW_DECLARE_CLASS", f"{len(decl)} declarations: flags, super and UClass address as the stock")
    else:
        rep.ok("NW_DECLARE_CLASS", f"{len(decl)} declarations, the rest as the stock", quiet=True)
    checks = source_vtable_checks()
    n = 0
    for cpp, lst in sorted(checks.items()):
        for base, va, slots in lst:
            r = classes.get(cpp)
            if r is None:
                n += 1
                continue  # the non-UObject vtables: no classes.json entry
            if va != r["vtable"] or slots != len(r.get("vtable_slots", [])):
                rep.fail(f"{cpp}:vtable", f"NW_CHECK_VTABLE {va:#x} {slots} slots, the stock vtable "
                                          f"{r['vtable']:#x} has {len(r.get('vtable_slots', []))}")
            else:
                n += 1
    missing = sorted(set(classes) - set(checks))
    for cpp in missing:
        rep.fail(f"{cpp}:vtable", "no NW_CHECK_VTABLE")
    rep.ok("NW_CHECK_VTABLE", f"{n} of {sum(len(v) for v in checks.values())} checks as the stock vtables")
    check_source_l2(rep, classes, define_rx, flags)


def check_source_l2(rep: Report, classes: dict[str, dict], define_rx, flags: dict[str, int]):
    """The phase 6 classes (NwL2Class.h): declared only in include/features with
    NW_DECLARE_L2_(ABSTRACT_)CLASS, registered once in src/features with NW_IMPLEMENT_L2_CLASS,
    super and class flags as tests/standalone/l2_extras.csv - which lists exactly those."""
    extras = read_l2_extras() if L2_EXTRAS.exists() else {}
    decl_rx = re.compile(r"NW_DECLARE_L2_(ABSTRACT_)?CLASS\(\s*(\w+)\s*,\s*(\w+)\s*,\s*([^,)]+?)\s*\)")
    impl_rx = re.compile(r"NW_IMPLEMENT_L2_CLASS\(\s*(\w+)\s*,\s*(\w+)\s*,\s*([^,)]+?)\s*\)")
    impl_flags = {"0": 0, "CLASS_Abstract": 1, "CLASS_Config": 4}
    decl, impl = {}, defaultdict(list)
    for p in sorted((ROOT / "include").rglob("*.h")):
        text = define_rx.sub("", p.read_text(encoding="utf-8", errors="replace"))
        for m in decl_rx.finditer(text):
            decl[m.group(2)] = (m.group(3), m.group(4), bool(m.group(1)), p.relative_to(ROOT).as_posix())
    for p in sorted((ROOT / "src").rglob("*.cpp")):
        text = define_rx.sub("", p.read_text(encoding="utf-8", errors="replace"))
        for m in impl_rx.finditer(text):
            impl[m.group(1)].append((m.group(2), m.group(3), p.relative_to(ROOT).as_posix()))
    bad = 0
    for cpp in sorted(set(decl) | set(impl) | set(extras)):
        e = extras.get(cpp)
        problems = []
        if cpp in classes:
            problems.append("a stock class: NW_DECLARE_CLASS, not the L2 macros")
        if e is None:
            problems.append("not in tests/standalone/l2_extras.csv")
        if cpp not in decl:
            problems.append("no NW_DECLARE_L2_CLASS")
        else:
            sup, fl, abstract, path = decl[cpp]
            f = flags.get(fl)
            if f is not None and abstract:
                f |= 1
            if not path.startswith("include/features/"):
                problems.append(f"declared in {path}, outside include/features (the faithful build would see it)")
            if e is not None and (sup != e["super"] or f != e["class_flags"]):
                problems.append(f"declared super {sup} flags {fl}{' abstract' if abstract else ''}, the list has "
                                f"super {e['super']} class_flags {e['class_flags']:#x}")
        if len(impl.get(cpp, [])) != 1:
            problems.append(f"{len(impl.get(cpp, []))} NW_IMPLEMENT_L2_CLASS (one wanted)")
        else:
            sup, fl, path = impl[cpp][0]
            if not path.startswith("src/features/"):
                problems.append(f"registered in {path}, outside src/features")
            if e is not None and (sup != e["super"] or impl_flags.get(fl) != e["class_flags"]):
                problems.append(f"registered with super {sup} flags {fl}, the list has super {e['super']} "
                                f"class_flags {e['class_flags']:#x}")
        if problems:
            bad += 1
            rep.fail(f"{cpp}:l2", "; ".join(problems))
    if not bad:
        rep.ok("NW_DECLARE_L2_CLASS", f"{len(extras)} L2 extras (tests/standalone/l2_extras.csv): declared in "
                                      "include/features, registered once in src/features, as listed")


# ---- fixture ------------------------------------------------------------------------------------
# A stand-in NWindow.dll built by the current compiler from analysis/classes.json: 178 empty
# classes of the stock sizes, UE2's IMPLEMENT_CLASS and IMPLEMENT_PACKAGE, the stock cursors
# embedded byte for byte. It has what the checks pe, resources, exports and classes read, made
# the way the real standalone build makes it, so the checker is tested on VS2022 code before
# the real NWindow.dll links (the vtables check is tested on the hook-mode DLL instead).

FIXTURE = ROOT / "build" / "standalone_fixture"

FIXTURE_HEAD = r"""// Generated by tools/verify_standalone.py --fixture from analysis/classes.json - do not edit.
// Stand-in classes with the stock sizes; only what IMPLEMENT_CLASS needs from Core and Engine.
#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#include <windows.h>   // TCHAR = WCHAR = unsigned short (/Zc:wchar_t-), TEXT(s) = L##s
#include <new>
#pragma warning(disable : 4291)   // operator new(size_t, EInternal*) without a delete, as in UE2
enum ENativeConstructor { EC_NativeConstructor };
enum EInternal { EC_Internal };
enum { RF_Public = 0x4, RF_Transient = 0x4000, RF_Standalone = 0x80000, RF_Native = 0x4000000 };
enum { CLASS_Abstract = 0x1, CLASS_Config = 0x4 };
class FGuid { public: DWORD A, B, C, D; FGuid(DWORD InA, DWORD InB, DWORD InC, DWORD InD) : A(InA), B(InB), C(InC), D(InD) {} };
class UClass;
class __declspec(dllimport) UObject
{
public:
    virtual ~UObject();
    UObject();
    static void operator delete(void* Object, size_t Size);
    static const TCHAR* StaticConfigName();
    void StaticConstructor();
    static UClass* StaticClass() { return &PrivateStaticClass; }
    DWORD Fields[9];
private:
    static UClass PrivateStaticClass;
};
class __declspec(dllimport) UClass
{
public:
    UClass(ENativeConstructor, DWORD InSize, DWORD InClassFlags, UClass* InSuperClass, UClass* InWithinClass,
           FGuid InGuid, const TCHAR* InNameString, const TCHAR* InPackageName, const TCHAR* InConfigName,
           DWORD InFlags, void (*InClassConstructor)(void*), void (UObject::*InClassStaticConstructor)());
    virtual ~UClass();
    BYTE Opaque[0x520 - 4];
};
class UL2Font : public UObject
{
public:
    static UClass* StaticClass() { return &PrivateStaticClass; }
    DWORD FExecVtbl;
private:
    __declspec(dllimport) static UClass PrivateStaticClass;
};
class UL2ConsoleWnd : public UObject
{
public:
    static UClass* StaticClass() { return &PrivateStaticClass; }
    DWORD FExecVtbl;
private:
    __declspec(dllimport) static UClass PrivateStaticClass;
};

#define DECLARE_CLASS(TClass, TSuperClass, TStaticFlags)                                   \
public:                                                                                     \
    enum { StaticClassFlags = TStaticFlags };                                               \
    typedef TSuperClass Super;                                                              \
    typedef UObject WithinClass;                                                            \
    static UClass* StaticClass() { return &PrivateStaticClass; }                            \
    static void InternalConstructor(void* X) { new ((EInternal*)X) TClass; }                \
    void* operator new(size_t, EInternal* Mem) { return (void*)Mem; }                       \
private:                                                                                    \
    static UClass PrivateStaticClass;                                                       \
public:

// IMPLEMENT_PACKAGE(NWindow)
extern "C" __declspec(dllexport) TCHAR GPackage[] = TEXT("NWindow");
extern "C" { HINSTANCE hInstance; }
extern "C" __declspec(dllexport) INT __stdcall DllMain(HINSTANCE hInInstance, DWORD Reason, void* Reserved)
{
    hInstance = hInInstance;
    return 1;
}

#define IMPLEMENT_CLASS(TClass)                                                             \
    UClass TClass::PrivateStaticClass(                                                      \
        EC_NativeConstructor, sizeof(TClass), TClass::StaticClassFlags,                    \
        TClass::Super::StaticClass(), TClass::WithinClass::StaticClass(),                   \
        FGuid(0, 0, 0, 0), TEXT(#TClass) + 1, GPackage, TClass::StaticConfigName(),         \
        RF_Public | RF_Standalone | RF_Transient | RF_Native,                               \
        (void (*)(void*))TClass::InternalConstructor,                                       \
        (void (UObject::*)()) & TClass::StaticConstructor);                                 \
    extern "C" __declspec(dllexport) UClass* autoclass##TClass;                             \
    UClass* autoclass##TClass = TClass::StaticClass();
"""

FIXTURE_CMAKE = r"""cmake_minimum_required(VERSION 3.20)
project(nwd_standalone_fixture CXX)
# the flags of the real build (CMakeLists.txt) and the link options docs/notas/Standalone.md asks
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
set(DEFS "@DEFS@")
foreach(dll Core Engine)
  add_custom_command(OUTPUT "${CMAKE_BINARY_DIR}/${dll}.lib"
    COMMAND "${CMAKE_AR}" /nologo /machine:x86 "/def:${DEFS}/${dll}.def" "/out:${CMAKE_BINARY_DIR}/${dll}.lib"
    DEPENDS "${DEFS}/${dll}.def")
  list(APPEND LIBS "${CMAKE_BINARY_DIR}/${dll}.lib")
endforeach()
add_custom_target(import_libs DEPENDS ${LIBS})
add_library(NWindow SHARED Fixture.gen.cpp)
add_dependencies(NWindow import_libs)
target_compile_options(NWindow PRIVATE /W3 /Zc:wchar_t- /GR- /EHa /arch:IA32 /Oy- /Zi /O2)
target_link_libraries(NWindow PRIVATE ${LIBS})
target_link_options(NWindow PRIVATE /MAP /DEBUG /OPT:REF /OPT:ICF /BASE:0x10000000 /DYNAMICBASE:NO
  "${CMAKE_SOURCE_DIR}/NWindow.res")
"""


def write_fixture(class_list: list[dict], stock: Image) -> Path:
    src = FIXTURE / "src"
    src.mkdir(parents=True, exist_ok=True)
    out = [FIXTURE_HEAD]
    flag_names = {0: "0", 1: "CLASS_Abstract", 4: "CLASS_Config", 5: "CLASS_Abstract | CLASS_Config"}
    by_cpp = {r["cpp"]: r for r in class_list}
    for r in sorted(class_list, key=lambda r: r["depth"]):
        sup = r["super"]
        sup_size = by_cpp[sup]["size"] if sup in by_cpp else 0x2C if sup != "UObject" else 0x28
        pad = r["size"] - sup_size
        out.append(f"class {r['cpp']} : public {sup}\n{{\n    DECLARE_CLASS({r['cpp']}, {sup}, "
                   f"{flag_names[r['class_flags']]})\n" + (f"    BYTE Pad{r['cpp']}[{pad:#x}];\n" if pad else "")
                   + f"}};\nstatic_assert(sizeof({r['cpp']}) == {r['size']:#x}, \"size\");\n")
    # the stock's initializer order (the .obj link order): one file, IMPLEMENT_CLASS in that order
    for r in sorted(class_list, key=lambda r: r["initializer"]):
        out.append(f"IMPLEMENT_CLASS({r['cpp']})\n")
    (src / "Fixture.gen.cpp").write_text("".join(out), encoding="utf-8", newline="\n")
    (src / "NWindow.res").write_bytes(stock_cursors_res(stock))
    (src / "CMakeLists.txt").write_text(
        FIXTURE_CMAKE.replace("@DEFS@", (ROOT / "third_party" / "ue_import").as_posix()), encoding="utf-8")
    return src


def stock_cursors_res(stock: Image) -> bytes:
    """The stock RT_CURSOR and RT_GROUP_CURSOR resources as a .res file the linker takes as it
    is: same ids, same language (Korean), same bytes. rc cannot do it: it gives the predefined
    numeric types their own syntax (a .cur file), and building from .cur files renumbers the
    images."""
    def entry(rtype: int, rid: int, lang: int, data: bytes) -> bytes:
        head = struct.pack("<HHHH", 0xFFFF, rtype, 0xFFFF, rid)
        head += struct.pack("<IHHII", 0, 0x1010 if rtype == 1 else 0x1030, lang, 0, 0)
        return struct.pack("<II", len(data), 8 + len(head)) + head + data + b"\0" * (-len(data) % 4)

    out = struct.pack("<II", 0, 0x20) + struct.pack("<HHHH", 0xFFFF, 0, 0xFFFF, 0) + b"\0" * 16
    pe = stock.pe
    for t in pe.DIRECTORY_ENTRY_RESOURCE.entries:
        if t.id not in (1, 12):  # RT_CURSOR, RT_GROUP_CURSOR
            continue
        for e in t.directory.entries:
            for lang in e.directory.entries:
                d = lang.data.struct
                out += entry(t.id, e.id, lang.id, pe.get_data(d.OffsetToData, d.Size))
    return out


def build_fixture(class_list: list[dict], stock: Image) -> Path | None:
    import subprocess
    src = write_fixture(class_list, stock)
    bld = FIXTURE / "build"
    env = dict(os.environ, MSYS_NO_PATHCONV="1")
    for cmd in (["cmake", "-S", str(src), "-B", str(bld), "-G", "Visual Studio 17 2022", "-A", "Win32"],
                ["cmake", "--build", str(bld), "--config", "Release"]):
        r = subprocess.run(cmd, capture_output=True, text=True, env=env, errors="replace")
        if r.returncode != 0:
            print(f"fixture: {' '.join(cmd[:2])} failed\n{r.stdout[-3000:]}{r.stderr[-2000:]}")
            return None
    dll = bld / "Release" / "NWindow.dll"
    print(f"fixture: {dll.relative_to(ROOT)} built from {len(class_list)} stand-in classes")
    return dll


# ---- main ---------------------------------------------------------------------------------------

def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--dll", type=Path, help="the standalone NWindow.dll (default: the first of "
                                              + ", ".join(str(c.relative_to(ROOT)) for c in CANDIDATES) + ")")
    ap.add_argument("--map", type=Path, help="its linker map (default: the DLL's name with .map)")
    ap.add_argument("--stock", action="store_true", help="check orig/NWindow.dll itself (must pass)")
    ap.add_argument("--game-src", action="store_true",
                    help="read src/nwindow + include/ (NW_DECLARE_CLASS, NW_CHECK_VTABLE): the default now "
                         "(the C4 tree is there); kept for the old command lines")
    ap.add_argument("--no-game-src", action="store_true",
                    help="do not read src/nwindow + include/ (the source check is skipped, and so are the "
                         "NW_CHECK_VTABLE vtables outside the class list)")
    ap.add_argument("--checks", default=",".join(CHECKS), help="comma list of " + ", ".join(CHECKS))
    ap.add_argument("--only", action="append", default=[], help="classes/vtables: only this class (repeatable)")
    ap.add_argument("--cursors-res", type=Path, metavar="FILE",
                    help="write the stock cursors (RT_CURSOR 1-7, RT_GROUP_CURSOR 101..131, Korean) as a "
                         ".res the linker takes as it is, and stop")
    ap.add_argument("--l2", action="store_true",
                    help="the phase 6 build (NWindowL2): also accept exactly the L2 extras of "
                         "tests/standalone/l2_extras.csv (exports, classes)")
    ap.add_argument("--fixture", action="store_true",
                    help="build a stand-in NWindow.dll with the current compiler (build/standalone_fixture) "
                         "and check it: pe, resources, exports, classes")
    args = ap.parse_args()
    if args.cursors_res:
        data = stock_cursors_res(Image(STOCK))
        args.cursors_res.write_bytes(data)
        print(f"verify_standalone: {args.cursors_res} ({len(data)} bytes, the 7 stock cursor groups)")
        return 0
    if args.fixture:
        stock = Image(STOCK)
        class_list = json.loads((ROOT / "analysis" / "classes.json").read_text(encoding="utf-8"))
        dll = build_fixture(class_list, stock)
        if dll is None:
            return 2
        args.dll, args.map, args.stock = dll, None, False
        if args.checks == ",".join(CHECKS):
            args.checks = "pe,resources,exports,classes"
    if not args.no_game_src:
        SRC_CPP.extend(sorted((ROOT / "src" / "nwindow").rglob("*.cpp")))
        SRC_H.extend(sorted((ROOT / "include").rglob("*.h")))
    checks = [c.strip() for c in args.checks.split(",") if c.strip()]
    for c in checks:
        if c not in CHECKS:
            print(f"verify_standalone: unknown check {c} ({', '.join(CHECKS)})")
            return 2

    stock = Image(STOCK)
    class_list = json.loads((ROOT / "analysis" / "classes.json").read_text(encoding="utf-8"))
    classes = all_classes = {r["cpp"]: r for r in class_list}
    stock_syms = stock_symbols(stock, class_list)
    for cpp, lst in source_vtable_checks().items():
        if cpp not in classes:
            for base, va, _ in lst:
                stock_syms.add(va, f"??_7{cpp}@@6B@")
    stock_syms.finish()
    stock_t = Target(STOCK, stock_syms, stock=True)
    # the order the stock's CRT table runs the IMPLEMENT_CLASS initializers in
    stock_order = [r["cpp"] for r in sorted(class_list, key=lambda r: initializer_slot(stock_t, r["initializer"]))]
    if args.only:
        classes = {k: v for k, v in classes.items() if k in args.only}
    extras = read_l2_extras() if args.l2 else None
    if extras is not None and args.only:
        extras = {k: v for k, v in extras.items() if k in args.only}
    rep = Report(read_notes(), stock=args.stock)

    needs_dll = [c for c in checks if c != "source"]
    t = None
    if needs_dll:
        if args.stock:
            t = stock_t
            print(f"verify_standalone: the stock {STOCK.relative_to(ROOT)} checked against itself "
                  "(symbols from db/functions.csv and analysis/classes.json)")
        else:
            dll = args.dll or next((c for c in CANDIDATES if c.exists()), None)
            if dll is None or not dll.exists():
                print("verify_standalone: no standalone NWindow.dll to check")
                print("  looked at: " + ", ".join(str(c) for c in ([args.dll] if args.dll else CANDIDATES)))
                print("  build the standalone target (cmake --build build --config Release --target <standalone>)"
                      " and pass --dll <path> if it lands elsewhere;")
                print("  python tools/verify_standalone.py --stock checks the tool on the original;"
                      " --checks source needs no DLL")
                return 2
            mp = args.map or dll.with_suffix(".map")
            syms = None
            if mp.exists():
                syms = load_map(mp)
                print(f"verify_standalone: {dll} with {mp.name} ({len(syms.by_addr)} symbol addresses)")
            else:
                print(f"verify_standalone: {dll} - no linker map {mp}")
                print("  link with /MAP (CMake: target_link_options(<target> PRIVATE /MAP)) or pass --map;"
                      " classes and vtables need it")
            t = Target(dll, syms, stock=False)
            if syms is None:
                for c in ("classes", "vtables"):
                    if c in checks:
                        rep.section(c, "needs the linker map")
                        rep.fail("map", f"no {mp.name}")
                checks = [c for c in checks if c not in ("classes", "vtables")]
            elif syms.base != t.img.base:
                print(f"  the map says base {syms.base:#x}, the DLL {t.img.base:#x}: map of another link?")
                return 2

    for c in checks:
        if c == "pe":
            check_pe(rep, t, stock)
        elif c == "imports":
            check_imports(rep, t, stock)
        elif c == "resources":
            check_resources(rep, t, stock)
        elif c == "exports":
            check_exports(rep, t, stock, classes, extras)
        elif c == "classes":
            check_classes(rep, t, classes, stock_order, extras)
        elif c == "vtables":
            check_vtables(rep, t, stock, classes, set(all_classes), args.only)
        elif c == "source":
            check_source(rep, classes)
    return rep.summary()


if __name__ == "__main__":
    os.environ.setdefault("MSYS_NO_PATHCONV", "1")
    sys.exit(main())
