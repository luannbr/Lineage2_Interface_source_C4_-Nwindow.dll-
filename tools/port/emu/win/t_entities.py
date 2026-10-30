# Differential test of NHtmlParser::ReplaceEntities (stock 0x100449e0 vs ours under unicorn): the eight HTML entities and the six
# numbered macros (&$ sysstring, &% castle name, &^ castle location, &* castle description, &@ NPC name, &# item name).
# The string class (NStlWString: Find / Erase / Insert / Substr) runs as the stock code on one side and as our compiled code on the
# other; the console's getters and the engine's GetNpcName / appAtoi are stubs that log. The result string and the log are compared.
#   python t_entities.py [cases] [seed]
import sys, os, random, struct
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from gw import *
import gw
import sa
import stubs
import pefile
from unicorn.x86_const import *

FUNC = 0x100449e0
CALLEES = {0x1006b040: ('CastleName', 4, 0), 0x1006b090: ('CastleLoc', 4, 0), 0x1006b0e0: ('CastleDesc', 4, 0),
           0x1007d680: ('ItemName', 4, 0)}
OURS_SYMS = {0x1006b040: '?GetCastleName@NConsoleWnd@@QAEPBGH@Z', 0x1006b090: '?GetCastleLocation@NConsoleWnd@@QAEPBGH@Z',
             0x1006b0e0: '?GetCastleDescription@NConsoleWnd@@QAEPBGH@Z', 0x1007d680: '?GetItemName@NConsoleWnd@@QAEPBGH@Z'}
stubs.IMPORT_POP.update({'?appAtoi@@YAHPBG@Z': 0, '?GetNpcName@FL2GameData@@QAEPAGH@Z': 4})
SYSSTR_BASE = 0x9871C


class EW(GW):
    def __init__(self, kind):
        GW.__init__(self, kind, CALLEES, OURS_SYMS)
        if kind == 'ours' and not sa.ENABLED:
            # the stock image's data our code reads through g_nwBase (the mark and entity tables)
            # (standalone: ours reads its own tables, which gen_globals.py --check proves equal to the stock's)
            pe = pefile.PE(stubs.STOCK if hasattr(stubs, 'STOCK') else os.path.join(ROOT, 'orig', 'NWindow.dll'))
            for s in pe.sections:
                nm = s.Name.rstrip(b'\0').decode()
                if nm in ('.data', '.rdata'):
                    self.uc.mem_write(NWBASE + s.VirtualAddress, s.get_data())
            self.write32(self.glob(GCONSOLE), self.console)
            # the tables of string pointers hold stock addresses, which are not valid in our image: point them at copies
            mem = pe.get_memory_mapped_image()
            base = pe.OPTIONAL_HEADER.ImageBase

            def stock_wstr(va):
                out = []
                for k in range(300):
                    c = struct.unpack('<H', bytes(mem[va - base + 2 * k:va - base + 2 * k + 2]))[0]
                    if c == 0:
                        break
                    out.append(chr(c))
                return ''.join(out)
            for va in range(0x102369E4, 0x10236A38, 4):
                ptr = struct.unpack('<I', bytes(mem[va - base:va - base + 4]))[0]
                self.write32(NWBASE + (va - 0x10000000), self.put_wstr(stock_wstr(ptr)))
        for i in range(0, 400):
            self.fs_set(self.gdata + SYSSTR_BASE + 12 * i, 'S%d' % i)
        self.rets['CastleName'] = lambda e, t, a: e.static_str('CN%d' % a[0]) if a[0] % 4 else 0
        self.rets['CastleLoc'] = lambda e, t, a: e.static_str('CL%d' % a[0]) if a[0] % 5 else 0
        self.rets['CastleDesc'] = lambda e, t, a: e.static_str('CD%d' % a[0]) if a[0] % 3 else 0
        self.rets['ItemName'] = lambda e, t, a: e.static_str('Item%d' % a[0]) if a[0] % 6 else 0

    def do_import(self, nm):
        if nm == '?appAtoi@@YAHPBG@Z':
            s = self.wstr(self.arg(0))
            k = 0
            while k < len(s) and s[k] in ' \t':
                k += 1
            sign = 1
            if k < len(s) and s[k] in '+-':
                sign = -1 if s[k] == '-' else 1
                k += 1
            n = 0
            while k < len(s) and s[k].isdigit() and s[k].isascii():
                n = n * 10 + int(s[k])
                k += 1
            self.log.append(('appAtoi', s))
            return (sign * n) & 0xFFFFFFFF
        if nm == '?GetNpcName@FL2GameData@@QAEPAGH@Z':
            i = self.rds32(self.reg(UC_X86_REG_ESP) + 4)
            self.log.append(('GetNpcName', i))
            return self.static_str('NPC%d' % i) if i % 7 else 0
        return GW.do_import(self, nm)


def put_stl(e, s):
    """an NStlWString (VC 7.1: +4 the 16-byte buffer or the heap pointer, +0x14 Size, +0x18 Res)"""
    p = e.alloc(0x1C)
    e.uc.mem_write(p, bytes(0x1C))
    n = len(s)
    raw = s.encode('utf-16-le') + b'\0\0'
    if n < 8:
        e.uc.mem_write(p + 4, raw)
        e.write32(p + 0x14, n)
        e.write32(p + 0x18, 7)
    else:
        cap = n + 5
        buf = e.alloc(2 * cap + 2)
        e.uc.mem_write(buf, bytes(2 * cap + 2))
        e.uc.mem_write(buf, raw)
        e.write32(p + 4, buf)
        e.write32(p + 0x14, n)
        e.write32(p + 0x18, cap)
    return p


def get_stl(e, p):
    n = e.read32(p + 0x14)
    res = e.read32(p + 0x18)
    ptr = e.read32(p + 4) if res >= 8 else p + 4
    return bytes(e.uc.mem_read(ptr, 2 * n)).decode('utf-16-le', 'replace'), n, res >= 8


ENT = ['&nbsp;', '&apos;', '&quot;', '&amp;', '&lt;', '&gt;', '&#160;', '&#10;']
MARKS = ['&$', '&%', '&^', '&*', '&@', '&#']
WORDS = ['hello', ' ', 'World', 'x', '<b>', '</b>', 'The lord of ', 'a&b', '& ', ';', '&;', '&1;', '$5;', 'END']


def rand_text(r):
    out = []
    for _ in range(r.randint(0, 9)):
        x = r.random()
        if x < 0.25:
            out.append(r.choice(WORDS))
        elif x < 0.45:
            out.append(r.choice(ENT))
        elif x < 0.9:
            m = r.choice(MARKS)
            body = r.choice(['1', '2', '3', '5', '7', '12', '20', '35', '100', '399', '0', '', 'abc', '4x', ' 6', '007'])
            if r.random() < 0.88:
                out.append(m + body + ';')
            else:
                out.append(m + body)   # no ';'
        else:
            out.append(r.choice(MARKS))
    s = ''.join(out)
    if r.random() < 0.15:
        s = s * r.choice([2, 3])
    return s


def run(e, A, c):
    mark = len(e.log)
    st = put_stl(e, c['text'])
    this = e.alloc(0x100)
    e.uc.mem_write(this, bytes(0x100))
    e.call(A[0], this=this, args=[st])
    text, n, heap = get_stl(e, st)
    return dict(text=text, n=n, log=e.log[mark:])


def main():
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 300
    seed = int(sys.argv[2]) if len(sys.argv) > 2 else 1
    es = EW('stock')
    eo = EW('ours')
    lm = log_map()
    es.heap_mark, eo.heap_mark = es.heap_ptr, eo.heap_ptr
    As = (FUNC, 0)
    Ao = (lm[FUNC], 0)
    r = random.Random(seed)
    cases = [dict(text=rand_text(r)) for _ in range(n)]
    cases += [dict(text=t) for t in ['', 'x', '&$1;', '&@3;', '&#2;&@2;', '&*3;&^4;&%5;', '&$0;ok', '&$1', '&nbsp;&nbsp;', '&&&$1;;;']]
    total = compare('ReplaceEntities', es, eo, run, cases, As, Ao)
    print('TOTAL problems', total)


if __name__ == '__main__':
    main()
