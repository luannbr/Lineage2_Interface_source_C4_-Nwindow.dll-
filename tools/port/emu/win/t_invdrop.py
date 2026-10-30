# Differential test of the C4 inventory page's drop, NCInvenItemWnd::OnLButtonUp (stock 0x100976C0, 2,701 bytes; "OnLButtonDown" in its guard
# names), stock against ours under unicorn. Every branch the drop can take is a scenario:
#   - the release inside the page (reorder: past the end / in the middle / same cell / outside the item range),
#   - the window found under the mouse: paperdoll, shortcut bar (slot found / none), pet window (one item / a stack), one of the inventory's
#     icons (trash: one item / a stack; recipe: allowed / not allowed / no record / no crystal count; a button of another parent), the game
#     screen (an actor targeted; a drop of one item / of a stack; an etc item of kind 8 placed), the recipe book's items (ignored), other,
#   - the flags (dragging, double click done, Selected out of range, a NULL entry),
#   - classes found through a chain of super classes (IsA walks the chain), a NULL class, a NULL window.
# Every callee is a logging service (the item entry's getters, the dialog info's setters, the console's requests, the window's virtuals
# IsInWindow / UnselectAll); the dialog info is dumped when ShowDialog gets it; the window object and the item order are compared.
#   python t_invdrop.py [cases] [seed]            differential run (stock vs ours), with the stock's instruction coverage
#   python t_invdrop.py cov [cases] [seed]        the stock only: which instructions of the function the cases reach
#   python t_invdrop.py mut [cases] [seed]        mutation test: every conditional jump of OUR function flipped in turn, the harness must see it
import sys, os, random, struct
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import tip4          # clears the edit box services of e4 (its side effect is wanted)
from gw import *
import gw
import sa
import stubs
import slotsyms
from slotsyms import auto_syms
slotsyms.install_cache()
from unicorn import UC_HOOK_CODE

gw.VPOP.update({27: 8, 79: 0})
stubs.IMPORT_POP.update({'??0L2ParamStack@@QAE@H@Z': 4, '??1L2ParamStack@@QAE@XZ': 0, '?PushBack@L2ParamStack@@QAEHPAX@Z': 4,
                         '?Insert@FArray@@QAEXHHH@Z': 12})

FN = 0x100976C0
RANGES = [(0x100976C0, 0x1009783C), (0x10097840, 0x10097859), (0x10097860, 0x10097879), (0x10097880, 0x10097899), (0x100978A0, 0x10098161)]

# stock va -> (name, pop, returns this)
CALLEES = {
    0x1001E790: ('HitTest', 0x10, 0), 0x10034A40: ('FindWindowAt', 8, 0), 0x10094890: ('EquipHitTest', 8, 0),
    0x10066320: ('RequestUseItem', 4, 0), 0x101062B0: ('SlotAtPoint', 8, 0), 0x1007A1E0: ('RequestShortCutReg', 4, 0),
    0x10069000: ('RequestGiveItemToPet', 4, 0), 0x1001E5B0: ('IsStackable', 0, 0), 0x1001E460: ('GetName', 0, 0),
    0x1001E470: ('MakeDisplayName', 0, 0), 0x10093F00: ('IsInTrash', 8, 0), 0x10093F60: ('IsInRecipe', 8, 0),
    0x101514A0: ('FindByKey', 4, 0), 0x10073E80: ('GetMyUser', 0, 0), 0x1006A7A0: ('GetTargetedActor', 4, 0),
    0x10001530: ('SetNormalTex', 4, 0), 0x10001570: ('SetPushedTex', 4, 0),
    0x100085C0: ('DlgCtor', 0, 1), 0x100079B0: ('DlgSetMsg', 4, 0), 0x10007960: ('DlgSetButtons', 8, 0), 0x10007B20: ('DlgSetParams', 8, 0),
    0x10007D50: ('DlgDtor', 0, 0), 0x10072E20: ('ShowDialog', 4, 0),
}
# the classes (stock va of the UClass): the checks of the stock function, in its order
CLS = {'Equip': 0x102DAE38, 'ShortCut': 0x10307210, 'Pet': 0x102F34F0, 'Button': 0x102B5DB8, 'Inventory': 0x102DBDD0,
       'RecipeBook': 0x102FF538, 'Console': 0x102D21A0, 'Other': 0x102AAAA0}
SCENARIOS = ['inpage', 'Equip', 'ShortCut', 'Pet', 'Button', 'RecipeBook', 'Console', 'Other', 'Inventory', 'none']
ITEMTABLE = 0x00ABCDE0     # what GL2GameData + 0x1818 holds (never dereferenced: FindByKey is a service)


def e_world(e):
    return e.world


class IW(GW):
    def __init__(self, kind, callees=None, extra_syms=None):
        cal = callees or CALLEES
        syms = {}
        if kind == 'ours':
            syms = auto_syms(cal, LOGFILE)
            syms.update(extra_syms or {})
        GW.__init__(self, kind, cal, syms)
        self.params = {}
        self.case = None
        self.vrets[27] = lambda e, t, a: e.case['inwin']
        self.write32(self.gdata + 0x1818, ITEMTABLE)
        if kind == 'ours' and not sa.ENABLED:
            # the HOOK build inlines NDataMap::FindRef in the drop handlers (the standalone calls the function, which the FindByKey service stands for):
            # give the inlined hash walk a real table, filled per case by fill_itemtable (every item id finds the case's data record)
            self.uc.mem_map(ITEMTABLE & ~0xFFF, 0x2000)
        self.engine = self.alloc(0x400)
        self.write32(self.console + 0x54, self.engine)
        self.objs[self.console] = 'console'
        self.objs[ITEMTABLE] = 'itemtable'
        self.cls_obj = {}
        # the class pointers are resolved here, before the heap mark of the test: in the standalone a class with no object of ours ('Other') gets a
        # scratch block from the heap, and the stock side takes the same block so that both heaps stay in step (a raw heap value shows in the logs)
        for k, v in CLS.items():
            self.cls_obj[k] = self.gptr(v)
            if kind == 'stock' and sa.ENABLED:
                try:
                    sa.xlate_va(v)
                except sa.NoCounterpart:
                    self.alloc(0x1000)

    def fill_itemtable(self, ids, data):
        """hook build only: a hash table (one bucket, one chain) in which every id of `ids` maps to `data`; layout of the inlined walk: +0 entries (stride 12:
        next, key, value), +0xC buckets, +0x10 bucket count"""
        n = len(ids)
        ent = self.alloc(12 * max(n, 1) + 16)        # (both sides allocate: the heap values that show in a snapshot stay equal)
        buckets = self.alloc(16)
        if self.kind != 'ours' or sa.ENABLED:
            return
        for i, k in enumerate(ids):
            self.write32(ent + 12 * i, (i + 1) if i + 1 < n else 0xFFFFFFFF)
            self.write32(ent + 12 * i + 4, k)
            self.write32(ent + 12 * i + 8, data)
        self.write32(buckets, 0 if n else 0xFFFFFFFF)
        self.write32(ITEMTABLE, ent)
        self.write32(ITEMTABLE + 0xC, buckets)
        self.write32(ITEMTABLE + 0x10, 1)

    def cls(self, name):
        # the pointer value for the class's stock address: itself (stock), NWBASE + rva (hook), the standalone's own object (sa; a class with
        # no object of ours, like 'Other', gets a scratch block)
        return self.cls_obj[name]

    def dispatch(self, name):
        kind, nm = name.split(':', 1)
        if kind == 'cb':
            va, pop, rthis = self.cb[nm]
            self.last_pop = pop
            t = self.this()
            a = self.sargs(pop // 4)
            r = self.service(nm, t, a)
            if r is not NotImplemented:
                return r
        return GW.dispatch(self, name)

    def service(self, nm, t, a):
        c = self.case
        log = self.log
        if nm == 'HitTest':
            log.append((nm, self.nm(t), a))
            return u32(c['hit'])
        if nm == 'FindWindowAt':
            log.append((nm, self.nm(t), a))
            return e_world(self)['found_entry']
        if nm == 'EquipHitTest':
            log.append((nm, self.nm(t), a))
            return 3
        if nm == 'RequestUseItem':
            log.append((nm, self.nm(t), a))
            return 0
        if nm == 'SlotAtPoint':
            log.append((nm, self.nm(t), a))
            return u32(c['slot'])
        if nm in ('RequestShortCutReg', 'RequestGiveItemToPet'):
            log.append((nm, self.nm(t), list(self.params.get(a[0], []))))
            return None
        if nm == 'IsStackable':
            log.append((nm, self.nm(t)))
            return 1 if self.rds32(t + 0x1B4C) in (1, 2, 3) else 0
        if nm == 'GetName':
            log.append((nm, self.nm(t)))
            p = t + 0x14
            return p if bytes(self.uc.mem_read(p, 2)) != b'\x00\x00' else 0
        if nm == 'MakeDisplayName':
            log.append((nm, self.nm(t)))
            return t + 0x1914
        if nm == 'IsInTrash':
            log.append((nm, self.nm(t), a))
            return c['intrash']
        if nm == 'IsInRecipe':
            log.append((nm, self.nm(t), a))
            return c['inrecipe']
        if nm == 'FindByKey':
            if sa.ENABLED:       # (the hook build inlines the lookup: the stock's call is not logged on either side there)
                log.append((nm, self.nm(t), self.rds32(a[0])))
            return e_world(self)['data']
        if nm == 'GetMyUser':
            log.append((nm, self.nm(t)))
            return e_world(self)['user']
        if nm == 'GetTargetedActor':
            log.append((nm, self.nm(t), a))
            return c['targeted']
        # the dialog info is a stack object: no address in the log, its contents are dumped by ShowDialog
        if nm == 'DlgCtor':
            self.uc.mem_write(t, bytes(0xA0))
            self.write32(t, 0xFFFFFFFF)
            self.write32(t + 4, 3)
            self.write32(t + 0xC, 1)
            log.append((nm,))
            return t
        if nm == 'DlgSetMsg':
            log.append((nm, a[0]))
            return None
        if nm == 'DlgSetButtons':
            log.append((nm, self.wstr(a[0]) if a[0] else None, a[1]))
            return None
        if nm == 'DlgSetParams':
            log.append((nm, a))
            return None
        if nm == 'DlgDtor':
            log.append((nm,))
            return None
        if nm == 'ShowDialog':
            info = a[0]
            log.append((nm, self.nm(t), [self.nm(self.read32(info + o)) for o in range(0, 0xA0, 4)]))
            return None
        return NotImplemented

    def do_import(self, nm):
        t = self.this()
        if nm == '??0L2ParamStack@@QAE@H@Z':
            self.params[t] = []
            self.log.append(('ParamStack', self.arg(0)))
            return t
        if nm == '??1L2ParamStack@@QAE@XZ':
            self.log.append(('~ParamStack',))
            return t
        if nm == '?PushBack@L2ParamStack@@QAEHPAX@Z':
            self.params.setdefault(t, []).append(self.arg(0))
            return 1
        if nm == '?Insert@FArray@@QAEXHHH@Z':
            idx, cnt, size = self.arg(0), self.arg(1), self.arg(2)
            data, num, mx = self.read32(t), self.read32(t + 4), self.read32(t + 8)
            nd = self.alloc((num + cnt + 8) * size)
            if data:
                self.uc.mem_write(nd, bytes(self.uc.mem_read(data, idx * size)))
                self.uc.mem_write(nd + (idx + cnt) * size, bytes(self.uc.mem_read(data + idx * size, (num - idx) * size)))
            self.write32(t, nd)
            self.write32(t + 4, num + cnt)
            self.write32(t + 8, num + cnt + 8)
            return None
        return GW.do_import(self, nm)


def put_item(e, it, k):
    p = e.alloc(0x1C60)
    e.uc.mem_write(p, bytes(0x1C60))
    e.write_wstr_at(p + 0x14, it['name'])
    e.write_wstr_at(p + 0x1914, it['display'])
    e.write32(p + 0x1B1C, it['objid'])
    e.write32(p + 0x1B20, it['id'])
    e.write32(p + 0x1B28, it['count'])
    e.write32(p + 0x1B4C, it['stk'])
    e.write32(p, it.get('icon', 0))
    e.write32(p + 0x1B5C, it.get('selected', 0))
    e.write32(p + 0x1BA4, it.get('warn', 0))
    e.write32(p + 0x1BB8, it.get('msg', 0))
    e.objs[p] = 'item%d' % k
    return p


def make_class(e, name, depth):
    """the UClass pointer a window gets: the static class itself or a chain of `depth` made-up subclasses (UClass +0x34 is the super)"""
    cur = e.cls(name)
    e.uc.mem_write(cur, bytes(0x60))
    e.objs[cur] = 'class:' + name
    for k in range(depth):
        sub = e.alloc(0x60)
        e.uc.mem_write(sub, bytes(0x60))
        e.write32(sub + 0x34, cur)
        e.objs[sub] = 'class:%s/%d' % (name, k + 1)
        cur = sub
    return cur


def make_window(e, label, cls, size=0x300):
    w = e.alloc(size)
    e.uc.mem_write(w, bytes(size))
    ent = e.alloc(0x10)
    e.uc.mem_write(ent, bytes(0x10))
    e.write32(ent, 1)
    e.write32(ent + 4, w)
    e.write32(w + 0x74, ent)
    e.write32(w + 0x24, cls)
    e.objs[w] = label
    e.objs[ent] = 'entry:' + label
    return w, ent


def run(e, A, c):
    mark = len(e.log)
    e.case = c
    e.params.clear()
    for k, v in CLS.items():
        e.uc.mem_write(e.cls(k), bytes(0x60))
    # the items
    ptrs = [put_item(e, it, k) for k, it in enumerate(c['items'])]
    arr = e.alloc(4 * 32)
    for i, p in enumerate(ptrs):
        e.write32(arr + 4 * i, p)
    if c['nullitem'] is not None and 0 <= c['nullitem'] < len(ptrs):
        e.write32(arr + 4 * c['nullitem'], 0)
    # the window
    w, ent = make_window(e, 'wnd', 0, 0x300)
    e.write32(w, e.vt)
    e.write32(w + 0x12C, arr)
    e.write32(w + 0x130, len(ptrs))
    e.write32(w + 0x134, 32)
    e.write32(w + 0x138, c['dragging'])
    e.write32(w + 0x13C, c['nodrag'])
    e.write32(w + 0x160, c['sel'] & 0xFFFFFFFF)
    e.write32(w + 0x168, c['dbl'])
    for i, v in enumerate((0x11111111, 0x22222222, 0x33333333)):
        e.write32(w + 0x16C + 4 * i, v)
    # what is under the mouse
    found = 0
    if c['found'] is not None:
        cname, depth = c['found'], c['depth']
        tw, tent = make_window(e, 'target', make_class(e, cname, depth) if cname != 'nullclass' else 0)
        found = tent
        if c['wnd_null']:
            e.write32(tent + 4, 0)
        if cname == 'Button' or c['found'] == 'Button':
            par = c['parent']
            if par != 'none':
                iw, pent = make_window(e, 'inv', 0)
                e.write32(tent + 0xC, pent)
                if par == 'nownd':
                    e.write32(pent + 4, 0)
                elif par in ('inv', 'invderived'):
                    e.write32(iw + 0x24, make_class(e, 'Inventory', 1 if par == 'invderived' else 0))
                elif par == 'other':
                    e.write32(iw + 0x24, make_class(e, 'Other', 0))
                elif par == 'nullclass':
                    e.write32(iw + 0x24, 0)
                e.write32(iw + 0x120, c['inv'][0])
                e.write32(iw + 0x124, c['inv'][1])
                tb, _ = make_window(e, 'trashbtn', 0)
                rb, _ = make_window(e, 'recipebtn', 0)
                e.write32(iw + 0x148, tb)
                e.write32(iw + 0x14C, rb)
    world = dict(found_entry=found)
    # the item table's record, the player, the engine's mouse location
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
    e.write32(user + 0x1F8, c['userflag'])
    e.objs[user] = 'user'
    for o, v in zip((0x21C, 0x220, 0x224), c['loc']):
        e.write32(e.engine + o, v)
    world['user'] = user
    e.world = world
    lp = (c['y'] << 16) | c['x']
    ret = e.call(A, this=w, args=[0, lp])
    out = dict(ret=ret, wnd=e.snapshot(w, 0x180), order=[e.nm(e.read32(e.read32(w + 0x12C) + 4 * i)) for i in range(e.read32(w + 0x130))],
               log=e.log[mark:])
    if c['found'] == 'Button' and c['parent'] != 'none':
        out['inv'] = e.snapshot(iw, 0x160)
    return out


def rand_case(r, scen=None):
    n = r.choice([0, 1, 2, 3, 4, 5, 6])
    items = []
    for i in range(n):
        items.append(dict(name=r.choice(['', 'Sword', 'Zubei Boots', 'A' * 20]), display=r.choice(['', '+3 Sword', 'Boots', 'X' * 12]),
                          objid=r.randint(100, 10 ** 6), id=r.choice([57, 1, 2, 3, 40, 1060]), count=r.choice([0, 1, 2, 7, 300, -1]),
                          stk=r.choice([0, 1, 2, 3, 4, 5])))
    scen = scen or r.choice(SCENARIOS + ['inpage'] * 3 + ['Console'] * 3 + ['Button'] * 3)
    c = dict(items=items)
    c['scen'] = scen
    c['sel'] = r.randint(0, n - 1) if (n and r.random() < 0.85) else r.randint(-2, n + 1)
    c['dragging'] = r.choice([1, 1, 1, 1, 0])
    c['nodrag'] = r.choice([0, 0, 1])
    c['dbl'] = r.choice([0, 0, 0, 0, 1])
    c['nullitem'] = c['sel'] if (n and 0 <= c['sel'] < n and r.random() < 0.06) else None
    c['x'] = r.choice([0, 1, 100, 400, 0xFFFF, r.randint(0, 800)])
    c['y'] = r.choice([0, 1, 50, 300, 0xFFFF, r.randint(0, 600)])
    c['inwin'] = 1 if scen == 'inpage' else 0
    if scen == 'inpage' and r.random() < 0.05:
        c['inwin'] = 0
    c['hit'] = r.choice([-1, 0, 1, 2, c['sel'], c['sel'], n - 1, n, n + 1, n + 3])
    c['found'] = None if scen in ('inpage', 'none') else scen
    if scen == 'none' and r.random() < 0.5:
        c['found'] = 'nullclass'
    c['depth'] = r.choice([0, 0, 0, 1, 2])
    c['wnd_null'] = 1 if (scen != 'inpage' and r.random() < 0.06) else 0
    c['parent'] = r.choice(['inv', 'inv', 'invderived', 'none', 'nownd', 'other', 'nullclass'])
    c['inv'] = [r.randint(0, 1), r.randint(0, 1)]
    c['slot'] = r.choice([-1, 0, 3, 11])
    c['intrash'] = r.choice([0, 1, 1])
    c['inrecipe'] = r.choice([0, 1, 1])
    c['data'] = r.choice([None, (2, 5, 8), (2, 0, 8), (1, 5, 8), (2, 3, 7), (0, 9, 8), (2, 4, 0)])
    c['userflag'] = r.choice([0, 1, 1])
    c['targeted'] = r.choice([0, 0, 0, 1])
    c['loc'] = [r.randint(0, 1 << 30) for _ in range(3)]
    # the scenario's essentials mostly in place so that the branches are reached
    if scen not in ('none',):
        if r.random() < 0.8:
            c['dbl'] = 0
        if r.random() < 0.85:
            c['dragging'] = 1
    return c


def build_cases(n, seed):
    r = random.Random(seed)
    return [rand_case(r) for _ in range(n)]


def coverage(es, cases, A):
    import pefile
    from capstone import Cs, CS_ARCH_X86, CS_MODE_32
    hit = set()
    h = es.uc.hook_add(UC_HOOK_CODE, lambda uc, a, s, u: hit.add(a), None, 0x100976C0, 0x10098162)
    es.heap_mark = es.heap_ptr
    for c in cases:
        es.heap_ptr = es.heap_mark
        import copy
        try:
            run(es, A, copy.deepcopy(c))
        except Exception:
            pass
        es.log.clear()
    es.uc.hook_del(h)
    st = pefile.PE(os.path.join(ROOT, 'orig', 'NWindow.dll'))
    base = st.OPTIONAL_HEADER.ImageBase
    mem = st.get_memory_mapped_image()
    md = Cs(CS_ARCH_X86, CS_MODE_32)
    allins = []
    for lo, hi in RANGES:
        allins += [i.address for i in md.disasm(bytes(mem[lo - base:hi + 1 - base]), lo)]
    miss = [a for a in allins if a not in hit]
    print('stock coverage: instructions', len(allins), 'covered', len(allins) - len(miss), 'missed', len(miss))
    if miss:
        print('  missed:', [hex(a) for a in miss[:60]])
    return miss


def main():
    args = [a for a in sys.argv[1:]]
    mode = 'diff'
    if args and args[0] in ('cov', 'mut'):
        mode = args.pop(0)
    n = int(args[0]) if args else 600
    seed = int(args[1]) if len(args) > 1 else 1
    cases = build_cases(n, seed)
    import copy
    if mode == 'cov':
        es = IW('stock')
        coverage(es, cases, FN)
        return
    es = IW('stock')
    eo = IW('ours')
    lm = log_map()
    if 0x100976C0 not in lm:
        print('our build has no hook at 0x100976C0 yet (stock stub): nothing to compare')
        return
    es.heap_mark, eo.heap_mark = es.heap_ptr, eo.heap_ptr
    if mode == 'diff':
        bad = compare('invdrop', es, eo, run, [copy.deepcopy(c) for c in cases], FN, lm[0x100976C0])
        miss = coverage(IW('stock'), cases, FN)
        cov = {}
        for c in cases:
            cov[c['scen']] = cov.get(c['scen'], 0) + 1
        print('scenarios', cov)
        print('RESULT', 'DIFFERENT' if bad else 'IDENTICAL', 'cases', n, 'seed', seed, 'stock instructions missed', len(miss))
        sys.exit(1 if bad else 0)
    mutation(cases, lm[0x100976C0], es)


class CountedUc(object):
    """a flipped jump can make a loop endless: every run of a mutant is limited to a number of instructions"""
    def __init__(self, uc, limit):
        self._uc = uc
        self._limit = limit

    def __getattr__(self, name):
        return getattr(self._uc, name)

    def emu_start(self, begin, until, timeout=0, count=0):
        return self._uc.emu_start(begin, until, timeout=timeout, count=self._limit)


def mutation(cases, ours_va, es):
    """flip each conditional jump of our function (jl <-> jge, jz <-> jnz ...) and run the cases against the stock's recorded results"""
    from capstone import Cs, CS_ARCH_X86, CS_MODE_32
    import copy
    # the stock's results, once
    es.heap_mark = es.heap_ptr
    want = []
    for c in cases:
        es.heap_ptr = es.heap_mark
        try:
            a = run(es, FN, copy.deepcopy(c))
        except Exception as ex:
            a = {'exc': str(ex)[:100]}
        want.append(a)
        es.log.clear()
    probe = IW('ours')
    # the hook's forwarder is `push ebp; mov ebp, esp; pop ebp; jmp member`: the code to mutate is the member's
    for _ in range(3):
        b = bytes(probe.uc.mem_read(ours_va, 9))
        if b[0] == 0xE9:
            ours_va = (ours_va + 5 + struct.unpack('<i', b[1:5])[0]) & 0xFFFFFFFF
        elif b[:4] == bytes([0x55, 0x8B, 0xEC, 0x5D]) and b[4] == 0xE9:
            ours_va = (ours_va + 9 + struct.unpack('<i', b[5:9])[0]) & 0xFFFFFFFF
        else:
            break
    # size of our function: the next symbol of the map after its address
    import re
    ends = sorted(v for v in probe.symbols.values() if v > ours_va)
    size = min(ends[0] - ours_va, 0x1200)
    code = bytes(probe.uc.mem_read(ours_va, size))
    md = Cs(CS_ARCH_X86, CS_MODE_32)
    jccs = []
    for i in md.disasm(code, ours_va):
        b = i.bytes
        if len(b) == 2 and 0x70 <= b[0] <= 0x7F:
            jccs.append((i.address, 0, b[0] ^ 1))
        elif len(b) == 6 and b[0] == 0x0F and 0x80 <= b[1] <= 0x8F:
            jccs.append((i.address, 1, b[1] ^ 1))
    print('our function: %d bytes at %x, %d conditional jumps to flip' % (size, ours_va, len(jccs)))
    killed = 0
    alive = []
    for addr, kind, newop in jccs:
        eo = IW('ours')
        eo.heap_mark = eo.heap_ptr
        eo.uc.mem_write(addr + kind, bytes([newop]))
        eo.uc = CountedUc(eo.uc, 1000000)
        diff = False
        for c, a in zip(cases, want):
            eo.heap_ptr = eo.heap_mark
            try:
                b = run(eo, ours_va, copy.deepcopy(c))
            except Exception as ex:
                b = {'exc': str(ex)[:100]}
            eo.log.clear()
            if a != b:
                diff = True
                break
        if diff:
            killed += 1
        else:
            alive.append(addr)
    print('mutants killed %d of %d' % (killed, len(jccs)))
    if alive:
        print('survivors (no case tells them from the original):', [hex(a) for a in alive])


if __name__ == '__main__':
    main()
