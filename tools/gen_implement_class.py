#!/usr/bin/env python3
"""The class registration of the standalone NWindow.dll (phase 5, docs/notas/Standalone.md),
generated from analysis/classes.json:

  src/standalone/Classes.gen.cpp  NW_IMPLEMENT_CLASS (UE2's IMPLEMENT_CLASS, NWindow.h) of the
                                  native classes (178 in C1, 225 in C2, 341 in C4), in the order of the stock
                                  initializers (the link order of the stock .obj files, NCButton first)
  src/standalone/NWindow.def      the exports of the stock image with their ordinals (180 in C1,
                                  227 in C2, 343 in C4): _DllMain@12 @1, GPackage @2, the autoclass<Class> @3..

The parent class and the class flags come from the stock registration, not from the headers:
the stock image registers NCTestHtmlViewer, NCTradeItemWnd, NCBroadcastGMFenceWnd and
NCBroadcastObserverBuffWnd under NCWnd and NCGMQuestWnd under NCQuestWnd (their C++ base is
another class), NL2Font and NConsoleWnd with CLASS_Config, NCAbstractMain and NCCmdTarget CLASS_Abstract.

With orig/NWindow.dll present the stock image is checked too: every initializer is read again
(tools/extract_classes.py's emulator) for what classes.json does not keep - EC_NativeConstructor,
the name TEXT("NClass")+1, the package GPackage, the config name from
UObject::StaticConfigName() and the static constructor UObject::StaticConstructor - and the
export table must be the one the .def describes.

  python tools/gen_implement_class.py [--check]   (--check: compare only, write nothing)

Classes.gen.cpp needs the NW_DECLARE_CLASS of every class (include/nwindow): while some are missing
only NWindow.def is written (it needs nothing but classes.json) and the missing classes are listed.
A constructor the InternalConstructor calls (`new ((EInternal*)X) TClass`) needs default arguments for
what the stock InternalConstructor pushes: tools/port/check_internal_ctors.py compares them.
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

OUT_CPP = ROOT / "src" / "standalone" / "Classes.gen.cpp"
OUT_DEF = ROOT / "src" / "standalone" / "NWindow.def"

# what IMPLEMENT_CLASS passes the same way for every class (NW_IMPLEMENT_CLASS hardcodes it)
OBJECT_FLAGS = 0x4084004   # RF_Public | RF_Standalone | RF_Transient | RF_Native
CLASS_FLAG_NAMES = {0x1: "CLASS_Abstract", 0x4: "CLASS_Config"}
STATIC_CTOR = "&?StaticConstructor@UObject@@QAEXXZ"
WITHIN = "&?PrivateStaticClass@UObject@@0VUClass@@A"
CONFIG = "ret:?StaticConfigName@UObject@@SAPBGXZ"

DECL_RE = re.compile(r"^\s*NW_DECLARE_(?:ABSTRACT_)?CLASS\((\w+),", re.M)


def flags_expr(flags: int) -> str:
    if flags == 0:
        return "0"
    names = [n for bit, n in sorted(CLASS_FLAG_NAMES.items()) if flags & bit]
    if sum(bit for bit in CLASS_FLAG_NAMES if flags & bit) != flags:
        raise SystemExit(f"class flags {flags:#x} without a name")
    return " | ".join(names)


def class_headers() -> dict[str, str]:
    """{class: header} for every NW_DECLARE_(ABSTRACT_)CLASS in include/nwindow."""
    out = {}
    for path in sorted((ROOT / "include" / "nwindow").glob("*.h")):
        for m in DECL_RE.finditer(path.read_text(encoding="utf-8", errors="replace")):
            out[m.group(1)] = f"nwindow/{path.name}"
    return out


def export_names(classes: list[dict]) -> list[tuple[str, bool]]:
    """The stock export list in ordinal order: (name, is_data)."""
    autos = sorted((f"autoclass{c['cpp']}" for c in classes), key=lambda s: s.encode())
    return [("_DllMain@12", False), ("GPackage", True)] + [(n, True) for n in autos]


def check_stock(classes: list[dict], exports: list[tuple[str, bool]]) -> list[str]:
    """Differences between the stock image and what the generated files assume."""
    from extract_classes import PARAMS, emulate_to  # noqa: E402 (needs capstone/pefile)
    from nwd.image import ORIG, Image  # noqa: E402

    if not ORIG.exists():
        print("orig/NWindow.dll not found: stock image not checked")
        return []
    img = Image()
    problems = []
    gpackage = img.exports.get("GPackage")
    ctor_slot = img.import_slot("??0UClass@@QAE@W4ENativeConstructor@@KKPAV0@1VFGuid@@PBG33KP6AXPAX@ZP8UObject@@AEXXZ@Z")
    call = b"\xff\x15" + ctor_slot.to_bytes(4, "little")
    for c in classes:
        start = c["initializer"]
        code = img.read(start, 0x100)
        site = code.find(call)
        if site < 0:
            problems.append(f"{c['cpp']}: no UClass constructor call in the initializer {start:#x}")
            continue
        regs, args = emulate_to(img, start, start + site)
        d = {k: v.v for k, v in zip(PARAMS, args)}
        want = {"ENative": 0, "Size": c["size"], "ClassFlags": c["class_flags"],
                "Guid0": 0, "Guid1": 0, "Guid2": 0, "Guid3": 0, "Package": gpackage,
                "Config": CONFIG, "Flags": OBJECT_FLAGS, "StaticConstructor": STATIC_CTOR,
                "Within": WITHIN}
        for k, v in want.items():
            if d[k] != v:
                problems.append(f"{c['cpp']}: {k} is {d[k]!r}, expected {v!r}")
        name = img.wstr(d["Name"]) if isinstance(d["Name"], int) else None
        if name != c["cpp"][1:]:
            problems.append(f"{c['cpp']}: name {name!r}")
        # TEXT("NClass")+1: the string one character into the literal
        if isinstance(d["Name"], int) and img.read(d["Name"] - 2, 2) != c["cpp"][0].encode("utf-16-le"):
            problems.append(f"{c['cpp']}: the name is not TEXT(\"{c['cpp']}\")+1")
    stock = sorted(img.pe.DIRECTORY_ENTRY_EXPORT.symbols, key=lambda s: s.ordinal)
    stock_names = [(s.ordinal, s.name.decode()) for s in stock]
    ours = [(k + 1, n) for k, (n, _) in enumerate(exports)]
    if stock_names != ours:
        problems.append("export table differs from the stock one")
    return problems


def render_cpp(classes: list[dict], headers: dict[str, str]) -> str:
    missing = [c["cpp"] for c in classes if c["cpp"] not in headers]
    if missing:
        raise SystemExit(f"no NW_DECLARE_CLASS for: {', '.join(missing)}")
    includes = sorted({headers[c["cpp"]] for c in classes})
    lines = [
        "// Generated by tools/gen_implement_class.py from analysis/classes.json - do not edit.",
        "//",
        f"// IMPLEMENT_CLASS of the {len(classes)} native classes (standalone build only): each defines",
        "// Class::PrivateStaticClass through Core's native UClass constructor (a dynamic",
        "// initializer plus the atexit of ~UClass, as in the stock image) and autoclass<Class>,",
        "// the export Core finds the class by. Order: the stock initializers (0x100d...).",
        "// Parent and class flags are the stock registration's (NWindow.h, NW_IMPLEMENT_CLASS).",
        "",
        '#include "nwindow/NWindow.h"',
        "",
    ]
    lines += [f'#include "{h}"' for h in includes]
    lines += ["", "#if NW_HOOK_MODE", '#error "Classes.gen.cpp is for the standalone build only"', "#endif", ""]
    # the size registered is sizeof(Class): it must be the stock one
    for c in sorted(classes, key=lambda c: c["initializer"]):
        lines.append(f"NW_ASSERT_SIZE({c['cpp']}, 0x{c['size']:X});")
    lines.append("")
    for c in sorted(classes, key=lambda c: c["initializer"]):
        lines.append(f"NW_IMPLEMENT_CLASS({c['cpp']}, {c['super']}, {flags_expr(c['class_flags'])})"
                     f"  // stock UClass 0x{c['static_class']:08x}")
    lines.append("")
    return "\n".join(lines)


def render_def(exports: list[tuple[str, bool]]) -> str:
    lines = [
        "; Generated by tools/gen_implement_class.py from analysis/classes.json - do not edit.",
        "; The exports of the stock NWindow.dll, same names and ordinals (Core finds GPackage and",
        "; autoclass<Class> by name with GetProcAddress; nothing imports this DLL).",
        "EXPORTS",
    ]
    for k, (name, data) in enumerate(exports):
        lines.append(f"\t{name} @{k + 1}" + (" DATA" if data else ""))
    lines.append("")
    return "\n".join(lines)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true", help="compare only, write nothing")
    args = ap.parse_args()
    classes = json.loads((ROOT / "analysis/classes.json").read_text(encoding="utf-8"))
    bad = [c["cpp"] for c in classes
           if c["flags"] != OBJECT_FLAGS or c["within"] != "UObject" or c["package"] != "NWindow"
           or c["guid"] != [0, 0, 0, 0] or c["static_ctor_raw"] != STATIC_CTOR]
    if bad:
        raise SystemExit(f"registration NW_IMPLEMENT_CLASS does not cover: {', '.join(bad)}")
    exports = export_names(classes)
    problems = check_stock(classes, exports)
    for p in problems:
        print("STOCK", p)
    try:
        cpp = render_cpp(classes, class_headers())
    except SystemExit as e:
        print(f"{OUT_CPP.relative_to(ROOT)} not generated: {e}")
        cpp = None
    deff = render_def(exports)
    status = 0 if not problems else 1
    for path, text in ((OUT_CPP, cpp), (OUT_DEF, deff)):
        if text is None:
            status = 1
            continue
        old = path.read_text(encoding="utf-8") if path.exists() else None
        if args.check:
            if old != text:
                print(f"out of date: {path.relative_to(ROOT)}")
                status = 1
        elif old != text:
            path.write_text(text, encoding="utf-8", newline="\n")
            print(f"wrote {path.relative_to(ROOT)}")
    print(f"{len(classes)} classes, {len(exports)} exports, stock check: "
          f"{'ok' if not problems else f'{len(problems)} problems'}")
    return status


if __name__ == "__main__":
    raise SystemExit(main())
