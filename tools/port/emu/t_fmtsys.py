"""Differential test of NConsoleWnd::FormatSystemMessage (stock 0x1006A3D0 in C4) against our port: random formats with "$s1"-like
tokens, "%" and Hangul particles (the stock calls HasNoFinalConsonant, 0x10064AD0, and ours inlines it), random parameters
(Hangul, ASCII, empty, NULL), the out buffer pre-filled so that a stray write shows, and the character before the buffer
(read by a leading "%") set at random. Compares the whole out buffer (and the red zone around it).
usage: python tools/port/emu/t_fmtsys.py <cases> <seed>   (needs build/bin/Release/NWindowDecomp.dll + .map, or NWD_SA=1 for the standalone)"""
import sys, os, random, struct
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from emu import *


class FmtEmu(Emu):
    def dispatch(self, name):
        raise RuntimeError('unexpected stub call ' + name)

    last_pop = 0

    def sym(self, prefix):
        c = [v for k, v in self.symbols.items() if k.startswith(prefix)]
        if len(c) != 1:
            raise RuntimeError('symbol %s: %s' % (prefix, c))
        return c[0]


HANGUL = [0xAC00, 0xAC01, 0xAC02, 0xAC1C, 0xB098, 0xB0A8, 0xD55C, 0xD7A3, 0xD7A2, 0xC774, 0xC744, 0xC740, 0xC640, 0xAC00 + 28 * 21 * 5,
          0xAC00 + 28 * 21 * 5 + 27, 0xAC00 + 588 * 3 + 28 * 4]
OTHER = [0x41, 0x7A, 0x30, 0x20, 0x24, 0x25, 0x2E, 0xABFF, 0xD7A4, 0x3131, 0xC741, 0xC743, 0xC73F, 0xC641, 0xC63F]


def rand_char(rnd):
    r = rnd.random()
    if r < 0.45:
        return chr(rnd.choice(HANGUL))
    if r < 0.7:
        return chr(rnd.choice(OTHER))
    if r < 0.85:
        return chr(0xAC00 + rnd.randint(0, 11171))
    return chr(rnd.randint(0x20, 0x7E))


def rand_format(rnd):
    parts = []
    for _ in range(rnd.randint(0, 12)):
        r = rnd.random()
        if r < 0.22:
            parts.append(rnd.choice(['$s1', '$s2', '$c1', '$', '$s', '$1']))
        elif r < 0.45:
            parts.append('%' + rnd.choice(['이', '을', '은', '와', 'x', '가', '']))
        else:
            parts.append(''.join(rand_char(rnd) for _ in range(rnd.randint(1, 4))))
    return ''.join(parts)


def rand_param(rnd):
    r = rnd.random()
    if r < 0.15:
        return None
    if r < 0.25:
        return ''
    return ''.join(rand_char(rnd) for _ in range(rnd.randint(1, 6)))


def main():
    cases = int(sys.argv[1]) if len(sys.argv) > 1 else 2000
    seed = int(sys.argv[2]) if len(sys.argv) > 2 else 1
    res = {}
    for kind in ('stock', 'ours'):
        e = FmtEmu(kind)
        fn = 0x1006A3D0 if kind == 'stock' else e.sym('?FormatSystemMessage@NConsoleWnd@@')
        rnd = random.Random(seed)
        out = []
        for i in range(cases):
            fmt = None if rnd.random() < 0.02 else rand_format(rnd)
            p1, p2 = rand_param(rnd), rand_param(rnd)
            prev = rnd.choice([0, 0x41] + HANGUL + [rnd.randint(1, 0xFFFF)])
            base = e.alloc(2 * 512)
            red = 8
            e.uc.mem_write(base, struct.pack('<H', prev) + b'\xAA\xAA' * (red - 1))   # [-1] = prev, then 0xAAAA
            buf = base + 2
            e.uc.mem_write(buf, b'\xAA\xAA' * 400)
            a_fmt = e.put_wstr(fmt) if fmt is not None else 0
            a1 = e.put_wstr(p1) if p1 is not None else 0
            a2 = e.put_wstr(p2) if p2 is not None else 0
            e.call(fn, this=0x1234, args=[buf, a_fmt, a1, a2])
            out.append((fmt, p1, p2, prev, bytes(e.uc.mem_read(base, 2 * 400))))
        res[kind] = out
    bad = 0
    for a, b in zip(res['stock'], res['ours']):
        if a != b:
            bad += 1
            if bad < 8:
                print('DIFF', repr(a[:4]), '\n  stock', a[4][:60].hex(), '\n  ours ', b[4][:60].hex())
    nonempty = sum(1 for a in res['stock'] if a[4][2:4] != b'\x00\x00' and a[0])
    print('FormatSystemMessage: %d of the cases made output' % nonempty)
    print('FormatSystemMessage cases %d  differ %d  raised 0' % (cases, bad))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
