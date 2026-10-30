#!/usr/bin/env python3
"""Stage 4 lanes (the C4 port by hand): what the C3 -> C4 pipeline could not copy, cut into lanes that do not share files.

  python tools/port/lanes_s4.py            write db/lanes_s4.json + db/anon_lane_s4.csv and print the table
  python tools/port/lanes_s4.py --hand     also append every lane file to db/hand_owned.txt (once, before the agents start)

The work = the `todo` functions of db/functions.csv (kind game / init): the stubs the generator left (category D, candidates that do not match,
judge demotions) and the C4-only functions it never saw (new methods, the 36 new classes). Out on purpose: the GameGuard `NPG*` (stays stock),
every `scalar deleting destructor` / `InternalConstructor` (never hooked).

A lane = the classes of a family (regexes below, first match wins) plus, for the long tail, contiguous alphabetical chunks of the remaining
classes (a prefix family such as NCManor* stays together). A source file belongs to one lane only: when two lanes both have unported functions
in a file, the smaller class moves to the lane that owns more of the file. An anonymous function (`sub_XXXXXXXX`) goes to the lane of the class
whose methods call it (db/anon_owner.csv), propagated through anonymous callers; a data-only or shared one goes to the lane of its neighbours.
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

OUT = re.compile(r"^(NPG)")                       # GameGuard: stays stock
NAME_SKIP = re.compile(r"deleting destructor|InternalConstructor|_ICF|#thunk")

# lane -> class regexes (first match wins, in this order); a class that matches none goes to the alphabetical chunks `rest*`
LANES = [
    ("nconsole", r"^(NConsoleWnd|NConsoelWnd)"),
    ("input", r"^(NCConsole$|NCChatWnd|NCChatFilter|NCEditBox|NCMultiEdit|NCHtmlMultiEdit|NCChineseIME|NCCandidateBox|NCIME|NCThai|"
              r"NCJapanese|NCKorean|NLineList|NCVirtualWndMain|NCMinimapWnd|NCShortCutWnd|NShortCutItem|NCMainWnd|NCGDevice)"),
    ("base", r"^(NCWnd$|NControl|NCObject|NCCmdTarget|NCFrameCtrl|NCButton|NCListCtrl|NCListBox|NCComboBox|NCScrollWnd|NCTabControl|"
             r"NCHeaderCtrl|NCSelectCtrl|NCSliderBar|NCSliderCtrl|NCCheckBox|NCCountSpin|NCProgressCtrl|NCRadioButton|NCPushButton|"
             r"NCExButton|NCThumbButton|NCStatusBarCtrl|NCCompositeBox|NCDynamicLinker|NCFileLinker|NCActionLinker|NStd|NStl|NString|"
             r"NPoint|Util|NL2Font|Shared|UGlobal|TWndEntry|TRadioButton|FL2|NKeyCrypt|NCKeyCrypt)"),
    ("msn", r"^(Msn|MSN|NMsn|Switchboard|NotificationProcess|DispatchProcess|ChatWnd|ChatBox|ChatFrameCtrl|FriendBox|FriendList|"
            r"InviteBox|InviteWnd|Task|L2Chat|L2Task|L2Word|L2Friend|NChat|AddFriendDialog|AddGroupDialog|AddedConfirmDialog|"
            r"MoveGroupDialog|RemoveFriendDialog|RemoveGroupDialog|RenameGroupDialog|OfflineConfirmDialog|MessageDialog|AlarmWnd|"
            r"LoadingWnd|LoginWnd|StatusWnd|OptionWnd|VersionWnd|MessengerWnd)"),
    ("bgm", r"^(NCBroadcast|NBroadcast|NCReplay|NCObserver|NCBenchMark)"),
    ("pawn", r"^(NCPawn|NCNpcView|NCMSViewr|NCViewport|NSimPawn|NCCharacter|NCSelectChar)"),
    ("status", r"^(NCPledge|NCDetailStatus|NCGMDetailStatus|NCGM|NCGod|NCPartyRoom|NCParty|NCPet|NCMatch|NCAbnormal|NCTargetStatus|"
               r"NCPlayerStatus|NCSummoned|NCStatus|NCHenna|NCEmpower|NCTrain|NCSkill|NCMagicSkill|NCAction|NCCoolTime|NCClan|NCAlliance)"),
    ("items", r"^(NCEquipItemWnd|NCShop|NCPrivate|NCStore|NCTrade|NCInventory|NCItem|NToolTip|NCVIPShop|NVIPShop|NCManor|NCRecipe|"
              r"NCMultiSell|NCWareHouse|NCWarehouse|NCFreight|NCDeliver|NCSelectDeliver|NCBBS|NCHtml|NHtml|NCTutorial|NCNpcHtml|"
              r"NCNPCHtml|NCMacro|NMacro|NCCalc|NCBoard|NCEnchant|NCCrystal|NCAugment|NCSeed|NCCrop)"),
    ("sys", r"^(NCVideoOption|NCGameOption|NCAudioOption|NCOption|NCServerSelect|NCLogin|NCCredit|NCEula|NCGaraDialogBox|NCNotice|"
            r"NCAnnounce|NCXMas|NCLoading|NCAuth|NCRestart|NCZoneTitle|NCTownMap|NCMapCover|NCQuest|NCSSQ|NCSiege|NCGaraT|NCTree|"
            r"NCTVItem|NDialogInfo)"),
]
FIXED = [n for n, _ in LANES]
TAIL_CHUNKS = 1          # alphabetical chunks of the classes no family claims
SPLIT_LIMIT = 80000      # a family heavier than this (bytes) is cut into parts
SPLIT_KEEP = {"nconsole"}   # one class (NConsoleWnd) over 13 files and one header: one agent


def family(cls: str):
    for name, pat in LANES:
        if re.match(pat, cls):
            return name
    return None


def main(argv) -> int:
    rows = list(csv.DictReader(open(ROOT / "db" / "functions.csv", newline="", encoding="utf-8", errors="replace")))
    anon = {r["va"]: r for r in csv.DictReader(open(ROOT / "db" / "anon_owner.csv", newline="", encoding="utf-8"))}
    gen = list(csv.DictReader(open(ROOT / "db" / "gen_report.csv", newline="", encoding="utf-8")))
    todo = [r for r in rows if r["status"] == "todo" and r["kind"] in ("game", "init") and not NAME_SKIP.search(r["name"])]
    named = [r for r in todo if not r["name"].startswith("sub_") and not OUT.match(r["class"] or "(free)")]
    anon_todo = {r["va"]: r for r in todo if r["name"].startswith("sub_")}

    # ---- files per class (every unit the generator wrote for the class, whatever its action) ----------------
    files_of = defaultdict(set)
    for g in gen:
        files_of[g["class"]].add("src/nwindow/" + g["file"])
    for c in list(files_of):
        if (ROOT / "include" / "nwindow" / (c + ".h")).exists():
            pass

    # ---- classes and their weights ------------------------------------------------------------------------
    weight = Counter()
    for r in named:
        weight[r["class"] or "(free)"] += int(r["size"])
    for va, a in anon.items():
        oc = a.get("owner_class")
        if oc and va in anon_todo and not OUT.match(oc):
            weight[oc] += int(anon_todo[va]["size"])
    classes = sorted(weight)

    cls_lane = {}
    rest = []
    for c in classes:
        f = family(c)
        if f:
            cls_lane[c] = f
        else:
            rest.append(c)
    total_rest = sum(weight[c] for c in rest)
    # alphabetical chunks of ~equal weight; a prefix family (NCManor*) is not split in the middle when avoidable
    chunk_names = ["rest%d" % (i + 1) for i in range(TAIL_CHUNKS)]
    acc, k = 0, 0
    for c in rest:
        cls_lane[c] = chunk_names[min(k, TAIL_CHUNKS - 1)]
        acc += weight[c]
        if acc >= total_rest * (k + 1) / TAIL_CHUNKS and k < TAIL_CHUNKS - 1:
            k += 1

    # ---- a family heavier than SPLIT_LIMIT is cut into alphabetical parts (a prefix family stays together) --
    fam_w = Counter()
    for c in classes:
        fam_w[cls_lane[c]] += weight[c]
    split_names = {}
    for ln in FIXED:
        if ln in SPLIT_KEEP or fam_w[ln] <= SPLIT_LIMIT:
            continue
        parts = -(-fam_w[ln] // SPLIT_LIMIT)
        members = [c for c in classes if cls_lane[c] == ln]
        acc, k = 0, 0
        for c in members:
            cls_lane[c] = "%s%d" % (ln, min(k, parts - 1) + 1)
            acc += weight[c]
            if acc >= fam_w[ln] * (k + 1) / parts and k < parts - 1:
                k += 1
        split_names[ln] = ["%s%d" % (ln, i + 1) for i in range(parts)]

    # ---- a file belongs to one lane: classes that share a file with todo in both move to the bigger lane ----
    todo_classes = set(classes)
    for _ in range(12):
        changed = False
        by_file = defaultdict(Counter)
        for c in todo_classes:
            for f in files_of.get(c, ()):
                by_file[f][cls_lane[c]] += weight[c]
        for f, lanes_here in by_file.items():
            if len(lanes_here) <= 1:
                continue
            win = lanes_here.most_common(1)[0][0]
            for c in todo_classes:
                if f in files_of.get(c, ()) and cls_lane[c] != win:
                    cls_lane[c] = win
                    changed = True
        if not changed:
            break

    name_lane = {r["name"]: cls_lane.get(r["class"] or "(free)") for r in named}

    # ---- neighbours in the image for the anonymous functions nobody owns ------------------------------------
    allr = sorted(rows, key=lambda r: int(r["va"], 16))
    starts = [int(r["va"], 16) for r in allr]

    def region_lane(va):
        i = bisect.bisect_right(starts, int(va, 16)) - 1
        for j in range(i, max(i - 40, -1), -1):
            r = allr[j]
            if not r["name"].startswith("sub_") and r["class"]:
                if OUT.match(r["class"]):
                    return "(stock)"
                if r["class"] in cls_lane:
                    return cls_lane[r["class"]]
        return "nconsole"

    lane, why = {}, {}
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
                    lane[va], why[va] = cls_lane.get(oc) or family(oc) or "nconsole", oc
                changed = True
                continue
            votes = Counter()
            for cn in (a.get("callers") or "").split(";"):
                if not cn or cn == "data":
                    continue
                l = lane.get(cn[4:]) if cn.startswith("sub_") else name_lane.get(cn)
                if l:
                    votes[l] += 1
            if len(votes) == 1:
                lane[va], why[va] = next(iter(votes)), "callers"
                changed = True
        if not changed:
            break
    for va in anon_todo:
        if va not in lane:
            lane[va], why[va] = region_lane(va), "neighbours"

    # ---- tally ---------------------------------------------------------------------------------------------
    order = []
    for ln in FIXED:
        order += split_names.get(ln, [ln])
    order += chunk_names
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
    for c in classes:                                   # classes carried only by anonymous functions
        lane_classes[cls_lane[c]].setdefault(c, 0)

    lanes = {}
    for ln in order:
        fs = set()
        for c in lane_classes[ln]:
            fs.update(files_of.get(c, ()))
            h = "include/nwindow/%s.h" % c
            if (ROOT / h).exists():
                fs.add(h)
        lanes[ln] = {
            "classes": sorted(lane_classes[ln]),
            "files": sorted(fs),
            "functions": lane_n[ln],
            "bytes": lane_b[ln],
            "anonymous": sorted(va for va in anon_todo if lane[va] == ln),
        }
    (ROOT / "db" / "lanes_s4.json").write_text(json.dumps(lanes, indent=1), encoding="utf-8")
    with open(ROOT / "db" / "anon_lane_s4.csv", "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["va", "size", "lane", "why"])
        for va in sorted(anon_todo):
            w.writerow([va, anon_todo[va]["size"], lane[va], why[va]])
    for ln, v in lanes.items():
        print("%-9s %4d functions %7d B  %3d classes %3d files %3d anonymous" % (
            ln, v["functions"], v["bytes"], len(v["classes"]), len(v["files"]), len(v["anonymous"])))
    stock = [va for va in anon_todo if lane[va].startswith("(")]
    print("anonymous left stock (NPG): %d, %d B" % (len(stock), sum(int(anon_todo[v]["size"]) for v in stock)))
    print("total in lanes: %d functions, %d B" % (sum(lane_n.values()), sum(lane_b.values())))
    multi = defaultdict(list)
    for ln, v in lanes.items():
        for f in v["files"]:
            multi[f].append(ln)
    bad = {f: l for f, l in multi.items() if len(l) > 1}
    print("files in more than one lane:", bad or "none")

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
                    f.write("# stage 4 lane %s\n" % ln)
                    cur = ln
                f.write(fpath + "\n")
        print("hand_owned.txt: %d files appended" % len(add))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
