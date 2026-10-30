# Differential test of the C4 paperdoll's drop, NCEquipItemWnd::OnLButtonUp (stock 0x10096370, 2,564 bytes; not this lane's file: the proof only),
# stock against ours under unicorn, with the world and the services of t_invdrop.py: the slot of the selected item is found through the
# virtual GetEquipSlot (slot 74), then the window under the mouse decides: the inventory page (the item is taken off, a finger / ear / body
# part by the player's equipped ids), the shortcut bar, the pet's items (an amount asks first), an inventory icon (trash / recipe), the
# game screen (the ground), anything else.
#   python t_equipdrop.py [cases] [seed]
import sys, os, random, struct, copy
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import t_invdrop as D
from t_invdrop import *
import gw
from unicorn import UC_HOOK_CODE

gw.VPOP.update({27: 8, 74: 8, 79: 0})
FN = 0x10096370
SIZE = 2564
D.CLS.update({'InvenItem': 0x102DB898, 'PetInven': 0x102F2FC0})
EQ = dict(D.CALLEES)
EQ.update({0x100663E0: ('RqUnEquip', 4, 0), 0x10094860: ('EquipUnselectAll', 0, 0)})
SCENARIOS = ['InvenItem', 'ShortCut', 'PetInven', 'Button', 'Console', 'Other', 'Equip', 'none', 'RecipeBook']


class EW(IW):
    def __init__(self, kind):
        IW.__init__(self, kind, EQ)
        self.vrets[74] = self.equip_slot

    def equip_slot(self, e, t, a):
        # the console's answer: a function of the body part and the object id, with the 14 / 15 / 16 / 17 cases of the stock's loop
        v = (a[0] * 5 + a[1]) % 20
        e.log.append(('GetEquipSlot', a[0], a[1], v))
        return v

    def do_import(self, nm):
        if nm == '??1L2ParamStack@@QAE@XZ':
            return self.this()       # not logged: the stock destroys the request's list after UnselectAll, ours before (benign)
        return IW.do_import(self, nm)

    def service(self, nm, t, a):
        if nm == 'RqUnEquip':
            self.log.append((nm, self.nm(t), list(self.params.get(a[0], []))))
            return None
        if nm == 'EquipUnselectAll':
            self.log.append((nm, self.nm(t)))
            return None
        return IW.service(self, nm, t, a)


def run(e, A, c):
    mark = len(e.log)
    e.case = c
    e.params.clear()
    for k in D.CLS:
        e.uc.mem_write(e.cls(k), bytes(0x60))
    ptrs = [put_item(e, it, k) for k, it in enumerate(c['items'])]
    for p, it in zip(ptrs, c['items']):
        e.write32(p + 0x1B30, it['bp'])
        e.write32(p + 0x1BA0, it['flag'])
    arr = e.alloc(4 * 32)
    # (the pet branch indexes the items by the SLOT number, as the stock does: the entries past the list are a dummy item)
    dummy = put_item(e, dict(name='Dummy', display='', objid=999, id=9, count=1, stk=0), 99)
    e.objs[dummy] = 'dummy'
    for i in range(32):
        e.write32(arr + 4 * i, ptrs[i] if i < len(ptrs) else dummy)
    if c['nullitem'] is not None and 0 <= c['nullitem'] < len(ptrs):
        e.write32(arr + 4 * c['nullitem'], 0)
    w, ent = make_window(e, 'wnd', 0, 0x300)
    e.write32(w, e.vt)
    e.write32(w + 0xF0, arr)
    e.write32(w + 0xF4, len(ptrs))
    e.write32(w + 0xF8, 32)
    e.write32(w + 0x15C, c['dragging'])
    e.write32(w + 0x174, c['numslots'])
    e.write32(w + 0x17C, c['sel'] & 0xFFFFFFFF)
    for i, v in enumerate((0x11111111, 0x22222222, 0x33333333)):
        e.write32(w + 0x184 + 4 * i, v)
    found = 0
    if c['found'] is not None:
        tw, tent = make_window(e, 'target', make_class(e, c['found'], c['depth']) if c['found'] != 'nullclass' else 0)
        found = tent
        if c['wnd_null']:
            e.write32(tent + 4, 0)
        if c['found'] == 'Button' and c['parent'] != 'none':
            iw, pent = make_window(e, 'inv', 0)
            e.write32(tent + 0xC, pent)
            par = c['parent']
            if par == 'nownd':
                e.write32(pent + 4, 0)
            elif par in ('inv', 'invderived'):
                e.write32(iw + 0x24, make_class(e, 'Inventory', 1 if par == 'invderived' else 0))
            elif par == 'other':
                e.write32(iw + 0x24, make_class(e, 'Other', 0))
            else:
                e.write32(iw + 0x24, 0)
            e.write32(iw + 0x120, c['inv'][0])
            e.write32(iw + 0x124, c['inv'][1])
            tb, _ = make_window(e, 'trashbtn', 0)
            rb, _ = make_window(e, 'recipebtn', 0)
            e.write32(iw + 0x148, tb)
            e.write32(iw + 0x14C, rb)
    world = dict(found_entry=found)
    data = 0
    if c['data'] is not None:
        data = e.alloc(0x240)
        e.uc.mem_write(data, bytes(0x240))
        e.write32(data + 4, c['data'][0])
        e.write32(data + 0x74, c['data'][1])
        e.write32(data + 0xA8, c['data'][2])
        e.objs[data] = 'data'
    world['data'] = data
    e.fill_itemtable([it['id'] for it in c['items']], data)
    user = e.alloc(0x400)
    e.uc.mem_write(user, bytes(0x400))
    for off, v in zip((0x98, 0x9C, 0xA4, 0xA8), c['equipped']):
        e.write32(user + off, v)
    e.write32(user + 0x1F8, c['userflag'])
    e.objs[user] = 'user'
    world['user'] = user
    e.world = world
    for o, v in zip((0x21C, 0x220, 0x224), c['loc']):
        e.write32(e.engine + o, v)
    lp = (c['y'] << 16) | c['x']
    ret = e.call(A, this=w, args=[0, lp])
    return dict(ret=ret, wnd=e.snapshot(w, 0x190), order=[e.nm(e.read32(e.read32(w + 0xF0) + 4 * i)) for i in range(e.read32(w + 0xF4))],
                log=e.log[mark:])


def rand_case(r, scen=None):
    n = r.choice([0, 1, 2, 3, 4, 6])
    items = []
    for i in range(n):
        items.append(dict(name=r.choice(['', 'Sword', 'Zubei Boots']), display=r.choice(['', '+3 Sword', 'Boots']),
                          objid=r.choice([100 + i, 100 + i, 5000 + i, 77]), id=r.choice([57, 1, 2, 40, 1060]),
                          count=r.choice([0, 1, 2, 7, 300]), stk=r.choice([0, 1, 2, 3, 4]),
                          bp=r.choice([0x1, 0x2, 0x4, 0x6, 0x8, 0x10, 0x20, 0x30, 0x40, 0x100, 0x200, 0x400, 0x10000]),
                          flag=r.choice([0, 0, 0, 1])))
        if r.random() < 0.2:
            items[-1]['objid'] = (16 - items[-1]['bp'] * 5) % 20 + 20 * r.randint(0, 5000)
    scen = scen or r.choice(SCENARIOS + ['InvenItem'] * 3 + ['Console'] * 3 + ['Button'] * 3)
    c = dict(items=items, scen=scen)
    c['numslots'] = r.choice([14, 15, 18, 3])
    # a slot an item sits in, most of the time
    sel = r.randint(-2, 20)
    if n and r.random() < 0.85:
        it = r.choice(items)
        s = (it['bp'] * 5 + it['objid']) % 20
        sel = 5 if s == 16 else (6 if s in (15, 17) else s)
    c['sel'] = sel
    c['dragging'] = r.choice([1, 1, 1, 0])
    c['nullitem'] = None     # a NULL entry in the list is a crash in the stock's slot loop, on both sides
    c['x'] = r.choice([0, 100, 400, r.randint(0, 800)])
    c['y'] = r.choice([0, 50, 300, r.randint(0, 600)])
    c['found'] = None if scen == 'none' else scen
    if scen == 'none' and r.random() < 0.5:
        c['found'] = 'nullclass'
    c['depth'] = r.choice([0, 0, 0, 1, 2])
    c['wnd_null'] = 1 if r.random() < 0.06 else 0
    c['parent'] = r.choice(['inv', 'inv', 'invderived', 'none', 'nownd', 'other', 'nullclass'])
    c['inv'] = [r.randint(0, 1), r.randint(0, 1)]
    c['slot'] = r.choice([-1, 0, 3, 11])
    c['intrash'] = r.choice([0, 1, 1])
    c['inrecipe'] = r.choice([0, 1, 1])
    c['data'] = r.choice([None, (2, 5, 8), (2, 0, 8), (1, 5, 8), (2, 3, 7)])
    c['userflag'] = r.choice([0, 1, 1])
    c['targeted'] = r.choice([0, 0, 0, 1])
    ids = [it['objid'] for it in items]
    c['equipped'] = [r.choice(ids + [0]) if ids else 0 for _ in range(4)]
    c['loc'] = [r.randint(0, 1 << 30) for _ in range(3)]
    c['hit'] = 0
    c['inwin'] = 0
    return c


def coverage(es, cases):
    import pefile
    from capstone import Cs, CS_ARCH_X86, CS_MODE_32
    hit = set()
    es.uc.hook_add(UC_HOOK_CODE, lambda uc, a, s, u: hit.add(a), None, FN, FN + SIZE)
    es.heap_mark = es.heap_ptr
    for c in cases:
        es.heap_ptr = es.heap_mark
        try:
            run(es, FN, copy.deepcopy(c))
        except Exception:
            pass
        es.log.clear()
    st = pefile.PE(os.path.join(ROOT, 'orig', 'NWindow.dll'))
    base = st.OPTIONAL_HEADER.ImageBase
    mem = st.get_memory_mapped_image()
    md = Cs(CS_ARCH_X86, CS_MODE_32)
    allins = [i.address for i in md.disasm(bytes(mem[FN - base:FN - base + SIZE]), FN)]
    miss = [a for a in allins if a not in hit]
    print('stock coverage: instructions', len(allins), 'covered', len(allins) - len(miss), 'missed', len(miss), [hex(a) for a in miss[:40]])
    return miss


def main():
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 1500
    seed = int(sys.argv[2]) if len(sys.argv) > 2 else 1
    r = random.Random(seed)
    cases = [rand_case(r) for _ in range(n)]
    es = EW('stock')
    eo = EW('ours')
    lm = log_map()
    es.heap_mark, eo.heap_mark = es.heap_ptr, eo.heap_ptr
    bad = compare('equipdrop', es, eo, run, [copy.deepcopy(c) for c in cases], FN, lm[FN])
    miss = coverage(EW('stock'), cases)
    print('RESULT', 'DIFFERENT' if bad else 'IDENTICAL', 'cases', n, 'seed', seed, 'stock instructions missed', len(miss))
    sys.exit(1 if bad else 0)


if __name__ == '__main__':
    main()
