# Differential test of the plain windows of the system lanes (login screen, game / video / audio option pages, the notice, the map cover,
# the quest list info and the credits windows): every function of the class table below is run stock against ours under unicorn on the
# fake world of pw.py. The window under test is a fake object with the class's stock vtable (every virtual call is logged), its
# fields are filled from the header's `// +0xNN` comments (pointers to fake child windows, small integers, strings), the children
# it creates are fake objects, and everything else it calls is a logging service identified by its stock address.
# Arguments of WindowProc / OnKeyDown / OnTimer ... come from the constants the stock function compares with.
#   python t_sys.py [cases] [seed] [Class::Method ...]     (no names: every entry of TESTS)
import sys, os, random, struct, re, zlib
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from pw import *
import pw
from capstone import Cs, CS_ARCH_X86, CS_MODE_32
from capstone.x86 import X86_OP_IMM

ROOT = fncalls.ROOT
INC = os.path.join(ROOT, 'include/nwindow')
pw.POP_OVERRIDE.update({0x10074e60: 4, 0x1002b420: 12})
ZERO4 = bytes(4)
SMALL = [0, 1, 2, 3, 4, 5, 6, 7, 10, -1, 100, 0x10, 0x13, 0x34, 0x46, 0x50, 0x5E6]
md = Cs(CS_ARCH_X86, CS_MODE_32)
md.detail = True


# ---- the header's fields -----------------------------------------------------------------------------------------------------
_HDR = {}
ARRSIZE = {'NAMEBOX_MAX': 6}      # the enum sizes of the arrays of the headers


def header_fields(cls):
    """[(offset, kind, type name)] of the fields `T Name; // +0xNN` of the class body in include/nwindow/<cls>.h"""
    if cls in _HDR:
        return _HDR[cls]
    out = []
    names = sorted(os.listdir(INC))
    if cls + '.h' in names:
        names.remove(cls + '.h')
        names.insert(0, cls + '.h')
    for fn in names:
        if not fn.endswith('.h'):
            continue
        txt = open(os.path.join(INC, fn), encoding='utf-8', errors='replace').read().split('\n')
        start = None
        for i, ln in enumerate(txt):
            if re.match(r'^(class|struct)\s+%s\b\s*(:|$|\{)' % cls, ln):
                start = i
                break
        if start is None:
            continue
        for ln in txt[start + 1:]:
            if ln.startswith('};'):
                break
            m = re.match(r'^\s*([A-Za-z_][A-Za-z0-9_<>:]*)\s*(\*?)\s*([A-Za-z_][A-Za-z0-9_]*)(\[[^\]]*\])?\s*;\s*//\s*\+0x([0-9A-Fa-f]+)(.*)$', ln)
            if not m:
                continue
            t, star, nm, arr, off, rest = m.groups()
            cnt = 1
            if arr:
                n = arr[1:-1].strip()
                cnt = int(n) if n.isdigit() else ARRSIZE.get(n, 0)      # only the arrays of pointers (NameBoxes[], Keypad[], Icons[] ...)
                if not star or cnt <= 0 or cnt > 32:
                    continue
            off = int(off, 16)
            if star and t in ('NCWnd', 'NControl'):
                for cand in re.findall(r'(NC[A-Za-z0-9]+)', rest):
                    if cand in classes() and cand != t:
                        t = cand
                        break
            if star:
                kind = 'tex' if t == 'UTexture' else ('wnd' if (t in classes() or t.startswith('NC')) else 'obj')
            elif t in ('INT', 'UBOOL', 'DWORD', 'WORD', 'BYTE', 'int', 'LONG', 'ULONG', 'UINT'):
                kind = 'int'
            elif t == 'FLOAT':
                kind = 'float'
            elif t == 'FString':
                kind = 'fstr'
            else:
                continue
            for i in range(cnt):
                out.append((off + 4 * i, kind, t))
        break
    _HDR[cls] = out
    return out


def imm_consts(va, near=True):
    size = fncalls.FUNCS[va]
    vals = set()
    for i in md.disasm(bytes(fncalls.MEM[va - fncalls.BASE:va + size - fncalls.BASE]), va):
        if i.mnemonic in ('cmp', 'sub', 'test', 'and') and i.operands and i.operands[-1].type == X86_OP_IMM:
            v = i.operands[-1].imm & 0xFFFFFFFF
            if not (0x10000000 <= v < 0x10400000 or 0x50000000 <= v < 0x50400000):      # (not an address of the image)
                vals.add(v)
                if near:
                    vals.add((v + 1) & 0xFFFFFFFF)
                    vals.add((v - 1) & 0xFFFFFFFF)
    return sorted(vals)


# what a virtual of the window itself returns in the fake world when it is not 0: NCLoginFunctionWnd slot 78 is OnReplayBtnClick
# (0x1009d6d0), which ends in `mov eax, 1` (ours: `return 1`); WindowProc returns `!= 0` of it in the stock and 1 in ours
# the chains of console pointers that the function tests for null (offsets from the console, then from each object)
CHAINS = {'NGaraConsoleFns::SetMapShowMark': [0x3BF4, 0x170, 0xE8], 'NConsoleWnd::SetServerPing': [0x3BE0, 0xEC, 0xF8, 0xE8]}
VRETS = {'NCLoginFunctionWnd': {78: lambda e, t, a: 1}}


class SYS(PW):
    def __init__(self, kind, funcs, cls, rets=None, skip=(), extra=None, syms=None, aliases=()):
        PW.__init__(self, kind, funcs, extra or {}, syms, skip)
        self.aliases = aliases
        self.nulls_ok = False
        self.cls = cls
        self.vt_win = self.class_vt('NCWnd')
        vr = {41: lambda e, t, a: (e.case['seed'] >> 5) & 1}     # IsShowWindow
        vr.update(VRETS.get(cls, {}))
        self.vt_this = self.class_vt(cls, rets=vr)
        self.fields = header_fields(cls)
        self.child_vt = {}
        self.vt_dev = self.make_vt('dev', {})
        self.parent = self.wnd('parent', self.vt_win, 0x300)
        self.net = self.obj('net', 0x100)
        self.engine = self.obj('engine', 0x1000)
        self.write32(self.console + 0x54, self.engine)
        self.write32(self.engine + 0x64, self.net)
        self.vt_cfg = self.make_vt('cfg', CFG_POPS, {0: self.cfg_get, 1: self.cfg_get, 2: self.cfg_getf, 3: self.cfg_get, 4: self.cfg_get, 5: self.cfg_getstr})
        self.cfgobj = self.obj('cfg', 0x40, self.vt_cfg)
        self.write32(self.gvar('GConfig'), self.cfgobj)
        inc = lambda n: vt_inc_pops(os.path.join(ROOT, 'include/ue2/gen', n))
        def setv(k):
            def f(e, t, a):
                e.volstate[k] = struct.unpack('<f', struct.pack('<I', a[0] & 0xFFFFFFFF))[0]
            return f
        getv = lambda k: (lambda e: e.volstate.get(k, e.case['vol'][k]))
        self.vt_audio = self.make_vt('audio', inc('UAudioSubsystem.vt.inc'), {45: lambda e, t, a: e.case['mp'], 50: lambda e, t, a: 0x4321,
                                                                              36: setv(0), 38: setv(1), 39: setv(2), 40: setv(3)}, nslots=120,
                                     floats={41: getv(0), 42: getv(1), 43: getv(2), 44: getv(3)})
        self.user = self.obj('myuser', 0x400)
        # the file manager (CreateFileReader 2, CreateFileWriter 3, Delete 5, MakeDirectory 10), its archives (dtor 0, Serialize 1,
        # TotalSize 11, Flush 21) and the canvas: the large clan crest reads a .bmp, writes it back, converts it, reads the .dds
        self.vt_fm = self.make_vt('filemgr', {2: 12, 3: 12, 5: 12, 10: 8}, {2: self.fm_reader, 3: self.fm_writer, 5: self.fm_delete, 10: self.fm_mkdir}, nslots=24)
        self.vt_ar = self.make_vt('archive', {0: 4, 1: 8, 11: 0, 21: 0}, {1: self.ar_serialize, 11: self.ar_size}, nslots=40)
        self.fm = self.obj('filemgr', 0x40, self.vt_fm)
        self.write32(self.gvar('GFileManager'), self.fm)
        self.canvas = self.obj('canvas', 0x40)
        self.write32(self.glob(0x102C39F8), self.canvas)
        self.arch = {}
        # the network handler (engine + 0x64): slot 52 GetPartyInfoMap(TMap&) fills the party table of the case (pairs of 0x18 bytes: the
        # hash link, the key, then the INT fields the map cover reads)
        self.vt_net = self.make_vt('net', inc('UL2NetHandler.vt.inc'), {52: self.party_fill}, nslots=260)
        self.write32(self.net, self.vt_net)
        # the engine: UGameEngine::Browse (slot 49) answers from the case (the stock tests its result for the "FailedBrowse" log)
        self.vt_engine = self.make_vt('engine', {**inc('UEngine.vt.inc'), **inc('UGameEngine.vt.inc')}, {49: lambda e, t, a: (e.case['seed'] >> 2) & 1}, nslots=120)
        self.write32(self.engine, self.vt_engine)
        self.vt_rd = self.make_vt('rendev', inc('URenderDevice.vt.inc'), {27: lambda e, t, a: (e.case['seed'] >> 16) & 1}, nslots=70)      # SetRes succeeds or not
        self.vt_vp = self.make_vt('viewport', inc('UViewport.vt.inc'), {}, nslots=70)
        self.audio = self.obj('audio', 0x100, self.vt_audio)
        self.rendev = self.obj('rendev', 0x100, self.vt_rd)
        self.client = self.obj('client', 0x100)
        self.vparr = self.obj('vparr', 16)
        self.vp0 = self.obj('viewport', 0x400, self.vt_vp)
        self.actor = self.obj('actor', 0x400)           # viewport + 0x3C: the player controller, + 0x3A8 its pawn, + 0x1BC the location
        self.pawn = self.obj('pawn', 0x1500)
        self.ctrl = self.obj('ctrl', 0x100, self.make_vt('ctrl', {200: 0}, nslots=260))      # pawn + 0x13F0: its controller (ResetMusicControll is slot 200)
        self.write32(self.engine + 0x58, self.client)
        self.write32(self.engine + 0x5C, self.audio)
        self.write32(self.engine + 0x60, self.rendev)
        self.write32(self.client + 0x38, self.vparr)
        self.write32(self.client + 0x3C, 1)
        self.write32(self.vparr, self.vp0)
        self.vt_con = self.class_vt('NConsoleWnd')
        for (_sym, _log, _pop, _vcls) in aliases:
            if _vcls not in self.child_vt:
                self.child_vt[_vcls] = self.class_vt(_vcls)
        self.write32(self.console, self.vt_con)
        self.child_vt['NConsoleWnd'] = self.vt_con
        if cls == 'NCGaraDialogBox':       # (the stock side allocates the same strings so that the heap addresses stay the same on both sides)
            # GKeypadTextures (stock 0x10219d30) is a table of pointers into the stock .rdata, which is not mapped on our side
            # (0x101axxxx is our own image there): give our copy of the table the same strings
            for k in range(26):
                p = struct.unpack('<I', bytes(fncalls.MEM[0x10219D30 - fncalls.BASE + 4 * k:0x10219D30 - fncalls.BASE + 4 * k + 4]))[0]
                chars = []
                while True:
                    c = struct.unpack('<H', bytes(fncalls.MEM[p - fncalls.BASE + 2 * len(chars):p - fncalls.BASE + 2 * len(chars) + 2]))[0]
                    if c == 0:
                        break
                    chars.append(chr(c))
                q = self.put_wstr(''.join(chars))
                if kind == 'ours':
                    self.write32(NWBASE + 0x219D30 + 4 * k, q)
        todo = [self.cls, 'NConsoleWnd']
        for _lvl in range(3):
            nxt = []
            for cn in todo:
                for (_o, kd, tt) in header_fields(cn):
                    if kd == 'wnd' and tt in classes() and tt not in self.child_vt:
                        self.child_vt[tt] = self.class_vt(tt)
                        nxt.append(tt)
            todo = nxt
        bit = lambda slot: (lambda e, t, a: (zlib.crc32(('%s|%d|%d' % (e.nm(t), slot, e.case['seed'])).encode()) >> 4) & 1)
        for cpp in list(self._cvt):
            # IsEnabled (slot 26, 0x10003020) and IsShowWindow (slot 41, 0x100345e0) of the fake windows: a bit of the case, so the branches
            # that test the state of a child window are taken both ways (the window under test keeps its own answer for 41)
            rr = {}
            for sl, va in ((26, 0x10003020), (41, 0x100345E0)):
                if any(x['slot'] == sl and x.get('va') == va for x in classes()[cpp]['vtable_slots']) and not (cpp == cls and sl == 41):
                    rr[sl] = bit(sl)
            if rr:
                self.class_vt(cpp, rets=rr)
        for k, v in (rets or {}).items():
            self.rets[k] = v
        self.case = None
        self.volstate = {}
        for _nm in list(self.callee_names.values()):
            if re.match(r'^NC\w+::~NC\w+$', _nm) and _nm.split('::')[0] != cls:
                self.lognames[_nm] = 'base dtor'           # a base class's destructor: ours has its chain inline, ending in ~UObject
        # The stock reaches the nProtect driver through its own thunks (0x1004c060 NPKGetLoadStartup, 0x1004beb0 NPKLoadAtStartup) and
        # IsAdministrator (0x1004bf50), with the console's NKeyCrypt in ecx, which none of the three reads. Ours (hook build and standalone
        # alike, since the options window calls the real functions of NKeyCrypt.cpp) calls the STATIC cdecl functions NKeyCrypt::GetLoadStartup /
        # LoadAtStartup / IsAdministrator: no `this`, the caller pops. So the three calls are logged by the stock's name WITHOUT the ecx
        # value on both sides, and ours' functions are trapped (otherwise they load npkcrypt.dll for real: LoadLibraryA).
        for _nm, _sym, _na in (('NPKGetLoadStartup', '?GetLoadStartup@NKeyCrypt@@SAHXZ', 0), ('NPKLoadAtStartup', '?LoadAtStartup@NKeyCrypt@@SAXH@Z', 1)):
            if _nm in self.callee_names.values():
                self.nothis.add(_nm)
                if self.kind == 'ours' and _sym in self.symbols:
                    self.alias_extra(_sym, _nm, 0, _na, rthis=0, cdecl=_na)
        self.nothis.add('NKeyCrypt::IsAdministrator')
        if 'CastToLineagePlayerController' in self.callee_names.values():
            # Cast<ALineagePlayerController>(Object) is a plain cdecl function (0x1005c450: the object at [esp+4], no `this`): its argument is
            # what is compared (the ecx the caller happens to hold is whatever the compiler left there). It casts or not, by the case
            self.cdecl_n['CastToLineagePlayerController'] = 1
            self.rets.setdefault('CastToLineagePlayerController', lambda e, t, a: a[0] if (e.case['seed'] >> 12) & 1 else 0)
        for _nm, _f in (('NPKGetLoadStartup', 1), ('NKeyCrypt::IsAdministrator', 2), ('NKeyCrypt::Open', 3), ('NKeyCrypt::RegisterMessage', 4)):
            if _nm in self.callee_names.values():
                self.rets.setdefault(_nm, (lambda f: (lambda e, t, a: (e.case['seed'] >> (8 + f)) & 1))(_f))
        self.rets.setdefault('NConsoleWnd::GetMyUser', lambda e, t, a: e.user if (e.case['seed'] >> 13) & 1 else 0)
        self.rets.setdefault('NCWnd::WindowProc', lambda e, t, a: e.case['basewp'])
        for _nm in list(self.callee_names.values()):
            if _nm.endswith('::WindowProc'):          # the base class's (NControl::WindowProc ...): handled or not, by the case
                self.rets.setdefault(_nm, lambda e, t, a: e.case['basewp'])
        self.rets.setdefault('NCObject::FindTexture', lambda e, t, a: 0x9000 + sum(ord(ch) for ch in (e.wstr(a[0]) or '')) + a[1])
        self.rets.setdefault('NListRow::NListRow', self.row_ctor)
        self.cdecl_n.setdefault('_malloc', 4)
        self.cdecl_n.setdefault('_free', 4)
        self.rets.setdefault('_malloc', lambda e, t, a: e.alloc(max(a[0], 16)))
        self.rets.setdefault('NToolTipInfo::NToolTipInfo', self.tip_ctor)
        self.cdecl_n.setdefault('NCGDevice::GetTextExtent', 8)
        self.rets.setdefault('NCGDevice::GetTextExtent', self.gd_extent)
        self.rets.setdefault('NCGDevice::DrawText', lambda e, t, a: 6 * len(e.wstr(a[3]) or ''))
        self.cdecl_n.setdefault('NCGDevice::GetCharSize', 3)
        self.cdecl_n.setdefault('NCGDevice::DestroyFont', 1)

    def cfg_pick(self, e, a, floats=False):
        """the value of (section, key): the case's ini, else a number or "not found" derived from the names and the case (the same on
        both sides, so every reading of the configuration the function makes gets values and absences alike)"""
        v = e.case['ini'].get((e.wstr(a[0]), e.wstr(a[1])))
        if v is not None:
            return v
        h = zlib.crc32(('%s|%s|%d' % (e.wstr(a[0]), e.wstr(a[1]), e.case['seed'])).encode())
        if h % 4 == 0:
            return None
        if floats:
            return [0.0, 0.25, 0.5, 1.0, 2.5][(h >> 3) % 5]
        return [0, 1, 1, 2, 3, 4, 5, 100, 640][(h >> 3) % 9]

    def cfg_get(self, e, t, a):
        """GetBool / GetInt / GetString(buf): the value of (section, key), else not found"""
        v = self.cfg_pick(e, a)
        if v is None:
            return 0
        if isinstance(v, str):
            e.write_wstr_at(a[2], v)
        else:
            e.write32(a[2], v & 0xFFFFFFFF)
        return 1

    def cfg_getf(self, e, t, a):
        v = self.cfg_pick(e, a, True)
        if v is None:
            return 0
        e.write32(a[2], f32(v))
        return 1

    def cfg_getstr(self, e, t, a):
        v = self.cfg_pick(e, a)
        return e.put_wstr(v if isinstance(v, str) else '')

    def bmp_bytes(self, kind, size):
        """the file of the case: BM, 64 x 64, 8 bits per pixel, or one of the ways it can be wrong"""
        d = bytearray(size)
        if kind != 'nobm':
            d[0:2] = b'BM'
        d[0x0E:0x12] = struct.pack('<I', 40)
        d[0x12:0x16] = struct.pack('<I', 32 if kind == 'baddim' else 64)
        d[0x16:0x1A] = struct.pack('<I', 64)
        d[0x1C:0x1E] = struct.pack('<H', {'bpp24': 24, 'ok4': 4, 'ok1': 1}.get(kind, 8))
        return bytes(d)

    def mk_archive(self, e, data):
        ar = e.obj('archive', 0x40, e.vt_ar)
        e.arch[ar] = data
        return ar

    def fm_reader(self, e, t, a):
        path = e.wstr(a[0]) or ''
        cr = e.case['crest']
        if path.endswith('.dds'):
            return self.mk_archive(e, bytes(cr['dds']))
        if cr['file'] is None:
            return 0
        return self.mk_archive(e, self.bmp_bytes(cr['file'], cr['size']))

    def fm_writer(self, e, t, a):
        return self.mk_archive(e, b'') if e.case['crest']['writer'] else 0

    def fm_delete(self, e, t, a):
        return 1

    def fm_mkdir(self, e, t, a):
        return e.case['crest']['mkdir']

    def ar_size(self, e, t, a):
        return len(e.arch.get(t, b''))

    def ar_serialize(self, e, t, a):
        d = e.arch.get(t, b'')
        if d and a[0]:
            e.uc.mem_write(a[0], d[:a[1]])
        return None

    def row_ctor(self, e, t, a):
        """NListRow::NListRow(cells): a row of 0x890 bytes with its cells (0x1A64 each) at +0"""
        e.uc.mem_write(t, bytes(0x890))
        e.write32(t, e.obj('cells', 0x1A64 * a[0] + 16))
        e.write32(t + 0x804, 0xFFFFFFFF)
        e.write32(t + 0x808, 0xFFFFFFFF)
        return t

    def tip_ctor(self, e, t, a):
        """NToolTipInfo(...) built on the stack to be passed by value: its bytes are the stock constructor's, here all zero"""
        e.uc.mem_write(t, bytes(0x80))
        return t

    def party_fill(self, e, t, a):
        n = e.case['pn']
        pairs = e.alloc(0x18 * max(n, 1)) if n else 0
        for k in range(n):
            for j in range(6):
                e.write32(pairs + 0x18 * k + 4 * j, e.case['pv'][(6 * k + j) % len(e.case['pv'])] & 0xFFFFFFFF)
        m = a[0]
        e.write32(m, pairs)
        e.write32(m + 4, n)
        e.write32(m + 8, n)
        e.write32(m + 0xC, e.alloc(32) if (e.case['seed'] & 1) else 0)      # the hash table (freed by the caller)
        return 0

    def gd_extent(self, e, t, a):
        text = e.wstr(a[1]) or ''
        e.write32(a[0], 6 * len(text))
        e.write32(a[0] + 4, 13)
        return 0

    def do_import(self, nm):
        return PW.do_import(self, nm)

    # ---- the window --------------------------------------------------------------------------------------------------------
    def vt_of(self, t):
        if t not in self.child_vt:
            self.child_vt[t] = self.vt_win
        return self.child_vt[t]

    def fill(self, w, cls, c, depth, base=0xE8, unknown_leaf=True, limit=0x2C0, nonnull=False):
        """fills the fields of the object w (class cls) from the header: pointers to fake windows (depth levels), small integers, strings;
        the slots the header does not know point to a leaf window (so a second dereference works)"""
        size = classes()[cls]['size'] if cls in classes() else 0x300
        known = set()
        fields = header_fields(cls) if cls in classes() else []
        for i, (off, kd, t) in enumerate(fields):
            known.add(off)
            v = c['fv'][(i + (off >> 2)) % len(c['fv'])]
            if kd == 'wnd':
                if (v[0] < 0.12 and not nonnull) or depth <= 0:
                    if depth <= 0 and (v[0] >= 0.12 or nonnull):
                        self.write32(w + off, self.leaf)
                    continue
                k = self.wnd('%s@%x' % (t, off), self.vt_of(t), min(max(classes().get(t, {}).get('size', 0x300), 0x300), 0x4000) + 0x40, parent=w)
                if depth == self.depth0:
                    self.kids['%x' % off] = k
                self.write32(w + off, k)
                self.fill(k, t, c, depth - 1, unknown_leaf=unknown_leaf, nonnull=nonnull)
            elif kd == 'tex':
                self.write32(w + off, 0 if v[0] < 0.2 else 0x5000 + off)
            elif kd == 'obj':
                self.write32(w + off, 0 if v[0] < 0.2 else self.leaf)
            elif kd == 'int':
                self.write32(w + off, (0 if v[1] < 0.3 else SMALL[int((v[1] - 0.3) / 0.7 * len(SMALL)) % len(SMALL)]) & 0xFFFFFFFF)      # (a third of the integers are 0: the flags of the real windows mostly are)
            elif kd == 'float':
                self.write32(w + off, f32([0.0, 1.0, 2.5, 100.0, -3.0][int(v[1] * 5) % 5]))
            elif kd == 'fstr':
                self.fs_set(w + off, ['', 'abc', 'x y z'][int(v[1] * 3) % 3])
        if unknown_leaf:
            # (the same as a read32 / write32 per dword, done on one block: the world of a case is built twice per case and this loop was its main cost)
            end = min(max(size, 0x200), limit)
            end -= (end - base) % 4
            if end > base:
                blk = bytearray(self.uc.mem_read(w + base, end - base))
                for off in range(base, end, 4):
                    if off not in known and blk[off - base:off - base + 4] == ZERO4:
                        struct.pack_into('<I', blk, off - base, self.leaf)
                self.uc.mem_write(w + base, bytes(blk))

    def make_this(self, c, fill):
        size = classes()[self.cls]['size']
        self.leaf = self.wnd('leaf', self.vt_win, 0x400)
        # every slot of the leaf points back to a second leaf (a third dereference works too)
        leaf2 = self.wnd('leaf2', self.vt_win, 0x400)
        for off in range(0xE8, 0x300, 4):
            self.write32(self.leaf + off, leaf2)
        w = self.wnd('this', self.vt_this, size + 0x40, parent=self.parent)
        self.write32(w + 0x44, c['sx'])
        self.write32(w + 0x48, c['sy'])
        self.write32(w + 0x4C, f32(c['pw']))
        self.write32(w + 0x50, f32(c['ph']))
        self.write32(w + 0x3C, self.leaf if (c["seed"] >> 4) & 1 else 0)          # NCWnd::Icon (shown while minimized)
        self.write32(self.langvar, c['lang'])                                  # GLanguageType
        self.volstate = {}
        self.write32(self.engine + 0x5C, 0 if (self.cls == 'NCAudioOptionWnd' and (c['seed'] >> 9) % 5 == 0) else self.audio)    # (no audio subsystem)
        self.write32(self.user + 0x19C, (c['seed'] >> 14) & 1)
        self.write32(self.console + 0x38, c['state'])                           # the console's state (L2ConsoleState; 6 = in the world)
        self.fs_set(self.gdata + 0xAA9D4, ['', 'notice', 'a b c'][c['seed'] % 3])        # the notice text and the test server's
        self.fs_set(self.gdata + 0xAA9E0, ['', 'test notice'][(c['seed'] >> 3) % 2])
        kk = (c['seed'] >> 6) % 3                                           # no controller / a controller without a pawn / a pawn
        self.write32(self.vp0 + 0x3C, 0 if kk == 0 else self.actor)
        self.write32(self.actor + 0x3A8, self.pawn if kk == 2 else 0)
        self.write32(self.pawn + 0x13F0, self.ctrl if (c['seed'] >> 15) & 1 else 0)
        for j in range(3):
            self.write32(self.pawn + 0x1BC + 4 * j, f32(c['pv'][j] / 7.0))
        self.kids = {}
        self.depth0 = 2
        gr = random.Random(c['seed'] ^ 0x5A5A)
        for nm in sorted(self.gvars):
            key = nm.split('@@')[0].lstrip('?')
            if key in ('GSEKScreenX', 'GSEKScreenY'):
                self.write32(self.gvars[nm], gr.choice([640, 800, 1024, 1280]))
            elif key in ('GConfig', 'GMalloc', 'GError', 'GLog', 'GNull', 'GFileManager', 'GMem', 'GGlobalTempObjects'):
                continue
            elif nm.endswith('MA'):
                self.write32(self.gvars[nm], f32(gr.choice([0.0, 1.0, 2.5, 100.0])))
            else:
                self.write32(self.gvars[nm], gr.choice([0, 0, 1, 1, 2, 3]))
        base = classes()[classes()[self.cls]['super']]['size'] if classes()[self.cls].get('super') in classes() else 0xE8
        if fill:
            self.fill(w, self.cls, c, self.depth0, base=max(0xE8, base), nonnull=not self.nulls_ok)
        if self.cls == 'NCCreditWnd':
            self.make_credit_table(c, w)
        if self.cls == 'NCQuestListInfoWnd':
            self.make_quest_table(c)
            self.write32(w + 0x8C, self.leaf if (c['seed'] >> 1) & 1 else 0)          # ToolTipSkin (OnCreate makes it)
        # the console (its windows and numbers)
        for lo_, hi_ in ((0x58, 0x3C64), (0x3C6C, 0x3F00)):          # (zeroed except the two dwords at 0x3C64 / 0x3C68)
            self.uc.mem_write(self.console + lo_, bytes(hi_ - lo_))
        self.fill(self.console, 'NConsoleWnd', c, 2, base=0x58, unknown_leaf=True, limit=0x3F00, nonnull=True)
        self.write32(self.console + 0x54, self.engine)
        # functions that test the pointers of a chain of the console for null: one level of the chain (or none) is cut in each case
        chain = CHAINS.get(getattr(self, 'label', ''))
        if chain:
            k = (c['seed'] >> 7) % (len(chain) + 1)
            p = self.console
            for i, off in enumerate(chain):
                if i == k:
                    self.write32(p + off, 0)
                    break
                p = self.read32(p + off)
                if p == 0:
                    break
        return w

    def make_quest_table(self, c):
        """FL2GameData + 0x3D28: 2000 quest records of 0xA0 bytes (FillList), a few of them filled; the two level strings"""
        rr = random.Random(c['seed'] ^ 0x0E57)
        for k in getattr(self, '_qk', ()):
            self.uc.mem_write(self.gdata + 0x3D28 + 0xA0 * k, bytes(0xA0))
        ks = sorted(rr.sample(range(2000), rr.choice([0, 1, 2, 4])))
        self._qk = ks
        for k in ks:
            q = self.gdata + 0x3D28 + 0xA0 * k
            self.write32(q, rr.randint(1, 500))
            self.write32(q + 0x04, rr.randint(0, 300))
            self.write32(q + 0x08, rr.choice([1, 1, 1, 0, 2]))                 # FillList wants 1
            self.fs_set(q + 0x0C, rr.choice(['Q', 'The quest', 'Tutorial']))
            self.write32(q + 0x5C, rr.choice([0, 1, 20, 78]))                  # level from
            self.write32(q + 0x60, rr.choice([0, 0, 40, 85]))                  # level to
            self.write32(q + 0x64, rr.choice([0, 1, 2, 3, 4, 7]))              # kind (0..3 pick the icon)
            self.write32(q + 0x78, rr.choice([0, 1000001, 1000002]))           # npc
            self.fs_set(q + 0x88, rr.choice(['', 'Elf', 'Human']))
            self.fs_set(q + 0x94, rr.choice(['', 'Hello', 'x y']))
        self.fs_set(self.gdata + 0x9AF60, rr.choice(['', 'Lv', 'level']))
        self.fs_set(self.gdata + 0x9AFB4, rr.choice(['', 'All', 'Any level']))

    def sel_row(self, e, t, a):
        """NCListCtrl::GetSelectedRow: no row, or one with the quest id at +0x804"""
        if e.case['seed'] % 4 == 0:
            return 0
        row = e.alloc(0x890)
        e.write32(row + 0x804, (e.case['seed'] >> 3) % 600)
        return row

    def make_credit_table(self, c, w):
        """FL2GameData+0x3CF4: the map page number -> page (pairs of 12 bytes, a hash of 4 entries)"""
        rr = random.Random(c['seed'] ^ 0xC0DE)
        n = rr.choice([0, 1, 2, 3, 5])
        pairs = self.obj('credpairs', 12 * max(n, 1) + 16)
        hashes = self.obj('credhash', 16)
        for h in range(4):
            self.write32(hashes + 4 * h, 0xFFFFFFFF)
        for i in range(n):
            page = self.obj('credpage%d' % i, 0x30)
            self.fs_set(page + 4, rr.choice(['', 'creditA.htm', 'creditB.htm']))
            self.fs_set(page + 0x10, rr.choice(['', 'L2UI.pic1', 'L2UI.pic2']))
            self.write32(page + 0x1C, rr.choice([0, 1, 3, 10]))
            self.write32(page + 0x20, rr.choice([0, 1, 2, 5, 9, 12]))
            h = i & 3
            self.write32(pairs + 12 * i, self.read32(hashes + 4 * h))        # HashNext
            self.write32(pairs + 12 * i + 4, i)                                # Key
            self.write32(pairs + 12 * i + 8, page)                             # Value
            self.write32(hashes + 4 * h, i)
        tab = self.obj('credtable', 0x20)
        self.write32(tab, pairs)
        self.write32(tab + 4, n)
        self.write32(tab + 8, n)
        self.write32(tab + 0xC, hashes)
        self.write32(tab + 0x10, 4)
        self.write32(self.gdata + 0x3CF4, tab)
        self.credit_n = n
        self.write32(w + 0xC08, n)                                  # PageCount
        self.write32(w + 0x200, rr.randint(-1, max(n - 1, -1)) & 0xFFFFFFFF)   # Page: one the table has (or the one before the first)

    def snaps(self, w, with_kids=True):
        size = classes()[self.cls]['size']
        s = {'this': (w, min(size + 0x40, 0x4000)), 'parent': (self.parent, 0x300), 'console': (self.console, 0x3E00)}
        if with_kids:
            for k, p in self.kids.items():
                s['kid' + k] = (p, 0x200)
            for i, p in enumerate(self.created):
                s['new%d' % i] = (p, min(self.created_size.get(p, 0x200), 0x2000))
        return s


class Case(dict):
    def __repr__(self):
        return 'case(seed=%d)' % self['seed']


def rand_case(r):
    return Case(sx=r.choice([0, 100, 640, -30]), sy=r.choice([0, 50, 300, -20]), pw=r.choice([100.0, 244.5, 300.0]), ph=r.choice([100.0, 330.25]),
                basewp=r.choice([0, 0, 1]), fv=[(r.random(), r.random()) for _ in range(40)], seed=r.randint(0, 10 ** 9), mp=r.randint(0, 1), vol=[r.choice([0.0, 0.1, 0.25, 0.5, 0.75, 1.0, 0.9, 0.3, 0.7, 0.19, 0.2, 0.41, 1.5, -0.5, float('nan')]) for _ in range(4)],
                time=[2005, 3, 4, 5, 6], ini={}, crest=dict(path=r.choice(['C:/crest.bmp', 'a.bmp']), extra=r.choice([0, 1, 5]),
                   file=r.choice([None, 'nobm', 'baddim', 'bpp24', 'ok8', 'ok8', 'ok4', 'ok1']), size=r.choice([64, 1078, 4150]),
                   mkdir=r.randint(0, 1), writer=r.choice([0, 1, 1]), dds=r.choice([0, 100, 8208])),
                lang=r.choice([0, 0, 1, 2, 3, 4, 4, 5]), state=r.choice([0, 1, 2, 3, 4, 5, 6, 6, 7, 8]), pn=r.choice([0, 1, 2, 3, 5]), pv=[r.randint(-3000, 3000) for _ in range(30)])


# ---- the table ---------------------------------------------------------------------------------------------------------------
# (label, class, stock va, argument kind, void?, mode)    kinds: none, proc, key, timer, paint, mouse, int1, int2, int3
TESTS = []


def T(label, cls, va, args='none', void=False, fill=True, rets=None, skip=(), extra=None, syms=None, aliases=()):
    TESTS.append(dict(label=label, cls=cls, va=va, args=args, void=void, fill=fill, rets=rets, skip=skip, extra=extra, syms=syms, aliases=aliases))


def gen_args(kind, va, r):
    imm = imm_consts(va)
    if kind == 'none':
        return []
    if kind == 'proc':
        msgs = [0x501, 0x501, 0x501, 0x111, 0x201] + [v for v in imm if 0xF0000000 <= v or 0x100 <= v < 0x600]
        ids = imm + 3 * imm_consts(va, near=False) + [0, 1, 2, 0x1234]      # (the exact constants of the compares weigh more)
        return [r.choice(msgs) & 0xFFFFFFFF, r.choice(ids) & 0xFFFFFFFF, r.choice([0, 1, 2, 5])]
    if kind == 'key':
        return [r.choice(imm + [0x0D, 0x1B, 0x41, 0x20]) & 0xFFFFFFFF, r.choice([0, 0x14B0001])]
    if kind == 'timer':
        return [r.choice(imm + [0, 1]) & 0xFFFFFFFF]
    if kind == 'mouse':
        return [r.choice([0, 1, 5]), (r.randint(0, 400) << 16) | r.randint(0, 400)]
    if kind == 'align':
        return [r.choice([0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, -1])]
    if kind == 'float1':
        return [f32(r.choice([0.0, 0.25, 1.0, 2.5, 100.0]))]
    if kind == 'mask':
        return [r.choice([0] + [1 << k for k in range(20)] + [r.randint(0, 0xFFFFF)])]
    if kind == 'range40':
        return [r.randint(0, 40)]
    if kind == 'int1':
        return [r.choice(imm + [0, 1, 2, 3, -1]) & 0xFFFFFFFF]
    if kind == 'int2':
        return [r.choice(imm + [0, 1, 2, 3, -1]) & 0xFFFFFFFF, r.choice(imm + [0, 1, 2, 3]) & 0xFFFFFFFF]
    if kind == 'text':
        return ['text']
    raise RuntimeError(kind)


def run(e, A, c, t, ci):
    r = random.Random(c['seed'])
    e.case = c
    e.nulls_ok = ('OnDestroy' in t['label'])
    e.label = t['label']
    e.nnew = 0
    e.created = []
    e.created_size = {}
    e.pstacks = {}
    w = e.make_this(c, t['fill'])
    if t['cls'] == 'NConsoleWnd':
        w = e.console          # the functions of the console itself: this is GConsole
    args = gen_args(t['args'], t['va'], r) if t['args'] not in ('paint', 'pstack', 'text', 'dlginfo', 'crest', 'partymap') else []
    snaps = e.snaps(w)
    if t['args'] == 'paint':
        dev = e.obj('dev', 0x100, e.vt_dev)
        snaps['dev'] = (dev, 0x100)
        args = [dev]
    if t['args'] == 'pstack':
        ps = e.obj('params', 0x40)
        e.pstacks[ps] = [r.choice([0, 1, 2, 3, 5, 100, -1, e.put_wstr('hello'), e.put_wstr('')]) & 0xFFFFFFFF for _ in range(12)]
        args = [ps]
    if t['args'] == 'dlginfo':
        di = e.obj('dlginfo', 0xA0)
        ent = e.obj('infoentry', 0x40)
        for off, v in ((0, r.choice([-1, 0, 1, 4, 0x1D, 0x1F, 0x10])), (4, r.choice([0, 3])), (8, ent if r.random() < 0.7 else 0), (0xC, r.randint(0, 1)),
                       (0x10, r.randint(0, 1)), (0x14, r.randint(0, 1)), (0x18, r.randint(0, 4)), (0x1C, r.randint(0, 1)), (0x5C, r.choice([0, 7])),
                       (0x60, r.choice([0, 3])), (0x70, r.randint(0, 3)), (0x74, r.randint(0, 3)), (0x78, r.randint(0, 3))):
            e.write32(di + off, v & 0xFFFFFFFF)
        for off in (0x20, 0x2C, 0x38, 0x44, 0x50, 0x64, 0x88, 0x94):
            e.fs_set(di + off, r.choice(['', 'abc', 'Some message', 'x']))
        for off in (0x7C, 0x80, 0x84):
            e.write32(di + off, f32(r.choice([0.0, 1.5, -2.0])))
        snaps['dlginfo'] = (di, 0xA0)
        args = [di]
    if t['args'] == 'crest':
        cr = c['crest']
        ps = e.obj('params', 0x40)
        e.pstacks[ps] = [e.put_wstr(cr['path']), cr['extra']]
        args = [ps]
        e.arch = {}
    if t['args'] == 'partymap':
        w = e.obj('party', 0x20)
        e.party_fill(e, None, [w])
        e.write32(w + 0x10, 8)
        snaps['party'] = (w, 0x20)
    if t['label'] == 'NCreditTable::Find':
        w = e.read32(e.gdata + 0x3CF4)          # this is the table of the pages; the argument is a pointer to the key
        kp = e.obj('key', 0x10)
        e.write32(kp, r.choice([0, 1, 2, 3, 4, 5, 6, 7, 100, 0xFFFFFFFF]))
        args = [kp]
    if t['args'] == 'text':
        args = [e.put_wstr(r.choice(['', 'abc', 'hello world', 'x' * 40]))]
    res = e.go(A, this=w, args=args, snaps=snaps)
    res['mem'].update({'new%d' % i: e.snapshot(p, min(e.created_size.get(p, 0x200), 0x2000)) for i, p in enumerate(e.created)})
    if t['label'].endswith('::dtor'):
        # the stock calls the base class's destructor (a stub here, logged 'base dtor'); ours has that chain inline, which repeats
        # UObject::ConditionalDestroy at every level (idempotent in the engine) before ending in ~UObject: the repeats are dropped
        out = []
        for x in res['log']:
            if x[0] == 'ConditionalDestroy' and any(y[0] == 'ConditionalDestroy' for y in out):
                continue
            if x[0] == 'base dtor' and out and out[-1][0] == 'base dtor':
                continue
            out.append(x)
        res['log'] = out
    if t['void']:
        del res['eax']
    return res


def main():
    argv = sys.argv[1:]
    n = int(argv[0]) if argv else 60
    seed = int(argv[1]) if len(argv) > 1 else 1
    sel = argv[2:]
    lm = log_map()
    total = 0
    # one pair of emulators per class and option set: the callees of every function of the group are stubbed together
    groups = {}
    for t in TESTS:
        if sel and not any(sl in t['label'] for sl in sel):
            continue
        if t['va'] not in lm:
            if sa.ENABLED:
                sa.skip(t['label'], sa.NoStandaloneFunction(t['va']))      # (the SKIP line the suite counts)
            else:
                print('%-40s not hooked' % t['label'])
            continue
        key = (t['cls'], repr(sorted((t['rets'] or {}).keys())), repr(sorted(t['skip'])), repr(t['extra']), repr(t['syms']), repr(t['aliases']))
        groups.setdefault(key, []).append(t)
    for key, tests in groups.items():
        t0 = tests[0]
        funcs = [t['va'] for t in tests]
        try:
            es = SYS('stock', funcs, t0['cls'], t0['rets'], t0['skip'], t0['extra'], t0['syms'], t0['aliases'])
            eo = SYS('ours', funcs, t0['cls'], t0['rets'], t0['skip'], t0['extra'], t0['syms'], t0['aliases'])
            for (sym, log, pop, vcls) in t0['aliases']:
                eo.alias_extra(sym, log, pop, 0, post=lambda e, tt, vcls=vcls: e.write32(tt, e.child_vt[vcls]))
        except Exception as ex:
            print('%-40s setup failed: %s' % (t0['label'], ex))
            total += 1
            continue
        es.heap_mark, eo.heap_mark = es.heap_ptr, eo.heap_ptr
        for t in tests:
            es.track(t['va'])
        for t in tests:
            r = random.Random(seed)
            cases = [rand_case(r) for _ in range(n)]
            total += run_tests(t['label'], es, eo, lambda e, A, c, t=t: run(e, A, c, t, 0), cases, t['va'], lm[t['va']])
            cov_report(es, t['va'])
    print('TOTAL problems', total)


# =============================================================================================================================
# the functions (stock addresses; the class is the `this` of the call)
# =============================================================================================================================
# label, class, stock va, argument kind, void?
TABLE = [
    ('NCNoticeInfoWnd::OnCreate', 'NCNoticeInfoWnd', 0x100bd7d0, 'none', False),
    ('NCNoticeInfoWnd::OnDestroy', 'NCNoticeInfoWnd', 0x100bd800, 'none', False),
    ('NCNoticeInfoWnd::SetText', 'NCNoticeInfoWnd', 0x100bd880, 'text', True),
    ('NCNoticeInfoWnd::ClearText', 'NCNoticeInfoWnd', 0x100bd8b0, 'none', True),
    ('NCNoticeInfoWnd::OnPaint', 'NCNoticeInfoWnd', 0x100bdbd0, 'paint', False),
    ('NCNoticeInfoWnd::ctor', 'NCNoticeInfoWnd', 0x100be620, 'none', False),
    # the login screen
    ('NCLoginFunctionWnd::WindowProc', 'NCLoginFunctionWnd', 0x1009d360, 'proc', False),
    ('NCLoginFunctionWnd::OnNewAccountBtnClick', 'NCLoginFunctionWnd', 0x1009d440, 'none', False),
    ('NCLoginFunctionWnd::OnLostAccountBtnClick', 'NCLoginFunctionWnd', 0x1009d4d0, 'none', False),
    ('NCLoginFunctionWnd::OnOptionsBtnClick', 'NCLoginFunctionWnd', 0x1009d560, 'none', False),
    ('NCLoginFunctionWnd::OnCreditsBtnClick', 'NCLoginFunctionWnd', 0x1009d5e0, 'none', False),
    ('NCLoginFunctionWnd::OnReplayBtnClick', 'NCLoginFunctionWnd', 0x1009d6d0, 'none', False),
    ('NCLoginFunctionWnd::OnPaint', 'NCLoginFunctionWnd', 0x1009d760, 'paint', False),
    ('NCLoginFunctionWnd::OnDestroy', 'NCLoginFunctionWnd', 0x1009d850, 'none', False),
    ('NCLoginFunctionWnd::OnCreate', 'NCLoginFunctionWnd', 0x1009d9a0, 'none', False),
    ('NCLoginFunctionWnd::ctor', 'NCLoginFunctionWnd', 0x1009ddd0, 'none', False),
    ('NCLoginServerWnd::ReceiveServerList', 'NCLoginServerWnd', 0x1009de80, 'int1', True),
    ('NCLoginServerWnd::OnDestroy', 'NCLoginServerWnd', 0x1009df10, 'none', False),
    ('NCLoginServerWnd::ctor', 'NCLoginServerWnd', 0x1009e080, 'none', False),
    ('NCLoginServerWnd::OnCreate', 'NCLoginServerWnd', 0x1009e120, 'none', False),
    ('NCLoginWnd::ShowWindow', 'NCLoginWnd', 0x1009e610, 'none', True),
    ('NCLoginWnd::HideWindow', 'NCLoginWnd', 0x1009e6d0, 'none', True),
    ('NCLoginWnd::ShowReplayMode', 'NCLoginWnd', 0x1009e700, 'none', True),
    ('NCLoginWnd::ShowLoginMode', 'NCLoginWnd', 0x1009e780, 'none', True),
    ('NCLoginWnd::ResetWindows', 'NCLoginWnd', 0x1009e800, 'none', True),
    ('NCLoginWnd::OnDestroy', 'NCLoginWnd', 0x1009edf0, 'none', False),
    ('NCLoginWnd::OnPaint', 'NCLoginWnd', 0x1009efd0, 'paint', False),
    ('NCLoginWnd::ctor', 'NCLoginWnd', 0x1009f700, 'none', False),
    ('NCLoginWnd::OnCreate', 'NCLoginWnd', 0x1009f7a0, 'none', False),
    # the game option page
    ('NCGameOptionWnd::WindowProc', 'NCGameOptionWnd', 0x100c4970, 'proc', False),
    ('NCGameOptionWnd::OnReset', 'NCGameOptionWnd', 0x100c4a10, 'none', False),
    ('NCGameOptionWnd::ShowWindow', 'NCGameOptionWnd', 0x100c4a30, 'none', True),
    ('NCGameOptionWnd::OnPaint', 'NCGameOptionWnd', 0x100c77f0, 'paint', False),
    ('NCGameOptionWnd::OnDestroy', 'NCGameOptionWnd', 0x100c7a70, 'none', False),
    ('NCGameOptionWnd::ClearItems', 'NCGameOptionWnd', 0x100c7e50, 'none', True),
    ('NCGameOptionWnd::LoadSettings', 'NCGameOptionWnd', 0x100c7f60, 'none', True),
    ('NCGameOptionWnd::Apply', 'NCGameOptionWnd', 0x100c8530, 'none', True),
    ('NCGameOptionWnd::OnSystemMsgClicked', 'NCGameOptionWnd', 0x100c8c40, 'none', False),
    ('NCGameOptionWnd::OnOtherPCClicked', 'NCGameOptionWnd', 0x100c8c90, 'none', False),
    ('NCGameOptionWnd::InitItems', 'NCGameOptionWnd', 0x100c97d0, 'none', True),
    ('NCGameOptionWnd::OnCreate', 'NCGameOptionWnd', 0x100ca4b0, 'none', False),
    ('NCGameOptionWnd::ctor', 'NCGameOptionWnd', 0x100cb430, 'none', False),
    # the options window (parent of the video / game / audio pages)
    ('NCOptionWnd::OnCancel', 'NCOptionWnd', 0x10084ee0, 'none', False),
    ('NCOptionWnd::WindowProc', 'NCOptionWnd', 0x100c4aa0, 'proc', False),
    ('NCOptionWnd::OnPaint', 'NCOptionWnd', 0x100c8d00, 'paint', False),
    ('NCOptionWnd::OnDestroy', 'NCOptionWnd', 0x100c8e30, 'none', False),
    ('NCOptionWnd::OnOK', 'NCOptionWnd', 0x100c9010, 'none', False),
    ('NCOptionWnd::OnApply', 'NCOptionWnd', 0x100c9070, 'none', False),
    ('NCOptionWnd::ShowWindow', 'NCOptionWnd', 0x100c9980, 'none', True),
    ('NCOptionWnd::ctor', 'NCOptionWnd', 0x100cb4e0, 'int1', False),
    ('NCOptionWnd::OnCreate', 'NCOptionWnd', 0x100cb590, 'none', False),
    # the audio page
    ('NCAudioOptionWnd::ClearItems', 'NCAudioOptionWnd', 0x100c4240, 'none', True),
    ('NCAudioOptionWnd::SelectVolumes', 'NCAudioOptionWnd', 0x100c4270, 'none', True),
    ('NCAudioOptionWnd::InitVolumes', 'NCAudioOptionWnd', 0x100c47b0, 'none', True),
    ('NCAudioOptionWnd::ShowWindow', 'NCAudioOptionWnd', 0x100c4940, 'none', True),
    ('NCAudioOptionWnd::OnPaint', 'NCAudioOptionWnd', 0x100c6f40, 'paint', False),
    ('NCAudioOptionWnd::OnDestroy', 'NCAudioOptionWnd', 0x100c7170, 'none', False),
    ('NCAudioOptionWnd::Apply', 'NCAudioOptionWnd', 0x100c7320, 'none', True),
    ('NCAudioOptionWnd::OnCreate', 'NCAudioOptionWnd', 0x100c9530, 'none', False),
    ('NCAudioOptionWnd::ctor', 'NCAudioOptionWnd', 0x100cb380, 'none', False),
    # the map cover
    ('NCMapCoverWnd::WindowProc', 'NCMapCoverWnd', 0x100b87e0, 'proc', False),
    ('NCMapCoverWnd::Minimize', 'NCMapCoverWnd', 0x100b8880, 'none', False),
    ('NCMapCoverWnd::OnTimerBody', 'NCMapCoverWnd', 0x100b88e0, 'timer', False),
    ('NCMapCoverWnd::OnPaint', 'NCMapCoverWnd', 0x100b9120, 'paint', False),
    ('NCMapCoverWnd::OnDestroy', 'NCMapCoverWnd', 0x100b91c0, 'none', False),
    ('NCMapCoverWnd::OnShowMyPosition', 'NCMapCoverWnd', 0x100bb8f0, 'none', False),
    ('NCMapCoverWnd::OnShowMark', 'NCMapCoverWnd', 0x100bb9e0, 'none', False),
    ('NCMapCoverWnd::ShowWindow', 'NCMapCoverWnd', 0x100bba20, 'none', True),
    ('NCMapCoverWnd::ctor', 'NCMapCoverWnd', 0x100bbd80, 'none', False),
    ('NCMapCoverWnd::OnPartyLocBtnClick', 'NCMapCoverWnd', 0x100bcdb0, 'none', False),
    ('NCMapCoverWnd::OnCreate', 'NCMapCoverWnd', 0x100bd420, 'none', False),
    # the quest list info
    ('NCQuestListInfoWnd::WindowProc', 'NCQuestListInfoWnd', 0x100ea250, 'proc', False),
    ('NCQuestListInfoWnd::FillList', 'NCQuestListInfoWnd', 0x100ea2b0, 'none', True),
    ('NCQuestListInfoWnd::OnDestroy', 'NCQuestListInfoWnd', 0x100eb960, 'none', False),
    ('NCQuestListInfoWnd::OnPaint', 'NCQuestListInfoWnd', 0x100eb9d0, 'paint', False),
    ('NCQuestListInfoWnd::OnMouseMove', 'NCQuestListInfoWnd', 0x100ebb00, 'mouse', False),
    ('NCQuestListInfoWnd::OnMoveBtnClick', 'NCQuestListInfoWnd', 0x100ec140, 'none', False),
    ('NCQuestListInfoWnd::OnCreate', 'NCQuestListInfoWnd', 0x100ec600, 'none', False),
    ('NCQuestListInfoWnd::OnDrawToolTip', 'NCQuestListInfoWnd', 0x100ecfb0, 'paint', False),
    ('NCQuestListInfoWnd::ctor', 'NCQuestListInfoWnd', 0x100ed9b0, 'none', False),
    # the destructors (80 bytes each: the vtable, the base's destructor) and the rest of the hooks of the lane's files
    ('NCLoginWnd::dtor', 'NCLoginWnd', 0x1009f750, 'none', True),
    ('NCLoginFunctionWnd::dtor', 'NCLoginFunctionWnd', 0x1009de20, 'none', True),
    ('NCLoginServerWnd::dtor', 'NCLoginServerWnd', 0x1009e0d0, 'none', True),
    ('NCGameOptionWnd::dtor', 'NCGameOptionWnd', 0x100cb490, 'none', True),
    ('NCOptionWnd::dtor', 'NCOptionWnd', 0x100cb540, 'none', True),
    ('NCAudioOptionWnd::dtor', 'NCAudioOptionWnd', 0x100cb3e0, 'none', True),
    ('NCNoticeInfoWnd::dtor', 'NCNoticeInfoWnd', 0x100be680, 'none', True),
    ('NCMapCoverWnd::dtor', 'NCMapCoverWnd', 0x100bbde0, 'none', True),
    ('NCQuestListInfoWnd::dtor', 'NCQuestListInfoWnd', 0x100eda40, 'none', True),
    ('NCCreditWnd::dtor', 'NCCreditWnd', 0x10084700, 'none', True),
    ('NCGaraDialogBox::dtor', 'NCGaraDialogBox', 0x1000b2b0, 'none', True),
    ('NMapCoverPartyMap::Destruct', 'NCMapCoverWnd', 0x100bbe30, 'partymap', True),
    ('NCreditTable::Find', 'NCCreditWnd', 0x10084010, 'int1', False),
    ('NConsoleWnd::ShowNoticeDialog', 'NConsoleWnd', 0x10075990, 'int1', True),
    ('NGaraConsoleFns::RegisterPledgeCrestLarge', 'NConsoleWnd', 0x100809d0, 'crest', True),
    # the credits
    ('NCCreditWnd::PlaceViewer', 'NCCreditWnd', 0x10083c00, 'align', True),
    ('NCCreditWnd::FreeFont', 'NCCreditWnd', 0x10083dc0, 'none', True),
    ('NCCreditWnd::ShowWindow', 'NCCreditWnd', 0x10083dd0, 'none', True),
    ('NCCreditWnd::OnDestroy', 'NCCreditWnd', 0x10083e40, 'none', False),
    ('NCCreditWnd::OnPaint', 'NCCreditWnd', 0x10083e80, 'paint', False),
    ('NCCreditWnd::OnKeyDown', 'NCCreditWnd', 0x10083f20, 'key', False),
    ('NCCreditWnd::NextPage', 'NCCreditWnd', 0x10084060, 'none', True),
    ('NCCreditWnd::OnLButtonDown', 'NCCreditWnd', 0x10084260, 'mouse', False),
    ('NCCreditWnd::Tick', 'NCCreditWnd', 0x10084280, 'float1', True),
    ('NCCreditWnd::Start', 'NCCreditWnd', 0x10084370, 'none', True),
    ('NCCreditWnd::OnCreate', 'NCCreditWnd', 0x100844e0, 'none', False),
    ('NCCreditWnd::ctor', 'NCCreditWnd', 0x10084670, 'none', False),
]
ALIASES = {
    # ours calls the compiled constructor of a class whose stock code inlines it (NCWnd's constructor, then the vtable)
    'NCLoginServerWnd::OnCreate': [('??0NCServerInfoWnd@@QAE@XZ', 'NCWnd::NCWnd', 0, 'NCServerInfoWnd')],
}
TABLE += [
    # the desktop's message box (OnOK / OnCancel are proved in t_gara.py)
    ('NCGaraDialogBox::WindowProc', 'NCGaraDialogBox', 0x10007b70, 'proc', False),
    ('NCGaraDialogBox::SetFocus', 'NCGaraDialogBox', 0x10007c50, 'none', True),
    ('NCGaraDialogBox::OnKeyDown', 'NCGaraDialogBox', 0x10007c70, 'key', False),
    ('NCGaraDialogBox::Reset', 'NCGaraDialogBox', 0x10007cc0, 'none', True),
    ('NCGaraDialogBox::HideWindow', 'NCGaraDialogBox', 0x10007d10, 'none', True),
    ('NCGaraDialogBox::OnEditNotify', 'NCGaraDialogBox', 0x10007d20, 'int1', False),
    ('NCGaraDialogBox::SetButtonTexts', 'NCGaraDialogBox', 0x10008280, 'int2', True),
    ('NCGaraDialogBox::SetEditText', 'NCGaraDialogBox', 0x100082f0, 'text', True),
    ('NCGaraDialogBox::GetOwner', 'NCGaraDialogBox', 0x10008320, 'none', False),
    ('NCGaraDialogBox::OnProgressEnd', 'NCGaraDialogBox', 0x10008330, 'none', False),
    ('NCGaraDialogBox::OnPaint', 'NCGaraDialogBox', 0x100086a0, 'paint', False),
    ('NCGaraDialogBox::OnDestroy', 'NCGaraDialogBox', 0x10008860, 'none', False),
    ('NCGaraDialogBox::UpdateLayout', 'NCGaraDialogBox', 0x100089d0, 'none', True),
    ('NCGaraDialogBox::SetInfo', 'NCGaraDialogBox', 0x10008b90, 'dlginfo', True),
    ('NCGaraDialogBox::OnKeypad', 'NCGaraDialogBox', 0x10008d40, 'int1', False),
    ('NCGaraDialogBox::OnCreate', 'NCGaraDialogBox', 0x100090f0, 'none', False),
    ('NCGaraDialogBox::ShowWindow', 'NCGaraDialogBox', 0x100095c0, 'none', True),
    ('NCGaraDialogBox::ctor', 'NCGaraDialogBox', 0x1000b1a0, 'none', False),
    ('NMinimapFns::SetShowMark', 'NCMinimapWnd', 0x10009090, 'int1', True),
    ('NGaraConsoleFns::SetMapShowMark', 'NConsoleWnd', 0x100090c0, 'int1', True),
    ('NGaraConsoleFns::RemovePledgeCrestLarge', 'NConsoleWnd', 0x100684d0, 'none', True),
]
TABLE += [
    # the login flow of the console (this = GConsole)
    ('NConsoleWnd::ReceiveServerList', 'NConsoleWnd', 0x10065740, 'int1', True),
    ('NConsoleWnd::AddServerList', 'NConsoleWnd', 0x100657c0, 'pstack', True),
    ('NConsoleWnd::EndServerList', 'NConsoleWnd', 0x10065850, 'none', True),
    ('NConsoleWnd::CharacterCreateFail', 'NConsoleWnd', 0x100658f0, 'int1', True),
    ('NConsoleWnd::AcceptCharacterDelete', 'NConsoleWnd', 0x10065b40, 'int1', True),
    ('NConsoleWnd::ReceiveServerClose', 'NConsoleWnd', 0x10065c00, 'none', True),
    ('NConsoleWnd::ReceiveVersionCheck', 'NConsoleWnd', 0x10065c90, 'pstack', True),
    ('NConsoleWnd::RestartResetWindows', 'NConsoleWnd', 0x10065da0, 'none', True),
    ('NConsoleWnd::RestartToLobby', 'NConsoleWnd', 0x10065e80, 'none', True),
    ('NConsoleWnd::EndRecommandedData', 'NConsoleWnd', 0x100661e0, 'none', True),
    ('NConsoleWnd::SetServerPing', 'NConsoleWnd', 0x1006bcd0, 'int2', True),
    ('NConsoleWnd::BlockedAccountWithMsg', 'NConsoleWnd', 0x1006ee00, 'text', True),
    ('NConsoleWnd::BlockedAccount', 'NConsoleWnd', 0x1006ef90, 'mask', True),
    ('NConsoleWnd::AuthLoginFail', 'NConsoleWnd', 0x1006f1d0, 'range40', True),
    ('NConsoleWnd::StartNotice', 'NConsoleWnd', 0x1006f5d0, 'pstack', True),
    ('NConsoleWnd::ReceiveCharacterInfoX', 'NConsoleWnd', 0x1006f8b0, 'int1', True),
    ('NConsoleWnd::AcceptGameStart', 'NConsoleWnd', 0x1006fa20, 'int1', True),
    ('NConsoleWnd::AcceptNewCharacter', 'NConsoleWnd', 0x1006fbd0, 'int1', True),
    ('NConsoleWnd::AcceptCharacterCreate', 'NConsoleWnd', 0x1006fd20, 'int1', True),
    ('NConsoleWnd::AcceptLogOut', 'NConsoleWnd', 0x10076ad0, 'none', True),
    ('NConsoleWnd::ServerClose', 'NConsoleWnd', 0x10076ae0, 'int1', True),
    ('NAuthWndExt::ResetMode', 'NCAuthWnd', 0x1006ea10, 'none', True),
]
# stock thunks into the key encryption driver (npkcrypt, never ported): logging callees (the pop is the argument count of the call)
EXTRA = {'NCGameOptionWnd': {0x1004c060: ('NPKGetLoadStartup', 0, 0), 0x1004beb0: ('NPKLoadAtStartup', 4, 0), 0x1004bf50: ('NPKIsAdmin', 0, 0)}}
EXTRA['NCCreditWnd'] = {0x10013430: ('NCGDevice::DestroyFont', 0, 0)}
EXTRA['NCGaraDialogBox'] = {0x1000e540: ('NCEditBox::InsertText', 8, 0)}
EXTRA['NGaraConsoleFns::RegisterPledgeCrestLarge'] = {0x10171E01: ('_malloc', 0, 0), 0x101722E3: ('_free', 0, 0)}      # the stock's C runtime
SKIP = {'NCCreditWnd': (0x10083dc0, 0x10084010),
        'NConsoleWnd::EndRecommandedData': (0x10043b50,),   # a bare `ret` (folded body); ours has nothing to call for the empty inline
      # FreeFont (ours inlines it) and the table's Find run for real on both sides
        'NCGaraDialogBox::SetInfo': (0x100082f0,)}    # SetEditText: ours inlines it, so it runs for real on the stock side (InsertText is stubbed)
# the stock's out-of-line destructors of three TArray instances (their bodies are import calls, FArray::Empty and ~FArray): ours inlines them
# (hook build and standalone), so only the stock calls them as functions: they run for real on the stock side (as the compiled code of ours does)
# (standalone mode only: the hook build still calls its out-of-line copies, which the stubs log like the stock's)
if sa.ENABLED:
    SKIP['NCMapCoverWnd'] = (0x100bb860,)                      # TMapPair24Array::~TMapPair24Array
    SKIP['NCQuestListInfoWnd'] = (0x1008e8a0, 0x1001f6e0)      # TCoolTimeArray::~TCoolTimeArray, TItemEntryArray::~TItemEntryArray
# the standalone links its own C runtime (the hook build calls the stock image's, found at the stock addresses): ours' malloc / free are trapped by symbol
SYMS = {'NGaraConsoleFns::RegisterPledgeCrestLarge': {0x10171E01: '_malloc', 0x101722E3: '_free'}}
RETS = {'NCQuestListInfoWnd::OnMoveBtnClick': {'NCListCtrl::GetSelectedRow': lambda e, t, a: e.sel_row(e, t, a)}}
for row in TABLE:
    T(*row, aliases=ALIASES.get(row[0], ()), extra=EXTRA.get(row[0], EXTRA.get(row[1])), skip=SKIP.get(row[0], SKIP.get(row[1], ())), rets=RETS.get(row[0]), syms=SYMS.get(row[0]))

if __name__ == '__main__':
    main()
