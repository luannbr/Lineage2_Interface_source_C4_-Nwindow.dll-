# Generic fake world for differential tests of window code (stock vs ours under unicorn): every callee in a table is a
# logging stub (stock: patched at its VA; ours: at NWBASE + rva and at the hook's address); every window is a fake
# object whose vtable logs its virtual calls; IsShowWindow (slot 41) answers by object.
import sys, os, random, struct, re
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import e4
from e4 import *

# pops of the virtual slots of NCWnd and its children (the arguments, 4 bytes each)
VPOP = {3: 4, 25: 12, 26: 0, 28: 0, 29: 0, 30: 0, 40: 28, 41: 0, 47: 8, 48: 0, 49: 0, 50: 8, 51: 8, 52: 8, 53: 0, 57: 4,
        56: 4, 75: 0, 76: 0, 77: 12, 80: 4, 81: 4, 9: 8}


R4 = bytes(b & 3 for b in range(256))


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


class GW(E4):
    """callees: {va: (name, pop, returns_this)}; ours_syms: {va: decorated symbol of a compiled copy to patch too}"""

    def __init__(self, kind, callees, ours_syms=None):
        E4.__init__(self, kind)
        self.callees = callees
        self.cb = {}
        self.gvars = {}
        for ee in self.pe.DIRECTORY_ENTRY_IMPORT:
            for imp in ee.imports:
                nm = imp.name.decode() if imp.name else ''
                if (nm.startswith('?GIs') or nm.startswith('?GL2')) and 'GameData' not in nm:
                    v = self.alloc(16)
                    self.gvars[nm] = v
                    self.write32(imp.address, v)
        n = 0
        for va, (name, pop, rthis) in sorted(callees.items()):
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
                for a in chase(self, va):
                    self.uc.mem_write(a, jump)
                if ours_syms and va in ours_syms:
                    self.uc.mem_write(self.symbols[ours_syms[va]], jump)
        self.vt = self.alloc(0x400)
        self.uc.mem_write(self.vt, bytes(0x400))
        for slot in range(0, 256):
            stub = STUB_BASE + 0xE000 + 16 * slot
            self.uc.mem_write(stub, bytes([0xC3]))
            self.stubs[stub] = ('vc:%d' % slot, None)
            self.write32(self.vt + 4 * slot, stub)
        # GMalloc's Free (slot 2) for the inline frees
        stub = STUB_BASE + 0xF800
        self.uc.mem_write(stub, bytes([0xC3]))
        self.stubs[stub] = ('mf:free', None)
        self.write32(self.gmalloc_vt + 8, stub)
        stub = STUB_BASE + 0xF900
        self.uc.mem_write(stub, bytes([0xC3]))
        self.stubs[stub] = ('mf:malloc', None)
        self.write32(self.gmalloc_vt, stub)
        self.objs = {}
        self.shown = {}
        self.rets = {}
        self.vrets = {}

    def dispatch(self, name):
        kind, nm = name.split(':', 1)
        if kind == 'cb':
            va, pop, rthis = self.cb[nm]
            self.last_pop = pop
            t = self.this()
            a = self.sargs(pop // 4)
            if nm in getattr(self, 'argfix', {}):
                a = self.argfix[nm](self, a)
            self.log.append((nm, self.nm(t), [self.val(x) for x in a]))
            if nm in self.rets:
                return self.rets[nm](self, t, a)
            return t if rthis else 0
        if kind == 'vc':
            slot = int(nm)
            if slot not in VPOP:
                raise RuntimeError('virtual call of slot %d' % slot)
            self.last_pop = VPOP[slot]
            t = self.this()
            a = self.sargs(self.last_pop // 4)
            self.log.append(('vcall', slot, self.nm(t), [self.val(x) for x in a]))
            if slot == 41:
                return self.shown.get(t, 0)
            if slot in self.vrets:
                return self.vrets[slot](self, t, a)
            return 0
        if kind == 'mf':
            if nm == 'malloc':
                self.last_pop = 8
                size = self.sargs(1)[0]
                p = self.alloc(max(size, 16))
                self.uc.mem_write(p, bytes(max(size, 16)))
                self.objs[p] = 'new%d' % size
                self.log.append(('GMalloc.Malloc', size))
                return p
            self.last_pop = 4
            self.log.append(('GMalloc.Free', self.val(self.sargs(1)[0])))
            return None
        return E4.dispatch(self, name)

    def nm(self, v):
        return self.objs.get(v, v)

    def val(self, v):
        if v is None or isinstance(v, tuple):
            return v
        if v in self.objs:
            return self.objs[v]
        if 0x60000000 <= v < 0x68000000 or 0x2F000000 <= v < 0x2F100000 or 0x10000000 <= v < 0x10400000:
            try:
                s = self.wstr(v, 80)
                if s is not None and all(32 <= ord(c) < 0xFFFF for c in s):
                    return 'S:' + s
            except Exception:
                pass
        return v

    def window(self, name, size=0x300, ints=None, rnd=None):
        w = self.alloc(size)
        self.uc.mem_write(w, bytes(size))
        self.write32(w, self.vt)
        ent = self.alloc(0x40)
        self.uc.mem_write(ent, bytes(0x40))
        self.objs[w] = name
        self.objs[ent] = 'entry:' + name
        self.write32(w + 0x74, ent)
        if rnd is not None:
            ba = bytearray(size)
            ba[0x90::4] = rnd.randbytes(len(ba[0x90::4])).translate(R4)
            self.uc.mem_write(w + 0x90, bytes(ba[0x90:]))
            self.write32(w + 0x74, ent)
        if ints:
            for off, v in ints.items():
                self.write32(w + off, v)
        return w

    def snapshot(self, base, size):
        mem = bytes(self.uc.mem_read(base, size))
        out = []
        for off in range(4, size, 4):
            v = struct.unpack('<I', mem[off:off + 4])[0]
            out.append(self.objs[v] if v in self.objs else v)
        return out


def compare(name, es, eo, run, cases, A_stock, A_ours, limit=int(os.environ.get('CMP_LIMIT', '3'))):
    bad = err = 0
    for i, c in enumerate(cases):
        es.heap_ptr, eo.heap_ptr = es.heap_mark, eo.heap_mark
        try:
            a = run(es, A_stock, c)
        except Exception as ex:
            a = {'exc': str(ex)[:300]}
        try:
            b = run(eo, A_ours, c)
        except Exception as ex:
            b = {'exc': str(ex)[:300]}
        if 'exc' in a or 'exc' in b:
            err += 1
            if err <= 4:
                print('--- case %d raised' % i, ascii(c)[:int(os.environ.get('CMP_CASEW', '200')) // 2], str(a.get('exc'))[:int(os.environ.get('CMP_EXCW', '300'))], '|', str(b.get('exc'))[:int(os.environ.get('CMP_EXCW', '300'))])
        elif a != b:
            bad += 1
            if bad <= limit:
                print('--- case', i, ascii(c)[:int(os.environ.get('CMP_CASEW', '400'))])
                for key in a:
                    if a[key] != b[key]:
                        print(' differs:', key)
                        if isinstance(a[key], list) and isinstance(b[key], list):
                            la, lb = a[key], b[key]
                            import difflib
                            sm = difflib.SequenceMatcher(None, [repr(x) for x in la], [repr(x) for x in lb], autojunk=False)
                            n = 0
                            for tag, i1, i2, j1, j2 in sm.get_opcodes():
                                if tag != 'equal':
                                    print('   %s stock[%d:%d] %s' % (tag, i1, i2, [ascii(x)[:int(os.environ.get('CMP_ENTW', '200'))] for x in la[i1:i2]][:3]))
                                    print('        ours[%d:%d] %s' % (j1, j2, [ascii(x)[:int(os.environ.get('CMP_ENTW', '200'))] for x in lb[j1:j2]][:3]))
                                    n += 1
                                    if n >= 4:
                                        break
                        elif isinstance(a[key], dict) and isinstance(b[key], dict):
                            for on in a[key]:
                                wa, wb = a[key].get(on), b[key].get(on)
                                if wa != wb:
                                    if wa is None or wb is None:
                                        print('  object %s is %s in the stock and %s in ours' % (on, 'missing' if wa is None else 'there', 'missing' if wb is None else 'there'))
                                        continue
                                    d = [(hex(4 * k), x, y) for k, (x, y) in enumerate(zip(wa, wb)) if x != y][:8]
                                    print('  object %s differs at (offset, stock, ours): %s' % (on, ascii(d)[:400]))
                        else:
                            print('  stock:', ascii(a[key])[:500])
                            print('  ours :', ascii(b[key])[:500])
        es.log.clear(); eo.log.clear()
    print(name, 'cases', len(cases), 'differ', bad, 'raised', err)
    return bad + err


def sa_pick(lm, va, label, alt=None):
    """the address of the function under test in `ours`: lm[va]. Standalone mode: a stock function with no standalone function of its own
    (sa.NoStandaloneFunction: a template instance / typedef'd class / stand-in the standalone compiles under another name) is taken from the
    compiled symbol `alt` when the test names it (the same source function), else None after printing the SKIP line that run_suite counts"""
    import sa
    try:
        return lm[va]
    except sa.NoStandaloneFunction as ex:
        if alt:
            a = sa.parse_map(sa.SA_MAP)[0].get(alt)
            if a is not None:
                return a
        sa.skip(label, ex)
        return None
