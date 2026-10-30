#!/usr/bin/env python3
"""Native class layouts of the Engine (C2 here), computed from the UnrealScript the package carries (copy of
the C1 project's tools/engine/layouts.py; C2: Engine.u / Core.u of the C2 client, the class sizes from
tools/ue2_engine_classes.py on orig/Engine.dll).

In UE2 the order of the `var` declarations of a native class IS the layout of its C++ object (the
header mirrors the script), so Engine.u (Lineage2Ver111, tools/engine/ue_pkg.py) plus the class sizes
the IMPLEMENT_CLASS initializers give (tools/engine/engine_classes.py) are enough to reconstruct what
no header of ours had: every field's offset, size and type, for ~270 classes.

Rules (UStruct::Link): a class starts at its parent's size; each property is aligned to its own
alignment and takes its size times ArrayDim; consecutive bools share a 32-bit word; the end is aligned
to the largest alignment seen (4 for a class). The result is checked against the C++ size: the script
part must end at or before it, and what is left is the native-only members (declared in the class's
cpptext in C++, invisible to the script): `native_extra`. A class whose script part ends after its C++
size, or whose extra is not a multiple of 1..4 bytes of alignment, got a wrong layout (`ok` false).

usage: ue2_layouts.py [--engine-u Engine.u] [--core-u Core.u] [--classes analysis/engine/classes.json]
Writes analysis/engine/layouts.json (classes AND structs, with every field) and analysis/engine/layouts.tsv.
"""
from __future__ import annotations

import argparse
import json
import os
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from nwd.image import ROOT  # noqa: E402
from ue2_pkg import Package, Reader  # noqa: E402

# the C4 client System folder (NWD_CLIENT_SYSTEM: the test copy has the same .u files)
SYSTEM = (Path(os.environ["NWD_CLIENT_SYSTEM"]) if os.environ.get("NWD_CLIENT_SYSTEM") else None)
UOBJECT_SIZE = 0x34   # sizeof(UObject): 0x28 in the C1, C2 and C3; 0x34 in the C4 Core.dll/Engine.dll (27/05/2006: three more dwords at +0x28..+0x30)
USUBSYSTEM_SIZE = 0x38   # UObject + the FExec base (UE2Core.h): the parent of UEngine, which has no size of its own here
CPF_NET = 0x20
# (size, alignment) of the property types that do not depend on anything else
SIMPLE = {"ByteProperty": (1, 1), "IntProperty": (4, 4), "FloatProperty": (4, 4), "ObjectProperty": (4, 4),
          "ClassProperty": (4, 4), "NameProperty": (4, 4), "StrProperty": (12, 4), "ArrayProperty": (12, 4),
          "DelegateProperty": (8, 4), "BoolProperty": (4, 4)}


def align(v, a):
    return (v + a - 1) // a * a


def strip_blocks(text):
    """The .uc text without comments, cpptext blocks and struct/enum/function/state bodies: what is left at
    depth 0 are the class-level `var` statements (and the const/enum/struct headers)."""
    out, i, n = [], 0, len(text)
    while i < n:
        if text.startswith("//", i):
            j = text.find("\n", i)
            i = n if j < 0 else j
        elif text.startswith("/*", i):
            j = text.find("*/", i)
            i = n if j < 0 else j + 2
        else:
            out.append(text[i])
            i += 1
    return "".join(out)


def strip_blocks_comments(text):
    return strip_blocks(text)


def declared_names(text):
    """Variable names of the class-level `var` statements, in declaration order. A `var` statement
    may carry its own `{...}` (an inline enum or struct): the braces are skipped, the names after
    them kept."""
    t = strip_blocks(text)
    names, i, n, depth = [], 0, len(t), 0
    import re
    while i < n:
        c = t[i]
        if (depth == 0 and t.startswith("var", i) and (i == 0 or not (t[i - 1].isalnum() or t[i - 1] == "_"))
                and i + 3 < n and not (t[i + 3].isalnum() or t[i + 3] == "_")):
            j, d, buf = i + 3, 0, []
            while j < n:
                if t[j] == "{":
                    d += 1
                elif t[j] == "}":
                    d -= 1
                elif t[j] == ";" and d == 0:
                    break
                elif d == 0:
                    buf.append(t[j])
                j += 1
            stmt = "".join(buf)
            stmt = re.sub(r"^\s*\([^)]*\)", "", stmt)           # var(Category)
            for piece in stmt.split(","):
                m = re.search(r"([A-Za-z_]\w*)\s*(\[[^\]]*\])?\s*$", piece.strip().split("=")[0].strip())
                if m:
                    names.append(m.group(1))
            i = j + 1
            continue
        if c == "{":
            depth += 1
        elif c == "}":
            depth -= 1
        i += 1
    return names


class Registry:
    """Fields of every class/struct of the packages, and the struct sizes."""

    def __init__(self, pkgs):
        self.pkgs = pkgs
        self.texts = [pk.texts("ScriptText") for pk in pkgs]
        import re
        # `struct Plane extends Vector`: a struct starts where its parent struct ends
        self.struct_super = {}
        for t in self.texts:
            for text in t.values():
                for m in re.finditer(r"\bstruct\s+(?:native\s+|export\s+|transient\s+)*(\w+)\s+extends\s+(\w+)",
                                     strip_blocks_comments(text)):
                    self.struct_super[m.group(1)] = m.group(2)
        self.structs = {}   # name -> dict(size, align, fields)
        self.owner = {}     # (pkg index, export idx) -> list of property exports

    def props(self, pi, export_index1, is_struct=False):
        """The property exports of a class/struct, in the order of its `var` declarations (the package
        lists them in another order: bools that were declared together come apart)."""
        pk = self.pkgs[pi]
        out = []
        for i, e in enumerate(pk.exports):
            if e["outer"] == export_index1 and pk.class_of(e).endswith("Property"):
                out.append((i + 1, e))
        if is_struct:
            out.reverse()   # the package lists a struct's members last-declared first
            return out
        order = self.texts[pi].get(pk.name(pk.exports[export_index1 - 1]["name"]))
        if order:
            rank = {n: k for k, n in enumerate(declared_names(order))}
            out.sort(key=lambda ie: rank.get(pk.name(ie[1]["name"]), 10 ** 6))
        return out

    def decode(self, pi, e):
        """(arraydim, flags, ref) of one property export: the tag, SuperField, Next, ArrayDim, flags,
        Category, RepOffset when the property is replicated, and the type's own reference."""
        pk = self.pkgs[pi]
        kind = pk.class_of(e)
        r = Reader(pk.blob(e))
        r.compact()
        r.compact()
        r.compact()
        dim = r.i32()
        flags = r.u32()
        r.compact()
        if flags & CPF_NET:
            r.p += 2
        ref = r.compact() if kind in ("StructProperty", "ByteProperty", "ArrayProperty", "ObjectProperty",
                                      "ClassProperty") else 0
        return dim, flags, ref

    def prop_size(self, pi, e, ref):
        kind = self.pkgs[pi].class_of(e)
        if kind == "StructProperty":
            name = self.pkgs[pi].ref_name(ref)
            s = self.struct(name)
            return (s["size"], s["align"]) if s else (None, 4)
        return SIMPLE.get(kind, (None, 4))

    def struct(self, name):
        if name in self.structs:
            return self.structs[name]
        for pi, pk in enumerate(self.pkgs):
            for i, e in enumerate(pk.exports):
                if pk.class_of(e) == "Struct" and pk.name(e["name"]) == name:
                    self.structs[name] = dict(size=None, align=4, fields=[])   # guards recursion
                    sup = self.struct_super.get(name)
                    base = self.struct(sup) if sup else None
                    lay = self.layout(pi, i + 1, base["size"] if base else 0, is_struct=True,
                                      min_align=base["align"] if base else 1)
                    self.structs[name] = dict(size=lay["size"], align=lay["align"], fields=lay["fields"])
                    return self.structs[name]
        return None

    def layout(self, pi, export_index1, start, is_struct=False, min_align=1):
        pk = self.pkgs[pi]
        off, maxal, fields, prev_bool = start, min_align, [], None
        for idx, e in self.props(pi, export_index1, is_struct):
            kind = pk.class_of(e)
            dim, flags, ref = self.decode(pi, e)
            size, al = self.prop_size(pi, e, ref)
            maxal = max(maxal, al)
            if size is None:
                fields.append(dict(name=pk.name(e["name"]), type=kind, offset=None, size=None, dim=dim))
                return dict(fields=fields, size=None, align=maxal, end=None)
            if kind == "BoolProperty" and dim == 1:
                if prev_bool is not None and prev_bool["bit"] < 31:
                    f = dict(name=pk.name(e["name"]), type="bool", offset=prev_bool["offset"], size=4, dim=1,
                             bit=prev_bool["bit"] + 1)
                else:
                    off = align(off, 4)
                    f = dict(name=pk.name(e["name"]), type="bool", offset=off, size=4, dim=1, bit=0)
                    off += 4
                fields.append(f)
                prev_bool = f
                continue
            prev_bool = None
            off = align(off, al)
            f = dict(name=pk.name(e["name"]), type=kind[:-8].lower() if kind != "StructProperty" else
                     "struct " + pk.ref_name(ref), offset=off, size=size * dim, dim=dim)
            if kind in ("ObjectProperty", "ClassProperty") and ref:
                f["type"] = f["type"] + " " + pk.ref_name(ref)
            if kind == "ArrayProperty":
                f["type"] = "array"
            fields.append(f)
            off += size * dim
        return dict(fields=fields, size=align(off, maxal), align=maxal, end=off)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--engine-u", default=str(SYSTEM / "Engine.u") if SYSTEM else None, required=SYSTEM is None, help="default: $NWD_CLIENT_SYSTEM/Engine.u")
    ap.add_argument("--core-u", default=str(SYSTEM / "Core.u") if SYSTEM else None, required=SYSTEM is None, help="default: $NWD_CLIENT_SYSTEM/Core.u")
    ap.add_argument("--classes", default=str(ROOT / "analysis" / "engine" / "classes.json"))
    ap.add_argument("--out", default=str(ROOT / "analysis" / "engine"))
    a = ap.parse_args()
    eng, core = Package(a.engine_u), Package(a.core_u)
    reg = Registry([eng, core])
    native = {c["name"]: c for c in json.load(open(a.classes))}
    sizes = {"Object": UOBJECT_SIZE, "Subsystem": USUBSYSTEM_SIZE}
    sizes.update({n: c["size"] for n, c in native.items()})

    exports_by_class = {eng.name(e["name"]): i + 1 for i, e in enumerate(eng.exports) if eng.class_of(e) == "None"}
    out, bad = [], []
    for name, c in sorted(native.items(), key=lambda kv: len(kv[1]["ancestors"])):
        ei = exports_by_class.get(name)
        parent = c["super"]
        start = sizes.get(parent)
        rec = dict(name=name, cpp=c["cpp"], parent=parent, native_size=c["size"], parent_size=start, fields=[])
        if ei is None or start is None:
            rec["ok"] = None
            rec["why"] = "no script class" if ei is None else "parent size unknown (%s)" % parent
        else:
            lay = reg.layout(0, ei, start)
            rec["fields"] = lay["fields"]
            if lay["size"] is None:
                rec["ok"], rec["why"] = False, "a property of unknown size"
            else:
                rec["script_end"] = align(lay["end"], 4)
                rec["native_extra"] = c["size"] - rec["script_end"]
                rec["ok"] = rec["native_extra"] >= 0
                if not rec["ok"]:
                    rec["why"] = "script part ends %d bytes after the C++ size" % -rec["native_extra"]
        out.append(rec)
        if rec["ok"] is False:
            bad.append(rec)
    outdir = Path(a.out)
    (outdir / "layouts.json").write_text(json.dumps(out, indent=1), encoding="utf-8")
    # every script struct (NMagicInfo, FVector...): the fields of a struct a class embeds, at their offset
    # inside it (tools/ue2_check_headers.py maps a header field that sits inside one)
    structs = {}
    for pi, pk in enumerate((eng, core)):
        for e in pk.exports:
            if pk.class_of(e) == "Struct":
                st = reg.struct(pk.name(e["name"]))
                if st:
                    structs[pk.name(e["name"])] = st
    (outdir / "structs.json").write_text(json.dumps(structs, indent=1), encoding="utf-8")
    # layouts.tsv: one block per class, every field of the class AND of its ancestors at its absolute
    # offset (what a Ghidra structure needs). `class` rows: cpp name, parent, C++ size, ok (True/None).
    by_name = {r["name"]: r for r in out}

    def flat(r):
        p = by_name.get(r["parent"])
        base = flat(p) if p and p["ok"] else []
        return base + [fl for fl in r["fields"] if fl["offset"] is not None]

    with open(outdir / "layouts.tsv", "w", encoding="utf-8", newline="") as f:
        for r in out:
            if not r["ok"]:
                continue
            f.write("class\t%s\t%s\t%d\t%s\n" % (r["cpp"], r["parent"], r["native_size"], r.get("ok")))
            groups = {}   # the bools of one 32-bit word become one field `bits_<first bool>`
            for fl in flat(r):
                if fl["type"] == "bool" and fl.get("bit") is not None:
                    groups.setdefault(fl["offset"], []).append(fl)
            done = set()
            for fl in flat(r):
                if fl["type"] == "bool" and fl.get("bit") is not None:
                    if fl["offset"] in done:
                        continue
                    done.add(fl["offset"])
                    g = groups[fl["offset"]]
                    desc = " ".join("%d:%s" % (b["bit"], b["name"]) for b in g)
                    f.write("field\t%s\t%d\t4\tbits\tbits_%s\t%s\n" % (r["cpp"], fl["offset"], g[0]["name"], desc))
                else:
                    f.write("field\t%s\t%d\t%d\t%s\t%s\t\n" % (r["cpp"], fl["offset"], fl["size"], fl["type"],
                                                               fl["name"]))
    ok = [r for r in out if r["ok"]]
    exact = [r for r in ok if r["native_extra"] == 0]
    unknown = [r for r in out if r["ok"] is None]
    print("%d classes: %d consistent (%d exactly, %d with native-only members), %d inconsistent, %d not checked"
          % (len(out), len(ok), len(exact), len(ok) - len(exact), len(bad), len(unknown)))
    for r in bad[:12]:
        print("  BAD %-26s native %#x script_end %s: %s" % (r["cpp"], r["native_size"], r.get("script_end"), r["why"]))
    for r in unknown[:6]:
        print("  --- %-26s %s" % (r["cpp"], r["why"]))


if __name__ == "__main__":
    main()
