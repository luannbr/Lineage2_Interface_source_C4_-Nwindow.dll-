# Differential test of NToolTipSkin::Draw (0x10032860) / DrawFull (0x10032560), the nine-slice frame routines (stock vs ours under unicorn).
# The tool tip comes BY VALUE (0x80 bytes on the stack, ret 0x9C with the device, X, Y and the window rectangle); the callee destroys it.
# What is compared: every call the routine makes (the desktop's rectangle, the clip, the nine textures, the text calls of DrawTitleText /
# DrawFullText, every free of the by-value copy), and the tip kept in the skin (0x24: operator=). The device and the console's text getters
# are logging stubs (tip4.py); the desktop is a fake object (the console's +0x3BF4) whose rectangle is 1024 x 768.
#   python t_tipdraw2.py [cases] [seed] [draw|drawfull]
import sys, os, random, struct
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from tip4 import *
import t_tipdraw as TD

FUNCS = {'draw': 0x10032860, 'drawfull': 0x10032560}
SKIN = 0xA4
DESKTOP_OFF = 0x3BF4


def rand_case(r):
    c = TD.rand_case(r)
    # the Draw gate: nothing is drawn for a tip with no name (or a one-character name) and no lines, unless Unknown18 >= 0
    c['name'] = r.choice(TD.NAMES + ['x', '', ''])
    if r.random() < 0.3:
        c['lines'] = []
    c['u18'] = r.choice([-1, -1, -1, 0, 5, 140])
    # the rectangle of the window the tip is drawn in (X, Y ints; W, H floats the routine does not read)
    c['wx'] = r.choice([0, 0, 10, 300, 900, 1020, -40, -5])
    c['wy'] = r.choice([0, 0, 10, 300, 700, -30, -5, 60])
    c['ww'] = r.choice([0, 100.5, 800])
    c['wh'] = r.choice([0, 20.25, 600])
    # the point X, Y (relative to the window): both sides of every clamp
    c['x'] = r.choice([0, 5, 100, 400, 1000, 1030, -20, -300])
    c['y'] = r.choice([0, 5, 20, 50, 200, 700, -3, -100])
    c['tw'] = r.choice([0, 0, 20, 120.5, 300, 1100, 9.5])
    c['th'] = r.choice([0, 0, 13, 26.25, 90, 800, 5.5])
    c['fw'] = r.choice([0, 0, 10, 100.5, 250, 300.25, 1100, -4.0])
    c['fh'] = r.choice([0, 0, 13, 90, 240.5, 780, -2.5])
    return c


def run(e, A, c):
    mark = len(e.log)
    skin = e.alloc(SKIN)
    e.uc.mem_write(skin, bytes(SKIN))
    for i in range(9):
        e.write32(skin + 4 * i, (0x1000 + i) if c['tex'][i] else 0)
    # the skin already holds an old tip (Draw replaces it: the strings and the line array of the old one are freed)
    TD.put_info(e, skin + 0x24, dict(name='old', name2='older', desc='was', lines=[dict(label='a', value='b')], u18=-1))
    desktop = e.alloc(0x40)
    e.objs[desktop] = 'desktop'
    e.write32(e.console + DESKTOP_OFF, desktop)
    info = e.alloc(0x80)
    TD.put_info(e, info, c)
    words = [e.read32(info + 4 * i) for i in range(0x20)]
    dev = 0x31337000
    e.objs[dev] = 'device'
    va, nargs = A
    args = [dev] + words + [c['x'] & 0xFFFFFFFF, c['y'] & 0xFFFFFFFF, c['wx'] & 0xFFFFFFFF, c['wy'] & 0xFFFFFFFF, f32(c['ww']), f32(c['wh'])]
    e.call(va, this=skin, args=args)
    return dict(log=e.log[mark:], info=dump_info(e, skin + 0x24))


def main():
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 200
    seed = int(sys.argv[2]) if len(sys.argv) > 2 else 1
    which = sys.argv[3].split(',') if len(sys.argv) > 3 else ['draw', 'drawfull']
    es = TW('stock')
    eo = TW('ours')
    lm = log_map()
    es.heap_mark, eo.heap_mark = es.heap_ptr, eo.heap_ptr
    r = random.Random(seed)
    total = 0
    for f in which:
        cases = [rand_case(r) for _ in range(n)]
        va = FUNCS[f]
        As, Ao = (va, 0), (lm[va], 0)
        total += compare(f, es, eo, run, cases, As, Ao)
    print('TOTAL problems', total)


if __name__ == '__main__':
    main()
