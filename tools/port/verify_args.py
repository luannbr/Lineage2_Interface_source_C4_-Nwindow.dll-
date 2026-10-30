#!/usr/bin/env python3
"""The constant arguments of every call: stock against ours.

verify_hooks compares the shape of a function (ret N, the virtual slots, the imports) and verify_data its
operands (displacements, constants as sets). Neither sees *which value a call gets*: NCTargetStatusWnd::OnCreate
built its frame with NewFrameCtrl(1, 0, 0) where the stock pushes `edi` three times (edi = 0), so the frame made a
close button of its own and the window showed two X. This judge follows the constants through the registers.

For every hooked function, on both sides:
  * a forward dataflow over the function's own control flow keeps what each register holds when that is a
    constant (`xor r, r`, `mov r, imm`, copies) or a cached import / vtable slot (`mov ebx, [IAT]; ... call ebx`);
    joins keep only what every path agrees on; calls clobber eax / ecx / edx;
  * each `call` / tail `jmp` gets a key (stock VA of the callee, an import name, or a virtual slot) and the values
    of the arguments pushed since the previous call (nearest first; None = not a known constant);
  * ours is mapped to stock addresses through the hook table (our forwarders, NW_STOCK stubs, import thunks).
A stock call whose known arguments contradict EVERY call of the same callee on our side is reported.

usage: python tools/port/verify_args.py [--log FILE] [--dll FILE] [--only HEXVA] [--quiet]
Output: db/verify_args.csv (va, name, callee, stock args, ours args, state); exit code 1 when there are
unexplained rows (db/verify_args_notes.csv: va,callee,explanation  -> NOTE)."""
from __future__ import annotations

import argparse
import os
import bisect
import csv
import re
import struct
import sys
from pathlib import Path

import pefile
from capstone import CS_ARCH_X86, CS_MODE_32, Cs

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import verify_hooks as vh  # noqa: E402
from nwd.image import ROOT, Image  # noqa: E402

REG32 = ("eax", "ebx", "ecx", "edx", "esi", "edi", "ebp", "esp")
PARENT = {"al": "eax", "ah": "eax", "ax": "eax", "bl": "ebx", "bh": "ebx", "bx": "ebx", "cl": "ecx", "ch": "ecx",
          "cx": "ecx", "dl": "edx", "dh": "edx", "dx": "edx", "si": "esi", "di": "edi", "bp": "ebp", "sp": "esp"}
PARENT.update({r: r for r in REG32})
NOWRITE = {"cmp", "test", "push", "jmp", "ret", "retn", "nop", "call", "fld", "fstp", "fst", "fild", "fadd", "fsub",
           "fmul", "fdiv", "fcom", "fcomp", "fnstsw", "fnstcw", "fldcw", "fxch", "fchs", "fabs", "fsubr", "fdivr",
           "faddp", "fsubp", "fmulp", "fdivp", "fucomp", "fcompp", "fucom", "fucompp", "fldz", "fld1", "fistp",
           "fist", "fldpi", "frndint", "fsqrt", "fsin", "fcos", "fpatan"}
ST_SET = {"stosd", "stosb", "stosw", "movsd", "movsb", "movsw", "scasb", "scasd", "lodsd", "lodsb", "cmpsb", "cmpsd"}
IMM = re.compile(r"^-?(0x[0-9a-f]+|\d+)$")
MEMABS = re.compile(r"^dword ptr \[(0x[0-9a-f]+)\]$")
MEMREG = re.compile(r"^dword ptr \[(e[a-ds][xip]|e[sd]i|ebp)(?: \+ (0x[0-9a-f]+))?\]$")
TABLE = re.compile(r"^dword ptr \[e[a-ds][xip]\*4 \+ (0x[0-9a-f]+)\]$")


def imm(s: str):
    s = s.strip()
    if IMM.match(s):
        return int(s, 0) & 0xFFFFFFFF
    return None


STACK = "$S"      # the pushed values (tuple, deepest first); None = not a known constant
FPC = "$F"         # the constant the x87 top holds right after `fld const`
SLOT = re.compile(r"^dword ptr \[esp(?: \+ (0x[0-9a-f]+|\d+))?\]$")
SLOTSET = re.compile(r"^dword ptr \[esp(?: \+ (0x[0-9a-f]+|\d+))?\], (.+)$")


def join(a: dict, b: dict) -> dict:
    out = {}
    for k, v in a.items():
        if k not in b:
            continue
        if k == STACK:
            w = b[k]
            n = min(len(v), len(w))
            out[k] = tuple(x if x == y else None for x, y in zip(v[:n], w[:n]))
        elif b[k] == v:
            out[k] = v
    return out


def ptr_none(v):
    """an address inside either image (string literals, SEH handlers, vtables) differs by construction"""
    if v is not None and 0x10000000 <= v < 0x20000000:
        return None
    return v


def analyze(md, code, start, resolve_target, stub_resolver):
    """-> {callee key: [(arglist, post), ...]} for the function at `start` (arglist[0] = the last value pushed;
    post = the stack words a `add esp, N` right after the call pops, or None)."""
    extent = code.extent(start)
    ins_list = []
    for a, b in extent:
        ins_list.extend(vh.decode_skipping_tables(md, code, a, b))
    if not ins_list:
        return {}
    by_addr = {i.address: n for n, i in enumerate(ins_list)}

    def inside(t):
        return any(a <= t < b for a, b in extent)

    succ = {}
    for n, ins in enumerate(ins_list):
        nxt = [ins_list[n + 1].address] if n + 1 < len(ins_list) else []
        mn, op = ins.mnemonic, ins.op_str
        if mn in ("ret", "retn", "int3", "ud2"):
            nxt = []
        elif mn == "jmp":
            if op.startswith("0x"):
                t = int(op, 16)
                nxt = [t] if inside(t) else []
            else:
                m = TABLE.match(op)
                nxt = []
                if m:
                    t = int(m.group(1), 16)
                    while inside(t):
                        v = int.from_bytes(code.read(t, 4), "little")
                        if not inside(v):
                            break
                        nxt.append(v)
                        t += 4
        elif mn.startswith("j") and op.startswith("0x"):
            t = int(op, 16)
            if inside(t):
                nxt.append(t)
        succ[ins.address] = nxt

    # ---- forward dataflow of the register state, the pushed stack and the x87 top ----------------
    state_in = {ins_list[0].address: {STACK: ()}}
    work = [ins_list[0].address]
    budget = 60 * len(ins_list)
    while work and budget > 0:
        budget -= 1
        a = work.pop()
        n = by_addr.get(a)
        if n is None:
            continue
        st = dict(state_in[a])
        transfer(ins_list[n], st, code)
        for s in succ.get(a, ()):
            if s not in by_addr:
                continue
            if s not in state_in:
                state_in[s] = dict(st)
                work.append(s)
            else:
                j = join(state_in[s], st)
                if j != state_in[s]:
                    state_in[s] = j
                    work.append(s)

    # ---- the calls and their pushed arguments --------------------------------------------------
    out: dict = {}
    for n, ins in enumerate(ins_list):
        a = ins.address
        st = state_in.get(a)
        if st is None:
            continue
        mn, op = ins.mnemonic, ins.op_str
        if os.environ.get("VA_TRACE") and mn.startswith(("f", "push", "sub", "call")):
            print("   trace %08x %-6s %-32s fpc=%s stack=%s" % (a, mn, op, st.get(FPC), st.get(STACK)))
        if mn not in ("call", "jmp"):
            continue
        key = None
        if op.startswith("0x"):
            t = int(op, 16)
            if mn == "jmp" and inside(t):
                continue
            key = resolve_target(t)
        else:
            m = MEMABS.match(op)
            if m and int(m.group(1), 16) in code.imports:
                key = ("imp", code.imports[int(m.group(1), 16)])
            elif TABLE.match(op):
                key = None
            else:
                m = MEMREG.match(op)
                if m:
                    key = ("vt", int(m.group(2), 0) if m.group(2) else 0)
                elif op in PARENT:
                    h = st.get(op)
                    if isinstance(h, tuple) and h[0] == "base":
                        key = ("va", vh.STUB_BASE + h[1])
                    elif isinstance(h, tuple):
                        key = h
        if key is None:
            continue
        post = None
        if n + 1 < len(ins_list):
            nx = ins_list[n + 1]
            if nx.mnemonic == "add" and nx.op_str.startswith("esp, "):
                v = imm(nx.op_str.split(",")[1])
                if v is not None and v % 4 == 0 and v <= 0x80:
                    post = v // 4
        out.setdefault(key, []).append((list(reversed(st.get(STACK, ()))), post))
    return out


def set_slot(st: dict, k: int, v):
    S = st.get(STACK, ())
    if k % 4 == 0 and 0 <= k // 4 < len(S):
        i = len(S) - 1 - k // 4
        st[STACK] = S[:i] + (v,) + S[i + 1:]


def transfer(ins, st: dict, code):
    mn, op = ins.mnemonic, ins.op_str
    nwbase = code.nwbase
    parts = [p.strip() for p in op.split(",", 1)] if op else []
    S = st.get(STACK, ())

    # ---- the pushed stack and the x87 constants ----
    if mn == "push":
        v = imm(op)
        if v is None and op in PARENT:
            v = st.get(PARENT[op])
            v = v if isinstance(v, int) else None
        st[STACK] = (S + (ptr_none(v),))[-64:]
        return
    if mn == "call":
        st[STACK] = ()
        st.pop(FPC, None)
        for r in ("eax", "ecx", "edx"):
            st.pop(r, None)
        return
    if mn in ("ret", "retn", "leave"):
        st[STACK] = ()
        if mn == "leave":
            st.pop("ebp", None)
        return
    if mn == "sub" and op.startswith("esp, "):
        v = imm(op.split(",")[1])
        if v is not None and v % 4 == 0 and 0 < v <= 0x40:
            st[STACK] = (S + (None,) * (v // 4))[-64:]
        return
    if mn == "add" and op.startswith("esp, "):
        v = imm(op.split(",")[1])
        if v is not None and v % 4 == 0 and 0 < v <= 0x100:
            st[STACK] = S[:max(0, len(S) - v // 4)]
        return
    if mn == "mov" and len(parts) == 2 and parts[0] in ("esp",):
        st[STACK] = ()
        return
    if mn == "mov" and op.startswith("dword ptr [esp"):
        m = SLOTSET.match(op)
        if m:
            src = m.group(2).strip()
            v = imm(src)
            if v is None and src in PARENT:
                v = st.get(PARENT[src])
                v = v if isinstance(v, int) else None
            set_slot(st, int(m.group(1), 0) if m.group(1) else 0, ptr_none(v))
        return
    if mn == "fld1":
        st[FPC] = 0x3F800000
        return
    if mn == "fldz":
        st[FPC] = 0
        return
    if mn == "fld":
        st.pop(FPC, None)
        m = re.match(r"^(dword|qword) ptr \[(0x[0-9a-f]+)\]$", op)
        if m:
            try:
                raw = bytes(code.read(int(m.group(2), 16), 8))
                if m.group(1) == "dword":
                    st[FPC] = int.from_bytes(raw[:4], "little")
                else:
                    st[FPC] = struct.unpack("<I", struct.pack("<f", struct.unpack("<d", raw)[0]))[0]
            except Exception:  # noqa: BLE001
                pass
        return
    if mn in ("fstp", "fst") and op.startswith("dword ptr [esp"):
        m = SLOT.match(op)
        if m:
            set_slot(st, int(m.group(1), 0) if m.group(1) else 0, ptr_none(st.get(FPC)))
        if mn == "fstp":
            st.pop(FPC, None)
        return
    if mn.startswith("f") and mn not in ("fnstcw", "fldcw"):
        if mn in ("fstp", "fst", "fistp", "fist", "fcomp", "fucomp"):
            st.pop(FPC, None)
        elif mn not in ("fcom", "fucom", "fnstsw"):
            st.pop(FPC, None)
        return
    if mn == "pop":
        st[STACK] = S[:-1]
        if parts and parts[0] in PARENT:
            st.pop(PARENT[parts[0]], None)
        return

    # ---- registers ----
    if mn in ST_SET or mn.startswith("rep"):
        for r in ("eax", "ecx", "esi", "edi"):
            st.pop(r, None)
        return
    if mn in ("cdq", "cwde", "cwd", "cbw", "mul", "div", "idiv", "imul") and (len(parts) <= 1):
        st.pop("eax", None)
        st.pop("edx", None)
        return
    if mn in ("pushad", "popad", "enter"):
        for r in REG32:
            st.pop(r, None)
        st[STACK] = ()
        return
    if not parts or mn in NOWRITE or mn.startswith("j"):
        return
    dst = parts[0]
    if dst not in PARENT:
        return                          # a store to memory: no register changes
    r = PARENT[dst]
    if mn == "xor" and len(parts) == 2 and parts[1] == dst:
        st[r] = 0
        return
    if mn == "mov" and dst in REG32 and len(parts) == 2:
        src = parts[1]
        v = imm(src)
        if v is not None:
            st[r] = v
            return
        if src in REG32:
            if src in st:
                st[r] = st[src]
            else:
                st.pop(r, None)
            return
        m = MEMABS.match(src)
        if m and int(m.group(1), 16) in code.imports:
            st[r] = ("imp", code.imports[int(m.group(1), 16)])
            return
        if m and nwbase is not None and int(m.group(1), 16) == nwbase:
            st[r] = ("base", 0)            # Addr(va) = g_nwBase + (va - 0x10000000)
            return
        m = MEMREG.match(src)
        if m:
            st[r] = ("vt", int(m.group(2), 0) if m.group(2) else 0)
            return
    if mn == "add" and len(parts) == 2 and dst in REG32 and isinstance(st.get(r), tuple) and st[r][0] == "base":
        v = imm(parts[1])
        if v is not None:
            st[r] = ("base", (st[r][1] + v) & 0xFFFFFFFF)
            return
    if mn == "lea" and len(parts) == 2 and dst in REG32:
        m = re.match(r"^\[(e[a-ds][xip]|e[sd]i|ebp) \+ (0x[0-9a-f]+)\]$", parts[1])
        if m and isinstance(st.get(m.group(1)), tuple) and st[m.group(1)][0] == "base":
            st[r] = ("base", (st[m.group(1)][1] + int(m.group(2), 16)) & 0xFFFFFFFF)
            return
    if mn == "xchg" and len(parts) == 2:
        st.pop(r, None)
        if parts[1] in PARENT:
            st.pop(PARENT[parts[1]], None)
        return
    st.pop(r, None)


def main() -> int:
    from install_test import DEFAULT_SYSTEM

    ap = argparse.ArgumentParser()
    ap.add_argument("--log", type=Path, default=DEFAULT_SYSTEM / "NWindowDecomp.log" if DEFAULT_SYSTEM else None)
    ap.add_argument("--dll", type=Path, default=ROOT / "build" / "bin" / "Release" / "NWindowDecomp.dll")
    ap.add_argument("--only", help="a stock VA (hex)")
    ap.add_argument("--quiet", action="store_true")
    ap.add_argument("--missing", action="store_true", help="also list stock callees that ours never calls")
    ap.add_argument("--smoke", action="store_true", help="read build/smoke_system/NWindowDecomp.log (the cycle's)")
    args = ap.parse_args()
    if args.smoke:
        args.log = ROOT / "build" / "smoke_system" / "NWindowDecomp.log"
    if args.log is None:
        ap.error("pass --log FILE or --smoke, or set NWD_CLIENT_SYSTEM (the test client's System folder, where NWindowDecomp.log is)")

    md = Cs(CS_ARCH_X86, CS_MODE_32)
    stock = Image()
    granges = {}
    for r in csv.DictReader((ROOT / "analysis/ghidra/functions.tsv").open(encoding="utf-8"), delimiter="\t"):
        granges[int(r["va"], 16)] = [tuple(int(x, 16) for x in rg.split("-")) for rg in r["ranges"].split(",")]
    for r in csv.DictReader(l for l in (ROOT / "db/not_functions.csv").open(encoding="utf-8") if not l.startswith("#")):
        owner, frag = int(r["part_of"], 16), int(r["va"], 16)
        if owner in granges and frag in granges:
            granges[owner] = granges[owner] + granges[frag]
    for r in csv.DictReader(l for l in (ROOT / "db/extra_functions.csv").open(encoding="utf-8") if not l.startswith("#")):
        va = int(r["va"], 16)
        granges[va] = [(va, va + int(r["size"]) - 1)]
    s_lo, s_hi = stock.sect_range(".text")
    stock_code = vh.Code(stock.read, {va: n for va, (_, n) in stock.imports.items()},
                         lambda va: [(a, b + 1) for a, b in granges.get(va, [(va, va + 0x3F)])],
                         lambda va: s_lo <= va < s_hi)

    pe = pefile.PE(str(args.dll))
    base = pe.OPTIONAL_HEADER.ImageBase
    mem = pe.get_memory_mapped_image()
    o_imports = {i.address: (i.name or b"").decode() for e in pe.DIRECTORY_ENTRY_IMPORT for i in e.imports}
    symbols = vh.our_symbols(args.dll)
    starts = [a for a, _ in symbols]
    o_text = next(s for s in pe.sections if s.Name.startswith(b".text"))
    o_lo, o_hi = base + o_text.VirtualAddress, base + o_text.VirtualAddress + o_text.Misc_VirtualSize

    def o_extent(va):
        i = bisect.bisect_right(starts, va)
        end = starts[i] if i < len(starts) else o_hi
        return [(va, end)]

    ours_code = vh.Code(lambda va, n: mem[va - base:va - base + n], o_imports, o_extent,
                        lambda va: o_lo <= va < o_hi)
    ours_code.nwbase = None
    mp = args.dll.with_suffix(".map")
    if mp.exists():
        for ln in mp.read_text(encoding="latin-1").splitlines():
            m = re.match(r"^\s*0003:[0-9a-f]{8}\s+_g_nwBase\s+([0-9a-f]{8})\s", ln)
            if m:
                ours_code.nwbase = int(m.group(1), 16)
                break
    stock_code.nwbase = None

    rows = []
    for line in args.log.read_text(encoding="latin-1").splitlines():
        m = vh.LOG_RE.match(line)
        if m:
            rows.append((int(m.group(1), 16), m.group(2), int(m.group(3), 16)))
    if args.only:
        rows = [r for r in rows if r[0] == int(args.only, 16)]

    # ---- ours -> stock address of every callee we can name ------------------------------------
    alias: dict[int, int] = {}          # our address (forwarder or compiled body) -> stock VA

    def follow_forwarder(a):
        """address of the compiled body behind a `push ebp; mov ebp, esp; pop ebp; jmp body` forwarder"""
        data = ours_code.read(a, 24)
        for ins in md.disasm(bytes(data), a):
            if ins.mnemonic in ("push", "mov", "pop", "nop"):
                continue
            if ins.mnemonic == "jmp" and ins.op_str.startswith("0x"):
                return int(ins.op_str, 16)
            break
        return None

    for va, _, rva in rows:
        alias.setdefault(base + rva, va)
        t = follow_forwarder(base + rva)
        if t is not None and o_lo <= t < o_hi:
            alias.setdefault(t, va)

    def resolve_ours(t):
        if t in alias:
            return ("va", alias[t])
        if not (o_lo <= t < o_hi):
            return None
        b = ours_code.read(t, 12)
        if len(b) == 12 and b[0] == 0xA1 and b[5] == 0x05 and tuple(b[10:12]) == (0xFF, 0xE0):
            return ("va", vh.STUB_BASE + int.from_bytes(b[6:10], "little"))
        if len(b) >= 6 and b[:2] == b"\xff\x25":
            name = o_imports.get(int.from_bytes(b[2:6], "little"))
            return ("imp", name) if name else None
        t2 = follow_forwarder(t)
        if t2 is not None and t2 != t and t2 in alias:
            return ("va", alias[t2])
        return None

    def resolve_stock(t):
        b = stock.read(t, 6)
        if b[:2] == b"\xff\x25":
            name = stock_code.imports.get(int.from_bytes(b[2:6], "little"))
            if name:
                return ("imp", name)
        return ("va", t)

    names = {}
    for r in csv.DictReader((ROOT / "db/functions.csv").open(encoding="utf-8")):
        names[int(r["va"], 16)] = r["name"]

    notes = {}
    nf = ROOT / "db" / "verify_args_notes.csv"
    if nf.exists():
        for r in csv.DictReader(l for l in nf.open(encoding="utf-8") if not l.startswith("#")):
            notes[(int(r["va"], 16), r["callee"])] = r["explanation"]

    def kname(k):
        if k[0] == "va":
            return names.get(k[1], "%08x" % k[1])
        if k[0] == "imp":
            return "import " + str(k[1])
        return "vslot +0x%x" % k[1]

    def fmt(v):
        return "?" if v is None else ("%d" % v if v < 0x10000 else "0x%x" % v)

    def compatible(s, o):
        for x, y in zip(s, o):
            if x is not None and y is not None and x != y:
                return False
        return True

    ret_cache = {}

    def callee_nargs(key):
        """stack arguments the callee pops (`ret N`), None when unknown"""
        if key[0] != "va":
            return None
        if key[1] not in ret_cache:
            try:
                r = vh.features(md, stock_code, key[1])[0]
            except Exception:  # noqa: BLE001
                r = set()
            ret_cache[key[1]] = (min(r) // 4) if len(r) == 1 else None
        return ret_cache[key[1]]

    fsize = {}
    for r in csv.DictReader((ROOT / "db/functions.csv").open(encoding="utf-8")):
        fsize[int(r["va"], 16)] = int(r["size"])

    def callee_size(v):
        return fsize.get(v, 0)

    out_rows = []
    n_ok = n_note = n_diff = n_skip = 0
    for va, name, rva in sorted(rows):
        try:
            sc = analyze(md, stock_code, va, resolve_stock, None)
            ostart = base + rva
            tf = follow_forwarder(ostart)
            if tf is not None and o_lo <= tf < o_hi:
                ostart = tf
            oc = analyze(md, ours_code, ostart, resolve_ours, None)
        except Exception as e:  # noqa: BLE001
            n_skip += 1
            if not args.quiet:
                print("SKIP %08x %s: %s" % (va, name, e))
            continue
        if args.only:
            for lab, d in (("stock", sc), ("ours", oc)):
                for k, lst in d.items():
                    for a_, post in lst:
                        print("  %s %-40s (%s) post=%s" % (lab, kname(k), " ".join(fmt(v) for v in a_), post))
        bad = []
        for key, slist in sc.items():
            olist0 = oc.get(key)
            if not olist0:
                continue
            nmax = callee_nargs(key)
            def cut(item):
                lst, post = item
                n_ = post if post is not None else nmax
                return lst[:n_] if n_ is not None else lst
            olist = [cut(i) for i in olist0]
            for s in (cut(i) for i in slist):
                known = [v for v in s if v is not None]
                if not known:
                    continue
                if any(compatible(s, o) for o in olist):
                    continue
                # report the closest candidate: the one that agrees on the most positions
                best = max(olist, key=lambda o: sum(1 for x, y in zip(s, o) if x is not None and x == y))
                bad.append((key, s, best))
        if args.missing:
            # direct callees the stock calls and ours never does (not even through a stub of its own)
            for key in sc:
                if key[0] == "va" and key not in oc and callee_size(key[1]) > 12:
                    print("MISS %08x %s -> %s" % (va, name, kname(key)))
        if not bad:
            n_ok += 1
            continue
        seen = set()
        for key, s, o in bad:
            sig = (key, tuple(s), tuple(o))
            if sig in seen:
                continue
            seen.add(sig)
            cn = kname(key)
            note = notes.get((va, cn)) or notes.get((va, "*"))
            tag = "NOTE" if note else "DIFF"
            n_note += bool(note)
            n_diff += not note
            out_rows.append((va, name, cn, " ".join(fmt(v) for v in s), " ".join(fmt(v) for v in o), tag))
            if not args.quiet or tag == "DIFF":
                print("%s %08x %s -> %s  stock (%s)  ours (%s)" % (tag, va, name, cn, " ".join(fmt(v) for v in s),
                                                                  " ".join(fmt(v) for v in o)))
    with (ROOT / "db" / "verify_args.csv").open("w", encoding="utf-8", newline="") as f:
        w = csv.writer(f)
        w.writerow(["va", "name", "callee", "stock_args", "ours_args", "state"])
        for r in out_rows:
            w.writerow(["%08x" % r[0]] + list(r[1:]))
    print("\nverify_args: %d functions clean, %d explained, %d unexplained argument mismatches (%d skipped)"
          % (n_ok, n_note, n_diff, n_skip))
    return 1 if n_diff else 0


if __name__ == "__main__":
    sys.exit(main())
