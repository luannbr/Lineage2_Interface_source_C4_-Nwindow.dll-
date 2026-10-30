# Differential tests of the messenger's contact lists (FriendList.cpp: CompareFriend, Sort, FindFriend, AddFriend, RemoveFriend, the group list, the
# four lists' getter) -- the roster fill: stock against ours under unicorn (t_msn1.py / tr.py). The maps' Find / Set / Remove and the arrays'
# Remove are trapped and modelled at the logical level (a pair array searched by key), the person records are the 0x44 byte ones of t_msn3.py.
#   python t_msn7.py [cases] [seed] [test,test,...]
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
from t_msn3 import *

TESTS7 = {}
tr.ARGS_OVERRIDE.update({0x101692e0: 8, 0x101513b0: 2})       # Util::MsnCompareFriend(FString, int, FString, int) (cdecl, the strings by value: 3 + 1 + 3 + 1 words)


class LW(MW):
    no_guess_ctor = True

    def __init__(self, kind):
        MW.__init__(self, kind)
        R = self.rules
        # NMsnGroup::GetId (a getter ours reads inline), the copy constructors and destructors of the records (ours compiles its own; the stock has
        # the same body under another name: NMsnPassport::~NMsnPassport is ~NMsnGroup)
        self.transparent |= {0x101528b0, 0x10150f20, 0x10150ff0, 0x101526e0, 0x10152760, 0x10160c90, 0x10152850}
        # ours inlines the by-id Set (and, in the standalone, TPtrArray::Remove): they run for real on both sides, on the valid tables of glist
        self.transparent |= {0x10151770, 0x10148f30}                      # NMsnWndIntMap::SetWnd (the by-id Set), NewStringArrayElement (FArray::Add of a pair)
        if sa.ENABLED:
            self.transparent.add(0x1001f3f0)                              # TPtrArray::Remove (a call of the hook build)
        R[0x101514f0] = self.m_find                               # NMsnStrMap::Find(&key) -> the value
        R[0x1015ad40] = self.m_set                                # NMsnStrMap::Set(key text, &value)
        R[0x101347d0] = self.m_find
        R[0x1001f3f0] = self.arr_remove                           # TPtrArray::Remove(index, count) (pointer arrays)
        R[0x101692e0] = self.cmp_friend                           # Util::MsnCompareFriend
        R[0x101514a0] = self.int_find                             # NMsnIntMap::FindRef(&key)
        R[0x10151770] = self.int_set                              # NMsnIntMap::Set(&key, &value)
        R[0x101527b0] = lambda e, ecx, a: e.log.append(('SetName', ecx and e.val(ecx))) or 0
        R[0x10151830] = self.int_remove                           # NMsnIntMap::Remove(&key) (ours hooks it; trapped here too)
        R[0x10151e60] = self.str_remove                           # NMsnStrMap::Remove(key text)
        R[0x1013fc50] = lambda e, ecx, a: 0                        # Rehash: not modelled (the tests never read the hash)

    # the maps: pairs at base+0 {HashNext, Key FString (12) | Key INT, Value}
    def m_find(self, e, ecx, a):
        key = e.fs_get(a[0]).lower() if a[0] else ''
        data, n = e.read32(ecx), e.read32(ecx + 4)
        for i in range(n):
            if e.fs_get(data + 0x14 * i + 4).lower() == key:
                return e.read32(data + 0x14 * i + 0x10)
        return 0

    def m_set(self, e, ecx, a):
        key = e.wstr(a[0]) or ''
        data, n, mx = e.read32(ecx), e.read32(ecx + 4), e.read32(ecx + 8)
        for i in range(n):
            if e.fs_get(data + 0x14 * i + 4).lower() == key.lower():
                e.write32(data + 0x14 * i + 0x10, e.read32(a[1]))
                return data + 0x14 * i + 0x10
        nd = e.alloc(0x14 * (n + 1) + 16)
        if n:
            e.uc.mem_write(nd, bytes(e.uc.mem_read(data, 0x14 * n)))
        e.write32(nd + 0x14 * n, 0xFFFFFFFF)
        e.fs_set(nd + 0x14 * n + 4, key)
        e.write32(nd + 0x14 * n + 0x10, e.read32(a[1]))
        e.write32(ecx, nd)
        e.write32(ecx + 4, n + 1)
        e.write32(ecx + 8, n + 1)
        return nd + 0x14 * n + 0x10

    def str_remove(self, e, ecx, a):
        key = (e.wstr(a[0]) or '').lower()
        data, n = e.read32(ecx), e.read32(ecx + 4)
        keep = [i for i in range(n) if e.fs_get(data + 0x14 * i + 4).lower() != key]
        for j, i in enumerate(keep):
            if i != j:
                e.uc.mem_write(data + 0x14 * j, bytes(e.uc.mem_read(data + 0x14 * i, 0x14)))
        e.write32(ecx + 4, len(keep))
        return n - len(keep)

    def int_find(self, e, ecx, a):
        key = e.read32(a[0])
        data, n = e.read32(ecx), e.read32(ecx + 4)
        for i in range(n):
            if e.read32(data + 0xC * i + 4) == key:
                return e.read32(data + 0xC * i + 8)
        return 0

    def int_set(self, e, ecx, a):
        key, val = e.read32(a[0]), e.read32(a[1])
        data, n = e.read32(ecx), e.read32(ecx + 4)
        for i in range(n):
            if e.read32(data + 0xC * i + 4) == key:
                e.write32(data + 0xC * i + 8, val)
                return 0
        nd = e.alloc(0xC * (n + 1) + 16)
        if n:
            e.uc.mem_write(nd, bytes(e.uc.mem_read(data, 0xC * n)))
        e.write32(nd + 0xC * n, 0xFFFFFFFF)
        e.write32(nd + 0xC * n + 4, key)
        e.write32(nd + 0xC * n + 8, val)
        e.write32(ecx, nd)
        e.write32(ecx + 4, n + 1)
        e.write32(ecx + 8, n + 1)
        return 0

    def int_remove(self, e, ecx, a):
        key = e.read32(a[0])
        data, n = e.read32(ecx), e.read32(ecx + 4)
        keep = [i for i in range(n) if e.read32(data + 0xC * i + 4) != key]
        for j, i in enumerate(keep):
            if i != j:
                e.uc.mem_write(data + 0xC * j, bytes(e.uc.mem_read(data + 0xC * i, 0xC)))
        e.write32(ecx + 4, len(keep))
        return n - len(keep)

    def arr_remove(self, e, ecx, a):
        data, n = e.read32(ecx), e.read32(ecx + 4)
        i, cnt = a[0], a[1]
        for j in range(i, n - cnt):
            e.write32(data + 4 * j, e.read32(data + 4 * (j + cnt)))
        e.write32(ecx + 4, n - cnt)
        return 0

    def cmp_friend(self, e, ecx, a):
        # (FString nameA, int onlineA, FString nameB, int onlineB) in the order they were pushed (the first argument is on top of the stack)
        na = e.wstr(a[0]) if a[0] else ''
        nb = e.wstr(a[4]) if a[4] else ''
        if a[3] != a[7]:
            return 0xFFFFFFFF if a[3] > a[7] else 1
        return 0 if na == nb else (0xFFFFFFFF if na < nb else 1)


def flist(e, c, names, label='FriendList'):
    """a FriendList (+0 word, +4 friends array of pointers, +0x10 the map by account): records with accounts `names`"""
    fl = e.raw_obj(label, 0x28)
    recs = []
    for i, nm in enumerate(names):
        r = person(e, '%s_rec%d' % (label, i), name='nick%s' % nm, status=c.get('sts', ['NLN', 'FLN', 'BSY', 'NLN'])[i % 4], f20=nm,
                   n1='dn%s' % nm, n2='')
        recs.append(r)
    data = e.alloc(4 * max(len(recs), 1) + 16)
    for i, r in enumerate(recs):
        e.write32(data + 4 * i, r)
    e.write32(fl + 4, data if recs else 0)
    e.write32(fl + 8, len(recs))
    e.write32(fl + 0xC, len(recs))
    pairs = e.alloc(0x14 * max(len(recs), 1) + 16)
    e.uc.mem_write(pairs, bytes(0x14 * max(len(recs), 1) + 16))
    for i, (nm, r) in enumerate(zip(names, recs)):
        e.write32(pairs + 0x14 * i, 0xFFFFFFFF)
        e.fs_set(pairs + 0x14 * i + 4, nm)
        e.write32(pairs + 0x14 * i + 0x10, r)
    e.write32(fl + 0x10, pairs if recs else 0)
    e.write32(fl + 0x14, len(recs))
    e.write32(fl + 0x18, len(recs))
    e.write32(fl + 0x20, 8)
    return fl


def gen7(r):
    names = r.sample(['a', 'b', 'c', 'd', 'e', 'f'], r.choice([0, 1, 2, 4]))
    return dict(names=sorted(names), acct=r.choice(['a', 'c', 'zz', 'B', '']), sts=[r.choice(['NLN', 'FLN', 'BSY']) for _ in range(4)],
                pstatus=r.choice(['NLN', 'FLN']), gid=r.choice([1, 2, 3]), ng=r.choice([0, 1, 3]), listname=r.choice(['FL', 'AL', 'BL', 'RL', 'xx', 'fl']),
                pname=r.choice(['x@y', '']), index=r.choice([0, 1, 2]))


def b_fl(argfn=None):
    def b(e, c):
        fl = flist(e, c, c['names'])
        return fl, (argfn(e, c) if argfn else [])
    return b


def person_arg(e, c):
    p = person(e, 'newrec', name='n-' + c['acct'], status=c['pstatus'], f20=c['acct'], n1='dn-' + c['acct'], n2='', gid=c['gid'])
    return [e.read32(p + 4 * i) for i in range(0x11)]


TESTS7['fl_find'] = (0x101515e0, 83, b_fl(lambda e, c: fsargs(e, c['acct'])), gen7, {'noret': False})
TESTS7['fl_add'] = (0x101519c0, 399, b_fl(person_arg), gen7, {'noret': False})
TESTS7['fl_remove'] = (0x10151f30, 171, b_fl(lambda e, c: fsargs(e, c['acct'])), gen7, {'noret': True})
TESTS7['fl_sort'] = (0x10151640, 25, b_fl(), gen7, {'noret': True})


def b_cmp(e, c):
    fl = flist(e, c, ['a', 'b'])
    data = e.read32(fl + 4)
    pa = e.alloc(8)
    e.write32(pa, e.read32(data))
    e.write32(pa + 4, e.read32(data + 4))
    # CompareFriend(const void* A, const void* B): pointers to pointers to records
    return 0, [pa, pa + 4]


TESTS7['fl_compare'] = (0x101513b0, 145, b_cmp, gen7, {'noret': False})


def b_contacts(e, c):
    ct = e.raw_obj('Contacts', 0x14)
    for i, off in enumerate((0, 4, 8, 0xC)):
        e.w32(ct, off, flist(e, c, ['a'], 'List%d' % i))
    return ct, fsargs(e, c['listname'])


TESTS7['contacts_getlist'] = (0x101511b0, 221, b_contacts, gen7, {'noret': False})


def glist(e, c):
    g = e.raw_obj('GroupList', 0x24)
    ids = list(range(1, c['ng'] + 1))
    data = e.alloc(4 * max(len(ids), 1) + 16)
    for i, gid in enumerate(ids):
        grec = e.raw_obj('Group%d' % gid, 0x1C)
        e.fs_set(grec, 'grp%d' % gid)
        e.w32(grec, 0x18, gid)
        e.write32(data + 4 * i, grec)
    e.write32(g, data if ids else 0)
    e.write32(g + 4, len(ids))
    e.write32(g + 8, len(ids))
    # the by-id map is a valid TMap<INT,pointer> (hash chains): ours inlines Set / Find and walks them
    intmap_fill(e, g + 0xC, [(gid, e.read32(data + 4 * i)) for i, gid in enumerate(ids)], 8, 0, 1)
    return g


TESTS7['gl_find'] = (0x10151660, 16, lambda e, c: (glist(e, c), [c['gid']]), gen7, {'noret': False})
TESTS7['gl_remove'] = (0x10151c90, 80, lambda e, c: (glist(e, c), [c['gid']]), gen7, {'noret': True})


def group_arg(e, c):
    p = e.raw_obj('grp', 0x1C)
    e.fs_set(p, 'newname')
    e.fs_set(p + 0xC, 'x')
    e.w32(p, 0x18, c['gid'])
    return [e.read32(p + 4 * i) for i in range(7)]


TESTS7['gl_add'] = (0x10151b90, 248, lambda e, c: (glist(e, c), group_arg(e, c)), gen7, {'noret': True})

if __name__ == '__main__':
    t_msn1.run_tests(TESTS7, LW, sys.argv)
