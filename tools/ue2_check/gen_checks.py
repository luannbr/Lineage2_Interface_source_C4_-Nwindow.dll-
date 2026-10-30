#!/usr/bin/env python3
"""Generates the two translation units tools/ue2_check/run_check.py compiles from the declarations of
include/ue2 (UE2Core.h, UE2Engine.h, gen/*.vt.inc):

  probes.cpp     one class per class of the headers that declares virtuals, derived from it, that overrides
                 every declared virtual with the SAME signature and stores its decorated name
                 (__FUNCDNAME__) in the body (out of class, so the compiler always emits it). run_check.py then
                 reads, from the assembler listing, the decorated name the compiler gives each virtual (with
                 the name of the probe class swapped for the class) and, from /d1reportSingleClassLayout, the
                 slot it sits in, and compares both with the vtable of the C4 DLL: the order AND the exact
                 signature of every virtual.
  link_refs.cpp  a function that references every ENGINE_API / CORE_API function and variable of the headers
                 (calls through a null pointer, never executed): linked against the import libraries made from
                 the .def files, a wrong decorated name is an unresolved external. Classes with an exported
                 constructor and pure virtuals are built through a derived class that implements the pure ones
                 (that also pulls in the imported virtuals of every base through the vtable).

usage: gen_checks.py <outdir>
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import hdrparse as hp  # noqa: E402

# class -> (dll, the DLL's vtable symbol of its primary (UObject-side) vftable)
PROBES = {
    "UObject": ("Core", "??_7UObject@@6B@"),
    "USubsystem": ("Core", "??_7USubsystem@@6BUObject@@@"),
    "FMalloc": ("Core", "??_7FMalloc@@6B@"),
    "FFileManager": ("Core", "??_7FFileManager@@6B@"),
    "FArchive": ("Core", "??_7FArchive@@6B@"),
    "FOutputDevice": ("Core", "??_7FOutputDevice@@6B@"),
    "FExec": ("Core", "??_7FExec@@6B@"),
    "UCanvas": ("Engine", "??_7UCanvas@@6B@"),
    "ULevel": ("Engine", "??_7ULevel@@6BUObject@@@"),
    "UL2Font": ("Engine", "??_7UL2Font@@6BUObject@@@"),
    "UPrimitive": ("Engine", "??_7UPrimitive@@6B@"),
    "UMesh": ("Engine", "??_7UMesh@@6B@"),
    "AActor": ("Engine", "??_7AActor@@6B@"),
    "APawn": ("Engine", "??_7APawn@@6B@"),
    "AController": ("Engine", "??_7AController@@6B@"),
    "UPlayer": ("Engine", "??_7UPlayer@@6BUObject@@@"),
    "UViewport": ("Engine", "??_7UViewport@@6BUObject@@@"),
    "UAudioSubsystem": ("Engine", "??_7UAudioSubsystem@@6BUObject@@@"),
    "URenderDevice": ("Engine", "??_7URenderDevice@@6BUObject@@@"),
    "UEngine": ("Engine", "??_7UEngine@@6BUObject@@@"),
    "UGameEngine": ("Engine", "??_7UGameEngine@@6BUObject@@@"),
    "UL2NetHandler": ("Engine", "??_7UL2NetHandler@@6BUObject@@@"),
    "UL2ConsoleWnd": ("Engine", "??_7UL2ConsoleWnd@@6BUObject@@@"),
    "FL2NetNotify": ("Engine", "??_7FL2NetNotify@@6B@"),
}

PRIMITIVES = {"void", "bool", "INT", "UINT", "LONG", "DWORD", "WORD", "_WORD", "BYTE", "SBYTE", "SWORD", "QWORD", "SQWORD",
              "FLOAT", "DOUBLE", "UBOOL", "TCHAR", "ANSICHAR", "UNICHAR", "BITFIELD", "SIZE", "size_t", "int", "char",
              "float", "double", "short", "long", "unsigned", "signed"}


def dummy_types(decls: list[hp.Decl]) -> str:
    """Types a virtual takes or returns BY VALUE that the headers only forward-declare (FMatrix, NItemRequest,
    HennaInfo...): the check units give them a body so the overrides compile. Their layout does not matter (the
    code never runs), only the tag (class / struct) does, because it is in the mangled name."""
    text = hp.strip_comments((hp.INC / "gen" / "EngineFwd.gen.h").read_text(encoding="utf-8"))
    for f in ("UE2Core.h", "UE2Engine.h"):
        text += hp.strip_comments((hp.INC / f).read_text(encoding="utf-8"))
    tag = {m.group(2): m.group(1) for m in re.finditer(r"^\s*(class|struct)\s+(\w+)\s*;", text, re.M)}
    enums = set(re.findall(r"\benum\s+(\w+)", text))
    typedefs = set(re.findall(r"\btypedef\s+[^;]*?\b(\w+)\s*;", text))
    out = set()
    for d in decls:
        if not (d.is_func and (d.virtual or d.api)):
            continue
        for t in d.params + [d.ret]:
            m = re.fullmatch(r"(?:const\s+)?(\w+)", t.strip())
            if not m:
                continue
            n = m.group(1)
            if n in PRIMITIVES or n in enums or n in typedefs or n in hp.BASES:
                continue
            if n in tag:
                out.add(n)
    lines = []
    for n in sorted(out):
        lines.append("%s %s { %sint Pad[16]; };" % (tag[n], n, "public: " if tag[n] == "class" else ""))
    lines.append("struct ThaiCharacter { int A[4]; };")
    return "\n".join(lines) + "\n"


def drop_defaults(raw: str) -> str:
    parts = []
    for p in hp.split_top(raw):
        parts.append(p if p == "..." else re.sub(r"\s*=\s*.*$", "", p))
    return ", ".join(parts)


def default_body(ret: str) -> str:
    if ret == "void":
        return ""
    if ret.endswith("&"):
        return "return *(%s*)0;" % ret[:-1].strip()
    return "%s x{}; return x;" % ret


def override_text(d: hp.Decl, probe: str, with_name: bool) -> tuple[str, str]:
    """(the declaration inside the probe class, the out-of-class definition: always emitted, so the
    decorated name string is in the listing even though nothing instantiates the probe)."""
    conv = (d.conv + " ") if d.conv else ""
    const = " const" if d.const else ""
    params = drop_defaults(d.raw_params)
    decl = "    %s %s%s(%s)%s;" % (d.ret, conv, d.name, params, const)
    mark = "const char* dn = __FUNCDNAME__; (void)dn; " if with_name else ""
    defn = "%s %s%s::%s(%s)%s { %s%s }" % (d.ret, conv, probe, d.name, params, const, mark, default_body(d.ret))
    return decl, defn


def gen_probes(decls: list[hp.Decl], out: Path) -> list[str]:
    lines = ['// generated by tools/ue2_check/gen_checks.py: do not edit',
             '#include "ue2/UE2Core.h"', '#include "ue2/UE2Engine.h"', dummy_types(decls)]
    done = []
    for cls in PROBES:
        vs = [d for d in decls if d.cls == cls and d.is_func and d.virtual and not d.is_dtor and d.access == "public"]
        if not vs:
            continue
        lines.append("class Probe_%s : public %s\n{\npublic:" % (cls, cls))
        defs = []
        for d in vs:
            decl, defn = override_text(d, "Probe_" + cls, True)
            lines.append(decl)
            defs.append(defn)
        lines.append("};\nvoid* g_Probe_%s = (void*)sizeof(Probe_%s);" % (cls, cls))
        lines.extend(defs)
        lines.append("")
        done.append(cls)
    (out / "probes.cpp").write_text("\n".join(lines) + "\n", encoding="utf-8")
    return done


def zv(t: str) -> str:
    return "ZG<%s>::get()" % t


def chain(cls: str) -> list[str]:
    """The class and its bases, bases first."""
    out: list[str] = []

    def walk(c: str) -> None:
        for b in hp.BASES.get(c, []):
            walk(b)
        if c not in out:
            out.append(c)
    walk(cls)
    return out


def remaining_pure(decls: list[hp.Decl], cls: str) -> list[hp.Decl]:
    """The pure virtuals of cls and its bases that no class of the chain implements."""
    cl = chain(cls)
    by_class = {c: [d for d in decls if d.cls == c and d.is_func and d.virtual and not d.is_dtor] for c in cl}
    out = []
    for i, c in enumerate(cl):
        for d in by_class[c]:
            if not d.pure:
                continue
            key = (d.name, tuple(d.params), d.const)
            if not any((q.name, tuple(q.params), q.const) == key and not q.pure
                       for c2 in cl[i + 1:] for q in by_class[c2]):
                out.append(d)
    return out


def gen_link_refs(decls: list[hp.Decl], out: Path) -> tuple[int, list[str]]:
    head = ['// generated by tools/ue2_check/gen_checks.py: do not edit',
            '#include "ue2/UE2Core.h"', '#include "ue2/UE2Engine.h"', '#include <new>', dummy_types(decls),
            'template <class T> struct Strip { typedef T type; };',
            'template <class T> struct Strip<T&> { typedef T type; };',
            'template <class T> struct ZG { static T get() { return *(typename Strip<T>::type*)0; } };', '']
    body = ['extern "C" __declspec(dllexport) void ue2_link_refs()', '{',
            '    static char buf[0x80000];', '    (void)buf;',
            '    static void* volatile sink[0x400];', '    int k = 0;']
    n = 0
    skipped: list[str] = []
    seen_cls: set[str] = set()
    link_classes: set[str] = set()
    for d in decls:
        if not d.api:
            continue
        what = (d.cls + "::" if d.cls else "") + d.name
        if d.is_func:
            if d.pure:
                continue
            if d.cls and d.access != "public":
                skipped.append(f"{what} ({d.access})")
                continue
            args = ", ".join(zv(t) for t in d.params)
            if d.is_ctor:
                if remaining_pure(decls, d.cls):
                    if not d.params:
                        link_classes.add(d.cls)          # built through Link_<class>, below
                        body.append(f"    new (buf) Link_{d.cls};   // {d.cls}::{d.cls}() through a derived class")
                        n += 1
                    else:
                        skipped.append(f"{what} (abstract class, constructor with arguments)")
                else:
                    body.append(f"    new (buf) {d.cls}({args});")
                    n += 1
            elif d.is_dtor:
                body.append(f"    ((({d.cls}*)buf))->{d.cls}::~{d.cls}();")
                n += 1
            elif d.cls and d.static:
                body.append(f"    {d.cls}::{d.name}({args});")
                n += 1
            elif d.cls:
                body.append(f"    ((({d.cls}*)buf))->{d.cls}::{d.name}({args});")
                n += 1
            else:
                body.append(f"    {d.name}({args});")
                n += 1
        elif d.extern or not d.cls:
            body.append(f"    sink[k++] = (void*)&{d.name};   // a store, so the compiler keeps the reference (the linker resolves the DATA export)")
            n += 1
        elif d.static and d.access == "public":
            body.append(f"    sink[k++] = (void*)&{d.cls}::{d.name};")
            n += 1
        elif d.static:
            skipped.append(f"{what} (private static data, referenced through StaticClass())")
    for d in decls:
        if d.is_func and d.name == "StaticClass" and d.access == "public" and d.cls and d.cls not in seen_cls:
            seen_cls.add(d.cls)
            body.append(f"    (void){d.cls}::StaticClass();")
            n += 1
    # the private members the inline code of the headers reaches: FMemMark's constructor calls FMemStack::GetThreadSafeMemStack (C4)
    body.append("    { FMemMark ue2_mark(GMem); ue2_mark.Pop(); }")
    body.append("}\n")
    body.append("extern \"C\" int __stdcall DllMain(void*, unsigned long, void*) { return 1; }")
    # Link_<class>: implements every pure virtual of the chain
    links = []
    for cls in sorted(link_classes):
        links.append("class Link_%s : public %s\n{\npublic:" % (cls, cls))
        defs = []
        for d in remaining_pure(decls, cls):
            decl, defn = override_text(d, "Link_" + cls, False)
            links.append(decl)
            defs.append(defn)
        links.append("};")
        links.extend(defs)
        links.append("")
    (out / "link_refs.cpp").write_text("\n".join(head + links + body) + "\n", encoding="utf-8")
    return n, skipped


def main(argv: list[str]) -> int:
    out = Path(argv[0])
    out.mkdir(parents=True, exist_ok=True)
    decls = hp.parse([hp.INC / "UE2Core.h", hp.INC / "UE2Engine.h"])
    probes = gen_probes(decls, out)
    n, skipped = gen_link_refs(decls, out)
    print(f"probes.cpp: {len(probes)} probe classes; link_refs.cpp: {n} references, {len(skipped)} not referenced")
    for s in skipped:
        if "PrivateStaticClass" not in s:
            print("   not referenced:", s)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
