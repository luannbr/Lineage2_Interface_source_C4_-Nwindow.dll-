# Differential tests of MsnWnd (the messenger's main window; wave C, lane proof2): stock against ours under unicorn (see t_msn1.py / tr.py).
#   python t_msn2.py [cases] [seed] [test,test,...]
import sys, os, random, struct
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import tr
from tr import *
import t_msn1
from t_msn1 import *

CHILD = {0x170: 'LoginWnd', 0x174: 'VersionWnd', 0x178: 'LoadingWnd', 0x17C: 'MsnTreeCtrl', 0x184: 'OptionWnd', 0x188: 'AlarmWnd',
         0x18C: 'StatusWnd', 0x190: 'AddFriendDialog', 0x194: 'RemoveFriendDialog', 0x198: 'MoveGroupDialog', 0x19C: 'AddedConfirmDialog',
         0x1A0: 'OfflineConfirmDialog', 0x1A4: 'AddGroupDialog', 0x1A8: 'RenameGroupDialog', 0x1AC: 'RemoveGroupDialog',
         0x1B0: 'MessageDialog', 0x1B4: 'NCButton', 0x1B8: 'NCButton', 0x1BC: 'NCButton', 0x1C0: 'NCButton', 0x1C4: 'NCButton',
         0x1C8: 'NCButton'}
CHILDNAME = {0x170: 'Login', 0x174: 'Version', 0x178: 'Loading', 0x17C: 'TreeCtrl', 0x184: 'Option', 0x188: 'Alarm', 0x18C: 'Status',
             0x190: 'AddFriendDlg', 0x194: 'RemoveFriendDlg', 0x198: 'MoveGroupDlg', 0x19C: 'AddedConfirmDlg', 0x1A0: 'OfflineDlg',
             0x1A4: 'AddGroupDlg', 0x1A8: 'RenameGroupDlg', 0x1AC: 'RemoveGroupDlg', 0x1B0: 'MessageDlg', 0x1B4: 'StatusBtn',
             0x1B8: 'AddFriendBtn', 0x1BC: 'RemoveFriendBtn', 0x1C0: 'MoveGroupBtn', 0x1C4: 'AddGroupBtn', 0x1C8: 'SignOutBtn'}
# ChatWnd (0xA14): Msn +0xEC, Switchboard +0xF0, bHidden +0x91C, Names/Unknown938 +0x938, InviteWindow ...
G = lambda e, name: e.gcell[name]


def person(e, label, name='a@b', status='NLN', f20='acct', n1='nick', n2='nick2', gid=3):
    """a 0x44 byte person record (two words and five FStrings)"""
    p = e.raw_obj(label, 0x44)
    e.w32(p, 4, gid)
    e.fs_set(p + 0x08, name)
    e.fs_set(p + 0x14, status)
    e.fs_set(p + 0x20, f20)
    e.fs_set(p + 0x2C, n1)
    e.fs_set(p + 0x38, n2)
    return p


def words(e, p, n):
    return [e.read32(p + 4 * i) for i in range(n)]


def strmap(e, base, keys_wnds, pair=0x14):
    """a TMap with FString keys (pairs {HashNext, Key, Wnd}) at base"""
    n = len(keys_wnds)
    data = e.alloc(pair * max(n, 1) + 16)
    e.uc.mem_write(data, bytes(pair * max(n, 1) + 16))
    for i, (k, w) in enumerate(keys_wnds):
        e.write32(data + pair * i, 0xFFFFFFFF)
        e.fs_set(data + pair * i + 4, k)
        e.write32(data + pair * i + 0x10, w)
    e.write32(base, data if n else 0)
    e.write32(base + 4, n)
    e.write32(base + 8, n)
    e.write32(base + 0x10, 8)


def intmap(e, base, keys_wnds):
    n = len(keys_wnds)
    data = e.alloc(0xC * max(n, 1) + 16)
    e.uc.mem_write(data, bytes(0xC * max(n, 1) + 16))
    for i, (k, w) in enumerate(keys_wnds):
        e.write32(data + 0xC * i, 0xFFFFFFFF)
        e.write32(data + 0xC * i + 4, k)
        e.write32(data + 0xC * i + 8, w)
    e.write32(base, data if n else 0)
    e.write32(base + 4, n)
    e.write32(base + 8, n)
    e.write32(base + 0x10, 8)


def chatwnd(e, label, hidden=0, sb=True):
    w = e.win('ChatWnd', label, 256, 401)
    e.w32(w, 0x91C, hidden)
    if sb:
        e.w32(w, 0xF0, e.raw_obj('sb:' + label, 0x80))
    return w


def msnwnd(e, c, kids=True):
    w = e.win('MsnWnd', 'MsnWnd')
    if not c.get('nomsn'):
        m = e.raw_obj('Msn', 0x58)
        e.w32(m, 0x4C, c.get('logged', 1))
        e.w32(w, 0xE8, m)
    for off, cls in CHILD.items():
        if kids and not (off in c.get('missing', ())):
            e.w32(w, off, e.win(cls, CHILDNAME[off]))
    for off in (0x190, 0x1A4):          # the dialogs that own an edit line at +0x1F8
        if e.r32(w, off):
            e.w32(e.r32(w, off), 0x1F8, e.win('NCEditBox', 'Edit%x' % off))
    e.w32(w, 0x16C, c.get('timer', 0))
    e.w32(w, 0x1CC, 0x7777 if c.get('back', 1) else 0)
    chats = []
    for i in range(c.get('nchat', 0)):
        chats.append(('chat%d' % i, chatwnd(e, 'Chat%d' % i, hidden=(c.get('hid', 0) >> i) & 1, sb=bool(c.get('sb', 1)))))
    strmap(e, w + 0xEC, chats)
    strmap(e, w + 0x100, [(k, wn) for (k, wn) in chats if c.get('names', 1)])
    inv = []
    for i in range(c.get('ninv', 0)):
        inv.append((0x1000 + i, e.win('InviteWnd', 'Invite%d' % i, 256, 100)))
    intmap(e, w + 0x114, inv)
    if c.get('l2friend'):
        l2 = e.win('L2FriendWnd', 'L2FriendWindow', 256, 353)
        e.w32(l2, 0xEC, c.get('ntasks', 0))       # TaskWnd::Tasks.Pairs.Num() (+0xE8 map, Num at +4)
        e.w32(w, 0x180, l2)
    me = person(e, 'MePerson', status=c.get('mestatus', 'NLN'))
    for i in range(0x44 // 4):
        e.write32(w + 0x128 + 4 * i, e.read32(me + 4 * i))
    e.write32(e.console + 0x38, c.get('cstate', 6))
    e.write32(e.gcell['?GMsnHideAll@@3HA'], c.get('hideall', 0))
    e.write32(e.gcell['?GMsnNewEMailAlarm@@3HA'], c.get('mail', 1))
    e.write32(e.gcell['?GMsnLoginAlarm@@3HA'], c.get('loginalarm', 1))
    e.write32(e.gcell['?GMsnDontDisturb@@3HA'], c.get('dontdist', 0))
    e.write32(e.glob(0x10322c68), c.get('blocked', 0))        # GMsnBlocked
    for off in c.get('shown', ()):
        e.shown[e.r32(w, off)] = 1
    return w


def gen_msnwnd(r):
    return dict(logged=r.choice([0, 1, 1]), nomsn=r.choice([0, 0, 0, 1]) if False else 0, nchat=r.choice([0, 1, 2, 3]), hid=r.randint(0, 7),
                ninv=r.choice([0, 1, 2]), l2friend=r.choice([0, 1]), ntasks=r.choice([0, 0, 2]), cstate=r.choice([6, 6, 3]),
                hideall=r.choice([0, 1]), mail=r.choice([0, 1]), loginalarm=r.choice([0, 1]), dontdist=r.choice([0, 1]),
                blocked=r.choice([0, 0, 1]), back=r.choice([0, 1]), presence=r.choice(PRESENCE), mestatus=r.choice(['NLN', 'FLN', 'BSY']),
                timer=r.choice([0, 77]), sb=r.choice([0, 1]), names=r.choice([0, 1]))


TESTS2 = {}


def reg(name, sva, size, gen=gen_msnwnd, noret=True):
    def deco(fn):
        TESTS2[name] = (sva, size, fn, gen, {'noret': noret})
        return fn
    return deco


@reg('msn_show', 0x1015b130, 1001)
def b_show(e, c):
    return msnwnd(e, c), []


@reg('msn_hide', 0x1015b550, 311)
def b_hide(e, c):
    return msnwnd(e, c), []


@reg('msn_ontimer', 0x1015c8c0, 435, noret=False)
def b_ontimer(e, c):
    return msnwnd(e, c, kids=True), [c.get('tid', 0xA55)]


def gen_timer(r):
    d = gen_msnwnd(r)
    d['tid'] = r.choice([0xA55, 0xA55, 0x77])
    d['shown'] = r.choice([(), (0x1B4,)])
    return d


TESTS2['msn_ontimer'] = (0x1015c8c0, 435, b_ontimer, gen_timer, {})


@reg('msn_onpaint', 0x10157870, 1772, noret=False)
def b_onpaint(e, c):
    return msnwnd(e, c), [e.device()]


# the stock asks the protocol object for the presence code before every comparison, ours once (a pure getter)
TESTS2['msn_onpaint'] = TESTS2['msn_onpaint'][:4] + ({'ignore': {0x1014f1d0}},)


@reg('msn_oncreate', 0x1015e3b0, 4171, noret=False)
def b_oncreate(e, c):
    w = e.win('MsnWnd', 'MsnWnd')
    e.write32(e.console + 0x3BF4, e.new_obj('NCWnd', 'Desktop'))
    return w, []


def run_tests2(argv):
    t_msn1.TESTS = TESTS2
    return t_msn1.run_tests(TESTS2, t_msn1.RW, argv)


if __name__ == '__main__':
    run_tests2(sys.argv)
