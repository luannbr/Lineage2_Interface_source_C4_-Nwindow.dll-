# Differential test of the w3ui lane's C2-only ports (the stock function and ours run side by side under unicorn, tools/port/emu): the
# word wrap converter (ConvertDocument, WrapText, SendRequest, ConnectServer, LoadInputFile, WriteOutputFile, OnGenerate),
# NCommandParser::ExpandToken, the system message expander (0x1003e5b0) and NCVideoOptionWnd::Apply. Needs build/bin/Release/NWindowDecomp.dll
# (+ .map) and `pip install unicorn`.
"""usage: python tools/port/emu/t_w3ui.py <test> <cases> <seed>
   tests: convert wrap send connect load write generate token sysmsg apply all"""
import sys, os, struct, random, re

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import stubs
from stubs import *
import sa

stubs.STOCK_FN.clear()
stubs.OURS_FN.clear()
TipEmu.CONSOLE_VA = 0x102C39F4        # GConsole of the C4 image (the C2 script: 0x101A415C)

GARBAGE = 0xCD
stubs.IMPORT_POP.update({
    '?appMemset@@YAXPAXHH@Z': 0, '?appAtoi@@YAHPBG@Z': 0, '?appStrncpy@@YAPAGPAGPBGH@Z': 0, '?appStricmp@@YAHPBG0@Z': 0,
    '?Logf2@FOutputDevice@@QAAXPBGZZ': 0, '?appMemmove@@YAPAXPAXPBXH@Z': 0,
    'FindFirstFileW': 8, 'FindNextFileW': 8,
    'WSAStartup': 8, 'socket': 12, 'inet_addr': 4, 'gethostbyname': 4, 'htons': 4, 'connect': 12, 'WSAGetLastError': 0,
    'send': 16, 'recv': 16,
})
stubs.FN_POP.update({
    'Malloc': 8, 'Realloc': 12, 'Free': 4, 'RdDtor': 4, 'RdSerialize': 8, 'WrSerialize': 8, 'WrFlush': 0, 'WrDtor': 4,
    'FsFileSize': 4, 'FsReader': 12, 'FsWriter': 12, 'ShowPage': 4, 'LoadHtml': 4, 'AddItem': 4, 'LogFlush': 0, 'SendReq': 12,
    'GetRes': 16, 'GetRefresh': 20, 'SetRes': 28, 'DelAllTex': 0, 'IsTrilinear': 0, 'SetTrilinear': 4, 'UpdateGamma': 0,
    'IsFullscreen': 0, 'ConfigSet': 16, 'ConfigGetFloat': 16, 'InitModes': 0, 'CurSet': 8, 'CurLock': 0, 'CurUnlock': 0,
    'GetSelected': 0, 'GetMyUser': 0, 'GetPet': 4, 'GetParty': 4,
})

DATA_RE = re.compile(r'^\?[A-Za-z0-9_]+@@3')


def wcut16(b):
    out = []
    for i in range(0, len(b) - 1, 2):
        c = b[i] | (b[i + 1] << 8)
        if c == 0:
            break
        out.append(chr(c))
    return ''.join(out)


class W3Emu(TipEmu):
    def __init__(self, kind):
        super().__init__(kind)
        self.k = kind
        self.nstub = 0
        self.stub_by_name = {}
        self.vars = {}
        self.sc = {}
        for i, nm in enumerate(('Malloc', 'Realloc', 'Free')):
            self.write32(self.gmalloc_vt + 4 * i, self.new_stub('fn:' + nm))
        for e in self.pe.DIRECTORY_ENTRY_IMPORT:
            for imp in e.imports:
                nm = imp.name.decode() if imp.name else ''
                if DATA_RE.match(nm) and 'GL2GameData' not in nm and not nm.startswith('?GMalloc'):
                    a = self.alloc(16)
                    self.uc.mem_write(a, bytes(16))
                    self.stubs.pop(self.read32(imp.address), None)
                    self.write32(imp.address, a)
                    self.vars[nm] = a
        # C4: FL2GameData's SystemMsg table sits at +0x722C8 (TipEmu's block is smaller): a block of its own
        self.gdata = self.alloc(0x100000)
        for e in self.pe.DIRECTORY_ENTRY_IMPORT:
            for imp in e.imports:
                if imp.name and b'GL2GameData' in imp.name:
                    self.write32(imp.address, self.gdata)
        self.heap_mark = self.heap_ptr

    # ---- plumbing ---------------------------------------------------------------------------------
    def new_stub(self, name):
        if name in self.stub_by_name:
            return self.stub_by_name[name]
        a = STUB_BASE + 0xA000 + 16 * self.nstub
        self.nstub += 1
        self.uc.mem_write(a, b'\xC3')
        self.stubs[a] = (name, None)
        self.stub_by_name[name] = a
        return a

    def stock_va(self, va):
        return va if self.k == 'stock' else NWBASE + (va - 0x10000000)

    def log_va(self, stock_va):
        """our function for a hooked stock address, from the smoke log (the map file misses some symbols)"""
        for line in open(ROOT + '\\build\\smoke_system\\NWindowDecomp.log', encoding='utf-8', errors='replace'):
            m = re.match(r"^\S+\s+([0-9a-f]{8}) (.+?)\s+ours=([0-9a-f]+) ", line)
            if m and int(m.group(1), 16) == stock_va:
                return self.base + int(m.group(3), 16)
        raise RuntimeError('not in the hook log: %x' % stock_va)

    def sym(self, pre):
        c = [v for s, v in self.symbols.items() if s.startswith(pre)]
        assert len(c) == 1, (pre, c)
        return c[0]

    def patch(self, stub_name, stock_va, ours_sym=None):
        """the stock function at stock_va (our symbol / the stock address inside our NWBASE image) becomes a stub"""
        st = self.new_stub('fn:' + stub_name)
        code = b'\x68' + struct.pack('<I', st) + b'\xC3'
        if self.k == 'stock':
            self.uc.mem_write(stock_va, code)
        elif ours_sym:
            self.uc.mem_write(self.sym(ours_sym), code)
        else:
            self.uc.mem_write(NWBASE + (stock_va - 0x10000000), code)
        return st

    def patch_import_thunk(self, imp_name, thunk_va, pop):
        """ours: the stock import thunk (jmp [IAT]) at thunk_va inside the NWBASE image becomes a stub of the import"""
        if self.k == 'ours' and not sa.ENABLED:       # (the standalone calls the imports through its own IAT: the stubs of install_imports)
            st = self.new_stub('imp:' + imp_name)
            self.uc.mem_write(NWBASE + (thunk_va - 0x10000000), b'\x68' + struct.pack('<I', st) + b'\xC3')
        stubs.IMPORT_POP[imp_name] = pop

    def V(self, name):
        ks = [k for k in self.vars if k.startswith('?' + name + '@@')]
        assert len(ks) == 1, (name, ks)
        return self.vars[ks[0]]

    def vset(self, name, v, fmt='<i'):
        self.uc.mem_write(self.V(name), struct.pack(fmt, v))

    def vget(self, name, fmt='<i'):
        return struct.unpack(fmt, bytes(self.uc.mem_read(self.V(name), 4)))[0]

    def new_obj(self, size=0x400, vt=None):
        p = self.alloc(size)
        self.uc.mem_write(p, bytes(size))
        if vt is not None:
            self.write32(p, vt)
        return p

    def new_vtable(self, slots):
        """slots: {offset: stub name}"""
        vt = self.alloc(0x400)
        self.uc.mem_write(vt, bytes(0x400))
        for off, nm in slots.items():
            self.write32(vt + off, self.new_stub('fn:' + nm))
        return vt

    def reset(self):
        self.heap_ptr = self.heap_mark
        self.log.clear()

    # ---- std::wstring in emulated memory ------------------------------------------------------------
    def ws_make(self, base, s, slack=0):
        self.uc.mem_write(base, bytes(0x1C))
        n = len(s)
        if n < 8 and slack < 8:
            self.uc.mem_write(base + 4, s.encode('utf-16-le') + b'\0\0')
            self.write32(base + 0x14, n)
            self.write32(base + 0x18, 7)
        else:
            cap = max(n + slack, 8)
            p = self.alloc(2 * (cap + 1))
            self.uc.mem_write(p, s.encode('utf-16-le') + b'\0\0')
            self.write32(base + 4, p)
            self.write32(base + 0x14, n)
            self.write32(base + 0x18, cap)

    def ws_read(self, base):
        n, res = self.read32(base + 0x14), self.read32(base + 0x18)
        p = base + 4 if res < 8 else self.read32(base + 4)
        return bytes(self.uc.mem_read(p, 2 * n)).decode('utf-16-le', errors='replace')

    # ---- services -----------------------------------------------------------------------------------
    def do_import(self, nm):
        t = self.this()
        a = self.arg
        if nm == '?appMemset@@YAXPAXHH@Z':
            n = self.rds32(self.reg(UC_X86_REG_ESP) + 12)
            self.uc.mem_write(a(0), bytes([self.rds32(self.reg(UC_X86_REG_ESP) + 8) & 0xFF]) * n)
            return None
        if nm == '?appSprintf@@YAHPAGPBGZZ':
            dst, fmt = a(0), self.wstr(a(1))
            out = []
            ai = 2
            i = 0
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
                ty = fmt[i]
                i += 1
                if ty == 's':
                    v = a(ai)
                    ai += 1
                    out.append(self.wstr(v) if v else '(null)')
                elif ty in 'di':
                    v = self.rds32(self.reg(UC_X86_REG_ESP) + 4 + 4 * ai)
                    ai += 1
                    out.append(('%' + spec.replace('l', '') + 'd') % v)
                elif ty == 'f':
                    lo, hi = a(ai), a(ai + 1)
                    ai += 2
                    d = struct.unpack('<d', struct.pack('<II', lo, hi))[0]
                    out.append('%f' % d)
                elif ty == '%':
                    out.append('%')
                else:
                    raise RuntimeError('format ' + fmt)
            s = ''.join(out)
            self.write_wstr_at(dst, s)
            self.log.append(('appSprintf', fmt, s))
            return len(s)
        if nm == '?appStrcpy@@YAPAGPAGPBG@Z' and not a(1):
            # a NULL source (the party words ask GetPartyMemberName after copying: the real appStrcpy would fault): logged, nothing copied
            self.log.append(('appStrcpy(NULL)',))
            return a(0)
        if nm == '?appMemmove@@YAPAXPAXPBXH@Z':
            d, sr, n = a(0), a(1), self.rds32(self.reg(UC_X86_REG_ESP) + 12)
            if n > 0:
                self.uc.mem_write(d, bytes(self.uc.mem_read(sr, n)))
            return d
        if nm == '?appStricmp@@YAHPBG0@Z':
            x, y = self.wstr(a(0)).lower(), self.wstr(a(1)).lower()
            return u32((x > y) - (x < y))
        if nm == '?appAtoi@@YAHPBG@Z':
            m = re.match(r'\s*([-+]?\d+)', self.wstr(a(0)))
            return u32(int(m.group(1)) if m else 0)
        if nm == '?appStrncpy@@YAPAGPAGPBGH@Z':
            src, mx = self.wstr(a(1)), self.rds32(self.reg(UC_X86_REG_ESP) + 12)
            if mx > 0:
                buf = (src[:mx] + chr(0) * mx)[:mx - 1] + chr(0)
                self.uc.mem_write(a(0), buf.encode('utf-16-le'))
            return a(0)
        if nm == '?Logf2@FOutputDevice@@QAAXPBGZZ':
            fmt = self.wstr(a(0))
            s = fmt.replace('%s', self.wstr(a(1)))
            self.log.append(('Logf2', s))
            return None
        if nm == 'FindFirstFileW':
            names = self.sc['files']
            self.log.append(('FindFirst', self.wstr(a(0))))
            if not names:
                return 0xFFFFFFFF
            self.sc['fi'] = 0
            self.uc.mem_write(a(1) + 0x2C, names[0].encode('utf-16-le') + b'\0\0')
            return 0x1234
        if nm == 'FindNextFileW':
            self.sc['fi'] += 1
            self.log.append(('FindNext', a(0)))
            if self.sc['fi'] >= len(self.sc['files']):
                return 0
            self.uc.mem_write(a(1) + 0x2C, self.sc['files'][self.sc['fi']].encode('utf-16-le') + b'\0\0')
            return 1
        if nm == 'WSAStartup':
            self.log.append(('WSAStartup', a(0)))
            return 0
        if nm == 'socket':
            self.log.append(('socket', a(0), a(1), a(2)))
            return u32(self.sc['socket'])
        if nm == 'inet_addr':
            h = bytes(self.uc.mem_read(a(0), 64)).split(b'\0')[0]
            self.log.append(('inet_addr', h))
            return u32(self.sc['inet'])
        if nm == 'gethostbyname':
            h = bytes(self.uc.mem_read(a(0), 64)).split(b'\0')[0]
            self.log.append(('gethostbyname', h))
            if not self.sc['host']:
                return 0
            ip = self.alloc(8)
            self.uc.mem_write(ip, bytes(self.sc['host']))
            lst = self.alloc(8)
            self.write32(lst, ip)
            ent = self.alloc(32)
            self.uc.mem_write(ent, bytes(32))
            self.uc.mem_write(ent + 0xA, struct.pack('<h', 4))
            self.write32(ent + 0xC, lst)
            return ent
        if nm == 'htons':
            v = a(0) & 0xFFFF
            self.log.append(('htons', v))
            return ((v & 0xFF) << 8) | (v >> 8)
        if nm == 'connect':
            self.log.append(('connect', a(0), bytes(self.uc.mem_read(a(1), 8)), a(2)))
            return u32(self.sc['connect'])
        if nm == 'WSAGetLastError':
            self.log.append(('WSAGetLastError',))
            return 0
        if nm == 'send':
            self.log.append(('send', a(0), a(2), a(3), bytes(self.uc.mem_read(a(1), max(0, a(2))))[:64].hex()))
            return u32(self.sc['send'])
        if nm == 'recv':
            self.sc['nrecv'] = self.sc.get('nrecv', 0) + 1
            if self.sc['nrecv'] > 30:
                raise RuntimeError('too many recv')
            chunks = self.sc['chunks']
            ch = chunks[min(self.sc['nrecv'] - 1, len(chunks) - 1)] if chunks else b''
            n = self.rds32(self.reg(UC_X86_REG_ESP) + 12)
            if isinstance(ch, int):
                self.log.append(('recv', a(0), a(2) - self.sc['reply'], n, ch))
                return u32(ch)
            ch = ch[:max(0, n)]
            self.uc.mem_write(a(1), ch)
            self.log.append(('recv', a(0), a(1) - self.sc['reply'], n, len(ch)))
            return len(ch)
        return super().do_import(nm)

    def do_fn(self, nm):
        a = self.arg
        t = self.this()
        if nm == 'Malloc':
            n = a(0)
            p = self.alloc(n)
            self.uc.mem_write(p, bytes([GARBAGE]) * n)
            self.log.append(('Malloc', n, self.wstr(a(1)) if self.sc.get('tagged') and a(1) else None))
            return p
        if nm == 'Free':
            self.log.append(('Free',))
            return None
        if nm == 'Realloc':
            raise RuntimeError('GMalloc::Realloc')
        if nm == 'FsFileSize':
            p = self.wstr(a(0))
            self.log.append(('FileSize', p))
            f = self.sc['fs'].get(p)
            return u32(len(f) if f is not None else -1)
        if nm == 'FsReader':
            p = self.wstr(a(0))
            self.log.append(('Reader', p, a(1)))
            f = self.sc['fs'].get(p)
            if f is None or self.sc.get('noreader'):
                return 0
            obj = self.new_obj(0x20, self.sc['rd_vt'])
            self.sc['cur'] = f
            return obj
        if nm == 'RdSerialize':
            n = a(1)
            self.log.append(('RdSerialize', n))
            self.uc.mem_write(a(0), self.sc['cur'][:n])
            return None
        if nm == 'RdDtor':
            self.log.append(('RdDtor', a(0)))
            return None
        if nm == 'FsWriter':
            self.log.append(('Writer', self.wstr(a(0)), a(1)))
            if self.sc.get('nowriter'):
                return 0
            return self.new_obj(0x20, self.sc['wr_vt'])
        if nm == 'WrSerialize':
            n = a(1)
            self.log.append(('WrSerialize', n, bytes(self.uc.mem_read(a(0), n)).decode('utf-16-le', errors='replace')))
            return None
        if nm == 'WrFlush':
            self.log.append(('WrFlush',))
            return 1
        if nm == 'WrDtor':
            self.log.append(('WrDtor', a(0)))
            return None
        if nm == 'ShowPage':
            self.log.append(('ShowPage', self.sc.get('names', {}).get(t), self.wstr(a(0))))
            return None
        if nm == 'LoadHtml':
            self.log.append(('LoadHtml', self.sc.get('names', {}).get(t), self.wstr(a(0))))
            return 1
        if nm == 'AddItem':
            self.log.append(('AddItem', self.sc.get('names', {}).get(t), self.wstr(a(0))))
            return None
        if nm == 'LogFlush':
            self.log.append(('LogFlush',))
            return None
        if nm == 'SendReq':
            ln, req, rep = a(0), a(1), a(2)
            width, cnt = self.rds32(req), self.rds32(req + 4)
            txt = bytes(self.uc.mem_read(req + 8, max(0, 2 * cnt))).decode('utf-16-le', errors='replace') if 0 <= cnt < 2000 else '?'
            self.log.append(('SendRequest', ln, width, cnt, txt))
            if self.sc.get('sendfail'):
                return 0
            ans = self.sc['answer']
            self.write32(rep, self.sc.get('anscount', len(ans)))
            self.uc.mem_write(rep + 4, ans.encode('utf-16-le'))
            return 1
        return super().do_fn(nm)


def run_pair(case, fn, kinds=('stock', 'ours')):
    res = {}
    for kind in kinds:
        e = W3Emu(kind) if kind not in EMUS else EMUS[kind]
        EMUS[kind] = e
        e.reset()
        try:
            res[kind] = fn(e, case)
        except (RuntimeError, AttributeError) as ex:
            if os.environ.get('W3DBG'):
                print(kind, 'EXC', ascii(str(ex)[:300]))
            res[kind] = ('EXC', re.sub(r'eip=[0-9a-f]+', 'eip=X', str(ex))[:60])
    return res


EMUS = {}


def first_diff(a, b, path=''):
    if isinstance(a, (tuple, list)) and isinstance(b, (tuple, list)):
        for i, (x, y) in enumerate(zip(a, b)):
            if x != y:
                return first_diff(x, y, path + '[%d]' % i)
        if len(a) != len(b):
            return (path, 'len', len(a), len(b), a[len(b):][:3] if len(a) > len(b) else None, b[len(a):][:3] if len(b) > len(a) else None)
    return (path, a, b)


def report(name, cases, outs, show=3):
    bad = 0
    exc = 0
    same = 0
    for i, c in enumerate(cases):
        a, b = outs['stock'][i], outs['ours'][i]
        if a[0] == 'EXC' and a == b and (a[1].startswith('unicorn error Unhandled CPU exception') or a[1].startswith('too many recv')):
            same += 1          # both sides fault alike (a CPU fault or a loop of the stock code that ours reproduces): noted, not a difference
            continue
        if a[0] == 'EXC' or b[0] == 'EXC':
            exc += 1           # anything else that raised (an unstubbed import ...) is a harness error: counted
        if a != b:
            bad += 1
            if bad <= show:
                print('case', i, ascii(c)[:600])
                print(' first diff', ascii(first_diff(a, b))[:700])
                print(' stock', ascii(a)[:900])
                print(' ours ', ascii(b)[:900])
    if same:
        print('t_w3ui %s: %d of the %d cases fault the same way in the stock and in ours (e.g. the stock loops on a connection that gives nothing; not counted)' % (name, same, len(cases)))
    print('t_w3ui %s cases %d  differ %d  raised %d' % (name, len(cases), bad, exc))
    return bad


def collect(cases, fn):
    outs = {'stock': [], 'ours': []}
    for c in cases:
        r = run_pair(c, fn)
        outs['stock'].append(r['stock'])
        outs['ours'].append(r['ours'])
    return outs


# ================================================================================================
#  L2WordWrapperWnd
# ================================================================================================
WW = {'stock': {'convert': 0x10043200, 'wrap': 0x10042DF0, 'send': 0x10042520, 'conn': 0x10042450, 'load': 0x100436C0,
                'write': 0x10042890, 'gen': 0x10043840}}      # C4 (C2: 0x1003F030 .. 0x1003FFF0)
# the window's fields moved with NCWnd (+0x54): ConvertedCount 0xF0 (C2 0x9C), Socket 0xF4 (0xA0), Document 0xF8 (0xA4), WrapCursor 0x114 (0xC0), size 0x118
WW_SOCK, WW_DOC, WW_CURSOR, WW_COUNT, WW_SIZE = 0xF4, 0xF8, 0x114, 0xF0, 0x118
WW_SYM = {'convert': '?ConvertDocument@L2WordWrapperWnd@@', 'wrap': '?WrapText@L2WordWrapperWnd@@',
          'send': '?SendRequest@L2WordWrapperWnd@@', 'conn': '?ConnectServer@L2WordWrapperWnd@@',
          'load': '?LoadInputFile@L2WordWrapperWnd@@', 'write': '?WriteOutputFile@L2WordWrapperWnd@@',
          'gen': '?OnGenerate@L2WordWrapperWnd@@'}


def ww_addr(e, k):
    return WW['stock'][k] if e.k == 'stock' else e.sym(WW_SYM[k])


def new_wnd(e, doc='', slack=0, cursor=0, sock=0):
    w = e.new_obj(WW_SIZE)
    e.write32(w + WW_SOCK, sock)
    e.ws_make(w + WW_DOC, doc, slack)
    e.write32(w + WW_CURSOR, cursor)
    return w


ALPHA = ['a', 'b', 'c', 'd', 'e', ' ', 'x', 'y', 'z', ' ', 'ก', 'ข', '1', '2', '.', '>', '/']


def rand_page(r, n=14):
    parts = []
    for _ in range(r.randint(0, n)):
        k = r.random()
        if k < 0.20:
            parts.append('<font color=%06x>' % r.randrange(1 << 24))
        elif k < 0.32:
            parts.append('</font>')
        elif k < 0.44:
            parts.append('<a href=%d>' % r.randrange(100))
        elif k < 0.54:
            parts.append('</a>')
        elif k < 0.66:
            parts.append('<br1>')
        else:
            parts.append(''.join(r.choice(ALPHA) for _ in range(r.randint(1, 8))))
    return ''.join(parts)


def t_convert(n, seed):
    r = random.Random(seed)
    cases = [{'doc': rand_page(r), 'slack': r.choice([0, 0, 5, 40])} for _ in range(n)]

    def fn(e, c):
        w = new_wnd(e, c['doc'], c['slack'])
        e.call(ww_addr(e, 'convert'), this=w)
        return (e.ws_read(w + WW_DOC), e.read32(w + WW_CURSOR))
    return report('convert', cases, collect(cases, fn))


def rand_wrap_case(r):
    T = ''.join(r.choice(ALPHA[:-3]) for _ in range(r.randint(1, 30)))
    # the page: T with tags between characters, a prefix and a suffix
    page = ''
    pre = ''.join(r.choice(ALPHA[:-3]) for _ in range(r.randint(0, 6))) if r.random() < 0.5 else ''
    for ch in T:
        if r.random() < 0.18:
            page += r.choice(['<font color=ff00ff>', '</font>', '<a href=1>', '</a>', '<font size=2>', '<b>'])
        page += ch
    page = pre + page + (''.join(r.choice(ALPHA[:-3]) for _ in range(r.randint(0, 5))) if r.random() < 0.3 else '')
    # the answer: T with line breaks
    ans = ''
    for ch in T:
        if r.random() < 0.12:
            ans += '<br1>'
        ans += ch
    if r.random() < 0.2:
        ans += '<br1>' + ''.join(r.choice(ALPHA[:-3]) for _ in range(r.randint(0, 4)))
    if r.random() < 0.1:
        ans = ans.replace('a', 'q', 1)
    return {'T': T, 'page': page, 'ans': ans, 'cursor': r.choice([0, 0, 0, r.randint(0, len(page) + 2)]),
            'width': r.randint(10, 400), 'sendfail': r.random() < 0.06, 'slack': r.choice([0, 6, 60]),
            'nullcount': r.random() < 0.04}


def norm_log(log):
    """the order of the allocator calls differs (ours frees a temporary at the end of its scope, the stock before the next call):
    compare the other calls in order, the allocation sizes sorted and the number of frees"""
    other = [x for x in log if x[0] not in ('Malloc', 'Free')]
    return (other, sorted(x[1] for x in log if x[0] == 'Malloc'), sum(1 for x in log if x[0] == 'Free'))


def t_wrap(n, seed):
    r = random.Random(seed)
    cases = [rand_wrap_case(r) for _ in range(n)]

    def fn(e, c):
        w = new_wnd(e, c['page'], c['slack'], c['cursor'], sock=1)
        e.sc = {'answer': c['ans'], 'sendfail': c['sendfail']}
        if c['nullcount']:
            e.sc['anscount'] = 256
        e.patch('SendReq', 0x10042520, '?SendRequest@L2WordWrapperWnd@@')
        txt = e.put_wstr(c['T'])
        e.call(ww_addr(e, 'wrap'), this=w, args=[txt, c['width']])
        return (e.ws_read(w + WW_DOC), e.read32(w + WW_CURSOR), norm_log(e.log))
    return report('wrap', cases, collect(cases, fn))


def t_send(n, seed):
    r = random.Random(seed)
    cases = []
    for _ in range(n):
        cnt = r.randint(0, 40)
        body = struct.pack('<I', cnt) + bytes(r.randrange(256) for _ in range(2 * cnt))
        tot = len(body)
        k = r.randint(1, 4)
        cuts = sorted(r.sample(range(1, tot), min(k - 1, tot - 1))) if tot > 1 else []
        chunks = [body[a:b] for a, b in zip([0] + cuts, cuts + [tot])]
        if r.random() < 0.15:
            chunks.insert(r.randint(0, len(chunks)), r.choice([0, -1]))
        if r.random() < 0.1:
            chunks = [bytes(r.randrange(256) for _ in range(r.randint(1, 3000))) for _ in range(3)]
        cases.append({'chunks': chunks, 'send': r.choice([5, 5, 5, 0, -1]), 'sock': r.choice([0, 1, 7]),
                      'nodata': r.random() < 0.05, 'noreply': r.random() < 0.05, 'len': r.randint(8, 60)})

    def fn(e, c):
        w = new_wnd(e, '', 0, 0, sock=c['sock'])
        e.patch_import_thunk('send', 0x10170B32, 16)
        e.patch_import_thunk('recv', 0x10170B2C, 16)
        reply = e.alloc(0x1000)
        e.uc.mem_write(reply, bytes([0x55]) * 0x1000)
        req = e.alloc(0x100)
        e.uc.mem_write(req, bytes(range(256)))
        msg = e.new_obj(0x200)
        e.write32(e.console + 0x353C, msg)
        e.patch('SetTextStub', 0x100987A0) if False else None
        e.sc = {'send': c['send'], 'chunks': c['chunks'], 'reply': reply}
        ret = e.call(ww_addr(e, 'send'), this=w, args=[c['len'], 0 if c['nodata'] else req, 0 if c['noreply'] else reply]) & 0xFF
        return (ret, bytes(e.uc.mem_read(reply, 0x1000)), list(e.log), e.fs_get(msg + 0x9C))
    return report('send', cases, collect(cases, fn))


def t_connect(n, seed):
    r = random.Random(seed)
    cases = [{'socket': r.choice([-1, 5, 9]), 'inet': r.choice([0x0100007F, -1, -1]), 'host': r.choice([None, [10, 0, 0, 7]]),
              'connect': r.choice([0, 0, -1]), 'name': r.choice(['localhost', '127.0.0.1', 'example']), 'port': r.choice([7778, 80, 70000])}
             for _ in range(n)]

    def fn(e, c):
        w = new_wnd(e, '', 0, 0)
        for nm, va, pop in (('WSAStartup', 0x10170B26, 8), ('socket', 0x10170B20, 12), ('inet_addr', 0x10170B1A, 4),
                            ('gethostbyname', 0x10170B14, 4), ('htons', 0x10170B0E, 4), ('connect', 0x10170B08, 12),
                            ('WSAGetLastError', 0x10170B02, 0)):       # the stock's import thunks (C2: 0x100E71F2 ..)
            e.patch_import_thunk(nm, va, pop)
        e.sc = {'socket': c['socket'], 'inet': c['inet'], 'host': c['host'], 'connect': c['connect']}
        host = bytes(c['name'].encode('ascii') + b'\0')
        hp = e.alloc(64)
        e.uc.mem_write(hp, host)
        ret = e.call(ww_addr(e, 'conn'), this=w, args=[hp, c['port']]) & 0xFF
        return (ret, e.read32(w + WW_SOCK), list(e.log))
    return report('connect', cases, collect(cases, fn))


def fs_objects(e, files):
    """GFileManager with the files {path: bytes}, GNull, the reader / writer vtables"""
    fm = e.new_obj(0x40, e.new_vtable({0x08: 'FsReader', 0x0C: 'FsWriter', 0x10: 'FsFileSize'}))
    e.write32(e.V('GFileManager'), fm)
    e.write32(e.V('GNull'), e.new_obj(0x20))
    e.sc['fs'] = files
    e.sc['rd_vt'] = e.new_vtable({0x00: 'RdDtor', 0x04: 'RdSerialize'})
    e.sc['wr_vt'] = e.new_vtable({0x00: 'WrDtor', 0x04: 'WrSerialize', 0x54: 'WrFlush'})      # Flush is slot 21 in C4 (C2: 19)
    e.patch_fs = True


def rand_file(r):
    s = ''.join(r.choice(['a', 'b', ' ', ' ', '  ', '<br1>', 'ก', '\n']) for _ in range(r.randint(0, 30)))
    return s


def t_load(n, seed):
    r = random.Random(seed)
    cases = [{'file': rand_file(r), 'doc': r.choice(['', 'old doc text']), 'noreader': r.random() < 0.08, 'missing': r.random() < 0.05}
             for _ in range(n)]

    def fn(e, c):
        e.sc = {'tagged': True, 'noreader': c['noreader']}
        path = '..\\INPUT_THAI_HTML\\x.html'
        fs_objects(e, {} if c['missing'] else {path: c['file'].encode('utf-16-le')})
        w = new_wnd(e, c['doc'])
        ret = e.call(ww_addr(e, 'load'), this=w, args=[e.put_wstr(path)])
        return (ret & 0xFFFFFFFF, e.ws_read(w + WW_DOC), list(e.log))
    return report('load', cases, collect(cases, fn))


def t_write(n, seed):
    r = random.Random(seed)
    cases = [{'doc': rand_file(r), 'name': r.choice(['a.html', 'b']), 'nowriter': r.random() < 0.1} for _ in range(n)]

    def fn(e, c):
        e.sc = {'nowriter': c['nowriter']}
        fs_objects(e, {})
        w = new_wnd(e, c['doc'], r.choice([0, 9]))
        e.call(ww_addr(e, 'write'), this=w, args=[e.put_wstr(c['name'])])
        return (list(e.log),)
    return report('write', cases, collect(cases, fn))


def t_generate(n, seed):
    r = random.Random(seed)
    cases = []
    for _ in range(n):
        files = ['f%d.html' % i for i in range(r.randint(0, 3))]
        fs = {}
        for f in files:
            if r.random() < 0.8:
                fs['..\\INPUT_THAI_HTML\\' + f] = (rand_page(r, 8) + '  ').encode('utf-16-le')
        cases.append({'files': files, 'fs': fs, 'log': r.random() < 0.7, 'sel': r.choice([0, 0, 1, 2, 3, 3, 4, -1])})

    def fn(e, c):
        e.sc = {'files': c['files'], 'tagged': False}
        fs_objects(e, c['fs'])
        glog = e.new_obj(0x20, e.new_vtable({0x04: 'LogFlush'}))
        e.write32(e.V('GLog'), glog if c['log'] else 0)
        # C4: the page kind is the combo box's Selected (+0x240); 0 the desktop's NPC viewer (+0xF8), 1 the petition window's help viewer
        # (desktop +0x110 -> +0xF0), 2 the help window (+0x100), 3 the server list's page (GodWnd +0x3BE0 -> +0xEC -> +0xF8 -> +0xEC -> +0xEC)
        names = {}

        def obj(name, size=0x200):
            o = e.new_obj(size)
            names[o] = name
            return o
        desk = e.new_obj(0x200)
        e.write32(desk + 0xF8, obj('npc'))
        e.write32(desk + 0x100, obj('help'))
        pet = e.new_obj(0x200)
        e.write32(pet + 0xF0, obj('petition'))
        e.write32(desk + 0x110, pet)
        e.write32(e.console + 0x3BF4, desk)
        god = e.new_obj(0x200)
        login = e.new_obj(0x200)
        server = e.new_obj(0x200)
        info = e.new_obj(0x200)
        e.write32(info + 0xEC, obj('serverinfo'))
        e.write32(server + 0xEC, info)
        e.write32(login + 0xF8, server)
        e.write32(god + 0xEC, login)
        e.write32(e.console + 0x3BE0, god)
        combo = obj('combo', 0x400)
        e.write32(combo + 0x240, u32(c['sel']))
        e.sc['names'] = names
        e.patch('ShowPage', 0x1011E9C0, '?ShowPage@NCNPCHtmlViewer@@')            # NpcHtmlWnd / HelpWnd's (folded in ours too)
        e.patch('LoadHtml', 0x100419D0, '?LoadHtml@NCHtmlViewer@@')
        e.patch('AddItem', 0x100068B0, '?AddItem@NCComboBox@@QAEXPBG@Z')
        w = new_wnd(e, '')
        e.write32(w + 0xE8, combo)                  # PageCombo
        e.write32(w + WW_COUNT, 99)
        ret = e.call(ww_addr(e, 'gen'), this=w)
        return (ret & 0xFFFFFFFF, e.read32(w + WW_COUNT), e.ws_read(w + WW_DOC), e.read32(w + WW_CURSOR), list(e.log))
    return report('generate', cases, collect(cases, fn))


# ================================================================================================
#  the more services: the video options' device, the config, the command tokens
# ================================================================================================
_base_do_fn = W3Emu.do_fn


def _do_fn2(self, nm):
    a = self.arg
    sp = self.reg(UC_X86_REG_ESP)
    if nm == 'GetRes':
        modes = self.sc['modes']
        self.log.append(('GetRes', a(0)))
        for i, (w, h, b) in enumerate(modes):
            self.write32(a(1) + 4 * i, w)
            self.write32(a(2) + 4 * i, h)
            self.write32(a(3) + 4 * i, b)
        return len(modes)
    if nm == 'GetRefresh':
        key = (self.rds32(sp + 8), self.rds32(sp + 12), self.rds32(sp + 16))
        rates = self.sc['rates'].get(key, [60])
        self.log.append(('GetRefresh', a(0), key))
        for i, r_ in enumerate(rates):
            self.write32(a(4) + 4 * i, r_)
        return len(rates)
    if nm == 'SetRes':
        args = [self.rds32(sp + 8 + 4 * i) for i in range(6)]
        self.log.append(('SetRes', args))
        q = self.sc['setres']
        return q.pop(0) if q else 0
    if nm == 'DelAllTex':
        self.log.append(('DeleteAllTexture',))
        return None
    if nm == 'IsTrilinear':
        self.log.append(('IsTrilinear',))
        return self.sc['trilinear']
    if nm == 'SetTrilinear':
        self.log.append(('SetTrilinear', a(0)))
        return None
    if nm == 'UpdateGamma':
        self.log.append(('UpdateGamma',))
        return None
    if nm == 'IsFullscreen':
        self.log.append(('IsFullscreen',))
        return self.sc['fullscreen']
    if nm == 'ConfigSet':
        self.log.append(('SetString', self.wstr(a(0)), self.wstr(a(1)), self.wstr(a(2)), self.wstr(a(3))))
        return None
    if nm == 'ConfigGetFloat':
        key = self.wstr(a(1))
        self.log.append(('GetFloat', self.wstr(a(0)), key, self.wstr(a(3))))
        v = self.sc['floats'].get(key)
        if v is None:
            return 0
        self.uc.mem_write(a(2), struct.pack('<f', v))
        return 1
    if nm == 'InitModes':
        self.log.append(('InitModes',))
        return None
    if nm == 'CurSet':
        self.log.append(('SetCursor', a(0), a(1)))
        return None
    if nm == 'CurLock':
        self.log.append(('LockCursor',))
        return None
    if nm == 'CurUnlock':
        self.log.append(('UnlockCursor',))
        return None
    if nm == 'GetSelected':
        self.log.append(('GetSelectedActor',))
        return self.sc['target']
    if nm == 'GetMyUser':
        self.log.append(('GetMyUser',))
        return self.sc['me']
    if nm == 'GetPet':
        self.log.append(('GetPetInfo',))
        self.uc.mem_write(a(0), self.sc['pet'])
        return a(0)
    if nm == 'GetParty':
        k = a(0)
        self.log.append(('Party', k))
        return self.sc['party'].get(k, 0)
    return _base_do_fn(self, nm)


W3Emu.do_fn = _do_fn2


# ---- 0x1004b8e0 (C2 0x1003e5b0): the "msgid;argument" expander -----------------------------------------------------
def t_sysmsg(n, seed):
    r = random.Random(seed)
    cases = []
    for _ in range(n):
        msgs = {}
        for k in range(1, 6):
            msgs[k] = r.choice(['', 'Hello', 'You got $s1 now', 'a $s1 b $s1', '$s1', '$s', 'x$s1', 'The siege of $s1 begins!' * r.randint(1, 3)])
        mid = r.choice([0, 1, 2, 3, 4, 5, 5, 6, -2])
        arg = ''.join(r.choice('abc ;12') for _ in range(r.randint(0, 12)))
        text = '%d;%s' % (mid, arg) if r.random() < 0.9 else '%d' % mid
        if text[:-1].find(';') < 0:
            # no ';' before the last character: the stock reads an uninitialised argument, so no $s1 is allowed to be replaced
            msgs = {k: m.replace('$s1', 'S1') for k, m in msgs.items()}
        cases.append({'msgs': msgs, 'text': text})

    def fn(e, c):
        for k in range(0, 8):
            base = e.gdata + 0x722C8 + 0x30 * k        # FL2GameData::SystemMsg[k] (0x30 bytes, Message at +4)
            e.uc.mem_write(base, bytes(0x30))
            e.fs_set(base + 4, c['msgs'].get(k, ''))
        buf = e.alloc(0x800)
        e.uc.mem_write(buf, bytes([0x77]) * 0x800)
        e.write_wstr_at(buf, c['text'])
        va = 0x1004B8E0 if e.k == 'stock' else e.sym('?ExpandSystemMsgCode@@')
        e.call(va, this=None, args=[buf])
        return (e.wstr(buf),)
    return report('sysmsg', cases, collect(cases, fn))


# ---- 0x1004bc30 (C2 0x1003e900): %target / %self / %pet / %party1..8 -------------------------------------------------
def t_token(n, seed):
    r = random.Random(seed)
    toks = ['%target', '%self', '%pet', '%party1', '%party2', '%party3', '%party4', '%party5', '%party6', '%party7', '%party8',
            '%TARGET', '%Pet', '%party9', 'hello', '', '%']
    cases = []
    for _ in range(n):
        cases.append({'tok': r.choice(toks), 'target': r.choice(['', 'Ana', 'Bob']), 'me': r.choice(['', 'Zed', 'Me']),
                      'pet': r.choice(['', 'Rex']), 'party': {k: r.choice(['', 'P%d' % k]) for k in range(8)},
                      'null': r.random() < 0.05})

    def fn(e, c):
        def user(name):
            if name == '':
                return 0
            u = e.new_obj(0x100)
            e.write_wstr_at(u + 0x1C, name)
            return u
        pet = bytearray(0xB4)            # PetInfo: 45 words in C4 (C2: 0xAC)
        pet[8:8 + 2 * len(c['pet'])] = c['pet'].encode('utf-16-le')
        party = {}
        for k, nm in c['party'].items():
            party[k] = e.put_wstr(nm) if nm != '' else 0
        e.sc = {'target': user(c['target']), 'me': user(c['me']), 'pet': bytes(pet), 'party': party}
        e.patch('GetSelected', 0x10073ED0, '?GetSelectedActor@NConsoleWnd@@')
        e.patch('GetMyUser', 0x10073E80, '?GetMyUser@NConsoleWnd@@')
        e.patch('GetPet', 0x1006A750, '?GetPetInfo@NConsoleWnd@@')
        e.patch('GetParty', 0x10065AF0, '?GetPartyMemberName@NConsoleWnd@@')
        if e.k == 'ours' and not sa.ENABLED:
            # the hook build reads the table of the eleven words (0x10238900) from the stock image (nw::Addr): put it there (the
            # standalone has its own table: GCommandTokenNames)
            for i, w_ in enumerate(['%target', '%self', '%pet'] + ['%%party%d' % j for j in range(1, 9)]):
                e.write32(NWBASE + 0x238900 + 4 * i, e.static_str(w_))
        buf = e.alloc(0x400)
        e.uc.mem_write(buf, bytes([0x77]) * 0x400)
        e.write_wstr_at(buf, c['tok'])
        va = 0x1004BC30 if e.k == 'stock' else e.sym('?ExpandToken@NCommandParser@@')
        ret = e.call(va, this=e.alloc(0x418), args=[0 if c['null'] else buf])
        return (ret & 0xFFFFFFFF, e.wstr(buf), list(e.log))
    return report('token', cases, collect(cases, fn))


# ---- 0x100a1b90: NCVideoOptionWnd::Apply -----------------------------------------------------------
INT_VARS = ['GL2KeepMinFrameRate', 'GL2TextureDetail', 'GL2UseTrilinear', 'GL2UseReducePawnLOD', 'GL2UseSkipAnim', 'GSEKScreenX',
            'GSEKScreenY', 'GColorBits', 'GRefreshRate', 'GL2RenderDeco', 'GIsL2Projector', 'GL2UseColorCursor', 'GIsL2WaterEffect',
            'GIsL2WaterEffectType', 'GL2RenderActorLimitOpt', 'GL2RenderActorLimit']
FLT_VARS = ['GL2OriginPawnCR', 'GL2OriginTerrainCR', 'GL2OriginStaticMeshCR', 'GL2OriginStaticMeshLodCR', 'GL2ActorCR', 'GL2FogStart',
            'GL2FogEnd']
MODES = [(640, 480, 16), (800, 600, 16), (800, 600, 32), (1024, 768, 32), (1280, 1024, 32), (1024, 768, 16)]


def ww_apply(e):
    return 0x100A1B90 if e.k == 'stock' else e.sym('?Apply@NCVideoOptionWnd@@')


def t_apply(n, seed):
    # stale: NCVideoOptionWnd::Apply (C4 0x100C5E70, 3,591 bytes) is proved by win/t_video.py (op `apply`) on the pw.py world, which models the C4
    # window (the C2 model below has its own device / config / combo layout). Left in place as the record of the C2 test.
    import sa as _sa
    _sa.skip('NCVideoOptionWnd::Apply', 'stale C2 model; covered by win/t_video.py apply')
    return 0
    r = random.Random(seed)
    cases = []
    for _ in range(n):
        modes = r.sample(MODES, r.randint(1, 6))
        cur = r.choice(modes + [(1024, 768, 32)])
        cases.append({
            'modes': modes, 'cur': cur, 'rate': r.choice([60, 75, 85]),
            'rates': {(w, h, b): r.sample([60, 75, 85, 100], r.randint(1, 4)) for (w, h, b) in MODES},
            'sel': {'res': r.randint(-1, len(modes)), 'ref': r.randint(-1, 4), 'gamma': r.randint(0, 5), 'tex': r.randint(0, 3),
                    'model': r.randint(0, 1), 'anim': r.randint(0, 1), 'terrain': r.randint(0, 5), 'pawn': r.randint(0, 5),
                    'refl': r.randint(0, 2), 'limit': r.randint(0, 6)},
            'box': {k: r.randint(0, 1) for k in ('shadow', 'ground', 'cursor', 'lower', 'filter')},
            'ints': {k: r.choice([0, 1, 2, 3, 5]) for k in INT_VARS},
            'flts': {k: r.choice([0.5, 1.0, 1.5, 2.0, 3.0, 4.0, 5.0, 5.5, 6.0, 7.0, 8.0]) for k in FLT_VARS},
            'gamma': r.choice([0.4, 0.6, 0.8, 1.0, 1.2, 1.5]),
            'trilinear': r.randint(0, 1), 'fullscreen': r.randint(0, 1), 'setres': [r.randint(0, 1) for _ in range(3)],
            'floats': {k: r.choice([None, 0.7, 2.5]) for k in ('StartRange1', 'EndRange1', 'StartRange2', 'EndRange2', 'StartRange3',
                                                                'EndRange3', 'StartRange4', 'EndRange4', 'StartRange5', 'EndRange5')},
            'noclient': r.random() < 0.05,
        })

    def fn(e, c):
        for k, v in c['ints'].items():
            e.vset(k, v)
        for k, v in c['flts'].items():
            e.uc.mem_write(e.V(k), struct.pack('<f', v))
        e.vset('GSEKScreenX', c['cur'][0])
        e.vset('GSEKScreenY', c['cur'][1])
        e.vset('GColorBits', c['cur'][2])
        e.vset('GRefreshRate', c['rate'])
        engine = e.new_obj(0x100, e.new_vtable({0x74: 'UpdateGamma'}))
        client = e.new_obj(0x100)
        vp = e.new_obj(0x100, e.new_vtable({0x78: 'IsFullscreen'}))
        arr = e.alloc(16)
        e.write32(arr, vp)
        e.write32(client + 0x2C, arr)
        e.write32(client + 0x30, 1)
        e.uc.mem_write(client + 0x78, struct.pack('<f', c['gamma']))
        rend = e.new_obj(0x100, e.new_vtable({0x6C: 'SetRes', 0x74: 'GetRes', 0x78: 'GetRefresh', 0xA8: 'DelAllTex', 0xAC: 'IsTrilinear',
                                               0xB0: 'SetTrilinear'}))
        e.write32(engine + 0x4C, 0 if c['noclient'] else client)
        e.write32(engine + 0x54, rend)
        e.write32(e.console + 0x44, engine)
        cfg = e.new_obj(0x100, e.new_vtable({0x30: 'ConfigSet', 0x08: 'ConfigGetFloat'}))
        e.write32(e.V('GConfig'), cfg)
        e.sc = {'modes': c['modes'], 'rates': c['rates'], 'setres': list(c['setres']), 'trilinear': c['trilinear'],
                'fullscreen': c['fullscreen'], 'floats': c['floats']}
        e.patch('InitModes', 0x100A01A0, '?InitModes@NCVideoOptionWnd@@')
        e.patch('CurSet', 0x10056110, '?SetCursor@NConsoleWnd@@')
        e.patch('CurLock', 0x10056140, '?LockCursor@NConsoleWnd@@')
        e.patch('CurUnlock', 0x10056150, '?UnlockCursor@NConsoleWnd@@')
        w = e.new_obj(0xE0)
        for off, key in ((0x98, 'res'), (0x9C, 'ref'), (0xA0, 'gamma'), (0xA4, 'tex'), (0xA8, 'model'), (0xAC, 'anim'),
                         (0xBC, 'pawn'), (0xB8, 'terrain'), (0xC4, 'refl'), (0xD0, 'limit')):
            o = e.new_obj(0x300)
            e.write32(o + 0x1F4, u32(c['sel'][key]))
            e.write32(w + off, o)
        for off, key in ((0xB0, 'shadow'), (0xB4, 'ground'), (0xC0, 'cursor'), (0xC8, 'lower'), (0xCC, 'filter')):
            o = e.new_obj(0x300)
            e.write32(o + 0x1C0, c['box'][key])
            e.write32(w + off, o)
        e.call(ww_apply(e), this=w)
        out = [e.vget(k) for k in INT_VARS] + [struct.unpack('<I', bytes(e.uc.mem_read(e.V(k), 4)))[0] for k in FLT_VARS]
        gm = struct.unpack('<I', bytes(e.uc.mem_read(client + 0x78, 4)))[0]
        return (out, gm, list(e.log))
    return report('apply', cases, collect(cases, fn))


TESTS = {'convert': t_convert, 'wrap': t_wrap, 'send': t_send, 'connect': t_connect, 'load': t_load, 'write': t_write,
         'generate': t_generate, 'sysmsg': t_sysmsg, 'token': t_token, 'apply': t_apply}

if __name__ == '__main__':
    which = sys.argv[1]
    n = int(sys.argv[2])
    seed = int(sys.argv[3])
    if which == 'all':
        for k, f in TESTS.items():
            f(n, seed)
    else:
        TESTS[which](n, seed)
