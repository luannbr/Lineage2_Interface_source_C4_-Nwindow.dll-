# Differential tests of the C4 skill grid, NCSkillItemWnd (stock against ours under unicorn; same fake world as t_invdrop.py / t_invpage.py):
# OnLButtonUp 0x100ADDE0 (the skill is used; dropped on the shortcut bar or on a line of the macro editor), OnMouseMove 0x100ADC10,
# OnPaint 0x100AD8A0, OnLButtonDown 0x100AD7B0, AddSkill 0x100AE800, Clear 0x100AE920, OnDestroy 0x100AE730 and GetCoolTime 0x100AE140.
# The console's virtual 319 (the Ctrl test), the input's KeyDown, the engine chain GConsole -> Engine -> Client -> Viewports(0) -> Input, the
# desktop's macro editor, the cool time icons' getters and the device are services; the objects are memory blocks with the stock layouts.
#   python t_skillitem.py [cases] [seed] [lbup,mmove,paint,lbdown,add,clear,destroy,cool]      (cov: coverage of the stock functions)
import sys, os, random, struct, copy
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import t_invdrop as D
from t_invdrop import *
import gw
import stubs
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *

gw.VPOP.update({27: 8, 28: 0, 76: 0, 79: 0, 319: 0})
stubs.IMPORT_POP.update({'?KeyDown@UInput@@QAEEH@Z': 4})
D.CLS.update({'EditBox': 0x102BABC0})

FUNCS = {'lbup': (0x100ADDE0, 813), 'mmove': (0x100ADC10, 403), 'paint': (0x100AD8A0, 820), 'lbdown': (0x100AD7B0, 35), 'add': (0x100AE800, 281),
         'clear': (0x100AE920, 170), 'destroy': (0x100AE730, 147), 'cool': (0x100AE140, 118)}
EXTRA = {
    0x100AD7E0: ('HitTestSkill', 8, 0), 0x1001EFB0: ('BaseLDown', 8, 0), 0x10068050: ('SkillUse', 4, 0), 0x100A9E20: ('HitLine', 8, 0),
    0x100AA520: ('ClearMarks', 0, 0), 0x100AA5D0: ('ClearMark', 4, 0), 0x1000E540: ('InsertText', 8, 0), 0x1002B570: ('SetCursor', 8, 0),
    0x1002B580: ('LockCursor', 0, 0), 0x1016EC20: ('GetFirstDword', 0, 0), 0x10013330: ('SetClip', 0x10, 0), 0x10013340: ('ReleaseClip', 0, 0),
    0x10012D90: ('DrawTexture', 0x2C, 0), 0x10012E90: ('DrawBright', 0x28, 0), 0x1006A9E0: ('IsToggleSkill', 8, 0), 0x100747D0: ('HasAbnormal', 4, 0),
    0x10074E60: ('PlaySound', 4, 0), 0x1001F340: ('FlashTex', 0, 0), 0x1001ECD0: ('IsIdle', 0, 0), 0x1001F610: ('CoolTex', 0, 0),
    0x1001F470: ('CoolCtor', 0, 1), 0x1002B450: ('FindTexture', 8, 0), 0x1001F960: ('ItemCtor', 4, 1),
    0x10030770: ('ToolTipFill', 0xC, 0), 0x1001EA60: ('UpdateScrollRange', 0, 0), 0x10050030: ('ItemDtor', 0, 0), 0x1001E710: ('BaseOnDestroy', 0, 0), 0x1001FD80: ('BaseClear', 0, 0),
}
# 0x10043B50 (~NCCoolTimeIcon, which the stock's OnDestroy / Clear call for every icon) is a bare `ret` that many classes share; our code does not call it
# (NCSkillItemWnd.cpp DeleteCoolTimes), so it is no logging service any more: the stock runs its real empty body and there is nothing to compare
CAL = dict(D.CALLEES)
CAL.update(EXTRA)
OURS = {0x1002B450: '?FindTexture@NCObject@@QAEPAVUTexture@@PBGH@Z', 0x1001F960: '??0NItemEntry@@QAE@PAUNItemInfo@@@Z',
        0x10012D90: '?DrawTexture@NCGDevice@@QAEXHHHHHHHHPAVUTexture@@KH@Z', 0x10013330: '?SetClip@NCGDevice@@QAEXHHHH@Z',
        0x10013340: '?ReleaseClip@NCGDevice@@QAEXXZ', 0x1001F470: '??0NCCoolTimeIcon@@QAE@XZ', 0x1000E540: '?InsertText@NCEditBox@@QAEXPBGH@Z'}
GCMD = 0x10256B3C           # this unit's pointer to the "/" the macro lines start with


class SW(IW):
    def __init__(self, kind):
        IW.__init__(self, kind, CAL, OURS)
        self.rets['FindTexture'] = lambda e, t, a: 0 if (e.case['notex'] and e.wstr(a[0]) != 'NWindow.BlackTexture') else 0x7777 + len(e.wstr(a[0]) or '')
        # the console's own vtable: slot 319 (and the others up to it) are logging stubs
        for slot in range(256, 330):
            stub = STUB_BASE + 0xE000 + 16 * slot
            self.uc.mem_write(stub, bytes([0xC3]))
            self.stubs[stub] = ('vc:%d' % slot, None)
        self.cvt = self.alloc(0x400 * 2)
        for slot in range(0, 330):
            self.write32(self.cvt + 4 * slot, STUB_BASE + 0xE000 + 16 * slot)
        self.write32(self.console, self.cvt)
        self.vrets[28] = lambda e, t, a: e.world['focus']
        self.vrets[319] = lambda e, t, a: e.world['ctrl']
        self.vrets[76] = lambda e, t, a: None
        self.vrets[27] = lambda e, t, a: 0
        self.write32(self.glob(GCMD), self.put_wstr('/'))
        # GConsole -> Engine -> Client -> Viewports(0) -> Input
        self.client = self.alloc(0x100)
        self.vparr = self.alloc(0x40)
        self.viewport = self.alloc(0x200)
        self.input = self.alloc(0x100)
        self.write32(self.engine + 0x58, self.client)
        self.write32(self.client + 0x38, self.vparr)
        self.write32(self.vparr, self.viewport)
        self.write32(self.viewport + 0x78, self.input)
        self.objs[self.input] = 'input'

    def do_import(self, nm):
        if nm == '?KeyDown@UInput@@QAEEH@Z':
            self.log.append(('KeyDown', self.nm(self.this()), self.arg(0)))
            return self.world['shift']
        return IW.do_import(self, nm)

    def dispatch(self, name):
        if name == 'mf:malloc' and self.case.get('nomalloc'):
            self.last_pop = 8
            size = self.sargs(1)[0]
            self.log.append(('GMalloc.Malloc', size, 'NULL'))
            return 0
        return IW.dispatch(self, name)

    def service(self, nm, t, a):
        log = self.log
        w = self.world
        if nm == 'HitTestSkill':
            log.append((nm, self.nm(t), a))
            return u32(w['hit'])
        if nm == 'BaseLDown':
            log.append((nm, self.nm(t), a))
            return 1
        if nm == 'SkillUse':
            log.append((nm, self.nm(t), list(self.params.get(a[0], []))))
            return None
        if nm == 'HitLine':
            log.append((nm, self.nm(t), a))
            return u32(w['line'])
        if nm in ('ClearMarks',):
            log.append((nm, self.nm(t)))
            return None
        if nm == 'ClearMark':
            log.append((nm, self.nm(t), a[0]))
            return None
        if nm == 'InsertText':
            log.append((nm, self.nm(t), self.wstr(a[0]), a[1]))
            return None
        if nm == 'SetCursor':
            log.append((nm, a[0], self.nm(a[1])))
            return None
        if nm == 'LockCursor':
            log.append((nm,))
            return None
        if nm == 'GetFirstDword':
            return self.read32(t)
        if nm in ('SetClip', 'DrawTexture', 'DrawBright'):
            log.append((nm, [self.nm(x) for x in a]))
            return None
        if nm == 'ReleaseClip':
            log.append((nm,))
            return None
        if nm == 'IsToggleSkill':
            log.append((nm, self.nm(t), a))
            return w['toggle']
        if nm == 'HasAbnormal':
            log.append((nm, self.nm(t), a))
            return w['abnormal']
        if nm == 'PlaySound':
            log.append((nm, self.nm(t), a[0]))
            return None
        if nm in ('FlashTex', 'CoolTex'):
            log.append((nm, self.nm(t)))
            return 0x9100 + (t - self.cool_base) // 0x60 if nm == 'FlashTex' else 0x9200 + (t - self.cool_base) // 0x60
        if nm == 'IsIdle':
            log.append((nm, self.nm(t)))
            return self.read32(t + 0x50) & 1
        if nm == 'CoolCtor':
            log.append((nm, self.nm(t)))
            return t
        if nm == 'ItemCtor':
            log.append((nm, self.nm(t), self.nm(a[0])))
            return t
        if nm == 'ToolTipFill':
            log.append((nm, t - a[0], a[1], a[2]))
            return None
        if nm == 'UpdateScrollRange':
            log.append((nm, self.nm(t)))
            return None
        if nm == 'ItemDtor':
            log.append((nm, self.nm(t)))
            return None
        if nm == 'BaseClear':
            log.append((nm, self.nm(t)))
            return None
        if nm == 'BaseOnDestroy':
            log.append((nm, self.nm(t)))
            return 1
        return IW.service(self, nm, t, a)


def f32(x):
    return struct.unpack('<I', struct.pack('<f', float(x)))[0]


def run(e, A, c):
    mark = len(e.log)
    f = c['f']
    e.case = c
    e.params.clear()
    for k in D.CLS:
        e.uc.mem_write(e.cls(k), bytes(0x60))
    ptrs = [put_item(e, it, k) for k, it in enumerate(c['items'])]
    for p, it in zip(ptrs, c['items']):
        e.write32(p + 0x1B14, it['type'])
        e.write32(p + 0x1B24, it['level'])
    arr = e.alloc(4 * 32)
    for i, p in enumerate(ptrs):
        e.write32(arr + 4 * i, p)
    # the cool time icons (one per item, 0x60 apart so that the stubs can tell the index)
    e.cool_base = e.alloc(0x60 * 40)
    cool = []
    for i in range(len(ptrs)):
        p = e.cool_base + 0x60 * i
        e.uc.mem_write(p, bytes(0x60))
        e.write32(p + 0x3C, c['cool'][i % len(c['cool'])][0])
        e.write32(p + 0x40, c['cool'][i % len(c['cool'])][1])
        e.write32(p + 0x44, c['cool'][i % len(c['cool'])][2])
        e.write32(p + 0x50, c['cool'][i % len(c['cool'])][3])
        for o, v in zip((0x48, 0x4C, 0x50, 0x54), (0x11 + i, 0x22 + i, 0x33 + i, 0x44 + i)):
            if o != 0x50:
                e.write32(p + o, v)
        if c['cool'][i % len(c['cool'])][3] == 0:
            pass
        e.objs[p] = 'cool%d' % i
        cool.append(p)
    carr = e.alloc(4 * 32)
    for i, p in enumerate(cool):
        e.write32(carr + 4 * i, p)
    if c['nullcool'] is not None and c['nullcool'] < len(cool):
        e.write32(carr + 4 * c['nullcool'], 0)
    w, ent = make_window(e, 'wnd', 0, 0x300)
    e.write32(w, e.vt)
    e.write32(w + 0x12C, arr)
    e.write32(w + 0x130, len(ptrs))
    e.write32(w + 0x134, 32)
    e.write32(w + 0x138, c['dragging'])
    e.write32(w + 0x140, c['cols'])
    e.write32(w + 0x144, c['vrows'])
    e.write32(w + 0x148, c['toprow'])
    e.write32(w + 0x154, c['cell'])
    e.write32(w + 0x124, 0x5E1EC7)
    e.write32(w + 0x160, c['sel'] & 0xFFFFFFFF)
    e.write32(w + 0x164, 0x77777777)
    e.write32(w + 0x168, c['tex_back'])
    e.write32(w + 0x16C, c['tex_click'])
    e.write32(w + 0x170, 0x7001)
    e.write32(w + 0x174, c['pressed'])
    e.write32(w + 0x178, carr)
    e.write32(w + 0x17C, len(cool))
    e.write32(w + 0x180, 32)
    e.write32(w + 0x44, c['sx'])
    e.write32(w + 0x48, c['sy'])
    e.write32(w + 0x4C, f32(c['width']))
    e.write32(w + 0x50, f32(c['height']))
    for o, nme in ((0x5E1EC7, 'selecttex'), (0x7001, 'toggletex'), (0x1111, 'backtex'), (0x1112, 'clicktex')):
        e.objs[o] = nme
    # the macro editor and the window found under the mouse
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
    e.objs[elist] = 'editlist'
    found = 0
    fwnd = 0
    if c['found'] is not None:
        fwnd, found = make_window(e, 'found', make_class(e, c['found'], c['depth']) if c['found'] != 'nullclass' else 0)
        if c['wnd_null']:
            e.write32(found + 4, 0)
        par = c['fparent']
        if par == 'none':
            pass
        elif par == 'one':
            p1 = e.alloc(0x10)
            e.uc.mem_write(p1, bytes(0x10))
            e.write32(found + 0xC, p1)
        else:
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
    e.write32(e.console + 0x3BF4, desktop if c['desk'] else 0)
    e.objs[desktop] = 'desktop'
    e.fs_set(e.gdata + 0xA14E8, c['cmd'])
    world = dict(found_entry=found, focus=c['focus'], ctrl=c['ctrl'], shift=c['shift'], hit=c['hit'], line=c['line'], toggle=c['toggle'],
                 abnormal=c['abnormal'], data=0, user=0)
    e.world = world
    args = []
    dev = info = 0
    out_total = out_rem = 0
    if f in ('lbup', 'mmove', 'lbdown'):
        args = [0, (c['y'] << 16) | c['x']]
    elif f == 'paint':
        dev = e.alloc(0x80)
        e.uc.mem_write(dev, bytes([0x5A]) * 0x80)
        e.objs[dev] = 'device'
        args = [dev]
    elif f == 'add':
        info = e.alloc(0x19B4)
        e.uc.mem_write(info, bytes(0x19B4))
        e.write_wstr_at(info + 0x400, c['iconname'])
        e.write32(info + 0x1900, c['infotex'])
        e.objs[info] = 'info'
        args = [info]
    elif f == 'cool':
        out_total = e.alloc(16)
        out_rem = e.alloc(16)
        e.uc.mem_write(out_total, bytes([0xAB]) * 16)
        e.uc.mem_write(out_rem, bytes([0xCD]) * 16)
        args = [c['skillid'], out_total, out_rem]
    ret = e.call(A, this=w, args=args)
    out = dict(ret=None if f in ('clear', 'cool') else ret, wnd=e.snapshot(w, 0x184), order=[e.nm(e.read32(e.read32(w + 0x12C) + 4 * i)) for i in range(e.read32(w + 0x130))],
               sel=[e.read32(p + 0x1B5C) for p in ptrs], cools=[e.nm(e.read32(e.read32(w + 0x178) + 4 * i)) for i in range(e.read32(w + 0x17C))],
               cflags=[e.read32(p + 0x44) for p in cool], edit=[e.read32(eo + 0x228) for eo in edit_objs], log=e.log[mark:])
    if dev:
        out['dev'] = [e.rds32(dev + o) for o in (0x38, 0x3C, 0x54, 0x58, 0x6C)] + [bytes(e.uc.mem_read(dev, 0x80)).hex()]
    if info:
        out['info'] = e.read32(info + 0x1900)
    if out_total:
        out['cool'] = [bytes(e.uc.mem_read(out_total, 16)).hex(), bytes(e.uc.mem_read(out_rem, 16)).hex()]
    return out


def rand_case(r, f):
    n = r.choice([0, 1, 2, 3, 5, 8, 14])
    items = []
    for i in range(n):
        items.append(dict(name=r.choice(['Heal', 'Wind Strike', '']), display='', objid=500 + i, id=r.choice([1, 2, 3, 1217, 1218]), count=1,
                          stk=0, icon=r.choice([0, 0x7100 + i]), selected=r.choice([0, 0, 1]), type=r.choice([0, 0, 1, 2]),
                          level=r.choice([-1, 1, 5])))
    c = dict(f=f, items=items)
    c['sel'] = r.randint(0, n - 1) if (n and r.random() < 0.85) else r.randint(-2, n + 1)
    c['dragging'] = r.choice([0, 1, 1])
    c['pressed'] = r.choice([0, 1, 1])
    c['cols'] = r.choice([1, 3, 6, 6])
    c['vrows'] = r.choice([2, 4, 7, 8])
    c['toprow'] = r.choice([0, 0, 1])
    c['cell'] = r.choice([32, 32, 30])
    c['sx'] = r.randint(0, 500)
    c['sy'] = r.randint(0, 400)
    c['width'] = r.choice([239.0, 200.5, 0.0])
    c['height'] = r.choice([280.0, 100.25, 0.0])
    c['x'] = r.choice([0, 1, 100, 0xFFFF, r.randint(0, 800)])
    c['y'] = r.choice([0, 50, 0xFFFF, r.randint(0, 600)])
    c['hit'] = r.choice([-1, 0, 1, c['sel'], c['sel'], c['sel'], n - 1, n])
    c['focus'] = r.choice([0, 1, 1, 1])
    c['ctrl'] = r.choice([0, 1, 2])
    c['shift'] = r.choice([0, 1, 255])
    c['line'] = r.choice([-1, 0, 1, 3])
    c['toggle'] = r.choice([0, 0, 1])
    c['abnormal'] = r.choice([0, 1, 1])
    c['tex_back'] = r.choice([0, 0x1111])
    c['tex_click'] = r.choice([0, 0x1112])
    c['cool'] = [[r.choice([0, 1]), r.choice([0, 1]), r.choice([0, 1]), r.choice([0, 0, 1, 2])] for _ in range(3)]
    c['nullcool'] = r.choice([None, None, None, 0, 1]) if f == 'paint' and False else None
    c['found'] = r.choice([None, 'ShortCut', 'ShortCut', 'EditBox', 'EditBox', 'EditBox', 'Other', 'Equip', 'nullclass'])
    c['depth'] = r.choice([0, 0, 1, 2])
    c['wnd_null'] = 1 if r.random() < 0.06 else 0
    c['fparent'] = r.choice(['none', 'one', 'editor', 'editor', 'editor', 'other'])
    c['desk'] = r.choice([1, 1, 1, 0])
    c['desk_fc'] = r.choice(['editor', 'editor', 'null', 'other'])
    c['cmd'] = r.choice(['Skill', 'S' * 20, 'S' * 40])
    c['slot'] = r.choice([-1, 0, 3])
    c['notex'] = r.choice([0, 0, 0, 1])
    c['nomalloc'] = r.choice([0] * 7 + [1])
    c['iconname'] = r.choice(['', 'icon.skill', 'L2UI_CH3.skill.sk_a'])
    c['infotex'] = r.choice([0, 0x8100])
    c['skillid'] = r.choice([i['id'] for i in items] + [9999]) if items else 9999
    c['ptin'] = []
    c['hit'] = c['hit']
    c['inwin'] = 0
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
        es = SW('stock')
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
    es = SW('stock')
    eo = SW('ours')
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
