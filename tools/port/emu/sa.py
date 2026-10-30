"""Standalone mode of the unicorn differential harness (tools/port/emu).

`ours` of a test is normally the HOOK build (build/bin/Release/NWindowDecomp.dll, .map): our functions at their compiled addresses, the
stock image's globals reached through g_nwBase (the harness maps a zeroed NWBASE region as the stand-in of the stock image).
With NWD_SA=1 (run_suite.py --mode sa) `ours` is the STANDALONE build (build/standalone/Release/NWindow.dll, .map): the same source
compiled with NW_HOOK_MODE=0, its own globals/vtables/functions, no stock image anywhere.

The tests were written for the hook build, so this module translates what they say in terms of the stock image:
  * log_map()      stock VA of a hooked function -> address of the same function in the standalone (through the symbol the hook
                   registration gives it: smoke log `ours=<rva>` + hook map, then the standalone map);
  * glob()/xlate() a stock address (a global, a table, a function) -> the standalone address that holds the same object: the stand-in
                   region of the hook mode (NWBASE + rva) is translated on every access by SaUc (a thin proxy around unicorn's Uc);
  * symbols        the standalone map (the decorated names are the same in both builds).
Everything that has no standalone counterpart raises NoCounterpart with the stock address, so a test never touches memory by accident.
"""
import os, re, sys, bisect, json

ENABLED = os.environ.get('NWD_SA') == '1'
ROOT = str(__import__('pathlib').Path(__file__).resolve().parents[3])
HOOK_DLL = ROOT + r'\build\bin\Release\NWindowDecomp.dll'
HOOK_MAP = ROOT + r'\build\bin\Release\NWindowDecomp.map'
# NWD_SA_DLL / NWD_SA_MAP: a copy of the standalone (a snapshot while the build directory is rebuilt, or a mutated copy: tools/port/emu/mutate_sa.py)
SA_DLL = os.environ.get('NWD_SA_DLL') or ROOT + r'\build\standalone\Release\NWindow.dll'
SA_MAP = os.environ.get('NWD_SA_MAP') or ROOT + r'\build\standalone\Release\NWindow.map'
LOG = ROOT + r'\build\smoke_system\NWindowDecomp.log'
STOCK_BASE = 0x10000000


class NoCounterpart(Exception):
    pass


MAP_RE = re.compile(r'^\s*([0-9a-f]{4}):([0-9a-f]{8})\s+(\S+)\s+([0-9a-f]{8})\s+(f|)\s*(i|)\s*(\S+)\s*$')

_maps = {}


def parse_map(path):
    """-> (sym -> va, va -> [sym], [(va, sym, is_function)])"""
    if path in _maps:
        return _maps[path]
    sym, byva, rows = {}, {}, []
    started = False
    for ln in open(path, encoding='latin-1'):
        if not started:
            if 'Publics by Value' in ln:
                started = True
            continue
        m = MAP_RE.match(ln)
        if not m:
            continue
        va = int(m.group(4), 16)
        name = m.group(3)
        isf = m.group(5) == 'f'
        sym.setdefault(name, va)
        byva.setdefault(va, []).append(name)
        rows.append((va, name, isf))
    rows.sort()
    _maps[path] = (sym, byva, rows)
    return _maps[path]


def hook_log():
    """[(stock va, log name, hook va)] of the hooks the smoke log reports `ok`"""
    out = []
    for ln in open(LOG, encoding='latin-1'):
        m = re.match(r'^\S+\s+([0-9a-f]{8}) (.*?)\s+ours=([0-9a-f]+) ok', ln)
        if m:
            out.append((int(m.group(1), 16), m.group(2).strip(), STOCK_BASE + int(m.group(3), 16)))
    return out


def _skip_sym(s):
    return (s.startswith('__unwindfunclet') or s.startswith('__ehhandler') or s.startswith('__catch') or s.startswith('__tailMerge')
            or '@?A0x' in s or s.startswith('??_G') or s.startswith('??_E') or s.startswith('__imp_') or s.startswith('_nw'))


def demangle_cm(sym):
    """('Class', 'Method') of a decorated name (ctor/dtor included), None otherwise"""
    m = re.match(r'^\?\?([01])([A-Za-z0-9_]+)@@', sym)
    if m:
        return (m.group(2), m.group(2) if m.group(1) == '0' else '~' + m.group(2))
    m = re.match(r'^\?([A-Za-z0-9_]+)@([A-Za-z0-9_]+)@@', sym)
    if m:
        return (m.group(2), m.group(1))
    m = re.match(r'^\?([A-Za-z0-9_]+)@@', sym)
    if m:
        return (None, m.group(1))
    return None


_fmap = None
_funmapped = {}
_FWD_RE = re.compile(r'^\?\?\$Fwd(C?)@.*?@NwFwd_\d+@\?A0x[0-9a-f]+@@(Q[AB]E.*)$')
_CTOR_RE = re.compile(r'^\?Run@NwCtor_\d+@\?A0x[0-9a-f]+@@SI[^@]*?@@PAV\d@(.*)$')
_DTOR_RE = re.compile(r'^\?Run@NwDtor_\d+@\?A0x[0-9a-f]+@@SI')


_dbghelp = None
_undcache = {}


def _und(sym):
    global _dbghelp
    if sym in _undcache:
        return _undcache[sym]
    if _dbghelp is None:
        import ctypes
        _dbghelp = ctypes.WinDLL('dbghelp.dll')
    import ctypes
    buf = ctypes.create_string_buffer(4096)
    n = _dbghelp.UnDecorateSymbolName(sym.encode(), buf, 4096, 0)
    _undcache[sym] = buf.value.decode('latin-1') if n else ''
    return _undcache[sym]


def _split_params(txt):
    out, depth, cur = [], 0, ''
    for ch in txt:
        if ch in '(<[':
            depth += 1
        elif ch in ')>]':
            depth -= 1
        if ch == ',' and depth == 0:
            out.append(cur)
            cur = ''
        else:
            cur += ch
    if cur.strip():
        out.append(cur)
    return out


def _norm(t):
    t = t.replace('class ', '').replace('struct ', '').replace('enum ', '').replace('union ', '')
    t = t.replace('wchar_t', 'unsignedshort').replace('__ptr64', '')
    return re.sub(r'\s+', '', t)


def _params(sym):
    """the normalised parameter list of a decorated function symbol (undname), [] when it has none; None when it cannot be read"""
    u = _und(sym)
    if not u:
        return None
    # the last top-level parenthesised group
    depth, end, start = 0, None, None
    for i in range(len(u) - 1, -1, -1):
        ch = u[i]
        if ch == ')':
            if depth == 0 and end is None:
                end = i
            depth += 1
        elif ch == '(':
            depth -= 1
            if depth == 0 and end is not None:
                start = i
                break
    if start is None:
        return None
    inner = u[start + 1:end].strip()
    if inner in ('', 'void'):
        return []
    return [_norm(x) for x in _split_params(inner)]


def _wrapper_params(sym, skip_first):
    """the parameters of a hook wrapper (Fwd<...>(params) or Run(Class*, void*, params)) without the first `skip_first` ones"""
    p = _params(sym)
    return None if p is None else p[skip_first:]


def _sa_index():
    """(class, method) -> [(symbol, va)] over the functions of the standalone map"""
    ssym, sbyva, rows = parse_map(SA_MAP)
    idx = {}
    for va, s, isf in rows:
        if not isf or _skip_sym(s):
            continue
        cm = demangle_cm(s)
        if cm:
            idx.setdefault(cm, []).append((s, va))
    return idx


def func_map():
    """stock va -> standalone va for the hooked functions (cached)"""
    global _fmap
    if _fmap is not None:
        return _fmap
    hsym, hbyva, _ = parse_map(HOOK_MAP)
    ssym, sbyva, _ = parse_map(SA_MAP)
    sidx = _sa_index()
    fm = {}
    for sva, name, hva in hook_log():
        cands = [s for s in hbyva.get(hva, []) if not _skip_sym(s)]
        got = {}
        for s in cands:
            if s in ssym:
                got.setdefault(ssym[s], []).append(s)
        nm = name.split('(')[0].strip()
        if len(got) == 1:
            fm[sva] = next(iter(got))
            continue
        if len(got) > 1:
            # several symbols at the hook address with different standalone addresses: the log name picks the right one
            pick = [a for a, ss in got.items() if any(demangle_cm(s) and ('%s::%s' % demangle_cm(s)) == nm for s in ss)]
            if len(pick) == 1:
                fm[sva] = pick[0]
                continue
            _funmapped[sva] = ('ambiguous', name, sorted(got))
            continue
        # no symbol of the hook address exists in the standalone: the registration target is a hook-only wrapper
        # (Fwd<> of a method, NwCtor/NwDtor Run): find the method by class, name and signature
        mm = re.match(r'^(?:.*::)?(\w+)::(~?\w+)$', nm)
        if not mm:
            _funmapped[sva] = ('no standalone symbol', name, hbyva.get(hva, [])[:3])
            continue
        cls, meth = mm.group(1), mm.group(2)
        found = None
        why = 'no standalone symbol'
        for s in hbyva.get(hva, []):
            if s.startswith('??$Fwd'):
                want = _wrapper_params(s, 0)
                c = [(x, v) for x, v in sidx.get((cls, meth), []) if _params(x) == want]
                if not c:
                    # a method inherited from a base class is declared there: any class, same name and signature
                    c = [(x, v) for (cc, mt), lst in sidx.items() if mt == meth for x, v in lst if _params(x) == want]
                if len({v for _, v in c}) == 1:
                    found = c[0][1]
                    break
                why = 'method not found by signature (%s::%s%s)' % (cls, meth, want)
                continue
            if s.startswith('?Run@NwDtor'):
                c = sidx.get((cls, '~' + cls), [])
                if len({v for _, v in c}) == 1:
                    found = c[0][1]
                    break
                why = 'destructor candidates %d' % len({v for _, v in c})
                continue
            if s.startswith('?Run@NwCtor'):
                want = _wrapper_params(s, 1)
                if want and want[0] == 'void*':
                    want = want[1:]
                c = sidx.get((cls, cls), [])
                c2 = [(x, v) for x, v in c if _params(x) == want]
                if len({v for _, v in c2}) == 1:
                    found = c2[0][1]
                    break
                if len({v for _, v in c}) == 1 and not c2:
                    found = c[0][1]
                    break
                why = 'constructor candidates %d (args %s)' % (len({v for _, v in c}), want)
                continue
            if s in ssym:
                found = ssym[s]
                break
        if found is None and meth.startswith('~'):
            # no separate destructor: the standalone folds it into the scalar deleting destructor; a trampoline calls that with flags = 0
            g = ssym.get('??_G%s@@UAEPAXI@Z' % cls)
            if g is not None:
                found = tramp_for(g)
        if found:
            fm[sva] = found
        else:
            _funmapped[sva] = (why, name, cands[:3])
    for sva, sym in OVERRIDES.items():
        # a stock function that the standalone has under another name (explicit; the evidence is in OVERRIDES)
        if sva not in fm and sym in ssym:
            fm[sva] = ssym[sym]
            _funmapped.pop(sva, None)
    _fmap = fm
    return fm


# stock va -> standalone symbol, for hooked functions that func_map() cannot find by class/method/signature but that the standalone has under
# another name. Every entry names its evidence.
OVERRIDES = {
    # NItemMapByKey::FindByKey is the stock's TMapBase::FindRef (one code serves every map of that layout); the source writes it as
    # NDataMap<NViewItemData>::FindRef / NMsnIntMap::FindRef and the linker folds the identical instances to one address (NWindow.map:
    # ?FindRef@NMsnIntMap@@QAEHABH@Z and ?FindRef@?$NDataMap@UNViewItemData@@@@QAEPAUNViewItemData@@ABH@Z are both 0x10018710; ret 4).
    0x101514a0: '?FindRef@NMsnIntMap@@QAEHABH@Z',
    # NShortCutItem::NShortCutItem(NItemInfo* A) in the smoke log: the C++ constructor takes `const NItemInfo&` (include/nwindow/NCShortCutWnd.h:54,
    # NWindow.map ??0NShortCutItem@@QAE@ABUNItemInfo@@@Z); a reference is the same 4-byte argument, so the pointer-vs-reference signature test misses it
    0x10108df0: '??0NShortCutItem@@QAE@ABUNItemInfo@@@Z',
    # NTVItemData::operator= : the log name is not a `Class::name` the matcher reads (operator=); the standalone has ??4NTVItemData@@QAEAAU0@ABU0@@Z
    # (NCTVItem.cpp, one copy-assignment, same `this` and one reference argument)
    0x10032c90: '??4NTVItemData@@QAEAAU0@ABU0@@Z',
}


TRAMP_BASE = 0x7E400000
_tramps = {}


def tramp_for(g_addr):
    """the address (in the emulated memory) of `push 0; call g_addr; ret`: the scalar deleting destructor run with flags = 0 is the destructor"""
    if g_addr not in _tramps:
        _tramps[g_addr] = TRAMP_BASE + 16 * len(_tramps)
    return _tramps[g_addr]


def tramps():
    func_map()
    return [(t, g) for g, t in _tramps.items()]


_rev = None


def rev_map():
    """standalone address -> [stock va] (the hooked functions that share it)"""
    global _rev
    if _rev is None:
        _rev = {}
        for sv, a in func_map().items():
            _rev.setdefault(a, []).append(sv)
    return _rev


def unmapped():
    func_map()
    return _funmapped


class NoStandaloneFunction(KeyError):
    """a stock function that the standalone does not have as a function of its own (inlined, folded, a template instance, hook-only):
    the test of that function cannot run in the standalone mode (a test catches it and prints skip())"""

    def __init__(self, va):
        why = _funmapped.get(va)
        KeyError.__init__(self, va)
        self.va = va
        self.why = ('%s (%s)' % (why[0], why[1])) if why else 'not a hooked function of the smoke log'

    def __str__(self):
        return 'stock function %08x has no standalone function: %s' % (self.va, self.why)


class LogMap(dict):
    """log_map() of the standalone mode: stock va -> standalone address; a missing function raises NoStandaloneFunction"""

    def __missing__(self, va):
        raise NoStandaloneFunction(va)


def skip(label, ex=None):
    """one line the suite runner counts: the function `label` was not tested in the standalone mode"""
    print('SKIP %s: %s' % (label, ex if ex is not None else 'no standalone function'))


# ---- globals ----------------------------------------------------------------------------------------------------------------
_gl = None
_glkeys = None
_inv = None
_sa_pe = None


def _image_ptr(va):
    """the dword at address `va` in the standalone image as the linker wrote it (a reference variable holds a relocated address)"""
    global _sa_pe
    if _sa_pe is None:
        import pefile
        _sa_pe = pefile.PE(SA_DLL)
        _sa_pe.mem = _sa_pe.get_memory_mapped_image()
    import struct
    base = _sa_pe.OPTIONAL_HEADER.ImageBase
    return struct.unpack('<I', _sa_pe.mem[va - base:va - base + 4])[0]


def globals_table():
    """sorted [(stock va, size, standalone va, name)]: the named globals of NwGlobals (tools/gen_globals.py scan()), the UClass objects
    (PrivateStaticClass) and the vtables of the classes (analysis/classes.json static_class / vtable)"""
    global _gl, _glkeys
    if _gl is not None:
        return _gl
    sys.path.insert(0, os.path.join(ROOT, 'tools'))
    import gen_globals
    found = gen_globals.scan()
    ssym, sbyva, rows = parse_map(SA_MAP)
    starts = sorted({r[0] for r in rows})
    out = []
    gsym = {}      # global name -> [(symbol, va)] of the symbols `?name@@3...` (one pass over the map instead of one regex scan per global: 13 s -> <1 s)
    for s, v in ssym.items():
        m = re.match(r'^\?([A-Za-z0-9_]+)@@3', s)
        if m:
            gsym.setdefault(m.group(1), []).append((s, v))
    for name, g in found.items():
        cands = gsym.get(name, [])
        if len(cands) != 1:
            continue
        sv = cands[0][1]
        i = bisect.bisect_right(starts, sv)
        nxt = starts[i] if i < len(starts) else sv + 4
        if re.match(r'^\?%s@@3AA' % re.escape(name), cands[0][0]):
            # the name is a REFERENCE (a 4-byte variable that holds the address of the object, e.g. GHtmlLinkTarget -> a member of the
            # static NHtmlLinkChain): the stock address stands for the object the reference refers to (its size: gen_globals.SPEC)
            sv = _image_ptr(sv)
            nxt = sv + (gen_globals.SPEC.get(name, {}).get('size') or 4)
        out.append((g.va, nxt - sv, sv, name))
    for c in json.load(open(os.path.join(ROOT, 'analysis', 'classes.json'))):
        cpp = c['cpp']
        sc = ssym.get('?PrivateStaticClass@%s@@0VUClass@@A' % cpp)
        if sc is not None and c.get('static_class'):
            out.append((c['static_class'], 0x530, sc, 'PrivateStaticClass@' + cpp))
        vt = ssym.get('??_7%s@@6B@' % cpp)
        if vt is not None and c.get('vtable'):
            out.append((c['vtable'], 4 * len(c['vtable_slots']), vt, 'vtable@' + cpp))
    out.sort()
    _gl = out
    _glkeys = [x[0] for x in out]
    return out


def _cover(va):
    t = globals_table()
    i = bisect.bisect_right(_glkeys, va) - 1
    if i >= 0:
        sva, size, sa, name = t[i]
        nxt = t[i + 1][0] if i + 1 < len(t) else sva + size
        ext = max(4, min(size, nxt - sva)) if size else 4
        if va < sva + ext:
            return t[i]
    return None


def xlate_va(va):
    """a stock address (a hooked function, or inside a named global / class object / vtable) -> the standalone address"""
    fm = func_map()
    if va in fm:
        return fm[va]
    g = _cover(va)
    if g:
        return g[2] + (va - g[0])
    raise NoCounterpart('stock address %08x has no standalone counterpart' % va)


_scratch = {}


def inv(addr):
    """a standalone address inside a named global / class object / vtable / function -> (stock va, name) or None"""
    global _inv
    if _inv is None:
        globals_table()
        _inv = sorted((x[2], x[1], x[0], x[3]) for x in _gl)
    ks = [x[0] for x in _inv]
    i = bisect.bisect_right(ks, addr) - 1
    if i >= 0:
        sa, size, sv, name = _inv[i]
        if addr < sa + max(4, size):
            return (sv + (addr - sa), name)
    r = rev_map().get(addr)
    if r:
        return (r[0], 'func')
    return None


# ---- the unicorn proxy ------------------------------------------------------------------------------------------------------
class SaUc:
    """The Uc of the standalone side: an access inside the stand-in region of the hook mode (NWBASE + rva) is redirected to the
    standalone object the stock address names. Everything else passes through."""

    def __init__(self, uc, nwbase):
        object.__setattr__(self, '_uc', uc)
        object.__setattr__(self, '_nw', nwbase)
        object.__setattr__(self, '_nwhi', nwbase + 0x400000)

    def __getattr__(self, name):
        return getattr(self._uc, name)

    def _x(self, addr):
        if self._nw <= addr < self._nwhi:
            return xlate_va(STOCK_BASE + addr - self._nw)
        return addr

    def mem_write(self, addr, data, *a):
        try:
            return self._uc.mem_write(self._x(addr), data, *a)
        except NoCounterpart:
            # a logging stub (push imm32; ret) for a stock function the standalone does not have as a function (inlined, folded, template
            # copy): nothing to patch, the call is not made as such (the test shows the difference, if the stock side logs it)
            if len(data) == 6 and data[0] == 0x68 and data[5] == 0xC3:
                UNSTUBBED.add(STOCK_BASE + addr - self._nw)
                return None
            if not LENIENT:
                raise
            note_missing(addr, 'w', len(data))

    def mem_read(self, addr, size, *a):
        try:
            return self._uc.mem_read(self._x(addr), size, *a)
        except NoCounterpart:
            if not LENIENT:
                raise
            note_missing(addr, 'r', size)
            return bytearray(size)


LENIENT = os.environ.get('NWD_SA_LENIENT') == '1'
MISSING = {}
UNSTUBBED = set()


def note_missing(addr, how, size):
    va = STOCK_BASE + addr - 0x50000000
    MISSING.setdefault(va, set()).add(how)


def _report_missing():
    if MISSING:
        print('SA-MISSING (stock addresses without a standalone counterpart; %d): %s' % (
            len(MISSING), ' '.join('%08x%s' % (a, ''.join(sorted(h))) for a, h in sorted(MISSING.items()))), file=sys.stderr)
    if UNSTUBBED and os.environ.get('NWD_SA_NOTES'):
        print('SA-UNSTUBBED (callee stubs not placed, no standalone function; %d): %s' % (
            len(UNSTUBBED), ' '.join('%08x' % a for a in sorted(UNSTUBBED))), file=sys.stderr)


import atexit
atexit.register(_report_missing)


def gptr(emu, va):
    """the pointer VALUE a test hands to ours for the stock address va (a UClass, a table, a function): in the hook mode the stand-in
    region, in the standalone the object itself; an address that is no object of ours (an opaque 'some other class') gets a scratch block"""
    try:
        return xlate_va(va)
    except NoCounterpart:
        if va not in _scratch:
            _scratch[va] = {}
        d = _scratch[va]
        key = id(emu)
        if key not in d:
            d[key] = emu.alloc(0x1000)
            emu.uc.mem_write(d[key], bytes(0x1000))
        return d[key]
