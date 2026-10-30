#!/usr/bin/env python3
"""Register assumptions of the stock code that a hook can break.

The stock NWindow.dll was built with whole-program optimization: a caller may keep using
ecx/edx (or eax) across a direct call when the callee is known not to touch them, and a
callee may take arguments in registers. Our replacements follow the standard conventions,
so every stock caller that still runs (not hooked, or hooked but disabled in the ini) must
not rely on a hooked function - or on a stock function that calls one - preserving a
volatile register.

  python tools/regcheck.py --game-src report problems for the hooks in src/nwindow (none without the flag)
  python tools/regcheck.py --all      also list the call sites that are safe only because
                                      the caller is hooked too (they break if the caller is
                                      disabled in NWindowDecomp.ini)

Checks:
  1. entry: a hooked function reads a register at entry that its convention does not
     define (edx/eax, or ecx for a non-method)
  2. after call: an unhooked stock caller reads a volatile register after a direct call
     (or tail jump) whose stock target preserved it but whose target now does not, because
     it is hooked or because it calls something hooked
"""
from __future__ import annotations

import argparse
import bisect
import csv
import re
import sys
from collections import defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from capstone import CS_ARCH_X86, CS_MODE_32, Cs  # noqa: E402
from capstone.x86 import X86_OP_IMM, X86_OP_MEM  # noqa: E402
from nwd.image import ROOT, Image  # noqa: E402
from build_db import HOOK_RE, hook_name  # noqa: E402

VOL = ("eax", "ecx", "edx")
FAMILY = {}
for fam, subs in (("eax", ("al", "ah", "ax", "eax")), ("ecx", ("cl", "ch", "cx", "ecx")),
                  ("edx", ("dl", "dh", "dx", "edx"))):
    for s in subs:
        FAMILY[s] = fam
LOW8 = {"al", "cl", "dl"}
HIGH8 = {"ah", "ch", "dh"}
W16 = {"ax", "cx", "dx"}
THISCALL_MANGLE = re.compile(r"@@[A-Z][AB]E")  # member (access, const) + E = thiscall


def load_ranges():
    ranges = {}
    for r in csv.DictReader((ROOT / "analysis/ghidra/functions.tsv").open(encoding="utf-8"), delimiter="\t"):
        ranges[int(r["va"], 16)] = [tuple(int(x, 16) for x in rg.split("-")) for rg in r["ranges"].split(",")]
    for r in csv.DictReader(l for l in (ROOT / "db/not_functions.csv").open(encoding="utf-8") if not l.startswith("#")):
        owner, frag = int(r["part_of"], 16), int(r["va"], 16)
        if owner in ranges and frag in ranges:
            ranges[owner] = ranges[owner] + ranges.pop(frag)
    for r in csv.DictReader(l for l in (ROOT / "db/extra_functions.csv").open(encoding="utf-8") if not l.startswith("#")):
        va = int(r["va"], 16)
        ranges[va] = [(va, va + int(r["size"]) - 1)]
        # take the body away from the function Ghidra filed it under
        for f, rgs in list(ranges.items()):
            if f != va:
                ranges[f] = [(lo, hi) for lo, hi in rgs if not (lo <= va <= hi)] or rgs
    return ranges


# The game source whose hooks are checked: src/nwindow, only with --game-src (until it is regenerated
# for the C2 image it is the C1 copy, whose hook addresses mean nothing here). Without it: no hooks.
SRC_DIRS: list[Path] = []


def source_files():
    return [p for d in SRC_DIRS for p in sorted(d.rglob("*.cpp"))]


def load_hooks():
    """{stock va: (kind, name)} for every NW_HOOK_* in the game source."""
    hooks = {}
    for path in source_files():
        for m in HOOK_RE.finditer(path.read_text(encoding="utf-8", errors="replace")):
            hooks[int(m.group(2), 16)] = (m.group(1), hook_name(m.group(1), m.group(3)))
    return hooks


def disabled_by_ini(path, hooks):
    """Hooks NWindowDecomp.ini turns off (Disable= addresses, Class::Method, Class::*)."""
    text = Path(path).read_text(encoding="latin-1")
    m = re.search(r"^\s*Disable\s*=(.*)$", text, re.M)
    items = [x for x in re.split(r"[\s,]+", m.group(1).split(";")[0]) if x] if m else []
    out = set()
    for it in items:
        for va, (_, name) in hooks.items():
            by_va = re.fullmatch(r"(0x)?[0-9A-Fa-f]{8}", it) and int(it, 16) == va
            by_class = it.endswith("::*") and name.startswith(it[:-1])
            if by_va or it == name or by_class:
                out.add(va)
    return out


class Analysis:
    def __init__(self):
        self.img = Image()
        self.md = Cs(CS_ARCH_X86, CS_MODE_32)
        self.md.detail = True
        self.ranges = load_ranges()
        self.names = {}
        for r in csv.DictReader((ROOT / "db/functions.csv").open(encoding="utf-8")):
            self.names[int(r["va"], 16)] = r["name"]
        self.imports = {va: n for va, (_, n) in self.img.imports.items()}
        self._entry = {}
        self._addrs = {}
        self.ins = {}       # address -> instruction
        self.owner = {}     # address -> function start
        for f, rgs in self.ranges.items():
            for lo, hi in rgs:
                data = self.img.read(lo, hi + 1 - lo)
                for i in self.md.disasm(data, lo):
                    self.ins[i.address] = i
                    self.owner[i.address] = f

    def name(self, va):
        return self.names.get(va, f"sub_{va:08x}")

    def in_func(self, f, va):
        return any(lo <= va <= hi for lo, hi in self.ranges.get(f, ()))

    # ---- per-instruction register effects ------------------------------------------------
    def direct_target(self, i):
        if len(i.operands) == 1 and i.operands[0].type == X86_OP_IMM:
            return i.operands[0].imm
        return None

    def mem_import(self, i):
        if len(i.operands) == 1 and i.operands[0].type == X86_OP_MEM:
            m = i.operands[0].mem
            if m.base == 0 and m.index == 0:
                return self.imports.get(m.disp)
        return None

    def effects(self, i, preserved):
        """(reads, writes) as volatile register names (eax, ax, al, ah...); a call reads what
        its target reads at entry and writes what `preserved`(target) does not keep."""
        mn = i.mnemonic
        if mn == "call":
            t = self.direct_target(i)
            if t is not None:
                return set(self.entry_live(t)), set(VOL) - preserved(t)
            imp = self.mem_import(i)
            if imp is None and i.op_str in ("eax", "ebx", "ecx", "edx", "esi", "edi", "ebp"):
                imp = self.reg_import(i.address, i.op_str)
            if imp is not None:
                reads = {"ecx"} if THISCALL_MANGLE.search(imp) else set()
            else:
                reads = {"ecx"}  # vtable or register call: thiscall as far as we can tell
            reads |= set(re.findall(r"\b(eax|ecx|edx)\b", i.op_str))
            return reads, set(VOL)
        ops = [o.strip() for o in i.op_str.split(",")]
        if mn in ("xor", "sub", "sbb") and len(ops) == 2 and ops[0] == ops[1]:
            return set(), {ops[0]} if ops[0] in FAMILY else set()
        # `or r, -1` / `and r, 0` set r whatever it held (MSVC's short `mov r, -1` / `mov r, 0`)
        if len(ops) == 2 and ops[0] in FAMILY and (
                (mn == "or" and ops[1] in ("0xffffffff", "-1")) or (mn == "and" and ops[1] == "0")):
            return set(), {ops[0]}
        # `lea r, [r]`: alignment filler before a loop head, no effect
        if mn == "lea" and len(ops) == 2 and ops[1] == f"[{ops[0]}]":
            return set(), set()
        if mn == "push" and i.op_str in FAMILY:
            return set(), set()  # stack slot allocation or saved copy: not a use of the value
        read, written = i.regs_access()
        r = {i.reg_name(x) for x in read if i.reg_name(x) in FAMILY}
        w = {i.reg_name(x) for x in written if i.reg_name(x) in FAMILY}
        return r, w

    def reg_import(self, va, reg):
        """Import loaded into `reg` by the nearest earlier `mov reg, [iat slot]` of the same
        function (the stock code keeps often-called imports in ebx/ebp/esi/edi)."""
        f = self.owner.get(va)
        if f not in self._addrs:
            self._addrs[f] = sorted(a for a, o in self.owner.items() if o == f)
        addrs = self._addrs[f]
        k = bisect.bisect_left(addrs, va) - 1
        for j in range(k, max(k - 400, -1), -1):
            i = self.ins[addrs[j]]
            ops = [o.strip() for o in i.op_str.split(",")]
            if i.mnemonic == "mov" and len(ops) == 2 and ops[0] == reg:
                m = re.fullmatch(r"dword ptr \[(0x[0-9a-f]+)\]", ops[1])
                return self.imports.get(int(m.group(1), 16)) if m else None
        return None

    def fam_writes(self, i):
        _, w = self.effects(i, lambda t: set())
        return {FAMILY[n] for n in w}

    # ---- control flow inside one function ------------------------------------------------
    def successors(self, f, i):
        mn = i.mnemonic
        nxt = i.address + i.size
        if mn in ("ret", "retn"):
            return [], None
        if mn == "jmp":
            t = self.direct_target(i)
            if t is not None:
                if self.in_func(f, t):
                    return [t], None
                return [], t  # tail call
            m = re.match(r"dword ptr \[(\w+)\*4 \+ (0x[0-9a-f]+)\]", i.op_str)
            if m:
                table, out = int(m.group(2), 16), []
                for k in range(256):
                    try:
                        v = self.img.u32(table + 4 * k)
                    except Exception:
                        break
                    if not self.in_func(f, v):
                        break
                    out.append(v)
                return out, None
            return [], None  # unknown indirect jump: stop
        if mn.startswith("j") or mn.startswith("loop"):
            t = self.direct_target(i)
            return ([nxt, t] if t is not None else [nxt]), None
        return [nxt], None

    def live_from(self, f, start, regs, preserved):
        """Register families of `regs` whose incoming value is read on some path from `start`
        before being overwritten. Tracks partial writes: after `mov al, x` a read of al uses
        the new byte, a read of eax still uses the old upper bits."""
        live, seen = set(), set()
        stack = [(start, frozenset(regs), frozenset(), frozenset())]
        steps = 0
        while stack and steps < 50000:
            va, pend, low, word = stack.pop()
            if not pend or (va, pend, low, word) in seen:
                continue
            seen.add((va, pend, low, word))
            i = self.ins.get(va)
            if i is None or self.owner.get(va) != f:
                continue
            steps += 1
            reads, writes = self.effects(i, preserved)
            for n in reads:
                fam = FAMILY[n]
                if fam not in pend:
                    continue
                if n in LOW8:
                    old = fam not in low
                elif n in HIGH8 or n in W16:
                    old = fam not in word
                else:
                    old = True
                if old:
                    live.add(fam)
            pend, low, word = set(pend), set(low), set(word)
            for n in writes:
                fam = FAMILY[n]
                if n in VOL:
                    pend.discard(fam)
                elif n in W16:
                    low.add(fam)
                    word.add(fam)
                elif n in LOW8:
                    low.add(fam)
                elif fam in low:
                    word.add(fam)
            pend, low, word = frozenset(pend), frozenset(low & pend), frozenset(word & pend)
            succ, tail = self.successors(f, i)
            if tail is not None:
                live |= self.entry_live(tail) & pend
                continue
            for s in succ:
                stack.append((s, pend, low, word))
        return live

    def entry_live(self, f):
        if f in self._entry:
            return self._entry[f]
        if f in self.imports or f not in self.ranges:
            return frozenset()
        self._entry[f] = frozenset({"ecx"})  # recursion guard (a cycle back to f)
        self._entry[f] = frozenset(self.live_from(f, f, VOL, self.pres_stock))
        return self._entry[f]

    # ---- which volatile registers a function never changes --------------------------------
    def preserved_sets(self, clobbered):
        """Greatest fixed point: pres[f] = VOL minus registers f or its direct callees write;
        functions in `clobbered` (hooked) preserve nothing."""
        pres = {f: set(VOL) for f in self.ranges}
        calls = defaultdict(set)
        self.calls = calls          # kept: --demote walks it from a callee to the hooks it reaches
        own = {}
        for f, rgs in self.ranges.items():
            w = set()
            for lo, hi in rgs:
                va = lo
                while va <= hi:
                    i = self.ins.get(va)
                    if i is None:
                        va += 1
                        continue
                    if i.mnemonic in ("call", "jmp"):
                        t = self.direct_target(i)
                        if t is not None and not (i.mnemonic == "jmp" and self.in_func(f, t)):
                            calls[f].add(t)
                        elif i.mnemonic == "call":
                            w |= set(VOL)
                        elif t is None and not re.match(r"dword ptr \[\w+\*4 \+ 0x", i.op_str):
                            w |= set(VOL)  # tail jump through a register or an import
                    else:
                        w |= self.fam_writes(i)
                    va += i.size
            own[f] = w
        for f in pres:
            pres[f] -= own[f]
            if f in clobbered:
                pres[f] = set()
        changed = True
        while changed:
            changed = False
            for f in pres:
                if not pres[f]:
                    continue
                new = set(pres[f])
                for t in calls[f]:
                    new &= pres.get(t, set())
                if new != pres[f]:
                    pres[f] = new
                    changed = True
        return pres

    def pres_stock(self, t):
        return self._stock.get(t, set())


def load_stock_calls():
    """Stock functions our code enters directly (NW_STOCK stubs, nw::StockCall)."""
    out = {}
    for path in source_files():
        text = path.read_text(encoding="utf-8", errors="replace")
        for m in re.finditer(r"NW_STOCK\(\s*(0x[0-9A-Fa-f]{8})", text):
            out[int(m.group(1), 16)] = "NW_STOCK"
        for m in re.finditer(r"StockCall<[^>]*>\(\s*(0x[0-9A-Fa-f]{8})", text):
            out[int(m.group(1), 16)] = "StockCall"
    return out


def main(argv):
    sys.setrecursionlimit(20000)
    ap = argparse.ArgumentParser()
    ap.add_argument("--all", action="store_true")
    ap.add_argument("--ini", type=Path, help="check with the Disable= list of this NWindowDecomp.ini")
    ap.add_argument("--demote", action="store_true",
                    help="append to db/demote.csv every hook a BREAKS call reaches (the callee itself or a hook below it): "
                         "the hook goes back to a stub and the caller's register assumption holds again")
    ap.add_argument("--game-src", action="store_true",
                    help="read the hooks of src/nwindow (once it has been regenerated for the C2 image)")
    args = ap.parse_args(argv)
    if args.game_src:
        SRC_DIRS.append(ROOT / "src" / "nwindow")

    a = Analysis()
    hooks = load_hooks()
    if args.ini:
        off = disabled_by_ini(args.ini, hooks)
        print(f"{args.ini}: {len(off)} hook(s) disabled")
        hooks = {va: h for va, h in hooks.items() if va not in off}
    a._stock = a.preserved_sets(set())
    hooked_pres = a.preserved_sets(set(hooks))
    problems = 0

    # 1. registers read at entry that our declaration does not receive
    for f, (kind, _) in sorted(hooks.items()):
        live = set(a.entry_live(f))
        expected = {"ecx"} if kind in ("METHOD", "METHOD_SIG", "VARARGS", "CTOR", "CTOR_ARGS", "DTOR") else set()
        extra = live - expected
        if extra:
            problems += 1
            print(f"ENTRY  {f:08x} {a.name(f)} reads {sorted(extra)} at entry (register arguments)")

    # 1b. stock functions our code enters: arguments beyond the standard convention are lost
    for f, how in sorted(load_stock_calls().items()):
        extra = set(a.entry_live(f)) - {"ecx"}
        if extra:
            problems += 1
            print(f"ENTRY  {f:08x} {a.name(f)} ({how}) reads {sorted(extra)} at entry (register arguments)")

    # 2. callers relying on preserved registers that a hook no longer preserves
    lost = {t: a._stock[t] - hooked_pres.get(t, set()) for t in a._stock}
    lost = {t: r for t, r in lost.items() if r}
    by_target = defaultdict(list)
    for va, i in a.ins.items():
        if i.mnemonic in ("call", "jmp"):
            t = a.direct_target(i)
            if t in lost and not (i.mnemonic == "jmp" and a.in_func(a.owner[va], t)):
                by_target[t].append(i)
    to_demote = {}
    for t in sorted(by_target):
        for i in by_target[t]:
            f = a.owner[i.address]
            if f in lost and i.mnemonic == "jmp":
                continue  # tail call: the caller's own callers are checked through lost[f]
            live = a.live_from(f, i.address + i.size, lost[t], a.pres_stock) if i.mnemonic == "call" else set()
            if not live:
                continue
            caller_hooked = f in hooks
            if caller_hooked and not args.all:
                continue
            problems += 0 if caller_hooked else 1
            if not caller_hooked and args.demote:
                # the hooks at or below the callee that stopped preserving what the caller relies on
                seen, stack = set(), [t]
                while stack:
                    g = stack.pop()
                    if g in seen:
                        continue
                    seen.add(g)
                    if g in hooks and lost.get(g):
                        to_demote.setdefault(g, "regcheck: a stock caller (%08x) keeps %s across a call that reaches this hook"
                                             % (f, "/".join(sorted(live))))
                    stack.extend(a.calls.get(g, ()))
            via = "" if t in hooks else " (calls a hook)"
            tag = "HOOKED" if caller_hooked else "BREAKS"
            print(f"{tag} {i.address:08x} in {a.name(f)}: after call to {a.name(t)}{via} reads {sorted(live)}")
    print(f"{len(hooks)} hooks checked, {problems} problem(s)")
    if args.demote and to_demote:
        path = ROOT / "db" / "demote.csv"
        have = set()
        if path.exists():
            have = {r["c2_va"] for r in csv.DictReader(path.open(encoding="utf-8"))}
        with path.open("a" if path.exists() else "w", encoding="utf-8", newline="") as fh:
            w = csv.writer(fh)
            if not have and not path.exists():
                w.writerow(["c2_va", "reason"])
            n = 0
            for g, why in sorted(to_demote.items()):
                if f"{g:08x}" not in have:
                    w.writerow([f"{g:08x}", why])
                    n += 1
        print(f"demoted {n} hook(s) into db/demote.csv")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
