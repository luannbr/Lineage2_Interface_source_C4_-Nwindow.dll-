# Differential test of the HTML page tree and the board viewer of the C2 NWindow.dll (written by the w2html lane): the stock function and ours run on identical object graphs under unicorn (tools/port/emu); the graphs, the globals and the device call logs are compared. Needs build/bin/Release/NWindowDecomp.dll (+ .map) and `pip install unicorn`.
# python tools/port/emu/t_html_layout.py <n> <seed> {layout|arrange|draw|height|release} {table|block|item} [thai]
import os, sys, random, traceback, re
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from html_emu import *

WORDS = ['A', 'hello', 'Quest', 'a long line of text that wraps', 'xx yy', 'The Dwarven Blacksmith says: welcome', '',
         'Z' * 20, 'ok']


def rand_text(r):
    return ' '.join(r.choice(WORDS) for _ in range(r.randint(1, 4)))


# ---- recipes --------------------------------------------------------------------------------------------
def r_item(r, mode_ok=True):
    t = r.choice([1, 1, 1, 1, 15, 5, 49, 51])
    return {'k': 'item', 'type': t, 'text': rand_text(r), 'colors': r.choice(['', '', '\x1eff0000\x1eQuest', '\x1e00ff00\x1ehello\x1e0000ff\x1ea']),
            'haln': r.choice([0, 0, 1, 2, 3]), 'valn': r.choice([0, 1, 2, 3]), 'mode': r.choice([0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8]),
            'strs': [r.choice(['', 'act', 'a long action string for the link', 'Bn']) for _ in range(5)], 'length': r.randint(0, 9), 'etype': r.randint(0, 5), 'tex': r.choice([0, 0x1234]),
            'font': r.choice([0, 0, 1, 2]), 'minw': r.randint(0, 30), 'w': r.randint(0, 80), 'h': r.randint(0, 40),
            'off': r.randint(0, 50), 'tail': r.randint(0, 50), 'parentoff': 0}


def r_block(r, depth, cell=False, border=0, padding=1):
    ch = []
    for _ in range(r.randint(0, 4)):
        x = r.random()
        if x < 0.7 or depth >= 2:
            ch.append(r_item(r))
        else:
            ch.append(r_table(r, depth + 1))
    return {'k': 'block', 'type': 0x2a if cell else 0x2c, 'haln': r.choice([0, 0, 1, 2, 3]), 'valn': r.choice([0, 1, 2, 3]),
            'minw': r.randint(0, 40), 'w': r.randint(0, 100), 'h': r.randint(0, 60), 'font': r.choice([0, 1]),
            'fixw': r.choice([-1, 30, 0]), 'wval': r.choice([0, 0, 20, 50]), 'pct': r.choice([0, 0, 1]),
            'bborder': r.choice([0, 1]) if cell else 0, 'mode': 0, 'children': ch, 'padding': padding, 'border': border,
            'spacing': 2, 'offy': r.randint(-5, 9)}


def r_table(r, depth):
    nr = r.randint(0, 3)
    nc = r.randint(1, 3)
    border = r.choice([0, 0, 1, 2, 3])
    padding = r.choice([0, 1, 1, 2])
    spacing = r.choice([0, 2, 2, 3])
    rows = []
    for _ in range(nr):
        rows.append([r_block(r, depth, True, border, padding) for _ in range(r.randint(0, nc))])
    ncols = max([len(x) for x in rows] + [0])
    t = {'k': 'table', 'type': 0x28, 'haln': r.choice([0, 1, 2, 3]), 'valn': r.choice([0, 1, 2, 3]),
         'minw': r.randint(0, 40), 'w': r.randint(0, 150), 'h': r.choice([0, 0, 0, 40, 80, 200]), 'font': r.choice([0, 1]),
         'fixw': -1, 'wval': r.choice([0, 0, 60, 120]), 'pct': r.choice([0, 0, 1]), 'bborder': 0, 'mode': 0,
         'rows': rows, 'border': border, 'padding': padding, 'spacing': spacing,
         'colw': [r.randint(0, 60) for _ in range(ncols)], 'colminw': [r.randint(0, 30) for _ in range(ncols)],
         'collw': r.choice([[], [r.randint(0, 50) for _ in range(ncols)]]), 'bg': r.choice([0, 1]), 'bgc': 0xFF336699,
         'numrows': nr, 'curcol': 0, 'offy': r.randint(-5, 9)}
    return t


def mat(e, rc, parent=0, viewer=0x5555):
    """build the raw object graph of a recipe, return the pointer"""
    k = rc['k']
    if k == 'item':
        p = e.newobj(0xFC)
        e.write32(p, e.vt('item'))
        e.write32(p + 4, rc['type'])
        e.write32(p + 0x34, rc['haln'])
        e.write32(p + 0x38, rc['valn'])
        e.write32(p + 0x2C, rc['minw'])
        e.write32(p + 0x24, rc['w'])
        e.write32(p + 0x28, rc['h'])
        e.write32(p + 0x30, 0x11)
        e.write32(p + 0x20, 0x22)
        e.write32(p + 0x4C, rc['font'])
        e.write32(p + 0x50, rc['mode'])
        for o, sv in zip((0x54, 0x70, 0x8C, 0xA8, 0xC4), rc['strs']):
            e.put_wstring(p + o, sv)
        e.write32(p + 0xE0, rc['length'])
        e.write32(p + 0xEC, rc['etype'])
        e.write32(p + 0xE8, rc['tex'])
        e.write32(p + 0xE4, e.put_wstr(rc['colors']))
        e.write32(p + 0xF0, e.put_wstr(rc['text']))
        e.write32(p + 0xF4, rc['off'])
        e.write32(p + 0xF8, rc['tail'])
        e.write32(p + 0x18, parent)
        e.write32(p + 0x1C, viewer)
        return p
    size = 0x870 if k == 'block' else 0x8BC
    p = e.newobj(size)
    e.write32(p, e.vt(k))
    e.write32(p + 4, rc['type'])
    e.write32(p + 0x34, rc['haln'])
    e.write32(p + 0x38, rc['valn'])
    e.write32(p + 0x2C, rc['minw'])
    e.write32(p + 0x24, rc['w'])
    e.write32(p + 0x28, rc['h'])
    e.write32(p + 0x3C, rc['padding'])
    e.write32(p + 0x40, rc['spacing'])
    e.write32(p + 0x44, rc['border'])
    e.write32(p + 0x48, rc['offy'])
    e.write32(p + 0x4C, rc['font'])
    e.write32(p + 0x50, rc['fixw'])
    e.write32(p + 0x54, rc['wval'])
    e.write32(p + 0x58, rc['pct'])
    e.write32(p + 0x68, rc['bborder'])
    e.write32(p + 0x6C, rc['mode'])
    e.write32(p + 0x18, parent)
    e.write32(p + 0x1C, viewer)
    if k == 'block':
        e.write_tarray(p + 0x5C, [mat(e, c, p, viewer) for c in rc['children']])
    else:
        e.write_tarray(p + 0x5C, [])
        e.write32(p + 0x870, rc['numrows'])
        e.write32(p + 0x874, rc['curcol'])
        rowptrs = []
        for row in rc['rows']:
            ra = e.newobj(12)
            e.write_tarray(ra, [mat(e, c, p, viewer) for c in row])
            rowptrs.append(ra)
        e.write_tarray(p + 0x878, rowptrs)
        e.write_tarray(p + 0x884, rc['colminw'])
        e.write_tarray(p + 0x890, rc['collw'])
        e.write_tarray(p + 0x89C, rc['colw'])
        e.write_tarray(p + 0x8A8, [])
        e.write32(p + 0x8B4, rc['bgc'])
        e.write32(p + 0x8B8, rc['bg'])
    return p


def run_case(e, rc, w, h, which):
    e.heap_ptr = e.heap_mark
    e.log.clear()
    root = mat(e, rc)
    kind = rc['k']
    out = {}
    try:
        e.log.clear()
        if which == 'layout':
            e.call(e.fn(kind + '_layout'), this=root, args=[w, h])
            out['layout'] = dump_obj(e, root)
            out['log'] = [x for x in e.log if x[0] not in ('Malloc',)]
        elif which == 'arrange':
            e.call(e.fn(kind + '_layout'), this=root, args=[w, h])
            e.log.clear()
            e.call(e.fn(kind + '_arrange'), this=root, args=[w + 3, h + 5])
            out['arr'] = dump_obj(e, root)
        elif which == 'draw':
            dev = e.newobj(0x80)
            e.call(e.fn(kind + '_draw'), this=root, args=[dev])
            out['log'] = [x for x in e.log]
            out['arr'] = dump_obj(e, root)
        elif which == 'height':
            out['h'] = e.call(e.fn('content_height'), this=root)
        elif which == 'release':
            e.call(e.fn(kind + '_release'), this=root)
            out['log'] = [x for x in e.log if x[0] in ('Free',)]
            out['arr'] = dump_obj(e, root) if kind == 'item' else None
    except Exception as ex:
        out['exc'] = re.sub(r'eip=[0-9a-f]+', 'eip=X', str(ex))[:80]
    return out


def main(n, seed, which, top):
    r = random.Random(seed)
    cases = []
    for _ in range(n):
        if top == 'table':
            rc = r_table(r, 0)
        elif top == 'block':
            rc = r_block(r, 0)
        else:
            rc = r_item(r)
        cases.append((rc, r.randint(0, 200), r.randint(0, 150)))
    res = {}
    for kind in ('stock', 'ours'):
        e = HEmu(kind)
        if len(sys.argv) > 5 and sys.argv[5] == 'thai':
            e.write32(e.thai_var, 1)
        e.heap_mark = e.heap_ptr
        outs = []
        for rc, w, h in cases:
            outs.append(run_case(e, rc, w, h, which))
        res[kind] = outs
    bad = 0
    exc = same_exc = 0
    for i, (a, b) in enumerate(zip(res['stock'], res['ours'])):
        if 'exc' in a and a.get('exc') == b.get('exc'):
            # both sides fault alike (a division by zero of the stock code, reproduced by ours): reported, not a difference
            same_exc += 1
            if same_exc <= 2:
                print('case', i, 'stock and ours fault alike:', a.get('exc'))
        elif 'exc' in a or 'exc' in b:
            exc += 1
            if exc <= 3:
                print('case', i, 'raised: stock', a.get('exc'), '| ours', b.get('exc'))
        if a != b:
            bad += 1
            if bad <= 2:
                print('case', i, cases[i][1:], 'top', cases[i][0]['k'])
                print(' stock', str(a)[:1500])
                print(' ours ', str(b)[:1500])
    if same_exc:
        print('t_html_layout %s %s: %d of the %d cases fault the same way in the stock and in ours (not counted)' % (which, top, same_exc, n))
    print('t_html_layout', which, top, 'cases', n, 'differ', bad, 'exceptions', exc)
    return 1 if bad or exc else 0


if __name__ == '__main__':
    sys.exit(main(int(sys.argv[1]), int(sys.argv[2]), sys.argv[3], sys.argv[4]))
