# Differential tests of the messenger's trees (wave C, lane proof2): MsnTree and L2FriendTree (DrawItem, the sorted insertions, the tool tip,
# the double click, the window procedure, constructors / destructors), NMsnTreeNode / NMsnTreeCtrlView (the walks) and MsnTreeCtrl (the
# roster fill: AddGroupBody, AddFriendBody, RefreshGroup, RemoveFriend, FindFriendItem); stock against ours under unicorn (t_msn1.py / tr.py).
#   python t_msn9.py [cases] [seed] [test,test,...]
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
from t_msn5 import CW, gen5, mk, make_args
import t_rest1
import t_msn8
from t_msn8 import BW, nstring

FILES = {'MsnTree': 'src/nwindow/MsnTree.cpp', 'L2FriendTree': 'src/nwindow/L2FriendTree.cpp'}
t_rest1.REST1_FILES.clear()
t_rest1.REST1_FILES.update(FILES)
CL = t_rest1.build()
t_msn5.CLASSES.update(CL)
TESTS9 = {}
tr.ARGS_OVERRIDE.update({0x10168dd0: 6, 0x101692e0: 8})      # MsnStrcmp(FString, FString), MsnCompareFriend(FString, int, FString, int): cdecl, by value
tr.POP_OVERRIDE.update({0x10168dd0: 0, 0x101692e0: 0, 0x10152c00: 0})
tr.ARGS_OVERRIDE[0x10152c00] = 2


def wtext(e, addr, text):
    e.uc.mem_write(addr, text.encode('utf-16le') + bytes(2))


def tvitem(e, label, r, text='', tag='', acct='', gid=0, kids=(), **k):
    """an NCTVItem (0x2880 bytes): the record (text, account, tag, boxes, pictures), the parent, the children, the flags"""
    it = e.raw_obj(label, 0x2880)
    wtext(e, it + 0x10, text)
    wtext(e, it + 0x1810, acct)
    wtext(e, it + 0x2010, tag)
    e.w32(it, 0x04, gid)
    e.w32(it, 0x2810, k.get('tx', 0))
    e.w32(it, 0x2814, k.get('ty', 0))
    e.wf(it, 0x2818, k.get('tw', 220.0))
    e.wf(it, 0x281C, k.get('th', 14.0))
    e.wf(it, 0x2828, k.get('i2w', 14.0))
    e.wf(it, 0x282C, k.get('i2h', 14.0))
    e.wf(it, 0x2838, k.get('iw', 15.0))
    e.wf(it, 0x283C, k.get('ih', 15.0))
    e.w32(it, 0x2840, k.get('tox', 3))
    e.w32(it, 0x2844, k.get('coy', 1))
    e.w32(it, 0x2848, 2)
    e.w32(it, 0x284C, k.get('center', 1))
    e.w32(it, 0x2850, 0x7C50 if k.get('i2t', 1) else 0)
    e.w32(it, 0x2854, 0x7C54 if k.get('ext', 1) else 0)
    e.w32(it, 0x2858, 0x7C58 if k.get('cot', 1) else 0)
    e.w32(it, 0x286C, -1)
    e.w32(it, 0x2870, k.get('exp', 0))
    e.w32(it, 0x2878, k.get('vis', 1))
    e.w32(it, 0x287C, k.get('sel', 0))
    n = len(kids)
    data = e.alloc(4 * max(n, 1) + 16)
    for i, kid in enumerate(kids):
        e.write32(data + 4 * i, kid)
        e.w32(kid, 0x285C, it)
    e.write32(it + 0x2860, data if n else 0)
    e.write32(it + 0x2864, n)
    e.write32(it + 0x2868, n)
    return it


TAGS = ['NLN', 'BRB', 'IDL', 'AWY', 'LUN', 'BSY', 'PHN', 'FLN', 'HDN', '', 'XYZ']


def gen9(r):
    return dict(rs=r.randint(0, 10 ** 6), x=r.choice([0, 5, 16, 100, 150, 300]), y=r.choice([0, 20, 100]), exp=r.choice([0, 1]), sel=r.choice([0, 1]),
                center=r.choice([0, 1]), tag=r.choice(TAGS), acct=r.choice(['a@b', 'c@d', '']), text=r.choice(['Friends', 'bob', '', 'a very long name that does not fit at all']),
                nkids=r.choice([0, 1, 3]), ktags=[r.choice(TAGS) for _ in range(4)], ext=r.choice([0, 1]), cot=r.choice([0, 1]), i2t=r.choice([0, 1]),
                iw=r.choice([15.0, 15.5, 0.0]), ih=r.choice([15.0, 12.0]), i2w=r.choice([14.0, 0.0]), i2h=r.choice([14.0, 20.5]), tox=r.choice([0, 3, 18]),
                coy=r.choice([0, 2]), tw=r.choice([220.0, 100.0, 40.0]), th=r.choice([14.0, 0.0]), listed=r.choice([0, 1]), used=r.choice([0, 18, 33]),
                w=r.choice([240.0, 300.5]), h=r.choice([200.0, 21.0]), sx=r.choice([100, 0]), sy=r.choice([50, 0]),
                ng=r.choice([0, 1, 3, 5]), gtexts=[r.choice(['Friends', 'bob', 'Family', 'alpha', '', 'Zed']) for _ in range(6)], gtext=r.choice(['Friends', 'bob', 'Family', 'alpha', '', 'Zed', 'A']),
                ftexts=[r.choice(['bob', 'alice', 'Eve', 'dave', 'carl']) for _ in range(6)], ftags=[r.choice(['NLN', 'FLN', 'BSY', 'HDN']) for _ in range(6)],
                ftext=r.choice(['bob', 'alice', 'Eve', 'dave', 'zed']), ftag=r.choice(['NLN', 'FLN', 'BSY']), msg=r.choice([0x203, 0x203, 0x201, 5]), wp=r.choice([0, 1, 5]),
                basewp=r.choice([0, 0, 1]), hover=r.choice([0, 1]), focus=r.choice([0, 1]), skin=r.choice([0, 1]), gid=r.choice([1, 2, 3, 7]), find=r.choice([0, 1, 2]))


class TW(BW):
    def __init__(self, kind):
        BW.__init__(self, kind)
        R = self.rules
        R[0x101515e0] = lambda e, ecx, a: getattr(e, 'listed_ret', 0)                              # FriendList::FindFriend(FString by value)
        R[0x10154750] = lambda e, ecx, a: getattr(e, 'child_used', 18)                             # MsnTree::DrawItem (virtual 77) of a child
        R[0x10167580] = lambda e, ecx, a: getattr(e, 'child_used', 18)                             # L2FriendTree::DrawItem
        R[0x10168dd0] = self.msn_strcmp                                                            # MsnStrcmp
        R[0x101692e0] = self.cmp_friend                                                            # MsnCompareFriend
        R[0x10003030] = lambda e, ecx, a: getattr(e, 'focus', 0)                                   # IsFocused (virtual 28)
        R[0x10032ec0] = self.next_sibling                                                          # NMsnTreeNode::GetNextSibling(node)
        R[0x10032f20] = self.next_node                                                             # NMsnTreeNode::GetNextNode(node)
        R[0x101552d0] = self.rec_dump                                                              # MsnTree::InsertGroupItem(Data): the record it is given
        R[0x101553a0] = self.rec_dump                                                              # MsnTree::InsertFriendItem(Data, Group)
        R[0x10034010] = self.del_item                                                              # NCTree::DeleteItem(&item)
        R[0x10166a60] = lambda e, ecx, a: getattr(e, 'group_by_id', {}).get(a[0], 0)               # NMsnTreeCtrlView::FindGroupNode(id)
        R[0x10152850] = lambda e, ecx, a: ecx                                                      # the name getter
        R[0x10154fe0] = self.ins_group                                                             # MsnTree::GetGroupInsertIndex(Data, Parent)
        R[0x10155120] = self.ins_friend                                                            # MsnTree::GetFriendInsertIndex(Data, Parent)
        # sub_10152c00(4, array): the array's Add(1, 4) and the new element's address (cdecl; the stock's: ours cannot run it for real)
        self.transparent.discard(0x10152c00)
        R[0x10152c00] = self.array_add1
        # the entries' constructor, the record copy and AddChild: the stock's own run for real (ours hooks them)
        self.transparent |= {0x10033d60, 0x10032c90, 0x10033800, 0x101546c0, 0x10033c90, 0x10032bf0, 0x10167130, 0x10150f20, 0x10150ff0, 0x10033e50, 0x100341b0}
        # the sorted-insertion tests copy the children array: the stock inline, ours through appMemcpy (a stock function of the hook build, an import of the standalone): real on both sides
        self.transparent.add(0x10001cb0)
        # NMsnGroup::~NMsnGroup and NMsnPassport::~NMsnPassport are one body of ours (the linker folded them): AddGroupBody's group record destructor is reported as the first
        self.fold(0x10152760, 0x10160c90)
        # NCButton::WindowProc (the stock's) is NControl::WindowProc made 0 / 1 again: the same value, ours calls the second directly
        R[0x10021e60] = lambda e, ecx, a: e.c.get('basewp', 0)
        R[0x10148f10] = lambda e, ecx, a: e.c.get('basewp', 0)

    def array_add1(self, e, ecx, a):
        size, arr = a[0], a[1]
        data, n, mx = e.read32(arr), e.read32(arr + 4), e.read32(arr + 8)
        if n + 1 > mx:
            nd = e.alloc(size * (n + 1 + 3) + 16)
            if n:
                e.uc.mem_write(nd, bytes(e.uc.mem_read(data, size * n)))
            e.write32(arr, nd)
            e.write32(arr + 8, n + 1 + 3)
            data = nd
        e.write32(arr + 4, n + 1)
        return data + n * size

    def rec_dump(self, e, ecx, a):
        d = a[0]
        e.log.append(('rec', e.wstr(d + 0x10), e.wstr(d + 0x1810), e.wstr(d + 0x2010), e.r32(d, 0x04), e.r32(d, 0x2810), e.r32(d, 0x2814), e.r32(d, 0x2818),
                      e.r32(d, 0x281C), e.r32(d, 0x2820), e.r32(d, 0x2824), e.r32(d, 0x2828), e.r32(d, 0x282C), e.r32(d, 0x2830), e.r32(d, 0x2834), e.r32(d, 0x2838),
                      e.r32(d, 0x283C), e.r32(d, 0x2840), e.r32(d, 0x2844), e.r32(d, 0x2848), e.r32(d, 0x284C), e.val(e.r32(d, 0x2850)), e.val(e.r32(d, 0x2854)),
                      e.val(e.r32(d, 0x2858)), 'group=%s' % (e.nmthis(a[1]) if len(a) > 1 and a[1] else '-')))
        return 0

    def del_item(self, e, ecx, a):
        e.log.append(('deleted', e.nmthis(e.read32(a[0])), e.wstr(e.read32(a[0]) + 0x1810)))
        return 0

    def ins_group(self, e, ecx, a):
        data, parent = a[0], a[1]
        arr, n = e.r32(parent, 0x2860), e.r32(parent, 0x2864)
        txt = e.wstr(data + 0x10) or ''
        for i in range(n):
            it = e.read32(arr + 4 * i)
            if (e.wstr(it + 0x10) or '').lower() >= txt.lower():
                return i
        return 0xFFFFFFFF

    def ins_friend(self, e, ecx, a):
        data, parent = a[0], a[1]
        arr, n = e.r32(parent, 0x2860), e.r32(parent, 0x2864)
        txt, tag = e.wstr(data + 0x10) or '', e.wstr(data + 0x2010) or ''
        for i in range(n):
            it = e.read32(arr + 4 * i)
            o_online, n_online = int((e.wstr(it + 0x2010) or '') != 'FLN'), int(tag != 'FLN')
            if o_online != n_online:
                c = 0xFFFFFFFF if o_online > n_online else 1
            else:
                t2 = e.wstr(it + 0x10) or ''
                c = 0 if t2 == txt else (0xFFFFFFFF if t2 < txt else 1)
            if c in (0, 1):
                return i
        return 0xFFFFFFFF

    def next_sibling(self, e, ecx, a):
        node = a[0]
        if not node:
            return 0
        parent = e.r32(node, 0x285C)
        if not parent:
            return 0
        data, n = e.r32(parent, 0x2860), e.r32(parent, 0x2864)
        for i in range(n):
            if e.read32(data + 4 * i) == node and i + 1 < n:
                return e.read32(data + 4 * (i + 1))
        return 0

    def next_node(self, e, ecx, a):
        node = a[0]
        if node and e.r32(node, 0x2864) > 0:
            return e.read32(e.r32(node, 0x2860))
        return self.next_sibling(e, ecx, a)

    def msn_strcmp(self, e, ecx, a):
        x, y = (e.wstr(a[0]) or '') if a[0] else '', (e.wstr(a[3]) or '') if a[3] else ''
        return 0 if x.lower() == y.lower() else (0xFFFFFFFF if x.lower() < y.lower() else 1)

    def cmp_friend(self, e, ecx, a):
        na, nb = (e.wstr(a[0]) or '') if a[0] else '', (e.wstr(a[4]) or '') if a[4] else ''
        if a[3] != a[7]:
            return 0xFFFFFFFF if a[3] > a[7] else 1
        return 0 if na == nb else (0xFFFFFFFF if na < nb else 1)


def tree_obj(e, c, cls):
    t = e.win(cls, 'Tree', c.get('w', 240.0), c.get('h', 200.0), c.get('sx', 100), c.get('sy', 50))
    e.w32(t, 0x214, 0x7D14)
    e.w32(t, 0x218, 0x7D18)
    if cls == 'MsnTree':
        e.w32(t, 0x224, e.raw_obj('Listed', 0x20))
    e.listed_ret = e.raw_obj('ListedRec', 0x50) if c.get('listed') else 0
    e.child_used = c.get('used', 18)
    return t


def item_for_draw(e, c, r):
    kids = [tvitem(e, 'Kid%d' % i, r, text='kid%d' % i, tag=c['ktags'][i], acct='k%d' % i, exp=r.choice([0, 1])) for i in range(c['nkids'])]
    return tvitem(e, 'Item', r, text=c['text'], tag=c['tag'], acct=c['acct'], kids=kids, exp=c['exp'], sel=c['sel'], center=c['center'], ext=c['ext'],
                  cot=c['cot'], i2t=c['i2t'], iw=c['iw'], ih=c['ih'], i2w=c['i2w'], i2h=c['i2h'], tox=c['tox'], coy=c['coy'], tw=c['tw'], th=c['th'])


def reg(name, cls, sva, size, build, noret=False, opts=None, gen=gen9):
    o = {'noret': noret}
    if opts:
        o.update(opts)

    def b(e, c):
        e.write32(e.console + 0x3BF4, e.new_obj('NCWnd', 'Desktop'))
        e.presence = ''
        e.shown = {}
        e.focus = c.get('focus', 0)
        return build(e, c)
    TESTS9[name] = (sva, size, b, gen, o)


def b_draw(cls):
    def b(e, c):
        r = random.Random(c['rs'])
        t = tree_obj(e, c, cls)
        return t, [e.device(), item_for_draw(e, c, r), c['x'], c['y']]
    return b


reg('MsnTree.DrawItem', 'MsnTree', 0x10154750, 2172, b_draw('MsnTree'))
reg('L2FriendTree.DrawItem', 'L2FriendTree', 0x10167580, 1205, b_draw('L2FriendTree'))


# the sorted insertions: a root / a group with children, the record of the new entry
def children_tree(e, c, r, kind):
    n = c['ng']
    if kind == 'group':
        kids = [tvitem(e, 'Grp%d' % i, r, text=c['gtexts'][i], gid=i + 1) for i in range(n)]
    else:
        kids = [tvitem(e, 'Fr%d' % i, r, text=c['ftexts'][i], tag=c['ftags'][i], acct='f%d' % i) for i in range(n)]
    return tvitem(e, 'Parent', r, text='root', kids=kids)


def new_record(e, c, kind):
    rec = e.raw_obj('NewRec', 0x285C)
    if kind == 'group':
        wtext(e, rec + 0x10, c['gtext'])
        e.w32(rec, 0x04, c['gid'])
    else:
        wtext(e, rec + 0x10, c['ftext'])
        wtext(e, rec + 0x2010, c['ftag'])
        wtext(e, rec + 0x1810, 'new@acct')
        e.w32(rec, 0x04, c['gid'])
        e.w32(rec, 0x284C, 1)
    return rec


def b_index(kind):
    def b(e, c):
        r = random.Random(c['rs'])
        t = tree_obj(e, c, 'MsnTree')
        parent = children_tree(e, c, r, kind)
        return t, [new_record(e, c, kind), parent]
    return b


reg('MsnTree.GetGroupInsertIndex', 'MsnTree', 0x10154fe0, 258, b_index('group'), opts={'ignore': {0x10152c00}})   # (the stock copies the array through its out-of-line copy, ours inline)
reg('MsnTree.GetFriendInsertIndex', 'MsnTree', 0x10155120, 373, b_index('friend'), opts={'ignore': {0x10152c00}})


def b_insert_group(e, c):
    r = random.Random(c['rs'])
    t = tree_obj(e, c, 'MsnTree')
    root = children_tree(e, c, r, 'group')
    e.w32(t, 0x1F4, root if c['ng'] or c['hover'] else 0)
    return t, [new_record(e, c, 'group')]


def b_insert_friend(e, c):
    r = random.Random(c['rs'])
    t = tree_obj(e, c, 'MsnTree')
    return t, [new_record(e, c, 'friend'), children_tree(e, c, r, 'friend')]


reg('MsnTree.InsertGroupItem', 'MsnTree', 0x101552d0, 159, b_insert_group, noret=True, opts={'nofsdtor': True})
reg('MsnTree.InsertFriendItem', 'MsnTree', 0x101553a0, 152, b_insert_friend, noret=True, opts={'nofsdtor': True})


# the tool tip
def b_tip(e, c):
    r = random.Random(c['rs'])
    t = tree_obj(e, c, 'MsnTree')
    if c['hover']:
        e.w32(t, 0x208, tvitem(e, 'Hover', r, text='x', acct=c['acct'], tx=c['x'], ty=c['y']))
    if c['skin']:
        sk = e.raw_obj('ToolTipSkin', 0x40)
        e.w32(t, 0x8C, sk)
    return t, [e.device()]


# NToolTipSkin::Draw is a forwarder in ours (NToolTip.cpp: the by-value tip is copied once more for the stock, ours keeps its own to destroy): the
# copy constructors and destructors of the tip and its 32 words are not compared
reg('MsnTree.OnDrawToolTip', 'MsnTree', 0x10155510, 361, b_tip,
    opts={'ignore': {0x10002eb0, 0x10002e40}, 'mask': {0x10032860: set(range(1, 33))}})


# L2FriendTree: the double click and the window procedure
def b_dbl(e, c):
    t = tree_obj(e, c, 'L2FriendTree')
    e.w32(t, 0x1C, e.win('NCWnd', 'ParentWnd'))
    return t, [c['wp'], ((c['y'] + c['sy']) << 16) | ((c['x'] + c['sx']) & 0xFFFF)]


def b_wp(e, c):
    t, _ = b_dbl(e, c)
    return t, [c['msg'], c['wp'], ((c['y'] + c['sy']) << 16) | ((c['x'] + c['sx']) & 0xFFFF)]


reg('L2FriendTree.OnDoubleClick', 'L2FriendTree', 0x10154630, 39, b_dbl)
reg('L2FriendTree.WindowProc', 'L2FriendTree', 0x10154660, 81, b_wp, opts={'ignore': {0x10021e60, 0x10148f10}})


# the walks of NMsnTreeNode / NMsnTreeCtrlView
def walk_tree(e, c, r):
    n = c['ng']
    groups = [tvitem(e, 'G%d' % i, r, text='g%d' % i, gid=r.choice([1, 2, 3, 7]), kids=[tvitem(e, 'F%d_%d' % (i, j), r, text='f') for j in range(r.choice([0, 1, 2]))])
              for i in range(n)]
    root = tvitem(e, 'Root', r, text='root', kids=groups)
    return root, groups


def b_nextsib(e, c):
    r = random.Random(c['rs'])
    root, groups = walk_tree(e, c, r)
    pool = [0, root] + groups
    node = pool[c['find'] * 2 + c['hover'] if c['find'] * 2 + c['hover'] < len(pool) else 0] if c['gid'] != 7 else (groups[-1] if groups else 0)
    return (node or e.raw_obj('Walker', 0x20)), [node]


reg('NMsnTreeNode.GetNextSibling', 'NMsnTreeNode', 0x10032ec0, 85, b_nextsib, gen=gen9)
reg('NMsnTreeNode.GetNextNode', 'NMsnTreeNode', 0x10032f20, 38, b_nextsib, gen=gen9)


def b_findgroup(e, c):
    r = random.Random(c['rs'])
    root, groups = walk_tree(e, c, r)
    view = e.raw_obj('View', 0x150)
    rootview = e.raw_obj('RootView', 0x200)
    e.w32(view, 0x144, rootview)
    e.w32(rootview, 0x1F4, root)
    return view, [c['gid']]


reg('NMsnTreeCtrlView.FindGroupNode', 'NMsnTreeCtrlView', 0x10166a60, 52, b_findgroup, gen=gen9)

# ---- MsnTreeCtrl: the roster fill -----------------------------------------------------------------------------------------------------
TW_EXTRA = {0x10032bf0, 0x10167130}


def ctrl_obj(e, c, tree=True):
    ctl = e.win('MsnTreeCtrl', 'Ctrl', 240, 270)
    if tree:
        t = tree_obj(e, c, 'MsnTree')
        e.w32(ctl, 0x144, t)
    return ctl


def group_words(e, c):
    g = e.raw_obj('GroupArg', 0x1C)
    e.fs_set(g, c['gtext'])
    e.fs_set(g + 0xC, 'decoded-' + c['gtext'])
    e.w32(g, 0x18, c['gid'])
    return [e.read32(g + 4 * i) for i in range(7)]


def b_addgroup(e, c):
    ctl = ctrl_obj(e, c, tree=c['hover'] == 1 or c['find'] > 0)
    return ctl, group_words(e, c)


def person_words(e, c):
    p = person(e, 'PersonArg', name='n' + c['ftext'], status=c['ftag'], f20='acct-' + c['ftext'], n1=c['ftext'], n2='', gid=c['gid'])
    return [e.read32(p + 4 * i) for i in range(0x11)]


def b_addfriend(e, c):
    r = random.Random(c['rs'])
    ctl = ctrl_obj(e, c)
    e.group_by_id = {c['gid']: tvitem(e, 'GroupNode', r, text='grp', gid=c['gid'])} if c['find'] > 0 else {}
    return ctl, person_words(e, c)


def roster(e, c, r):
    n = c['ng']
    groups = []
    for i in range(n):
        fr = [tvitem(e, 'Fr%d_%d' % (i, j), r, text=c['ftexts'][j], tag=c['ftags'][j], acct=r.choice(['a@b', 'c@d', 'e@f', 'g@h'])) for j in range(r.choice([0, 1, 3]))]
        groups.append(tvitem(e, 'Grp%d' % i, r, text=c['gtexts'][i], gid=c['gid'] + i, kids=fr))
    return tvitem(e, 'Root', r, text='root', kids=groups), groups


def b_removefriend(e, c):
    r = random.Random(c['rs'])
    ctl = ctrl_obj(e, c)
    root, groups = roster(e, c, r)
    e.w32(e.r32(ctl, 0x144), 0x1F4, root)
    acct = fsargs(e, r.choice(['a@b', 'c@d', 'e@f', 'zz']))
    return ctl, acct


def b_findfriend(e, c):
    r = random.Random(c['rs'])
    ctl = ctrl_obj(e, c)
    root, groups = roster(e, c, r)
    item = root if c['hover'] else (groups[0] if groups else 0)
    return ctl, [e.put_wstr(r.choice(['a@b', 'c@d', 'e@f', 'zz'])), item]


def b_refresh(e, c):
    r = random.Random(c['rs'])
    ctl = ctrl_obj(e, c)
    root, groups = roster(e, c, r)
    e.group_by_id = {g_id: g for g_id, g in ((e.r32(g, 4), g) for g in groups)}
    persons = [person(e, 'Pers%d' % i, name='n%d' % i, status=r.choice(['NLN', 'FLN']), f20='acct%d' % i, n1='p%d' % i, n2='', gid=c['gid'] + r.choice([0, 1, 2, 9]))
               for i in range(r.choice([0, 1, 3]))]
    book = e.raw_obj('Book', 0x20)
    data = e.alloc(4 * max(len(persons), 1) + 16)
    for i, p in enumerate(persons):
        e.write32(data + 4 * i, p if r.choice([1, 1, 1, 0]) else 0)
    e.write32(book + 4, data if persons else 0)
    e.write32(book + 8, len(persons))
    e.write32(book + 12, len(persons))
    e.w32(ctl, 0x13C, book)
    cell = e.raw_obj('GroupCell', 8)
    target = groups[c['find'] % len(groups)] if groups else 0
    e.write32(cell, target if c['hover'] else 0)
    return ctl, [cell]


reg('MsnTreeCtrl.AddGroupBody', 'MsnTreeCtrl', 0x10167290, 361, b_addgroup, noret=True, opts={'ignore': {0x10152850, 0x101528b0}})   # (the stock calls the two getters of the group, ours reads the fields)
reg('MsnTreeCtrl.AddFriendBody', 'MsnTreeCtrl', 0x10167400, 379, b_addfriend, noret=True)
reg('MsnTreeCtrl.RemoveFriend', 'MsnTreeCtrl', 0x10167ee0, 403, b_removefriend, noret=True, opts={'ignore': {0x10152c00}})
reg('MsnTreeCtrl.FindFriendItem', 'MsnTreeCtrl', 0x10166aa0, 121, b_findfriend, noret=False)
# (word 0 of a person record is not touched by its constructor: the stock leaves its stack's garbage in it, ours another)
reg('MsnTreeCtrl.RefreshGroup', 'MsnTreeCtrl', 0x10167ce0, 176, b_refresh, noret=True, opts={'ignore': {0x10152c00}, 'mask': {0x10167400: {0}}})

# the constructors and destructors of the two trees
for cls, (cva, csz, ckinds, methods) in CL.items():
    def bctor(e, c, cls=cls, ckinds=ckinds):
        return poison(e, cls, cls), make_args(e, ckinds)
    TESTS9['%s.ctor' % cls] = (cva, csz, bctor, lambda r: {}, {'noret': True})

    def bdtor(e, c, cls=cls, cva=cva, csz=csz, ckinds=ckinds):
        p = poison(e, cls, cls, 0)
        e.run_real(cva, csz, p, make_args(e, ckinds))
        e.pre_len = len(e.log)
        return p, []
    for va2, (sz2, nm2, cl2) in tr.functions().items():
        if nm2 == '%s::~%s' % (cls, cls):
            TESTS9['%s.dtor' % cls] = (va2, sz2, bdtor, lambda r: {}, {'noret': True})

if __name__ == '__main__':
    t_msn1.run_tests(TESTS9, TW, sys.argv)
