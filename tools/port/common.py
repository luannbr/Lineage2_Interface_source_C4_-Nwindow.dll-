"""Shared model for the C1 -> C2 port tools (tools/port/).

Two images are compared: the C1 build this project started from (its tree lives next to this one,
NWD_C3_ROOT or --c1) and the C2 build in orig/NWindow.dll. Each function is reduced to token lists
that survive a recompilation of the same source:
  s   strict   mnemonic + operands, every address (calls, globals, strings, vtables) replaced by a
               symbol; registers, small constants and field offsets kept
  d   dmask    strict, with the displacement of every [reg+disp] left out (a class layout that
               moved changes only these), stack-frame accesses (esp/ebp) kept
  l   loose    mnemonic + operand kinds only
and the set of wide strings the function refers to (the guard() names among them).
"""
from __future__ import annotations

import collections
import csv
import hashlib
import os
import pickle
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from nwd.image import Image  # noqa: E402
from capstone.x86 import X86_OP_IMM, X86_OP_MEM, X86_OP_REG  # noqa: E402

# In this tree (the C4 project) the "c1" side of every port tool is the BASE tree: our C3 project
# (read only), whose source is already ported and confirmed in game; "c2" is the C4 image in orig/. The names
# (C1_ROOT, NW_C1_ROOT, --c1, the c1_va/c2_va columns) are the ones the C1 -> C2 port used.
# The base tree: the NWD_C3_ROOT variable (a checkout of the C3 project with its orig/ and analysis/ghidra/; the
# old name NW_C1_ROOT works too), or --c1 where a tool takes it. C1_ROOT is looked up on first use, so only
# the tools that read it ask for it.


def _base_root() -> Path:
    if "C1_ROOT" in globals():  # set by a tool's --c1
        return globals()["C1_ROOT"]
    env = os.environ.get("NWD_C3_ROOT") or os.environ.get("NW_C1_ROOT")
    if not env:
        sys.exit("set NWD_C3_ROOT to a checkout of the C3 project (the base tree of the C3 -> C4 port, "
                 "with its orig/ and analysis/ghidra/)")
    return Path(env)


def __getattr__(name: str):
    if name == "C1_ROOT":
        return _base_root()
    raise AttributeError(name)


CACHE = ROOT / "build" / "port_cache"

BASEREG_FRAME = ("esp", "ebp")


def h(toks) -> str:
    return hashlib.md5("\n".join(toks).encode()).hexdigest()


def read_csv(path: Path) -> list[dict]:
    if not path.exists():
        return []
    return list(csv.DictReader((l for l in path.open(encoding="utf-8") if not l.startswith("#"))))


def read_funcs_tsv(path: Path) -> list[dict]:
    out = []
    for r in csv.DictReader(path.open(encoding="utf-8"), delimiter="\t"):
        rg = [tuple(int(x, 16) for x in p.split("-")) for p in r["ranges"].split(",")]
        out.append(dict(va=int(r["va"], 16), size=int(r["size"]), ranges=rg, name=r["name"]))
    out.sort(key=lambda x: x["va"])
    return out


def norm(img: Image, ranges, imports: dict):
    """(strict, dmask, loose, strings, calls) of one function (all its ranges).
    calls: direct call targets in instruction order (VA or 'imp:<name>')."""
    strict, dmask, loose, strs, calls = [], [], [], set(), []
    fstart, fend = ranges[0][0], max(r[1] for r in ranges) + 1
    hi_addr = img.base + img.pe.OPTIONAL_HEADER.SizeOfImage

    def isaddr(v):
        return img.base <= v < hi_addr

    for lo, hi in ranges:
        for i in img.md.disasm(img.read(lo, hi - lo + 1), lo):
            m = i.mnemonic
            if m in ("int3", "nop"):
                continue
            ops, dops, lops = [], [], []
            for op in i.operands:
                if op.type == X86_OP_REG:
                    ops.append(i.reg_name(op.reg)); dops.append(i.reg_name(op.reg)); lops.append("r")
                elif op.type == X86_OP_IMM:
                    v = op.imm & 0xFFFFFFFF
                    if m == "call":
                        t = "T"
                        calls.append(v)
                    elif m.startswith("j"):
                        t = ("J%d" % (v - fstart)) if fstart <= v < fend else "T"
                    elif isaddr(v):
                        t = "A"
                        s = img.wstr(v, 200) if not img.in_text(v) else None
                        if s and len(s) >= 4 and all(32 <= ord(c) < 0x2000 for c in s):
                            strs.add(s)
                    else:
                        t = "%x" % v
                    ops.append(t); dops.append(t); lops.append("i")
                elif op.type == X86_OP_MEM:
                    mm = op.mem
                    d = mm.disp & 0xFFFFFFFF
                    base = i.reg_name(mm.base) if mm.base else ""
                    if isaddr(d) and not mm.base:
                        imp = imports.get(d)
                        dd = "I:" + imp if imp else "A"
                        if m == "call" and imp:
                            calls.append("imp:" + imp)
                    else:
                        dd = "%x" % d
                    idx = i.reg_name(mm.index) if mm.index else ""
                    ops.append("[%s+%s*%d+%s]" % (base, idx, mm.scale, dd))
                    dd2 = dd if (base in BASEREG_FRAME or dd == "A" or dd.startswith("I:")) else "D"
                    dops.append("[%s+%s*%d+%s]" % (base, idx, mm.scale, dd2))
                    lops.append("m")
            strict.append(m + " " + ",".join(ops))
            dmask.append(m + " " + ",".join(dops))
            loose.append(m + " " + ",".join(lops))
    return strict, dmask, loose, strs, calls


class Model:
    """One image + its function list, normalized (cached by file size/mtime)."""

    def __init__(self, tag: str, dll: Path, funcs_tsv: Path):
        self.tag, self.dll, self.tsv = tag, Path(dll), Path(funcs_tsv)
        self.img = Image(self.dll)
        self.img.md.detail = True
        key = "%s_%d_%d_%d_%d" % (tag, self.dll.stat().st_size, int(self.tsv.stat().st_mtime),
                                  self.tsv.stat().st_size, 2)
        CACHE.mkdir(parents=True, exist_ok=True)
        cf = CACHE / (key + ".pkl")
        if cf.exists():
            self.funcs = pickle.load(open(cf, "rb"))
        else:
            imports = {va: n for va, (_, n) in self.img.imports.items()}
            self.funcs = read_funcs_tsv(self.tsv)
            for x in self.funcs:
                x["s"], x["d"], x["l"], x["str"], x["calls"] = norm(self.img, x["ranges"], imports)
                x["hs"], x["hd"], x["hl"] = h(x["s"]), h(x["d"]), h(x["l"])
            pickle.dump(self.funcs, open(cf, "wb"))
        self.by_va = {x["va"]: x for x in self.funcs}
        self.idx = {x["va"]: i for i, x in enumerate(self.funcs)}


def c1_model() -> Model:
    base = _base_root()
    m = Model("c1", base / "orig" / "NWindow.dll", base / "analysis" / "ghidra" / "functions.tsv")
    kinds = {int(r["va"], 16): r for r in read_csv(base / "db" / "functions.csv")}
    for x in m.funcs:
        k = kinds.get(x["va"])
        x["kind"] = k["kind"] if k else "?"
        x["status"] = k["status"] if k else "?"
        x["cls"] = (k["class"] or k["name"].split("::")[0]) if k else ""
        x["nm"] = k["name"] if k else x["name"]
    return m


def c2_model() -> Model:
    # the census list (ghidra/census/funcs.tsv, the seeded project) is complete and stable; the
    # export's own functions.tsv is written while the export runs
    p = ROOT / "ghidra" / "census" / "funcs.tsv"
    if not p.exists():
        p = ROOT / "analysis" / "ghidra" / "functions.tsv"
    return Model("c2", ROOT / "orig" / "NWindow.dll", p)
