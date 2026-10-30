"""Differential test of NKeyCrypt (nProtect's keyboard protection helper, src/nwindow/NKeyCrypt.cpp) against the stock code:
Open (0x1004c080), Close (0x1004be60), RegisterMessage (0x1004be90), EnableProtectRegistry (0x1004bec0),
IsAdministrator (0x1004bf50) and the two thunks GetLoadStartup / LoadAtStartup (0x1004c060 / 0x1004beb0). (C4 addresses; the C2
script had 0x1003xxxx. The standalone resolves npkcrypt the same way, with LoadLibraryA / GetProcAddress, as the hook build.)

The stock calls npkcrypt.dll through its IAT (imports by ordinal); ours resolves the same ordinals with
LoadLibraryA / GetProcAddress when it first needs them. Both run under unicorn with the same Python stand-ins for what they
call (the file manager, GLog, the registry, the security API, the eight npkcrypt functions); the log of those calls, the
return value and the handle in the object must be equal for every scenario.

Known difference, masked: when the registry query of EnableProtectRegistry fails, the stock hands RegSetValueExW the
type left in an uninitialised local; ours gives REG_DWORD (src/nwindow/NKeyCrypt.cpp).

usage: python tools/port/emu/t_nkeycrypt.py <cases> <seed>   (needs build/bin/Release/NWindowDecomp.dll + .map, or NWD_SA=1 for the standalone)"""
import sys, os, random, struct
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from emu import *
from stubs import wfmt

NPK_POP = {1: 0, 2: 4, 16: 12, 25: 4, 26: 4, 36: 4, 37: 8, 41: 0}
IMPORT_POP = {
    '?GLog@@3PAVFOutputDevice@@A': None,
    '?Logf2@FOutputDevice@@QAAXW4EName@@PBGZZ': 0, '?appStrcpy@@YAPAGPAGPBG@Z': 0, '?appStrcat@@YAPAGPAGPBG@Z': 0,
    '?appStrrchr@@YAPAGPBGH@Z': 0, '?appSprintf@@YAHPAGPBGZZ': 0, '??DFString@@QBEPBGXZ': 0, '??1FString@@QAE@XZ': 0,
    'GetCurrentDirectoryA': 8, 'GetLastError': 0, 'LoadLibraryA': 4, 'GetProcAddress': 8,
    'RegOpenKeyW': 12, 'RegQueryValueExW': 24, 'RegSetValueExW': 24, 'RegCloseKey': 4,
    'AllocateAndInitializeSid': 44, 'OpenProcessToken': 12, 'GetTokenInformation': 20, 'EqualSid': 8, 'FreeSid': 4,
    'CloseHandle': 4, 'GetCurrentProcess': 0,
}
FN_POP = {'GetDefaultDirectory': 4, 'malloc': 0, 'free': 0}
KEY_PATH = 'Software\\INCAInternet\\npkcx'
HIDDEN = {'LoadLibraryA', 'GetProcAddress'}    # ours only: infrastructure, not compared
STUB_NPK = STUB_BASE + 0xA000


class NkEmu(Emu):
    def __init__(self, kind):
        super().__init__(kind)
        self.last_pop = 0
        self.sc = None
        self.hidden = []
        # the data imports: the IAT slot holds the address of Core's variable
        self.glog_var = self.alloc(4)
        self.glog_obj = self.alloc(16)
        self.fm_var = self.alloc(4)
        self.fm_obj = self.alloc(16)
        self.fm_vt = self.alloc(4 * 24)
        self.unicode_var = self.alloc(4)
        self.write32(self.fm_obj, self.fm_vt)
        self.write32(self.fm_var, self.fm_obj)
        self.fn_stub_n = 0
        slot14 = self.new_stub('fn:GetDefaultDirectory')
        self.write32(self.fm_vt + 4 * 14, slot14)
        for e in self.pe.DIRECTORY_ENTRY_IMPORT:
            for imp in e.imports:
                nm = imp.name.decode() if imp.name else ''
                if nm == '?GLog@@3PAVFOutputDevice@@A':
                    self.write32(imp.address, self.glog_var)
                elif nm == '?GFileManager@@3PAVFFileManager@@A':
                    self.write32(imp.address, self.fm_var)
                elif nm == '?GUnicodeOS@@3HA':
                    self.write32(imp.address, self.unicode_var)
        if kind == 'stock':
            self.hwnd_addr = 0x102C39F0            # GViewportHWnd
            self.redirect(0x10171E01, 'malloc')    # _malloc / _free of the stock's static CRT
            self.redirect(0x101722E3, 'free')
        else:
            self.hwnd_addr = NWBASE + 0x2C39F0     # (the standalone: translated to its own GViewportHWnd by SaUc)
            self.redirect(self.sym('_malloc'), 'malloc')
            self.redirect(self.sym('_free'), 'free')
        self.heap_base = self.heap_ptr

    def sym(self, name):
        return self.symbols[name]

    def forget_npk(self):
        """ours: the npkcrypt functions are resolved on first use; make the next call resolve them again."""
        self.write32(self.find('?GNpkTried@@'), 0)
        self.write32(self.find('?NPKOpenDriver@@'), 0)

    def find(self, prefix):
        c = [v for k, v in self.symbols.items() if k.startswith(prefix)]
        if len(c) != 1:
            raise RuntimeError('symbol %s: %s' % (prefix, c))
        return c[0]

    def new_stub(self, name):
        a = STUB_BASE + 0x8000 + 16 * self.fn_stub_n
        self.fn_stub_n += 1
        self.uc.mem_write(a, b'\xC3')
        self.stubs[a] = (name, None)
        return a

    def redirect(self, va, name):
        st = self.new_stub('fn:' + name)
        self.uc.mem_write(va, b'\x68' + struct.pack('<I', st) + b'\xC3')

    # ---- the stand-ins -------------------------------------------------------------------------
    def setup(self, sc):
        """sc: the scenario; resets the world."""
        self.sc = sc
        self.log = []
        self.hidden = []
        self.heap_ptr = self.heap_base
        self.write32(self.glog_var, self.glog_obj if sc['glog'] else 0)
        self.write32(self.unicode_var, 1 if sc.get('unicode', 1) else 0)
        self.write32(self.hwnd_addr, sc.get('hwnd', 0))
        self.flags = dict(sc.get('flags', {}))
        self.regstate = dict(sc.get('reg', {'key': 0}))
        self.groups = sc.get('groups', [])
        self.sids = {}
        self.proc_dirs = sc.get('cwd', 'C:\\L2\\system')

    def dispatch(self, name):
        kind, nm = name.split(':', 1)
        if kind == 'fn':
            self.last_pop = FN_POP[nm]
            return self.do_fn(nm)
        if nm.startswith('ord'):
            n = int(nm[3:])
            self.last_pop = NPK_POP[n]
            return self.do_npk(n)
        self.last_pop = IMPORT_POP.get(nm)
        if self.last_pop is None:
            raise RuntimeError('unstubbed import ' + nm)
        return self.do_import(nm)

    def arg(self, i):
        return self.stack_arg(i)

    def ansi(self, a):
        out = bytearray()
        while True:
            b = bytes(self.uc.mem_read(a, 1))[0]
            if not b:
                return out.decode('latin-1')
            out.append(b)
            a += 1

    def do_fn(self, nm):
        if nm == 'GetDefaultDirectory':
            sret = self.arg(0)
            self.fs_set(sret, self.sc['dir'])
            self.log.append(('GetDefaultDirectory',))
            return sret
        if nm == 'malloc':
            n = self.arg(0)
            self.log.append(('malloc', n))
            return self.alloc(max(n, 16))
        if nm == 'free':
            self.log.append(('free', self.arg(0) != 0))
            return None
        raise RuntimeError(nm)

    def do_npk(self, n):
        sc = self.sc
        if n == 1:
            self.log.append(('NPKOpenDriver',))
            return u32(sc['open'])
        if n == 2:
            self.log.append(('NPKCloseDriver', self.rds32(self.reg_esp() + 4)))
            return u32(sc['close'])
        if n == 16:
            self.log.append(('NPKRegCryptMsg', self.rds32(self.reg_esp() + 4), self.arg(1), self.arg(2)))
            return u32(sc['regmsg'])
        if n == 25:
            self.log.append(('NPKSetDrvPath', self.ansi(self.arg(0))))
            return 1
        if n == 26:
            self.log.append(('NPKLoadAtStartup', self.rds32(self.reg_esp() + 4)))
            return 1
        if n == 36:
            p = self.wstr(self.arg(0))
            v = self.flags.get(p.lower(), -1)
            self.log.append(('NPKGetAppCompatFlag', p, v))
            return u32(v)
        if n == 37:
            p = self.wstr(self.arg(0))
            f = self.rds32(self.reg_esp() + 8)
            self.log.append(('NPKSetAppCompatFlag', p, f))
            if f == -1:
                self.flags.pop(p.lower(), None)
            else:
                self.flags[p.lower()] = f
            return 1
        if n == 41:
            self.log.append(('NPKGetLoadStartup',))
            return u32(sc['loadstart'])
        raise RuntimeError(n)

    def reg_esp(self):
        return self.reg_read(UC_X86_REG_ESP)

    def reg_read(self, r):
        return self.uc.reg_read(r)

    def do_import(self, nm):
        t = self.reg_read(UC_X86_REG_ECX)
        sc = self.sc
        if nm in HIDDEN:
            if nm == 'LoadLibraryA':
                self.hidden.append(('LoadLibraryA', self.ansi(self.arg(0))))
                return 0x6000000 if sc.get('dll', 1) else 0
            n = self.arg(1)
            self.hidden.append(('GetProcAddress', n))
            if n >= 0x10000 or n not in NPK_POP:
                return 0
            a = STUB_NPK + 16 * n
            self.uc.mem_write(a, b'\xC3')
            self.stubs[a] = ('imp:ord%d' % n, None)
            return a
        if nm == '?Logf2@FOutputDevice@@QAAXW4EName@@PBGZZ':
            fmt = self.wstr(self.arg(2))
            args = [self.stack_arg(3 + i) for i in range(4)]
            self.log.append(('Logf2', self.arg(0) == self.glog_obj, self.arg(1), wfmt(self, fmt, args)))
            return None
        if nm == '?appStrcpy@@YAPAGPAGPBG@Z':
            d, s = self.arg(0), self.arg(1)
            self.write_wstr_at(d, self.wstr(s))
            return d
        if nm == '?appStrcat@@YAPAGPAGPBG@Z':
            d, s = self.arg(0), self.arg(1)
            self.write_wstr_at(d, self.wstr(d) + self.wstr(s))
            return d
        if nm == '?appStrrchr@@YAPAGPBGH@Z':
            s, c = self.arg(0), self.arg(1)
            txt = self.wstr(s)
            i = txt.rfind(chr(c & 0xFFFF))
            return s + 2 * i if i >= 0 else 0
        if nm == '?appSprintf@@YAHPAGPBGZZ':
            dst, fmt = self.arg(0), self.wstr(self.arg(1))
            s = wfmt(self, fmt, [self.stack_arg(2 + i) for i in range(4)])
            self.write_wstr_at(dst, s)
            return len(s)
        if nm == '??DFString@@QBEPBGXZ':
            return self.read32(t)
        if nm == '??1FString@@QAE@XZ':
            self.log.append(('~FString',))
            return t
        if nm == 'GetCurrentDirectoryA':
            buf = self.arg(1)
            d = self.proc_dirs.encode('latin-1')
            self.uc.mem_write(buf, d + b'\0')
            self.log.append(('GetCurrentDirectoryA', self.arg(0)))
            return len(d)
        if nm == 'GetLastError':
            self.log.append(('GetLastError',))
            return u32(sc['lasterr'])
        # the registry
        if nm == 'RegOpenKeyW':
            hk, sub, phk = self.arg(0), self.wstr(self.arg(1)), self.arg(2)
            self.log.append(('RegOpenKeyW', hk, sub))
            if sub.lower() == KEY_PATH.lower() and self.regstate['key']:
                self.write32(phk, 0x77)
                return 0
            return 2
        if nm == 'RegQueryValueExW':
            h, name, res, ptype, pdata, pcb = [self.arg(i) for i in range(6)]
            self.log.append(('RegQueryValueExW', h, self.wstr(name), res, self.read32(pcb)))
            if 'protect' in self.regstate and name:
                self.write32(ptype, 4)
                self.write32(pdata, self.regstate['protect'])
                self.write32(pcb, 4)
                return 0
            return 2
        if nm == 'RegSetValueExW':
            h, name, res, typ, pdata, cb = [self.arg(i) for i in range(6)]
            ok_query = 'protect' in self.regstate
            self.log.append(('RegSetValueExW', h, self.wstr(name), res, typ if ok_query else '?', self.read32(pdata), cb))
            self.regstate['protect'] = self.read32(pdata)
            return 0
        if nm == 'RegCloseKey':
            self.log.append(('RegCloseKey', self.arg(0)))
            return 0
        # the security API
        if nm == 'AllocateAndInitializeSid':
            auth = bytes(self.uc.mem_read(self.arg(0), 6))
            cnt = self.arg(1)
            subs = [self.arg(2 + i) for i in range(8)]
            ppsid = self.arg(10)
            sid = self.alloc(16)
            self.sids[sid] = (auth, cnt, tuple(subs[:cnt]))
            self.write32(ppsid, sid)
            self.log.append(('AllocateAndInitializeSid', auth.hex(), cnt, subs))
            return 1
        if nm == 'OpenProcessToken':
            self.log.append(('OpenProcessToken', self.arg(0), self.arg(1)))
            self.write32(self.arg(2), 0x99)
            return 1
        if nm == 'GetCurrentProcess':
            return 0xFFFFFFFF
        if nm == 'GetTokenInformation':
            tok, cls, buf, ln, pret = [self.arg(i) for i in range(5)]
            need = 4 + 8 * len(self.groups)
            self.log.append(('GetTokenInformation', tok, cls, buf != 0, ln))
            if buf == 0:
                self.write32(pret, need)
                return 0
            self.write32(buf, len(self.groups))
            for i, g in enumerate(self.groups):
                sid = self.alloc(16)
                self.sids[sid] = g
                self.write32(buf + 4 + 8 * i, sid)
                self.write32(buf + 8 + 8 * i, 7)
            self.write32(pret, need)
            return 1
        if nm == 'EqualSid':
            a, b = self.sids.get(self.arg(0)), self.sids.get(self.arg(1))
            self.log.append(('EqualSid',))
            return 1 if a == b else 0
        if nm == 'FreeSid':
            self.log.append(('FreeSid',))
            return 0
        if nm == 'CloseHandle':
            self.log.append(('CloseHandle', self.arg(0)))
            return 1
        raise RuntimeError('import ' + nm)


ADMIN = (bytes([0, 0, 0, 0, 0, 5]), 2, (0x20, 0x220))


def scenario(r):
    parent = r.choice(['C:\\Games\\Lineage II', 'D:\\L2', 'E:\\a b\\c', 'C:\\x'])
    d = parent + '\\system'
    exe = d + '\\L2.exe'
    launcher = parent + '\\LineageII.exe'
    flags = {}
    if r.random() < 0.3:
        flags[exe.lower()] = r.choice([0x10, 1, 0x200])
    if r.random() < 0.3:
        flags[launcher.lower()] = r.choice([0x10, 1])
    reg = {'key': r.choice([0, 1, 1, 1])}
    st = r.choice(['none', 'zero', 'one'])
    if reg['key'] and st != 'none':
        reg['protect'] = 0 if st == 'zero' else 1
    groups = []
    for _ in range(r.randint(0, 5)):
        groups.append((bytes([0, 0, 0, 0, 0, 5]), 2, (r.choice([0x20, 0x21, 0x15]), r.choice([0x220, 0x221, 0x222]))))
    if r.random() < 0.5:
        groups.insert(r.randint(0, len(groups)), ADMIN)
    return {'glog': r.choice([0, 1, 1]), 'dir': d, 'flags': flags, 'reg': reg, 'groups': groups,
            'open': r.choice([-1, 0, 5, 0x1234, 1]), 'close': r.choice([0, 1, 7]), 'regmsg': r.choice([0, 1, 5]),
            'lasterr': r.choice([0, 5, 0x7E, 1223]), 'hwnd': r.randint(1, 0xFFFFF), 'unicode': r.choice([0, 1, 1, 1]),
            'loadstart': r.choice([0, 1, 3]), 'handle': r.choice([0, 0, 3, -1, 99]), 'cwd': r.choice(['C:\\L2\\system', 'D:\\x']),
            'bload': r.choice([0, 1])}


def run_case(e, fns, sc):
    e.setup(sc)
    obj = e.alloc(16)
    e.write32(obj, u32(sc['handle']))
    res = []
    for name in ('Close', 'RegisterMessage', 'Open', 'Close', 'EnableProtectRegistry', 'IsAdministrator', 'GetLoadStartup',
                 'LoadAtStartup'):
        va = fns[name]
        if name == 'LoadAtStartup':
            r = e.call(va, this=obj, args=[sc['bload']])
        elif name in ('EnableProtectRegistry', 'IsAdministrator', 'GetLoadStartup'):
            r = e.call(va, this=obj, args=[])
        else:
            r = e.call(va, this=obj, args=[])
        if name in ('EnableProtectRegistry', 'LoadAtStartup'):
            r = None
        res.append((name, r, e.rds32(obj), list(e.log), dict(e.regstate), dict(e.flags)))
        e.log.clear()
    return res


def main(n, seed):
    r = random.Random(seed)
    cases = [scenario(r) for _ in range(n)]
    out = {}
    for kind in ('stock', 'ours'):
        e = NkEmu(kind)
        if kind == 'stock':
            fns = {'Close': 0x1004BE60, 'RegisterMessage': 0x1004BE90, 'Open': 0x1004C080, 'EnableProtectRegistry': 0x1004BEC0,
                   'IsAdministrator': 0x1004BF50, 'GetLoadStartup': 0x1004C060, 'LoadAtStartup': 0x1004BEB0}
        else:
            fns = {'Close': e.find('?Close@NKeyCrypt@@QAEHXZ'), 'RegisterMessage': e.find('?RegisterMessage@NKeyCrypt@@QAEHXZ'),
                   'Open': e.find('?Open@NKeyCrypt@@QAEHXZ'),
                   'EnableProtectRegistry': e.find('?EnableProtectRegistry@NKeyCrypt@@SAXXZ'),
                   'IsAdministrator': e.find('?IsAdministrator@NKeyCrypt@@SAHXZ'),
                   'GetLoadStartup': e.find('?GetLoadStartup@NKeyCrypt@@SAHXZ'),
                   'LoadAtStartup': e.find('?LoadAtStartup@NKeyCrypt@@SAXH@Z')}
        outs = []
        for sc in cases:
            if kind == 'ours':
                e.forget_npk()     # the function pointers are resolved again, on first use
            outs.append(run_case(e, fns, sc))
        out[kind] = outs
    bad = 0
    n_open_ok = n_compat = 0
    for i, (a, b) in enumerate(zip(out['stock'], out['ours'])):
        for x, y in zip(a, b):
            xl, yl = x[3], y[3]
            if x != y:
                bad += 1
                if bad <= 4:
                    print('case', i, cases[i])
                    print('  call', x[0], 'stock ret/handle', x[1:3], 'ours', y[1:3])
                    for j in range(max(len(xl), len(yl))):
                        p = xl[j] if j < len(xl) else None
                        q = yl[j] if j < len(yl) else None
                        if p != q:
                            print('  first diff at log', j, '\n   stock', p, '\n   ours ', q)
                            break
                    if x[4:] != y[4:]:
                        print('  registry/flags: stock', x[4:], 'ours', y[4:])
            if x[0] == 'Open':
                n_open_ok += x[1] == 1
                n_compat += any('Compatibility' in str(l) for l in xl)
    print('nkeycrypt calls compared', n * 8, '(Open succeeded in', n_open_ok, ', compat branch in', n_compat, ')')
    print('nkeycrypt cases %d  differ %d  raised 0' % (n, bad))
    if '--show' in sys.argv:
        for kind in ('stock', 'ours'):
            for x in out[kind][0]:
                print(kind, x[0], 'ret', x[1], 'handle', x[2])
                for l in x[3]:
                    print('     ', l)
    # ours only: no npkcrypt.dll (LoadLibrary fails): nothing of it runs, the keyboard protection simply does not open
    e = NkEmu('ours')
    fns = {'Close': e.find('?Close@NKeyCrypt@@QAEHXZ'), 'RegisterMessage': e.find('?RegisterMessage@NKeyCrypt@@QAEHXZ'),
           'Open': e.find('?Open@NKeyCrypt@@QAEHXZ'), 'EnableProtectRegistry': e.find('?EnableProtectRegistry@NKeyCrypt@@SAXXZ'),
           'IsAdministrator': e.find('?IsAdministrator@NKeyCrypt@@SAHXZ'),
           'GetLoadStartup': e.find('?GetLoadStartup@NKeyCrypt@@SAHXZ'), 'LoadAtStartup': e.find('?LoadAtStartup@NKeyCrypt@@SAXH@Z')}
    nodll_bad = 0
    for k in range(50):
        sc = scenario(r)
        sc['dll'] = 0
        sc['handle'] = r.choice([0, 0, 5])
        e.forget_npk()
        res = {x[0]: x for x in run_case(e, fns, sc)}
        ops = [l for x in res.values() for l in x[3] if l[0].startswith('NPK')]
        # no npkcrypt function was reached; Open says 0 and logs the failure with LoadLibrary's error (here the stand-in's
        # GetLastError); RegisterMessage / GetLoadStartup answer 0; a handle that was open stays (it cannot be closed)
        want_fail = [l for l in res['Open'][3] if l[0] == 'Logf2' and l[3].startswith('Open KeyCrypt Driver Failed')]
        ok = (not ops and res['Open'][1] == 0 and res['RegisterMessage'][1] == 0 and res['GetLoadStartup'][1] == 0
              and (not sc['glog'] or len(want_fail) == 1))
        if not ok:
            nodll_bad += 1
            if nodll_bad <= 3:
                print('no-DLL case', sc)
                print('   ', {k2: v[1:3] for k2, v in res.items()}, ops, want_fail)
    print('nkeycrypt-nodll cases 50  differ %d  raised 0' % nodll_bad)
    return 1 if bad or nodll_bad else 0


if __name__ == '__main__':
    sys.exit(main(int(sys.argv[1]), int(sys.argv[2])))
