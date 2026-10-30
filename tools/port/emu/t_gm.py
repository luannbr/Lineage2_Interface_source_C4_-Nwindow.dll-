# STALE (C4, sa-emu agent `old`): the C4 GM painters are in win/t_oldwnd.py (NCGMEquipItemWnd / NCGMInventoryWnd / NCGMPledgeWnd OnPaint); the OnCreate / button tests have no C4 successor; C2 model, does not run.
"""Differential tests of the GM windows (stock against ours, unicorn): the paint functions (NCGMEquipItemWnd::OnPaint
0x10073cd0, NCGMInventoryWnd::OnPaint 0x10074250, NCGMPledgeWnd::OnPaint 0x10076800), GetEquipSlot (0x10072e80), the
OnCreate functions (NCGMWnd 0x100770a0, NCGMInventoryWnd 0x10078570, NCGMActionWnd 0x10076c70), the name searches of
NCGMWnd (OnNewButton4..6) and the privileges buttons of NCPledgeWnd (0x10042920, 0x100429c0). The calls the stubs see
(and the memory a creation leaves) must be equal.
usage: python tools/port/emu/t_gm.py <cases> <seed> [paint|create|buttons|all]"""
import sys, random
import os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from t_fill import *
import stubs
from t_equippaint import make_wnd as make_equip_wnd

# services stubbed: name -> (stock va, pop, (class, method) of ours)
SERV = {
    'FindTexture': (0x10025290, 4, ('@NCObject@@', '?FindTexture@')),
    'FormatNumber': (0x1002bae0, 8, ('@NCWnd@@', '?FormatNumber@')),
    'GetAdena': (0x1007c440, 0, ('@NCInvenItemWnd@@', '?GetAdena@')),
    'GetServerID': (0x10056680, 0, ('@NConsoleWnd@@', '?GetServerID@')),
    'WndOnCreate': (0x1002c9e0, 0, ('@NCWnd@@', '?OnCreate@')),
    'SetWindowRect': (0x1002c910, 0x10, ('@NCWnd@@', '?SetWindowRect@')),
    'AddChildWindow': (0x1002c820, 8, ('@NCWnd@@', '?AddChildWindow@')),
    'ctorFrame': (0x1000eb80, 4, ('@NCFrameCtrl@@', '??0NCFrameCtrl@@QAE@H@Z')),
    'ctorEdit': (0x1000d5d0, 0x14, ('@NCEditBox@@', '??0NCEditBox@@QAE@')),
    'ctorButton3': (0x10002440, 0xC, ('@NCButton@@', '??0NCButton@@QAE@PBG')),
    'ctorButton0': (0x10002340, 0, ('@NCButton@@', '??0NCButton@@QAE@XZ')),
    'ctorTab': (0x10027270, 0, ('@NCTabControl@@', '??0NCTabControl@@QAE@XZ')),
    'AddTab': (0x10026c80, 0x20, ('@NCTabControl@@', '?AddTab@')),
    'ctorGMEquip': (0x100783f0, 4, ('@NCGMEquipItemWnd@@', '??0NCGMEquipItemWnd@@QAE@H@Z')),
    'ctorGMInven': (0x10078540, 0x10, ('@NCGMInvenItemWnd@@', '??0NCGMInvenItemWnd@@QAE@HHHH@Z')),
    'ctorGMPledge': (0x10076760, 0, ('@NCGMPledgeWnd@@', '??0NCGMPledgeWnd@@QAE@XZ')),
    'SetTextID': (0x10001220, 4, ('@NCButton@@', '?SetTextID@')),
    'SetText': (0x10001250, 4, ('@NCButton@@', '?SetText@')),
    'SetPushedTex': (0x100012d0, 4, ('@NCButton@@', '?SetPushedTex@')),
    'DlgCtor': (0x10006e60, 0, ('@NDialogInfo@@', '??0NDialogInfo@@QAE@XZ')),
    'DlgSetText': (0x10006550, 4, ('@NDialogInfo@@', '?SetText@NDialogInfo@@')),
    'DlgSetSysMsg': (0x10006500, 4, ('@NDialogInfo@@', '?SetSystemMessage@NDialogInfo@@')),
    'ShowDialog': (0x1005b310, 4, ('@NConsoleWnd@@', '?ShowDialog@')),
    'GetMyUser': (0x10060ac0, 0, ('@NConsoleWnd@@', '?GetMyUser@')),
    'GetSelectedRow': (0x1001d120, 0, ('@NCListCtrl@@', '?GetSelectedRow@')),
    'EmpowerReset': (0x10071f30, 0, ('@NCEmpowerWnd@@', '?Reset@NCEmpowerWnd@@')),
    'SetMember': (0x10040930, 0xC, None),   # stock only: ours inlines it (the stub does the stores)
}
stubs.FN_POP['GetEquipSlot'] = 8
# the constructors a creation of ours may inline (a base class's constructor stands for the GM class's: same arguments)
ALTS = {
    'ctorGMEquip': ['??0NCEquipItemWnd@@QAE@H@Z'],
    'ctorGMInven': ['??0NCInvenItemWnd@@QAE@HHHH@Z', '??0NCItemWnd@@QAE@HHHH@Z'],
    'ctorGMPledge': ['??0NCPledgeWnd@@QAE@XZ'],
}

VSLOT_POP = {0x0C: 4, 0x64: 12, 0x9C: 4, 0xAC: 0, 0xB0: 0, 0x110: 0, 0x538: 12}
GAME_IMPORT_POP = {
    '?GetTransientPackage@UObject@@SAPAVUPackage@@XZ': 0,
    '?StaticAllocateObject@UObject@@SAPAV1@PAVUClass@@PAV1@VFName@@K1PAVFOutputDevice@@11@Z': 0,
    '??DL2FName@@QBEPBGXZ': 0, '?appStrcmp@@YAHPBG0@Z': 0,
}
stubs.IMPORT_POP.update(GAME_IMPORT_POP)


class GmEmu(TipEmu):
    def __init__(self, kind):
        # our symbols that are not in OURS_FN's single-candidate scheme are resolved in TipEmu.__init__ (it raises on
        # an ambiguous one): the constructors are matched by prefix and the first candidate with the right arity wins
        super().__init__(kind)
        self.ids = {}
        self.names = {}
        self.fname_table = {}
        self.vt = self.alloc(0x800)
        self.vstubs = {}
        for off in sorted(set(list(VSLOT_POP.keys()) + [0x538])):
            st = STUB_BASE + 0xA000 + off
            self.uc.mem_write(st, b'\xC3')
            self.stubs[st] = ('vs:%x' % off, None)
            self.vstubs[off] = st
        for i in range(0x200):
            self.write32(self.vt + 4 * i, self.vstubs.get(4 * i, 0))
        # data imports: GError, a cell with a NULL
        self.gerr = self.alloc(16)
        for e in self.pe.DIRECTORY_ENTRY_IMPORT:
            for imp in e.imports:
                nm = imp.name.decode() if imp.name else ''
                if nm == '?GError@@3PAVFOutputDeviceError@@A':
                    self.write32(imp.address, self.gerr)
        self.mallocs = []
        for slot, nm in ((0, 'GMallocM'), (8, 'GMallocF')):
            st = STUB_BASE + 0xB000 + slot
            self.uc.mem_write(st, b'\xC3')
            self.stubs[st] = ('fn:' + nm, None)
            self.write32(self.gmalloc_vt + slot, st)
        self.nobj = 0
        self.rows = {}
        self.cur_user = 0
        self.sel_row = 0
        self.commit_text = None
        self.serverid = 7
        self.adena = 0
        self.slotseed = 0

    # ---- identities -------------------------------------------------------------------------------------------
    def norm(self, v):
        v &= 0xFFFFFFFF
        if v in self.ids:
            return self.ids[v]
        if NWBASE <= v < NWBASE + 0x400000:
            return 'va+%x' % (v - NWBASE + 0x10000000)
        if 0x10000000 <= v < 0x10400000:
            return 'va+%x' % v
        return v

    def newobj(self, name, size=0x1000):
        p = self.alloc(size)
        self.uc.mem_write(p, bytes(size))
        self.write32(p, self.vt)
        self.ids[p] = name
        return p

    # ---- dispatch ---------------------------------------------------------------------------------------------
    def dispatch(self, name):
        kind, nm = name.split(':', 1)
        if kind == 'vs':
            off = int(nm, 16)
            self.last_pop = VSLOT_POP[off]
            return self.do_vslot(off)
        if kind == 'imp' and nm in GAME_IMPORT_POP:
            self.last_pop = GAME_IMPORT_POP[nm]
            return self.do_gameimport(nm)
        if kind == 'fn' and nm in SERV:
            self.last_pop = SERV[nm][1]
            return self.do_serv(nm)
        if kind == 'fn' and nm in ('GMallocM', 'GMallocF'):
            if nm == 'GMallocM':
                self.last_pop = 8
                size, tag = self.arg(0), self.wstr(self.arg(1)) if self.arg(1) else None
                p = self.alloc(max(size, 4))
                self.uc.mem_write(p, bytes(max(size, 4)))
                self.log.append(('Malloc', size, tag))
                self.mallocs.append(p)
                return p
            self.last_pop = 4
            self.log.append(('Free', self.arg(0)))
            return None
        return super().dispatch(name)

    def do_gameimport(self, nm):
        if nm.startswith('?GetTransientPackage'):
            return 0x5555
        if nm.startswith('?StaticAllocateObject'):
            cls = self.arg(0)
            self.nobj += 1
            p = self.newobj('new%d' % self.nobj)
            self.log.append(('new', self.norm(cls)))
            return p
        if nm.startswith('??DL2FName'):
            idx = self.read32(self.this())
            return self.static_str(self.fname_table[idx]) if idx in self.fname_table else 0
        if nm.startswith('?appStrcmp'):
            a, b = self.wstr(self.arg(0)), self.wstr(self.arg(1))
            return 0 if a == b else (1 if a > b else 0xFFFFFFFF)
        raise RuntimeError(nm)

    def do_vslot(self, off):
        t = self.this()
        self.log.append(('v%x' % off, self.norm(t), [self.norm(self.arg(i)) for i in range(VSLOT_POP[off] // 4)]))
        if off == 0x110:      # NCEditBox::CommitText
            return self.static_str(self.commit_text) if self.commit_text is not None else 0
        return 1

    def do_serv(self, nm):
        t = self.this()
        a = self.arg
        if nm == 'FindTexture':
            s = self.wstr(a(0)) if a(0) else None
            self.log.append(('FindTexture', self.norm(t), s))
            return 0x7700 + (len(s) if s else 0) + sum(map(ord, s or '')) % 97
        if nm == 'FormatNumber':
            out, dig = a(0), self.wstr(a(1))
            self.write_wstr_at(out, 'F' + dig)
            self.log.append(('FormatNumber', dig))
            return None
        if nm == 'GetAdena':
            return u32(self.adena)
        if nm == 'GetServerID':
            return self.serverid
        if nm == 'WndOnCreate':
            self.log.append(('NCWnd::OnCreate', self.norm(t)))
            return 1
        if nm == 'SetWindowRect':
            self.log.append(('SetWindowRect', self.norm(t), [self.rds32(self.reg(UC_X86_REG_ESP) + 4 + 4 * i) for i in range(4)]))
            return None
        if nm == 'AddChildWindow':
            child, st = a(0), a(1)
            self.log.append(('AddChildWindow', self.norm(t), self.norm(child), st))
            pe_ = self.alloc(0x20)
            self.uc.mem_write(pe_, bytes(0x20))
            self.write32(pe_ + 4, t)
            en = self.alloc(0x20)
            self.uc.mem_write(en, bytes(0x20))
            self.write32(en + 4, child)
            self.write32(en + 0xC, pe_)
            self.nent = getattr(self, 'nent', 0) + 1
            self.ids[en] = 'en%d' % self.nent
            self.ids[pe_] = 'pe%d' % self.nent
            return en
        if nm.startswith('ctor'):
            nargs = SERV[nm][1] // 4
            if nm == 'ctorEdit':
                targs = [a(0), self.wstr(a(1)) if a(1) else None, a(2), a(3), a(4)]
            elif nm == 'ctorButton3':
                targs = [self.wstr(a(0)), self.wstr(a(1)), a(2)]
            else:
                targs = [a(i) for i in range(nargs)]
            self.log.append(('ctor', nm, self.norm(t), targs))
            if nm == 'ctorGMPledge':
                # the clan page's buttons (+0x6CC list, +0x6D0 .. +0x6F8): objects the creation then enables or disables
                for i, off in enumerate(range(0x6CC, 0x6FC, 4)):
                    self.write32(t + off, self.newobj('pl%d' % i))
            return t
        if nm == 'AddTab':
            self.log.append(('AddTab', self.norm(t), [a(0), self.norm(a(1)), a(2), a(3), self.wstr(a(4)), self.wstr(a(5)), a(6), self.rds32(self.reg(UC_X86_REG_ESP) + 4 + 28)]))
            return None
        if nm == 'SetTextID':
            self.log.append(('SetTextID', self.norm(t), a(0)))
            return None
        if nm == 'SetText':
            self.log.append(('SetText', self.norm(t), self.wstr(a(0))))
            return None
        if nm == 'SetPushedTex':
            self.log.append(('SetPushedTex', self.norm(t), self.wstr(a(0))))
            return 1
        if nm == 'DlgCtor':
            self.uc.mem_write(t, bytes(0x78))
            self.write32(t, 0xFFFFFFFF)
            self.write32(t + 4, 3)
            self.write32(t + 0xC, 1)
            return t
        if nm == 'DlgSetText':
            self.fs_set(t + 0x1C, self.wstr(a(0)))
            return None
        if nm == 'DlgSetSysMsg':
            self.log.append(('DlgSetSysMsg', a(0)))
            return None
        if nm == 'ShowDialog':
            info = a(0)
            self.log.append(('ShowDialog', [self.rds32(info + o) for o in (0, 4, 0xC, 0x10, 0x14, 0x18)], self.norm(self.read32(info + 8)),
                             self.fs_get(info + 0x1C)))
            return None
        if nm == 'GetMyUser':
            return self.cur_user
        if nm == 'GetSelectedRow':
            self.log.append(('GetSelectedRow', self.norm(t)))
            return self.sel_row
        if nm == 'EmpowerReset':
            self.log.append(('EmpowerReset', self.norm(t)))
            return None
        if nm == 'SetMember':
            self.write32(t + 0xF8, a(0))
            self.write32(t + 0xFC, a(2))
            if a(1):
                self.fs_set(t + 0x100, self.wstr(a(1)))
            return None
        if nm == 'GetEquipSlot':
            self.log.append(('GetEquipSlot', a(0), a(1)))
            return u32((a(0) * 7 + a(1)) % 16)
        raise RuntimeError('serv ' + nm)

    # ---- installing the stubs (stock: over the stock code, ours: over our symbols) --------------------------------
    def install_extra(self):
        n = 0
        for nm, (va, pop, ours) in SERV.items():
            stub = STUB_BASE + 0x9800 + 16 * n
            n += 1
            self.uc.mem_write(stub, b'\xC3')
            self.stubs[stub] = ('fn:' + nm, None)
            code = b'\x68' + struct.pack('<I', stub) + b'\xC3'
            if self.kind == 'stock':
                self.uc.mem_write(va, code)
            elif ours:
                cls, meth = ours
                cands = [v for s, v in self.symbols.items() if s.startswith(meth) and (cls in s or meth.startswith('??'))]
                for alt in ALTS.get(nm, []):
                    cands += [v for s, v in self.symbols.items() if s.startswith(alt)]
                if not cands:
                    raise RuntimeError('ours symbol for ' + nm)
                for v in cands:
                    self.uc.mem_write(v, code)
        if self.kind == 'ours':
            # a creation of ours knows the class it has just made and calls WindowProc(WM_CREATE) directly instead of through
            # the vtable: the stock code does the virtual call (slot 25, +0x64), so those are the same stub
            code = b'\x68' + struct.pack('<I', self.vstubs[0x64]) + b'\xC3'
            for cls in ('NCGMInvenItemWnd', 'NCGMEquipItemWnd', 'NCGMPledgeWnd'):
                for s, v in self.symbols.items():
                    if s.startswith('?WindowProc@%s@@' % cls):
                        self.uc.mem_write(v, code)
            # the objects of ours made by an inlined constructor carry our vtable: HideWindow goes to NCWnd's
            code = b'\x68' + struct.pack('<I', self.vstubs[0xB0]) + b'\xC3'
            for s, v in self.symbols.items():
                if s.startswith('?HideWindow@NCWnd@@'):
                    self.uc.mem_write(v, code)


def new_emu(kind):
    e = GmEmu(kind)
    e.install_extra()
    return e


def ours_hook(va):
    """Where our code for the hook at stock address va sits (the smoke log of the last cycle: `ours=RVA`): for a function
    the compiler gave no symbol of its own (a forwarder thunk)."""
    import re
    log = open(os.path.join(ROOT, 'cycle.log'), encoding='latin-1').read()
    m = re.search(r'%s [^\n]*?ours=([0-9a-f]+)' % va, log)
    return 0x10000000 + int(m.group(1), 16)


def sym(e, prefix):
    c = [v for s, v in e.symbols.items() if s.startswith(prefix)]
    assert len(c) == 1, (prefix, c)
    return c[0]


def compare(name, cases, res, show=3):
    bad = 0
    for i, (a, b) in enumerate(zip(res['stock'], res['ours'])):
        if a != b:
            bad += 1
            if bad <= show:
                print(name, 'case', i, cases[i] if len(str(cases[i])) < 400 else str(cases[i])[:400])
                sa, sb = str(a), str(b)
                k = 0
                while k < min(len(sa), len(sb)) and sa[k] == sb[k]:
                    k += 1
                print('  first difference at', k)
                print('  stock', sa[max(0, k - 150):k + 250])
                print('  ours ', sb[max(0, k - 150):k + 250])
    print(name, 'cases', len(cases), 'differ', bad)
    return bad


# ---------------------------------------------------------------------------------------------------------------------
def test_equip_paint(n, r):
    cases = []
    for _ in range(n):
        items = []
        for _ in range(r.randint(0, 7)):
            it = rand_item(r)
            it['ints'][0x1914] = r.randint(1, 99)
            it['ints'][0x1934] = r.choice([0, 1, 6, 6])
            items.append({'item': it, 'sel': r.choice([0, 1]), 'icon': r.choice([0, 0x2001, 0x2002]),
                          'icon2': r.randint(0, 3) * 0x100, 'icon3': r.randint(0, 3) * 0x111,
                          's2': r.choice(['', 'a']), 's3': r.choice(['', '', 'b'])})
        hennas = [{'icon': r.choice([0, 0x3001, 0x3002]), 'usable': r.choice([0, 1, 5]),
                   'box': [0x6D, 27 * i, 24, 24]} for i in range(r.randint(0, 5))]
        cases.append({'items': items, 'hennas': hennas, 'sx': r.randint(0, 600), 'sy': r.randint(0, 400), 'w': r.randint(100, 300),
                      'h': r.randint(100, 300), 'cell': r.choice([32, 32, 34]), 'dual': r.random() < 0.8, 'hback': r.random() < 0.8,
                      'bow': r.choice([0, 1])})
    res = {}
    for kind in ('stock', 'ours'):
        e = new_emu(kind)
        fp = 0x10073CD0 if kind == 'stock' else sym(e, '?OnPaint@NCGMEquipItemWnd@@')
        stub = STUB_BASE + 0x9000
        e.uc.mem_write(stub, b'\xC3')
        e.stubs[stub] = ('fn:GetEquipSlot', None)
        stub2 = STUB_BASE + 0x9010
        e.uc.mem_write(stub2, b'\xC3')
        e.stubs[stub2] = ('fn:GetEquipSlot', None)
        ds = {'getequip': stub, 'ptinrect': stub2}
        e.heap_mark = e.heap_ptr
        outs = []
        for c in cases:
            e.heap_ptr = e.heap_mark
            e.log.clear()
            wnd = make_equip_wnd(e, None, c, ds)
            dev = e.alloc(0x80)
            e.uc.mem_write(dev, bytes(0x80))
            ret = e.call(fp, this=wnd, args=[dev])
            dv = [e.read32(dev + o) for o in (0x2C, 0x30, 0x48, 0x4C, 0x5C)]
            outs.append((ret, dv, list(e.log)))
        res[kind] = outs
    return compare('equip paint', cases, res)


def test_equip_slot(r):
    vals = [0, 1, 2, 3, 4, 5, 6, 7, 8, 0x10, 0x20, 0x30, 0x40, 0x80, 0x100, 0x200, 0x400, 0x800, 0x1000, 0x2000, 0x4000,
            0x8000, 0x10000, 0x6, 0x41, 0x81, 0x4001] + [r.randint(0, 0x1FFFF) for _ in range(400)]
    res = {}
    for kind in ('stock', 'ours'):
        e = new_emu(kind)
        fp = 0x10072E80 if kind == 'stock' else sym(e, '?GetEquipSlot@NCGMEquipItemWnd@@')
        res[kind] = [(v, e.call(fp, this=e.alloc(16), args=[v, 5])) for v in vals]
    return compare('GM GetEquipSlot', vals, res)


def test_inventory_paint(n, r):
    cases = []
    for _ in range(n):
        cases.append({'sx': r.randint(0, 600), 'sy': r.randint(0, 400), 'w': r.randint(100, 300), 'h': r.randint(100, 400),
                      'back': r.choice([0, 0x4001]), 'adena': r.choice([0, 1, 999, 1000, 1234567, 2000000000, -5]),
                      'atex': r.choice([0, 0x4002])})
    res = {}
    for kind in ('stock', 'ours'):
        e = new_emu(kind)
        fp = 0x10074250 if kind == 'stock' else sym(e, '?OnPaint@NCGMInventoryWnd@@')
        e.heap_mark = e.heap_ptr
        outs = []
        for c in cases:
            e.heap_ptr = e.heap_mark
            e.log.clear()
            e.adena = c['adena']
            wnd = e.alloc(0x100)
            e.uc.mem_write(wnd, bytes(0x100))
            for off, v in ((0x44, c['sx']), (0x48, c['sy']), (0x4C, c['w']), (0x50, c['h']), (0x98, c['back']), (0x9C, c['atex'])):
                e.write32(wnd + off, v)
            page = e.alloc(0x200)
            e.uc.mem_write(page, bytes(0x200))
            e.write32(wnd + 0xA4, page)
            dev = e.alloc(0x80)
            e.uc.mem_write(dev, bytes(0x80))
            ret = e.call(fp, this=wnd, args=[dev])
            dv = [e.read32(dev + o) for o in (0x2C, 0x30, 0x48, 0x4C, 0x5C)]
            outs.append((ret, dv, list(e.log)))
        res[kind] = outs
    return compare('GM inventory paint', cases, res)


def test_pledge_paint(n, r):
    cases = []
    for _ in range(n):
        cases.append({'sx': r.randint(0, 600), 'sy': r.randint(0, 400), 'w': r.randint(100, 300), 'h': r.randint(100, 400),
                      'back': r.choice([0, 0x4001]), 'crest': r.choice([-1, 0, 0, 5, 77]), 'pid': r.randint(0, 99),
                      'name': r.choice(['', 'Warriors', 'A' * 40]), 'leader': r.choice(['', 'Ana', 'Bartholomew the third']),
                      'status': r.choice(['', '', 'In War']), 'level': r.choice([0, 1, 5, 8]),
                      'castle': r.choice([0, 1]), 'hideout': r.choice([0, 1]), 'server': r.choice([1, 7, 21])})
    res = {}
    for kind in ('stock', 'ours'):
        e = new_emu(kind)
        fp = 0x10076800 if kind == 'stock' else sym(e, '?OnPaint@NCGMPledgeWnd@@')
        e.heap_mark = e.heap_ptr
        outs = []
        for c in cases:
            e.heap_ptr = e.heap_mark
            e.log.clear()
            e.serverid = c['server']
            wnd = e.alloc(0x900)
            e.uc.mem_write(wnd, bytes(0x900))
            for off, v in ((0x44, c['sx']), (0x48, c['sy']), (0x4C, c['w']), (0x50, c['h']), (0x98, c['back']),
                           (0x2A4, c['crest']), (0x4A8, c['level']), (0x4AC, c['castle']), (0x4B0, c['hideout'])):
                e.write32(wnd + off, v)
            e.write_wstr_at(wnd + 0xA0, c['name'])
            e.write_wstr_at(wnd + 0x2A8, c['leader'])
            e.write_wstr_at(wnd + 0x4BC, c['status'])
            dev = e.alloc(0x80)
            e.uc.mem_write(dev, bytes(0x80))
            ret = e.call(fp, this=wnd, args=[dev])
            dv = [e.read32(dev + o) for o in (0x2C, 0x30, 0x48, 0x4C, 0x5C)]
            outs.append((ret, dv, e.read32(wnd + 0x9C), list(e.log)))
        res[kind] = outs
    return compare('GM pledge paint', cases, res)


# ---------------------------------------------------------------------------------------------------------------------
def dump_objs(e, mark_ptr):
    """The objects made since mark_ptr (the heap is bump-allocated), their words with the pointers named."""
    out = []
    for p, nm in sorted(e.ids.items()):
        if p < mark_ptr or not nm.startswith(('new', 'pl', 'WND')):
            continue
        out.append((nm, [e.norm(e.read32(p + o)) for o in range(4, 0x260, 4)]))   # the vtable pointer is not compared
    return out


def test_create(n, r):
    cases = []
    for _ in range(n):
        cases.append({'sx': r.randint(0, 600), 'sy': r.randint(0, 400), 'w': r.randint(100, 300), 'h': r.randint(100, 400)})
    res = {}
    for which, stock_va, ours_sym in (('GMWnd', 0x100770A0, '?OnCreate@NCGMWnd@@'),
                                      ('GMInventory', 0x10078570, '?OnCreate@NCGMInventoryWnd@@'),
                                      ('GMAction', 0x10076C70, '?OnCreate@NCGMActionWnd@@')):
        res = {}
        for kind in ('stock', 'ours'):
            e = new_emu(kind)
            fp = stock_va if kind == 'stock' else sym(e, ours_sym)
            e.heap_mark = e.heap_ptr
            outs = []
            for c in cases:
                e.heap_ptr = e.heap_mark
                e.ids = {}
                e.log.clear()
                e.nobj = 0
                e.nent = 0
                wnd = e.newobj('WND', 0x400)
                for off, v in ((0x44, c['sx']), (0x48, c['sy']), (0x4C, c['w']), (0x50, c['h'])):
                    e.write32(wnd + off, v)
                ret = e.call(fp, this=wnd, args=[])
                outs.append((ret, list(e.log), dump_objs(e, 0), [e.norm(e.read32(wnd + o)) for o in range(0x58, 0xE0, 4)]))
            res[kind] = outs
        compare('create ' + which, cases, res, show=2)


# ---------------------------------------------------------------------------------------------------------------------
def make_map(e, entries, valoff_setup):
    """A TMap of records: entries = [(key, record ptr)] -> pointer to {Pairs(data, num, max), Hash, HashCount}."""
    data = e.alloc(12 * max(1, len(entries)))
    for i, (k, v) in enumerate(entries):
        e.write32(data + 12 * i, 0xFFFFFFFF)
        e.write32(data + 12 * i + 4, k)
        e.write32(data + 12 * i + 8, v)
    m = e.alloc(0x20)
    e.uc.mem_write(m, bytes(0x20))
    e.write32(m, data)
    e.write32(m + 4, len(entries))
    e.write32(m + 8, len(entries))
    return m


def test_buttons(n, r):
    names = ['Orc', 'Elf', 'Dagger', 'Heal', 'Zaken', 'Sword of Doom']
    cases = []
    for _ in range(n):
        cases.append({'text': r.choice([None, '', 'Orc', 'Elf', 'Dagger', 'Heal', 'Zaken', 'Sword of Doom', 'nobody']),
                      'recs': [{'null': r.random() < 0.15, 'name': r.choice(names + ['']), 'id': r.randint(1, 20000),
                                'fid': r.randint(1, 9)} for _ in range(r.randint(0, 6))]})
    for which, stock_va, ours_sym, table in (('NewButton4', 0x10078BF0, '?OnNewButton4@NCGMWnd@@', 0x27E8),
                                              ('NewButton5', 0x10078D60, '?OnNewButton5@NCGMWnd@@', 0x27E4),
                                              ('NewButton6', 0x10078EC0, '?OnNewButton6@NCGMWnd@@', 0x4CB8)):
        res = {}
        for kind in ('stock', 'ours'):
            e = new_emu(kind)
            fp = stock_va if kind == 'stock' else sym(e, ours_sym)
            e.heap_mark = e.heap_ptr
            outs = []
            for c in cases:
                e.heap_ptr = e.heap_mark
                e.log.clear()
                e.commit_text = c['text']
                e.fname_table = {}
                wnd = e.newobj('WND', 0x400)
                edit = e.newobj('EDIT', 0x400)
                e.write32(wnd + 0xA4, edit)
                entries = []
                for i, rec in enumerate(c['recs']):
                    if rec['null']:
                        entries.append((i + 1, 0))
                        continue
                    p = e.alloc(0x300)
                    e.uc.mem_write(p, bytes(0x300))
                    if which == 'NewButton4':
                        e.write32(p, rec['id'])
                        e.fs_set(p + 4, rec['name'])
                    elif which == 'NewButton5':
                        e.write32(p + 0xC, rec['id'])
                        if rec['name']:
                            e.fname_table[rec['fid'] * 16 + i] = rec['name']
                            e.write32(p + 0x40, rec['fid'] * 16 + i)
                    else:
                        e.write32(p + 4, rec['id'])
                        e.fs_set(p + 0x28, rec['name'])
                    entries.append((i + 1, p))
                m = make_map(e, entries, None)
                e.write32(e.gdata + table, m)
                ret = e.call(fp, this=wnd, args=[])
                outs.append((ret, list(e.log)))
            res[kind] = outs
        compare(which, cases, res, show=2)


def test_pledge_buttons(n, r):
    cases = []
    for _ in range(n):
        cases.append({'flags': r.choice([0, 0x40, 0x40, 0x41, 0xC0]), 'me': r.choice([0, 1, 1]), 'oid': r.randint(1, 99999),
                      'row': r.choice(['none', 'noname', 'offline', 'online', 'online', 'nocells']),
                      'name': r.choice(['Ana', 'Bartholomew']), 'pid': r.randint(1, 50000)})
    for which, stock_va, ours_sym in (('PledgeButton1', 0x10042920, '?OnNewButton1@NCPledgeWnd@@'),
                                       ('PledgeButton2', 0x100429C0, '?OnNewButton2@NCPledgeWnd@@')):
        res = {}
        for kind in ('stock', 'ours'):
            e = new_emu(kind)
            fp = stock_va if kind == 'stock' else sym(e, ours_sym)
            e.heap_mark = e.heap_ptr
            outs = []
            for c in cases:
                e.heap_ptr = e.heap_mark
                e.log.clear()
                # the console: its vtable (RequestPledgePower is slot 334), the desktop and the empower window at +0xA0
                cvt = e.alloc(0x600)
                for i in range(0x180):
                    e.write32(cvt + 4 * i, e.vstubs.get(4 * i, 0))
                e.write32(e.console, cvt)
                emp = e.newobj('EMPOWER', 0x120)
                e.write32(e.desktop + 0xA0, emp)
                user = e.alloc(0x200)
                e.uc.mem_write(user, bytes(0x200))
                e.write32(user + 0x18, c['oid'])
                e.write32(user + 0x1C8, c['flags'] << 0)
                e.write_wstr_at(user + 0x1C, 'Hero')
                e.cur_user = user if c['me'] else 0
                wnd = e.newobj('PLEDGE', 0x900)
                lst = e.newobj('LIST', 0x100)
                e.write32(wnd + 0x6CC, lst)
                row = 0
                if c['row'] != 'none':
                    row = e.alloc(0x888)
                    e.uc.mem_write(row, bytes(0x888))
                    if c['row'] != 'nocells':
                        cells = e.alloc(0x1820 * 3)
                        e.uc.mem_write(cells, bytes(0x1820 * 3))
                        e.write32(row, cells)
                        if c['row'] != 'noname':
                            e.write_wstr_at(cells, c['name'])
                    e.write32(row + 0x804, 0 if c['row'] == 'offline' else c['pid'])
                e.sel_row = row
                ret = e.call(fp, this=wnd, args=[])
                outs.append((ret, list(e.log), [e.read32(emp + o) for o in (0xF8, 0xFC)], e.fs_get(emp + 0x100)))
            res[kind] = outs
        compare(which, cases, res, show=2)


def test_rehash(n, r):
    cases = []
    for _ in range(n):
        cnt = r.randint(0, 40)
        hc = r.choice([1, 2, 4, 8, 16, 32, 64])
        cases.append({'keys': [r.randint(0, 100000) for _ in range(cnt)], 'hc': hc, 'oldhash': r.random() < 0.8})
    res = {}
    for kind in ('stock', 'ours'):
        e = new_emu(kind)
        fp = 0x10073AC0 if kind == 'stock' else ours_hook('10073ac0')
        e.heap_mark = e.heap_ptr
        outs = []
        for c in cases:
            e.heap_ptr = e.heap_mark
            e.log.clear()
            e.mallocs = []
            m = e.alloc(0x20)
            e.uc.mem_write(m, bytes(0x20))
            data = e.alloc(12 * max(1, len(c['keys'])))
            for i, k in enumerate(c['keys']):
                e.write32(data + 12 * i, 0x7777)
                e.write32(data + 12 * i + 4, k)
                e.write32(data + 12 * i + 8, 0x9000 + i)
            e.write32(m, data); e.write32(m + 4, len(c['keys'])); e.write32(m + 8, len(c['keys']))
            old = e.alloc(4 * c['hc']) if c['oldhash'] else 0
            e.write32(m + 0xC, old); e.write32(m + 0x10, c['hc'])
            e.call(fp, this=m, args=[])
            nh = e.read32(m + 0xC)
            outs.append((list(e.log), [e.rds32(nh + 4 * i) for i in range(c['hc'])] if nh else None,
                         [e.read32(data + 12 * i) for i in range(len(c['keys']))], e.read32(m + 0x10), e.read32(m + 4)))
        res[kind] = outs
    return compare('Rehash', cases, res)


def main(n, seed, what='all'):
    r = random.Random(seed)
    if what in ('all', 'paint'):
        test_equip_slot(r)
        test_equip_paint(n, r)
        test_inventory_paint(n, r)
        test_pledge_paint(n, r)
    if what in ('all', 'create'):
        test_create(max(20, n // 10), r)
    if what in ('all', 'rehash'):
        test_rehash(n, r)
    if what in ('all', 'buttons'):
        test_buttons(n, r)
        test_pledge_buttons(n, r)


if __name__ == '__main__':
    main(int(sys.argv[1]), int(sys.argv[2]), sys.argv[3] if len(sys.argv) > 3 else 'all')
