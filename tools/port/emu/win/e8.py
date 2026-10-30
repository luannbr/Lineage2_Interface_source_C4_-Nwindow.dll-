# Differential test of NCConsole::ShowWindow and NCConsole::OnDestroy (stock vs ours under unicorn)
#   python e8.py [cases] [seed] [show|destroy]
import sys, os, random, struct, re
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import e4
from e4 import *

SHOW = 0x1005c530
DESTROY = 0x1005df20

CB = {0x10035880: ('RemoveChildWindow', 4), 0x1002b430: ('KillTimer', 4), 0x10036160: ('NCWnd::OnDestroy', 0),
      0x100345a0: ('NCWnd::ShowWindow', 0), 0x1015dfc0: ('HideAll', 0), 0x1005c450: ('CastToLineagePlayerController', 0)}
imp_pop = {'ImmGetContext': 4, 'ImmSetConversionStatus': 12}
stubs.IMPORT_POP.update(imp_pop)
OFFS_FILE = os.path.join(HERE, 'offsets.txt')


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


class E8(E4):
    def __init__(self, kind):
        E4.__init__(self, kind)
        self.cb = {}
        self.gvars = {}
        for ee in self.pe.DIRECTORY_ENTRY_IMPORT:
            for imp in ee.imports:
                nm = imp.name.decode() if imp.name else ''
                if nm in ('?GIsOnlyL2Console@@3HA', '?GIsTestHtml@@3HA', '?GIsOnlyMSN@@3HA', '?GLanguageType@@3HA'):
                    if nm == '?GLanguageType@@3HA':
                        continue
                    v = self.alloc(16)
                    self.gvars[nm] = v
                    self.write32(imp.address, v)
        n = 0
        for va, (name, pop) in sorted(CB.items()):
            stub = STUB_BASE + 0xC000 + 16 * n
            n += 1
            self.uc.mem_write(stub, bytes([0xC3]))
            self.stubs[stub] = ('cb:' + name, None)
            self.cb[name] = (va, pop)
            jump = b'\x68' + struct.pack('<I', stub) + b'\xC3'
            if self.kind == 'stock':
                self.uc.mem_write(va, jump)
            else:
                self.uc.mem_write(NWBASE + (va - 0x10000000), jump)
                for a in chase(self, va):
                    self.uc.mem_write(a, jump)
        self.vt = self.alloc(0x400)
        self.uc.mem_write(self.vt, bytes(0x400))
        self.vslot_pop = {3: 4, 48: 0, 49: 0}
        for slot in (3, 48, 49):
            stub = STUB_BASE + 0xE000 + 16 * slot
            self.uc.mem_write(stub, bytes([0xC3]))
            self.stubs[stub] = ('vc:%d' % slot, None)
            self.write32(self.vt + 4 * slot, stub)
        # GMalloc's Free (slot 2)
        stub = STUB_BASE + 0xE800
        self.uc.mem_write(stub, bytes([0xC3]))
        self.stubs[stub] = ('mf:free', None)
        self.write32(self.gmalloc_vt + 8, stub)
        self.objs = {}
        self.ret_ctrl = 0

    def dispatch(self, name):
        kind, nm = name.split(':', 1)
        if kind == 'cb':
            self.last_pop = self.cb[nm][1]
            t = self.this()
            a = self.sargs(1 if nm == 'CastToLineagePlayerController' else self.last_pop // 4)
            self.log.append((nm, None if nm == 'CastToLineagePlayerController' else self.nm(t), [self.nm(x) for x in a]))
            if nm == 'CastToLineagePlayerController':
                return self.ret_ctrl
            return 0
        if kind == 'vc':
            self.last_pop = self.vslot_pop[int(nm)]
            self.log.append(('vcall', int(nm), self.nm(self.this()), self.sargs(self.last_pop // 4)))
            return 0
        if kind == 'mf':
            self.last_pop = 4
            self.log.append(('GMalloc.Free', self.sargs(1)[0]))
            return None
        return E4.dispatch(self, name)

    def nm(self, v):
        return self.objs.get(v, v)

    def do_import(self, nm):
        if nm == 'ImmGetContext':
            self.log.append(('ImmGetContext', self.sargs(1)[0] != 0))
            return 0x4321
        if nm == 'ImmSetConversionStatus':
            self.log.append(('ImmSetConversionStatus', self.sargs(3)))
            return 1
        return E4.do_import(self, nm)


def parse_offsets():
    offs = set()
    for src in (os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE)))), 'src', 'nwindow', 'NCConsole.cpp'),):
        t = open(src, encoding='utf-8').read()
        for m in re.finditer(r'0x([0-9A-Fa-f]{2,3})\b', t):
            v = int(m.group(1), 16)
            if 0xC8 <= v <= 0x290 and v % 4 == 0:
                offs.add(v)
    return sorted(offs)


FIELD_OFFS = parse_offsets()


def world(e, c, mode):
    e.case = c
    e.objs = {}
    con = e.console
    e.uc.mem_write(con, bytes(0x4000))
    e.objs[con] = 'console'
    r = random.Random(c['seed'])

    def window(name, nullable=False):
        if nullable and r.random() < 0.2:
            return 0
        w = e.alloc(0x200)
        e.uc.mem_write(w, bytes(0x200))
        e.write32(w, e.vt)
        ent = e.alloc(0x40)
        e.uc.mem_write(ent, bytes(0x40))
        e.objs[w] = name
        e.objs[ent] = 'entry:' + name
        e.write32(w + 0x74, ent)
        return w

    e.fields = {}
    for off in FIELD_OFFS:
        if off in (0x27c, 0x280, 0x284, 0x288, 0x28c):
            continue
        w = window('w%x' % off, nullable=(mode == 'destroy'))
        e.write32(con + off, w)
        e.fields[off] = w
        # the ints the show conditions test
        for o2 in (0xEC, 0xF8, 0xF4, 0x100, 0x114):
            if w:
                e.write32(w + o2, r.choice([0, 0, 1, 2]))
    # pairs at 0x214 / 0x21c (a loop of two)
    for off in (0x214, 0x218, 0x21c, 0x220):
        w = window('w%x' % off, nullable=(mode == 'destroy'))
        e.write32(con + off, w)
    if mode == 'show':
        # no null windows: every field exists
        pass
    # the console's other pointers
    god = e.alloc(0x200); e.uc.mem_write(god, bytes(0x200)); e.objs[god] = 'god'
    gw = window('godwin')
    e.write32(god + 0xF4, gw)
    e.write32(con + 0x3BE0, god)
    pw = window('petition', nullable=True)
    e.write32(con + 0x3C00, pw)
    # engine -> client -> viewports(0) -> actor
    eng = e.alloc(0x100); e.uc.mem_write(eng, bytes(0x100))
    cl = e.alloc(0x100); e.uc.mem_write(cl, bytes(0x100))
    vps = e.alloc(0x20); vp = e.alloc(0x100); e.uc.mem_write(vp, bytes(0x100))
    e.write32(vp + 0x3C, 0xAC70)
    e.write32(vps, vp)
    e.write32(cl + 0x38, vps); e.write32(cl + 0x3C, 1)
    e.write32(eng + 0x58, cl)
    e.write32(con + 0x54, eng)
    ctrl = e.alloc(0x1000); e.uc.mem_write(ctrl, bytes(0x1000))
    e.write32(ctrl + 0xA30, c['ctrlbit'])
    e.objs[ctrl] = 'ctrl'
    e.ret_ctrl = ctrl if c['ctrl'] else 0
    # the child list (GIsOnlyMSN)
    head = 0
    prev = 0
    for k in range(c['nchild']):
        node = e.alloc(0x20); e.uc.mem_write(node, bytes(0x20))
        ent = e.alloc(0x20); e.uc.mem_write(ent, bytes(0x20))
        cw = window('child%d' % k)
        e.write32(ent + 4, cw)
        e.write32(node, ent)
        if prev:
            e.write32(prev + 8, node)
        else:
            head = node
        prev = node
    e.write32(con + 0x34, head)
    # balloons
    e.write32(e.glob(0x102C6BC0), 7)
    nb = c['nballoon']
    arr = e.alloc(12 * max(1, nb))
    e.uc.mem_write(arr, bytes(12 * max(1, nb)))
    for k in range(nb):
        txt = e.alloc(48) if r.random() < 0.7 else 0
        if txt:
            e.objs[txt] = 'text%d' % k
        e.write32(arr + 12 * k, 0x100 + k)
        e.write32(arr + 12 * k + 4, txt)
        e.write32(arr + 12 * k + 8, 0x200 + k)
    e.write32(con + 0x27c, arr); e.write32(con + 0x280, nb); e.write32(con + 0x284, nb)
    e.write32(con + 0x288, c['timer'])
    e.write32(e.gvars['?GIsOnlyL2Console@@3HA'], c['only'])
    e.write32(e.gvars['?GIsTestHtml@@3HA'], c['test'])
    e.write32(e.gvars['?GIsOnlyMSN@@3HA'], c['msn'])
    e.set_lang(c['lang'])
    return con


def snapshot(e, con):
    mem = bytes(e.uc.mem_read(con, 0x3C0))
    out = []
    for off in range(0, 0x3C0, 4):
        v = struct.unpack('<I', mem[off:off + 4])[0]
        out.append(e.objs[v] if v in e.objs else v)
    return out


def run8(e, A, c, mode):
    mark = len(e.log)
    con = world(e, c, mode)
    r = e.call(A[mode], this=con, args=[])
    return dict(ret=(None if mode == 'show' else r), log=e.log[mark:], console=snapshot(e, con))


def rand8(r):
    return dict(seed=r.randint(0, 10 ** 6), only=r.randint(0, 1), test=r.randint(0, 1), msn=r.choice([0, 0, 0, 1]),
                lang=r.choice([0, 1, 5]), ctrl=r.randint(0, 1), ctrlbit=r.randint(0, 1), nchild=r.randint(0, 3),
                nballoon=r.randint(0, 3), timer=r.choice([0, 0, 5]))


def main(n, seed, mode):
    r = random.Random(seed)
    cases = [rand8(r) for _ in range(n)]
    es = E8('stock'); eo = E8('ours')
    lm = log_map()
    As = {'show': SHOW, 'destroy': DESTROY}
    Ao = {'show': lm[SHOW], 'destroy': lm[DESTROY]}
    es.heap_mark, eo.heap_mark = es.heap_ptr, eo.heap_ptr
    bad = err = 0
    for i, c in enumerate(cases):
        es.heap_ptr, eo.heap_ptr = es.heap_mark, eo.heap_mark
        try:
            a = run8(es, As, c, mode)
        except Exception as ex:
            a = {'exc': str(ex)[:300]}
        try:
            b = run8(eo, Ao, c, mode)
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
    print(mode, 'cases', n, 'differ', bad, 'raised', err)


if __name__ == '__main__':
    main(int(sys.argv[1]) if len(sys.argv) > 1 else 10, int(sys.argv[2]) if len(sys.argv) > 2 else 1,
         sys.argv[3] if len(sys.argv) > 3 else 'show')
