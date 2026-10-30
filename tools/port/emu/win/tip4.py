# Fake world for the C4 tool tip tests (stock vs ours under unicorn): NToolTipInfo / NToolTipSkin functions with the device, the
# console's text getters and the engine's string/array imports stubbed (logging). Shared by t_tipdraw.py, t_tiplayout.py, t_tipfill.py.
import sys, os, random, struct
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from gw import *
import gw
import stubs
import e4
from unicorn.x86_const import *

# the edit box test's service tables are not ours: every service here is a GW callee
e4.STOCK_X.clear()
e4.OURS_X.clear()

# thiscall services: stock VA -> (name, bytes the callee pops, returns this)
CALLEES = {
    0x10012aa0: ('DrawText', 0x3C, 0), 0x10012d90: ('DrawTexture', 0x2C, 0), 0x10012bf0: ('DrawStretchedTex', 0x1C, 0),
    0x10013330: ('SetClip', 0x10, 0), 0x10013340: ('ReleaseClip', 0, 0),
    0x1006b440: ('GetGradeText', 4, 0), 0x1006acd0: ('GetItemSlotText', 0x10, 0), 0x1006abe0: ('GetWeaponTypeName', 4, 0),
    0x1006afc0: ('GetAttackSpeedText', 4, 0), 0x100741b0: ('GetEnchantedPAtk', 0x14, 0), 0x10074250: ('GetEnchantedMAtk', 0x14, 0),
    0x100742f0: ('GetEnchantedDefense', 0xC, 0), 0x1007d730: ('IsMagicArmor', 4, 0), 0x1006aa10: ('GetSkillTypeText', 8, 0),
    0x1006ab20: ('GetSkillMpCost', 8, 0), 0x1006ab50: ('GetSkillRange', 8, 0), 0x1006aa80: ('GetMSHpConsume', 8, 0),
    0x1006b250: ('GetClassNameString', 4, 0), 0x1006b130: ('ConvertNewLines', 8, 0), 0x10035650: ('GetLocalRect', 4, 0),
    # NItemEntry's getters (the entry is a plain buffer in the tests; ours has them as NW_STOCK stubs)
    0x1001e460: ('ItemGetName', 0, 0), 0x1001e510: ('ItemGetAdditionalName', 0, 0), 0x1001e530: ('ItemGetDescription', 0, 0),
    0x1001e5b0: ('ItemIsStackable', 0, 0),
}
# cdecl services: stock VA -> (name, number of dword arguments)
CDECL = {0x10012960: ('GetTextExtent', 8), 0x100134b0: ('GetCharSize', 3), 0x10037ce0: ('MakeTimeStr', 2)}
OURS_SYMS = {
    0x10012aa0: '?DrawText@NCGDevice@@QAEKHHKPBGKKKW4L2FontType@@HHKHHHH@Z',
    0x10012d90: '?DrawTexture@NCGDevice@@QAEXHHHHHHHHPAVUTexture@@KH@Z',
    0x10012bf0: '?DrawStretchedTex@NCGDevice@@QAEXHHHHPAVUTexture@@HK@Z',
    0x10013330: '?SetClip@NCGDevice@@QAEXHHHH@Z', 0x10013340: '?ReleaseClip@NCGDevice@@QAEXXZ',
    0x10012960: '?GetTextExtent@NCGDevice@@SAKPAUtagSIZE@@PBGKKHHKW4L2FontType@@@Z',
    0x100134b0: '?GetCharSize@NCGDevice@@SAXPAUtagSIZE@@GH@Z',
    # the console's getters: ours may call a standalone compiled copy instead of the hook's (db: two copies)
    0x1006aa10: '?GetSkillTypeText@NConsoleWnd@@QAEPBGHH@Z', 0x1006ab20: '?GetSkillMpCost@NConsoleWnd@@QAEHHH@Z',
    0x1006ab50: '?GetSkillRange@NConsoleWnd@@QAEHHH@Z', 0x100742f0: '?GetEnchantedDefense@NConsoleWnd@@QAEHHHH@Z',
    0x1006abe0: '?GetWeaponTypeName@NConsoleWnd@@QAEPBGH@Z', 0x1006afc0: '?GetAttackSpeedText@NConsoleWnd@@QAEPBGH@Z',
    0x1006b250: '?GetClassNameString@NConsoleWnd@@QAEPBGH@Z', 0x1006b440: '?GetGradeText@NConsoleWnd@@QAEPBGH@Z',
    0x1006acd0: '?GetItemSlotText@NConsoleWnd@@QAEXHHAAVFString@@H@Z', 0x1006aa80: '?GetMSHpConsume@NConsoleWnd@@QAEHHH@Z',
    0x1007d730: '?IsMagicArmor@NConsoleWnd@@QAEHH@Z', 0x1006b130: '?ConvertNewLines@NConsoleWnd@@QAEXPAGAAVFString@@@Z',
}
GRADES = {1: 'D', 2: 'C', 3: 'B', 4: 'A', 5: 'S'}
stubs.IMPORT_POP.update({'?appStrcpy@@YAPAGPAGPBG@Z': 0, '?appItoa@@YAPAGH@Z': 0, '?appSprintf@@YAHPAGPBGZZ': 0,
                         '?appStrcat@@YAPAGPAGPBG@Z': 0, '?appStrlen@@YAHPBG@Z': 0, '?appStrcmp@@YAHPBG0@Z': 0,
                         '?GetMSData@FL2GameData@@QAEPAUFL2MagicSkillData@@HH@Z': 8, '?appAtoi@@YAHPBG@Z': 0})
SYSSTR_BASE = 0x9871C


class TW(GW):
    def __init__(self, kind, extra_callees=None, extra_cdecl=None):
        cal = dict(CALLEES)
        self.cdecl = {}
        for va, (nm, n) in list(CDECL.items()) + list((extra_cdecl or {}).items()):
            cal[va] = (nm, 0, 0)
            self.cdecl[nm] = n
        cal.update(extra_callees or {})
        GW.__init__(self, kind, cal, OURS_SYMS)
        for i in range(0, 3000):
            self.fs_set(self.gdata + SYSSTR_BASE + 12 * i, 'S%d' % i)
        self.wait = []

    # ---- the services ------------------------------------------------------------------------------------------
    def dispatch(self, name):
        kind, nm = name.split(':', 1)
        if kind == 'cb':
            if nm in self.cdecl:
                self.last_pop = 0
                a = self.sargs(self.cdecl[nm])
                return self.cdecl_call(nm, a)
            a = self.sargs(self.cb[nm][1] // 4)
            r = self.thiscall(nm, self.this(), a)
            if r is not NotImplemented:
                self.last_pop = self.cb[nm][1]
                return r
        return GW.dispatch(self, name)

    def cdecl_call(self, nm, a):
        if nm == 'GetTextExtent':
            size, text = a[0], self.wstr(a[1])
            w = a[2]
            cx = 6 * len(text) + (a[6] if a[6] else 0)
            lines = 1
            if w and w < 0x7FFFFFFF:
                lines = max(1, -(-cx // w))
                cx = min(cx, w)
            self.write32(size, cx)
            self.write32(size + 4, 13 * lines)
            self.log.append(('GetTextExtent', text, a[2:]))
            return 6 * len(text) + 3
        if nm == 'GetCharSize':
            self.write32(a[0], 6)
            self.write32(a[0] + 4, 13)
            self.log.append(('GetCharSize', a[1], a[2]))
            return None
        if nm == 'MakeTimeStr':
            # cdecl, the FString comes back through the hidden first argument (and in eax)
            self.log.append(('MakeTimeStr', a[1]))
            self.fs_set(a[0], 'T%d' % a[1])
            return a[0]
        raise RuntimeError('cdecl ' + nm)

    def thiscall(self, nm, t, a):
        if nm == 'DrawText':
            text = self.wstr(a[3])
            self.log.append(('DrawText', a[:3], text, a[4:]))
            return 6 * len(text) + 1
        if nm == 'DrawTexture':
            self.log.append(('DrawTexture', [self.val(x) for x in a]))
            return None
        if nm == 'DrawStretchedTex':
            self.log.append(('DrawStretchedTex', [self.val(x) for x in a]))
            return None
        if nm == 'SetClip':
            self.log.append(('SetClip', a))
            return None
        if nm == 'ReleaseClip':
            self.log.append(('ReleaseClip',))
            return None
        if nm == 'GetGradeText':
            g = a[0] if a[0] < 0x80000000 else a[0] - (1 << 32)
            self.log.append(('GetGradeText', g))
            return self.static_str(GRADES[g]) if g in GRADES else 0
        if nm == 'GetItemSlotText':
            ty, bp, res, sub = a
            self.fs_set(res, 'slot%d_%d_%d' % (ty, bp, sub))
            self.log.append(('GetItemSlotText', ty, bp, sub))
            return None
        if nm == 'GetWeaponTypeName':
            n = a[0] if a[0] < 0x80000000 else a[0] - (1 << 32)
            self.log.append(('GetWeaponTypeName', n))
            return self.static_str('wt%d' % n) if n >= 0 and n != 9 else 0
        if nm == 'GetAttackSpeedText':
            self.log.append(('GetAttackSpeedText', a[0]))
            return self.static_str('spd%d' % a[0])
        if nm in ('GetEnchantedPAtk', 'GetEnchantedMAtk'):
            self.log.append((nm, a))
            return u32(a[4] + 100 * a[2] + a[3] + a[0] + (1 if nm == 'GetEnchantedMAtk' else 0))
        if nm == 'GetEnchantedDefense':
            self.log.append((nm, a))
            return u32(a[2] + 100 * a[0] + a[1] + 1)
        if nm == 'IsMagicArmor':
            self.log.append((nm, a[0]))
            return 1 if a[0] % 2 == 0 else 0
        if nm == 'GetSkillTypeText':
            self.log.append((nm, a))
            return self.static_str('stype') if a[0] % 3 != 0 else 0
        if nm == 'GetSkillMpCost':
            self.log.append((nm, a))
            return u32((a[0] * a[1]) % 7)
        if nm == 'GetSkillRange':
            self.log.append((nm, a))
            return u32((a[0] + a[1]) % 5 - 1)
        if nm == 'GetMSHpConsume':
            self.log.append((nm, a))
            return u32((a[0] + 2 * a[1]) % 4)
        if nm == 'GetClassNameString':
            self.log.append((nm, a))
            return self.static_str('class%d' % a[0])
        if nm == 'ConvertNewLines':
            text = self.wstr(a[0]) if a[0] else ''
            self.log.append((nm, text))
            self.fs_set(a[1], text.replace('\\n', '\r\n'))
            return None
        if nm in ('ItemGetName', 'ItemGetAdditionalName', 'ItemGetDescription'):
            self.log.append((nm,))
            p = t + {'ItemGetName': 0x14, 'ItemGetAdditionalName': 0x214, 'ItemGetDescription': 0x914}[nm]
            return p if bytes(self.uc.mem_read(p, 2)) != b'\x00\x00' else 0
        if nm == 'ItemIsStackable':
            self.log.append((nm,))
            return 1 if self.rds32(t + 0x1B4C) in (1, 2, 3) else 0
        if nm == 'GetLocalRect':
            self.log.append((nm,))
            self.write32(a[0], 0); self.write32(a[0] + 4, 0)
            self.write32(a[0] + 8, struct.unpack('<I', struct.pack('<f', 1024.0))[0])
            self.write32(a[0] + 12, struct.unpack('<I', struct.pack('<f', 768.0))[0])
            return a[0]
        return NotImplemented

    def do_import(self, nm):
        t = self.this()
        if nm == '?appItoa@@YAPAGH@Z':
            v = self.rds32(self.reg(UC_X86_REG_ESP) + 4)
            self.write_wstr_at(self.itoa_buf, str(v))
            return self.itoa_buf
        if nm == '?GetMSData@FL2GameData@@QAEPAUFL2MagicSkillData@@HH@Z':
            sid = self.rds32(self.reg(UC_X86_REG_ESP) + 4)
            lvl = self.rds32(self.reg(UC_X86_REG_ESP) + 8)
            self.log.append(('GetMSData', sid, lvl))
            if sid % 5 == 0:
                return 0
            p = self.alloc(0x78)
            self.uc.mem_write(p, bytes(0x78))
            self.write32(p + 0x68, (sid + lvl) % 2)
            self.write32(p + 0x6C, lvl + 100)
            return p
        if nm == '?appAtoi@@YAHPBG@Z':
            import re as _re
            m = _re.match(r'\s*([+-]?\d+)', self.wstr(self.arg(0)))
            return u32(int(m.group(1))) if m else 0
        if nm == '?appStrcmp@@YAHPBG0@Z':
            x, y = self.wstr(self.arg(0)), self.wstr(self.arg(1))
            return 0 if x == y else (1 if x > y else 0xFFFFFFFF)
        return GW.do_import(self, nm)


def f32(x):
    return struct.unpack('<I', struct.pack('<f', float(x)))[0]


def get_f32(v):
    return struct.unpack('<f', struct.pack('<I', v & 0xFFFFFFFF))[0]


# ---- the objects ----------------------------------------------------------------------------------------------------
LINE = 0x418


def put_tex(e, usize, vsize):
    p = e.alloc(0x60)
    e.uc.mem_write(p, bytes(0x60))
    e.write32(p + 0x4C, usize)
    e.write32(p + 0x50, vsize)
    e.objs[p] = 'tex%dx%d' % (usize, vsize)
    return p


def put_line(e, data, k, ln):
    base = data + k * LINE
    e.uc.mem_write(base, bytes(LINE))
    e.write_wstr_at(base, ln['label'])
    e.write_wstr_at(base + 0x200, ln['value'])
    e.write32(base + 0x400, ln.get('lcol', 0xFFA3A3A3))
    e.write32(base + 0x404, ln.get('vcol', 0xFFB09B79))
    e.write32(base + 0x408, ln.get('u408', 0))
    e.write32(base + 0x40C, ln.get('blabel', 1))
    e.write32(base + 0x410, ln.get('bcolon', 1))
    e.write32(base + 0x414, put_tex(e, ln['tw'], ln['th']) if ln.get('tw') else 0)


def put_info(e, info, c):
    """NToolTipInfo (C4, 0x80 bytes) at `info` from the dict c"""
    e.uc.mem_write(info, bytes(0x80))
    e.fs_set(info + 0x00, c.get('name', ''))
    e.fs_set(info + 0x0C, c.get('name2', ''))
    for off, key in ((0x18, 'u18'), (0x1C, 'enchant'), (0x20, 'grade'), (0x24, 'count'), (0x28, 'extra'), (0x2C, 'show'),
                     (0x30, 'type'), (0x34, 'level'), (0x38, 'itemid'), (0x3C, 'sctype'), (0x58, 'style'), (0x5C, 'opts')):
        e.write32(info + off, c.get(key, 0))
    lines = c.get('lines', [])
    data = e.alloc(LINE * max(1, len(lines)) + 16)
    for k, ln in enumerate(lines):
        put_line(e, data, k, ln)
    e.write32(info + 0x40, data if lines else 0)
    e.write32(info + 0x44, len(lines))
    e.write32(info + 0x48, len(lines))
    e.fs_set(info + 0x4C, c.get('desc', ''))
    for off, key in ((0x60, 'fx'), (0x64, 'fy')):
        e.write32(info + off, c.get(key, 0))
    e.write32(info + 0x68, f32(c.get('fw', 0)))
    e.write32(info + 0x6C, f32(c.get('fh', 0)))
    e.write32(info + 0x70, c.get('tx', 0))
    e.write32(info + 0x74, c.get('ty', 0))
    e.write32(info + 0x78, f32(c.get('tw', 0)))
    e.write32(info + 0x7C, f32(c.get('th', 0)))


def wcut(b):
    out = []
    for i in range(0, len(b), 2):
        c = b[i] | (b[i + 1] << 8)
        if c == 0:
            break
        out.append(chr(c))
    return ''.join(out)


def dump_info(e, info):
    d = {}
    d['name'] = e.fs_get(info)
    d['name2'] = e.fs_get(info + 0xC)
    d['ints'] = [e.rds32(info + o) for o in range(0x18, 0x40, 4)]
    data, num = e.read32(info + 0x40), e.read32(info + 0x44)
    lines = []
    for i in range(min(num, 64)):
        b = bytes(e.uc.mem_read(data + LINE * i, LINE))
        tex = struct.unpack('<I', b[0x414:0x418])[0]
        lines.append((wcut(b[:0x200]), wcut(b[0x200:0x400]), struct.unpack('<IIiii', b[0x400:0x414]),
                      e.objs.get(tex, tex) if tex else 0))
    d['lines'] = lines
    d['desc'] = e.fs_get(info + 0x4C)
    d['tail'] = [e.rds32(info + o) for o in range(0x58, 0x80, 4)]
    return d
