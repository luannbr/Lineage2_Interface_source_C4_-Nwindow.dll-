#!/usr/bin/env python3
"""What the standalone NWindow.dll still misses (phase 5, docs/notas/Standalone.md, section 10).

Builds the target NWindowStandalone (the game source with NW_HOOK_MODE=0, not part of the hook
cycle) and turns the link errors into the work list, grouped by what has to be written:

  NW_STOCK        a body still in the stock image (_nw_stock_<va>; NW_STOCK_AT is the same)
  nw::StockCallAt thiscall into the stock image by address (C3/C4 sources; one symbol per signature, the sites are listed)
  nw::StockCall   the same, the C1/C2 spelling (none left in C4)
  NW_GLOBAL       a stock global reached by address (nw::StockData<va>::Bytes), and the package
                  globals declared for the standalone build but not defined yet
  nw::Addr        a stock table or object reached by address (one symbol; the sites are listed)
  CRT             C runtime / C-linkage names
  other           anything else (a declared function without a body, a missing import...)

Each unresolved symbol comes with the objects that reference it and, for the scaffolding, the
source lines outside hook-only blocks (#if NW_HOOK_MODE) that produce it.

  python tools/standalone_report.py [--no-build] [--probe] [--md]
    --no-build  parse the last build log (build/standalone_build.log) instead of building
    --probe     also link NWindowStandaloneProbe (/FORCE:UNRESOLVED, never loaded) and compare
                its exports, resources, class registrations and vtables with orig/NWindow.dll
                (tools/verify_standalone.py and tools/port/check_internal_ctors.py on the probe; the
                vtable slots that still hold an int3 stub are counted apart from the other differences)
    --md        Markdown tables (for docs/)
Exit code: 0 when the DLL links, 1 when symbols are unresolved, 2 when the build fails before
the link (compile errors).
"""
from __future__ import annotations

import argparse
import os
import re
import subprocess
import sys
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build"
LOG = BUILD / "standalone_build.log"
OUT_DLL = BUILD / "standalone" / "Release" / "NWindow.dll"
PROBE_DLL = BUILD / "standalone_probe" / "Release" / "NWindow_probe.dll"
ORIG = ROOT / "orig" / "NWindow.dll"

# LNK2001 / LNK2019 in any language: the object, then the first quoted name with its decorated
# form in parentheses, or a bare C name (/include of _nw_stock_<va>)
LNK_RE = re.compile(r'^\s*(?P<obj>[^\s:][^:]*?)\s*:\s*(?:error|warning)\s+LNK20(?:01|19):[^"\n]*?'
                    r'(?:"(?P<und>[^"]+)"\s*\((?P<dec>[^)\s]+)\)|\s(?P<bare>_[A-Za-z0-9_@$?]+)\s*\[)', re.M)
COMPILE_ERR_RE = re.compile(r"^.*?: (?:fatal )?error C\d{4}:.*$", re.M)
LINK_FATAL_RE = re.compile(r"^.*?error LNK(?!20(?:01|19))\d{4}:.*$", re.M)

GROUPS = ["NW_STOCK", "nw::StockCallAt", "nw::StockCall", "NW_GLOBAL", "nw::Addr", "CRT", "other"]
# who converts a group (db/lanes_s5.json; docs/notas/C4_Standalone.md)
OWNERS = {"NW_STOCK": "the lane of the file (NNewVirtuals.gen.cpp: sa-vslots-console / sa-vslots-other)",
          "nw::StockCallAt": "the lane of the file (python tools/port/sa_sites.py --list names the target)",
          "nw::StockCall": "the lane of the file",
          "NW_GLOBAL": "sa-globals-rest (NwGlobals.cpp)", "nw::Addr": "the lane of the file; sa-globals-rest for the data tables",
          "CRT": "sa-globals-rest", "other": "?"}


# ---- build ----------------------------------------------------------------------------------

def build(target: str) -> str:
    env = dict(os.environ, VSLANG="1033", MSYS_NO_PATHCONV="1")  # English tool messages
    # configure every time (well under a second): the src/standalone glob must see a file that
    # came or went with a checkout or a merge before MSBuild loads the project
    cfg = ["cmake", "-S", ".", "-B", "build"]
    if not (BUILD / "CMakeCache.txt").exists():
        cfg += ["-G", "Visual Studio 17 2022", "-A", "Win32"]
    subprocess.run(cfg, cwd=ROOT, env=env, check=True, capture_output=True)
    p = subprocess.run(["cmake", "--build", "build", "--config", "Release", "--target", target, "--", "-m"],
                       cwd=ROOT, env=env, capture_output=True, text=True, errors="replace")
    return p.stdout + p.stderr


# ---- source sites ---------------------------------------------------------------------------

def standalone_lines(path: Path):
    """(line number, text) of the lines a standalone compile sees: #if NW_HOOK_MODE blocks are
    skipped (their #else kept); any other conditional is kept whole."""
    stack: list[tuple[str, bool]] = []  # (kind, active) per open #if
    for n, line in enumerate(path.read_text(encoding="utf-8", errors="replace").splitlines(), 1):
        s = line.strip()
        m = re.match(r"#\s*(if|ifdef|ifndef|elif|else|endif)\b(.*)", s)
        if m:
            d, cond = m.group(1), m.group(2).strip()
            if d in ("if", "ifdef", "ifndef"):
                if d == "if" and cond == "NW_HOOK_MODE":
                    stack.append(("hook", False))
                elif d == "if" and cond in ("!NW_HOOK_MODE", "NW_HOOK_MODE == 0"):
                    stack.append(("hook", True))
                else:
                    stack.append(("other", True))
            elif d == "else" and stack and stack[-1][0] == "hook":
                stack[-1] = ("hook", not stack[-1][1])
            elif d == "endif" and stack:
                stack.pop()
            continue
        if all(active for _, active in stack):
            yield n, line


def source_sites() -> dict[str, list[tuple[str, str, str]]]:
    """{'NW_STOCK:<va>' / 'NW_GLOBAL:<va>' / 'nw::Addr' / 'nw::StockCall': [(file:line, va, name)]}"""
    out: dict[str, list] = defaultdict(list)
    files = sorted((ROOT / "include").rglob("*.h")) + sorted((ROOT / "src" / "nwindow").rglob("*.cpp"))
    for path in files:
        rel = path.relative_to(ROOT).as_posix()
        for n, line in standalone_lines(path):
            code = line.split("//")[0]
            if code.lstrip().startswith("#define NW_"):
                continue  # the macros themselves
            define = re.match(r"\s*#\s*define\s+(\w+)", code)
            name = define.group(1) if define else ""
            for m in re.finditer(r"NW_STOCK(?:_AT)?\(\s*(0x[0-9A-Fa-f]+)", code):
                out[f"NW_STOCK:{int(m.group(1), 16):08x}"].append((f"{rel}:{n}", m.group(1), name))
            for m in re.finditer(r"NW_GLOBAL\(\s*[^,]+,\s*(0x[0-9A-Fa-f]+)\s*\)", code):
                out[f"NW_GLOBAL:{int(m.group(1), 16):08x}"].append((f"{rel}:{n}", m.group(1), name))
            for m in re.finditer(r"nw::Addr\(\s*(0x[0-9A-Fa-f]+)", code):
                out["nw::Addr"].append((f"{rel}:{n}", m.group(1), name))
            for m in re.finditer(r"StockCall<[^>]*>\(\s*(0x[0-9A-Fa-f]+)", code):
                out["nw::StockCall"].append((f"{rel}:{n}", m.group(1), name))
            for m in re.finditer(r"StockCallAt\s*<[^(]*>\s*\(\s*(0x[0-9A-Fa-f]+)", code):
                out["nw::StockCallAt"].append((f"{rel}:{n}", m.group(1), name))
    return out


# ---- link errors ----------------------------------------------------------------------------

def classify(sym: str, und: str) -> tuple[str, str]:
    """(group, key) of an unresolved symbol."""
    if sym.startswith("_nw_stock_"):
        return "NW_STOCK", f"NW_STOCK:{int(sym[len('_nw_stock_'):], 16):08x}"
    m = re.search(r"nw::StockData<(\d+)>::Bytes", und)
    if m:
        return "NW_GLOBAL", f"NW_GLOBAL:{int(m.group(1)):08x}"
    if "nw::StockCallAt<" in und:
        return "nw::StockCallAt", "nw::StockCallAt"
    if "nw::StockCall<" in und:
        return "nw::StockCall", "nw::StockCall"
    if re.search(r"\bnw::Addr\(", und):
        return "nw::Addr", "nw::Addr"
    # a package global: C++ data (?Name@@3...) named G<Something>
    if re.match(r"\?G\w+@@3", sym):
        return "NW_GLOBAL", "declared"
    if not sym.startswith("?"):
        return "CRT", sym
    return "other", sym


def parse_link(log: str):
    """{symbol: {'und', 'group', 'key', 'objs'}}"""
    syms: dict[str, dict] = {}
    for m in LNK_RE.finditer(log):
        sym = m.group("dec") or m.group("bare")
        und = m.group("und") or sym
        obj = Path(m.group("obj").strip()).name
        e = syms.setdefault(sym, {"und": und, "objs": set()})
        e["objs"].add(obj)
    for sym, e in syms.items():
        e["group"], e["key"] = classify(sym, e["und"])
    return syms


# ---- probe ----------------------------------------------------------------------------------

def probe_verify(mp: Path) -> list[str]:
    """The probe against orig/NWindow.dll: tools/verify_standalone.py (pe, resources, exports, classes, vtables,
    imports) and tools/port/check_internal_ctors.py (the compiled InternalConstructors and registrations). A vtable
    slot that points to an int3 stub (a NW_STOCK / NW_UNPORTED placeholder nobody replaced yet) is counted apart from
    the other differences; so are the imports /OPT:REF dropped with the unreachable code (they come back when the
    link closes)."""
    lines = []
    py = [sys.executable]
    r = subprocess.run(py + [str(ROOT / "tools" / "verify_standalone.py"), "--dll", str(PROBE_DLL), "--map", str(mp)],
                       cwd=ROOT, capture_output=True, text=True, errors="replace")
    section, stubs, imps = "", 0, 0
    for line in r.stdout.splitlines():
        m = re.match(r"=== (\w+):", line)
        if m:
            section = m.group(1)
            continue
        if re.match(r"  (\w+\s+\d+ ok,|\d+/\d+ (classes|vtables))", line) or \
                (line.startswith("verify_standalone: ") and ("failed" in line or line.endswith("OK"))):
            lines.append("verify: " + line.strip())
        if re.match(r"  \d+ slots in \d+ vtables still point to an int3 stub", line):
            lines.append("vtables: " + line.strip())
        if not line.startswith("  FAIL"):
            continue
        if section == "imports":
            imps += 1
        elif section == "vtables" and ":stubs:" in line:
            stubs += 1
        elif section in ("pe", "resources", "exports", "classes", "vtables"):
            lines.append("DIFFERENCE " + line.strip()[:600])
    if imps:
        lines.append(f"imports: {imps} names the stock's game code uses are not imported yet (the code that calls them is "
                     "unreachable or still a stub; see `python tools/verify_standalone.py --checks imports`)")
    r = subprocess.run(py + [str(ROOT / "tools" / "port" / "check_internal_ctors.py"), "--probe", str(PROBE_DLL),
                             "--map", str(mp)], cwd=ROOT, capture_output=True, text=True, errors="replace")
    for line in r.stdout.splitlines():
        if re.match(r"\d+ compiled (InternalConstructors|UClass registrations)", line) or "MISMATCH" in line \
                or "NO-DEFAULT-CTOR" in line:
            lines.append("ctors: " + line.strip()[:400])
    return lines

def check_probe() -> list[str]:
    import pefile

    lines = []
    o, p = pefile.PE(str(ORIG)), pefile.PE(str(PROBE_DLL))

    def exports(pe):
        return [(s.ordinal, s.name.decode()) for s in sorted(pe.DIRECTORY_ENTRY_EXPORT.symbols, key=lambda s: s.ordinal)]

    def resources(pe):
        out = {}
        for t in pe.DIRECTORY_ENTRY_RESOURCE.entries:
            for e in t.directory.entries:
                for lang in e.directory.entries:
                    d = lang.data.struct
                    out[(t.id, e.id, lang.id)] = pe.get_data(d.OffsetToData, d.Size)
        return out

    eo, ep = exports(o), exports(p)
    lines.append(f"exports: {len(ep)}, " + ("same names and ordinals as the stock image" if eo == ep
                                            else "DIFFERENT from the stock image"))
    ro, rp = resources(o), resources(p)
    same = sorted(ro) == sorted(rp) and all(ro[k] == rp[k] for k in ro)
    lines.append(f"resources: {len(rp)} entries, " + ("byte-identical to the stock ones" if same
                                                      else f"DIFFERENT (stock {sorted(ro)}, ours {sorted(rp)})"))
    oh = p.OPTIONAL_HEADER
    lines.append(f"ImageBase {oh.ImageBase:#x}, DllCharacteristics {oh.DllCharacteristics:#x} "
                 f"(stock {o.OPTIONAL_HEADER.ImageBase:#x}, {o.OPTIONAL_HEADER.DllCharacteristics:#x})")
    imps = {e.dll.decode().lower(): len(e.imports) for e in p.DIRECTORY_ENTRY_IMPORT}
    lines.append("imports: " + ", ".join(f"{k} {v}" for k, v in sorted(imps.items())))
    mp = PROBE_DLL.with_suffix(".map")
    lines.append(f"map: {mp.relative_to(ROOT).as_posix()}" + ("" if mp.exists() else " (missing)"))
    if mp.exists():
        lines += probe_verify(mp)
    return lines


# ---- report ---------------------------------------------------------------------------------

def objlist(objs, limit: int = 8) -> str:
    o = sorted(objs)
    return ", ".join(o[:limit]) + (f" (+{len(o) - limit} more)" if len(o) > limit else "")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--no-build", action="store_true")
    ap.add_argument("--probe", action="store_true")
    ap.add_argument("--md", action="store_true")
    args = ap.parse_args()

    if args.no_build:
        log = LOG.read_text(encoding="utf-8", errors="replace")
    else:
        print("building NWindowStandalone...", file=sys.stderr, flush=True)
        log = build("NWindowStandalone")
        LOG.write_text(log, encoding="utf-8")

    errs = sorted(set(COMPILE_ERR_RE.findall(log)))
    if errs:
        print(f"COMPILE ERRORS ({len(errs)}), see {LOG}:")
        for e in errs[:40]:
            print("  " + e.strip())
        return 2
    syms = parse_link(log)
    fatal = [f for f in set(LINK_FATAL_RE.findall(log)) if "LNK1120" not in f]
    linked = not syms and not fatal and OUT_DLL.exists()

    sites = source_sites()
    by_group: dict[str, list] = defaultdict(list)
    for sym, e in syms.items():
        by_group[e["group"]].append((sym, e))

    md = args.md
    h = (lambda s: f"\n### {s}\n") if md else (lambda s: f"\n== {s}")

    def fence():
        if md:
            print("```")
    print("# Standalone report" if md else "Standalone report (NWindowStandalone, Release)")
    if linked:
        print(f"LINKED: {OUT_DLL.relative_to(ROOT).as_posix()}")
    else:
        print(f"link: {len(syms)} unresolved symbols" + (f", {len(fatal)} other link errors" if fatal else ""))
    counts = ", ".join(f"{g} {len(by_group[g])}" for g in GROUPS if by_group[g])
    if counts:
        print("by group: " + counts)
    for f in sorted(fatal):
        print("  " + f.strip())

    for g in GROUPS:
        items = by_group[g]
        if not items:
            continue
        print(h(f"{g} - {len(items)} (owner: {OWNERS[g]})"))
        if g in ("NW_STOCK", "NW_GLOBAL"):
            rows = []
            for sym, e in items:
                if e["key"] == "declared":
                    name = re.findall(r"\w+", e["und"])[-1]
                    rows.append(("-", name, "declared (NwStandalone.h, hook-only in its header), not defined",
                                 e["objs"]))
                    continue
                va = e["key"].split(":")[1]
                ss = sites.get(e["key"], [])
                names = sorted({n for _, _, n in ss if n}) or ["-"]
                where = ", ".join(sorted({s for s, _, _ in ss})) or "(no site found)"
                rows.append((f"0x{va}", " ".join(names), where, e["objs"]))
            rows.sort()
            if md:
                print("| VA | Name | Source | Referenced by |\n|---|---|---|---|")
            for va, name, where, objs in rows:
                o = objlist(objs)
                print(f"| {va} | `{name}` | {where} | {o} |" if md else f"  {va:10s} {name:28s} {where}\n{'':13s}<- {o}")
        elif g in ("nw::StockCallAt", "nw::StockCall", "nw::Addr"):
            fence()
            for sym, e in sorted(items):
                print(f"  {e['und']}\n    <- {objlist(e['objs'])}")
            ss = sites.get(g, [])
            print(f"  sites ({len(ss)}):")
            for where, va, name in sorted(ss):
                print(f"    {where}  {va}" + (f"  {name}" if name else ""))
            fence()
        else:
            fence()
            for sym, e in sorted(items):
                print(f"  {e['und']}" + (f"  [{sym}]" if sym != e["und"] else "")
                      + f"\n    <- {objlist(e['objs'])}")
            fence()

    if args.probe:
        print(h("probe (NWindow_probe.dll, /FORCE:UNRESOLVED - never loaded)"))
        plog = build("NWindowStandaloneProbe")
        if not PROBE_DLL.exists() or COMPILE_ERR_RE.search(plog):
            print("  probe link failed")
        else:
            for line in check_probe():
                print("  " + line)
    return 0 if linked else 1


if __name__ == "__main__":
    sys.exit(main())
