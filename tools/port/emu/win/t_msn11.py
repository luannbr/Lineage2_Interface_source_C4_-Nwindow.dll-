# Differential tests of the messenger's manager (wave C, lane proof2): Msn.cpp (the notifications to the window with their by-value records, the
# switchboard session table, the socket messages, the creation of the connections); stock against ours under unicorn (t_msn1.py / tr.py).
#   python t_msn11.py [cases] [seed] [test,test,...]
import sys, os, random, struct
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import tr
from tr import *
import t_msn1
from t_msn1 import *
import t_msn3
from t_msn3 import person, fsargs
import t_msn10
from t_msn10 import DW, friend_obj, gen10, poison_obj

TESTS11 = {}
# fake virtuals of the connections (Connect 8, OnConnect 9, OnSocketMessage 10)
FAKE = {8: (0x1FFF0200, 'Conn::Connect', 0), 9: (0x1FFF0210, 'Conn::OnConnect', 8), 10: (0x1FFF0220, 'Conn::OnSocketMessage', 8)}
for _sl, (_va, _nm, _pop) in FAKE.items():
    tr.POP_OVERRIDE[_va] = _pop
tr.ARGS_OVERRIDE.update({0x1015cdd0: 1})


class MW11(DW):
    def __init__(self, kind):
        DW.__init__(self, kind)
        for _sl, (_va, _nm, _pop) in FAKE.items():
            self.names[_va] = (0, _nm, '')
        self.conn_vt = self.perm_vt({sl: v[0] for sl, v in FAKE.items()}, 16)
        R = self.rules
        R[0x101515e0] = lambda e, ecx, a: getattr(e, 'found_friend', 0)                            # FriendList::FindFriend(FString)
        R[0x1013c450] = self.get_status                                                            # NotificationProcess::GetStatus(hidden result)
        R[0x1012b460] = self.int_find                                                              # NMsnIntMap::FindRef (the session map)
        R[0x101514a0] = self.int_find
        R[0x10151830] = self.int_remove                                                            # NMsnIntMap::Remove
        R[0x10151770] = self.int_set                                                               # NMsnWndIntMap::SetWnd(&key, &value)
        R[0x10151640] = lambda e, ecx, a: e.log.append(('sort',)) or 0                              # FriendList::Sort
        R[0x1016ec20] = lambda e, ecx, a: e.r32(ecx, 0)                                            # the getter of the lists' first field (FL), folded
        R[0x101652b0] = self.construct_ns                                                          # NotificationProcess::ConstructNS(this = memory)
        # the by-value records: the copy constructors and destructors run for real on both sides, so that the window gets the record's content
        self.transparent |= {0x1014f090, 0x1014efe0, 0x1014f880, 0x10150f20, 0x1013e9a0, 0x10150ff0, 0x10160c90, 0x10152760, 0x101526e0}
        # the session table's Set and the connections' constructors run for real on both sides (the standalone inlines them: Msn::AddSession calls NMsnIntMap::Set,
        # which allocates through the "room for one more pair" 0x10148f30 (folded with MsnNewIntPair in the hook build); StartNotification / CreateSession construct inline)
        self.transparent |= {0x10151770, 0x10148f30}
        if sa.ENABLED and self.ctors_real:          # (the hook build constructs through the stock stand-in: the rules below give the object its fake vtable)
            self.transparent |= {0x101652b0, 0x10153b60, 0x10164ab0}

    def construct_ns(self, e, ecx, a):
        e.write32(ecx, e.conn_vt)
        return ecx

    def get_status(self, e, ecx, a):
        e.fs_set(a[0], getattr(e, 'status_text', 'NLN'))
        return a[0]

    def int_find(self, e, ecx, a):
        key = a[0] if a[0] < 0x1000 else e.read32(a[0])      # the key by reference or by value
        data, n = e.read32(ecx), e.read32(ecx + 4)
        for i in range(n):
            if e.read32(data + 0xC * i + 4) == key:
                return e.read32(data + 0xC * i + 8)
        return 0

    def int_remove(self, e, ecx, a):
        key = e.read32(a[0]) if a[0] >= 0x1000 else a[0]
        data, n = e.read32(ecx), e.read32(ecx + 4)
        keep = [i for i in range(n) if e.read32(data + 0xC * i + 4) != key]
        for j, i in enumerate(keep):
            if i != j:
                e.uc.mem_write(data + 0xC * j, bytes(e.uc.mem_read(data + 0xC * i, 0xC)))
        e.write32(ecx + 4, len(keep))
        return n - len(keep)

    def int_set(self, e, ecx, a):
        key, val = e.read32(a[0]), e.read32(a[1])
        data, n, mx = e.read32(ecx), e.read32(ecx + 4), e.read32(ecx + 8)
        if n + 1 > mx:
            nd = e.alloc(0xC * (n + 4) + 16)
            if n:
                e.uc.mem_write(nd, bytes(e.uc.mem_read(data, 0xC * n)))
            e.write32(ecx, nd)
            e.write32(ecx + 8, n + 4)
            data = nd
        e.write32(data + 0xC * n, 0xFFFFFFFF)
        e.write32(data + 0xC * n + 4, key)
        e.write32(data + 0xC * n + 8, val)
        e.write32(ecx + 4, n + 1)
        return 0


def conn_obj(e, label, hsock, size=0xA4, sid=''):
    o = e.raw_obj(label, size)
    e.write32(o, e.conn_vt)
    e.w32(o, 4, hsock)
    e.fs_set(o + 0x64, sid)
    return o


def msn_obj(e, c):
    r = random.Random(c['rs'])
    m = e.raw_obj('Msn', 0x60)
    wnd = e.win('MsnWnd', 'MsnWndObj') if c['wnd'] else 0
    if wnd:
        ctrl = e.win('MsnTreeCtrl', 'TreeCtrl', 240, 270)
        tree = e.raw_obj('Tree', 0x230)
        e.w32(tree, 0x1F4, e.raw_obj('Root', 0x20) if c['hover'] else 0)
        e.w32(ctrl, 0x144, tree)
        e.w32(wnd, 0x17C, ctrl)
    e.w32(m, 4, wnd)
    e.w32(m, 8, conn_obj(e, 'Dispatch', c['sock1'], 0x70) if c['disp'] else 0)
    e.w32(m, 0xC, conn_obj(e, 'Notif', c['sock2'], 0x98) if c['notif'] else 0)
    # the sessions table: {chain, socket, session}
    ns = c['nsess']
    data = e.alloc(0xC * max(ns, 1) + 16)
    e.uc.mem_write(data, bytes(0xC * max(ns, 1) + 16))
    sess = []
    for i in range(ns):
        s = conn_obj(e, 'Sess%d' % i, c['socks'][i], 0xA4, c['sids'][i]) if c['sess_ok'][i] else 0
        sess.append(s)
        e.write32(data + 0xC * i + 4, c['socks'][i])
        e.write32(data + 0xC * i + 8, s)
    e.write32(m + 0x10, data if ns else 0)
    e.write32(m + 0x14, ns)
    e.write32(m + 0x18, ns)
    hs = e.raw_obj('SessHash', 32)
    for i in range(8):
        e.write32(hs + 4 * i, 0xFFFFFFFF)
    for i in range(ns):
        b = c['socks'][i] & 7
        e.write32(data + 0xC * i, e.read32(hs + 4 * b))
        e.write32(hs + 4 * b, i)
    e.write32(m + 0x1C, hs)
    e.write32(m + 0x20, 8)
    e.fs_set(m + 0x24, c['acct'])
    e.fs_set(m + 0x30, c['text'])
    e.fs_set(m + 0x3C, c['pname'])
    e.w32(m, 0x4C, c['basync'])
    contacts = e.raw_obj('Contacts', 0x14)
    e.w32(contacts, 0, e.raw_obj('FL', 0x28))
    e.w32(m, 0x50, contacts)
    e.w32(m, 0x54, friend_obj(e, 'Me', dict(c, acct='me@x', pname='me', pdisp='', pstatus='FLN', gid=0)))
    e.found_friend = friend_obj(e, 'Found', c) if c['berror'] else 0
    e.status_text = c['pstatus']
    e.write32(e.glob(0x102c3a00) if False else e.gcell['?GMsnSoundAlarm@@3HA'], c['sound'])
    return m


def gen11(r):
    d = gen10(r)
    ns = r.choice([0, 1, 3])
    d.update(wnd=r.choice([0, 1, 1]), hover=r.choice([0, 1]), disp=r.choice([0, 1]), notif=r.choice([0, 1]), sock1=r.choice([0x40, 0x41]), sock2=r.choice([0x42, 0x43]),
             nsess=ns, socks=r.sample([0x44, 0x45, 0x41, 0x43, 0x4C], 3), sids=[r.choice(['sid1', 'sid2', '']) for _ in range(3)],
             sess_ok=[r.choice([1, 1, 1, 0]) for _ in range(3)], basync=r.choice([0, 1]), berror=r.choice([0, 1]), sound=r.choice([0, 1]),
             kind=r.choice([0, 1, 2, 3, -1]), sid=r.choice(['sid1', 'sid2', 'zz', '']), sock=r.choice([0x40, 0x41, 0x42, 0x43, 0x44, 0x99]), lp=r.choice([0, 1, 0x30000]))
    return d


# word 0 of a person record is not written by the constructors (stack garbage): not compared; the folded getter of the lists and the map lookup that ours inlines
MASK11 = {0x10155ce0: {0}, 0x1015bb10: {0}, 0x1015bc80: {0}, 0x10158790: {0}, 0x10158800: {0}, 0x10155ef0: {0}, 0x10155fd0: {0},
          0x1015b980: {1}, 0x1015fe70: {1}, 0x1015aa10: {1}, 0x10160580: {1, 28, 31}, 0x101606b0: {1, 28, 31}}   # (the headers' pair array and hash of the message copy are heap addresses)


def regm(name, sva, size, build, noret=True, opts=None):
    o = {'noret': noret, 'ignore': {0x1016ec20, 0x101514a0, 0x1013fba0, 0x1014e550, 0x1014e5a0, 0x10143760}, 'mask': MASK11}
    if opts:
        o.update(opts)

    def b(e, c):
        e.write32(e.console + 0x3BF4, e.new_obj('NCWnd', 'Desktop'))
        e.presence = ''
        e.shown = {}
        e.conn_vt = e.conn_vt
        m = msn_obj(e, dict(c, wnd=1) if o.get('force_wnd') else c)
        return build(e, c, m)
    TESTS11[name] = (sva, size, b, gen11, o)


def pw(e, c, tag='Arg'):
    return [e.read32(friend_obj(e, tag, c) + 4 * i) for i in range(0x11)]


def mw(e, c):
    m = e.raw_obj('MsgArg', 0x58)
    e.w32(m, 0, 1)
    e.fs_set(m + 4, c['text'])
    for off in (0x10, 0x1C, 0x3C, 0x48):
        e.fs_set(m + off, 'x%x' % off)
    e.w32(m, 0x54, 7)
    return [e.read32(m + 4 * i) for i in range(0x16)]


def gw(e, c):
    g = e.raw_obj('GroupArg', 0x1C)
    e.fs_set(g, c['text'])
    e.fs_set(g + 0xC, c['pname'])
    e.w32(g, 0x18, c['gid'])
    return [e.read32(g + 4 * i) for i in range(7)]


def sv(e, c):
    s = e.raw_obj('ServerArg', 0x10)
    e.fs_set(s, c['text'])
    e.w32(s, 0x0C, c['port'])
    return [e.read32(s + 4 * i) for i in range(4)]


regm('Msn.NotifyAddFriend', 0x1014f8f0, 112, lambda e, c, m: (m, pw(e, c)))
regm('Msn.NotifyRemoveFriend', 0x1014f9a0, 114, lambda e, c, m: (m, fsargs(e, c['acct'])))
regm('Msn.NotifyAddGroup', 0x1014fa50, 112, lambda e, c, m: (m, gw(e, c)))
regm('Msn.PlayAlarmSound', 0x1014fca0, 113, lambda e, c, m: (m, [c['kind'] & 0xFFFFFFFF]))
regm('Msn.NotifyDispatchProcessEnd', 0x101501a0, 93, lambda e, c, m: (m, []))
regm('Msn.NotifyNewMail', 0x1014f630, 97, lambda e, c, m: (m, fsargs(e, c['text'])))
regm('Msn.NotifyError', 0x1014f880, 104, lambda e, c, m: (m, fsargs(e, c['text'])))
for nm, va, sz in (('NotifyFriendOnline', 0x1014f6a0, 110), ('NotifyFriendStatus', 0x1014f710, 110), ('NotifyFriendName', 0x1014f780, 110),
                   ('NotifyFriendBlocked', 0x1014f7f0, 95), ('NotifySignedIn', 0x1014fb00, 122), ('NotifyMyInfo', 0x1014fb80, 115)):
    regm('Msn.' + nm, va, sz, lambda e, c, m: (m, pw(e, c)))
regm('Msn.FindFriend', 0x1014fc30, 108, lambda e, c, m: (m, fsargs(e, c['acct'])), noret=False)
regm('Msn.SortFriends', 0x1014f850, 36, lambda e, c, m: (m, []))
regm('Msn.RefreshFriends', 0x1014fef0, 58, lambda e, c, m: (m, []))
regm('Msn.GetAccount', 0x10152c90, 33, lambda e, c, m: (m, [e.raw_obj('Result', 0x10)]), noret=False)
regm('Msn.GetPassword', 0x10160f60, 33, lambda e, c, m: (m, [e.raw_obj('Result', 0x10)]), noret=False)
regm('Msn.GetUnknown3C', 0x10160f90, 33, lambda e, c, m: (m, [e.raw_obj('Result', 0x10)]), noret=False)
regm('Msn.GetStatus', 0x1014f1d0, 54, lambda e, c, m: (m, [e.raw_obj('Result', 0x10)]), noret=False)
regm('Msn.DiscardString', 0x1014fc00, 13, lambda e, c, m: (m, fsargs(e, c['text'])))
regm('Msn.NotifySwitchboardSessionEnd', 0x10150840, 244, lambda e, c, m: (m, []))
regm('Msn.NotifyNotificationProcessEnd', 0x10150ba0, 145, lambda e, c, m: (m, fsargs(e, c['text'])))
# (a missing window: the stock calls the window's error dialog through the NULL pointer, ours NotifyError tests it first)
regm('Msn.StartNotification', 0x1014fd50, 201, lambda e, c, m: (m, sv(e, c)), opts={'force_wnd': True})
regm('Msn.AddSession', 0x10150300, 47, lambda e, c, m: (m, [conn_obj(e, 'NewSess', c['sock'], 0xA4, c['sid'])]))
regm('Msn.RemoveSession', 0x10150970, 92, lambda e, c, m: (m, [c['sock']]))
for nm, va, sz in (('NotifySessionJoin', 0x10150330, 194), ('NotifySessionLeave', 0x10150400, 194), ('NotifySessionEvent', 0x101505d0, 194)):
    regm('Msn.' + nm, va, sz, lambda e, c, m: (m, fsargs(e, c['sid']) + pw(e, c)))
for nm, va, sz in (('NotifySessionMessage', 0x101504d0, 252), ('NotifySessionMessage2', 0x101506a0, 252)):
    regm('Msn.' + nm, va, sz, lambda e, c, m: (m, fsargs(e, c['sid']) + pw(e, c) + mw(e, c)))
regm('Msn.NotifySessionStarted', 0x101507a0, 158, lambda e, c, m: (m, fsargs(e, c['sid'])))
regm('Msn.CreateSession', 0x1014fe20, 201, lambda e, c, m: (m, sv(e, c) + fsargs(e, c['sid'])), noret=False)
regm('Msn.OnSocketConnect', 0x1014ffc0, 91, lambda e, c, m: (m, [c['sock'], c['lp']]))
regm('Msn.OnSocketData', 0x10150020, 91, lambda e, c, m: (m, [c['sock'], c['lp']]))

if __name__ == '__main__':
    t_msn1.run_tests(TESTS11, MW11, sys.argv)
