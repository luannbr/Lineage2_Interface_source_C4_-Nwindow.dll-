# "Proof world" for the proof1 lane: a general differential harness for the window code of the C4 NWindow.dll (stock vs ours under
# unicorn). Built on gw.GW / e4.E4 (the engine imports stubbed in Python, ours mapped with NWBASE standing for the stock image's
# globals). What it adds:
#   * callees are found automatically (every direct call of the stock function that goes to a game function) and stubbed by an
#     address hook (stock: the VA; ours: NWBASE+rva, the hook's compiled address and the thunk chain), no memory is patched;
#   * typed fake vtables (one per class / engine interface, the pops come from the stock vtable's `ret N` or from a table): every
#     virtual call is logged with its slot and arguments and returns what the test says;
#   * every data import of the engine (GL2*, GConfig, GSEKScreenX ...) is a variable, and the stock image's data sections are copied
#     into ours at NWBASE so that both sides start from the same bytes; the data writes of a call are compared (stock address vs
#     NWBASE+rva);
#   * the stack balance (`ret N` really popped) is compared: a call that pushes one argument too few is a difference.
# Used by t_video.py, t_srvsel.py, t_gara.py ... (docs/notas/C4_Agentes.md section 8.3, lane proof1).
import sys, os, random, struct, re, json, csv
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import e4, stubs
import sa
from gw import *
import gw
import fncalls
from unicorn import UC_HOOK_CODE

e4.STOCK_X.clear()
e4.OURS_X.clear()

AUDIT = set()      # PW_DATA_AUDIT: stock data dwords written outside the named globals (see PW.data_audit)
import atexit
atexit.register(lambda: AUDIT and print('PW-DATA-AUDIT: stock data dwords written outside the named globals (%d): %s' % (
    len(AUDIT), ' '.join('%08x' % a for a in sorted(AUDIT)[:60]))))
SYSSTR_BASE = 0x9871C      # GL2GameData SysString (stride 12), C4
OURS_PATH = e4._emu.OURS_MAP.replace('.map', '.dll')
MYBASE = 0x7E200000
ESP0 = STACK_TOP - 0x1000
STOCK_DB = {}
for _r in csv.DictReader(open(os.path.join(fncalls.ROOT, 'db/functions.csv'), encoding='utf-8')):
    STOCK_DB[int(_r['va'], 16)] = _r
CLASSES = None
_CLASS_POPS = {}
# callees that tail-jump into a virtual (no `ret` of their own): the pop of the virtual they reach
POP_OVERRIDE = {0x10013330: 0x10,      # NCGDevice::SetClip -> slot 71 (4 ints)
                0x10013340: 0,         # NCGDevice::ReleaseClip -> slot 36
                0x10036dc0: 8}         # NCWnd::SetWindowSize (slot 47): its `ret 8` is at 0x10037008, past two `sub_` pieces of the db


_CSIZE = None


def class_sizes():
    global _CSIZE
    if _CSIZE is None:
        _CSIZE = {c['static_class']: c['size'] for c in classes().values() if c.get('static_class')}
    return _CSIZE


def classes():
    global CLASSES
    if CLASSES is None:
        CLASSES = {c['cpp']: c for c in json.load(open(os.path.join(fncalls.ROOT, 'analysis/classes.json')))}
    return CLASSES


def f32(x):
    return struct.unpack('<I', struct.pack('<f', float(x)))[0]


def getf(v):
    return struct.unpack('<f', struct.pack('<I', v & 0xFFFFFFFF))[0]


def vt_inc_pops(path):
    """{slot: pop bytes} from a `gen/*.vt.inc` of ue2 (the comment `//  27 +0x06c` gives the slot)"""
    out = {}
    for ln in open(path, encoding='utf-8'):
        m = re.search(r'\((.*)\)\s*(=\s*0)?;\s*//\s*(\d+)\s+\+0x', ln)
        if not m:
            continue
        args = m.group(1).strip()
        n = 0
        if args and args != 'void':
            parts, depth, cur = [], 0, ''
            for ch in args:
                depth += (ch == '<') - (ch == '>')
                if ch == ',' and depth == 0:
                    parts.append(cur)
                    cur = ''
                else:
                    cur += ch
            parts.append(cur)      # (the commas inside a template argument list do not split)
            for a in parts:
                a = a.strip()
                if '*' in a or '&' in a:
                    n += 4
                elif a in ('FString', 'FVector', 'FRotator', 'FPlane'):
                    n += 12       # by value
                elif a == 'FURL':
                    n += 0x44     # by value (the stock's `sub esp, 0x44`)
                elif a in ('QWORD', 'DOUBLE', 'double', 'FName', 'INT64'):
                    n += 8
                else:
                    n += 4
        out[int(m.group(3))] = n
    return out


# FConfigCache (UE2Core.h): slot -> pop (every call passes the file name explicitly)
stubs.IMPORT_POP.update({
    '?appAtoi@@YAHPBG@Z': 0, '?appStrcmp@@YAHPBG0@Z': 0, '?appStricmp@@YAHPBG0@Z': 0, '?appStrncpy@@YAPAGPAGPBGH@Z': 0,
    '?appLocalTime@@YAXAAH0000@Z': 0, '??0L2ParamStack@@QAE@H@Z': 4, '?PushBack@L2ParamStack@@QAEHPAX@Z': 4,
    '?Top@L2ParamStack@@QAEPAXXZ': 0, '??1L2ParamStack@@QAE@XZ': 0, '?SetState@UL2ConsoleWnd@@QAEXW4L2ConsoleState@@@Z': 4,
    '?appRandInit@@YAXH@Z': 0, '?appRand@@YAHXZ': 0, '?Clear@FL2GameData@@QAEXXZ': 0, '?Load@FL2GameData@@QAEHH@Z': 4,
    '?InvalidateZoneName@ALineagePlayerController@@QAEXXZ': 0, '?GetNpcName@FL2GameData@@QAEPAGH@Z': 4,
    '?GetQuestData@FL2GameData@@QAEPAUFL2QuestData@@HH@Z': 8,
    '??0FURL@@QAE@PBG@Z': 4, '?LoadURLConfig@FURL@@QAEXPBG0@Z': 8, '??0FURL@@QAE@PAV0@PBGW4ETravelType@@@Z': 12,
    '?AddOption@FURL@@QAEXPBG@Z': 4, '??0FURL@@QAE@ABV0@@Z': 4, '??1FURL@@QAE@XZ': 0, '?LocalizeError@@YAPBGPBG00@Z': 0,
    '?Logf@FOutputDevice@@QAAXPBGZZ': 0, '?appRequestExit@@YAXH@Z': 0, '?MakeBMPToDXT1@UCanvas@@QAEXPAG0AAHHHH@Z': 24, '??1UObject@@UAE@XZ': 0})
CFG_POPS = {0: 16, 1: 16, 2: 16, 3: 16, 4: 20, 5: 12, 6: 16, 7: 16, 8: 8, 9: 16, 10: 16, 11: 16, 12: 16}


NETTHUNK = bytes.fromhex('8b4154' '8b4864' '8b11' 'ffa2')    # mov eax,[ecx+54]; mov ecx,[eax+64]; mov edx,[ecx]; jmp [edx+N]
_NET = None


def net_pops():
    global _NET
    if _NET is None:
        _NET = vt_inc_pops(os.path.join(fncalls.ROOT, 'include/ue2/gen/UL2NetHandler.vt.inc'))
    return _NET


UNDNAME = r"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.38.33130\bin\Hostx64\x86\undname.exe"
_IMPPOP = {}
ARG_SIZES = {'class FName': 8, 'class FString': 12, 'struct FVector': 12, 'class FVector': 12, 'struct FColor': 4}


def import_pop(sym):
    """bytes a call of the (thiscall / stdcall member) import `sym` pops, from the undecorated signature (None when a by-value class of
    unknown size is among the arguments)"""
    if sym in _IMPPOP:
        return _IMPPOP[sym]
    import subprocess
    out = subprocess.run([UNDNAME, sym], capture_output=True, text=True).stdout
    m = re.search(r'is :- "(.*)"', out)
    pop = None
    if m:
        d = m.group(1)
        m2 = re.search(r'\((.*)\)\s*(const)?$', d)
        if m2:
            args = m2.group(1).strip()
            if args in ('void', ''):
                pop = 0
            else:
                depth = 0
                cur = ''
                parts = []
                for ch in args:
                    if ch in '<(':
                        depth += 1
                    elif ch in '>)':
                        depth -= 1
                    if ch == ',' and depth == 0:
                        parts.append(cur.strip())
                        cur = ''
                    else:
                        cur += ch
                parts.append(cur.strip())
                pop = 0
                for a in parts:
                    if '*' in a or '&' in a or a.startswith('enum ') or a in ('int', 'unsigned int', 'long', 'unsigned long', 'float', 'short',
                                                                              'unsigned short', 'char', 'unsigned char', 'bool', 'wchar_t'):
                        pop += 4
                    elif a in ('double', '__int64', 'unsigned __int64'):
                        pop += 8
                    elif a in ARG_SIZES:
                        pop += ARG_SIZES[a]
                    else:
                        pop = None
                        break
    _IMPPOP[sym] = pop
    return pop


def tail_slot(va):
    """N//4 of a function that is `mov eax, [ecx]; jmp dword ptr [eax + N]` (a tail jump through its own object's vtable), else None"""
    code = bytes(fncalls.MEM[va - fncalls.BASE:va - fncalls.BASE + 16])
    if code[:2] == bytes([0x8B, 0x01]):
        if code[2:4] == bytes([0xFF, 0xA0]):
            return struct.unpack('<i', code[4:8])[0] // 4
        if code[2:4] == bytes([0xFF, 0x60]):
            return code[4] // 4
    return None


def ret_pop_any(va):
    """the `ret N` reached from va by linear decoding (a function the db does not list: a folded duplicate); a tail `jmp` of
    an absolute address is followed"""
    from capstone import Cs, CS_ARCH_X86, CS_MODE_32
    md = Cs(CS_ARCH_X86, CS_MODE_32)
    for _ in range(4):
        code = bytes(fncalls.MEM[va - fncalls.BASE:va - fncalls.BASE + 256])
        if code[:10] == NETTHUNK:
            # the console's tail jump into the network handler: the pop is the handler slot's
            return net_pops().get(struct.unpack('<I', code[10:14])[0] // 4)
        nxt = None
        for i in md.disasm(code, va):
            if i.mnemonic == 'ret':
                return int(i.op_str, 16) if i.op_str else 0
            if i.mnemonic == 'jmp':
                if i.op_str.startswith('0x'):
                    nxt = int(i.op_str, 16)
                    break
                return None
        if nxt is None:
            return None
        va = nxt
    return None


def ret_pop_ours(e, a):
    from capstone import Cs, CS_ARCH_X86, CS_MODE_32
    md = Cs(CS_ARCH_X86, CS_MODE_32)
    for i in md.disasm(bytes(e.uc.mem_read(a, 0x400)), a):
        if i.mnemonic == 'ret':
            return int(i.op_str, 16) if i.op_str else 0
    return 0


def sym_class_method(sym):
    """('Class', 'Method') of a decorated symbol (`?M@C@@...`, `??0C@@`, `??1C@@`), None otherwise"""
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


class PW(GW):
    def __init__(self, kind, funcs=(), extra=None, ours_syms=None, skip=()):
        GW.__init__(self, kind, {}, None)
        self.uc.mem_map(MYBASE, 0x100000)
        self.uc.hook_add(UC_HOOK_CODE, self.on_code, None, MYBASE, MYBASE + 0xFFFFF)
        self.mynext = MYBASE
        self.vtinfo = {}
        self.lm = log_map()
        self.tag = {}
        self.callee_names = {}
        self.sp_ids = {}
        self.cdecl_n = {}
        self.nolog = set()
        self.nothis = set()       # callees whose `this` (ecx) is not logged: static functions of ours that the stock calls as members (they never read ecx)
        self.sargs_n = {}         # callee -> number of stack arguments to read when the callee pops none (a cdecl static function of ours: alias_extra(cdecl=N))
        self.pstacks = {}
        self.case = None
        self.skipped = set(skip)
        self.entry_skip = set()
        self.hh = {}
        self.lognames = {}
        self.posts = {}
        self.lognargs = {}
        self.diag = os.environ.get('PW_DIAG')
        for ee in self.pe.DIRECTORY_ENTRY_IMPORT:
            for imp in ee.imports:
                nm = imp.name.decode() if imp.name else ''
                if nm.startswith('?StaticAllocateObject@UObject@@') or nm in ('?GetTransientPackage@UObject@@SAPAVUPackage@@XZ', '?ConditionalDestroy@UObject@@QAEHXZ'):
                    stubs.IMPORT_POP[nm] = 0
        self.map_data_imports()
        self.load_stock_data()
        self.argfix = {}
        self.stock_text = (self.base, self.base + 0x100000)
        done = set()
        for f in funcs:
            for va, (name, pop, rthis) in self.auto_callees(f, skip).items():
                if va not in done:
                    done.add(va)
                    self.add_callee(va, name, pop, rthis)
        for va, (name, pop, rthis) in (extra or {}).items():
            if va not in done:
                done.add(va)
                self.add_callee(va, name, pop, rthis)
        for va, sym in (ours_syms or {}).items():
            if self.kind == 'ours' and sym in self.symbols:
                self.hook_addr(self.symbols[sym], self.callee_names[va])
        self.install_defaults()
        if self.kind == 'ours':
            for va in self.skipped:
                if sa.ENABLED:
                    break              # (the standalone calls its own copy of the function: nothing to give it from the stock image)
                size = fncalls.FUNCS.get(va) or 16
                self.uc.mem_write(NWBASE + (va - 0x10000000), bytes(fncalls.MEM[va - fncalls.BASE:va - fncalls.BASE + size]))
            self.alias_scan(funcs)
        self.write32(self.glob(e4.GCONSOLE), self.console)
        for i in range(0, 3000):
            self.fs_set(self.gdata + SYSSTR_BASE + 12 * i, 'S%d' % i)

    def install_defaults(self):
        """constructors of known classes set their (fake) vtable and return this; the creation helpers answer like the engine"""
        self.nnew = 0
        self.created = []
        self.created_size = {}
        for va, nm in list(self.callee_names.items()):
            if va < 0:
                continue
            base = nm.split('@')[0]
            m = re.match(r'^(\w+)::(\w+)$', base)
            if m and m.group(1) == m.group(2) and m.group(1) in classes() and nm not in self.rets:
                vt = self.class_vt(m.group(1))
                self.rets[nm] = (lambda vt_: (lambda e, t, a: (e.write32(t, vt_), t)[1]))(vt)
        self.rets.setdefault('NCWnd::AddChildWindow', self.default_add_child)
        self.rets.setdefault('NCObject::FindTexture', lambda e, t, a: 0x9000 + sum(ord(ch) for ch in (e.wstr(a[0]) or '')) + a[1])
        self.rets.setdefault('NCObject::SetTimer', lambda e, t, a: 77)

    def default_add_child(self, e, t, a):
        ent = e.obj('entry:' + str(e.nm(a[0])), 0x40)
        e.write32(ent + 4, a[0])
        e.write32(ent + 0xC, e.read32(t + 0x74))
        return ent

    # ---- the stubs --------------------------------------------------------------------------------------------------------
    def hook_addr(self, a, name):
        self.stubs[a] = ('cb:' + name, None)
        h = self.uc.hook_add(UC_HOOK_CODE, self.on_code, None, a, a)
        self.hh.setdefault(a, []).append(h)

    def add_callee(self, va, name, pop, rthis=0):
        if name in self.cb and self.cb[name][0] != va:
            name = '%s@%x' % (name, va & 0xFFFF)
        self.cb[name] = (va, pop, rthis)
        self.callee_names[va] = name
        if self.kind == 'stock':
            self.hook_addr(va, name)
        else:
            self.hook_addr(NWBASE + (va - 0x10000000), name)
            for a in chase(self, va):
                self.hook_addr(a, name)

    def ours_body(self, a):
        """(start, bytes) of the compiled function at a (up to the next symbol or the int3 padding)"""
        if not hasattr(self, 'sym_starts'):
            sys.path.insert(0, os.path.join(fncalls.ROOT, 'tools'))
            import verify_hooks as vh
            sy = vh.our_symbols(OURS_PATH)
            self.sym_starts = [x for x, _ in sy]
            self.sym_names = dict(sy)
        import bisect
        i = bisect.bisect_right(self.sym_starts, a)
        end = self.sym_starts[i] if i < len(self.sym_starts) else a + 0x2000
        return bytes(self.uc.mem_read(a, min(end - a, 0x3000)))

    def alias_scan(self, funcs):
        """ours calls hooked functions through the compiled standalone copy (a different address than the hook's forwarder): every
        direct call target of our compiled body that is not a stub yet and whose symbol is Class::Method of a callee is stubbed too"""
        from capstone import Cs, CS_ARCH_X86, CS_MODE_32
        md = Cs(CS_ARCH_X86, CS_MODE_32)
        byhook = {}
        for ln in open(LOGFILE, encoding='latin-1'):
            m = re.match(r'^\S+\s+([0-9a-f]{8}) (.*?)\s+ours=([0-9a-f]+) ok', ln)
            if m:
                byhook.setdefault(m.group(2).strip(), []).append(int(m.group(1), 16))
        todo = []
        for f in funcs:
            a = self.lm.get(f)
            if a is None:
                continue
            for x in chase(self, f)[-1:]:
                todo.append(x)
        seen = set()
        for a in todo:
            body = self.ours_body(a)
            for i in md.disasm(body, a):
                if i.mnemonic not in ('call', 'jmp') or not i.op_str.startswith('0x'):
                    continue
                t = int(i.op_str, 16)
                if i.mnemonic == 'jmp' and (a <= t < a + len(body) or t not in self.sym_names):
                    continue      # a jump inside the function (or to code that is not a function): not a tail call
                if t in seen or t in self.stubs:
                    continue
                seen.add(t)
                sym = self.sym_names.get(t)
                cm = sym_class_method(sym) if sym else None
                if not cm:
                    if self.diag:
                        print('PW: unpatched call target %08x %s' % (t, sym))
                    continue
                key = '%s::%s' % cm if cm[0] else cm[1]
                hooked = byhook.get(key, [])
                if any(h in self.skipped for h in hooked):
                    continue      # a function the test lets run for real on both sides
                cands = [va for va in hooked if va in self.callee_names]
                if cands:
                    pass
                elif not hooked:
                    # not hooked at all (a compiled standalone body): the stock callee of the same db name
                    cands = [va for va, nmx in self.callee_names.items() if nmx == key or nmx.startswith(key + '@')]
                if len(cands) == 1:
                    self.hook_addr(t, self.callee_names[cands[0]])
                    if self.diag:
                        print('PW: alias %08x %s -> %s' % (t, sym, self.callee_names[cands[0]]))
                elif len(cands) > 1:
                    print('PW: AMBIGUOUS alias %08x %s -> %s' % (t, sym, [hex(c) for c in cands]))
                elif hooked and cm[0] and (cm[1] == cm[0] or cm[1] == '~' + cm[0]):
                    # a base constructor / destructor the stock inlines: it runs for real here (its own stores are compared)
                    continue
                elif hooked:
                    # ours calls a hooked function the stock never calls here: the stock address of what it replaces is in the name
                    nm ='OURS-ONLY %s (hooked at %s)' % (key, ','.join('%x' % h for h in hooked))
                    self.cb[nm] = (hooked[0], ret_pop_ours(self, t), 0)
                    self.callee_names[-t] = nm
                    self.hook_addr(t, nm)
                    print('PW: ours calls %s %08x that the stock function does not call' % (nm, t))
                elif self.diag:
                    print('PW: unpatched call target %08x %s' % (t, sym))

    def alias_extra(self, sym, logname, pop, nargs=None, rthis=1, post=None, cdecl=None):
        """a call of ours that the stock does not make as such (the compiler inlined a wrapper): the compiled function `sym` is logged as
        the stock callee `logname`, popping `pop` bytes and showing its first `nargs` arguments"""
        if self.kind != 'ours':
            return
        nm = 'alias:' + sym
        self.cb[nm] = (0, pop, rthis)
        self.lognames[nm] = logname
        if post:
            self.posts[nm] = post
        if nargs is not None:
            self.lognargs[nm] = nargs
        if cdecl is not None:
            self.sargs_n[nm] = cdecl      # (a cdecl function of ours: the caller pops, `pop` is 0, but its `cdecl` arguments are read and logged)
        self.hook_addr(self.symbols[sym], nm)

    def auto_callees(self, va, skip=()):
        out = {}
        direct, imports, vcalls = fncalls.scan(va)
        for t in direct:
            row = STOCK_DB.get(t)
            if not row or row['kind'] not in ('game', 'init') or t == va or t in skip:
                continue
            pops = fncalls.ret_pop(t)
            if t in POP_OVERRIDE:
                pops = [POP_OVERRIDE[t]]
            elif not pops:
                rp = ret_pop_any(t)         # a function that ends in a tail jump: the pop of the function it jumps to
                if rp is None:
                    print('PW: callee %08x %s has no ret (an indirect tail jump?): pop taken as 0, add it to POP_OVERRIDE' % (t, row['name']))
                    rp = 0
                pops = [rp]
            if len(pops) > 1 and self.diag:
                print('PW: callee %08x %s has several rets %s' % (t, row['name'], pops))
            mc = re.match(r'^(?:.*::)?(\w+)::(\w+)$', row['name'])
            rthis = 1 if (mc and mc.group(1) == mc.group(2)) else 0      # a constructor returns this (MSVC callers use eax)
            out[t] = (row['name'], max(pops), rthis)
        return out

    def new_stub(self, name):
        a = self.mynext
        self.mynext += 16
        self.uc.mem_write(a, b'\xC3')
        self.stubs[a] = (name, None)
        return a

    def make_vt(self, tname, pops, rets=None, nslots=130, floats=None):
        """a fake vtable; `floats` = {slot: getter(e) -> float} for virtuals that return a FLOAT in st0: those slots run real code
        (`fld dword [cell]; ret N`) after a hook that logs the call and stores the value in the cell"""
        vt = self.alloc(4 * nslots)
        for s in range(nslots):
            self.write32(vt + 4 * s, self.new_stub('vt:%s:%d' % (tname, s)))
        self.vtinfo[tname] = (dict(pops), dict(rets or {}))
        for slot, getter in (floats or {}).items():
            cell = self.alloc(16)
            a = self.read32(vt + 4 * slot)
            pop = pops.get(slot, 0)
            self.stubs.pop(a, None)
            self.uc.mem_write(a, bytes([0xD9, 0x05]) + struct.pack("<I", cell) + bytes([0xC2]) + struct.pack("<H", pop))
            def cb(uc, address, size, user, tname=tname, slot=slot, cell=cell, getter=getter, pop=pop):
                this = uc.reg_read(UC_X86_REG_ECX)
                esp = uc.reg_read(UC_X86_REG_ESP)
                args = [self.rds32(esp + 4 + 4 * i) for i in range(pop // 4)]
                self.log.append(('vcall', tname, slot, self.nm(this), [self.val(x) for x in args]))
                self.write32(cell, f32(getter(self)))
            self.uc.hook_add(UC_HOOK_CODE, cb, None, a, a)
        return vt

    def class_pops(self, cpp):
        # (the result depends only on the stock image, classes.json and POP_OVERRIDE, never on the emulator: computed once per process, a copy handed out;
        # it was the main cost of building a world: ~58,000 ret_pop disassemblies per pair of emulators)
        key = (cpp, len(POP_OVERRIDE))
        if key not in _CLASS_POPS:
            _CLASS_POPS[key] = self._class_pops(cpp)
        return dict(_CLASS_POPS[key])

    def _class_pops(self, cpp):
        c = classes()[cpp]
        pops = {}
        unknown = []
        tails = {}
        for s in c['vtable_slots']:
            if s.get('import'):
                ip = import_pop(s['import'])
                if ip is not None:
                    pops[s['slot']] = ip
                continue
            if s.get('va') and s['va'] in POP_OVERRIDE:
                pops[s['slot']] = POP_OVERRIDE[s['va']]
                continue
            if s.get('va'):
                p = fncalls.ret_pop(s['va']) if s['va'] in STOCK_DB else None
                if not p:
                    p = [ret_pop_any(s['va'])]
                if p[0] is not None:
                    pops[s['slot']] = max(p)
                else:
                    unknown.append(s['slot'])      # a tail jump through a virtual: the pop of that slot, else the parent's, else none
                    tj = tail_slot(s['va'])
                    if tj is not None:
                        tails[s['slot']] = tj
        sup = c.get('super')
        if sup and sup in classes():
            for sl, pv in self.class_pops(sup).items():
                pops.setdefault(sl, pv)
        for sl in unknown:
            # `mov eax, [ecx]; jmp [eax + N]` (NCWnd::OffsetWindow -> slot 39): it pops what the slot it jumps to pops
            if sl in tails and tails[sl] in pops:
                pops[sl] = pops[tails[sl]]
            else:
                pops.setdefault(sl, 0)
        return pops

    def class_vt(self, cpp, rets=None, extra_pops=None, floats=None):
        if not hasattr(self, '_cvt'):
            self._cvt = {}
        if cpp in self._cvt:
            # one fake table per class (it also replaces the real vtable): later requests only add handlers
            if rets:
                self.vtinfo['w:' + cpp][1].update(rets)
            return self._cvt[cpp]
        pops = self.class_pops(cpp)
        pops.update(extra_pops or {})
        n = len(classes()[cpp]['vtable_slots'])
        ns = n + 4
        if cpp == 'NCWnd':
            # the generic window of the fake world stands for any control (a pointer the header types as NCWnd): the virtuals the
            # subclasses add (slots 74 ...) are taken as having no arguments, as the callers of the harness' windows use them
            ns = 130
            for sl in range(n, ns):
                pops.setdefault(sl, 0)
        vt = self.make_vt('w:' + cpp, pops, rets, nslots=ns, floats=floats)
        if self.kind == 'stock' and classes()[cpp].get('vtable'):
            # objects the stock code constructs with its real vtable: the real table becomes the fake one
            a = classes()[cpp]['vtable']
            for sl in range(n):
                self.write32(a + 4 * sl, self.read32(vt + 4 * sl))
        if self.kind == 'ours' and ('??_7%s@@6B@' % cpp) in self.symbols:
            # objects that our own (inlined) constructors make carry the compiled vtable: it becomes the fake one
            a = self.symbols['??_7%s@@6B@' % cpp]
            for sl in range(n):
                self.write32(a + 4 * sl, self.read32(vt + 4 * sl))
        self._cvt[cpp] = vt
        return vt

    def dispatch(self, name):
        kind, nm = name.split(':', 1)
        self.sp_ids = {}       # stack pointers are numbered per call (the two builds do not share the frame layout)
        if kind == 'cb' and nm not in self.cdecl_n:
            va, pop, rthis = self.cb[nm]
            self.last_pop = pop
            t = self.this()
            a = self.sargs(self.sargs_n.get(nm, pop // 4))
            if nm in self.argfix:
                a = self.argfix[nm](self, a)
            tv = self.val(t) if STACK_TOP - 0x100000 <= t < STACK_TOP else self.nm(t)
            ln = self.lognames.get(nm, nm)
            if nm in self.nothis or ln in self.nothis:
                tv = None
            self.log.append((ln, tv, [self.val(x) for x in a[:self.lognargs.get(nm, len(a))]]))
            rv = self.rets[ln](self, t, a) if ln in self.rets else (t if rthis else 0)
            if nm in self.posts:
                self.posts[nm](self, t)
            return rv
        if kind == 'cb' and nm in self.cdecl_n:
            # a cdecl callee (the caller pops): `ret 0` in the stock, n arguments to log
            self.last_pop = 0
            t = self.this()
            a = self.sargs(self.cdecl_n[nm])
            if nm not in self.nolog:
                self.log.append((nm, [self.val(x) for x in a]))
            h = self.rets.get(nm)
            return h(self, t, a) if h else 0
        if kind == 'vt':
            t, slot = nm.rsplit(':', 1)
            slot = int(slot)
            pops, rets = self.vtinfo[t]
            if slot not in pops:
                raise RuntimeError('virtual call of slot %d of %s (no pop known)' % (slot, t))
            self.last_pop = pops[slot]
            this = self.this()
            a = self.sargs(self.last_pop // 4)
            self.log.append(('vcall', t, slot, self.nm(this), [self.val(x) for x in a]))
            h = rets.get(slot)
            return h(self, this, a) if h else 0
        return GW.dispatch(self, name)

    # ---- objects ----------------------------------------------------------------------------------------------------------
    def obj(self, name, size=0x300, vt=0):
        p = self.alloc(size)
        self.uc.mem_write(p, bytes(size))
        self.objs[p] = name
        if vt:
            self.write32(p, vt)
        return p

    def wnd(self, name, vt, size=0x300, parent=None):
        """a fake window with an entry at +0x74 (entry+4 = the window, entry+0xC = the parent's entry)"""
        p = self.obj(name, size, vt)
        ent = self.obj('entry:' + name, 0x40)
        self.write32(p + 0x74, ent)
        self.write32(ent + 4, p)
        if parent:
            self.write32(ent + 0xC, self.read32(parent + 0x74))
        return p

    def f32w(self, a, x):
        self.write32(a, f32(x))

    # ---- data -------------------------------------------------------------------------------------------------------------
    def map_data_imports(self):
        special = ('?GLanguageType@@3HA', '?GMalloc@@3PAVFMalloc@@A')
        for ee in self.pe.DIRECTORY_ENTRY_IMPORT:
            for imp in ee.imports:
                nm = imp.name.decode() if imp.name else ''
                if '@@3' not in nm or nm in self.gvars or 'GL2GameData' in nm or nm in special:
                    continue
                v = self.alloc(0x40)
                self.uc.mem_write(v, bytes(0x40))
                self.gvars[nm] = v
                self.write32(imp.address, v)

    def load_stock_data(self):
        st = pefile.PE(STOCK, fast_load=True)      # (only the sections: the directories are not needed here)
        self.dlo = self.dhi = None
        for s in st.sections:
            ch = s.Characteristics
            if ch & 0x20000000:      # executable
                continue
            lo = s.VirtualAddress
            hi = lo + max(s.Misc_VirtualSize, s.SizeOfRawData)
            self.dlo = lo if self.dlo is None else min(self.dlo, lo)
            self.dhi = hi if self.dhi is None else max(self.dhi, hi)
            if self.kind == 'ours' and not sa.ENABLED:
                self.uc.mem_write(NWBASE + lo, s.get_data())      # (the standalone has its own, verified initial data: tools/gen_globals.py --check)

    def data_base(self):
        return (0x10000000 if self.kind == 'stock' else NWBASE) + self.dlo

    def data_snap(self):
        if sa.ENABLED and self.kind == 'ours':
            # the data of the stock image as the standalone keeps it: every named global / class object at its stock offset; the rest is not compared
            buf = bytearray(self.dhi - self.dlo)
            for sva, size, sav, name in sa.globals_table():
                off = sva - 0x10000000 - self.dlo
                if 0 <= off < len(buf):
                    n = min(max(4, size), len(buf) - off)
                    buf[off:off + n] = bytes(self.uc.mem_read(sav, n))
            return bytes(buf)
        return bytes(self.uc.mem_read(self.data_base(), self.dhi - self.dlo))

    def data_mask(self):
        """standalone mode: the stock offsets (dword granularity) that belong to a named global"""
        if not hasattr(self, '_dmask'):
            m = set()
            for sva, size, sav, name in sa.globals_table():
                off = sva - 0x10000000 - self.dlo
                for o in range(off & ~3, off + max(4, size), 4):
                    m.add(o)
            self._dmask = m
        return self._dmask

    def data_diff(self, before):
        after = self.data_snap()
        out = {}
        if after == before:
            return out                      # (nothing written: the common case, without the dword loop)
        if sa.ENABLED and self.kind == 'stock' and os.environ.get('PW_DATA_AUDIT'):
            self.data_audit(before, after)
        mask = self.data_mask() if sa.ENABLED else None
        if mask is not None and not hasattr(self, '_dmask_list'):
            self._dmask_list = sorted(m for m in mask if 0 <= m < len(after))
        for i in (self._dmask_list if mask is not None else range(0, len(after), 4)):
            if before[i:i + 4] != after[i:i + 4]:
                out['%08x' % (self.dlo + i)] = struct.unpack('<I', after[i:i + 4])[0]
        return out

    def data_audit(self, before, after):
        """PW_DATA_AUDIT=1 (standalone mode, stock side): the stock data dwords the call wrote that are OUTSIDE the named globals (the part of the
        stock image's data that the comparison cannot see); collected in AUDIT and printed at exit"""
        mask = self.data_mask()
        for i in range(0, min(len(before), len(after)), 4):
            if before[i:i + 4] != after[i:i + 4] and i not in mask:
                AUDIT.add(self.dlo + i)

    def gvar(self, name):
        for k, v in self.gvars.items():
            if k.startswith('?' + name + '@@3'):
                return v
        raise KeyError(name)

    def setg(self, name, val, isfloat=False):
        self.write32(self.gvar(name), f32(val) if isfloat else val & 0xFFFFFFFF)

    def getg(self, name):
        return self.read32(self.gvar(name))

    def gsnap(self):
        return {k.split('@@')[0]: self.read32(v) for k, v in sorted(self.gvars.items())}

    # ---- coverage of the stock function (stock side only) -----------------------------------------------------------------
    def track(self, va):
        """records the instructions of the stock function at va that run"""
        if not hasattr(self, 'covered'):
            self.covered = set()
            self.cov_all = {}
        size = fncalls.FUNCS[va]
        from capstone import Cs, CS_ARCH_X86, CS_MODE_32
        md = Cs(CS_ARCH_X86, CS_MODE_32)
        self.cov_all[va] = [i.address for i in md.disasm(bytes(fncalls.MEM[va - fncalls.BASE:va + size - fncalls.BASE]), va)]
        self.uc.hook_add(UC_HOOK_CODE, lambda uc, a, sz, ud: self.covered.add(a), None, va, va + size - 1)

    def coverage(self, va):
        allv = self.cov_all[va]
        miss = [a for a in allv if a not in self.covered]
        return len(allv) - len(miss), len(allv), miss

    # ---- the report of a call ---------------------------------------------------------------------------------------------
    def go(self, va, this=None, args=(), snaps=()):
        """call the function at va (a stock VA for the stock side, ours' address for ours: the caller passes A[...]) and return what a
        test compares: the log of the calls, eax, the stack balance (bytes popped by the callee), the data writes, the data
        imports, and the memory blocks in `snaps` (name -> (address, size))"""
        mark = len(self.log)
        self.sp_ids = {}
        # the function under test may be a callee of another function of the group (its entry carries a stub): its own entry runs
        self.entry_skip = {va}
        if self.kind == 'ours':
            if not hasattr(self, 'rev_lm'):
                self.rev_lm = {a_: v_ for v_, a_ in self.lm.items()}
            sv = self.rev_lm.get(va)
            if sv is not None:
                self.entry_skip |= set(chase(self, sv))
        saved = {}
        for a_ in list(self.entry_skip):
            if a_ in self.stubs:
                saved[a_] = self.stubs.pop(a_)
                for h_ in self.hh.pop(a_, []):
                    self.uc.hook_del(h_)
        self.entry_skip = set()
        if saved:
            self.uc.ctl_flush_tb()
        before = self.data_snap()
        g0 = self.gsnap()
        try:
            eax = self.call(va, this=this, args=list(args))
        finally:
            for a_, st_ in saved.items():
                self.stubs[a_] = st_
                self.hh.setdefault(a_, []).append(self.uc.hook_add(UC_HOOK_CODE, self.on_code, None, a_, a_))
            if saved:
                self.uc.ctl_flush_tb()
        pop = self.reg(UC_X86_REG_ESP) - ESP0 + 4 * len(args)
        g1 = self.gsnap()
        return dict(log=self.log[mark:], eax=eax, pop=pop, data=self.data_diff(before),
                    gl={k: v for k, v in g1.items() if g0.get(k) != v},
                    mem={k: self.snapshot(a, n) for k, (a, n) in snaps.items()})

    def on_code(self, uc, address, size, user):
        if address in self.stubs:
            if address in self.entry_skip:
                self.entry_skip.discard(address)
                return
            self.pending = address
            uc.emu_stop()

    def snapshot(self, base, size):
        out = GW.snapshot(self, base, size)
        for i, v in enumerate(out):
            if isinstance(v, int) and 0x10000000 <= v < 0x10400000:
                try:
                    t = self.wstr(v, 80)
                except Exception:
                    t = None
                if t and all(32 <= ord(ch) < 0xFFFF for ch in t):
                    out[i] = 'S:' + t
        return out

    def val(self, v):
        # a pointer into the stack: the frames of the two builds differ, so a local is named by the order its address is first seen
        if isinstance(v, int) and STACK_TOP - 0x100000 <= v < STACK_TOP:
            return ('stk', self.sp_ids.setdefault(v, len(self.sp_ids)))
        if sa.ENABLED and self.kind == 'ours' and isinstance(v, int) and 0x10000000 <= v < 0x10400000:
            try:
                t = self.wstr(v, 80)
            except Exception:
                t = None
            if t and all(32 <= ord(ch) < 0xFFFF for ch in t):
                return 'S:' + t
            r = sa.inv(v)               # a pointer into the standalone's own data: the stock address of the same object
            if r:
                return r[0]
        if isinstance(v, int) and NWBASE <= v < NWBASE + 0x400000:
            # a pointer into the copy of the stock data (ours reaches the stock's tables through g_nwBase): its text, if it is one
            try:
                t = self.wstr(v, 80)
            except Exception:
                t = None
            if t and all(32 <= ord(ch) < 0xFFFF for ch in t):
                return 'S:' + t
            return ('stockdata', v - NWBASE)
        r = GW.val(self, v)
        if isinstance(r, int) and 0x60000000 <= r < 0x68000000:
            return 'heap'      # an unnamed block (a buffer of the engine's allocator): its address differs between the builds
        return r

    # ---- engine imports ---------------------------------------------------------------------------------------------------
    def do_import(self, nm):
        t = self.this()
        if nm == '?GetTransientPackage@UObject@@SAPAVUPackage@@XZ':
            return 0x7777
        if nm.startswith('?StaticAllocateObject@UObject@@'):
            cls = self.arg(0)
            if sa.ENABLED and self.kind == 'ours':
                r = sa.inv(cls)
                if r:
                    cls = r[0]
            elif cls >= NWBASE:
                cls = cls - NWBASE + 0x10000000
            size = class_sizes().get(cls, 0x400) + 0x40
            p = self.obj('new%d' % self.nnew, size)
            self.nnew += 1
            self.created.append(p)
            self.created_size[p] = size
            return p
        if nm == '?InvalidateZoneName@ALineagePlayerController@@QAEXXZ':
            self.log.append(('InvalidateZoneName', self.nm(t)))
            return None
        if nm == '?Load@FL2GameData@@QAEHH@Z':
            self.log.append(('FL2GameData::Load', self.arg(0)))
            return 1
        if nm == '?Clear@FL2GameData@@QAEXXZ':
            self.log.append(('FL2GameData::Clear',))
            return None
        if nm == '?GetNpcName@FL2GameData@@QAEPAGH@Z':
            n = self.arg(0)
            self.log.append(('GetNpcName', n))
            return self.put_wstr('npc%d' % n)
        if nm == '?GetQuestData@FL2GameData@@QAEPAUFL2QuestData@@HH@Z':
            qid, kind = self.arg(0), self.arg(1)
            self.log.append(('GetQuestData', qid, kind))
            if ((self.case or {}).get('seed', 0) + qid) % 5 == 0:
                return 0
            q = self.alloc(0x90)
            for j in range(0x24):
                self.write32(q + 4 * j, (qid * 31 + j * 17) & 0xFFFF)
            return q
        if nm == '??0FURL@@QAE@PBG@Z':
            self.uc.mem_write(t, bytes(0x44))
            self.log.append(('FURL', self.wstr(self.arg(0)) if self.arg(0) else None))
            return t
        if nm == '??0FURL@@QAE@PAV0@PBGW4ETravelType@@@Z':
            self.uc.mem_write(t, bytes(0x44))
            self.log.append(('FURL(base, map, travel)', self.wstr(self.arg(1)), self.arg(2)))
            return t
        if nm == '??0FURL@@QAE@ABV0@@Z':
            self.uc.mem_write(t, bytes(self.uc.mem_read(self.arg(0), 0x44)))
            self.log.append(('FURL(copy)',))
            return t
        if nm == '?LoadURLConfig@FURL@@QAEXPBG0@Z':
            self.log.append(('FURL::LoadURLConfig', self.wstr(self.arg(0)), self.wstr(self.arg(1))))
            return None
        if nm == '?AddOption@FURL@@QAEXPBG@Z':
            self.log.append(('FURL::AddOption', self.wstr(self.arg(0))))
            return None
        if nm == '??1FURL@@QAE@XZ':
            self.log.append(('~FURL',))
            return None
        if nm == '?LocalizeError@@YAPBGPBG00@Z':
            self.log.append(('LocalizeError', self.wstr(self.arg(0)), self.wstr(self.arg(1))))
            return self.put_wstr('err:' + (self.wstr(self.arg(0)) or ''))
        if nm == '?Logf@FOutputDevice@@QAAXPBGZZ':
            self.log.append(('Logf', self.wstr(self.arg(0))))
            return None
        if nm == '?MakeBMPToDXT1@UCanvas@@QAEXPAG0AAHHHH@Z':
            self.write32(self.arg(2), (self.case or {}).get('crest', {}).get('dds', 0))      # the size of the .dds the conversion made
            self.log.append(('MakeBMPToDXT1', self.wstr(self.arg(0)), self.wstr(self.arg(1)), self.arg(3), self.arg(4), self.arg(5)))
            return None
        if nm == '??1UObject@@UAE@XZ':
            # the end of a base class's destructor chain that our compiler inlined into the hook (the stock calls that base destructor)
            self.log.append(('base dtor', self.nm(t), []))
            return None
        if nm == '?appRequestExit@@YAXH@Z':
            self.log.append(('appRequestExit', self.arg(0)))
            return None
        if nm == '?appRandInit@@YAXH@Z':
            self.log.append(('appRandInit', 'seed'))      # the seed is the clock
            self.randn = 0
            return None
        if nm == '?appRand@@YAHXZ':
            self.randn = getattr(self, 'randn', 0) + 1
            return (((self.case or {}).get('seed', 1) if isinstance(self.case, dict) else 1) * 7 + self.randn * 31) & 0x7FFF
        if nm == '??0L2ParamStack@@QAE@H@Z':
            self.pstacks[t] = []
            self.log.append(('PS.ctor', self.arg(0)))
            return t
        if nm == '?PushBack@L2ParamStack@@QAEHPAX@Z':
            self.pstacks.setdefault(t, []).append(self.arg(0))
            self.log.append(('PS.PushBack', self.val(self.arg(0))))
            return 1
        if nm == '?Top@L2ParamStack@@QAEPAXXZ':
            return self.pstacks[t].pop(0)
        if nm == '??1L2ParamStack@@QAE@XZ':
            self.log.append(('PS.dtor',))
            return None
        if nm == '?appAtoi@@YAHPBG@Z':
            m = re.match(r'\s*([+-]?\d+)', self.wstr(self.arg(0)))
            return u32(int(m.group(1))) if m else 0
        if nm in ('?appStrcmp@@YAHPBG0@Z', '?appStricmp@@YAHPBG0@Z'):
            x, y = self.wstr(self.arg(0)), self.wstr(self.arg(1))
            if nm.startswith('?appStricmp'):
                x, y = x.lower(), y.lower()
            return 0 if x == y else (1 if x > y else 0xFFFFFFFF)
        if nm == '?appStrncpy@@YAPAGPAGPBGH@Z':
            d, sr, n = self.arg(0), self.wstr(self.arg(1)), self.rds32(self.reg(UC_X86_REG_ESP) + 12)
            self.write_wstr_at(d, sr[:max(0, n - 1)])
            return d
        if nm == '?appLocalTime@@YAXAAH0000@Z':
            for i, v in enumerate(self.case['time']):
                self.write32(self.arg(i), v)
            self.log.append(('appLocalTime',))
            return None
        if nm == '?SetState@UL2ConsoleWnd@@QAEXW4L2ConsoleState@@@Z':
            self.log.append(('SetState', self.nm(t), self.arg(0)))
            return None
        if nm == '?appSprintf@@YAHPAGPBGZZ':
            dst, fmt = self.arg(0), self.wstr(self.arg(1))
            i = 2
            out = []
            k = 0
            while k < len(fmt):
                ch = fmt[k]
                if ch != '%':
                    out.append(ch)
                    k += 1
                    continue
                k += 1
                spec = ''
                while fmt[k] in '-+ #0123456789.l':
                    spec += fmt[k]
                    k += 1
                ty = fmt[k]
                k += 1
                if ty == 'f':
                    lo, hi = self.stack_arg(i), self.stack_arg(i + 1)
                    i += 2
                    d = struct.unpack('<d', struct.pack('<II', lo, hi))[0]
                    out.append(('%' + spec + 'f') % d)
                elif ty in 'di':
                    v = self.stack_arg(i)
                    i += 1
                    out.append(str(v - (1 << 32) if v & 0x80000000 else v))
                elif ty == 's':
                    v = self.stack_arg(i)
                    i += 1
                    out.append(self.wstr(v) if v else '(null)')
                elif ty == '%':
                    out.append('%')
                else:
                    raise RuntimeError('format ' + fmt)
            s = ''.join(out)
            self.write_wstr_at(dst, s)
            self.log.append(('appSprintf', fmt, s))
            return len(s)
        if nm == '?ConditionalDestroy@UObject@@QAEHXZ':
            self.log.append(('ConditionalDestroy', self.nm(t)))
            return 1
        return GW.do_import(self, nm)


def cov_report(es, va, label=''):
    got, tot, miss = es.coverage(va)
    cf = os.environ.get('PW_COVOUT')
    if cf:
        # the union of the covered instructions over several runs (other seeds / dialog ids / classes of cases) of the same function
        import json
        try:
            db = json.load(open(cf))
        except Exception:
            db = {}
        cov = set(db.get('%08x' % va, [])) | (set(es.cov_all[va]) - set(miss))
        db['%08x' % va] = sorted(cov)
        db['%08x_total' % va] = len(es.cov_all[va])
        json.dump(db, open(cf, 'w'))
        miss = [a for a in es.cov_all[va] if a not in cov]
        got = len(es.cov_all[va]) - len(miss)
        label += ' (union)'
    runs = []
    for a in miss:
        if runs and a - runs[-1][1] < 16:
            runs[-1][1] = a
        else:
            runs.append([a, a])
    print('  coverage %s %08x: %d of %d instructions; not run: %s' % (label, va, got, tot,
          ' '.join('%x-%x' % (x, y) if x != y else '%x' % x for x, y in runs[:24])))


def diff_logs(la, lb, limit=4):
    import difflib
    sm = difflib.SequenceMatcher(None, [repr(x) for x in la], [repr(x) for x in lb], autojunk=False)
    n = 0
    for tag, i1, i2, j1, j2 in sm.get_opcodes():
        if tag != 'equal':
            print('   %s stock[%d:%d] %s' % (tag, i1, i2, [ascii(x)[:240] for x in la[i1:i2]][:4]))
            print('        ours[%d:%d] %s' % (j1, j2, [ascii(x)[:240] for x in lb[j1:j2]][:4]))
            n += 1
            if n >= limit:
                break


def show(a, b):
    for key in a:
        if a[key] != b.get(key):
            print('  differs:', key)
            if key == 'log':
                diff_logs(a[key], b[key])
            elif isinstance(a[key], dict) and isinstance(b[key], dict):
                for kk in sorted(set(a[key]) | set(b[key])):
                    if a[key].get(kk) != b[key].get(kk):
                        va_, vb_ = a[key].get(kk), b[key].get(kk)
                        if isinstance(va_, list) and isinstance(vb_, list) and len(va_) == len(vb_):
                            # a snapshot (dwords from +4): the offsets that differ
                            dd = [('+0x%x' % (4 * (i + 1)), x, y) for i, (x, y) in enumerate(zip(va_, vb_)) if x != y]
                            print('    %s: %d dwords differ (stock, ours): %s' % (kk, len(dd), ascii(dd[:8])[:400]))
                        else:
                            print('    %s stock %s ours %s' % (kk, ascii(va_)[:300], ascii(vb_)[:300]))
            else:
                print('   stock', ascii(a[key])[:400])
                print('   ours ', ascii(b[key])[:400])


def run_tests(label, es, eo, run, cases, A_stock, A_ours, limit=3):
    """compare(): `run(e, A, case)` returns the dict of PW.go; prints the first differences"""
    bad = err = 0
    for i, c in enumerate(cases):
        es.heap_ptr, eo.heap_ptr = es.heap_mark, eo.heap_mark
        try:
            a = run(es, A_stock, c)
        except Exception as ex:
            a = {'exc': str(ex)[:300], 'partial': list(es.log)}
        try:
            b = run(eo, A_ours, c)
        except Exception as ex:
            b = {'exc': str(ex)[:300], 'partial': list(eo.log)}
        es.log.clear()
        eo.log.clear()
        if os.environ.get('PW_SHOW') and i < int(os.environ['PW_SHOW']):
            print('--- %s case %d stock log:' % (label, i))
            for x in a.get('log', []):
                print('    ', ascii(x)[:200])
            print('    eax', a.get('eax'), 'pop', a.get('pop'), 'data', a.get('data'), 'gl', a.get('gl'))
            if os.environ.get('PW_SHOWO'):
                print('--- ours log:')
                for x in b.get('log', []):
                    print('    ', ascii(x)[:200])
        if 'exc' in a or 'exc' in b:
            err += 1
            if err <= 4:
                print('--- %s case %d raised' % (label, i), ascii(c)[:300], '\n   stock:', a.get('exc'), '\n   ours :', b.get('exc'))
                if os.environ.get('PW_PARTIAL'):
                    for nm_, d_ in (('stock', a), ('ours', b)):
                        print('   %s partial log:' % nm_)
                        for x in d_.get('partial', d_.get('log', []))[-16:]:
                            print('      ', ascii(x)[:200])
        elif a != b:
            bad += 1
            if bad <= limit:
                print('--- %s case %d differs' % (label, i), ascii(c)[:600])
                show(a, b)
    print('%-28s cases %4d  differ %3d  raised %3d' % (label, len(cases), bad, err))
    return bad + err
