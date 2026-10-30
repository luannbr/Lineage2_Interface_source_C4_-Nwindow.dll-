# More differential tests of NCChineseIME: GetImeId, ReadCandList, Deactivate, OnDestroy (stock vs ours under unicorn)
#   python t_ime2.py [cases] [seed] [getid,readcand,deact,destroy]
import sys, os, random, struct
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import t_ime
from t_ime import *
import gw, stubs

FUNCS2 = {'getid': (0x10017200, 0), 'readcand': (0x1001b620, 0), 'deact': (0x1001c750, 0), 'destroy': (0x1001b510, 0)}
stubs.IMPORT_POP.update({'GetKeyboardLayout': 4, 'ImmGetIMEFileNameA': 12, 'lstrcmpiA': 8, 'GetFileVersionInfoSizeA': 8,
                         'GetFileVersionInfoA': 16, 'VerQueryValueA': 16, 'ImmGetCandidateListW': 16, 'ImmNotifyIME': 16})
gw.VPOP.update({3: 4, 41: 0, 49: 0})


class IW2(IW):
    def __init__(self, kind, test):
        IW.__init__(self, kind, test)
        # extra callees (the stock addresses are patched by hand here; GW has installed the base set)
        self.extra = {}
        extra = {0x10016640: ('NCIMEDeactivate', 0, 0), 0x10035880: ('RemoveChild', 4, 0),
                 0x10017ed0: ('FillCandidateBox', 0, 0), 0x10016880: ('StockDelete', 0, 0)}
        ours = {0x10016640: '?Deactivate@NCIME@@UAEXXZ', 0x10035880: '?RemoveChildWindow@NCWnd@@QAEXPAUNWndEntry@@@Z',
                0x10017ed0: '?FillCandidateBox@NCChineseIME@@QAEXXZ', 0x10016880: None}
        n = 40
        for va, (name, pop, rthis) in extra.items():
            stub = STUB_BASE + 0xC000 + 16 * n
            n += 1
            self.uc.mem_write(stub, bytes([0xC3]))
            self.stubs[stub] = ('cb:' + name, None)
            self.cb[name] = (va, pop, rthis)
            jump = b'\x68' + struct.pack('<I', stub) + b'\xC3'
            if self.kind == 'stock':
                self.uc.mem_write(va, jump)
            else:
                self.uc.mem_write(NWBASE + (va - 0x10000000), jump)
                for a in gw.chase(self, va):
                    self.uc.mem_write(a, jump)
                if ours[va]:
                    self.uc.mem_write(self.symbols[ours[va]], jump)
        self.vrets[3] = lambda e, t, a: (e.log.append(('delete', e.objs.get(t, t), a)), 0)[1]

    def dispatch(self, name):
        kind, nm = name.split(':', 1)
        if kind == 'cb' and nm == 'StockDelete':
            self.last_pop = 0
            self.log.append(('GMalloc.Free', self.val(self.sargs(1)[0])))
            return None
        return IW.dispatch(self, name)

    def do_import(self, nm):
        c = self.case
        a = self.arg
        if nm == 'GetKeyboardLayout':
            self.log.append(('GetKeyboardLayout', a(0)))
            return c['layout']
        if nm == 'ImmGetIMEFileNameA':
            self.log.append(('ImmGetIMEFileNameA', a(0), a(2)))
            name = c['imefile']
            if name is None:
                return 0
            self.uc.mem_write(a(1), name.encode() + b'\0')
            return len(name)
        if nm == 'lstrcmpiA':
            def cs(p):
                out = bytearray()
                while True:
                    b = bytes(self.uc.mem_read(p, 1))[0]
                    if not b:
                        return bytes(out).decode('latin-1').lower()
                    out.append(b)
                    p += 1
            x, y = cs(a(0)), cs(a(1))
            self.log.append(('lstrcmpiA', x, y))
            return 0 if x == y else (1 if x > y else 0xFFFFFFFF)
        if nm == 'GetFileVersionInfoSizeA':
            self.log.append(('GetFileVersionInfoSizeA', self.wstr_a(a(0))))
            self.write32(a(1), 0x1234)
            return c['vsize']
        if nm == 'GetFileVersionInfoA':
            self.log.append(('GetFileVersionInfoA', a(1), a(2), self.val(a(3))))
            return c['vinfo_ok']
        if nm == 'VerQueryValueA':
            blk = self.alloc(64)
            self.uc.mem_write(blk, bytes(64))
            self.write32(blk + 8, c['verms'])
            self.write32(a(2), blk)
            self.write32(a(3), 52)
            self.log.append(('VerQueryValueA', self.val(a(0)), self.wstr_a(a(1))))
            return c['vq_ok']
        if nm == 'ImmGetCandidateListW':
            hIMC, idx, buf, blen = a(0), a(1), a(2), a(3)
            self.log.append(('ImmGetCandidateListW', hIMC, idx, blen))
            data = c['candlist']
            if blen == 0 or not buf:
                return len(data)
            n = min(blen, len(data))
            self.uc.mem_write(buf, data[:n])
            return n
        if nm == 'ImmNotifyIME':
            self.log.append(('ImmNotifyIME', a(0), a(1), a(2), a(3)))
            return 1
        return IW.do_import(self, nm)

    def wstr_a(self, p):
        out = bytearray()
        while True:
            b = bytes(self.uc.mem_read(p, 1))[0]
            if not b:
                return bytes(out).decode('latin-1')
            out.append(b)
            p += 1


def rand_case2(r, test):
    c = rand_case(r, 'comp')
    c['test'] = test
    c['layout'] = r.choice([0xE00E0804, 0xE00E0804, 0xE00E0804, 0x04090409, 0xE0010804])
    c['imefile'] = r.choice(['PINTLGNT.IME', 'MSSCIPYA.IME', 'pintlgnt.ime', 'OTHER.IME', None])
    c['vsize'] = r.choice([0, 100, 200])
    c['vinfo_ok'] = r.randint(0, 1) if r.random() < 0.3 else 1
    c['vq_ok'] = r.randint(0, 1) if r.random() < 0.3 else 1
    c['verms'] = r.choice([0x40001, 0x40002, 0x50003, 0x40001 | 0x10000, 0x50003 | 0x7700, 0x20002, 0xFFFF00FF])
    c['has_rs'] = r.randint(0, 1)
    # a candidate list: dwSize, dwStyle, dwCount, dwSelection, dwPageStart, dwPageSize, offsets
    cnt = r.randint(0, 25)
    page = r.choice([0, 5, 9, 10, 9, 10, 7])
    sel = r.randint(0, cnt + 2)
    hdr = struct.pack('<6I', 0, 0, cnt, sel, r.randint(0, 3), page) + b''.join(struct.pack('<I', 0x30 + 4 * cnt + 2 * k) for k in range(cnt))
    body = b''.join(rand_wide(r, 2) + bytes(2) for _ in range(cnt)) + bytes(16)
    c['candlist'] = (hdr + body) if r.random() < 0.9 else b''
    c['ime_type'] = r.choice([8, 9, 10, 10, 11, 12, 9])
    return c


def run2(e, A, c):
    mark = len(e.log)
    obj = world(e, c)
    va = A[c['test']][0]
    ret = None
    if c['test'] == 'getid':
        e.call(va, this=obj, args=[])
        ret = e.reg(UC_X86_REG_EAX)
    else:
        e.call(va, this=obj, args=[])
    # the candidate list pointer is normalised (a heap address)
    cl = e.read32(obj + 0x4254)
    snap = snapshot_obj(e, obj)
    cands = bytes(e.uc.mem_read(cl, 0x80)) if cl else None
    return dict(log=e.log[mark:], ret=ret, obj=snap, cand=cands)


if __name__ == '__main__':
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 20
    seed = int(sys.argv[2]) if len(sys.argv) > 2 else 1
    which = sys.argv[3].split(',') if len(sys.argv) > 3 else list(FUNCS2)
    lm = log_map()
    r = random.Random(seed)
    total = 0
    for f in which:
        es = IW2('stock', f); eo = IW2('ours', f)
        es.heap_mark, eo.heap_mark = es.heap_ptr, eo.heap_ptr
        As = {k: (v[0], v[1]) for k, v in FUNCS2.items()}
        Ao = {k: (lm[v[0]], v[1]) for k, v in FUNCS2.items()}
        cases = [rand_case2(r, f) for _ in range(n)]
        total += compare(f, es, eo, run2, cases, As, Ao)
    print('TOTAL problems', total)
