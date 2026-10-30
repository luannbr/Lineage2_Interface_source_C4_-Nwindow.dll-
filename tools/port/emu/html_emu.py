# Differential test of the HTML page tree and the board viewer of the C2 NWindow.dll (written by the w2html lane): the stock function and ours run on identical object graphs under unicorn (tools/port/emu); the graphs, the globals and the device call logs are compared. Needs build/bin/Release/NWindowDecomp.dll (+ .map) and `pip install unicorn`.
# python tools/port/emu/t_html_layout.py
"""Differential harness for the HTML page tree (NHtmlBlock / NHtmlTable / NHtmlItem): the stock function and
ours run on identical raw object graphs under unicorn (tools/port/emu), then the graphs and the device call
logs are compared.  Written by the w2html lane."""
import sys, os, struct, random, re

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import stubs
from stubs import *
import sa

# ---- extra services -------------------------------------------------------------------------------
# C4 stock addresses (the C2 script had 0x1000f430 ... : the C4 NCGDevice is at 0x10012960 .. 0x10014870, the viewer's services at 0x1003D3D0 ..)
STOCK_EXTRA = {
    0x100129C0: 'DrawColoredLine', 0x10013010: 'DrawLine', 0x10012B80: 'DrawRect', 0x10014870: 'PushOrigin',
    0x100142F0: 'PopOrigin', 0x10012D90: 'DrawTexture', 0x10012960: 'GetTextExtent', 0x100134B0: 'GetCharSize',
    0x10012AA0: 'DrawText', 0x10042DF0: 'WrapText', 0x1003B980: 'ParseColor',
    0x1003D3D0: 'AddFileLink', 0x1003D520: 'AddActionLink', 0x1003D660: 'AddButton', 0x1003D7D0: 'AddMultiEdit',
    0x1003D8E0: 'AddEdit', 0x1003F170: 'AddComboBox', 0x1003AEF0: 'FindPageTexture',
}
POP_EXTRA = {
    'DrawColoredLine': 0x34, 'DrawLine': 0x18, 'DrawRect': 0x14, 'PushOrigin': 8, 'PopOrigin': 0, 'WrapText': 8, 'ParseColor': 0, 'AddFileLink': 0x30, 'AddActionLink': 0x30,
    'AddButton': 0x20, 'AddMultiEdit': 0x14, 'AddEdit': 0x20, 'AddComboBox': 0x1C, 'FindPageTexture': 4,
}
OURS_EXTRA = {
    'DrawColoredLine': ('@NCGDevice@@', '?DrawColoredLine@'), 'DrawLine': ('@NCGDevice@@', '?DrawLine@'),
    'DrawRect': ('@NCGDevice@@', '?DrawRect@'), 'PushOrigin': ('@NCGDevice@@', '?PushOrigin@'),
    'PopOrigin': ('@NCGDevice@@', '?PopOrigin@'),
    'ParseColor': ('', '?HtmlParseColor@@'),
    'AddFileLink': ('@NCHtmlViewer@@', '?AddFileLink@'), 'AddActionLink': ('@NCHtmlViewer@@', '?AddActionLink@'),
    'AddButton': ('@NCHtmlViewer@@', '?AddButton@'), 'AddMultiEdit': ('@NCHtmlViewer@@', '?AddMultiEdit@'),
    'AddEdit': ('@NCHtmlViewer@@', '?AddEdit@'), 'AddComboBox': ('@NCHtmlViewer@@', '?AddComboBox@'),
    'FindPageTexture': ('@NCHtmlViewer@@', '?FindPageTexture@'),
}
stubs.FN_POP.update(POP_EXTRA)
stubs.IMPORT_POP['?appAtoi@@YAHPBG@Z'] = 0
stubs.FN_POP['wcsicmp'] = 0
stubs.FN_POP['wtol'] = 0
stubs.FN_POP['Malloc'] = 8
stubs.FN_POP['DrawTexture'] = 0x2C        # C4: 11 arguments (the C2 one took 8 + 1)
TipEmu.CONSOLE_VA = 0x102C39F4            # GConsole of the C4 image
stubs.FN_POP['Realloc'] = 12
stubs.FN_POP['Free'] = 4
stubs.STOCK_FN.clear()
stubs.STOCK_FN.update(STOCK_EXTRA)
for k in list(stubs.OURS_FN):
    if k not in ('GetTextExtent', 'GetCharSize', 'DrawText', 'DrawTexture'):
        del stubs.OURS_FN[k]
stubs.OURS_FN.update(OURS_EXTRA)

GARBAGE = 0xCD


class HEmu(TipEmu):
    def __init__(self, kind):
        # TipEmu patches the function stubs itself (STOCK_FN / OURS_FN)
        super().__init__(kind)
        self.kind = kind
        self.nwbase = 0 if kind == 'stock' else NWBASE
        # GMalloc vtable
        for i, nm in enumerate(('Malloc', 'Realloc', 'Free')):
            stub = STUB_BASE + 0x9100 + 16 * i
            self.uc.mem_write(stub, b'\xC3')
            self.stubs[stub] = ('fn:' + nm, None)
            self.write32(self.gmalloc_vt + 4 * i, stub)
        self.thai_var = self.alloc(4)
        self.write32(self.thai_var, 0)
        for e in self.pe.DIRECTORY_ENTRY_IMPORT:
            for imp in e.imports:
                nm = imp.name.decode() if imp.name else ''
                if 'GL2ThaiHtmlMode' in nm:
                    self.write32(imp.address, self.thai_var)
                    self.stubs.pop(self.read32(imp.address), None)
        # the word wrapper: a stub for L2WordWrapperWnd::WrapText (the stock's is patched by TipEmu from STOCK_FN; ours: the compiled function)
        wrap = STUB_BASE + 0x9200
        self.uc.mem_write(wrap, bytes([0xC3]))
        self.stubs[wrap] = ('fn:WrapText', None)
        if kind == 'ours':
            self.uc.mem_write([v for k, v in self.symbols.items() if k.startswith('?WrapText@L2WordWrapperWnd@@')][0],
                              bytes([0x68]) + struct.pack('<I', wrap) + bytes([0xC3]))
        # CRT functions that need the per-thread data of the CRT: stubbed in both images
        for i, (nm, sva, osym) in enumerate((('wcsicmp', 0x10171E4E, '__wcsicmp'), ('wtol', 0x10171439, None))):
            stub = STUB_BASE + 0x9300 + 16 * i
            self.uc.mem_write(stub, b'\xC3')
            self.stubs[stub] = ('fn:' + nm, None)
            if kind == 'stock':
                self.uc.mem_write(sva, b'\x68' + struct.pack('<I', stub) + b'\xC3')
            elif osym:
                self.uc.mem_write(self.symbols[osym], b'\x68' + struct.pack('<I', stub) + b'\xC3')
        if kind == 'ours' and not sa.ENABLED:
            # the stock tables the hook build reads from the stock image (nw::Addr): the alignment names (the standalone has its own)
            for i, nm in enumerate(('CENTER', 'RIGHT', 'LEFT', 'TOP', 'BOTTOM')):
                self.write32(NWBASE + 0x235328 + 4 * i, self.put_wstr(nm))
        self.wrapper = self.alloc(0x40)
        self.write32(self.console + 0x3C1C, self.wrapper)        # NConsoleWnd::ThaiWordWrapperWnd (C2: +0x3564)
        self.garbage_fill = True

    # addresses of the stock globals
    def G(self, va):
        return va if self.kind == 'stock' else NWBASE + (va - 0x10000000)

    def do_fn(self, nm):
        t = self.this()
        a = self.arg
        if nm == 'Malloc':
            n = a(0)
            p = self.alloc(n)
            self.uc.mem_write(p, bytes([GARBAGE]) * n)
            self.log.append(('Malloc', n))
            return p
        if nm == 'Free':
            self.log.append(('Free',))
            return None
        if nm == 'Realloc':
            raise RuntimeError('GMalloc::Realloc')
        if nm == 'GetTextExtent':
            size, text = a(0), self.wstr(a(1))
            p3, p4, p5, p7, p8, font = a(2), a(3), a(4), a(5), a(6), a(7)
            cw = 6 + font
            total = cw * len(text) + p8
            if p3 > 0 and p3 < 0x7FFFFFFF:
                lines = max(1, -(-total // p3))
                cx = min(cw * len(text), p3)
                ret = total % p3
            else:
                lines = 1
                cx = cw * len(text)
                ret = total
            self.write32(size, cx)
            self.write32(size + 4, (13 + font) * lines)
            self.log.append(('GetTextExtent', text, p3, p4, p5, p7, p8, font))
            return ret
        if nm == 'GetCharSize':
            font = a(2)
            self.write32(a(0), 6 + font)
            self.write32(a(0) + 4, 13 + font)
            self.log.append(('GetCharSize', a(1), font))
            return None
        if nm == 'DrawText':
            text = self.wstr(a(3))
            self.log.append(('DrawText', [self.rds32(self.reg(UC_X86_REG_ESP) + 4 + 4 * i) for i in range(3)], text,
                             [self.rds32(self.reg(UC_X86_REG_ESP) + 4 + 4 * i) for i in range(4, 15)]))
            return 6 * len(text)
        if nm == 'DrawColoredLine':
            text = self.wstr(a(3))
            ci = a(11)
            ranges = []
            if ci:
                data, num = self.read32(ci), self.read32(ci + 4)
                for i in range(num):
                    p = self.read32(data + 4 * i)
                    ranges.append(tuple(self.rds32(p + 4 * j) for j in range(3)))
            self.log.append(('DrawColoredLine', [self.rds32(self.reg(UC_X86_REG_ESP) + 4 + 4 * i) for i in range(3)], text,
                             [self.rds32(self.reg(UC_X86_REG_ESP) + 4 + 4 * i) for i in range(4, 11)], ranges,
                             self.rds32(self.reg(UC_X86_REG_ESP) + 4 + 4 * 12)))
            return 6 * len(text)
        if nm == 'DrawTexture':
            self.log.append(('DrawTexture', [self.rds32(self.reg(UC_X86_REG_ESP) + 4 + 4 * i) for i in range(11)]))
            return None
        if nm in ('DrawLine', 'DrawRect', 'PushOrigin'):
            n = {'DrawLine': 6, 'DrawRect': 5, 'PushOrigin': 2}[nm]
            self.log.append((nm, [self.rds32(self.reg(UC_X86_REG_ESP) + 4 + 4 * i) for i in range(n)]))
            return None
        if nm == 'PopOrigin':
            self.log.append(('PopOrigin',))
            return None
        if nm == 'ParseColor':
            sp = self.read32(self.reg(UC_X86_REG_ESP) + 4)
            st = self.wstr(sp) if sp else None
            if st is None:
                return 0
            if st == 'LEVEL':
                return 0xFFFFCC00
            try:
                return 0xFF000000 | int(st, 16) & 0xFFFFFF
            except Exception:
                return 0xFF000000
        if nm in ('AddFileLink', 'AddActionLink', 'AddButton', 'AddMultiEdit', 'AddEdit', 'AddComboBox', 'FindPageTexture'):
            n = {'AddFileLink': 11, 'AddActionLink': 12, 'AddButton': 8, 'AddMultiEdit': 5, 'AddEdit': 8,
                 'AddComboBox': 7, 'FindPageTexture': 1}[nm]
            args = []
            for i in range(n):
                v = self.rds32(self.reg(UC_X86_REG_ESP) + 4 + 4 * i)
                if nm in ('AddFileLink', 'AddActionLink') and i >= 10 or nm == 'AddButton' and i >= 4 or                         nm == 'AddMultiEdit' and i == 4 or nm == 'AddEdit' and i in (4, 6) or nm == 'AddComboBox' and i >= 4 or                         nm == 'FindPageTexture':
                    v = self.wstr(v & 0xFFFFFFFF) if v else None
                args.append(v)
            self.log.append((nm, args))
            return 0x7777 if nm == 'FindPageTexture' else 1
        if nm == 'wcsicmp':
            x, y = self.wstr(a(0)).lower(), self.wstr(a(1)).lower()
            return 0 if x == y else (1 if x > y else 0xFFFFFFFF)
        if nm == 'wtol':
            st = self.wstr(a(0))
            m = re.match(r'\s*([+-]?)(\d*)', st)
            v = int(m.group(2) or '0')
            if m.group(1) == '-':
                v = -v
            return u32(v)
        if nm == 'WrapText':
            self.log.append(('WrapText', self.wstr(a(0)), a(1)))
            return None
        return super().do_fn(nm)

    def do_import(self, nm):
        if nm == '?Remove@FArray@@QAEXHHH@Z':
            # keep the stub of the base class (it moves the tail down)
            return super().do_import(nm)
        if nm == '?appAtoi@@YAHPBG@Z':
            s = self.wstr(self.arg(0))
            m = re.match(r'\s*([+-]?\d+)', s)
            return u32(int(m.group(1))) if m else 0
        if nm == '?appFailAssert@@YAXPBD0H@Z':
            raise RuntimeError('appFailAssert')
        return super().do_import(nm)

    # ---- raw object helpers -------------------------------------------------------------------------
    def newobj(self, size):
        p = self.alloc(size)
        self.uc.mem_write(p, bytes(size))
        return p

    def put_wstring(self, addr, s):
        """a VC7.1 std::wstring at addr (0x1C bytes)"""
        self.uc.mem_write(addr, bytes(0x1C))
        n = len(s)
        if n < 8:
            self.uc.mem_write(addr + 4, s.encode('utf-16-le') + b'\0\0')
            self.write32(addr + 0x14, n)
            self.write32(addr + 0x18, 7)
        else:
            cap = n + 4
            buf = self.alloc(2 * cap + 2)
            self.uc.mem_write(buf, s.encode('utf-16-le') + b'\0\0')
            self.write32(addr + 4, buf)
            self.write32(addr + 0x14, n)
            self.write32(addr + 0x18, cap)

    def get_wstring(self, addr):
        n = self.read32(addr + 0x14)
        res = self.read32(addr + 0x18)
        p = addr + 4 if res < 8 else self.read32(addr + 4)
        return ''.join(chr(struct.unpack('<H', bytes(self.uc.mem_read(p + 2 * i, 2)))[0]) for i in range(n))

    def tarray(self, ptrs, size=4):
        d = self.alloc(max(1, len(ptrs)) * size + 16)
        for i, v in enumerate(ptrs):
            self.write32(d + 4 * i, v)
        return d

    def write_tarray(self, addr, vals):
        d = self.tarray(vals) if vals else 0
        self.write32(addr, d)
        self.write32(addr + 4, len(vals))
        self.write32(addr + 8, len(vals) if vals else 0)

    def read_tarray(self, addr):
        d, n = self.read32(addr), self.rds32(addr + 4)
        return [self.read32(d + 4 * i) for i in range(n)] if n > 0 else []

    def read_tarray_i(self, addr):
        d, n = self.read32(addr), self.rds32(addr + 4)
        return [self.rds32(d + 4 * i) for i in range(n)] if n > 0 else []

    # vtables (the classes in this image)
    def vt(self, cls):
        if self.kind == 'stock':
            return {'block': 0x101A88B4, 'table': 0x101A88CC, 'item': 0x101A889C}[cls]
        sym = {'block': '??_7NHtmlBlock@@6B@', 'table': '??_7NHtmlTable@@6B@', 'item': '??_7NHtmlItem@@6B@'}[cls]
        return self.symbols[sym]

    def fn(self, name):
        """address of the function under test in this image"""
        if self.kind == 'stock':
            return STOCK[name]
        return self.symbols[OURS[name]]


STOCK = {   # C4 (the C2 addresses were 0x1002F430 .. 0x10035BA0)
    'block_layout': 0x1003BAB0, 'block_arrange': 0x1003BCD0, 'block_draw': 0x1003BD20, 'table_layout': 0x1003CE00,
    'table_arrange': 0x1003BE40, 'table_draw': 0x1003BF10, 'item_layout': 0x1003BA10, 'item_draw': 0x10041210,
    'item_arrange': 0x10040260, 'block_parse': 0x10041450, 'table_parse': 0x1003E110, 'item_parse': 0x10040960,
    'celattrs': 0x1003DDE0, 'settext': 0x1003CC90, 'drawcolored': 0x10040E10, 'getabspos': 0x1003B930,
    'alignx': 0x1003AC50, 'block_release': 0x1003CDA0, 'table_release': 0x1003E480, 'item_release': 0x10040670,
    'content_height': 0x1003C450, 'block_ctor3': 0x1003DCF0, 'table_ctor': 0x1003DFB0, 'item_ctor': 0x10040840,
    'getpercent': 0x1003AD20,
}
OURS = {
    'block_layout': '?Layout@NHtmlBlock@@UAEXHH@Z', 'block_arrange': '?Arrange@NHtmlBlock@@UAEXHH@Z',
    'block_draw': '?Draw@NHtmlBlock@@UAEXPAVNCGDevice@@@Z', 'table_layout': '?Layout@NHtmlTable@@UAEXHH@Z',
    'table_arrange': '?Arrange@NHtmlTable@@UAEXHH@Z', 'table_draw': '?Draw@NHtmlTable@@UAEXPAVNCGDevice@@@Z',
    'item_layout': '?Layout@NHtmlItem@@UAEXHH@Z', 'item_draw': '?Draw@NHtmlItem@@UAEXPAVNCGDevice@@@Z',
    'item_arrange': '?Arrange@NHtmlItem@@UAEXHH@Z', 'block_parse': '?Parse@NHtmlBlock@@QAEXPAPAPAUNHtmlTag@@H@Z',
    'table_parse': '?Parse@NHtmlTable@@QAEXPAPAPAUNHtmlTag@@@Z', 'item_parse': '?Parse@NHtmlItem@@QAEXPAPAPAUNHtmlTag@@@Z',
    'celattrs': '?ParseCellAttrs@NHtmlBlock@@QAEXPAUNHtmlTag@@@Z', 'settext': '?SetText@NHtmlItem@@QAEXPAUNHtmlTag@@@Z',
    'drawcolored': '?DrawColoredText@NHtmlItem@@QAEXPAVNCGDevice@@H@Z', 'getabspos': '?GetAbsPos@NHtmlObject@@QAEPAUtagPOINT@@PAU2@@Z',
    'block_release': '?Release@NHtmlBlock@@UAEXXZ', 'table_release': '?Release@NHtmlTable@@UAEXXZ',
    'item_release': '?Release@NHtmlItem@@UAEXXZ', 'content_height': '?GetContentHeight@NHtmlBlock@@QAEHXZ',
    'getpercent': '?GetPercentWidth@NHtmlBlock@@UAEHH@Z',
}


# ---- canonical dumps ------------------------------------------------------------------------------------
def dump_obj(e, p, seen=None, depth=0):
    """a structure of plain values for the object at p (kind from its vtable), children recursively"""
    vt = e.read32(p)
    kinds = {e.vt('block'): 'block', e.vt('table'): 'table', e.vt('item'): 'item'}
    kind = kinds.get(vt, 'unk%x' % vt)
    base = [e.rds32(p + o) for o in range(4, 0x4C, 4) if o not in (0x18, 0x1C)]
    out = {'kind': kind, 'base': base}
    if kind in ('block', 'table'):
        out['blk'] = [e.rds32(p + o) for o in (0x4C, 0x50, 0x54, 0x58, 0x60, 0x68, 0x6C)]
        out['buf'] = e.wstr(p + 0x70, 64)
        ch = e.read_tarray(p + 0x5C)
        out['children'] = [dump_obj(e, c, seen, depth + 1) for c in ch]
    if kind == 'table':
        out['tbl'] = [e.rds32(p + o) for o in (0x870, 0x874, 0x8B4, 0x8B8)]
        out['arrays'] = [e.read_tarray_i(p + o) for o in (0x884, 0x890, 0x89C, 0x8A8)]
        rows = []
        for r in e.read_tarray(p + 0x878):
            rows.append([dump_obj(e, c, seen, depth + 1) for c in e.read_tarray(r)])
        out['rows'] = rows
        out['arr_hdr'] = [e.rds32(p + o) for o in (0x87C, 0x880, 0x888, 0x88C, 0x894, 0x898, 0x8A0, 0x8A4, 0x8AC, 0x8B0)]
    if kind == 'item':
        out['itm'] = [e.rds32(p + o) for o in (0x4C, 0x50, 0xE0, 0xEC, 0xF4, 0xF8)]
        out['strs'] = [e.get_wstring(p + o) for o in (0x54, 0x70, 0x8C, 0xA8, 0xC4)]
        for nm, o in (('colors', 0xE4), ('text', 0xF0)):
            q = e.read32(p + o)
            out[nm] = e.wstr(q, 4000) if q not in (0, 0xCDCDCDCD) else q
        out['tex'] = e.rds32(p + 0xE8)
    return out
