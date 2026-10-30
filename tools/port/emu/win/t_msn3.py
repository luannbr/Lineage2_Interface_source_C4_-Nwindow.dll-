# Differential tests of MsnWnd, second batch (wave C, lane proof2): the small handlers, the dialogs, the buttons, the handlers that take the
# person record by value, the window procedure, the constructor and the destructor. Stock against ours under unicorn (t_msn1.py / tr.py).
#   python t_msn3.py [cases] [seed] [test,test,...]
import sys, os, random, struct
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import tr
from tr import *
import t_msn1
from t_msn1 import *
import t_msn2
from t_msn2 import *

TESTS3 = {}


class MW(RW):
    def __init__(self, kind):
        RW.__init__(self, kind)
        R = self.rules
        R[0x101526e0] = lambda e, ecx, a: copy_rec(e, ecx, a[0], 0x1C)         # NMsnGroup copy constructor
        R[0x10152b90] = lambda e, ecx, a: ecx + 0x64                           # SwitchboardSession::GetSessionIDRef
        R[0x10152bf0] = lambda e, ecx, a: e.r32(ecx, 0x7C)                     # SwitchboardSession::GetLastFriend
        R[0x10150f20] = lambda e, ecx, a: copy_rec(e, ecx, a[0], 0x44)         # NMsnFriend copy constructor (this = the copy)
        R[0x10151070] = lambda e, ecx, a: copy_rec(e, ecx, a[0], 0x44)         # NMsnFriend::operator=
        R[0x101510d0] = lambda e, ecx, a: disp_name(e, ecx, a)                 # NMsnFriend::GetDisplayName(out)
        R[0x1014fc10] = lambda e, ecx, a: e.lists                              # the protocol object's person lists
        R[0x1016ec20] = lambda e, ecx, a: e.r32(ecx, 0)                        # FL getter (mov eax,[ecx]: ours inlines it, so it reads the memory with_state fills)
        R[0x10005b90] = lambda e, ecx, a: e.r32(ecx, 8)                        # BL getter (mov eax,[ecx+8], folded with NComboItemFns' record getter)
        R[0x101515e0] = lambda e, ecx, a: e.found                              # find in a list (account by value)
        R[0x1014fc30] = lambda e, ecx, a: e.friend_rec                         # Msn::FindFriend(account by value)
        R[0x10166a60] = lambda e, ecx, a: e.group_item                         # group entry by id
        R[0x1015b820] = lambda e, ecx, a: e.chat_open                          # FindOneToOneChat
        R[0x1015b920] = lambda e, ecx, a: e.chat_free                          # FindFreeChat
        R[0x10033060] = lambda e, ecx, a: e.hit_item                           # NCTree::FindItemAt
        R[0x101417b0] = lambda e, ecx, a: e.chat_with                          # chat window: is it with this account
        R[0x101347d0] = lambda e, ecx, a: e.chat_by_key                        # NMsnWndStrMap::FindWnd
        R[0x10141c30] = lambda e, ecx, a: e.listed                             # L2 task window entry of a session
        R[0x10159260] = lambda e, ecx, a: e.c.get('iconactive', 0)             # MessengerWnd::IsIconActive
        R[0x10166aa0] = lambda e, ecx, a: e.old_item                           # MsnTreeCtrl::FindFriendItem
        R[0x10034390] = lambda e, ecx, a: e.c.get('basewp', 0)                 # NCWnd::WindowProc (base)
        R[0x101514f0] = lambda e, ecx, a: e.chat_by_key                        # NMsnWndStrMap::FindWnd (OnSessionRemoved's)
        # NMsnGroup::~NMsnGroup and NMsnPassport::~NMsnPassport are one body of ours (the linker folded them): reported as the first, on both sides
        self.fold(0x10152760, 0x10160c90)
        # appMemcpy: a hooked function of the hook build (called through its address, the stock inlines the copy), an import of the standalone: real on both sides
        self.transparent.add(0x10001cb0)
        if not sa.ENABLED:
            # the hook build inlines these small members (the standalone calls them as functions, which the tests compare): real on both sides there
            self.transparent |= {0x10156250, 0x101473d0, 0x1013fc50}      # NMoveGroupDialogSetter::SetPerson, AddedConfirmDialog::AddBuddy, the chat window's name map allocation

    def reset_case(self, c):
        RW.reset_case(self, c)
        self.c = c
        self.lists = self.flist = self.blist = self.found = self.friend_rec = self.group_item = 0
        self.old_item = 0
        self.chat_open = self.chat_free = self.hit_item = self.chat_with = self.peer = self.sacct = self.chat_by_key = self.listed = 0


def copy_rec(e, dst, src, n):
    e.uc.mem_write(dst, bytes(e.uc.mem_read(src, n)))
    return dst


def intmap_fill(e, m, pairs, hc, slack, hash_):
    """a TMap<INT,INT> at m: the pairs {HashNext, Key, Value} in order, the hash heads and the chains of `hc` heads (hash_ = 0: no hash array), `slack` spare pairs.
    Code of ours that inlines the table (find / set / remove walk the chains) needs a real one; the stock's trapped versions never read it."""
    n = len(pairs)
    data = e.alloc(0xC * max(n, 1) + 16)
    e.uc.mem_write(data, bytes(0xC * max(n, 1) + 16))
    for i, (k, v) in enumerate(pairs):
        e.write32(data + 0xC * i, 0xFFFFFFFF)
        e.write32(data + 0xC * i + 4, k & 0xFFFFFFFF)
        e.write32(data + 0xC * i + 8, v)
    e.write32(m, data if n else 0)
    e.write32(m + 4, n)
    e.write32(m + 8, n + slack)
    h = e.alloc(4 * hc + 16)
    for i in range(hc):
        e.write32(h + 4 * i, 0xFFFFFFFF)
    for i, (k, v) in enumerate(pairs):
        hh = (k & 0xFFFFFFFF) & (hc - 1)
        e.write32(data + 0xC * i, e.read32(h + 4 * hh))
        e.write32(h + 4 * hh, i)
    e.write32(m + 0xC, h if hash_ else 0)
    e.write32(m + 0x10, hc)


def disp_name(e, ecx, a):
    e.fs_set(a[0], 'disp:' + (e.fs_get(ecx + 0x2C) or e.fs_get(ecx + 0x38) or ''))
    return a[0]


def fsargs(e, text):
    p = e.alloc(16)
    e.fs_set(p, text)
    return [e.read32(p), e.read32(p + 4), e.read32(p + 8)]


def recargs(e, c, label='arg'):
    st = c.get('pstatus', 'NLN')
    p = person(e, label, name=c.get('pname', 'x@y'), status=st, f20=c.get('pf20', 'acc1'), n1='n1', n2='n2', gid=c.get('gid', 5))
    return [e.read32(p + 4 * i) for i in range(0x11)]


def with_state(e, c, w):
    """the lookups the handlers make through the protocol object and the tree"""
    e.lists = e.raw_obj('Lists', 0x20) if c.get('lists', 1) else 0
    e.flist = e.raw_obj('FL', 0x20) if c.get('fl', 1) else 0
    e.blist = e.raw_obj('BL', 0x20) if c.get('bl', 1) else 0
    if e.lists:
        e.w32(e.lists, 0, e.flist)               # the lists' first and third fields: what the four-byte getters read
        e.w32(e.lists, 8, e.blist)
    e.found = person(e, 'FoundRec', status=c.get('fstat', 'NLN')) if c.get('found', 1) else 0
    e.friend_rec = person(e, 'FriendRec', status=c.get('fstat', 'NLN'), f20='facc')
    e.group_item = e.raw_obj('GroupItem', 0x40) if c.get('group', 1) else 0
    if c.get('openchat') and e.r32(w, 0xEC):
        e.chat_open = e.r32(e.r32(w, 0xEC), 0x10)                    # the window of the first pair of the chat map
    e.chat_free = e.win('ChatWnd', 'FreeChat', 256, 401) if c.get('freechat') else 0
    e.old_item = e.raw_obj('OldItem', 0x40) if c.get('olditem') else 0
    if c.get('bykey'):
        cw = e.win('ChatWnd', 'ByKeyChat', 256, 401)
        e.w32(cw, 0xF0, 0x1234)
        iw = e.win('InviteWnd', 'ByKeyInvite', 256, 100)
        e.w32(iw, 0xEC, 0x4321)
        e.w32(cw, 0x9FC, iw)
        e.chat_by_key = cw
    e.chat_with = c.get('with', 0)
    e.peer = e.raw_obj('Peer', 0x80) if c.get('peer') else 0
    e.sacct = e.raw_obj('SAcct', 0x20)
    e.fs_set(e.sacct, 'sacct')
    e.listed = e.raw_obj('Listed', 0x100) if c.get('listed') else 0
    # the window that holds this page (Entry -> parent entry -> window): a MessengerWnd with its icon
    holder = e.win('MessengerWnd', 'Holder')
    icon = e.win('NCIcon', 'HolderIcon')
    e.w32(icon, 0x214, c.get('tip', 1))
    e.w32(holder, 0x3C, icon if c.get('hicon', 1) else 0)
    pent = e.raw_obj('pent', 0x40)
    e.w32(e.r32(w, 0x74), 0xC, pent)
    e.w32(pent, 4, holder)
    if c.get('hshown'):
        e.shown[holder] = 1
    # the tree: MsnTreeCtrl +0x144 is the tree window; its +0x204 the chosen entry (Text3 at +0x1810 of the entry), +0x1F4 the root
    tc = e.r32(w, 0x17C)
    if tc and not c.get('notree'):
        tree = e.raw_obj('MsnTreeWnd', 0x230)
        e.w32(tc, 0x144, tree)
        root = e.raw_obj('Root', 0x40)
        e.w32(tree, 0x1F4, root)
        if c.get('sel', 1):
            sel = e.raw_obj('Selected', 0x2880)
            e.write_wstr_at(sel + 0x1810, c.get('seltext', 'sel@acct'))
            e.w32(tree, 0x204, sel)
        if c.get('hit'):
            hit = e.raw_obj('HitItem', 0x2880)
            e.w32(hit, 0x2848, c.get('hit3', 3))
            e.write_wstr_at(hit + 0x1810, 'hit@acct')
            e.hit_item = hit


def gen2(r):
    d = gen_msnwnd(r)
    d.update(lists=r.choice([0, 1, 1]), fl=r.choice([0, 1, 1]), bl=r.choice([0, 1, 1]), found=r.choice([0, 1]), group=r.choice([0, 1]),
             freechat=r.choice([0, 1]), openchat=r.choice([0, 1]), peer=r.choice([0, 1]), listed=r.choice([0, 1]), sel=r.choice([0, 1, 1]),
             seltext=r.choice(['', 'a@b', 'sel@acct']), hit=r.choice([0, 1, 1]), hit3=r.choice([3, 3, 2]), fstat=r.choice(['NLN', 'FLN']),
             pstatus=r.choice(['NLN', 'FLN', 'BSY']), gid=r.choice([0, 5, 9]), x=r.choice([5, 40]), y=r.choice([5, 60]))
    d['with'] = r.choice([0, 1])
    d['text'] = r.choice(['hello', '', 'a longer text'])
    d['code'] = r.choice(['201', '205', '207', '911', '930', '5', ''])
    d['reason'] = r.choice(['OTH', 'SSD', 'xx', ''])
    d['olditem'] = r.choice([0, 1])
    d['notree'] = r.choice([0, 0, 1])
    d['iconactive'] = r.choice([0, 1])
    d['basewp'] = r.choice([0, 0, 1])
    d['bykey'] = r.choice([0, 1])
    d['shown'] = r.choice([(), (0x18C,)])
    d['ntasks'] = r.choice([0, 1, 2])
    d['tip'] = r.choice([0, 1])
    d['hicon'] = r.choice([0, 1])
    d['hshown'] = r.choice([0, 1])
    return d


def std(argfn=None):
    def b(e, c):
        w = msnwnd(e, c)
        with_state(e, c, w)
        return w, (argfn(e, c) if argfn else [])
    return b


# The tests that pass the person record by value: its word 0 is "not touched by the constructor" (include/nwindow/MsnWnd.h), so the copy the callee
# receives has whatever the caller's stack held there (the stock's and ours differ, and so does the run history): the word is indeterminate, never read.
PERSON_MASK = {0x10156250: {0}, 0x101473d0: {0}, 0x10158180: {0}, 0x10167400: {0}, 0x1015ce90: {0}, 0x10155910: {0}, 0x10158090: {0}}


def regs(name, sva, size, argfn=None, noret=True, ignore=None, ignore_alloc=False, mask=None):
    o = {'noret': noret}
    # the folded four-byte getters of the lists (0x1016EC20 FL, 0x10005B90 BL): ours inlines them, the stock calls them (pure reads)
    o['ignore'] = {0x1016ec20, 0x10005b90}
    if mask:
        o['mask'] = mask
    if ignore:
        o['ignore'] = o['ignore'] | ignore
    if ignore_alloc:
        o['ignore_alloc'] = True
    TESTS3[name] = (sva, size, std(argfn), gen2, o)


for nm, va, sz in [('show_login', 0x10155aa0, 14), ('show_version', 0x10155ab0, 14), ('signout', 0x10155b80, 11), ('showopt', 0x10156300, 29),
                   ('show_offline', 0x10158210, 142), ('show_addgroup', 0x101582a0, 152), ('show_rengroup', 0x10158340, 142),
                   ('show_rmgroup', 0x101583d0, 142), ('show_addfriend', 0x10157f60, 152), ('show_rmfriend', 0x10158000, 142),
                   ('resetafter', 0x1015ca80, 391)]:
    regs('msn_' + nm, va, sz)
for nm, va, sz in [('btn_status', 0x10155b90, 68), ('btn_add', 0x10158500, 72), ('btn_rm', 0x10158550, 131), ('btn_block', 0x101585e0, 219),
                   ('btn_move', 0x101586c0, 208), ('btn_signout', 0x1015f400, 491)]:
    regs('msn_' + nm, va, sz, noret=False, mask=PERSON_MASK)
regs('msn_showmsg', 0x10158460, 153, lambda e, c: [e.put_wstr(c['text'])])
regs('msn_startlogin', 0x10155ac0, 188, lambda e, c: fsargs(e, 'acct@x') + fsargs(e, c['text']))
regs('msn_onnewmail', 0x10155c60, 124, lambda e, c: fsargs(e, c['text']))
regs('msn_notifyrm', 0x10155d90, 120, lambda e, c: fsargs(e, c['text']))
regs('msn_showerror', 0x10158880, 427, lambda e, c: fsargs(e, c['code']))
regs('msn_onsignedout', 0x1015d180, 510, lambda e, c: fsargs(e, c['reason']))
for nm, va, sz in [('showmove', 0x10158090, 238), ('showadded', 0x10158180, 136), ('onfriendreq', 0x10158800, 117),
                   ('refreshgroup', 0x10158790, 108), ('refreshfriend', 0x1015bc80, 213), ('onsignedin', 0x10155ef0, 223),
                   ('setme', 0x10155fd0, 79), ('notifyadd', 0x10155ce0, 118), ('openchat', 0x1015ce90, 739), ('alarmclick', 0x1015e070, 94)]:
    # (hook build: the ChatWnd constructor is forced inline, so ~NMsnFriend of its by-value record is a call of ours, inlined in the stock and in the standalone)
    regs('msn_' + nm, va, sz, lambda e, c: recargs(e, c), ignore_alloc=(nm == 'openchat'), mask=PERSON_MASK,
         ignore=({0x10150ff0} if nm == 'openchat' and not sa.ENABLED else None))
regs('msn_onfriendpres', 0x1015bb10, 356, lambda e, c: recargs(e, c), ignore=None, mask=PERSON_MASK)
regs('msn_notifygroup', 0x10155e40, 118, lambda e, c: [e.read32(e.raw_obj('grp', 0x1C) + 4 * i) for i in range(7)])
regs('msn_doubleclick', 0x1015d430, 170, lambda e, c: [c['x'], c['y']], noret=False)
def session(e, c, label):
    if not c.get('peer'):
        return 0
    s = e.raw_obj(label, 0x80)
    e.fs_set(s + 0x64, 'sess-id')
    e.w32(s, 0x7C, e.raw_obj('Peer', 0x40) if c.get('listed') else 0)
    if e.r32(s, 0x7C):
        e.fs_set(e.r32(s, 0x7C) + 0x20, 'peeracct')
    return s


regs('msn_chatofsession', 0x1015cd90, 56, lambda e, c: [session(e, c, 'Session')])
regs('msn_sessremoved', 0x1015cdd0, 182, lambda e, c: [session(e, c, 'Sess2')], ignore={0x10152b90})
def closed_chat(e, c):
    w = e.win('ChatWnd', 'ClosedChat', 256, 401)
    if c.get('peer'):
        e.w32(w, 0xF0, session(e, c, 'ClosedSb'))
    return [w]


regs('msn_chatclosed', 0x1015d380, 171, closed_chat)
regs('msn_ondestroy', 0x1015c3d0, 1238, noret=False)


def b_winproc(e, c):
    w = msnwnd(e, c)
    with_state(e, c, w)
    return w, [c['msg'], c['wp'], 0]


def gen_wp(r):
    d = gen2(r)
    d['msg'] = r.choice([0x203, 0x501, 0x501, 0x501, 0x111, 5])
    d['wp'] = r.choice([0xA04, 0xA05, 0xA06, 0xA07, 0xA08, 0xA52, 0xA03, 1])
    return d


TESTS3['msn_winproc'] = (0x1015f710, 261, b_winproc, gen_wp, {'noret': False})
TESTS3['msn_ctor'] = (0x1015f820, 354, lambda e, c: (poison(e, 'MsnWnd', 'MsnWnd'), []), lambda r: {}, {'noret': True})


def b_dtor(e, c):
    p = poison(e, 'MsnWnd', 'MsnWnd')
    e.run_real(0x1015f820, 354, p)
    e.pre_len = len(e.log)
    return p, []


TESTS3['msn_dtor'] = (0x1015f9c0, 144, b_dtor, lambda r: {}, {'noret': True})

if __name__ == '__main__':
    t_msn1.run_tests(TESTS3, MW, sys.argv)
