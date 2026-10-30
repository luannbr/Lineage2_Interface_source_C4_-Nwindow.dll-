# Differential tests of the windows of lane rest1 that the console makes at the first entry into the world (wave C, lane proof2): olympiad
# (control / player / target windows), hero tower (+ content), command channel (+ info, chat command), the chat list box, the fish viewport:
# constructor, destructor, OnCreate, OnDestroy, OnPaint, WindowProc and the small handlers, each on the state OnCreate leaves; stock against
# ours under unicorn (t_msn1.py / tr.py; the tests are made from db/functions.csv).
#   python t_rest1.py [cases] [seed] [test,test,...]      (names are  Class.Method)
import sys, os, random, struct
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import tr
from tr import *
import t_msn1
from t_msn1 import *
import t_msn5
from t_msn5 import CW, gen5, mk, make_args, set_globals

REST1_FILES = {
    'NCOlympiadControlWnd': 'src/nwindow/NCOlympiadControlWnd.cpp', 'NCOlympiadPlayerWnd': 'src/nwindow/NCOlympiadPlayerWnd.cpp',
    'NCOlympiadTargetWnd': 'src/nwindow/NCOlympiadTargetWnd.cpp', 'NCHeroTowerWnd': 'src/nwindow/NCHeroTowerWnd.cpp',
    'NCHeroTowerContentWnd': 'src/nwindow/NCHeroTowerContentWnd.cpp', 'NCCommandChannelWnd': 'src/nwindow/NCCommandChannelWnd.cpp',
    'NCCommandInfoWnd': 'src/nwindow/NCCommandInfoWnd.cpp', 'NCChatCommandWnd': 'src/nwindow/NCChatCommandWnd.cpp',
    'NCChatListBox': 'src/nwindow/NCChatListBox.cpp', 'NCFishViewportWnd': 'src/nwindow/NCFishViewportWnd.cpp',
}
KIND = {'OnCreate': 'C', 'OnDestroy': 'D', 'OnPaint': 'P', 'WindowProc': 'W', 'OnTimer': 'T', 'ShowWindow': 'S', 'HideWindow': 'S', 'Reset': 'S'}
SKIP = {'IsChannelShown'}          # takes by-value structures / needs the chat tables: handled by hand or not at all


def build():
    import csv
    rows = list(csv.DictReader(open(os.path.join(tr.ROOTP, 'db', 'functions.csv'), encoding='utf-8')))
    classes = tr.classes()
    out = {}
    for cls, src in REST1_FILES.items():
        c = classes.get(cls)
        if not c:
            continue
        methods = []
        for r in rows:
            if r['src'] != src or r['status'] != 'done':
                continue
            nm = r['name']
            if not nm.startswith(cls + '::'):
                continue
            m = nm.split('::', 1)[1]
            if m == cls or m == '~' + cls or m in SKIP:
                continue
            methods.append((m, int(r['va'], 16), int(r['size']), KIND.get(m, 'N')))
        ctor = c['ctor']
        csize = tr.functions().get(ctor, (0x40,))[0]
        out[cls] = (ctor, csize, '', methods)
    return out


# what the base class' OnCreate (trapped) would have made: the scroll buttons of the list box
t_msn5.FIXPRE['NCChatListBox'] = {0x210: 'NCButton', 0x214: 'NCButton', 0x218: 'NCButton', 0x21C: 'NCButton'}
t_msn5.FIX['NCChatListBox'] = t_msn5.FIXPRE['NCChatListBox']


def user_info(e, c):
    return [c.get('oid', 77), c.get('pname', 'Player'), 5, c.get('hp', 100), 200, c.get('cp', 10), 50]


def hero_list(e, c):
    n = c.get('nhero', 2)
    out = [n]
    for i in range(n):
        out += ['Hero%d' % i, 88 + i, 'Pledge%d' % i, c.get('pid', 5), 'Ally%d' % i, c.get('aid', 6), 10 + i]
    return out


t_msn5.PSVALS.update({'NCOlympiadPlayerWnd.OnOlympiadUserInfo': user_info, 'NCOlympiadTargetWnd.OnOlympiadUserInfo': user_info,
                      'NCHeroTowerWnd.OnHeroList': hero_list})
for k in ('OnOlympiadUserInfo', 'OnHeroList'):
    KIND[k] = 'K'


def gen_r1(r):
    d = gen5(r)
    d.update(rs=r.randint(0, 10 ** 6), oid=r.choice([0, 77, 12345]), pname=r.choice(['Player', '', 'N']), hp=r.choice([0, 100, 200]), cp=r.choice([0, 10]),
             nhero=r.choice([0, 1, 3]), pid=r.choice([0, 5]), aid=r.choice([0, 6]))
    return d


class R1(CW):
    def build_rev(self):
        CW.build_rev(self)
        # NCChatCommandWnd::ClearList is our out-of-line copy of the stock's folded 0x100efdc0 (mov ecx,[ecx+0xE8]; jmp [vtable+0x134]): a call of it
        # is a call of that stock function (its body is one virtual call, read against the stock's)
        a = self.symbols.get('?ClearList@NCChatCommandWnd@@QAEXXZ')
        if a:
            self.rev[a] = 0x100efdc0

    def __init__(self, kind):
        CW.__init__(self, kind)
        self.rules[0x1002d4b0] = lambda e, ecx, a: e.r32(ecx, 4) if a[0] < e.r32(ecx, 8) else 0     # NString::GetAt(index)
        self.rules[0x1002e4a0] = lambda e, ecx, a: e.split                                        # NCTabControl::IsPageSplit(page)
        self.rules[0x100562f0] = lambda e, ecx, a: a[0]                                           # the checked cast (cdecl): the object
        self.rules[0x10011740] = lambda e, ecx, a: getattr(e, 'expanded', 0)                      # NFrameExpandFns::IsExpanded
        self.rules[0x10022f70] = lambda e, ecx, a: getattr(e, 'sel_row', 0)                       # NCListCtrl::GetSelectedRow
        self.rules[0x1000c230] = lambda e, ecx, a: getattr(e, 'commit', 0)                        # NCEditBox::CommitText (virtual)
        self.transparent |= {0x1013d0f0}                                                          # GetPlayerID: reads the window's ObjectID
        self.rules[0x10073e80] = lambda e, ecx, a: getattr(e, 'me_user', 0)                       # NConsoleWnd::GetMyUser
        self.rules[0x1006a8e0] = lambda e, ecx, a: (0x7A00 + a[0]) if a[0] and getattr(e, 'crest', (1, 1))[0] else 0   # GetPledgeCrestTexture(id)
        self.rules[0x1006a8f0] = lambda e, ecx, a: (0x7B00 + a[0]) if a[0] and getattr(e, 'crest', (1, 1))[1] else 0   # GetAllyCrestTexture(id)
        # the console's system message formatter (vslot 191: ParamStack, 1, Text, 0): the text it makes (it is read by what follows)
        self.rules[0x10081700] = self.fmt_sysmsg
        self.write32(self.console, self.class_vt('NConsoleWnd'))      # the console answers its virtuals (trapped)
        # the bases' destructors (the stock calls them, ours expands them): run for real on both sides
        self.transparent |= {0x10021a70, 0x10120c60, 0x10021850, 0x10002fb0, 0x10002d00, 0x10002e30, 0x10002350, 0x100022c0, 0x10020890}


def _fmt_sysmsg(self, e, ecx, a):
    ids = e.ps.get(a[0], [])
    text = 'SM%s' % (ids[0] if ids else '?')
    e.uc.mem_write(a[2], text.encode('utf-16le') + bytes(2))
    return 0


R1.fmt_sysmsg = lambda self, e, ecx, a: _fmt_sysmsg(self, e, ecx, a)


# ---- NCChatListBox::OnPaint: the chat window's lines, painted every frame --------------------------------------------------------------
def chatlist_obj(e, c):
    w = e.win('NCChatListBox', 'ChatList', c.get('w', 348.0), c.get('h', 120.0))
    parent = e.win('NCChatWnd', 'ChatWnd', 400, 200)
    tab = e.win('NCTabControl', 'TabCtrl')
    e.w32(parent, 0x1A4, tab)
    pent = e.raw_obj('pent', 0x40)
    e.w32(e.r32(w, 0x74), 0xC, pent)
    e.w32(pent, 4, parent)
    e.w32(w, 0x218, e.win('NCSliderBar', 'ScrollBar', 15, 80))
    for off in (0x278, 0x27C, 0x280):
        e.w32(w, off, 0x7700 + off if c.get('tex', 1) else 0)
    n = c.get('nitems', 0)
    data = e.alloc(0xC * max(n, 1) + 16)
    e.uc.mem_write(data, bytes(0xC * max(n, 1) + 16))
    for i in range(n):
        ns = e.raw_obj('NString%d' % i, 0x14)
        text = c.get('texts', [])[i] if i < len(c.get('texts', [])) else 'line%d' % i
        e.w32(ns, 0, len(text) + 1)
        e.w32(ns, 4, e.put_wstr(text))
        e.w32(ns, 8, len(text))
        e.w32(ns, 0xC, 0xFF000000 | (i * 0x111111 & 0xFFFFFF))
        e.w32(ns, 0x10, c.get('indents', [0] * 50)[i])
        e.write32(data + 0xC * i, ns)
    e.write32(w + 0x204, data if n else 0)
    e.write32(w + 0x208, n)
    e.write32(w + 0x20C, n)
    e.w32(w, 0x21C, c.get('vis', 5))
    e.w32(w, 0x224, c.get('top', 0))
    e.w32(w, 0x228, c.get('numlines', n))
    e.w32(w, 0x230, 23)
    e.w32(w, 0x234, 14)
    e.w32(w, 0x238, c.get('gap', 2))
    e.split = c.get('split', 0)
    return w


def gen_chatlist(r):
    n = r.choice([0, 1, 3, 8, 15])
    return dict(w=r.choice([348.0, 200.5]), h=r.choice([120.0, 36.0, 200.0, 10.0]), nitems=n, tex=r.choice([0, 1]), vis=r.choice([0, 5, 8, 11]),
                top=r.choice([0, 1, 4]), numlines=r.choice([n, max(n - 1, 0), n + 3]), gap=r.choice([0, 2, 5]), split=r.choice([0, 1]),
                indents=[r.choice([0, 0, 12, 24]) for _ in range(20)], texts=[r.choice(['hello', 'x', '', 'a longer chat line']) for _ in range(20)])


def b_chatlist_paint(e, c):
    return chatlist_obj(e, c), [e.device()]


# ---- NCFishViewportWnd::OnPaint (the fishing window): every field the paint reads ------------------------------------------------------
def fish_obj(e, c):
    w = e.win('NCFishViewportWnd', 'Fish', c.get('w', 256.0), c.get('h', 300.0))
    for off in (0xF8, 0xFC, 0x100, 0x104, 0x108, 0x10C, 0x110):
        e.w32(w, off, 0x7800 + off if c.get('tex', 1) else 0)
    e.w32(w, 0x114, c.get('maxlife', 100))
    e.w32(w, 0x118, c.get('life', 40))
    e.w32(w, 0x120, c.get('time', 25))
    e.w32(w, 0x124, c.get('fight', 1))
    e.w32(w, 0x128, c.get('ealpha', 200))
    e.w32(w, 0x12C, c.get('effect', 1))
    e.fs_set(w + 0x138, c.get('msg', 'caught'))
    e.w32(w, 0x144, c.get('delta', 5))
    e.fs_set(w + 0x148, c.get('dtext', '+5'))
    e.wf(w, 0x154, c.get('fade', 0.5))
    e.w32(w, 0x158, c.get('red', 0))
    e.w32(w, 0x15C, c.get('hint', 1))
    e.wf(w, 0x160, c.get('htimer', 0.05))
    e.write32(w + 0x165, c.get('alt', 0))
    return w


def gen_fish(r):
    return dict(w=r.choice([256.0, 300.5]), h=r.choice([300.0, 200.0]), tex=r.choice([0, 1, 1]), maxlife=r.choice([0, 100, 7]),
                life=r.choice([0, 40, 100, 120]), time=r.choice([0, 25, 300]), fight=r.choice([0, 1]), ealpha=r.choice([0, 100, 300]),
                effect=r.choice([0, 1]), msg=r.choice(['', 'caught', 'x y']), delta=r.choice([-5, 0, 5]), dtext=r.choice(['', '+5', '-12']),
                fade=r.choice([0.0, 0.3, 0.9999, 1.0, 1.5, -1.0]), red=r.choice([0, 1]), hint=r.choice([0, 1]),
                htimer=r.choice([0.0, 0.05, 0.1, 0.15, 0.25, 0.35, 0.45, 2.0, -0.5]), alt=r.choice([0, 1]))


def b_fish_paint(e, c):
    return fish_obj(e, c), [e.device()]


# ---- the state a window has some time after OnCreate: the numbers the packets set, the selected row, the typed text ----------------------
def mk_sel_row(e, r, cell_text):
    row = e.raw_obj('SelRow', 0x890)
    cells = e.alloc(7 * 0x1A64 + 16)
    e.uc.mem_write(cells, bytes(7 * 0x1A64 + 16))
    e.uc.mem_write(cells, cell_text.encode('utf-16le') + bytes(2))
    e.w32(row, 0, cells)
    e.w32(row, 0x804, r.choice([0, 88, 123]))
    e.sel_row = row if r.choice([0, 1, 1]) else 0


def rs_olyplayer(e, p, c):
    r = random.Random(c.get('rs', 0))
    e.expanded = r.choice([0, 1])
    for off in (0x11C, 0x120, 0x124, 0x128):
        e.w32(p, off, r.choice([0, 1, 100, 337, 600]))
    e.w32(p, 0x12C, r.choice([0, 77, 12345]))
    e.w32(p, 0x130, r.randint(0, 4))
    e.fs_set(p + 0x110, r.choice(['', 'Player', 'A long competitor name']))
    for i in range(5):
        e.fs_set(p + 0x134 + 12 * i, r.choice(['', 'msg%d' % i, 'x']))
    desk = e.r32(e.console, 0x3BF4)
    t1, t2 = e.win('NCWnd', 'Target1'), e.win('NCWnd', 'Target2')
    side = r.choice([0, 1, 2])
    if side:
        e.w32(desk, 0x210 + 4 * side + 0, p)
    e.w32(desk, 0x21C, t1)
    e.w32(desk, 0x220, t2)


def rs_olytarget(e, p, c):
    r = random.Random(c.get('rs', 0))
    for off in (0x114, 0x118, 0x11C, 0x120):
        e.w32(p, off, r.choice([0, 1, 100, 337, 600]))
    e.fs_set(p + 0x108, r.choice(['', 'Opponent', 'A long competitor name']))


def rs_hero(e, p, c):
    r = random.Random(c.get('rs', 0))
    mk_sel_row(e, r, 'hero')
    e.crest = (r.choice([0, 1]), r.choice([0, 1]))
    me = r.choice([0, 1, 1])
    if me:
        u = e.raw_obj('MyUser', 0x400)
        e.w32(u, 0x240, r.choice([0, 1, 5]))
        e.me_user = u
    else:
        e.me_user = 0


def rs_cmdinfo(e, p, c):
    r = random.Random(c.get('rs', 0))
    mk_sel_row(e, r, r.choice(['Leader', '', 'Name with spaces']))
    e.uc.mem_write(p + 0x114, bytes([r.choice([0, 1])]))
    e.fs_set(p + 0xFC, r.choice(['', 'Master']))
    e.w32(p, 0x108, r.choice([0, 2, 5]))
    e.w32(p, 0x10C, r.choice([0, 9, 45]))


def rs_cmdchannel(e, p, c):
    r = random.Random(c.get('rs', 0))
    text = r.choice(['', 'hi', 'x' * 254, 'y' * 255, 'z' * 300, None])
    e.commit = e.put_wstr(text) if text is not None else 0
    chk = e.r32(p, 0xFC)
    if chk:
        e.w32(chk, 0x294, r.choice([0, 1]))


def b_onframe_args(base):
    def b(e, c):
        p, _ = base(e, c)
        return p, [random.Random(c.get('rs', 0) + 3).choice([0xFFFF, 0xFFFE, 0xFFFF, 0xFFFE, 0, 7])]
    return b


t_msn5.RESTATE.update({'NCOlympiadPlayerWnd': rs_olyplayer, 'NCOlympiadTargetWnd': rs_olytarget, 'NCHeroTowerWnd': rs_hero, 'NCCommandInfoWnd': rs_cmdinfo,
                       'NCCommandChannelWnd': rs_cmdchannel})

t_msn5.CLASSES.update(build())
TESTSR = {}
for cls, (cva, csz, ckinds, methods) in build().items():
    if not any(m[0] == 'OnCreate' for m in methods):
        continue
    for (mname, va, size, kind) in methods:
        TESTSR['%s.%s' % (cls, mname)] = (va, size, mk(cls, mname, va, size, kind), gen_r1, {'noret': kind not in ('C', 'D', 'P', 'W', 'T')})

    def bctor(e, c, cls=cls, ckinds=ckinds):
        return poison(e, cls, cls), make_args(e, ckinds)
    TESTSR['%s.ctor' % cls] = (cva, csz, bctor, lambda r: {}, {'noret': True})

    def bdtor(e, c, cls=cls, cva=cva, csz=csz, ckinds=ckinds):
        p = poison(e, cls, cls, 0)           # zero filled: the base constructors (trapped) leave the arrays as they found them
        e.run_real(cva, csz, p, make_args(e, ckinds))
        e.pre_len = len(e.log)
        return p, []
    for va2, (sz2, nm2, cl2) in tr.functions().items():
        if nm2 == '%s::~%s' % (cls, cls):
            TESTSR['%s.dtor' % cls] = (va2, sz2, bdtor, lambda r: {}, {'noret': True})

# OnAttack(Attacker, Victim, P3, P4, P5): real User records (ObjectID +0x18, the name from +0x1C)
_base_attack = TESTSR['NCOlympiadPlayerWnd.OnAttack'][2]


def b_attack(e, c):
    p, _ = _base_attack(e, c)
    r = random.Random(c.get('rs', 0) + 1)
    me = 77
    e.w32(p, 0x12C, me)
    role = r.choice(['att', 'vic', 'none', 'both'])

    def user(label, oid):
        u = e.raw_obj(label, 0x400)
        e.w32(u, 0x18, oid)
        e.uc.mem_write(u + 0x1C, 'UserName'.encode('utf-16le') + bytes(2))
        return u
    att = user('Attacker', me if role in ('att', 'both') else 5)
    vic = user('Victim', me if role in ('vic', 'both') else 6)
    return p, [att, vic, r.choice([0, 1]), r.choice([0, 1]), r.choice([0, 1])]


_fr = TESTSR['NCOlympiadPlayerWnd.OnFrame']
TESTSR['NCOlympiadPlayerWnd.OnFrame'] = _fr[:2] + (b_onframe_args(_fr[2]),) + _fr[3:]
TESTSR['NCOlympiadPlayerWnd.OnAttack'] = TESTSR['NCOlympiadPlayerWnd.OnAttack'][:2] + (b_attack,) + TESTSR['NCOlympiadPlayerWnd.OnAttack'][3:]
TESTSR['NCFishViewportWnd.OnPaint'] = (0x101204a0, 1212, b_fish_paint, gen_fish, {'noret': False})
# the stock asks for the first character of the line twice (a pure getter), ours once
# AddMsg takes its text by value (a ring of five lines at +0x134, the next index right after it)
import t_msn3
_base_addmsg = TESTSR['NCOlympiadPlayerWnd.AddMsg'][2]


def b_addmsg(e, c):
    p, _ = _base_addmsg(e, c)
    e.w32(p, 0x130, c.get('next', 0))
    return p, t_msn3.fsargs(e, c.get('text', 'msg'))


def gen_addmsg(r):
    d = gen_r1(r)
    d.update(next=r.choice([0, 1, 4]), text=r.choice(['msg', '', 'a longer message']))
    return d


TESTSR['NCOlympiadPlayerWnd.AddMsg'] = (0x1013d030, 128, b_addmsg, gen_addmsg, {'noret': True})
# the stock passes 0 as the first argument of BOTH AddColumn calls, ours 0 and 1: the argument is only compared with the column limit (2): masked
TESTSR['NCCommandInfoWnd.OnCreate'] = TESTSR['NCCommandInfoWnd.OnCreate'][:4] + ({'noret': False, 'mask': {0x10023ee0: {0}}},)
TESTSR['NCChatListBox.OnPaint'] = (0x10056530, 773, b_chatlist_paint, gen_chatlist, {'noret': False, 'ignore': {0x1002d4b0}})

if __name__ == '__main__':
    t_msn1.run_tests(TESTSR, R1, sys.argv)
