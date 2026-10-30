#!/usr/bin/env python3
"""Move the field offsets of include/ue2/UE2Engine.h from one client's layout to the next one's.

The header declares, per class, the fields NWindow uses at their offsets (`// +0xOFF` comments,
NW_ASSERT_OFFSET, and `BYTE _UnknownXXXX[END - START]` fillers for the bytes in between). When the
Engine of the next client inserts script variables, every later offset moves. The script layouts
(tools/ue2_layouts.py -> analysis/engine/layouts.json: in UE2 the order of the `var` declarations IS the
layout) say by how much, so a header field is translated like this:

  * the class's own script variables of the old and the new layout are matched by name: each pair is an
    anchor (old offset -> new offset);
  * an old offset X takes the shift of the nearest anchor at or below it (a native member, which no
    script variable names, keeps its distance to the variable before it); below the first anchor the
    shift is the parent's size change; the class size is the new native size.

That model is right while the next client only inserts or removes script variables and leaves the
native-only members alone; `--report` prints every translation with how it was reached ("name" = a
script variable at exactly that offset, "after X" = interpolated from variable X) so the guesses can be
read, and the cycle's tools/port/verify_data.py (stock displacements against ours) checks them against
the stock code later. Classes with no script class (UL2ConsoleWnd, UViewport, UMeshAnimation: native
only) have no anchors: their fields are moved by hand from what the Engine's own code shows.

  python tools/ue2_shift_layout.py --report [CLASS ...]
  python tools/ue2_shift_layout.py --apply   [--header include/ue2/UE2Engine.h] [CLASS ...]
Options: --from analysis/c2/engine/layouts.json  --to analysis/engine/layouts.json  --old-label C2
(C4: --from analysis/c3/engine/layouts.json --old-label C3; the comments of a moved field then read `(C3 +0x..)`)
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

CLASS_RE = re.compile(r"^(class|struct)\s+(?:\w+_API\s+)?(\w+)\s*(?::[^{;]*)?\{", re.M)
ASSERT_OFF_RE = re.compile(r"^NW_ASSERT_OFFSET\(\s*(\w+)\s*,\s*(\w+)\s*,\s*(0x[0-9A-Fa-f]+|\d+)\s*\);", re.M)
ASSERT_SIZE_RE = re.compile(r"^NW_ASSERT_SIZE\(\s*(\w+)\s*,\s*(0x[0-9A-Fa-f]+|\d+)\s*\);", re.M)
FILLER_RE = re.compile(r"^(\s*)BYTE\s+_Unknown([0-9A-Fa-f]+)\[\s*0x([0-9A-Fa-f]+)\s*-\s*0x([0-9A-Fa-f]+)\s*\];\s*$")
FIELD_LINE_RE = re.compile(r"^\s+[A-Za-z_][\w:<>\*, ]*[\s\*]\w+(\[[^\]]*\])?\s*;")
OFF_COMMENT_RE = re.compile(r"//\s*\+0x([0-9A-Fa-f]+)")


def load_layouts(path: Path) -> dict[str, dict]:
    return {c["cpp"]: c for c in json.loads(path.read_text(encoding="utf-8"))}


def flatten(fields: list[dict], structs: dict, base: int = 0, prefix: str = ""):
    """(dotted name, absolute offset) of every script variable, struct members included (a struct
    variable of dimension 1 is expanded with its members at base + their offset)."""
    for f in fields:
        off, name = base + f["offset"], prefix + f["name"]
        yield name, off
        t = f.get("type", "")
        if t.startswith("struct ") and f.get("dim", 1) == 1:
            s = structs.get(t[7:])
            if s:
                yield from flatten(s["fields"], structs, off, name + ".")


class Translator:
    """old offset -> (new offset, how) for one class."""

    def __init__(self, old: dict, new: dict, old_structs: dict | None = None, new_structs: dict | None = None):
        self.cls = old["cpp"]
        self.anchors: list[tuple[int, int, str]] = []   # (old off, new off, name)
        by_name_new: dict[str, list] = {}
        for name, off in flatten(new["fields"], new_structs or {}):
            by_name_new.setdefault(name, []).append(off)
        for name, off in flatten(old["fields"], old_structs or {}):
            cand = by_name_new.get(name, [])
            if len(cand) == 1:
                self.anchors.append((off, cand[0], name))
        self.anchors.sort()
        self.base = (old["parent_size"], new["parent_size"])
        self.old_size = old["native_size"]
        self.new_size = new["native_size"]

    def shift_at(self, x: int) -> tuple[int, str]:
        best = None
        for o, n, name in self.anchors:
            if o <= x:
                best = (o, n, name)
            else:
                break
        if best is None:
            return self.base[1] - self.base[0], "below the first variable (parent size change)"
        o, n, name = best
        return n - o, name if o == x else f"after {name}"

    def __call__(self, x: int) -> tuple[int, str]:
        d, how = self.shift_at(x)
        return x + d, how


def parse_header(text: str):
    """class name -> [start, end of the class text, {asserted field: old offset}, asserted size or None]."""
    classes: dict[str, list] = {}
    for m in CLASS_RE.finditer(text):
        name = m.group(2)
        s = m.start()
        e = text.find("\n};", s)
        if e < 0:
            continue
        classes[name] = [s, e + 3, {}, None]
    for m in ASSERT_OFF_RE.finditer(text):
        c = classes.get(m.group(1))
        if c is not None:
            c[2][m.group(2)] = int(m.group(3), 0)
    for m in ASSERT_SIZE_RE.finditer(text):
        c = classes.get(m.group(1))
        if c is not None:
            c[3] = int(m.group(2), 0)
    return classes


def shift_class(body_text: str, t: Translator, new_sizes: dict[int, int] | None = None, old_label: str = "C2") -> str:
    """The text of one class with its fields moved by t and its fillers rebuilt ("!reason" on a conflict).
    new_sizes: old offset -> the field's size in the new layout, for the fields that changed size
    (an array of another length, a record of another size); the others keep the size the old text implies."""
    lines = body_text.split("\n")
    kind: list = [None] * len(lines)   # "filler" | "field" | None
    info: list = [None] * len(lines)   # filler: (start, end); field: old offset
    for i, ln in enumerate(lines):
        m = FILLER_RE.match(ln)
        if m:
            kind[i], info[i] = "filler", (int(m.group(4), 16), int(m.group(3), 16))   # [END - START]
            continue
        mo = OFF_COMMENT_RE.search(ln)
        if mo and FIELD_LINE_RE.match(ln):
            kind[i], info[i] = "field", int(mo.group(1), 16)
    order = [i for i in range(len(lines)) if kind[i]]
    # the old end of every field: the start of the filler after it, else the next field's offset
    old_end: dict[int, int | None] = {}
    for n_, i in enumerate(order):
        if kind[i] != "field":
            continue
        nxt = order[n_ + 1] if n_ + 1 < len(order) else None
        if nxt is None:
            old_end[i] = None
        elif kind[nxt] == "filler":
            old_end[i] = info[nxt][0]
        else:
            old_end[i] = info[nxt]
    new_lines: list = list(lines)
    pending: dict[int, str] = {}
    prev_new_end = None
    for n_, i in enumerate(order):
        if kind[i] == "filler":
            start, _end = info[i]
            if n_ == 0:
                prev_new_end = t(start)[0]
            new_lines[i] = None   # rebuilt before the next field, or here at the class end
            if n_ + 1 == len(order) and prev_new_end is not None and t.new_size > prev_new_end:
                new_lines[i] = f"    BYTE _Unknown{prev_new_end:X}[0x{t.new_size:X} - 0x{prev_new_end:X}];"
            continue
        off = info[i]
        new_off, _ = t(off)
        if prev_new_end is not None:
            if new_off < prev_new_end:
                return (f"!field at old {off:#x} -> {new_off:#x} overlaps the previous member "
                        f"(ends {prev_new_end:#x})")
            if new_off > prev_new_end:
                pending[i] = f"    BYTE _Unknown{prev_new_end:X}[0x{new_off:X} - 0x{prev_new_end:X}];"
        ln = lines[i]
        if new_off != off:
            m = OFF_COMMENT_RE.search(ln)
            tail = ln[m.end():]
            if tail.startswith(" ("):
                tail = f" ({old_label} +0x{off:X}; " + tail[2:]
            else:
                tail = f" ({old_label} +0x{off:X})" + tail
            ln = ln[:m.start()] + f"// +0x{new_off:X}" + tail
        new_lines[i] = ln
        if new_sizes and off in new_sizes:
            prev_new_end = new_off + new_sizes[off]
        else:
            prev_new_end = (new_off + (old_end[i] - off)) if old_end[i] is not None else None
    out: list[str] = []
    for i, ln in enumerate(new_lines):
        if i in pending:
            out.append(pending[i])
        if ln is not None:
            out.append(ln)
    return "\n".join(out)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("classes", nargs="*")
    ap.add_argument("--from", dest="old", type=Path, default=ROOT / "analysis/c2/engine/layouts.json")
    ap.add_argument("--to", dest="new", type=Path, default=ROOT / "analysis/engine/layouts.json")
    ap.add_argument("--header", type=Path, default=ROOT / "include/ue2/UE2Engine.h")
    ap.add_argument("--old-label", default="C2", help="how the comments call the client the header is moved from (C4: --from analysis/c3/engine/layouts.json --old-label C3)")
    ap.add_argument("--report", action="store_true")
    ap.add_argument("--apply", action="store_true")
    a = ap.parse_args()
    old, new = load_layouts(a.old), load_layouts(a.new)
    old_structs = json.loads((a.old.parent / "structs.json").read_text(encoding="utf-8"))
    new_structs = json.loads((a.new.parent / "structs.json").read_text(encoding="utf-8"))
    raw = a.header.read_bytes().decode("utf-8")
    crlf = "\r\n" in raw
    text = raw.replace("\r\n", "\n")
    classes = parse_header(text)

    todo = []
    for name, (s, e, fields, size) in classes.items():
        if a.classes and name not in a.classes:
            continue
        o, n = old.get(name), new.get(name)
        if not o or not n or not n.get("fields"):
            continue   # no script class: no anchors, moved by hand
        if o["native_size"] == n["native_size"] and not a.classes:
            continue   # unchanged class: nothing was inserted in it
        todo.append(name)

    if not a.apply:
        for name in todo:
            s, e, fields, size = classes[name]
            t = Translator(old[name], new[name], old_structs, new_structs)
            print(f"== {name}: size {t.old_size:#x} -> {t.new_size:#x}"
                  + (f" (header size assert {size:#x})" if size is not None else " (no size assert)"))
            for f, off in sorted(fields.items(), key=lambda kv: kv[1]):
                no, how = t(off)
                flag = "" if no == off else f"   {no - off:+#x}"
                print(f"   {f:28s} {off:#6x} -> {no:#6x}  [{how}]{flag}")
        return 0

    out = text
    for name in sorted(todo, key=lambda n: -classes[n][0]):   # bottom-up: earlier offsets stay valid
        s0, e0, fields, size = classes[name]
        t = Translator(old[name], new[name], old_structs, new_structs)
        res = shift_class(out[s0:e0], t, old_label=a.old_label)
        if res.startswith("!"):
            print(f"{name}: {res[1:]}", file=sys.stderr)
            return 1
        out = out[:s0] + res + out[e0:]
        out = ASSERT_OFF_RE.sub(
            lambda m, name=name, t=t: (f"NW_ASSERT_OFFSET({name}, {m.group(2)}, 0x{t(int(m.group(3), 0))[0]:X});"
                                       if m.group(1) == name else m.group(0)), out)
        if size is not None:
            out = ASSERT_SIZE_RE.sub(
                lambda m, name=name: (f"NW_ASSERT_SIZE({name}, 0x{new[name]['native_size']:X});"
                                      if m.group(1) == name else m.group(0)), out)
    a.header.write_bytes((out.replace("\n", "\r\n") if crlf else out).encode("utf-8"))
    print(f"rewrote {a.header} for {len(todo)} classes: {', '.join(todo)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
