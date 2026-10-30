import struct
from emu import *

GRADES = {1: 'D', 2: 'C', 3: 'B', 4: 'A', 5: 'S'}

# stock addresses of the services stubbed (everything else runs as code)
STOCK_FN = {
    0x1000f3d0: 'GetTextExtent', 0x1000f380: 'GetCharSize', 0x1000f510: 'DrawText', 0x1000f750: 'DrawTexture',
    0x1000f660: 'DrawStretchedTex', 0x1000fb20: 'SetClip', 0x1000fb30: 'ReleaseClip',
    0x100570b0: 'GetGradeText', 0x10056b30: 'GetItemSlotText', 0x10056a40: 'GetWeaponTypeName',
    0x10056e00: 'GetAttackSpeedText', 0x10060e30: 'GetEnchantedAttack', 0x10060e60: 'GetEnchantedDefense',
    0x1006abf0: 'IsMagicArmor', 0x10056910: 'GetSkillTypeText', 0x10056980: 'GetSkillMpCost',
    0x100569b0: 'GetSkillRange', 0x10057090: 'GetClassNameString', 0x10056f70: 'ConvertNewLines',
}
# (name, pop bytes of the callee)
FN_POP = {
    'GetTextExtent': 0, 'GetCharSize': 0, 'DrawText': 0x3C, 'DrawTexture': 0x24, 'DrawStretchedTex': 0x14,
    'SetClip': 0x10, 'ReleaseClip': 0, 'GetGradeText': 4, 'GetItemSlotText': 0x10, 'GetWeaponTypeName': 4,
    'GetAttackSpeedText': 4, 'GetEnchantedAttack': 0xC, 'GetEnchantedDefense': 0xC, 'IsMagicArmor': 4,
    'GetSkillTypeText': 8, 'GetSkillMpCost': 8, 'GetSkillRange': 8, 'GetClassNameString': 4, 'ConvertNewLines': 8,
    'GetEquipSlot': 8,
}
# the same services in our DLL: a substring of the decorated name (class, method)
OURS_FN = {
    'GetTextExtent': ('@NCGDevice@@', '?GetTextExtent@'), 'GetCharSize': ('@NCGDevice@@', '?GetCharSize@'),
    'DrawText': ('@NCGDevice@@', '?DrawText@'), 'DrawTexture': ('@NCGDevice@@', '?DrawTexture@'),
    'DrawStretchedTex': ('@NCGDevice@@', '?DrawStretchedTex@'), 'SetClip': ('@NCGDevice@@', '?SetClip@'),
    'ReleaseClip': ('@NCGDevice@@', '?ReleaseClip@'),
    'GetGradeText': ('@NConsoleWnd@@', '?GetGradeText@'), 'GetItemSlotText': ('@NConsoleWnd@@', '?GetItemSlotText@'),
    'GetWeaponTypeName': ('@NConsoleWnd@@', '?GetWeaponTypeName@'),
    'GetAttackSpeedText': ('@NConsoleWnd@@', '?GetAttackSpeedText@'),
    'GetEnchantedAttack': ('@NConsoleWnd@@', '?GetEnchantedAttack@'),
    'GetEnchantedDefense': ('@NConsoleWnd@@', '?GetEnchantedDefense@'),
    'IsMagicArmor': ('@NConsoleWnd@@', '?IsMagicArmor@'), 'GetSkillTypeText': ('@NConsoleWnd@@', '?GetSkillTypeText@'),
    'GetSkillMpCost': ('@NConsoleWnd@@', '?GetSkillMpCost@'), 'GetSkillRange': ('@NConsoleWnd@@', '?GetSkillRange@'),
    'GetClassNameString': ('@NConsoleWnd@@', '?GetClassNameString@'),
    'ConvertNewLines': ('@NConsoleWnd@@', '?ConvertNewLines@'),
}

IMPORT_POP = {
    '??0FString@@QAE@XZ': 0, '??0FString@@QAE@PBG@Z': 4, '??0FString@@QAE@ABV0@@Z': 4, '??1FString@@QAE@XZ': 0,
    '??4FString@@QAEAAV0@PBG@Z': 4, '??4FString@@QAEAAV0@ABV0@@Z': 4, '??YFString@@QAEAAV0@PBG@Z': 4,
    '??YFString@@QAEAAV0@ABV0@@Z': 4, '??DFString@@QBEPBGXZ': 0, '?Empty@FString@@QAEXXZ': 0,
    '?Add@FArray@@QAEHHH@Z': 8, '?Realloc@FArray@@IAEXH@Z': 4, '?Remove@FArray@@QAEXHHH@Z': 12,
    '??1FArray@@QAE@XZ': 0, '?appSprintf@@YAHPAGPBGZZ': 0, '?appItoa@@YAPAGH@Z': 0,
    '?appStrcpy@@YAPAGPAGPBG@Z': 0, '?appStrcat@@YAPAGPAGPBG@Z': 0, '?appStrlen@@YAHPBG@Z': 0,
    '?appFailAssert@@YAXPBD0H@Z': 0,
    '??0UObject@@QAE@XZ': 0,
}


def wfmt(emu, fmt, args):
    out = []
    i = 0
    ai = 0
    while i < len(fmt):
        c = fmt[i]
        if c != '%':
            out.append(c)
            i += 1
            continue
        i += 1
        spec = ''
        while fmt[i] in '-+ #0123456789.l':
            spec += fmt[i]
            i += 1
        t = fmt[i]
        i += 1
        if t == '%':
            out.append('%')
        elif t == 's':
            v = args[ai]
            ai += 1
            out.append(emu.wstr(v) if v else '(null)')
        elif t in 'dui':
            v = args[ai]
            ai += 1
            if v & 0x80000000:
                v -= 1 << 32
            s = spec.replace('l', '')
            out.append(('%' + s + 'd') % v)
        elif t in 'xX':
            v = args[ai]
            ai += 1
            out.append(('%' + spec.replace('l', '') + t) % v)
        else:
            raise RuntimeError('format ' + fmt)
    return ''.join(out)


class TipEmu(Emu):
    # the stock address of GConsole (the C2 image's; a C4 script sets TipEmu.CONSOLE_VA = 0x102C39F4 and fills STOCK_FN / OURS_FN with the C4 services)
    CONSOLE_VA = 0x101A415C

    def __init__(self, kind):
        super().__init__(kind)
        self.last_pop = 0
        self.static_cache = {}
        # fake engine objects
        self.gdata = self.alloc(0x60000)
        self.empty_wstr = self.put_wstr('')
        for i in range(0, 3000):
            self.fs_set(self.gdata + 0x4B4E8 + 12 * i, 'S%d' % i)
        self.console = self.alloc(0x4000)
        self.desktop = self.alloc(0x200)
        self.write32(self.desktop + 0x38, 1024)
        self.write32(self.console + 0x3540, self.desktop)
        self.gmalloc_obj = self.alloc(16)
        self.gmalloc_vt = self.alloc(64)
        self.write32(self.gmalloc_obj, self.gmalloc_vt)
        self.gmalloc_var = self.alloc(4)
        self.write32(self.gmalloc_var, self.gmalloc_obj)
        self.itoa_buf = self.alloc(64)
        self.heap_mark = None
        # data imports
        for addr, name in list(self.import_names.items()):
            pass
        for e in self.pe.DIRECTORY_ENTRY_IMPORT:
            for imp in e.imports:
                nm = imp.name.decode() if imp.name else ''
                if 'GL2GameData' in nm:
                    self.write32(imp.address, self.gdata)
                    self.stubs.pop(self.read32_stub(nm), None)
                elif nm.startswith('?GMalloc'):
                    self.write32(imp.address, self.gmalloc_var)
        if kind == 'stock':
            self.write32(self.CONSOLE_VA, self.console)
            fns = STOCK_FN
        else:
            self.write32(NWBASE + (self.CONSOLE_VA - 0x10000000), self.console)
            fns = {}
            for nm, (cls, meth) in OURS_FN.items():
                cands = [(k, v) for k, v in self.symbols.items() if k.startswith(meth) and cls in k]
                if nm == 'DrawStretchedTex':
                    cands = [min(cands, key=lambda c: len(c[0]))]
                if len(cands) != 1:
                    raise RuntimeError('ours symbol %s: %s' % (nm, cands))
                fns[cands[0][1]] = nm
        self.fn_stub_addr = {}
        n = 0
        for va, nm in fns.items():
            stub = STUB_BASE + 0x8000 + 16 * n
            n += 1
            self.uc.mem_write(stub, b'\xC3')
            self.stubs[stub] = ('fn:' + nm, None)
            self.uc.mem_write(va, b'\x68' + struct.pack('<I', stub) + b'\xC3')

    def read32_stub(self, nm):
        return None

    # ---- the dispatcher -----------------------------------------------------------------------
    def arg(self, i):
        return self.stack_arg(i)

    def dispatch(self, name):
        kind, nm = name.split(':', 1)
        if kind == 'imp':
            self.last_pop = IMPORT_POP.get(nm)
            if self.last_pop is None:
                raise RuntimeError('unstubbed import ' + nm)
            return self.do_import(nm)
        self.last_pop = FN_POP[nm]
        return self.do_fn(nm)

    def this(self):
        return self.reg(UC_X86_REG_ECX)

    def static_str(self, s):
        if s not in self.static_cache:
            if not hasattr(self, 'static_ptr'):
                self.uc.mem_map(0x64000000, 0x100000)
                self.static_ptr = 0x64000000
            a = self.static_ptr
            self.uc.mem_write(a, s.encode('utf-16-le') + b'\0\0')
            self.static_ptr += (2 * len(s) + 2 + 15) & ~15
            self.static_cache[s] = a
        return self.static_cache[s]

    def do_import(self, nm):
        t = self.this()
        if nm == '??0FString@@QAE@XZ':
            self.write32(t, 0); self.write32(t + 4, 0); self.write32(t + 8, 0)
            return t
        if nm == '??0FString@@QAE@PBG@Z':
            self.fs_set(t, self.wstr(self.arg(0)) if self.arg(0) else '')
            return t
        if nm == '??0FString@@QAE@ABV0@@Z':
            self.fs_set(t, self.fs_get(self.arg(0)))
            return t
        if nm == '??1FString@@QAE@XZ':
            return t
        if nm == '??4FString@@QAEAAV0@PBG@Z':
            p = self.arg(0)
            self.fs_set(t, self.wstr(p) if p else '')
            return t
        if nm == '??4FString@@QAEAAV0@ABV0@@Z':
            self.fs_set(t, self.fs_get(self.arg(0)))
            return t
        if nm == '??YFString@@QAEAAV0@PBG@Z':
            p = self.arg(0)
            self.fs_set(t, self.fs_get(t) + (self.wstr(p) if p else ''))
            return t
        if nm == '??YFString@@QAEAAV0@ABV0@@Z':
            self.fs_set(t, self.fs_get(t) + self.fs_get(self.arg(0)))
            return t
        if nm == '??DFString@@QBEPBGXZ':
            return self.read32(t) if self.read32(t + 4) else self.empty_wstr
        if nm == '?Empty@FString@@QAEXXZ':
            self.write32(t, 0); self.write32(t + 4, 0); self.write32(t + 8, 0)
            return None
        if nm == '?Add@FArray@@QAEHHH@Z':
            cnt, size = self.arg(0), self.arg(1)
            data, num, mx = self.read32(t), self.read32(t + 4), self.read32(t + 8)
            idx = num
            num += cnt
            if num > mx:
                mx = num + 3 * num // 8 + 16
                nd = self.alloc(mx * size)
                if data:
                    self.uc.mem_write(nd, bytes(self.uc.mem_read(data, idx * size)))
                data = nd
            self.write32(t, data); self.write32(t + 4, num); self.write32(t + 8, mx)
            return idx
        if nm == '?Realloc@FArray@@IAEXH@Z':
            size = self.arg(0)
            data, num, mx = self.read32(t), self.read32(t + 4), self.read32(t + 8)
            if mx == 0:
                self.write32(t, 0)
            else:
                nd = self.alloc(mx * size)
                if data:
                    self.uc.mem_write(nd, bytes(self.uc.mem_read(data, min(num, mx) * size)))
                self.write32(t, nd)
            return None
        if nm == '?Remove@FArray@@QAEXHHH@Z':
            idx, cnt, size = self.arg(0), self.arg(1), self.arg(2)
            data, num = self.read32(t), self.read32(t + 4)
            if cnt:
                tail = bytes(self.uc.mem_read(data + (idx + cnt) * size, (num - idx - cnt) * size))
                self.uc.mem_write(data + idx * size, tail)
            self.write32(t + 4, num - cnt)
            return None
        if nm == '??1FArray@@QAE@XZ':
            return None
        if nm == '?appSprintf@@YAHPAGPBGZZ':
            dst, fmt = self.arg(0), self.wstr(self.arg(1))
            args = [self.stack_arg(2 + i) for i in range(8)]
            s = wfmt(self, fmt, args)
            self.write_wstr_at(dst, s)
            self.log.append(('appSprintf', fmt, s))
            return len(s)
        if nm == '?appItoa@@YAPAGH@Z':
            v = self.rds32(self.reg(UC_X86_REG_ESP) + 4)
            self.write_wstr_at(self.itoa_buf, str(v))
            return self.itoa_buf
        if nm == '?appStrcpy@@YAPAGPAGPBG@Z':
            d, s = self.arg(0), self.arg(1)
            self.write_wstr_at(d, self.wstr(s))
            return d
        if nm == '?appStrcat@@YAPAGPAGPBG@Z':
            d, s = self.arg(0), self.arg(1)
            self.write_wstr_at(d, self.wstr(d) + self.wstr(s))
            return d
        if nm == '?appStrlen@@YAHPBG@Z':
            return len(self.wstr(self.arg(0)))
        if nm == '??0UObject@@QAE@XZ':           # the engine's base constructor (our window constructors run inline now): returns this
            return t
        if nm == '?appFailAssert@@YAXPBD0H@Z':
            raise RuntimeError('appFailAssert')
        raise RuntimeError('import ' + nm)

    def do_fn(self, nm):
        t = self.this()
        a = self.arg
        if nm == 'GetTextExtent':
            size, text = a(0), self.wstr(a(1))
            p3 = a(2)
            cx = 6 * len(text)
            lines = 1
            if p3:
                lines = max(1, -(-cx // p3))
                cx = min(cx, p3)
            self.write32(size, cx)
            self.write32(size + 4, 13 * lines)
            self.log.append(('GetTextExtent', text, [a(i) for i in range(2, 8)]))
            return 0
        if nm == 'GetCharSize':
            self.write32(a(0), 6)
            self.write32(a(0) + 4, 13)
            self.log.append(('GetCharSize', a(1), a(2)))
            return None
        if nm == 'DrawText':
            text = self.wstr(a(3))
            self.log.append(('DrawText', [self.rds32(self.reg(UC_X86_REG_ESP) + 4 + 4 * i) for i in range(3)], text,
                             [a(i) for i in range(4, 15)]))
            return 6 * len(text)
        if nm == 'DrawTexture':
            self.log.append(('DrawTexture', [self.rds32(self.reg(UC_X86_REG_ESP) + 4 + 4 * i) for i in range(8)], a(8)))
            return None
        if nm == 'DrawStretchedTex':
            self.log.append(('DrawStretchedTex', [self.rds32(self.reg(UC_X86_REG_ESP) + 4 + 4 * i) for i in range(4)], a(4)))
            return None
        if nm == 'SetClip':
            self.log.append(('SetClip', [self.rds32(self.reg(UC_X86_REG_ESP) + 4 + 4 * i) for i in range(4)]))
            return None
        if nm == 'ReleaseClip':
            self.log.append(('ReleaseClip',))
            return None
        if nm == 'GetGradeText':
            g = self.rds32(self.reg(UC_X86_REG_ESP) + 4)
            return self.static_str(GRADES[g]) if g in GRADES else 0
        if nm == 'GetItemSlotText':
            ty, bp, res, sub = a(0), a(1), a(2), a(3)
            self.fs_set(res, 'slot%d_%d_%d' % (ty, bp, sub))
            self.log.append(('GetItemSlotText', ty, bp, sub))
            return None
        if nm == 'GetWeaponTypeName':
            n = self.rds32(self.reg(UC_X86_REG_ESP) + 4)
            return self.static_str('wt%d' % n) if n >= 0 and n != 9 else 0
        if nm == 'GetAttackSpeedText':
            return self.static_str('spd%d' % self.rds32(self.reg(UC_X86_REG_ESP) + 4))
        if nm == 'GetEnchantedAttack':
            g, e, b = a(0), a(1), a(2)
            self.log.append(('EA', g, e, b))
            return u32(b + 100 * g + e)
        if nm == 'GetEnchantedDefense':
            g, e, b = a(0), a(1), a(2)
            self.log.append(('ED', g, e, b))
            return u32(b + 100 * g + e + 1)
        if nm == 'IsMagicArmor':
            return 1 if a(0) % 2 == 0 else 0
        if nm == 'GetSkillTypeText':
            return self.static_str('stype') if a(0) % 3 != 0 else 0
        if nm == 'GetSkillMpCost':
            return u32((a(0) * a(1)) % 7)
        if nm == 'GetSkillRange':
            return u32((a(0) + a(1)) % 5 - 1)
        if nm == 'GetClassNameString':
            return self.static_str('class%d' % a(0))
        if nm == 'ConvertNewLines':
            text = self.wstr(a(0))
            self.fs_set(a(1), text.replace('\\n', '\r\n'))
            return None
        if nm == 'GetEquipSlot':
            return u32((a(0) * 7 + a(1)) % 16)
        raise RuntimeError('fn ' + nm)
