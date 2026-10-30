# Union of the instruction coverage of several differential runs (PW_COVOUT=<file>.json python t_xxx.py ...): the runs of one function
# (other seeds, other dialog ids, GARA_LONG=1 ...) write the covered instruction addresses of the stock function to their json; this prints
# the union and the runs of instructions nobody ran, with the first instruction of each (to say why: padding, a null `new`, dead code ...).
#   python cov_union.py a.json [b.json ...]
import sys, json
from capstone import Cs, CS_ARCH_X86, CS_MODE_32
import fncalls

md = Cs(CS_ARCH_X86, CS_MODE_32)


def main():
    cov = {}
    for fn in sys.argv[1:]:
        for k, v in json.load(open(fn)).items():
            if not k.endswith('_total'):
                cov.setdefault(k, set()).update(v)
    for k in sorted(cov):
        va = int(k, 16)
        size = fncalls.FUNCS.get(va)
        insns = list(md.disasm(bytes(fncalls.MEM[va - fncalls.BASE:va + size - fncalls.BASE]), va)) if size else []
        miss = [i for i in insns if i.address not in cov[k]]
        print('%s %s: %d of %d instructions in the union' % (k, fncalls.NAMES.get(va), len(insns) - len(miss), len(insns)))
        run = []
        for i in miss + [None]:
            if run and (i is None or i.address != run[-1].address + run[-1].size):
                print('    not run %x-%x (%d insns): %s %s' % (run[0].address, run[-1].address, len(run), run[0].mnemonic, run[0].op_str))
                run = []
            if i is not None:
                run.append(i)


if __name__ == '__main__':
    main()
