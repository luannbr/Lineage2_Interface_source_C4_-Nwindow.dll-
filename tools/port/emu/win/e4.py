# Differential test of the C4 NCEditBox (stock vs ours under unicorn), adapted from tools/port/emu/t_edit.py
#   python e4.py [cases] [seed] [ops,comma,separated]
import sys, os, random, struct
EMU = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))   # tools/port/emu
sys.path.insert(0, EMU)
import stubs
from stubs import *
import emu as _emu, shutil, tempfile
# COPYDLL: the emulator maps a private copy of our DLL so that the build can overwrite the original while it runs
_tmp = os.path.join(tempfile.gettempdir(), 'e4_ours_%d' % os.getpid())
os.makedirs(_tmp, exist_ok=True)
shutil.copy(_emu.OURS, os.path.join(_tmp, 'NWindowDecomp.dll'))
shutil.copy(_emu.OURS_MAP, os.path.join(_tmp, 'NWindowDecomp.map'))
_emu.OURS = os.path.join(_tmp, 'NWindowDecomp.dll')
_emu.OURS_MAP = os.path.join(_tmp, 'NWindowDecomp.map')

# ---- the services (stock VA -> name; ours by decorated name) ----------------------------------------------------
STOCK_X = {
    0x10064b10: 'GetIMEType', 0x100178a0: 'IsNotComposing', 0x10006b30: 'NotifyParent', 0x10034ad0: 'WndOnKeyDown',
    0x10034b60: 'WndOnKeyUp', 0x10036240: 'WndOnMouseMove', 0x10036410: 'WndOnLButtonUp', 0x100347a0: 'WndKillFocus', 0x10017860: 'GetReadingStr',
    0x10012960: 'GetTextExtent', 0x100134b0: 'GetCharSize', 0x10012aa0: 'DrawText', 0x10012bf0: 'DrawStretchedS',
    0x10012cb0: 'DrawStretchedL', 0x10012d90: 'DrawTexture', 0x10013330: 'SetClip', 0x10013340: 'ReleaseClip',
    0x10013450: 'MakeThai', 0x1000d5a0: 'ThaiWidth', 0x10148f10: 'CtlWindowProc', 0x10025050: 'OnGetCaretStub',
}
OURS_X = {
    'GetIMEType': '?GetIMEType@NConsoleWnd@@QAEHXZ', 'IsNotComposing': '?IsNotComposing@NCIMEManager@@QAEHXZ',
    'NotifyParent': '?NotifyParent@NControl@@QAEHJ@Z', 'WndOnKeyDown': '?OnKeyDown@NCWnd@@UAEHIJ@Z',
    'WndOnKeyUp': '?OnKeyUp@NCWnd@@UAEHIJ@Z', 'WndOnMouseMove': '?OnMouseMove@NCWnd@@UAEHIJ@Z',
    'WndOnLButtonUp': '?OnLButtonUp@NCWnd@@UAEHIJ@Z', 'WndKillFocus': '?KillFocus@NCWnd@@UAEXXZ', 'GetReadingStr': '?GetReadingStr@NCIMEManager@@QAEPAGXZ',
    'GetTextExtent': '?GetTextExtent@NCGDevice@@SAKPAUtagSIZE@@PBGKKHHKW4L2FontType@@@Z',
    'GetCharSize': '?GetCharSize@NCGDevice@@SAXPAUtagSIZE@@GH@Z',
    'DrawText': '?DrawText@NCGDevice@@QAEKHHKPBGKKKW4L2FontType@@HHKHHHH@Z',
    'DrawStretchedS': '?DrawStretchedTex@NCGDevice@@QAEXHHHHPAVUTexture@@HK@Z',
    'DrawStretchedL': '?DrawStretchedTex@NCGDevice@@QAEXHHHHHHHHPAVUTexture@@K@Z',
    'DrawTexture': '?DrawTexture@NCGDevice@@QAEXHHHHHHHHPAVUTexture@@KH@Z', 'SetClip': '?SetClip@NCGDevice@@QAEXHHHH@Z',
    'ReleaseClip': '?ReleaseClip@NCGDevice@@QAEXXZ',
    'MakeThai': '?MakeThaiCharArray@NCGDevice@@SAXPBGAAV?$TArray@UThaiCharacter@@@@PAG@Z',
    'ThaiWidth': '?GetWidth@ThaiCharacter@@QBEHPAVUCanvas@@H@Z', 'CtlWindowProc': '?WindowProc@NControl@@UAEHIIJ@Z', 'OnGetCaretStub': '?OnGetCaret@NCEditBox@@UAEHIJ@Z',
}
stubs.FN_POP.update({'GetIMEType': 0, 'IsNotComposing': 0, 'NotifyParent': 4, 'WndOnKeyDown': 8, 'WndOnKeyUp': 8,
                     'WndOnMouseMove': 8, 'WndOnLButtonUp': 8, 'WndKillFocus': 0, 'MgrDeactivate': 0, 'GetReadingStr': 0, 'GetTextExtent': 0, 'GetCharSize': 0,
                     'DrawText': 0x3C, 'DrawStretchedS': 0x1C, 'DrawStretchedL': 0x28, 'DrawTexture': 0x2C,
                     'SetClip': 0x10, 'ReleaseClip': 0, 'MakeThai': 0, 'ThaiWidth': 8, 'CtlWindowProc': 12, 'OnGetCaretStub': 8})
stubs.IMPORT_POP.update({
    '?Mid@FString@@QBE?AV1@HH@Z': 12, '?Left@FString@@QBE?AV1@H@Z': 8, '?Right@FString@@QBE?AV1@H@Z': 8,
    '?Delete@FString@@QAEXHH@Z': 8, '??HFString@@QAE?AV0@ABV0@@Z': 8, '??HFString@@QAE?AV0@PBG@Z': 8,
    '??8FString@@QBEHPBG@Z': 4, '__stdio_common_vswprintf': 0, '?appMemset@@YAXPAXHH@Z': 0, '?appMemmove@@YAPAXPAXPBXH@Z': 0,
    '?Insert@FArray@@QAEXHHH@Z': 12, '?AddZeroed@FArray@@QAEHHH@Z': 8})

NCEDIT = {'paint': 0x1000ef20, 'keydown': 0x1000fec0, 'char': 0x1000f9c0, 'insert': 0x1000e540, 'delete': 0x1000e790,
          'reformat': 0x1000c3b0, 'keyup': 0x1000c210, 'mousemove': 0x1000d260, 'lbdown': 0x1000ea70,
          'lbup': 0x1000d4e0, 'dblclick': 0x100112a0, 'delsel': 0x1000ebf0, 'selall': 0x1000d500,
          'killfocus': 0x10011260, 'join': 0x1000fd40, 'delselmsg': 0x1000ecf0, 'winproc': 0x1000b4c0, 'reading': 0x1000cfe0,
          'textwidth': 0x1000c300}
OURS_EDIT = {'paint': '?OnPaint@NCEditBox@@UAEHPAVNCGDevice@@@Z', 'keydown': '?OnKeyDown@NCEditBox@@UAEHIJ@Z',
             'char': '?OnChar@NCEditBox@@UAEHIJ@Z', 'insert': '?InsertText@NCEditBox@@QAEXPBGH@Z',
             'delete': '?DeleteChar@NCEditBox@@QAEXH@Z', 'reformat': '?ReformatNumber@NCEditBox@@QAEXXZ',
             'keyup': '?HandleKeyUp@NCEditBox@@QAEHIJ@Z', 'mousemove': '?HandleMouseMove@NCEditBox@@QAEHIJ@Z',
             'lbdown': '?HandleLButtonDown@NCEditBox@@QAEHIJ@Z', 'lbup': '?HandleLButtonUp@NCEditBox@@QAEHIJ@Z',
             'dblclick': '?OnDoubleClick@NCEditBox@@QAEHIJ@Z', 'delsel': '?DeleteSelection@NCEditBox@@QAEHXZ',
             'selall': '?SelectAll@NCEditBox@@QAEXXZ', 'killfocus': '?KillFocus@NCEditBox@@UAEXXZ',
             'join': '?JoinPieces@NCEditBox@@QAEXXZ', 'delselmsg': '?OnDeleteSelectionMsg@NCEditBox@@QAEHIJ@Z',
             'winproc': '?WindowProc@NCEditBox@@UAEHIIJ@Z', 'reading': '?OnReading@NCEditBox@@UAEHIJ@Z', 'textwidth': '?GetTextWidth@NCEditBox@@QAEHVFString@@@Z'}

GCONSOLE = 0x102c39f4
GSHIFT = 0x102C6BB8
STOCK_VTABLE = 0x101a16a0

_stub_wfmt = wfmt


def wfmt(emu, fmt, args):
    if '%c' in fmt and fmt.replace('%c', '', 1).find('%') < 0:
        return fmt.replace('%c', chr(args[0] & 0xFFFF), 1)
    return _stub_wfmt(emu, fmt, args)


THAI_MARKS = set([0xE31] + list(range(0xE34, 0xE3B)) + list(range(0xE47, 0xE4F)))
F = dict(bFocus=0x40, ScreenX=0x44, ScreenY=0x48, Width=0x4C, Height=0x50, Style=0x68, bEnabled=0x78,
         IMEManager=0x1F4, Edit1=0x1F8, Type=0x214, U1C8=0x218, U1CC=0x21C, bCanInsert=0x220, bCaretOn=0x224,
         U1D8=0x228, CaretTimer=0x22C, BackTex=0x230, WhiteTex=0x234, bIMEPosFixed=0x238, bReading=0x23C,
         U1F0=0x240, ViewStart=0x244, ExtraWidth=0x248, MaxWidth=0x24C, ViewStart2=0x250, MaxLen=0x254,
         Left=0x258, Right=0x264, Commit=0x270, ThaiLeft=0x27C, ThaiRight=0x288, Comp=0x294, Reading=0x2A0,
         bSelecting=0x2AC, SelLength=0x2B0, SelWidth=0x2B4, bSelectAll=0x2B8, CaretX=0x2BC, U2C0=0x2C0,
         IMECaretX=0x2C4, IMECaretY=0x2C8, CursorPos=0x2CC, ReadingParam=0x2D0)
INTS = ['bFocus', 'Type', 'U1C8', 'U1CC', 'bCanInsert', 'bCaretOn', 'U1D8', 'bReading', 'U1F0', 'ViewStart',
        'ExtraWidth', 'MaxWidth', 'ViewStart2', 'MaxLen', 'bSelecting', 'SelLength', 'SelWidth', 'bSelectAll',
        'CaretX', 'IMECaretX', 'IMECaretY', 'CursorPos', 'ReadingParam']


def thai_split(text):
    out = []
    for c in text:
        if out and ord(c) in THAI_MARKS and len(out[-1]) < 6:
            out[-1] += c
        else:
            out.append(c)
    return out


def cluster_words(cl):
    w = [0] * 10
    if len(cl) == 1:
        w[5] = ord(cl[0])
    else:
        w[1] = ord(cl[0])
        for k, ch in enumerate(cl[1:]):
            w[[2, 3, 0, 4, 6][k]] = ord(ch)
    return w


def char_w(c):
    o = ord(c)
    if o in THAI_MARKS:
        return 0
    if o >= 0xE00:
        return 7
    return 12 if (o >= 0x80 or c in 'MW@') else 6


class E4(TipEmu):
    def __init__(self, kind):
        Emu.__init__(self, kind)
        self.last_pop = 0
        self.static_cache = {}
        self.heap_mark = None
        self.gdata = self.alloc(0x100000)
        self.empty_wstr = self.put_wstr('')
        self.itoa_buf = self.alloc(64)
        self.gmalloc_obj = self.alloc(16)
        self.gmalloc_vt = self.alloc(64)
        self.write32(self.gmalloc_obj, self.gmalloc_vt)
        self.gmalloc_var = self.alloc(4)
        self.write32(self.gmalloc_var, self.gmalloc_obj)
        self.ime_type = 1
        self.not_composing = 1
        self.reading_ptr = 0
        self.basewp = 0
        self.langvar = self.alloc(16)
        self.console = self.alloc(0x4000)
        for e in self.pe.DIRECTORY_ENTRY_IMPORT:
            for imp in e.imports:
                nm = imp.name.decode() if imp.name else ''
                if nm == '?GLanguageType@@3HA':
                    self.write32(imp.address, self.langvar)
                elif 'GL2GameData' in nm:
                    self.write32(imp.address, self.gdata)
                elif nm.startswith('?GMalloc'):
                    self.write32(imp.address, self.gmalloc_var)
        base = 0 if kind == 'stock' else NWBASE - 0x10000000
        self.glob = lambda va: va + base
        self.write32(self.glob(GCONSOLE), self.console)
        fns = {}
        if kind == 'stock':
            fns = dict(STOCK_X)
        else:
            for nm, sym in OURS_X.items():
                c = [v for k, v in self.symbols.items() if k == sym]
                if len(c) != 1:
                    raise RuntimeError('ours symbol %s: %s' % (sym, c))
                fns[c[0]] = nm
        # a fake IME manager vtable: slot 76 (Deactivate) is a stub
        self.fake_vt = self.alloc(0x200)
        self.uc.mem_write(self.fake_vt, bytes(0x200))
        mstub = STUB_BASE + 0x9F00
        self.uc.mem_write(mstub, bytes([0xC3]))
        self.stubs[mstub] = ('fn:MgrDeactivate', None)
        self.write32(self.fake_vt + 4 * 76, mstub)
        n = 0
        for va, nm in fns.items():
            stub = STUB_BASE + 0x9000 + 16 * n
            n += 1
            self.uc.mem_write(stub, b'\xC3')
            self.stubs[stub] = ('fn:' + nm, None)
            self.uc.mem_write(va, b'\x68' + struct.pack('<I', stub) + b'\xC3')

    def set_lang(self, v):
        self.write32(self.langvar, v)

    def set_shift(self, v):
        if os.environ.get('E4_MUT') and self.kind == 'ours':
            v = 1 - v
        self.write32(self.glob(GSHIFT), v)

    def sargs(self, n):
        return [self.rds32(self.reg(UC_X86_REG_ESP) + 4 + 4 * i) for i in range(n)]

    def do_import(self, nm):
        t = self.this()
        a = self.arg
        if nm in ('?Mid@FString@@QBE?AV1@HH@Z', '?Left@FString@@QBE?AV1@H@Z', '?Right@FString@@QBE?AV1@H@Z'):
            s = self.fs_get(t)
            ret = a(0)
            if nm.startswith('?Mid'):
                st, cnt = self.rds32(self.reg(UC_X86_REG_ESP) + 8), self.rds32(self.reg(UC_X86_REG_ESP) + 12)
                st = max(0, min(st, len(s)))
                r = s[st:st + max(0, cnt)] if cnt < 0x7FFFFFF0 else s[st:]
            elif nm.startswith('?Left'):
                c = max(0, min(self.rds32(self.reg(UC_X86_REG_ESP) + 8), len(s)))
                r = s[:c]
            else:
                c = max(0, min(self.rds32(self.reg(UC_X86_REG_ESP) + 8), len(s)))
                r = s[len(s) - c:]
            self.fs_set(ret, r)
            self.log.append((nm.split('@')[0], s, [self.rds32(self.reg(UC_X86_REG_ESP) + 8 + 4 * i) for i in range(2 if nm.startswith('?Mid') else 1)]))
            return ret
        if nm == '?Delete@FString@@QAEXHH@Z':
            s = self.fs_get(t)
            i, c = self.rds32(self.reg(UC_X86_REG_ESP) + 4), self.rds32(self.reg(UC_X86_REG_ESP) + 8)
            self.log.append(('Delete', s, i, c))
            if 0 <= i <= len(s) and c >= 0:
                self.fs_set(t, s[:i] + s[i + c:])
            return None
        if nm in ('??HFString@@QAE?AV0@ABV0@@Z', '??HFString@@QAE?AV0@PBG@Z'):
            ret = a(0)
            o = self.fs_get(a(1)) if 'ABV0' in nm else (self.wstr(a(1)) if a(1) else '')
            self.fs_set(ret, self.fs_get(t) + o)
            return ret
        if nm == '??8FString@@QBEHPBG@Z':
            return 1 if self.fs_get(t).lower() == self.wstr(a(0)).lower() else 0
        if nm == '__stdio_common_vswprintf':
            buf, fmt, argp = a(2), self.wstr(a(4)), a(6)
            args = [self.read32(argp + 4 * i) for i in range(4)]
            s = wfmt(self, fmt, args)
            self.write_wstr_at(buf, s)
            return len(s)
        return super().do_import(nm)

    def do_fn(self, nm):
        t = self.this()
        a = self.arg
        if nm == 'GetTextExtent':
            text = self.wstr(a(1))
            self.write32(a(0), sum(char_w(c) for c in text))
            self.write32(a(0) + 4, 13)
            self.log.append(('GetTextExtent', text))
            return 0
        if nm == 'GetCharSize':
            c = a(1) & 0xFFFF
            self.write32(a(0), char_w(chr(c)))
            self.write32(a(0) + 4, 13)
            self.log.append(('GetCharSize', c, a(2)))
            return None
        if nm == 'CtlWindowProc':
            self.log.append(('CtlWindowProc', hex(a(0)), a(1), a(2)))
            return self.basewp
        if nm == 'OnGetCaretStub':
            self.log.append(('OnGetCaret', a(0), a(1)))
            return 1
        if nm == 'GetReadingStr':
            self.log.append(('GetReadingStr',))
            return self.reading_ptr
        if nm == 'WndKillFocus':
            self.log.append(('WndKillFocus',))
            return None
        if nm == 'MgrDeactivate':
            self.log.append(('MgrDeactivate',))
            return None
        if nm == 'GetIMEType':
            if os.environ.get('E4_MUT') and self.kind == 'ours' and self.ime_type == 9:
                return 10
            return self.ime_type
        if nm == 'IsNotComposing':
            return self.not_composing
        if nm == 'NotifyParent':
            self.log.append(('NotifyParent', a(0)))
            return 1
        if nm in ('WndOnKeyDown', 'WndOnKeyUp', 'WndOnMouseMove', 'WndOnLButtonUp'):
            self.log.append((nm, a(0), a(1)))
            return 1
        if nm == 'MakeThai':
            text = self.wstr(a(0))
            arr = a(1)
            cl = thai_split(text)
            data = self.alloc(max(1, len(cl)) * 20)
            for i, c in enumerate(cl):
                self.uc.mem_write(data + 20 * i, struct.pack('<10H', *cluster_words(c)))
            self.write32(arr, data); self.write32(arr + 4, len(cl)); self.write32(arr + 8, len(cl))
            self.log.append(('MakeThai', text, len(cl)))
            return None
        if nm == 'ThaiWidth':
            w = struct.unpack('<10H', bytes(self.uc.mem_read(t, 20)))
            r = sum(7 for x in w if x)
            self.log.append(('ThaiWidth', r, a(1)))
            return r
        if nm == 'DrawText':
            text = self.wstr(a(3))
            self.log.append(('DrawText', self.sargs(3), text, self.sargs(15)[4:]))
            return char_w('a') * len(text)
        if nm in ('DrawStretchedS', 'DrawStretchedL', 'DrawTexture'):
            n = {'DrawStretchedS': 7, 'DrawStretchedL': 10, 'DrawTexture': 11}[nm]
            self.log.append((nm, self.sargs(n)))
            return None
        if nm == 'SetClip':
            self.log.append(('SetClip', self.sargs(4)))
            return None
        if nm == 'ReleaseClip':
            self.log.append(('ReleaseClip',))
            return None
        return super().do_fn(nm)


LOGFILE = ROOT + '/build/smoke_system/NWindowDecomp.log'


def log_map():
    import re
    out = {}
    if _emu.sa.ENABLED:
        return _emu.sa.LogMap(_emu.sa.func_map())          # stock va -> the standalone address of the same function (a missing one raises sa.NoStandaloneFunction)
    for ln in open(LOGFILE, encoding='latin-1'):
        m = re.match(r'^\S+\s+([0-9a-f]{8}) (.*?)\s+ours=([0-9a-f]+) ok', ln)
        if m:
            out[int(m.group(1), 16)] = 0x10000000 + int(m.group(3), 16)
    return out


def addrs(e):
    if e.kind == 'stock':
        return NCEDIT
    lm = log_map()
    return {k: lm[v] for k, v in NCEDIT.items()}


def vtable(e):
    if e.kind == 'stock':
        return STOCK_VTABLE
    vt = [v for s, v in e.symbols.items() if s == '??_7NCEditBox@@6B@'][0]
    if _emu.sa.ENABLED:
        return vt            # the standalone's vtable is the real one
    # hook build: slots 84..86 (OnCursorPos, OnDeleteSelectionMsg, OnDoubleClick) are NewVirtualNN naked stubs that jump to the STOCK
    # address (the hook DLL has no vtable of its own for them; the installer patches the stock vtable). Without the stock image the
    # jump lands in the zeroed stand-in (WindowProc 0xF000001B / 0x203 raised only in ours): a private copy of the vtable with those
    # slots pointing at the hook functions (what the installed stock vtable does).
    pv = getattr(e, 'vt_patched', None)
    if pv is None:
        pv = e.vt_patched = e.alloc(0x300)
        e.uc.mem_write(pv, bytes(e.uc.mem_read(vt, 0x300)))
        lm = log_map()
        for slot, va in ((84, 0x1000d160), (85, 0x1000ecf0), (86, 0x100112a0)):
            e.write32(pv + 4 * slot, lm[va])
    return pv


def f32(x):
    return struct.unpack('<I', struct.pack('<f', float(x)))[0]


def make_edit(e, st):
    p = e.alloc(0x300)
    e.uc.mem_write(p, bytes(0x300))
    e.write32(p, vtable(e))
    mgr = e.alloc(0x1C0)
    e.uc.mem_write(mgr, bytes(0x1C0))
    e.write32(mgr, e.fake_vt)
    for k in INTS:
        if k in st:
            e.write32(p + F[k], st[k] & 0xFFFFFFFF)
    e.write32(p + F['bEnabled'], st.get('enabled', 1))
    e.write32(p + F['Width'], f32(st['Width']))
    e.write32(p + F['Height'], f32(st['Height']))
    e.write32(p + F['IMEManager'], mgr)
    e.write32(p + F['ScreenX'], st.get('ScreenX', 10)); e.write32(p + F['ScreenY'], st.get('ScreenY', 20))
    for k in ('Edit1', ):
        for i in range(7):
            e.write32(p + 0x1F8 + 4 * i, 0x7000 + 0x1F8 + 4 * i)
    e.write32(p + F['WhiteTex'], 0x7234)
    e.write32(p + F['BackTex'], 0x7230)
    for k in ('Left', 'Right', 'Comp', 'Reading', 'Commit'):
        e.fs_set(p + F[k], st.get(k, ''))
    if st['lang'] == 5:
        for k, tk in (('Left', 'ThaiLeft'), ('Right', 'ThaiRight')):
            cl = thai_split(st[k])
            data = e.alloc(max(1, len(cl)) * 20)
            for i, c in enumerate(cl):
                e.uc.mem_write(data + 20 * i, struct.pack('<10H', *cluster_words(c)))
            e.write32(p + F[tk], data); e.write32(p + F[tk] + 4, len(cl)); e.write32(p + F[tk] + 8, len(cl))
    return p


def dump(e, p):
    d = {}
    for k in ('Left', 'Right', 'Comp', 'Reading', 'Commit'):
        d[k] = e.fs_get(p + F[k])
    for k in INTS:
        if k not in ('bFocus', 'U1C8', 'U1CC', 'U1D8', 'U1F0'):
            d[k] = e.rds32(p + F[k])
    for k in ('ThaiLeft', 'ThaiRight'):
        data, num = e.read32(p + F[k]), e.read32(p + F[k] + 4)
        d[k] = [struct.unpack('<10H', bytes(e.uc.mem_read(data + 20 * i, 20))) for i in range(num)] if data else []
    return d


def run_case(e, A, c):
    mark = len(e.log)
    e.set_lang(c['lang'])
    e.set_shift(c.get('shift', 0))
    e.ime_type = c['ime']
    e.not_composing = c['nc']
    p = make_edit(e, c['st'])
    op = c['op']
    r = None
    extra = {}
    if op == 'key':
        vk = c['vk'] + (1 if (os.environ.get('E4_MUT2') and e.kind == 'ours' and c['vk'] == 0x25) else 0)
        r = e.call(A['keydown'], this=p, args=[vk, c['lp']])
    elif op == 'char':
        r = e.call(A['char'], this=p, args=[c['ch'], 0])
    elif op == 'insert':
        r = e.call(A['insert'], this=p, args=[e.put_wstr(c['text']), c['app']])
    elif op == 'delete':
        r = e.call(A['delete'], this=p, args=[c['idx'] & 0xFFFFFFFF])
    elif op == 'reading':
        e.reading_ptr = e.put_wstr('rd') if c['hasread'] else 0
        r = e.call(A['reading'], this=p, args=[c['wp'], c['lp']])
    elif op in ('keyup', 'mousemove', 'lbdown', 'lbup', 'dblclick', 'delselmsg'):
        r = e.call(A[op], this=p, args=[c['wp'], c['lp']])
    elif op in ('delsel', 'selall', 'killfocus', 'join', 'reformat'):
        r = e.call(A[op], this=p)
    elif op == 'winproc':
        e.basewp = c.get('basewp', 0)
        r = e.call(A['winproc'], this=p, args=[c['msg'], c['wp'], c['lp']])
    elif op == 'paint':
        dev = e.alloc(0x80)
        e.uc.mem_write(dev, bytes(0x80))
        r = e.call(A['paint'], this=p, args=[dev])
        extra['dev'] = [e.rds32(dev + o) for o in (0x38, 0x3C, 0x54, 0x58, 0x6C)]
    return dict(ret=r if op not in ('insert', 'delete', 'reformat', 'selall', 'killfocus', 'join') else None,
                state=dump(e, p), log=e.log[mark:], **extra)


def rand_text(r, lang, digits=False, wide=True):
    if digits:
        alpha = '0123456789,'
    elif lang == 5:
        alpha = 'ab ' + ''.join(chr(x) for x in [0xE01, 0xE02, 0xE14, 0xE32, 0xE33, 0xE31, 0xE34, 0xE48, 0xE40, 0xE50])
    else:
        alpha = 'abcMW 01,' + ('\uac00\uac01' if wide else '')
    return ''.join(r.choice(alpha) for _ in range(r.choice([0, 0, 1, 2, 3, 5, 8, 12, 20])))


OPS = ['reading', 'key', 'char', 'insert', 'delete', 'reformat', 'paint', 'keyup', 'mousemove', 'lbdown', 'lbup', 'dblclick',
       'delsel', 'selall', 'killfocus', 'join', 'delselmsg', 'winproc']


def rand_case(r, ops):
    lang = r.choice([0, 1, 5, 5])
    typ = r.choice([0, 0, 1, 2, 3, 3])
    digits = typ == 3
    st = dict(Width=r.choice([30, 60, 100, 160]), Height=16, bFocus=r.randint(0, 1), Type=typ,
              U1C8=1 if lang == 1 else r.choice([0, 0, 1]), U1CC=1, bCanInsert=r.randint(0, 1), bCaretOn=r.randint(0, 1),
              U1D8=0, ViewStart=0, ExtraWidth=r.choice([0, 0, 20]), MaxWidth=0, ViewStart2=0,
              MaxLen=r.choice([0, 0, 0, 5, 12]), CaretX=r.randint(0, 120),
              Left=rand_text(r, lang, digits), Right=rand_text(r, lang, digits), Comp=rand_text(r, lang, False)[:3],
              Reading='', lang=lang, bSelecting=r.choice([0, 0, 1]), SelLength=0, SelWidth=0, bSelectAll=r.choice([0, 0, 1]),
              U1F0=r.choice([0, 1]), enabled=r.choice([1, 1, 1, 1, 0]))
    ll, rl = len(st['Left']), len(st['Right'])
    if r.random() < 0.4 and (ll or rl):
        # a selection to the left or to the right of the caret
        if ll and (r.random() < 0.5 or not rl):
            st['SelLength'] = -r.randint(1, ll)
            st['SelWidth'] = sum(char_w(ch) for ch in st['Left'][st['SelLength']:])
        else:
            st['SelLength'] = r.randint(1, rl)
            st['SelWidth'] = sum(char_w(ch) for ch in st['Right'][:st['SelLength']])
    st['ViewStart'] = r.choice([0, 0, 0, 1, 2, ll // 2, ll]) if ll else 0
    st['ViewStart2'] = r.choice([0, st['ViewStart'], ll])
    ime = r.choice([1, 1, 9, 10, 13, 6]) if lang == 5 else r.choice([1, 1, 9, 10, 11, 12, 18])
    c = dict(lang=lang, ime=ime, nc=r.choice([1, 1, 1, 0, -1]), st=st, shift=r.choice([0, 0, 1]))
    op = r.choice(ops)
    c['op'] = op
    if op == 'paint':
        c['lang'] = st['lang'] = r.choice([1, 5])
        if st['Type'] == 3:
            st['Type'] = r.choice([0, 1, 2])
        st['U1CC'] = r.choice([0, 1, 1])
        st['U1D8'] = r.choice([0, 0, 1])
        st['ScreenX'] = r.randint(0, 300); st['ScreenY'] = r.randint(0, 300)
        st['ExtraWidth'] = r.choice([0, 0, 25])
        st['Reading'] = r.choice(['', '', 'ab'])
    if op == 'reformat' and not (st['Left'] or st['Right']):
        st['Left'] = rand_text(r, lang, True) or '12'
    if op == 'key':
        c['vk'] = r.choice([8, 8, 0xD, 0x10, 0x23, 0x24, 0x25, 0x25, 0x26, 0x27, 0x27, 0x28, 0x2E, 0x2E, 0xE5, 0x41])
        c['lp'] = r.choice([0, 0, 0x14B0001, 0x14D0001])
    elif op == 'char':
        c['ch'] = r.choice([8, 0x20, 0x41, 0x30, 0x35, 0x22, 0x2C, 0x7F, 0xAC00, 5, 0xE01, 0xE33, 0xE31, 0xE48, 0xE50, 0xE32, 1, 3, 0x16, 0x18])
    elif op == 'insert':
        c['text'] = rand_text(r, lang, digits)
        c['app'] = r.randint(0, 1)
    elif op == 'delete':
        c['idx'] = r.randint(-1, ll + rl + len(st['Comp']) + 1)
    elif op == 'reading':
        c['wp'] = r.choice([0x30, 0x35, 0x41, 0x2C, 0x39]); c['lp'] = r.randint(0, 9); c['hasread'] = r.choice([0, 1, 1, 1])
    elif op in ('keyup',):
        c['wp'] = r.choice([0x10, 0x10, 0x41, 0x25]); c['lp'] = 0
    elif op in ('mousemove', 'lbdown', 'lbup', 'dblclick', 'delselmsg'):
        c['wp'] = r.choice([0, 1, 5]); c['lp'] = (r.randint(0, 20) << 16) | r.randint(0, 130)
        st['bSelecting'] = r.choice([0, 1])
    elif op == 'winproc':
        c['msg'] = r.choice([0x203, 0x201, 0x202, 0x200, 0x100, 0x101, 0xF000001B & 0xFFFFFFFF, 0x402, 0x403, 0xF0000012, 0xF0000013, 0xF0000014, 0xF0000015, 0xF0000017, 0xF0000018, 0xF0000019, 0xF000001A, 0xF000001B])
        c['wp'] = r.choice([0, 1, 5, 0x41]); c['lp'] = (r.randint(0, 20) << 16) | r.randint(0, 130)
        c['basewp'] = r.choice([0, 0, 0, 1])
    return c


def main(n, seed, ops):
    r = random.Random(seed)
    cases = [rand_case(r, ops) for _ in range(n)]
    es = E4('stock')
    eo = E4('ours')
    As, Ao = addrs(es), addrs(eo)
    vtable(eo)    # builds the patched vtable copy (hook mode) before the per-case heap mark
    es.heap_mark, eo.heap_mark = es.heap_ptr, eo.heap_ptr
    bad = 0
    err = 0
    byop = {}
    for i, c in enumerate(cases):
        es.heap_ptr, eo.heap_ptr = es.heap_mark, eo.heap_mark
        try:
            a = run_case(es, As, c)
        except Exception as ex:
            a = {'exc': str(ex)[:300]}
        try:
            b = run_case(eo, Ao, c)
        except Exception as ex:
            b = {'exc': str(ex)[:300]}
        k = byop.setdefault(c['op'], [0, 0, 0])
        k[0] += 1
        if 'exc' in a or 'exc' in b:
            err += 1
            k[2] += 1
            if err <= 6:
                print('--- case %d raised' % i, c['op'], a.get('exc'), '|', b.get('exc'))
        elif a != b:
            bad += 1
            k[1] += 1
            if bad <= 8:
                print('--- case', i, c['op'], ascii({k2: v for k2, v in c.items() if k2 != 'st'}))
                print(' state in:', ascii(c['st']))
                for key in a:
                    if a[key] != b[key]:
                        print(' differs:', key)
                        if isinstance(a[key], dict):
                            for kk in a[key]:
                                if a[key][kk] != b[key].get(kk):
                                    print('   ', kk, 'stock', ascii(a[key][kk]), 'ours', ascii(b[key].get(kk)))
                        else:
                            print('  stock:', ascii(a[key])[:1200])
                            print('  ours :', ascii(b[key])[:1200])
        es.log.clear(); eo.log.clear()
    print('cases', n, 'differ', bad, 'raised', err)
    for op, (cnt, d, x) in sorted(byop.items()):
        print('  %-10s %4d cases, %3d differ, %3d raised' % (op, cnt, d, x))


if __name__ == '__main__':
    ops = sys.argv[3].split(',') if len(sys.argv) > 3 else OPS
    main(int(sys.argv[1]) if len(sys.argv) > 1 else 200, int(sys.argv[2]) if len(sys.argv) > 2 else 1, ops)
