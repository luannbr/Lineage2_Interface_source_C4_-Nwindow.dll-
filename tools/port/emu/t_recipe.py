# STALE (C4, sa-emu agent `old`): no C4 successor for the recipe making windows (win/t_oldwnd.py only has NCRecipeManufactureItemWnd::OnPaint; the two big OnPaints need the inventory pages built by hand); C2 model, does not run.
"""Differential test of the recipe windows' paint code (stock vs ours under unicorn): NCRecipeManufactureWnd::OnPaint,
NCRecipeBuyManufactureWnd::OnPaint, NCRecipeTreeWnd::OnPaint, NCRecipeManufactureItemWnd::OnPaint / OnDrawToolTip and
NCRecipeTree::DrawNode. Random windows (positions, sizes, textures, numbers, inventory pages, node graphs) are painted by both
and the logs of the stubbed services (DrawText, DrawTexture, ...), the device fields and the objects written are compared.
usage: python tools/port/emu/t_recipe.py <cases> <seed>   (needs build/bin/Release/NWindowDecomp.dll and its .map)"""
import sys, random, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import emu as _emu
import stubs
from stubs import *
from t_fill import addrs

# NWD_OURS=<dir with NWindowDecomp.dll and .map> tests a snapshot of the build (the cycles of other agents replace build/bin)
if os.environ.get('NWD_OURS'):
    _emu.OURS = os.path.join(os.environ['NWD_OURS'], 'NWindowDecomp.dll')
    _emu.OURS_MAP = os.path.join(os.environ['NWD_OURS'], 'NWindowDecomp.map')

# more services of the stock image (and ours) the recipe paint code calls
stubs.STOCK_FN.update({0x1000f6d0: 'DrawStretchedTex9', 0x10056230: 'FormatSystemMessage', 0x10025290: 'FindTexture'})
stubs.FN_POP.update({'DrawStretchedTex9': 0x24, 'FormatSystemMessage': 0x10, 'FindTexture': 4, 'HideWindow': 0})
stubs.OURS_FN.update({
    'DrawStretchedTex9': ('@NCGDevice@@', '?DrawStretchedTex@NCGDevice@@QAEXHHHHHHHH'),
    'FormatSystemMessage': ('@NConsoleWnd@@', '?FormatSystemMessage@'),
    'FindTexture': ('@NCObject@@', '?FindTexture@NCObject@@'),
})


class RecipeEmu(TipEmu):
    def do_fn(self, nm):
        a = self.arg
        if nm == 'DrawStretchedTex9':
            self.log.append(('DrawStretchedTex9', [self.rds32(self.reg(UC_X86_REG_ESP) + 4 + 4 * i) for i in range(8)], a(8)))
            return None
        if nm == 'FormatSystemMessage':
            out, fmt, p1, p2 = a(0), self.wstr(a(1)), a(2), a(3)
            s = '%s|%s|%s' % (fmt, self.wstr(p1) if p1 else None, self.wstr(p2) if p2 else None)
            self.write_wstr_at(out, s)
            self.log.append(('FormatSystemMessage', s))
            return None
        if nm == 'HideWindow':
            self.log.append(('HideWindow', self.reg(UC_X86_REG_ECX)))
            return None
        if nm == 'FindTexture':
            name = self.wstr(a(0))
            self.log.append(('FindTexture', name))
            return 0x5000 + (sum(ord(c) for c in name) & 0xFFF)
        return super().do_fn(nm)


def rstr(r):
    return r.choice(['', 'Mithril Sword', 'Zubei', 'A' * 30, 'Iron Ore (Lv 3)'])


def sym(e, pre):
    c = [v for s, v in e.symbols.items() if s.startswith(pre)]
    assert len(c) >= 1, pre
    return c[0]


def make_page(e, r, ids):
    """an inventory page: an object with an items array at +0xdc (Data, Num, Max)"""
    page = e.alloc(0x120)
    e.uc.mem_write(page, bytes(0x120))
    n = r.randint(0, 6)
    ents = []
    for _ in range(n):
        p = e.alloc(0x1A28)
        e.uc.mem_write(p, bytes(0x1A28))
        e.write32(p + 0x1918, r.choice(ids))
        e.write32(p + 0x1920, r.choice([0, 1, 2, 5, 99, 1000]))
        ents.append(p)
    d = e.alloc(4 * max(1, n))
    for i, p in enumerate(ents):
        e.write32(d + 4 * i, p)
    e.write32(page + 0xDC, d)
    e.write32(page + 0xE0, n)
    e.write32(page + 0xE4, n)
    return page


# ---- the two recipe making windows --------------------------------------------------------------------------------
def make_wnd_setup(case, buy):
    def setup(e):
        r = random.Random(case['seed'])
        for i, s in ((959, 'success $s1 $s2'), (960, 'failure $s1')):
            e.fs_set(e.gdata + 0x2DD34 + 0x24 * i + 4, s)
        inv = e.alloc(0x110)
        e.uc.mem_write(inv, bytes(0x110))
        p0 = make_page(e, r, case['ids']) if case['page0'] else 0
        p1 = make_page(e, r, case['ids']) if case['page1'] else 0
        e.write32(inv + 0xC4, p0)
        e.write32(inv + 0xC8, p1)
        e.write32(e.desktop + 0xCC, inv)
        w = e.alloc(0x1B10)
        e.uc.mem_write(w, bytes(0x1B10))
        for off, v in ((0x44, case['sx']), (0x48, case['sy']), (0x4C, case['w']), (0x50, case['h'])):
            e.write32(w + off, v)
        e.write32(w + 0x9C, case['back'])
        e.write32(w + 0xA0, case['mptex'])
        e.write32(w + 0xA4, case['icon'])
        e.write_wstr_at(w + 0xB0, case['name'])
        e.write32(w + 0x19FC, case['grade'])
        for off, k in ((0x1ACC, 'id'), (0x1AD0, 'prod'), (0x1AD4, 'pcount'), (0x1AD8, 'mp'), (0x1ADC, 'rate'), (0x1AE0, 'cmp'), (0x1AE4, 'cmax')):
            e.write32(w + off, case[k])
        e.write32(w + (0x1AF0 if buy else 0x1AE8), case['result'])
        return w, [], (lambda e: None)
    return setup


def rand_make_case(r):
    return {
        'seed': r.randint(0, 1 << 30), 'ids': [r.randint(1, 5) for _ in range(4)], 'page0': r.random() < 0.9, 'page1': r.random() < 0.8,
        'sx': r.randint(0, 600), 'sy': r.randint(0, 400), 'w': r.randint(100, 300), 'h': r.randint(100, 400),
        'back': r.choice([0, 0x1111]), 'mptex': r.choice([0, 0x2222]), 'icon': r.choice([0, 0x3333]),
        'name': rstr(r), 'grade': r.choice([-1, 0, 1, 2, 3, 4, 5, 9]),
        'id': r.randint(0, 999), 'prod': r.randint(1, 5), 'pcount': r.randint(0, 20), 'mp': r.randint(0, 500), 'rate': r.randint(0, 100),
        'cmp': r.choice([0, 1, 100, 500, 12345, -5]), 'cmax': r.choice([0, 1, 100, 500, 12345]),
        'result': r.choice([-1, 0, 1, 2]),
    }


# ---- the recipe information window ----------------------------------------------------------------------------------
def tree_wnd_setup(case):
    def setup(e):
        w = e.alloc(0x1AE0)
        e.uc.mem_write(w, bytes(0x1AE0))
        for off, v in ((0x44, case['sx']), (0x48, case['sy']), (0x4C, case['w']), (0x50, case['h'])):
            e.write32(w + off, v)
        e.write32(w + 0x98, case['back'])
        e.write32(w + 0xA0, case['icon'])
        e.write_wstr_at(w + 0xAC, case['name'])
        e.write32(w + 0x19F8, case['grade'])
        e.write32(w + 0x1AC8, case['mp'])
        e.write32(w + 0x1ACC, case['rate'])
        e.write32(w + 0x1AD0, case['lv'])
        return w, [], (lambda e: None)
    return setup


def rand_tree_case(r):
    return {'sx': r.randint(0, 600), 'sy': r.randint(0, 400), 'w': r.randint(100, 300), 'h': r.randint(100, 400),
            'back': r.choice([0, 0x1111]), 'icon': r.choice([0, 0x3333]), 'name': rstr(r), 'grade': r.choice([-1, 0, 1, 2, 3, 4, 5, 9]),
            'mp': r.randint(0, 500), 'rate': r.randint(0, 100), 'lv': r.randint(0, 80)}


# ---- the ingredient grid ----------------------------------------------------------------------------------------------
def item_wnd_setup(case):
    def setup(e):
        A = addrs(e)
        w = e.alloc(0x120)
        e.uc.mem_write(w, bytes(0x120))
        for off, v in ((0x44, case['sx']), (0x48, case['sy']), (0x4C, case['w']), (0x50, case['h']),
                       (0xF0, case['cols']), (0xF4, case['rows']), (0xF8, case['top']), (0x100, 160), (0x104, case['cell']),
                       (0x10C, case['hover'])):
            e.write32(w + off, v)
        n = case['n']
        ents = []
        for i in range(n):
            p = e.alloc(0x1A28)
            e.uc.mem_write(p, bytes(0x1A28))
            it = case['items'][i]
            e.write32(p, it['icon'])
            e.write32(p + 0x1920, it['count'])
            e.write32(p + 0x1938, it['need'])
            if case['tip']:
                info = p + 0x19B0
                e.call(A['ctor'], this=info)
                e.call(A['settext'], this=info, args=[e.put_wstr(it['text']), 0x400])
            ents.append(0 if it['null'] else p)
        d = e.alloc(4 * max(1, n))
        for i, p in enumerate(ents):
            e.write32(d + 4 * i, p)
        e.write32(w + 0xDC, d)
        e.write32(w + 0xE0, n)
        e.write32(w + 0xE4, n)
        if case['skin']:
            skin = e.alloc(0x9C)
            e.uc.mem_write(skin, bytes(0x9C))
            e.call(A['skinctor'], this=skin)
            for i in range(9):
                e.write32(skin + 4 * i, (0x1000 + i) if case['tex'][i] else 0)
            e.write32(w + 0x70, skin)
        return w, [], (lambda e: [e.read32(w + o) for o in (0x44, 0x48, 0x10C)])
    return setup


def rand_item_case(r):
    n = r.randint(0, 20)
    return {'sx': r.randint(0, 600), 'sy': r.randint(0, 400), 'w': r.randint(100, 300), 'h': r.randint(100, 300),
            'cols': r.choice([1, 3, 6]), 'rows': r.choice([1, 3, 4]), 'top': r.randint(0, 3), 'cell': r.choice([32, 34]),
            'hover': r.randint(-1, 22), 'skin': r.choice([0, 1, 1, 1]), 'n': n, 'tip': True,
            'tex': [r.random() < 0.8 for _ in range(9)],
            'items': [{'icon': r.choice([0, 0x4444]), 'count': r.randint(0, 5), 'need': r.randint(0, 5), 'null': r.random() < 0.1,
                       'text': r.choice(['', 'Iron Ore', 'A rather long ingredient name for a tool tip'])}
                      for _ in range(n)]}


# ---- the tree's node drawing --------------------------------------------------------------------------------------------
def node_setup(case):
    """a tree object (+0x1A4 the root, the five textures) with a node graph; DrawNode(device, root, x, y)"""
    def setup(e):
        t = e.alloc(0x1E0)
        e.uc.mem_write(t, bytes(0x1E0))
        for i, off in enumerate((0x1BC, 0x1C0, 0x1C4, 0x1C8, 0x1CC)):
            e.write32(t + off, 0x6000 + i)
        vt = e.alloc(0x400)
        e.uc.mem_write(vt, bytes(0x400))
        e.write32(t, vt)
        e.write32(vt + 0x11C, e.draw_node_fn)
        nodes = []

        def make(spec):
            p = e.alloc(0x1A28)
            e.uc.mem_write(p, bytes(0x1A28))
            nv = e.alloc(0x400)
            e.uc.mem_write(nv, bytes(0x400))
            e.write32(nv + 0xB0, e.hide_stub)   # HideWindow logs; SetWindowRect (non-virtual) is the real code
            e.write32(p, nv)
            e.write32(p + 0x1A4, spec['icon'])
            e.write32(p + 0x1AC, spec['need'])
            e.write32(p + 0x1B0, spec['have'])
            e.write_wstr_at(p + 0x1B4, spec['name'])
            e.write32(p + 0x19B4, spec['wd'])
            e.write32(p + 0x19B8, spec['ht'])
            e.write32(p + 0x19BC, 32)
            e.write32(p + 0x19C0, 32)
            e.write32(p + 0x19F0, spec['open'])
            kids = [make(k) for k in spec['kids']]
            d = e.alloc(4 * max(1, len(kids)))
            for i, k in enumerate(kids):
                e.write32(d + 4 * i, k)
            e.write32(p + 0x19D0, d)
            e.write32(p + 0x19D4, len(kids))
            nodes.append(p)
            return p
        root = make(case['root'])
        e.write32(t + 0x1A4, root)
        return t, [root, case['x'], case['y']], (lambda e: [(e.read32(p + 0x1A1C), e.read32(p + 0x1A20), e.read32(p + 0x30),
                                                              e.read32(p + 0x34), e.read32(p + 0x38), e.read32(p + 0x3C)) for p in nodes])
    return setup


def rand_node(r, depth):
    kids = [rand_node(r, depth + 1) for _ in range(r.randint(0, 3))] if depth < 3 else []
    return {'icon': r.choice([0, 0x7777]), 'need': r.randint(0, 5), 'have': r.randint(0, 5), 'name': rstr(r),
            'wd': r.randint(20, 200), 'ht': 32, 'open': r.choice([0, 1]), 'kids': kids}


def node_pre(e, kind):
    # the vtable slot 0x11C of the tree points to the function under test (the recursion); HideWindow is a logging stub
    stub = STUB_BASE + 0x9100
    e.uc.mem_write(stub, b'\xC3')
    e.stubs[stub] = ('fn:HideWindow', None)
    e.hide_stub = stub
    e.draw_node_fn = 0x100C5A90 if kind == 'stock' else sym(e, '?DrawItem@NCRecipeTree@@')


# ---- the runner -------------------------------------------------------------------------------------------------------------
def run_generic(e, setup, fn):
    e.heap_ptr = e.heap_mark
    e.log.clear()
    this, args, probe = setup(e)
    dev = e.alloc(0x80)
    e.uc.mem_write(dev, bytes(0x80))
    e.log.clear()
    ret = e.call(fn, this=this, args=[dev] + args)
    dv = [e.read32(dev + o) for o in (0x2C, 0x30, 0x48, 0x4C, 0x5C)]
    return (ret, dv, list(e.log), probe(e))


def run_one(label, n, r, mk_case, setup_fn, stock_fn, ours_pre, pre=None):
    cases = [mk_case(r) for _ in range(n)]
    res = {}
    for kind in ('stock', 'ours'):
        e = RecipeEmu(kind)
        e.heap_mark = e.heap_ptr
        if pre:
            pre(e, kind)
        fn = stock_fn if kind == 'stock' else sym(e, ours_pre)
        res[kind] = []
        for c in cases:
            try:
                res[kind].append(run_generic(e, setup_fn(c), fn))
            except Exception as ex:   # a crash of the function (RuntimeError) or of a stub reading what it crashed on
                res[kind].append(('error', type(ex).__name__))
    bad = 0
    crashes = 0
    for i, (a, b) in enumerate(zip(res['stock'], res['ours'])):
        if a[0] == 'error' and b[0] == 'error':
            crashes += 1   # both crash (e.g. the stock's count loop reads the first page through the second page's indices)
            continue
        if a != b:
            bad += 1
            if bad <= 2:
                print(label, 'case', i, cases[i])
                print(' stock', str(a)[:1500])
                print(' ours ', str(b)[:1500])
    print(label, 'cases', n, 'differ', bad, '(both crash: %d)' % crashes)
    return bad


def main(n, seed):
    r = random.Random(seed)
    bad = 0
    bad += run_one('Manufacture OnPaint', n, r, rand_make_case, lambda c: make_wnd_setup(c, False), 0x100C5240,
                   '?OnPaint@NCRecipeManufactureWnd@@UAEHPAVNCGDevice@@@Z')
    bad += run_one('BuyManufacture OnPaint', n, r, rand_make_case, lambda c: make_wnd_setup(c, True), 0x100C28D0,
                   '?OnPaint@NCRecipeBuyManufactureWnd@@UAEHPAVNCGDevice@@@Z')
    bad += run_one('TreeWnd OnPaint', n, r, rand_tree_case, tree_wnd_setup, 0x100C6660, '?OnPaint@NCRecipeTreeWnd@@UAEHPAVNCGDevice@@@Z')
    bad += run_one('ManufactureItemWnd OnPaint', n, r, rand_item_case, item_wnd_setup, 0x100C4FC0,
                   '?OnPaint@NCRecipeManufactureItemWnd@@UAEHPAVNCGDevice@@@Z')
    bad += run_one('ManufactureItemWnd OnDrawToolTip', n, r, rand_item_case, item_wnd_setup, 0x100C78F0,
                   '?OnDrawToolTip@NCRecipeManufactureItemWnd@@UAEHPAVNCGDevice@@@Z')
    bad += run_one('NCRecipeTree DrawItem', n, r, lambda r: {'root': rand_node(r, 0), 'x': r.randint(0, 50), 'y': r.randint(0, 50)},
                   node_setup, 0x100C5A90, '?DrawItem@NCRecipeTree@@', pre=node_pre)
    return bad


if __name__ == '__main__':
    sys.exit(1 if main(int(sys.argv[1]), int(sys.argv[2])) else 0)
