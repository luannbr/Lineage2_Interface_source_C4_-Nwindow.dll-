#!/usr/bin/env python3
"""Hooked functions that can reach a function whose body is `NW_UNPORTED()` (an int3).

A copied or ported body that calls `Class::Method()` calls OUR definition of it; when that definition is
an NW_UNPORTED stub (no C2 address was known, or the function is not ported) the process breaks the first
time the call runs. The cycle cannot see it (it never runs the hooks). This walks the compiled
NWindowDecomp.dll: every function that starts with int3 is a breaker, every `call rel32` / `jmp rel32`
inside a hooked function (and, transitively, inside the non-hooked functions it calls in our module) that
lands on a breaker is reported with the path.

usage: python tools/port/check_breakers.py [--log build/smoke_system/NWindowDecomp.log]"""
from __future__ import annotations

import argparse
import bisect
import re
import sys
from pathlib import Path

import pefile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import verify_hooks as vh  # noqa: E402


def ops_imm(ins):
    from capstone import CS_OP_IMM
    return ins.operands[0].imm if ins.operands and ins.operands[0].type == CS_OP_IMM else None


def reachable_int3(md, mem, base, f, end, noret=frozenset(), hard=frozenset()):
    """Offset of an int3 that execution can reach inside [f, end), or None. A recursive descent from the entry
    (branches, and the entries of a `jmp [reg*4 + table]` switch table that lie inside the function): the bytes
    of a jump table and the padding between functions are never decoded, so an address byte of 0xCC is not an
    int3 (NCMultiEdit::OnKeyDown was a false positive of the linear sweep). A call of a library function (`noret`:
    the CRT's __report_rangecheckfailure, _invalid_parameter_noinfo_noreturn, the Xlength / Xran throwers) that is
    followed by an int3 does not return: the int3 is the compiler's, not NW_UNPORTED()'s."""
    from capstone import CS_OP_IMM, CS_OP_MEM
    seen = set()
    work = [f]
    while work:
        a = work.pop()
        while f <= a < end and a not in seen:
            seen.add(a)
            ins = next(md.disasm(bytes(mem[a - base:a - base + 16]), a), None)
            if ins is None:
                break
            if ins.mnemonic == "int3":
                return a - f
            nxt = ins.address + ins.size
            if ins.mnemonic == "call" and ops_imm(ins) in noret and mem[nxt - base] == 0xCC:
                break
            # the CRT's known no-return reporters stop the path wherever the compiler put the call (this build
            # put the exception tables right behind __report_rangecheckfailure, not an int3)
            if ins.mnemonic == "call" and ops_imm(ins) in hard:
                break
            if ins.mnemonic in ("ret", "retn"):
                break
            ops = ins.operands
            if ins.mnemonic == "jmp":
                if ops and ops[0].type == CS_OP_IMM:
                    work.append(ops[0].imm)
                elif ops and ops[0].type == CS_OP_MEM and ops[0].mem.index and ops[0].mem.disp:
                    t = ops[0].mem.disp & 0xFFFFFFFF
                    k = 0
                    while f <= t - 0 and t + 4 * k + 4 - base <= len(mem):
                        v = int.from_bytes(bytes(mem[t + 4 * k - base:t + 4 * k + 4 - base]), "little")
                        if not (f <= v < end):
                            break
                        work.append(v)
                        k += 1
                break
            if ins.mnemonic.startswith("j") and ops and ops[0].type == CS_OP_IMM:
                work.append(ops[0].imm)
            a = nxt
    return None


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--log", type=Path, default=ROOT / "build" / "smoke_system" / "NWindowDecomp.log")
    ap.add_argument("--dll", type=Path, default=ROOT / "build" / "bin" / "Release" / "NWindowDecomp.dll")
    ap.add_argument("--verbose", action="store_true", help="also list the breaker slots of classes whose constructor is not hooked")
    ap.add_argument("--demote", action="store_true",
                    help="append every hooked function that reaches a breaker to db/demote.csv (it becomes a stub)")
    a = ap.parse_args()
    pe = pefile.PE(str(a.dll))
    base = pe.OPTIONAL_HEADER.ImageBase
    mem = pe.get_memory_mapped_image()
    text = next(s for s in pe.sections if s.Name.startswith(b".text"))
    lo, hi = base + text.VirtualAddress, base + text.VirtualAddress + text.Misc_VirtualSize
    symbols = vh.our_symbols(a.dll)
    starts = [x for x, _ in symbols]
    label = dict(symbols)

    def fn_of(va):
        i = bisect.bisect_right(starts, va) - 1
        return starts[i] if i >= 0 else None

    def body(f):
        i = starts.index(f)
        end = starts[i + 1] if i + 1 < len(starts) else hi
        return f, end

    breakers = {f for f in starts if lo <= f < hi and mem[f - base] == 0xCC and mem[f - base + 1] in (0xCC, 0x90, 0xC3, 0xCC)
                and bytes(mem[f - base:f - base + 1]) == b"\xcc"}
    # a breaker is a function that BEGINS with int3 followed by padding or ret (the whole body is the break)
    real = set()
    for f in breakers:
        _, end = body(f)
        b = bytes(mem[f - base:end - base])
        if b[:1] == b"\xcc" and all(c in (0xCC, 0x90, 0xC3) for c in b[:16]):
            real.add(f)
    # the other kind: an NW_UNPORTED() in a constructor / destructor sits behind the prologue and the member
    # construction (the first game run died in NToolTipInfo::NToolTipInfo that way). Our own functions only: the
    # CRT has int3 padding inside some bodies.
    INT3 = bytes([0xCC])
    libva = set()
    hardva = set()
    NORETURN = {"___report_rangecheckfailure", "___report_securityfailure", "___report_gsfailure",
                "__invalid_parameter_noinfo_noreturn", "_abort", "__invoke_watson", "_exit", "__amsg_exit",
                "?_Xlength_error@std@@YAXPBD@Z", "?_Xout_of_range@std@@YAXPBD@Z", "?_Xbad_alloc@std@@YAXXZ"}
    mp = a.dll.with_suffix(".map")
    if mp.exists():
        for ln in mp.read_text(encoding="latin-1").splitlines():
            m = re.match(r"^\s*0001:[0-9a-f]{8}\s+(\S+)\s+([0-9a-f]{8})\s+f(?:\s+i)?\s+(\S+)", ln)
            if m and ":" in m.group(3):
                libva.add(int(m.group(2), 16))
                if m.group(1) in NORETURN:
                    hardva.add(int(m.group(2), 16))
    from capstone import CS_ARCH_X86, CS_MODE_32, Cs
    md = Cs(CS_ARCH_X86, CS_MODE_32)
    md.detail = True
    # Functions of ours that never return (the length_error thrower ends in `for (;;)`, its callers just `call` it and
    # the compiler pads with int3): a call to one ends the path, the int3 behind it is the compiler's, not
    # NW_UNPORTED()'s. A function with a reachable int3 of its own is never "no-return" (that is a breaker).
    from capstone import CS_OP_IMM

    def can_return(f, end, stop):
        seen = set()
        work = [f]
        while work:
            a = work.pop()
            while f <= a < end and a not in seen:
                seen.add(a)
                ins = next(md.disasm(bytes(mem[a - base:a - base + 16]), a), None)
                if ins is None or ins.mnemonic in ("ret", "retn"):
                    return True
                if ins.mnemonic == "int3":
                    break
                nxt = ins.address + ins.size
                if ins.mnemonic == "call" and ops_imm(ins) in stop:
                    break
                if ins.mnemonic == "jmp":
                    t = ops_imm(ins)
                    if t is None or (t not in stop and not (f <= t < end)):
                        return True                       # a switch or a tail call: assume it returns
                    if t not in stop:
                        work.append(t)
                    break
                if ins.mnemonic.startswith("j") and ins.operands and ins.operands[0].type == CS_OP_IMM:
                    work.append(ins.operands[0].imm)
                a = nxt
        return False

    # our `NW_NAKED ... NW_STOCK(va)` forwarders (`mov eax,[base]; add eax,RVA; jmp eax`) of stock functions that throw:
    # <xstring>'s _Xlen (C4 0x10170BC9; C3 0x1014C20B), _Xran (C4 0x10170B71; C3 0x1014C1B3) and _CxxThrowException (C4
    # 0x10171169; C3 0x1014C6E2)
    STOCK_NORETURN_RVA = {0x170BC9, 0x170B71, 0x171169}
    never = set()
    for i, f in enumerate(starts):
        if not (lo <= f < hi) or f in libva or f in real:
            continue
        end = starts[i + 1] if i + 1 < len(starts) else hi
        ins3 = list(md.disasm(bytes(mem[f - base:min(f - base + 16, end - base)]), f))[:3]
        if (len(ins3) == 3 and ins3[0].mnemonic == "mov" and ins3[1].mnemonic == "add" and ins3[2].mnemonic == "jmp"
                and ins3[2].op_str == "eax" and ins3[1].operands[1].type == CS_OP_IMM
                and ins3[1].operands[1].imm in STOCK_NORETURN_RVA):
            never.add(f)
    hardva |= never
    for _ in range(4):
        grew = False
        for i, f in enumerate(starts):
            if not (lo <= f < hi) or f in libva or f in real or f in never:
                continue
            end = starts[i + 1] if i + 1 < len(starts) else hi
            if not can_return(f, end, hardva | never) and reachable_int3(md, mem, base, f, end, libva, hardva | never) is None:
                never.add(f)
                grew = True
        if not grew:
            break
    hardva |= never

    inner = {}
    for i, f in enumerate(starts):
        if not (lo <= f < hi) or f in libva or f in real:
            continue
        end = starts[i + 1] if i + 1 < len(starts) else hi
        code = bytes(mem[f - base:end - base]).rstrip(INT3)
        if INT3 not in code:
            continue
        off = reachable_int3(md, mem, base, f, end, libva, hardva)
        if off is not None:
            inner[f] = off
    real |= set(inner)
    hooked = {}
    stock_va = {}
    for line in a.log.read_text(encoding="latin-1").splitlines():
        m = vh.LOG_RE.match(line)
        if m:
            hooked[base + int(m.group(3), 16)] = m.group(2)
            stock_va[m.group(2)] = int(m.group(1), 16)

    def callees(f):
        f0, end = body(f)
        data = bytes(mem[f0 - base:end - base])
        out = set()
        for m in re.finditer(rb"[\xe8\xe9](....)", data, re.S):
            rel = int.from_bytes(m.group(1), "little", signed=True)
            t = f0 + m.start() + 5 + rel
            if lo <= t < hi and t in starts:
                out.add(t)
        return out

    bad = []
    for h, name in sorted(hooked.items()):
        if h in inner:
            bad.append((name, [label.get(h, "%x" % h), "(int3 at +%x of the function itself)" % inner[h]]))
        seen, stack = {h}, [(h, [h])]
        while stack:
            f, path = stack.pop()
            for t in callees(f):
                if t in real:
                    bad.append((name, [label.get(x, "%x" % x) for x in path + [t]]))
                elif t not in seen and t not in hooked and len(path) < 10:
                    seen.add(t)
                    stack.append((t, path + [t]))
    # a breaker that sits in a vtable of ours: an object our compiled code constructs (a hooked constructor leaves
    # our vtable on it) calls it from stock code through the slot. Only a class with a hooked constructor counts.
    vt = []
    if mp.exists():
        for ln in mp.read_text(encoding="latin-1").splitlines():
            m = re.match(r"^\s*000[1-9]:[0-9a-f]{8}\s+(\?\?_7\S+)\s+([0-9a-f]{8})\s", ln)
            if m:
                vt.append((int(m.group(2), 16), m.group(1)))
    vt.sort()
    vstarts = [x for x, _ in vt]
    ctor_classes = {n.split("::")[0] for n in hooked.values() if "::" in n and n.split("::")[0] == n.split("::")[1].split("(")[0]}
    in_vtable = []
    for sec in pe.sections:
        if sec.Name.startswith(b".text"):
            continue
        blob = sec.get_data()
        for off in range(0, len(blob) - 3, 4):
            v = int.from_bytes(blob[off:off + 4], "little")
            if v in real:
                at = base + sec.VirtualAddress + off
                j = bisect.bisect_right(vstarts, at) - 1
                if j >= 0 and at - vstarts[j] < 0x800:
                    cls = vt[j][1][4:].split("@")[0]
                    in_vtable.append((cls, label.get(v, "%x" % v), (at - vstarts[j]) // 4, cls in ctor_classes))
    live = [x for x in in_vtable if x[3]]
    for cls, fn, slot, ctor in in_vtable:
        if not ctor and a.verbose:
            print("  (not live: its constructor is not hooked) vtable of %s slot %d -> %s" % (cls, slot, fn))
    print("%d breaker slot(s) in vtables, %d of classes whose constructor is hooked" % (len(in_vtable), len(live)))
    for cls, fn, slot, _ in live[:60]:
        print("  vtable of %s slot %d -> %s" % (cls, slot, fn))
        bad.append((cls + "::<vtable>", [cls, fn]))
    print("%d breakers (%d with the int3 behind a prologue) in %d functions; %d hooked functions reach one" % (
        len(real), len(inner), len(starts), len({b[0] for b in bad})))
    for name, path in bad[:200]:
        print("  %s -> %s" % (name, " -> ".join(path[1:])[:200]))
    if a.demote and bad:
        import csv
        dp = ROOT / "db" / "demote.csv"
        have = {r["c2_va"] for r in csv.DictReader(dp.open(encoding="utf-8"))} if dp.exists() else set()
        new = sorted({stock_va[b[0]] for b in bad if b[0] in stock_va}
                     - {int(h, 16) for h in have if len(h) == 8 and all(c in "0123456789abcdefABCDEF" for c in h)})
        with dp.open("a" if dp.exists() else "w", encoding="utf-8", newline="") as fh:
            w = csv.writer(fh)
            if not have and not dp.exists():
                w.writerow(["c2_va", "reason"])
            for v in new:
                w.writerow(["%08x" % v, "check_breakers: reaches a function whose body is NW_UNPORTED()"])
        print("demoted %d hook(s) into db/demote.csv" % len(new))
    return 1 if bad else 0


if __name__ == "__main__":
    raise SystemExit(main())
