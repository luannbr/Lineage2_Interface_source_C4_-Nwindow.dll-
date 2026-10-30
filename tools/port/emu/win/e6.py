# Differential test of NCChatWnd::ProcessChatLine (stock vs ours under unicorn). Every function it calls is replaced by a
# logging stub (args and the parameter stacks are compared); the console, desktop, users and windows are fake objects.
#   python e6.py [cases] [seed]
import sys, os, random, struct
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import e4
from e4 import *
import slotsyms
from capstone import Cs, CS_ARCH_X86, CS_MODE_32

FUNC = 0x100582d0
FUNC_END = FUNC + 7861
CRT = {0x10171439, 0x10172354, 0x10171a20, 0x1017187d}   # __wtol _wcsncpy __wcsicmp __chkstk wcslen: real code

IMP_POP = {'??0L2ParamStack@@QAE@H@Z': 4, '?PushBack@L2ParamStack@@QAEHPAX@Z': 4, '??1L2ParamStack@@QAE@XZ': 0,
           '?GetMSCommandData@FL2GameData@@QAEPAUFL2MagicSkillCommandData@@PAG@Z': 4,
           '?OnSaveStart@FL2ReplayManager@@QAEXXZ': 0, '?OnSaveStop@FL2ReplayManager@@QAEXXZ': 0,
           '?GetRemainStr@FParser@@QAEPAGXZ': 0, '?GetNextToken@FParser@@QAEPAGXZ': 0, '?GetName@User@@QAEPAGXZ': 0, '?appAtoi@@YAHPBG@Z': 0,
           '?appAtof@@YAMPBG@Z': 0}
IMP_POP.update({'GetLastError': 0, 'SetLastError': 4, 'IsProcessorFeaturePresent': 4})
stubs.IMPORT_POP.update(IMP_POP)

stock_pe = None


def scan_calls():
    """the direct call targets of the stock function outside its own range"""
    import pefile
    pe = pefile.PE(e4.STOCK)
    base = pe.OPTIONAL_HEADER.ImageBase
    mem = pe.get_memory_mapped_image()
    md = Cs(CS_ARCH_X86, CS_MODE_32)
    out = set()
    for i in md.disasm(bytes(mem[FUNC - base:FUNC_END - base]), FUNC):
        if i.mnemonic == 'call' and i.op_str.startswith('0x'):
            t = int(i.op_str, 16)
            if not (FUNC <= t < FUNC_END) and t not in CRT:
                out.add(t)
    return pe, base, mem, md, out


def ret_pop(mem, base, md, va):
    for i in md.disasm(bytes(mem[va - base:va - base + 600]), va):
        if i.mnemonic == 'ret':
            return int(i.op_str, 16) if i.op_str else 0
        if i.mnemonic == 'jmp' and i.op_str.startswith('dword ptr ['):
            return None      # an import thunk: leave to the import table
    return None


PE_, BASE_, MEM_, MD_, CALLEES = scan_calls()
POPS = {
    0x1004bc20: 0, 0x1004bc30: 4, 0x1004bbd0: 4, 0x10052f40: 16, 0x10053950: 4, 0x10056cd0: 4, 0x10053a50: 8,
    0x100563b0: 8, 0x10058210: 32, 0x10064ee0: 12, 0x1007d380: 8, 0x1006a2a0: 8, 0x10072df0: 4,
    0x10073e80: 0, 0x10073ed0: 0, 0x1006a770: 4,
    0x100683f0: 4, 0x1007e8d0: 12, 0x100802f0: 12, 0x1007a130: 4,
    0x100689d0: 4, 0x10068a80: 4, 0x10068aa0: 4, 0x10068ab0: 4, 0x10068790: 4, 0x10068990: 4, 0x100689a0: 4,
    0x10069ff0: 4, 0x10068050: 4, 0x100683a0: 4, 0x10067f40: 4, 0x10067f60: 4, 0x1006db80: 4, 0x1006df40: 4,
    0x10068400: 4, 0x10068410: 4,
    0x10069150: 0, 0x10069160: 0, 0x100687c0: 0, 0x100689c0: 0, 0x10069a80: 0, 0x100698a0: 0, 0x1006a000: 0,
    0x1007a210: 0, 0x100684b0: 0,
    0x10071c00: 24, 0x100683b0: 16, 0x10066780: 4, 0x100718b0: 4, 0x100719c0: 8, 0x10071930: 8, 0x10068fb0: 8,
    0x10171e4e: 0, 0x100e0170: 0, 0x10028450: 4, 0x10005d10: 4, 0x100085c0: 0, 0x100079b0: 4, 0x10072e20: 4, 0x10007d50: 0,
}
missing = [hex(v) for v in CALLEES if v not in POPS]
if missing:
    print('callees without a pop:', missing)


def chase(e, va):
    """addresses of ours to patch for a callee: the hook's entry, and through its thunks"""
    out = []
    lm = log_map()
    a = lm.get(va)
    n = 0
    while a and n < 4:
        out.append(a)
        b = bytes(e.uc.mem_read(a, 8))
        if b[0] == 0xE9:
            a = (a + 5 + struct.unpack('<i', b[1:5])[0]) & 0xFFFFFFFF
        elif b[:4] == bytes([0x55, 0x8B, 0xEC, 0x5D]) and b[4] == 0xE9:
            out.append(a)
            a = (a + 9 + struct.unpack('<i', bytes(e.uc.mem_read(a + 5, 4)))[0]) & 0xFFFFFFFF
        else:
            break
        n += 1
    return out


class E6(E4):
    def __init__(self, kind):
        E4.__init__(self, kind)
        self.fs_set(self.gdata + 0x722C8 + 1256 * 0x30 + 4, 'USAGE1256')
        self.cb = {}
        self.pstack = {}
        self.parser = {'tokens': [], 'idx': 0}
        self.case = None
        self.vtn = 0
        for nm in ('?GIsOnlyL2Console@@3HA', '?GIsOldChat@@3HA'):
            pass
        self.gvars = {}
        for e in self.pe.DIRECTORY_ENTRY_IMPORT:
            for imp in e.imports:
                nm = imp.name.decode() if imp.name else ''
                if nm in ('?GIsOnlyL2Console@@3HA', '?GIsOldChat@@3HA'):
                    v = self.alloc(16)
                    self.gvars[nm] = v
                    self.write32(imp.address, v)
        # the stubs of the callees
        n = 0
        lm = log_map()
        # hook build: our code calls the compiled member of a hooked callee (e.g. NCommandParser::ExpandToken), not the hook's forwarder that
        # log_map gives: patched through the decorated symbol too (slotsyms.auto_syms). The standalone has no forwarders (sa.func_map = the member)
        msyms = {} if e4._emu.sa.ENABLED or kind == 'stock' else slotsyms.auto_syms(list(POPS), e4.LOGFILE, verbose=False)
        for va, pop in sorted(POPS.items()):
            stub = STUB_BASE + 0xC000 + 16 * n
            n += 1
            self.uc.mem_write(stub, bytes([0xC3]))
            name = 'cb%08x' % va
            self.stubs[stub] = ('cb:' + name, None)
            self.cb[name] = (va, pop)
            jump = b'\x68' + struct.pack('<I', stub) + b'\xC3'
            if self.kind == 'stock':
                self.uc.mem_write(va, jump)
            else:
                self.uc.mem_write(NWBASE + (va - 0x10000000), jump)     # StockCallAt / the NW_STOCK stubs
                for a in chase(self, va):
                    self.uc.mem_write(a, jump)
                if va in msyms and msyms[va] in self.symbols:
                    self.uc.mem_write(self.symbols[msyms[va]], jump)
        # fake vtables: slots stubbed by number
        self.vslot_names = {}
        # the prefix table ours reads through NWBASE: pointers to copies of the strings
        # (standalone: ours reads its own initial table, which gen_globals.py --check proves equal to the stock's: nothing to seed)
        if self.kind == 'ours' and not e4._emu.sa.ENABLED:
            tab = NWBASE + (0x1023DBB8 - 0x10000000)
            for t in range(0, 20):
                for k in range(2):
                    p = struct.unpack('<I', bytes(MEM_[0x1023DBB8 + 8 * t + 4 * k - BASE_:0x1023DBB8 + 8 * t + 4 * k - BASE_ + 4]))[0]
                    s = self.wstr_stock(p)
                    self.write32(tab + 8 * t + 4 * k, self.put_wstr(s))

    def wstr_stock(self, a):
        out = []
        while True:
            c = struct.unpack('<H', bytes(MEM_[a - BASE_:a - BASE_ + 2]))[0]
            if c == 0:
                break
            out.append(chr(c))
            a += 2
        return ''.join(out)

    def vtable_with(self, slots):
        vt = self.alloc(0x400)
        self.uc.mem_write(vt, bytes(0x400))
        for slot, (nm, pop) in slots.items():
            stub = STUB_BASE + 0xE000 + 16 * self.vtn
            self.vtn += 1
            self.uc.mem_write(stub, bytes([0xC3]))
            self.stubs[stub] = ('vc:' + nm, None)
            self.cb[nm] = (slot, pop)
            self.write32(vt + 4 * slot, stub)
        return vt

    def dispatch(self, name):
        kind, nm = name.split(':', 1)
        if kind == 'cb':
            self.last_pop = self.cb[nm][1]
            return self.do_cb(nm)
        if kind == 'vc':
            self.last_pop = self.cb[nm][1]
            return self.do_vc(nm)
        return E4.dispatch(self, name)

    # ---- helpers ---------------------------------------------------------------------------------------------
    def nm(self, p):
        return self.objs.get(p, p if p < 0x1000 else '?')

    def val(self, v):
        """an argument for the log: a known object, a parameter stack, a string or a number"""
        if v in self.objs:
            return self.objs[v]
        if v in self.pstack:
            return ('PS', [self.val(x) for x in self.pstack[v]])
        if 0x60000000 <= v < 0x68000000 or 0x2F000000 <= v < 0x2F100000 or 0x10000000 <= v < 0x10400000:
            try:
                s = self.wstr(v, 80)
                if s is not None and all(32 <= ord(c) < 0xFFFF for c in s):
                    return 'S:' + s
            except Exception:
                pass
        return v

    def sargs(self, n):
        return [self.read32(self.reg(UC_X86_REG_ESP) + 4 + 4 * i) for i in range(n)]

    # ---- the callees --------------------------------------------------------------------------------------------
    def do_cb(self, nm):
        va, pop = self.cb[nm]
        t = self.this()
        a = self.sargs(pop // 4)
        c = self.case
        if va == 0x1004bc20:      # FParser::GetNextToken
            p = self.parser
            if p['idx'] < len(p['tokens']):
                tok = p['tokens'][p['idx']]
                p['idx'] += 1
                a2 = self.alloc(2 * 80)
                self.write_wstr_at(a2, tok)
                self.log.append(('GetNextToken',))
                return a2
            self.log.append(('GetNextToken', None))
            return 0
        if va == 0x10171e4e:      # __wcsicmp (the stock CRT uses the thread data)
            b2 = self.sargs(2); x, y = self.wstr(b2[0]).lower(), self.wstr(b2[1]).lower()
            return (0 if x == y else (1 if x > y else 0xFFFFFFFF))
        if va == 0x1004bc30:      # ExpandToken
            tok = self.wstr(a[0]) if a[0] else None
            self.n_exp += 1
            r = 1 if (self.n_exp * 7 + c['seed']) % 7 == 0 else 0
            if r and a[0]:
                self.write_wstr_at(a[0], str((int(tok) + 1) % 10) if tok.isdigit() else 'N' + tok[:20])
            self.log.append(('ExpandToken', tok, r))
            return r
        if va == 0x1004bbd0:      # GetCommand
            self.parser = {'tokens': list(c['tokens']), 'idx': 0}
            self.log.append(('GetCommand', self.wstr(a[0])))
            return c['cmd'] & 0xFFFFFFFF
        if va == 0x10053a50:      # ExpandLine(out, in)
            s = self.wstr(a[1])
            self.write_wstr_at(a[0], s)
            self.log.append(('ExpandLine', s))
            return None
        if va == 0x100563b0:      # NChatLimiter::AddLine(len, text)
            self.uc.mem_write(self.chat + 0x20C, bytes([c['limited']]))
            self.log.append(('AddLine', a[0], self.wstr(a[1])))
            return None
        if va == 0x10073e80:      # GetMyUser
            self.log.append(('GetMyUser',))
            return self.me
        if va == 0x10073ed0:      # GetSelectedActor
            self.log.append(('GetSelectedActor',))
            return self.target
        if va == 0x1006a770:      # FindUserByName
            self.log.append(('FindUserByName', self.wstr(a[0])))
            return self.found
        if va == 0x100085c0:      # NDialogInfo::NDialogInfo
            self.write32(t, 0xFFFFFFFF); self.write32(t + 4, 3); self.write32(t + 8, 0)
            self.write32(t + 12, 1); self.write32(t + 16, 0); self.write32(t + 20, 0)
            self.log.append(('NDialogInfo',))
            return t
        if va == 0x10007d50:
            return None
        if va == 0x100079b0:      # SetSystemMessage
            self.log.append(('SetSystemMessage', a[0]))
            return None
        if va == 0x10072e20:      # ShowDialog(info)
            self.log.append(('ShowDialog', [self.rds32(a[0] + 4 * i) for i in range(6)] if False else
                             [self.read32(a[0] + 4 * i) for i in range(6)]))
            return None
        # generic: log the callee, this and its arguments (parameter stacks expanded)
        self.log.append((hex(va), self.val(t), [self.val(x) for x in a]))
        if va == 0x10064ee0:
            return 1
        return 0

    def do_vc(self, nm):
        t = self.this()
        a = self.sargs(self.cb[nm][1] // 4)
        self.log.append((nm, self.val(t), [self.val(x) for x in a]))
        if nm == 'IconIsShow':
            return self.case['icon_shown']
        return 0

    def do_import(self, nm):
        t = self.this()
        a = self.arg
        if nm == '??0L2ParamStack@@QAE@H@Z':
            self.pstack[t] = []
            return t
        if nm == '?PushBack@L2ParamStack@@QAEHPAX@Z':
            self.pstack[t].append(a(0))
            return 1
        if nm == '??1L2ParamStack@@QAE@XZ':
            return None
        if nm == '?GetMSCommandData@FL2GameData@@QAEPAUFL2MagicSkillCommandData@@PAG@Z':
            self.log.append(('GetMSCommandData', self.wstr(a(0))))
            return self.skill
        if nm in ('?OnSaveStart@FL2ReplayManager@@QAEXXZ', '?OnSaveStop@FL2ReplayManager@@QAEXXZ'):
            self.log.append((nm.split('@')[0], self.val(t)))
            return None
        if nm == '?GetNextToken@FParser@@QAEPAGXZ':
            return self.do_cb('cb%08x' % 0x1004bc20)
        if nm == '?GetRemainStr@FParser@@QAEPAGXZ':
            p = self.parser
            rest = ' '.join(p['tokens'][p['idx']:])
            self.log.append(('GetRemainStr', rest))
            if not rest:
                return 0
            a2 = self.alloc(2 * 400)
            self.write_wstr_at(a2, rest)
            return a2
        if nm == '?GetName@User@@QAEPAGXZ':
            return t + 0x1C
        if nm == '?appAtoi@@YAHPBG@Z':
            s = self.wstr(a(0))
            try:
                return int(s) & 0xFFFFFFFF
            except Exception:
                return 0
        if nm == 'IsProcessorFeaturePresent':
            return 0
        if nm in ('GetLastError', 'SetLastError'):
            return 0
        if nm == '?appMemset@@YAXPAXHH@Z':
            self.uc.mem_write(a(0), bytes([a(1) & 0xFF]) * a(2))
            return None
        if nm == '?appAtof@@YAMPBG@Z':
            import math
            try:
                x = float(self.wstr(a(0)))
            except Exception:
                x = 0.0
            if x == 0.0:
                mant, ex = 0, 0
            else:
                m, e2 = math.frexp(abs(x))
                mant, ex = int(m * 2 ** 64), e2 - 1 + 16383
            self.log.append(('appAtof', x))
            self.uc.reg_write(UC_X86_REG_FPCW, 0x37F)
            self.uc.reg_write(UC_X86_REG_FPSW, 0)
            self.uc.reg_write(UC_X86_REG_FPTAG, 0xFFFC)
            self.uc.reg_write(UC_X86_REG_FP0, (mant, ex))
            return None
        return E4.do_import(self, nm)


def build_world(e, c):
    e.case = c
    e.n_exp = 0
    e.objs = {}
    e.pstack = {}
    e.parser = {'tokens': [], 'idx': 0}
    chat = e.alloc(0x700)
    e.uc.mem_write(chat, bytes(0x700))
    e.chat = chat
    e.objs[chat] = 'chat'
    e.write32(chat + 0x1D0, c['curtab'])
    e.write32(chat + 0x74, 0x7777)
    # the console, its engine and the desktop
    con = e.console
    e.uc.mem_write(con, bytes(0x4000))
    e.objs[con] = 'console'
    eng = e.alloc(0x200); e.uc.mem_write(eng, bytes(0x200)); e.objs[eng] = 'engine'
    nh = e.alloc(0x40); e.uc.mem_write(nh, bytes(0x40)); e.objs[nh] = 'nethandler'
    e.write32(nh, e.vt_nh)
    e.write32(eng + 0x64, nh)
    if c['replay']:
        rm = e.alloc(0x40); e.uc.mem_write(rm, bytes(0x40)); e.objs[rm] = 'replay'
        e.write32(eng + 0x18C, rm)
    e.write32(con + 0x54, eng)
    e.write32(con, e.vt_con)
    e.write32(con + 0x3C6C, c['dist'])
    desk = e.alloc(0x400); e.uc.mem_write(desk, bytes(0x400)); e.objs[desk] = 'desktop'
    e.write32(con + 0x3BF4, desk)
    pb = e.alloc(0x100); e.uc.mem_write(pb, bytes(0x100)); e.objs[pb] = 'petition'
    e.write32(pb, e.vt_wnd)
    te = e.alloc(0x40); e.objs[te] = 'textedit'
    e.write32(pb + 0xF8, te)
    e.write32(desk + 0x110, pb)
    mw = e.alloc(0x100); e.uc.mem_write(mw, bytes(0x100)); e.objs[mw] = 'match'
    cb1 = e.alloc(0x40); e.objs[cb1] = 'combo1'
    cb2 = e.alloc(0x40); e.objs[cb2] = 'combo2'
    e.write32(mw + 0xEC, cb1); e.write32(mw + 0xF0, cb2)
    e.write32(desk + 0x164, mw)
    pr = e.alloc(0x100); e.uc.mem_write(pr, bytes(0x100)); e.objs[pr] = 'partyroom'
    e.write32(pr, e.vt_wnd)
    ic = e.alloc(0x100); e.uc.mem_write(ic, bytes(0x100)); e.objs[ic] = 'icon'
    e.write32(ic, e.vt_icon)
    e.write32(pr + 0x3C, ic)
    e.write32(desk + 0x16C, pr)
    e.skill = e.alloc(0x40) if c['skill'] else 0
    if e.skill:
        e.write32(e.skill, 0x1234)
        e.objs[e.skill] = 'skill'
    # users
    def user(name, level, unk8, oid, tag):
        u = e.alloc(0x300); e.uc.mem_write(u, bytes(0x300)); e.objs[u] = tag
        e.write32(u + 8, unk8); e.write32(u + 0x18, oid); e.write_wstr_at(u + 0x1C, name)
        e.write32(u + 0x1A0, level)
        pw = e.alloc(0x300); e.uc.mem_write(pw, bytes(0x300))
        e.write32(pw + 0x60, 0x500 + oid)
        for i in range(3):
            e.write32(pw + 0x1BC + 4 * i, f32(1.5 + i + oid))
        e.write32(u + 0x158, pw if (c['pawn'] or tag == 'me') else 0)
        return u
    e.me = user('Me', c['level'], 0, 7, 'me') if c['hasme'] else 0
    e.target = user('Tgt', 0, c['unk8'], 9, 'target') if c['hastarget'] else 0
    e.found = user('Fnd', 0, 0, 11, 'found') if c['hasfound'] else 0
    e.write32(e.gvars['?GIsOnlyL2Console@@3HA'], c['onlyconsole'])
    e.write32(e.gvars['?GIsOldChat@@3HA'], c['oldchat'])
    return chat


def run6(e, A, c):
    mark = len(e.log)
    chat = build_world(e, c)
    text = e.put_wstr(c['text'])
    e.call(A['pcl'], this=chat, args=[text])
    return dict(log=e.log[mark:])


def prefixes():
    out = {}
    for t in range(0, 20):
        a = 0x1023DBB8 + 8 * t
        out[t] = [''.join([]) for _ in range(2)]
        for k in range(2):
            p = struct.unpack('<I', bytes(MEM_[a + 4 * k - BASE_:a + 4 * k - BASE_ + 4]))[0]
            out[t][k] = wstr_stock_global(p)
    return out


def wstr_stock_global(a):
    out = []
    while True:
        c = struct.unpack('<H', bytes(MEM_[a - BASE_:a - BASE_ + 2]))[0]
        if c == 0:
            break
        out.append(chr(c))
        a += 2
    return ''.join(out)


PREFIX = None
CMDS = list(range(0, 0x70)) + [0x7F, 0x80, 0xFF, 0x10000000, 0x10000001, 0x20000000, 0x20000000, 0x20000000, -1]


def rand6(r):
    global PREFIX
    if PREFIX is None:
        PREFIX = prefixes()
    cmd = 0x20000000 if r.random() < 0.45 else (0x10000001 if r.random() < 0.15 else r.choice(CMDS))
    words = ['bob', 'a', '12', '3', 'x y', 'Zed']
    ntok = r.choice([0, 0, 1, 1, 2, 3])
    tokens = [r.choice(words).replace(' ', '') for _ in range(ntok)]
    text = '/cmd ' + ' '.join(tokens)
    if cmd == 0x10000001:
        kind = r.choice(['yo', 'setcl', 'setcl', 'setcl', 'other', 'stat'])
        if kind == 'yo':
            text = '///yo bob'; tokens = ['bob']
        elif kind == 'setcl':
            n = r.choice([0, 1, 2, 3, 4, 5, 6, 7, 8, 8, 8])
            tokens = [str(r.randint(0, 99)) for _ in range(n)]
            text = '///setcl ' + ' '.join(tokens)
        elif kind == 'stat':
            text = '///stat fps'; tokens = []
        else:
            text = '///foo bar'; tokens = ['bar']
    elif cmd == 0x10000000:
        text = '//build ' + ' '.join(tokens)
    elif cmd == 0x20000000:
        t = r.choice([0, 1, 2, 3, 4, 6, 7, 8, 9, 17, 5, 12])
        pre = r.choice([PREFIX[t][0], PREFIX[t][1], '', '＂', 'x']) if t in PREFIX else ''
        body = r.choice(['hello', 'hello world', ' ', '', 'bob hi there', 'bob　hi', 'a b c', 'x', ' hi', 'bob', 'bob ', 'a　', 'z' * 100 + ' q', 'bob　'])
        text = pre + body
        tokens = body.split()
    elif cmd in (0x1B, 0x1C, 0x1D, 0x50):
        tokens = [str(r.randint(0, 11)), str(r.randint(0, 13))][:r.choice([0, 1, 2, 2, 2])]
    return dict(cmd=cmd, tokens=tokens, text=text, curtab=r.randint(0, 4), seed=r.randint(0, 99),
                limited=r.randint(0, 1), icon_shown=r.randint(0, 1), replay=r.randint(0, 1), dist=r.randint(0, 3),
                skill=r.randint(0, 1), pawn=r.randint(0, 1), level=r.choice([0, 0, 1, 9, 5]),
                hasme=r.choice([1, 1, 1, 0]), hastarget=r.choice([1, 1, 0]), unk8=r.choice([0, 0, 1]),
                hasfound=r.choice([1, 0]), onlyconsole=r.choice([0, 1]), oldchat=r.choice([0, 0, 1]))


def main(n, seed):
    r = random.Random(seed)
    cases = [rand6(r) for _ in range(n)]
    es = E6x('stock'); eo = E6x('ours')
    lm = log_map()
    As = {'pcl': FUNC}
    Ao = {'pcl': lm[FUNC]}
    es.heap_mark, eo.heap_mark = es.heap_ptr, eo.heap_ptr
    bad = err = 0
    bycmd = {}
    for i, c in enumerate(cases):
        es.heap_ptr, eo.heap_ptr = es.heap_mark, eo.heap_mark
        try:
            a = run6(es, As, c)
        except Exception as ex:
            a = {'exc': str(ex)[:300]}
        try:
            b = run6(eo, Ao, c)
        except Exception as ex:
            b = {'exc': str(ex)[:300]}
        k = bycmd.setdefault(c['cmd'], [0, 0, 0])
        k[0] += 1
        if 'exc' in a or 'exc' in b:
            err += 1
            k[2] += 1
            if err <= 5:
                print('--- case %d raised cmd %x' % (i, c['cmd']), ascii(c['text']), a.get('exc'), '|', b.get('exc'))
        elif a != b:
            bad += 1
            k[1] += 1
            if bad <= 6:
                print('--- case', i, 'cmd %x' % c['cmd'], ascii({kk: v for kk, v in c.items() if kk not in ('seed',)}))
                print('  stock:', ascii(a['log'])[:1500])
                print('  ours :', ascii(b['log'])[:1500])
        es.log.clear(); eo.log.clear()
    print('cases', n, 'differ', bad, 'raised', err)
    nd = sorted((cm, v) for cm, v in bycmd.items() if v[1] or v[2])
    for cm, v in nd[:40]:
        print('  cmd %x: %d cases, %d differ, %d raised' % (cm & 0xFFFFFFFF, v[0], v[1], v[2]))


class E6x(E6):
    def __init__(self, kind):
        E6.__init__(self, kind)
        self.vt_con = self.vtable_with({194: ('BuilderCommand', 4)})
        self.vt_nh = self.vtable_with({37: ('NetHandler37', 4)})
        self.vt_wnd = self.vtable_with({48: ('WndShow', 0), 29: ('WndFocus', 0), 53: ('WndRestore', 0)})
        self.vt_icon = self.vtable_with({41: ('IconIsShow', 0)})


if __name__ == '__main__':
    main(int(sys.argv[1]) if len(sys.argv) > 1 else 300, int(sys.argv[2]) if len(sys.argv) > 2 else 1)
