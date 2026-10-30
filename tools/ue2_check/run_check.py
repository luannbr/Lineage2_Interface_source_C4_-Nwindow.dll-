#!/usr/bin/env python3
"""Checks include/ue2 (UE2Core.h, UE2Engine.h, gen/*.vt.inc) against the C4 DLLs of orig/ (Core.dll / Engine.dll of 27/05/2006), with the project's compiler
(VS2022 x86, the flags of CMakeLists.txt NWD_CXX_FLAGS / NWD_DEFS):

  1. compile    tools/ue2_check/check_ue2.cpp, NW_HOOK_MODE=1 and 0 (the headers must compile clean, every
                static_assert -- field offsets and sizes -- must hold);
  2. link       every ENGINE_API / CORE_API function and variable of the headers is referenced by a generated
                function (link_refs.cpp) and linked against import libraries made from third_party/ue_import/*.def:
                a wrong decorated name is an unresolved external;
  3. slots      for every class whose virtuals the headers declare, a probe class overrides each of them with the
                same signature; the compiler's own layout report (/d1reportSingleClassLayout) gives each override's
                vtable slot and the assembler listing its decorated name (__FUNCDNAME__): both are compared with the
                C4 DLL's vtable (slot by slot, undecorated signature included).

  python tools/ue2_check/run_check.py [--keep]       # exit 0: everything holds
"""
from __future__ import annotations

import argparse
import re
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
sys.path.insert(0, str(ROOT / "tools"))
sys.path.insert(0, str(HERE))
import engine_vtables as ev  # noqa: E402
import gen_checks as gc  # noqa: E402

VCVARS = r"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat"
BUILD = ROOT / "build" / "ue2check"
FLAGS = ("/nologo /std:c++17 /W3 /utf-8 /Zc:wchar_t- /GR- /EHa /arch:IA32 /Oy- /DWIN32 /D_WINDOWS /DUNICODE "
         "/D_UNICODE /DWIN32_LEAN_AND_MEAN /DNOMINMAX /D_CRT_SECURE_NO_WARNINGS /D_CRT_NON_CONFORMING_SWPRINTFS "
         "/Iinclude")


def vs(cmds: list[str], log: Path) -> tuple[int, str]:
    """Runs the commands in a cmd.exe with the x86 toolset of VS2022; (exit code of the last one, output)."""
    bat = BUILD / "step.bat"
    bat.write_text("@echo off\r\ncall \"%s\" >nul 2>nul\r\ncd /d \"%s\"\r\n%s\r\n" % (
        VCVARS, ROOT, "\r\n".join(cmds)), encoding="ascii")
    r = subprocess.run(["cmd", "/c", str(bat)], capture_output=True)
    text = r.stdout.decode("cp850", "replace") + r.stderr.decode("cp850", "replace")
    log.write_text(text, encoding="utf-8")
    return r.returncode, text


def warnings_in(text: str) -> list[str]:
    return [l for l in text.splitlines() if re.search(r"\b(warning|error) [CLD]\d+", l)]


def parse_layouts(text: str) -> dict[tuple[str, str], list[tuple[int, str]]]:
    """class -> [(slot, text)] of its primary vftable, from /d1reportSingleClassLayout."""
    out: dict[tuple[str, str], list[tuple[int, str]]] = {}
    cur = None
    for line in text.splitlines():
        # `Probe_X::$vftable@:` (one primary table) or `Probe_X::$vftable@UObject@:` (a class with several bases)
        m = re.match(r"^(Probe_\w+)::\$vftable@(\w*)@?:\s*$", line)
        if m:
            cur = (m.group(1), m.group(2))
            out[cur] = []
            continue
        if cur is None:
            continue
        m = re.match(r"^\s*(\d+)\t\| &(.+?)\s*$", line)
        if m:
            out[cur].append((int(m.group(1)), m.group(2)))
        elif line.strip() == "":
            cur = None
    return out


def parse_asm_strings(path: Path) -> list[str]:
    """Every string literal of an assembler listing, in order (MSVC writes them as `$SGnnn DB 'text', 00H`)."""
    out: list[str] = []
    cur = None
    for line in path.read_text(encoding="latin-1").splitlines():
        m = re.match(r"^\$SG\d+\s+DB\s+(.*)$", line)
        if m:
            cur = ""
            body = m.group(1)
        else:
            m = re.match(r"^\s+DB\s+(.*)$", line)
            if m and cur is not None:
                body = m.group(1)
            else:
                cur = None
                continue
        for item in re.findall(r"'((?:[^']|'')*)'|(\w+H)", body):
            if item[0] or (not item[0] and not item[1]):
                cur += item[0].replace("''", "'")
            elif item[1] == "00H":
                out.append(cur)
                cur = None
                break
    return out


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--keep", action="store_true")
    a = ap.parse_args()
    BUILD.mkdir(parents=True, exist_ok=True)
    fails = 0

    # ---- generated sources + import libraries -------------------------------------------------------------
    gc.main([str(BUILD)])
    code, text = vs([f'lib /nologo /machine:x86 /def:third_party\\ue_import\\{d}.def /out:build\\ue2check\\{d}.lib'
                     for d in ("Core", "Engine")], BUILD / "lib.log")
    print("import libraries from the .def files:", "ok" if code == 0 else "FAILED\n" + text)
    fails += code != 0

    # ---- 1. compile the headers, both modes ---------------------------------------------------------------
    for mode in (1, 0):
        code, text = vs([f"cl /c {FLAGS} /DNW_HOOK_MODE={mode} /Fobuild\\ue2check\\check_ue2_{mode}.obj "
                         f"tools\\ue2_check\\check_ue2.cpp"], BUILD / f"compile{mode}.log")
        w = warnings_in(text)
        print(f"1. compile check_ue2.cpp NW_HOOK_MODE={mode}: exit {code}, {len(w)} diagnostics")
        for l in w[:20]:
            print("     ", l)
        fails += code != 0 or bool(w)

    # ---- 2. link every ENGINE_API / CORE_API reference ----------------------------------------------------
    code, text = vs([f"cl /c {FLAGS} /MT /Od /DNW_HOOK_MODE=0 /Fobuild\\ue2check\\link_refs.obj build\\ue2check\\link_refs.cpp",
                     "if errorlevel 1 exit /b 1",
                     "link /nologo /DLL /MACHINE:X86 /OUT:build\\ue2check\\link_refs.dll build\\ue2check\\link_refs.obj "
                     "build\\ue2check\\Core.lib build\\ue2check\\Engine.lib user32.lib"], BUILD / "link.log")
    w = [l for l in text.splitlines() if re.search(r"\b(error|warning) (C|LNK)\d+|unresolved", l)]
    print(f"2. link every API reference against the C4 import libraries: exit {code}, {len(w)} diagnostics")
    for l in w[:40]:
        print("     ", l)
    fails += code != 0

    # ---- 3. slots and decorated names ---------------------------------------------------------------------
    probes = sorted(c for c in gc.PROBES if c != "FNetworkNotify")
    layout_flags = "/d1reportAllClassLayout"      # (only one /d1reportSingleClassLayout flag is honoured: take all, keep the Probe_ ones)
    code, text = vs([f"cl /c {FLAGS} /DNW_HOOK_MODE=0 /FAs /Fabuild\\ue2check\\probes.asm "
                     f"/Fobuild\\ue2check\\probes.obj {layout_flags} build\\ue2check\\probes.cpp"], BUILD / "probes.log")
    w = [l for l in warnings_in(text) if "C4" not in l or "C4715" not in l]
    print(f"3. compile the probes: exit {code}, {len(w)} diagnostics")
    for l in w[:10]:
        print("     ", l)
    fails += code != 0
    layouts = parse_layouts(text)
    strings = [s for s in parse_asm_strings(BUILD / "probes.asm") if re.match(r"^\?.*Probe_\w+@@", s)]
    pretty = ev.undname(strings)
    by_class: dict[str, list[str]] = {}
    for s, u in zip(strings, pretty):
        m = re.search(r"Probe_(\w+)::", u)
        by_class.setdefault(m.group(1) if m else "?", []).append(u.replace("Probe_", ""))
    total = ok = pure = bad = impl_ok = 0
    import hdrparse as hp
    # the implementing class of the pure slots (gen_engine_decls.py TARGETS): its vtable names them
    import gen_engine_decls as ged
    impl_pret: dict[str, list[str]] = {}
    for tcls, _dll, _sym, _parent, _api, _table, impl in ged.TARGETS:
        if impl:
            lab = [l for _, _, l in ev.dll(impl[0]).vtable(impl[1])]
            impl_pret[tcls] = ev.undname([l[7:] if l.startswith("import:") else l for l in lab])
    pure_decls = {(d.cls, d.name) for d in hp.parse([hp.INC / "UE2Core.h", hp.INC / "UE2Engine.h"])
                  if d.is_func and d.virtual and d.pure}
    PREFIX = {"FFileManager"}
    for cls in probes:
        dll, sym = gc.PROBES[cls]
        vt = ev.dll(dll).vtable(sym)
        labels = [lab for _, _, lab in vt]
        pret = ev.undname([l[7:] if l.startswith("import:") else l for l in labels])
        # the DLL names a table `??_7Class@@6B<base>@@@`; the compiler's report `Class::$vftable@<base>@:`
        base = re.search(r"@@6B(\w*?)@*$", sym)
        suffix = base.group(1).rstrip("@") if base else ""
        report = layouts.get((f"Probe_{cls}", suffix))
        if report is None:
            print(f"   {cls}: no layout report (not probed)")
            continue
        def opname(t: str) -> str:
            """the report prints `<<` for operator<<"""
            n = t.split("::", 1)[1].strip()
            return "operator" + n if re.fullmatch(r"[^\w~{]+", n) else n

        names = {s: t for s, t in report if t.startswith(f"Probe_{cls}::") and "{dtor}" not in t}
        # FFileManager: the header lists only the virtuals NWindow calls (the first ones): a prefix of the DLL's table
        if len(report) != len(vt) and not (cls in PREFIX and len(report) < len(vt)):
            print(f"   {cls}: {len(report)} slots in the header, {len(vt)} in the C4 {dll}.dll")
            bad += 1
        decor = by_class.get(cls, [])
        # decorated signatures of each name, in the declaration order of the probe; same-name overloads sit in
        # reverse order in the vtable
        per_name: dict[str, list[str]] = {}
        for u in decor:
            m = re.search(r"%s::(\S+?)\(" % re.escape(cls), u)
            per_name.setdefault(m.group(1) if m else "?", []).append(u)
        used: dict[str, int] = {}
        for s in sorted(names):
            nm = opname(names[s])
            lst = per_name.get(nm, [])
            run = [t for t, tt in names.items() if opname(tt) == nm]
            # the k-th slot of a same-name run takes the (n-1-k)-th declaration
            k = sorted(run).index(s)
            want = lst[len(lst) - 1 - k] if len(lst) == len(run) else None
            total += 1
            have = pret[s] if s < len(pret) else None
            label = labels[s] if s < len(labels) else None
            if want is None:
                print(f"   {cls} slot {s} {nm}: {len(lst)} probe signatures for {len(run)} slots")
                bad += 1
            elif label is not None and ("_purecall" in label or label.startswith("sub_")):
                pure += 1          # pure in the DLL: the name is not there to compare (the impl class names them)
                header_pure = (cls, nm) in pure_decls
                if cls in impl_pret and s < len(impl_pret[cls]) and want is not None:
                    # the implementing class's slot (UNetworkHandler, UALAudioSubsystem...) must have this signature
                    norm = lambda t: re.sub(r"\s+", " ", re.sub(r"\w+::", "C::", t, count=1))
                    if norm(impl_pret[cls][s]) != norm(want):
                        bad += 1
                        print(f"   {cls} slot {s} {nm} DIFFERENT from its implementation:\n        header {want}\n"
                              f"        impl   {impl_pret[cls][s]}")
                    else:
                        impl_ok += 1
                if ("_purecall" in label) != header_pure and "_purecall" in label:
                    bad += 1
                    print(f"   {cls} slot {s} {nm}: pure in the C4 DLL, the header declares it with a body")
            elif re.sub(r"\s+", " ", want) == re.sub(r"\s+", " ", have):
                ok += 1
                if (cls, nm) in pure_decls:
                    bad += 1
                    print(f"   {cls} slot {s} {nm}: declared pure in the header, implemented in the C4 DLL")
            else:
                bad += 1
                print(f"   {cls} slot {s} DIFFERENT:\n        header {want}\n        C4 dll {have}")
        # the slots the header does not override must still be the class's own: nothing to compare
    print(f"   virtual slots checked: {total}: {ok} equal to the C4 DLL (slot and signature), {pure} pure in the DLL "
          f"({impl_ok} of them equal to the implementing class's slot), {bad} wrong")
    fails += bad
    print("RESULT:", "ALL OK" if not fails else f"{fails} PROBLEM(S)")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
