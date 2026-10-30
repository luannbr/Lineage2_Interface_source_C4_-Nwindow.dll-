#!/usr/bin/env python3
"""Wave 5 lanes (the residue of wave 4): the C3-only families that waves 1-3 left stock (broadcast / observer / replay, the messenger and its
chat windows), cut into lanes that do not share files.

  python tools/port/lanes_wave5.py            write db/lanes_wave5.json + db/anon_lane5.csv and print the table
  python tools/port/lanes_wave5.py --hand     also append the lane classes' files to db/hand_owned.txt (once)

None of these classes has a C2 source: the lanes write new `.h` / `.cpp` files from the C3 binary (the class names come
from RTTI, the vtables from analysis/classes.json). Out of the wave on purpose: the GameGuard `NPG*` (a vendor library,
nProtect's, linked into the DLL: it stays stock) and every `scalar deleting destructor` (never hooked).

An anonymous function (`sub_XXXXXXXX`) goes to the lane of the classes that call it (db/anon_owner.csv, propagated
through anonymous callers); one that only data tables or several lanes reach goes to the lane of its neighbours in the
image (the compiler emits the helpers of a unit next to its methods).
"""
from __future__ import annotations

import bisect
import csv
import json
import re
import sys
from collections import Counter, defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

# lane -> class regexes (first match wins, in this order)
LANES = [
    ("bgm", r"^(NCBroadcastGM)"),
    ("bobs", r"^(NCBroadcastObserver|NCBroadcast|NCObserverWnd|NCReplay|NCBenchMark|NCLoginWnd$)"),
    ("dlg", r"^(MessageDialog|AddFriendDialog|AddGroupDialog|AddedConfirmDialog|MoveGroupDialog|RemoveFriendDialog|"
            r"RemoveGroupDialog|RenameGroupDialog|OfflineConfirmDialog)"),
    ("chat", r"^(ChatWnd|ChatBox|ChatFrameCtrl|FriendBox|InviteBox|InviteWnd|Task|TaskBox|TaskWnd|L2Chat|L2Task|L2Word|"
            r"L2Friend$)"),
    ("msn1", r"^(Msn$|MSNSocket|SwitchboardSession|DispatchProcess|FriendList|MSN)"),
    ("msn2", r"^(MsnWnd|MsnTree|MessengerWnd|LoginWnd|OptionWnd|StatusWnd|AlarmWnd|LoadingWnd|VersionWnd|L2Friend|Friend)"),
]
ORDER = ["bgm", "bobs", "dlg", "chat", "msn1", "msn2", "core"]
OUT = re.compile(r"^(NPG)")                 # stays stock (vendor library)
NAME_SKIP = re.compile(r"deleting destructor|InternalConstructor|_ICF|#thunk")
CORE_CLASSES = re.compile(r"^(NConsoleWnd|NConsoelWnd)$")


def lane_of(cls: str) -> str:
    for name, pat in LANES:
        if re.match(pat, cls):
            return name
    return "core"


def main(argv) -> int:
    rows = list(csv.DictReader(open(ROOT / "db" / "functions.csv", newline="", encoding="utf-8", errors="replace")))
    anon = {r["va"]: r for r in csv.DictReader(open(ROOT / "db" / "anon_owner.csv", newline="", encoding="utf-8"))}
    todo = [r for r in rows if r["status"] == "todo" and r["kind"] in ("game", "init") and not NAME_SKIP.search(r["name"])]
    named = [r for r in todo if not r["name"].startswith("sub_") and not OUT.match(r["class"] or "(free)")]

    # ---- the class lane of every named function ---------------------------------------------------------
    cls_lane = {}
    for r in named:
        c = r["class"] or "(free)"
        cls_lane[c] = lane_of(c)
    name_lane = {r["name"]: cls_lane.get(r["class"] or "(free)") for r in named}

    # ---- neighbours in the image (any status) for the anonymous functions nobody calls directly ----------
    allr = sorted(rows, key=lambda r: int(r["va"], 16))
    starts = [int(r["va"], 16) for r in allr]

    def region_lane(va):
        i = bisect.bisect_right(starts, int(va, 16)) - 1
        for j in range(i, max(i - 40, -1), -1):
            r = allr[j]
            if not r["name"].startswith("sub_") and r["class"]:
                c = r["class"]
                if OUT.match(c):
                    return "(stock)"
                l = lane_of(c)
                if l != "core" or CORE_CLASSES.match(c):
                    return l
        return "core"

    # ---- anonymous: the lanes of the callers, propagated -------------------------------------------------
    anon_todo = {r["va"]: r for r in todo if r["name"].startswith("sub_")}
    lane = {}
    why = {}
    for _ in range(14):
        changed = False
        for va, r in anon_todo.items():
            if va in lane:
                continue
            a = anon.get(va, {})
            oc = a.get("owner_class", "")
            if oc:
                if OUT.match(oc):
                    lane[va], why[va] = "(stock)", oc
                else:
                    lane[va], why[va] = lane_of(oc), oc
                changed = True
                continue
            votes = Counter()
            for cn in (a.get("callers") or "").split(";"):
                if not cn or cn == "data":
                    continue
                if cn.startswith("sub_"):
                    l = lane.get(cn[4:])
                    if l:
                        votes[l] += 1
                else:
                    l = name_lane.get(cn)
                    if l:
                        votes[l] += 1
            if len(votes) == 1:
                l = next(iter(votes))
                lane[va], why[va] = l, "callers"
                changed = True
        if not changed:
            break
    for va in anon_todo:
        if va not in lane:
            lane[va], why[va] = region_lane(va), "neighbours"

    # ---- tally ---------------------------------------------------------------------------------------------
    lane_classes = defaultdict(Counter)
    lane_n, lane_b = Counter(), Counter()
    for r in named:
        c = r["class"] or "(free)"
        ln = cls_lane[c]
        lane_classes[ln][c] += int(r["size"])
        lane_n[ln] += 1
        lane_b[ln] += int(r["size"])
    for va, r in anon_todo.items():
        ln = lane[va]
        if ln.startswith("("):
            continue
        lane_n[ln] += 1
        lane_b[ln] += int(r["size"])

    src_of = defaultdict(set)
    for r in rows:
        if r["class"] and r["src"]:
            src_of[r["class"]].add(r["src"])
    files = defaultdict(set)
    for ln, cs in lane_classes.items():
        for c in cs:
            files[ln].update(src_of.get(c, ()))
            for h in ("include/nwindow/%s.h" % c,):
                if (ROOT / h).exists():
                    files[ln].add(h)
    shared = "src/nwindow/NConsoleWndMisc.cpp"          # one-liners of many classes, ported: only `core` may edit it
    for ln in files:
        if ln != "core":
            files[ln].discard(shared)
    lanes = {}
    for ln in ORDER:
        lanes[ln] = {
            "classes": sorted(lane_classes[ln]),
            "files": sorted(files[ln]),
            "functions": lane_n[ln],
            "bytes": lane_b[ln],
            "anonymous": sorted(va for va in anon_todo if lane[va] == ln),
        }
    (ROOT / "db" / "lanes_wave5.json").write_text(json.dumps(lanes, indent=1), encoding="utf-8")
    with open(ROOT / "db" / "anon_lane5.csv", "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["va", "size", "lane", "why"])
        for va in sorted(anon_todo):
            w.writerow([va, anon_todo[va]["size"], lane[va], why[va]])
    for ln, v in lanes.items():
        print("%-6s %4d functions %7d B  %3d classes %3d files %3d anonymous" % (
            ln, v["functions"], v["bytes"], len(v["classes"]), len(v["files"]), len(v["anonymous"])))
    if "--hand" in argv:
        have = set(l.strip() for l in open(ROOT / "db" / "hand_owned.txt", encoding="utf-8")
                   if l.strip() and not l.startswith("#"))
        add = []
        for ln, v in lanes.items():
            for fpath in v["files"]:
                if fpath not in have:
                    add.append((ln, fpath))
                    have.add(fpath)
        with open(ROOT / "db" / "hand_owned.txt", "a", encoding="utf-8") as f:
            cur = None
            for ln, fpath in add:
                if ln != cur:
                    f.write("# wave 5 lane %s\n" % ln)
                    cur = ln
                f.write(fpath + "\n")
        print("hand_owned.txt: %d files appended" % len(add))
    stock = [va for va in anon_todo if lane[va].startswith("(")]
    print("anonymous left stock (NPG): %d, %d B" % (len(stock), sum(int(anon_todo[v]["size"]) for v in stock)))
    multi = defaultdict(list)
    for ln, v in lanes.items():
        for f in v["files"]:
            multi[f].append(ln)
    bad = {f: l for f, l in multi.items() if len(l) > 1}
    print("files in more than one lane:", bad or "none")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
