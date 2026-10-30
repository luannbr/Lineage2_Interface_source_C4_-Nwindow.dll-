# Differential tests of the messenger's data structures (wave C, lane proof2): MsnData.cpp (the manager, the contact store, the lists and the group
# list: constructors, destructors, DeleteAll), FriendList.cpp (the string map's Rehash, the pair array's destructor), L2Friend.cpp, TaskWnd.cpp (the
# task map: Rehash, RemoveKey, the iterator and the destructors); stock against ours under unicorn (t_msn1.py / tr.py).
#   python t_msn13.py [cases] [seed] [test,test,...]
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
from t_msn10 import poison_obj, strihash, friend_obj, gen10
import t_msn11
from t_msn11 import MW11, gen11
import t_msn12
from t_msn12 import UW, gen12, strmap_w, intmap_w, fsargs_obj

TESTS13 = {}
tr.POP_OVERRIDE[0x1006e880] = 0


class DW13(UW):
    def __init__(self, kind):
        UW.__init__(self, kind)
        R = self.rules
        R[0x1013e3c0] = None
        del R[0x1013e3c0]
        self.transparent |= {0x1013e3c0, 0x10150ff0}
        self.fold(0x10152760, 0x10160c90, transparent=True)      # NMsnGroup::~NMsnGroup / NMsnPassport::~NMsnPassport: one body of ours, run for real
        # the stock removes the string pairs through the chat lane's out-of-line Remove (ours inline: the pair destructor calls are logged below instead), so the trap is back
        self.transparent.discard(0x1013fef0)
        # the pair arrays' Remove and destructors: the stock calls them out of line, ours inlines (the standalone) or folds them with other arrays' (the hook build): real on both sides
        self.transparent |= {0x101495b0, 0x10150270}
        R[0x1006e880] = lambda e, ecx, a: e.log.append(('pairdtor', e.fs_get(ecx + 4))) or 0       # the string pair's destructor (the stock's)
        R[0x10141f90] = lambda e, ecx, a: e.log.append(('strmapdtor',)) or 0                          # NMsnStrMap / int map destructors the stock calls out of line
        R[0x101480c0] = lambda e, ecx, a: e.log.append(('arraydtor', e.nmthis(ecx))) or 0
        R[0x101522d0] = lambda e, ecx, a: e.log.append(('listctor', a[0])) or ecx                  # NMsnListInit::Construct(bit)
        R[0x101523a0] = lambda e, ecx, a: e.log.append(('groupsctor',)) or ecx
        R[0x10152410] = lambda e, ecx, a: e.log.append(('contactsctor',)) or ecx
        R[0x10152340] = lambda e, ecx, a: e.log.append(('listdestroy', e.nmthis(ecx))) or 0
        R[0x101520f0] = lambda e, ecx, a: e.log.append(('groupsdestroy', e.nmthis(ecx))) or 0
        R[0x10152550] = lambda e, ecx, a: e.log.append(('contactsdestroy',)) or 0
        R[0x10151ce0] = lambda e, ecx, a: e.log.append(('groupsdeleteall',)) or 0
        R[0x10152080] = lambda e, ecx, a: e.log.append(('inttablefree',)) or 0
        R[0x10150270] = lambda e, ecx, a: e.log.append(('sessionpairsdtor',)) or 0
        R[0x1013e9a0] = lambda e, ecx, a: e.log.append(('personctor',)) or ecx
        R[0x101495b0] = self.task_pair_remove                                                       # the task map's pair array Remove(index, count)
        R[0x1013fe50] = self.task_rehash                                                            # NTaskMap::Rehash (when trapped)
        R[0x10153c80] = lambda e, ecx, a: e.log.append(('sessiondestroy', e.nmthis(ecx))) or 0

    def task_pair_remove(self, e, ecx, a):
        i, cnt = a[0], a[1]
        data, n = e.read32(ecx), e.read32(ecx + 4)
        for j in range(i, n - cnt):
            e.uc.mem_write(data + 0xC * j, bytes(e.uc.mem_read(data + 0xC * (j + cnt), 0xC)))
        e.write32(ecx + 4, n - cnt)
        return 0

    def task_rehash(self, e, ecx, a):
        data, n, hc = e.read32(ecx), e.read32(ecx + 4), e.read32(ecx + 0x10)
        nh = e.alloc(4 * hc + 16)
        for i in range(hc):
            e.write32(nh + 4 * i, 0xFFFFFFFF)
        for i in range(n):
            key = e.read32(data + 0xC * i + 4)
            b = (e.r32(key, 4) if key else 0) & (hc - 1)
            e.write32(data + 0xC * i, e.read32(nh + 4 * b))
            e.write32(nh + 4 * b, i)
        e.write32(ecx + 0xC, nh)
        return 0


def gen13(r):
    d = gen12(r)
    d.update(nlist=r.choice([0, 1, 3]), nfr=r.choice([0, 1, 3]), ngrp=r.choice([0, 1, 2]), ntask=r.choice([0, 1, 3, 9]), tkeys=[r.choice([0, 0x50, 0x51, 0x52, 0x53]) for _ in range(12)],
             tkey=r.choice([0, 0x50, 0x51, 0x52, 0x53, 0x54]), removed=r.choice([0, 1]), tcount=r.choice([8, 8, 16, 32, 2]))
    return d


def regd(name, sva, size, build, noret=True, opts=None):
    o = {'noret': noret, 'ignore': {0x1008e8a0}}
    if opts:
        o.update(opts)

    def b(e, c):
        e.write32(e.console + 0x3BF4, e.new_obj('NCWnd', 'Desktop'))
        e.presence = ''
        e.shown = {}
        return build(e, c)
    TESTS13[name] = (sva, size, b, gen13, o)


# ---- FriendList.cpp ----------------------------------------------------------------------------------------------------------------------
regd('NMsnStrMap.Rehash', 0x1013fc50, 166, lambda e, c: (strmap_w(e, c), []), opts={'no_heap_log': False})


def pair_array(e, c, label='PairArr'):
    n = c['nmap']
    arr = e.raw_obj(label, 0xC)
    data = e.alloc(0x14 * (n + 2) + 16)
    e.uc.mem_write(data, bytes(0x14 * (n + 2) + 16))
    for i in range(n):
        e.fs_set(data + 0x14 * i + 4, c['mkeys'][i])
    e.write32(arr, data if n else 0)
    e.write32(arr + 4, n)
    e.write32(arr + 8, n + 2)
    return arr


# (the stock removes the pairs through the chat lane's out-of-line Remove, ours inline)
regd('TMsnStrPairArray.dtor', 0x10151ee0, 80, lambda e, c: (pair_array(e, c), []), opts={'ignore': {0x1013fef0}, 'alt': '??1?$TArray@UNMsnStrPair@@@@QAE@XZ'})


def friendlist_obj(e, c, label='List', fr=None):
    fl = e.raw_obj(label, 0x28)
    n = c['nfr'] if fr is None else fr
    recs = [friend_obj(e, '%sRec%d' % (label, i), dict(c, acct='a%d@x' % i, pname='p%d' % i)) for i in range(n)]
    data = e.alloc(4 * max(n, 1) + 16)
    for i, r in enumerate(recs):
        e.write32(data + 4 * i, r)
    e.write32(fl + 4, data if n else 0)
    e.write32(fl + 8, n)
    e.write32(fl + 0xC, n)
    hc = 8
    pairs = e.alloc(0x14 * (n + 2) + 16)
    e.uc.mem_write(pairs, bytes(0x14 * (n + 2) + 16))
    h = e.raw_obj(label + 'Hash', 4 * hc)
    for i in range(hc):
        e.write32(h + 4 * i, 0xFFFFFFFF)
    for i, r in enumerate(recs):
        k = 'a%d@x' % i
        e.fs_set(pairs + 0x14 * i + 4, k)
        e.write32(pairs + 0x14 * i + 0x10, r)
        b = strihash(k) & (hc - 1)
        e.write32(pairs + 0x14 * i, e.read32(h + 4 * b))
        e.write32(h + 4 * b, i)
    e.write32(fl + 0x10, pairs)
    e.write32(fl + 0x14, n)
    e.write32(fl + 0x18, n + 2)
    e.write32(fl + 0x1C, h)
    e.write32(fl + 0x20, hc)
    e.w32(fl, 0, 1)
    return fl


def contacts_obj(e, c):
    ct = e.raw_obj('Contacts', 0x14)
    for i, off in enumerate((0, 4, 8, 0xC)):
        e.w32(ct, off, friendlist_obj(e, c, 'L%d' % i, fr=(c['nfr'] if c['nlist'] > i else 0)) if (c['nlist'] > i or c['hover']) else 0)
    gl = e.raw_obj('GroupList', 0x24)
    e.w32(ct, 0x10, gl if c['basync'] else 0)
    return ct


regd('NMsnContacts.DestroyLists', 0x10152150, 43, lambda e, c: (contacts_obj(e, c), []))


# ---- L2Friend ------------------------------------------------------------------------------------------------------------------------
def l2friend_obj(e, c, label='L2F'):
    o = e.raw_obj(label, 0x20)
    e.fs_set(o, c['pname'])
    e.fs_set(o + 0xC, c['pstatus'])
    e.w32(o, 0x10, c['gid'])
    e.w32(o, 0x14, c['port'])
    return o


regd('L2Friend.copy', 0x1014afd0, 106, lambda e, c: (poison_obj(e, 'L2F', 0x20), [l2friend_obj(e, c, 'Src')]))
regd('L2Friend.SetName', 0x1014b0a0, 69, lambda e, c: (l2friend_obj(e, c), [e.put_wstr(c['text'])]))


# ---- TaskWnd.cpp: the task map ------------------------------------------------------------------------------------------------------------
def taskmap_obj(e, c, label='TaskMap'):
    n, hc = c['ntask'], c['tcount']
    m = e.raw_obj(label, 0x14)
    data = e.alloc(0xC * (n + 4) + 16)
    e.uc.mem_write(data, bytes(0xC * (n + 4) + 16))
    h = e.raw_obj(label + 'Hash', 4 * hc)
    for i in range(hc):
        e.write32(h + 4 * i, 0xFFFFFFFF)
    keys = {}
    for i in range(n):
        kv = c['tkeys'][i]
        if kv and kv not in keys:
            keys[kv] = e.raw_obj('Key%x' % kv, 0x20)
            e.w32(keys[kv], 4, kv)               # UObject::Index
        k = keys.get(kv, 0)
        e.write32(data + 0xC * i + 4, k)
        e.write32(data + 0xC * i + 8, 0x7000 + i)
        b = (kv if k else 0) & (hc - 1)
        e.write32(data + 0xC * i, e.read32(h + 4 * b))
        e.write32(h + 4 * b, i)
    e.write32(m, data)
    e.write32(m + 4, n)
    e.write32(m + 8, n + 4)
    e.write32(m + 0xC, h)
    e.write32(m + 0x10, hc)
    kcell = e.raw_obj('KeyCell', 8)
    if c['tkey'] in keys:
        e.write32(kcell, keys[c['tkey']])
    else:
        e.write32(kcell, 0 if not c['tkey'] else e.raw_obj('Other', 0x20))
    e.kcell = kcell
    return m


regd('NTaskMap.Rehash', 0x1013fe50, 146, lambda e, c: (taskmap_obj(e, c), []))
regd('NTaskMap.RemoveKey', 0x10140e00, 110, lambda e, c: (lambda m: (m, [e.kcell]))(taskmap_obj(e, c)), noret=False)


def b_taskiter(e, c):
    m = taskmap_obj(e, c)
    it = e.raw_obj('Iter', 0x10)
    e.w32(it, 0, m)
    e.w32(it, 4, m)
    e.w32(it, 0xC, c['removed'])
    return it, []


regd('NTaskIter.dtor', 0x10140e70, 50, b_taskiter)       # (no standalone counterpart: the iterator's destructor is inlined into its users)
for nm, va in (('NTaskPairArray.dtor1', 0x10140ff0), ('NTaskPairArray.dtor2', 0x10149ae0)):
    regd(nm, va, 144, lambda e, c: (taskmap_obj(e, c), []), opts={'alt': '??1?$TArray@UNTaskPair@@@@QAE@XZ'})
for nm, va in (('NTaskMap.dtor1', 0x10142000), ('NTaskMap.dtor2', 0x1014a210)):
    regd(nm, va, 103, lambda e, c: (taskmap_obj(e, c), []))


# ---- MsnData.cpp -------------------------------------------------------------------------------------------------------------------------
regd('NMsnIntPairArray.dtor', 0x10151930, 144, lambda e, c: (intmap_w(e, c), []), opts={'alt': '??1?$TArray@UNMsnIntPair@@@@QAE@XZ'})
regd('NMsnSessionPairArray.dtor', 0x10150270, 144, lambda e, c: (intmap_w(e, c), []), opts={'alt': '??1?$TArray@UNPair@?$NDataMap@VSwitchboardSession@@@@@@QAE@XZ'})
regd('NMsnListInit.Construct', 0x101522d0, 107, lambda e, c: (poison_obj(e, 'List', 0x28), [c['gid'] & 0xFFFFFFFF]), noret=False)
regd('NMsnListInit.DeleteAll', 0x10151fe0, 142, lambda e, c: (friendlist_obj(e, c), []))
regd('NMsnListInit.Destroy', 0x10152340, 87, lambda e, c: (friendlist_obj(e, c), []))


def groups_obj(e, c):
    g = e.raw_obj('Groups', 0x24)
    n = c['ngrp']
    data = e.alloc(4 * max(n, 1) + 16)
    for i in range(n):
        gr = e.raw_obj('Grp%d' % i, 0x1C)
        e.fs_set(gr, 'g%d' % i)
        e.w32(gr, 0x18, i + 1)
        e.write32(data + 4 * i, gr)
    e.write32(g, data if n else 0)
    e.write32(g + 4, n)
    e.write32(g + 8, n)
    pairs = e.alloc(0xC * (n + 2) + 16)
    e.uc.mem_write(pairs, bytes(0xC * (n + 2) + 16))
    hh = e.raw_obj('GHash', 32)
    for i in range(8):
        e.write32(hh + 4 * i, 0xFFFFFFFF)
    for i in range(n):
        e.write32(pairs + 0xC * i + 4, i + 1)
        e.write32(pairs + 0xC * i + 8, e.read32(data + 4 * i))
        b = (i + 1) & 7
        e.write32(pairs + 0xC * i, e.read32(hh + 4 * b))
        e.write32(hh + 4 * b, i)
    e.write32(g + 0xC, pairs)
    e.write32(g + 0x10, n)
    e.write32(g + 0x14, n + 2)
    e.write32(g + 0x18, hh)
    e.write32(g + 0x1C, 8)
    return g


regd('NMsnGroupsInit.Construct', 0x101523a0, 98, lambda e, c: (poison_obj(e, 'Groups', 0x24), []), noret=False)
regd('NMsnGroupsInit.DeleteAll', 0x10151ce0, 106, lambda e, c: (groups_obj(e, c), []))
regd('NMsnGroupsInit.Destroy', 0x101520f0, 86, lambda e, c: (groups_obj(e, c), []))
regd('NMsnIntTableInit.Free', 0x10152080, 103, lambda e, c: (intmap_w(e, c), []), opts={'objmask': {'IntMap': [0, 1, 3, 4]}, 'ignore': {0x10151930}})
regd('NMsnContactsInit.Construct', 0x10152410, 311, lambda e, c: (poison_obj(e, 'Contacts', 0x14), []), noret=False)
regd('NMsnContactsInit.Destroy', 0x10152550, 159, lambda e, c: (contacts_obj(e, dict(c, nlist=4)), []))


def manager_obj(e, c):
    m = e.raw_obj('Msn', 0x60)
    wnd = e.win('MsnWnd', 'MsnWndObj')
    e.w32(m, 4, wnd if c['wnd'] else 0)
    ns = c['nsess']
    data = e.alloc(0xC * (ns + 2) + 16)
    e.uc.mem_write(data, bytes(0xC * (ns + 2) + 16))
    hs = e.raw_obj('SessHash', 32)
    for i in range(8):
        e.write32(hs + 4 * i, 0xFFFFFFFF)
    for i in range(ns):
        s = e.raw_obj('Sess%d' % i, 0xA4) if c['sess_ok'][i] else 0
        e.write32(data + 0xC * i + 4, c['socks'][i])
        e.write32(data + 0xC * i + 8, s)
        b = c['socks'][i] & 7
        e.write32(data + 0xC * i, e.read32(hs + 4 * b))
        e.write32(hs + 4 * b, i)
    e.write32(m + 0x10, data)
    e.write32(m + 0x14, ns)
    e.write32(m + 0x18, ns + 2)
    e.write32(m + 0x1C, hs)
    e.write32(m + 0x20, 8)
    e.fs_set(m + 0x24, c['acct'])
    e.fs_set(m + 0x30, c['text'])
    e.fs_set(m + 0x3C, c['pname'])
    e.w32(m, 0x50, e.raw_obj('Contacts', 0x14) if c['basync'] else 0)
    e.w32(m, 0x54, friend_obj(e, 'Me', dict(c, acct='me@x', pname='me', pdisp='', pstatus='FLN', gid=0)) if c['berror'] else 0)
    return m


# (the standalone's Msn has a real vtable: its constructor Msn::Msn writes it and then runs Construct, which has no vtable store in the standalone build; Destroy neither
# restores it. The word 0 of the object is the one thing the two bodies do not share: masked for Construct / Destroy; the real constructor is tested whole below.)
SA_VT = {'objmask': {'Msn': [0]}} if sa.ENABLED else {}
regd('NMsnManagerInit.Construct', 0x10150c40, 319, lambda e, c: (poison_obj(e, 'Msn', 0x60), [e.win('MsnWnd', 'MsnWndObj')]), noret=False, opts=SA_VT)
regd('NMsnManagerInit.Destroy', 0x10150a40, 340, lambda e, c: (manager_obj(e, c), []), opts=SA_VT)
regd('NMsnManagerInit.ctor', 0x10150c40, 319, lambda e, c: (poison_obj(e, 'Msn', 0x60), [e.win('MsnWnd', 'MsnWndObj')]), noret=True,
     opts={'sa_only': True, 'alt': '??0Msn@@QAE@PAVMsnWnd@@@Z', 'alt_force': True})

if __name__ == '__main__':
    t_msn1.run_tests(TESTS13, DW13, sys.argv)
