# Differential test (stock vs ours under unicorn) of the item grid NCItemWnd (src/nwindow/NCItemWnd.cpp), wave C lane debt2: HitTest
# 0x1001E790, GetSelected 0x1001E830, GetItemCount 0x1001E850, OnMouseWheel 0x1001E8B0, InitScrollBar 0x1001EA20, UpdateScrollRange
# 0x1001EA60, OnScrollBar 0x1001EAE0, ScrollUp 0x1001EB00, ScrollDown 0x1001EB40, OnLButtonDown 0x1001EFB0, OnMouseMove 0x1001F0C0,
# UnselectAll 0x1001F130, Clear 0x1001FD80, OnPaint 0x1001EDF0, OnCreate 0x1001ED90, OnDestroy 0x1001E710, ShowWindow 0x1001EAD0,
# HideWindow 0x1001E980 and NCShopItemWnd::OnPaint 0x10102AA0 (the same fields plus Mode +0x16C / Side +0x170 on a 0x174 grid).
# The grid is 0x168 bytes (scroll bar +0x114, Items +0x12C, bDragging +0x138, bNoDrag +0x13C,
# Columns +0x140, VisibleRows +0x144, TopRow +0x148, NumRows +0x14C, MaxItems +0x150, CellSize +0x154, gaps +0x158 / +0x15C, Selected
# +0x160, HoverIndex +0x164; ScreenX / ScreenY +0x44 / +0x48); entries are 0x1C40-byte records (icon +0, bSelected +0x1B5C). The slider's
# SetValue / SetRange, SetRepeatDelay, the message box's Reset, the interface sound, the cursor calls and ~NItemEntry are logging stubs on
# both sides, and so are the device (SetClip, ReleaseClip, DrawTexture, DrawText), NItemEntry::IsSelected, the base class's ShowWindow / HideWindow /
# OnCreate / OnDestroy, FindTexture and GetOwner; the desktop (console +0x3BF4) holds the two message boxes at +0x240 / +0x244 (HideWindow
# also runs with them or the desktop missing). Every byte of the grid, of the entries and of the device block is compared.
#   python tools/port/emu/win/t_itemwnd.py [cases] [seed] [names]   (needs the built DLL + .map and the smoke log)
import sys, os, random, struct
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from gw import *
import gw
import stubs

gw.VPOP.update({79: 0, 28: 0, 49: 0, 29: 0, 78: 0})
GRID = 0x174   # the shop grid's size (the item grid is 0x168: the last dword is never touched by the functions here)
ENTRY = 0x1C40
F = dict(Bar=0x114, Data=0x12C, Num=0x130, Max=0x134, Drag=0x138, NoDrag=0x13C, Cols=0x140, Rows=0x144, Top=0x148, NumRows=0x14C,
         MaxItems=0x150, Cell=0x154, GapX=0x158, GapY=0x15C, Sel=0x160, Hover=0x164, ScreenX=0x44, ScreenY=0x48, Mode=0x16C, Side=0x170, Sel_Tex=0x124, W=0x4C, H=0x50)
SVC = {0x1002C150: ('SliderSetValue', 4, 0), 0x1002C0A0: ('SliderSetRange', 8, 0), 0x1002B850: ('SetRepeatDelay', 4, 0),
       0x10007CC0: ('DlgReset', 0, 0), 0x10074E60: ('PlaySound', 4, 0), 0x1002B570: ('SetCursor', 8, 0), 0x1002B580: ('LockCursor', 0, 0),
       0x10050030: ('EntryDtor', 0, 0), 0x10013330: ('SetClip', 0x10, 0), 0x10013340: ('ReleaseClip', 0, 0), 0x10012D90: ('DrawTexture', 0x2C, 0),
       0x10012AA0: ('DrawText', 0x3C, 0), 0x1001E590: ('IsSelected', 0, 0),
       0x100EFC40: ('WndShow', 0, 0), 0x10035AB0: ('WndHide', 0, 0), 0x1002BD60: ('ScrollOnCreate', 0, 0), 0x1002B880: ('ScrollOnDestroy', 0, 0),
       0x1002B450: ('FindTexture', 8, 0), 0x10008320: ('GetOwner', 0, 0)}
SYMS = {0x1002C150: '?SetValue@NCSliderBar@@QAEXH@Z', 0x1002C0A0: '?SetRange@NCSliderBar@@QAEXHH@Z',
        0x1002B850: '?SetRepeatDelay@NCScrollWnd@@QAEXH@Z', 0x10007CC0: '?Reset@NCDialogBox@@QAEXXZ',
        0x10074E60: '?PlayInterfaceSound@NConsoleWnd@@QAEXH@Z', 0x1002B570: '?SetCursor@NCObject@@QAEXHPAVUTexture@@@Z',
        0x1002B580: '?LockCursor@NCObject@@QAEXXZ', 0x10050030: '??1NItemEntry@@QAE@XZ',
        0x10013330: '?SetClip@NCGDevice@@QAEXHHHH@Z', 0x10013340: '?ReleaseClip@NCGDevice@@QAEXXZ',
        0x10012D90: '?DrawTexture@NCGDevice@@QAEXHHHHHHHHPAVUTexture@@KH@Z',
        0x10012AA0: '?DrawText@NCGDevice@@QAEKHHKPBGKKKW4L2FontType@@HHKHHHH@Z',
        0x100EFC40: '?ShowWindow@NCWnd@@UAEXXZ', 0x10035AB0: '?HideWindow@NCWnd@@UAEXXZ', 0x1002BD60: '?OnCreate@NCScrollWnd@@UAEHXZ',
        0x1002B880: '?OnDestroy@NCScrollWnd@@UAEHXZ', 0x1002B450: '?FindTexture@NCObject@@QAEPAVUTexture@@PBGH@Z',
        0x10008320: '?GetOwner@NCDialogBox@@QAEPAUNWndEntry@@XZ'}
SYMS.pop(0x1001E590, None)   # (ours has no compiled IsSelected to patch: the stub jumps to the stock address, patched by the service table)
# name: (stock va, argument kinds)
FUNCS = {
    'hit': (0x1001E790, ('x', 'y', 'ox', 'oy')), 'getsel': (0x1001E830, ()), 'getcount': (0x1001E850, ()),
    'wheel': (0x1001E8B0, ('wp', 'lp')), 'initbar': (0x1001EA20, ()), 'updaterange': (0x1001EA60, ()), 'onscrollbar': (0x1001EAE0, ('val',)),
    'scrollup': (0x1001EB00, ()), 'scrolldown': (0x1001EB40, ()), 'ldown': (0x1001EFB0, ('wp', 'lp')), 'mmove': (0x1001F0C0, ('wp', 'lp')),
    'unselall': (0x1001F130, ()), 'clear': (0x1001FD80, ()),
    'paint': (0x1001EDF0, ('dev',)), 'shoppaint': (0x10102AA0, ('dev',)),
    'oncreate': (0x1001ED90, ()), 'ondestroy': (0x1001E710, ()), 'show': (0x1001EAD0, ()), 'hide': (0x1001E980, ()),
}


class IW(GW):
    def __init__(self, kind):
        have = None
        if kind == 'ours':
            import emu as _e
            have = set()
            for line in open(_e.OURS_MAP, errors='replace'):
                p = line.split()
                if len(p) > 2:
                    have.add(p[1])
        GW.__init__(self, kind, SVC, {va: s for va, s in SYMS.items() if have is None or s in have})
        self.vrets[28] = lambda e, t, a: self.focused.get(t, 0)
        self.focused = {}
        # NItemEntry::IsSelected is a stock stub in ours (`jmp` to the stock address): a service on both sides
        self.rets['IsSelected'] = lambda e, t, a: e.read32(t + 0x1B5C)
        self.owners = {}
        self.rets['GetOwner'] = lambda e, t, a: self.owners.get(t, 0)
        self.rets['FindTexture'] = lambda e, t, a: 0x7000 + len(e.wstr(a[0])) if a[0] else 0

    def dispatch(self, name):
        kind, nm = name.split(':', 1)
        if kind == 'imp' and nm == 'IsProcessorFeaturePresent':   # the CRT's float-to-int helper asks in ours
            self.last_pop = 4
            return 0
        return GW.dispatch(self, name)


def grid(e, r, c):
    g = e.window('grid', size=GRID)
    n = r.choice([0, 1, 3, 7, 12, 25])
    mx = n + r.choice([0, 2, 5])
    data = e.alloc(max(mx, 1) * 4 + 16)
    ents = []
    for i in range(n):
        p = e.alloc(ENTRY + 16)
        b = bytearray(r.getrandbits(8) for _ in range(ENTRY))
        b[0:4] = struct.pack('<I', r.choice([0, 0x7000 + i]))
        b[0x1B5C:0x1B60] = struct.pack('<I', r.choice([0, 1]))
        b[0x1B28:0x1B2C] = struct.pack('<i', r.choice([-1, 0, 1, 7, 999, 100000]))
        e.uc.mem_write(p, bytes(b))
        e.objs[p] = 'item%d' % i
        e.write32(data + 4 * i, p if not (c.get('holes') and r.random() < 0.15) else 0)
        ents.append(p)
    e.write32(g + F['Data'], data if mx else 0)
    e.write32(g + F['Num'], n)
    e.write32(g + F['Max'], mx)
    cols = r.randint(1, 6)
    rows = r.randint(1, 6)
    e.write32(g + F['Cols'], cols)
    e.write32(g + F['Rows'], rows)
    e.write32(g + F['Top'], r.choice([0, 0, 1, 2, 5]))
    e.write32(g + F['NumRows'], r.choice([0, 1, rows, rows + 1, rows + 4, 20]))
    e.write32(g + F['MaxItems'], r.choice([25, 40]))
    e.write32(g + F['Cell'], r.choice([16, 32, 40]))
    e.write32(g + F['GapX'], r.choice([0, 3, 5]))
    e.write32(g + F['GapY'], r.choice([0, 3, 5]))
    e.write32(g + F['Sel'], r.choice([-1, -1, 0, n // 2, n, n + 3]) & 0xFFFFFFFF)
    e.write32(g + F['Hover'], r.choice([-1, 0, n // 2]) & 0xFFFFFFFF)
    e.write32(g + F['Drag'], r.choice([0, 1]))
    e.write32(g + F['NoDrag'], r.choice([0, 1]))
    e.write32(g + F['ScreenX'], r.choice([0, 10, 100, 633]))
    e.write32(g + F['ScreenY'], r.choice([0, 20, 200, 479]))
    e.write32(g + F['Mode'], r.choice([0, 1, 2, 3]))
    e.write32(g + F['Side'], r.choice([0, 1, 2]))
    e.write32(g + F['Sel_Tex'], 0x7100 + r.getrandbits(8))
    e.uc.mem_write(g + F['W'], struct.pack('<ff', r.choice([0.0, 150.0, 300.5, 1000.0]), r.choice([0.0, 100.0, 333.25, 700.0])))
    bar = e.window('bar', size=0x100)
    e.write32(g + F['Bar'], bar)
    return g, data, ents


def run(e, A, c):
    mark = len(e.log)
    r = random.Random(c['seed'])
    e.heap_ptr = e.heap_mark
    e.shown.clear()
    e.focused.clear()
    c = dict(c, holes=c['f'] in ('paint', 'shoppaint'))
    g, data, ents = grid(e, r, c)
    con = e.console
    e.uc.mem_write(con, bytes(0x4000))
    desk = e.alloc(0x800)
    e.uc.mem_write(desk, bytes(0x800))
    e.write32(con + 0x3BF4, desk)
    box = e.window('mainbox', size=0x200)
    # missing boxes and desktop only where the stock code looks before it uses them (HideWindow); the mouse handlers use them blindly
    nulls = c['f'] == 'hide'
    e.write32(desk + 0x240, 0 if (r.random() < 0.15 and nulls) else box)
    box2 = e.window('secondbox', size=0x200)
    e.write32(desk + 0x244, 0 if (r.random() < 0.15 and nulls) else box2)
    e.shown[box2] = r.choice([0, 1])
    own = e.read32(g + 0x74)
    e.owners = {box: r.choice([own, own, 0x1234]), box2: r.choice([own, own, 0x4321])}
    if r.random() < 0.1 and nulls:
        e.write32(con + 0x3BF4, 0)    # no desktop
    e.shown[box] = r.choice([0, 1])
    e.focused[g] = r.choice([0, 1])
    va, kinds = A[c['f']]
    args = []
    devs = []
    sx, sy = e.read32(g + F['ScreenX']), e.read32(g + F['ScreenY'])
    cols, rows = e.read32(g + F['Cols']), e.read32(g + F['Rows'])
    cw = e.read32(g + F['Cell']) + e.read32(g + F['GapX'])
    ch = e.read32(g + F['Cell']) + e.read32(g + F['GapY'])

    def inside_x():   # mostly inside the grid, the edges and just outside
        return sx + r.choice([-1, 0, 1, cw // 2, cols * cw - 1, cols * cw, cols * cw + 1, r.randint(0, cols * cw)])

    def inside_y():
        return sy + r.choice([-1, 0, 1, ch // 2, rows * ch - 1, rows * ch, rows * ch + 1, r.randint(0, rows * ch)])
    for k in kinds:
        if k == 'x':
            args.append((inside_x() if r.random() < 0.8 else r.choice([-5, 700])) & 0xFFFFFFFF)
        elif k == 'y':
            args.append((inside_y() if r.random() < 0.8 else r.choice([-5, 500])) & 0xFFFFFFFF)
        elif k in ('ox', 'oy'):
            args.append(r.choice([0, 0, 7, -3]) & 0xFFFFFFFF)
        elif k == 'wp':
            args.append((r.choice([-240, -120, 0, 120, 240, 360, 100]) & 0xFFFF) << 16 | r.choice([0, 8]))
        elif k == 'lp':
            args.append((inside_x() & 0xFFFF) | (inside_y() & 0xFFFF) << 16)
        elif k == 'dev':
            dev = e.alloc(0x200)
            e.uc.mem_write(dev, bytes(r.getrandbits(8) for _ in range(0x200)))
            e.objs[dev] = 'device'
            devs.append(dev)
            args.append(dev)
        elif k == 'val':
            args.append(r.choice([0, 1, 3, 9]))
    ret = e.call(va, this=g, args=args)
    n = e.read32(g + F['Num'])
    d = e.read32(g + F['Data'])
    objs = [(g, GRID), (box, 0x40), (box2, 0x40)] + [(p, ENTRY) for p in ents] + [(dv, 0x200) for dv in devs]
    if d and 0 < n < 64:
        objs.append((d, 4 * n))
    return dict(log=e.log[mark:], ret=ret if c['f'] in RET_MATTERS else None, st=[e.snapshot(b, s) for b, s in objs])


RET_MATTERS = {'hit', 'getsel', 'getcount', 'oncreate', 'ondestroy'}   # the others return 1 or whatever the last call left

if __name__ == '__main__':
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 60
    seed = int(sys.argv[2]) if len(sys.argv) > 2 else 1
    which = sys.argv[3].split(',') if len(sys.argv) > 3 else list(FUNCS)
    lm = log_map()
    es, eo = IW('stock'), IW('ours')
    es.heap_mark, eo.heap_mark = es.heap_ptr, eo.heap_ptr
    A_s = dict(FUNCS)
    A_o = {k: (lm[v[0]], v[1]) for k, v in FUNCS.items()}
    r = random.Random(seed)
    total = 0
    for f in which:
        cases = [dict(f=f, seed=r.randint(0, 10 ** 6)) for _ in range(n)]
        total += compare(f, es, eo, run, cases, A_s, A_o)
    print('TOTAL problems', total)
