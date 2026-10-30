# Differential test of NCConsole::Say (stock vs ours under unicorn)
#   python e9.py [cases] [seed]
import sys, os, random, struct
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import e4
from e4 import *

SAY = 0x1005d580
CB = {0x10052f40: ('AddChatMessage', 16), 0x100dfe90: ('PetAddGM', 16), 0x100dfc60: ('PetAddPlayer', 16),
      0x100b1f70: ('PartyRoomAdd', 8), 0x100513f0: ('Wnd264Add', 16), 0x1006a780: ('FindUserByID', 4)}


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


class E9(E4):
    def __init__(self, kind):
        E4.__init__(self, kind)
        self.cb = {}
        n = 0
        names = {va: nm for va, (nm, _) in CB.items()}
        if e4._emu.sa.ENABLED:
            # the standalone's linker folded NCPetitionWnd::AddMessage and NCGMPetitionWnd::AddMessage (identical code) to one address: a call of ours
            # cannot say which of the two stock functions it stands for, so both sides log the pair under one name
            fm = e4._emu.sa.func_map()
            byaddr = {}
            for va in CB:
                if va in fm:
                    byaddr.setdefault(fm[va], []).append(va)
            for vas in byaddr.values():
                if len(vas) > 1:
                    joined = '|'.join(CB[v][0] for v in sorted(vas))
                    for v in vas:
                        names[v] = joined
        for va, (name, pop) in sorted(CB.items()):
            name = names[va]
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
        self.vslot_pop = {29: 0, 41: 0, 48: 0}
        for slot in (29, 41, 48):
            stub = STUB_BASE + 0xE000 + 16 * slot
            self.uc.mem_write(stub, bytes([0xC3]))
            self.stubs[stub] = ('vc:%d' % slot, None)
            self.write32(self.vt + 4 * slot, stub)
        # GMalloc: Malloc (slot 0) and Free (slot 2)
        for slot, nm, pop in ((0, 'malloc', 8), (2, 'free', 4)):
            stub = STUB_BASE + 0xE800 + 16 * slot
            self.uc.mem_write(stub, bytes([0xC3]))
            self.stubs[stub] = ('mf:' + nm, None)
            self.write32(self.gmalloc_vt + 4 * slot, stub)
        self.objs = {}
        self.show_ret = 0

    def dispatch(self, name):
        kind, nm = name.split(':', 1)
        if kind == 'cb':
            self.last_pop = self.cb[nm][1]
            t = self.this()
            a = self.sargs(self.last_pop // 4)
            if nm == 'FindUserByID':
                self.log.append(('FindUserByID', a[0]))
                return self.speaker
            self.log.append((nm, self.nm(t), [self.val(x) for x in a]))
            return 0
        if kind == 'vc':
            self.last_pop = 0
            self.log.append(('vcall', int(nm), self.nm(self.this())))
            return self.show_ret if int(nm) == 41 else 0
        if kind == 'mf':
            if nm == 'malloc':
                self.last_pop = 8
                size = self.sargs(1)[0]
                p = self.alloc(size)
                self.uc.mem_write(p, bytes(size))
                self.objs[p] = 'text'
                self.log.append(('malloc', size))
                return p
            self.last_pop = 4
            self.log.append(('free', self.nm(self.sargs(1)[0])))
            return None
        return E4.dispatch(self, name)

    def nm(self, v):
        return self.objs.get(v, v)

    def val(self, v):
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


def world(e, c):
    e.objs = {}
    con = e.console
    e.uc.mem_write(con, bytes(0x4000))
    e.objs[con] = 'console'

    def window(name):
        w = e.alloc(0x300)
        e.uc.mem_write(w, bytes(0x300))
        e.write32(w, e.vt)
        e.objs[w] = name
        return w
    e.write32(con + 0xF4, window('chat'))
    e.write32(con + 0x10C, window('gm'))
    e.write32(con + 0x16C, window('partyroom'))
    e.write32(con + 0x264, window('w264'))
    e.write32(con + 0x3C00, window('petition'))
    e.show_ret = c['gmshown']
    # balloons
    nb = c['nb']
    arr = e.alloc(12 * (nb + 4))
    e.uc.mem_write(arr, bytes(12 * (nb + 4)))
    for k in range(nb):
        txt = e.alloc(48)
        e.objs[txt] = 'oldtext%d' % k
        e.write32(arr + 12 * k, c['ids'][k])
        e.write32(arr + 12 * k + 4, txt if c['hastext'][k] else 0)
        e.write32(arr + 12 * k + 8, 77 + k)
    e.write32(con + 0x27c, arr); e.write32(con + 0x280, nb); e.write32(con + 0x284, nb + 4)
    e.write32(con + 0x28c, c['ticks'])
    # the speaker
    e.speaker = 0
    e.pawn = 0
    if c['speaker']:
        u = e.alloc(0x300); e.uc.mem_write(u, bytes(0x300)); e.objs[u] = 'speaker'
        e.write16(u + 0x200, c['store']) if hasattr(e, 'write16') else e.uc.mem_write(u + 0x200, struct.pack('<H', c['store']))
        if c['pawn']:
            pw = e.alloc(0x1800); e.uc.mem_write(pw, bytes(0x1800)); e.objs[pw] = 'pawn'
            e.write32(pw + 0x4C, 5)
            e.write32(pw + 0x1684, c['oldballoon'])
            e.write32(u + 0x158, pw)
            e.pawn = pw
        e.speaker = u
    return con


def run9(e, A, c):
    mark = len(e.log)
    con = world(e, c)
    name = e.put_wstr(c['name'])
    text = e.put_wstr(c['text'])
    e.call(A['say'], this=con, args=[c['oid'], name, text, c['color'], c['type']])
    nb = e.rds32(con + 0x280)
    arr = e.read32(con + 0x27c)
    balloons = []
    for k in range(nb):
        o, t, tk = e.read32(arr + 12 * k), e.read32(arr + 12 * k + 4), e.read32(arr + 12 * k + 8)
        balloons.append((o, e.wstr(t) if t and t in e.alloc_texts() else (e.objs.get(t, t)), tk))
    pawn = None
    if e.pawn:
        pawn = (e.rds32(e.pawn + 0x4C), e.wstr(e.read32(e.pawn + 0x1684)) if e.read32(e.pawn + 0x1684) > 0x60000000 and e.read32(e.pawn + 0x1684) not in e.objs else e.objs.get(e.read32(e.pawn + 0x1684), e.read32(e.pawn + 0x1684)))
    return dict(log=e.log[mark:], nb=nb, balloons=balloons, pawn=pawn)


def alloc_texts(self):
    return {k for k, v in self.objs.items() if v == 'text'}


E9.alloc_texts = alloc_texts


def rand9(r):
    n = r.randint(0, 3)
    ids = [r.randint(1, 4) for _ in range(n)]
    return dict(oid=r.randint(1, 4), name=r.choice(['Bob', 'Zed', '']), text=r.choice(['hi', 'hello world, how are you doing today', 'x' * 19, 'y' * 18, '']),
                color=r.choice([0xFFDCDCDC, 0xFF112233]), type=r.choice([0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0xE, 0xF, 0x10, 0x11, 0x12, 0x13]),
                gmshown=r.randint(0, 1), nb=n, ids=ids, hastext=[r.randint(0, 1) for _ in range(n)], ticks=r.randint(0, 99),
                speaker=r.randint(0, 1), store=r.choice([0, 0, 1, 3]), pawn=r.randint(0, 1), oldballoon=r.choice([0, 0, 0x1234]))


def main(n, seed):
    r = random.Random(seed)
    cases = [rand9(r) for _ in range(n)]
    es = E9('stock'); eo = E9('ours')
    lm = log_map()
    As = {'say': SAY}
    Ao = {'say': lm[SAY]}
    es.heap_mark, eo.heap_mark = es.heap_ptr, eo.heap_ptr
    bad = err = 0
    for i, c in enumerate(cases):
        es.heap_ptr, eo.heap_ptr = es.heap_mark, eo.heap_mark
        try:
            a = run9(es, As, c)
        except Exception as ex:
            a = {'exc': str(ex)[:300]}
        try:
            b = run9(eo, Ao, c)
        except Exception as ex:
            b = {'exc': str(ex)[:300]}
        if 'exc' in a or 'exc' in b:
            err += 1
            if err <= 4:
                print('--- case %d raised' % i, a.get('exc'), '|', b.get('exc'))
        elif a != b:
            bad += 1
            if bad <= 4:
                print('--- case', i, ascii(c))
                for key in a:
                    if a[key] != b[key]:
                        print(' differs:', key)
                        print('  stock:', ascii(a[key])[:700])
                        print('  ours :', ascii(b[key])[:700])
        es.log.clear(); eo.log.clear()
    print('cases', n, 'differ', bad, 'raised', err)


if __name__ == '__main__':
    main(int(sys.argv[1]) if len(sys.argv) > 1 else 10, int(sys.argv[2]) if len(sys.argv) > 2 else 1)
