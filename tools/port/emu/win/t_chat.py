# Differential tests of the NCChatWnd tab / resize / tell-name functions (stock vs ours under unicorn)
import sys, os, random, struct
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from gw import *
import sa

FUNCS = {'resize1': (0x10054b30, 1), 'onresize': (0x10054d50, 1), 'tabclick': (0x10056ec0, 1), 'option': (0x100575d0, 0),
         'pledge': (0x10053cd0, 1), 'ally': (0x10053d60, 1), 'clearall': (0x100576a0, 0), 'ondestroy': (0x10056a10, 0),
         'addtell': (0x10056cd0, 1), 'tabsel': (0x100539b0, 1), 'tabmerge': (0x10057550, 1)}
CALLEES = {
    0x1002e4a0: ('IsSplit', 4, 0), 0x10035650: ('GetLocalRect', 4, 0), 0x1002c0a0: ('SetRange', 8, 0), 0x1002c150: ('SetValue', 4, 0),
    0x10055110: ('SetFilter', 52, 0), 0x10055870: ('LoadFilters', 0, 0), 0x10074e60: ('PlaySound', 4, 0),
    0x1002d7a0: ('SetDisable', 8, 0), 0x10035880: ('RemoveChild', 4, 0), 0x10056350: ('LimiterClear', 0, 0),
    0x10036160: ('WndOnDestroy', 0, 0), 0x1002d660: ('NStringCtor', 4, 1), 0x1002d690: ('NStringDtor', 0, 0),
    0x1002d4b0: ('NStringGetAt', 4, 0), 0x1005cf40: ('DockAboveChat', 0, 0), 0x1000e790: ('DeleteChar', 4, 0),
    0x1000e540: ('InsertText', 8, 0), 0x10171e4e: ('wcsicmp', 0, 0),
}
OURS_SYMS = {0x10055870: '?LoadFilters@NCChatFilterWnd@@QAEXXZ', 0x10055110: '?SetFilter@NCChatFilterWnd@@QAEXUNChatFilterC4@@@Z',
             0x10056350: '?Clear@NChatLimiter@@QAEXXZ', 0x1002d660: '??0NString@@QAE@PBG@Z', 0x1002d690: '??1NString@@QAE@XZ',
             0x1002d4b0: '?GetAt@NString@@QAEPAGI@Z', 0x1005cf40: '?DockAboveChat@NCConsole@@QAEXXZ',
             0x1000e790: '?DeleteChar@NCEditBox@@QAEXH@Z', 0x1000e540: '?InsertText@NCEditBox@@QAEXPBGH@Z',
             0x10035880: '?RemoveChildWindow@NCWnd@@QAEXPAUNWndEntry@@@Z', 0x10036160: '?OnDestroy@NCWnd@@UAEHXZ',
             0x1002c0a0: '?SetRange@NCSliderBar@@QAEXHH@Z', 0x1002c150: '?SetValue@NCSliderBar@@QAEXH@Z',
             0x10035650: '?GetLocalRect@NCWnd@@QAEPAUNRectF@@PAU2@@Z', 0x10074e60: '?PlayInterfaceSound@NConsoleWnd@@QAEXH@Z'}
import gw
gw.VPOP[76] = 0
gw.VPOP[77] = 0
WIN_FLOATS = (0x4C, 0x50)


class CW(GW):
    def __init__(self, kind):
        GW.__init__(self, kind, CALLEES, OURS_SYMS)
        self.split = 0
        if kind == 'ours' and not sa.ENABLED:
            # the prefix table of the stock image (GChatPrefix 0x1023DBB8, 2 pointers per type) at the stock-globals stand-in
            # (standalone: ours reads its own initial table, which gen_globals.py --check proves equal to the stock's)
            import pefile
            from emu import STOCK
            st = pefile.PE(STOCK)
            mm = st.get_memory_mapped_image()
            for k in range(0, 0xA0, 4):
                ptr = struct.unpack('<I', mm[0x23DBB8 + k:0x23DBB8 + k + 4])[0]
                if ptr:
                    off = ptr - 0x10000000
                    txt = ''
                    while off < len(mm) and mm[off:off + 2] != bytes(2):
                        txt += mm[off:off + 2].decode('utf-16le')
                        off += 2
                    self.write32(NWBASE + 0x23DBB8 + k, self.put_wstr(txt))
        self.rect_h = 100.0
        self.commit = 0
        self.vrets[75] = lambda e, t, a: e.commit if e.objs.get(t) == 'edit' else 0

        def rect(e, t, a):
            for i, v in enumerate((0, 0, f32(100.0), f32(e.rect_h))):
                e.write32(a[0] + 4 * i, v)
            return 0
        self.rets['GetLocalRect'] = rect
        self.rets['IsSplit'] = lambda e, t, a: e.split
        self.rets['NStringCtor'] = self.nctor
        self.rets['NStringGetAt'] = lambda e, t, a: e.read32(t) + 2 * a[0]
        self.argfix = {'SetFilter': lambda e, a: [e.read32(a[0] + 4 * i) for i in range(0)] or a}

    def nctor(self, e, t, a):
        s = e.wstr(a[0]) if a[0] else ''
        p = e.put_wstr(s)
        e.write32(t, p)
        e.write32(t + 4, len(s))
        e.log.append(('NStringCtor', s))
        return t

    def sargs(self, n):
        return GW.sargs(self, n)

    def dispatch(self, name):
        kind, nm = name.split(':', 1)
        if kind == 'cb' and nm == 'wcsicmp':
            self.last_pop = 0
            b = self.sargs(2)
            x, y = self.wstr(b[0]).lower(), self.wstr(b[1]).lower()
            return 0 if x == y else (1 if x > y else 0xFFFFFFFF)
        if kind == 'cb' and nm == 'SetFilter':
            self.last_pop = 52
            a = self.sargs(13)
            self.log.append(('SetFilter', self.nm(self.this()), a))
            return 0
        return GW.dispatch(self, name)


def make_list(e, rnd, name):
    lst = e.window(name, size=0x300, rnd=rnd)
    sb = e.window(name + '_sb', size=0x300, rnd=rnd)
    dn = e.window(name + '_dn', size=0x300, rnd=rnd)
    e.write32(lst + 0x218, sb)
    e.write32(lst + 0x214, dn)
    e.write32(lst + 0x208, rnd.randint(0, 40))     # Items.Num
    e.write32(lst + 0x21C, rnd.randint(1, 30))     # VisibleLines
    e.write32(lst + 0x224, rnd.randint(0, 20))     # TopIndex
    e.write32(lst + 0x228, rnd.randint(0, 60))     # NumLines
    for k in range(11):
        e.write32(lst + 0x240 + 4 * k, rnd.randint(0, 1))
    return lst


def world(e, c):
    rnd = random.Random(c['seed'])
    e.objs = {}
    e.shown = {}
    con = e.console
    e.uc.mem_write(con, bytes(0x4000))
    e.objs[con] = 'console'
    e.write32(con, e.vt)
    e.split = c.get('split', 0)
    e.rect_h = c.get('recth', 100.0)
    chat = e.window('chat', size=0x700, rnd=rnd)
    for off, v in ((0x44, rnd.randint(0, 800)), (0x48, rnd.randint(0, 600)), (0x4C, f32(348.0)), (0x50, f32(c['height']))):
        e.write32(chat + off, v)
    edit = e.window('edit', size=0x300, rnd=rnd)
    e.write32(chat + 0x134, edit)
    e.commit = e.put_wstr(c['commit']) if c['commit'] is not None else 0
    for k in range(5):
        e.write32(chat + 0x18C + 4 * k, make_list(e, rnd, 'list%d' % k))
    e.write32(chat + 0x1A0, make_list(e, rnd, 'syslist'))
    for off, nm in ((0x1A4, 'tab'), (0x1A8, 'optbtn'), (0x1AC, 'unk138'), (0x1B0, 'filter'), (0x1B8, 'resizeframe')):
        e.write32(chat + off, e.window(nm, size=0x300, rnd=rnd))
    e.shown[e.read32(chat + 0x1B0)] = rnd.randint(0, 1)
    e.write32(chat + 0x1B4, c.get('bfilter', 0))
    e.write32(chat + 0x1BC, c.get('rdelta', 0))
    e.write32(chat + 0x1D0, c.get('curtab', 0))
    e.write32(chat + 0x184, -1)
    # tell names
    nn = c.get('ntell', 0)
    arr = e.alloc(4 * 70)
    e.uc.mem_write(arr, bytes(4 * 70))
    for k in range(nn):
        o = e.alloc(0x14)
        e.uc.mem_write(o, bytes(0x14))
        e.write32(o, e.put_wstr(c['tells'][k]))
        e.write32(o + 4, len(c['tells'][k]))
        e.objs[o] = 'tell%d' % k
        e.write32(arr + 4 * k, o)
    e.write32(chat + 0x178, arr); e.write32(chat + 0x17C, nn); e.write32(chat + 0x180, 70)
    e.write32(chat + 0x188, c.get('browse', 0))
    # GIsOldChat
    for nm, v in e.gvars.items():
        if 'GIsOldChat' in nm:
            e.write32(v, c.get('oldchat', 0))
    e.chat = chat
    e.objs[chat + 0x1DC] = 'limiter'
    return chat


def run(e, A, c):
    mark = len(e.log)
    chat = world(e, c)
    which = c['f']
    va, nargs = A[which]
    args = list(c['args'])
    if which == 'addtell':
        args = [e.put_wstr(args[0]) if args[0] is not None else 0]
    e.call(va, this=chat, args=args)
    lists = [e.snapshot(e.read32(chat + 0x18C + 4 * k), 0x300) for k in range(5)]
    subs = [e.snapshot(e.read32(e.read32(chat + 0x18C + 4 * k) + 0x218), 0x300) for k in range(5)]
    subs += [e.snapshot(e.read32(chat + off), 0x300) for off in (0x1A4, 0x1A8, 0x1AC, 0x1B8, 0x134)]
    tells = []
    nn = e.read32(chat + 0x17C)
    arr = e.read32(chat + 0x178)
    for k in range(min(nn, 70)):
        p = e.read32(arr + 4 * k)
        tells.append((e.objs.get(p, p), e.wstr(e.read32(p)) if p and e.read32(p) > 0x60000000 else None))
    e.write32(chat + 0x178, 0)
    snap = e.snapshot(chat, 0x1F0)
    return dict(log=e.log[mark:], chat=snap, lists=lists, subs=subs, tells=tells, nn=nn)


def rand(r, f):
    c = dict(seed=r.randint(0, 10 ** 6), f=f, height=r.choice([187.0, 190.0, 250.5, 400.0, 502.0, 510.0]),
             split=r.randint(0, 1), recth=r.choice([60.0, 100.0, 187.0, 300.5, 450.0]), curtab=r.randint(-1, 6),
             bfilter=r.randint(0, 1), rdelta=r.choice([-30, -16, -14, -3, 0, 3, 14, 15, 16, 30, 45]), oldchat=r.randint(0, 1),
             browse=r.randint(0, 1), commit=r.choice([None, '', '/', '"', '!', '+', '$', '#', '@', '(', 'a', '/x', '@hello', '!!']))
    prefixes = ['', '/', '"', '!', '+', '$', '#', '@']
    if f in ('resize1', 'tabclick', 'tabsel', 'tabmerge'):
        c['args'] = [r.randint(-1, 11)]
    elif f == 'onresize':
        c['args'] = [r.choice([-40, -20, -15, -14, -3, 0, 3, 14, 15, 16, 30, 45])]
    elif f in ('pledge', 'ally'):
        c['args'] = [r.randint(0, 1)]
    elif f == 'addtell':
        n = r.choice([0, 1, 2, 5, 63, 64])
        c['ntell'] = n
        c['tells'] = ['name%d' % k for k in range(n)]
        c['args'] = [r.choice([None, '', '-x', 'bob', 'name0', 'NAME1', 'x' * 23, 'x' * 24, 'name%d' % max(0, n - 1), 'Name%d' % (n // 2)])]
    elif f in ('clearall', 'ondestroy'):
        n = r.choice([0, 1, 3])
        c['ntell'] = n
        c['tells'] = ['name%d' % k for k in range(n)]
        c['args'] = []
    else:
        c['args'] = []
    return c


if __name__ == '__main__':
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 40
    seed = int(sys.argv[2]) if len(sys.argv) > 2 else 1
    which = sys.argv[3].split(',') if len(sys.argv) > 3 else list(FUNCS)
    es = CW('stock'); eo = CW('ours')
    lm = log_map()
    es.heap_mark, eo.heap_mark = es.heap_ptr, eo.heap_ptr
    As = {k: (v[0], v[1]) for k, v in FUNCS.items()}
    Ao = {k: (lm[v[0]], v[1]) for k, v in FUNCS.items()}
    r = random.Random(seed)
    total = 0
    for f in which:
        cases = [rand(r, f) for _ in range(n)]
        total += compare(f, es, eo, run, cases, As, Ao)
    print('TOTAL problems', total)
