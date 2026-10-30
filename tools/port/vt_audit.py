#!/usr/bin/env python3
"""Does the vtable the compiler builds from our headers have the slots the headers say?

The generated headers number every virtual in a comment (`// 365 0x1006a330`: the slot of the stock vtable). What decides
the slot of a call in our code is only the declaration ORDER and the names: a virtual whose name or signature differs from
the ancestor's declaration (an engine class' `NewVirtual<N>()`, an override with other parameters) is a NEW slot appended
at the end, and every virtual call ours makes through it goes to the wrong place (verify_hooks: `vcalls` differ; a hooked
constructor: `VT BAD`).

This asks MSVC for the layout of every class of the headers (/d1reportSingleClassLayout, which lists the vftable with the
function of each slot) and compares it with the header: a class whose declaration says slot N but whose vtable has the name
elsewhere is listed, with the number of slots the class has against the stock one (db/vslots.json: n2).

  python tools/port/vt_audit.py [--include include] [--class NConsoleWnd ...] [--quiet]

Exit status 1 when a class differs. The compile needs the Visual C++ environment (vcvars32.bat, found by vswhere).
"""
from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "port"))
import hdr  # noqa: E402


def vcvars() -> str:
    pf = os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")
    vw = Path(pf) / "Microsoft Visual Studio" / "Installer" / "vswhere.exe"
    r = subprocess.run([str(vw), "-latest", "-products", "*", "-property", "installationPath"], capture_output=True, text=True)
    return str(Path(r.stdout.strip()) / "VC" / "Auxiliary" / "Build" / "vcvars32.bat")


def header_slots(inc: Path):
    """class -> {slot: name} of the virtuals the headers number."""
    out = {}
    for p in sorted((inc / "nwindow").glob("*.h")):
        try:
            _, cds = hdr.parse(p)
        except Exception:
            continue
        for cd in cds:
            d = {}
            for m in cd.methods:
                if m.kind == "virtual" and m.slot is not None and not m.name.startswith("~"):
                    d.setdefault(m.slot, m.name)
            if d:
                out.setdefault(cd.name, {}).update(d)
    return out


def layouts(inc: Path, classes):
    """class -> {slot: (class, name)} (the primary vftable) from the compiler's report. One compile per class (the option
    takes a single class), several at a time."""
    from concurrent.futures import ThreadPoolExecutor
    # headers that define a type another header already defines under the same name (the alternatives the sources never
    # include together): in this one translation unit the second definition is an error and the compiler's layout report of
    # the classes after it is cut (the audit then claimed 20 classes had lost slots). None today: stage 5 (sa-verify) gave
    # every stand-in struct (NConsoleLeftFns, NConsoleForwardFns, NChatSbSender ...) and NMsnGroup ONE definition, so every
    # header is in the translation unit, and layouts() refuses to go on when the unit does not compile (see main)
    skip = set()
    heads = sorted(p.name for p in (inc / "nwindow").glob("*.h") if p.name not in skip)
    tmp = Path(tempfile.mkdtemp(prefix="vt_audit_"))
    src = tmp / "all.cpp"
    src.write_text('#include "nwindow/NWindow.h"\n' + "".join('#include "nwindow/%s"\n' % h for h in heads), encoding="utf-8")
    vc = vcvars()

    def one(c):
        bat = tmp / ("run_%s.bat" % c)
        bat.write_text("@echo off\r\ncall \"%s\" >nul 2>&1\r\ncd /d \"%s\"\r\n"
                       "cl /nologo /c /W0 /utf-8 /Zc:wchar_t- /GR- /EHa /arch:IA32 /Oy- /std:c++17 /DWIN32 /D_WINDOWS /DUNICODE /D_UNICODE "
                       "/DWIN32_LEAN_AND_MEAN /DNOMINMAX /D_CRT_SECURE_NO_WARNINGS /D_CRT_NON_CONFORMING_SWPRINTFS /DNW_GAME_SOURCE=1 "
                       "/DNW_HOOK_MODE=1 /I\"%s\" /I\"%s\" /d1reportSingleClassLayout%s /Fo\"%s\" \"%s\"\r\n" % (
                           vc, ROOT, inc, ROOT / "src", c, tmp / (c + ".obj"), src), encoding="ascii")
        r = subprocess.run(["cmd", "/c", str(bat)], capture_output=True)
        return c, r.stdout.decode("cp1252", errors="replace")

    out, texts = {}, []
    with ThreadPoolExecutor(max_workers=8) as ex:
        results = list(ex.map(one, classes))
    for c, text in results:
        texts.append(text)
        cur, in_vt, seen_vt = None, False, False
        for l in text.split("\n"):
            l = l.rstrip("\r")
            m = re.match(r"^(?:class|struct)\s+(\w+)\s+size\(", l)
            if m:
                cur, in_vt, seen_vt = m.group(1), False, False
                if cur == c:
                    out[cur] = {}
                continue
            if cur != c:
                continue
            if re.match(r"^\w+::\$vftable@", l):
                in_vt = not seen_vt
                seen_vt = True
                continue
            if in_vt:
                m = re.match(r"^\s*(\d+)\s*\|\s*&(\w+)::(\S+)", l)
                if m:
                    out[cur][int(m.group(1))] = (m.group(2), m.group(3))
                elif l.strip() == "":
                    in_vt = False
    return out, "\n".join(texts)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--include", default=str(ROOT / "include"))
    ap.add_argument("--class", dest="classes", action="append", default=[])
    ap.add_argument("--quiet", action="store_true")
    a = ap.parse_args()
    inc = Path(a.include)
    hs = header_slots(inc)
    todo = sorted(a.classes or hs)
    lay, text = layouts(inc, todo)
    # an error of the all-headers translation unit (two headers defining the same type, ...) lands in the middle of the
    # compiler's layout report and makes classes look as if they had lost slots: say it instead of reporting that
    errs = sorted(set(l.strip() for l in text.split("\n") if " error " in l))
    if errs:
        print("vt_audit: the all-headers translation unit does not compile (%d distinct error(s)); the layouts below it cannot be trusted:" % len(errs))
        for e in errs[:12]:
            print("   ", e[:240])
        return 1
    vs = json.loads((ROOT / "db" / "vslots.json").read_text(encoding="utf-8"))
    bad = 0
    for c in todo:
        if c not in lay:
            continue
        slots = lay[c]
        probs = []
        for s, n in sorted(hs[c].items()):
            got = slots.get(s)
            if got is not None and got[0] == c and got[1] == n:
                continue
            where = [k for k, v in slots.items() if v[0] == c and v[1] == n]
            if got is not None and got[1] == n:
                # the name is there but the ancestor owns it: ours did not override it (other parameters?)
                probs.append("%s: header slot %d is %s::%s, ours is %s" % (
                    n, s, got[0], n, ("slot " + str(where[0])) if where else "nowhere"))
            else:
                probs.append("%s: header slot %d, vtable has %s" % (n, s, ("slot " + str(where[0])) if where else "no such slot"))
        n2 = vs.get(c, {}).get("n2")
        count = (max(slots) + 1) if slots else 0
        if n2 and count != n2:
            probs.append("%d slots, the stock vtable has %d" % (count, n2))
        if probs:
            bad += 1
            print("%s: %d problem(s)" % (c, len(probs)))
            if not a.quiet:
                for p in (probs if a.classes else probs[:8]):
                    print("   ", p)
    print("vt_audit: %d class(es) checked, %d differ" % (len([c for c in todo if c in lay]), bad))
    if not lay:
        print(text[-1500:])
    return 1 if bad else 0


if __name__ == "__main__":
    raise SystemExit(main())
