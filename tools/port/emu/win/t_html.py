# Differential test of NCConsole::OnHtmlCommand (stock vs ours under unicorn)
#   python t_html.py [cases] [seed]
import sys, os, random, struct
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from gw import *
import gw
import stubs
from unicorn.x86_const import *

FUNC = 0x1005cb80
CALLEES = {0x10068d30: ('RequestLinkHtml', 4, 0), 0x10068ec0: ('RequestTutorialLinkHtml', 4, 0), 0x10068d50: ('RequestBBSWrite', 4, 0)}
OURS_SYMS = {}
stubs.IMPORT_POP.update({'?ParseCommand@@YAHPAPBGPBG@Z': 0, '?ParseToken@@YAHAAPBGPAGHH@Z': 0, '?appStrcmp@@YAHPBG0@Z': 0,
                         '??0L2ParamStack@@QAE@H@Z': 4, '??1L2ParamStack@@QAE@XZ': 0, '?PushBack@L2ParamStack@@QAEHPAX@Z': 4,
                         '?IsA@UObject@@QBEHPAVUClass@@@Z': 4})
gw.VPOP.update({78: 4})
TUTORIAL_CLASS = 0x102C89E0


class HW(GW):
    def __init__(self, kind):
        GW.__init__(self, kind, CALLEES, OURS_SYMS)
        self.case = None
        self.stacks = {}
        self.vrets[78] = self.control
        self.tut = self.gptr(TUTORIAL_CLASS)       # the pointer VALUE ours compares IsA's argument with (hook: the stand-in, standalone: its own UClass object)

    def control(self, e, t, a):
        name = e.wstr(a[0])
        e.log.append(('GetControlValue', name))
        v = e.case['controls'].get(name)
        return e.put_wstr(v) if v is not None else 0

    def do_import(self, nm):
        a = self.arg
        c = self.case
        if nm == '?ParseCommand@@YAHPAPBGPBG@Z':
            pstream, match = a(0), self.wstr(a(1))
            p = self.read32(pstream)
            s = self.wstr(p)
            i = 0
            while i < len(s) and s[i] == ' ':
                i += 1
            ok = s[i:i + len(match)].lower() == match.lower()
            self.log.append(('ParseCommand', match, ok))
            if ok:
                self.write32(pstream, p + 2 * (i + len(match)))
            return 1 if ok else 0
        if nm == '?ParseToken@@YAHAAPBGPAGHH@Z':
            pstream, out, mx, esc = a(0), a(1), a(2), a(3)
            p = self.read32(pstream)
            s = self.wstr(p)
            i = 0
            while i < len(s) and s[i] == ' ':
                i += 1
            j = i
            while j < len(s) and s[j] != ' ':
                j += 1
            tok = s[i:j]
            self.write_wstr_at(out, tok[:mx - 1])
            self.write32(pstream, p + 2 * j)
            self.log.append(('ParseToken', tok, mx, esc))
            return 1 if tok else 0
        if nm == '?appStrcmp@@YAHPBG0@Z':
            x, y = self.wstr(a(0)), self.wstr(a(1))
            return 0 if x == y else (1 if x > y else 0xFFFFFFFF)
        if nm == '??0L2ParamStack@@QAE@H@Z':
            self.stacks[self.this()] = []
            self.log.append(('ParamStack', a(0)))
            return self.this()
        if nm == '??1L2ParamStack@@QAE@XZ':
            self.log.append(('~ParamStack',))
            return None
        if nm == '?PushBack@L2ParamStack@@QAEHPAX@Z':
            self.stacks.setdefault(self.this(), []).append(self.wstr(a(0)))
            return 1
        if nm == '?IsA@UObject@@QBEHPAVUClass@@@Z':
            t = self.this()
            cls = self.read32(t + 0x24)
            while cls:
                if cls == a(0):
                    return 1
                cls = self.read32(cls + 0x34)
            return 0
        return GW.do_import(self, nm)

    def dispatch(self, name):
        kind, nm = name.split(':', 1)
        if kind == 'cb':
            self.last_pop = 4
            arg = self.sargs(1)[0]
            self.log.append((nm, self.stacks.get(arg)))
            return 0
        return GW.dispatch(self, name)


def world(e, c):
    rnd = random.Random(c['seed'])
    e.case = c
    e.objs = {}
    e.stacks = {}
    con = e.console
    e.uc.mem_write(con, bytes(0x4000))
    e.objs[con] = 'console'
    if c['source']:
        src = e.window('source', size=0x100, rnd=rnd)
        # class chain: Class(+0x24) -> Super(+0x34) -> ...
        k1 = e.alloc(0x40); k2 = e.alloc(0x40)
        e.uc.mem_write(k1, bytes(0x40)); e.uc.mem_write(k2, bytes(0x40))
        e.write32(src + 0x24, k1)
        e.write32(k1 + 0x34, e.tut if c['tutorial'] else k2)
        e.write32(k2 + 0x34, 0)
    else:
        src = 0
    e.src = src
    return con


def run(e, A, c):
    mark = len(e.log)
    con = world(e, c)
    cmd = e.put_wstr(c['cmd'])
    e.call(A['f'], this=con, args=[cmd, e.src])
    ret = e.reg(UC_X86_REG_EAX)
    return dict(log=e.log[mark:], ret=ret)


def rand(r):
    boards = ['1', '2', '4', '5', '7', '9']
    b = r.choice(boards)
    toks = lambda n: ' '.join(r.choice(['a1', 'b2', 'c3', 'dd']) for _ in range(n))
    cmd = r.choice([
        'Link', 'Link  bypass_x', 'Link abc def', 'Write', 'Write ' + b, 'Write %s x' % b, 'Write %s x y' % b,
        'Write %s x y c1' % b, 'Write %s x y c1 c2' % b, 'Write %s x y c1 c2 c3' % b, 'Write %s x y c1 c3' % b,
        'Write %s x y c1 c2 c3 c4' % b, 'Foo bar', '', 'link x', 'write 4 x y c1', ' Write 7 x y c1', 'Write 5 x y c1 c3',
        'Write 4 x y c1 c3', 'Write 7 x y c1 c2'])
    controls = {}
    for nm in ('c1', 'c2', 'c3', 'c4'):
        if r.random() < 0.8:
            controls[nm] = r.choice(['text', 'hello world', '', 'x'])
    return dict(seed=r.randint(0, 10 ** 6), cmd=cmd, source=1 if r.random() < 0.85 else 0, tutorial=r.randint(0, 1), controls=controls)


if __name__ == '__main__':
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 50
    seed = int(sys.argv[2]) if len(sys.argv) > 2 else 1
    es = HW('stock'); eo = HW('ours')
    lm = log_map()
    es.heap_mark, eo.heap_mark = es.heap_ptr, eo.heap_ptr
    r = random.Random(seed)
    cases = [rand(r) for _ in range(n)]
    compare('html', es, eo, run, cases, {'f': FUNC}, {'f': lm[FUNC]})
