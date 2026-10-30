# Differential tests of the conversation windows' text boxes and title bars (wave C, lane proof2): L2ChatWnd (the Lineage II friend chat window),
# L2ChatBox / ChatBox (the lists of lines: Add, AddMessage, AddWithIcon, OnPaint), ChatFrameCtrl / L2ChatFrameCtrl (the title bars); stock against ours under
# unicorn (t_msn1.py / tr.py).
#   python t_msn15.py [cases] [seed] [test,test,...]
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
from t_msn5 import make_args, gen5, mk
import t_msn8
from t_msn8 import BW, nstring
import t_msn10
from t_msn10 import friend_obj, gen10, poison_obj
import t_rest1

TESTS15 = {}
tr.ARGS_OVERRIDE.update({0x100137e0: 3, 0x10013be0: 2})            # NCGDevice::FirstWrappedLine(text, width, font), NextWrappedLine(width, font): cdecl, ret 0
tr.POP_OVERRIDE.update({0x100137e0: 0, 0x10013be0: 0})
GDATA_SYSMSG = 0x722C8
GDATA_SYSSTR = 0x9871C
CLS = ('L2ChatWnd', 'L2ChatBox', 'L2ChatFrameCtrl', 'ChatBox', 'ChatFrameCtrl')
FILES = {'L2ChatWnd': 'src/nwindow/L2ChatWnd.cpp', 'L2ChatBox': 'src/nwindow/L2ChatBox.cpp', 'ChatBox': 'src/nwindow/ChatBox.cpp',
         'ChatFrameCtrl': 'src/nwindow/ChatFrameCtrl.cpp', 'L2ChatFrameCtrl': 'src/nwindow/L2ChatFrameCtrl.cpp'}
t_rest1.REST1_FILES.clear()
t_rest1.REST1_FILES.update(FILES)
CL = t_rest1.build()
t_msn5.CLASSES.update(CL)


class LW15(BW):
    def __init__(self, kind):
        BW.__init__(self, kind)
        R = self.rules
        R[0x100137e0] = self.first_wrapped                                                         # NCGDevice::FirstWrappedLine
        R[0x10013be0] = self.next_wrapped                                                          # NCGDevice::NextWrappedLine
        R[0x101495b0] = self.items_remove                                                          # TMsnIntPairArray::Remove(index, count) of the 12-byte items
        R[0x101560c0] = lambda e, ecx, a: getattr(e, 'l2friend', 0)                                # L2FriendWnd::FindFriend(FString)
        R[0x10073e80] = lambda e, ecx, a: getattr(e, 'me_user', 0)                                 # GetMyUser
        R[0x10151150] = lambda e, ecx, a: e.r32(ecx, 0xC)                                          # the friend record's online flag getter
        # ChatBox::OnCreate (shared with L2ChatBox); the minimize / close handlers (ours inlines them into the window procedure); the frame's destructor
        self.transparent |= {0x1013f170, 0x10148d90, 0x10148db0, 0x100124c0}
        # the items' Remove(index, count) (a pair array of 12 bytes) is a function of the stock and of the hook build, inlined by the standalone: real on both sides
        self.transparent.add(0x101495b0)

    # the wrapping of a text into lines (a model: pieces of at most width // 7 characters, cut after a space when there is one)
    def pieces(self, e, text, width):
        n = max(width // 7, 1)
        out = []
        while text:
            if text[0] == chr(10):                # a line break is an empty line
                out.append('')
                text = text[1:]
                continue
            nl = text.find(chr(10))
            if 0 < nl < n:
                n_save, n = n, nl
                out.append(text[:nl])
                text = text[nl:]
                n = n_save
                continue
            if len(text) <= n:
                out.append(text)
                break
            cut = text.rfind(' ', 0, n)
            cut = cut + 1 if cut > 0 else n
            out.append(text[:cut])
            text = text[cut:]
        return out

    def wrap_buf(self, e, piece):
        if not hasattr(e, 'wbuf'):
            e.wbuf = e.alloc(0x2100)
        e.uc.mem_write(e.wbuf, piece.encode('utf-16le') + bytes(2))
        return e.wbuf

    def first_wrapped(self, e, ecx, a):
        text = (e.wstr(a[0]) or '') if a[0] else ''
        e.wq = self.pieces(e, text, a[1])
        if not e.wq:
            return 0
        return self.wrap_buf(e, e.wq.pop(0))

    def next_wrapped(self, e, ecx, a):
        if not getattr(e, 'wq', None):
            return 0
        return self.wrap_buf(e, e.wq.pop(0))

    def items_remove(self, e, ecx, a):
        i, cnt = a[0], a[1]
        data, n = e.read32(ecx), e.read32(ecx + 4)
        for j in range(i, n - cnt):
            e.uc.mem_write(data + 0xC * j, bytes(e.uc.mem_read(data + 0xC * (j + cnt), 0xC)))
        e.write32(ecx + 4, n - cnt)
        return 0


def sysmsg(e, idx, text):
    e.fs_set(e.gdata + GDATA_SYSMSG + 0x30 * idx + 4, text)


def sysstr(e, idx, text):
    e.fs_set(e.gdata + GDATA_SYSSTR + 12 * idx, text)


TXT = ['', chr(10) + 'abc', 'ab' + chr(10) + 'cd', 'hello', 'hello world this is a long line that wraps over and over again', 'x y', 'abcdefghijklmnopqrstuvwxyz0123456789' * 3, '$s1 says', 'a']


def gen15(r):
    d = gen10(r)
    d.update(rs=r.randint(0, 10 ** 6), n=r.choice([0, 1, 3, 6]), vis=r.choice([1, 4, 14]), max=r.choice([3, 6, 200]), top=r.choice([0, 1, 2]),
             numlines=r.choice([0, 1, 3, 7]), gap=r.choice([0, 6]), w=r.choice([238.0, 140.5, 90.0, 30.0]), h=r.choice([252.0, 126.0, 21.0]), sx=r.choice([100, 0]),
             sy=r.choice([50, 0]), color=r.choice([0xFFDCDCDC, 0xFF112233, 0]), bind=r.choice([0, 1]), icon=r.choice([0, 0x7A01]), from_=r.choice(['', 'bob', 'a b']),
             text=r.choice(TXT), tmpl=r.choice(['$s1 said:', 'no marker', '']), ptxts=[r.choice(TXT) for _ in range(8)], tex=r.choice([0, 1, 1]),
             tid=r.choice([-1, 0, 5, 1000]), titlex=r.choice([0, 20]), titley=r.choice([0, 6]), lp=r.choice([0, 1, 2]), found=r.choice([0, 1]), online=r.choice([0, 1]),
             hasme=r.choice([0, 1]), cmt=r.choice(['', 'hi there', 'x' * 50]), hid=r.choice([0, 1]), fname=r.choice(['', 'Fred', 'a long friend name']),
             msg=r.choice([0x501, 0x501, 0x5, 0x111]), wp=r.choice([0xF0F0, 0xF0F1, 0xA71, 0, 1]), parent=r.choice([0, 1, 1]), pw=r.choice([0, 1, 1]))
    return d


def chatbox_obj(e, c, cls):
    r = random.Random(c['rs'])
    w = e.win(cls, 'Box', c['w'], c['h'], c['sx'], c['sy'])
    n = c['n']
    items = []
    for i in range(n):
        text = c['ptxts'][i]
        s = nstring(e, 'Str%d' % i, text, 0xFF000000 | (0x112233 * (i + 1) & 0xFFFFFF))
        items.append((s, r.choice([-1, 0, 1]), r.choice([0, 0x7A00 + i])))
    data = e.alloc(0xC * max(n, 1) + 16)
    for i, (s, p, ic) in enumerate(items):
        e.write32(data + 0xC * i, s)
        e.write32(data + 0xC * i + 4, p & 0xFFFFFFFF)
        e.write32(data + 0xC * i + 8, ic)
    e.write32(w + 0x204, data if n else 0)
    e.write32(w + 0x208, n)
    e.write32(w + 0x20C, n + 2 if n else 0)       # an empty array has no room (room without a block would write through NULL, over the SEH chain at fs:[0])
    e.w32(w, 0x21C, c['vis'])
    e.w32(w, 0x220, c['max'])
    e.w32(w, 0x224, c['top'])
    e.w32(w, 0x228, n)              # the count of lines follows the array (a count above it reads past the array in the stock as well)
    e.w32(w, 0x22C, -1)
    e.w32(w, 0x230, 4)
    e.w32(w, 0x234, 4)
    e.w32(w, 0x238, c['gap'])
    sysmsg(e, 1326, c['tmpl'])
    return w


def reg(name, sva, size, build, noret=True, opts=None):
    o = {'noret': noret, 'ignore': {0x1008e8a0, 0x10001cb0, 0x10151150}}
    if opts:
        o.update(opts)

    def b(e, c):
        e.write32(e.console + 0x3BF4, e.new_obj('NCWnd', 'Desktop'))
        e.presence = ''
        e.shown = {}
        for i in range(4):
            sysstr(e, 1002 if i == 0 else 100 + i, ['title ' + str(i), 'a', 'b', 'c'][i])
        e.me_user = 0
        e.l2friend = 0
        e.__dict__.pop('wbuf', None)          # the wrapping buffer is allocated again in each case (the heap is reset between them)
        e.wq = []
        return build(e, c)
    TESTS15[name] = (sva, size, b, gen15, o)


# ---- the boxes ---------------------------------------------------------------------------------------------------------------------------
reg('L2ChatBox.OnPaint', 0x10148f50, 334, lambda e, c: (chatbox_obj(e, c, 'L2ChatBox'), [e.device()]), noret=False)
reg('L2ChatBox.Add', 0x10149630, 804, lambda e, c: (chatbox_obj(e, c, 'L2ChatBox'), [e.put_wstr(c['text']), c['color'], c['bind']]))
reg('L2ChatBox.AddMessage', 0x10149b70, 363, lambda e, c: (chatbox_obj(e, c, 'L2ChatBox'), [e.put_wstr(c['from_']) if c['lp'] else 0, e.put_wstr(c['text']), c['color']]))
reg('ChatBox.OnPaint', 0x1013f1a0, 596, lambda e, c: (chatbox_obj(e, c, 'ChatBox'), [e.device()]), noret=False)
reg('ChatBox.Add', 0x10140680, 576, lambda e, c: (chatbox_obj(e, c, 'ChatBox'), [e.put_wstr(c['text']), c['color'], c['bind']]))
reg('ChatBox.AddWithIcon', 0x101403f0, 654, lambda e, c: (chatbox_obj(e, c, 'ChatBox'), [c['icon'], e.put_wstr(c['text']), c['color']]))


def b_cbm(e, c):
    w = chatbox_obj(e, c, 'ChatBox')
    user = friend_obj(e, 'From', dict(c, pname=c['from_'] or 'nick')) if c['lp'] else 0
    return w, [e.put_wstr(c['text']), user, c['color']]


reg('ChatBox.AddMessage', 0x10141220, 410, b_cbm)

# ---- the title bars ----------------------------------------------------------------------------------------------------------------------
def frame_obj(e, c, cls):
    w = e.win(cls, 'Frame', c['w'], 20.0, c['sx'], c['sy'])
    for off, v in ((0x208, 0x7F08), (0x20C, 0x7F0C), (0x210, 0x7F10)):
        e.w32(w, off, v if c['tex'] else 0)
    e.w32(w, 0x200, c['tid'])
    e.w32(w, 0x23C, c['titlex'])
    e.w32(w, 0x240, c['titley'])
    sysstr(e, 1002, 'Messenger')
    for i in range(0, 12):
        if i != 2:
            sysstr(e, 1000 + (i if i < 2 else i), 'title%d' % i)
    sysstr(e, 1002, 'Messenger')
    if c['tid'] >= 0:
        sysstr(e, c['tid'], c['fname'] or 'name')
    pent = e.raw_obj('ParentEntry', 0x40)
    pw = e.win('L2ChatWnd', 'ParentChat', 256, 401) if c['pw'] else 0
    e.w32(pent, 4, pw)
    ent = e.r32(w, 0x74)
    e.w32(ent, 0xC, pent if c['parent'] else 0)
    return w


for cls, ev in (('ChatFrameCtrl', 0x1013f430), ('ChatFrameCtrl', 0)):
    pass
reg('ChatFrameCtrl.OnPaint', 0x1013f430, 637, lambda e, c: (frame_obj(e, c, 'ChatFrameCtrl'), [e.device()]), noret=False)
reg('ChatFrameCtrl.OnMinimize', 0x10148d10, 43, lambda e, c: (frame_obj(e, c, 'ChatFrameCtrl'), []), noret=False)
reg('ChatFrameCtrl.OnClose', 0x10148d40, 43, lambda e, c: (frame_obj(e, c, 'ChatFrameCtrl'), []), noret=False)
reg('ChatFrameCtrl.WindowProc', 0x1013e340, 116, lambda e, c: (frame_obj(e, c, 'ChatFrameCtrl'), [c['msg'], c['wp'], 0]), noret=False)


# ---- L2ChatWnd ---------------------------------------------------------------------------------------------------------------------------
def l2chat_obj(e, c):
    w = e.win('L2ChatWnd', 'L2Chat', 256, 401)
    e.w32(w, 0xE8, e.win('NCWnd', 'Messenger') if c['hasme'] else 0)
    e.fs_set(w + 0xEC, c['fname'])
    e.w32(w, 0xF8, 1)
    e.w32(w, 0xFC, c['hid'])
    e.w32(w, 0x100, e.win('L2ChatFrameCtrl', 'TitleBar', 256, 20))
    e.w32(w, 0x104, chatbox_obj(e, c, 'L2ChatBox'))
    ed = e.win('NCEditBox', 'EditBox', 239, 16)
    e.w32(w, 0x108, ed)
    e.w32(w, 0x10C, 0x7F20 if c['tex'] else 0)
    e.commit = e.put_wstr(c['cmt']) if c['cmt'] else 0
    if c['found']:
        fr = e.raw_obj('FriendRec', 0x40)
        e.w32(fr, 0xC, c['online'])
        e.l2friend = fr
    if c['lp'] == 1:
        u = e.raw_obj('MeUser', 0x80)
        e.uc.mem_write(u + 0x1C, 'MyName'.encode('utf-16le') + bytes(2))
        e.me_user = u
    sysmsg(e, 1327, 'not online')
    return w


reg('L2ChatWnd.OnSend', 0x10149dd0, 383, lambda e, c: (l2chat_obj(e, c), [c['lp']]), noret=False)
reg('L2ChatWnd.SetFriendName', 0x101562a0, 81, lambda e, c: (l2chat_obj(e, c), fsargs(e, c['fname'] or 'Zed')))
reg('L2ChatWnd.OnClose', 0x10148db0, 34, lambda e, c: (l2chat_obj(e, c), []), noret=False)
reg('L2ChatWnd.OnMinimize', 0x10148d90, 21, lambda e, c: (l2chat_obj(e, c), []), noret=False)
reg('L2ChatWnd.ClearAll', 0x10148df0, 42, lambda e, c: (l2chat_obj(e, c), []))
reg('L2ChatWnd.ShowWindow', 0x10148d70, 20, lambda e, c: (l2chat_obj(e, c), []))
reg('L2ChatWnd.OnPaint', 0x10149170, 219, lambda e, c: (l2chat_obj(e, c), [e.device()]), noret=False)
reg('L2ChatWnd.OnDestroy', 0x101490d0, 159, lambda e, c: (l2chat_obj(e, c), []), noret=False)
reg('L2ChatWnd.WindowProc', 0x1014a420, 144, lambda e, c: (l2chat_obj(e, c), [c['msg'], c['wp'], c['lp']]), noret=False)

# ---- the constructors / OnCreate through the generic builders -------------------------------------------------------------------------------
for cls, (cva, csz, ckinds, methods) in CL.items():
    for (mname, va, size, kind) in methods:
        key = '%s.%s' % (cls, mname)
        if mname in ('OnCreate',) and cls in ('L2ChatWnd', 'ChatBox'):
            TESTS15[key] = (va, size, mk(cls, mname, va, size, kind), gen5, {'noret': False, 'ignore': {0x1008e8a0, 0x1014a3f0, 0x100126e0, 0x1014a320, 0x10021980}})

    def bctor(e, c, cls=cls, ckinds=ckinds):
        return poison(e, cls, cls), make_args(e, ckinds)
    TESTS15['%s.ctor' % cls] = (cva, csz, bctor, lambda r: {}, {'noret': True})

    def bdtor(e, c, cls=cls, cva=cva, csz=csz, ckinds=ckinds):
        p = poison(e, cls, cls, 0)
        e.run_real(cva, csz, p, make_args(e, ckinds))
        e.pre_len = len(e.log)
        return p, []
    for va2, (sz2, nm2, cl2) in tr.functions().items():
        if nm2 == '%s::~%s' % (cls, cls):
            TESTS15['%s.dtor' % cls] = (va2, sz2, bdtor, lambda r: {}, {'noret': True})


def b_ctor2(cls):
    def b(e, c):
        return poison(e, cls, cls), [c['n'] * 10, c['vis'], c['max']]
    return b


for cls, va, sz in (('L2ChatBox', 0x1014a320, 37), ('ChatBox', 0x10142690, 37), ('ChatFrameCtrl', 0x10142760, 37), ('L2ChatFrameCtrl', 0x1014a3f0, 37)):
    TESTS15['%s.ctor2' % cls] = (va, sz, b_ctor2(cls), gen15, {'noret': True})
TESTS15['L2ChatWnd.ctor2'] = (0x1014a5d0, 173, lambda e, c: (poison(e, 'L2ChatWnd', 'L2ChatWnd'), [e.win('NCWnd', 'Messenger')] + fsargs(e, c['fname'] or 'Zed')), gen15, {'noret': True})

if __name__ == '__main__':
    t_msn1.run_tests(TESTS15, LW15, sys.argv)
