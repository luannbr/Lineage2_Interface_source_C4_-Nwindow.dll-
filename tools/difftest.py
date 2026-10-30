#!/usr/bin/env python3
"""Differential tests: pieces of NWindow.dll run outside the game on fixed inputs (the runner in
tools/difftest/, CMake target nwd_difftest), once with the stock code and once with ours, and
what they wrote compared. The verifier only compares sets of calls; this compares results.

  python tools/difftest.py                  stock vs ours, every case (exit 1 on a difference)
  python tools/difftest.py --golden         ours vs tests/golden/ (what the stock code wrote)
  python tools/difftest.py --update-golden  rewrite tests/golden/ from the stock code
  python tools/difftest.py --case html      only that case (repeatable); --list shows them
  python tools/difftest.py --no-pages       without the server page sets (the fast cases only)
  python tools/difftest.py --pages NAME=DIR a page set of one's own (the case pages_NAME)
  python tools/difftest.py --standalone [NWindow.dll]
                                            our standalone NWindow.dll (phase 5) vs tests/golden/
  python tools/difftest.py --standalone-self-test
                                            the same path run on orig/NWindow.dll (must be all same)

Standalone: the cases call stock addresses. Each is looked up in our NWindow.dll through its
linker map (/MAP): functions by the full signature of the function our hook-mode build installs
there (build/smoke_system/NWindowDecomp.log + the hook DLL's map, from the last tools/cycle.py);
UClass objects and vtables by class (analysis/classes.json); globals by their NW_GLOBAL name. A case that needs a function the standalone inlined away is SKIPPED (named),
not compared. The standalone System is build/difftest_standalone_system.

Page sets: the HTML tokenizer over every *.htm of a server, read in place (nothing is copied).
Their golden files keep only "text-hash result-hash" pairs: a page edited or added on the server
since the golden was written is not compared (the count is shown), it does not fail.

The test System (build/difftest_system) holds the engine DLLs and a fresh install of the build;
its NWindowDecomp.ini selects the stock code ([Hooks] Enabled=0) or ours. Case outputs go to
build/difftest/stock and build/difftest/ours; lines ending in "h=..." are hashes of details that
--verbose writes out (the driver reruns a differing case that way and shows the diff).
"""
from __future__ import annotations

import argparse
import difflib
import json
import os
import re
import shutil
import subprocess
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from install_test import DEFAULT_SYSTEM, NO_SYSTEM  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from nwd.image import require_c1_tables  # noqa: E402
EXE = ROOT / "build" / "difftest" / "Release" / "nwd_difftest.exe"
SYSDIR = ROOT / "build" / "difftest_system"
OUT = ROOT / "build" / "difftest"
GOLDEN = ROOT / "tests" / "golden"
ENGINE_DEPS = ["Core.dll", "Engine.dll", "IFC23.DLL", "MSVCR70.dll", "ogg.dll", "vorbis.dll", "vorbisfile.dll"]
PAGE_SETS = {
    # the L2J Mobius C1 server next to the repo (a fixed download): html/ and the quest scripts
    "mobius": ROOT.parent / "L2J_Mobius_C1_HarbingersOfWar" / "L2J_Mobius_C1_HarbingersOfWar" / "dist" / "game" / "data",
    # the pages the project's own server sends (they change with it): NWD_C4_SERVER = the server's deploy folder (<deploy>)
    "server": Path(os.environ["NWD_C4_SERVER"]) / "gameserver" / "data" / "html-en" if os.environ.get("NWD_C4_SERVER") else None,
}
# [Log] Flush=0: the log is only read for the hooks' addresses, and a flush per line costs seconds
INI = "[Hooks]\r\nEnabled={}\r\nDisable=\r\n\r\n[Debug]\r\nMarker=0\r\n\r\n[Log]\r\nFlush=0\r\n"
MAX_DETAILS = 10


def prepare(system: Path) -> bool:
    """The test System: engine DLLs (copied once) and the current build installed."""
    SYSDIR.mkdir(parents=True, exist_ok=True)
    for dep in ENGINE_DEPS:
        if not (SYSDIR / dep).exists():
            src = system / dep
            if not src.exists():
                print(f"difftest: {src} missing (engine DLLs come from the test client)")
                return False
            shutil.copy2(src, SYSDIR / dep)
    r = subprocess.run([sys.executable, str(ROOT / "tools" / "install_test.py"), "--system", str(SYSDIR)],
                       capture_output=True, text=True)
    if r.returncode != 0:
        print(r.stdout + r.stderr)
        return False
    return True


class Ours:
    """Where "ours" runs: the hook-mode System (NWindowDecomp.ini, hooks on) or, with
    --standalone, our NWindow.dll and its address table."""
    system = SYSDIR
    extra: list[str] = []
    label = "ours"


def run(stock: bool, cases: list[str], outdir: Path, pages: dict[str, Path], extra: list[str] = ()) -> bool:
    system = SYSDIR if stock else Ours.system
    if system == SYSDIR:
        (SYSDIR / "NWindowDecomp.ini").write_text(INI.format(0 if stock else 1), encoding="ascii", newline="")
    if outdir.exists():
        shutil.rmtree(outdir)
    outdir.mkdir(parents=True)
    cmd = [str(EXE), str(system), str(outdir)]
    for c in cases:
        cmd += ["--case", c]
    for name, folder in pages.items():
        if f"pages_{name}" in cases:
            cmd += ["--pages", f"{name}={folder}"]
    cmd += list(extra) + ([] if stock else Ours.extra)
    t = time.time()
    r = subprocess.run(cmd, capture_output=True, text=True, timeout=900)
    label = "stock" if stock else Ours.label
    if r.returncode != 0:
        print(f"difftest: the runner failed ({label}, exit {r.returncode})\n{r.stdout}{r.stderr}")
        return False
    print(f"  {label}: {r.stdout.strip()} in {time.time() - t:.1f} s")
    return True


def show_diff(old: list[str], new: list[str], la: str, lb: str, limit: int = 40) -> None:
    lines = list(difflib.unified_diff(old, new, la, lb, n=2, lineterm=""))
    for line in lines[:limit]:
        print("    " + line)
    if len(lines) > limit:
        print(f"    ... {len(lines) - limit} more diff lines")


def read_lines(path: Path) -> list[str]:
    return path.read_text(encoding="utf-8", errors="replace").splitlines()


def list_cases() -> list[tuple[str, str]]:
    r = subprocess.run([str(EXE), "--list"], capture_output=True, text=True)
    out = []
    for line in r.stdout.splitlines():
        name, _, about = line.partition(" ")
        out.append((name, about.strip()))
    return out


# ---- page sets: "page text-hash result-hash" lines --------------------------------------------

def page_lines(path: Path) -> list[tuple[str, str, str]]:
    out = []
    for line in read_lines(path):
        name, text, result = line.rsplit(" ", 2)
        out.append((name, text, result))
    return out


def write_page_golden(case: str, src: Path, dst: Path) -> int:
    pairs = sorted({(t, r) for _, t, r in page_lines(src)})
    with dst.open("w", encoding="ascii", newline="\n") as f:
        f.write(f"# {case}: text hash, result hash of each page (tools/difftest.py; no paths, no text)\n")
        for t, r in pairs:
            f.write(f"{t} {r}\n")
    return len(pairs)


def compare_page_golden(case: str, golden: Path, ours: Path) -> list[str]:
    """The pages whose text the golden file knows and whose result differs."""
    known: dict[str, set[str]] = {}
    for line in read_lines(golden):
        if line and not line.startswith("#"):
            t, r = line.split()
            known.setdefault(t, set()).add(r)
    bad, unknown, total = [], 0, 0
    for name, t, r in page_lines(ours):
        total += 1
        if t not in known:
            unknown += 1
        elif r not in known[t]:
            bad.append(name)
    print(f"  {'DIFF' if bad else 'ok  '} {case}: {total - unknown} page(s) compared, {len(bad)} different"
          + (f", {unknown} not in the golden file (edited or new)" if unknown else ""))
    for name in bad[:MAX_DETAILS]:
        print(f"    {name}")
    return bad


def page_details(case: str, pages: dict[str, Path], names: list[str], golden: bool) -> None:
    """The differing pages written out in full, stock against ours (or ours alone)."""
    extra = ["--verbose"]
    for n in names[:MAX_DETAILS]:
        extra += ["--only", n]
    if golden:
        verbose = OUT / f"{Ours.label}_verbose"
        if run(False, [case], verbose, pages, extra):
            print(f"  {Ours.label} on those pages: {verbose / (case + '.txt')}")
        return
    if run(True, [case], OUT / "stock_verbose", pages, extra) and run(False, [case], OUT / "ours_verbose", pages, extra):
        print(f"  details of {case}:")
        show_diff(read_lines(OUT / "stock_verbose" / f"{case}.txt"), read_lines(OUT / "ours_verbose" / f"{case}.txt"),
                  f"stock/{case}", f"ours/{case}", 120)


# ---- standalone ------------------------------------------------------------------------------

STANDALONE_SYSDIR = ROOT / "build" / "difftest_standalone_system"
HOOK_LOG = ROOT / "build" / "smoke_system" / "NWindowDecomp.log"   # tools/cycle.py's smoke run
HOOK_MAP = ROOT / "build" / "bin" / "Release" / "NWindowDecomp.map"   # linked /MAP
GLOBAL_RE = re.compile(r"#define\s+(\w+)\s+NW_GLOBAL\(.+?,\s*(0x[0-9a-fA-F]{8})\)")


def nw_globals() -> dict[int, str]:
    """Stock va -> name of every NW_GLOBAL (the standalone defines them under the same name)."""
    out = {}
    for p in sorted(list((ROOT / "include").rglob("*.h")) + list((ROOT / "src").rglob("*.h"))
                    + list((ROOT / "src").rglob("*.cpp"))):
        for m in GLOBAL_RE.finditer(p.read_text(encoding="utf-8", errors="replace")):
            out[int(m.group(2), 16)] = m.group(1)
    return out


def hook_names() -> dict[int, str]:
    from nwd.symbols import LOG_RE
    out = {}
    if HOOK_LOG.exists():
        for line in HOOK_LOG.read_text(encoding="latin-1").splitlines():
            m = LOG_RE.match(line)
            if m:
                out[int(m.group(1), 16)] = m.group(2).strip()
    return out


def standalone_table(dll: Path) -> tuple[dict[int, tuple[int, str]], dict[int, str]] | None:
    """Stock va -> (rva in our NWindow.dll, name) for what the cases may use; and the names of
    the hooked functions (for the messages)."""
    from nwd.symbols import hook_functions, load_map, signature_index
    mp = dll.with_suffix(".map")
    if not mp.exists():
        print(f"difftest: no linker map {mp}: link the standalone NWindow.dll with /MAP")
        return None
    syms = load_map(mp)
    names = hook_names()
    rows: dict[int, tuple[int, str]] = {}
    if not names or not HOOK_MAP.exists():
        print(f"difftest: no hook-mode run to learn the functions from ({HOOK_LOG}, {HOOK_MAP}): "
              "run tools/cycle.py first; only classes and globals are resolved")
    else:
        # each hook's function by its full signature (name, parameters, const): an overload
        # the standalone inlined away is missing (its cases are skipped), never another one
        hook, unnamed = hook_functions(HOOK_LOG, HOOK_MAP)
        for line in unnamed:
            print(f"difftest: hook without a function name in the map: {line}")
        names.update(hook)  # the messages name an overload by its parameters
        index = signature_index(syms)
        for va, sig in hook.items():
            hits = index.get(sig, set())
            if len(hits) == 1:
                rows[va] = (next(iter(hits)) - syms.base, sig)
            elif hits:
                print(f"difftest: {sig} at {len(hits)} addresses in the standalone, not used")
    classes = json.loads((ROOT / "analysis" / "classes.json").read_text(encoding="utf-8"))
    for c in classes:
        cpp = c["cpp"]
        for dec in (f"?PrivateStaticClass@{cpp}@@0VUClass@@A", f"?PrivateStaticClass@{cpp}@@2VUClass@@A"):
            if syms.find(dec) is not None:
                rows[c["static_class"]] = (syms.find(dec) - syms.base, dec)
        vt = [f"??_7{cpp}@@6BUObject@@@"] if c.get("secondary_vtables") else [f"??_7{cpp}@@6B@"]
        for dec in vt:
            if syms.find(dec) is not None and c.get("vtable"):
                rows[c["vtable"]] = (syms.find(dec) - syms.base, dec)
        for sv in c.get("secondary_vtables", []):
            dec = f"??_7{cpp}@@6BFExec@@@"
            if syms.find(dec) is not None:
                rows[sv["vtable"]] = (syms.find(dec) - syms.base, dec)
    for va, name in nw_globals().items():
        hits = syms.find_pretty(name)
        if len(hits) == 1:
            rows[va] = (next(iter(hits)) - syms.base, name)
    return rows, names


def self_test_table() -> tuple[dict[int, tuple[int, str]], dict[int, str]]:
    """--standalone-self-test: orig/NWindow.dll in the standalone's place, every known stock
    address mapped onto itself (the path of --standalone without its lookups)."""
    import csv
    rows = {}
    for r in csv.DictReader((ROOT / "db" / "functions.csv").open(encoding="utf-8")):
        rows[int(r["va"], 16)] = (int(r["va"], 16) - 0x10000000, r["name"])
    for c in json.loads((ROOT / "analysis" / "classes.json").read_text(encoding="utf-8")):
        for va in [c["static_class"], c.get("vtable")] + [s["vtable"] for s in c.get("secondary_vtables", [])]:
            if va:
                rows[va] = (va - 0x10000000, c["cpp"])
    for va, name in nw_globals().items():
        rows[va] = (va - 0x10000000, name)
    return rows, hook_names()


def prepare_standalone(system: Path, dll: Path) -> bool:
    """build/difftest_standalone_system: the engine DLLs and our NWindow.dll (+ its PDB)."""
    STANDALONE_SYSDIR.mkdir(parents=True, exist_ok=True)
    for dep in ENGINE_DEPS:
        if not (STANDALONE_SYSDIR / dep).exists():
            src = system / dep
            if not src.exists():
                print(f"difftest: {src} missing (engine DLLs come from the test client)")
                return False
            shutil.copy2(src, STANDALONE_SYSDIR / dep)
    for old in STANDALONE_SYSDIR.glob("NWindow*"):
        old.unlink()
    shutil.copy2(dll, STANDALONE_SYSDIR / "NWindow.dll")
    pdb = dll.with_suffix(".pdb")
    if pdb.exists():
        shutil.copy2(pdb, STANDALONE_SYSDIR / pdb.name)
    return True


def setup_standalone(args) -> bool:
    if args.standalone_self_test:
        dll = ROOT / "orig" / "NWindow.dll"
        got = self_test_table()
        print(f"difftest: standalone self-test on {dll.relative_to(ROOT)} (every address onto itself)")
    else:
        from verify_standalone import CANDIDATES
        dll = Path(args.standalone) if args.standalone else next((c for c in CANDIDATES if c.exists()), None)
        if dll is None or not dll.exists():
            print("difftest: no standalone NWindow.dll (looked at "
                  + ", ".join(str(c) for c in ([Path(args.standalone)] if args.standalone else CANDIDATES))
                  + "); build it or pass --standalone <path>")
            return False
        got = standalone_table(dll)
        if got is None:
            return False
    rows, names = got
    if not prepare_standalone(args.system, dll):
        return False
    OUT.mkdir(parents=True, exist_ok=True)
    table = OUT / "standalone_table.txt"
    with table.open("w", encoding="ascii", errors="replace", newline="\n") as f:
        for va, (rva, name) in sorted(rows.items()):
            f.write(f"{va:08x} {rva:08x} {name}\n")
    Ours.system, Ours.extra, Ours.label = STANDALONE_SYSDIR, ["--standalone", str(table)], "standalone"
    Ours.names = names
    print(f"difftest: {dll} - {len(rows)} stock addresses located ({table.relative_to(ROOT)})")
    return True


def skipped(path: Path) -> str | None:
    """The reason a standalone run skipped the case ("SKIPPED: stock <va> ..."), with the name."""
    text = path.read_text(encoding="utf-8", errors="replace")
    if not text.startswith("SKIPPED:"):
        return None
    m = re.search(r"stock ([0-9a-f]{8})", text)
    name = getattr(Ours, "names", {}).get(int(m.group(1), 16), "?") if m else "?"
    return text.strip() + f" ({name}: inlined away or missing)"


# ---- main ----------------------------------------------------------------------------------

def main() -> int:
    # the cases call stock functions by C1 address and tests/golden/ holds the C1 outputs: not for C2 yet
    require_c1_tables("difftest.py", "--force" in sys.argv)
    ap = argparse.ArgumentParser()
    ap.add_argument("--force", action="store_true", help="run on the C2 image anyway (the cases are C1's)")
    ap.add_argument("--golden", action="store_true", help="compare ours with tests/golden/")
    ap.add_argument("--update-golden", action="store_true", help="rewrite tests/golden/ from the stock code")
    ap.add_argument("--case", action="append", default=[])
    ap.add_argument("--pages", action="append", default=[], metavar="NAME=DIR")
    ap.add_argument("--no-pages", action="store_true")
    ap.add_argument("--list", action="store_true")
    ap.add_argument("--system", type=Path, default=DEFAULT_SYSTEM, help="where the engine DLLs come from")
    ap.add_argument("--standalone", nargs="?", const="", default=None, metavar="DLL",
                    help="our standalone NWindow.dll against tests/golden/ (default: the standalone build)")
    ap.add_argument("--standalone-self-test", action="store_true",
                    help="the --standalone path on orig/NWindow.dll itself")
    args = ap.parse_args()
    standalone = args.standalone is not None or args.standalone_self_test
    if standalone:
        args.golden = True

    if not EXE.exists():
        print(f"difftest: {EXE} missing - build first (cmake --build build --config Release)")
        return 2
    pages: dict[str, Path] = {}
    if not args.no_pages:
        pages = {name: folder for name, folder in PAGE_SETS.items() if folder is not None and folder.is_dir()}
        for name, folder in PAGE_SETS.items():
            if name not in pages and not args.list:
                print(f"difftest: page set {name} not found ({folder or 'NWD_C4_SERVER not set'}), skipped")
    for spec in args.pages:
        name, _, folder = spec.partition("=")
        if not name or not Path(folder).is_dir():
            print(f"difftest: --pages NAME=DIR, not {spec}")
            return 2
        pages[name] = Path(folder)
    known = list_cases() + [(f"pages_{n}", f"HTML tokenizer over {p}") for n, p in pages.items()]
    if args.list:
        for name, about in known:
            print(f"{name:14} {about}")
        return 0
    names = [n for n, _ in known]
    for c in args.case:
        if c not in names:
            print(f"difftest: no case {c} ({', '.join(names)})")
            return 2
    cases = args.case or names
    if args.system is None:
        print(f"difftest: {NO_SYSTEM}")
        return 2
    if standalone:
        if not setup_standalone(args):
            return 2
    elif not prepare(args.system):
        return 2
    print(f"difftest: {len(cases)} case(s)" + (f", page sets {', '.join(pages)}" if pages else ""))

    if args.update_golden:
        if not run(True, cases, OUT / "stock", pages):
            return 3
        GOLDEN.mkdir(parents=True, exist_ok=True)
        for c in cases:
            if c.startswith("pages_"):
                n = write_page_golden(c, OUT / "stock" / f"{c}.txt", GOLDEN / f"{c}.txt")
                print(f"  {c}: {n} page(s)")
            else:
                shutil.copy2(OUT / "stock" / f"{c}.txt", GOLDEN / f"{c}.txt")
        print(f"difftest: {len(cases)} golden file(s) written to {GOLDEN.relative_to(ROOT)}")
        return 0

    if args.golden:
        if not run(False, cases, OUT / Ours.label, pages):
            return 3
        bad = skips = 0
        for c in cases:
            g = GOLDEN / f"{c}.txt"
            o = OUT / Ours.label / f"{c}.txt"
            why = skipped(o) if standalone else None
            if why:
                print(f"  SKIP {c}: {why}")
                skips += 1
            elif not g.exists():
                print(f"  MISSING golden {g.relative_to(ROOT)}")
                bad += 1
            elif c.startswith("pages_"):
                diff = compare_page_golden(c, g, o)
                if diff:
                    page_details(c, pages, diff, True)
                    bad += 1
            elif g.read_bytes().replace(b"\r\n", b"\n") != o.read_bytes():
                print(f"  DIFF {c} (golden vs {Ours.label}; --verbose details: build/difftest/{Ours.label}_verbose)")
                show_diff(read_lines(g), read_lines(o), f"golden/{c}", f"{Ours.label}/{c}")
                run(False, [c], OUT / f"{Ours.label}_verbose", pages, ["--verbose"])
                bad += 1
            else:
                print(f"  ok   {c}")
        print(f"difftest: {len(cases) - bad - skips} same, {bad} different"
              + (f", {skips} skipped (not in the standalone)" if skips else ""))
        return 1 if bad else 0

    if not run(True, cases, OUT / "stock", pages) or not run(False, cases, OUT / "ours", pages):
        return 3
    bad = []
    for c in cases:
        s, o = OUT / "stock" / f"{c}.txt", OUT / "ours" / f"{c}.txt"
        if s.read_bytes() == o.read_bytes():
            print(f"  ok   {c}")
            continue
        bad.append(c)
        print(f"  DIFF {c}")
        if c.startswith("pages_"):
            diff = [a[0] for a, b in zip(page_lines(s), page_lines(o)) if a != b]
            for name in diff[:MAX_DETAILS]:
                print(f"    {name}")
            page_details(c, pages, diff, False)
        else:
            show_diff(read_lines(s), read_lines(o), f"stock/{c}", f"ours/{c}")
    plain = [c for c in bad if not c.startswith("pages_")]
    if plain:
        # the hashed lines: the details of the differing cases
        if run(True, plain, OUT / "stock_verbose", pages, ["--verbose"]) and \
                run(False, plain, OUT / "ours_verbose", pages, ["--verbose"]):
            for c in plain:
                print(f"  details of {c}:")
                show_diff(read_lines(OUT / "stock_verbose" / f"{c}.txt"), read_lines(OUT / "ours_verbose" / f"{c}.txt"),
                          f"stock/{c}", f"ours/{c}", 80)
    print(f"difftest: {len(cases) - len(bad)} same, {len(bad)} different")
    return 1 if bad else 0


if __name__ == "__main__":
    os.environ.setdefault("MSYS_NO_PATHCONV", "1")
    sys.exit(main())
