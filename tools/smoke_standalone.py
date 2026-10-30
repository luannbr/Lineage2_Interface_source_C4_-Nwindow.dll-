#!/usr/bin/env python3
"""Load the standalone NWindow.dll outside the game the way the engine does (phase 5,
docs/notas/Standalone.md, section 10), and do the same with the stock NWindow.dll as a control.

Two smoke Systems under build/, each with the engine DLLs of the test client (<C4 client>/system_nwtest, read only)
and a NWindow.dll installed by tools/install_test.py --standalone (no NWindowDecomp.dll):
  build/smoke_standalone        our standalone build
  build/smoke_standalone_stock  orig/NWindow.dll, unpatched
nwd_smoke --standalone runs the engine's path in stages (tools/smoke/nwd_smoke_standalone.cpp):
bind (Core's appGetDllHandle), dllmain, exports (appGetDllExport), queue (GAutoRegister), init
(FName/UObject::StaticInit, i.e. ProcessRegistrants, with stand-ins for what appInit sets up),
registered, load (StaticLoadClass of "ini:Engine.Engine.L2Console" / "...L2Font" as Engine.dll
calls it) and construct (StaticConstructObject of both, as Engine.dll does next).

The output has no addresses: ours must print exactly what the stock DLL prints (Core's log
included), and every registered class must match analysis/classes.json (name, package, parent,
size). What the UClass objects hold before registration is difftest's case "classes".

C4 status (docs/notas/C4_Standalone.md): the control run (orig/NWindow.dll, `--control-only`) passes on the C4 Core.dll and
Engine.dll (27/05/2006): all 9 stages, 341 classes registered and checked against analysis/classes.json, both objects
constructed. The stand-ins in tools/smoke/nwd_smoke_standalone.cpp (FMalloc, FOutputDevice, FConfigCache vtables) serve the
C4 Core as they are; the one C4 change is UField::SuperField at +0x34 (UObject grew to 0x34). The Engine.dll source tests
and the phase 6 (NWindowL2) checks of the C1 tool are gone.

npkcrypt.dll (and the npkpdb.dll it imports) are copied into the stock System only: the standalone NWindow.dll does not
import npkcrypt.dll (src/nwindow/NKeyCrypt.cpp loads it by ordinal when the keyboard protection is used), so our System
has none of the two and our output must still be the stock's. --with-npk puts them in ours too.

  python tools/smoke_standalone.py [--dll DLL] [--system DIR] [--show] [--control-only] [--with-npk]
    --dll           the standalone NWindow.dll (default: the first of CANDIDATES)
    --system        where the engine DLLs are copied from (default: the C4 test client; only read)
    --show          print the whole output of our run
    --control-only  run only the stock NWindow.dll (orig/), no standalone DLL needed
    --with-npk      also copy npkcrypt.dll / npkpdb.dll into our System

Exit code 0 when every stage passes and the output is the stock one.
"""
from __future__ import annotations

import argparse
import difflib
import json
import re
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(Path(__file__).resolve().parent))
from install_test import DEFAULT_SYSTEM, NO_SYSTEM  # noqa: E402
from nwd.image import Image  # noqa: E402

EXE = ROOT / "build" / "smoke" / "Release" / "nwd_smoke.exe"
OURS = ROOT / "build" / "smoke_standalone"
STOCK = ROOT / "build" / "smoke_standalone_stock"
ORIG = ROOT / "orig" / "NWindow.dll"
# what the loader needs to map the C4 NWindow.dll (see tools/cycle.py; the C4 client's system folder has all of them)
ENGINE_DEPS = ["Core.dll", "Engine.dll", "IFC23.DLL", "MSVCR70.dll", "ogg.dll", "vorbis.dll", "vorbisfile.dll"]
# what only the stock NWindow.dll's import table needs (the standalone loads npkcrypt.dll at run time, if at all)
NPK_DEPS = ["npkcrypt.dll", "npkpdb.dll"]
CANDIDATES = [ROOT / "build" / "standalone" / "Release" / "NWindow.dll",
              ROOT / "build" / "bin" / "Release" / "NWindow.dll",
              ROOT / "build" / "Release" / "NWindow.dll"]
ENGINE_PARENT = {"UObject": "Core.Object", "UL2ConsoleWnd": "Engine.L2ConsoleWnd", "UL2Font": "Engine.L2Font"}


def prepare(sysdir: Path, dll: Path, engine: Path, with_npk: bool = True) -> bool:
    sysdir.mkdir(parents=True, exist_ok=True)
    for dep in ENGINE_DEPS + (NPK_DEPS if with_npk else []):
        if not (sysdir / dep).exists():
            if not (engine / dep).exists():
                print(f"smoke_standalone: {engine / dep} missing (the engine DLLs come from the test client)")
                return False
            shutil.copy2(engine / dep, sysdir / dep)
    if not with_npk:
        for dep in NPK_DEPS:   # a copy left by an earlier run: the standalone must load without them
            if (sysdir / dep).exists():
                (sysdir / dep).unlink()
    r = subprocess.run([sys.executable, str(ROOT / "tools" / "install_test.py"), "--system", str(sysdir),
                        "--standalone", str(dll)], capture_output=True, text=True)
    if r.returncode:
        print(r.stdout + r.stderr)
    return r.returncode == 0


def instance_rva_ours(dll: Path) -> int | None:
    """GNWindowInstance (defined by NW_IMPLEMENT_PACKAGE) in our map."""
    mp = dll.with_suffix(".map")
    if not mp.exists():
        return None
    text = mp.read_text(encoding="latin-1")
    base = int(re.search(r"Preferred load address is ([0-9a-f]+)", text).group(1), 16)
    m = re.search(r"\?GNWindowInstance@@3PAUHINSTANCE__@@A\s+([0-9a-f]{8})", text)
    return int(m.group(1), 16) - base if m else None


def instance_rva_stock() -> int:
    """The stock GNWindowInstance, read from the stock DllMain (the IMPLEMENT_PACKAGE one:
    `mov eax, [esp+4]; mov [GNWindowInstance], eax; mov eax, 1; ret 0Ch`)."""
    img = Image(ORIG)
    va = img.exports["_DllMain@12"]
    for ins in img.disasm(va, 0x20):
        m = re.fullmatch(r"dword ptr \[(0x[0-9a-f]+)\], e[a-d]x", ins.op_str)
        if ins.mnemonic == "mov" and m:
            return int(m.group(1), 16) - img.base
    raise SystemExit("smoke_standalone: no store of hInstance in the stock DllMain")


def smoke(sysdir: Path, rva: int | None) -> tuple[int, list[str]]:
    cmd = [str(EXE), "--standalone", str(sysdir)]
    if rva is not None:
        cmd += ["--instance-rva", f"{rva:x}"]
    try:
        r = subprocess.run(cmd, capture_output=True, text=True, errors="replace", timeout=300)
    except subprocess.TimeoutExpired:
        return -1, ["TIMEOUT after 300 s"]
    return r.returncode, (r.stdout + r.stderr).splitlines()


def normalize(lines: list[str]) -> list[str]:
    """What must be the same for the stock DLL and ours."""
    out = []
    for line in lines:
        if line.startswith("System: "):
            continue
        out.append(re.sub(r"image: (stock C\d build|not the stock build)", "image: -", line))
    return out


def check_registrations(lines: list[str]) -> list[str]:
    """The 'registered' lines against analysis/classes.json."""
    classes = {c["cpp"]: c for c in json.loads((ROOT / "analysis" / "classes.json").read_text(encoding="utf-8"))}
    seen, problems = set(), []
    for line in lines:
        m = re.match(r"\s+(\w+)\s+(\w+)\.(\w+) super=(\S+) size=0x([0-9a-f]+) flags=([0-9a-f]+)", line)
        if not m:
            continue
        cpp, pkg, name, sup, size = m.group(1), m.group(2), m.group(3), m.group(4), int(m.group(5), 16)
        c = classes.get(cpp)
        if not c:
            problems.append(f"{cpp}: not in classes.json")
            continue
        seen.add(cpp)
        sc = classes.get(c["super"])
        want_sup = ENGINE_PARENT.get(c["super"]) or f"NWindow.{sc['name'] if sc else c['super'][1:]}"
        if (pkg, name, sup, size) != ("NWindow", c["name"], want_sup, c["size"]):
            problems.append(f"{cpp}: {pkg}.{name} super={sup} size={size:#x}, expected NWindow.{c['name']} "
                            f"super={want_sup} size={c['size']:#x}")
    missing = sorted(set(classes) - seen)
    if missing:
        problems.append(f"{len(missing)} classes not reported as registered: {', '.join(missing[:8])}")
    return problems


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--dll", type=Path)
    ap.add_argument("--system", type=Path, default=DEFAULT_SYSTEM)
    ap.add_argument("--show", action="store_true")
    ap.add_argument("--control-only", action="store_true")
    ap.add_argument("--with-npk", action="store_true")
    args = ap.parse_args()
    if args.system is None:
        ap.error(NO_SYSTEM)
    if not EXE.exists():
        print(f"smoke_standalone: {EXE} missing (cmake --build build --config Release --target nwd_smoke)")
        return 1
    if args.control_only:
        if not prepare(STOCK, ORIG, args.system):
            return 1
        scode, stock = smoke(STOCK, instance_rva_stock())
        print("\n".join(stock))
        problems = check_registrations(stock)
        print(f"control: exit {scode}; registered classes checked against classes.json: "
              + ("all match" if not problems else f"{len(problems)} problems"))
        for p in problems[:20]:
            print("    " + p)
        return 0 if scode == 0 and not problems else 1
    dll = args.dll or next((c for c in CANDIDATES if c.exists()), None)
    if not dll or not dll.exists():
        print("smoke_standalone: no standalone NWindow.dll (cmake --build build --config Release --target NWindowStandalone)")
        return 1
    if not prepare(OURS, dll, args.system, args.with_npk) or not prepare(STOCK, ORIG, args.system):
        return 1

    code, ours = smoke(OURS, instance_rva_ours(dll))
    scode, stock = smoke(STOCK, instance_rva_stock())
    if args.show:
        print("\n".join(ours))

    stages = [l for l in ours if l.startswith(("STAGE ", "STOPPED", "ALL STAGES"))]
    print(f"standalone {dll.relative_to(ROOT) if ROOT in dll.parents else dll} in {OURS.relative_to(ROOT)}:")
    for line in stages:
        print("  " + line)
    for line in ours:
        if line.startswith("  last log line") or "WRONG" in line or " NO" in line:
            print("  " + line.strip())
    reg = [l for l in ours if re.match(r"\s+\w+\s+\w+\.\w+ super=", l)]
    problems = check_registrations(ours)
    print(f"  registered classes checked against classes.json: {len(reg)}, "
          + ("all match" if not problems else f"{len(problems)} problems"))
    for p in problems[:20]:
        print("    " + p)

    a, b = normalize(stock), normalize(ours)
    same = a == b
    stock_stages = [l for l in stock if l.startswith(("STOPPED", "ALL STAGES"))]
    print(f"control: orig/NWindow.dll in {STOCK.relative_to(ROOT)}: {stock_stages[-1] if stock_stages else f'exit {scode}'}")
    print(f"  same output as the stock NWindow.dll ({len(b)} lines, Core's log included): {'yes' if same else 'NO'}")
    if not same:
        diff = list(difflib.unified_diff(a, b, "stock", "ours", n=1, lineterm=""))
        for line in diff[:40]:
            print("    " + line)
        if len(diff) > 40:
            print(f"    ... {len(diff) - 40} more diff lines")
    ok = code == 0 and any(l.startswith("ALL STAGES OK") for l in ours) and not problems and same
    print(f"smoke_standalone: {'OK' if ok else 'FAILED'}")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
