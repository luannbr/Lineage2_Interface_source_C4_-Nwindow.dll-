# The states the generic window tests of t_msn5.py do not reach (wave C, lane proof2): the messenger's alarm and status boxes, the dialogs'
# OK / Cancel / paint paths, the trees' scrolling, the log-in and option windows. The windows are made by t_msn5.py (the constructor and OnCreate run
# for real), then given the state a window has some time later (RESTATE), and the handlers get arguments that mean something (a click on the hot line,
# a wheel notch); stock against ours under unicorn (t_msn1.py / tr.py).
#   python t_msn16.py [cases] [seed] [test,test,...]
import sys, os, random, struct
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import tr
from tr import *
import t_msn1
from t_msn1 import *
import t_msn5
from t_msn5 import CW, gen5
import t_msn10
from t_msn10 import friend_obj, gen10
import t_msn8
from t_msn8 import BW

GDATA_SYSMSG = 0x722C8        # GL2GameData.SystemMsg[i] (stride 0x30, the text at +4)
tr.ARGS_OVERRIDE[0x10168b10] = 4          # MsnFitText(text, out, width, height): cdecl, ret 0
tr.POP_OVERRIDE[0x10168b10] = 0


class CW16(BW):
    def __init__(self, kind):
        BW.__init__(self, kind)
        R = self.rules
        R[0x1002d4b0] = lambda e, ecx, a: e.r32(ecx, 4) if a[0] < e.r32(ecx, 8) else 0           # NString::GetAt(index)
        R[0x1000c230] = lambda e, ecx, a: getattr(e, 'commits', {}).get(ecx, getattr(e, 'commit', 0))   # NCEditBox::CommitText (per box)
        R[0x1014fc20] = lambda e, ecx, a: e.r32(ecx, 0x54)                                       # the protocol object's user record
        R[0x101510d0] = self.get_display_name                                                    # NMsnFriend::GetDisplayName(hidden result)
        R[0x10168b10] = self.fit_text                                                            # MsnFitText(text, out, width, height)
        self.transparent |= {0x1006b460, 0x1002d640, 0x1002d4f0, 0x1002d4d0, 0x1002d620, 0x1002d490, 0x1002d660, 0x1002d690, 0x10150f20, 0x10150ff0, 0x10159a70, 0x1013e9a0, 0x101437d0}

    def get_display_name(self, e, ecx, a):
        e.fs_set(a[0], e.fs_get(ecx + 0x2C) or '')
        return a[0]

    def fit_text(self, e, ecx, a):
        t = (e.wstr(a[0]) or '') if a[0] else ''
        e.log.append(('fit', t, a[2], a[3]))
        e.uc.mem_write(a[1], t[:60].encode('utf-16le') + bytes(2))
        return 0


def rnd(c, salt):
    return random.Random(c.get('wpseed', 0) * 977 + salt)


def sysmsg(e, idx, text):
    e.fs_set(e.gdata + GDATA_SYSMSG + 0x30 * idx + 4, text)


TEXTS = ['', 'plain text', 'hi $s1 there', '$s1', 'a long message that goes past the width of the box and wraps']


# ---- the states --------------------------------------------------------------------------------------------------------------------------
def rs_message(e, p, c):
    r = rnd(c, 1)
    e.fs_set(p + 0x1F4, r.choice(TEXTS))


def rs_addgroup(e, p, c):
    r = rnd(c, 2)
    e.w32(p, 0x1F4, e.raw_obj('Link', 0x58))
    e.commit = e.put_wstr(r.choice(['', 'Friends', 'a b'])) if r.choice([0, 1, 1]) else 0


def rs_removefriend(e, p, c):
    r = rnd(c, 3)
    wnd = e.win('MsnWnd', 'OwnerWnd')
    ctrl = e.win('MsnTreeCtrl', 'TreeCtrl', 240, 270)
    tree = e.raw_obj('Tree', 0x230)
    item = e.raw_obj('Item', 0x2880)
    acct = r.choice(['', 'a@b', 'x'])
    e.uc.mem_write(item + 0x1810, acct.encode('utf-16le') + bytes(2))
    e.w32(tree, 0x204, item if r.choice([0, 1, 1]) else 0)
    e.w32(ctrl, 0x144, tree if r.choice([0, 1, 1]) else 0)
    e.w32(wnd, 0x17C, ctrl)
    e.w32(p, 0x1F4, wnd)
    e.w32(p, 0x1F8, e.raw_obj('Link', 0x58))
    chk = e.r32(p, 0x240)
    if chk:
        e.w32(chk, 0x294, r.choice([0, 1]))


def rs_added(e, p, c):
    r = rnd(c, 4)
    n = r.choice([0, 1, 3])
    data = e.alloc(4 * max(n, 1) + 16)
    for i in range(n):
        rec = friend_obj(e, 'Buddy%d' % i, dict(c, acct='b%d@x' % i, pname='bn%d' % i, pdisp='', pstatus='NLN', gid=1))
        e.write32(data + 4 * i, rec if r.choice([1, 1, 1, 0]) else 0)
    e.write32(p + 0x1F8, data if n else 0)
    e.write32(p + 0x1FC, n)
    e.write32(p + 0x200, n)
    e.w32(p, 0x1F4, e.raw_obj('Link', 0x58))
    for off in (0x204, 0x208, 0x20C):
        chk = e.r32(p, off)
        if chk:
            e.w32(chk, 0x294, r.choice([0, 1]))
    for i, idx in enumerate((1325, 1326, 1359, 1360, 1361)):
        sysmsg(e, idx, r.choice(TEXTS))


def rs_alarm(e, p, c):
    r = rnd(c, 5)
    e.w32(p, 0xF0, r.choice([0, 1, 2, 3, 4, 5]))
    e.w32(p, 0x1A0, r.choice([0, 9]))
    e.w32(p, 0x1A4, r.choice([0, 9]))
    e.w32(p, 0x1A8, r.choice([0, 30, 300]))
    e.w32(p, 0x19C, e.win('NCWnd', 'TargetWnd') if r.choice([0, 1]) else 0)
    e.w32(p, 0xE8, e.win('MsnWnd', 'OwnerWnd'))
    e.w32(p, 0xEC, e.raw_obj('Msn', 0x60))
    e.fs_set(p + 0xF4 + 0x2C, r.choice(['bob', 'a%20b', '']))
    e.fs_set(p + 0xF4 + 0x38, 'shown')
    e.fs_set(p + 0x138, r.choice(['subject', '']))
    e.fs_set(p + 0x144 + 4, r.choice(['body text', '']))
    for idx in (1325, 1326, 1359, 1360, 1361):
        sysmsg(e, idx, r.choice(TEXTS))


def rs_status(e, p, c):
    r = rnd(c, 6)
    e.w32(p, 0xFC, r.choice([-1, 0, 1, 2, 3, 4, 5, 6, 7, 7, 9]))
    for i in range(9):
        e.w32(p, 0x100 + 4 * i, 0x7E00 + i if r.choice([0, 1, 1]) else 0)
    e.write32(e.glob(0x10322c68), r.choice([0, 1]))
    e.w32(p, 0xE8, e.win('MsnWnd', 'OwnerWnd'))
    e.w32(p, 0xEC, e.raw_obj('Msn', 0x60) if r.choice([0, 1, 1]) else 0)


def rs_scroll(e, p, c, tree_off=0x144):
    r = rnd(c, 7)
    e.w32(p, 0xEC, -r.choice([0, 15, 45, 100]))
    e.w32(p, 0x100, -r.choice([0, 30, 200]))
    e.w32(p, 0xFC, r.choice([15, 15, 20]))
    e.w32(p, 0xE8, r.choice([0, 3]))
    tree = e.r32(p, tree_off)
    if tree:
        e.w32(tree, 0x210, r.choice([0, 7]))
    # the parent chain of the double click: the entry's parent entry and its window
    ent = e.r32(p, 0x74)
    pent = e.raw_obj('ParentEntry', 0x40)
    e.w32(ent, 0xC, pent if r.choice([0, 1, 1]) else 0)
    e.w32(pent, 4, e.win('NCWnd', 'ParentWnd') if r.choice([0, 1, 1]) else 0)


def rs_login(e, p, c):
    r = rnd(c, 8)
    e.w32(p, 0xFC, e.win('MsnWnd', 'OwnerWnd') if r.choice([0, 1, 1]) else 0)
    e.commits = {}
    for off in (0xE8, 0xEC):
        box = e.r32(p, off)
        if box:
            e.commits[box] = e.put_wstr(r.choice(['', 'user@x', 'pw'])) if r.choice([0, 1, 1]) else 0


def rs_option(e, p, c):
    r = rnd(c, 9)
    m = e.raw_obj('Msn', 0x60)
    e.w32(m, 0x54, friend_obj(e, 'Me', dict(c, acct='me@x', pname='my%20name', pdisp='', pstatus='NLN', gid=0)) if r.choice([0, 1, 1]) else 0)
    e.w32(p, 0xEC, m)
    e.commit = e.put_wstr(r.choice(['', 'My name'])) if r.choice([0, 1, 1]) else 0
    for nm in ('?GMsnLoginAlarm@@3HA', '?GMsnMessageAlarm@@3HA', '?GMsnNewEMailAlarm@@3HA', '?GMsnSoundAlarm@@3HA', '?GMsnDontDisturb@@3HA', '?GMsnHideAll@@3HA'):
        if nm in e.gcell:
            e.write32(e.gcell[nm], r.choice([0, 1]))


for _k, _f in (('MessageDialog', rs_message), ('AddGroupDialog', rs_addgroup), ('RemoveFriendDialog', rs_removefriend), ('AddedConfirmDialog', rs_added),
               ('AlarmWnd', rs_alarm), ('StatusWnd', rs_status), ('MsnTreeCtrl', rs_scroll), ('L2FriendTreeCtrl', lambda e, p, c: rs_scroll(e, p, c, 0x138)), ('LoginWnd', rs_login), ('OptionWnd', rs_option)):
    t_msn5.RESTATE[_k] = _f


# ---- the arguments -----------------------------------------------------------------------------------------------------------------------
def wrap_args(key, argfn):
    t = t_msn5.TESTS5[key]
    base = t[2]

    def b(e, c):
        p, _ = base(e, c)
        return p, argfn(e, c, p)
    return t[:2] + (b,) + t[3:]


def lp_for(e, c, p, index):
    """the lParam of a click on line `index` of the status box (17 pixels each from y 2), x inside the window"""
    r = rnd(c, 21)
    sx, sy = e.r32(p, W['ScreenX']), e.r32(p, W['ScreenY'])
    wd = int(e.rf(p, W['Width'])) if hasattr(e, 'rf') else 200
    x = sx + r.choice([0, 5, wd // 2, wd + 20])
    y = sy + 2 + 17 * index + r.choice([0, 5, 16, 17, -30])
    return ((y & 0xFFFF) << 16) | (x & 0xFFFF)


TESTS16 = {}
for key in ('AddGroupDialog.OnOK', 'MessageDialog.OnPaint', 'RemoveFriendDialog.OnOK', 'AddedConfirmDialog.OnPaint', 'AddedConfirmDialog.HideWindow',
            'AddedConfirmDialog.OnOK', 'AddedConfirmDialog.OnCancel', 'AddedConfirmDialog.ShowWindow', 'AlarmWnd.OnDestroy', 'AlarmWnd.OnTimer', 'AlarmWnd.ShowWindow',
            'AlarmWnd.HideWindow', 'AlarmWnd.OnPaint', 'AlarmWnd.OnLButtonDown', 'StatusWnd.OnDestroy', 'StatusWnd.OnPaint', 'StatusWnd.OnLButtonUp', 'StatusWnd.OnMouseMove',
            'MsnTreeCtrl.ScrollDown', 'MsnTreeCtrl.ScrollUp', 'MsnTreeCtrl.OnMouseWheel', 'L2FriendTreeCtrl.ScrollDown', 'L2FriendTreeCtrl.ScrollUp',
            'L2FriendTreeCtrl.OnMouseWheel', 'L2FriendTreeCtrl.OnDoubleClick', 'LoginWnd.OnLoginButton', 'LoginWnd.OnClearButton', 'OptionWnd.LoadSettings',
            'OptionWnd.ShowWindow', 'AddGroupDialog.ShowWindow', 'RemoveFriendDialog.ShowWindow'):
    if key in t_msn5.TESTS5:
        TESTS16[key] = t_msn5.TESTS5[key]


def wheel_args(e, c, p):
    r = rnd(c, 22)
    return [((r.choice([-3, -2, -1, 0, 1, 2]) * 120) & 0xFFFF) << 16, 0]


for k in ('MsnTreeCtrl.OnMouseWheel', 'L2FriendTreeCtrl.OnMouseWheel'):
    TESTS16[k] = wrap_args(k, wheel_args)
TESTS16['StatusWnd.OnLButtonUp'] = wrap_args('StatusWnd.OnLButtonUp', lambda e, c, p: [0, lp_for(e, c, p, e.r32(p, 0xFC) if e.r32(p, 0xFC) < 0x80000000 else 0)])
TESTS16['StatusWnd.OnMouseMove'] = wrap_args('StatusWnd.OnMouseMove', lambda e, c, p: [0, lp_for(e, c, p, rnd(c, 23).choice([0, 1, 4, 7, 9]))])
for k in TESTS16:
    TESTS16[k] = TESTS16[k][:4] + (dict(TESTS16[k][4], ignore=set(TESTS16[k][4].get('ignore', ())) | {0x1008e8a0}),)
# (word 0 of the person record the alarm hands to the owner is not written by its constructors: stack garbage)
TESTS16['AlarmWnd.OnLButtonDown'] = TESTS16['AlarmWnd.OnLButtonDown'][:4] + (dict(TESTS16['AlarmWnd.OnLButtonDown'][4], mask={0x1015e070: {0}}),)

if __name__ == '__main__':
    t_msn1.run_tests(TESTS16, CW16, sys.argv)
