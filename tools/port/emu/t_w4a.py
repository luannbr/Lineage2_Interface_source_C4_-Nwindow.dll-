"""Differential test of three small functions audited in wave 4 (w4a), stock against ours under unicorn:
NString::Insert (C4 stock 0x1002D4F0: the tail byte count Length*2 - Pos and the growth rule), NStlWString::Find
(0x1003CB90) and HtmlParseColor (0x1003B980). Compared: the return value, the object, and the whole heap the call
touched (so the stock's byte-count overrun past the text is compared too). (The C2 script had 0x10026560 / 0x10031600 /
0x10030340; `htmlcmd` (NCConsole::OnHtmlCommand, C2 0x1004fab0) is stale: win/t_html.py is its C4 successor.)
usage: python tools/port/emu/t_w4a.py insert|find|color <cases> <seed>   (needs build/bin/Release/NWindowDecomp.dll + .map, or NWD_SA=1 for the standalone)"""
import sys, os, random, struct
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import stubs
from stubs import *

stubs.STOCK_FN.clear()      # the C2 services of stubs.py sit at other C4 addresses (and these functions use none of them)
stubs.OURS_FN.clear()
TipEmu.CONSOLE_VA = 0x102C39F4
stubs.FN_POP.update({'GMalloc_Malloc': 8, 'GMalloc_Free': 4, 'wcstoul': 0})
stubs.IMPORT_POP.update({'?appMemmove@@YAPAXPAXPBXH@Z': 0, '?appMemset@@YAXPAXHH@Z': 0})


class W4Emu(TipEmu):
    def __init__(self, kind):
        super().__init__(kind)
        self.next_stub = STUB_BASE + 0x9000
        self.write32(self.gmalloc_vt + 0, self.mkst('GMalloc_Malloc'))
        self.write32(self.gmalloc_vt + 8, self.mkst('GMalloc_Free'))
        self.heap_mark = self.heap_ptr
        # the stock appMemcpy (0x10001790) tests GIsSSE: run its plain path
        for e in self.pe.DIRECTORY_ENTRY_IMPORT:
            for imp in e.imports:
                if imp.name == b'?GIsSSE@@3HA':
                    self.write32(self.read32(imp.address), 0)

    def do_import(self, nm):
        if nm == '?appMemmove@@YAPAXPAXPBXH@Z':
            d, s, n = self.arg(0), self.arg(1), self.arg(2)
            if n > 0:
                self.uc.mem_write(d, bytes(self.uc.mem_read(s, n)))
            return d
        if nm == '?appMemset@@YAXPAXHH@Z':
            d, v, n = self.arg(0), self.arg(1), self.arg(2)
            self.uc.mem_write(d, bytes([v & 0xFF]) * n)
            return None
        return super().do_import(nm)

    def mkst(self, name):
        st = self.next_stub
        self.next_stub += 16
        self.uc.mem_write(st, b'\xC3')
        self.stubs[st] = ('fn:' + name, None)
        return st

    def do_fn(self, nm):
        if nm == 'GMalloc_Malloc':
            n = self.arg(0)
            self.log.append(('Malloc', n))
            return self.alloc(n)
        if nm == 'GMalloc_Free':
            self.log.append(('Free', self.arg(0) - self.heap_mark))
            return None
        if nm == 'wcstoul':     # the C runtime's: stubbed (thread data), the string and the base it gets are logged
            txt, base = self.wstr(self.arg(0)), self.arg(2)
            self.log.append(('wcstoul', txt, self.arg(1), base))
            return (sum(ord(c) * (i + 1) for i, c in enumerate(txt)) * 2654435761) & 0xFFFFFFFF
        return super().do_fn(nm)

    def sym(self, prefix):
        c = [v for k, v in self.symbols.items() if k.startswith(prefix)]
        if len(c) != 1:
            raise RuntimeError('symbol %s: %s' % (prefix, c))
        return c[0]

    def begin(self):
        self.heap_ptr = self.heap_mark
        self.uc.mem_write(self.heap_mark, bytes(0x20000))
        self.log.clear()

    def snapshot(self):
        return bytes(self.uc.mem_read(self.heap_mark, self.heap_ptr - self.heap_mark))


def compare(name, res, cases, show):
    bad = 0
    for i, (a, b) in enumerate(zip(res['stock'], res['ours'])):
        if a != b:
            bad += 1
            if bad <= 5:
                print('DIFF case', i, show(cases[i]))
                print('  stock', str(a)[:600])
                print('  ours ', str(b)[:600])
    print('%s: %d cases, %d differences' % (name, len(cases), bad))
    return bad


def wch(r, alphabet):
    return r.choice(alphabet)


def insert(n, seed):
    r = random.Random(seed)
    cases = []
    for _ in range(n):
        length = r.choice([0, 0, 1, 2, 5, 17, r.randint(0, 80), r.randint(0, 300)])
        ins = r.choice([None, '', 'x', 'ab', 'hello world', ''.join(chr(r.randint(0x41, 0x7a)) for _ in range(r.randint(0, 40))),
                        ''.join(chr(r.randint(0x41, 0x7a)) for _ in range(r.randint(250, 300)))])
        if r.random() < 0.7 or length == 0:
            cap = r.choice([0, length + 1, length + 2, length + r.randint(1, 40), length + r.randint(1, 400), 256])
        else:
            cap = length + r.randint(1, 400)
        if cap <= length:
            cap = length + 1 if length else 0
        if length and cap == 0:
            cap = length + 1
        pos = r.choice([0, length, length + 1, r.randint(0, length + 1), r.randint(0, length + 1)])
        text0 = ''.join(chr(r.randint(0x41, 0x7a)) for _ in range(length))
        noise = bytes(r.randint(1, 255) for _ in range(2 * (cap * 2 + 400)))
        cases.append({'length': length, 'cap': cap, 'ins': ins, 'pos': pos, 'text0': text0, 'noise': noise,
                      'color': r.randint(0, 1 << 24), 'indent': r.randint(0, 9)})
    res = {}
    for kind in ('stock', 'ours'):
        e = W4Emu(kind)
        fn = 0x1002D4F0 if kind == 'stock' else e.sym('?Insert@NString@@QAEHPBGI@Z')
        out = []
        for c in cases:
            e.begin()
            obj = e.alloc(0x14)
            e.write32(obj + 0xC, c['color'])
            e.write32(obj + 0x10, c['indent'])
            if c['cap']:
                size = c['cap'] * 4 + 64
                buf = e.alloc(size)
                e.uc.mem_write(buf, c['noise'][:size])
                e.write_wstr_at(buf, c['text0'])
                e.write32(obj, c['cap'])
                e.write32(obj + 4, buf)
                e.write32(obj + 8, c['length'])
            txt = e.put_wstr(c['ins']) if c['ins'] is not None else 0
            ret = e.call(fn, this=obj, args=[txt, c['pos']])
            out.append((ret & 0xFFFFFFFF, e.snapshot(), list(e.log)))
        res[kind] = out
    return compare('insert', res, cases, lambda c: (c['length'], c['cap'], c['pos'], None if c['ins'] is None else len(c['ins'])))


def find(n, seed):
    r = random.Random(seed)
    cases = []
    for _ in range(n):
        alpha = r.choice(['ab', 'abc', 'a', 'abcdefgh'])
        size = r.choice([0, 1, 3, 7, 8, 9, r.randint(0, 40)])
        text = ''.join(r.choice(alpha) for _ in range(size))
        slen = r.choice([0, 1, 2, 3, 5])
        if r.random() < 0.5 and size >= slen:
            off = r.randint(0, size - slen) if size >= slen else 0
            sub = text[off:off + slen]
        else:
            sub = ''.join(r.choice(alpha) for _ in range(slen))
        count = r.choice([slen, slen, max(0, slen - 1)])
        off = r.choice([0, size, size + 1, r.randint(0, size + 2), r.randint(0, size + 2)])
        cases.append({'text': text, 'sub': sub, 'count': count, 'off': off})
    res = {}
    for kind in ('stock', 'ours'):
        e = W4Emu(kind)
        fn = 0x1003CB90 if kind == 'stock' else e.sym('?Find@NStlWString@@QBEIPBGII@Z')
        out = []
        for c in cases:
            e.begin()
            obj = e.alloc(0x1C)
            size = len(c['text'])
            e.write32(obj + 0x14, size)
            if size < 8:
                e.write_wstr_at(obj + 4, c['text'])
                e.write32(obj + 0x18, 7)
            else:
                buf = e.put_wstr(c['text'])
                e.write32(obj + 4, buf)
                e.write32(obj + 0x18, size)
            sub = e.put_wstr(c['sub'])
            ret = e.call(fn, this=obj, args=[sub, c['off'], c['count']])
            out.append(ret & 0xFFFFFFFF)
        res[kind] = out
    return compare('find', res, cases, lambda c: c)


def color(n, seed):
    r = random.Random(seed)
    cases = [None, '', 'LEVEL', 'level', 'ff0000', 'FF00FF', '00', 'zz', '123456789', 'ffffffffff', '12', ' 12', '-1', 'LEVELX']
    for _ in range(n):
        cases.append(''.join(r.choice('0123456789abcdefABCDEFxgz-+ ') for _ in range(r.randint(0, 12))))
    res = {}
    for kind in ('stock', 'ours'):
        e = W4Emu(kind)
        fn = 0x1003B980 if kind == 'stock' else e.sym('?HtmlParseColor@@YAKPBG@Z')
        st = e.mkst('wcstoul')
        va = 0x101720DA if kind == 'stock' else e.sym('_wcstoul')
        e.uc.mem_write(va, b'\x68' + struct.pack('<I', st) + b'\xC3')    # push stub; ret
        out = []
        for c in cases:
            e.begin()
            p = e.put_wstr(c) if c is not None else 0
            ret = e.call(fn, this=0, args=[p])
            out.append((ret & 0xFFFFFFFF, list(e.log)))
        res[kind] = out
    return compare('color', res, cases, lambda c: c)


STACK_IMPORTS = {
    '?ParseCommand@@YAHPAPBGPBG@Z': 0, '?ParseToken@@YAHAAPBGPAGHH@Z': 0, '??0L2ParamStack@@QAE@H@Z': 4,
    '?PushBack@L2ParamStack@@QAEHPAX@Z': 4, '??1L2ParamStack@@QAE@XZ': 0, '?appStrcmp@@YAHPBG0@Z': 0,
    '?appStrcpy@@YAPAGPAGPBG@Z': 0,
}
stubs.IMPORT_POP.update(STACK_IMPORTS)
stubs.FN_POP.update({'ReqLink': 4, 'ReqTutorial': 4, 'ReqBBS': 4, 'GetControlValue': 4})


class HtmlCmdEmu(W4Emu):
    """NCConsole::OnHtmlCommand: the core parsing functions, L2ParamStack, the three NConsoleWnd requests and the
    page's GetControlValue (vslot 70) are stubs that log; the class chain of the source window is built by hand."""
    CONTROLS = {'c1': 'v1', 'c2': 'v2', 'c3': 'v3', 'e': '', 'zz': None}

    def __init__(self, kind):
        super().__init__(kind)
        self.stacks = {}
        self.marker = (0x101AA530 if kind == 'stock' else NWBASE + 0x1AA530)
        reqs = (('ReqLink', 0x10055050, '?RequestLinkHtml@NConsoleWnd@@'),
                ('ReqTutorial', 0x10055190, '?RequestTutorialLinkHtml@NConsoleWnd@@'),
                ('ReqBBS', 0x10055070, '?RequestBBSWrite@NConsoleWnd@@'))
        for nm, va, sym in reqs:
            a = va if kind == 'stock' else self.sym(sym)
            self.uc.mem_write(a, b'\x68' + struct.pack('<I', self.mkst(nm)) + b'\xC3')
        self.gcv = self.mkst('GetControlValue')

    def do_fn(self, nm):
        if nm in ('ReqLink', 'ReqTutorial', 'ReqBBS'):
            self.log.append((nm, list(self.stacks.get(self.arg(0), ['?']))))
            return 0
        if nm == 'GetControlValue':
            name = self.wstr(self.arg(0))
            self.log.append(('GetControlValue', name))
            v = self.CONTROLS.get(name)
            return self.static_str(v) if v is not None else 0
        return super().do_fn(nm)

    def do_import(self, nm):
        if nm == '?ParseCommand@@YAHPAPBGPBG@Z':
            pp, match = self.arg(0), self.wstr(self.arg(1))
            p = self.read32(pp)
            s = self.wstr(p)
            i = 0
            while i < len(s) and s[i] in ' \t':
                i += 1
            ok = s[i:i + len(match)].lower() == match.lower() and (i + len(match) == len(s) or s[i + len(match)] in ' \t')
            self.log.append(('ParseCommand', s, match, ok))
            if ok:
                i += len(match)
                while i < len(s) and s[i] in ' \t':
                    i += 1
                self.write32(pp, p + 2 * i)
                return 1
            return 0
        if nm == '?ParseToken@@YAHAAPBGPAGHH@Z':
            pp, res, maxlen, esc = self.arg(0), self.arg(1), self.arg(2), self.arg(3)
            p = self.read32(pp)
            s = self.wstr(p)
            i = 0
            while i < len(s) and s[i] in ' \t':
                i += 1
            tok = ''
            if i < len(s) and s[i] == '"':
                i += 1
                while i < len(s) and s[i] != '"':
                    tok += s[i]
                    i += 1
                i += 1
            else:
                while i < len(s) and s[i] not in ' \t':
                    tok += s[i]
                    i += 1
            tok = tok[:maxlen - 1]
            self.write_wstr_at(res, tok)
            self.write32(pp, p + 2 * min(i, len(s)))
            self.log.append(('ParseToken', s, tok, maxlen, esc))
            return 1 if tok else 0
        if nm == '??0L2ParamStack@@QAE@H@Z':
            self.stacks[self.this()] = []
            self.log.append(('PS_new', self.arg(0)))
            return self.this()
        if nm == '?PushBack@L2ParamStack@@QAEHPAX@Z':
            v = self.arg(0)
            self.stacks[self.this()].append(self.wstr(v))
            self.log.append(('push', self.wstr(v)))
            return 1
        if nm == '??1L2ParamStack@@QAE@XZ':
            self.log.append(('PS_del', len(self.stacks.get(self.this(), []))))
            return None
        if nm == '?appStrcmp@@YAHPBG0@Z':
            a, b = self.wstr(self.arg(0)), self.wstr(self.arg(1))
            self.log.append(('appStrcmp', a, b))
            return 0 if a == b else (1 if a > b else 0xFFFFFFFF)
        if nm == '?appStrcpy@@YAPAGPAGPBG@Z':
            d, s = self.arg(0), self.wstr(self.arg(1))
            self.log.append(('appStrcpy', s))
            self.write_wstr_at(d, s)
            return d
        return super().do_import(nm)


def htmlcmd(n, seed):
    r = random.Random(seed)
    names = ['c1', 'c2', 'c3', 'zz', 'e', '"c 1"']
    cases = []
    for _ in range(n):
        kind = r.choice(['Link', 'Link', 'Write', 'Write', 'Write', 'write', 'other', ''])
        sp = r.choice([' ', '  ', ' '])
        if kind.lower() == 'link':
            toks = [r.choice(['go', 'a_b', '"x y"', '', '10'])] if r.random() < 0.8 else []
            cmd = kind + sp + sp.join(toks)
            chain = r.choice([0, 1, 2, 3, 4])
        elif kind.lower() == 'write':
            board = r.choice(['4', '5', '7', '1', '3', '12', '4 ', '47'])
            rest = [board, r.choice(['a', 'x1']), r.choice(['b', '-'])]
            rest += [r.choice(names) for _ in range(r.randint(0, 4))]
            cmd = kind + sp + sp.join(rest[:r.randint(0, len(rest))] if r.random() < 0.3 else rest)
            chain = r.choice([0, 1, 2, 3, 4])
        else:
            cmd = kind + ' foo'
            chain = 0
        cases.append({'cmd': cmd, 'nosrc': r.random() < 0.08 and kind.lower() == 'write', 'chain': chain})
    res = {}
    for kind in ('stock', 'ours'):
        e = HtmlCmdEmu(kind)
        fn = 0x1004fab0 if kind == 'stock' else e.sym('?OnHtmlCommand@NCConsole@@')
        out = []
        for c in cases:
            e.begin()
            e.stacks.clear()
            vt = e.alloc(0x200)
            e.write32(vt + 0x118, e.gcv)
            src = e.alloc(0x40)
            e.write32(src, vt)
            # the class chain: marker at depth chain-1 (0: no class, 1: no marker, 2: marker first, 3: second, 4: third)
            chain = c['chain']
            nodes = []
            if chain >= 1:
                depth = {1: 0, 2: 1, 3: 2, 4: 3}[chain]
                for d in range(depth):
                    nodes.append(e.alloc(0x40))
                if chain >= 2:
                    nodes.append(e.marker)
                nodes.append(e.alloc(0x40))
                for a, b in zip(nodes, nodes[1:]):
                    e.write32(a + 0x28, b)
                e.write32(nodes[-1] + 0x28, 0)
                e.write32(src + 0x24, nodes[0])
            cmd = e.put_wstr(c['cmd'])
            ret = e.call(fn, this=0x1234, args=[cmd, 0 if c['nosrc'] else src])
            out.append((ret & 0xFFFFFFFF, list(e.log)))
        res[kind] = out
    return compare('htmlcmd', res, cases, lambda c: c)


if __name__ == '__main__':
    what = sys.argv[1]
    n, seed = int(sys.argv[2]), int(sys.argv[3])
    {'insert': insert, 'find': find, 'color': color, 'htmlcmd': htmlcmd}[what](n, seed)
