# Differential test (stock vs ours under unicorn) of the NPC shop's item grid, wave C lane debt2 (src/nwindow/NCShopWnd.cpp):
#   NCShopItemWnd::MoveItem 0x10103C70 (the mode / side ladder with its eighteen tool tip fills, the stack merge, the take-off, the
#   total of the shop window shown, the preview's or the manor's), NCShopItemWnd::AddCopy 0x10103B40, NCShopItemWnd::AddItem 0x101039E0.
# Two grids (0x174 bytes: Items at +0x12C, MaxItems +0x150, Selected +0x160, Target +0x168, Mode +0x16C, Side +0x170) hold entries of
# 0x1C40 bytes with random contents (ID +0x1B20 in a small range so that the same-id merge is taken, bStackable +0x1B4C, TradeCount
# +0x1B54 as a signed 64-bit); the desktop (the console buffer at 0x102C39F4) holds the shop window (+0x134), the preview (+0x138) and
# the manor shop (+0x13C), each absent, disabled, hidden or shown. The calls that leave the function (NToolTipInfo::Fill with its
# flags and extra, the three UpdateTotalPrice, UpdateScrollRange, AddCopy for the merge, the entry constructors, FindTexture) are
# logging stubs on both sides; every byte of both grids, of every entry and of the arrays is compared, with the return value.
#   python tools/port/emu/win/t_shopwnd.py [cases] [seed] [move,addcopy,additem,total,rmove,rcopy,radd] [--any]   (needs the built DLL + .map and the smoke log)
import sys, os, random, struct
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from gw import *
import gw
import e4
import stubs

gw.VPOP.update({79: 0})
GDESK = 0x102C39F4
GRID = 0x174
ENTRY = 0x1C40
F = dict(Data=0x12C, Num=0x130, Max=0x134, MaxItems=0x150, Selected=0x160, Target=0x168, Mode=0x16C, Side=0x170, TopRow=0x148,
         Columns=0x140)
EF = dict(ID=0x1B20, Stackable=0x1B4C, Trade=0x1B54, Selected=0x1B5C, Count=0x1B28)

MOVE_SVC = {0x10030770: ('TipFill', 0xC, 0), 0x101036F0: ('ShopTotal', 0, 0), 0x100AFC60: ('ManorTotal', 0, 0),
            0x10103B40: ('AddCopy', 8, 0), 0x1001EA60: ('UpdateScrollRange', 0, 0)}
MOVE_SYMS = {0x10030770: '?Fill@NToolTipInfo@@QAEXPAUNItemEntry@@KK@Z', 0x101036F0: '?UpdateTotalPrice@NCShopWnd@@QAEXXZ',
             0x100AFC60: '?UpdateTotalPrice@NCManorShopWnd@@QAEXXZ', 0x10103B40: '?AddCopy@NCShopItemWnd@@QAEHPAUNItemEntry@@H@Z',
             0x1001EA60: '?UpdateScrollRange@NCItemWnd@@QAEXXZ'}
COPY_SVC = {0x10030770: ('TipFill', 0xC, 0), 0x1001EA60: ('UpdateScrollRange', 0, 0), 0x1001F7F0: ('ItemCtor', 0, 1),
            0x1001FE00: ('ItemCopy', 4, 0)}
COPY_SYMS = {0x10030770: MOVE_SYMS[0x10030770], 0x1001EA60: MOVE_SYMS[0x1001EA60], 0x1001F7F0: '??0NItemEntry@@QAE@XZ',
             0x1001FE00: '?Copy@NItemEntry@@QAEXPAU1@@Z'}
ADD_SVC = {0x10030770: ('TipFill', 0xC, 0), 0x1001EA60: ('UpdateScrollRange', 0, 0), 0x1001F960: ('ItemCtorInfo', 4, 1),
           0x1002B450: ('FindTexture', 8, 0)}
ADD_SYMS = {0x10030770: MOVE_SYMS[0x10030770], 0x1001EA60: MOVE_SYMS[0x1001EA60], 0x1001F960: '??0NItemEntry@@QAE@PAUNItemInfo@@@Z',
            0x1002B450: '?FindTexture@NCObject@@QAEPAVUTexture@@PBGH@Z'}


class SW(GW):
    def __init__(self, kind, svc, syms):
        have = None
        if kind == 'ours':
            import emu as _e
            have = set()
            for line in open(_e.OURS_MAP, errors='replace'):
                p = line.split()
                if len(p) > 2:
                    have.add(p[1])
        GW.__init__(self, kind, svc, {va: s for va, s in syms.items() if have is None or s in have})
        self.enabled = {}
        self.vrets[26] = lambda e, t, a: self.enabled.get(t, 0)
        self.rets['FindTexture'] = lambda e, t, a: 0 if (a[0] and e.wstr(a[0]).startswith('x')) else 0x7000 + (len(e.wstr(a[0])) if a[0] else 0)


def entry(e, r, name, c):
    p = e.alloc(ENTRY + 16)
    e.uc.mem_write(p, bytes(r.getrandbits(8) for _ in range(ENTRY)))
    e.write32(p + EF['ID'], r.randint(1, 4))
    e.write32(p + EF['Stackable'], r.choice([0, 0, 1, 2, 3, 4]))
    tc = r.choice([0, 1, 2, 3, 5, 100, 2 ** 31 + 5, -3])
    e.uc.mem_write(p + EF['Trade'], struct.pack('<q', tc))
    e.write32(p + EF['Selected'], r.choice([0, 1]))
    e.objs[p] = name
    return p


def grid(e, r, name, c, n, mx, mode, side):
    g = e.window(name, size=GRID)
    data = e.alloc(max(mx, n, 1) * 4 + 16)
    ents = []
    for i in range(n):
        p = entry(e, r, '%s.item%d' % (name, i), c)
        e.write32(data + 4 * i, p)
        ents.append(p)
    e.write32(g + F['Data'], data if n or mx else 0)
    e.write32(g + F['Num'], n)
    e.write32(g + F['Max'], max(mx, n))
    e.write32(g + F['MaxItems'], r.choice([2, 3, 5, 8]))
    e.write32(g + F['Mode'], mode)
    e.write32(g + F['Side'], side)
    e.write32(g + F['Columns'], r.randint(1, 6))
    e.write32(g + F['TopRow'], r.randint(0, 3))
    e.write32(g + F['Selected'], -1 & 0xFFFFFFFF)
    return g, data, ents


def desktop(e, r):
    con = e.console
    e.uc.mem_write(con, bytes(0x4000))
    d = e.alloc(0x800)            # the desktop: the console's +0x3BF4 (GDesktop = GConsole->Desktop in the C4 image)
    e.uc.mem_write(d, bytes(0x800))
    e.write32(con + 0x3BF4, d)
    ws = []
    for off, nm in ((0x134, 'shopwnd'), (0x138, 'preview'), (0x13C, 'manor')):
        st = r.choice(['none', 'hidden', 'disabled', 'shown', 'shown'])
        if st == 'none':
            e.write32(d + off, 0)
            continue
        w = e.window(nm, size=0x200)
        e.write32(d + off, w)
        e.enabled[w] = 0 if st == 'disabled' else 1
        e.shown[w] = 0 if st == 'hidden' else 1
        ws.append(w)
    return d, ws


def snap(e, objs):
    out = []
    for base, size in objs:
        out.append(e.snapshot(base, size))
    return out


def run_move(e, A, c):
    mark = len(e.log)
    r = random.Random(c['seed'])
    e.heap_ptr = e.heap_mark
    e.enabled.clear()
    e.shown.clear()
    d, ws = desktop(e, r)
    n1, n2 = r.randint(1, 4), r.randint(0, 4)
    mode = r.choice([0, 0, 1, 1, 2, 2, 3])
    side_g = r.choice([0, 1])
    side_t = r.choice([0, 1, 1, 2])
    mode_t = r.choice([0, 1, 2])
    if c.get('real'):
        # the two grids of one shop window: the window's SetMode gives both the same mode, and their sides are the two sides
        mode, mode_t, side_t = r.choice([0, 1, 2]), None, 1 - side_g
        mode_t = mode
    g, d1, e1 = grid(e, r, 'G', c, n1, n1 + 2, mode, side_g)
    t, d2, e2 = grid(e, r, 'T', c, n2, n2 + 2, mode_t, side_t)
    # the same ids on both sides, so that the stack merge is taken
    for p in e2:
        if e1 and r.random() < 0.6:
            e.write32(p + EF['ID'], e.read32(r.choice(e1) + EF['ID']))
    e.write32(g + F['Target'], t if r.random() > 0.08 else 0)
    sel = r.randrange(n1)
    e.write32(g + F['Selected'], sel)
    item = e1[sel]
    count = r.choice([1, 1, 2, 3, 5, 200])
    e.call(A['move'], this=g, args=[item, count])
    objs = [(g, GRID), (t, GRID)] + [(p, ENTRY) for p in e1 + e2]
    for w in (g, t):   # the live part of each array only (the slack beyond Num is stale heap)
        da, na = e.read32(w + F['Data']), e.read32(w + F['Num'])
        if da and 0 < na < 64:
            objs.append((da, 4 * na))
    return dict(log=e.log[mark:], st=snap(e, objs))


def run_addcopy(e, A, c):
    mark = len(e.log)
    r = random.Random(c['seed'])
    e.heap_ptr = e.heap_mark
    mode = r.choice([0, 1, 2, 3])
    side = r.choice([0, 1, 2])
    n = r.randint(0, 6)
    g, d1, e1 = grid(e, r, 'G', c, n, n + r.choice([0, 1, 3]), mode, side)
    src = entry(e, r, 'src', c)
    count = r.choice([1, 2, 7, 100000])
    e.write32(g + F['MaxItems'], r.choice([n, n + 1, n + 3, 8]))
    ret = e.call(A['addcopy'], this=g, args=[src, count])
    nn = e.read32(g + F['Num'])
    d = e.read32(g + F['Data'])
    new = [e.read32(d + 4 * i) for i in range(n, nn)] if d else []
    objs = [(g, GRID)] + [(p, ENTRY) for p in e1 + new + [src]] + ([(d, 4 * nn)] if d and nn else [])
    return dict(log=e.log[mark:], ret=ret & 0xFF if ret is not None else None, st=snap(e, objs))


def run_additem(e, A, c):
    mark = len(e.log)
    r = random.Random(c['seed'])
    e.heap_ptr = e.heap_mark
    mode = r.choice([0, 1, 2, 3])
    side = r.choice([0, 1, 2])
    n = r.randint(0, 6)
    g, d1, e1 = grid(e, r, 'G', c, n, n + r.choice([0, 1, 3]), mode, side)
    e.write32(g + F['MaxItems'], r.choice([n, n + 1, n + 3, 8]))
    info = e.alloc(0x19B4 + 16)
    b = bytearray(r.getrandbits(8) for _ in range(0x19B4))
    names = ['', 'x', 'icon.a', 'xmissing', 'l2ui.item.long_name_here']
    for off, n_ in ((0, 256), (0x200, 256), (0x400, 128), (0x500, 128), (0x600, 128), (0x700, 128), (0x800, 128), (0x900, 2048)):
        s = (r.choice(names) if off == 0x400 else r.choice(['', 'abc'])).encode('utf-16-le') + b'\0\0'
        b[off:off + len(s)] = s
    e.uc.mem_write(info, bytes(b))
    e.objs[info] = 'info'
    ret = e.call(A['additem'], this=g, args=[info])
    nn = e.read32(g + F['Num'])
    d = e.read32(g + F['Data'])
    new = [e.read32(d + 4 * i) for i in range(n, nn)] if d else []
    objs = [(g, GRID), (info, 0x19B4)] + [(p, ENTRY) for p in e1 + new] + ([(d, 4 * nn)] if d and nn else [])
    return dict(log=e.log[mark:], ret=ret & 0xFF if ret is not None else None, st=snap(e, objs))


TESTS = {
    'move': ('NCShopItemWnd::MoveItem', 0x10103C70, MOVE_SVC, MOVE_SYMS, run_move),
    'addcopy': ('NCShopItemWnd::AddCopy', 0x10103B40, COPY_SVC, COPY_SYMS, run_addcopy),
    'additem': ('NCShopItemWnd::AddItem', 0x101039E0, ADD_SVC, ADD_SYMS, run_additem),
}


def run_total(e, A, c):
    mark = len(e.log)
    r = random.Random(c['seed'])
    e.heap_ptr = e.heap_mark
    w = e.window('shop', size=0x124)
    ps, ss = [], []
    grids = []
    for nm in ('P', 'S'):
        g, d, ents = grid(e, r, nm, c, r.randint(0, 6), 8, 0, 0)
        for p in ents:
            e.write32(p + 0x1B2C, r.choice([0, 1, 5, 1000, 2 ** 31 - 1, 0x80000000, 0xFFFFFFFF, r.getrandbits(32)]))
            e.uc.mem_write(p + EF['Trade'], struct.pack('<q', r.choice([0, 1, 7, 99999, -5, 2 ** 40, 2 ** 62, r.getrandbits(63)])))
        grids.append((g, ents))
    e.write32(w + 0xE8, grids[0][0])
    e.write32(w + 0xEC, grids[1][0])
    e.write32(w + 0x110, r.choice([0, 1, 2, 3, -1 & 0xFFFFFFFF]))
    e.uc.mem_write(w + 0x11C, struct.pack('<q', r.getrandbits(62)))   # the old total: the function starts from 0
    e.call(A['total'], this=w, args=[])
    objs = [(w, 0x124)] + [(p, ENTRY) for g, ents in grids for p in ents]
    return dict(log=e.log[mark:], st=snap(e, objs))


TESTS['total'] = ('NCShopWnd::UpdateTotalPrice', 0x101036F0, {}, {}, run_total)


# ---- the recipe shop's grids (NCRecipeShopItemWnd, src/nwindow/NCRecipeShopItemWnd.cpp): AddItem 0x100F1850, AddCopy 0x100F1980, MoveItem 0x100F1A60
RMOVE_SVC = {0x10030770: ('TipFill', 0xC, 0), 0x100F1980: ('AddCopy', 4, 0), 0x1001EA60: ('UpdateScrollRange', 0, 0)}
RMOVE_SYMS = {0x10030770: MOVE_SYMS[0x10030770], 0x100F1980: '?AddCopy@NCRecipeShopItemWnd@@QAEHPAUNItemEntry@@@Z',
              0x1001EA60: MOVE_SYMS[0x1001EA60]}
RCOPY_SVC = {0x10030770: ('TipFill', 0xC, 0), 0x1001EA60: ('UpdateScrollRange', 0, 0), 0x1001F7F0: ('ItemCtor', 0, 1),
             0x1001FE00: ('ItemCopy', 4, 0)}
RADD_SVC = {0x10030770: ('TipFill', 0xC, 0), 0x1001EA60: ('UpdateScrollRange', 0, 0), 0x1001F960: ('ItemCtorInfo', 4, 1),
            0x1002B450: ('FindTexture', 8, 0)}


def run_rmove(e, A, c):
    mark = len(e.log)
    r = random.Random(c['seed'])
    e.heap_ptr = e.heap_mark
    n1, n2 = r.randint(1, 4), r.randint(0, 4)
    mode = r.choice([0, 1])
    g, d1, e1 = grid(e, r, 'G', c, n1, n1 + 2, mode, 0)
    t, d2, e2 = grid(e, r, 'T', c, n2, n2 + 2, r.choice([0, 1, 2]) if not c.get('real') else 1 - mode, 0)
    for p in e2:
        if e1 and r.random() < 0.6:
            e.write32(p + EF['ID'], e.read32(r.choice(e1) + EF['ID']))
    e.write32(g + F['Target'], t if r.random() > 0.08 else 0)
    sel = r.randrange(n1)
    e.write32(g + F['Selected'], sel)
    e.call(A['rmove'], this=g, args=[e1[sel]])
    objs = [(g, GRID), (t, GRID)] + [(p, ENTRY) for p in e1 + e2]
    for w in (g, t):
        da, na = e.read32(w + F['Data']), e.read32(w + F['Num'])
        if da and 0 < na < 64:
            objs.append((da, 4 * na))
    return dict(log=e.log[mark:], st=snap(e, objs))


TESTS['rmove'] = ('NCRecipeShopItemWnd::MoveItem', 0x100F1A60, RMOVE_SVC, RMOVE_SYMS, run_rmove)


def run_rcopy(e, A, c):
    mark = len(e.log)
    r = random.Random(c['seed'])
    e.heap_ptr = e.heap_mark
    n = r.randint(0, 6)
    g, d1, e1 = grid(e, r, 'G', c, n, n + r.choice([0, 1, 3]), r.choice([0, 1, 2]), 0)
    src = entry(e, r, 'src', c)
    e.write32(g + F['MaxItems'], r.choice([n, n + 1, n + 3, 8]))
    ret = e.call(A['rcopy'], this=g, args=[src])
    nn = e.read32(g + F['Num'])
    d = e.read32(g + F['Data'])
    new = [e.read32(d + 4 * i) for i in range(n, nn)] if d else []
    objs = [(g, GRID)] + [(p, ENTRY) for p in e1 + new + [src]] + ([(d, 4 * nn)] if d and nn else [])
    return dict(log=e.log[mark:], ret=ret & 0xFF if ret is not None else None, st=snap(e, objs))


def run_radd(e, A, c):
    a2 = dict(A)
    a2['additem'] = A['radd']
    return run_additem(e, a2, c)


TESTS['rcopy'] = ('NCRecipeShopItemWnd::AddCopy', 0x100F1980, RCOPY_SVC, COPY_SYMS, run_rcopy)
TESTS['radd'] = ('NCRecipeShopItemWnd::AddItem', 0x100F1850, RADD_SVC, ADD_SYMS, run_radd)

REAL = '--any' not in sys.argv   # --any: the two grids of a case may have different modes (a state the game never builds)
sys.argv = [a for a in sys.argv if a != '--any']

if __name__ == '__main__':
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 30
    seed = int(sys.argv[2]) if len(sys.argv) > 2 else 1
    which = sys.argv[3].split(',') if len(sys.argv) > 3 else list(TESTS)
    lm = log_map()
    total = 0
    for key in which:
        title, va, svc, syms, runf = TESTS[key]
        es = SW('stock', svc, syms)
        eo = SW('ours', svc, syms)
        es.heap_mark, eo.heap_mark = es.heap_ptr, eo.heap_ptr
        A_s, A_o = {key: va}, {key: lm[va]}
        r = random.Random(seed)
        cases = [dict(f=key, seed=r.randint(0, 10 ** 6), real=REAL) for _ in range(n)]
        total += compare(title, es, eo, runf, cases, A_s, A_o)
    print('TOTAL problems', total)
