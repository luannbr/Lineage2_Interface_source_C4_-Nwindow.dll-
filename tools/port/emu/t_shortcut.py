# STALE (C4, sa-emu agent `old`): the C4 shortcut bar tests that run are in win/t_oldwnd.py (OnPaint, SlotAtPoint, OnMouseMove, OnLButtonUp, UpdateMacro) and win/t_shortcutitem.py (NShortCutItem); the rest has no successor; C2 model, does not run.
"""Differential test of the shortcut bar (NCShortCutWnd), stock against ours under unicorn: the two painters (PaintBar 0x100CD790,
PaintPad 0x100CE160) and OnPaint (0x100CE740), the hit tests (SlotAtPoint 0x100CCEF0, PadSlotAt 0x100CCE40), OnMouseMove (0x100CE820),
OnLButtonUp (0x100CE9D0), OnShortCutKeyDown (0x100CF080), SetShortCut (0x100CFE90), UpdateMacro (0x100CC8D0), ReloadNames (0x100CD250)
and NShortCutKey::OnKeyUp (0x100CD610). Random bars (layout, page, slots with their types, cool times, macros), the engine and the
console services stubbed alike on both sides (calls and arguments logged and compared), then the window's fields, the keys and the
slots' memory compared.
usage: python tools/port/emu/t_shortcut.py <cases> <seed> [name ...]   (NWD_OURS=<dir with NWindowDecomp.dll and .map> tests a snapshot)"""
import sys, random, os, struct, hashlib
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import emu as _emu
import stubs
from stubs import *
import t_macro
from t_macro import MacroEmu
from t_recipe import sym

if os.environ.get('NWD_OURS'):
    _emu.OURS = os.path.join(os.environ['NWD_OURS'], 'NWindowDecomp.dll')
    _emu.OURS_MAP = os.path.join(os.environ['NWD_OURS'], 'NWindowDecomp.map')

# the device's and the cool time's services (TipEmu puts them at the stock address, or at our symbol)
stubs.STOCK_FN.update({0x1000F7E0: 'DrawBrightenTexture', 0x10025290: 'FindTexture', 0x10019D50: 'CT_Stop', 0x10019D60: 'CT_IsIdle',
                       0x1001A430: 'CT_Flash', 0x1001A670: 'CT_Tex'})
stubs.FN_POP.update({'DrawBrightenTexture': 0x28, 'FindTexture': 4, 'CT_Stop': 0, 'CT_IsIdle': 0, 'CT_Flash': 0, 'CT_Tex': 0})
stubs.OURS_FN.update({'DrawBrightenTexture': ('@NCGDevice@@', '?DrawBrightenTexture@'), 'FindTexture': ('@NCObject@@', '?FindTexture@'),
                      'CT_Stop': ('@NCCoolTimeIcon@@', '?Stop@'), 'CT_IsIdle': ('@NCCoolTimeIcon@@', '?IsIdle@'),
                      'CT_Flash': ('@NCCoolTimeIcon@@', '?GetFlashTexture@'), 'CT_Tex': ('@NCCoolTimeIcon@@', '?GetTexture@')})
stubs.IMPORT_POP.update({'??0L2ParamStack@@QAE@H@Z': 4, '??1L2ParamStack@@QAE@XZ': 0})

# services the test stubs by hand (stock address: (name, pop, our symbol: (class, method prefix) or None for a StockCall target))
SVC = {
    0x10056A20: ('MacroCount', 0, None), 0x10060CE0: ('MacroAt', 4, None), 0x10060DD0: ('MacroIcon', 4, None),
    0x10060D70: ('MacroName', 4, None),
    0x100568E0: ('IsToggle', 8, ('@NConsoleWnd@@', '?IsToggleSkill@')), 0x100611C0: ('HasAbn', 4, ('@NConsoleWnd@@', '?HasAbnormal@')),
    0x10061D20: ('PlaySound', 4, ('@NConsoleWnd@@', '?PlayInterfaceSound@')), 0x100573D0: ('IsShot', 4, ('@NConsoleWnd@@', '?IsShotItem@')),
    0x1006AB40: ('ItemName', 4, ('@NConsoleWnd@@', '?GetItemName@')), 0x10056880: ('SkillName', 8, ('@NConsoleWnd@@', '?GetSkillName@')),
    0x10060C90: ('ActionName', 4, ('@NConsoleWnd@@', '?GetActionName@')),
    0x1006C210: ('UseShortCut', 0xC, ('@NConsoleWnd@@', '?UseShortCut@')),
    0x100545E0: ('ShortCutDel', 4, ('@NConsoleWnd@@', '?RequestShortCutDel@')),
    0x10060AC0: ('GetMyUser', 0, ('@NConsoleWnd@@', '?GetMyUser@')), 0x1005B310: ('ShowDialog', 4, ('@NConsoleWnd@@', '?ShowDialog@')),
    0x100253C0: ('UnlockCursor', 0, ('@NCObject@@', '?UnlockCursor@')), 0x100253A0: ('SetCursor', 8, ('@NCObject@@', '?SetCursor@')),
    0x100253B0: ('LockCursor', 0, ('@NCObject@@', '?LockCursor@')), 0x1002B940: ('FindWindowAt', 8, ('@NCWnd@@', '?FindWindowAt@')),
    0x1008D7D0: ('HitLine', 8, ('@NCMacroEditList@@', '?HitLine@')), 0x1008DE20: ('ClearMarks', 0, ('@NCMacroEditWnd@@', '?ClearMarks@')),
    0x1008DED0: ('ClearMark', 4, ('@NCMacroEditWnd@@', '?ClearMark@')),
    0x100198B0: ('ItemHitTest', 8, ('@NCItemWnd@@', '?HitTest@')),
    0x1000C250: ('InsertText', 8, ('@NCEditBox@@', '?InsertText@')),
    0x100CC700: ('CopyFrom', 4, ('@NShortCutItem@@', '?CopyFrom@')), 0x100CC7F0: ('SlotClear', 0, ('@NShortCutItem@@', '?Clear@')),
    0x1001A9A0: ('ItemCtor', 4, ('NItemEntry@@', '??0NItemEntry@@QAE@PAUNItemInfo@@@Z')),   # the slot's constructor 0x100CF560 is inlined in ours
    0x1001A4E0: ('CTCtor', 0, ('NCCoolTimeIcon@@', '??0NCCoolTimeIcon@@QAE@XZ')),
    0x100CF5D0: ('SlotDtor', 0, ('NItemEntry@@', '??1NItemEntry@@QAE@XZ')),
    0x10028B20: ('Fill', 0xC, None),            # NToolTipInfo::Fill: ours stops one level up, at NItemEntry::FillToolTip
}
OURS_FILL = ('@NItemEntry@@', '?FillToolTip@')
for _va, (_nm, _pop, _o) in SVC.items():
    stubs.FN_POP[_nm] = _pop
stubs.FN_POP.update({'ItemCtor': 4, 'CTCtor': 0})
for _nm, _pop in (('IsCtrl', 0), ('ClearText', 0), ('IsInWin', 8), ('IsFocused', 0), ('IsEditFocus', 0), ('Malloc', 8), ('Free', 4)):
    stubs.FN_POP[_nm] = _pop

CLS_EDIT = 0x1019C8F8       # NCEditBox's UClass
ITEMS = 100
SLOT = 0x1A7C


class ScEmu(MacroEmu):
    def __init__(self, kind):
        super().__init__(kind)
        self.case = None
        self.nextstub = STUB_BASE + 0x9800
        self.cache = {}
        for i in range(1300):
            self.fs_set(self.gdata + 0x2DD34 + 0x24 * i + 4, 'MSG%d' % i)       # the system messages (NDialogInfo::SetSystemMessage)
        self.gchat = self.alloc(16)
        for e in self.pe.DIRECTORY_ENTRY_IMPORT:
            for imp in e.imports:
                if imp.name and imp.name.decode() == '?GIsEnterChatting@@3HA':
                    self.write32(imp.address, self.gchat)
        for va, (nm, pop, spec) in SVC.items():
            st = self.stub(nm)
            code = b'\x68' + struct.pack('<I', st) + b'\xC3'
            if kind == 'stock':
                self.uc.mem_write(va, code)
            elif nm == 'Fill':
                c = [v for k, v in self.symbols.items() if k.startswith(OURS_FILL[1]) and OURS_FILL[0] in k]
                assert len(c) == 1, c
                self.uc.mem_write(c[0], code)
            elif spec is None:
                self.uc.mem_write(NWBASE + (va - 0x10000000), code)
            else:
                c = [v for k, v in self.symbols.items() if k.startswith(spec[1]) and spec[0] in k]
                assert len(c) == 1, (nm, c)
                self.uc.mem_write(c[0], code)
        if kind == 'ours':
            self.write32(NWBASE + 0x1878EC, self.put_wstr('/'))     # the command prefix this unit's pointer holds (in the stock image: 0x10107E78)
        self.write32(self.gmalloc_vt, self.stub('Malloc'))
        self.write32(self.gmalloc_vt + 8, self.stub('Free'))
        self.vt_console = self.alloc(0x1000)
        self.uc.mem_write(self.vt_console, bytes(0x1000))
        self.write32(self.vt_console + 0x448, self.stub('IsCtrl'))
        self.write32(self.console, self.vt_console)
        self.engine = self.alloc(0x400)
        self.write32(self.console + 0x44, self.engine)

    def stub(self, nm):
        if nm not in self.cache:
            st = self.nextstub
            self.nextstub += 16
            self.uc.mem_write(st, b'\xC3')
            self.stubs[st] = ('fn:' + nm, None)
            self.cache[nm] = st
        return self.cache[nm]

    def cls(self, va):
        return va if self.kind == 'stock' else NWBASE + (va - 0x10000000)

    def glob(self, va):
        return va if self.kind == 'stock' else NWBASE + (va - 0x10000000)

    # ---- the pointers of a case are shown relative to the window's arrays, so that the two runs compare ----------------------
    def rel(self, p):
        c = self.case
        if c is None:
            return p
        if c['items'] - 4 * SLOT <= p < c['items'] + ITEMS * SLOT:
            return ('slot', (p - c['items']) // SLOT, (p - c['items']) % SLOT)
        for base, (idx, n) in (c.get('allocs') or {}).items():
            if base <= p < base + n:
                return ('alloc', idx, p - base)
        return p

    def do_import(self, nm):
        if nm == '??0L2ParamStack@@QAE@H@Z':
            self.log.append(('ParamStack', self.arg(0)))
            return self.this()
        if nm == '??1L2ParamStack@@QAE@XZ':
            return None
        return super().do_import(nm)

    def do_fn(self, nm):
        a = self.arg
        c = self.case
        sp = self.reg(UC_X86_REG_ESP)
        s32 = lambda i: self.rds32(sp + 4 + 4 * i)
        log = self.log
        t = self.this()
        if nm == 'DrawBrightenTexture':
            log.append(('DrawBrighten', [s32(i) for i in range(8)], a(8), a(9)))
            return None
        if nm == 'FindTexture':
            s = self.wstr(a(0)) if a(0) else None
            log.append(('FindTexture', s))
            return 0x7700 + (len(s) if s else 0) + (0x100 if s and s.endswith('1') else 0)
        if nm == 'CT_Stop':
            log.append(('CT_Stop', self.rel(t)))
            return None
        if nm in ('CT_IsIdle', 'CT_Flash', 'CT_Tex'):
            idx = self.rel(t)[1]
            log.append((nm, idx))
            if nm == 'CT_IsIdle':
                return c['idle'][idx]
            if nm == 'CT_Flash':
                return 0x6000 + idx
            return c['cttex'][idx]
        if nm == 'MacroCount':
            log.append(('MacroCount',))
            return len(c['macros'])
        if nm == 'MacroAt':
            i = s32(0)
            log.append(('MacroAt', i))
            return c['macro_ptrs'][i] if 0 <= i < len(c['macros']) else 0
        if nm == 'MacroIcon':
            log.append(('MacroIcon', s32(0)))
            return u32(c['macroicon'])
        if nm == 'MacroName':
            log.append(('MacroName', s32(0)))
            return c['macroname_ptr']
        if nm == 'IsToggle':
            log.append(('IsToggle', s32(0), s32(1)))
            return 1 if (s32(0) * 7 + s32(1)) % 3 == 0 else 0
        if nm == 'HasAbn':
            log.append(('HasAbn', s32(0)))
            return 1 if s32(0) % 2 == 0 else 0
        if nm == 'PlaySound':
            log.append(('PlaySound', s32(0)))
            return None
        if nm == 'IsShot':
            log.append(('IsShot', s32(0)))
            return 1 if s32(0) % 5 == 0 else 0
        if nm == 'ItemName':
            log.append(('ItemName', s32(0)))
            return c['names'][0] if s32(0) % 4 else 0
        if nm == 'SkillName':
            log.append(('SkillName', s32(0), s32(1)))
            return c['names'][1] if s32(0) % 4 else 0
        if nm == 'ActionName':
            log.append(('ActionName', s32(0)))
            return c['names'][2] if s32(0) % 4 else 0
        if nm == 'UseShortCut':
            log.append(('UseShortCut', s32(0), s32(1), s32(2)))
            return None
        if nm == 'ShortCutDel':
            log.append(('ShortCutDel',))
            return None
        if nm == 'GetMyUser':
            log.append(('GetMyUser',))
            return c['user']
        if nm == 'ShowDialog':
            p = a(0)
            d = {'ints': [self.rds32(p + o) for o in (0, 4, 0xC, 0x10, 0x14, 0x18, 0x58, 0x5C, 0x6C, 0x70, 0x74)],
                 'strs': [self.fs_get(p + o) for o in (0x1C, 0x28, 0x34, 0x40, 0x4C, 0x60)],
                 'owner': self.rds32(p + 8) == c['wnd_entry']}
            log.append(('ShowDialog', d))
            return None
        if nm in ('UnlockCursor', 'LockCursor'):
            log.append((nm,))
            return None
        if nm == 'SetCursor':
            log.append(('SetCursor', s32(0), s32(1)))
            return None
        if nm == 'FindWindowAt':
            log.append(('FindWindowAt', s32(0), s32(1)))
            return c['found_entry']
        if nm == 'HitLine':
            log.append(('HitLine', t == c['editlist'], s32(0), s32(1)))
            return u32(c['hitline'])
        if nm in ('ClearMarks', 'ClearMark'):
            log.append((nm, t == c['editor'], s32(0) if nm == 'ClearMark' else None))
            return None
        if nm == 'ItemHitTest':
            log.append(('ItemHitTest', s32(0), s32(1)))
            return u32(c['hit'])
        if nm == 'InsertText':
            log.append(('InsertText', t in c['edit_ptrs'] and c['edit_ptrs'].index(t), self.wstr(a(0)), s32(1)))
            return None
        if nm == 'ClearText':
            log.append(('ClearText', t in c['edit_ptrs'] and c['edit_ptrs'].index(t)))
            return None
        if nm == 'CopyFrom':
            src = a(0)
            log.append(('CopyFrom', self.rel(t), self.rel(src), [self.rds32(src + o) for o in (0x190C, 0x1914, 0x1918, 0x191C, 0x1A70, 0x1A74)]))
            return None
        if nm == 'SlotClear':
            log.append(('SlotClear', self.rel(t)))
            return None
        if nm == 'ItemCtor':
            info = a(0)
            log.append(('ItemCtor', self.rel(t)))
            for dst, src in ((0x190C, 0x170C), (0x1914, 0x1714), (0x1918, 0x1718), (0x191C, 0x1724)):
                self.write32(t + dst, self.read32(info + src))
            return t
        if nm == 'CTCtor':
            log.append(('CTCtor', self.rel(t)))
            return t
        if nm == 'SlotDtor':
            log.append(('SlotDtor', self.rel(t)))
            return None
        if nm == 'Fill':
            # stock: this = the tool tip inside the slot, (slot, flags, extra); ours is the slot's FillToolTip(flags, extra)
            if self.kind == 'stock':
                log.append(('Fill', self.rel(a(0)), s32(1), s32(2)))
            else:
                self.last_pop = 8
                log.append(('Fill', self.rel(t), s32(0), s32(1)))
            return None
        if nm == 'IsCtrl':
            log.append(('IsCtrl',))
            return c['ctrl']
        if nm == 'IsInWin':
            log.append(('IsInWin', s32(0), s32(1)))
            return c['inwin']
        if nm == 'IsFocused':
            log.append(('IsFocused',))
            return c['focus']
        if nm == 'IsEditFocus':
            log.append(('IsEditFocus',))
            return c['editfocus']
        if nm == 'Malloc':
            n = s32(0)
            p = self.alloc(n)
            self.uc.mem_write(p, bytes(n))
            c['allocs'][p] = (len(c['allocs']), n)
            log.append(('Malloc', n))
            return p
        if nm == 'Free':
            log.append(('Free', self.rel(s32(0))))
            return None
        return super().do_fn(nm)

    def dispatch(self, name):
        kind, nm = name.split(':', 1)
        if kind == 'fn' and nm in ('Fill',) and self.kind == 'ours':
            self.last_pop = 8
            return self.do_fn(nm)
        return super().dispatch(name)


# ---- the world of a case ------------------------------------------------------------------------------------------------------
def code_returning(e, value):
    a = e.alloc(16)
    e.uc.mem_write(a, b'\xB8' + struct.pack('<I', value & 0xFFFFFFFF) + b'\xC3')
    return a


def build(e, c):
    """the bar, its arrays, the macros, the editor and the stubs' state; fills the case's pointers"""
    e.heap_ptr = e.heap_mark
    e.log.clear()
    e.case = c
    c['allocs'] = {}
    cols, pages = c['cols'], c['pages']
    items = e.alloc(ITEMS * SLOT)
    e.uc.mem_write(items, bytes(ITEMS * SLOT))
    keys = e.alloc(ITEMS * 0x14)
    e.uc.mem_write(keys, bytes(ITEMS * 0x14))
    c['items'] = items
    c['keys'] = keys
    for i in range(ITEMS):
        k = c['keyst'][i]
        for off, v in zip((0, 4, 8, 0xC, 0x10), k):
            e.write32(keys + 0x14 * i + off, v)
    names = c['slot_names']
    for i, s in enumerate(c['slots']):
        if s is None:
            continue
        p = items + i * SLOT
        e.write32(p, s['icon'])
        e.write_wstr_at(p + 0xC, s['name'])
        e.write_wstr_at(p + 0x40C, s['iconname'])
        for off, k in ((0x190C, 'type'), (0x1914, 'obj'), (0x1918, 'id'), (0x191C, 'lvl'), (0x1998, 'u1998'), (0x19AC, 'u19ac'),
                       (0x1A70, 'owner'), (0x1A74, 'u1a74'), (0x1A78, 'used'), (0x1A64, 'running'), (0x1A68, 'flashing'),
                       (0x1A6C, 'sound')):
            e.write32(p + off, s[k])
    # the macros
    c['macro_ptrs'] = []
    for m in c['macros']:
        p = e.alloc(0xA60)
        e.uc.mem_write(p, bytes(0xA60))
        e.write32(p + 0x9DC, m['id'])
        e.write_wstr_at(p + 0xA48, m['short'])
        c['macro_ptrs'].append(p)
    c['macroname_ptr'] = e.put_wstr(c['macroname']) if c['macroname'] is not None else 0
    c['names'] = [e.put_wstr(n) for n in c['name_pool']]
    # the textures the code writes to
    toggle = e.alloc(0x100)
    e.uc.mem_write(toggle, bytes(0x100))
    c['toggle'] = toggle
    # the window
    wnd = e.alloc(0x260)
    e.uc.mem_write(wnd, bytes(0x260))
    vt = e.alloc(0x400)
    e.uc.mem_write(vt, bytes(0x400))
    e.write32(wnd, vt)
    e.write32(vt + 0x6C, e.stub('IsInWin'))
    e.write32(vt + 0x70, e.stub('IsFocused'))
    entry = e.alloc(0x40)
    e.uc.mem_write(entry, bytes(0x40))
    c['wnd_entry'] = entry
    e.write32(wnd + 0x58, entry)
    for off, k in ((0x44, 'sx'), (0x48, 'sy'), (0x4C, 'w'), (0x50, 'h')):
        e.write32(wnd + off, c[k])
    fields = {0x1A4: 'padmode', 0x1A8: None, 0x1AC: None, 0x1B0: 'back', 0x1B4: 'mask', 0x1B8: 'tip', 0x1BC: 'iconback',
              0x1C0: 'iconclick', 0x1C8: 'next', 0x1CC: 'prev', 0x1D0: 'rot', 0x1D4: 'padbtn', 0x1D8: 'pressedf', 0x1DC: 'dragging',
              0x1E0: 'horiz', 0x1E4: 'cols', 0x1E8: 'pages', 0x1EC: 'page', 0x1F0: 'iconsize', 0x1F4: 'pressed', 0x1F8: 'hover',
              0x1FC: 'padhold', 0x200: 'holdl', 0x204: 'holdr'}
    for off, k in fields.items():
        if k:
            e.write32(wnd + off, c[k])
    e.write32(wnd + 0x1A8, keys)
    e.write32(wnd + 0x1AC, items)
    e.write32(wnd + 0x1C4, toggle)
    # the console's desktop and the macro editor, the edit boxes under it
    editor = e.alloc(0x100)
    e.uc.mem_write(editor, bytes(0x100))
    editlist = e.alloc(0x100)
    e.uc.mem_write(editlist, bytes(0x100))
    arr = e.alloc(4 * 12)
    evt = e.alloc(0x400)
    e.uc.mem_write(evt, bytes(0x400))
    e.write32(evt + 0x114, e.stub('ClearText'))
    c['edit_ptrs'] = []
    for i in range(12):
        ed = e.alloc(0x200)
        e.uc.mem_write(ed, bytes(0x200))
        e.write32(ed, evt)
        e.write32(arr + 4 * i, ed)
        c['edit_ptrs'].append(ed)
    e.write32(editlist + 0xD0, arr)
    e.write32(editor + 0x9C, editlist)
    c['editor'] = editor
    c['editlist'] = editlist
    e.write32(e.desktop + 0xFC, editor if c['has_editor'] else (0xE1D0 if c['other_editor'] else 0))
    # the window found under the mouse
    found = 0
    if c['found'] is not None:
        found = e.alloc(0x20)
        e.uc.mem_write(found, bytes(0x20))
        target = e.alloc(0x100)
        e.uc.mem_write(target, bytes(0x100))
        k1 = e.alloc(0x40)
        e.uc.mem_write(k1, bytes(0x40))
        k0 = e.alloc(0x40)
        e.uc.mem_write(k0, bytes(0x40))
        if c['found'] == 'edit':
            e.write32(target + 0x24, e.cls(CLS_EDIT))
        elif c['found'] == 'edit_derived':
            e.write32(k1 + 0x28, e.cls(CLS_EDIT))
            e.write32(target + 0x24, k1)
        else:
            e.write32(target + 0x24, k0)
        e.write32(found + 4, target)
        if c['parents'] >= 1:
            par = e.alloc(0x20)
            e.uc.mem_write(par, bytes(0x20))
            e.write32(found + 0xC, par)
            e.write32(par + 4, 0xAAAA)
            if c['parents'] >= 2:
                gp = e.alloc(0x20)
                e.uc.mem_write(gp, bytes(0x20))
                e.write32(par + 0xC, gp)
                e.write32(gp + 4, editor if c['gp_is_editor'] else (0xE1D1 if c['gp_wnd'] else 0))
    c['found_entry'] = found
    c['user'] = e.alloc(0x400)
    e.uc.mem_write(c['user'], bytes(0x400))
    e.write32(c['user'] + 0x1D8, c['canc'])
    # the focus entry (KeySlot): 0, or an entry whose window answers vfn 33
    fe = 0
    if c['focus_entry']:
        fe = e.alloc(0x20)
        e.uc.mem_write(fe, bytes(0x20))
        fw = e.alloc(0x20)
        fvt = e.alloc(0x400)
        e.uc.mem_write(fvt, bytes(0x400))
        e.write32(fvt + 0x84, e.stub('IsEditFocus'))
        e.write32(fw, fvt)
        e.write32(fe + 4, fw)
    e.write32(e.glob(0x101A72B4), fe)
    e.write32(e.gchat, c['enterchat'])
    return wnd


def snapshot(e, c, wnd):
    win = [e.read32(wnd + o) for o in range(0x1A4, 0x208, 4)]
    # the heap addresses differ between the two runs: the three pointers are shown against what they point at
    win[1] -= c['keys']
    win[2] -= c['items']
    win[8] -= c['toggle']
    keys = hashlib.md5(bytes(e.uc.mem_read(c['keys'], ITEMS * 0x14))).hexdigest()
    items = hashlib.md5(bytes(e.uc.mem_read(c['items'], ITEMS * SLOT))).hexdigest()
    edits = [e.read32(p + 0x1D8) for p in c['edit_ptrs']]
    return (win, keys, items, edits, e.read32(c['toggle'] + 0xA4), ['editor' if e.read32(e.desktop + 0xFC) == c['editor'] else e.read32(e.desktop + 0xFC)])


def setup_grid(e, c, wnd):
    """the skill grid (NCItemWnd) fields the skill window's OnMouseMove reads: the items (+0xDC), their count (+0xE0), the
    carrying flag (+0xE8), the selected one (+0x108) and the hover (+0x10C)"""
    ptrs = []
    for i in range(c['nitems']):
        p = e.alloc(0x40)
        e.uc.mem_write(p, bytes(0x40))
        e.write32(p, 0x4400 + i)
        ptrs.append(p)
    arr = e.alloc(4 * 8)
    for i, p in enumerate(ptrs):
        e.write32(arr + 4 * i, p)
    e.write32(wnd + 0xDC, arr)
    e.write32(wnd + 0xE0, len(ptrs))
    e.write32(wnd + 0xE4, len(ptrs))
    e.write32(wnd + 0xE8, c['dragging'])
    e.write32(wnd + 0x108, c['selected'])
    e.write32(wnd + 0x10C, 0xFFFFFFFF)


def run(e, c, va, args):
    wnd = build(e, c)
    if c.get('grid'):
        setup_grid(e, c, wnd)
    e.log.clear()
    dev = e.alloc(0x80)
    e.uc.mem_write(dev, bytes(0x80))
    if args == 'dev':
        a = [dev]
    elif args == 'xy':
        a = [c['x'], c['y']]
    elif args == 'lparam':
        a = [0, (c['y'] << 16) | c['x']]
    elif args == 'key':
        a = [c['key'], 0]
    elif args == 'macro':
        a = [c['macro_id'], c['mode']]
    elif args == 'none':
        a = []
    elif args == 'setsc':
        info = e.alloc(0x17A8)
        e.uc.mem_write(info, bytes(0x17A8))
        i = c['info']
        e.write_wstr_at(info + 0x400, i['iconname'])
        for off, k in ((0x1700, 'icon'), (0x170C, 'param'), (0x1714, 'obj'), (0x1718, 'id'), (0x1724, 'lvl'), (0x1768, 'mp'),
                       (0x1798, 'slot')):
            e.write32(info + off, i[k])
        a = [info, i['owner']]
    ret = e.call(va, this=wnd + (c['keyoff'] if args == 'keyobj' else 0), args=a)
    dv = [e.read32(dev + o) for o in (0x2C, 0x30, 0x48, 0x4C, 0x5C)]
    def norm(x):
        # the toggle texture is a heap object: its address differs between the two runs
        if isinstance(x, (tuple, list)):
            return type(x)(norm(y) for y in x)
        return 'TOGGLE' if x == c['toggle'] else x
    out = [u32(ret), dv, norm(list(e.log)), snapshot(e, c, wnd)]
    if c.get('grid'):
        out.append([e.read32(wnd + o) for o in (0xDC, 0xE0, 0xE8, 0x108, 0x10C)][1:])
    if args == 'setsc':
        out.append([e.read32(info + 0x1700)])
    return out


# ---- the random cases ---------------------------------------------------------------------------------------------------------
def rand_slot(r, macros, i):
    ty = r.choice([0, 1, 1, 2, 2, 3, 4, 4])
    sid = r.choice([57, 1, 2, 3, 40, 1665, 1863]) if ty != 4 else (r.choice([m['id'] for m in macros]) if macros and r.random() < 0.8 else r.randint(1, 20))
    return {'icon': r.choice([0, 0x4000 + i]), 'name': r.choice(['', 'Sword', 'Zubei Boots']), 'iconname': r.choice(['', 'icon.a', 'x']),
            'type': ty, 'obj': 100 + i, 'id': sid, 'lvl': r.choice([0, 1, 5, 40]), 'u1998': r.choice([0, 0, 1]),
            'u19ac': r.choice([0, 0, 0, 300, 700]), 'owner': r.choice([1, 2]), 'u1a74': r.choice([0, 0, 5]),
            'used': r.choice([0, 1, 1, 1]), 'running': r.choice([0, 1]), 'flashing': r.choice([0, 0, 1]),
            'sound': r.choice([0, 1])}


def rand_world(r):
    cols = r.choice([10, 10, 10, 6, 4])
    pages = 10
    macros = [{'id': r.randint(1, 20), 'short': r.choice(['', 'A', 'AB', 'ABC', 'ABCD', 'Zz'])} for _ in range(r.randint(0, 5))]
    slots = [rand_slot(r, macros, i) if r.random() < 0.55 else None for i in range(ITEMS)]
    page = r.randint(0, pages - 1)
    horiz = r.choice([0, 1])
    c = {'cols': cols, 'pages': pages, 'page': page, 'iconsize': r.choice([32, 32, 32, 34, 40]), 'horiz': horiz,
         'padmode': r.choice([0, 0, 1, 1, 2]), 'padhold': r.choice([0, 1, 2]), 'back': r.choice([0, 0x1010]), 'mask': 0x1011, 'tip': 0x1012,
         'iconback': 0x1111, 'iconclick': 0x1112, 'next': 0, 'prev': 0, 'rot': 0, 'padbtn': 0, 'holdl': 0x2201, 'holdr': 0x2202,
         'pressedf': r.choice([0, 1]), 'dragging': r.choice([0, 1, 1]), 'pressed': r.randint(-1, ITEMS - 1),
         'hover': r.randint(-1, 20), 'sx': r.randint(0, 600), 'sy': r.randint(0, 400), 'w': r.choice([44, 417, 167]), 'h': r.choice([44, 417, 84]),
         'slots': slots, 'slot_names': None, 'macros': macros, 'macroname': r.choice([None, 'MacroName', 'M2']),
         'macroicon': r.choice([-1, 0, 3, 6]), 'name_pool': ['ItemN', 'SkillN', 'ActionN'],
         'idle': [r.choice([0, 1]) for _ in range(ITEMS)], 'cttex': [r.choice([0, 0x5000 + i]) for i in range(ITEMS)],
         'keyst': [(r.choice([0, 5, 300]), r.choice([0, 200, 500]), r.choice([0, 1, 1]), r.choice([0, 1]), r.choice([0, 1]))
                   for _ in range(ITEMS)],
         'ctrl': r.choice([0, 1]), 'focus': r.choice([0, 1, 1]), 'inwin': r.choice([0, 1]), 'canc': r.choice([0, 1]),
         'x': r.randint(0, 700), 'y': r.randint(0, 600), 'found': r.choice([None, 'edit', 'edit', 'edit_derived', 'other']),
         'parents': r.choice([0, 1, 2, 2, 2]), 'gp_is_editor': r.choice([0, 1, 1]), 'gp_wnd': r.choice([0, 1]),
         'has_editor': r.choice([0, 1, 1]), 'other_editor': r.choice([0, 1]), 'hitline': r.choice([-1, 0, 3, 11]),
         'enterchat': r.choice([0, 1]), 'focus_entry': r.choice([0, 1]), 'editfocus': r.choice([0, 1]),
         'key': r.choice([0x70, 0x71, 0x79, 0x7A, 0x30, 0x31, 0x39, 0x3A, 0x41, 0x6F]), 'keyoff': 0}
    # a point on the bar: near a slot of the current layout most of the time
    if r.random() < 0.7:
        c['x'] = c['sx'] + r.randint(0, 180)
        c['y'] = c['sy'] + r.randint(0, 430)
    if r.random() < 0.5 and c['pressed'] >= 0:
        c['pressed'] = page * cols + r.randint(0, cols - 1)
    return c


def case_paint(r):
    c = rand_world(r)
    c['padmode'] = r.choice([0, 0, 1])
    return c


def case_paintbar(r):
    c = rand_world(r)
    c['padmode'] = 0
    return c


def case_paintpad(r):
    c = rand_world(r)
    c['padmode'] = 1
    return c


def case_point(r):
    c = rand_world(r)
    # points around the cells of the pad layout and of the bar
    if r.random() < 0.6:
        cx, cy = r.choice([(0x23, 0x1A), (0x49, 0x2C), (0x6F, 0x1A), (0x49, 7), (6, 31), (31, 6)])
        c['x'] = c['sx'] + cx + r.randint(-1, c['iconsize'] + 2)
        c['y'] = c['sy'] + cy + r.randint(-1, c['iconsize'] + 2)
    return c


def case_mouse(r):
    c = case_point(r)
    c['pressed'] = r.choice([c['pressed'], -1, c['page'] * c['cols']])
    if r.random() < 0.5 and c['pressed'] >= 0 and c['slots'][c['pressed']] is None:
        c['slots'][c['pressed']] = rand_slot(r, c['macros'], c['pressed'])
    return c


def case_lbup(r):
    c = case_mouse(r)
    # released over the slot pressed in many cases
    if r.random() < 0.5 and c['pressed'] >= 0:
        if c['padmode'] == 1:
            c['padhold'] = c['pressed'] % 12 // 4 if c['pressed'] % 12 < 12 else 0
            cx, cy = [(0x23, 0x1A), (0x49, 0x2C), (0x6F, 0x1A), (0x49, 7)][c['pressed'] % 4]
            c['x'], c['y'] = c['sx'] + cx + 2, c['sy'] + cy + 2
        elif c['padmode'] == 0:
            i = c['pressed'] - c['page'] * c['cols']
            along = 31 + i * (c['iconsize'] + 5) + 2
            c['x'], c['y'] = (c['sx'] + along, c['sy'] + 8) if c['horiz'] else (c['sx'] + 8, c['sy'] + along)
    return c


def case_skillmove(r):
    c = case_mouse(r)
    c.update({'grid': 1, 'nitems': r.randint(0, 5), 'selected': r.randint(-1, 5), 'hit': r.randint(-1, 5)})
    c['dragging'] = r.choice([0, 1, 1])
    return c


def case_keydown(r):
    c = rand_world(r)
    s = c['page'] * c['cols']
    c['pressed'] = -1
    k = r.randint(0, min(9, ITEMS - s - 1))
    if r.random() < 0.6:
        c['slots'][s + k] = rand_slot(r, c['macros'], s)
    if r.random() < 0.7:
        c['key'] = r.choice([0x70 + k, 0x30 + (k + 1) % 10])
        c['keyst'][s + k] = (0, 0, 1, 0, 0) if r.random() < 0.8 else (0, 0, 0, 0, 0)
    return c


def case_setsc(r):
    c = rand_world(r)
    c['info'] = {'iconname': r.choice(['', 'a.b', 'Macro_Icon1']), 'icon': r.choice([0, 0x4444]), 'param': r.choice([1, 2, 3, 4, 0, 5]),
                 'obj': r.randint(1, 500), 'id': r.choice([2, 5, 57, 1, 40, 0x681, 1665]), 'lvl': r.randint(0, 20), 'mp': r.choice([0, 0, 7]),
                 'slot': r.choice([-1, 0, 3, 11, 50, 99, 100, 101]), 'owner': r.choice([1, 2])}
    return c


def case_macro(r):
    c = rand_world(r)
    c['macro_id'] = r.choice([m['id'] for m in c['macros']] + [r.randint(1, 20)]) if c['macros'] else r.randint(1, 20)
    c['mode'] = r.choice([0, 2, 2, 1, 3])
    for i in range(ITEMS):
        if r.random() < 0.15:
            c['slots'][i] = {'icon': 0, 'name': '', 'iconname': '', 'type': 4, 'obj': i, 'id': c['macro_id'], 'lvl': 0, 'u1998': 0,
                             'u19ac': 0, 'owner': 1, 'u1a74': 0, 'used': r.choice([0, 1, 1]), 'running': 0, 'flashing': 0, 'sound': 0}
    return c


def case_key(r):
    c = rand_world(r)
    c['keyoff'] = 0
    return c


def amount_test(n, seed):
    r = random.Random(seed)
    texts = [''.join(r.choice('0123456789,,') for _ in range(r.randint(0, 16))) for _ in range(n)]
    res = {}
    for kind in ('stock', 'ours'):
        e = ScEmu(kind)
        e.heap_mark = e.heap_ptr
        va = 0x1002BB70 if kind == 'stock' else sym_of(e, '?NAmountColor@@')
        out = []
        for t in texts:
            e.heap_ptr = e.heap_mark
            p = e.put_wstr(t)
            out.append(u32(e.call(va, this=None, args=[p])))
        res[kind] = out
    nb = sum(1 for a, b in zip(res['stock'], res['ours']) if a != b)
    print('NAmountColor cases', n, 'differ', nb, 'colors', sorted(set(res['stock'])))
    return nb


def sym_of(e, pre):
    c = [v for s, v in e.symbols.items() if s.startswith(pre)]
    assert len(c) >= 1, pre
    return c[0]


NORET = ('PaintBar', 'PaintPad', 'ReloadNames')   # void: eax is whatever the stock code left there

TESTS = {
    'NAmountColor': (0, '', '', None),
    'PaintBar': (0x100CD790, '?PaintBar@NCShortCutWnd@@', 'dev', case_paintbar),
    'PaintPad': (0x100CE160, '?PaintPad@NCShortCutWnd@@', 'dev', case_paintpad),
    'OnPaint': (0x100CE740, '?OnPaint@NCShortCutWnd@@', 'dev', case_paint),
    'SlotAtPoint': (0x100CCEF0, '?SlotAtPoint@NCShortCutWnd@@', 'xy', case_point),
    'PadSlotAt': (0x100CCE40, '?PadSlotAt@NCShortCutWnd@@', 'xy', case_point),
    'OnMouseMove': (0x100CE820, '?OnMouseMove@NCShortCutWnd@@', 'lparam', case_mouse),
    'OnLButtonUp': (0x100CE9D0, '?OnLButtonUp@NCShortCutWnd@@', 'lparam', case_lbup),
    'SkillMouseMove': (0x10090EE0, '?OnMouseMove@NCSkillItemWnd@@', 'lparam', case_skillmove),
    'OnShortCutKeyDown': (0x100CF080, '?OnShortCutKeyDown@NCShortCutWnd@@', 'key', case_keydown),
    'SetShortCut': (0x100CFE90, '?SetShortCut@NCShortCutWnd@@', 'setsc', case_setsc),
    'UpdateMacro': (0x100CC8D0, '?UpdateMacro@NCShortCutWnd@@', 'macro', case_macro),
    'ReloadNames': (0x100CD250, '?ReloadNames@NCShortCutWnd@@', 'none', rand_world),
}


def main(n, seed, only):
    bad = 0
    for name, (sva, opre, args, mk) in TESTS.items():
        if only and name not in only:
            continue
        if name == 'NAmountColor':
            bad += amount_test(n, seed)
            continue
        r = random.Random(seed)
        cases = [mk(r) for _ in range(n)]
        res = {}
        for kind in ('stock', 'ours'):
            e = ScEmu(kind)
            e.heap_mark = e.heap_ptr
            va = sva if kind == 'stock' else sym_of(e, opre)
            res[kind] = []
            for c in cases:
                try:
                    res[kind].append(run(e, c, va, args))
                except Exception as ex:
                    if os.environ.get('NWD_TRACE') and not getattr(e, 'traced', False):
                        import traceback
                        traceback.print_exc()
                        e.traced = True
                    res[kind].append(('error', type(ex).__name__, str(ex)[:200]))
        nb = 0
        crashes = 0
        cov = {}
        for i, (a, b) in enumerate(zip(res['stock'], res['ours'])):
            if a[0] == 'error' and b[0] == 'error':
                crashes += 1
                continue
            if a[0] != 'error':
                for l in a[2]:
                    cov[l[0]] = cov.get(l[0], 0) + 1
            if name in NORET and a[0] != 'error' and b[0] != 'error':
                a = [0] + list(a[1:])
                b = [0] + list(b[1:])
            if a != b:
                nb += 1
                if nb <= 2:
                    print(name, 'case', i, 'ret', a[0], b[0], 'device', a[1] == b[1] if a[0] != 'error' and b[0] != 'error' else '')
                    if a[0] != 'error' and b[0] != 'error':
                        la, lb = a[2], b[2]
                        k = 0
                        while k < min(len(la), len(lb)) and la[k] == lb[k]:
                            k += 1
                        print('  logs differ at', k, 'of', len(la), len(lb), 'stock', str(la[k:k + 3])[:500], 'ours', str(lb[k:k + 3])[:500])
                        if la == lb:
                            print('  state stock', str(a[3])[:700])
                            print('  state ours ', str(b[3])[:700])
                    else:
                        print('  stock', str(a)[:300], 'ours', str(b)[:300])
        print(name, 'cases', n, 'differ', nb, '(both crash: %d)' % crashes, 'coverage', len(cov), sorted(cov.items())[:40] if os.environ.get('NWD_COV') else '')
        bad += nb
    return bad


if __name__ == '__main__':
    sys.exit(1 if main(int(sys.argv[1]), int(sys.argv[2]), sys.argv[3:]) else 0)
