"""Differential test of w3misc's console helpers against the stock code: the class-tier switch of GetHennaSlotCount (C4 0x10074450,
GetMyUser 0x10073E80 stubbed; the class id is at User +0x210) and, from the C2 script, ScaleRect (rect: C2 only, the C4 image has no such
function, so it prints a SKIP), limiter and expand (stale, with C2 addresses: the C4 successors are win/t_limiter.py and win/t_chat.py).
usage: python tools/port/emu/t_misc.py rect|henna <cases> <seed>   (needs build/bin/Release/NWindowDecomp.dll + .map, or NWD_SA=1 for the standalone)"""
import sys, os, random, struct
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import stubs
from stubs import *

stubs.STOCK_FN.clear()      # the C2 services of stubs.py sit at other C4 addresses (and these functions use none of them)
stubs.OURS_FN.clear()
TipEmu.CONSOLE_VA = 0x102C39F4
stubs.FN_POP.update({'GetMyUser': 0, 'GMalloc_Malloc': 8, 'GMalloc_Free': 4, 'ShowSystemMessage': 8, 'NStringCtor': 4,
                     'NStringGetAt': 4, 'ExpandToken': 4})
stubs.IMPORT_POP.update({'?SetString@FParser@@QAEPAGPAG@Z': 4, '?GetNextToken@FParser@@QAEPAGXZ': 0,
                         '?appStrcmp@@YAHPBG0@Z': 0})


class MiscEmu(TipEmu):
    def __init__(self, kind):
        super().__init__(kind)
        self.user = 0
        self.calls = []
        self.next_stub = STUB_BASE + 0x9000

    def service(self, va, name):
        st = self.next_stub
        self.next_stub += 16
        self.uc.mem_write(st, b'\xC3')
        self.stubs[st] = ('fn:' + name, None)
        self.uc.mem_write(va, b'\x68' + struct.pack('<I', st) + b'\xC3')

    def do_fn(self, nm):
        if nm == 'GetMyUser':
            return self.user
        if nm == 'GMalloc_Malloc':
            return self.alloc(self.arg(0))
        if nm == 'GMalloc_Free':
            return None
        if nm == 'ShowSystemMessage':
            self.calls.append(('ShowSystemMessage', self.arg(0), self.arg(1)))
            return None
        if nm == 'NStringCtor':          # NString(const TCHAR*): +4 the text copy, +8 its length
            t = self.this()
            txt = self.wstr(self.arg(0))
            self.write32(t + 4, self.put_wstr(txt))
            self.write32(t + 8, len(txt))
            return t
        if nm == 'NStringGetAt':
            return self.read32(self.this() + 4)
        if nm == 'ExpandToken':
            p = self.arg(0)
            if not p:
                return 0
            tok = self.wstr(p)
            if tok.startswith('%'):
                self.write_wstr_at(p, 'Name' + tok[1:])
                self.calls.append(('expand', tok))
                return p
            return 0
        return super().do_fn(nm)

    def do_import(self, nm):
        if nm == '?appStrcmp@@YAHPBG0@Z':
            a, b = self.wstr(self.arg(0)), self.wstr(self.arg(1))
            return 0 if a == b else (1 if a > b else 0xFFFFFFFF)
        if nm == '?SetString@FParser@@QAEPAGPAG@Z':
            toks = [x for x in self.wstr(self.arg(0)).split(' ') if x]
            self.tokens = [self.put_wstr(x) for x in toks]
            return self.tokens.pop(0) if self.tokens else 0
        if nm == '?GetNextToken@FParser@@QAEPAGXZ':
            return self.tokens.pop(0) if self.tokens else 0
        return super().do_import(nm)

    def mkst(self, name):
        st = self.next_stub
        self.next_stub += 16
        self.uc.mem_write(st, b'\xC3')
        self.stubs[st] = ('fn:' + name, None)
        return st

    def sym(self, prefix):
        c = [v for k, v in self.symbols.items() if k.startswith(prefix)]
        if len(c) != 1:
            raise RuntimeError('symbol %s: %s' % (prefix, c))
        return c[0]


def setup_services(e, kind):
    """The allocator, the console's ShowSystemMessage, NString and ExpandToken as stubs at their stock / our places."""
    e.write32(e.gmalloc_vt + 0, e.mkst('GMalloc_Malloc'))
    e.write32(e.gmalloc_vt + 8, e.mkst('GMalloc_Free'))
    if kind == 'stock':
        e.service(0x1006A8C0, 'ShowSystemMessage')
        e.service(0x100266D0, 'NStringCtor')
        e.service(0x10026510, 'NStringGetAt')
        e.service(0x1003E900, 'ExpandToken')
    else:
        e.service(e.sym('?ShowSystemMessage@NConsoleWnd@@'), 'ShowSystemMessage')
        e.service(e.sym('??0NString@@QAE@PBG@Z'), 'NStringCtor')
        e.service(e.sym('?GetAt@NString@@QAEPAGI@Z'), 'NStringGetAt')
        e.service(NWBASE + 0x3E900, 'ExpandToken')


def rect(cases, seed):
    # NConsoleWnd::ScaleRect (C2 0x10056500) is "C2 only (no C3 function: 0x10056500 is the middle of NCConsole::OnTimer), nobody calls it"
    # (src/nwindow/NConsoleWndResize.cpp): the C4 image has no such function, nothing to compare
    print('SKIP rect (NConsoleWnd::ScaleRect): no C4 function (stale, C2 only)')
    return 0
    res = {}
    for kind in ('stock', 'ours'):
        e = MiscEmu(kind)
        fn = 0x10056500 if kind == 'stock' else e.sym('?ScaleRect@NConsoleWnd@@')
        out = []
        rnd = random.Random(seed)
        for i in range(cases):
            W0 = rnd.choice([800, 1024, 1280, 1600, 640, rnd.randint(300, 2000)])
            H0 = rnd.choice([600, 768, 1024, 1200, 480, rnd.randint(300, 2000)])
            W1 = rnd.choice([800, 1024, 1280, 1600, 640, rnd.randint(300, 2000)])
            H1 = rnd.choice([600, 768, 1024, 1200, 480, rnd.randint(300, 2000)])
            X = rnd.randint(-50, W0 + 50)
            Y = rnd.randint(-50, H0 + 50)
            W = rnd.randint(1, W0 + 100)
            H = rnd.randint(1, H0 + 100)
            r = e.alloc(16)
            for k, v in enumerate((X, Y, W, H)):
                e.write32(r + 4 * k, v)
            e.call(fn, this=0x1234, args=[W0, H0, W1, H1, r])
            out.append(((W0, H0, W1, H1, X, Y, W, H), e.rds32(r), e.rds32(r + 4), e.rds32(r + 8), e.rds32(r + 12)))
        res[kind] = out
    bad = 0
    for a, b in zip(res['stock'], res['ours']):
        if a != b:
            bad += 1
            if bad < 10:
                print('DIFF', a, b)
    print('rect cases %d  differ %d  raised 0' % (cases, bad))
    return 1 if bad else 0


def henna(cases, seed):
    res = {}
    for kind in ('stock', 'ours'):
        e = MiscEmu(kind)
        user = e.alloc(0x400)
        gm = 0x10073E80 if kind == 'stock' else e.sym('?GetMyUser@NConsoleWnd@@')
        e.service(gm, 'GetMyUser')
        fn = 0x10074450 if kind == 'stock' else e.sym('?GetHennaSlotCount@NConsoleWnd@@')
        out = []
        e.user = user
        for cid in list(range(-3, 140)) + [0xFFFFFFFF, 0x7FFFFFFF, 0x80000000]:
            e.write32(user + 0x210, cid & 0xFFFFFFFF)
            v = e.call(fn, this=0x1234)
            out.append((cid, v & 0xFFFFFFFF))
        e.user = 0
        out.append(('nouser', e.call(fn, this=0x1234) & 0xFFFFFFFF))
        res[kind] = out
    bad = 0
    for a, b in zip(res['stock'], res['ours']):
        if a != b:
            bad += 1
            print('DIFF', a, b)
    print('henna cases %d  differ %d  raised 0' % (len(res['stock']), bad))
    return 1 if bad else 0


def limiter(cases, seed):
    res = {}
    for kind in ('stock', 'ours'):
        e = MiscEmu(kind)
        setup_services(e, kind)
        fn = 0x100479F0 if kind == 'stock' else e.sym('?AddLine@NChatLimiter@@')
        rnd = random.Random(seed)
        out = []
        pool = ['hello', 'hi', 'a', 'sell wts armor', 'Lineage', 'x' * 30]
        for c in range(cases):
            lim = e.alloc(0x40)
            vals = [0, 0, 0, 0, rnd.choice([20000, 5000, 100]), rnd.choice([10000, 3000]), 100000, rnd.choice([200, 50]),
                    rnd.choice([60, 5, 0]), rnd.choice([100, 20]), rnd.choice([2000, 100, 1500])]
            for k, v in enumerate(vals):
                e.write32(lim + 4 * k, v)
            e.write32(lim + 0x2C, struct.unpack('<I', struct.pack('<f', rnd.choice([0.1, 0.5, 0.0, 1.0])))[0])
            e.uc.mem_write(lim + 0x30, b'\x00')
            e.calls = []
            for step in range(rnd.randint(1, 12)):
                txt = rnd.choice(pool)
                e.call(fn, this=lim, args=[len(txt), e.put_wstr(txt) if rnd.random() < 0.97 else 0])
            n = e.read32(lim + 8)
            items = []
            for i in range(n):
                it = e.read32(e.read32(lim + 4) + 8 * i)
                items.append((e.wstr(e.read32(it + 4)), e.rds32(e.read32(lim + 4) + 8 * i + 4)))
            out.append((e.rds32(lim), bytes(e.uc.mem_read(lim + 0x30, 1)), items, list(e.calls)))
        res[kind] = out
    bad = 0
    for i, (a, b) in enumerate(zip(res['stock'], res['ours'])):
        if a != b:
            bad += 1
            if bad < 6:
                print('DIFF case', i, a, b)
    print('limiter: %d cases, %d differences' % (cases, bad))


def expand(cases, seed):
    res = {}
    words = ['hello', '%target', '%self', 'a', '$', '$foo', '%party1', 'sell', 'wts', '/cmd', 'x', 'y']
    for kind in ('stock', 'ours'):
        e = MiscEmu(kind)
        setup_services(e, kind)
        fn = 0x100450B0 if kind == 'stock' else e.sym('?ExpandLine@NCChatWnd@@')
        rnd = random.Random(seed)
        out = []
        chatwnd = e.alloc(0x800)
        for c in range(cases):
            line = ' '.join(rnd.choice(words) for _ in range(rnd.randint(0, 6)))
            if rnd.random() < 0.15:
                line = '$' + line
            outbuf = e.alloc(4096)
            e.write_wstr_at(outbuf, 'OLD')
            e.calls = []
            inp = e.put_wstr(line)
            e.call(fn, this=chatwnd, args=[outbuf, inp if rnd.random() < 0.97 else 0])
            out.append((line, e.wstr(outbuf), list(e.calls)))
        res[kind] = out
    bad = 0
    for i, (a, b) in enumerate(zip(res['stock'], res['ours'])):
        if a != b:
            bad += 1
            if bad < 8:
                print('DIFF case', i, a, b)
    print('expand: %d cases, %d differences' % (cases, bad))


if __name__ == '__main__':
    what = sys.argv[1]
    n = int(sys.argv[2]) if len(sys.argv) > 2 else 1000
    seed = int(sys.argv[3]) if len(sys.argv) > 3 else 1
    sys.exit({'rect': rect, 'henna': henna, 'limiter': limiter, 'expand': expand}[what](n, seed) or 0)
