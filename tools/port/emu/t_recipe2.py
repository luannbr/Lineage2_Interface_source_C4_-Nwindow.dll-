# STALE (C4, sa-emu agent `old`): no C4 successor (NCRecipeManufactureWnd::SetRecipe 0x100F73F0, the tree fill NCRecipeGaraTreeCtrl::AddRecipe 0x100F59C0): needs a rewrite on the C4 layouts; C2 model, does not run.
"""Differential test of the recipe data code (stock vs ours under unicorn): NCRecipeManufactureWnd::SetRecipe (the code of
NCRecipeBuyManufactureWnd::SetRecipe too: the linker folds the two), NCRecipeTreeWnd::SetRecipe and NCRecipeTreeCtrl::AddRecipe
(the tree fill, 0x100c6af0). A fake item table (the hash map the stock FindRef walks), fake recipe records and inventory pages are
built in the emulated memory; what the functions write (the window, the entries they allocate, the node infos handed to
NCTree::AddNode) and the services they call are compared.
usage: python tools/port/emu/t_recipe2.py <cases> <seed>   (NWD_OURS=<dir> as in t_recipe.py)"""
import sys, random, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import t_recipe
from t_recipe import *
import stubs

stubs.IMPORT_POP.update({
    '?GetRecipeDataBy2Condition@FL2GameData@@QAEPAUFL2RecipeData@@HH@Z': 8,
    '?GetRecipeDataByIndex@FL2GameData@@QAEPAUFL2RecipeData@@H@Z': 4,
    '??DL2FName@@QBEPBGXZ': 0,
    '?GetItemName@FL2GameData@@QAEPAGH@Z': 4,
    '?GetItemAdditionalName@FL2GameData@@QAEPAGH@Z': 4,
    '?appStrcmp@@YAHPBG0@Z': 0,
})
# services of the stock image (and ours) stubbed: the tool tip fill and the grid's scroll range are other functions' business
stubs.STOCK_FN.update({0x10028B20: 'ToolTipFill', 0x10019B20: 'UpdateScrollRange'})
stubs.FN_POP.update({'ToolTipFill': 0xC, 'UpdateScrollRange': 0, 'AddNode': 8, 'Malloc': 8})
stubs.OURS_FN.update({'ToolTipFill': ('@NToolTipInfo@@', '?Fill@NToolTipInfo@@'),
                      'UpdateScrollRange': ('@NCItemWnd@@', '?UpdateScrollRange@NCItemWnd@@')})


class DataEmu(RecipeEmu):
    recipes = {}

    def do_import(self, nm):
        if nm == '?GetRecipeDataBy2Condition@FL2GameData@@QAEPAUFL2RecipeData@@HH@Z':
            a, b = self.arg(0), self.arg(1)
            self.log.append(('GetRecipeDataBy2Condition', a, b))
            return self.recipes.get((a, b), 0)
        if nm == '??DL2FName@@QBEPBGXZ':
            t = self.this()
            return self.static_str('N%d' % self.read32(t))
        if nm == '?GetItemName@FL2GameData@@QAEPAGH@Z':
            i = self.arg(0)
            return 0 if i % 5 == 0 else self.static_str('Item%d' % i)
        if nm == '?GetItemAdditionalName@FL2GameData@@QAEPAGH@Z':
            i = self.arg(0)
            return (0, self.static_str('Add%d' % i), self.static_str('NAME_None'))[i % 3]
        if nm == '?appStrcmp@@YAHPBG0@Z':
            x, y = self.wstr(self.arg(0)), self.wstr(self.arg(1))
            return 0 if x == y else (1 if x > y else 0xFFFFFFFF)
        return super().do_import(nm)

    def do_fn(self, nm):
        if nm == 'ToolTipFill':
            self.log.append(('ToolTipFill', self.this() & 0xFFF, self.arg(1), self.arg(2)))
            return None
        if nm == 'UpdateScrollRange':
            self.log.append(('UpdateScrollRange',))
            return None
        if nm == 'AddNode':
            info, parent = self.arg(0), self.arg(1)
            self.node_count += 1
            node = 0x9000 + self.node_count
            self.log.append(('AddNode', parent if parent < 0x9000 else parent - 0x9000, self.node_info(info)))
            return node
        if nm == 'Malloc':
            n = self.arg(0)
            p = self.alloc(n)
            self.uc.mem_write(p, bytes(n))
            return p
        return super().do_fn(nm)

    def node_info(self, p):
        return (self.read32(p), self.rds32(p + 4), self.rds32(p + 8), self.rds32(p + 0xC), self.wstr(p + 0x10), self.wstr(p + 0x810),
                self.read32(p + 0x1810), self.read32(p + 0x1814), self.read32(p + 0x1818), self.read32(p + 0x181C))


def install_stub(e, name, addr_off):
    stub = STUB_BASE + addr_off
    e.uc.mem_write(stub, b'\xC3')
    e.stubs[stub] = ('fn:' + name, None)
    return stub


def world(e, case):
    """item table, recipes, inventory; returns the console-side pieces the functions read"""
    # malloc: the allocator object's vtable slot 0 is a stub
    e.write32(e.gmalloc_vt, install_stub(e, 'Malloc', 0x9200))
    e.node_count = 0
    # items: id -> NViewItemData
    items = {}
    for iid, it in case['items'].items():
        p = e.alloc(0x240)
        e.uc.mem_write(p, bytes(0x240))
        e.write32(p + 4, it['class'])
        e.write32(p + 0x34, it['icon'])
        e.write32(p + 0x40, it['name'])
        e.write32(p + 0x44, it['add'])
        e.fs_set(p + 0x48, it['desc'])
        e.write32(p + 0xBC, it['g0'])
        e.write32(p + 0x214, it['g1'])
        e.write32(p + 0x8C, it['g2'])
        items[iid] = p
    # the hash map: Pairs (Data, Num, Max), Hash, HashCount; pair = (HashNext, Key, Value)
    ids = sorted(items)
    hc = 8
    pairs = e.alloc(12 * max(1, len(ids)))
    hashp = e.alloc(4 * hc)
    for i in range(hc):
        e.write32(hashp + 4 * i, 0xFFFFFFFF)
    for i, iid in enumerate(ids):
        e.write32(pairs + 12 * i + 4, iid)
        e.write32(pairs + 12 * i + 8, items[iid])
        e.write32(pairs + 12 * i, e.read32(hashp + 4 * (iid & (hc - 1))))
        e.write32(hashp + 4 * (iid & (hc - 1)), i)
    tab = e.alloc(0x20)
    e.write32(tab, pairs); e.write32(tab + 4, len(ids)); e.write32(tab + 8, len(ids))
    e.write32(tab + 0xC, hashp); e.write32(tab + 0x10, hc)
    e.write32(e.gdata + 0x27E4, tab)
    # recipes
    recs = {}
    for key, rc in case['recipes'].items():
        p = e.alloc(0x40)
        e.uc.mem_write(p, bytes(0x40))
        e.write32(p, items.get(rc['prod'], 0) if rc['has_rec'] else 0)
        mats = []
        for (mid, cnt) in rc['mats']:
            m = e.alloc(8)
            e.write32(m, mid); e.write32(m + 4, cnt)
            mats.append(m)
        d = e.alloc(4 * max(1, len(mats)))
        for i, m in enumerate(mats):
            e.write32(d + 4 * i, m)
        e.write32(p + 4, d); e.write32(p + 8, len(mats)); e.write32(p + 0xC, len(mats))
        for off, k in ((0x1C, 'rid'), (0x20, 'ritem'), (0x24, 'lv'), (0x28, 'prod'), (0x2C, 'pcount'), (0x30, 'mp'), (0x34, 'rate')):
            e.write32(p + off, rc[k])
        recs[key] = p
    DataEmu.recipes = recs
    # the inventory pages
    r = random.Random(case['seed'])
    inv = e.alloc(0x110)
    e.uc.mem_write(inv, bytes(0x110))
    p0 = make_page(e, r, list(case['items'])) if case['page0'] else 0
    p1 = make_page(e, r, list(case['items'])) if case['page1'] else 0
    e.write32(inv + 0xC4, p0)
    e.write32(inv + 0xC8, p1)
    e.write32(e.desktop + 0xCC, inv)
    return recs


def run_nodev(e, setup, fn):
    e.heap_ptr = e.heap_mark
    e.log.clear()
    this, args, probe = setup(e)
    e.log.clear()
    ret = e.call(fn, this=this, args=args)
    return (ret, None, list(e.log), probe(e))


def dump_entries(e, grid):
    out = []
    data, n = e.read32(grid + 0xDC), e.read32(grid + 0xE0)
    for i in range(n):
        p = e.read32(data + 4 * i)
        out.append((e.read32(p), e.wstr(p + 0xC), e.wstr(p + 0x20C), e.wstr(p + 0x40C), e.wstr(p + 0x70C), e.rds32(p + 0x1918),
                    e.rds32(p + 0x1920), e.rds32(p + 0x1938)))
    return out


def set_recipe_setup(case, kind_sym):
    def setup(e):
        recs = world(e, case)
        w = e.alloc(0x1B10)
        e.uc.mem_write(w, bytes(0x1B10))
        grid = 0
        if case['grid']:
            grid = e.alloc(0x120)
            e.uc.mem_write(grid, bytes(0x120))
            e.write32(grid + 0x100, case['maxitems'])
            e.write32(grid + 0x98, 0)
            e.write32(w + 0x98, grid)
        rec = recs[case['rec']] if case['rec'] is not None else 0
        return w, [rec], (lambda e: ([e.read32(w + o) for o in (0xA4, 0x19FC, 0x1ACC, 0x1AD0, 0x1AD4, 0x1AD8, 0x1ADC)],
                                     e.wstr(w + 0xB0), e.wstr(w + 0x4B0), e.wstr(w + 0x7B0), dump_entries(e, grid) if grid else None))
    return setup


def tree_set_setup(case):
    def setup(e):
        recs = world(e, case)
        w = e.alloc(0x1AE0)
        e.uc.mem_write(w, bytes(0x1AE0))
        ctrl = e.alloc(0x100)
        e.uc.mem_write(ctrl, bytes(0x100))
        if case['has_ctrl']:
            tree = e.alloc(0x200)
            e.uc.mem_write(tree, bytes(0x200))
            vt = e.alloc(0x400)
            e.uc.mem_write(vt, bytes(0x400))
            e.write32(vt + 0x110, install_stub(e, 'AddNode', 0x9300))
            e.write32(tree, vt)
            e.write32(ctrl + 0xE4, tree)
            e.write32(w + 0x9C, ctrl)
            e.ctrl_addr = ctrl
        return w, [case['index'], 0], (lambda e: ([e.read32(w + o) for o in (0xA0, 0x19F8, 0x1AC8, 0x1ACC, 0x1AD0)],
                                                  e.wstr(w + 0xAC), e.wstr(w + 0x4AC), e.wstr(w + 0x7AC)))
    return setup


def add_recipe_setup(case):
    def setup(e):
        recs = world(e, case)
        ctrl = e.alloc(0x100)
        e.uc.mem_write(ctrl, bytes(0x100))
        tree = e.alloc(0x200)
        e.uc.mem_write(tree, bytes(0x200))
        vt = e.alloc(0x400)
        e.uc.mem_write(vt, bytes(0x400))
        e.write32(vt + 0x110, install_stub(e, 'AddNode', 0x9300))
        e.write32(tree, vt)
        e.write32(ctrl + 0xE4, tree)
        return ctrl, [case['item'], case['rate'], case['need'], case['parent']], (lambda e: None)
    return setup


def rand_world(r):
    ids = [1, 2, 3, 4, 5, 6, 7]
    items = {}
    for i in ids:
        items[i] = {'class': r.choice([0, 1, 2, 3]), 'icon': r.randint(1, 50), 'name': r.randint(1, 50), 'add': r.randint(1, 50),
                    'desc': r.choice(['', 'a description', 'x' * 40]), 'g0': r.randint(0, 5), 'g1': r.randint(0, 5), 'g2': r.randint(0, 5)}
    recipes = {}
    for k in range(r.randint(1, 5)):
        prod = r.choice(ids)
        rate = r.choice([100, 70, 60])
        mats = [(r.choice(ids), r.randint(1, 9)) for _ in range(r.randint(0, 4))]
        recipes[(prod, rate)] = {'prod': prod, 'has_rec': r.random() < 0.85, 'mats': mats, 'rid': r.randint(1, 900), 'ritem': r.choice(ids),
                                 'lv': r.randint(1, 9), 'pcount': r.randint(1, 4), 'mp': r.randint(0, 99), 'rate': rate}
    return {'seed': r.randint(0, 1 << 30), 'items': items, 'recipes': recipes, 'page0': r.random() < 0.9, 'page1': r.random() < 0.8}


def rand_set_case(r):
    c = rand_world(r)
    keys = list(c['recipes'])
    c['rec'] = r.choice(keys + [None])
    c['grid'] = r.random() < 0.9
    c['maxitems'] = r.choice([0, 2, 160, 160, 160])
    return c


def rand_tree_set_case(r):
    c = rand_world(r)
    c['has_ctrl'] = True
    keys = list(c['recipes'])
    # GetRecipeDataByIndex: the stub answers by index through the same table
    c['index'] = r.choice(range(len(keys) + 1))
    c['index_keys'] = keys
    return c


def rand_add_case(r):
    c = rand_world(r)
    keys = list(c['recipes'])
    k = r.choice(keys)
    c['item'] = r.choice([k[0], k[0], r.choice([1, 2, 3, 4, 5, 6, 7, 9])])
    c['rate'] = k[1] if r.random() < 0.8 else r.choice([100, 70, 60])
    c['need'] = r.randint(0, 9)
    c['parent'] = r.choice([0, 0x9001])
    return c


class IndexEmu(DataEmu):
    """GetRecipeDataByIndex answers with the recipes in key order (the tree window's SetRecipe asks by index)"""
    def do_import(self, nm):
        if nm == '?GetRecipeDataByIndex@FL2GameData@@QAEPAUFL2RecipeData@@H@Z':
            idx = self.arg(0)
            keys = list(self.recipes)
            self.log.append(('GetRecipeDataByIndex', idx))
            return self.recipes[keys[idx]] if 0 <= idx < len(keys) else 0
        return super().do_import(nm)


def run_case_set(emu_cls, label, n, r, mk, setup_fn, stock_fn, ours_pre, pre=None, snapshot=False):
    cases = [mk(r) for _ in range(n)]
    res = {}
    for kind in ('stock', 'ours'):
        e = emu_cls(kind)
        e.heap_mark = e.heap_ptr
        # the NItemEntry constructor (the window's member is made by the stock constructor; here by hand)
        e.itementry_ctor = 0x1001A850 if kind == 'stock' else sym(e, '??0NItemEntry@@QAE@XZ')
        fn = stock_fn if kind == 'stock' else sym(e, ours_pre)
        res[kind] = []
        for c in cases:
            try:
                res[kind].append(run_nodev(e, setup_fn(c), fn))
            except Exception as ex:
                res[kind].append(('error', type(ex).__name__, str(ex)[:200]))
    bad = 0
    crashes = 0
    for i, (a, b) in enumerate(zip(res['stock'], res['ours'])):
        if a[0] == 'error' and b[0] == 'error':
            crashes += 1
            if crashes == 1:
                print(label, 'a crash, for the record:', a[1], a[2][:200], '| ours:', b[1], b[2][:200])
            continue
        # the device is not used by these functions: compare the return value, the log and the probe
        if a[2] != b[2] or a[3] != b[3]:   # (the return value of these void functions is whatever eax held)
            bad += 1
            if bad <= 2:
                print(label, 'case', i, {k: v for k, v in cases[i].items() if k not in ('items',)})
                print(' stock', str(a)[:1800])
                print(' ours ', str(b)[:1800])
    print(label, 'cases', n, 'differ', bad, '(both crash: %d)' % crashes)
    return bad


def main(n, seed):
    r = random.Random(seed)
    bad = 0
    bad += run_case_set(DataEmu, 'Manufacture SetRecipe', n, r, rand_set_case, lambda c: set_recipe_setup(c, None), 0x100C7DE0,
                        '?SetRecipe@NCRecipeBuyManufactureWnd@@QAEXPAUNRecipeData@@@Z')
    bad += run_case_set(IndexEmu, 'TreeWnd SetRecipe', n, r, rand_tree_set_case, tree_set_setup, 0x100C6E20, '?SetRecipe@NCRecipeTreeWnd@@')
    bad += run_case_set(DataEmu, 'TreeCtrl AddRecipe', n, r, rand_add_case, add_recipe_setup, 0x100C6AF0, '?AddRecipe@NCRecipeTreeCtrl@@')
    return bad


if __name__ == '__main__':
    sys.exit(1 if main(int(sys.argv[1]), int(sys.argv[2])) else 0)
