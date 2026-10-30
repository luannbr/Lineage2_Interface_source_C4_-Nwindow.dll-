# Differential tests of the messenger's helpers, tables and log-in (wave C, lane proof2): MsnUtil.cpp (the client description, the conversions, the
# protocol object's thin calls, the windows' string / number tables), MsnLogin.cpp (the dispatch connection, the sign-in, the commands of the
# notification connection, the answer to SYN, the pending-names table), MsnData.cpp (the manager's data); stock against ours under unicorn
# (t_msn1.py / tr.py).
#   python t_msn12.py [cases] [seed] [test,test,...]
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
from t_msn10 import SW10, gen10, friend_obj, poison_obj, strihash, cmd_obj, gen_cmd
import t_msn11
from t_msn11 import gen11, conn_obj

TESTS12 = {}
tr.stubs.IMPORT_POP['InternetCanonicalizeUrlW'] = 16
tr.ARGS_OVERRIDE[0x10148f30] = 2
tr.POP_OVERRIDE[0x10148f30] = 0
tr.POP_OVERRIDE[0x1006e880] = 0            # NChatNamePair::~NChatNamePair (add ecx,4; jmp ~FString)


class UW(SW10):
    def __init__(self, kind):
        SW10.__init__(self, kind)
        R = self.rules
        R[0x10151560] = self.str_new_pair                                                          # the string map's new pair (the protocol lane's)
        R[0x1013fef0] = self.str_remove                                                            # the string map's pair removal (index, count)
        R[0x1013fc50] = self.str_rehash                                                            # the string map's Rehash
        R[0x10148f30] = self.int_room                                                              # the number map's "room for one more" (cdecl 12, map)
        R[0x10089d40] = self.int_rehash                                                            # the number map's Rehash
        R[0x1013e230] = self.intstr_rehash                                                         # NMsnIntStrMap::Rehash
        R[0x1013e9a0] = self.intstr_remove                                                         # NMsnIntStrMap::RemovePairs(index, count) / the person's default ctor
        R[0x1014fc10] = lambda e, ecx, a: e.r32(ecx, 0x50)                                          # the manager's lists getter
        R[0x1014fc20] = lambda e, ecx, a: e.r32(ecx, 0x54)                                          # the manager's user getter
        R[0x1016ec20] = lambda e, ecx, a: e.r32(ecx, 0)                                            # the lists' first field
        R[0x10151fe0] = lambda e, ecx, a: e.log.append(('DeleteAll', e.nmthis(ecx))) or 0          # NMsnListInit::DeleteAll
        R[0x10160f90] = self.get_unknown3c                                                         # Msn::GetUnknown3C(hidden result)
        R[0x101515e0] = lambda e, ecx, a: getattr(e, 'found_friend', 0)                            # FriendList::FindFriend(FString)
        self.transparent |= {0x1014ff30}                                                           # MsnDispatchServer: fills the server
        # the string / number map insertions (NMsnWndStrMap::SetWnd -> AddWnd, NMsnWndIntMap::SetWnd) run for real on both sides: ours calls its own copies of the helpers
        # (the standalone inlines them), the stock calls the hooked ones. The hash, the pair constructor and the "room for one more pair" run for real; Rehash stays trapped.
        # (RemoveWnd / FreePairs: the pairs go through the out-of-line Remove (a pair destructor each, then FArray::Remove) and the hash is rebuilt by NMsnStrMap::Rehash; ours has
        # other copies of them, the standalone inlines Remove)
        self.transparent |= {0x10151560, 0x101561d0, 0x101561b0, 0x1013e3c0, 0x10148f30, 0x1013fef0, 0x1013fc50}
        if sa.ENABLED and self.ctors_real:       # the dispatch connection's constructors (the standalone constructs inline; the hook build goes through the stock stand-in and the rules below)
            self.transparent |= {0x10164c20, 0x10164ab0}
        R[0x10151140] = lambda e, ecx, a: e.r32(ecx, 4)                                            # the lists' getters (AL, BL, RL), folded
        R[0x10005b90] = lambda e, ecx, a: e.r32(ecx, 8)
        R[0x10151150] = lambda e, ecx, a: e.r32(ecx, 0xC)
        R[0x10163460] = lambda e, ecx, a: 0                                                        # RegisterPending
        R[0x10164ab0] = lambda e, ecx, a: ecx                                                      # MSNSocket::Construct
        R[0x10158880] = lambda e, ecx, a: 0                                                        # MsnWnd::ShowError
        for _sl, (_va, _nm, _pop) in t_msn11.FAKE.items():
            self.names[_va] = (0, _nm, '')
        self.conn_vt = self.perm_vt({sl: v[0] for sl, v in t_msn11.FAKE.items()}, 16)
        R[0x10164c20] = self.construct_disp                                                        # NMsnDispatchInit::Construct (the vtable of the connection)

    def construct_disp(self, e, ecx, a):
        e.write32(ecx, e.conn_vt)
        return ecx

    def get_unknown3c(self, e, ecx, a):
        e.fs_set(a[0], e.fs_get(ecx + 0x3C) or '')
        return a[0]

    # the string map (pairs of 0x14: chain, key FString, window)
    def str_new_pair(self, e, ecx, a):
        key, val = (e.wstr(a[0]) or '') if a[0] else '', e.read32(a[1])
        data, n, mx, hc = e.read32(ecx), e.read32(ecx + 4), e.read32(ecx + 8), e.read32(ecx + 0x10)
        if n + 1 > mx:
            nd = e.alloc(0x14 * (n + 4) + 16)
            if n:
                e.uc.mem_write(nd, bytes(e.uc.mem_read(data, 0x14 * n)))
            e.write32(ecx, nd)
            e.write32(ecx + 8, n + 4)
            data = nd
        p = data + 0x14 * n
        e.fs_set(p + 4, key)
        e.write32(p + 0x10, val)
        e.write32(ecx + 4, n + 1)
        h = e.read32(ecx + 0xC)
        b = strihash(key) & (hc - 1)
        e.write32(p, e.read32(h + 4 * b))
        e.write32(h + 4 * b, n)
        return p + 0x10

    def str_remove(self, e, ecx, a):
        i, cnt = a[0], a[1]
        data, n = e.read32(ecx), e.read32(ecx + 4)
        for j in range(i, n - cnt):
            e.uc.mem_write(data + 0x14 * j, bytes(e.uc.mem_read(data + 0x14 * (j + cnt), 0x14)))
        e.write32(ecx + 4, n - cnt)
        return 0

    def str_rehash(self, e, ecx, a):
        data, n, hc, h = e.read32(ecx), e.read32(ecx + 4), e.read32(ecx + 0x10), e.read32(ecx + 0xC)
        nh = e.alloc(4 * hc + 16)
        for i in range(hc):
            e.write32(nh + 4 * i, 0xFFFFFFFF)
        for i in range(n):
            key = e.fs_get(data + 0x14 * i + 4) or ''
            b = strihash(key) & (hc - 1)
            e.write32(data + 0x14 * i, e.read32(nh + 4 * b))
            e.write32(nh + 4 * b, i)
        e.write32(ecx + 0xC, nh)
        return 0

    # the number map (pairs of 0xC: chain, key, window)
    def int_room(self, e, ecx, a):
        size, m = a[0], a[1]
        data, n, mx = e.read32(m), e.read32(m + 4), e.read32(m + 8)
        if n + 1 > mx:
            nd = e.alloc(size * (n + 4) + 16)
            if n:
                e.uc.mem_write(nd, bytes(e.uc.mem_read(data, size * n)))
            e.write32(m, nd)
            e.write32(m + 8, n + 4)
            data = nd
        e.write32(m + 4, n + 1)
        return data + n * size

    def int_rehash(self, e, ecx, a):
        data, n, hc = e.read32(ecx), e.read32(ecx + 4), e.read32(ecx + 0x10)
        nh = e.alloc(4 * hc + 16)
        for i in range(hc):
            e.write32(nh + 4 * i, 0xFFFFFFFF)
        for i in range(n):
            b = e.read32(data + 0xC * i + 4) & (hc - 1)
            e.write32(data + 0xC * i, e.read32(nh + 4 * b))
            e.write32(nh + 4 * b, i)
        e.write32(ecx + 0xC, nh)
        return 0

    def intstr_rehash(self, e, ecx, a):
        return self.int_rehash(e, ecx, a)

    def intstr_remove(self, e, ecx, a):
        e.log.append(('RemovePairs',))
        return 0


def gen12(r):
    d = gen11(r)
    d.update(gen_cmd(r))
    d.update(plat=r.choice([0, 1, 2]), major=r.choice([4, 5, 6, 10]), minor=r.choice([0, 1, 2]), arch=r.choice([0, 1, 2, 3]),
             wtext=r.choice(['plain', '', 'caf\xe9', 'a b c', 'x' * 30]), nmap=r.choice([0, 1, 2, 4, 9]), hcnt=r.choice([8, 8, 16, 2]),
             mkeys=[r.choice(['a@b', 'c@d', 'A@B', 'x', 'y']) for _ in range(12)], mkey=r.choice(['a@b', 'c@d', 'A@B', 'zz', 'x']),
             inkeys=[r.choice([1, 2, 3, 9, 17, 25]) for _ in range(12)], inkey=r.choice([1, 2, 3, 9, 17, 25, 33]),
             wnds=[r.choice([0x7701, 0x7702, 0x7703]) for _ in range(12)], wnd=r.choice([0x7701, 0x7702, 0x7703, 0x7704]), serial=r.choice([0, 5, 12]),
             code=r.choice(['NLN', 'BSY', '', 'HDN']), blk=r.choice([0, 1]), plist=r.choice([0, 1]))
    return d


def regu(name, sva, size, build, noret=True, opts=None, gen=gen12):
    # (ours' MSNSocket::Send copies the command twice and destroys the first copy: the heap traffic of the by-value copies differs, balanced)
    o = {'noret': noret, 'ignore': {0x1008e8a0, 0x10150f20, 0x10150ff0, 0x1016ec20, 0x10151140, 0x10005b90, 0x10151150}, 'mask': {0x101607e0: {4}}, 'no_heap_log': True}
    if opts:
        o.update(opts)

    def b(e, c):
        e.write32(e.console + 0x3BF4, e.new_obj('NCWnd', 'Desktop'))
        e.presence = ''
        e.shown = {}
        e.os_info = (c['major'], c['minor'], 2600, c['plat'])
        e.cpu_arch = c['arch']
        return build(e, c)
    TESTS12[name] = (sva, size, b, gen, o)


def outbuf(e, n=0x2100):
    p = e.alloc(n)
    e.uc.mem_write(p, bytes(n))
    e.objs[p] = 'Out'
    e.reg_size[p] = 0x100
    return p


def dummy(e):
    return e.raw_obj('Dummy', 8)


regu('MsnPlatformName', 0x10168730, 125, lambda e, c: (dummy(e), [e.raw_obj('Result', 0x10)]), noret=False)
regu('MsnSystemVersion', 0x101687b0, 93, lambda e, c: (dummy(e), [e.raw_obj('Result', 0x10)]), noret=False)
regu('MsnProcessorName', 0x10168810, 125, lambda e, c: (dummy(e), [e.raw_obj('Result', 0x10)]), noret=False)


def b_w2a(e, c):
    w = e.alloc(0x200)
    e.uc.mem_write(w, c['wtext'].encode('utf-16le') + bytes(2))
    return dummy(e), [w, outbuf(e)]


def b_a2w(e, c):
    a = e.alloc(0x100)
    e.uc.mem_write(a, c['wtext'].encode('latin-1', 'replace') + bytes(2))
    return dummy(e), [a, outbuf(e)]


regu('MsnWideToAnsi', 0x10168be0, 32, b_w2a, noret=False)
regu('MsnAnsiToWide', 0x10168c00, 28, b_a2w, noret=False)
regu('MsnUrlCanonicalize', 0x10168c90, 37, b_w2a, noret=False)
regu('DecodeMsnUrl', 0x10168cc0, 37, b_w2a, noret=False)


# the protocol object's thin calls (the session at +0xC)
def ctl_obj(e, c):
    o = e.raw_obj('Ctl', 0x60)
    e.w32(o, 0x0C, e.raw_obj('Session', 0x60) if c['plist'] else 0)
    return o


regu('NMsnCtl.SignOut', 0x1014f1c0, 13, lambda e, c: (ctl_obj(e, c), []))
regu('NMsnCtl.Block', 0x1014f300, 108, lambda e, c: (ctl_obj(e, c), fsargs(e, c['acct']) + [c['blk']]))
regu('NMsnCtl.SetStatus', 0x1014f550, 104, lambda e, c: (ctl_obj(e, c), fsargs(e, c['code'])))
regu('NMsnCtl.SetDisplayName', 0x1014f5c0, 104, lambda e, c: (ctl_obj(e, c), fsargs(e, c['text'])))


# ---- the windows' tables ---------------------------------------------------------------------------------------------------------------
def strmap_w(e, c, label='StrMap'):
    n, hc = c['nmap'], c['hcnt']
    m = e.raw_obj(label, 0x14)
    data = e.alloc(0x14 * (n + 4) + 16)
    e.uc.mem_write(data, bytes(0x14 * (n + 4) + 16))
    h = e.raw_obj(label + 'Hash', 4 * hc)
    for i in range(hc):
        e.write32(h + 4 * i, 0xFFFFFFFF)
    for i in range(n):
        k = c['mkeys'][i]
        e.fs_set(data + 0x14 * i + 4, k)
        e.write32(data + 0x14 * i + 0x10, c['wnds'][i])
        b = strihash(k) & (hc - 1)
        e.write32(data + 0x14 * i, e.read32(h + 4 * b))
        e.write32(h + 4 * b, i)
    e.write32(m, data if n or True else 0)
    e.write32(m + 4, n)
    e.write32(m + 8, n + 4)
    e.write32(m + 0xC, h)
    e.write32(m + 0x10, hc)
    return m


def intmap_w(e, c, label='IntMap'):
    n, hc = c['nmap'], c['hcnt']
    m = e.raw_obj(label, 0x14)
    data = e.alloc(0xC * (n + 4) + 16)
    e.uc.mem_write(data, bytes(0xC * (n + 4) + 16))
    h = e.raw_obj(label + 'Hash', 4 * hc)
    for i in range(hc):
        e.write32(h + 4 * i, 0xFFFFFFFF)
    for i in range(n):
        k = c['inkeys'][i]
        e.write32(data + 0xC * i + 4, k)
        e.write32(data + 0xC * i + 8, c['wnds'][i])
        b = k & (hc - 1)
        e.write32(data + 0xC * i, e.read32(h + 4 * b))
        e.write32(h + 4 * b, i)
    e.write32(m, data)
    e.write32(m + 4, n)
    e.write32(m + 8, n + 4)
    e.write32(m + 0xC, h)
    e.write32(m + 0x10, hc)
    return m


def cellp(e, v):
    p = e.raw_obj('Cell', 8)
    e.write32(p, v)
    return p


regu('NMsnWndStrMap.FindWnd', 0x101514f0, 106, lambda e, c: (strmap_w(e, c), [e.read32(fsargs_obj(e, c['mkey']))]), noret=False)


def fsargs_obj(e, text):
    p = e.alloc(16)
    e.fs_set(p, text)
    e.objs[p] = 'KeyFS'
    e.reg_size[p] = 12
    return p


regu('NMsnWndStrMap.FindWnd', 0x101514f0, 106, lambda e, c: (strmap_w(e, c), [fsargs_obj(e, c['mkey'])]), noret=False)
regu('NMsnWndStrMap.SetWnd', 0x1015ad40, 120, lambda e, c: (strmap_w(e, c), [e.put_wstr(c['mkey']), cellp(e, c['wnd'])]), noret=False)
regu('NMsnWndIntMap.SetWnd', 0x10151770, 181, lambda e, c: (intmap_w(e, c), [cellp(e, c['inkey']), cellp(e, c['wnd'])]), noret=False)
regu('NMsnWndStrMap.RemoveWnd', 0x1015be40, 133, lambda e, c: (strmap_w(e, c), [e.put_wstr(c['mkey']), cellp(e, c['wnd'])]), noret=False)
# (ours clears the pair array's pointer / count after the destruction: the stock leaves them; dead memory, not compared)
regu('NMsnWndStrMap.FreePairs', 0x1015bed0, 80, lambda e, c: (strmap_w(e, c), []), opts={'objmask': {'StrMap': [0]}})
regu('NMsnWndStrMap.Free', 0x1015e0d0, 103, lambda e, c: (strmap_w(e, c), []), opts={'objmask': {'StrMap': [0, 1]}})
regu('NMsnWndIntMap.FreePairs', 0x1015ae90, 144, lambda e, c: (intmap_w(e, c), []), opts={'objmask': {'IntMap': [0, 1]}})
regu('NMsnWndIntMap.Free', 0x1015bf20, 103, lambda e, c: (intmap_w(e, c), []), opts={'objmask': {'IntMap': [0, 1]}})


# ---- the log-in -------------------------------------------------------------------------------------------------------------------------
def notif_obj(e, c):
    n = e.raw_obj('Notif', 0xA0)
    e.write32(n, e.conn_vt)
    m = e.raw_obj('Msn', 0x60)
    e.w32(m, 0x50, e.raw_obj('Contacts', 0x14))
    fl = e.raw_obj('FL', 0x28)
    e.w32(e.r32(m, 0x50), 0, fl)
    for off in (4, 8, 0xC):
        e.w32(e.r32(m, 0x50), off, e.raw_obj('List%d' % off, 0x28))
    e.w32(m, 0x54, friend_obj(e, 'Me', dict(c, acct='me@x', pname='me', pdisp='', pstatus='FLN', gid=0)))
    e.fs_set(m + 0x3C, c['code'])
    e.w32(n, 0x38, m)
    e.w32(n, 0x4C, c['serial'] + 3)
    e.fs_set(n + 0x74, 'old')
    e.w32(n, 0x80, c['serial'] if c['blk'] else 9)
    e.found_friend = friend_obj(e, 'Found', c) if c['plist'] else 0
    return n


regu('MsnDispatchServer', 0x1014ff30, 139, lambda e, c: (dummy(e), [e.raw_obj('Server', 0x10)]), noret=False)
def b_dispinit(e, c):
    msn = e.raw_obj('Msn', 0x60)
    srv = e.alloc(32)                            # (allocated on both sides so that the object, which the function returns, has the same address)
    words = fsargs(e, c['text']) + [c['port']]
    for i, v in enumerate(words):
        e.write32(srv + 4 * i, v)
    disp = poison_obj(e, 'Disp', 0x70)
    if sa.ENABLED and e.kind == 'ours':          # the standalone's DispatchProcess(Msn*, const NMsnServer&, INT): the server record by reference (FString + port)
        return disp, [msn, srv, c['serial']]
    return disp, [msn] + words + [c['serial']]


regu('NMsnDispatchInit.Construct', 0x10164c20, 124, b_dispinit, noret=False, opts={'alt': '??0DispatchProcess@@QAE@PAVMsn@@ABUNMsnServer@@H@Z'})
regu('NMsnNotifyCommands.SendSignOut', 0x10161df0, 123, lambda e, c: (notif_obj(e, c), []))
regu('NMsnNotifyCommands.SendStatus', 0x10163ec0, 204, lambda e, c: (notif_obj(e, c), fsargs(e, c['code'])))
regu('NMsnNotifyCommands.SendDisplayName', 0x10163f90, 293, lambda e, c: (notif_obj(e, c), fsargs(e, c['text'])))
regu('NMsnNotifyCommands.SendBlock', 0x10164290, 491, lambda e, c: (notif_obj(e, c), fsargs(e, c['acct']) + [c['blk']]))


def line_words(e, c):
    o = cmd_obj(e, dict(c, nargs=1, args=[str(c['serial'])] + c['args'][1:], verbs=[], codes=[]), 'LineArg', 0x3C, True)
    return [e.read32(o + 4 * i) for i in range(15)]


regu('NMsnNotifySync.HandleSYNStart', 0x10164d60, 190, lambda e, c: (notif_obj(e, c), line_words(e, c)))


def b_signin(e, c):
    m = e.raw_obj('Msn', 0x60)
    e.fs_set(m + 0x24, 'old-acct')
    e.fs_set(m + 0x30, 'old-pw')
    return m, fsargs(e, c['acct']) + fsargs(e, c['text'])


regu('NMsnManagerLogin.SignIn', 0x10150120, 118, b_signin)


def b_startdispatch(e, c):
    m = e.raw_obj('Msn', 0x60)
    e.w32(m, 4, e.win('MsnWnd', 'MsnWndObj'))
    return m, []


regu('NMsnManagerLogin.StartDispatch', 0x10150080, 157, b_startdispatch)


def b_intstr(e, c):
    m = intmap_w(e, c, 'Names')
    return m, []


regu('NMsnIntStrMapFree.FreePairs', 0x101649f0, 80, b_intstr, opts={'objmask': {'Names': [0, 1]}})
regu('NMsnIntStrMapFree.Free', 0x10165240, 103, b_intstr, opts={'objmask': {'Names': [0, 1]}})

if __name__ == '__main__':
    t_msn1.run_tests(TESTS12, UW, sys.argv)
