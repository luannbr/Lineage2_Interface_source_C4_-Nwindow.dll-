#!/usr/bin/env python3
"""Wave 3 leftovers lane (`left`): what the seven lanes reported as not done, minus what stays stock on purpose.

  python tools/port/lane_left.py        write db/lane_left.csv (va,kind,name,size,class,why) and print a summary

Leaves out: the messenger / broadcast / observer / replay / GameGuard families, the NConsoleWnd constructor and
destructor, every `scalar deleting destructor` and InternalConstructor (never hooked), and everything in the files of
the `shophtml` lane (the shophtml2 agent owns them).
"""
from __future__ import annotations

import csv
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

OUT = re.compile(r"^(Msn|MSN|Messenger|Switchboard|ChatWnd|ChatBox|ChatFrameCtrl|L2|Friend|Invite|Task|AddFriend|AddGroup|"
                 r"AddedConfirm|MoveGroup|RemoveFriend|RemoveGroup|RenameGroup|OfflineConfirm|MessageDialog|AlarmWnd|"
                 r"LoadingWnd|LoginWnd|StatusWnd|OptionWnd|VersionWnd|NPG|NCBroadcast|NCReplay|DispatchProcess|"
                 r"NCObserverWnd|NCBenchMarkResultWnd|Util)")
SKIP_NAME = re.compile(r"deleting destructor|InternalConstructor|NConsoleWnd::(NConsoleWnd|~NConsoleWnd|InitializeReplayMode|"
                       r"SwitchUIMode|OnEventMatchObserver|InitReplayMode)|_ICF|operator new|#thunk")


def main() -> int:
    rows = list(csv.DictReader(open(ROOT / "db" / "functions.csv", newline="", encoding="utf-8", errors="replace")))
    lanes = json.load(open(ROOT / "db" / "lanes_wave3.json", encoding="utf-8"))
    shop_files = set(lanes["shophtml"]["files"])
    shop_classes = set(lanes["shophtml"]["classes"])
    anon_lane = {r["va"]: r for r in csv.DictReader(open(ROOT / "db" / "anon_lane.csv", newline="", encoding="utf-8"))}
    src_of = {}
    for r in rows:
        if r["class"] and r["src"]:
            src_of.setdefault(r["class"], set()).add(r["src"])
    todo = [r for r in rows if r["status"] == "todo" and r["kind"] in ("game", "init")]
    out = []
    for r in todo:
        va, name, s, c = r["va"], r["name"], int(r["size"]), r["class"]
        if SKIP_NAME.search(name):
            continue
        if name.startswith("sub_"):
            a = anon_lane.get(va)
            if not a or a["lane"] in ("(stock)", "(none)", "shophtml"):
                continue
            if OUT.match(a["why"]) or re.search(r"Msn|Friend|L2|Broadcast|Observer|Replay|ChatWnd::ChatWnd", a["why"]):
                continue
            if int(va, 16) >= 0x10125000 and a["why"] == "NConsoleWnd":
                continue               # the messenger / broadcast helpers that only NConsoleWnd's stock code reaches
            out.append((va, "anon", name, s, "", a["why"][:60]))
        else:
            if OUT.match(c or "(free)") or c in shop_classes:
                continue
            if set(src_of.get(c, ())) & shop_files:
                continue
            out.append((va, "named", name, s, c, ""))
    with open(ROOT / "db" / "lane_left.csv", "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["va", "kind", "name", "size", "class", "why"])
        for x in sorted(out):
            w.writerow(x)
    n = sum(1 for x in out if x[1] == "named")
    print("%d named + %d anonymous = %d functions, %d B -> db/lane_left.csv" % (n, len(out) - n, len(out), sum(x[3] for x in out)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
