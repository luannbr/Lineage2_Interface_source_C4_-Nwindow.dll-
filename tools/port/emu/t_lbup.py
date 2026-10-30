# STALE (C4, sa-emu agent `old`): covered by win/t_equipdrop.py (NCEquipItemWnd::OnLButtonUp 0x10096370) and win/t_invdrop.py (NCInvenItemWnd::OnLButtonUp 0x100976C0); C2 model, does not run.
"""Differential test of the item windows' OnLButtonUp (NCEquipItemWnd 0x1007d3e0, NCInvenItemWnd 0x1007e790) against ours:
random windows, the window found under the mouse (inventory page, paperdoll, shortcut bar, pet window, inventory, console, none),
the console services stubbed and logged (requests with their parameter lists, ShowDialog with the dialog info dumped), the
window's fields and the cursor calls compared.
usage: python tools/port/emu/t_lbup.py equip|inven <cases> <seed> [dll map]   (dll/map: a snapshot of the build to test)"""
import sys, random
import os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import emu as emu_mod
from t_fill import *
import stubs

if len(sys.argv) > 5:
    emu_mod.OURS = sys.argv[4]
    emu_mod.OURS_MAP = sys.argv[5]

# class objects (stock address; ours: NWBASE + offset)
CLS = {'InvenItem': 0x101B9310, 'ShortCut': 0x101D95D8, 'PetInven': 0x101CE1B8, 'Inventory': 0x101B9840, 'Console': 0x101B1248,
       'Equip': 0x101B8DF0, 'Pet': 0x101CF650, 'RecipeBookItem': 0x101D7C18, 'Other': 0x10100000}

# stock address: (name, pop)
SVC = {
    0x100253C0: ('UnlockCursor', 0), 0x100253A0: ('SetCursor', 8), 0x100253B0: ('LockCursor', 0),
    0x1002B940: ('FindWindowAt', 8), 0x10060AC0: ('GetMyUser', 0), 0x100534C0: ('RqUnEquip', 4),
    0x10067080: ('RqShortCut', 4), 0x10055270: ('RqGivePet', 4), 0x10053440: ('RqUseItem', 4),
    0x100566F0: ('GetTargeted', 4), 0x1005B310: ('ShowDialog', 4), 0x100CCEF0: ('SlotAt', 8),
    0x100198B0: ('ItemHitTest', 8), 0x1007BAD0: ('EquipHitTest', 8), 0x10055280: ('RqGetPet', 4),
}
# the same in ours: (class substring, method prefix) of a compiled symbol, or None: the stub sits at NWBASE + offset
OURS_SVC = {
    'UnlockCursor': ('@NCObject@@', '?UnlockCursor@'), 'SetCursor': ('@NCObject@@', '?SetCursor@'),
    'LockCursor': ('@NCObject@@', '?LockCursor@'), 'FindWindowAt': ('@NCWnd@@', '?FindWindowAt@'),
    'GetMyUser': ('@NConsoleWnd@@', '?GetMyUser@'), 'RqUnEquip': ('@NConsoleWnd@@', '?RequestUnEquipItem@'),
    'RqShortCut': ('@NConsoleWnd@@', '?RequestShortCutReg@'), 'RqGivePet': ('@NConsoleWnd@@', '?RequestGiveItemToPet@'),
    'RqUseItem': ('@NConsoleWnd@@', '?RequestUseItem@'), 'GetTargeted': ('@NConsoleWnd@@', '?GetTargetedActor@'),
    'ShowDialog': ('@NConsoleWnd@@', '?ShowDialog@'), 'SlotAt': None,
    'ItemHitTest': ('@NCItemWnd@@', '?HitTest@'), 'EquipHitTest': ('@NCEquipItemWnd@@', '?HitTest@'),
    'RqGetPet': ('@NConsoleWnd@@', '?RequestGetItemFromPet@'),
}
for va, (nm, pop) in SVC.items():
    stubs.FN_POP[nm] = pop
stubs.FN_POP['IsCtrlDown'] = 0
stubs.FN_POP['RqEnchant'] = 4
stubs.FN_POP['GetEquipSlot'] = 8
stubs.FN_POP['IsInWindow'] = 8
stubs.IMPORT_POP['??0L2ParamStack@@QAE@H@Z'] = 4
stubs.IMPORT_POP['??1L2ParamStack@@QAE@XZ'] = 0
stubs.IMPORT_POP['?PushBack@L2ParamStack@@QAEHPAX@Z'] = 4
stubs.IMPORT_POP['?Insert@FArray@@QAEXHHH@Z'] = 12


class LbEmu(TipEmu):
    def __init__(self, kind):
        super().__init__(kind)
        self.params = {}
        self.case = None
        for i in range(1300):
            self.fs_set(self.gdata + 0x2DD34 + 0x24 * i + 4, 'MSG%d' % i)   # systemmsg texts, to tell the questions apart
        self.nextstub = STUB_BASE + 0x9400
        self.svc_stub = {}
        for va, (nm, pop) in SVC.items():
            st = self.mkstub(nm)
            code = b'\x68' + struct.pack('<I', st) + b'\xC3'
            if kind == 'stock':
                self.uc.mem_write(va, code)
            else:
                spec = OURS_SVC[nm]
                if spec is None:
                    self.uc.mem_write(NWBASE + (va - 0x10000000), code)
                else:
                    cands = [(k, v) for k, v in self.symbols.items() if k.startswith(spec[1]) and spec[0] in k]
                    if len(cands) != 1:
                        raise RuntimeError('ours symbol %s: %s' % (nm, cands))
                    self.uc.mem_write(cands[0][1], code)
        self.vt_console = self.alloc(0x1000)
        self.write32(self.vt_console + 0x448, self.mkstub('IsCtrlDown'))
        self.write32(self.vt_console + 0x510, self.mkstub('RqEnchant'))
        self.write32(self.console, self.vt_console)
        self.engine = self.alloc(0x400)
        self.write32(self.console + 0x44, self.engine)
        # the desktop's pointer is read by nothing here

    def mkstub(self, nm):
        st = self.nextstub
        self.nextstub += 16
        self.uc.mem_write(st, b'\xC3')
        self.stubs[st] = ('fn:' + nm, None)
        return st

    def cls(self, name):
        va = CLS[name]
        return va if self.kind == 'stock' else NWBASE + (va - 0x10000000)

    def dispatch(self, name):
        kind, nm = name.split(':', 1)
        if kind == 'imp' and nm in ('??0L2ParamStack@@QAE@H@Z', '??1L2ParamStack@@QAE@XZ', '?PushBack@L2ParamStack@@QAEHPAX@Z'):
            self.last_pop = stubs.IMPORT_POP[nm]
            t = self.this()
            if nm.startswith('??0'):
                self.params[t] = []
            elif nm.startswith('?Push'):
                self.params.setdefault(t, []).append(self.arg(0))
                return 1
            return t
        if kind == 'imp' and nm == '?Insert@FArray@@QAEXHHH@Z':
            self.last_pop = 12
            t = self.this()
            idx, cnt, size = self.arg(0), self.arg(1), self.arg(2)
            data, num, mx = self.read32(t), self.read32(t + 4), self.read32(t + 8)
            nd = self.alloc((num + cnt + 8) * size)
            if data:
                self.uc.mem_write(nd, bytes(self.uc.mem_read(data, idx * size)))
                self.uc.mem_write(nd + (idx + cnt) * size, bytes(self.uc.mem_read(data + idx * size, (num - idx) * size)))
            self.write32(t, nd); self.write32(t + 4, num + cnt); self.write32(t + 8, num + cnt + 8)
            return None
        return super().dispatch(name)

    def do_fn(self, nm):
        c = self.case
        a = self.arg
        log = self.log
        if nm == 'IsCtrlDown':
            log.append(('IsCtrlDown',))
            return c['ctrl']
        if nm == 'RqEnchant':
            log.append(('RqEnchant', self.rds32(self.reg(UC_X86_REG_ESP) + 4)))
            return None
        if nm in ('UnlockCursor', 'LockCursor'):
            log.append((nm,))
            return None
        if nm == 'SetCursor':
            log.append(('SetCursor', a(0), a(1)))
            return None
        if nm == 'FindWindowAt':
            log.append(('FindWindowAt', a(0), a(1)))
            return c['found_entry']
        if nm == 'GetMyUser':
            log.append(('GetMyUser',))
            return c['user']
        if nm in ('RqUnEquip', 'RqShortCut', 'RqGivePet', 'RqUseItem', 'RqGetPet'):
            if nm == 'RqUseItem':
                log.append((nm, a(0)))
            else:
                log.append((nm, list(self.params.get(a(0), []))))
            return None
        if nm == 'GetTargeted':
            log.append(('GetTargeted', a(0)))
            return c['target']
        if nm == 'SlotAt':
            log.append(('SlotAt', a(0), a(1)))
            return u32(c['slotat'])
        if nm == 'ItemHitTest':
            log.append(('ItemHitTest', a(0), a(1)))
            return u32(c['hit'])
        if nm == 'EquipHitTest':
            log.append(('EquipHitTest', a(0), a(1)))
            return u32(c['hit'])
        if nm == 'IsInWindow':
            log.append(('IsInWindow', a(0), a(1)))
            return c['inwin']
        if nm == 'GetEquipSlot':
            log.append(('GetEquipSlot', a(0), a(1)))
            return u32((a(0) * 7 + a(1)) % 16)
        if nm == 'ShowDialog':
            p = a(0)
            d = {'ints': [self.rds32(p + o) for o in (0, 4, 8, 0xC, 0x10, 0x14, 0x18, 0x58, 0x5C, 0x6C, 0x70, 0x74)]}
            d['strs'] = [self.fs_get(p + o) for o in (0x1C, 0x28, 0x34, 0x40, 0x4C, 0x60)]
            if d['ints'][0] == 4:
                d['loc'] = [self.read32(p + o) for o in (0x78, 0x7C, 0x80)]
            log.append(('ShowDialog', d))
            return None
        return super().do_fn(nm)


def build_map(e, data):
    """NDataMap at gdata + 0x27E4: {Pairs(data,num,max), Hash, HashCount}"""
    hc = 16
    pairs = e.alloc(12 * max(1, len(data)))
    hashp = e.alloc(4 * hc)
    for i in range(hc):
        e.write32(hashp + 4 * i, 0xFFFFFFFF)
    for i, (key, val) in enumerate(data):
        v = e.alloc(0x240)
        e.uc.mem_write(v, bytes(0x240))
        e.write32(v + 4, val['class'])
        e.write32(v + 0x64, val['crystal'])
        h = key & (hc - 1)
        e.write32(pairs + 12 * i, e.read32(hashp + 4 * h))
        e.write32(pairs + 12 * i + 4, key)
        e.write32(pairs + 12 * i + 8, v)
        e.write32(hashp + 4 * h, i)
    m = e.alloc(0x20)
    e.write32(m, pairs); e.write32(m + 4, len(data)); e.write32(m + 8, len(data))
    e.write32(m + 0xC, hashp); e.write32(m + 0x10, hc)
    e.write32(e.gdata + 0x27E4, m)


def run_case(e, c, mode):
    e.heap_ptr = e.heap_mark
    e.log.clear()
    e.params.clear()
    e.case = c
    build_map(e, c['table'])
    # items
    ptrs = []
    for it in c['items']:
        p = e.alloc(0x1B00)
        e.uc.mem_write(p, bytes(0x1B00))
        e.write_wstr_at(p + 0xC, it['name'])
        e.write_wstr_at(p + 0x20C, it['name2'])
        for off, v in it['ints'].items():
            e.write32(p + off, v)
        ptrs.append(p)
    arr = e.alloc(4 * 16)
    dummy = e.alloc(0x1B00)
    e.uc.mem_write(dummy, bytes(0x1B00))
    for i in range(16):
        e.write32(arr + 4 * i, ptrs[i] if i < len(ptrs) else dummy)   # (the pet branch indexes the items by slot, as the stock code does)
    entry = e.alloc(0x40)           # the window's own NWndEntry
    wnd = e.alloc(0x400)
    e.uc.mem_write(wnd, bytes(0x400))
    vt = e.alloc(0x400)
    e.write32(wnd, vt)
    e.write32(wnd + 0x58, entry)
    # the entry found under the mouse and its window
    found = 0
    target = 0
    if c['found'] is not None:
        found = e.alloc(0x20)
        e.uc.mem_write(found, bytes(0x20))
        target = e.alloc(0x400)
        e.uc.mem_write(target, bytes(0x400))
        cls = e.alloc(0x40)
        e.write32(target + 0x24, e.cls(c['found']))
        e.write32(found + 4, target)
        if c['found'] == 'Inventory':
            for off, v in zip((0x44, 0x48, 0xDC, 0xE0, 0xE4, 0xE8, 0xEC, 0xF0, 0xF4, 0xF8), c['invrect']):
                e.write32(target + off, v)
            e.write32(target + 0xD4, 1)
            e.write32(target + 0xD8, 1)
    c['found_entry'] = found
    # the player
    user = e.alloc(0x400)
    e.uc.mem_write(user, bytes(0x400))
    for off, v in zip((0x98, 0x9C, 0xA4, 0xA8), c['equipped']):
        e.write32(user + off, v)
    e.write32(user + 0x1D8, c['canc'])
    c['user'] = user
    # engine location
    for off, v in zip((0x200, 0x204, 0x208), c['loc']):
        e.write32(e.engine + off, v)
    if mode == 'equip':
        e.write32(wnd + 0xA0, arr); e.write32(wnd + 0xA4, len(ptrs)); e.write32(wnd + 0xA8, len(ptrs))
        e.write32(wnd + 0x11C, 14)
        e.write32(wnd + 0x124, c['selected'])
        e.write32(wnd + 0x104, c['dragging'])
        e.write32(wnd + 0x12C, c['enchant'])
        e.write32(vt + 0x10C, e.mkstub_cached('GetEquipSlot'))
        fn = c['fn']
        fields = [0x104, 0x124, 0x130, 0x134, 0x138]
    else:
        if mode == 'pet':
            e.write32(wnd + 0x118, 0)
        e.write32(wnd + 0xDC, arr); e.write32(wnd + 0xE0, len(ptrs)); e.write32(wnd + 0xE4, len(ptrs))
        e.write32(wnd + 0xE8, c['dragging'])
        e.write32(wnd + 0x108, c['selected'])
        e.write32(wnd + 0x110, c['enchant'])
        e.write32(wnd + (0x11C if mode == 'pet' else 0x114), c['dblclk'])
        e.write32(vt + 0x6C, e.mkstub_cached('IsInWindow'))
        e.write32(vt + 0x11C, e.unselect_va)
        fn = c['fn']
        fields = [0xE8, 0x108, 0x114, 0x118, 0x11C, 0x120, 0xE0]
        if mode == 'pet':
            fn = c['fn']
    lparam = (c['y'] << 16) | c['x']
    ret = e.call(fn, this=wnd, args=[0, lparam])
    out = [ret, [e.read32(wnd + f) for f in fields], list(e.log)]
    for i, p in enumerate(ptrs):
        out.append(e.read32(p + 0x1950))
    if target and c['found'] == 'Inventory':
        out.append((e.read32(target + 0xD4), e.read32(target + 0xD8)))
    # the window's array order (the page's move)
    if mode in ('inven', 'pet'):
        n = e.read32(wnd + 0xE0)
        d = e.read32(wnd + 0xDC)
        out.append([e.read32(d + 4 * i) for i in range(n)])
    return out


def main(mode, n, seed):
    r = random.Random(seed)
    cases = []
    founds = [None, 'InvenItem', 'ShortCut', 'PetInven', 'Inventory', 'Console', 'Equip', 'Pet', 'RecipeBookItem', 'Other']
    for _ in range(n):
        ni = r.randint(0, 5)
        items = []
        for i in range(ni):
            ints = {0x1914: 100 + i, 0x1918: r.choice([57, 57, 1, 2, 3, 40]), 0x1920: r.choice([0, 1, 2, 7, 300]),
                    0x1928: r.choice([0, 1, 2, 4, 6, 0x10, 0x20, 0x30, 0x100]), 0x1944: r.choice([0, 1, 1]),
                    0x1994: r.choice([0, 0, 1]), 0x190C: r.choice([0, 1, 5]), 0x1910: r.choice([-1, 0, 7]),
                    0x193C: r.choice([0, 0, 3]), 0x1950: r.choice([0, 1])}
            ints = {k: v & 0xFFFFFFFF for k, v in ints.items()}
            items.append({'name': r.choice(['', 'Sword', 'Zubei Boots', 'A' * 20]), 'name2': r.choice(['', '', 'Of Doom']), 'ints': ints})
        sel = r.randint(-1, max(ni, 1))
        if mode == 'equip' and ni and r.random() < 0.7:
            # a slot an item sits in (the GetEquipSlot stub: (BodyPart * 7 + ObjectID) % 16, 14 / 15 mean 6 / 5)
            it = r.choice(items)
            s = (it['ints'][0x1928] * 7 + it['ints'][0x1914]) % 16
            sel = 5 if s == 15 else (6 if s == 14 else s)
        if mode in ('inven', 'pet') and ni and r.random() < 0.8:
            sel = r.randint(0, ni - 1)
        equipped = [r.choice([0, 100, 101, 102, 103]) for _ in range(4)]
        table = [(k, {'class': r.choice([0, 1, 2, 2]), 'crystal': r.choice([0, 0, 5])}) for k in set(r.choice([57, 1, 2, 3, 40]) for _ in range(4))]
        c = {'items': items, 'selected': sel, 'dragging': r.choice([0, 1, 1, 1]), 'enchant': r.choice([0, 0, 0, 1]), 'ctrl': r.choice([0, 1]),
             'x': r.randint(0, 400), 'y': r.randint(0, 300), 'found': r.choice(founds), 'slotat': r.choice([-1, 0, 3]),
             'target': r.choice([0, 0, 1]), 'equipped': equipped, 'canc': r.choice([0, 1]), 'table': table,
             'loc': [r.randint(0, 1 << 30) for _ in range(3)], 'dblclk': r.choice([0, 0, 0, 1]), 'inwin': r.choice([0, 0, 1]),
             'hit': r.randint(-1, 4)}
        # the inventory window's rects: screen x/y, trash x/y/w/h, crystallize x/y/w/h: make the point fall in them sometimes
        px, py = c['x'], c['y']
        sx, sy = r.randint(0, 200), r.randint(0, 100)
        tx, ty = (px - sx - r.randint(0, 20)) if r.random() < 0.4 else 190, (py - sy - r.randint(0, 20)) if r.random() < 0.4 else 315
        cx, cy = (px - sx - r.randint(0, 20)) if r.random() < 0.3 else 10, (py - sy - r.randint(0, 20)) if r.random() < 0.3 else 315
        c['invrect'] = [sx, sy, tx, ty, 32, 32, cx, cy, 32, 32]
        cases.append(c)
    res = {}
    for kind in ('stock', 'ours'):
        e = LbEmu(kind)
        e.heap_mark = e.heap_ptr
        _d = {}
        def cached(nm, _e=e, _d=_d):
            if nm not in _d:
                _d[nm] = _e.mkstub(nm)
            return _d[nm]
        e.mkstub_cached = cached
        if kind == 'stock':
            fn = {'equip': 0x1007D3E0, 'inven': 0x1007E790, 'pet': 0x100B5C10}[mode]
            e.unselect_va = 0x1001A240
        else:
            cls = {'equip': '@NCEquipItemWnd@@', 'inven': '@NCInvenItemWnd@@', 'pet': '@NCPetInvenItemWnd@@'}[mode]
            fn = [v for s, v in e.symbols.items() if s.startswith('?OnLButtonUp@') and cls in s][0]
            e.unselect_va = [v for s, v in e.symbols.items() if s.startswith('?UnselectAll@NCItemWnd@@')][0]
        outs = []
        for c in cases:
            c['fn'] = fn
            outs.append(run_case(e, c, mode))
        res[kind] = outs
    bad = 0
    for i, (a, b) in enumerate(zip(res['stock'], res['ours'])):
        if a != b:
            bad += 1
            if bad <= 3:
                c = cases[i]
                print('case', i, {k: c[k] for k in ('selected', 'dragging', 'enchant', 'ctrl', 'found', 'x', 'y', 'inwin', 'hit')})
                print(' stock', str(a)[:1200])
                print(' ours ', str(b)[:1200])
    print(mode, 'lbup cases', n, 'differ', bad)
    # coverage: which services were hit
    cov = {}
    for o in res['stock']:
        for l in o[2]:
            cov[l[0]] = cov.get(l[0], 0) + 1
    print('stock coverage', cov)


if __name__ == '__main__':
    main(sys.argv[1], int(sys.argv[2]), int(sys.argv[3]))
