#!/usr/bin/env python3
"""Recover the native classes of the image (178 in the C1 build, 225 in the C2 one, 309 in the C3 one) from their
IMPLEMENT_CLASS static initializers.

Every IMPLEMENT_CLASS(NCxxx) leaves a dynamic initializer that constructs
NCxxx::PrivateStaticClass through Core's
  UClass::UClass(ENativeConstructor, Size, ClassFlags, Super, Within, FGuid, Name, Package,
                 Config, Flags, InternalConstructor, StaticConstructor)
A tiny stack emulator reads the pushed arguments at each call site (every one must be recovered:
the FGuid built in place and the Config returned by TClass::StaticConfigName() included). From
InternalConstructor we follow the C++ constructor to the vtable it stores and the base
constructor it calls first; a vtable it stores at another offset (multiple inheritance, FExec)
is a secondary one. tools/verify_standalone.py reuses the emulator on our NWindow.dll.

Writes analysis/classes.json and analysis/classes.md.
"""
from __future__ import annotations

import json
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from nwd.image import ROOT, Image  # noqa: E402

UCLASS_CTOR = "??0UClass@@QAE@W4ENativeConstructor@@KKPAV0@1VFGuid@@PBG33KP6AXPAX@ZP8UObject@@AEXXZ@Z"
# Stack order at the call (esp upwards): first parameter first.
PARAMS = ["ENative", "Size", "ClassFlags", "Super", "Within", "Guid0", "Guid1", "Guid2", "Guid3",
          "Name", "Package", "Config", "Flags", "InternalConstructor", "StaticConstructor"]

REG32 = {"eax", "ebx", "ecx", "edx", "esi", "edi", "ebp"}
MEM = re.compile(r"dword ptr \[(e[a-ds][xip]|e[sd]i)(?: ([+-]) (0x[0-9a-f]+|\d+))?\]")
# instructions that only read their operands
NO_WRITE = {"cmp", "test", "push", "call", "jmp", "ret", "retn", "nop", "int3"}


def func_start(img: Image, va: int) -> int:
    """Start of the function containing va: the 16-aligned address right after int3/nop padding."""
    p = va & ~0xF
    while p > va - 0x400:
        if img.read(p - 1, 1)[0] in (0xCC, 0x90):
            return p
        p -= 16
    raise RuntimeError(f"no function start before {va:#x}")


class Sym:
    """Symbolic value: an int, a string tag (import address, call result, unknown) or
    ("esp", offset) for a register holding a stack address."""

    def __init__(self, v):
        self.v = v

    def __repr__(self):
        return hex(self.v) if isinstance(self.v, int) else str(self.v)


def emulate_to(img: Image, start: int, stop: int):
    """Run the straight-line code [start, stop) and return (regs, stack args at stop).

    Models what an IMPLEMENT_CLASS initializer does with its arguments: push/pop, esp
    arithmetic, stores through esp or a register copied from it (the FGuid built in place),
    xor-zeroing, register and memory moves, and a call leaving "ret:<callee>" in eax. Stack
    space reserved by `sub esp` is "?uninit" until stored, so nothing is assumed. Any other
    instruction makes the register or stack slot it writes unknown."""
    regs: dict[str, Sym] = {}
    stack: dict[int, Sym] = {}  # offset from entry esp -> value
    esp = 0

    def slot(op: str):
        """Stack offset a memory operand addresses, when it is [esp+X] or [reg+X], reg = esp+Y."""
        m = MEM.fullmatch(op.strip())
        if not m:
            return None
        off = int(m.group(3), 0) if m.group(3) else 0
        if m.group(2) == "-":
            off = -off
        if m.group(1) == "esp":
            return esp + off
        r = regs.get(m.group(1))
        if r is not None and isinstance(r.v, tuple) and r.v[0] == "esp":
            return r.v[1] + off
        return None

    def val(op: str) -> Sym:
        op = op.strip()
        if op == "esp":
            return Sym(("esp", esp))
        if op in REG32:
            return regs.get(op, Sym("?" + op))
        m = re.fullmatch(r"(?:dword ptr )?\[(0x[0-9a-f]+)\]", op)
        if m:
            a = int(m.group(1), 16)
            if a in img.imports:
                return Sym("&" + img.imports[a][1])  # an IAT slot holds the import's address
            return Sym(f"[{a:#x}]")
        s = slot(op)
        if s is not None:
            return stack.get(s, Sym("?stack"))
        try:
            return Sym(int(op, 0) & 0xFFFFFFFF)
        except ValueError:
            return Sym("?" + op)

    for ins in img.disasm(start, stop - start + 16, stop_at_ret=False):
        if ins.address >= stop:
            break
        mn, ops = ins.mnemonic, ins.op_str
        a = [x.strip() for x in ops.split(",")] if ops else []
        if mn == "push":
            v = val(a[0])  # before esp moves: push [esp+X] reads the old esp
            esp -= 4
            stack[esp] = v
        elif mn == "pop":
            if a[0] in REG32:
                regs[a[0]] = stack.get(esp, Sym("?"))
            esp += 4
        elif mn == "sub" and a[0] == "esp":
            for _ in range(int(a[1], 0) // 4):
                esp -= 4
                stack[esp] = Sym("?uninit")
        elif mn == "add" and a[0] == "esp":
            esp += int(a[1], 0)
        elif mn == "xor" and len(a) == 2 and a[0] == a[1] and a[0] in REG32:
            regs[a[0]] = Sym(0)
        elif mn == "mov" and a[0] in REG32:
            regs[a[0]] = val(a[1])
        elif mn == "mov" and slot(a[0]) is not None:
            stack[slot(a[0])] = val(a[1])
        elif mn == "lea" and a[0] in REG32 and slot("dword ptr " + a[1]) is not None:
            regs[a[0]] = Sym(("esp", slot("dword ptr " + a[1])))
        elif mn == "call":
            regs["eax"] = Sym("ret:" + (img.import_name_in(ops) or ops))
            for r in ("ecx", "edx"):
                regs[r] = Sym("?" + r)  # volatile across the call
        elif mn not in NO_WRITE and a:
            if a[0] in REG32:
                regs[a[0]] = Sym("?" + mn)
            elif slot(a[0]) is not None:
                stack[slot(a[0])] = Sym("?" + mn)
    args = [stack.get(esp + 4 * k, Sym("?")) for k in range(len(PARAMS))]
    return regs, args


def config_expr(img: Image, v: Sym) -> str:
    """The Config argument as source: TClass::StaticConfigName() resolved to the class whose
    function the initializer calls (an import, or a literal when inlined)."""
    s = str(v)
    m = re.fullmatch(r"ret:\?StaticConfigName@(\w+)@@SAPBGXZ", s)
    if m:
        return f"{m.group(1)}::StaticConfigName()"
    if isinstance(v.v, int) and img.section(v.v) is not None:
        return f'TEXT("{img.wstr(v.v)}")'
    return s


def follow_internal_ctor(img: Image, va: int):
    """InternalConstructor is `if (X) new(X) T;` -> the jmp/call target is T's constructor."""
    # up to the ret: a constructor with arguments (NCEditBox in C2: `new NCEditBox(0, 0, 1, 0, 0)`)
    # pushes them first, so its call is not among the first few instructions
    for ins in img.disasm(va, 0x40, stop_at_ret=True, limit=24):
        if ins.mnemonic in ("jmp", "call") and ins.op_str.startswith("0x"):
            return int(ins.op_str, 16)
    return None


def ctor_facts(img: Image, ctor: int):
    """First direct call (the base constructor) and the first vtable stored into the object."""
    base_ctor = None
    vtable = None
    rlo, rhi = img.sect_range(".rdata")
    for ins in img.disasm(ctor, 0x800, stop_at_ret=True):
        if ins.mnemonic == "call" and ins.op_str.startswith("0x") and base_ctor is None:
            base_ctor = int(ins.op_str, 16)
        m = re.fullmatch(r"dword ptr \[(esi|ecx|eax|edi|ebx|ebp)\], (0x[0-9a-f]+)", ins.op_str)
        if ins.mnemonic == "mov" and m and vtable is None:
            v = int(m.group(2), 16)
            if rlo <= v < rhi:
                vtable = v
    return base_ctor, vtable


def secondary_vptrs(img: Image, ctor: int) -> dict[int, int]:
    """Multiple inheritance: the vtables a constructor stores at offsets other than 0
    (`mov dword ptr [reg + X], imm`), the last store per offset (bases store theirs first)."""
    out = {}
    rlo, rhi = img.sect_range(".rdata")
    for ins in img.disasm(ctor, 0x800, stop_at_ret=True):
        m = re.fullmatch(r"dword ptr \[(esi|ecx|eax|edi|ebx|ebp) \+ (0x[0-9a-f]+)\], (0x[0-9a-f]+)", ins.op_str)
        if ins.mnemonic == "mov" and m:
            v = int(m.group(3), 16)
            if rlo <= v < rhi:
                out[int(m.group(2), 16)] = v
    return out


def direct_calls(img: Image, va: int) -> list[int]:
    """Targets of the direct calls of the function at va, in order, up to its ret."""
    return [int(ins.op_str, 16) for ins in img.disasm(va, 0x800, stop_at_ret=True)
            if ins.mnemonic == "call" and ins.op_str.startswith("0x")]


def base_dtor_vtable(img: Image, child_vtable: int):
    """The vtable a class's own destructor stores, reached from a subclass: the subclass's
    deleting destructor (slot 3) calls its destructor, whose last direct call is the base
    destructor; that one stores the base's vtable first."""
    calls = direct_calls(img, img.u32(child_vtable + 3 * 4))
    if not calls:
        return None
    calls = direct_calls(img, calls[0])
    if not calls:
        return None
    return ctor_facts(img, calls[-1])[1]


def vtable_stores(img: Image) -> set[int]:
    """Every .rdata address stored by `mov dword ptr [reg], imm32` (C7 00/01/02/03/06/07):
    the vptr stores of constructors and destructors, i.e. the start of every vtable."""
    rlo, rhi = img.sect_range(".rdata")
    out = set()
    for r in img.relocs:
        if not img.in_text(r):
            continue
        op = img.read(r - 2, 2)
        if op[0] == 0xC7 and op[1] in (0x00, 0x01, 0x02, 0x03, 0x06, 0x07):
            v = img.u32(r)
            if rlo <= v < rhi:
                out.add(v)
    return out


def vtable_slots(img: Image, vt: int, stop: set[int]):
    """Consecutive relocated pointers into .text starting at vt (up to the next known vtable)."""
    slots = []
    p = vt
    while (p == vt or p not in stop) and p in img.reloc_set:
        t = img.u32(p)
        if not img.in_text(t):
            break
        slots.append(t)
        p += 4
    return slots


def thunk_import(img: Image, va: int):
    """If va is `jmp dword ptr [IAT]`, the name of that import."""
    b = img.read(va, 6)
    if b[:2] == b"\xff\x25":
        slot = int.from_bytes(b[2:6], "little")
        if slot in img.imports:
            return img.imports[slot][1]
    return None


def main() -> int:
    img = Image()
    ctor_slot = img.import_slot(UCLASS_CTOR)
    lo, hi = img.sect_range(".text")
    code = img.read(lo, hi - lo)
    pat = b"\xff\x15" + ctor_slot.to_bytes(4, "little")
    sites = []
    i = code.find(pat)
    while i >= 0:
        sites.append(lo + i)
        i = code.find(pat, i + 1)

    by_this: dict[int, dict] = {}
    for site in sites:
        start = func_start(img, site)
        regs, args = emulate_to(img, start, site)
        this = regs.get("ecx")
        if this is None or not isinstance(this.v, int):
            raise RuntimeError(f"no this at {site:#x}")
        d = dict(zip(PARAMS, args))
        unknown = [p for p in PARAMS if str(d[p]).startswith("?")]
        if unknown:
            raise RuntimeError(f"initializer {start:#x}: arguments not recovered: {unknown}")
        name = img.wstr(d["Name"].v)
        # TEXT(#TClass)+1: the full literal (the C++ class name) starts one character before the name
        literal = img.wstr(d["Name"].v - 2)
        # the C1 build had only N-prefixed classes; the C2 one adds L2WordWrapperWnd and MSNSocket
        cpp = literal if literal and literal[1:] == name else "N" + name
        by_this[this.v] = {
            "name": name,
            "cpp": cpp,
            "name_literal": literal,
            "static_class": this.v,
            "initializer": start,
            "size": d["Size"].v,
            "class_flags": d["ClassFlags"].v,
            "flags": d["Flags"].v,
            "super_raw": repr(d["Super"]),
            "within_raw": repr(d["Within"]),
            "internal_ctor": d["InternalConstructor"].v,
            "static_ctor_raw": repr(d["StaticConstructor"]),
            "package": img.wstr(d["Package"].v) if isinstance(d["Package"].v, int) else None,
            "package_va": d["Package"].v,
            "config_raw": repr(d["Config"]),
            "config": config_expr(img, d["Config"]),
            "guid": [d["Guid0"].v, d["Guid1"].v, d["Guid2"].v, d["Guid3"].v],
        }

    # the autoclass exports: which variable belongs to which class
    for exp, va in img.exports.items():
        if not exp.startswith("autoclass"):
            continue
        hit = [r for r in by_this.values() if r["cpp"] == exp[len("autoclass"):]]
        if len(hit) != 1:
            print("WARN autoclass without initializer:", exp)
            continue
        hit[0]["autoclass_va"] = va
        hit[0]["autoclass_file_value"] = img.u32(va)

    for r in by_this.values():
        for key in ("super", "within"):
            raw = r[key + "_raw"]
            if raw.startswith("0x"):
                v = int(raw, 16)
                r[key] = by_this[v]["cpp"] if v in by_this else raw
            elif raw.startswith("&?PrivateStaticClass@"):
                r[key] = raw[len("&?PrivateStaticClass@"):].split("@@")[0]
            else:
                r[key] = raw

    vtables = vtable_stores(img)
    for r in by_this.values():
        ctor = follow_internal_ctor(img, r["internal_ctor"])
        r["ctor"] = ctor
        r["base_ctor"], r["vtable"] = ctor_facts(img, ctor) if ctor else (None, None)
        if r["vtable"]:
            vtables.add(r["vtable"])
        second = secondary_vptrs(img, ctor) if ctor else {}
        if second:
            r["secondary_vtables"] = [{"offset": off, "vtable": vt} for off, vt in sorted(second.items())]
            vtables.update(second.values())
    # A constructor the linker folded (ICF) serves several classes and stores the vtable of the
    # most basic one (NCObject's, also for NCCmdTarget and NCAbstractMain). The others' own
    # vtable is the one their destructor stores, found through a subclass's destructor.
    by_ctor: dict[int, list] = {}
    for r in by_this.values():
        if r["ctor"]:
            by_ctor.setdefault(r["ctor"], []).append(r)
    for group in by_ctor.values():
        names = {r["cpp"] for r in group}
        for r in group:
            if r.get("super") not in names:
                continue  # the class the folded constructor was written for
            kids = [k for k in by_this.values() if k.get("super") == r["cpp"] and k["vtable"]
                    and len(by_ctor.get(k["ctor"], [])) == 1]
            vt = base_dtor_vtable(img, kids[0]["vtable"]) if kids else None
            if vt:
                r["vtable"] = vt
                r["vtable_from"] = "dtor"
                vtables.add(vt)
            else:
                print("WARN no vtable of its own:", r["cpp"])
    (ROOT / "analysis").mkdir(exist_ok=True)
    (ROOT / "analysis" / "vtable_starts.txt").write_text(
        "".join(f"{v:08x}\n" for v in sorted(vtables)), encoding="utf-8")
    for r in by_this.values():
        if r["vtable"]:
            r["vtable_slots"] = [{"slot": k, "va": t, "import": thunk_import(img, t)}
                                 for k, t in enumerate(vtable_slots(img, r["vtable"], vtables))]
        for sv in r.get("secondary_vtables", []):
            sv["vtable_slots"] = [{"slot": k, "va": t, "import": thunk_import(img, t)}
                                  for k, t in enumerate(vtable_slots(img, sv["vtable"], vtables))]

    cpp = {r["cpp"]: r for r in by_this.values()}

    def chain(r):
        out, s = [], r.get("super")
        while s in cpp:
            out.append(s)
            s = cpp[s].get("super")
        out.append(s)
        return out

    for r in by_this.values():
        c = chain(r)
        r["depth"] = len(c) - 1
        r["engine_base"] = c[-1]
        r["ancestors"] = c
    classes = sorted(by_this.values(), key=lambda r: (r["depth"], r["cpp"]))

    out = ROOT / "analysis"
    out.mkdir(exist_ok=True)
    (out / "classes.json").write_text(json.dumps(classes, indent=1), encoding="utf-8")

    def common(key):
        vals = {json.dumps(r[key]) for r in classes}
        return json.loads(vals.pop()) if len(vals) == 1 else "(varia)"

    lines = ["# Classes nativas do NWindow.dll (IMPLEMENT_CLASS)", "",
             f"Total: **{len(classes)}** — gerado por `tools/extract_classes.py` a partir dos",
             "inicializadores estáticos (`UClass::UClass(ENativeConstructor, ...)`).", "",
             "Argumentos iguais em todas (lidos de cada inicializador, não supostos): "
             f"pacote `{common('package')}`, Within `{common('within')}`, GUID `{common('guid')}`, "
             f"config `{common('config')}`, flags de objeto `{common('flags'):#x}`.", "",
             "| Classe C++ | Super | sizeof | ClassFlags | vtable | slots | ctor |",
             "|---|---|---:|---:|---|---:|---|"]
    for r in sorted(classes, key=lambda r: r["cpp"].lower()):
        lines.append("| %s | %s | 0x%X | 0x%X | %s | %s | %s |" % (
            r["cpp"], r["super"], r["size"], r["class_flags"],
            f"{r['vtable']:#010x}" if r["vtable"] else "-",
            len(r.get("vtable_slots", [])),
            f"{r['ctor']:#010x}" if r["ctor"] else "-"))
    (out / "classes.md").write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"{len(classes)} classes; engine bases: {sorted({r['engine_base'] for r in classes})}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
