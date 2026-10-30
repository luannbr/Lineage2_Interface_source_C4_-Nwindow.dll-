# Differential tests of the windows MsnWnd::OnCreate makes on the first entry into the world (wave C, lane proof2): LoginWnd, VersionWnd,
# LoadingWnd, OptionWnd, StatusWnd, AlarmWnd, the nine dialogs and the person tree: constructor, destructor, OnCreate, OnDestroy and the
# paints / handlers (run on the state OnCreate leaves), stock against ours under unicorn (t_msn1.py / tr.py).
#   python t_msn5.py [cases] [seed] [test,test,...]      (names are  Class.Method)
import sys, os, random, struct
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import tr
from tr import *
import t_msn1
from t_msn1 import *
import t_msn2
from t_msn2 import *
import t_msn3
import t_msn4
from t_msn4 import FW
from t_msn3 import *

TESTS5 = {}

# class: (ctor va, ctor size, ctor args, size of the object (classes.json), methods)
# method kinds: C create, D destroy, P paint(device), W window proc(msg, wparam, lparam), N plain (the arguments are small numbers, as many
# as the stock pops), T timer(id), M mouse(x, y), S show/hide (no arguments)
CLASSES = {
    'LoginWnd': (0x1015bf90, 74, 'M', [('OnCreate', 0x10159480, 1151, 'C'), ('OnDestroy', 0x10156320, 239, 'D'), ('OnPaint', 0x10156410, 460, 'P'),
                                       ('OnLoginButton', 0x101565e0, 209, 'N'), ('OnClearButton', 0x10155690, 38, 'N'),
                                       ('OnVersionButton', 0x101566c0, 29, 'N'), ('OnEditNotify', 0x10159900, 20, 'N'), ('WindowProc', 0x1015af20, 172, 'W')]),
    'VersionWnd': (0x1015c060, 44, 'M', [('OnCreate', 0x10159980, 231, 'C'), ('OnPaint', 0x10156720, 336, 'P'), ('OnOKButton', 0x10156870, 29, 'N'),
                                         ('WindowProc', 0x10159920, 83, 'W')]),
    'LoadingWnd': (0x1015c110, 50, 'M', [('OnCreate', 0x101556c0, 35, 'C'), ('ShowWithMode', 0x101556f0, 18, 'N'), ('OnPaint', 0x10156890, 279, 'P')]),
    'OptionWnd': (0x1015c2d0, 120, 'Mm', [('OnCreate', 0x1015a0d0, 2247, 'C'), ('OnDestroy', 0x10157270, 547, 'D'), ('OnPaint', 0x101574a0, 292, 'P'),
                                          ('LoadSettings', 0x101575d0, 246, 'N'), ('OnOKButton', 0x101576d0, 416, 'N'), ('ShowWindow', 0x1015a9a0, 16, 'S'),
                                          ('OnAddGroupButton', 0x1015a9b0, 17, 'N'), ('OnRenameGroupButton', 0x1015a9d0, 17, 'N'),
                                          ('OnRemoveGroupButton', 0x1015a9f0, 17, 'N'), ('WindowProc', 0x1015b060, 195, 'W')]),
    'AlarmWnd': (0x1015e150, 162, 'M0', [('OnCreate', 0x10155710, 35, 'C'), ('OnDestroy', 0x10155740, 71, 'D'), ('OnTimer', 0x10155790, 191, 'T'),
                                         ('ShowWindow', 0x10155850, 86, 'S'), ('HideWindow', 0x101558b0, 95, 'S'), ('OnPaint', 0x101569b0, 1109, 'P'),
                                         ('OnLButtonDown', 0x1015e2c0, 100, 'M')]),
    'StatusWnd': (0x1015c1d0, 99, 'Mm', [('OnCreate', 0x1015afd0, 138, 'C'), ('OnDestroy', 0x10159ea0, 129, 'D'), ('OnPaint', 0x10156e10, 1110, 'P'),
                                         ('BuildItems', 0x10159a70, 1071, 'N'), ('OnMouseMove', 0x10155a10, 110, 'M'), ('KillFocus', 0x10155a80, 29, 'S'),
                                         ('OnLButtonUp', 0x10159f30, 401, 'M')]),
    'AddFriendDialog': (0x10148530, 50, 'm', [('OnCreate', 0x101468f0, 591, 'C'), ('OnDestroy', 0x10145f20, 159, 'D'), ('OnPaint', 0x101454f0, 245, 'P'),
                                              ('OnOK', 0x10145170, 83, 'N'), ('ShowWindow', 0x101451d0, 23, 'S'), ('WindowProc', 0x10145480, 110, 'W')]),
    'RemoveFriendDialog': (0x101485f0, 123, 'Mm', [('OnCreate', 0x10146bb0, 575, 'C'), ('OnDestroy', 0x101455f0, 159, 'D'), ('OnPaint', 0x10145690, 322, 'P'),
                                                   ('OnOK', 0x10145800, 147, 'N'), ('ShowWindow', 0x101457e0, 26, 'S'), ('WindowProc', 0x10146b40, 110, 'W')]),
    'MoveGroupDialog': (0x101487f0, 113, 'm', [('OnCreate', 0x10147540, 567, 'C'), ('OnDestroy', 0x10145cb0, 159, 'D'), ('OnPaint', 0x10145d50, 245, 'P'),
                                               ('OnOK', 0x10145e50, 84, 'N'), ('ShowWindow', 0x10148220, 183, 'S'), ('WindowProc', 0x101474d0, 110, 'W')]),
    'AddedConfirmDialog': (0x10148700, 86, 'm', [('OnCreate', 0x10146df0, 982, 'C'), ('OnDestroy', 0x101458a0, 239, 'D'), ('OnPaint', 0x10145990, 626, 'P'),
                                                 ('ShowWindow', 0x10145c10, 55, 'S'), ('HideWindow', 0x101471d0, 101, 'S'), ('OnSelectBlock', 0x10145c50, 48, 'N'),
                                                 ('OnSelectAllow', 0x10145c80, 46, 'N'), ('OnOK', 0x10147240, 227, 'N'), ('OnCancel', 0x10147330, 156, 'N'),
                                                 ('WindowProc', 0x10148150, 195, 'W')]),
    'OfflineConfirmDialog': (0x10148470, 50, 'M', [('OnCreate', 0x10146740, 422, 'C'), ('OnDestroy', 0x101452c0, 111, 'D'), ('OnPaint', 0x10145330, 284, 'P'),
                                                   ('OnOK', 0x10145150, 31, 'N'), ('WindowProc', 0x10145250, 110, 'W')]),
    'AddGroupDialog': (0x10148900, 56, 'm', [('OnCreate', 0x10147780, 591, 'C'), ('OnPaint', 0x10145fc0, 245, 'P'), ('OnOK', 0x101451f0, 81, 'N'),
                                             ('WindowProc', 0x10145eb0, 110, 'W')]),
    'RenameGroupDialog': (0x101489c0, 62, 'm', [('OnCreate', 0x10147a40, 733, 'C'), ('OnDestroy', 0x101460c0, 195, 'D'), ('OnPaint', 0x10146190, 245, 'P'),
                                                ('OnOK', 0x10146290, 114, 'N'), ('ShowWindow', 0x101482e0, 202, 'S'), ('WindowProc', 0x101479d0, 110, 'W')]),
    'RemoveGroupDialog': (0x10148a80, 66, 'Mm', [('OnCreate', 0x10147d90, 567, 'C'), ('OnDestroy', 0x10146310, 159, 'D'), ('OnPaint', 0x101463b0, 245, 'P'),
                                                 ('OnOK', 0x101464b0, 124, 'N'), ('ShowWindow', 0x101483b0, 183, 'S'), ('WindowProc', 0x10147d20, 110, 'W')]),
    'MessageDialog': (0x10148b50, 98, '', [('OnCreate', 0x10147fd0, 228, 'C'), ('OnDestroy', 0x10146590, 63, 'D'), ('OnPaint', 0x101465d0, 222, 'P'),
                                           ('WindowProc', 0x10146530, 83, 'W')]),
    'MsnTreeCtrl': (0x10168260, 54, '', [('OnCreate', 0x101671c0, 205, 'C'), ('OnDestroy', 0x10166570, 63, 'D'), ('OnPaint', 0x101665b0, 160, 'P'),
                                         ('Clear', 0x10166690, 143, 'N'), ('ScrollUp', 0x10166720, 185, 'N'), ('ScrollDown', 0x101667e0, 193, 'N'),
                                         ('OnScrollBar', 0x101668b0, 125, 'N'), ('OnMouseWheel', 0x10166930, 291, 'W')]),
    'L2FriendTreeCtrl': (0x101683c0, 28, '', [('OnCreate', 0x10168460, 255, 'C'), ('OnDestroy', 0x10166bc0, 63, 'D'), ('OnPaint', 0x10166c00, 160, 'P'),
                                              ('Clear', 0x10166ce0, 143, 'N'), ('ScrollUp', 0x10166d70, 185, 'N'), ('ScrollDown', 0x10166e30, 193, 'N'),
                                              ('OnScrollBar', 0x10166f00, 125, 'N'), ('OnMouseWheel', 0x10166f80, 291, 'W'), ('WindowProc', 0x10166b60, 81, 'W'),
                                              ('OnDoubleClick', 0x10166530, 52, 'M')]),
}


class CW(FW):
    def __init__(self, kind):
        FW.__init__(self, kind)
        # the groups' array copies (the stock has two copies of the pointer array's destructor, ours calls one) and the tree control's destructor
        self.transparent |= {0x101480c0, 0x1015ae00, 0x10034250, 0x10015950}
        # the group dialogs' combo box: the store's group array is copied (the stock calls the array's copy constructor, ours expands it), the
        # getters of the store and of the group read for real
        self.transparent |= {0x101466b0, 0x10152c00}
        # the stock's getters are two (0x1014FC10 the store, 0x10151160 the groups) and ours calls its own: they are not compared, the values come
        # from the memory (the four-byte bodies cannot be redirected)
        self.rules[0x1014fc10] = lambda e, ecx, a: e.r32(ecx, 0x50)
        self.rules[0x10151160] = lambda e, ecx, a: e.r32(ecx, 0x10)
        self.rules[0x1012c6c0] = lambda e, ecx, a: e.r32(ecx, 0x10)
        self.rules[0x101528b0] = lambda e, ecx, a: e.r32(ecx, 0x18)
        self.rules[0x1000c230] = lambda e, ecx, a: getattr(e, 'commit', 0)                         # NCEditBox::CommitText (virtual)
        # NComboItemFns::GetItemData(index) (0x10006410) is a function of its own in the stock and an out-of-line copy in ours that is not a hooked address:
        # it runs for real on both sides, on the item records rs_groupdlg builds in the combo box (12 bytes each at [+0x1FC], the count at [+0x200], the data
        # word is the third dword, read through the folded four-byte getter 0x10005b90, which t_msn3's MW answers from the memory: [ecx+8])
        self.transparent.add(0x10006410)
        self.rules[0x10166a60] = lambda e, ecx, a: getattr(e, 'group_node', 0)                     # NMsnTreeCtrlView::FindGroupNode(id)
        self.rules[0x10152860] = self.group_name                                                    # NMsnGroup::GetName: the name decoded, at +0xC

        # GConfig->SetBool(section, key, value, file): virtual 9 of the engine's config cache (Option.ini)
        cfg = self.alloc(16)
        self.write32(cfg, self.perm_vt({9: 0x1FFF0100}))
        self.names[0x1FFF0100] = (0, 'FConfigCache::SetBool', '')
        cell = self.gcell.get('?GConfig@@3PAVFConfigCache@@A')
        if cell:
            self.write32(cell, cfg)

    def reset_case(self, c):
        FW.reset_case(self, c)
        self.item_recs = None

    def group_name(self, e, ecx, a):
        e.fs_set(ecx + 0xC, e.fs_get(ecx))
        return ecx + 0xC


def make_args(e, kinds):
    a = []
    for k in kinds:
        if k == 'M':
            a.append(e.win('MsnWnd', 'OwnerMsnWnd'))
        elif k == 'm':
            m = e.raw_obj('Msn', 0x58)
            e.w32(m, 0x4C, 1)
            a.append(m)
        else:
            a.append(0)
    return a


# what a base class' OnCreate (trapped here) would have made: {class: {offset: size of the object stored there}}
FIX = {'MsnTreeCtrl': {0x114: 0x300}, 'L2FriendTreeCtrl': {0x114: 0x300}}
FIXPRE = {}        # the same, but needed BEFORE the class' own OnCreate runs (its code reads what the base made)


def apply_fix(e, p, cls, table=None):
    for off, n in (table if table is not None else FIX).get(cls, {}).items():
        if isinstance(n, str):
            e.w32(p, off, e.win(n, 'Fix%x' % off, 20, 20))
        else:
            e.w32(p, off, e.raw_obj('Fix%x' % off, n))


def new_state(e, c, cls):
    """a window as OnCreate leaves it: the constructor then OnCreate, both run for real (the stock's on the stock side)"""
    cva, csz, ckinds, methods = CLASSES[cls]
    n = tr.classes()[cls]['size']
    p = poison(e, cls, cls)
    e.write32(e.console + 0x3BF4, e.new_obj('NCWnd', 'Desktop'))
    e.wf(p, W['Width'], c.get('w', 256.0))
    e.wf(p, W['Height'], c.get('h', 353.0))
    e.w32(p, W['ScreenX'], c.get('sx', 100))
    e.w32(p, W['ScreenY'], c.get('sy', 50))
    ent = e.raw_obj('entry:' + cls, 0x40)
    e.w32(p, W['Entry'], ent)
    args = make_args(e, ckinds)
    e.run_real(cva, csz, p, args)
    apply_fix(e, p, cls, FIXPRE)
    # the constructor set its own fields (it does not touch Width/Height/Entry: NCWnd's constructor is trapped)
    e.wf(p, W['Width'], c.get('w', 256.0))
    e.wf(p, W['Height'], c.get('h', 353.0))
    e.w32(p, W['ScreenX'], c.get('sx', 100))
    e.w32(p, W['ScreenY'], c.get('sy', 50))
    e.w32(p, W['Entry'], ent)
    return p


def gen5(r):
    return dict(wpseed=r.randint(0, 10 ** 6), w=r.choice([256.0, 420.5, 150.0]), h=r.choice([353.0, 168.0]), sx=r.choice([100, 0]), sy=r.choice([50, 0]),
                a0=r.choice([0, 1, 5]), a1=r.choice([0, 1, 7]), a2=r.choice([0, 3]), x=r.choice([3, 30, 90]), y=r.choice([3, 20, 60]),
                msg=r.choice([1, 0x501, 0x201, 0x203, 0x111, 5]), wp=r.choice([0, 1, 0xA00, 0xA10, 0xA52, 0xA04]), tid=r.choice([0xA00, 0xA55, 5, 0xA60]),
                basewp=r.choice([0, 0, 1]), shown=r.choice([0, 1]), presence=r.choice(PRESENCE), blocked=r.choice([0, 1]),
                dontdist=r.choice([0, 1]), mail=r.choice([0, 1]), loginalarm=r.choice([0, 1]), hideall=r.choice([0, 1]))


def set_globals(e, c):
    e.write32(e.gcell['?GMsnHideAll@@3HA'], c.get('hideall', 0))
    e.write32(e.gcell['?GMsnNewEMailAlarm@@3HA'], c.get('mail', 1))
    e.write32(e.gcell['?GMsnLoginAlarm@@3HA'], c.get('loginalarm', 1))
    e.write32(e.gcell['?GMsnDontDisturb@@3HA'], c.get('dontdist', 0))
    e.write32(e.glob(0x10322c68), c.get('blocked', 0))


def mk_msn_store(e, r):
    """the protocol object the group dialogs read: +0x50 the store, its +0x10 the array of the groups (pointers, some NULL)"""
    msn = e.raw_obj('Msn', 0x100)
    store = e.raw_obj('Store', 0x40)
    e.w32(msn, 0x50, store)
    e.w32(msn, 0x4C, 1)
    n = r.choice([0, 1, 2, 4])
    garr = e.raw_obj('GroupsArr', 0xC)
    data = e.alloc(4 * max(n, 1) + 16)
    for i in range(n):
        g = 0
        if r.choice([1, 1, 1, 0]):
            g = e.raw_obj('Group%d' % i, 0x1C)
            e.fs_set(g, r.choice(['grp%d' % i, 'Friends', 'A%20B']))
            e.w32(g, 0x18, r.choice([0, 1, 2, 7]) + i)
        e.write32(data + 4 * i, g)
    e.write32(garr, data if n else 0)
    e.write32(garr + 4, n)
    e.write32(garr + 8, n)
    e.w32(store, 0x10, garr)
    return msn


def rs_groupdlg(msn_off, combo_off, wnd=False):
    def f(e, p, c):
        r = random.Random(c.get('wpseed', 0) * 13 + 7)
        e.w32(p, msn_off, mk_msn_store(e, r))
        if wnd:
            w = e.win('MsnWnd', 'OwnerMsnWnd')
            tv = e.raw_obj('TreeView', 0x40)
            e.w32(w, 0x17C, tv)
            e.w32(p, 0x1F4, w)
            node = e.raw_obj('GroupNode', 0x2870)
            e.w32(node, 0x2864, r.choice([0, 0, 1, 3]))
            e.group_node = node
        combo = e.r32(p, combo_off)
        if combo:
            e.w32(combo, 0x240, r.choice([-1, 0, 1, 2]))
        e.item_data = r.choice([0, 1, 5, 9])
        if combo:
            n_ = r.choice([0, 1, 2, 3])
            recs = e.alloc(12 * max(n_, 1) + 16)
            e.uc.mem_write(recs, bytes(12 * max(n_, 1) + 16))
            for i_ in range(n_):
                e.write32(recs + 12 * i_ + 8, r.choice([0, 1, 5, 9]))
            e.w32(combo, 0x1FC, recs)
            e.w32(combo, 0x200, n_)
            e.item_recs = (recs, recs + 12 * max(n_, 1))
        e.commit = e.put_wstr(r.choice(['', 'NewName', 'a b'])) if r.choice([0, 1, 1]) else 0
    return f


tr.POP_OVERRIDE[0x1FFF0100] = 0x10


def rs_option(e, p, c):
    r = random.Random(c.get('wpseed', 0) * 17 + 3)
    for off in (0x100, 0x104, 0x108, 0x10C, 0x110, 0x114):
        box = e.r32(p, off)
        if box:
            e.w32(box, 0x294, r.choice([0, 1]))
    e.commit = e.put_wstr(r.choice(['', 'My name', 'x'])) if r.choice([0, 1, 1]) else 0


RESTATE = {'OptionWnd': rs_option, 'MoveGroupDialog': rs_groupdlg(0x1F4, 0x23C), 'RenameGroupDialog': rs_groupdlg(0x1F4, 0x1F8), 'RemoveGroupDialog': rs_groupdlg(0x1F8, 0x1FC, True)}
# RESTATE: class -> function(e, p, c): the state a window has some time after OnCreate (the numbers, the lists, the selected row)
PSVALS = {}       # 'Class.Method' -> function(e, c) -> the values of the packet in the order they are read (an int, or a str = a heap wide string)


def pstack(e, c, cls, mname):
    vals = PSVALS['%s.%s' % (cls, mname)](e, c)
    p = e.raw_obj('ParamStack', 0x20)
    e.ps[p] = [(e.put_wstr(v) if isinstance(v, str) else (v & 0xFFFFFFFF)) for v in vals]
    return p


def small_callees(va, cls):
    """the stock functions of the same class (<= 64 bytes) a window procedure calls: ours may inline them, so they run for real (they are proved by their own tests)"""
    out = set()
    asm = os.path.join(tr.ROOTP, 'analysis', 'ghidra', 'asm', '%08x.asm' % va)
    import re as _re
    if os.path.exists(asm):
        for ln in open(asm, encoding='utf-8', errors='replace'):
            m = _re.match(r'^[0-9a-f]{8}\s+[0-9a-f]+\s+CALL\s+(0x[0-9a-f]{8})', ln)
            if m:
                t = int(m.group(1), 16)
                f = tr.functions().get(t)
                if f and f[0] <= 64 and f[1].startswith(cls + '::'):
                    out.add(t)
    return out


_WPC = {}


def wp_constants(va, size):
    """the immediates the window procedure compares its message and parameter with (so that the cases reach its branches)"""
    if va not in _WPC:
        base, mem = tr.stock_bytes()
        out = set()
        asm = os.path.join(tr.ROOTP, 'analysis', 'ghidra', 'asm', '%08x.asm' % va)
        import re as _re
        if os.path.exists(asm):
            for ln in open(asm, encoding='utf-8', errors='replace'):
                m = _re.match(r'^[0-9a-f]{8}\s+[0-9a-f]+\s+(?:CMP|SUB)\s+[^,]+,(-?0x[0-9a-f]+)\s*$', ln)
                if m:
                    v = int(m.group(1), 16)
                    if 0 < v < 0x100000000:       # the messages (small) and the ids of the controls (0xAFDDCEA0 ...)
                        out.add(v)
        _WPC[va] = sorted(out)
    return _WPC[va]


def mk(cls, mname, va, size, kind):
    def b(e, c):
        set_globals(e, c)
        if kind == 'C':
            p = poison(e, cls, cls)
            e.write32(e.console + 0x3BF4, e.new_obj('NCWnd', 'Desktop'))
            e.wf(p, W['Width'], c.get('w', 256.0))
            e.wf(p, W['Height'], c.get('h', 353.0))
            e.w32(p, W['ScreenX'], c.get('sx', 100))
            e.w32(p, W['ScreenY'], c.get('sy', 50))
            # the constructor ran first (a poisoned object would make OnCreate read garbage that the stock never sees)
            cva, csz, ckinds, methods = CLASSES[cls]
            args = make_args(e, ckinds)
            e.run_real(cva, csz, p, args)
            apply_fix(e, p, cls, FIXPRE)
            e.pre_len = len(e.log)
            e.wf(p, W['Width'], c.get('w', 256.0))
            e.wf(p, W['Height'], c.get('h', 353.0))
            e.w32(p, W['ScreenX'], c.get('sx', 100))
            e.w32(p, W['ScreenY'], c.get('sy', 50))
            e.w32(p, W['Entry'], e.raw_obj('entry:' + cls, 0x40))
            return p, []
        p = new_state(e, c, cls)
        cva, csz, ckinds, methods = CLASSES[cls]
        oc = [m for m in methods if m[0] == 'OnCreate'][0]
        e.run_real(oc[1], oc[2], p)
        apply_fix(e, p, cls)
        if cls in RESTATE:
            RESTATE[cls](e, p, c)
        e.pre_len = len(e.log)
        e.presence = c.get('presence', '')
        if c.get('shown'):
            for pp, nm in list(e.objs.items()):
                if pp != p:
                    e.shown[pp] = 1
        if kind == 'D' or kind == 'S':
            return p, []
        if kind == 'K':
            return p, [pstack(e, c, cls, mname)]
        if kind == 'P':
            return p, [e.device()]
        if kind == 'W':
            e.transparent |= small_callees(va, cls)
            imm = wp_constants(va, size)
            small = [v for v in imm if v < 0x10000]
            r_ = random.Random(c.get('wpseed', 0) * 7919 + 13)
            msg = r_.choice(small + [0x501, 0x501, 1]) if imm else c['msg']
            wp = r_.choice(imm + [0xA04, 1]) if imm else c['wp']
            lp = r_.choice([0, 0, 0xFFFF, 0xFFFE, 1, 3]) if imm else 0
            return p, [msg, wp, lp]
        if kind == 'T':
            imm = wp_constants(va, size)
            return p, [random.Random(c.get('wpseed', 0) * 31 + 5).choice(imm + [5, c['tid']]) if imm else c['tid']]
        if kind == 'M':
            return p, [c['x'], c['y']] if tr.ret_n(va) >= 8 else ([c['x']] if tr.ret_n(va) == 4 else [])
        n = (tr.ret_n(va) or 0) // 4
        return p, [c.get('a%d' % i, 0) for i in range(n)]
    return b


for cls, (cva, csz, ckinds, methods) in CLASSES.items():
    for (mname, va, size, kind) in methods:
        TESTS5['%s.%s' % (cls, mname)] = (va, size, mk(cls, mname, va, size, kind), gen5, {'noret': kind not in ('C', 'D', 'P', 'W', 'T')})

    def bctor(e, c, cls=cls, cva=cva, csz=csz, ckinds=ckinds):
        p = poison(e, cls, cls)
        return p, make_args(e, ckinds)
    TESTS5['%s.ctor' % cls] = (cva, csz, bctor, lambda r: {}, {'noret': True})

    def bdtor(e, c, cls=cls, cva=cva, csz=csz, ckinds=ckinds):
        p = poison(e, cls, cls, 0)           # zero filled: the base constructors (trapped) leave the arrays as they found them
        e.run_real(cva, csz, p, make_args(e, ckinds))
        e.pre_len = len(e.log)
        return p, []
    # the destructor of the class: its address is in db/functions.csv as Class::~Class
    for va2, (sz2, nm2, cl2) in tr.functions().items():
        if nm2 == '%s::~%s' % (cls, cls):
            TESTS5['%s.dtor' % cls] = (va2, sz2, bdtor, lambda r: {}, {'noret': True})


for _t in ('MoveGroupDialog', 'RenameGroupDialog', 'RemoveGroupDialog'):
    _o = TESTS5[_t + '.ShowWindow']
    TESTS5[_t + '.ShowWindow'] = _o[:4] + (dict(_o[4], ignore={0x1014fc10, 0x10151160, 0x1012c6c0, 0x101528b0}),)

# OnOK reads the picked group's id through NComboItemFns::GetItemData (real on both sides, see CW): the folded four-byte getter it calls in the stock is inlined in ours.
# MoveGroupDialog.OnOK passes the friend record by value: its word 0 is not written by the copy constructor (include/nwindow/MsnWnd.h), so it is indeterminate
for _t in ('MoveGroupDialog', 'RenameGroupDialog', 'RemoveGroupDialog'):
    _o = TESTS5[_t + '.OnOK']
    TESTS5[_t + '.OnOK'] = _o[:4] + (dict(_o[4], ignore={0x10005b90}, mask={0x1014f3e0: {0}}),)

if __name__ == '__main__':
    t_msn1.run_tests(TESTS5, CW, sys.argv)
