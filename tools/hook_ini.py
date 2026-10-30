#!/usr/bin/env python3
"""NWindowDecomp.ini of the test folder, made easy: which of our functions are live.

The game is tested by opening it from `system_nwtest`; when something is wrong the quickest way to
the function is to turn hooks off by class and look again. This writes the ini for that.

  python tools/hook_ini.py list                 the classes with hooks (from NWindowDecomp.log), count each
  python tools/hook_ini.py all                  every hook on (Enabled=1, Disable= empty)
  python tools/hook_ini.py none                 every hook off (Enabled=0): the stock NWindow.dll alone
  python tools/hook_ini.py off  NCWnd NCButton  these classes off, the rest on
  python tools/hook_ini.py only NCWnd NCButton  only these classes on, every other off
  python tools/hook_ini.py half 1|2             bisection: the first / second half (by class name) of the
                                                classes that have hooks stays on, the other half goes off
  python tools/hook_ini.py addr 1002fa20 ...    single stock addresses off
  python tools/hook_ini.py log NCActionItemWnd::* NConsoleWnd::Request*   every hook on, and every call of the named
                                                functions (hooked or stock) written to the log as `[call] ...`
                                                (any mode takes --log PATTERN... and --logmax N too)

Add --marker to turn on the proof marks ([Debug] Marker=1). Run it with the game closed; the ini is read
when the game starts. A function name works as a token too (`NCWnd::OnCreate`).
"""
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TEST = ROOT.parent / "C4_Client" / "system_nwtest"


def hook_classes():
    # the newest of the two: the game's own log (a run of an older build lists fewer classes) and the one of the
    # cycle's smoke run, which always belongs to the installed build
    logs = [p for p in (TEST / "NWindowDecomp.log", ROOT / "build" / "smoke_system" / "NWindowDecomp.log") if p.exists()]
    if not logs:
        sys.exit("no NWindowDecomp.log: run the game once (or tools/cycle.py) first")
    log = max(logs, key=lambda p: p.stat().st_mtime)
    count = {}
    for ln in log.read_text(encoding="latin-1").splitlines():
        m = re.match(r"^\S+\s+([0-9a-f]{8}) (\S+).*\bok\b", ln)
        if m and "ours=" in ln:
            n = m.group(2)
            c = n.split("::")[0] if "::" in n else "(free)"
            count[c] = count.get(c, 0) + 1
    return count, log


LOGCALLS: list[str] = []
LOGMAX = 300


def write_ini(enabled: bool, disable: list[str], marker: bool) -> None:
    ini = TEST / "NWindowDecomp.ini"
    text = ("; NWindowDecomp.ini - written by tools/hook_ini.py\n"
            "[Hooks]\nEnabled=%d\nDisable=%s\n\n[Debug]\nMarker=%d\n" % (1 if enabled else 0, " ".join(disable),
                                                                         1 if marker else 0))
    if LOGCALLS:
        # every call of these functions (hooked or stock) is written to NWindowDecomp.log as `[call] ...`
        text += "LogCalls=%s\nLogMax=%d\n" % (" ".join(LOGCALLS), LOGMAX)
    ini.write_text(text, encoding="ascii")
    print("wrote %s: Enabled=%d, %d token(s) in Disable=" % (ini, 1 if enabled else 0, len(disable)))
    if len(" ".join(disable)) > 4000:
        print("warning: Disable= is %d characters; the reader keeps 4095 (src/re/NwLog.h)" % len(" ".join(disable)))


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("mode", choices=["list", "all", "none", "off", "only", "half", "addr", "log"])
    ap.add_argument("items", nargs="*")
    ap.add_argument("--marker", action="store_true")
    ap.add_argument("--log", nargs="+", metavar="PATTERN", help="also write [Debug] LogCalls= (NWindowDecomp.names "
                    "next to the dll gives the stock functions their names): Class::Method, Class::*, a prefix "
                    "ending in *, or a stock address")
    ap.add_argument("--logmax", type=int, default=300, help="calls logged per function (default 300)")
    a = ap.parse_args()
    global LOGCALLS, LOGMAX
    LOGMAX = a.logmax
    if a.log:
        LOGCALLS = list(a.log)
    if a.mode == "list":
        count, log = hook_classes()
        print("%s: %d classes, %d hooks" % (log, len(count), sum(count.values())))
        for c in sorted(count, key=lambda k: -count[k]):
            print("  %-34s %4d" % (c, count[c]))
        return 0
    if a.mode == "log":
        LOGCALLS = list(a.items) + LOGCALLS
        write_ini(True, [], a.marker)
    elif a.mode == "all":
        write_ini(True, [], a.marker)
    elif a.mode == "none":
        write_ini(False, [], a.marker)
    elif a.mode == "off":
        write_ini(True, [(i if "::" in i or re.fullmatch(r"(0x)?[0-9a-fA-F]{8}", i) else i + "::*") for i in a.items], a.marker)
    elif a.mode == "addr":
        write_ini(True, a.items, a.marker)
    elif a.mode == "only":
        count, _ = hook_classes()
        keep = set(a.items)
        write_ini(True, [c + "::*" for c in sorted(count) if c not in keep and c != "(free)"], a.marker)
    elif a.mode == "half":
        count, _ = hook_classes()
        names = sorted(c for c in count if c != "(free)")
        k = len(names) // 2
        keep = set(names[:k] if a.items and a.items[0] == "1" else names[k:])
        write_ini(True, [c + "::*" for c in names if c not in keep], a.marker)
        print("classes kept on: %s ... (%d of %d)" % (", ".join(sorted(keep)[:6]), len(keep), len(names)))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
