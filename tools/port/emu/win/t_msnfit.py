# Differential test of the messenger's text fitters MsnFitLine (0x10168A10, cdecl: Text, Out, MaxWidth, EllipsisReserve) and MsnFitText
# (0x10168B10, cdecl: Text, Out, MaxWidth, MaxHeight) against the stock (stock vs ours under unicorn). The canvas (the global at 0x102C39F8)
# is a fake whose L2Fonts[0] (+0x13C) is a fake font: its GetChar (vslot 28) answers a glyph (X, Width, Y, Height) or NULL for the
# characters the font does not have. NCGDevice::GetTextExtent / GetCharSize are the logging stubs of tip4.py. Compared: the result, the
# whole output buffer (also past the terminator), the calls made.
#   python t_msnfit.py [cases] [seed] [line|text]
import sys, os, random, struct
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from tip4 import *

FUNCS = {'line': 0x10168A10, 'text': 0x10168B10}
OUTSIZE = 0x400
CANVAS_VA = 0x102C39F8
CHARS = 'abcdefgh ijklmnopqrstuvwxyz ABCDEFGH 0123456789 .,;:!?é中文กิ'


class MW(TW):
    def thiscall(self, nm, t, a):
        if nm == 'GetChar':
            c = a[0] & 0xFFFF
            self.log.append(('GetChar', c))
            return self.glyphs.get(c, 0)
        return TW.thiscall(self, nm, t, a)


def world(kind):
    e = MW(kind, extra_callees={})
    canvas = e.alloc(0x200)
    e.uc.mem_write(canvas, bytes(0x200))
    font = e.alloc(0x40)
    vt = e.alloc(0x400)
    e.uc.mem_write(vt, bytes(0x400))
    stub = STUB_BASE + 0xA800
    e.uc.mem_write(stub, bytes([0xC3]))
    e.stubs[stub] = ('cb:GetChar', None)
    e.cb['GetChar'] = (0, 4, 0)
    e.write32(vt + 28 * 4, stub)
    e.write32(font, vt)
    e.write32(canvas + 0x13C, font)
    e.write32(e.glob(CANVAS_VA), canvas)
    e.glyphs = {}
    for c in set(CHARS) | {'\n'}:
        o = ord(c)
        if o % 11 == 5 or c == '\n':       # the font has no glyph for these (a newline included)
            continue
        g = e.alloc(16)
        e.write32(g, 0)
        e.write32(g + 4, 4 + o % 9)        # Width
        e.write32(g + 8, 0)
        e.write32(g + 12, 13)
        e.glyphs[o] = g
    return e


def rand_text(r):
    n = r.choice([0, 1, 2, 5, 12, 30, 60, 120, 200])
    s = ''.join(r.choice(CHARS + '\n' * r.choice([0, 0, 1, 3])) for _ in range(n))
    return s


def rand_case(r):
    return dict(text=rand_text(r), width=r.choice([0, 1, 5, 20, 60, 100, 128, 200, 300, 0x7FFF]),
                reserve=r.choice([0, 0, 12, 12, -3, 5, 1]), height=r.choice([0, 12, 13, 26, 39, 70, 130, 1000]),
                pre=r.choice(['', '', 'AB']))


def run_line(e, A, c):
    mark = len(e.log)
    text = e.put_wstr(c['text'])
    out = e.alloc(OUTSIZE)
    e.uc.mem_write(out, struct.pack('<H', 0x7777) * (OUTSIZE // 2))
    r = e.call(A[0], args=[text, out, c['width'] & 0xFFFFFFFF, c['reserve'] & 0xFFFFFFFF], conv='cdecl')
    return dict(ret=r, out=bytes(e.uc.mem_read(out, OUTSIZE)), log=e.log[mark:])


def run_text(e, A, c):
    mark = len(e.log)
    text = e.put_wstr(c['text'])
    out = e.alloc(OUTSIZE)
    e.uc.mem_write(out, struct.pack('<H', 0x7777) * (OUTSIZE // 2))
    e.write_wstr_at(out, c['pre'])
    e.call(A[0], args=[text, out, c['width'] & 0xFFFFFFFF, c['height'] & 0xFFFFFFFF], conv='cdecl')
    return dict(out=bytes(e.uc.mem_read(out, OUTSIZE)), log=e.log[mark:])


def main():
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 300
    seed = int(sys.argv[2]) if len(sys.argv) > 2 else 1
    which = sys.argv[3].split(',') if len(sys.argv) > 3 else ['line', 'text']
    es = world('stock')
    eo = world('ours')
    lm = log_map()
    es.heap_mark, eo.heap_mark = es.heap_ptr, eo.heap_ptr
    r = random.Random(seed)
    total = 0
    for f in which:
        cases = [rand_case(r) for _ in range(n)]
        va = FUNCS[f]
        total += compare(f, es, eo, run_line if f == 'line' else run_text, cases, (va,), (lm[va],))
    print('TOTAL problems', total)


if __name__ == '__main__':
    main()
