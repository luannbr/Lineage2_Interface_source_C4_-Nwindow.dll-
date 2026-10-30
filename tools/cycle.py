#!/usr/bin/env python3
"""One decompilation cycle for the C2 NWindow.dll, after editing src/:

  1. configure + build (Release, Win32). By default only src/re is compiled (cmake option
     NWD_GAME_SOURCES=OFF): the hook DLL has no function to replace and the cycle proves the
     infrastructure (build, import injection, load outside the game, verify). --game-source
     compiles src/nwindow in too, once it has been regenerated for the C2 image (it still holds
     the C1 copy until then: docs/PLANO_C2.md)
  2. smoke: install into build/smoke_system (a minimal copy of the engine DLLs) and load
     NWindow.dll there outside the game - every hook must report "ok" (with zero hooks: the log
     says "hooks: 0 installed, 0 registered, 0 errors")
  3. verify: compare every hook with the stock function (tools/verify_hooks.py), then check
     the register assumptions of the stock callers (tools/regcheck.py)
  4. difftest (--difftest): run pieces of the DLL outside the game with our code and compare what
     they write with what the stock code wrote (tests/golden/, tools/difftest.py --golden). The
     cases still hard-code C1 addresses (docs/PLANO_C2.md): off by default
  4b. standalone (--standalone, phase 5, docs/PLANO_C2.md): our NW_DECLARE_CLASS/NW_CHECK_VTABLE
     against the stock class table (tools/verify_standalone.py --checks source); then the
     standalone NWindow.dll is built (target NWindowStandalone, build/standalone/Release),
     checked without running it (tools/verify_standalone.py: PE, imports, resources, exports, classes,
     vtables), run on the same cases against tests/golden/ (tools/difftest.py --standalone; only
     with --difftest: the cases are still the C1 ones) and loaded the engine's way outside the
     game next to the stock DLL (tools/smoke_standalone.py); each of these stops the cycle
  5. install into the test System (tools/install_test.py: <C3 client>/system_nwtest, a copy of the
     client's system folder; the client's own system folder is never touched); with the game
     running from it the new build takes effect at the next start; --no-install skips it
  6. regenerate db/functions.csv and docs/PROGRESSO.md (tools/build_db.py)

  python tools/cycle.py [--system DIR] [--no-install] [--game-source] [--hook-selftest] [--difftest] [--standalone]

--hook-selftest also compiles tools/smoke/hook_selftest.cpp (one hook on NCWnd::IsEnabled) into the
DLL: the smoke must then report 1 hook installed and verify_hooks.py compare it with the stock
function (a proof of the installer on the C2 image while the game source is empty).
Stops at the first failing step.
"""
from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from install_test import DEFAULT_SYSTEM, NO_SYSTEM, writable  # noqa: E402

SMOKE = ROOT / "build" / "smoke_system"
# what the loader needs to map the C2 NWindow.dll (Engine/Core, their dependencies, and npkcrypt.dll
# with the npkpdb.dll it imports; the rest - WS2_32, ADVAPI32, WINMM... - is Windows')
ENGINE_DEPS = ["Core.dll", "Engine.dll", "IFC23.DLL", "MSVCR70.dll", "ogg.dll", "vorbis.dll",
               "vorbisfile.dll", "npkcrypt.dll", "npkpdb.dll"]


def no_log_flush(ini: Path) -> None:
    """[Log] Flush=0: the smoke log is read after the process ends, and a flush per line (about
    14,500 lines with a full game source) takes from 5 s to over 2 minutes depending on how busy
    the disk is."""
    text = ini.read_text(encoding="latin-1") if ini.exists() else ""
    if "[Log]" not in text:
        with ini.open("a", encoding="latin-1", newline="") as f:
            f.write("\r\n[Log]\r\nFlush=0\r\n")


def run(title: str, cmd: list[str]) -> bool:
    print(f"\n=== {title}", flush=True)
    env = dict(os.environ, MSYS_NO_PATHCONV="1")
    return subprocess.run(cmd, cwd=ROOT, env=env).returncode == 0


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--system", type=Path, default=DEFAULT_SYSTEM)
    ap.add_argument("--no-install", action="store_true")
    ap.add_argument("--game-source", action="store_true",
                    help="compile src/nwindow in (cmake NWD_GAME_SOURCES=ON) and read its hooks; "
                         "only once it has been regenerated for the C2 image")
    ap.add_argument("--hook-selftest", action="store_true",
                    help="compile the one-hook self-test (tools/smoke/hook_selftest.cpp) into NWindowDecomp.dll")
    ap.add_argument("--difftest", action="store_true", help="run the difftest cases (C1 addresses: not yet for C2)")
    ap.add_argument("--standalone", action="store_true", help="also run the phase 5 standalone steps")
    args = ap.parse_args()
    if args.system is None:
        ap.error(NO_SYSTEM)
    py = sys.executable
    gs = ["--game-src"] if args.game_source else []
    if not run("configure", ["cmake", "-S", ".", "-B", "build", "-G", "Visual Studio 17 2022", "-A", "Win32",
                             f"-DNWD_GAME_SOURCES={'ON' if args.game_source else 'OFF'}",
                             f"-DNWD_HOOK_SELFTEST={'ON' if args.hook_selftest else 'OFF'}"]):
        return 1
    if not run("build", ["cmake", "--build", "build", "--config", "Release"]):
        return 1

    SMOKE.mkdir(parents=True, exist_ok=True)
    for dep in ENGINE_DEPS:
        if not (SMOKE / dep).exists():
            shutil.copy2(args.system / dep, SMOKE / dep)
    if not run("smoke install", [py, "tools/install_test.py", "--system", str(SMOKE)]):
        return 1
    no_log_flush(SMOKE / "NWindowDecomp.ini")
    smoke_exe = ROOT / "build" / "smoke" / "Release" / "nwd_smoke.exe"
    if not run("smoke", [str(smoke_exe), str(SMOKE)]):
        return 1
    log = SMOKE / "NWindowDecomp.log"
    text = log.read_text(encoding="latin-1") if log.exists() else ""
    if "ERROR" in text or "hooks:" not in text:
        print("smoke: hook errors, see", log)
        return 1
    if not run("verify", [py, "tools/verify_hooks.py", "--log", str(log)]):
        return 1
    # stock callers that keep a volatile register across a call to a hook (whole-program
    # optimization in the stock build): must be none before anything reaches the game
    if not run("regcheck", [py, "tools/regcheck.py"] + gs):
        return 1
    if args.difftest:
        # results, not only calls: the fixed cases and the server page sets
        if not run("difftest", [py, "tools/difftest.py", "--golden", "--system", str(args.system)]):
            return 1
    if args.standalone:
        if not run("standalone: declarations", [py, "tools/verify_standalone.py", "--checks", "source"]):
            return 1
        if not run("standalone: build", ["cmake", "--build", "build", "--config", "Release", "--target",
                                         "NWindowStandalone"]):
            return 1
        standalone = ROOT / "build" / "standalone" / "Release" / "NWindow.dll"
        if not run("standalone: verify", [py, "tools/verify_standalone.py", "--dll", str(standalone)]):
            return 1
        # the difftest cases still hard-code C1 addresses and refuse to run on the C2 image
        # (docs/PLANO_C2.md, item 4): only with --difftest, until they are redone
        if args.difftest and not run("standalone: difftest", [py, "tools/difftest.py", "--standalone", str(standalone),
                                                              "--system", str(args.system)]):
            return 1
        # loaded, registered and constructed through Core outside the game, the output compared with
        # the stock NWindow.dll's (phase 5 step 6)
        if not run("standalone: smoke", [py, "tools/smoke_standalone.py", "--dll", str(standalone),
                                         "--system", str(args.system)]):
            return 1

    if not args.no_install:
        if not run("install", [py, "tools/install_test.py", "--system", str(args.system)]):
            return 1
        if not writable(args.system / "NWindow.dll"):
            print(f"    the game is running from {args.system}: restart it to load this build")
    return 0 if run("db", [py, "tools/build_db.py"] + gs) else 1


if __name__ == "__main__":
    sys.exit(main())
