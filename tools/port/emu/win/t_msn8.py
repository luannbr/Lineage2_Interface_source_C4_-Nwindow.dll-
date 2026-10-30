# Differential tests of the messenger's list boxes and task entries (wave C, lane proof2): TaskBox, L2TaskBox, FriendBox (the lists of the
# conversation windows: constructors, destructor, OnCreate, WindowProc, HitTest, OnPaint, OnLButtonUp, UpdateString, AddString, AddUser) and
# Task (the entry of an open conversation: constructor, destructor, OnCreate, OnTimer, OnDestroy); stock against ours under unicorn
# (t_msn1.py / tr.py; the tests are made from db/functions.csv).
#   python t_msn8.py [cases] [seed] [test,test,...]      (names are  Class.Method)
import sys, os, random, struct
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import tr
from tr import *
import t_msn1
from t_msn1 import *
import t_msn3
from t_msn3 import person, fsargs
import t_msn5
from t_msn5 import CW, gen5, mk, make_args
import t_rest1

FILES = {'TaskBox': 'src/nwindow/TaskBox.cpp', 'L2TaskBox': 'src/nwindow/L2TaskBox.cpp', 'FriendBox': 'src/nwindow/FriendBox.cpp', 'Task': 'src/nwindow/Task.cpp'}
t_rest1.REST1_FILES.clear()
t_rest1.REST1_FILES.update(FILES)
CL = t_rest1.build()
t_msn5.CLASSES.update(CL)
TESTS8 = {}
tr.ARGS_OVERRIDE[0x10168a10] = 4
tr.ARGS_OVERRIDE[0x10001cb0] = 3       # appMemcpy(dst, src, n): cdecl, the stock's out-of-line copy (the imports of ours are not logged)
tr.POP_OVERRIDE[0x10001cb0] = 0       # NFitText(src, dst, width, extra): cdecl, ret 0

def rs_task(e, p, c):
    r = random.Random(c.get('wpseed', 0) * 5 + 1)
    e.w32(p, 0xF8, r.choice([0, 7]))
    e.w32(p, 0xF4, r.choice([0, 1]))
    e.w32(p, 0xF0, r.choice([0, 1]))


t_msn5.RESTATE['Task'] = rs_task
BOXES = ('TaskBox', 'L2TaskBox', 'FriendBox')
# Task / L2Task fields: ChatWindow, bAlert, bBlink
TASKF = {'Task': (0xEC, 0xF0, 0xF4), 'L2Task': (0xF4, 0xF8, 0xFC)}


class BW(CW):
    def __init__(self, kind):
        CW.__init__(self, kind)
        R = self.rules
        R[0x1002d4b0] = lambda e, ecx, a: e.r32(ecx, 4) if a[0] < e.r32(ecx, 8) else 0           # NString::GetAt(index)
        R[0x100204e0] = lambda e, ecx, a: e.c.get('basewp', 0)                                   # NCListBox::WindowProc (base)
        R[0x10001cb0] = self.memcpy                                                               # the stock's inline memcpy (cdecl), out of line
        R[0x10168a10] = self.fit_text                                                             # NFitText(src, dst, width, extra)
        # the line texts: the stock's own NString functions run for real (ours hooks them)
        self.transparent |= {0x101510d0, 0x1006b460, 0x1002d640, 0x1002d4f0, 0x1002d4d0, 0x1002d620, 0x1002d490, 0x1002d660, 0x1002d690}
        if sa.ENABLED:
            self.transparent.add(0x101495b0)             # TMsnIntPairArray::Remove: a function in the hook build, inlined in the standalone (the box's int-pair array)

    def memcpy(self, e, ecx, a):
        if a[2] > 0:
            e.uc.mem_write(a[0], bytes(e.uc.mem_read(a[1], a[2])))
        return a[0]

    def fit_text(self, e, ecx, a):
        t = (e.wstr(a[0]) or '') if a[0] else ''
        e.uc.mem_write(a[1], t[:30].encode('utf-16le') + bytes(2))
        return min(len(t), 30)


def nstring(e, label, text, color):
    s = e.raw_obj(label, 0x14)
    e.w32(s, 0, len(text) + 1)
    e.w32(s, 4, e.put_wstr(text))
    e.w32(s, 8, len(text))
    e.w32(s, 0xC, color)
    e.w32(s, 0x10, 0)
    return s


def task_obj(e, r, label, kind='Task', chat=True):
    chat_off, alert_off, blink_off = TASKF[kind]
    t = e.win(kind, label, 150, 20)
    e.w32(t, chat_off, e.win('L2ChatWnd', label + 'Chat', 256, 401) if chat else 0)
    e.w32(t, alert_off, r.choice([0, 1, 1]))
    e.w32(t, blink_off, r.choice([0, 1]))
    return t


def box_obj(e, c, cls, label='Box'):
    r = random.Random(c.get('rs', 0))
    w = e.win(cls, label, c.get('w', 140.0), c.get('h', 126.0), c.get('sx', 100), c.get('sy', 50))
    kind = 'L2Task' if cls == 'L2TaskBox' else 'Task'
    n = c.get('n', 3)
    items = []
    for i in range(n):
        text = c.get('texts', ['alpha', '', 'a longer name than fits'] * 8)[i]
        s = nstring(e, 'Str%d' % i, text, 0xFF000000 | (0x112233 * (i + 1) & 0xFFFFFF))
        if cls == 'FriendBox':
            param, icon = -1, (0x7A00 + i if r.choice([0, 1]) else 0)
        else:
            param = task_obj(e, r, 'T%d' % i, kind, chat=r.choice([0, 1, 1])) if r.choice([0, 1, 1]) else 0
            icon = (0x7A00 + i) if (cls == 'TaskBox' and r.choice([0, 1])) else 0
        items.append((s, param, icon))
    data = e.alloc(0xC * max(n, 1) + 16)
    for i, (s, p, ic) in enumerate(items):
        e.write32(data + 0xC * i, s)
        e.write32(data + 0xC * i + 4, p & 0xFFFFFFFF)
        e.write32(data + 0xC * i + 8, ic)
    e.write32(w + 0x204, data if n else 0)
    e.write32(w + 0x208, n)
    e.write32(w + 0x20C, n)
    e.w32(w, 0x21C, c.get('vis', 4))
    e.w32(w, 0x220, c.get('max', 6))
    e.w32(w, 0x224, c.get('top', 0))
    e.w32(w, 0x228, c.get('numlines', n))
    e.w32(w, 0x22C, c.get('sel', -1))
    e.w32(w, 0x230, 4)
    e.w32(w, 0x234, 4)
    e.w32(w, 0x238, c.get('gap', 0))
    if cls != 'FriendBox':
        for off in (0x240, 0x244, 0x248):
            e.w32(w, off, 0x7B00 + off if c.get('tex', 1) else 0)
    return w


def gen_box(r):
    n = r.choice([0, 1, 3, 6, 9])
    return dict(rs=r.randint(0, 10 ** 6), n=n, vis=r.choice([1, 4, 6, 16]), max=r.choice([3, 6, 200]), top=r.choice([0, 1, 2, 5]),
                numlines=r.choice([n, max(n - 1, 0), n + 2]), sel=r.choice([-1, 0, 1, 2, 7]), gap=r.choice([0, 2]), tex=r.choice([0, 1, 1]),
                w=r.choice([140.0, 140.5, 90.0]), h=r.choice([126.0, 63.0, 21.0]), sx=r.choice([100, 0]), sy=r.choice([50, 0]),
                x=r.choice([-3, 0, 5, 100, 130, 239, 245, 300]), y=r.choice([-3, 0, 5, 21, 42, 70, 100, 140, 200]),
                msg=r.choice([0x201, 0x201, 0x202, 0x501, 0x5]), wp=r.choice([0, 1, 5]), basewp=r.choice([0, 0, 1]), color=r.choice([0xFFDCDCDC, 0xFF112233, 0]),
                texts=[r.choice(['alpha', '', 'a longer name than fits', 'x']) for _ in range(30)], text=r.choice(['new text', '', 'x y']),
                pstatus=r.choice(['NLN', 'FLN', 'BRB', 'IDL', 'AWY', 'LUN', 'BSY', 'PHN', 'HDN', '']), pname=r.choice(['x@y', 'n%20m', '']),
                hit=r.choice([0, 1, 1]), tw=r.choice([0, 1, 2]))


def reg(name, cls, sva, size, argfn=None, noret=True, opts=None):
    def b(e, c):
        w = box_obj(e, c, cls)
        e.write32(e.console + 0x3BF4, e.new_obj('NCWnd', 'Desktop'))
        e.presence = ''
        e.shown = {}
        return w, (argfn(e, c, w) if argfn else [])
    o = {'noret': noret, 'ignore': {0x10001cb0}}
    if opts:
        o.update(opts)
    TESTS8[name] = (sva, size, b, gen_box, o)


def rows(cls):
    import csv
    return [r for r in csv.DictReader(open(os.path.join(tr.ROOTP, 'db', 'functions.csv'), encoding='utf-8')) if r['src'] == FILES[cls] and r['status'] == 'done']


# the paints, the click, the hit test, the update of a line, the new line (by value person), the window procedure
for cls, hit_va, up_va, paint_va in (('TaskBox', 0x1013e740, 0x1013f980, 0x1013f740), ('L2TaskBox', 0x1013e740, 0x10149460, 0x10149250)):
    reg('%s.OnPaint' % cls, cls, paint_va, 510 if cls == 'TaskBox' else 460, lambda e, c, w: [e.device()], noret=False)
    reg('%s.HitTest' % cls, cls, hit_va, 179, lambda e, c, w: [c['x'] + c['sx'], c['y'] + c['sy']], noret=False)
    reg('%s.OnLButtonUp' % cls, cls, up_va, 124, lambda e, c, w: [c['wp'], ((c['y'] + c['sy']) << 16) | ((c['x'] + c['sx']) & 0xFFFF)], noret=False,
        opts={})
reg('FriendBox.OnPaint', 'FriendBox', 0x1013ef80, 436, lambda e, c, w: [e.device()], noret=False)
reg('TaskBox.UpdateString', 'TaskBox', 0x1013fa00, 48,
    lambda e, c, w: [e.put_wstr(c['text']), (e.r32(e.r32(w, 0x204) + 0xC * c['tw'] + 4, 0) if c['tw'] < c['n'] else 0x1234)], noret=True)
reg('TaskBox.WindowProc', 'TaskBox', 0x10148eb0, 84, lambda e, c, w: [c['msg'], c['wp'], ((c['y'] + c['sy']) << 16) | ((c['x'] + c['sx']) & 0xFFFF)], noret=False)
reg('L2TaskBox.WindowProc', 'L2TaskBox', 0x10148eb0, 84, lambda e, c, w: [c['msg'], c['wp'], ((c['y'] + c['sy']) << 16) | ((c['x'] + c['sx']) & 0xFFFF)], noret=False)
reg('L2TaskBox.AddString', 'L2TaskBox', 0x10149960, 270, lambda e, c, w: [e.put_wstr(c['text']) if c['hit'] else 0, c['tw'] * 7, c['color']], noret=True)


def user_words(e, c):
    p = person(e, 'newuser', name='nick' + c['pname'], status=c['pstatus'], f20='acc', n1=c['pname'], n2='', gid=1)
    return [e.read32(p + 4 * i) for i in range(0x11)]


reg('FriendBox.AddUser', 'FriendBox', 0x10140180, 609, lambda e, c, w: user_words(e, c) + [c['color']], noret=True)

# the generic ones: constructors, destructors, OnCreate (and the Task's OnTimer / OnDestroy)
for cls, (cva, csz, ckinds, methods) in CL.items():
    for (mname, va, size, kind) in methods:
        key = '%s.%s' % (cls, mname)
        if key in TESTS8 or mname in ('OnPaint', 'HitTest', 'OnLButtonUp', 'UpdateString', 'AddString', 'AddUser', 'WindowProc') and cls in BOXES:
            continue
        if mname == 'WindowProc' and cls == 'Task':
            pass
        TESTS8[key] = (va, size, mk(cls, mname, va, size, kind), gen5, {'noret': kind not in ('C', 'D', 'P', 'W', 'T')})

    def bctor(e, c, cls=cls, ckinds=ckinds):
        return poison(e, cls, cls), make_args(e, ckinds)
    TESTS8['%s.ctor' % cls] = (cva, csz, bctor, lambda r: {}, {'noret': True})

    def bdtor(e, c, cls=cls, cva=cva, csz=csz, ckinds=ckinds):
        p = poison(e, cls, cls, 0)
        e.run_real(cva, csz, p, make_args(e, ckinds))
        e.pre_len = len(e.log)
        return p, []
    for va2, (sz2, nm2, cl2) in tr.functions().items():
        if nm2 == '%s::~%s' % (cls, cls):
            TESTS8['%s.dtor' % cls] = (va2, sz2, bdtor, lambda r: {}, {'noret': True})


# the constructors with the sizes given (ret 0xC)
def b_ctor2(cls):
    def b(e, c):
        return poison(e, cls, cls), [c['a0'] * 50, c['a1'], c['a2']]
    return b


for cls, va in (('TaskBox', 0x101433f0), ('L2TaskBox', 0x1014ab30), ('FriendBox', 0x101425c0)):
    TESTS8['%s.ctor2' % cls] = (va, 60, b_ctor2(cls), gen5, {'noret': True})

if __name__ == '__main__':
    t_msn1.run_tests(TESTS8, BW, sys.argv)
