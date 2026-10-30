"""Differential test of the messenger's MD5 (C4: src/nwindow/Md5.cpp, Md5::Init / Md5::Update / Md5::Final; the C2 port called them NMd5Init /
NMd5Update / NMd5Final, so this script went stale until the symbols were renamed here; t_md5_class.py is the sibling with other message cuts
and the direct tests of Transform / CopyWords / DecodeWords) against the stock code
(Md5::Init 0x1014B120, Md5::Update 0x1014BA90, Md5::Final 0x1014BB50; cdecl: (context), (context, data, length), (digest, context)),
and both against hashlib. The context (0x58 bytes) is compared after every step, the digest at the end, for random messages cut at
random places (the CHL answer of the notification server feeds the challenge and the product key through this).

usage: python tools/port/emu/t_md5.py <cases> <seed>   (needs build/bin/Release/NWindowDecomp.dll + .map, or NWD_SA=1 for the standalone)"""
import sys, os, random, hashlib
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from emu import *

CTX = 0x58


class Md5Emu(Emu):
    def __init__(self, kind):
        super().__init__(kind)
        self.last_pop = 0

    def dispatch(self, name):
        raise RuntimeError('unexpected stub call %s' % name)


def addrs(e):
    if e.kind == 'stock':
        return 0x1014B120, 0x1014BA90, 0x1014BB50
    sym = e.symbols
    f = lambda pre: [v for k, v in sym.items() if k.startswith(pre)][0]
    return f('?Init@Md5@@'), f('?Update@Md5@@'), f('?Final@Md5@@')


def run(kind, cases, seed):
    e = Md5Emu(kind)
    init, update, final = addrs(e)
    rnd = random.Random(seed)
    out = []
    ctx = e.alloc(CTX + 16)
    data = e.alloc(0x4000)
    dig = e.alloc(32)
    for c in range(cases):
        n = rnd.choice([0, 1, 3, 55, 56, 57, 63, 64, 65, 119, 120, 128, 1000, rnd.randrange(0, 3000)])
        msg = bytes(rnd.randrange(256) for _ in range(n))
        e.uc.mem_write(ctx, bytes([0xAA]) * (CTX + 16))
        e.call(init, args=[ctx], conv='cdecl')
        states = [bytes(e.uc.mem_read(ctx, CTX))]
        pos = 0
        while pos < n:
            k = min(n - pos, rnd.choice([1, 7, 64, 100, rnd.randrange(1, 300)]))
            e.uc.mem_write(data, msg[pos:pos + k])
            e.call(update, args=[ctx, data, k], conv='cdecl')
            states.append(bytes(e.uc.mem_read(ctx, CTX)))
            pos += k
        e.uc.mem_write(dig, bytes(32))
        e.call(final, args=[dig, ctx], conv='cdecl')
        d = bytes(e.uc.mem_read(dig, 16))
        out.append((n, states, d, bytes(e.uc.mem_read(ctx, CTX)), hashlib.md5(msg).digest()))
    return out


if __name__ == '__main__':
    cases = int(sys.argv[1]) if len(sys.argv) > 1 else 40
    seed = int(sys.argv[2]) if len(sys.argv) > 2 else 1
    a = run('stock', cases, seed)
    b = run('ours', cases, seed)
    bad = 0
    for x, y in zip(a, b):
        ok_states = x[1] == y[1]
        ok_dig = x[2] == y[2] and x[3] == y[3]
        ok_ref = x[2] == x[4] and y[2] == y[4]
        if not (ok_states and ok_dig and ok_ref):
            bad += 1
            print('DIFF len %d: states %s digest %s hashlib %s' % (x[0], ok_states, ok_dig, ok_ref))
    print('t_md5 Init/Update/Final cases %d  differ %d  raised 0' % (len(a), bad))
    sys.exit(1 if bad else 0)
