# Differential tests of NCTaiwanIME (stock vs ours under unicorn): the Taiwan class of the IME harness (t_ime.py, t_ime2.py: the
# Chinese IME's). Every function of the class is run on the same random worlds by the stock code and by our hook, comparing the return
# value, the callee-saved registers, the stack pointer, every byte of the object and of the candidate box, the heap blocks written,
# and the log of every call made (imports, callees, virtual calls).
#   python t_taiwan.py [cases] [seed] [comp,notify,rrs,getid,readcand,deact,destroy,create,lang]   (default: all)
#   python t_taiwan.py --cov [cases] [seed]     the stock instructions the cases reach in each function
#   python t_taiwan.py --mut=K [cases] [seed] [tests]   flip K conditional jumps of our compiled function, one at a time: how many the cases kill
import sys, os, random, struct, re
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from gw import *
import gw
import stubs
import t_ime
import t_ime2
from t_ime2 import IW2
from unicorn.x86_const import *
import emu as _emu

OBJ_SIZE = 0x625C
FUNCS = {'comp': 0x1001a460, 'notify': 0x1001c950, 'rrs': 0x10016d40, 'getid': 0x10016bd0, 'readcand': 0x1001b2b0,
         'deact': 0x10017060, 'destroy': 0x1001a380, 'create': 0x1001a2b0, 'lang': 0x10017040}
# the extent of each function in the stock image, funclets included (for --cov)
EXTENT = {'comp': (0x1001a460, 0x1001b2a8), 'notify': (0x1001c950, 0x1001cb64), 'rrs': (0x10016d40, 0x10016f77),
          'getid': (0x10016bd0, 0x10016d39), 'readcand': (0x1001b2b0, 0x1001b38d), 'deact': (0x10017060, 0x100170bd),
          'destroy': (0x1001a380, 0x1001a429), 'create': (0x1001a2b0, 0x1001a37d), 'lang': (0x10017040, 0x1001705b)}

# stock va: (name, bytes the callee pops, returns this, ours' symbols (the first present is patched))
CALLEES = {
    0x10016bd0: ('GetImeId', 0, 0, ['?GetImeId@NCTaiwanIME@@SAKXZ']),
    0x10016f80: ('IsTaiwanIME', 0, 0, ['?IsTaiwanIME@NCTaiwanIME@@QAEHXZ']),
    0x1002b400: ('ImeIsUIMessage', 12, 0, []),
    0x1002b3d0: ('ImeGetContext', 0, 0, ['?ImeGetContext@NCObject@@QAEPAUHIMC__@@XZ']),
    0x1002b3e0: ('ImeReleaseContext', 4, 0, ['?ImeReleaseContext@NCObject@@QAEHPAUHIMC__@@@Z']),
    0x100166f0: ('IsUnicodeOS', 0, 0, ['?IsUnicodeOS@NCIME@@QAEHXZ']),
    0x10006c10: ('SendToParent', 12, 0, ['?SendToParent@NControl@@QAEHIIJ@Z']),
    0x100169f0: ('FreeCandList', 0, 0, ['?FreeCandList@NCTaiwanIME@@QAEXXZ', '?FreeCandList@NCJapaneseIME@@QAEXXZ']),
    0x10016530: ('NCIMEMode', 0, 0, ['?IsRestrictedMode@NCIME@@QAEHXZ']),
    0x1001b2b0: ('ReadCandList', 0, 0, ['?ReadCandList@NCTaiwanIME@@QAEXXZ']),
    0x10016d40: ('ReadReadingString', 0, 0, ['?ReadReadingString@NCTaiwanIME@@QAEXXZ']),
    0x100179c0: ('FillCandidateBox', 0, 0, ['?FillCandidateBox@NCTaiwanIME@@QAEXXZ', '?FillCandidateBox@NCJapaneseIME@@QAEXXZ']),
    0x10016640: ('NCIMEDeactivate', 0, 0, ['?Deactivate@NCIME@@UAEXXZ']),
    0x100178e0: ('NCIMEOnCreate', 0, 0, ['?OnCreate@NCIME@@UAEHXZ']),
    0x10035880: ('RemoveChild', 4, 0, ['?RemoveChildWindow@NCWnd@@QAEXPAUNWndEntry@@@Z']),
    0x10035840: ('AddChild', 8, 0, ['?AddChildWindow@NCWnd@@QAEPAUNWndEntry@@PAV1@K@Z']),
    0x10016b20: ('LoadReadingString', 0, 0, ['?LoadReadingString@NCTaiwanIME@@SAXXZ']),
    0x10016b80: ('LoadImmFunctions', 0, 0, ['?LoadImmFunctions@NCIME@@SAXXZ']),
    0x10016750: ('UpdateCodePage', 0, 0, ['?UpdateCodePage@NCIME@@QAEXXZ']),
    0x10004f50: ('NCCandidateBox', 8, 1, ['??0NCCandidateBox@@QAE@HH@Z']),
    0x10016880: ('StockDelete', 0, 0, []),
    0x10004d30: ('ClauseAdd', 4, 0, []),
}
stubs.IMPORT_POP.update({'?StaticAllocateObject@UObject@@SAPAV1@PAVUClass@@PAV1@VFName@@K1PAVFOutputDevice@@11@Z': 0x20,
                         '?GetTransientPackage@UObject@@SAPAVUPackage@@XZ': 0, '?Realloc@FArray@@IAEXH@Z': 4,
                         'ImmNotifyIME': 16, 'ImmGetCandidateListW': 16, 'ImmGetConversionStatus': 12,
                         'ImmSetConversionStatus': 12, '?appStrcpy@@YAPAGPAGPBG@Z': 0})
gw.VPOP.update({3: 4, 40: 28, 41: 0, 49: 0, 25: 12})
STATIC_ALLOC = '?StaticAllocateObject@UObject@@SAPAV1@PAVUClass@@PAV1@VFName@@K1PAVFOutputDevice@@11@Z'


def map_symbols():
    out = {}
    for ln in open(_emu.OURS_MAP, encoding='latin-1'):
        m = re.match(r'\s*[0-9a-f]{4}:[0-9a-f]{8}\s+(\S+)\s+([0-9a-f]{8})\s', ln)
        if m:
            out.setdefault(m.group(1), int(m.group(2), 16))
    return out


MAP = map_symbols()


class TW(IW2):
    def __init__(self, kind, test):
        callees = {}
        syms = {}
        self.extra_syms = {}
        for va, (name, pop, rthis, cand) in CALLEES.items():
            if va == FUNCS[test]:
                continue
            callees[va] = (name, pop, rthis)
            present = [s for s in cand if s in MAP]
            if present:
                syms[va] = present[0]
                self.extra_syms[va] = present[1:]
        GW.__init__(self, kind, callees, syms)
        if kind == 'ours':
            for va, more in self.extra_syms.items():
                name, pop, rthis = callees[va]
                stub = [s for s, (n, _) in self.stubs.items() if n == 'cb:' + name][0]
                jump = b'\x68' + struct.pack('<I', stub) + b'\xC3'
                for s in more:
                    self.uc.mem_write(self.symbols[s], jump)
        self.case = None
        self.rets['GetImeId'] = lambda e, t, a: e.case['imeid']
        self.rets['IsTaiwanIME'] = lambda e, t, a: e.case['taiwan']
        self.rets['IsUnicodeOS'] = lambda e, t, a: e.case['unicode']
        self.rets['ImeGetContext'] = lambda e, t, a: 0x1234
        self.rets['NCIMEMode'] = lambda e, t, a: e.case.get('mode_ok', 0)
        self.rets['AddChild'] = self.add_child
        self.vrets[3] = lambda e, t, a: (e.log.append(('delete', e.objs.get(t, t), a)), 0)[1]
        base = 0 if kind == 'stock' else NWBASE - 0x10000000
        self.gbase = base
        self.rs_stub = self.mkstub('GetReadingString', 24)
        self.lock_ic = self.mkstub('LockIMC', 4)
        self.unlock_ic = self.mkstub('UnlockIMC', 4)
        self.lock_icc = self.mkstub('LockIMCC', 4)
        self.unlock_icc = self.mkstub('UnlockIMCC', 4)
        self.candbox_for_new = None

    def add_child(self, e, t, a):
        # the entry of the new child: [entry+0xC] -> parent node, [node+4] -> parent entry's window data with the offsets at +0x44, +0x48
        c = e.case
        ent = e.alloc(0x40)
        e.uc.mem_write(ent, bytes(0x40))
        if c.get('entry_chain', 1):
            node = e.alloc(0x20)
            e.uc.mem_write(node, bytes(0x20))
            top = e.alloc(0x60)
            e.uc.mem_write(top, bytes(0x60))
            e.write32(top + 0x44, c.get('off_x', 7))
            e.write32(top + 0x48, c.get('off_y', 9))
            e.write32(node + 4, top)
            e.write32(ent + 0xC, node)
        e.objs[ent] = 'entry:child'
        return ent

    def dispatch(self, name):
        kind, nm = name.split(':', 1)
        if kind == 'imp' and nm == STATIC_ALLOC:
            self.last_pop = 0      # cdecl: the caller adds 0x20
            a = self.sargs(8)
            # the class argument as the STOCK address of the UClass (hook build: the stand-in NWBASE + rva; standalone: the PrivateStaticClass object)
            self.log.append(('StaticAllocateObject', hex(self.stockptr(a[0])), a[1], a[2:6]))
            return self.candbox_for_new
        if kind == 'imp' and nm == '?GetTransientPackage@UObject@@SAPAVUPackage@@XZ':
            self.last_pop = 0
            self.log.append(('GetTransientPackage',))
            return 0x7777
        if kind == 'cb' and nm == 'ClauseAdd':
            # TImeTextArray::AddItem: the item (0x804 bytes, its text up to the first NUL and its flag count) appended
            self.last_pop = 4
            t = self.this()
            item = self.sargs(1)[0]
            data, num, mx = self.read32(t), self.read32(t + 4), self.read32(t + 8)
            idx = num
            num += 1
            if num > mx:
                mx = num + 3 * num // 8 + 16
                nd = self.alloc(mx * 0x804)
                if data:
                    self.uc.mem_write(nd, bytes(self.uc.mem_read(data, idx * 0x804)))
                data = nd
            raw = bytes(self.uc.mem_read(item, 0x804))
            self.uc.mem_write(data + idx * 0x804, raw)
            self.write32(t, data)
            self.write32(t + 4, num)
            self.write32(t + 8, mx)
            self.log.append(('ClauseAdd', idx, item_text(raw), struct.unpack('<i', raw[0x800:0x804])[0]))
            return idx
        return IW2.dispatch(self, name)

    def do_import(self, nm):
        if nm == 'MultiByteToWideChar' and self.case.get('mb_zero'):
            # the conversion fails (0 characters): the callers guard the length arithmetic against it
            self.log.append(('MultiByteToWideChar', 'fails'))
            return 0
        if nm == '?Realloc@FArray@@IAEXH@Z':
            self.log.append(('Realloc', self.val(self.this()), self.arg(0)))
        return IW2.do_import(self, nm)


def item_text(raw):
    """the NImeText's text up to its first NUL (the rest of the 1024 characters is uninitialised stack in both sides)"""
    out = []
    for i in range(0, 0x800, 2):
        ch = struct.unpack('<H', raw[i:i + 2])[0]
        if ch == 0:
            break
        out.append(ch)
    return tuple(out)


def rand_case_tw(r, test):
    c = t_ime.rand_case(r, 'comp')
    c['test'] = test
    c['taiwan'] = r.randint(0, 1) if r.random() < 0.25 else 1
    c['imeid'] = r.choice([0, 0x40001, 0x40002, 0x40003, 0x40004, 0x40005, 0x50000, 0x50001, 0x50002, 0x50003, 0x60000,
                           0x60001, 0x12345, 0x40002, 0x50000, 0x50001, 0x50002, 0x40003])
    c['ime_type'] = r.choice([0, 1, 7, 7, 8, 9, 10, 11, 12])
    c['mb_zero'] = r.random() < 0.15
    c['entry_chain'] = r.choice([0, 1, 1])
    c['off_x'] = r.randint(0, 50)
    c['off_y'] = r.randint(0, 50)
    if test == 'notify':
        c['wparam'] = r.choice([1, 2, 3, 4, 5, 6, 6, 7, 8, 9, 10, 11, 12, 13, 14, 14, 14, 14, 15, 0, 100, 3, 4, 5, 5])
        c['lparam'] = r.choice([1, 2, 3, 16, 17, 26, 27, 28, 1, 2, 16, 26, 28, 0, 5, -1, 29, 15])
    if test == 'lang':
        c['wparam'] = r.randint(0, 3)
        c['lparam'] = r.randint(0, 3)
    # the private data of the old IMEs and the reading string API
    c['rs'] = t_ime.rand_wide(r, r.randint(0, 14))
    c['rs_len'] = r.choice([0, len(c['rs']) // 2, len(c['rs']) // 2, 3, 1])
    c['rs_err'] = r.randint(0, 5)
    c['has_rs'] = r.randint(0, 1)
    c['private'] = t_ime.rand_wide(r, 80) + bytes(80)
    c['ptr_zero'] = [r.random() < 0.15 for _ in range(5)]
    c['len4x'] = r.choice([0, 1, 2, 3, 5, 8, 12])
    c['err4x'] = r.randint(0, 8)
    c['len50'] = r.choice([0, 2, 4, 6, 10, 16, 1, 3])
    c['err50'] = r.choice([0, 2, 4, 6, 1])
    c['len5x'] = r.choice([0, 1, 2, 3, 5, 8, 12])
    c['err5x'] = r.randint(0, 8)
    # GetImeId's world
    c['layout'] = r.choice([0xE0080404, 0xE0090404, 0xE0080404, 0xE0090404, 0xE00E0804, 0x04090409, 0xE0010404, 0xE0080804])
    c['imefile'] = r.choice(['TINTLGNT.IME', 'CINTLGNT.IME', 'MSTCIPHA.IME', 'tintlgnt.ime', 'mstcipha.ime', 'OTHER.IME', 'PINTLGNT.IME',
                             None, 'TINTLGNT.IME', 'CINTLGNT.IME'])
    c['vsize'] = r.choice([0, 100, 200, 100])
    c['vinfo_ok'] = r.randint(0, 1) if r.random() < 0.3 else 1
    c['vq_ok'] = r.randint(0, 1) if r.random() < 0.3 else 1
    c['verms'] = r.choice([0x40001, 0x40002, 0x40003, 0x40004, 0x40005, 0x50000, 0x50001, 0x50002, 0x50003, 0x60000, 0x60001,
                           0x40002 | 0x10000, 0x50001 | 0x7700, 0x20002, 0xFFFF00FF, 0x60000 | 0xAB00, 0x40003 | 0x550000])
    # the candidate list the IME answers
    cnt = r.randint(0, 25)
    page = r.choice([0, 5, 9, 10, 9, 9, 7])
    sel = r.randint(0, cnt + 2)
    start = r.choice([0, 0, 9, 18, r.randint(0, 3)])
    hdr = struct.pack('<6I', 0, 0, cnt, sel, start, page) + b''.join(struct.pack('<I', 0x30 + 4 * cnt + 2 * k) for k in range(cnt))
    body = b''.join(t_ime.rand_wide(r, 2) + bytes(2) for _ in range(cnt)) + bytes(16)
    c['candlist'] = (hdr + body) if r.random() < 0.8 else b''
    return c


def clear_obj_world(e, c):
    rnd = random.Random(c['seed'])
    e.case = c
    e.objs = {}
    e.shown = {}
    e.set_lang(c['lang'])
    e.ime_type = c['ime_type']
    obj = e.alloc(OBJ_SIZE + 16)
    # random bytes over the whole object: a store that one side makes and the other does not shows in the snapshot
    e.uc.mem_write(obj, rnd.randbytes(OBJ_SIZE) + bytes(16))
    e.write32(obj, e.vt)
    e.objs[obj] = 'ime'
    e.write32(obj + 0x1FC, c['active'])
    e.write32(obj + 0x1F8, c['mode'])
    e.write32(obj + 0x21C, 949)
    for off in (0x220, 0xA20, 0x1220, 0x2220):
        e.uc.mem_write(obj + off, t_ime.rand_wide(rnd, 40) + bytes(2))
    cbox = e.window('candbox', size=0x300, rnd=rnd)
    e.write32(obj + 0x4258, cbox)
    e.shown[cbox] = c['candshown']
    e.candbox_for_new = cbox
    # the clauses array: empty, or with a few dead entries (OnDestroy walks them)
    n = rnd.choice([0, 0, 2, 4])
    data = e.alloc(0x804 * 4 + 16) if n else 0
    e.write32(obj + 0x4248, data)
    e.write32(obj + 0x424C, n)
    e.write32(obj + 0x4250, n + rnd.choice([0, 3]))
    if c['hascand']:
        size = 0x20 + 4 * c['ncand'] + 40 * 2
        cl = e.alloc(size + 16)
        e.uc.mem_write(cl, bytes(size + 16))
        e.write32(cl, size)
        e.write32(cl + 8, c['ncand'])
        e.objs[cl] = 'cand'
        e.write32(obj + 0x4254, cl)
    else:
        e.write32(obj + 0x4254, 0)
    # the reading string machinery (stock globals: GetReadingString is the Taiwanese IME's own, 0x102BF048)
    g = e.gbase
    e.write32((0x102BF048 + g) if e.kind == 'stock' else NWBASE + 0x2BF048, e.rs_stub if c['has_rs'] else 0)
    for off, stub in ((0x50, e.lock_ic), (0x54, e.unlock_ic), (0x58, e.lock_icc), (0x5C, e.unlock_icc)):
        e.write32((0x102BF000 + off) if e.kind == 'stock' else NWBASE + 0x2BF000 + off, stub)
    # the fake input context and the private data per IME version:
    #   4.2 .. 4.4: [icc + 0x18] -> p, text at p + 0x38, length [p + 0x9C], error [p + 0xA0]
    #   5.0: [icc + 0xC] -> q, [q + 0x20] -> p, ANSI text at p + 0x40, length [p + 0x50], error [p + 0x54] (bytes)
    #   5.1, 5.2: [icc + 4] -> q, [q + 0x18] -> p, text at p + 0x40, length [p + 0x60], error [p + 0x64]
    e.ic = e.alloc(0x200)
    e.uc.mem_write(e.ic, bytes(0x200))
    e.icc = e.alloc(0x100)
    e.uc.mem_write(e.icc, bytes(0x100))
    e.write32(e.ic + 0x124, 0x5555)
    priv = e.alloc(0x400)
    e.uc.mem_write(priv, bytes(0x400))
    e.uc.mem_write(priv + 0x38, c['private'])
    e.write32(priv + 0x9C, c['len4x'])
    e.write32(priv + 0xA0, c['err4x'])
    e.write32(priv + 0x50, c['len50'])
    e.write32(priv + 0x54, c['err50'])
    e.write32(priv + 0x60, c['len5x'])
    e.write32(priv + 0x64, c['err5x'])
    pz = c['ptr_zero']
    e.write32(e.icc + 0x18, 0 if pz[0] else priv)
    q0 = e.alloc(0x100)
    e.uc.mem_write(q0, bytes(0x100))
    e.write32(q0 + 0x20, 0 if pz[1] else priv)
    e.write32(e.icc + 0xC, 0 if pz[2] else q0)
    q1 = e.alloc(0x100)
    e.uc.mem_write(q1, bytes(0x100))
    e.write32(q1 + 0x18, 0 if pz[3] else priv)
    e.write32(e.icc + 4, 0 if pz[4] else q1)
    return obj


def snapshot_obj(e, obj):
    words = struct.unpack('<%dI' % (OBJ_SIZE // 4), bytes(e.uc.mem_read(obj, OBJ_SIZE)))
    out = []
    for i, w in enumerate(words):
        off = 4 * i
        if off in (0x4248, 0x4254):
            out.append('ptr' if w else 0)
        else:
            out.append(e.objs.get(w, w))
    return out


def run(e, A, c):
    mark = len(e.log)
    obj = clear_obj_world(e, c)
    t = c['test']
    va = A[t]
    args = {'comp': [0, c['lparam']], 'notify': [c['wparam'], c['lparam']], 'lang': [c['wparam'], c['lparam']]}.get(t, [])
    args = [a & 0xFFFFFFFF for a in args]
    e.call(va, this=obj, args=args)
    ret = e.reg(UC_X86_REG_EAX) if t in ('comp', 'notify', 'getid', 'create', 'lang', 'destroy') else None
    esp = e.reg(UC_X86_REG_ESP) - (STACK_TOP - 0x1000)
    regs = [e.reg(r) for r in (UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP)]
    clauses = []
    n = e.read32(obj + 0x424C)
    d = e.read32(obj + 0x4248)
    for k in range(min(n, 8)):
        if d:
            raw = bytes(e.uc.mem_read(d + k * 0x804, 0x804))
            clauses.append((item_text(raw), struct.unpack('<i', raw[0x800:0x804])[0]))
    cl = e.read32(obj + 0x4254)
    cands = bytes(e.uc.mem_read(cl, 0x80)) if cl else None
    box = e.read32(obj + 0x4258)
    return dict(log=e.log[mark:], ret=ret, esp=esp, regs=regs, obj=snapshot_obj(e, obj), clauses=clauses, cand=cands,
                box=e.snapshot(box, 0x300) if box else None)


def make(kind, f):
    return TW(kind, f)


OURS_SYM = {'comp': '?OnImeComposition@NCTaiwanIME@@UAEHIJ@Z', 'notify': '?OnImeNotify@NCTaiwanIME@@UAEHIJ@Z',
            'rrs': '?ReadReadingString@NCTaiwanIME@@QAEXXZ', 'getid': '?GetImeId@NCTaiwanIME@@SAKXZ',
            'readcand': '?ReadCandList@NCTaiwanIME@@QAEXXZ', 'deact': '?Deactivate@NCTaiwanIME@@UAEXXZ',
            'destroy': '?OnDestroy@NCTaiwanIME@@UAEHXZ', 'create': '?OnCreate@NCTaiwanIME@@UAEHXZ',
            'lang': '?OnInputLangChange@NCTaiwanIME@@UAEHIJ@Z'}


def mutate(f, cases, As, Ao, es, limit):
    """Mutation test of the harness: flip the polarity of one conditional jump of our compiled function (its own symbol, not the
    hook's forwarder) and run the same cases; a mutant that no case tells from the stock code is a branch the cases do not judge.
    Prints the survivors."""
    from capstone import Cs, CS_ARCH_X86, CS_MODE_32
    start = MAP[OURS_SYM[f]]
    # the function's catch funclets are placed inside it (the compiler's try/catch of guard()): they are not its end
    catches = sorted(a for n, a in MAP.items() if n.startswith('__catch$' + OURS_SYM[f]))
    end = min(a for n, a in MAP.items() if a > start and not n.startswith('__catch$'))
    probe = make('ours', f)
    code = bytes(probe.uc.mem_read(start, end - start))
    sites = []
    for i in Cs(CS_ARCH_X86, CS_MODE_32).disasm(code, start):
        if i.mnemonic.startswith('j') and i.mnemonic != 'jmp' and not i.mnemonic.startswith('jmp'):
            sites.append(i.address + (1 if i.bytes[0] == 0x0F else 0))
    random.Random(7).shuffle(sites)
    sites = sites[:limit]
    ref = []
    for c in cases:
        es.heap_ptr = es.heap_mark
        try:
            ref.append(run(es, As, c))
        except Exception as ex:
            ref.append({'exc': str(ex)[:100]})
        es.log.clear()
    survivors = []
    for site in sorted(sites):
        eo = make('ours', f)
        eo.heap_mark = eo.heap_ptr
        old = bytes(eo.uc.mem_read(site, 1))[0]
        eo.uc.mem_write(site, bytes([old ^ 1]))
        # a flipped loop test may never end: stop a run after two million instructions and count it as a difference
        real_start = eo.uc.emu_start

        def limited(pc, until, timeout=0, count=0, _real=real_start, _eo=eo):
            _real(pc, until, timeout=0, count=2000000)
            if _eo.pending is None and _eo.uc.reg_read(UC_X86_REG_EIP) != SENTINEL:
                raise RuntimeError('mutant does not finish')
        eo.uc.emu_start = limited
        killed = False
        for c, want in zip(cases, ref):
            eo.heap_ptr = eo.heap_mark
            try:
                got = run(eo, Ao, c)
            except Exception as ex:
                got = {'exc': str(ex)[:100]}
            eo.log.clear()
            if got != want:
                killed = True
                break
        if not killed:
            survivors.append(site)
    print('%s mutation: %d conditional jumps of %s (%d bytes), %d flipped one at a time, %d killed, %d survived' %
          (f, len(sites), OURS_SYM[f], end - start, len(sites), len(sites) - len(survivors), len(survivors)))
    for s in survivors:
        near = [c for c in catches if c <= s < c + 0x20]
        print('   survivor: jump at %08x (+0x%x)%s' % (s, s - start, '  (inside a catch funclet: never executed)' if near else ''))


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    cov = '--cov' in sys.argv
    mut = [a for a in sys.argv if a.startswith('--mut=')]
    n = int(args[0]) if len(args) > 0 else 20
    seed = int(args[1]) if len(args) > 1 else 1
    which = args[2].split(',') if len(args) > 2 else list(FUNCS)
    lm = log_map()
    r = random.Random(seed)
    total = 0
    for f in which:
        es = make('stock', f)
        es.heap_mark = es.heap_ptr
        As = dict(FUNCS)
        cases = [rand_case_tw(r, f) for _ in range(n)]
        if cov:
            from unicorn import UC_HOOK_CODE
            from capstone import Cs, CS_ARCH_X86, CS_MODE_32
            lo, hi = EXTENT[f]
            hit = set()
            es.uc.hook_add(UC_HOOK_CODE, lambda uc, a, s, u: hit.add(a), None, lo, hi)
            for c in cases:
                es.heap_ptr = es.heap_mark
                try:
                    run(es, As, c)
                except Exception as ex:
                    print('raised', ex)
                es.log.clear()
            mem = es.pe.get_memory_mapped_image()
            base = es.base
            miss = []
            pc = lo
            allins = 0
            md = Cs(CS_ARCH_X86, CS_MODE_32)
            while pc < hi:
                got = False
                for i in md.disasm(bytes(mem[pc - base:hi - base + 16]), pc):
                    if i.address >= hi:
                        break
                    allins += 1
                    if i.address not in hit:
                        miss.append(i)
                    pc = i.address + i.size
                    got = True
                if not got:
                    pc += 1
            runs = []
            for i in miss:
                if runs and runs[-1][-1].address + runs[-1][-1].size == i.address:
                    runs[-1].append(i)
                else:
                    runs.append([i])
            print(f, 'instructions', allins, 'missed', len(miss), 'in', len(runs), 'runs')
            for run_ in runs:
                print('   %08x..%08x (%d)  %s %s ... %s %s' % (run_[0].address, run_[-1].address, len(run_), run_[0].mnemonic,
                                                             run_[0].op_str, run_[-1].mnemonic, run_[-1].op_str))
            continue
        Ao = {k: lm[v] for k, v in FUNCS.items() if v in lm}
        if mut:
            mutate(f, cases, As, Ao, es, int(mut[0].split('=')[1]))
            continue
        eo = make('ours', f)
        eo.heap_mark = eo.heap_ptr
        total += compare(f, es, eo, run, cases, As, Ao)
    if not cov:
        print('TOTAL problems', total)


if __name__ == '__main__':
    main()
