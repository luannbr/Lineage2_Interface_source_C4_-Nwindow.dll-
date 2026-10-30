import os
# Compares the direct call targets (as stock VAs) of every hooked function of the lane: stock vs ours
import re, sys, csv, struct, bisect
sys.path.insert(0, str(__import__('pathlib').Path(__file__).resolve().parents[3]))   # tools
import pefile
from capstone import Cs, CS_ARCH_X86, CS_MODE_32
import verify_hooks as vh
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
LOG = ROOT / 'build/smoke_system/NWindowDecomp.log'
OURS = ROOT / 'build/bin/Release/NWindowDecomp.dll'
CLASSES = sys.argv[1].split(',') if len(sys.argv) > 1 else ['NCChatWnd', 'NCChatFilterWnd', 'NCConsole', 'NCEditBox', 'NCChineseIME', 'NCCandidateBox']
md = Cs(CS_ARCH_X86, CS_MODE_32)
st = pefile.PE(str(ROOT / 'orig/NWindow.dll'))
sbase = st.OPTIONAL_HEADER.ImageBase
smem = st.get_memory_mapped_image()
op = pefile.PE(str(OURS))
obase = op.OPTIONAL_HEADER.ImageBase
omem = op.get_memory_mapped_image()
funcs = {}
for r in csv.DictReader((ROOT / 'db/functions.csv').open(encoding='utf-8')):
    funcs[int(r['va'], 16)] = int(r['size'])
byname = {}
for r in csv.DictReader((ROOT / 'db/functions.csv').open(encoding='utf-8')):
    byname.setdefault(r['name'], []).append(int(r['va'], 16))
import re as _re


def demangle(sym):
    m = _re.match(r'^\?\?([01])([A-Za-z0-9_]+)@@', sym)
    if m:
        c = m.group(2)
        return c + '::' + ('~' if m.group(1) == '1' else '') + c
    m = _re.match(r'^\?([A-Za-z0-9_]+)@([A-Za-z0-9_]+)@@', sym)
    if m:
        return m.group(2) + '::' + m.group(1)
    return None


hooks = {}   # stock va -> (name, ours va)
rev = {}
for ln in open(LOG, encoding='latin-1'):
    m = re.match(r'^\S+\s+([0-9a-f]{8}) (.*?)\s+ours=([0-9a-f]+) ok', ln)
    if m:
        va, nm, o = int(m.group(1), 16), m.group(2).strip(), 0x10000000 + int(m.group(3), 16)
        hooks[va] = (nm, o)
syms = vh.our_symbols(OURS)
starts = [a for a, _ in syms]
label = dict(syms)


def follow(a):
    seen = 0
    while seen < 4:
        b = bytes(omem[a - obase:a - obase + 12])
        if len(b) < 9:
            break
        if b[0] == 0xE9:
            a = (a + 5 + struct.unpack('<i', b[1:5])[0]) & 0xFFFFFFFF
        elif b[:4] == bytes([0x55, 0x8B, 0xEC, 0x5D]) and b[4] == 0xE9:
            a = (a + 9 + struct.unpack('<i', b[5:9])[0]) & 0xFFFFFFFF
        else:
            break
        seen += 1
    return a


for va, (nm, o) in hooks.items():
    rev[o] = va
    rev[follow(o)] = va


def stock_calls(va):
    size = funcs.get(va, 0)
    out = set()
    if not size:
        return None
    for i in md.disasm(bytes(smem[va - sbase:va + size - sbase]), va):
        if i.mnemonic in ('call', 'jmp') and i.op_str.startswith('0x'):
            t = int(i.op_str, 16)
            if va <= t < va + size:
                continue
            out.add(t)
    return out


def resolve_ours(t):
    t = follow(t)
    if t in rev:
        return rev[t]
    b = bytes(omem[t - obase:t - obase + 16])
    ins = list(md.disasm(b, t))[:3]
    if len(ins) == 3 and ins[0].mnemonic == 'mov' and ins[1].mnemonic == 'add' and ins[2].mnemonic in ('jmp', 'ret') and (ins[2].op_str == 'eax' or ins[2].mnemonic == 'ret'):
        return 0x10000000 + int(ins[1].op_str.split(',')[1].strip(), 16)
    return None


def ours_calls(o):
    a = follow(o)
    i = bisect.bisect_right(starts, a) - 1
    start = starts[i]
    end = starts[i + 1] if i + 1 < len(starts) else start + 0x4000
    out = set()
    unknown = set()
    for ins in md.disasm(bytes(omem[start - obase:end - obase]), start):
        if ins.mnemonic in ('call', 'jmp') and ins.op_str.startswith('0x'):
            t = int(ins.op_str, 16)
            if start <= t < end:
                continue
            r = resolve_ours(t)
            if r is not None:
                out.add(r)
            else:
                lab = label.get(follow(t), hex(t))
                dm = demangle(lab)
                if dm and dm in byname and len(byname[dm]) == 1:
                    out.add(byname[dm][0])
                else:
                    unknown.add(lab[:50])
    return out, unknown


bad = 0
for va, (nm, o) in sorted(hooks.items()):
    cls = nm.split('::')[0]
    if cls not in CLASSES:
        continue
    sc = stock_calls(va)
    if sc is None:
        continue
    sc = {t for t in sc if t < 0x10170000 or t >= 0x10180000}   # the CRT
    oc, unk = ours_calls(o)
    sonly = sorted(sc - oc)
    oonly = sorted(oc - sc)
    if sonly or oonly:
        bad += 1
        print('%08x %-45s stock-only %s ours-only %s' % (va, nm[:45], [hex(x) for x in sonly], [hex(x) for x in oonly]))
        if unk:
            print('         ours internals:', sorted(unk)[:6])
print('functions with differences:', bad)
