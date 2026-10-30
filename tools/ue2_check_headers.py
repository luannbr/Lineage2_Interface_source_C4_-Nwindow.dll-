#!/usr/bin/env python3
"""The NW_ASSERT_OFFSET field offsets of include/ue2/UE2Engine.h against the C4 script layouts.

In UE2 the order of the `var` declarations of a native class is the layout of its C++ object
(tools/ue2_layouts.py computes it from the C4 Engine.u / Core.u: analysis/engine/layouts.json and
structs.json, every class checked against the size its IMPLEMENT_CLASS initializer gives). A field of
the header either has the name of a script variable (checked by name) or is a name of ours for one
(tools/ue2_data/field_map.csv: header field -> script variable, made from the C1 layouts: the script
variable that started at the C1 offset the header used). Fields that lie in a native-only region (no
script variable there) cannot be proven from the packages and are listed as "not provable".

  python tools/ue2_check_headers.py                    # check include/ue2/UE2Engine.h (exit 1 on a difference)
  python tools/ue2_check_headers.py --map-c1 <C1-version header> <C1 layouts dir>
        # print the header-field -> script-variable table of a C1 header (the field_map.csv rows)
"""
from __future__ import annotations

import csv
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HDR = ROOT / "include" / "ue2" / "UE2Engine.h"
MAP = ROOT / "tools" / "ue2_data" / "field_map.csv"
ASSERT_RE = re.compile(r"NW_ASSERT_OFFSET\(\s*(\w+)\s*,\s*(\w+)\s*,\s*(0x[0-9A-Fa-f]+|\d+)\s*\)")


class Layouts:
    def __init__(self, d: Path):
        self.by_cpp = {r["cpp"]: r for r in json.load((d / "layouts.json").open(encoding="utf-8"))}
        self.by_name = {r["name"]: r for r in self.by_cpp.values()}
        self.structs = json.load((d / "structs.json").open(encoding="utf-8"))

    def chain(self, cls: str):
        r = self.by_cpp.get(cls)
        while r:
            yield r
            r = self.by_name.get(r["parent"])

    def find(self, cls: str, name: str):
        for r in self.chain(cls):
            for f in r["fields"]:
                if f["name"] == name and f["offset"] is not None:
                    return f, r["cpp"]
        return None

    def covering(self, cls: str, off: int):
        for r in self.chain(cls):
            for f in r["fields"]:
                if f["offset"] is not None and f["offset"] <= off < f["offset"] + max(f["size"] or 1, 1):
                    return f, r["cpp"]
        return None

    def resolve(self, f: dict, sub: str) -> int | None:
        """Offset of `sub` ('' | '[i]' | '.inner') of the script variable f."""
        if not sub:
            return f["offset"]
        if sub.startswith("["):
            return f["offset"] + int(sub[1:-1]) * (f["size"] // max(f["dim"], 1))
        st = self.structs.get(f["type"].replace("struct ", ""))
        if st:
            for g in st["fields"]:
                if g["name"] == sub[1:] and g["offset"] is not None:
                    return f["offset"] + g["offset"]
        return None


def parse_asserts(text: str):
    return [(m.group(1), m.group(2), int(m.group(3), 0)) for m in ASSERT_RE.finditer(text)]


def read_map() -> dict[tuple[str, str], tuple[str, str]]:
    out = {}
    if MAP.exists():
        for r in csv.DictReader(l for l in MAP.open(encoding="utf-8") if not l.startswith("#")):
            out[(r["class"], r["field"])] = (r["script_var"], r["sub"])
    return out


def map_c1(hdr_text: str, c1dir: Path) -> int:
    """Rows of field_map.csv: what each field of a C1 header is, by the C1 script layout."""
    L = Layouts(c1dir)
    print("class,field,script_var,sub,note")
    for cls, field, off in parse_asserts(hdr_text):
        hit = L.covering(cls, off)
        if hit is None:
            continue
        f, owner = hit
        if f["offset"] == off:
            sub = ""
        elif f["dim"] > 1 and (off - f["offset"]) % (f["size"] // f["dim"]) == 0:
            sub = "[%d]" % ((off - f["offset"]) // (f["size"] // f["dim"]))
        elif f["type"].startswith("struct "):
            st = L.structs.get(f["type"][7:])
            inner = [g for g in (st["fields"] if st else []) if g["offset"] == off - f["offset"]]
            sub = "." + inner[0]["name"] if inner else ""
            if not inner:
                print(f"# {cls}.{field}: inside {f['name']} ({f['type']}) at +{off - f['offset']:#x}, no member there", file=sys.stderr)
                continue
        else:
            print(f"# {cls}.{field}: {off:#x} is inside {f['name']} @{f['offset']:#x}+{f['size']}, not at its start", file=sys.stderr)
            continue
        if f["name"] == field and not sub:
            continue            # same name: nothing to map
        print(f"{cls},{field},{f['name']},{sub},C1 script variable of {owner} at {f['offset']:#x}")
    return 0


def check(hdr: Path, d: Path) -> int:
    L = Layouts(d)
    fmap = read_map()
    same = diff = unproven = 0
    bad = []
    for cls, field, off in parse_asserts(hdr.read_text(encoding="utf-8")):
        var, sub = fmap.get((cls, field), (field, ""))
        hit = L.find(cls, var)
        if hit is None:
            unproven += 1
            continue
        want = L.resolve(hit[0], sub)
        if want is None:
            unproven += 1
        elif want == off:
            same += 1
        else:
            diff += 1
            bad.append(f"  DIFFERENT {cls}.{field}: header {off:#x}, C4 script {hit[1]}.{var}{sub} at {want:#x}")
    print(f"{same + diff + unproven} NW_ASSERT_OFFSETs in {hdr.name}: {same} equal to the C4 script layout, "
          f"{diff} different, {unproven} not provable from the script (native members, or no script class)")
    for b in bad:
        print(b)
    return 1 if diff else 0


def main(argv: list[str]) -> int:
    if argv and argv[0] == "--map-c1":
        return map_c1(Path(argv[1]).read_text(encoding="utf-8"), Path(argv[2]))
    return check(Path(argv[0]) if argv else HDR, ROOT / "analysis" / "engine")


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
