# Lists what a stock function (or a range) of the C4 NWindow.dll calls: direct call targets (with the callee's name from
# db/functions.csv and its `ret N`), IAT imports (name), virtual calls (register, slot) and the global addresses it reads/writes.
# The proof1 tests use it to decide which callees to stub.   python fncalls.py 100c5e70 [more VAs]
import sys, csv, struct, re
from pathlib import Path
import pefile
from capstone import Cs, CS_ARCH_X86, CS_MODE_32
from capstone.x86 import X86_OP_MEM, X86_OP_IMM

ROOT = Path(__file__).resolve().parents[4]
st = pefile.PE(str(ROOT / 'orig/NWindow.dll'))
BASE = st.OPTIONAL_HEADER.ImageBase
MEM = st.get_memory_mapped_image()
md = Cs(CS_ARCH_X86, CS_MODE_32)
md.detail = True
FUNCS = {}
NAMES = {}
for r in csv.DictReader((ROOT / 'db/functions.csv').open(encoding='utf-8')):
    va = int(r['va'], 16)
    FUNCS[va] = int(r['size'])
    NAMES[va] = r['name']
IMPORTS = {}
for e in st.DIRECTORY_ENTRY_IMPORT:
    for imp in e.imports:
        IMPORTS[imp.address] = imp.name.decode() if imp.name else 'ord%d' % imp.ordinal


_STARTS = sorted(FUNCS)


def _last_mnemonic(a, b):
    """the mnemonic of the last instruction decoded in [a, b) (capstone skips the bytes it cannot decode)"""
    code = bytes(MEM[a - BASE:b - BASE])
    off = 0
    last = None
    while off < len(code):
        n = 0
        for i in md.disasm(code[off:], a + off):
            last = i.mnemonic
            n = i.address - (a + off) + i.size
        off += n if n else 1
    return last


def real_end(va):
    """the end of the code of the function at va. The db `size` can stop before the end:
      - exception-handling epilogues in a short gap before the next named function (NCEditBox::InsertText 0x1000e540 has its `ret 8` at
        0x1000e780; NCVideoOptionWnd::InitItems 0x100c90c0 its last `call LoadSettings` at 0x100c951e);
      - a function whose last instruction is a `call` (the noreturn __CxxThrowException of an EH path) goes on in the `sub_` pieces of the
        db that follow it (NCWnd::SetWindowSize 0x10036dc0, MoveChildWindow 0x100351d0: their `ret 8` is in a piece);
    a `sub_` that follows a `ret` is another function (the 2nd overload after NCComboBox::AddItemID 0x10006800)"""
    import bisect
    end = va + FUNCS[va]
    start = va
    for _ in range(6):
        k = bisect.bisect_left(_STARTS, end)
        if k >= len(_STARTS) or _STARTS[k] - end > 0x80:
            break
        nxt = _STARTS[k]
        if NAMES.get(nxt, '').startswith('sub_'):
            if nxt - end <= 2 and _last_mnemonic(start, nxt) == 'call':
                start = nxt
                end = nxt + FUNCS[nxt]
                continue
            break
        # a short gap of code before the next NAMED function: up to its first `ret` (what follows can be jump tables)
        if nxt - end <= 0x40 and any(x not in (0xCC, 0x90, 0x00) for x in bytes(MEM[end - BASE:nxt - BASE])):
            for i in md.disasm(bytes(MEM[end - BASE:nxt - BASE]), end):
                if i.mnemonic == 'ret':
                    end = i.address + i.size
                    break
        break
    return end


def ret_pop(va):
    """the `ret N` of the function at va (all of them)"""
    size = FUNCS.get(va)
    if not size:
        return None
    end = real_end(va)
    pops = set()
    code = bytes(MEM[va - BASE:end - BASE])
    off = 0
    while off < len(code):
        last = off
        for i in md.disasm(code[off:], va + off):
            last = i.address - va + i.size
            if i.mnemonic == "ret":
                pops.add(int(i.op_str, 16) if i.op_str else 0)
        off = last + 1 if last < len(code) else len(code)      # capstone stopped on bytes it cannot decode: skip one and go on
    return sorted(pops)


def scan(va, size=None):
    size = size or (real_end(va) - va)
    code = bytes(MEM[va - BASE:va + size - BASE])
    direct = {}
    imports = {}
    vcalls = {}
    for i in md.disasm(code, va):
        if i.mnemonic == 'jmp' and i.operands and i.operands[0].type == X86_OP_IMM and not (va <= i.operands[0].imm < va + size):
            # a tail jump out of the function is a call that never returns here
            direct.setdefault(i.operands[0].imm, 0)
            direct[i.operands[0].imm] += 1
        if i.mnemonic == 'call':
            op = i.operands[0]
            if op.type == X86_OP_IMM:
                direct.setdefault(op.imm, 0)
                direct[op.imm] += 1
            elif op.type == X86_OP_MEM:
                if op.mem.base == 0 and op.mem.index == 0 and op.mem.disp in IMPORTS:
                    imports[IMPORTS[op.mem.disp]] = imports.get(IMPORTS[op.mem.disp], 0) + 1
                elif op.mem.base != 0 and op.mem.index == 0:
                    key = (i.reg_name(op.mem.base), op.mem.disp // 4, op.mem.disp)
                    vcalls[key] = vcalls.get(key, 0) + 1
        elif i.mnemonic == 'mov' or True:
            pass
    return direct, imports, vcalls


if __name__ == '__main__':
    for a in sys.argv[1:]:
        va = int(a, 16)
        direct, imports, vcalls = scan(va)
        print('===== %08x %s (%d bytes)' % (va, NAMES.get(va), FUNCS[va]))
        for t, n in sorted(direct.items()):
            print('  call %08x x%d  %-45s ret %s size %s' % (t, n, NAMES.get(t, '?'), ret_pop(t), FUNCS.get(t)))
        for nm, n in sorted(imports.items()):
            print('  import x%d %s' % (n, nm))
        for (r, slot, disp), n in sorted(vcalls.items()):
            print('  vcall x%d [%s+0x%x] slot %d' % (n, r, disp, slot))
