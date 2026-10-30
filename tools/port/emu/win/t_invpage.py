# Differential tests of the other hooked functions of the C4 inventory page, NCInvenItemWnd (stock against ours under unicorn): the mouse
# handlers (OnLButtonDown 0x10094E40, OnRButtonDown 0x10094BE0, OnLButtonDblClick 0x10094ED0, OnMouseMove 0x10095430), WindowProc 0x10093CE0,
# OnPaint 0x10095210, UpdateItem 0x100950E0, GetAdena 0x100951E0, AddItem 0x10098170 and RemoveItem 0x100989C0. Same fake world as t_invdrop.py
# (every callee is a logging service, the window is a memory block with the stock layout, the dialog info is dumped when ShowDialog gets it);
# the page's items, the parent inventory window and the device are blocks of memory compared after the call. It found the C3 test of the
# console in UseItem (NConsoleLeftFns::IsDwarf 0x100753D0 where the stock tests nothing).
#   python t_invpage.py [cases] [seed] [lbdown,rbdown,dbl,mmove,wproc,paint,update,adena,add,remove]
import sys, os, random, struct, copy
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import t_invdrop as D
from t_invdrop import *
import gw

gw.VPOP.update({27: 8, 28: 0, 43: 0x18, 79: 0, 81: 8})

FUNCS = {'lbdown': 0x10094E40, 'rbdown': 0x10094BE0, 'dbl': 0x10094ED0, 'mmove': 0x10095430, 'wproc': 0x10093CE0, 'paint': 0x10095210,
         'update': 0x100950E0, 'adena': 0x100951E0, 'add': 0x10098170, 'remove': 0x100989C0}
EXTRA = {
    0x1001E580: ('ItemSelect', 0, 0), 0x10074E60: ('PlaySound', 4, 0), 0x1001E5D0: ('GetUnknown19AC', 0, 0), 0x1006ABC0: ('GetMacroCount', 0, 0),
    0x10074060: ('GetMacro', 4, 0), 0x100A76C0: ('MacroReset', 0, 0), 0x1002B450: ('FindTexture', 8, 0), 0x1001F960: ('ItemCtor', 4, 1),
    0x10030770: ('ToolTipFill', 0xC, 0), 0x1001FE00: ('ItemCopy', 4, 0), 0x1001EA60: ('UpdateScrollRange', 0, 0), 0x10050030: ('ItemDtor', 0, 0),
    0x10013330: ('SetClip', 0x10, 0), 0x10013340: ('ReleaseClip', 0, 0), 0x10012D90: ('DrawTexture', 0x2C, 0), 0x1016EC20: ('GetFirstDword', 0, 0),
    0x1002B570: ('SetCursor', 8, 0), 0x1002B580: ('LockCursor', 0, 0), 0x10014B90: ('BaseWndProc', 0xC, 0),
}
CAL = dict(D.CALLEES)
CAL.update(EXTRA)
OURS = {0x1002B450: '?FindTexture@NCObject@@QAEPAVUTexture@@PBGH@Z', 0x1001F960: '??0NItemEntry@@QAE@PAUNItemInfo@@@Z',
        0x10012D90: '?DrawTexture@NCGDevice@@QAEXHHHHHHHHPAVUTexture@@KH@Z', 0x10013330: '?SetClip@NCGDevice@@QAEXHHHH@Z',
        0x10013340: '?ReleaseClip@NCGDevice@@QAEXXZ'}


class PW(IW):
    def __init__(self, kind):
        IW.__init__(self, kind, CAL, OURS)
        self.rets['FindTexture'] = self.find_texture
        self.vrets[28] = lambda e, t, a: e.world['focus']
        self.vrets[43] = self.ptinrect
        self.vrets[81] = lambda e, t, a: e.world['dblret']

    def find_texture(self, e, t, a):
        name = e.wstr(a[0]) or ''
        if e.case['notex'] and name != 'NWindow.BlackTexture':
            return 0
        return 0x7777 + len(name)

    def dispatch(self, name):
        if name == 'mf:malloc' and self.case.get('nomalloc'):
            self.last_pop = 8
            size = self.sargs(1)[0]
            self.log.append(('GMalloc.Malloc', size, 'NULL'))
            return 0
        return IW.dispatch(self, name)

    def ptinrect(self, e, t, a):
        p = e.world['ptin']
        return p.pop(0) if p else 0

    def service(self, nm, t, a):
        log = self.log
        w = self.world
        if nm == 'ItemSelect':
            log.append((nm, self.nm(t)))
            self.write32(t + 0x1B5C, 1)
            return None
        if nm == 'PlaySound':
            log.append((nm, self.nm(t), a[0]))
            return None
        if nm == 'GetUnknown19AC':
            log.append((nm, self.nm(t)))
            return self.rds32(t + 0x1BB8) & 0xFFFFFFFF
        if nm == 'GetMacroCount':
            log.append((nm, self.nm(t)))
            return w['nmacro']
        if nm == 'GetMacro':
            log.append((nm, self.nm(t), a[0]))
            m = w['macros']
            return m[a[0]] if 0 <= a[0] < len(m) else 0
        if nm == 'MacroReset':
            log.append((nm, self.nm(t)))
            return None
        if nm == 'ItemCtor':
            log.append((nm, self.nm(t), self.nm(a[0])))
            return t
        if nm == 'ToolTipFill':
            log.append((nm, t - a[0], a[1], a[2]))
            return None
        if nm == 'ItemCopy':
            log.append((nm, self.nm(t), self.nm(a[0])))
            return None
        if nm == 'UpdateScrollRange':
            log.append((nm, self.nm(t)))
            return None
        if nm == 'ItemDtor':
            log.append((nm, self.nm(t)))
            return None
        if nm in ('SetClip', 'DrawTexture'):
            log.append((nm, [self.nm(x) for x in a]))
            return None
        if nm == 'ReleaseClip':
            log.append((nm,))
            return None
        if nm == 'GetFirstDword':
            return self.read32(t)
        if nm == 'SetCursor':
            log.append((nm, a[0], self.nm(a[1])))
            return None
        if nm == 'LockCursor':
            log.append((nm,))
            return None
        if nm == 'BaseWndProc':
            log.append((nm, a))
            return w['basewp']
        return IW.service(self, nm, t, a)


def f32(x):
    return struct.unpack('<I', struct.pack('<f', float(x)))[0]


def run_page(e, A, c):
    mark = len(e.log)
    e.case = c
    e.params.clear()
    f = c['f']
    for k in CLS:
        e.uc.mem_write(e.cls(k), bytes(0x60))
    ptrs = [put_item(e, it, k) for k, it in enumerate(c['items'])]
    arr = e.alloc(4 * 32)
    for i, p in enumerate(ptrs):
        e.write32(arr + 4 * i, p)
    if c['nullitem'] is not None and 0 <= c['nullitem'] < len(ptrs):
        e.write32(arr + 4 * c['nullitem'], 0)
    w, ent = make_window(e, 'wnd', 0, 0x300)
    e.write32(w, e.vt)
    e.write32(w + 0x12C, arr)
    e.write32(w + 0x130, len(ptrs))
    e.write32(w + 0x134, len(ptrs))
    e.write32(w + 0x138, c['dragging'])
    e.write32(w + 0x13C, c['nodrag'])
    e.write32(w + 0x140, c['cols'])
    e.write32(w + 0x144, c['vrows'])
    e.write32(w + 0x148, c['toprow'])
    e.write32(w + 0x150, c['maxitems'])
    e.write32(w + 0x154, c['cell'])
    e.write32(w + 0x124, 0x5E1EC7)
    e.write32(w + 0x178, 0x0D0A)
    e.write32(w + 0x17C, 0x0D0B)
    e.write32(w + 0x160, c['sel'] & 0xFFFFFFFF)
    e.write32(w + 0x164, 0x77777777)
    e.write32(w + 0x168, c['dbl'])
    e.write32(w + 0x44, c['sx'])
    e.write32(w + 0x48, c['sy'])
    e.write32(w + 0x4C, f32(c['width']))
    e.write32(w + 0x50, f32(c['height']))
    for i, v in enumerate((0x11111111, 0x22222222, 0x33333333)):
        e.write32(w + 0x16C + 4 * i, v)
    e.objs[0x5E1EC7] = 'selecttex'
    e.objs[0x0D0A] = 'outlinedown'
    e.objs[0x0D0B] = 'outline'
    world = dict(focus=c['focus'], ptin=list(c['ptin']), dblret=c['dblret'], basewp=c['basewp'], nmacro=c['nmacro'], macros=[])
    # the parent (OnMouseMove): the inventory window
    iw = 0
    if f == 'mmove':
        iw, pent = make_window(e, 'inv', 0)
        e.write32(ent + 0xC, pent)
        par = c['parent']
        if par == 'none':
            e.write32(ent + 0xC, 0)
        elif par == 'nownd':
            e.write32(pent + 4, 0)
        elif par in ('inv', 'invderived'):
            e.write32(iw + 0x24, make_class(e, 'Inventory', 1 if par == 'invderived' else 0))
        elif par == 'other':
            e.write32(iw + 0x24, make_class(e, 'Other', 0))
        else:
            e.write32(iw + 0x24, 0)
        for o, v in zip((0x44, 0x48, 0x120, 0x124, 0x128, 0x12C, 0x130, 0x134, 0x138, 0x13C, 0x140, 0x144),
                        (c['isx'], c['isy'], 5, 6, 208, 351, f32(34.0), f32(35.0), 14, 351, f32(34.0), f32(33.0))):
            e.write32(iw + o, v)
        tb, _ = make_window(e, 'trashbtn', 0)
        rb, _ = make_window(e, 'recipebtn', 0)
        e.write32(iw + 0x148, tb)
        e.write32(iw + 0x14C, rb)
    for k in range(c['nmacro']):
        m = e.alloc(0xA40)
        e.uc.mem_write(m, bytes(0xA40))
        e.write32(m + 0xA34, c['running'][k % len(c['running'])])
        e.objs[m] = 'macro%d' % k
        world['macros'].append(m)
    if c['nullmacro'] is not None and c['nullmacro'] < len(world['macros']):
        world['macros'][c['nullmacro']] = 0
    e.world = world
    args = []
    dev = info = 0
    if f in ('lbdown', 'rbdown', 'dbl', 'mmove'):
        args = [0, (c['y'] << 16) | c['x']]
        e.world['hit'] = c['hit']
    elif f == 'wproc':
        args = [c['msg'], 0x1234, (c['y'] << 16) | c['x']]
    elif f == 'paint':
        dev = e.alloc(0x80)
        e.uc.mem_write(dev, bytes([0x5A]) * 0x80)
        e.objs[dev] = 'device'
        args = [dev]
    elif f in ('update', 'add'):
        info = e.alloc(0x19B4)
        e.uc.mem_write(info, bytes(0x19B4))
        e.write_wstr_at(info + 0x400, c['iconname'])
        e.write32(info + 0x1900, c['infotex'])
        e.write32(info + 0x191C, c['info_objid'])
        e.objs[info] = 'info'
        args = [info]
    elif f == 'remove':
        args = [c['rm_objid']]
    c['hit'] = c.get('hit', 0)
    ret = e.call(A, this=w, args=args)
    out = dict(ret=ret, wnd=e.snapshot(w, 0x180),
               order=[e.nm(e.read32(e.read32(w + 0x12C) + 4 * i)) for i in range(e.read32(w + 0x130))],
               sel=[e.read32(p + 0x1B5C) for p in ptrs], log=e.log[mark:])
    if dev:
        out['dev'] = [e.rds32(dev + o) for o in (0x38, 0x3C, 0x54, 0x58, 0x6C)] + [bytes(e.uc.mem_read(dev, 0x80)).hex()]
    if info:
        out['info'] = [e.read32(info + 0x1900), e.read32(info + 0x191C)]
    if iw:
        out['inv'] = e.snapshot(iw, 0x160)
    return out


def rand_case(r, f):
    n = r.choice([0, 1, 2, 3, 4, 6, 9, 12])
    items = []
    for i in range(n):
        items.append(dict(name=r.choice(['', 'Sword', 'Zubei Boots']), display='', objid=r.randint(100, 10 ** 6) if r.random() < 0.9 else 77,
                          id=r.choice([57, 57, 57, 1, 2, 40]), count=r.choice([0, 1, 2, 7, 300, -1, 100000]), stk=r.choice([0, 1, 2, 3, 4]),
                          icon=r.choice([0, 0x7100 + i, 0x7200 + i]), selected=r.choice([0, 0, 1]), warn=r.choice([0, 0, 1]),
                          msg=r.choice([0, 0, 5, 0x1234, -1])))
    c = dict(f=f, items=items)
    c['sel'] = r.randint(0, n - 1) if (n and r.random() < 0.8) else r.randint(-2, n + 1)
    c['dragging'] = r.choice([0, 1, 1])
    c['nodrag'] = r.choice([0, 0, 1])
    c['dbl'] = r.choice([0, 0, 1])
    c['nullitem'] = r.randint(0, n - 1) if (n and r.random() < 0.08 and f in ('paint', 'adena')) else None
    c['cols'] = r.choice([1, 3, 6, 6, 8])
    c['vrows'] = r.choice([1, 2, 4, 5])
    c['toprow'] = r.choice([0, 0, 1, 2])
    c['maxitems'] = r.choice([3, 6, 12, 160])
    c['cell'] = r.choice([32, 32, 30])
    c['sx'] = r.randint(0, 500)
    c['sy'] = r.randint(0, 400)
    c['width'] = r.choice([210.0, 211.5, 192.0, 0.0])
    c['height'] = r.choice([140.0, 131.25, 0.0])
    c['x'] = r.choice([0, 1, 100, 400, 0xFFFF, r.randint(0, 800)])
    c['y'] = r.choice([0, 1, 50, 300, 0xFFFF, r.randint(0, 600)])
    c['hit'] = r.choice([-1, 0, 1, 2, c['sel'], n - 1, n, n + 2])
    c['focus'] = r.choice([0, 1, 1, 1])
    c['ptin'] = [r.choice([0, 1]), r.choice([0, 1])]
    c['dblret'] = r.choice([0, 1, 5, -1 & 0xFFFFFFFF])
    c['basewp'] = r.choice([0, 0, 1, 7])
    c['msg'] = r.choice([0x203, 0x201, 0x202, 0x200, 0x0F, 0x100, 0x204])
    c['nmacro'] = r.choice([0, 1, 2, 4])
    c['running'] = [r.choice([0, 1, 1]) for _ in range(3)]
    c['nullmacro'] = r.choice([None, None, 0, 1])
    c['parent'] = r.choice(['inv', 'inv', 'invderived', 'none', 'nownd', 'other', 'nullclass'])
    c['isx'] = r.randint(0, 300)
    c['isy'] = r.randint(0, 200)
    c['notex'] = r.choice([0, 0, 0, 1])
    c['nomalloc'] = r.choice([0] * 7 + [1])
    c['iconname'] = r.choice(['', 'icon.Sword', 'L2UI_CH3.item.weapon_a'])
    c['infotex'] = r.choice([0, 0x8100, 0x8200])
    ids = [it['objid'] for it in items]
    c['info_objid'] = r.choice(ids) if ids and r.random() < 0.7 else r.randint(100, 10 ** 6)
    c['rm_objid'] = r.choice(ids) if ids and r.random() < 0.7 else r.randint(100, 10 ** 6)
    return c


SIZES = {'lbdown': 143, 'rbdown': 595, 'dbl': 519, 'mmove': 506, 'wproc': 84, 'paint': 494, 'update': 247, 'adena': 48, 'add': 252, 'remove': 126}


def coverage(which, n, seed):
    import pefile
    from capstone import Cs, CS_ARCH_X86, CS_MODE_32
    from unicorn import UC_HOOK_CODE
    st = pefile.PE(os.path.join(ROOT, 'orig', 'NWindow.dll'))
    base = st.OPTIONAL_HEADER.ImageBase
    mem = st.get_memory_mapped_image()
    md = Cs(CS_ARCH_X86, CS_MODE_32)
    r = random.Random(seed)
    for f in which:
        va = FUNCS[f]
        es = PW('stock')
        es.heap_mark = es.heap_ptr
        hit = set()
        es.uc.hook_add(UC_HOOK_CODE, lambda uc, a, s_, u: hit.add(a), None, va, va + SIZES[f])
        for _ in range(n):
            es.heap_ptr = es.heap_mark
            try:
                run_page(es, va, rand_case(r, f))
            except Exception:
                pass
            es.log.clear()
        allins = [i.address for i in md.disasm(bytes(mem[va - base:va - base + SIZES[f]]), va)]
        miss = [a for a in allins if a not in hit]
        print('%-8s stock instructions %d covered %d missed %d %s' % (f, len(allins), len(allins) - len(miss), len(miss), [hex(a) for a in miss[:20]]))


def main():
    if len(sys.argv) > 1 and sys.argv[1] == 'cov':
        coverage(sys.argv[4].split(',') if len(sys.argv) > 4 else list(FUNCS), int(sys.argv[2]) if len(sys.argv) > 2 else 300,
                 int(sys.argv[3]) if len(sys.argv) > 3 else 1)
        return
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 300
    seed = int(sys.argv[2]) if len(sys.argv) > 2 else 1
    which = sys.argv[3].split(',') if len(sys.argv) > 3 else list(FUNCS)
    es = PW('stock')
    eo = PW('ours')
    lm = log_map()
    es.heap_mark, eo.heap_mark = es.heap_ptr, eo.heap_ptr
    r = random.Random(seed)
    total = 0
    for f in which:
        va = FUNCS[f]
        if va not in lm:
            print('%-8s: not hooked in our build, skipped' % f)
            continue
        cases = [rand_case(r, f) for _ in range(n)]
        total += compare(f, es, eo, run_page, cases, va, lm[va])
    print('TOTAL problems', total)
    sys.exit(1 if total else 0)


if __name__ == '__main__':
    main()
