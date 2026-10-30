#!/usr/bin/env python3
"""Install the hook-mode build into the test client (--system, default: the NWD_CLIENT_SYSTEM variable).

  python tools/install_test.py              install build/bin/NWindowDecomp.dll
  python tools/install_test.py --restore    put back the NWindow.dll found before the first install
  python tools/install_test.py --system DIR another System folder
  python tools/install_test.py --system build/DIR --standalone [DLL]
                                            a NWindow.dll that runs on its own (phase 5): our
                                            standalone build by default, orig/NWindow.dll for the
                                            stock control; no NWindowDecomp.dll. Only into a folder
                                            under build/ (tools/smoke_standalone.py): the in-game
                                            test of the standalone build needs the user's permission

Install = NWindow.dll rebuilt from orig/NWindow.dll (stock C4) with one extra import,
NWindowDecomp.dll!NWindowDecomp_Attach, + NWindowDecomp.dll/.pdb + NWindowDecomp.ini (kept
if it exists). The NWindow.dll that was there first is kept once as NWindow.dll.pre-decomp-bak.

With the game running from that folder the install still works when NWindow.dll already is the
patched one: the loaded NWindowDecomp.dll is renamed aside (a mapped image can be renamed) and
the new one copied in; the running game keeps the old code, the next start loads the new one.
Copies left aside are deleted by later installs once no process maps them.
"""
from __future__ import annotations

import argparse
import hashlib
import os
import shutil
import sys
import time
from pathlib import Path

import pefile

sys.path.insert(0, str(Path(__file__).resolve().parent))
from nwd.image import ROOT, STOCK_MD5  # noqa: E402
from nwd.peimport import add_import, verify_added  # noqa: E402

# The C4 client's own System folder is read-only for this project: the test client is a copy of it
# (xcopy /E /I /Y system system_nwtest, orig/README.md) and every install goes there. The test client's
# System folder: --system, or the NWD_CLIENT_SYSTEM variable.
DEFAULT_SYSTEM = Path(os.environ["NWD_CLIENT_SYSTEM"]) if os.environ.get("NWD_CLIENT_SYSTEM") else None
NO_SYSTEM = ("no client System folder: pass --system DIR or set NWD_CLIENT_SYSTEM "
             "(the test client's System folder, a copy of the client's own)")
BACKUP = "NWindow.dll.pre-decomp-bak"
INI = """; NWindowDecomp.ini - hook-mode switches (read when the game starts)
[Hooks]
; 0 = keep the whole stock NWindow.dll code (NWindowDecomp.dll only logs)
Enabled=1
; keep chosen functions stock, separated by spaces or commas:
;   1000cc40            one stock address
;   NCGDevice::DrawText one method
;   NCGDevice::*        one class
Disable=

[Debug]
; 1 = proof marks that only our code draws, to check in game that the hooks are live:
;     a yellow line at the top of the screen (hook count, build time), '#' instead of '*'
;     in password fields and a red caret in text fields. 0 = normal.
Marker=0
"""


def writable(p: Path) -> bool:
    if not p.exists():
        return True
    try:
        with p.open("r+b"):
            return True
    except PermissionError:
        return False


def replace_file(src: Path, dst: Path) -> str:
    """Copy src over dst; a dst held by a running game is renamed aside first."""
    if writable(dst):
        shutil.copy2(src, dst)
        return f"{dst.name}: copied"
    aside = dst.with_name(f"{dst.stem}.in-use-{int(time.time())}{dst.suffix}")
    dst.rename(aside)
    shutil.copy2(src, dst)
    return f"{dst.name}: copied (the running game keeps {aside.name}; restart it to load the new one)"


STANDALONE_DLL = ROOT / "build" / "standalone" / "Release" / "NWindow.dll"


def install_standalone(sysdir: Path, dll: Path) -> int:
    """NWindow.dll = dll as it is (with its .map/.pdb), and no hook-mode files."""
    build = (ROOT / "build").resolve()
    if build not in sysdir.resolve().parents:
        print(f"FAIL: {sysdir} is not under {build} - the standalone NWindow.dll is only installed "
              "into build/ (the in-game test needs the user's permission)")
        return 1
    if not dll.exists():
        print(f"FAIL: {dll} missing - build it first (cmake --build build --config Release --target NWindowStandalone)")
        return 1
    imports = [e.dll.decode().lower() for e in pefile.PE(str(dll), fast_load=False).DIRECTORY_ENTRY_IMPORT]
    if "nwindowdecomp.dll" in imports:
        print(f"FAIL: {dll} imports NWindowDecomp.dll (a hook-mode install, not a standalone NWindow.dll)")
        return 1
    sysdir.mkdir(parents=True, exist_ok=True)
    shutil.copy2(dll, sysdir / "NWindow.dll")
    for ext in (".map", ".pdb"):
        side = dll.with_suffix(ext)
        if side.exists():
            shutil.copy2(side, sysdir / ("NWindow" + ext))
        elif (sysdir / ("NWindow" + ext)).exists():
            (sysdir / ("NWindow" + ext)).unlink()
    for old in sysdir.glob("NWindowDecomp.*"):
        old.unlink()
    stock = hashlib.md5(dll.read_bytes()).hexdigest() == STOCK_MD5
    print(f"installed into {sysdir}:")
    print(f"  NWindow.dll = {dll}" + (" (the stock C4 build)" if stock else ""))
    print("  no NWindowDecomp.dll")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--system", type=Path, default=DEFAULT_SYSTEM)
    ap.add_argument("--restore", action="store_true")
    ap.add_argument("--build", type=Path, default=ROOT / "build" / "bin" / "Release")
    ap.add_argument("--standalone", type=Path, nargs="?", const=STANDALONE_DLL, metavar="DLL",
                    help="install a NWindow.dll that runs on its own (only under build/)")
    args = ap.parse_args()
    if args.system is None:
        ap.error(NO_SYSTEM)
    sysdir: Path = args.system
    target = sysdir / "NWindow.dll"
    if sysdir.name.lower() == "system" and not args.standalone:
        print(f"FAIL: {sysdir} looks like the C4 client's own System folder - never modified; install into "
              f"a copy of it (xcopy /E /I /Y <client>\\system <client>\\system_nwtest)")
        return 1
    if not args.restore and not sysdir.exists() and not args.standalone:
        print(f"FAIL: {sysdir} does not exist - create the test client first: xcopy /E /I /Y <client>\\system \"{sysdir}\"")
        return 1
    if args.standalone:
        return install_standalone(sysdir, args.standalone)

    running = not writable(target)
    if running and args.restore:
        print(f"FAIL: {target} is in use - close the game first")
        return 1

    if args.restore:
        bak = sysdir / BACKUP
        if not bak.exists():
            print(f"FAIL: no {bak}")
            return 1
        shutil.copy2(bak, target)
        print(f"restored {target} from {bak.name}")
        return 0

    stock = (ROOT / "orig" / "NWindow.dll").read_bytes()
    if hashlib.md5(stock).hexdigest() != STOCK_MD5:
        print(f"FAIL: orig/NWindow.dll is not the stock C4 build (md5 {STOCK_MD5})")
        return 1
    dll = args.build / "NWindowDecomp.dll"
    if not dll.exists():
        print(f"FAIL: {dll} missing - build first (cmake --build build --config Release)")
        return 1
    imports = [e.dll.decode().lower() for e in pefile.PE(str(dll)).DIRECTORY_ENTRY_IMPORT]
    bad = [i for i in imports if i.startswith(("vcruntime", "msvcp", "api-ms-win-crt", "ucrtbase"))]
    if bad:
        print(f"FAIL: NWindowDecomp.dll needs the VC++ runtime ({bad}); build with the static CRT")
        return 1

    patched = add_import(stock, "NWindowDecomp.dll", "NWindowDecomp_Attach")
    verify_added(stock, patched, "NWindowDecomp.dll", "NWindowDecomp_Attach")
    if running:
        if target.read_bytes() != patched:
            print(f"FAIL: {target} is in use and is not the patched stock DLL - close the game first")
            return 1
        print("game running: NWindow.dll already patched, left as is")
    else:
        if target.exists() and not (sysdir / BACKUP).exists():
            shutil.copy2(target, sysdir / BACKUP)
            print(f"backup: {BACKUP}")
        target.write_bytes(patched)
    for old in sysdir.glob("NWindowDecomp.*.in-use-*"):
        try:
            old.unlink()
        except OSError:
            pass  # still mapped by a running game
    print("  " + replace_file(dll, sysdir / "NWindowDecomp.dll"))
    pdb = args.build / "NWindowDecomp.pdb"
    if pdb.exists():
        print("  " + replace_file(pdb, sysdir / "NWindowDecomp.pdb"))
    if not (sysdir / "NWindowDecomp.ini").exists():
        (sysdir / "NWindowDecomp.ini").write_text(INI, encoding="ascii")
    # the names of the stock functions for [Debug] LogCalls= (read only when it is set): `va name` per line
    names = []
    fcsv = ROOT / "db" / "functions.csv"
    if fcsv.exists():
        import csv as _csv
        for r in _csv.DictReader(fcsv.open(encoding="utf-8")):
            if r["kind"] in ("game", "init") and r["name"]:
                names.append("%s %s" % (r["va"], r["name"]))
    (sysdir / "NWindowDecomp.names").write_text("\n".join(names) + "\n", encoding="ascii", errors="replace")
    print(f"installed into {sysdir}:")
    print("  NWindow.dll       = stock + import NWindowDecomp.dll")
    print(f"  NWindowDecomp.dll = {dll}")
    print("  log after a run   : NWindowDecomp.log")
    return 0


if __name__ == "__main__":
    sys.exit(main())
