# Generic call tracer for differential tests of window code (stock vs ours under unicorn), written by the proof2 lane (wave C).
#
# The hand-made worlds of gw.py need a table of callees per test. Here every call that leaves the function under test is trapped by
# its ADDRESS instead:
#   stock : any instruction fetch in the stock .text outside the function, except the static CRT zone (0x10170000..0x10180000: the
#           float conversions, the memory helpers, the SEH prolog run for real on both sides)
#   ours  : an instruction fetch at the entry of one of our hooked functions (the "ours=" column of the smoke log) or at a
#           standalone copy of a hooked function (matched by name to the db), or anywhere in the NWBASE region (StockCallAt/NW_STOCK
#           go to NWBASE + rva: the stand-in of the stock image)
# The trapped call is logged as ('call', stock va, name, this, args) and answered by a rule (constructors return `this` and give the
# object its fake vtable; everything else returns 0 unless the test says otherwise). The virtual calls on the fake objects go
# through a per-class fake vtable whose slot i is a stub keyed by the STOCK function the real vtable has at slot i, so a virtual
# call of the stock and a devirtualised direct call of ours log the same entry when they reach the same function. A stock function
# that is only `mov eax,[ecx]; jmp [eax+disp]` (NCWnd::OffsetWindow) is resolved to the virtual function it jumps to, on both sides.
#
# Not compared: the CRT (real code on both sides), the engine imports that only compute (FString copies...); compared: the order,
# the target and the arguments of every call, the stores into every registered object, and the return value.
import sys, os, json, csv, struct, re, bisect
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import tip4          # clears the edit box service tables of e4 (must come first)
import gw
from gw import *
import e4
import stubs
import sa
from unicorn.x86_const import *
from unicorn import UC_HOOK_CODE
from capstone import Cs, CS_ARCH_X86, CS_MODE_32

ROOTP = ROOT
stubs.IMPORT_POP.update({
    '?Substitute@FString@@QAE?AV1@V1@0@Z': 28, '?GetName@User@@QAEPAGXZ': 0,
    'MultiByteToWideChar': 24, 'WideCharToMultiByte': 32, '?appAtoi@@YAHPBG@Z': 0, 'UrlCanonicalizeW': 16, 'GetVersionExA': 4, 'GetSystemInfo': 4,
    '?GetTransientPackage@UObject@@SAPAVUPackage@@XZ': 0,
    '?StaticAllocateObject@UObject@@SAPAV1@PAVUClass@@PAV1@VFName@@K1PAVFOutputDevice@@11@Z': 0,
    '?ConditionalDestroy@UObject@@QAEHXZ': 0,
    '?Printf@FString@@SA?AV1@PBGZZ': 0,
    '??9FString@@QBEHPBG@Z': 4,
    '?InStr@FString@@QBEHPBGH@Z': 8,
    '??8FString@@QBEHABV0@@Z': 4, '?Insert@FArray@@QAEXHHH@Z': 12,
    '?appQsort@@YAXPAXHHP6AHPBX1@Z@Z': 0,
    '??0L2ParamStack@@QAE@H@Z': 4, '?PushBack@L2ParamStack@@QAEHPAX@Z': 4, '?Top@L2ParamStack@@QAEPAXXZ': 0,
    '?Clear@L2ParamStack@@QAEXXZ': 0, '??1L2ParamStack@@QAE@XZ': 0, '??4L2ParamStack@@QAEAAV0@AAV0@@Z': 4, '??1UObject@@UAE@XZ': 0,
    '?Len@FString@@QBEHXZ': 0,
    '??0FString@@QAE@PBD@Z': 4,
    '?appRand@@YAHXZ': 0,
    '?appStrcmp@@YAHPBG0@Z': 0,
    '?appMemset@@YAXPAXHH@Z': 0,
    '?appMemcpy@@YAXPAXPBXH@Z': 0,
    '?appMemmove@@YAPAXPAXPBXH@Z': 0,
    '??0UObject@@QAE@XZ': 0,
})
VSTUB_BASE = 0x7C000000
VSTUB_SIZE = 0x100000
STOCK_TEXT = (0x10001000, 0x10170000)
CRT_ZONE = (0x10170000, 0x10180000)

_md = Cs(CS_ARCH_X86, CS_MODE_32)
_stock_img = None


def stock_bytes():
    global _stock_img
    if _stock_img is None:
        import pefile
        pe = pefile.PE(STOCK)
        _stock_img = (pe.OPTIONAL_HEADER.ImageBase, pe.get_memory_mapped_image())
    return _stock_img


_FUNCS = None


def functions():
    """db/functions.csv: va -> (size, name, class)"""
    global _FUNCS
    if _FUNCS is None:
        _FUNCS = {}
        for r in csv.DictReader(open(os.path.join(ROOTP, 'db', 'functions.csv'), encoding='utf-8')):
            _FUNCS[int(r['va'], 16)] = (int(r['size']), r['name'], r['class'])
    return _FUNCS


_CLASSES = None


def classes():
    """cpp name -> classes.json record"""
    global _CLASSES
    if _CLASSES is None:
        d = json.load(open(os.path.join(ROOTP, 'analysis', 'classes.json')))
        _CLASSES = {c['cpp']: c for c in d}
    return _CLASSES


_RETN = {}
# cdecl stock functions: how many dword arguments the caller pushes (the callee pops none)
ARGS_OVERRIDE = {
    0x100562f0: 1,          # CastToChatWnd(obj) (cdecl)
    0x101685f0: 1, 0x10168730: 1, 0x101687b0: 1, 0x10168810: 1,     # Util::GetLanguageID, MsnPlatformName, MsnSystemVersion, MsnProcessorName (hidden FString return)
    0x10168c60: 2,          # the UTF-8 encoder(text, buffer)
    0x10168be0: 2,          # MsnWideToAnsi(text, buffer)
    0x10012960: 8,          # NCGDevice::GetTextExtent(SIZE*, text, ...)
    0x100134b0: 3,          # NCGDevice::GetCharSize(SIZE*, char, font)
}
# stock functions that are only a jump through another object's vtable (no ret of their own): bytes they pop
# the Winsock thunks of the stock image (jmp [IAT]); in the CRT zone, so trapped on purpose here: (name, bytes popped)
WS_THUNKS = {
    0x10170b02: ('WSAGetLastError', 0), 0x10170b08: ('connect', 12), 0x10170b0e: ('htons', 4), 0x10170b14: ('gethostbyname', 4),
    0x10170b1a: ('inet_addr', 4), 0x10170b20: ('socket', 12), 0x10170b26: ('WSAStartup', 8), 0x10170b2c: ('recv', 16),
    0x10170b32: ('send', 16), 0x10170b38: ('WSAAsyncSelect', 16), 0x10170b3e: ('htonl', 4), 0x10170b44: ('closesocket', 4),
}
# the standalone calls the same functions as imports of ws2_32 (by name, stdcall): do_import logs the entry the stock thunk call logs and answers with the same rule
WS_NAMES = {nm: va for va, (nm, _pop) in WS_THUNKS.items()}
stubs.IMPORT_POP.update({nm: pop for nm, pop in WS_THUNKS.values()})
POP_OVERRIDE = {
    0x10013330: 0x10,       # NCGDevice::SetClip(x, y, w, h): [this+0x34] vslot 71
    0x10013340: 0x0,        # NCGDevice::ReleaseClip(): [this+0x34] vslot 36
    0x10148df0: 0x0,        # L2ChatWnd::ClearAll: ends with jmp [edit box vtable + 0x130] = NCEditBox slot 76 (ret 0)
    0x1006ca80: 0x8,        # UL2ConsoleWnd::RequestSendL2FriendSay(name, text): a vslot-272 wrapper of the console, two arguments (see L2FriendWnd::SendWhisper)
    0x101451d0: 0x0,        # AddFriendDialog::ShowWindow
    0x10168c60: 0x0,        # the UTF-8 encoder (cdecl)
    0x10069ff0: 4,          # NConsoleWnd::BypassUserCmd(L2ParamStack&): a wrapper of the net handler's virtual 262
    0x10068d40: 4,          # NConsoleWnd::RequestBypassToServer(L2ParamStack&): virtual 105 of the net handler
    0x10067f60: 4,          # NConsoleWnd::RequestExOustFromMPCC(L2ParamStack&): virtual 191
    0x1002e4a0: 4,          # NCTabControl::IsPageSplit(page) (the asm export ends in a far chunk)
    0x100efdc0: 0,          # NRecipeBuyListWndExt::ClearList(): jmp [list vtable + 0x134]
    0x1006a8e0: 4,          # NConsoleWnd::GetPledgeCrestTexture(id): a wrapper of the net handler's virtual 43
    0x1006a8f0: 4,          # NConsoleWnd::GetAllyCrestTexture(id): virtual 44
    0x1fff0036: 0x0,        # test only: NetHandler::RefreshWSAAsyncSelect (engine vslot 54 of the net handler, no argument): ends with jmp [edit box vtable + 0x130] = NCEditBox slot 76 (ret 0)
}


def ret_n(va):
    """bytes the stock function at va pops on return (last `ret imm16` of its body; 0 when it ends with a plain ret)"""
    if va in _RETN:
        return _RETN[va]
    _RETN[va] = None                    # a cycle of tail jumps ends here
    n = None
    asm =os.path.join(ROOTP, 'analysis', 'ghidra', 'asm', '%08x.asm' % va)
    if os.path.exists(asm):
        # Ghidra's export holds every chunk of the function (a split function keeps its epilogue elsewhere)
        for ln in open(asm, encoding='utf-8', errors='replace'):
            m = re.match(r'^[0-9a-f]{8}\s+[0-9a-f]+\s+RET(?:\s+(0x[0-9a-f]+))?\s*$', ln)
            if m:
                n = int(m.group(1), 16) if m.group(1) else 0
    else:
        base, mem = stock_bytes()
        size = functions().get(va, (0, '', ''))[0] or 0x60
        code = bytes(mem[va - base:va - base + size])
        for ins in _md.disasm(code, va):
            if ins.mnemonic == 'ret':
                n = int(ins.op_str, 16) if ins.op_str else 0
    if n is None and os.path.exists(asm):
        n = tail_pop(va, asm)
    _RETN[va] = n
    return n


_SLOTPOP = {}


def slot_pop(slot):
    """bytes popped by the virtual function at `slot` of the window classes, when every class that has the slot agrees"""
    if slot not in _SLOTPOP:
        _SLOTPOP[slot] = None           # a slot whose functions tail-jump to the same slot ends here
        vals = set()
        for c in classes().values():
            if 'NCWnd' not in c.get('ancestors', []) and c['cpp'] != 'NCWnd':
                continue
            for s in c['vtable_slots']:
                if s['slot'] == slot and not s.get('import') and 0x10001000 <= s['va'] < 0x10170000:
                    vals.add(ret_n(s['va']))
        vals.discard(None)
        _SLOTPOP[slot] = vals.pop() if len(vals) == 1 else None
    return _SLOTPOP[slot]


def tail_pop(va, asm):
    """a function that ends in a tail jump pops what its target pops (`jmp rel32`, or `jmp [eax+disp]` = a virtual slot)"""
    rng = stock_ranges(va, 1)
    for ln in open(asm, encoding='utf-8', errors='replace'):
        m = re.match(r'^[0-9a-f]{8}\s+[0-9a-f]+\s+JMP\s+(0x[0-9a-f]{8})\b', ln)
        if m:
            t = int(m.group(1), 16)
            if not any(lo <= t < hi for lo, hi in rng) and 0x10001000 <= t < 0x10170000:
                return ret_n(t)
        m = re.match(r'^[0-9a-f]{8}\s+[0-9a-f]+\s+JMP\s+dword ptr \[E[A-Z]X \+ (0x[0-9a-f]+)\]\s*$', ln)
        if m:
            return slot_pop(int(m.group(1), 16) // 4)
    return None


_RANGES = {}


def stock_ranges(va, size):
    """the address ranges of a stock function (Ghidra's header line `; FUN_va  lo-hi,lo-hi`; a function can have a far epilogue)"""
    if va in _RANGES:
        return _RANGES[va]
    out = []
    asm = os.path.join(ROOTP, 'analysis', 'ghidra', 'asm', '%08x.asm' % va)
    if os.path.exists(asm):
        head = open(asm, encoding='utf-8', errors='replace').readline()
        m = re.match(r'^; \S+\s+(.*)$', head)
        if m:
            for part in m.group(1).split(','):
                mm = re.match(r'^([0-9a-f]{8})-([0-9a-f]{8})$', part.strip())
                if mm:
                    out.append((int(mm.group(1), 16), int(mm.group(2), 16) + 1))
    if not out:
        out = [(va, va + size)]
    # a function that Ghidra cut at a `nop` (the body goes on at the next address, listed as another function of the db that nobody ported):
    # the chunks after it are part of this function
    asm_c = os.path.join(ROOTP, 'analysis', 'ghidra', 'asm', '%08x.asm' % va)
    if os.path.exists(asm_c):
        last = [ln for ln in open(asm_c, encoding='utf-8', errors='replace') if re.match(r'^[0-9a-f]{8}\s', ln)]
        if last and re.match(r'^[0-9a-f]{8}\s+[0-9a-f]+\s+NOP\s*$', last[-1]):
            hi = max(h for _, h in out)
            nxt = _NEXT_TODO().get(hi)
            if nxt:
                out.extend(stock_ranges(hi, nxt))
    # the fragments the db lists as `part of <va>` (appMemcpy's movsb sled, entered by jmp ecx, and the non-SSE copy after it): part of this function, up to the next one
    for fva, fsz in _FRAGMENTS().get(va, ()):
        starts = sorted(k for k in functions() if k > fva)
        out.append((fva, max(fva + fsz, starts[0] if starts else fva + fsz)))
    _RANGES[va] = out
    return out


_TODO = None


_FRAG = None


def _FRAGMENTS():
    """parent va -> [(fragment va, size)] for the db rows whose note says `part of <va>`"""
    global _FRAG
    if _FRAG is None:
        _FRAG = {}
        for r in csv.DictReader(open(os.path.join(ROOTP, 'db', 'functions.csv'), encoding='utf-8')):
            m = re.match(r'^part of ([0-9a-f]{8})', r['notes'] or '')
            if m:
                _FRAG.setdefault(int(m.group(1), 16), []).append((int(r['va'], 16), int(r['size'])))
    return _FRAG


def _NEXT_TODO():
    """db rows that nobody ported (status todo / part): start -> size"""
    global _TODO
    if _TODO is None:
        _TODO = {}
        for r in csv.DictReader(open(os.path.join(ROOTP, 'db', 'functions.csv'), encoding='utf-8')):
            if r['status'] in ('todo', 'part'):
                _TODO[int(r['va'], 16)] = int(r['size'])
    return _TODO


def is_ctor_name(va):
    f = functions().get(va)
    if not f:
        return None
    nm, cl = f[1], f[2]
    m = re.match(r'^(\w+)::(\w+)$', nm)
    if m and m.group(1) == m.group(2):
        return m.group(1)
    return None


def demangle(sym):
    m = re.match(r'^\?\?([01])([A-Za-z0-9_]+)@@', sym)
    if m:
        c = m.group(2)
        return c + '::' + ('~' if m.group(1) == '1' else '') + c
    m = re.match(r'^\?([A-Za-z0-9_]+)@([A-Za-z0-9_]+)@@', sym)
    if m:
        return m.group(2) + '::' + m.group(1)
    return None


class TrapStubs(dict):
    """the emulator's stub table: any trapped address answers ('tr', ...)"""

    def __init__(self, owner, base):
        dict.__init__(self, base)
        self.owner = owner

    def __missing__(self, addr):
        if self.owner.trapping(addr):
            return ('tr:%x' % addr, None)
        raise KeyError(addr)


class VSet(set):
    """a set that counts its mutations (the interval cache of TR.trapping is rebuilt when it changes)"""
    ver = 0

    def _bump(self):
        self.ver += 1

    def add(self, x):
        self._bump(); set.add(self, x)

    def discard(self, x):
        self._bump(); set.discard(self, x)

    def remove(self, x):
        self._bump(); set.remove(self, x)

    def update(self, *a):
        self._bump(); set.update(self, *a)

    def __ior__(self, o):
        self._bump(); return set.__ior__(self, o)

    def __isub__(self, o):
        self._bump(); return set.__isub__(self, o)


class TR(GW):
    # the constructors of the db run for real on both sides (the standalone and the C4 hook build construct inline: new (mem) T(args)); NWD_TR_CTORS=0 gives the old stubbed constructors
    ctors_real = os.environ.get('NWD_TR_CTORS', '1') == '1'

    def __init__(self, kind, extra_cdecl_imports=None):
        GW.__init__(self, kind, {}, None)
        self.kind = kind
        self.cls_vt = {}
        self.vstub_of = {}          # target stock va -> stub address
        self.vstub_va = {}          # stub address -> target stock va
        self.uc.mem_map(VSTUB_BASE, VSTUB_SIZE)
        self.vstub_next = VSTUB_BASE
        self.perm_next = VSTUB_BASE + 0x80000
        self.rules = {}             # stock va -> function(e, ecx, args) -> eax
        self.transparent = VSet()   # stock vas that run for real on both sides (their own callees are still trapped): helpers that ours inlines
        self.alias = {}             # stock va -> the stock va it is reported as (functions the linker folded to one address: see fold())
        self.fn_lo = self.fn_hi = 0
        self.entry_sva = 0
        self.ranges = []
        self.skip = set()
        self.entry_va = 0
        self.trace_internal = False
        self.internal = []
        self.names = functions()
        self.stubs = TrapStubs(self, self.stubs)
        self.rev = {}
        self.nwbase_hi = NWBASE + 0x400000
        self.imp_pop_extra = {}
        self.reg_size = {}
        self.cls_objs = set()
        self.ps = {}
        if kind == 'ours':
            self.build_rev()
            sy = {}
            for nm, a in self.symbols.items():
                sy.setdefault(a, nm)
            self.addr_sym = sy
            self.sym_starts = sorted(sy)
        # the variables the engine exports: a cell each (the screen size is a plain number)
        self.gcell = {}
        for ee in self.pe.DIRECTORY_ENTRY_IMPORT:
            for imp in ee.imports:
                nm = imp.name.decode() if imp.name else ''
                if re.match(r'^\?\w+@@3', nm) and not (nm.startswith('?GIs') or nm.startswith('?GL2') or nm.startswith('?GMalloc')
                                                      or nm == '?GLanguageType@@3HA'):
                    cell = self.alloc(64)
                    self.uc.mem_write(cell, bytes(64))
                    self.write32(imp.address, cell)
                    self.gcell[nm] = cell
        for nm, v in (('?GSEKScreenX@@3HA', 1024), ('?GSEKScreenY@@3HA', 768)):
            if nm in self.gcell:
                self.write32(self.gcell[nm], v)
        self.gcell_rev = {c: n for n, c in self.gcell.items()}         # (val(): a pointer to an engine variable is named, not a heap number)
        # GConfig: a ChatWnd constructor reads [URL] MSNHideWarnMsg now that the constructors run for real. A default object (FConfigCache, include/ue2/UE2Core.h
        # slots 0..12) whose every slot logs the call (the fake stock va 0x1FFF0110 + slot) and finds nothing; a test that needs more sets its own object in the cell
        if '?GConfig@@3PAVFConfigCache@@A' in self.gcell:
            cfg = self.alloc(16)
            slots = {}
            for sl, (cn, cp) in enumerate((('GetBool', 0x10), ('GetInt', 0x10), ('GetFloat', 0x10), ('GetString', 0x10), ('GetString', 0x14), ('GetStr', 0xC),
                                           ('GetSection', 0x10), ('GetSectionPrivate', 0x10), ('EmptySection', 8), ('SetBool', 0x10), ('SetInt', 0x10),
                                           ('SetFloat', 0x10), ('SetString', 0x10))):
                slots[sl] = 0x1FFF0110 + sl
                POP_OVERRIDE[0x1FFF0110 + sl] = cp
                self.names[0x1FFF0110 + sl] = (0, 'FConfigCache::' + cn, '')
            self.write32(cfg, self.perm_vt(slots))
            self.write32(self.gcell['?GConfig@@3PAVFConfigCache@@A'], cfg)
        self.uc.hook_add(UC_HOOK_CODE, self.on_trap, None, 0x10000000, 0x10400000)
        self.uc.hook_add(UC_HOOK_CODE, self.on_trap, None, NWBASE, NWBASE + 0x400000)
        self.uc.hook_add(UC_HOOK_CODE, self.on_trap, None, VSTUB_BASE, VSTUB_BASE + VSTUB_SIZE)
        if self.ctors_real:
            self.make_ctors_real()

    def make_ctors_real(self):
        """The constructors run for real on both sides (ours inlines them: new (mem) T(args)); every vtable of the image (the stock's and
        ours) is overwritten with the fake one of its class, so that a constructor that writes the real vtable pointer still gives the
        object the fake, trapping slots."""
        self._auto_ctors = {va for va in functions() if is_ctor_name(va)}
        self.transparent.update(self._auto_ctors)
        for cpp, c in classes().items():
            try:
                vt = self.class_vt(cpp)
            except Exception:
                continue
            n = len(c['vtable_slots'])
            if self.kind == 'stock':
                real = c.get('vtable')
            else:
                real = self.symbols.get('??_7%s@@6B@' % cpp)
            if real:
                self.uc.mem_write(real, bytes(self.uc.mem_read(vt, 4 * n)))

    # ---- the map stock va <-> our address ---------------------------------------------------------------------------
    def follow(self, x):
        chain = [x]
        for _ in range(5):
            b = bytes(self.uc.mem_read(x, 12))
            if b[0] == 0xE9:
                x = (x + 5 + struct.unpack('<i', b[1:5])[0]) & 0xFFFFFFFF
            elif b[:4] == bytes([0x55, 0x8B, 0xEC, 0x5D]) and b[4] == 0xE9:
                x = (x + 9 + struct.unpack('<i', bytes(self.uc.mem_read(x + 5, 4)))[0]) & 0xFFFFFFFF
            else:
                break
            chain.append(x)
        return chain

    no_guess_ctor = False
    freed = set()

    def build_rev(self):
        lm = log_map()
        self.hook_of = lm
        for sva, a in lm.items():
            for x in self.follow(a):
                self.rev.setdefault(x, sva)
        # the Control Flow Guard no-op of our image (`ret 0`, folded with every empty function): not a call of the hooked function it is mapped to
        # standalone copies: our symbol Class::Method that is the name of one stock function of the db (unique)
        byname = {}
        for va, (sz, nm, cl) in functions().items():
            byname.setdefault(nm, []).append(va)
        self.byname = byname
        for nm, a in self.symbols.items():
            if a in self.rev:
                continue
            if self.no_guess_ctor and (nm.startswith('??0') or nm.startswith('??1')):
                continue            # (t_msn7.py) the db names one stock body NMsnFriend / NChatUser...: a constructor is not guessed by name there
            dm = demangle(nm)
            if dm and dm in byname and len(byname[dm]) == 1 and byname[dm][0] in lm:
                self.rev[a] = byname[dm][0]
        # the Control Flow Guard check of our image calls through __guard_check_icall_fptr, a `ret 0` that the linker folded with the empty
        # hooked functions: it is not a call of the function the map gives that address to
        g = self.symbols.get('___guard_check_icall_fptr')
        if g:
            nop = self.read32(g)
            self.rev.pop(nop, None)

    # ---- trapping ----------------------------------------------------------------------------------------------------
    def trapping(self, addr):
        if VSTUB_BASE <= addr < VSTUB_BASE + VSTUB_SIZE:
            return True
        if self.kind == 'stock':
            if addr in WS_THUNKS:
                return True
            if not (STOCK_TEXT[0] <= addr < STOCK_TEXT[1]):
                return False
            for lo, hi in self.ranges:
                if lo <= addr < hi:
                    return False
            return not self.in_transparent(addr)
        if NWBASE <= addr < self.nwbase_hi:
            v = 0x10000000 + addr - NWBASE
            if v in self.transparent:
                return False
            return not self.in_transparent(v)           # a transparent stock function that ours calls through the stand-in runs for real: its whole body is not trapped
        return (addr in self.rev and addr not in self.skip and self.alias.get(self.rev[addr], self.rev[addr]) not in self.transparent
                and self.rev[addr] != self.entry_sva)      # (the forwarder of a by-value hook calls the body through the same stock va)

    def fold(self, canon, *others, transparent=False):
        """Functions the linker folded to ONE address of ours (identical bodies, e.g. NMsnGroup::~NMsnGroup and NMsnPassport::~NMsnPassport) are
        ambiguous in the trap tables: the address maps to one of the stock vas, the stock side calls the real one. A test names the set: every
        call of `others` is reported as a call of `canon`, on both sides (transparent=True: they all run for real)."""
        for o in others:
            self.alias[o] = canon
        if transparent:
            self.transparent.update((canon,) + others)

    @property
    def transparent(self):
        return self._transparent

    @transparent.setter
    def transparent(self, v):
        # (a test assigns a plain set: it is wrapped so that the interval cache sees every later change)
        self._transparent = v if isinstance(v, VSet) else VSet(v)
        self._transparent.update(getattr(self, '_auto_ctors', ()))       # (the constructors stay real when a test assigns its own set)
        self._tcache = (-1, [], [])

    def in_transparent(self, a):
        """a is inside the body of a transparent stock function"""
        ver = self.transparent.ver
        if self._tcache[0] != ver:
            iv = []
            for t in self.transparent:
                iv.extend(stock_ranges(t, 1))
            iv.sort()
            self._tcache = (ver, iv, [x[0] for x in iv])
        _, iv, los = self._tcache
        i = bisect.bisect_right(los, a) - 1
        # ranges of different functions may nest or touch: look back over the few that start before a
        j = i
        while j >= 0 and j > i - 4:
            if iv[j][0] <= a < iv[j][1]:
                return True
            j -= 1
        return False

    def on_trap(self, uc, address, size, user):
        if self.kind == 'ours' and self.trace_internal and address in self.addr_sym:
            self.internal.append(self.addr_sym[address])
        if self.trapping(address):
            self.pending = address
            uc.emu_stop()

    def target_va(self, addr):
        if VSTUB_BASE <= addr < VSTUB_BASE + VSTUB_SIZE:
            return self.vstub_va[addr & ~15]
        if self.kind == 'stock':
            return addr
        if NWBASE <= addr < self.nwbase_hi:
            return 0x10000000 + (addr - NWBASE)
        return self.rev[addr]

    def vstub(self, target):
        if target not in self.vstub_of:
            a = self.vstub_next
            self.vstub_next += 16
            self.uc.mem_write(a, b'\xC3')
            self.vstub_of[target] = a
            self.vstub_va[a] = target
        return self.vstub_of[target]

    # ---- fake classes and objects -------------------------------------------------------------------------------------
    def class_vt(self, cpp):
        if cpp not in self.cls_vt:
            c = classes()[cpp]
            slots = c['vtable_slots']
            vt = self.perm_next
            self.perm_next += 4 * (len(slots) + 4)
            self.uc.mem_write(vt, bytes(4 * (len(slots) + 4)))
            for s in slots:
                self.write32(vt + 4 * s['slot'], self.vstub(s['va']))
            self.cls_vt[cpp] = vt
        return self.cls_vt[cpp]

    def perm_vt(self, slots, n=80):
        """a fake vtable of n slots whose given slots are stubs keyed by fake stock vas ({slot: va})"""
        vt = self.perm_next
        self.perm_next += 4 * (n + 4)
        self.uc.mem_write(vt, bytes(4 * (n + 4)))
        for sl, va in slots.items():
            self.write32(vt + 4 * sl, self.vstub(va))
        return vt

    def new_obj(self, cpp, label=None, size=None):
        c = classes()[cpp]
        n = size or c['size']
        p = self.alloc(n + 16)
        self.uc.mem_write(p, bytes(n + 16))
        self.write32(p, self.class_vt(cpp))
        self.objs[p] = label or '%s#%d' % (cpp, len(self.objs))
        self.reg_size[p] = n
        self.cls_objs.add(p)
        return p

    def raw_obj(self, label, size):
        p = self.alloc(size + 16)
        self.uc.mem_write(p, bytes(size + 16))
        self.objs[p] = label
        self.reg_size[p] = size
        return p

    def w32(self, p, off, v):
        self.write32(p + off, v & 0xFFFFFFFF)

    def r32(self, p, off):
        return self.read32(p + off)

    def wf(self, p, off, x):
        self.write32(p + off, struct.unpack('<I', struct.pack('<f', float(x)))[0])

    # ---- normalising values for the log --------------------------------------------------------------------------------
    def val(self, v):
        if v is None or isinstance(v, tuple):
            return v
        if v in self.objs:
            return self.objs[v]
        if v in self.ps:
            return ('ParamStack', [self.val(x) for x in self.ps[v]])
        if v in self.gcell_rev:
            return 'GV:' + self.gcell_rev[v]
        if NWBASE <= v < self.nwbase_hi:
            return 'G%x' % (0x10000000 + v - NWBASE)
        if VSTUB_BASE <= v < VSTUB_BASE + VSTUB_SIZE:
            if (v & ~15) in self.vstub_va:
                return 'vstub%x' % self.vstub_va[v & ~15]
            return 'vtbl'
        if 0x10000000 <= v < 0x10400000:
            if self.kind == 'ours' and v in self.rev:
                return 'F%x' % self.rev[v]
            try:
                s = self.wstr(v, 80)
                if s is not None and s != '' and all(ord(c) >= 9 and ord(c) != 11 and ord(c) != 12 and ord(c) < 0x250 for c in s):
                    return 'S:' + s
            except Exception:
                pass
            if sa.ENABLED and self.looks_vtable(v):
                return 'vt'                 # a vtable of a class that is not a UObject (Msn ...): its address is the build's, not a result
            if self.kind == 'stock':
                return 'F%x' % v if v in functions() else 'G%x' % v
            if sa.ENABLED:
                r = sa.inv(v)               # a pointer into one of the standalone's own globals: named by the stock address of that object
                if r and r[1] != 'func':
                    return 'G%x' % r[0]
            if os.environ.get('TR_DEBUGG'):
                i = bisect.bisect_right(self.sym_starts, v) - 1
                print('TR: unresolved pointer %08x in ours: %s +%x' % (v, self.addr_sym.get(self.sym_starts[i]) if i >= 0 else '?', v - self.sym_starts[i] if i >= 0 else 0))
            return 'G?'
        if 0x2EF00000 <= v < 0x2F100000:
            return 'stack'
        if 0x60000000 <= v < 0x68000000:
            ins = self.inside(v)
            if ins:
                return ins
        if 0x60000000 <= v < 0x68000000:
            try:
                s = self.wstr(v, 80)
                if s is not None and s != '' and all(ord(c) >= 9 and ord(c) != 11 and ord(c) != 12 and ord(c) < 0x250 for c in s):
                    return 'S:' + s
            except Exception:
                pass
        return v

    def looks_vtable(self, v):
        """v points at a table whose first entry is a function of the image or one of the harness' vstubs"""
        try:
            f = self.read32(v)
        except Exception:
            return False
        return (0x10001000 <= f < 0x10300000 and (v >= 0x10190000 or self.kind == 'ours')) or VSTUB_BASE <= f < VSTUB_BASE + VSTUB_SIZE

    def inside(self, v):
        """'label+0xoff' when v points into a registered object"""
        for p, n in self.reg_size.items():
            if p < v < p + n:
                return '%s+0x%x' % (self.objs.get(p, '?'), v - p)
        return None

    def nmthis(self, ecx):
        if ecx in self.objs:
            return self.objs[ecx]
        return self.inside(ecx) or '?'

    # ---- the dispatcher --------------------------------------------------------------------------------------------------
    def dispatch(self, name):
        kind, nm = name.split(':', 1)
        if kind == 'tr':
            return self.do_trap(int(nm, 16))
        if kind == 'mf' and nm == 'malloc':
            self.last_pop = 8
            size = self.sargs(1)[0]
            p = self.alloc(max(size, 16))
            self.uc.mem_write(p, bytes(max(size, 16)))
            self.objs[p] = 'new%d' % size
            self.reg_size[p] = size
            self.log.append(('GMalloc.Malloc', size))
            return p
        return GW.dispatch(self, name)

    def do_trap(self, addr):
        esp = self.reg(UC_X86_REG_ESP)
        va = self.target_va(addr)
        ecx = self.reg(UC_X86_REG_ECX)
        if self.kind == 'stock' and CRT_ZONE[0] <= addr < CRT_ZONE[1] and addr not in WS_THUNKS:
            raise RuntimeError('trapped CRT zone %x' % addr)
        va = self.resolve(va, ecx)
        va = self.alias.get(va, va)
        if va in WS_THUNKS:
            pop = WS_THUNKS[va][1]
        else:
            pop = POP_OVERRIDE.get(va, ret_n(va))
        if pop is None:
            raise RuntimeError('no ret in the stock function %x %s: add it to POP_OVERRIDE' % (va, self.names.get(va, (0, '?', ''))[1]))
        n = ARGS_OVERRIDE.get(va, pop // 4)
        args = [self.read32(esp + 4 + 4 * i) for i in range(n)]
        self.last_pop = pop
        nm = WS_THUNKS[va][0] if va in WS_THUNKS else self.names.get(va, (0, 'sub_%x' % va, ''))[1]
        # (a Winsock function has no `this`: ecx is whatever the caller had, the stock's and the standalone's differ)
        self.log.append(('call', '%x' % va, nm, '-' if (va in ARGS_OVERRIDE or va in WS_THUNKS) else self.nmthis(ecx), [self.val(a) for a in args]))
        return self.rule(va, ecx, args)

    def resolve(self, va, ecx):
        """a stock function that is only `mov eax,[ecx]; jmp [eax+disp]` is the virtual call it jumps to"""
        if not (0x10001000 <= va < 0x10300000):
            return va
        base, mem = stock_bytes()
        b = bytes(mem[va - base:va - base + 8])
        disp = None
        if b[:2] == b'\x8b\x01':
            if b[2:4] == b'\xff\xa0':
                disp = struct.unpack('<I', b[4:8])[0]
            elif b[2:4] == b'\xff\x60':
                disp = b[4]
        if b[0] == 0xE9 and 0x10001000 <= va < 0x10170000:
            t = (va + 5 + struct.unpack('<i', b[1:5])[0]) & 0xFFFFFFFF
            if 0x10001000 <= t < 0x10170000:
                return self.resolve(t, ecx)
        if disp is not None and ecx in self.objs:
            vt = self.read32(ecx)
            t = self.read32(vt + disp)
            if VSTUB_BASE <= t < VSTUB_BASE + VSTUB_SIZE:
                return self.vstub_va[t & ~15]
        return va

    def rule(self, va, ecx, args):
        if va in self.rules:
            return self.rules[va](self, ecx, args)
        c = is_ctor_name(va)
        if c is not None:
            if c in classes():
                self.write32(ecx, self.class_vt(c))
            return ecx
        return 0

    # ---- the imports of the Core / Engine / Winsock side -------------------------------------------------------------------
    def do_import(self, nm):
        if nm in WS_NAMES:                       # Winsock (an import of the standalone; the stock calls a jmp thunk, trapped by do_trap): the same log entry, the same answer
            va = WS_NAMES[nm]
            args = [self.arg(i) for i in range(stubs.IMPORT_POP[nm] // 4)]
            self.log.append(('call', '%x' % va, nm, '-', [self.val(a) for a in args]))
            return self.rule(va, self.this(), args)
        if nm == '??1FString@@QAE@XZ':           # a destructor frees the buffer: the same buffer freed twice is a double free (a by-value argument the callee destroys, destroyed again)
            t = self.this()
            d, n = self.read32(t), self.read32(t + 4)
            if d and 0x60000000 <= d < 0x70000000:
                if d in self.freed:
                    try:
                        txt = self.wstr(d) or ''
                    except Exception:
                        txt = '?'
                    raise RuntimeError('FString buffer %x (%r) freed twice (the second ~FString, this=%s)' % (d, txt, self.nmthis(t)))
                self.freed.add(d)
            return t
        if nm == '?appAtoi@@YAHPBG@Z':            # _wtoi: leading blanks, an optional sign, the digits
            t = (self.wstr(self.arg(0)) or '').lstrip(' 	')
            m = re.match(r'^[+-]?[0-9]+', t)
            v = int(m.group(0)) if m else 0
            return v & 0xFFFFFFFF
        if nm == '?GetName@User@@QAEPAGXZ':      # the name buffer of the user record
            return self.this() + 0x1C
        if nm == '?Substitute@FString@@QAE?AV1@V1@0@Z':   # (hidden result, From by value, To by value): every From of this replaced by To; the callee destroys both arguments
            t, ret = self.this(), self.arg(0)
            fd, td = self.arg(1), self.arg(4)
            ftxt = (self.wstr(fd) or '') if fd else ''
            ttxt = (self.wstr(td) or '') if td else ''
            src = self.fs_get(t) or ''
            self.fs_set(ret, src.replace(ftxt, ttxt) if ftxt else src)
            for d in (fd, td):
                if d and 0x60000000 <= d < 0x70000000:
                    if d in self.freed:
                        raise RuntimeError('FString buffer %x freed twice (a by-value argument of Substitute destroyed by the callee and again by the caller)' % d)
                    self.freed.add(d)
            return ret
        if nm == 'UrlCanonicalizeW':             # (url, out, &length, flags): the spaces escaped; the call is logged (flags, length)
            url, out, plen, flags = self.arg(0), self.arg(1), self.arg(2), self.arg(3)
            ln = self.read32(plen)
            self.log.append(('UrlCanonicalizeW', flags, ln))
            t = (self.wstr(url) or '').replace(' ', '%20')
            self.uc.mem_write(out, t[:max(ln - 1, 0)].encode('utf-16le') + bytes(2))
            self.write32(plen, len(t))
            return 0
        if nm == 'InternetCanonicalizeUrlW':     # (url, out, &length, flags): the %XX escapes decoded; the call is logged (flags, length)
            url, out, plen, flags = self.arg(0), self.arg(1), self.arg(2), self.arg(3)
            ln = self.read32(plen)
            self.log.append(('InternetCanonicalizeUrlW', flags, ln))
            t = re.sub(r'%([0-9A-Fa-f]{2})', lambda m: chr(int(m.group(1), 16)), self.wstr(url) or '')
            self.uc.mem_write(out, t[:max(ln - 1, 0)].encode('utf-16le') + bytes(2))
            self.write32(plen, len(t))
            return 1
        if nm == 'GetVersionExA':                # OSVERSIONINFOA: size, major, minor, build, platform
            p = self.arg(0)
            info = getattr(self, 'os_info', (5, 1, 2600, 2))
            self.write32(p + 4, info[0])
            self.write32(p + 8, info[1])
            self.write32(p + 0xC, info[2])
            self.write32(p + 0x10, info[3])
            return 1
        if nm == 'GetSystemInfo':                # wProcessorArchitecture at +0
            p = self.arg(0)
            self.uc.mem_write(p, struct.pack('<H', getattr(self, 'cpu_arch', 0)))
            return None
        if nm == 'MultiByteToWideChar':          # (codepage, flags, in, bytes, out, chars): UTF-8 (65001) or the ANSI code page (the tests use plain Latin-1)
            cp, src, cb, dst, cch = self.arg(0), self.arg(2), self.arg(3), self.arg(4), self.arg(5)
            self.log.append(('MultiByteToWideChar', cp, self.arg(1), cb, cch))
            if cb & 0x80000000:
                cb = 0
                while self.uc.mem_read(src + cb, 1)[0]:
                    cb += 1
                cb += 1
            raw = bytes(self.uc.mem_read(src, cb)) if cb else b''
            text = raw.decode('utf-8', 'replace') if cp == 65001 else raw.decode('latin-1')
            out = text.encode('utf-16le')
            if cch == 0:
                return len(out) // 2
            out = out[:2 * cch]
            self.uc.mem_write(dst, out)
            return len(out) // 2
        if nm == 'WideCharToMultiByte':          # (codepage, flags, in, chars, out, bytes, default, used)
            src, cc, dst, cb = self.arg(2), self.arg(3), self.arg(4), self.arg(5)
            self.log.append(('WideCharToMultiByte', self.arg(0), self.arg(1), cc, cb))
            if cc & 0x80000000:
                cc = 0
                while self.uc.mem_read(src + 2 * cc, 2) != bytes(2):
                    cc += 1
                cc += 1
            text = bytes(self.uc.mem_read(src, 2 * cc)).decode('utf-16le', 'replace')
            out = text.encode('latin-1', 'replace')
            if cb == 0:
                return len(out)
            out = out[:cb]
            self.uc.mem_write(dst, out)
            return len(out)
        if nm == '?StaticAllocateObject@UObject@@SAPAV1@PAVUClass@@PAV1@VFName@@K1PAVFOutputDevice@@11@Z':
            cls = self.arg(0)
            cv = 0x10000000 + cls - NWBASE if NWBASE <= cls < self.nwbase_hi else cls
            if sa.ENABLED and self.kind == 'ours':
                r = sa.inv(cls)                 # the standalone's own UClass object -> the stock address of the same class
                if r:
                    cv = r[0]
            self.log.append(('StaticAllocateObject', 'G%x' % cv))
            c = [k for k, v in classes().items() if v['static_class'] == cv]
            size = classes()[c[0]]['size'] if c else 0x400
            p = self.alloc(size + 16)
            self.uc.mem_write(p, bytes(size + 16))
            self.objs[p] = 'new:%s#%d' % (c[0] if c else '?', len(self.objs))
            self.reg_size[p] = size
            if c:
                self.cls_objs.add(p)
            return p
        if nm == '?GetTransientPackage@UObject@@SAPAVUPackage@@XZ':
            return 0x5E5E5E5E
        if nm == '?ConditionalDestroy@UObject@@QAEHXZ':
            self.log.append(('ConditionalDestroy', self.nmthis(self.this())))
            return 1
        if nm in ('?appMemset@@YAXPAXHH@Z',):
            d, v, n = self.arg(0), self.arg(1) & 0xFF, self.arg(2)
            if n > 0:
                self.uc.mem_write(d, bytes([v]) * n)
            return None
        if nm in ('?appMemcpy@@YAXPAXPBXH@Z', '?appMemmove@@YAPAXPAXPBXH@Z'):
            d, sr, n = self.arg(0), self.arg(1), self.arg(2)
            if n > 0:
                self.uc.mem_write(d, bytes(self.uc.mem_read(sr, n)))
            return d
        if nm == '?appStrcmp@@YAHPBG0@Z':
            x, y = self.wstr(self.arg(0)) or '', self.wstr(self.arg(1)) or ''
            return 0 if x == y else (1 if x > y else 0xFFFFFFFF)
        if nm == '?appRand@@YAHXZ':
            self.rand_n = getattr(self, 'rand_n', 0) + 1
            return (12345 + 7919 * self.rand_n) & 0x7FFF
        if nm == '??0FString@@QAE@PBD@Z':           # FString(const char*): an ANSI text
            t = self.this()
            p = self.arg(0)
            raw = b''
            if p:
                i = 0
                while True:
                    c = bytes(self.uc.mem_read(p + i, 1))
                    if c == bytes(1):
                        break
                    raw += c
                    i += 1
            self.fs_set(t, raw.decode('latin-1'))
            return t
        if nm == '??0L2ParamStack@@QAE@H@Z':        # the packet parameter stack, as a queue of values (read in the order they were pushed)
            t = self.this()
            self.ps[t] = []
            return t
        if nm == '?PushBack@L2ParamStack@@QAEHPAX@Z':
            t = self.this()
            self.ps.setdefault(t, []).append(self.arg(0))
            return 1
        if nm == '?Top@L2ParamStack@@QAEPAXXZ':
            t = self.this()
            q = self.ps.setdefault(t, [])
            return q.pop(0) if q else 0
        if nm == '?Clear@L2ParamStack@@QAEXXZ':
            self.ps[self.this()] = []
            return None
        if nm == '??1L2ParamStack@@QAE@XZ':
            return None
        if nm == '??4L2ParamStack@@QAEAAV0@AAV0@@Z':
            t = self.this()
            self.ps[t] = list(self.ps.get(self.arg(0), []))
            return t
        if nm == '??1UObject@@UAE@XZ':
            return None
        if nm == '??0UObject@@QAE@XZ':               # the engine's base constructor (a constructor now runs for real): returns this, the fields stay as they are
            return self.this()
        if nm == '?appQsort@@YAXPAXHHP6AHPBX1@Z@Z':    # logged, not sorted (the comparator is a hooked function: compared as an argument)
            self.log.append(('appQsort', self.val(self.arg(0)), self.arg(1), self.arg(2), self.val(self.arg(3))))
            return None
        if nm == '??8FString@@QBEHABV0@@Z':          # FString == FString (case-insensitive like the engine's)
            t = self.this()
            return 1 if (self.fs_get(t) or '').lower() == (self.fs_get(self.arg(0)) or '').lower() else 0
        if nm == '?Insert@FArray@@QAEXHHH@Z':        # FArray::Insert(index, count, elementSize): room for `count` more elements at `index`
            t = self.this()
            idx, cnt, sz = self.arg(0), self.arg(1), self.arg(2)
            data, num, mx = self.read32(t), self.read32(t + 4), self.read32(t + 8)
            nd = self.alloc(sz * (num + cnt) + 16)
            if num:
                old = bytes(self.uc.mem_read(data, sz * num))
                self.uc.mem_write(nd, old[:sz * idx])
                self.uc.mem_write(nd + sz * (idx + cnt), old[sz * idx:])
            self.write32(t, nd)
            self.write32(t + 4, num + cnt)
            self.write32(t + 8, num + cnt)
            return None
        if nm == '?InStr@FString@@QBEHPBGH@Z':      # InStr(const TCHAR* sub, UBOOL bRight): the index of the first (last) match, or -1
            t = self.this()
            hay, sub = self.fs_get(t) or '', self.wstr(self.arg(0)) or ''
            r = hay.rfind(sub) if self.arg(1) else hay.find(sub)
            return r & 0xFFFFFFFF
        if nm == '?Len@FString@@QBEHXZ':
            t = self.this()
            n = self.read32(t + 4)
            return max(n - 1, 0)
        if nm == '??9FString@@QBEHPBG@Z':
            t = self.this()
            return 1 if (self.fs_get(t) or '').lower() != (self.wstr(self.arg(0)) or '').lower() else 0
        if nm == '?Printf@FString@@SA?AV1@PBGZZ':
            ret, fmt = self.arg(0), self.wstr(self.arg(1))
            a = [self.stack_arg(2 + i) for i in range(8)]
            text = stubs.wfmt(self, fmt, a)
            self.log.append(('FString::Printf', fmt, text))
            self.fs_set(ret, text)
            return ret
        if nm in self.imp_pop_extra:
            return self.imp_pop_extra[nm](self)
        return GW.do_import(self, nm)

    # ---- running -----------------------------------------------------------------------------------------------------------
    def set_fn(self, entry_va, size):
        """the range of the function under test (never trapped)"""
        self.entry_va = entry_va
        self.skip = set()
        if self.kind == 'ours' and not sa.ENABLED:
            # a transparent stock function called through the stand-in image (StockCallAt) runs OUR hooked body: a jump placed there
            for tv in self.transparent:
                if tv in self.hook_of:
                    at = NWBASE + (tv - 0x10000000)
                    rel = (self.hook_of[tv] - (at + 5)) & 0xFFFFFFFF
                    self.uc.mem_write(at, bytes([0xE9]) + struct.pack('<I', rel))
        if self.kind == 'stock':
            self.fn_lo, self.fn_hi = entry_va, entry_va + size
            self.ranges = stock_ranges(entry_va, size)
        else:
            self.fn_lo, self.fn_hi = 0, 0
            self.skip = set(self.follow(entry_va))

    def dump_objs(self):
        out = {}
        for p, nm in self.objs.items():
            n = self.reg_size.get(p)
            if not n:
                continue
            mem = bytes(self.uc.mem_read(p, n))
            words = []
            for off in range(0, n - 3, 4):
                v = struct.unpack('<I', mem[off:off + 4])[0]
                if off == 0 and p in self.cls_objs:
                    words.append('vt')
                    continue
                x = self.val(v) if v else 0
                if isinstance(x, int) and 0x60000000 <= x < 0x68000000:
                    x = 'heap'          # an address of the emulator's heap that is not a registered object or a string: the allocation order may differ
                words.append(x)
            out[nm] = words
        return out


def diff_words(a, b, limit=12):
    out = []
    for i, (x, y) in enumerate(zip(a, b)):
        if x != y:
            out.append((hex(4 * i), x, y))
            if len(out) >= limit:
                break
    return out
