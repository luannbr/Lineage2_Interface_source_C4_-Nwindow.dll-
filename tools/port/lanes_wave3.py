#!/usr/bin/env python3
"""Wave 3 lanes: the classes that still have unported functions, cut into lanes that do not share files.

  python tools/port/lanes_wave3.py            write db/lanes_wave3.json + db/anon_lane.csv and print the table
  python tools/port/lanes_wave3.py --hand     also append every lane file to db/hand_owned.txt (once, before the agents start)

Out of this wave on purpose (stay stock stubs): the messenger (Msn*, MessengerWnd, Switchboard*, the L2*/Friend*/Task*
families, ChatWnd...), the GameGuard NPG*, the broadcast / observer / replay windows (NCBroadcast*, NCReplay*).
An anonymous function belongs to the lane of the class whose methods call it (db/anon_owner.csv, tools/port/
anon_owner.py); the shared ones go to the lane that calls them most, and only that lane ports them. A source file
belongs to one lane only: a class whose file another lane owns moves to that lane. A class whose only unported
functions are destructors goes to the `sys` lane unless its file is claimed by another lane.
"""
from __future__ import annotations

import csv
import json
import re
import sys
from collections import Counter, defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

OUT = re.compile(r"^(Msn|MSN|Messenger|Switchboard|ChatWnd|ChatBox|ChatFrameCtrl|L2|Friend|Invite|Task|AddFriend|AddGroup|"
                 r"AddedConfirm|MoveGroup|RemoveFriend|RemoveGroup|RenameGroup|OfflineConfirm|MessageDialog|AlarmWnd|"
                 r"LoadingWnd|LoginWnd|StatusWnd|OptionWnd|VersionWnd|NPG|NCBroadcast|NCReplay|DispatchProcess)")

# lane -> class name regexes (first match wins, in this order); everything else is `console`
LANES = [
    ("nconsole", r"^(NConsoleWnd|NConsoelWnd)"),
    ("gm", r"^(NCGM|NCGod|NCPetition|NCPetitionWnd|NCGMPetition|NCSkillWnd|NCMagicSkillWnd)"),
    ("recipe", r"^(NCRecipe|NCMacro|NMacroEntry|NCCalc)"),
    ("trees", r"^(NCGaraT|NCTree|NCTVItem|NCQuestGara|NCQuestItem|NCQuestInfo|NCQuestWnd|NCSSQ|NCSiege|NCTownMap|NCMapCover|NCTabControl|NCHeaderCtrl|NCScrollWnd|NCSelectCtrl)"),
    ("shophtml", r"^(NCVIPShop|NVIPShop|NCDeliver|NCSelectDeliver|NCHtml|NHtml|NCBBS|NCTutorial|NCTestHtml|NCNPCHtml|NCNpcHtml|NCBoard|NCCompositeBox|NCDynamicLinker|NCFileLinker|NCActionLinker|NCSKill|NCActionItem|NCActionWnd|NActionItem|NCHenna|NCEquipItemWnd|NCItemWnd|NCCoolTimeIcon|NCSliderBar|NCSliderCtrl|NCCheckBox|NCCountSpin|NCProgressCtrl|NCRadioButton|NCPushButton|NCExButton|NCThumbButton|NCStatusBarCtrl)"),
    ("sys", r"^(NCLogin|NCServer|NCLoading|NCAuth|NCCredit|NCEula|NCOption|NCVideoOption|NCAudioOption|NCGameOption|NCNotice|NCAnnounce|NCBenchMark|NCXMas|NCKeyCrypt|NKeyCrypt|NCThai|NCChinese|NCJapanese|NCKorean|NCIME|NCCandidateBox|NCMultiEdit|NStd|NStl|NString|NPoint|NSimPawn|Util|NL2Font|NControl|NCObject|NCCmdTarget|Shared|UGlobal|TWndEntry|TRadioButton|FL2)"),
]
ORDER = ["nconsole", "gm", "trees", "recipe", "shophtml", "sys", "console"]
DTOR = re.compile(r"::~")


def lane_of(cls: str) -> str:
    for name, pat in LANES:
        if re.match(pat, cls):
            return name
    return "console"


def main(argv) -> int:
    rows = list(csv.DictReader(open(ROOT / "db" / "functions.csv", newline="", encoding="utf-8", errors="replace")))
    anon = {r["va"]: r for r in csv.DictReader(open(ROOT / "db" / "anon_owner.csv", newline="", encoding="utf-8"))}
    todo = [r for r in rows if r["status"] == "todo" and r["kind"] in ("game", "init")
            and "deleting destructor" not in r["name"]]      # those are never hooked: the stock one calls our dtor

    # ---- the class of every named function, and which classes have more than a destructor to do --------
    named = [r for r in todo if not r["name"].startswith("sub_") and not OUT.match(r["class"] or "(free)")]
    rich = {r["class"] for r in named if not DTOR.search(r["name"])}
    for a in anon.values():
        if a["owner_class"] and not OUT.match(a["owner_class"]):
            rich.add(a["owner_class"])
    classes = {r["class"] or "(free)" for r in named} | {c for c in rich}
    cls_lane = {c: (lane_of(c) if c in rich else "sys") for c in classes}

    # ---- a file belongs to one lane: the class moves to the lane that owns its file ----------------------
    src_of = defaultdict(set)
    for r in rows:
        if r["class"] in classes and r["src"]:
            src_of[r["class"]].add(r["src"])
    weight = Counter()
    for r in named:
        weight[r["class"] or "(free)"] += int(r["size"])
    for a in anon.values():
        if a["owner_class"]:
            weight[a["owner_class"]] += int(a["size"])
    for _ in range(8):
        best = {}                              # file -> (weight, lane) of its heaviest class
        for c in classes:
            for f in src_of.get(c, ()):
                if f not in best or weight[c] > best[f][0]:
                    best[f] = (weight[c], cls_lane[c])
        moved = False
        for c in sorted(classes, key=lambda c: -weight[c]):
            for f in src_of.get(c, ()):
                if best[f][1] != cls_lane[c]:
                    cls_lane[c] = best[f][1]
                    moved = True
                    break
        if not moved:
            break

    lane_classes = defaultdict(Counter)
    lane_n, lane_b = Counter(), Counter()
    for r in named:
        c = r["class"] or "(free)"
        ln = cls_lane[c]
        lane_classes[ln][c] += int(r["size"])
        lane_n[ln] += 1
        lane_b[ln] += int(r["size"])

    # ---- anonymous functions: the lane of the owning class, the shared ones by the callers' lanes ---------
    cls_of_name = {r["name"]: r["class"] for r in rows}
    anon_rows = []
    for r in todo:
        if not r["name"].startswith("sub_"):
            continue
        a = anon.get(r["va"], {})
        s = int(r["size"])
        oc = a.get("owner_class", "")
        if oc:
            if OUT.match(oc):
                anon_rows.append((r["va"], s, "(stock)", oc))
                continue
            ln = cls_lane.get(oc) or lane_of(oc)
            anon_rows.append((r["va"], s, ln, oc))
        else:
            votes = Counter()
            for cn in (a.get("callers") or "").split(";"):
                c = cls_of_name.get(cn, "")
                if c and not OUT.match(c):
                    votes[cls_lane.get(c) or lane_of(c)] += 1
            if not votes:
                anon_rows.append((r["va"], s, "(none)" if not a.get("callers") else "(stock)", a.get("callers", "")))
                continue
            ln = votes.most_common(1)[0][0]
            anon_rows.append((r["va"], s, ln, "shared: " + a.get("callers", "")))
        lane_n[ln] += 1
        lane_b[ln] += s

    # ---- files per lane ------------------------------------------------------------------------------------
    files = defaultdict(set)
    for ln, cs in lane_classes.items():
        for c in cs:
            files[ln].update(src_of.get(c, ()))
            h = "include/nwindow/%s.h" % c
            if (ROOT / h).exists():
                files[ln].add(h)
    lanes = {}
    for ln in ORDER:
        lanes[ln] = {
            "classes": sorted(lane_classes[ln]),
            "files": sorted(files[ln]),
            "functions": lane_n[ln],
            "bytes": lane_b[ln],
            "anonymous": sorted(va for va, s, l, w in anon_rows if l == ln),
        }
    (ROOT / "db" / "lanes_wave3.json").write_text(json.dumps(lanes, indent=1), encoding="utf-8")
    with open(ROOT / "db" / "anon_lane.csv", "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["va", "size", "lane", "why"])
        for va, s, ln, why in sorted(anon_rows):
            w.writerow([va, s, ln, why])
    for ln, v in lanes.items():
        print("%-9s %4d functions %7d B  %3d classes %3d files %3d anonymous" % (
            ln, v["functions"], v["bytes"], len(v["classes"]), len(v["files"]), len(v["anonymous"])))
    stock = [x for x in anon_rows if x[2] in ("(stock)", "(none)")]
    print("anonymous left alone (messenger / NPG / broadcast / no caller): %d, %d B" % (len(stock), sum(x[1] for x in stock)))
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
                    f.write("# wave 3 lane %s\n" % ln)
                    cur = ln
                f.write(fpath + "\n")
        print("hand_owned.txt: %d files appended" % len(add))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
