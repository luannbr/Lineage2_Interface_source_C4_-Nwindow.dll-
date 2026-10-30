"""Differential test of the w4c audit (the stock function and ours run side by side under unicorn, tools/port/emu):
NConsoleWnd::RunHtmlCommand (0x1005aa90), NConsoleWnd::MasterConsoleEventProcess (0x1006bf40) and
NCLoadingWnd::SetLoadingTexture (0x100807b0). Needs build/bin/Release/NWindowDecomp.dll (+ .map) and `pip install unicorn`.
usage: python tools/port/emu/t_w4c.py html|master|loading|all <cases> <seed>"""
import sys, os, random, struct, re
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from emu import *
from stubs import wfmt

IMPORT_POP = {
    '?ParseCommand@@YAHPAPBGPBG@Z': 0, '?ParseToken@@YAHAAPBGPAGHH@Z': 0, '?appSprintf@@YAHPAGPBGZZ': 0,
    '??0L2ParamStack@@QAE@H@Z': 4, '?PushBack@L2ParamStack@@QAEHPAX@Z': 4, '??1L2ParamStack@@QAE@XZ': 0,
    'GetCursorPos': 4,
    '??0FString@@QAE@XZ': 0, '??1FString@@QAE@XZ': 0, '??4FString@@QAEAAV0@PBG@Z': 4, '??DFString@@QBEPBGXZ': 0,
}
FN_POP = {'OnHtml': 8, 'GetCtl': 4, 'ReqBypass': 4, 'ReqTut': 4, 'DispatchX': 16, 'GetString': 16, 'FindTex': 8}


class W4Emu(Emu):
    def __init__(self, kind):
        super().__init__(kind)
        self.k = kind
        self.nstub = 0
        self.stub_by_name = {}
        self.last_pop = 0
        self.sc = {}
        # data imports (variables of Core.dll): the IAT slot holds the address of the variable
        self.vars = {}
        for e in self.pe.DIRECTORY_ENTRY_IMPORT:
            for imp in e.imports:
                nm = imp.name.decode() if imp.name else ''
                if nm in ('?GLanguageType@@3HA', '?GCountryName@@3PAGA', '?GConfig@@3PAVFConfigCache@@A'):
                    a = self.alloc(128)       # GCountryName is a TCHAR array: the IAT holds its address
                    self.uc.mem_write(a, bytes(128))
                    self.stubs.pop(self.read32(imp.address), None)
                    self.write32(imp.address, a)
                    self.vars[nm] = a

    def new_stub(self, name):
        if name in self.stub_by_name:
            return self.stub_by_name[name]
        a = STUB_BASE + 0xA000 + 16 * self.nstub
        self.nstub += 1
        self.uc.mem_write(a, b'\xC3')
        self.stubs[a] = ('fn:' + name, None)
        self.stub_by_name[name] = a
        return a

    def sym(self, pre):
        c = [v for s, v in self.symbols.items() if s.startswith(pre)]
        assert len(c) == 1, (pre, c)
        return c[0]

    def patch(self, name, stock_va, ours_sym):
        st = self.new_stub(name)
        code = b'\x68' + struct.pack('<I', st) + b'\xC3'
        self.uc.mem_write(stock_va if self.k == 'stock' else self.sym(ours_sym), code)

    def new_vtable(self, slots):
        vt = self.alloc(0x400)
        self.uc.mem_write(vt, bytes(0x400))
        for off, nm in slots.items():
            self.write32(vt + off, self.new_stub(nm))
        return vt

    def this(self):
        return self.reg(UC_X86_REG_ECX)

    def arg(self, i):
        return self.stack_arg(i)

    def nwaddr(self, stock_va):
        return stock_va if self.k == 'stock' else NWBASE + (stock_va - 0x10000000)

    def dispatch(self, name):
        kind, nm = name.split(':', 1)
        if kind == 'imp':
            self.last_pop = IMPORT_POP[nm]
            return self.do_import(nm)
        self.last_pop = FN_POP[nm]
        return self.do_fn(nm)

    def do_import(self, nm):
        t = self.this()
        if nm == '?ParseCommand@@YAHPAPBGPBG@Z':
            pp, match = self.arg(0), self.wstr(self.arg(1))
            p = self.read32(pp)
            s = self.wstr(p)
            if s[:len(match)].upper() == match.upper() and not (len(s) > len(match) and s[len(match)].isalnum()):
                self.write32(pp, p + 2 * len(match))
                return 1
            return 0
        if nm == '?ParseToken@@YAHAAPBGPAGHH@Z':
            pp, res, mx = self.arg(0), self.arg(1), self.rds32(self.reg(UC_X86_REG_ESP) + 12)
            p = self.read32(pp)
            s = self.wstr(p)
            i = 0
            while i < len(s) and s[i] in ' \t':
                i += 1
            tok = ''
            if i < len(s) and s[i] == '"':
                i += 1
                while i < len(s) and s[i] != '"':
                    tok += s[i]
                    i += 1
                if i < len(s):
                    i += 1
            else:
                while i < len(s) and s[i] not in ' \t':
                    tok += s[i]
                    i += 1
            tok = tok[:mx - 1]
            self.write_wstr_at(res, tok)
            self.write32(pp, p + 2 * i)
            self.log.append(('ParseToken', tok))
            return 1 if tok else 0
        if nm == '?appSprintf@@YAHPAGPBGZZ':
            dst, fmt = self.arg(0), self.wstr(self.arg(1))
            s = wfmt(self, fmt, [self.stack_arg(2 + i) for i in range(6)])
            self.write_wstr_at(dst, s)
            self.log.append(('appSprintf', fmt, s))
            return len(s)
        if nm == '??0L2ParamStack@@QAE@H@Z':
            self.write32(t, 0xABCD0000 + self.arg(0))
            self.log.append(('PSctor', self.arg(0)))
            return t
        if nm == '?PushBack@L2ParamStack@@QAEHPAX@Z':
            self.log.append(('PushBack', self.wstr(self.arg(0))))
            self.sc['pushed'] = self.wstr(self.arg(0))
            return 1
        if nm == '??1L2ParamStack@@QAE@XZ':
            self.log.append(('PSdtor',))
            return None
        if nm == 'GetCursorPos':
            p = self.arg(0)
            self.write32(p, self.sc['cx'])
            self.write32(p + 4, self.sc['cy'])
            self.log.append(('GetCursorPos',))
            return 1
        if nm == '??0FString@@QAE@XZ':
            self.write32(t, 0); self.write32(t + 4, 0); self.write32(t + 8, 0)
            return t
        if nm == '??1FString@@QAE@XZ':
            return t
        if nm == '??4FString@@QAEAAV0@PBG@Z':
            p = self.arg(0)
            s = self.wstr(p) if p else ''
            if s:
                d = self.put_wstr(s)
                self.write32(t, d); self.write32(t + 4, len(s) + 1); self.write32(t + 8, len(s) + 1)
            else:
                self.write32(t, 0); self.write32(t + 4, 0); self.write32(t + 8, 0)
            return t
        if nm == '??DFString@@QBEPBGXZ':
            return self.read32(t) if self.read32(t + 4) else self.sc['empty']
        raise RuntimeError('import ' + nm)

    def do_fn(self, nm):
        t = self.this()
        if nm == 'OnHtml':
            self.log.append(('OnHtml', self.wstr(self.arg(0)), self.arg(1) == self.sc.get('viewer')))
            return self.sc['onhtml']
        if nm == 'GetCtl':
            name = self.wstr(self.arg(0))
            self.log.append(('GetCtl', name))
            v = self.sc['ctl'].get(name)
            return self.put_wstr(v) if v is not None else 0
        if nm in ('ReqBypass', 'ReqTut'):
            self.log.append((nm, self.read32(self.arg(0)) & 0xFFFF))
            return None
        if nm == 'DispatchX':
            self.log.append(('DispatchX', self.arg(0), self.arg(1), self.arg(2), self.arg(3)))
            return self.sc['dispret'].pop(0) if self.sc['dispret'] else 0
        if nm == 'GetString':     # GConfig->GetString(Section, Key, FString& Result, File)
            sec, key, res, fil = self.wstr(self.arg(0)), self.wstr(self.arg(1)), self.arg(2), self.wstr(self.arg(3))
            self.log.append(('GetString', sec, key, fil))
            v = self.sc['cfg'].get(key)
            if v is None:
                return 0
            d = self.put_wstr(v)
            self.write32(res, d); self.write32(res + 4, len(v) + 1); self.write32(res + 8, len(v) + 1)
            return 1
        if nm == 'FindTex':
            self.log.append(('FindTexture', self.wstr(self.arg(0)), self.arg(1)))
            return 0x7777
        raise RuntimeError('fn ' + nm)


def setup(kind):
    e = W4Emu(kind)
    e.sc['empty'] = e.put_wstr('')
    e.console = e.alloc(0x4000)
    e.desktop = e.alloc(0x200)
    e.write32(e.console + 0x3BF4, e.desktop)      # GetDesktop() (C2: +0x3540)
    return e


# ---------------------------------------------------------------------------------------------------------------
# NConsoleWnd::RunHtmlCommand(TCHAR* Command, NCHtmlViewer* Viewer)
def gen_command(rnd):
    words = ['a', 'b', 'Hello', 'x1', '-h', '-H', '$name', '$id', '$missing', '"quoted word"', '12', '$', 'dlg', '-hx']
    n = rnd.choice([0, 1, 1, 2, 3, 4, 6])
    toks = [rnd.choice(words) for _ in range(n)]
    pre = rnd.choice(['bypass', 'bypass', 'bypass', 'BYPASS', 'Bypass', 'other', 'bypassx', ''])
    return (pre + ' ' + ' '.join(toks)).rstrip() if pre else ' '.join(toks)


def html(cases, seed):
    res = {}
    for kind in ('stock', 'ours'):
        e = setup(kind)
        e.patch('ReqBypass', 0x10068D40, '?RequestBypassToServer@NConsoleWnd@@')
        e.patch('ReqTut', 0x10068ED0, '?RequestTutorialPassCmdToServer@NConsoleWnd@@')
        fn = 0x10072560 if kind == 'stock' else e.sym('?RunHtmlCommand@NConsoleWnd@@')
        tut = e.gptr(0x102C89E0)      # NCTutorialHtmlViewer's UClass (the stock walks the viewer's class chain for it)
        e.write32(e.desktop, e.new_vtable({0x128: 'OnHtml'}))      # vslot 74
        out = []
        rnd = random.Random(seed)
        for i in range(cases):
            cmd = gen_command(rnd)
            has_view = rnd.random() < 0.8
            e.sc['ctl'] = {'name': rnd.choice(['Bob', 'with space', '', 'X']), 'id': rnd.choice(['77', '', 'Y z'])}
            e.sc['onhtml'] = rnd.randint(-2, 5)
            vt = e.new_vtable({0x138: 'GetCtl'})        # vslot 78
            view = 0
            if has_view:
                view = e.alloc(0x400)
                e.uc.mem_write(view, bytes(0x400))
                e.write32(view, vt)
                # class chain: the tutorial viewer class is somewhere up (or not)
                chain_has = rnd.random() < 0.4
                c3 = e.alloc(0x40); e.uc.mem_write(c3, bytes(0x40))
                c2 = tut if chain_has and rnd.random() < 0.5 else e.alloc(0x40)
                if c2 != tut:
                    e.uc.mem_write(c2, bytes(0x40))
                    e.write32(c2 + 0x34, tut if chain_has else c3)     # UClass::SuperField (C2: +0x28)
                c1 = e.alloc(0x40); e.uc.mem_write(c1, bytes(0x40))
                e.write32(c1 + 0x34, c2)
                start = rnd.choice([c1, c1, tut, 0])
                e.write32(view + 0x24, start)
            e.sc['viewer'] = view
            e.sc.pop('pushed', None)
            e.log.clear()
            buf = e.put_wstr(cmd)
            r = e.call(fn, this=e.console, args=[buf, view])
            out.append((cmd, has_view, r, [x for x in e.log]))
        res[kind] = out
    bad = 0
    for a, b in zip(res['stock'], res['ours']):
        if a != b:
            bad += 1
            if bad < 6:
                print('DIFF', a, '\n    ', b)
    print('html cases %d  differ %d  raised 0' % (cases, bad))
    n_tut = sum(1 for a in res['stock'] if any(x[0] == 'ReqTut' for x in a[3]))
    n_byp = sum(1 for a in res['stock'] if any(x[0] == 'ReqBypass' for x in a[3]))
    n_dsk = sum(1 for a in res['stock'] if any(x[0] == 'OnHtml' for x in a[3]))
    print('  coverage: tutorial %d, bypass %d, desktop %d' % (n_tut, n_byp, n_dsk))
    return bad


# ---------------------------------------------------------------------------------------------------------------
# NConsoleWnd::MasterConsoleEventProcess(INT Key, INT Action, FLOAT Delta)
def master(cases, seed):
    res = {}
    for kind in ('stock', 'ours'):
        e = setup(kind)
        e.patch('DispatchX', 0x1007F5D0, '?DispatchWndMsgX@NConsoleWnd@@')
        fn = 0x1007FD80 if kind == 'stock' else e.sym('?MasterConsoleEventProcess@NConsoleWnd@@UAEHHHM@Z')
        hwnd = e.nwaddr(0x102C39F0)         # GViewportHWnd
        e.write32(hwnd, 0x00ABCDEF)
        out = []
        rnd = random.Random(seed)
        for i in range(cases):
            key = rnd.choice([1, 2, 3, 0xE7, 0xEC, 0xED, 0xEE, 0, 0x10, rnd.randint(0, 0xF0), rnd.randint(-3, 0x200)])
            action = rnd.choice([0, 1, 1, 2, 3, 3, 4, rnd.randint(0, 5)])
            delta = rnd.choice([0.0, 1.0, -1.0, 2.5, -120.0, 120.0, 0.4, rnd.uniform(-500, 500)])
            e.sc['cx'] = rnd.randint(-100, 1500)
            e.sc['cy'] = rnd.randint(-100, 1200)
            e.sc['dispret'] = [rnd.choice([0, 1, 0, 1]) for _ in range(2)]
            e.write32(e.console + 0x3C3C, rnd.randint(0, 5))         # MouseWheelDelta (C2: +0x3574)
            e.write32(e.console + 0x3C54, rnd.choice([0, 1]))        # bWheelHandled (C2: +0x3590)
            before = (e.rds32(e.console + 0x3C3C), e.rds32(e.console + 0x3C54))
            e.log.clear()
            r = e.call(fn, this=e.console, args=[key & 0xFFFFFFFF, action, struct.unpack('<I', struct.pack('<f', delta))[0]])
            out.append(((key, action, delta, before), r, e.rds32(e.console + 0x3C3C), e.rds32(e.console + 0x3C54),
                        [x for x in e.log]))
        res[kind] = out
    bad = 0
    for a, b in zip(res['stock'], res['ours']):
        if a != b:
            bad += 1
            if bad < 6:
                print('DIFF', a, '\n    ', b)
    print('master cases %d  differ %d  raised 0' % (cases, bad))
    n = sum(1 for a in res['stock'] if any(x[0] == 'DispatchX' for x in a[4]))
    print('  coverage: %d cases dispatched a message' % n)
    return bad


# ---------------------------------------------------------------------------------------------------------------
# NCLoadingWnd::SetLoadingTexture(INT ScreenID)
def loading(cases, seed):
    res = {}
    for kind in ('stock', 'ours'):
        e = setup(kind)
        e.patch('FindTex', 0x1002B450, '?FindTexture@NCObject@@QAEPAVUTexture@@PBGH@Z')
        fn = 0x1009A600 if kind == 'stock' else e.sym('?SetLoadingTexture@NCLoadingWnd@@')
        gcfg = e.vars['?GConfig@@3PAVFConfigCache@@A']
        cfgobj = e.alloc(16)
        e.write32(gcfg, cfgobj)
        e.write32(cfgobj, e.new_vtable({0xC: 'GetString'}))
        lang = e.vars['?GLanguageType@@3HA']
        cn = e.vars['?GCountryName@@3PAGA']
        out = []
        rnd = random.Random(seed)
        wnd = e.alloc(0x400)
        for i in range(cases):
            e.uc.mem_write(wnd, bytes(0x400))
            e.write32(lang, rnd.choice([0, 0, 1, 2, 3]))
            e.write_wstr_at(cn, rnd.choice(['KOR', 'USA', 'JPN', 'TW']))
            e.sc['cfg'] = {}
            for c in ('KOR', 'USA', 'JPN', 'TW'):
                if rnd.random() < 0.5:
                    e.sc['cfg']['%s_LoadingTexture' % c] = 'Tex.%s%d' % (c, rnd.randint(0, 9))
            sid = rnd.choice([10001, 10002, 10003, 10004, 0, -1, rnd.randint(9990, 10010)])
            e.log.clear()
            e.call(fn, this=wnd, args=[sid & 0xFFFFFFFF])
            out.append(((e.rds32(lang), sid), e.read32(wnd + 0xE8), [x for x in e.log]))
        res[kind] = out
    bad = 0
    for a, b in zip(res['stock'], res['ours']):
        if a != b:
            bad += 1
            if bad < 6:
                print('DIFF', a, '\n    ', b)
    print('loading cases %d  differ %d  raised 0' % (cases, bad))
    return bad


if __name__ == '__main__':
    t = sys.argv[1] if len(sys.argv) > 1 else 'all'
    cases = int(sys.argv[2]) if len(sys.argv) > 2 else 2000
    seed = int(sys.argv[3]) if len(sys.argv) > 3 else 1
    bad = 0
    if t in ('html', 'all'):
        bad += html(cases, seed)
    if t in ('master', 'all'):
        bad += master(cases, seed)
    if t in ('loading', 'all'):
        bad += loading(cases, seed)
    sys.exit(1 if bad else 0)
