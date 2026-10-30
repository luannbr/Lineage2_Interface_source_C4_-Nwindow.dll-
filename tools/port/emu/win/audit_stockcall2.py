import os
# For every StockCall(At)<>(va, this, args...) of the given files: the target's expectations of ecx and of the stack
import re, sys, csv, glob, os
import pefile
from capstone import Cs, CS_ARCH_X86, CS_MODE_32
from capstone.x86_const import *
ROOT = str(__import__('pathlib').Path(__file__).resolve().parents[4])
st = pefile.PE(os.path.join(ROOT, 'orig', 'NWindow.dll')); base = st.OPTIONAL_HEADER.ImageBase; mem = st.get_memory_mapped_image()
md = Cs(CS_ARCH_X86, CS_MODE_32); md.detail = True
funcs = {int(r['va'], 16): int(r['size']) for r in csv.DictReader(open(ROOT + r'\db\functions.csv', encoding='utf-8'))}
files = [a for a in sys.argv[1:] if not a.startswith('--')]


def split_args(s):
    out, depth, cur = [], 0, ''
    for ch in s:
        if ch in '([{':
            depth += 1
        if ch in ')]}':
            depth -= 1
        if ch == ',' and depth == 0:
            out.append(cur.strip()); cur = ''
        else:
            cur += ch
    if cur.strip():
        out.append(cur.strip())
    return out


def analyse(va):
    """(uses_ecx_before_def, max_stack_arg_index_read, ret_pops) on the straight-line start of the function"""
    size = funcs.get(va, 0x60)
    uses_ecx = False
    max_arg = 0
    pops = set()
    ecx_defined = False
    n = 0
    for i in md.disasm(bytes(mem[va - base:va - base + size]), va):
        n += 1
        if i.mnemonic == 'ret':
            pops.add(int(i.op_str, 16) if i.op_str else 0)
        if n <= 40:
            ops = i.operands
            # reads of ecx before it is written
            for o in ops:
                if o.type == X86_OP_REG and o.reg in (X86_REG_ECX, X86_REG_CL, X86_REG_CX):
                    pass
            txt = i.op_str
            if not ecx_defined and re.search(r'\becx\b|\bcl\b', txt):
                if i.mnemonic in ('xor', 'mov', 'lea', 'pop') and txt.startswith(('ecx,', 'cl,')) and not re.search(r'\[.*ecx', txt):
                    ecx_defined = True
                else:
                    uses_ecx = True
            m = re.search(r'esp \+ (0x[0-9a-f]+|\d+)\]', txt)
            if m and (n <= 8):
                off = int(m.group(1), 0)
                if off >= 4 and off % 4 == 0:
                    max_arg = max(max_arg, off // 4)
    return uses_ecx, max_arg, pops


seen = set()
for f in files:
    txt = open(f, encoding='utf-8', errors='replace').read()
    for m in re.finditer(r'StockCall(?:At)?<([^>]*)>\(\s*(0x[0-9A-Fa-f]{8})\s*,([^;]*?)\)\s*;', txt):
        va = int(m.group(2), 16)
        args = split_args(m.group(3).replace('->', '.'))
        n = len(args) - 1
        line = txt.count('\n', 0, m.start()) + 1
        if (va, n) in seen:
            continue
        seen.add((va, n))
        ue, ma, pops = analyse(va)
        note = ''
        if not ue and ma >= 1 and n == 0:
            note = 'SUSPECT: reads [esp+%d] at entry, never ecx, called with this only' % (4 * ma)
        elif not ue and ma >= 1 and (ma > n):
            note = 'SUSPECT: reads arg %d, called with %d args' % (ma, n)
        elif not ue and ma == 0 and n >= 0:
            note = '?  ecx not read early'
        print('%s:%d %08x args=%d ecx=%s stack<=%d ret=%s %s' % (os.path.basename(f), line, va, n, ue, ma, sorted(pops), note))
