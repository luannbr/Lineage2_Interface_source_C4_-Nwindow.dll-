# Differential tests of the C4 action grid, NCActionItemWnd (stock against ours under unicorn; same fake world as t_skillitem.py):
# OnLButtonUp 0x1004EC10 (the action runs; dropped on the shortcut bar; dropped on a line of the macro editor), UseAction 0x1004D240,
# OnMouseMove 0x1004D0F0, OnLButtonDown 0x1004EB90, HitTest 0x1004C370, ClearSelection 0x1004D270, AddItem 0x10050040, ClearItems 0x100505A0,
# OnDestroy 0x10050BB0, OnPaint 0x1004CF20.
#   python t_actionitem.py [cases] [seed] [lbup,use,mmove,lbdown,hit,clearsel,add,clear,destroy,paint]      (cov: coverage of the stock functions)
import sys, os, random, struct, copy
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import t_invdrop as D
from t_invdrop import *
import t_skillitem as S
import gw
import stubs
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *

gw.VPOP.update({27: 8, 28: 0, 74: 0, 76: 0, 79: 0})
stubs.IMPORT_POP.update({'?GetCmdStringByActionID@FL2GameData@@QAEPAGH@Z': 4, '?appMemset@@YAXPAXHH@Z': 0})

FUNCS = {'lbup': (0x1004EC10, 865), 'use': (0x1004D240, 41), 'mmove': (0x1004D0F0, 328), 'lbdown': (0x1004EB90, 117), 'hit': (0x1004C370, 118),
         'clearsel': (0x1004D270, 44), 'add': (0x10050040, 186), 'clear': (0x100505A0, 94), 'destroy': (0x10050BB0, 150), 'paint': (0x1004CF20, 410)}
EXTRA = {
    0x1004C360: ('GetActionID', 0, 0), 0x1007E8D0: ('ConsoleUseAction', 0xC, 0), 0x10074E60: ('PlaySound', 4, 0), 0x1001E580: ('ItemSelect', 0, 0),
    0x1001E5A0: ('ItemUnselect', 0, 0), 0x10050030: ('ItemDtor', 0, 0), 0x1004FF60: ('ActionCtor', 4, 1), 0x10030770: ('ToolTipFill', 0xC, 0),
    0x10052F40: ('AddChat', 0x10, 0), 0x100A9E20: ('HitLine', 8, 0), 0x100AA520: ('ClearMarks', 0, 0), 0x100AA5D0: ('ClearMark', 4, 0),
    0x1000E540: ('InsertText', 8, 0), 0x1002B570: ('SetCursor', 8, 0), 0x1002B580: ('LockCursor', 0, 0), 0x1016EC20: ('GetFirstDword', 0, 0),
    0x10013330: ('SetClip', 0x10, 0), 0x10013340: ('ReleaseClip', 0, 0), 0x10012D90: ('DrawTexture', 0x2C, 0), 0x10002E40: ('ToolTipDtor', 0, 0),
    0x10036160: ('BaseOnDestroy', 0, 0),
}
CAL = dict(D.CALLEES)
CAL.update(EXTRA)
OURS = {0x1004FF60: '??0NActionItem@@QAE@ABUNItemInfo@@@Z', 0x10012D90: '?DrawTexture@NCGDevice@@QAEXHHHHHHHHPAVUTexture@@KH@Z',
        0x10013330: '?SetClip@NCGDevice@@QAEXHHHH@Z', 0x10013340: '?ReleaseClip@NCGDevice@@QAEXXZ', 0x1000E540: '?InsertText@NCEditBox@@QAEXPBGH@Z'}
GCMDA = 0x1023AA84          # this unit's pointer to the "/" of an action's command


class AW(S.SW):
    def __init__(self, kind):
        D.IW.__init__(self, kind, CAL, OURS)
        # the pieces SW.__init__ builds (its own table of callees differs: the same code, this table)
        self.setup_world()

    def setup_world(self):
        for slot in range(256, 330):
            stub = STUB_BASE + 0xE000 + 16 * slot
            self.uc.mem_write(stub, bytes([0xC3]))
            self.stubs[stub] = ('vc:%d' % slot, None)
        self.vrets[28] = lambda e, t, a: e.world['focus']
        self.vrets[74] = lambda e, t, a: None
        self.vrets[76] = lambda e, t, a: None
        self.vrets[27] = lambda e, t, a: 0
        self.write32(self.glob(GCMDA), self.put_wstr('/'))
        self.chat_obj = self.alloc(0x100)

    def do_import(self, nm):
        if nm == '?GetCmdStringByActionID@FL2GameData@@QAEPAGH@Z':
            v = self.rds32(self.reg(UC_X86_REG_ESP) + 4)
            self.log.append(('GetCmdStringByActionID', v))
            return self.static_str(self.world['cmds'].get(v, 'cmd%d' % v))
        if nm == '?appMemset@@YAXPAXHH@Z':
            p, val, n = self.arg(0), self.arg(1), self.arg(2)
            self.uc.mem_write(p, bytes([val & 0xFF]) * n)
            self.log.append(('appMemset', val, n))
            return None
        return D.IW.do_import(self, nm)

    def service(self, nm, t, a):
        log = self.log
        w = self.world
        if nm == 'GetActionID':
            log.append((nm, self.nm(t)))
            return self.read32(t + 0x1C40)
        if nm == 'ConsoleUseAction':
            log.append((nm, self.nm(t), a))
            return None
        if nm == 'ItemUnselect':
            log.append((nm, self.nm(t)))
            self.write32(t + 0x1B5C, 0)
            return 0
        if nm == 'ActionCtor':
            log.append((nm, self.nm(t), self.nm(a[0])))
            return t
        if nm == 'AddChat':
            log.append((nm, self.nm(t), self.wstr(a[0]), a[1:]))
            return None
        if nm == 'ToolTipDtor':
            log.append((nm, self.nm(t)))
            return None
        if nm == 'BaseOnDestroy':
            log.append((nm, self.nm(t)))
            return 1
        return S.SW.service(self, nm, t, a)


def f32(x):
    return struct.unpack('<I', struct.pack('<f', float(x)))[0]


def run(e, A, c):
    mark = len(e.log)
    f = c['f']
    e.case = c
    e.params.clear()
    for k in D.CLS:
        e.uc.mem_write(e.cls(k), bytes(0x60))
    ptrs = []
    for k, it in enumerate(c['items']):
        p = put_item(e, it, k)
        e.write32(p + 0x1C40, it['action'])
        ptrs.append(p)
    arr = e.alloc(4 * 32)
    for i, p in enumerate(ptrs):
        e.write32(arr + 4 * i, p)
    if c['nullitem'] is not None and 0 <= c['nullitem'] < len(ptrs):
        e.write32(arr + 4 * c['nullitem'], 0)
    w, ent = make_window(e, 'wnd', 0, 0x300)
    e.write32(w, e.vt)
    e.write32(w + 0xE8, arr)
    e.write32(w + 0xEC, len(ptrs))
    e.write32(w + 0xF0, c['arrmax'] if f != 'add' else len(ptrs))
    e.write32(w + 0x140, 0x1111)
    e.write32(w + 0x144, 0x1112)
    e.write32(w + 0x14C, c['pressed'])
    e.write32(w + 0x150, c['dragging'])
    e.write32(w + 0x154, c['cols'])
    e.write32(w + 0x158, c['rows'])
    e.write32(w + 0x15C, c['maxitems'])
    e.write32(w + 0x160, c['icon'])
    e.write32(w + 0x164, c['pidx'] & 0xFFFFFFFF)
    e.write32(w + 0x168, 0x77777777)
    e.write32(w + 0x44, c['sx'])
    e.write32(w + 0x48, c['sy'])
    e.write32(w + 0x4C, f32(c['width']))
    e.write32(w + 0x50, f32(c['height']))
    for o, nme in ((0x1111, 'frametex'), (0x1112, 'pressedtex')):
        e.objs[o] = nme
    editor, ed_ent = make_window(e, 'editor', 0, 0x300)
    elist = e.alloc(0x200)
    e.uc.mem_write(elist, bytes(0x200))
    e.write32(editor + 0xEC, elist)
    edits = e.alloc(4 * 8)
    e.write32(elist + 0x120, edits)
    edit_objs = []
    for k in range(4):
        eo, _ = make_window(e, 'edit%d' % k, 0, 0x300)
        e.write32(eo, e.vt)
        e.write32(eo + 0x228, 0x5151)
        e.write32(edits + 4 * k, eo)
        edit_objs.append(eo)
    fwnd = 0
    found = 0
    if c['found'] is not None:
        fwnd, found = make_window(e, 'found', make_class(e, c['found'], c['depth']) if c['found'] != 'nullclass' else 0)
        if c['wnd_null']:
            e.write32(found + 4, 0)
        par = c['fparent']
        if par == 'one':
            p1 = e.alloc(0x10)
            e.uc.mem_write(p1, bytes(0x10))
            e.write32(found + 0xC, p1)
        elif par in ('editor', 'other'):
            p1 = e.alloc(0x10)
            p2 = e.alloc(0x10)
            e.uc.mem_write(p1, bytes(0x10))
            e.uc.mem_write(p2, bytes(0x10))
            e.write32(found + 0xC, p1)
            e.write32(p1 + 0xC, p2)
            e.write32(p2 + 4, editor if par == 'editor' else 0)
    desktop = e.alloc(0x200)
    e.uc.mem_write(desktop, bytes(0x200))
    e.write32(desktop + 0x154, editor if c['desk_fc'] == 'editor' else (0 if c['desk_fc'] == 'null' else fwnd or 0x4242))
    e.write32(desktop + 0xF4, e.chat_obj)
    e.objs[e.chat_obj] = 'chat'
    e.write32(e.console + 0x3BF4, desktop if c['desk'] else 0)
    e.fs_set(e.gdata + 0x80E7C, 'NoMacro')
    cmds = {i: ('A' * 3 if i % 3 else 'B' * 40) for i in range(-1, 1200)}
    world = dict(found_entry=found, focus=c['focus'], hit=c['hit'], line=c['line'], data=0, user=0, cmds=cmds, ctrl=0, shift=0, toggle=0, abnormal=0)
    e.world = world
    args = []
    dev = info = 0
    if f in ('lbup', 'mmove', 'lbdown'):
        args = [0, (c['y'] << 16) | c['x']]
    elif f == 'hit':
        args = [c['x'] & 0xFFFF, c['y'] & 0xFFFF]
    elif f == 'paint':
        dev = e.alloc(0x80)
        e.uc.mem_write(dev, bytes([0x5A]) * 0x80)
        e.objs[dev] = 'device'
        args = [dev]
    elif f == 'add':
        info = e.alloc(0x19B4)
        e.uc.mem_write(info, bytes(0x19B4))
        e.objs[info] = 'info'
        args = [info]
    ret = e.call(A, this=w, args=args)
    out = dict(ret=None if f in ('use', 'clearsel', 'clear', 'destroy') else ret, wnd=e.snapshot(w, 0x1A0),
               order=[e.nm(e.read32(e.read32(w + 0xE8) + 4 * i)) for i in range(e.read32(w + 0xEC))],
               sel=[e.read32(p + 0x1B5C) for p in ptrs], edit=[e.read32(eo + 0x228) for eo in edit_objs], log=e.log[mark:])
    if dev:
        out['dev'] = [e.rds32(dev + o) for o in (0x38, 0x3C, 0x54, 0x58, 0x6C)] + [bytes(e.uc.mem_read(dev, 0x80)).hex()]
    return out


def rand_case(r, f):
    n = r.choice([0, 1, 2, 3, 5, 8, 15])
    if f == 'use':
        n = max(n, 1)       # (an empty page with a pressed index reads the array's NULL data: a crash in both)
    items = []
    for i in range(n):
        items.append(dict(name='Act%d' % i, display='', objid=600 + i, id=i, count=1, stk=0, icon=r.choice([0, 0x7100 + i]),
                          selected=r.choice([0, 1]), action=r.choice([-1, 0, 5, 14, 15, 20, 27, 28, 0x14, 0x1B, 0x20, 0x24, 0x27, 0x29, 0x30, 0x31, 0x32, 999, 1000, 2000,
                                                                         r.randint(0, 60)])))
    c = dict(f=f, items=items)
    c['nullitem'] = r.randint(0, n - 1) if (n and r.random() < 0.05 and f in ('paint', 'use')) else None
    c['pidx'] = r.randint(0, n - 1) if (n and r.random() < 0.85) else r.randint(-2, n + 1)
    if f == 'use' and c['pidx'] >= n:
        # an index past the end reads a stale pointer of the heap in the stock and in ours alike (both fault: seed 2 case 10, in the hook mode too);
        # the game never presses an icon that is not there, so there is nothing to compare
        c['pidx'] = r.randint(0, n - 1)
    c['dragging'] = r.choice([0, 1, 1])
    c['pressed'] = r.choice([0, 1, 1])
    c['cols'] = r.choice([1, 3, 5, 5])
    c['rows'] = r.choice([1, 2, 3])
    c['maxitems'] = r.choice([n, n + 1, 15, 3])
    c['arrmax'] = n + r.choice([0, 4])
    c['icon'] = r.choice([32, 32, 30])
    c['sx'] = r.randint(0, 500)
    c['sy'] = r.randint(0, 400)
    c['width'] = r.choice([200.0, 100.5, 0.0])
    c['height'] = r.choice([90.0, 50.25, 0.0])
    c['x'] = r.choice([0, 1, 100, 0xFFFF, r.randint(0, 800), c['sx'] + r.randint(0, 220)])
    c['y'] = r.choice([0, 50, 0xFFFF, r.randint(0, 600), c['sy'] + r.randint(0, 120)])
    c['hit'] = 0
    c['focus'] = r.choice([0, 1, 1, 1])
    c['line'] = r.choice([-1, 0, 1, 3])
    c['found'] = r.choice([None, 'ShortCut', 'ShortCut', 'EditBox', 'EditBox', 'EditBox', 'Other', 'nullclass'])
    c['depth'] = r.choice([0, 0, 1, 2])
    c['wnd_null'] = 1 if r.random() < 0.06 else 0
    c['fparent'] = r.choice(['none', 'one', 'editor', 'editor', 'editor', 'other'])
    c['desk'] = r.choice([1, 1, 1, 0])
    c['desk_fc'] = r.choice(['editor', 'editor', 'null', 'other'])
    c['slot'] = r.choice([-1, 0, 3])
    c['notex'] = 0
    c['nomalloc'] = r.choice([0] * 7 + [1]) if f == 'add' else 0
    c['skillid'] = 0
    c['ptin'] = []
    c['inwin'] = 0
    if f == 'lbup' and r.random() < 0.5:
        # a drop on a line of the macro editor, away from the pressed icon
        c['found'], c['fparent'], c['desk'], c['desk_fc'], c['dragging'] = 'EditBox', 'editor', 1, 'editor', 1
        c['line'] = r.choice([0, 1, 3])
        c['wnd_null'] = 0
        c['x'], c['y'] = 0xFFFF, 0xFFFF
        if items:
            c['pidx'] = r.randint(0, n - 1)
    if f in ('hit', 'mmove', 'lbdown'):
        if r.random() < 0.3:
            c['x'] = c['sx'] + c['cols'] * (c['icon'] + 5) + r.choice([0, 0, -1, 1])
        if r.random() < 0.3:
            c['y'] = c['sy'] + c['rows'] * (c['icon'] + 3) + r.choice([0, 0, -1, 1])
    return c


def coverage(which, n, seed):
    import pefile
    from capstone import Cs, CS_ARCH_X86, CS_MODE_32
    st = pefile.PE(os.path.join(ROOT, 'orig', 'NWindow.dll'))
    base = st.OPTIONAL_HEADER.ImageBase
    mem = st.get_memory_mapped_image()
    md = Cs(CS_ARCH_X86, CS_MODE_32)
    r = random.Random(seed)
    for f in which:
        va, size = FUNCS[f]
        es = AW('stock')
        es.heap_mark = es.heap_ptr
        hit = set()
        es.uc.hook_add(UC_HOOK_CODE, lambda uc, a, s_, u: hit.add(a), None, va, va + size)
        for _ in range(n):
            es.heap_ptr = es.heap_mark
            try:
                run(es, va, rand_case(r, f))
            except Exception as ex:
                pass
            es.log.clear()
        allins = [i.address for i in md.disasm(bytes(mem[va - base:va - base + size]), va)]
        miss = [a for a in allins if a not in hit]
        print('%-8s stock instructions %d covered %d missed %d %s' % (f, len(allins), len(allins) - len(miss), len(miss), [hex(a) for a in miss[:25]]))


def main():
    if len(sys.argv) > 1 and sys.argv[1] == 'cov':
        coverage(sys.argv[4].split(',') if len(sys.argv) > 4 else list(FUNCS), int(sys.argv[2]) if len(sys.argv) > 2 else 300,
                 int(sys.argv[3]) if len(sys.argv) > 3 else 1)
        return
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 300
    seed = int(sys.argv[2]) if len(sys.argv) > 2 else 1
    which = sys.argv[3].split(',') if len(sys.argv) > 3 else list(FUNCS)
    es = AW('stock')
    eo = AW('ours')
    lm = log_map()
    es.heap_mark, eo.heap_mark = es.heap_ptr, eo.heap_ptr
    r = random.Random(seed)
    total = 0
    for f in which:
        va = FUNCS[f][0]
        if va not in lm:
            print('%-8s: not hooked in our build, skipped' % f)
            continue
        cases = [rand_case(r, f) for _ in range(n)]
        total += compare(f, es, eo, run, cases, va, lm[va])
    print('TOTAL problems', total)
    sys.exit(1 if total else 0)


if __name__ == '__main__':
    main()
