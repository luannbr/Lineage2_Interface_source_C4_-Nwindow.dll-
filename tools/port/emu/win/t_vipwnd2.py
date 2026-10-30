# Differential tests of the C4 multisell window's logic (scrolling, clearing, destroying, ShowItem, the click): stock against ours under
# unicorn; the second half of t_vipwnd.py's tests (see there).
#   python t_vipwnd2.py [cases] [seed] [moveinfo,moveneeded,infoup,...]
import sys, os, random, struct
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import tip4
from tip4 import *
import gw
from t_tipfill import rand_item, put_item, ENTRY
from t_vipwnd import VW, setf, rand_win, f32bits

tip4.OURS_SYMS.update({0x100a3f40: '?Reset@NCVIPShopWnd@@QAEXXZ', 0x100a1f50: '?Clear@NCVIPShopItemInfoWnd@@QAEXXZ',
                       0x100a25a0: '?Clear@NCVIPShopNeededItemWnd@@QAEXXZ', 0x100a3200: '?DestroyNeeded@NVIPRecord@@QAEXXZ',
                       0x100a2360: '?AddNeeded@NCVIPShopNeededItemWnd@@QAEXPBUNVIPNeededItem@@@Z',
                       0x100a26c0: '?AddItem@NCVIPShopItemInfoWnd@@QAEXHPAUNItemEntry@@@Z',
                       0x1001f7f0: '??0NItemEntry@@QAE@XZ', 0x1000e540: '?InsertText@NCEditBox@@QAEXPBGH@Z',
                       0x1001e790: '?HitTest@NCItemWnd@@QAEHHHHH@Z', 0x100a2e90: '?ShowItem@NCVIPShopWnd@@QAEXXZ',
                       0x1002b880: '?OnDestroy@NCScrollWnd@@UAEHXZ'})
SHOP_SVC_RESET = {0x100a3f40: ('Reset', 0, 0)}


def snap(e, base, size):
    return e.snapshot(base, size)


# ---- scroll ----------------------------------------------------------------------------------------------------------------
def move_case(r):
    c = rand_win(r)
    c['n'] = r.choice([0, 1, 3, 12, 50])
    c['dx'] = r.choice([0, 0, 7, -3])
    c['dy'] = r.choice([0, 15, -15, 100, 3])
    c['deltay'] = r.choice([0, 15, -15, 30, 100])
    c['mask'] = [r.random() < 0.85 for _ in range(50)]
    c['rects'] = {}
    for i in range(50):
        c['rects']['name%d' % i] = (r.choice([0, 46, -5]), r.choice([-300, -37, -36, -35, -1, 0, 10, 100, 149, 150, 151, 330, 331, 400]),
                                    r.choice([100.0, 170.0]), r.choice([0.0, 14.0, 36.5, 72.0, 400.0]))
    return c


def make_names(e, w, off, c):
    for i in range(50):
        k = e.window('name%d' % i, size=0x300) if (i < c['n'] and c['mask'][i]) else 0
        e.write32(w + off + 4 * i, k)


def info_wnd(e, c):
    w = e.window('info', size=0x58700)
    setf(e, w, 0x50, c['h'])
    e.write32(w + 0xF4, c['deltay'] & 0xFFFFFFFF)
    make_names(e, w, 0x585AC, c)
    return w


def needed_wnd(e, c):
    w = e.window('needed', size=0x400)
    setf(e, w, 0x50, c['h'])
    e.write32(w + 0xF4, c['deltay'] & 0xFFFFFFFF)
    make_names(e, w, 0x134, c)
    return w


def make_run_scroll(kind, nargs, void=False):
    def run(e, A, c):
        mark = len(e.log)
        w = info_wnd(e, c) if kind == 'info' else needed_wnd(e, c)
        args = [c['dx'] & 0xFFFFFFFF, c['dy'] & 0xFFFFFFFF][:nargs]
        e.call(A[0], this=w, args=args)
        if void:
            return dict(log=e.log[mark:])
        return dict(log=e.log[mark:], ret=e.reg(UC_X86_REG_EAX))
    return run


# ---- clear / destroy ---------------------------------------------------------------------------------------------------------------
def clear_case(r):
    return dict(n=r.choice([0, 1, 3, 20, 50]), mask=[r.random() < 0.8 for _ in range(50)], items=r.choice([0, 0, 1, 3, 7]),
                itemnull=[r.random() < 0.2 for _ in range(7)], num=r.choice([0, 1, 5, 50]))


def clear_case_noitems(r):
    c = clear_case(r)
    c['items'] = 0
    return c


def build_items(e, w, c):
    n = c['items']
    data = e.alloc(4 * 8 + 16)
    e.objs[data] = 'itemsarray'
    for k in range(n):
        if c['itemnull'][k]:
            e.write32(data + 4 * k, 0)
        else:
            p = e.alloc(0x320)
            e.uc.mem_write(p, bytes(0x320))
            e.objs[p] = 'needitem%d' % k
            e.write32(data + 4 * k, p)
    e.write32(w + 0x120, data if n else 0)
    e.write32(w + 0x124, n)
    e.write32(w + 0x128, n + 2 if n else 0)


def run_neededclear(e, A, c):
    mark = len(e.log)
    w = e.window('needed', size=0x400)
    build_items(e, w, c)
    make_names(e, w, 0x134, c)
    e.write32(w + 0x1FC, 77)
    e.write32(w + 0x200, 88)
    e.call(A[0], this=w, args=[])
    return dict(log=e.log[mark:], mem=snap(e, w + 0x120, 0xE4), ret=e.reg(UC_X86_REG_EAX))


def run_infoclear(e, A, c):
    mark = len(e.log)
    w = e.window('info', size=0x58700)
    for i in range(50):
        e.objs[w + 0x120 + i * 0x1C40] = 'entry%d' % i
    make_names(e, w, 0x585AC, c)
    e.write32(w + 0x585A0, c['num'])
    e.call(A[0], this=w, args=[])
    return dict(log=e.log[mark:], mem=snap(e, w + 0x585A0, 0xD4), ret=e.reg(UC_X86_REG_EAX))


# ---- the window -----------------------------------------------------------------------------------------------------------------------
def rand_rec(r):
    if r.random() < 0.1:
        return None
    ni = r.choice([0, 1, 1, 2, 5, 49])
    return dict(list=r.choice([0, 1, 1, 2, -1]), nitems=ni,
                items=[dict(icon=0x6300 + m, param=r.choice([-1, 0, 1, 2, 3, 5]), id=r.choice([57, 1001, 20]), count=r.choice([1, 5, 99999]),
                            body=r.choice([0, 6, 0x100]), ench=r.choice([0, 3, 15]), grade=r.choice([0, 1, 4]), prod=r.choice([500, 12345]))
                       for m in range(ni)],
                needed=[dict(id=r.choice([57, 4037]), name=r.choice(['Adena', 'Coin of Luck', 'A longer needed item name']), count=r.choice([1, 100]),
                             icon=0x6400 + m, ench=r.choice([0, 7]), grade=r.choice([0, 2]), type=r.choice([0, 1, 5, -1]))
                        for m in range(r.choice([0, 1, 2, 4]))])


def shop_world(e, c):
    w = e.window('shopwnd', size=0x400)
    for nm, off in (('list', 0xE8), ('info', 0xEC), ('needed', 0xF0), ('edit', 0xF4), ('ok', 0xF8), ('cancel', 0xFC)):
        k = e.window(nm, size=0x300)
        e.write32(w + off, k if c['has'][nm] else 0)
    e.write32(w + 0x104, c['sel'] & 0xFFFFFFFF)
    e.write32(w + 0x108, c['listid'])
    n = len(c['recs'])
    data = e.alloc(4 * max(n, 1) + 16)
    e.objs[data] = 'recarray'
    for k, rc in enumerate(c['recs']):
        if rc is None:
            e.write32(data + 4 * k, 0)
            continue
        p = e.alloc(0x50560)
        e.uc.mem_write(p, bytes(0x50560))
        e.objs[p] = 'rec%d' % k
        e.write32(p, rc['list'] & 0xFFFFFFFF)
        e.write32(p + 4, rc['nitems'])
        for m, it in enumerate(rc['items']):
            ip = p + 8 + m * 0x19B4
            e.write32(ip + 0x1900, it['icon'])
            e.write32(ip + 0x1914, it['param'] & 0xFFFFFFFF)
            e.write32(ip + 0x1920, it['id'])
            e.write32(ip + 0x1924, it['count'])
            e.write32(ip + 0x1930, it['body'])
            e.write32(ip + 0x1980, it['ench'])
            e.write32(ip + 0x195C, it['grade'])
            e.write32(ip + 0x19A4, it['prod'])
        nn = len(rc['needed'])
        nd = e.alloc(4 * max(nn, 1) + 16)
        e.objs[nd] = 'needarray%d' % k
        for m, nit in enumerate(rc['needed']):
            q = e.alloc(0x320)
            e.uc.mem_write(q, bytes(0x320))
            e.objs[q] = 'need%d_%d' % (k, m)
            e.write32(q, nit['id'])
            e.write_wstr_at(q + 4, nit['name'])
            e.write32(q + 0x204, nit['count'])
            e.write32(q + 0x208, nit['icon'])
            e.write32(q + 0x30C, nit['ench'])
            e.write32(q + 0x310, nit['grade'])
            e.write32(q + 0x314, nit['type'] & 0xFFFFFFFF)
            e.write32(nd + 4 * m, q)
        e.write32(p + 0x50530, nd if nn else 0)
        e.write32(p + 0x50534, nn)
        e.write32(p + 0x50538, nn)
        e.write32(data + 4 * k, p)
    e.write32(w + 0x110, data if n else 0)
    e.write32(w + 0x114, n)
    e.write32(w + 0x118, n)
    return w


def shop_case(r):
    n = r.choice([0, 1, 2, 4])
    c = dict(has={k: (r.random() < 0.9) for k in ('list', 'info', 'needed', 'edit', 'ok', 'cancel')},
             recs=[rand_rec(r) for _ in range(n)], listid=r.choice([0, 5, 77]))
    c['sel'] = r.choice([-1, 0, 1, 2, 3, n - 1 if n else 0, n + 1, 50])
    return c


def snap_shop(e, w):
    return e.snapshot(w + 0x100, 0x20)


def run_shop(e, A, c):
    mark = len(e.log)
    w = shop_world(e, c)
    e.call(A[0], this=w, args=[])
    return dict(log=e.log[mark:], mem=snap_shop(e, w), ret=e.reg(UC_X86_REG_EAX))


def run_shop_void(e, A, c):
    mark = len(e.log)
    w = shop_world(e, c)
    e.call(A[0], this=w, args=[])
    return dict(log=e.log[mark:], mem=snap_shop(e, w))


def shop_case_pages(r):
    c = shop_case(r)
    for k in ('list', 'info', 'needed'):
        c['has'][k] = True
    return c


def show_case(r):
    c = shop_case(r)
    c['recs'] = [rand_rec(r) or dict(list=r.choice([0, 1]), nitems=1, items=[dict(icon=0x6300, param=1, id=57, count=1, body=0, ench=0, grade=0, prod=5)],
                                      needed=[]) for _ in range(r.choice([1, 2, 3]))]
    c['sel'] = r.choice([-1, 0, 0, 1, 2])
    c['sel'] = min(c['sel'], len(c['recs']) - 1)
    return c


def show_case_pages(r):
    c = show_case(r)
    for k in ('list', 'info', 'needed', 'edit'):
        c['has'][k] = True
    return c


def lbdown_case(r):
    c = show_case(r)
    c['lparam'] = r.choice([0x00100020, 0x0000FFFF, 0x00340012])
    c['hit'] = r.choice([-1, 0, 1, 2, 3, 9])
    c['nitems'] = r.choice([0, 1, 3])
    c['dialog_shown'] = r.choice([0, 1])
    c['dialog_id'] = r.choice([0x51, 0x50, 2])
    return c


def run_lbdown(e, A, c):
    mark = len(e.log)
    shop = shop_world(e, c)
    grid = e.window('grid', size=0x400)
    ent = e.alloc(0x40)
    parent_ent = e.alloc(0x40)
    e.uc.mem_write(ent, bytes(0x40))
    e.uc.mem_write(parent_ent, bytes(0x40))
    e.objs[ent] = 'gridentry'
    e.write32(ent + 0xC, parent_ent)
    e.write32(parent_ent + 4, shop)
    e.write32(grid + 0x74, ent)
    n = c['nitems']
    data = e.alloc(4 * 4 + 16)
    e.objs[data] = 'itemarray'
    for k in range(n):
        p = e.alloc(ENTRY + 16)
        e.uc.mem_write(p, bytes(ENTRY))
        e.objs[p] = 'item%d' % k
        e.write32(data + 4 * k, p)
    e.write32(grid + 0x12C, data)
    e.write32(grid + 0x130, n)
    e.write32(grid + 0x138, 9)
    con = e.console
    e.uc.mem_write(con, bytes(0x4000))
    desk = e.window('desktop', size=0x400)
    e.write32(con + 0x3BF4, desk)
    dlg = e.window('dialog', size=0x400)
    e.write32(dlg + 0x244, c['dialog_id'])
    e.shown[dlg] = c['dialog_shown']
    e.write32(desk + 0x240, dlg)
    e.hit = c['hit']
    e.call(A[0], this=grid, args=[0, c['lparam']])
    return dict(log=e.log[mark:], ret=e.reg(UC_X86_REG_EAX), grid=e.snapshot(grid + 0x120, 0x60), shop=snap_shop(e, shop))


def arr_case(r):
    return dict(n=r.choice([0, 0, 1, 3, 9]), neg=r.random() < 0.0)


def build_arr(e, base, c):
    n = c['n']
    data = e.alloc(4 * 10 + 16) if n else 0
    for k in range(n):
        e.write32(data + 4 * k, 0x7000 + k)
    e.write32(base, data)
    e.write32(base + 4, n)
    e.write32(base + 8, n + 1 if n else 0)
    if data:
        e.objs[data] = 'arrdata'


def run_arrdtor(e, A, c):
    mark = len(e.log)
    arr = e.alloc(0x40)
    e.uc.mem_write(arr, bytes(0x40))
    build_arr(e, arr, c)
    e.call(A[0], this=arr, args=[])
    return dict(log=e.log[mark:], mem=e.snapshot(arr, 0x10))


def run_destroyneeded(e, A, c):
    mark = len(e.log)
    rec = e.alloc(0x50560)
    e.uc.mem_write(rec, bytes(0x50560))
    build_arr(e, rec + 0x50530, c)
    e.call(A[0], this=rec, args=[])
    return dict(log=e.log[mark:], mem=e.snapshot(rec + 0x50530, 0x10))


RESET_SVC = {0x100a1f50: ('InfoClear', 0, 0), 0x100a25a0: ('NeededClear', 0, 0), 0x100a3200: ('RecDestroyNeeded', 0, 0)}
SHOW_SVC = {0x100a1f50: ('InfoClear', 0, 0), 0x100a25a0: ('NeededClear', 0, 0), 0x100a2360: ('AddNeeded', 4, 0), 0x100a26c0: ('AddItem', 8, 0),
            0x1001f7f0: ('ItemCtor', 0, 1), 0x1000e540: ('InsertText', 8, 0)}

TESTS = {
    'moveinfo': (0x100a1e00, 2, move_case, make_run_scroll('info', 2, True), None),
    'moveneeded': (0x100a0010, 2, move_case, make_run_scroll('needed', 2, True), None),
    'infoup': (0x100a1eb0, 0, move_case, make_run_scroll('info', 0), None),
    'infodown': (0x100a1ed0, 0, move_case, make_run_scroll('info', 0), None),
    'infobar': (0x100a1ef0, 1, move_case, make_run_scroll('info', 1), None),
    'infowheel': (0x100a1f20, 2, move_case, make_run_scroll('info', 2), None),
    'neededup': (0x100a0050, 0, move_case, make_run_scroll('needed', 0), None),
    'neededdown': (0x100a0070, 0, move_case, make_run_scroll('needed', 0), None),
    'neededbar': (0x100a0090, 1, move_case, make_run_scroll('needed', 1), None),
    'neededwheel': (0x100a00c0, 2, move_case, make_run_scroll('needed', 2), None),
    'neededclear': (0x100a25a0, 0, clear_case, run_neededclear, None),
    'infoclear': (0x100a1f50, 0, clear_case, run_infoclear, None),
    'infodestroy': (0x100a2640, 0, clear_case, run_infoclear, {0x1002b880: ('SWOnDestroy', 0, 0)}),
    'neededdestroy': (0x100a2dc0, 0, clear_case_noitems, run_neededclear, {0x1002b880: ('SWOnDestroy', 0, 0)}),
    'arrdtor': (0x100a2cb0, 0, arr_case, run_arrdtor, None),
    'destroyneeded': (0x100a3200, 0, arr_case, run_destroyneeded, None),
    'reset': (0x100a3f40, 0, shop_case_pages, run_shop_void, RESET_SVC),
    'cancel': (0x100a4000, 0, shop_case, run_shop, SHOP_SVC_RESET),
    'hide': (0x100a41d0, 0, shop_case, run_shop, SHOP_SVC_RESET),
    'wnddestroy': (0x100a4040, 0, shop_case, run_shop, {0x100a3f40: ('Reset', 0, 0), 0x10036160: ('WndOnDestroy', 0, 0)}),
    'show': (0x100a2e90, 0, show_case_pages, run_shop_void, SHOW_SVC),
    'lbdown': (0x100a36c0, 2, lbdown_case, run_lbdown, {0x1001e790: ('HitTest', 16, 0), 0x100a2e90: ('ShowItem', 0, 0)}),
}


# standalone mode: stock functions that sa.func_map() does not find by class name but the standalone has as a compiled template instance
SA_ALT = {'arrdtor': '??1?$TArray@PAUNVIPNeededItem@@@@QAE@XZ'}      # TVIPNeededItemArray::~TVIPNeededItemArray


def main():
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 100
    seed = int(sys.argv[2]) if len(sys.argv) > 2 else 1
    which = sys.argv[3].split(',') if len(sys.argv) > 3 else list(TESTS)
    lm = log_map()
    total = 0
    for name in which:
        va, nargs, mk, run, svc = TESTS[name]
        ova = sa_pick(lm, va, name, SA_ALT.get(name))      # (standalone mode: None = no standalone function, SKIP line printed)
        if ova is None:
            continue
        r = random.Random(seed)
        es = VW('stock', svc)
        eo = VW('ours', svc)
        es.heap_mark, eo.heap_mark = es.heap_ptr, eo.heap_ptr
        cases = [mk(r) for _ in range(n)]

        def runner(e, A, c, run=run):
            e.case = c
            e.hit = c.get('hit', 0)
            return run(e, A, c)
        total += compare(name, es, eo, runner, cases, (va, nargs), (ova, nargs))
    print('TOTAL problems', total)


if __name__ == '__main__':
    main()
