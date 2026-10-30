# Differential test of the HTML page tree and the board viewer of the C2 NWindow.dll (written by the w2html lane): the stock function and ours run on identical object graphs under unicorn (tools/port/emu); the graphs, the globals and the device call logs are compared. Needs build/bin/Release/NWindowDecomp.dll (+ .map) and `pip install unicorn`.
# python tools/port/emu/t_html_parse.py <n> <seed> {body|td|table}
import os, sys, random, re
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from html_emu import *

TEXTS = ['hello', 'A long piece of text here', '', 'x', 'Quest 5', 'Z' * 12]
COLORS = ['', '', '\x1eff0000\x1ehello', '\x1e00ff00\x1ea\x1e0000ff\x1ex']
ALIGNS = ['CENTER', 'right', 'Left', 'bogus', 'center']
VALIGNS = ['TOP', 'bottom', 'CENTER', 'x']
NUMS = ['0', '10', '50', '30%', '100%', 'abc', '7', '120', '5%']
FILES = ['pg1.htm', 'link/a.htm', '']
ACTIONS = ['bypass -h x', 'act1', '']
MSGS = ['', '123', 'msg']
COLORVALS = ['FF0000', '00FF00', 'LEVEL', '123456']
ETYPES = ['normal', 'password', 'number', 'date', 'time', 'zzz']
LISTS = ['a;b;c', '']


def tag(t, op, text='', colors='', attrs=()):
    return {'tag': t, 'open': op, 'text': text, 'colors': colors, 'attrs': list(attrs)}


def gen_content(r, depth, tags, ctx):
    for _ in range(r.randint(0, 6)):
        x = r.random()
        if x < 0.3:
            tags.append(tag(1, 1, r.choice(TEXTS), r.choice(COLORS)))
        elif x < 0.37:
            tags.append(tag(15, 1))
        elif x < 0.42:
            tags.append(tag(53, 1))
        elif x < 0.48:
            at = [(2, r.choice(FILES))] + ([(4, r.choice(NUMS))] if r.random() < 0.6 else []) + ([(1, r.choice(NUMS))] if r.random() < 0.6 else [])
            tags.append(tag(5, 1, attrs=sorted(at)))
        elif x < 0.54:
            at = [(i, r.choice(NUMS if i in (4, 6) else FILES)) for i in (1, 2, 3, 4, 5, 6) if r.random() < 0.5]
            tags.append(tag(49, 1, attrs=at))
        elif x < 0.58:
            at = [(i, r.choice(NUMS if i in (1, 3) else FILES)) for i in (1, 2, 3) if r.random() < 0.6]
            tags.append(tag(50, 1, attrs=at))
        elif x < 0.64:
            at = [(i, r.choice(ETYPES) if i == 4 else r.choice(NUMS if i in (2, 3, 6) else FILES)) for i in (1, 2, 3, 4, 5, 6) if r.random() < 0.6]
            tags.append(tag(51, 1, attrs=at))
        elif x < 0.68:
            at = [(i, r.choice(NUMS if i == 4 else LISTS if i == 1 else FILES)) for i in (1, 2, 3, 4) if r.random() < 0.6]
            tags.append(tag(52, 1, attrs=at))
        elif x < 0.74:
            tags.append(tag(18, 1))
            gen_content(r, depth + 1, tags, ctx) if depth < 2 else None
            tags.append(tag(18, 0))
        elif x < 0.84:
            at = [(i, r.choice(ACTIONS if i in (1, 2, 3) else MSGS)) for i in (1, 2, 3, 4) if r.random() < 0.5]
            tags.append(tag(14, 1, attrs=at))
            tags.append(tag(1, 1, r.choice(TEXTS), r.choice(COLORS)))
            tags.append(tag(14, 0))
        elif x < 0.88:
            tags.append(tag(20, 1))
            tags.append(tag(1, 1, r.choice(TEXTS)))
            tags.append(tag(20, 0))
        elif depth < 2:
            gen_table(r, depth + 1, tags)


def gen_table(r, depth, tags):
    at = [(i, r.choice(COLORVALS) if i == 3 else r.choice(NUMS)) for i in (3, 4, 8, 9, 10, 11) if r.random() < 0.5]
    tags.append(tag(40, 1, attrs=at))
    for _ in range(r.randint(0, 3)):
        tags.append(tag(41, 1))
        for _ in range(r.randint(0, 3)):
            at = [(1, r.choice(ALIGNS))] if r.random() < 0.5 else []
            at += [(i, r.choice(VALIGNS) if i == 7 else r.choice(NUMS)) for i in (5, 6, 7, 8) if r.random() < 0.4]
            tags.append(tag(42, 1, attrs=sorted(at)))
            gen_content(r, depth, tags, None)
            tags.append(tag(42, 0))
        tags.append(tag(41, 0))
    tags.append(tag(40, 0))


def gen_doc(r, kind):
    tags = []
    if kind == 'body':
        tags.append(tag(44, 1))
        gen_content(r, 0, tags, None)
        tags.append(tag(44, 0))
    elif kind == 'td':
        at = [(1, r.choice(ALIGNS))] if r.random() < 0.5 else []
        at += [(i, r.choice(VALIGNS) if i == 7 else r.choice(NUMS)) for i in (5, 6, 7, 8) if r.random() < 0.5]
        tags.append(tag(42, 1, attrs=sorted(at)))
        gen_content(r, 0, tags, None)
        tags.append(tag(42, 0))
    elif kind == 'table':
        gen_table(r, 0, tags)
    tags.append(tag(2, 1))   # a trailing unknown tag, never read
    return tags


def put_tag(e, t):
    p = e.newobj(0x60)
    e.write32(p, t['tag'])
    e.uc.mem_write(p + 4, bytes([t['open']]))
    e.put_wstring(p + 8, t['text'])
    e.put_wstring(p + 0x44, t['colors'])
    ids = [a[0] for a in t['attrs']]
    if ids:
        buf = e.alloc(2 * len(ids) + 8)
        for i, v in enumerate(ids):
            e.uc.mem_write(buf + 2 * i, struct.pack('<H', v))
        e.write32(p + 0x28, buf)
        e.write32(p + 0x2C, buf + 2 * len(ids))
        e.write32(p + 0x30, buf + 2 * len(ids))
        vb = e.alloc(0x1C * len(ids) + 8)
        for i, a in enumerate(t['attrs']):
            e.put_wstring(vb + 0x1C * i, a[1])
        e.write32(p + 0x38, vb)
        e.write32(p + 0x3C, vb + 0x1C * len(ids))
        e.write32(p + 0x40, vb + 0x1C * len(ids))
    return p


def reset_globals(e):
    for va, n in ((0x102C943C, 12), (0x102C89C4, 12), (0x102C8098, 512), (0x102C8298, 512)):      # GHtmlAlignStack, GHtmlVAlignStack, GHtmlLinkTarget, GHtmlLinkMsg (C2: 0x101A9FFC ..)
        e.uc.mem_write(e.G(va), bytes(n))


def dump_globals(e):
    out = {}
    for nm, va in (('align', 0x102C943C), ('valign', 0x102C89C4)):
        out[nm] = e.read_tarray_i(e.G(va))
    out['target'] = e.wstr(e.G(0x102C8098), 256)
    out['msg'] = e.wstr(e.G(0x102C8298), 256)
    return out


def ctor_block(e, parent, viewer, font):
    p = e.alloc(0x870)
    e.uc.mem_write(p, bytes([GARBAGE]) * 0x870)
    if e.kind == 'stock':
        e.call(0x1003DCF0, this=p, args=[parent, viewer, font])
    else:
        e.call(e.symbols['??0NHtmlBlock@@QAE@PAVNHtmlObject@@PAVNCHtmlViewer@@H@Z'], this=p, args=[parent, viewer, font])
    return p


def run_case(e, doc, kind, font, border):
    e.heap_ptr = e.heap_mark
    e.log.clear()
    reset_globals(e)
    # remember the heap start of this case so that pointer values in garbage stay comparable
    out = {}
    try:
        tp = [put_tag(e, t) for t in doc]
        arr = e.tarray(tp)
        itp = e.alloc(8)
        e.write32(itp, arr)
        e.log.clear()
        if kind == 'table':
            root = e.alloc(0x8BC)
            e.uc.mem_write(root, bytes([GARBAGE]) * 0x8BC)
            if e.kind == 'stock':
                e.call(0x1003DFB0, this=root, args=[0, 0x5555, font])
                e.call(0x1003E110, this=root, args=[itp])
            else:
                e.call(e.symbols['??0NHtmlTable@@QAE@PAVNHtmlObject@@PAVNCHtmlViewer@@H@Z'], this=root, args=[0, 0x5555, font])
                e.call(e.fn('table_parse'), this=root, args=[itp])
        else:
            root = ctor_block(e, 0, 0x5555, font)
            e.call(e.fn('block_parse'), this=root, args=[itp, border])
        out['it'] = (e.read32(itp) - arr) // 4
        out['tree'] = dump_obj(e, root)
        out['glob'] = dump_globals(e)
        out['log'] = [x for x in e.log if x[0] not in ('Malloc',)]
    except Exception as ex:
        out['exc'] = re.sub(r'eip=[0-9a-f]+', 'eip=X', str(ex))[:80]
    return out


def main(n, seed, kind):
    r = random.Random(seed)
    cases = [(gen_doc(r, kind), kind, r.choice([0, 1, 2]), r.choice([0, 1])) for _ in range(n)]
    res = {}
    for k in ('stock', 'ours'):
        e = HEmu(k)
        e.heap_mark = e.heap_ptr
        res[k] = [run_case(e, *c) for c in cases]
    bad = 0
    exc = 0
    for i, (a, b) in enumerate(zip(res['stock'], res['ours'])):
        if 'exc' in a or 'exc' in b:
            exc += 1
        if a != b:
            bad += 1
            if bad <= 2:
                print('case', i)
                print(' stock', str(a)[:1800])
                print(' ours ', str(b)[:1800])
    print('t_html_parse', kind, 'cases %d  differ %d  raised %d' % (n, bad, exc))
    return 1 if bad or exc else 0


if __name__ == '__main__':
    sys.exit(main(int(sys.argv[1]), int(sys.argv[2]), sys.argv[3]))
