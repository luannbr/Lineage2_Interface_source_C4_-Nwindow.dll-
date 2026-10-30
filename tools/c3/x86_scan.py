"""Full-coverage x86 scans of the client modules (callers of an address, accesses to a struct offset).

Why this exists: capstone's `Cs.disasm()` STOPS at the first byte it cannot decode (jump tables and data inside
.text). On the C3 client it covered 21 % of Engine.dll's .text and 3.8 % of NWindow.dll's, so "nobody calls X"
conclusions drawn from it were wrong (the name-colour investigation of 18/10/2026). `sweep()` restarts one byte
after every stop and covers 100 %.

    python -I -X utf8 tools/c3/x86_scan.py calls  <dir> 0x10304020 Engine.dll,NWindow.dll
    python -I -X utf8 tools/c3/x86_scan.py vslot  <dir> 0xd8       Engine.dll       # call dword ptr [reg + 0xd8]
    python -I -X utf8 tools/c3/x86_scan.py offset <dir> 0x22c      Engine.dll       # any [reg + 0x22c] (R/W marked)

Prints `module address kind instruction`; `--ctx N` also prints the N instructions before each hit.
Run it from a directory that has no `dis.py`/`inspect.py` of its own (a local `dis.py` shadows the stdlib one).
"""
import argparse
import os
import re

import pefile
from capstone import Cs, CS_ARCH_X86, CS_MODE_32

_MD = Cs(CS_ARCH_X86, CS_MODE_32)
_WRITE_MNEMONICS = ('mov', 'add', 'sub', 'or', 'and', 'xor', 'inc', 'dec', 'fstp', 'fst')


def sweep(data, va0):
    """Linear sweep that skips undecodable bytes instead of stopping."""
    off, n = 0, len(data)
    while off < n:
        for ins in _MD.disasm(data[off:], va0 + off):
            off = ins.address - va0 + ins.size
            yield ins
        if off < n:
            off += 1


def code_sections(path):
    pe = pefile.PE(path)
    base = pe.OPTIONAL_HEADER.ImageBase
    for s in pe.sections:
        if s.Characteristics & 0x20000000:
            yield base + s.VirtualAddress, s.get_data()


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('mode', choices=('calls', 'vslot', 'offset'))
    ap.add_argument('root', help='client system folder')
    ap.add_argument('value', help='address (calls) or offset (vslot/offset), hex')
    ap.add_argument('modules', help='comma separated, e.g. Engine.dll,NWindow.dll')
    ap.add_argument('--ctx', type=int, default=0)
    args = ap.parse_args()

    value = int(args.value, 16)
    off_re = re.compile(r'\[[a-z]{3}(?: \+ [a-z]{3}\*\d)? \+ %s\]' % re.escape(hex(value)))
    for mod in args.modules.split(','):
        for va0, data in code_sections(os.path.join(args.root, mod)):
            window = []
            for ins in sweep(data, va0):
                kind = None
                if args.mode == 'calls' and ins.mnemonic == 'call' and ins.op_str.startswith('0x'):
                    try:
                        if int(ins.op_str, 16) == value:
                            kind = 'call'
                    except ValueError:
                        pass
                elif args.mode == 'vslot' and ins.mnemonic == 'call' and off_re.search(ins.op_str):
                    kind = 'vslot'
                elif args.mode == 'offset' and off_re.search(ins.op_str):
                    dst = ins.op_str.split(',')[0].strip()
                    is_w = ins.mnemonic in _WRITE_MNEMONICS and dst.startswith(('dword ptr [', 'word ptr [', 'byte ptr ['))
                    kind = 'W' if is_w else 'R'
                if kind:
                    print(mod, hex(ins.address), kind, ins.mnemonic, ins.op_str)
                    for w in window[-args.ctx:] if args.ctx else ():
                        print('     %#x  %s %s' % (w.address, w.mnemonic, w.op_str))
                if args.ctx:
                    window.append(ins)
                    if len(window) > 4 * args.ctx:
                        del window[:2 * args.ctx]


if __name__ == '__main__':
    main()
