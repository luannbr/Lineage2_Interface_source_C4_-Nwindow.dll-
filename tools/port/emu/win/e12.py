# Differential test of NCChineseIME::OnCreate (stock 0x1001b390) (stock vs ours under unicorn): the window classes, constructors and every call it
# makes are logged; the fake windows' vtables log their virtual calls; the console's and the windows' memory is compared
# (NCConsole::OnCreate itself is e7.py)
#   python e12.py [cases] [seed]
import sys, os, random, struct, re
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import e4
from e4 import *
from capstone import Cs, CS_ARCH_X86, CS_MODE_32

FUNC = 0x1001b390
FUNC_END = FUNC + 400
POPS = {0x10035840: 8, 0x100178e0: 0, 0x10004f50: 8, 0x10005160: 20, 0x100170c0: 0, 0x10016b80: 0, 0x10016750: 0}
CTORS = {0x10004f50, 0x10005160}
OURS_CTOR = {0x10004f50: '??0NCCandidateBox@@QAE@HH@Z', 0x100170c0: '?LoadReadingString@NCChineseIME@@SAXXZ',
             0x10016b80: '?LoadImmFunctions@NCIME@@SAXXZ', 0x10016750: '?UpdateCodePage@NCIME@@QAEXXZ', 0x100178e0: '?OnCreate@NCIME@@UAEHXZ'}
NAMES = {0x100014a0: 'SetTextID', 0x10035840: 'AddChildWindow', 0x1002b450: 'FindTexture', 0x1002b420: 'SetTimer', 0x10035650: 'GetLocalRect',
         0x1002ded0: 'AddTab', 0x100015b0: 'SetToolTip', 0x10055870: 'LoadFilters', 0x10035f20: 'NCWnd::OnCreate'}
imp_pop = {'?GetTransientPackage@UObject@@SAPAVUPackage@@XZ': 0,
           '?StaticAllocateObject@UObject@@SAPAV1@PAVUClass@@PAV1@VFName@@K1PAVFOutputDevice@@11@Z': 0}
stubs.IMPORT_POP.update(imp_pop)


def chase(e, va):
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
            a = (a + 9 + struct.unpack('<i', bytes(e.uc.mem_read(a + 5, 4)))[0]) & 0xFFFFFFFF
        else:
            break
        n += 1
    return out


class E7(E4):
    def __init__(self, kind):
        E4.__init__(self, kind)
        self.cb = {}
        self.gvars = {}
        n = 0
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
                self.uc.mem_write(NWBASE + (va - 0x10000000), jump)
                for a in chase(self, va):
                    self.uc.mem_write(a, jump)
                if va in OURS_CTOR:
                    self.uc.mem_write(self.symbols[OURS_CTOR[va]], jump)
        # the fake windows' vtable: every slot logs itself
        self.vt = self.alloc(0x400)
        self.uc.mem_write(self.vt, bytes(0x400))
        self.vslot_pop = {25: 12, 40: 28, 49: 0, 56: 4, 48: 0, 29: 0}
        for slot in range(0, 256):
            stub = STUB_BASE + 0xE000 + 16 * slot
            self.uc.mem_write(stub, bytes([0xC3]))
            self.stubs[stub] = ('vc:%d' % slot, None)
            self.write32(self.vt + 4 * slot, stub)
        self.objs = {}
        self.allocated = []

    def dispatch(self, name):
        kind, nm = name.split(':', 1)
        if kind == 'cb':
            self.last_pop = self.cb[nm][1]
            return self.do_cb(nm)
        if kind == 'vc':
            slot = int(nm)
            if slot not in self.vslot_pop:
                raise RuntimeError('virtual call of slot %d' % slot)
            self.last_pop = self.vslot_pop[slot]
            t = self.this()
            self.log.append(('vcall', slot, self.nm(t), self.sargs(self.last_pop // 4)))
            return 0
        return E4.dispatch(self, name)

    def nm(self, v):
        if v in self.objs:
            return self.objs[v]
        return v

    def val(self, v):
        if v in self.objs:
            return self.objs[v]
        if 0x60000000 <= v < 0x68000000 or 0x2F000000 <= v < 0x2F100000 or 0x10000000 <= v < 0x10400000:
            try:
                s = self.wstr(v, 80)
                if s and all(32 <= ord(c) < 0xFFFF for c in s):
                    return 'S:' + s
            except Exception:
                pass
        return v

    def do_cb(self, nm):
        va, pop = self.cb[nm]
        t = self.this()
        a = self.sargs(pop // 4)
        if va == 0x10035840:      # AddChildWindow(child, flags) -> an entry without a parent
            ent = self.alloc(0x40)
            self.uc.mem_write(ent, bytes(0x40))
            self.objs[ent] = 'entry%d' % len(self.objs)
            self.log.append(('AddChildWindow', self.nm(t), self.nm(a[0]), a[1]))
            return ent
        if va == 0x10035650:      # GetLocalRect(rect) (never used for the sizes here)
            self.write32(a[0], 0); self.write32(a[0] + 4, 0)
            self.write32(a[0] + 8, f32(self.case['w'])); self.write32(a[0] + 12, f32(self.case['h']))
            self.log.append(('GetLocalRect',))
            return None
        if va == 0x1002b450:      # FindTexture(name, 1)
            self.log.append(('FindTexture', self.nm(t), self.val(a[0]), a[1]))
            return 0xDEAD0000 + len(self.log)
        if va == 0x1002b420:      # SetTimer
            self.log.append(('SetTimer', self.nm(t), a))
            return 0x77
        self.log.append((hex(va) if va not in NAMES else NAMES[va], self.nm(t), [self.nm(x) if x in self.objs else self.val(x) for x in a]))
        return t if va in CTORS else 0

    def do_import(self, nm):
        t = self.this()
        if nm == '?GetTransientPackage@UObject@@SAPAVUPackage@@XZ':
            return 0x1234
        if nm == '?StaticAllocateObject@UObject@@SAPAV1@PAVUClass@@PAV1@VFName@@K1PAVFOutputDevice@@11@Z':
            cls = self.sargs(1)[0]
            cls = self.stockptr(cls)
            o = self.alloc(0x400)
            self.uc.mem_write(o, bytes(0x400))
            self.write32(o, self.vt)
            self.objs[o] = 'obj%d' % len(self.allocated)
            self.allocated.append(o)
            self.log.append(('alloc', hex(cls)))
            return o
        return E4.do_import(self, nm)


def world(e, c):
    e.case = c
    e.objs = {}
    e.allocated = []
    con = e.alloc(0x7000)
    e.uc.mem_write(con, bytes(0x7000))
    e.objs[con] = 'chatwnd'
    e.write32(con + 0x4C, f32(c['w'])); e.write32(con + 0x50, f32(c['h']))
    return con


def run7(e, A, c):
    mark = len(e.log)
    con = world(e, c)
    r = e.call(A['create'], this=con, args=[])
    mem = bytes(e.uc.mem_read(con, 0x6300))
    objs = [bytes(e.uc.mem_read(o, 0xB0)) for o in e.allocated]
    # window pointers in the console's memory are named so that the two heaps compare equal
    names = {v: k for k, v in e.objs.items()}
    fix = []
    for off in range(0, 0x6300, 4):
        v = struct.unpack('<I', mem[off:off + 4])[0]
        if v in e.objs:
            fix.append((off, e.objs[v]))
    ofix = []
    for k, o in enumerate(e.allocated):
        b = objs[k]
        row = []
        for off in range(4, 0xB0, 4):
            v = struct.unpack('<I', b[off:off + 4])[0]
            row.append(e.objs[v] if v in e.objs else v)
        ofix.append(row)
    cmem = []
    for off in range(0, 0x6300, 4):
        v = struct.unpack('<I', mem[off:off + 4])[0]
        cmem.append(e.objs[v] if v in e.objs else v)
    return dict(ret=r, log=e.log[mark:], console=cmem[1:], objs=ofix)


def rand7(r):
    return dict(w=r.choice([348, 348, 400]), h=r.choice([187, 250, 400, 502]))


def main(n, seed):
    r = random.Random(seed)
    cases = [rand7(r) for _ in range(n)]
    es = E7('stock'); eo = E7('ours')
    lm = log_map()
    As = {'create': FUNC}
    Ao = {'create': lm[FUNC]}
    es.heap_mark, eo.heap_mark = es.heap_ptr, eo.heap_ptr
    bad = err = 0
    for i, c in enumerate(cases):
        es.heap_ptr, eo.heap_ptr = es.heap_mark, eo.heap_mark
        try:
            a = run7(es, As, c)
        except Exception as ex:
            a = {'exc': str(ex)[:300]}
        try:
            b = run7(eo, Ao, c)
        except Exception as ex:
            b = {'exc': str(ex)[:300]}
        if 'exc' in a or 'exc' in b:
            err += 1
            if err <= 4:
                print('--- case %d raised' % i, a.get('exc'), '|', b.get('exc'))
        elif a != b:
            bad += 1
            if bad <= 3:
                print('--- case', i, ascii(c))
                for key in a:
                    if a[key] != b[key]:
                        print(' differs:', key)
                        if isinstance(a[key], list) and len(a[key]) == len(b[key]):
                            k = 0
                            for x, y in zip(a[key], b[key]):
                                if x != y:
                                    print('   #%d stock %s ours %s' % (k, ascii(x)[:300], ascii(y)[:300]))
                                    break
                                k += 1
                        else:
                            print('  stock:', ascii(a[key])[:600])
                            print('  ours :', ascii(b[key])[:600])
        es.log.clear(); eo.log.clear()
    print('cases', n, 'differ', bad, 'raised', err)


if __name__ == '__main__':
    main(int(sys.argv[1]) if len(sys.argv) > 1 else 8, int(sys.argv[2]) if len(sys.argv) > 2 else 1)
