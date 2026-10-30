"""Differential test of Md5 (Init / Update / Final, 0x1014B120 / 0x1014BA90 / 0x1014BB50) against the stock code, and of both against
Python's hashlib: random messages cut into random pieces (the edges of the 64-byte block and of the padding included); the
context after every call is compared too (state, bit count, the block being filled), and the last one must be cleared.
The three register-convention helpers the hook build left stock (Md5::CopyWords 0x1014B150: source eax, destination ecx, count edx;
Md5::DecodeWords 0x1014B190: block eax, words edx, count ecx; Md5::Transform 0x1014B1D0: block eax, context pushed) are compared
too, called directly (a small register shim in front of the stock body; ours is cdecl): in the hook build only Transform is compiled
(the other two are inlined), in the standalone all three are functions of their own.
usage: python tools/port/emu/t_md5_class.py <cases> <seed>   (needs build/bin/Release/NWindowDecomp.dll + .map, or NWD_SA=1 for the standalone)"""
import sys, os, random, hashlib, struct
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from emu import *
import sa

STOCK_INIT, STOCK_UPDATE, STOCK_FINAL = 0x1014B120, 0x1014BA90, 0x1014BB50
STOCK_COPY, STOCK_DECODE, STOCK_TRANSFORM = 0x1014B150, 0x1014B190, 0x1014B1D0


class Md5Emu(Emu):
    def __init__(self, kind):
        super().__init__(kind)
        self.f_copy = self.f_decode = self.f_transform = None
        if kind == 'ours':
            # (the Final of ours has its own static Padding table, in both builds: nothing of the stock image is read)
            sym = lambda p: [v for k, v in self.symbols.items() if k.startswith(p)][0]
            has = lambda p: [v for k, v in self.symbols.items() if k.startswith(p)]
            self.f_init = sym('?Init@Md5@@')
            self.f_update = sym('?Update@Md5@@')
            self.f_final = sym('?Final@Md5@@')
            self.f_transform = (has('?Transform@Md5@@') or [None])[0]
            self.f_copy = (has('?CopyWords@Md5@@') or [None])[0]
            self.f_decode = (has('?DecodeWords@Md5@@') or [None])[0]
        else:
            self.f_init, self.f_update, self.f_final = STOCK_INIT, STOCK_UPDATE, STOCK_FINAL
            self.f_copy, self.f_decode, self.f_transform = STOCK_COPY, STOCK_DECODE, STOCK_TRANSFORM
            # register shims: cdecl (a, b, c) in front of the stock bodies
            self.shim_copy = self.make_shim(b'\x8B\x44\x24\x04\x8B\x4C\x24\x08\x8B\x54\x24\x0C', STOCK_COPY)       # eax=src ecx=dst edx=n
            self.shim_decode = self.make_shim(b'\x8B\x44\x24\x04\x8B\x54\x24\x08\x8B\x4C\x24\x0C', STOCK_DECODE)   # eax=blk edx=words ecx=n
            self.shim_transform = self.make_shim(b'\x8B\x44\x24\x08\xFF\x74\x24\x04', STOCK_TRANSFORM, b'\x83\xC4\x04')  # eax=block, push ctx
        self.ctx = self.alloc(0x58)
        self.digest = self.alloc(32)
        self.buf = self.alloc(0x2000)

    def make_shim(self, pre, target, post=b''):
        a = self.alloc(64)
        code = pre + b'\xE8' + struct.pack('<i', target - (a + len(pre) + 5)) + post + b'\xC3'
        self.uc.mem_write(a, code)
        return a

    def dispatch(self, name):
        raise RuntimeError('unexpected stub call ' + name)

    def snapshot(self):
        return bytes(self.uc.mem_read(self.ctx, 0x58))

    def run(self, pieces):
        out = []
        self.uc.mem_write(self.ctx, b'\xA5' * 0x58)          # garbage: Init must set what matters
        self.call(self.f_init, args=[self.ctx], conv='cdecl')
        out.append(self.snapshot()[:0x18])                   # state and count (the block is not touched by Init)
        for p in pieces:
            self.uc.mem_write(self.buf, p if p else b'\0')
            self.call(self.f_update, args=[self.ctx, self.buf, len(p)], conv='cdecl')
            out.append(self.snapshot())
        self.uc.mem_write(self.digest, b'\xCC' * 32)
        self.call(self.f_final, args=[self.digest, self.ctx], conv='cdecl')
        out.append(self.snapshot())
        dig = bytes(self.uc.mem_read(self.digest, 16))
        tail = bytes(self.uc.mem_read(self.digest + 16, 16))
        return out, dig, tail


def main():
    cases = int(sys.argv[1]) if len(sys.argv) > 1 else 200
    seed = int(sys.argv[2]) if len(sys.argv) > 2 else 1
    random.seed(seed)
    stock, ours = Md5Emu('stock'), Md5Emu('ours')
    edges = [0, 1, 54, 55, 56, 57, 63, 64, 65, 119, 120, 121, 127, 128, 129, 1000]
    bad = 0
    for c in range(cases):
        n = edges[c] if c < len(edges) else random.choice([random.randint(0, 70), random.randint(0, 300), random.randint(0, 2000)])
        msg = bytes(random.getrandbits(8) for _ in range(n))
        pieces = []
        i = 0
        while i < n:
            k = random.choice([1, 3, 7, 31, 63, 64, 65, 100, 1 << 30])
            pieces.append(msg[i:i + k])
            i += k
        if random.random() < 0.3:
            pieces.insert(random.randint(0, len(pieces)), b'')
        a = stock.run(pieces)
        b = ours.run(pieces)
        ref = hashlib.md5(msg).digest()
        ok = a == b and a[1] == ref and a[2] == b'\xCC' * 16 and a[0][-1] == b'\0' * 0x58
        if not ok:
            bad += 1
            print('DIFF case %d (len %d, %d pieces): stock %s ours %s hashlib %s' % (c, n, len(pieces), a[1].hex(), b[1].hex(), ref.hex()))
            for k, (x, y) in enumerate(zip(a[0], b[0])):
                if x != y:
                    print('   step %d: stock %s ours %s' % (k, x.hex(), y.hex()))
                    break
    print('t_md5_class Init/Update/Final cases %d  differ %d  raised 0' % (cases, bad))
    bad2 = direct(stock, ours, cases, seed)
    return 1 if (bad or bad2) else 0


def md5_ref_transform(state, block):
    """RFC 1321's transform in Python: the state (4 words) after one 64-byte block"""
    import math
    K = [int(abs(math.sin(i + 1)) * 2 ** 32) & 0xFFFFFFFF for i in range(64)]
    S = [7, 12, 17, 22] * 4 + [5, 9, 14, 20] * 4 + [4, 11, 16, 23] * 4 + [6, 10, 15, 21] * 4
    x = struct.unpack('<16I', block)
    a, b, c, d = state
    for i in range(64):
        if i < 16:
            f, g = (b & c) | (~b & d), i
        elif i < 32:
            f, g = (d & b) | (~d & c), (5 * i + 1) % 16
        elif i < 48:
            f, g = b ^ c ^ d, (3 * i + 5) % 16
        else:
            f, g = c ^ (b | ~d), (7 * i) % 16
        f = (f + a + K[i] + x[g]) & 0xFFFFFFFF
        a, d, c = d, c, b
        b = (b + (((f << S[i]) | (f >> (32 - S[i]))) & 0xFFFFFFFF)) & 0xFFFFFFFF
    return [(state[0] + a) & 0xFFFFFFFF, (state[1] + b) & 0xFFFFFFFF, (state[2] + c) & 0xFFFFFFFF, (state[3] + d) & 0xFFFFFFFF]


def direct(stock, ours, cases, seed):
    """the helpers the hook build left stock, called directly: stock (through its register shim) vs ours (cdecl) vs a Python reference"""
    rnd = random.Random(seed * 7919 + 1)
    tot = 0
    for name, fs, fo in (('Transform', 'shim_transform', 'f_transform'), ('CopyWords', 'shim_copy', 'f_copy'), ('DecodeWords', 'shim_decode', 'f_decode')):
        if getattr(ours, fo) is None:
            sa.skip('Md5::' + name, 'inlined in the hook build (no function of its own)')
            continue
        bad = raised = 0
        for c in range(cases):
            if name == 'Transform':
                ctx = bytes(rnd.getrandbits(8) for _ in range(0x58))
                blk = bytes(rnd.getrandbits(8) for _ in range(64))
                off = rnd.randint(0, 3)
                res = []
                for e, f in ((stock, stock.shim_transform), (ours, ours.f_transform)):
                    try:
                        e.uc.mem_write(e.buf, bytes(0x100))
                        e.uc.mem_write(e.ctx, ctx)
                        e.uc.mem_write(e.buf + 16 + off, blk)
                        e.call(f, args=[e.ctx, e.buf + 16 + off], conv='cdecl')
                        res.append((bytes(e.uc.mem_read(e.ctx, 0x58)), bytes(e.uc.mem_read(e.buf + 16 + off, 64))))
                    except Exception as ex:
                        raised += 1
                        res.append(str(ex))
                want = md5_ref_transform(struct.unpack('<4I', ctx[:16]), blk)
                got = struct.unpack('<4I', res[0][0][:16]) if not isinstance(res[0], str) else None
                ok = res[0] == res[1] and not isinstance(res[0], str) and list(got) == want and res[0][0][16:] == ctx[16:] and res[0][1] == blk
            else:
                n = rnd.choice([0, 1, 2, 3, 4, 5, 8, 16, 63, 64, rnd.randint(0, 70)])
                src = bytes(rnd.getrandbits(8) for _ in range(96))
                so, do = rnd.randint(0, 3), rnd.randint(0, 3)
                res = []
                for e, f in ((stock, getattr(stock, fs)), (ours, getattr(ours, fo))):
                    try:
                        e.uc.mem_write(e.buf, src)
                        e.uc.mem_write(e.buf + 0x200, bytes([0xAA]) * 128)
                        # CopyWords (source, destination, count) / DecodeWords (block, words, count)
                        e.call(f, args=[e.buf + so, e.buf + 0x200 + do, n], conv='cdecl')
                        res.append(bytes(e.uc.mem_read(e.buf + 0x200, 128)))
                    except Exception as ex:
                        raised += 1
                        res.append(str(ex))
                groups = ((n - 1) >> 2) + 1 if n > 0 else 0
                want = bytearray(bytes([0xAA]) * 128)
                # both copy `groups` groups of four bytes (CopyWords bytewise, DecodeWords as little-endian words: the same bytes on x86)
                want[do:do + 4 * groups] = src[so:so + 4 * groups]
                ok = res[0] == res[1] and res[0] == bytes(want)
            if not ok:
                bad += 1
                if bad <= 3:
                    print('DIFF Md5::%s case %d: stock %s ours %s' % (name, c, res[0] if isinstance(res[0], str) else '', res[1] if isinstance(res[1], str) else ''))
        print('t_md5_class Md5::%s cases %d  differ %d  raised %d' % (name, cases, bad, raised))
        tot += bad + raised
    return tot


if __name__ == '__main__':
    sys.exit(main())
