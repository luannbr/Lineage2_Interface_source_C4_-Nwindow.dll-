# Differential test (stock vs ours under unicorn) of the shortcut bar's slot record NShortCutItem (src/nwindow/NCShortCutWnd.cpp), wave C
# lane debt2: NShortCutItem::NShortCutItem() 0x10108D90, NShortCutItem(NItemInfo&) 0x10108DF0, ~NShortCutItem 0x10108E70 (our
# DestroyItem), CopyFrom 0x101056E0 and Clear 0x101057F0, on the C4 layout (entry 0x1C40 bytes, the cool time icon at +0x1C40, Owner
# +0x1C98, +0x1C9C, bUsed +0x1CA0, bAutoShot +0x1CA4, +0x1CB8, +0x1CBC; the slot's kind at +0x1B18, read in UpdateItem 0x10105ED0).
# The entry's constructors / Copy / Reset, the cool time's constructor / Stop / destructor and the tool tip's destructor are logging
# stubs on both sides; the getters (GetName, the first dword) and appStrcpy run for real. Every byte of both slots is compared.
#   python tools/port/emu/win/t_shortcutitem.py [cases] [seed]   (needs the built DLL + .map and the smoke log)
import sys, os, random, struct
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from gw import *
import gw
import stubs

SIZE = 0x1CC0
CALLEES = {
    0x1001F7F0: ('ItemCtor', 0, 1), 0x1001F960: ('ItemCtorInfo', 4, 1), 0x1001F470: ('CoolCtor', 0, 1), 0x1001FE00: ('ItemCopy', 4, 0),
    0x1001E5E0: ('ItemReset', 0, 0), 0x1001ECB0: ('CoolStop', 0, 0), 0x10002E40: ('TipDtor', 0, 0),   # (the cool time's destructor 0x10043B50 is a lone `ret`: it runs natively)
}
SYMS = {
    0x1001F7F0: '??0NItemEntry@@QAE@XZ', 0x1001F960: '??0NItemEntry@@QAE@PAUNItemInfo@@@Z', 0x1001F470: '??0NCCoolTimeIcon@@QAE@XZ',
    0x1001FE00: '?Copy@NItemEntry@@QAEXPAU1@@Z', 0x1001E5E0: '?Reset@NItemEntry@@QAEXXZ', 0x1001ECB0: '?Stop@NCCoolTimeIcon@@QAEXXZ',
    0x10002E40: '??1NToolTipInfo@@QAE@XZ',
}
FUNCS = {'ctor': (0x10108D90, ()), 'ctor_info': (0x10108DF0, ('info',)), 'destroy': (0x10108E70, ()), 'copyfrom': (0x101056E0, ('src',)),
         'clear': (0x101057F0, ())}


class SC(GW):
    def __init__(self, kind):
        have = None
        if kind == 'ours':
            import emu as _e
            have = set()
            for line in open(_e.OURS_MAP, errors='replace'):
                p = line.split()
                if len(p) > 2:
                    have.add(p[1])
        GW.__init__(self, kind, CALLEES, {va: s for va, s in SYMS.items() if have is None or s in have})


def rnd_block(e, r, size):
    p = e.alloc(size + 16)
    b = bytearray(r.getrandbits(8) for _ in range(size))
    names = ['', 'A', 'Healing Potion', 'x' * 200]
    s = r.choice(names).encode('utf-16-le') + b'\0\0'
    b[0x14:0x14 + len(s)] = s
    ic = r.choice([0, 0x7000 + r.getrandbits(8)])
    b[0:4] = struct.pack('<I', ic)
    e.uc.mem_write(p, bytes(b))
    return p


def run(e, A, c):
    mark = len(e.log)
    r = random.Random(c['seed'])
    e.heap_ptr = e.heap_mark
    va, kinds = A[c['f']]
    slot = rnd_block(e, r, SIZE)
    e.objs[slot] = 'slot'
    args = []
    extra = []
    for k in kinds:
        if k == 'info':
            p = rnd_block(e, r, 0x19B4)
            e.objs[p] = 'info'
            args.append(p)
            extra.append((p, 0x19B4))
        elif k == 'src':
            p = rnd_block(e, r, SIZE)
            e.objs[p] = 'src'
            args.append(p)
            extra.append((p, SIZE))
    e.call(va, this=slot, args=args)
    objs = [(slot, SIZE)] + extra
    return dict(log=e.log[mark:], st=[e.snapshot(b, n) for b, n in objs])


if __name__ == '__main__':
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 40
    seed = int(sys.argv[2]) if len(sys.argv) > 2 else 1
    lm = log_map()
    es, eo = SC('stock'), SC('ours')
    es.heap_mark, eo.heap_mark = es.heap_ptr, eo.heap_ptr
    A_s = dict(FUNCS)
    A_o = {k: (lm[v[0]], v[1]) for k, v in FUNCS.items()}
    r = random.Random(seed)
    total = 0
    for f in FUNCS:
        cases = [dict(f=f, seed=r.randint(0, 10 ** 6)) for _ in range(n)]
        total += compare(f, es, eo, run, cases, A_s, A_o)
    print('TOTAL problems', total)
