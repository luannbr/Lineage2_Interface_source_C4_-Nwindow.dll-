# Disassembles OUR compiled copy of a hooked function, given the stock VA (the address comes from the smoke log of the last
# cycle; the end of the function is the next symbol of the map).   python ours_dis.py 100c5e70
import sys, re, struct, bisect
from pathlib import Path
import pefile
from capstone import Cs, CS_ARCH_X86, CS_MODE_32

ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(ROOT / 'tools'))
import verify_hooks as vh

LOG = ROOT / 'build/smoke_system/NWindowDecomp.log'
OURS = ROOT / 'build/bin/Release/NWindowDecomp.dll'

op = pefile.PE(str(OURS))
obase = op.OPTIONAL_HEADER.ImageBase
omem = op.get_memory_mapped_image()
syms = vh.our_symbols(OURS)
starts = [a for a, _ in syms]
label = dict(syms)
md = Cs(CS_ARCH_X86, CS_MODE_32)


def ours_addr(va):
    for ln in open(LOG, encoding='latin-1'):
        m = re.match(r'^\S+\s+([0-9a-f]{8}) (.*?)\s+ours=([0-9a-f]+) ok', ln)
        if m and int(m.group(1), 16) == va:
            return 0x10000000 + int(m.group(3), 16)
    return None


def dis(a):
    i = bisect.bisect_right(starts, a)
    end = starts[i] if i < len(starts) else a + 0x1000
    code = bytes(omem[a - obase:end - obase])
    for ins in md.disasm(code, a):
        tgt = ''
        if ins.mnemonic in ('call', 'jmp') and ins.op_str.startswith('0x'):
            t = int(ins.op_str, 16)
            tgt = '  ; ' + str(label.get(t, ''))
        print('  %08x  %-8s %s%s' % (ins.address, ins.mnemonic, ins.op_str, tgt))


if __name__ == '__main__':
    for s in sys.argv[1:]:
        va = int(s, 16)
        a = ours_addr(va)
        print('===== stock %08x -> ours %s' % (va, hex(a) if a else None))
        if a:
            # follow a one-jump thunk
            for _ in range(3):
                b = bytes(omem[a - obase:a - obase + 9])
                if b[0] == 0xE9:
                    a = (a + 5 + struct.unpack('<i', b[1:5])[0]) & 0xFFFFFFFF
                elif b[:4] == bytes([0x55, 0x8B, 0xEC, 0x5D]) and b[4] == 0xE9:
                    a = (a + 9 + struct.unpack('<i', b[5:9])[0]) & 0xFFFFFFFF
                else:
                    break
                print('  (thunk -> %08x %s)' % (a, label.get(a)))
            dis(a)
