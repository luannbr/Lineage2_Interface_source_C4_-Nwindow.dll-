# Differential tests of the C4 multisell window's builders (OnOK, OnCreate, AddItem, AddNeeded, NCNameCtrl::SetItem): stock against ours
# under unicorn. The objects the stock creates with StaticAllocateObject are fake windows that log their virtual calls; the
# constructors, the engine's string / parameter-stack / item-table functions and the dialog are logging services.
#   python t_vipwnd3.py [cases] [seed] [ok,additem,addneeded,setitem,oncreate]
import sys, os, random, struct
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import tip4
from tip4 import *
import gw
import stubs
from t_tipfill import rand_item, put_item, ENTRY
from t_vipwnd import VW, setf, f32bits
from t_vipwnd2 import shop_world, shop_case, rand_rec, snap_shop

NWSHIFT = NWBASE - 0x10000000

stubs.IMPORT_POP.update({
    '?StaticAllocateObject@UObject@@SAPAV1@PAVUClass@@PAV1@VFName@@K1PAVFOutputDevice@@11@Z': 0,
    '?GetTransientPackage@UObject@@SAPAVUPackage@@XZ': 0,
    '??0L2ParamStack@@QAE@H@Z': 4, '?PushBack@L2ParamStack@@QAEHPAX@Z': 4, '?Top@L2ParamStack@@QAEPAXXZ': 0, '??1L2ParamStack@@QAE@XZ': 0,
    '?GetItemName@FL2GameData@@QAEPAGH@Z': 4, '?GetItemDescription@FL2GameData@@QAEPAGH@Z': 4, '??DL2FName@@QBEPBGXZ': 0,
    '??4FString@@QAEAAV0@PBG@Z': 4, '?ConditionalDestroy@UObject@@QAEHXZ': 0, '??1UObject@@UAE@XZ': 0,
})
gw.VPOP.update({25: 12, 40: 28, 46: 4, 75: 0, 3: 4})

tip4.OURS_SYMS.update({
    0x100085c0: '??0NDialogInfo@@QAE@XZ', 0x10007b20: '?SetParams@NDialogInfo@@QAEXHH@Z', 0x10007af0: '?SetParamText@NDialogInfo@@QAEXPBG@Z',
    0x100079b0: '?SetSystemMessage@NDialogInfo@@QAEXH@Z', 0x10072e20: '?ShowDialog@NConsoleWnd@@QAEXPAUNDialogInfo@@@Z',
    0x10007d50: '??1NDialogInfo@@QAE@XZ',
    0x1001fe00: '?Copy@NItemEntry@@QAEXPAU1@@Z', 0x1006c2e0: '?GetItemAdditionalName@NConsoleWnd@@QAEPBGH@Z',
    0x1002d260: None, 0x10035840: '?AddChildWindow@NCWnd@@QAEPAUNWndEntry@@PAV1@K@Z', 0x10006470: '?AttachEntry@NCWnd@@QAEXPAUNWndEntry@@@Z',
    0x1002d020: '?SetItem@NCNameCtrlView@@QAEXPAVL2ParamStack@@HH@Z',
    0x10011810: '?SetTitleID@NCFrameCtrl@@QAEXH@Z', 0x100014a0: '?SetTextID@NCButton@@QAEXH@Z',
    0x10010450: None,
    0x100a3640: '??0NCVIPShopItemWnd@@QAE@HHHHHH@Z', 0x100a3850: '??0NCVIPShopItemInfoWnd@@QAE@XZ',
    0x100a3790: '??0NCVIPShopNeededItemWnd@@QAE@XZ',
    0x10003340: '??0NCButton@@QAE@PBG0H@Z', 0x100112c0: '??0NCEditBox@@QAE@HPBGHHH@Z',
    0x100126e0: None, 0x1002b450: '?FindTexture@NCObject@@QAEPAVUTexture@@PBGH@Z',
    0x10032460: '?SetText@NToolTipInfo@@QAEXPBGH@Z', 0x10030770: '?Fill@NToolTipInfo@@QAEXPAUNItemEntry@@KK@Z',
    0x10002e40: '??1NToolTipInfo@@QAE@XZ',
    0x1002c040: '??0NCScrollWnd@@QAE@XZ', 0x10015950: '??1NCScrollWnd@@UAE@XZ', 0x10050030: '??1NToolTipInfo@@QAE@XZ',
    0x10032860: '?Draw@NToolTipSkin@@QAEXPAVNCGDevice@@UNToolTipInfo@@HHUNToolTipRect@@@Z',
    0x10032560: '?DrawFull@NToolTipSkin@@QAEXPAVNCGDevice@@UNToolTipInfo@@HHUNToolTipRect@@@Z',
    0x10002eb0: '??0NToolTipInfo@@QAE@ABU0@@Z',
})
tip4.OURS_SYMS = {k: v for k, v in tip4.OURS_SYMS.items() if v}

SVC_OK = {0x100085c0: ('DlgCtor', 0, 1), 0x10007b20: ('DlgSetParams', 8, 0), 0x10007af0: ('DlgSetParamText', 4, 0),
          0x100079b0: ('DlgSetSystemMessage', 4, 0), 0x10072e20: ('ShowDialog', 4, 0), 0x10007d50: ('DlgDtor', 0, 0)}
SVC_ADD = {0x1001fe00: ('ItemCopy', 4, 0), 0x1006c2e0: ('GetItemAdditionalName', 4, 0), 0x1002d260: ('NameCtrlCtor', 0, 1),
           0x10035840: ('AddChildWindow', 8, 0), 0x10006470: ('AttachEntry', 4, 0), 0x1002d020: ('SetItem', 0xC, 0)}
SVC_SETITEM = {0x1001f7f0: ('ItemCtor', 0, 1), 0x10032460: ('TipSetText', 8, 0), 0x10030770: ('TipFill', 0xC, 0), 0x10002e40: ('TipDtor', 0, 0)}
SVC_CREATE = {0x10011810: ('SetTitleID', 4, 0), 0x100014a0: ('SetTextID', 4, 0), 0x10035840: ('AddChildWindow', 8, 0),
              0x100a3640: ('ItemWndCtor', 0x18, 1), 0x100a3850: ('InfoWndCtor', 0, 1), 0x100a3790: ('NeededWndCtor', 0, 1),
              0x10003340: ('ButtonCtor', 0xC, 1), 0x100112c0: ('EditCtor', 0x14, 1), 0x100126e0: ('FrameCtor', 0xC, 1),
              0x1002b450: ('FindTexture', 8, 0)}

SVC_INFOCTOR = {0x1001f7f0: ('ItemCtorIdx', 0, 1), 0x1002c040: ('ScrollBaseCtor', 0, 1)}
SVC_INFODTOR = {0x10050030: ('EntryDtor', 0, 0), 0x10015950: ('BaseDtor', 0, 0)}
SVC_TOOLTIP = {0x1001e590: ('ItemIsSelected', 0, 0), 0x10032860: ('SkinDraw', 0x9C, 0), 0x10032560: ('SkinDrawFull', 0x9C, 0),
               0x10002eb0: ('TipCopy', 4, 1)}

CLASS_NAMES = {0x102bb108: 'frame', 0x102e0178: 'itemwnd', 0x102e06a8: 'infownd', 0x102dfc48: 'neededwnd', 0x102b5db8: 'button',
               0x102babc0: 'editbox', 0x102c4ee8: 'namectrl'}


class V3(VW):
    def __init__(self, kind, svc=None):
        VW.__init__(self, kind, svc)
        self.params = {}
        self.nobj = 0
        self.table = {}
        self.rets['GetItemAdditionalName'] = lambda e, t, a: (e.static_str('add%d' % a[0]) if a[0] % 3 else 0)
        self.rets['AddChildWindow'] = self.add_child

    def add_child(self, e, t, a):
        ent = e.alloc(0x40)
        e.uc.mem_write(ent, bytes(0x40))
        e.objs[ent] = 'entry%d' % self.nobj
        self.nobj += 1
        if self.case.get('parent_ent'):
            e.write32(ent + 0xC, self.case['parent_ent'])
        self.log.append(('AddChildWindow', self.nm(t), self.nm(a[0]), a[1], 'entry'))
        return ent

    def thiscall(self, nm, t, a):
        if nm == 'DlgCtor':
            self.uc.mem_write(t, bytes(0xA0))
            self.log.append(('DlgCtor',))
            return t
        if nm == 'DlgSetParams':
            self.log.append(('DlgSetParams', a[0], a[1]))
            return None
        if nm == 'DlgSetParamText':
            self.log.append(('DlgSetParamText', self.wstr(a[0]) if a[0] else None))
            return None
        if nm == 'DlgSetSystemMessage':
            self.log.append(('DlgSetSystemMessage', a[0]))
            return None
        if nm == 'ShowDialog':
            d = bytes(self.uc.mem_read(a[0], 0x20))
            self.log.append(('ShowDialog', struct.unpack_from('<8I', d, 0)))
            return None
        if nm == 'DlgDtor':
            self.log.append(('DlgDtor',))
            return None
        if nm == 'ItemCopy':
            self.uc.mem_write(t, bytes(self.uc.mem_read(a[0], 0x1BC0)))
            self.log.append(('ItemCopy', self.nm(t), self.nm(a[0])))
            return None
        if nm == 'NameCtrlCtor':
            self.log.append(('NameCtrlCtor',))
            return t
        if nm == 'AttachEntry':
            self.log.append(('AttachEntry', self.nm(t), self.nm(a[0])))
            return None
        if nm == 'SetItem':
            self.log.append(('SetItem', self.nm(t), list(self.params.get(a[0], [])), a[1], a[2]))
            return None
        if nm == 'GetItemAdditionalName':
            r = (self.static_str('add%d' % a[0]) if a[0] % 3 else 0)
            self.log.append(('GetItemAdditionalName', a[0]))
            return r
        if nm == 'TipSetText':
            self.log.append(('TipSetText', self.nm(t), self.wstr(a[0]), a[1]))
            return None
        if nm == 'TipFill':
            ent = bytes(self.uc.mem_read(a[0], 0x1C40))
            keys = [struct.unpack_from('<i', ent, o)[0] for o in (0x1B14, 0x1B20, 0x1B28, 0x1B44, 0x1B64)]
            self.log.append(('TipFill', self.nm(t), wcut(ent[0x14:0x214]), keys, a[1], a[2]))
            return None
        if nm == 'ItemCtorIdx':
            self.uc.mem_write(t, bytes(0x1C40))
            self.log.append(('ItemCtor', (t - self.entry_base) // 0x1C40, (t - self.entry_base) % 0x1C40))
            return t
        if nm == 'EntryDtor':
            base = t - (0x1BC0 if self.kind == 'ours' else 0)
            self.log.append(('EntryDtor', (base - self.entry_base) // 0x1C40, (base - self.entry_base) % 0x1C40))
            return None
        if nm == 'BaseDtor':
            self.log.append(('BaseDtor', self.nm(t)))
            return None
        if nm == 'TipCopy':
            self.uc.mem_write(t, bytes(self.uc.mem_read(a[0], 0x80)))
            self.log.append(('TipCopy', self.nm(a[0] - 0x1BC0)))
            return t
        if nm in ('SkinDraw', 'SkinDrawFull'):
            self.log.append((nm, self.nm(t), [self.val(x) for x in a]))
            return None
        if nm == 'TipDtor':
            self.log.append(('TipDtor',))
            return None
        if nm == 'SetTitleID':
            self.log.append(('SetTitleID', self.nm(t), a[0]))
            return None
        if nm == 'SetTextID':
            self.log.append(('SetTextID', self.nm(t), a[0]))
            return None
        if nm == 'ScrollBaseCtor':
            return t
        if nm in ('ItemWndCtor', 'InfoWndCtor', 'NeededWndCtor', 'ButtonCtor', 'EditCtor', 'FrameCtor'):
            args = []
            for x in a:
                v = self.val(x)
                args.append(v)
            if nm == 'ButtonCtor':
                args = [self.wstr(a[0]), self.wstr(a[1]), a[2]]
            self.log.append((nm, self.nm(t), args))
            return t
        return VW.thiscall(self, nm, t, a)

    def do_import(self, nm):
        t = self.this()
        if nm == '?StaticAllocateObject@UObject@@SAPAV1@PAVUClass@@PAV1@VFName@@K1PAVFOutputDevice@@11@Z':
            cls = self.arg(0)
            if self.kind == 'ours':
                cls = self.stockptr(cls)      # hook: NWBASE + rva -> stock VA; standalone: the PrivateStaticClass object -> its stock VA (sa.inv)
            name = CLASS_NAMES.get(cls, 'class%x' % cls)
            size = 0x60000 if name == 'infownd' else 0x800
            w = self.window('%s%d' % (name, self.nobj), size=size)
            self.nobj += 1
            self.log.append(('StaticAllocateObject', name))
            return w
        if nm == '?GetTransientPackage@UObject@@SAPAVUPackage@@XZ':
            return 0x7777
        if nm == '??1UObject@@UAE@XZ':
            return None
        if nm == '?ConditionalDestroy@UObject@@QAEHXZ':
            self.log.append(('ConditionalDestroy', self.nm(t)))
            return 1
        if nm == '??0L2ParamStack@@QAE@H@Z':
            self.params[t] = []
            self.log.append(('L2ParamStack', self.arg(0)))
            return t
        if nm == '?PushBack@L2ParamStack@@QAEHPAX@Z':
            v = self.arg(0)
            # a string pointer is logged by its text, a number by itself
            self.params.setdefault(t, []).append(('S:' + self.wstr(v)) if (0x60000000 <= v < 0x70000000 and self.objs.get(v) is None and self.is_text(v)) else self.val(v))
            return 1
        if nm == '?Top@L2ParamStack@@QAEPAXXZ':
            lst = self.params.get(t, [])
            v = lst.pop(0) if lst else 0
            if isinstance(v, str) and v.startswith('S:'):
                return self.static_str(v[2:])
            return v if isinstance(v, int) else 0
        if nm == '??1L2ParamStack@@QAE@XZ':
            self.params.pop(t, None)
            return None
        if nm == '?GetItemName@FL2GameData@@QAEPAGH@Z':
            return self.static_str('name%d' % self.arg(0))
        if nm == '?GetItemDescription@FL2GameData@@QAEPAGH@Z':
            return self.static_str('desc%d' % self.arg(0))
        if nm == '??DL2FName@@QBEPBGXZ':
            return self.static_str('icon%d' % self.rds32(t))
        return VW.do_import(self, nm)

    def snap(self, base, size):
        """the words of a block; a pointer into the heap the case allocated is shown relative to its start (the two emulators' heaps start
        at different addresses)"""
        out = []
        for v in self.snapshot(base, size):
            if isinstance(v, int) and self.heap_mark <= v < self.heap_mark + 0x2000000:
                out.append(('heap', v - self.heap_mark))
            else:
                out.append(v)
        return out

    def is_text(self, v):
        try:
            s = self.wstr(v, 80)
            return s is not None and all(32 <= ord(ch) < 0xFFFF for ch in s)
        except Exception:
            return False


# ---- OnOK ---------------------------------------------------------------------------------------------------------------------
def ok_case(r):
    c = shop_case(r)
    c['recs'] = [rc or rand_rec(random.Random(r.random())) or dict(list=0, nitems=0, items=[], needed=[]) for rc in c['recs']]
    c['has']['edit'] = True
    c['text'] = r.choice(['1', '25', '', '999999'])
    return c


def run_ok(e, A, c):
    mark = len(e.log)
    w = shop_world(e, c)
    e.vrets[75] = lambda ee, t, a: ee.static_str(c['text'])
    e.call(A[0], this=w, args=[])
    return dict(log=e.log[mark:], ret=e.reg(UC_X86_REG_EAX))


# ---- AddItem --------------------------------------------------------------------------------------------------------------------
def additem_case(r):
    ids = [57, 1001, 20, -1, 0, 4037, 5]
    c = dict(index=r.choice([0, 1, 7, 49, 50, 51]), cls=r.choice([0, 1, 2, 3]), found=r.random() < 0.85, src=rand_item(r),
             rec=[r.randint(0, 3) for _ in range(0x140)], etc=r.choice([0, 2, 0x10, 5]), n=r.choice([0, 3]),
             recwords=[r.choice([0, 1, 7, 100, -3, 55555]) for _ in range(0x180)], id=r.choice(ids))
    c['src']['id'] = c['id']
    return c


def build_table(e, c, id_):
    """the item table of the fake GL2GameData: one record for the id (when found), the stock's hash map layout"""
    rec = e.alloc(0x5E0)
    e.uc.mem_write(rec, bytes(0x5E0))
    for k in range(0x178):
        e.write32(rec + 8 + 4 * k, c['recwords'][k % len(c['recwords'])] & 0xFFFFFFFF)
    e.write32(rec + 4, c['cls'])
    e.write32(rec + 0x3C, 3)
    e.write32(rec + 0xA8, c['etc'])
    e.objs[rec] = 'record'
    pairs = e.alloc(12 * 4 + 16)
    for k in range(4):
        e.write32(pairs + 12 * k, 0xFFFFFFFF)
    hashp = e.alloc(4 * 64)
    for k in range(64):
        e.write32(hashp + 4 * k, 0xFFFFFFFF)
    if c['found']:
        e.write32(pairs, 0xFFFFFFFF)
        e.write32(pairs + 4, id_ & 0xFFFFFFFF)
        e.write32(pairs + 8, rec)
        e.write32(hashp + 4 * (id_ & 63), 0)
    tab = e.alloc(0x40)
    e.uc.mem_write(tab, bytes(0x40))
    e.write32(tab, pairs)
    e.write32(tab + 4, 1 if c['found'] else 0)
    e.write32(tab + 8, 4)
    e.write32(tab + 0xC, hashp)
    e.write32(tab + 0x10, 64)
    e.write32(e.gdata + 0x1818, tab)
    return rec


def run_additem(e, A, c):
    mark = len(e.log)
    w = e.window('info', size=0x58700)
    for i in range(50):
        e.objs[w + 0x120 + i * 0x1C40] = 'entry%d' % i
    build_table(e, c, c['id'])
    src = e.alloc(ENTRY + 16)
    put_item(e, c['src'], src)
    e.objs[src] = 'src'
    ent = w + 0x120 + (c['index'] % 50) * 0x1C40
    # the entry the stock Copy would have left: the source's id comes with it (ItemCopy copies the entry's start)
    e.write32(w + 0x585A0, c['n'])
    e.call(A[0], this=w, args=[c['index'], src])
    return dict(log=e.log[mark:], entry=bytes(e.uc.mem_read(ent, 0x1BC0)) if c['index'] < 50 else None,
                tail=e.snap(w + 0x585A0, 0xD4))


# ---- AddNeeded -------------------------------------------------------------------------------------------------------------------
def addneeded_case(r):
    return dict(k=r.choice([0, 1, 3, 12, 49]), src=dict(id=r.choice([57, 4037]), name=r.choice(['Adena', 'Coin', 'A long name']), count=r.choice([1, 100]),
                                                      icon=0x6400, ench=r.choice([0, 7]), grade=r.choice([0, 2]), type=r.choice([0, 1, 5, -1])),
                parent=r.random() < 0.6)


def run_addneeded(e, A, c):
    mark = len(e.log)
    w = e.window('needed', size=0x400)
    k = c['k']
    data = e.alloc(4 * k + 16)
    e.objs[data] = 'items0'
    for m in range(k):
        q = e.alloc(0x320)
        e.uc.mem_write(q, bytes(0x320))
        e.objs[q] = 'old%d' % m
        e.write32(data + 4 * m, q)
    e.write32(w + 0x120, data if k else 0)
    e.write32(w + 0x124, k)
    e.write32(w + 0x128, k)
    src = e.alloc(0x320)
    e.uc.mem_write(src, bytes(0x320))
    e.objs[src] = 'src'
    s = c['src']
    e.write32(src, s['id'])
    e.write_wstr_at(src + 4, s['name'])
    e.write32(src + 0x204, s['count'])
    e.write32(src + 0x208, s['icon'])
    e.write32(src + 0x30C, s['ench'])
    e.write32(src + 0x310, s['grade'])
    e.write32(src + 0x314, s['type'] & 0xFFFFFFFF)
    if c['parent']:
        pent = e.alloc(0x40)
        e.uc.mem_write(pent, bytes(0x40))
        pw = e.window('parentwnd', size=0x100)
        e.write32(pw + 0x44, 11)
        e.write32(pw + 0x48, 22)
        e.write32(pent + 4, pw)
        c['parent_ent'] = pent
    else:
        c['parent_ent'] = 0
    e.call(A[0], this=w, args=[src])
    n = e.read32(w + 0x124)
    last = e.read32(e.read32(w + 0x120) + 4 * (n - 1)) if n else 0
    kid = e.read32(w + 0x134 + 4 * (n - 1)) if n else 0
    return dict(log=e.log[mark:], hdr=e.snap(w + 0x120, 0x20), last=bytes(e.uc.mem_read(last, 0x318)) if last else None,
                kid=e.snap(kid, 0x100) if kid else None)


# ---- NCNameCtrl::SetItem -------------------------------------------------------------------------------------------------------------
def setitem_case(r):
    return dict(mode=r.choice([0, 1, 1, 2]), flag=r.choice([0, 1]), name=r.choice(['Sword', '', 'A name with spaces']),
                vals=[r.choice([57, 1001, 0]), r.choice([0, 1, 5, -1]), r.choice([0, 3]), r.choice([0, 4]), r.choice([0, 1, 99])])


def run_setitem(e, A, c):
    mark = len(e.log)
    ctrl = e.window('ctrl', size=0x300)
    e.objs[ctrl + 0x220] = 'tooltip'
    stack = e.alloc(0x20)
    e.uc.mem_write(stack, bytes(0x20))
    e.objs[stack] = 'stack'
    e.params[stack] = ['S:' + c['name']] + [v & 0xFFFFFFFF for v in c['vals']]
    e.call(A[0], this=ctrl, args=[stack, c['mode'], c['flag']])
    return dict(log=e.log[mark:], mem=e.snap(ctrl + 0x1F0, 0x40), left=list(e.params.get(stack, [])))


# ---- OnCreate --------------------------------------------------------------------------------------------------------------------------
def create_case(r):
    return dict(sx=r.choice([0, 100]), sy=r.choice([0, 50]), w=r.choice([100.0, 244.0, 513.5]), h=r.choice([20.0, 330.0]),
                parent=r.random() < 0.7)


def run_create(e, A, c):
    mark = len(e.log)
    w = e.window('shopwnd', size=0x400)
    setf(e, w, 0x4C, c['w'])
    setf(e, w, 0x50, c['h'])
    if c['parent']:
        pent = e.alloc(0x40)
        e.uc.mem_write(pent, bytes(0x40))
        pw = e.window('parentwnd', size=0x100)
        e.write32(pw + 0x44, 11)
        e.write32(pw + 0x48, 22)
        e.write32(pent + 4, pw)
        c['parent_ent'] = pent
    else:
        c['parent_ent'] = 0
    e.nobj = 0
    e.call(A[0], this=w, args=[])
    kids = {}
    for nm, off in (('frame', 0x7C), ('list', 0xE8), ('info', 0xEC), ('needed', 0xF0), ('edit', 0xF4), ('ok', 0xF8), ('cancel', 0xFC)):
        p = e.read32(w + off)
        kids[nm] = e.snapshot(p, 0x100) if p else None
    return dict(log=e.log[mark:], ret=e.reg(UC_X86_REG_EAX), mem=e.snapshot(w + 0x70, 0xA0), kids=kids)


def infoctor_case(r):
    return dict(fill=r.choice([0, 0xCC]))


def run_infoctor(e, A, c):
    mark = len(e.log)
    w = e.window('info', size=0x58700)
    e.uc.mem_write(w + 0x120, bytes([c['fill']]) * 0x58000)
    e.write32(w + 0x585A0, 77)
    e.entry_base = w + 0x120
    e.call(A[0], this=w, args=[])
    return dict(log=e.log[mark:], n=e.read32(w + 0x585A0))


def run_infodtor(e, A, c):
    mark = len(e.log)
    w = e.window('info', size=0x58700)
    e.entry_base = w + 0x120
    e.call(A[0], this=w, args=[])
    return dict(log=[x for x in e.log[mark:] if x[0] == 'EntryDtor'])


def tooltip_case(r):
    n = r.choice([0, 1, 4, 20])
    return dict(cols=r.choice([1, 3, 6]), top=r.choice([0, 1, 2]), cell=r.choice([32, 40]), gx=r.choice([0, 5]), gy=r.choice([0, 3]), n=n,
                hover=r.choice([-1, 0, 1, 3, 7, 19, 25]), sel=[r.random() < 0.4 for _ in range(n)], skin=r.random() < 0.85,
                sx=r.choice([0, 100]), sy=r.choice([0, 50]), w=r.choice([100.0, 244.5]), h=r.choice([20.0, 330.0]), pat=r.randint(1, 250))


def run_tooltip(e, A, c):
    mark = len(e.log)
    w = e.window('grid', size=0x400)
    e.write32(w + 0x44, c['sx'])
    e.write32(w + 0x48, c['sy'])
    setf(e, w, 0x4C, c['w'])
    setf(e, w, 0x50, c['h'])
    e.write32(w + 0x140, c['cols'])
    e.write32(w + 0x148, c['top'])
    e.write32(w + 0x154, c['cell'])
    e.write32(w + 0x158, c['gx'])
    e.write32(w + 0x15C, c['gy'])
    e.write32(w + 0x164, c['hover'] & 0xFFFFFFFF)
    skin = e.window('skin', size=0x100)
    e.write32(w + 0x8C, skin if c['skin'] else 0)
    n = c['n']
    data = e.alloc(4 * max(n, 1) + 16)
    e.objs[data] = 'itemarray'
    for k in range(n):
        p = e.alloc(ENTRY + 16)
        e.uc.mem_write(p, bytes(ENTRY))
        e.uc.mem_write(p + 0x1BC0, bytes([(c['pat'] + k) & 0xFF]) * 0x80)
        e.write32(p + 0x1B5C, 1 if c['sel'][k] else 0)
        e.objs[p] = 'item%d' % k
        e.write32(data + 4 * k, p)
    e.write32(w + 0x12C, data if n else 0)
    e.write32(w + 0x130, n)
    dev = e.window('dev', size=0x100)
    e.call(A[0], this=w, args=[dev])
    return dict(log=e.log[mark:], ret=e.reg(UC_X86_REG_EAX), dev=e.snapshot(dev, 0x100))


TESTS = {
    'infoctor': (0x100a3850, 0, infoctor_case, run_infoctor, SVC_INFOCTOR),
    'infodtor': (0x100a38f0, 0, infoctor_case, run_infodtor, SVC_INFODTOR),
    'tooltip': (0x10020370, 1, tooltip_case, run_tooltip, SVC_TOOLTIP),
    'ok': (0x100a21b0, 0, ok_case, run_ok, SVC_OK),
    'additem': (0x100a26c0, 2, additem_case, run_additem, SVC_ADD),
    'addneeded': (0x100a2360, 1, addneeded_case, run_addneeded, SVC_ADD),
    'setitem': (0x1002d020, 3, setitem_case, run_setitem, SVC_SETITEM),
    'oncreate': (0x100a3a20, 0, create_case, run_create, SVC_CREATE),
}


def main():
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 100
    seed = int(sys.argv[2]) if len(sys.argv) > 2 else 1
    which = sys.argv[3].split(',') if len(sys.argv) > 3 else list(TESTS)
    lm = log_map()
    total = 0
    for name in which:
        va, nargs, mk, run, svc = TESTS[name]
        ova = sa_pick(lm, va, name)      # (standalone mode: None = no standalone function, SKIP line printed)
        if ova is None:
            continue
        r = random.Random(seed)
        es = V3('stock', svc)
        eo = V3('ours', svc)
        es.heap_mark, eo.heap_mark = es.heap_ptr, eo.heap_ptr
        cases = [mk(r) for _ in range(n)]

        def runner(e, A, c, run=run):
            e.case = c
            e.nobj = 0
            e.params = {}
            return run(e, A, c)
        total += compare(name, es, eo, runner, cases, (va, nargs), (ova, nargs))
    print('TOTAL problems', total)


if __name__ == '__main__':
    main()
