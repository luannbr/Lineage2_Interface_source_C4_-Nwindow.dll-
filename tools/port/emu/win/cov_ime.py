import os
import sys, random
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from t_ime import *
from unicorn import UC_HOOK_CODE
import pefile, csv
from capstone import Cs, CS_ARCH_X86, CS_MODE_32
ROOT = str(__import__('pathlib').Path(__file__).resolve().parents[4])
st = pefile.PE(os.path.join(ROOT, 'orig', 'NWindow.dll')); base = st.OPTIONAL_HEADER.ImageBase; mem = st.get_memory_mapped_image()
md = Cs(CS_ARCH_X86, CS_MODE_32)
sizes = {0x1001b720: 3600, 0x1001cb90: 1592, 0x10017c70: 603}
n = int(sys.argv[1]); seed = int(sys.argv[2])
r = random.Random(seed)
for f, (va, _) in FUNCS.items():
    es = IW('stock', f)
    hit = set()
    es.uc.hook_add(UC_HOOK_CODE, lambda uc, a, s, u: hit.add(a), None, va, va + sizes[va])
    As = {k: (v[0], v[1]) for k, v in FUNCS.items()}
    es.heap_mark = es.heap_ptr
    for i in range(n):
        es.heap_ptr = es.heap_mark
        c = rand_case(r, f)
        try:
            run(es, As, c)
        except Exception as ex:
            pass
        es.log.clear()
    allins = [i.address for i in md.disasm(bytes(mem[va - base:va - base + sizes[va]]), va)]
    miss = [a for a in allins if a not in hit]
    print(f, 'instructions', len(allins), 'covered', len(allins) - len(miss), 'missed', len(miss))
    print('  first missed:', [hex(a) for a in miss[:30]])
