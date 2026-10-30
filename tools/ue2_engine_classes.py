#!/usr/bin/env python3
"""The native classes of the Engine.dll (and Core.dll), from their IMPLEMENT_CLASS initializers (copy of the C1
project's tools/engine/engine_classes.py, run on the C2 DLLs of orig/).

Same idea as tools/extract_classes.py (the NWindow's 178): every IMPLEMENT_CLASS leaves a dynamic
initializer that constructs Class::PrivateStaticClass through Core's
  UClass::UClass(ENativeConstructor, Size, ClassFlags, Super, Within, FGuid, Name, Package, Config, ...)
and a small stack emulator reads the arguments pushed at each call site. Differences here: the Engine's
initializers do not all build the FGuid the way the NWindow's do, so only the arguments the layouts need
(Name, Size, ClassFlags, Super, Package, InternalConstructor) must be recovered; the rest may stay unknown.

usage: ue2_engine_classes.py [orig/Engine.dll] [--out analysis/engine/classes.json]
Writes {name, cpp, size, super, class_flags, package, static_class, initializer, internal_ctor}.
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import extract_classes as ec  # noqa: E402
from nwd.image import ROOT, Image  # noqa: E402

NEEDED = ["Size", "ClassFlags", "Super", "Name", "Package"]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("dll", nargs="?", default=str(ROOT / "orig" / "Engine.dll"))
    ap.add_argument("--out", default=str(ROOT / "analysis" / "engine" / "classes.json"))
    a = ap.parse_args()
    img = Image(a.dll)
    slot = img.import_slot(ec.UCLASS_CTOR)
    lo, hi = img.sect_range(".text")
    code = img.read(lo, hi - lo)
    pat = b"\xff\x15" + slot.to_bytes(4, "little")
    sites, i = [], code.find(pat)
    while i >= 0:
        sites.append(lo + i)
        i = code.find(pat, i + 1)

    by_this, skipped = {}, []
    for site in sites:
        start = ec.func_start(img, site)
        regs, args = ec.emulate_to(img, start, site)
        this = regs.get("ecx")
        if this is None or not isinstance(this.v, int):
            skipped.append((site, "no this"))
            continue
        d = dict(zip(ec.PARAMS, args))
        missing = [p for p in NEEDED if str(d[p]).startswith("?")]
        if missing:
            skipped.append((site, "arguments not recovered: %s" % missing))
            continue
        try:
            by_this[this.v] = dict(
                name=img.wstr(d["Name"].v), size=d["Size"].v, class_flags=d["ClassFlags"].v,
                super_raw=repr(d["Super"]),
                package=img.wstr(d["Package"].v) if isinstance(d["Package"].v, int) else None,
                static_class=this.v, initializer=start, internal_ctor=d["InternalConstructor"].v if isinstance(d["InternalConstructor"], type(this)) and isinstance(d["InternalConstructor"].v, int) else None)
        except Exception as ex:  # a call site that is not an IMPLEMENT_CLASS initializer
            skipped.append((site, "not an initializer (%s)" % ex))

    for r in by_this.values():
        raw = r["super_raw"]
        if raw.startswith("0x") and int(raw, 16) in by_this:
            r["super"] = by_this[int(raw, 16)]["name"]
        elif raw.startswith(("&?PrivateStaticClass@", "ret:?StaticClass@")):
            # a class of another package (Core's UObject...): the C++ name without its A/U/F letter
            r["super"] = raw.split("@", 1)[1].split("@@")[0][1:]
        else:
            r["super"] = raw

    # C++ name = the letter UE2 puts in front: A under Actor, U under Object, from the chain
    def chain(r):
        out, s = [], r["super"]
        by_name = {c["name"]: c for c in by_this.values()}
        while s in by_name:
            out.append(s)
            s = by_name[s]["super"]
        out.append(s)
        return out

    for r in by_this.values():
        r["ancestors"] = chain(r)
        r["cpp"] = ("A" if "Actor" in r["ancestors"] or r["name"] == "Actor" else "U") + r["name"]
    classes = sorted(by_this.values(), key=lambda r: (len(r["ancestors"]), r["name"]))
    out = Path(a.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(classes, indent=1), encoding="utf-8")
    print("%d initializers, %d classes, %d skipped -> %s" % (len(sites), len(classes), len(skipped), out))
    for s, why in skipped[:10]:
        print("  skipped %#x: %s" % (s, why))


if __name__ == "__main__":
    main()
