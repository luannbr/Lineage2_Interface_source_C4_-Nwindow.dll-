# Differential tests of NCChineseIME::OnImeComposition, OnImeNotify and ReadReadingString (stock vs ours under unicorn)
#   python t_ime.py [cases] [seed] [comp,notify,rrs]
import sys, os, random, struct
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from gw import *
import gw
import stubs
import sa
from unicorn.x86_const import *

OBJ_SIZE = 0x6274
FUNCS = {'comp': (0x1001b720, 2), 'notify': (0x1001cb90, 2), 'rrs': (0x10017c70, 0)}
# stock va: (name, pop, returns this); the object under test is a NCChineseIME
BASE_CALLEES = {
    0x10017200: ('GetImeId', 0, 0), 0x10017120: ('IsChineseIME', 0, 0), 0x1002b400: ('ImeIsUIMessage', 12, 0),
    0x1002b3d0: ('ImeGetContext', 0, 0), 0x1002b3e0: ('ImeReleaseContext', 4, 0), 0x100166f0: ('IsUnicodeOS', 0, 0),
    0x10006c10: ('SendToParent', 12, 0), 0x100169f0: ('FreeCandList', 0, 0), 0x100175b0: ('IsNT', 0, 0),
    0x10004b80: ('CompositeSetText', 4, 0), 0x100040d0: ('CompositeGetText', 4, 0),
}
OURS_SYMS = {0x10017200: '?GetImeId@NCChineseIME@@SAKXZ', 0x10017120: '?IsChineseIME@NCChineseIME@@QAEHXZ',
             0x1002b3d0: '?ImeGetContext@NCObject@@QAEPAUHIMC__@@XZ', 0x1002b3e0: '?ImeReleaseContext@NCObject@@QAEHPAUHIMC__@@@Z',
             0x100166f0: '?IsUnicodeOS@NCIME@@QAEHXZ', 0x10006c10: '?SendToParent@NControl@@QAEHIIJ@Z',
             0x100169f0: '?FreeCandList@NCChineseIME@@QAEXXZ', 0x100175b0: '?IsNT@NCChineseIME@@SA_NXZ',
             0x10004b80: '?SetText@NCCompositeBox@@QAEXPBG@Z'}
stubs.IMPORT_POP.update({'ImmGetCompositionStringW': 16, 'MultiByteToWideChar': 24, 'ImmGetConversionStatus': 12,
                         'ImmSetConversionStatus': 12, 'GetKeyboardState': 4, 'ImmReleaseContext': 8,
                         '?appMemset@@YAXPAXHH@Z': 0, '?appMemcpy@@YAXPAXPBXH@Z': 0, '?appFree@@YAXPAX@Z': 0,
                         '?Delete@FString@@QAEXHH@Z': 8, '??HFString@@QAE?AV0@ABV0@@Z': 8, '??HFString@@QAE?AV0@PBG@Z': 8,
                         '??1FString@@QAE@XZ': 0, '?Len@FString@@QBEHXZ': 0,
                         # standalone: NCObject::ImeIsUIMessage / ImeGetContext (one Imm call each) are inlined into the IME
                         # functions, which call the imports directly where the hook build calls the stock functions (stubbed below by address)
                         'ImmIsUIMessageW': 16, 'ImmGetContext': 4})
gw.VPOP.update({41: 0, 48: 0, 49: 0})


class IW(GW):
    def __init__(self, kind, test):
        callees = dict(BASE_CALLEES)
        syms = dict(OURS_SYMS)
        if test == 'getid':
            del callees[0x10017200]
            del syms[0x10017200]
        if test == 'notify':
            callees[0x1001b620] = ('ReadCandList', 0, 0)
            syms[0x1001b620] = '?ReadCandList@NCChineseIME@@QAEXXZ'
            callees[0x10017c70] = ('ReadReadingString', 0, 0)
            syms[0x10017c70] = '?ReadReadingString@NCChineseIME@@QAEXXZ'
            callees[0x10016530] = ('NCIMEMode', 0, 0)
        GW.__init__(self, kind, callees, syms)
        self.case = None
        self.rets['GetImeId'] = lambda e, t, a: e.case['imeid']
        self.rets['IsChineseIME'] = lambda e, t, a: e.case['chinese']
        self.rets['IsUnicodeOS'] = lambda e, t, a: e.case['unicode']
        self.rets['IsNT'] = lambda e, t, a: e.case['nt']
        self.rets['ImeGetContext'] = lambda e, t, a: 0x1234
        self.rets['NCIMEMode'] = lambda e, t, a: e.case.get('mode_ok', 0)
        self.rets['CompositeSetText'] = self.set_text
        self.rets['CompositeGetText'] = self.get_text
        self.vrets[41] = lambda e, t, a: e.case.get('candshown', 0) if e.objs.get(t) == 'candbox' else 0
        # the reading string and the IMM context locks of the stock image's globals
        base = 0 if kind == 'stock' else NWBASE - 0x10000000
        self.gbase = base
        self.rs_stub = self.mkstub('GetReadingString', 24)
        self.lock_ic = self.mkstub('LockIMC', 4)
        self.unlock_ic = self.mkstub('UnlockIMC', 4)
        self.lock_icc = self.mkstub('LockIMCC', 4)
        self.unlock_icc = self.mkstub('UnlockIMCC', 4)

    def mkstub(self, name, pop):
        n = getattr(self, 'nmk', 0)
        self.nmk = n + 1
        stub = STUB_BASE + 0xD000 + 16 * n
        self.uc.mem_write(stub, bytes([0xC3]))
        self.stubs[stub] = ('x:' + name, None)
        self.xpop = getattr(self, 'xpop', {})
        self.xpop[name] = pop
        return stub

    def set_text(self, e, t, a):
        s = e.wstr(a[0]) if a[0] else ''
        e.log.append(('CompositeText', s))
        e.fs_set(t + 0xEC, s)
        return 0

    def get_text(self, e, t, a):
        s = e.fs_get(t + 0xEC)
        e.fs_set(a[0], s)
        e.log.append(('GetText', s))
        return a[0]

    def do_import(self, nm):
        c = self.case
        a = self.arg
        if nm == 'ImmGetCompositionStringW':
            hIMC, idx, buf, blen = a(0), a(1), a(2), a(3)
            data = c['comp'].get(idx)
            if data is None:
                data = b''
            self.log.append(('ImmGetCompositionStringW', hIMC, hex(idx), blen))
            if blen == 0 or not buf:
                return len(data)
            n = min(len(data), blen)
            self.uc.mem_write(buf, data[:n])
            return n
        if nm == 'MultiByteToWideChar':
            cp, fl, src, cb, dst, cch = a(0), a(1), a(2), a(3), a(4), a(5)
            raw = bytearray()
            p = src
            while True:
                b = bytes(self.uc.mem_read(p, 1))[0]
                if b == 0:
                    break
                raw.append(b)
                p += 1
            txt = bytes(raw).decode('latin-1') + '\0'
            self.log.append(('MultiByteToWideChar', cp, fl, cb, cch))
            if cch == 0:
                return len(txt)
            n = min(len(txt), cch)
            self.uc.mem_write(dst, txt[:n].encode('utf-16-le'))
            return n
        if nm == 'ImmGetConversionStatus':
            self.write32(a(1), c.get('conv', 0))
            self.write32(a(2), c.get('sent', 0))
            self.log.append(('ImmGetConversionStatus', a(0)))
            return 1
        if nm == 'ImmSetConversionStatus':
            self.log.append(('ImmSetConversionStatus', a(0), a(1), a(2)))
            return 1
        if nm == 'GetKeyboardState':
            self.uc.mem_write(a(0), bytes(c['keys']))
            self.log.append(('GetKeyboardState',))
            return 1
        if nm == 'ImmReleaseContext':
            self.log.append(('ImmReleaseContext', a(1)))
            return 1
        if nm == 'ImmIsUIMessageW':          # NCObject::ImeIsUIMessage(Msg, wParam, lParam) inlined (hWnd = GViewportHWnd, not compared: the stock stub does not see it)
            self.log.append(('ImeIsUIMessage', hex(a(1)), a(2), a(3)))
            return 0
        if nm == 'ImmGetContext':            # NCObject::ImeGetContext() inlined (the stock stub answers 0x1234)
            return 0x1234
        if nm == '?appMemset@@YAXPAXHH@Z':
            n = a(2)
            self.uc.mem_write(a(0), bytes([a(1) & 0xFF]) * n)
            return None
        if nm == '?Len@FString@@QBEHXZ':
            s = self.fs_get(self.this())
            return len(s)
        if nm == '?appMemcpy@@YAXPAXPBXH@Z':
            n = a(2)
            self.uc.mem_write(a(0), bytes(self.uc.mem_read(a(1), n)))
            return None
        if nm == '?appFree@@YAXPAX@Z':
            self.log.append(('GMalloc.Free', self.val(a(0))))
            return None
        return GW.do_import(self, nm)

    def dispatch(self, name):
        kind, nm = name.split(':', 1)
        if kind == 'x':
            self.last_pop = self.xpop[nm]
            a = self.sargs(self.last_pop // 4)
            c = self.case
            if nm == 'GetReadingString':
                hIMC, mx, buf, perr, pvert, pmax = a
                data = c['rs']
                self.log.append(('GetReadingString', hIMC, mx))
                if buf:
                    n = min(len(data), mx)
                    self.uc.mem_write(buf, data[:n])
                self.write32(perr, c['rs_err'])
                return c['rs_len']
            if nm == 'LockIMC':
                self.log.append(('LockIMC', a[0]))
                return self.ic
            if nm == 'UnlockIMC':
                self.log.append(('UnlockIMC', a[0]))
                return 1
            if nm == 'LockIMCC':
                self.log.append(('LockIMCC', a[0]))
                return self.icc
            if nm == 'UnlockIMCC':
                self.log.append(('UnlockIMCC', a[0]))
                return 1
        if kind == 'cb' and nm in ('IsNT', 'GetImeId'):
            self.last_pop = 0
            self.log.append((nm,))
            return self.case['nt'] if nm == 'IsNT' else self.case['imeid']
        if kind == 'cb' and nm == 'SendToParent':
            self.last_pop = 12
            a = self.sargs(3)
            self.log.append(('SendToParent', hex(a[0]), a[1], a[2]))
            return 1
        if kind == 'cb' and nm == 'ImeIsUIMessage':
            self.last_pop = 12
            a = self.sargs(3)
            self.log.append(('ImeIsUIMessage', hex(a[0]), a[1], a[2]))
            return 0
        if kind == 'cb' and nm == 'ImeReleaseContext':
            self.last_pop = 4
            a = self.sargs(1)
            self.log.append(('ImeReleaseContext', a[0]))
            return 1
        return GW.dispatch(self, name)


def rand_wide(r, n):
    return bytes(sum(([r.randint(0x41, 0x7A), 0] if r.random() < 0.8 else [r.randint(1, 255), r.randint(1, 0x4F)] for _ in range(n)), []))


def rand_case(r, test):
    c = dict(seed=r.randint(0, 10 ** 6), test=test)
    c['lang'] = r.choice([0, 1, 2, 3, 4, 5, 2, 3, 4])
    c['imeid'] = r.choice([0, 0x40001, 0x40002, 0x50003, 0x40001, 0x50003, 0x12345])
    c['chinese'] = r.randint(0, 1) if r.random() < 0.3 else 1
    c['unicode'] = r.randint(0, 1)
    c['nt'] = r.randint(0, 1)
    c['ime_type'] = r.choice([0, 1, 8, 9, 10, 11, 12, 12, 11])
    c['active'] = 1 if r.random() < 0.9 else 0
    c['candshown'] = r.randint(0, 1)
    c['mode'] = r.choice([0, 1, 2, 6])
    c['mode_ok'] = r.randint(0, 1)
    c['conv'] = r.choice([0, 1, 9])
    c['bcand'] = r.randint(0, 1)
    c['bopen'] = r.randint(0, 1)
    comp = {}
    ansi = c['lang'] in (2, 3, 4) and not c['unicode']
    for idx in (0x800, 0x1000, 0x8, 0x10, 0x20, 0x4, 0x2, 0x100, 0x400, 0x200):
        if idx in (0x10, 0x4, 0x2, 0x1000, 0x20, 0x200, 0x400):    # attrs / clauses
            n = r.randint(0, 12)
            comp[idx] = bytes(r.choice([0, 1, 1, 2, 0x10]) for _ in range(n)) if idx in (0x10, 0x100) else bytes(sum(([r.randint(0, 9), 0, 0, 0] for _ in range(n // 2)), []))
        else:
            n = r.randint(0, 20)
            comp[idx] = (bytes(r.randint(0x41, 0x7A) for _ in range(n)) if ansi else rand_wide(r, n))
    # GCS_COMPATTR 0x10, GCS_COMPCLAUSE 0x20, GCS_RESULTCLAUSE 0x1000, GCS_RESULTSTR 0x800, GCS_COMPSTR 8, GCS_CURSORPOS 0x80,
    # GCS_COMPREADSTR 1 .. (the real constants)
    comp = {}
    for idx, kind in ((0x0001, 'str'), (0x0002, 'attr'), (0x0004, 'clause'), (0x0008, 'str'), (0x0010, 'attr'), (0x0020, 'clause'),
                      (0x0100, 'str'), (0x0200, 'clause'), (0x0800, 'str'), (0x1000, 'clause'), (0x0080, 'none')):
        n = r.randint(0, 20)
        if kind == 'str':
            comp[idx] = bytes(r.randint(0x41, 0x7A) for _ in range(n)) if ansi else rand_wide(r, n)
        elif kind == 'attr':
            comp[idx] = bytes(r.choice([0, 0, 1, 1, 2]) for _ in range(n))
        elif kind == 'clause':
            m = r.randint(0, 6)
            vals = sorted(r.randint(0, 20) for _ in range(m))
            comp[idx] = b''.join(struct.pack('<I', v) for v in vals)
        else:
            comp[idx] = b'\0' * r.randint(0, 3) * 4
    c['comp'] = comp
    flags = 0
    for f in (0x1, 0x2, 0x4, 0x8, 0x10, 0x20, 0x80, 0x100, 0x200, 0x400, 0x800, 0x1000):
        if r.random() < 0.6:
            flags |= f
    c['lparam'] = flags
    # notify
    c['wparam'] = r.choice([1, 2, 3, 4, 5, 6, 7, 8, 9, 14, 14, 14, 3, 4, 5, 6])
    if test == 'notify':
        c['lparam'] = r.choice([-1, 0, 1, 2, 16, 17, 23, 26, 27, 28, 5])
    c['keys'] = [0] * 256
    for k in range(256):
        if r.random() < 0.04:
            c['keys'][k] = 0x80
    if r.random() < 0.3:
        c['keys'][0x08] = 0x80
    if r.random() < 0.3:
        c['keys'][0x20] = 0x80
    c['ncand'] = r.randint(0, 14)
    c['pagestart'] = r.randint(0, 3)
    c['hascand'] = r.randint(0, 1)
    # the reading string
    if test == 'notify' and r.random() < 0.7:
        c['ime_type'] = r.choice([11, 12, 12, 12, 8, 9])
        c['wparam'] = r.choice([5, 5, 14, 14, 14, 3, 4, 6])
        c['hascand'] = 1 if r.random() < 0.85 else 0
        c['ncand'] = r.randint(9, 14)
        c['mode_ok'] = 0 if r.random() < 0.85 else 1
        c['active'] = 1
        c['chinese'] = 1
        for k in range(0x30, 0x3A):
            if r.random() < 0.25:
                c['keys'][k] = 0x80
        for k in range(0x41, 0x5B):
            if r.random() < 0.08:
                c['keys'][k] = 0x80
    c['rs'] = rand_wide(r, r.randint(0, 14))
    c['rs_len'] = r.choice([0, len(c['rs']) // 2, len(c['rs']) // 2, 3])
    c['rs_err'] = r.randint(0, 5)
    c['has_rs'] = r.randint(0, 1)
    c['private'] = rand_wide(r, 80) + bytes(80)
    c['ptr_zero'] = [r.random() < 0.15 for _ in range(4)]
    c['len41'] = r.randint(0, 6)
    c['err41'] = r.randint(0, 8)
    return c


def world(e, c):
    rnd = random.Random(c['seed'])
    e.case = c
    e.objs = {}
    e.shown = {}
    e.set_lang(c['lang'])
    e.ime_type = c['ime_type']
    obj = e.alloc(OBJ_SIZE + 16)
    e.uc.mem_write(obj, bytes(OBJ_SIZE + 16))
    e.objs[obj] = 'ime'
    e.write32(obj + 0x1FC, c['active'])
    e.write32(obj + 0x1F8, c['mode'])
    e.write32(obj + 0x21C, 949)
    e.write32(obj + 0x626C, c['bcand'])
    e.write32(obj + 0x6270, c['bopen'])
    # random old contents of the composition buffers
    for off in (0x220, 0xA20, 0x1220, 0x2220):
        e.uc.mem_write(obj + off, rand_wide(rnd, 40) + bytes(2))
    cbox = e.window('candbox', size=0x300, rnd=rnd)
    comp = e.window('compbox', size=0x300, rnd=rnd)
    e.write32(obj + 0x4258, cbox)
    e.write32(obj + 0x425C, comp)
    e.fs_set(comp + 0xEC, rnd.choice(['', 'ab', 'xyz']))
    e.shown[cbox] = c['candshown']
    # the candidate list
    if c['hascand']:
        n = c['ncand']
        size = 0x20 + 4 * n + 40 * 2
        cl = e.alloc(size + 16)
        e.uc.mem_write(cl, bytes(size + 16))
        e.write32(cl, size); e.write32(cl + 8, n); e.write32(cl + 0xC, 0); e.write32(cl + 0x10, c['pagestart'])
        s = rand_wide(rnd, 6) + bytes(2)
        for k in range(n + 2):
            e.write32(cl + 0x18 + 4 * k, 0x1C + 4 * n + len(s) * (k % 4))
        e.uc.mem_write(cl + 0x1C + 4 * n, s * 4)
        e.write32(obj + 0x4254, cl)
        e.objs[cl] = 'cand'
    # the Clauses array: empty with room
    e.write32(obj + 0x4248, 0); e.write32(obj + 0x424C, 0); e.write32(obj + 0x4250, 0)
    e.write32(obj + 0x4260, 0); e.write32(obj + 0x4264, 0); e.write32(obj + 0x4268, 0)
    # the reading string machinery (stock globals)
    g = e.gbase
    e.write32(0x102BF04C + g if e.kind == 'stock' else NWBASE + 0x2BF04C, e.rs_stub if c['has_rs'] else 0)
    for off, stub in ((0x50, e.lock_ic), (0x54, e.unlock_ic), (0x58, e.lock_icc), (0x5C, e.unlock_icc)):
        e.write32((0x102BF000 + off) if e.kind == 'stock' else NWBASE + 0x2BF000 + off, stub)
    # fake input context / private data per IME version
    e.ic = e.alloc(0x200)
    e.uc.mem_write(e.ic, bytes(0x200))
    e.icc = e.alloc(0x100)
    e.uc.mem_write(e.icc, bytes(0x100))
    e.write32(e.ic + 0x124, 0x5555)
    priv = e.alloc(0x400)
    e.uc.mem_write(priv, bytes(0x400))
    e.uc.mem_write(priv + 0x38, c['private'])
    e.write32(priv + 0x9C, c['len41'])
    e.write32(priv + 0xA0, c['err41'])
    imeid = c['imeid']
    # 1.5: [icc + 0x20] -> p ; 2: the same with lengths at p+0x40+16*cs ; 3: [icc + 4] -> q ; [q + 0x18] -> p
    e.write32(e.icc + 0x20, 0 if c['ptr_zero'][0] else priv)
    q = e.alloc(0x100)
    e.uc.mem_write(q, bytes(0x100))
    e.write32(q + 0x18, 0 if c['ptr_zero'][2] else priv)
    e.write32(e.icc + 4, 0 if c['ptr_zero'][1] else q)
    cs = 2 if c['nt'] else 1
    e.write32(priv + 0x40 + 16 * cs, c['len41'])
    e.write32(priv + 0x44 + 16 * cs, c['err41'])
    e.write32(priv + 0x60, c['len41'])
    e.write32(priv + 0x64, c['err41'])
    return obj


def snapshot_obj(e, obj):
    words = struct.unpack('<%dI' % (OBJ_SIZE // 4), bytes(e.uc.mem_read(obj, OBJ_SIZE)))
    out = []
    for i, w in enumerate(words):
        off = 4 * i
        if off == 0x4248 or off == 0x4254 or off == 0x4260:
            out.append('ptr' if w else 0)
        else:
            out.append(e.objs.get(w, w))
    return out


def run(e, A, c):
    mark = len(e.log)
    obj = world(e, c)
    va, nargs = A[c['test']]
    if c['test'] == 'comp':
        e.call(va, this=obj, args=[0, c['lparam']])
    elif c['test'] == 'notify':
        r = e.call(va, this=obj, args=[c['wparam'], c['lparam'] & 0xFFFFFFFF])
    else:
        e.call(va, this=obj, args=[])
    ret = e.reg(UC_X86_REG_EAX)
    clauses = []
    n = e.read32(obj + 0x424C)
    d = e.read32(obj + 0x4248)
    for k in range(min(n, 8)):
        item = bytes(e.uc.mem_read(d + k * 0x804, 0x804))
        clauses.append(item)
    log = [x for x in e.log[mark:] if x[0] not in ('CompositeGetText', 'GetText')]
    if c['test'] == 'rrs':
        ret = 0
    return dict(log=log, ret=ret, obj=snapshot_obj(e, obj), clauses=clauses,
                lastcomp=e.fs_get(obj + 0x4260), comptext=e.fs_get(e.read32(obj + 0x425C) + 0xEC))


if __name__ == '__main__':
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 20
    seed = int(sys.argv[2]) if len(sys.argv) > 2 else 1
    which = sys.argv[3].split(',') if len(sys.argv) > 3 else list(FUNCS)
    lm = log_map()
    r = random.Random(seed)
    total = 0
    for f in which:
        es = IW('stock', f); eo = IW('ours', f)
        es.heap_mark, eo.heap_mark = es.heap_ptr, eo.heap_ptr
        As = {k: (v[0], v[1]) for k, v in FUNCS.items()}
        Ao = {k: (lm[v[0]], v[1]) for k, v in FUNCS.items()}
        cases = [rand_case(r, f) for _ in range(n)]
        total += compare(f, es, eo, run, cases, As, Ao)
    print('TOTAL problems', total)
