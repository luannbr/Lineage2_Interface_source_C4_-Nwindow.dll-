# Differential test (stock vs ours under unicorn) of the item-side constructors / destructors of wave C, lane debt2.
# Group 1 (hooked in this tree, proved):
#   NItemEntry::~NItemEntry 0x10050030 (the tool tip's destructor 0x10002E40 on entry + 0x1BC0),
#   NCShopWnd::NCShopWnd 0x10105120 (0x124 bytes), NCGaraTree::NCGaraTree 0x10015B70 (0x20C bytes),
#   NCShopWnd::~NCShopWnd 0x100B0D10, NCGaraTree::~NCGaraTree 0x10015BF0, NCShopItemWnd::~NCShopItemWnd 0x10105090.
# Group 2 (OPTIONAL: run only the functions whose stock address is hooked in the DLL under test, so that the lane that
# hooks them has the proof ready): NCManorShopWnd::NCManorShopWnd 0x100B0E70 and NCRecipeGaraTree::NCRecipeGaraTree 0x100F6D80
# (the derived constructors that waited for the real NCShopWnd() / NCGaraTree()), and the three recipe window destructors
# 0x100F3600 / 0x100F6870 / 0x100F7030 (they destroy a NItemEntry member: ~NItemEntry is a real hook, so they are not blocked
# by an unported stub any more; the stock inlines it as a call of the tool tip's destructor 0x10002E40).
# The object is filled with a random pattern, the function runs on both sides, the whole object and the calls the body makes
# (the base constructor / destructor, the tool tip destructor: logging stubs on both sides) are compared; the vtable pointer is
# replaced by 'vt' (ours is a different table).
#   python tools/port/emu/win/t_itemdebt.py [cases] [seed]     (needs build/bin/Release/NWindowDecomp.dll + .map and the smoke log)
import sys, os, random, struct
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from gw import *
import stubs

# name: (stock va, object size, argument count)
FUNCS1 = {
    'entry_dtor': (0x10050030, 0x1C40, 0),
    'shop_ctor': (0x10105120, 0x124, 0),
    'gara_ctor': (0x10015B70, 0x20C, 0),
    'shop_dtor': (0x100B0D10, 0x124, 0),
    'gara_dtor': (0x10015BF0, 0x20C, 0),
    'shopitem_dtor': (0x10105090, 0x174, 0),
}
# callees: stock va -> (name, pop, returns this)
CALLEES1 = {
    0x10002E40: ('ToolTipDtor', 0, 0),     # NToolTipInfo::~NToolTipInfo
    0x10036CE0: ('NCWndCtor', 0, 1),       # NCWnd::NCWnd
    0x10002FB0: ('NCWndDtor', 0, 0),       # NCWnd::~NCWnd
    0x10020270: ('NCItemWndDtor', 0, 0),   # NCItemWnd::~NCItemWnd
}
OURS_SYMS1 = {
    0x10002E40: '??1NToolTipInfo@@QAE@XZ',
    0x10036CE0: '??0NCWnd@@QAE@XZ',
    0x10002FB0: '??1NCWnd@@UAE@XZ',
    0x10020270: '??1NCItemWnd@@UAE@XZ',
}
FUNCS2 = {
    'manorshop_ctor': (0x100B0E70, 0x160, 0),        # NCManorShopWnd(): NCShopWnd() and the vtable 0x101BB550
    'recipegara_ctor': (0x100F6D80, 0x228, 0),       # NCRecipeGaraTree(): NCGaraTree() and the vtable 0x101C8030
    'recipebuy_dtor': (0x100F3600, 0x1D7C, 0),       # ~NCRecipeBuyManufactureWnd
    'recipemake_dtor': (0x100F6870, 0x1D78, 0),      # ~NCRecipeManufactureWnd
    'recipegarawnd_dtor': (0x100F7030, 0x1D40, 0),   # ~NCRecipeGaraTreeWnd
}
CALLEES2 = {
    0x10105120: ('NCShopWndCtor', 0, 1),   # NCShopWnd::NCShopWnd
    0x10015B70: ('NCGaraTreeCtor', 0, 1),  # NCGaraTree::NCGaraTree
    0x10002E40: ('ToolTipDtor', 0, 0),
    0x10002FB0: ('NCWndDtor', 0, 0),
}
OURS_SYMS2 = {
    0x10105120: '??0NCShopWnd@@QAE@XZ',
    0x10015B70: '??0NCGaraTree@@QAE@XZ',
    0x10002E40: '??1NToolTipInfo@@QAE@XZ',
    0x10002FB0: '??1NCWnd@@UAE@XZ',
}
stubs.IMPORT_POP.update({'?ConditionalDestroy@UObject@@QAEHXZ': 0})


def map_symbols():
    import emu as _e
    out = set()
    for line in open(_e.OURS_MAP, errors='replace'):
        p = line.split()
        if len(p) > 2:
            out.add(p[1])
    return out


class CW(GW):
    def __init__(self, kind, callees, ours_syms):
        have = map_symbols() if kind == 'ours' else None
        syms = {va: nm for va, nm in ours_syms.items() if have is None or nm in have}
        GW.__init__(self, kind, callees, syms)

    def dispatch(self, name):
        kind, nm = name.split(':', 1)
        if kind == 'imp' and nm == '?ConditionalDestroy@UObject@@QAEHXZ':
            self.last_pop = 0
            self.log.append(('ConditionalDestroy', self.nm(self.this())))
            return 1
        return GW.dispatch(self, name)


def run(e, A, c):
    mark = len(e.log)
    va, nargs, size = A[c['f']]
    obj = e.alloc(size + 16)
    e.uc.mem_write(obj, bytes([0xCD]) * (size + 16))
    r = random.Random(c['seed'])
    # a plausible, random content: dwords of the object (the poison stays in the guard words after the end)
    e.uc.mem_write(obj, bytes(r.getrandbits(8) for _ in range(size)))
    e.objs[obj] = 'obj'
    if c['f'] == 'entry_dtor':
        e.objs[obj + 0x1BC0] = 'tooltip'
    e.call(va, this=obj, args=[])
    mem = bytes(e.uc.mem_read(obj, size + 16))
    words = struct.unpack('<%dI' % ((size + 16) // 4), mem)
    snap = [('vt' if i == 0 and c['f'] != 'entry_dtor' else w) for i, w in enumerate(words)]
    return dict(log=e.log[mark:], obj=snap)


def group(title, funcs, callees, syms, lm, n, r, optional):
    names = [f for f in funcs if (not optional) or funcs[f][0] in lm]
    for f in funcs:
        if f not in names:
            print('%s: %s (0x%08X) is not hooked in this DLL: skipped' % (title, f, funcs[f][0]))
    if not names:
        return 0
    es = CW('stock', callees, syms)
    eo = CW('ours', callees, syms)
    es.heap_mark, eo.heap_mark = es.heap_ptr, eo.heap_ptr
    As = {k: (v[0], v[1], v[2]) for k, v in funcs.items()}
    Ao = {k: (lm[v[0]], v[1], v[2]) for k, v in funcs.items() if k in names}
    total = 0
    for f in names:
        cases = [dict(f=f, seed=r.randint(0, 10 ** 6)) for _ in range(n)]
        total += compare(f, es, eo, run, cases, As, Ao)
    return total


if __name__ == '__main__':
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 8
    seed = int(sys.argv[2]) if len(sys.argv) > 2 else 1
    lm = log_map()
    r = random.Random(seed)
    total = group('group 1', FUNCS1, CALLEES1, OURS_SYMS1, lm, n, r, False)
    total += group('group 2', FUNCS2, CALLEES2, OURS_SYMS2, lm, n, r, True)
    print('TOTAL problems', total)
