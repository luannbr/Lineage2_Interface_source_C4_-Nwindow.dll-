# Differential tests of the C4 multisell window ("VIP shop", NCVIPShopWnd.cpp): stock against ours under unicorn.
# The pages are fake windows (a memory block with the stock layout and a fake vtable that logs its virtual calls); the device, the
# console's getters, the scroll window's base functions and the item entry's methods are logging services.
#   python t_vipwnd.py [cases] [seed] [draw,infopaint,neededpaint,gridpaint,wndpaint,move,clear,reset,show,ok,lbdown]
import sys, os, random, struct
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import tip4
from tip4 import *
import gw
from t_tipfill import rand_item, put_item, ENTRY

gw.VPOP.update({46: 4, 74: 0, 75: 0, 76: 0, 78: 0, 79: 0, 3: 4})

BASE_SVC = {
    0x10014870: ('PushOrigin', 8, 0), 0x100142f0: ('PopOrigin', 0, 0), 0x1002b750: ('SetContentHeight', 4, 0),
    0x1001e590: ('ItemIsSelected', 0, 0), 0x1001e580: ('ItemSelect', 0, 0), 0x1001e5e0: ('ItemReset', 0, 0),
    0x1002bac0: ('SWScrollUp', 0, 0), 0x1002bb50: ('SWScrollDown', 0, 0), 0x1002bbf0: ('SWOnScrollBar', 4, 0),
    0x1002bc60: ('SWOnMouseWheel', 8, 0), 0x10036160: ('WndOnDestroy', 0, 0), 0x10035ab0: ('WndHideWindow', 0, 0),
    0x10035880: ('RemoveChildWindow', 4, 0),
}
tip4.OURS_SYMS.update({
    0x10014870: '?PushOrigin@NCGDevice@@QAEXHH@Z', 0x100142f0: '?PopOrigin@NCGDevice@@QAEXXZ',
    0x1002b750: '?SetContentHeight@NCScrollWnd@@QAEXH@Z', 0x1001e590: '?IsSelected@NItemEntry@@QAEHXZ',
    0x1001e580: '?Select@NItemEntry@@QAEXXZ', 0x1001e5e0: '?Reset@NItemEntry@@QAEXXZ',
    0x1002bac0: '?ScrollUp@NCScrollWnd@@UAEHXZ', 0x1002bb50: '?ScrollDown@NCScrollWnd@@UAEHXZ',
    0x1002bbf0: '?OnScrollBar@NCScrollWnd@@UAEHH@Z', 0x1002bc60: '?OnMouseWheel@NCScrollWnd@@UAEHIJ@Z',
    0x10036160: '?OnDestroy@NCWnd@@UAEHXZ', 0x10035ab0: '?HideWindow@NCWnd@@UAEXXZ',
    0x10035880: '?RemoveChildWindow@NCWnd@@QAEXPAUNWndEntry@@@Z', 0x10035650: '?GetLocalRect@NCWnd@@QAEPAUNRectF@@PAU2@@Z',
})


class VW(TW):
    def __init__(self, kind, svc=None):
        extra = dict(BASE_SVC)
        extra.update(svc or {})
        TW.__init__(self, kind, extra_callees=extra)
        self.case = None
        self.rets['ItemIsSelected'] = lambda e, t, a: e.read32(t + 0x1B5C)
        self.rets['ItemSelect'] = self.item_select
        self.rets['FindTexture'] = lambda e, t, a: 0x9000 + len(e.wstr(a[0]) or '')
        self.rets['ItemCtor'] = self.item_ctor
        self.hit = 0
        self.rets['HitTest'] = lambda e, t, a: e.hit

    def item_select(self, e, t, a):
        e.write32(t + 0x1B5C, 1)
        return 0

    def item_ctor(self, e, t, a):
        e.uc.mem_write(t, bytes(0x1C40))
        return t

    def thiscall(self, nm, t, a):
        if nm == 'GetLocalRect':
            r = self.case.get('rects', {}).get(self.objs.get(t, t), (0, 0, 100.0, 100.0))
            self.log.append(('GetLocalRect', self.nm(t)))
            self.write32(a[0], r[0] & 0xFFFFFFFF)
            self.write32(a[0] + 4, r[1] & 0xFFFFFFFF)
            self.write32(a[0] + 8, f32(r[2]))
            self.write32(a[0] + 12, f32(r[3]))
            return a[0]
        if nm == 'ItemCtor':
            self.uc.mem_write(t, bytes(0x1C40))
            self.log.append(('ItemCtor',))
            return t
        if nm == 'AddNeeded':
            ids = bytes(self.uc.mem_read(a[0], 0x318))
            f = struct.unpack_from('<I', ids, 0)[0]
            self.log.append(('AddNeeded', f, wcut(ids[4:0x204]), struct.unpack_from('<IIII', ids, 0x204)[:2], wcut(ids[0x20C:0x30C]),
                             struct.unpack_from('<III', ids, 0x30C)))
            return None
        if nm == 'AddItem':
            ent = bytes(self.uc.mem_read(a[1], 0x1C40))
            keys = [struct.unpack_from('<i', ent, o)[0] for o in (0, 0x1B14, 0x1B20, 0x1B28, 0x1B30, 0x1B44, 0x1B64)]
            self.log.append(('AddItem', a[0], keys))
            return None
        return TW.thiscall(self, nm, t, a)


def f32bits(x):
    return struct.unpack('<I', struct.pack('<f', float(x)))[0]


def setf(e, p, off, x):
    e.write32(p + off, f32bits(x))


def wnd_common(e, w, c):
    e.write32(w + 0x44, c['sx'])
    e.write32(w + 0x48, c['sy'])
    setf(e, w, 0x4C, c['w'])
    setf(e, w, 0x50, c['h'])


def rand_win(r):
    return dict(sx=r.choice([0, 100, 640]), sy=r.choice([0, 50, 300]), w=r.choice([100.0, 244.0, 300.5]), h=r.choice([20.0, 150.0, 330.75]),
                scx=r.choice([0, 0, -5]), scy=r.choice([0, 0, -30, -140]))


def make_children(e, w, off, n, c, prefix='name'):
    kids = []
    for i in range(50):
        if i < n:
            k = e.window('%s%d' % (prefix, i), size=0x300)
            kids.append(k)
        else:
            kids.append(0)
        e.write32(w + off + 4 * i, kids[i])
    return kids


# ---- draw -------------------------------------------------------------------------------------------------------------------
def draw_case(r):
    return dict(item=None if r.random() < 0.1 else rand_item(r), x=r.choice([10, 0, 5]), y=r.choice([4, 0, 100]), sx=0, sy=0)


def run_draw(e, A, c):
    mark = len(e.log)
    w = e.window('info', size=0x58700)
    e.write32(w + 0x585A8, 0x5A5A)
    item = 0
    if c['item'] is not None:
        item = e.alloc(ENTRY + 16)
        put_item(e, c['item'], item)
        e.write32(item, 0x7777)
    dev = e.window('dev', size=0x100)
    e.call(A[0], this=w, args=[dev, item, c['x'], c['y']])
    return dict(log=e.log[mark:], ret=e.reg(UC_X86_REG_EAX), dev=e.snapshot(dev, 0x100))


# ---- paints -----------------------------------------------------------------------------------------------------------------
def infopaint_case(r):
    c = rand_win(r)
    c['n'] = r.choice([0, 1, 1, 2, 5, 49, 50])
    c['items'] = [rand_item(r) for _ in range(50)] if c['n'] == 1 else []
    return c


def run_infopaint(e, A, c):
    mark = len(e.log)
    w = e.window('info', size=0x58700)
    wnd_common(e, w, c)
    e.write32(w + 0xE8, c['scx'] & 0xFFFFFFFF)
    e.write32(w + 0xEC, c['scy'] & 0xFFFFFFFF)
    e.write32(w + 0x585A0, c['n'])
    e.write32(w + 0x585A8, 0x5A5A)
    for i in range(50):
        ent = w + 0x120 + i * 0x1C40
        if c['n'] == 1 and i == 0:
            put_item(e, c['items'][0], ent)
        e.write32(ent, 0x6000 + i)
        e.objs[ent] = 'entry%d' % i
    dev = e.window('dev', size=0x100)
    e.call(A[0], this=w, args=[dev])
    return dict(log=e.log[mark:], ret=e.reg(UC_X86_REG_EAX), dev=e.snapshot(dev, 0x100))


def neededpaint_case(r):
    c = rand_win(r)
    c['items'] = [dict(count=r.choice([0, 1, 5, 99999, -3]), icon=0x6100 + k) for k in range(r.choice([0, 0, 1, 3, 20]))]
    return c


def run_neededpaint(e, A, c):
    mark = len(e.log)
    w = e.window('needed', size=0x400)
    wnd_common(e, w, c)
    e.write32(w + 0xE8, c['scx'] & 0xFFFFFFFF)
    e.write32(w + 0xEC, c['scy'] & 0xFFFFFFFF)
    e.write32(w + 0x130, 0x5B5B)
    n = len(c['items'])
    data = e.alloc(4 * max(n, 1) + 16)
    for k, it in enumerate(c['items']):
        p = e.alloc(0x320)
        e.uc.mem_write(p, bytes(0x320))
        e.write32(p + 0x204, it['count'] & 0xFFFFFFFF)
        e.write32(p + 0x208, it['icon'])
        e.write32(data + 4 * k, p)
    e.write32(w + 0x120, data if n else 0)
    e.write32(w + 0x124, n)
    e.write32(w + 0x128, n)
    dev = e.window('dev', size=0x100)
    e.call(A[0], this=w, args=[dev])
    return dict(log=e.log[mark:], ret=e.reg(UC_X86_REG_EAX), dev=e.snapshot(dev, 0x100))


def gridpaint_case(r):
    c = rand_win(r)
    cols = r.choice([1, 3, 6])
    c.update(cols=cols, rows=r.choice([1, 4, 9]), top=r.choice([0, 0, 1, 2]), cell=r.choice([32, 40]), gx=r.choice([0, 5]), gy=r.choice([0, 3]))
    n = r.choice([0, 1, 5, 20, 54])
    c['items'] = [dict(null=r.random() < 0.1, sel=r.random() < 0.3, icon=0 if r.random() < 0.15 else 0x6200 + k) for k in range(n)]
    c['nums'] = [r.choice([1, 1, 2, 3, 0]) for _ in range(n)]
    return c


def run_gridpaint(e, A, c):
    mark = len(e.log)
    w = e.window('grid', size=0x400)
    wnd_common(e, w, c)
    e.write32(w + 0x124, 0x5C5C)
    e.write32(w + 0x140, c['cols'])
    e.write32(w + 0x144, c['rows'])
    e.write32(w + 0x148, c['top'])
    e.write32(w + 0x154, c['cell'])
    e.write32(w + 0x158, c['gx'])
    e.write32(w + 0x15C, c['gy'])
    e.write32(w + 0x168, 0x5D5D)
    n = len(c['items'])
    data = e.alloc(4 * max(n, 1) + 16)
    recs = e.alloc(4 * max(n, 1) + 16)
    for k, it in enumerate(c['items']):
        if it['null']:
            e.write32(data + 4 * k, 0)
        else:
            p = e.alloc(ENTRY + 16)
            e.uc.mem_write(p, bytes(ENTRY))
            e.write32(p, it['icon'])
            e.write32(p + 0x1B5C, 1 if it['sel'] else 0)
            e.objs[p] = 'item%d' % k
            e.write32(data + 4 * k, p)
        rec = e.alloc(0x40)
        e.uc.mem_write(rec, bytes(0x40))
        e.write32(rec + 4, c['nums'][k])
        e.write32(recs + 4 * k, rec)
    e.write32(w + 0x12C, data if n else 0)
    e.write32(w + 0x130, n)
    e.write32(w + 0x134, n)
    # the desktop's VIP shop window and its records
    con = e.console
    e.uc.mem_write(con, bytes(0x4000))
    desk = e.window('desktop', size=0x400)
    e.write32(con + 0x3BF4, desk)
    shop = e.window('shop', size=0x400)
    e.write32(desk + 0x140, shop)
    e.write32(shop + 0x110, recs if n else 0)
    e.write32(shop + 0x114, n)
    dev = e.window('dev', size=0x100)
    e.call(A[0], this=w, args=[dev])
    return dict(log=e.log[mark:], ret=e.reg(UC_X86_REG_EAX), dev=e.snapshot(dev, 0x100))


def wndpaint_case(r):
    return rand_win(r)


def run_wndpaint(e, A, c):
    mark = len(e.log)
    w = e.window('shopwnd', size=0x400)
    wnd_common(e, w, c)
    e.write32(w + 0x100, 0x5E5E)
    dev = e.window('dev', size=0x100)
    e.call(A[0], this=w, args=[dev])
    return dict(log=e.log[mark:], ret=e.reg(UC_X86_REG_EAX), dev=e.snapshot(dev, 0x100))


# ---- the table ----------------------------------------------------------------------------------------------------------------
TESTS = {
    'draw': (0x100a0160, 4, draw_case, run_draw, None),
    'infopaint': (0x100a1c50, 1, infopaint_case, run_infopaint, None),
    'neededpaint': (0x100a1a80, 1, neededpaint_case, run_neededpaint, None),
    'gridpaint': (0x100a1870, 1, gridpaint_case, run_gridpaint, None),
    'wndpaint': (0x100a1fc0, 1, wndpaint_case, run_wndpaint, None),
}


def main():
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 100
    seed = int(sys.argv[2]) if len(sys.argv) > 2 else 1
    which = sys.argv[3].split(',') if len(sys.argv) > 3 else list(TESTS)
    lm = log_map()
    total = 0
    for name in which:
        va, nargs, mk, run, svc = TESTS[name]
        r = random.Random(seed)
        es = VW('stock', svc)
        eo = VW('ours', svc)
        es.heap_mark, eo.heap_mark = es.heap_ptr, eo.heap_ptr
        cases = [mk(r) for _ in range(n)]

        def runner(e, A, c, run=run):
            e.case = c
            return run(e, A, c)
        total += compare(name, es, eo, runner, cases, (va, nargs), (lm[va], nargs))
    print('TOTAL problems', total)


if __name__ == '__main__':
    main()
