# Disassemble OUR compiled copy of a hooked function (from build/bin/Release/NWindowDecomp.dll, found through the smoke log) with the call
# targets mapped back to stock vas / our symbol names.   python dis_ours.py <stock va hex> [max instructions]
import sys, os, re, bisect, struct
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import tr
from tr import *
import pefile

va = int(sys.argv[1], 16)
mx = int(sys.argv[2]) if len(sys.argv) > 2 else 400
lm = log_map()
pe = pefile.PE(OURS)
base = pe.OPTIONAL_HEADER.ImageBase
mem = pe.get_memory_mapped_image()
syms = {}
for ln in open(OURS_MAP, encoding='latin-1'):
    m = re.match(r'\s*[0-9a-f]{4}:[0-9a-f]{8}\s+(\S+)\s+([0-9a-f]{8})\s', ln)
    if m:
        syms.setdefault(int(m.group(2), 16), m.group(1))
starts = sorted(syms)
rev = {v: k for k, v in lm.items()}
a = lm[va]
i = bisect.bisect_right(starts, a) - 1
lo = starts[i]
hi = starts[i + 1] if i + 1 < len(starts) else lo + 0x1000
print('; ours %08x (%s) %d bytes, stock %08x' % (lo, syms.get(lo, '?')[:60], hi - lo, va))
md = Cs(CS_ARCH_X86, CS_MODE_32)
code = bytes(mem[lo - base:hi - base])
n = 0
for ins in md.disasm(code, lo):
    note = ''
    m = re.match(r'^(?:call|jmp) (0x[0-9a-f]+)$', '%s %s' % (ins.mnemonic, ins.op_str))
    if m:
        t = int(m.group(1), 16)
        if t in rev:
            note = '  ; stock %08x %s' % (rev[t], tr.functions().get(rev[t], (0, '?', ''))[1])
        elif t in syms:
            note = '  ; ' + syms[t][:70]
    print('  %08x  %-6s %s%s' % (ins.address, ins.mnemonic, ins.op_str, note))
    n += 1
    if n >= mx:
        break
