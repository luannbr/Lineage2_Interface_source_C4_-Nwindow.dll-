# Differential tests of the Lineage II friend page and the task windows (wave C, lane proof2): L2FriendWnd, L2TaskWnd, TaskWnd, L2Task,
# L2Friend, NL2Book: stock against ours under unicorn (t_msn1.py / tr.py).
#   python t_msn4.py [cases] [seed] [test,test,...]
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

TESTS4 = {}


class FW(MW):
    def __init__(self, kind):
        MW.__init__(self, kind)
        R = self.rules
        R[0x101514f0] = lambda e, ecx, a: e.chat_by_key
        R[0x10125580] = lambda e, ecx, a: 0
        R[0x101494e0] = lambda e, ecx, a: e.task_found                        # the task map's Find
        R[0x1014a080] = lambda e, ecx, a: e.task_found                        # L2TaskWnd::FindTask (by name)
        R[0x10151670] = lambda e, ecx, a: e.book_found                        # NL2Book::Find
        # the task map's destructors and Rehash run for real: ours inlines the first and hooks the others
        self.transparent |= {0x1014a210, 0x10142000, 0x10149ae0, 0x10140ff0, 0x1013fe50}
        if not sa.ENABLED:
            # hook build: ours inlines NL2Book::Find into FindFriend, so it calls the map's FindWnd (0x101514f0, on the map at Book+0xC) itself: the stock's Find runs for real
            # and the answer comes from the FindWnd rule (the book's record in the tests that look in the book)
            self.transparent.add(0x10151670)
            R[0x101514f0] = lambda e, ecx, a: e.book_found if e.nmthis(ecx).startswith('Book') else e.chat_by_key

    def reset_case(self, c):
        MW.reset_case(self, c)
        self.task_found = self.book_found = 0


def l2chat(e, label, inuse=0):
    w = e.win('L2ChatWnd', label, 256, 401)
    e.w32(w, 0xF8, inuse)
    e.fs_set(w + 0xEC, 'friend-' + label)
    return w


def l2task(e, label, name='nm', chat=0):
    w = e.win('L2Task', label, 150, 20)
    e.fs_set(w + 0xE8, name)
    e.w32(w, 0xF4, chat)
    e.w32(w, 0xF8, 1)
    return w


def taskmap(e, base, pairs):
    """TMap<NCWnd*,NCWnd*> at base: pairs {HashNext, Key, Value}"""
    n = len(pairs)
    data = e.alloc(0xC * max(n, 1) + 16)
    e.uc.mem_write(data, bytes(0xC * max(n, 1) + 16))
    for i, (k, v) in enumerate(pairs):
        e.write32(data + 0xC * i, 0xFFFFFFFF)
        e.write32(data + 0xC * i + 4, k)
        e.write32(data + 0xC * i + 8, v)
    e.write32(base, data if n else 0)
    e.write32(base + 4, n)
    e.write32(base + 8, n)
    e.write32(base + 0x10, 8)


def l2friend(e, c):
    w = e.win('L2FriendWnd', 'L2FriendWnd')
    if c.get('book', 1):
        e.w32(w, 0xE8, e.raw_obj('Book', 0x20))
    chats = [('c%d' % i, l2chat(e, 'Chat%d' % i, inuse=(c.get('inuse', 0) >> i) & 1)) for i in range(c.get('nchat', 0))]
    strmap(e, w + 0xEC, chats)
    free = [(0x100 + i, l2chat(e, 'Free%d' % i, inuse=(c.get('finuse', 0) >> i) & 1)) for i in range(c.get('nfree', 0))]
    intmap(e, w + 0x100, free)
    if c.get('tree', 1):
        tc = e.win('L2FriendTreeCtrl', 'TreeCtrl', 240, 270)
        tree = e.raw_obj('FriendTree', 0x21C)
        e.w32(tree, 0x1F4, e.raw_obj('Root', 0x40))
        e.w32(tc, 0x138, tree)
        e.w32(w, 0x114, tc)
    if c.get('task', 1):
        tw = e.win('L2TaskWnd', 'TaskWnd', 172, 132)
        e.w32(tw, 0xEC, c.get('ntasks', 1))
        e.w32(w, 0x118, tw)
    e.w32(w, 0x11C, 0x7777 if c.get('back', 1) else 0)
    e.write32(e.console + 0x3BF4, e.new_obj('NCWnd', 'Desktop'))
    return w


def gen4(r):
    d = dict(book=r.choice([0, 1]), nchat=r.choice([0, 1, 2]), inuse=r.randint(0, 3), nfree=r.choice([0, 1, 2, 3]), finuse=r.randint(0, 7),
             tree=r.choice([0, 1, 1]), task=r.choice([0, 1, 1]), ntasks=r.choice([0, 1, 2]), back=r.choice([0, 1]),
             hit=r.choice([0, 1, 1]), hit3=r.choice([3, 3, 2]), hit8=r.choice([0, 1]), x=r.choice([5, 40]), y=r.choice([5, 60]),
             bykey=r.choice([0, 1]), found=r.choice([0, 1]), text=r.choice(['frnd', '', 'x y']), w=r.choice([256.0, 276.5]),
             h=r.choice([353.0, 400.0]), sx=r.choice([100, 0]), sy=r.choice([50, 0]), shown=r.choice([0, 1]), basewp=r.choice([0, 0, 1]), msg=r.choice([0x203, 0x203, 0x5]))
    return d


def std4(argfn=None, lookups=True):
    def b(e, c):
        w = l2friend(e, c)
        if c.get('hit') and e.r32(w, 0x114):
            tree = e.r32(e.r32(w, 0x114), 0x138)
            hit = e.raw_obj('HitItem', 0x2880)
            e.w32(hit, 0x2848, c.get('hit3', 3))
            e.w32(hit, 8, c.get('hit8', 1))
            e.write_wstr_at(hit + 0x10, 'friendname')
            e.hit_item = hit
        if c.get('bykey') and e.r32(w, 0xEC):
            e.chat_by_key = e.r32(e.r32(w, 0xEC), 0x10)
        if c.get('found'):
            e.task_found = l2task(e, 'FoundTask')
            e.book_found = e.raw_obj('BookRec', 0x30)
        return w, (argfn(e, c) if argfn else [])
    return b


def reg4(name, sva, size, argfn=None, noret=True, ignore=None, ignore_alloc=False):
    o = {'noret': noret}
    if ignore:
        o['ignore'] = ignore
    if ignore_alloc:
        o['ignore_alloc'] = True
    TESTS4[name] = (sva, size, std4(argfn), gen4, o)


reg4('l2f_oncreate', 0x1015ab80, 193, noret=False)
reg4('l2f_ondestroy', 0x10158ad0, 63, noret=False)
reg4('l2f_show', 0x1015bd60, 118)
reg4('l2f_onpaint', 0x10158a30, 153, lambda e, c: [e.device()], noret=False)
reg4('l2f_showtask', 0x1015ac50, 238)
reg4('l2f_freechat', 0x1015bde0, 93, noret=False)
reg4('l2f_dblclick', 0x1015de80, 129, lambda e, c: [c['x'], c['y']], noret=False)
reg4('l2f_openchat', 0x1015d640, 649, lambda e, c: fsargs(e, c['text']), ignore_alloc=True)
reg4('l2f_findfriend', 0x101560c0, 104, lambda e, c: fsargs(e, c['text']), noret=False)
reg4('l2f_whisper', 0x10156020, 112, lambda e, c: fsargs(e, c['text']) + fsargs(e, 'hello'))
reg4('l2f_chatclosed', 0x1015de00, 115, lambda e, c: [l2chat(e, 'Closed')])
reg4('l2f_winproc', 0x1015f5f0, 81, lambda e, c: [c['msg'], 5, 6], noret=False)
TESTS4['l2f_ctor'] = (0x101600c0, 215, lambda e, c: (poison(e, 'L2FriendWnd', 'L2FriendWnd'), []), lambda r: {}, {'noret': True})


def b_l2f_dtor(e, c):
    p = poison(e, 'L2FriendWnd', 'L2FriendWnd')
    e.run_real(0x101600c0, 215, p)
    e.pre_len = len(e.log)
    return p, []


TESTS4['l2f_dtor'] = (0x101601d0, 112, b_l2f_dtor, lambda r: {}, {'noret': True})


def b_book_ctor(e, c):
    p = e.alloc(0x30)
    e.uc.mem_write(p, bytes([0xCD]) * 0x30)
    e.objs[p] = 'Book'
    e.reg_size[p] = 0x20
    return p, []


TESTS4['l2f_bookctor'] = (0x101525f0, 98, b_book_ctor, lambda r: {}, {'noret': True})
TESTS4['l2f_bookfind'] = (0x10151670, 83, lambda e, c: (e.raw_obj('Book', 0x20), fsargs(e, 'who')), lambda r: {'found': r.choice([0, 1])}, {'noret': False})


# ---- the task windows ------------------------------------------------------------------------------------------------------------
def l2taskwnd(e, c, cls='L2TaskWnd'):
    w = e.win(cls, cls, 172, 132)
    n = c.get('ntasks', 2)
    tasks = []
    for i in range(n):
        t = l2task(e, 'Task%d' % i, name=('n%d' % i) if c.get('named', 1) else 'zz', chat=0)
        tasks.append((0x500 + i, t))
    taskmap(e, w + 0xE8, tasks)
    e.w32(w, 0xFC, e.win('L2TaskBox', 'List', 140, 126) if c.get('list', 1) else 0)
    for off in (0x100, 0x104, 0x108):
        e.w32(w, off, 0x7000 + off if c.get('tex', 1) else 0)
    e.w32(w, 0xA0 - 0xA0 + 0x7C, e.win('NCFrameCtrl', 'Frame') if c.get('frame', 0) else 0)
    return w


def gen_task(r):
    return dict(ntasks=r.choice([0, 1, 2, 3]), named=r.choice([0, 1]), list=r.choice([0, 1, 1]), tex=r.choice([0, 1, 1]), found=r.choice([0, 1]),
                text=r.choice(['n0', 'n1', 'qq', '']))


def reg_task(name, sva, size, cls='L2TaskWnd', argfn=None, noret=True, gen=gen_task, ignore=None):
    def b(e, c):
        w = l2taskwnd(e, c, cls)
        if c.get('found'):
            e.task_found = e.r32(w + 0xE8, 0) and e.r32(e.r32(w + 0xE8, 0), 8) or l2task(e, 'FoundTask')
        return w, (argfn(e, c) if argfn else [])
    o = {'noret': noret}
    if ignore:
        o['ignore'] = ignore
    TESTS4[name] = (sva, size, b, gen, o)


reg_task('l2t_oncreate', 0x1014ab70, 400, noret=False)
reg_task('l2t_ondestroy', 0x10149f50, 218, noret=False)
reg_task('l2t_removetask', 0x1014a030, 79, argfn=lambda e, c: [e.win('L2ChatWnd', 'KeyChat', 256, 401)])
reg_task('l2t_findtask', 0x1014a080, 191, argfn=lambda e, c: fsargs(e, c['text']), noret=False)
reg_task('t_onpaint', 0x1013fab0, 239, cls='TaskWnd', argfn=lambda e, c: [e.device()], noret=False)
reg_task('t_oncreate', 0x10143430, 400, cls='TaskWnd', noret=False)
reg_task('t_ondestroy', 0x10141b50, 218, cls='TaskWnd', noret=False)
TESTS4['l2t_ctor'] = (0x1014aec0, 112, lambda e, c: (poison(e, 'L2TaskWnd', 'L2TaskWnd'), []), lambda r: {}, {'noret': True})
TESTS4['t_ctor'] = (0x101446f0, 112, lambda e, c: (poison(e, 'TaskWnd', 'TaskWnd'), []), lambda r: {}, {'noret': True})


def b_l2t_dtor(e, c):
    p = poison(e, 'L2TaskWnd', 'L2TaskWnd')
    e.run_real(0x1014aec0, 112, p)
    e.pre_len = len(e.log)
    return p, []


def b_t_dtor(e, c):
    p = poison(e, 'TaskWnd', 'TaskWnd')
    e.run_real(0x101446f0, 112, p)
    e.pre_len = len(e.log)
    return p, []


TESTS4['l2t_dtor'] = (0x1014af60, 96, b_l2t_dtor, lambda r: {}, {'noret': True})
TESTS4['t_dtor'] = (0x10144790, 96, b_t_dtor, lambda r: {}, {'noret': True})

# L2Task (the entry of one conversation in the list) and L2Friend (a record)
TESTS4['l2task_ctor'] = (0x1014a8b0, 102, lambda e, c: (poison(e, 'L2Task', 'L2Task'), []), lambda r: {}, {'noret': True})


def b_l2task_dtor(e, c):
    p = poison(e, 'L2Task', 'L2Task')
    e.run_real(0x1014a8b0, 102, p)
    e.pre_len = len(e.log)
    return p, []


TESTS4['l2task_dtor'] = (0x1014a950, 97, b_l2task_dtor, lambda r: {}, {'noret': True})


def b_l2task_obj(e, c):
    w = l2task(e, 'L2Task', name='nm', chat=e.win('L2ChatWnd', 'Chat', 256, 401))
    e.w32(w, 0xFC, c.get('blink', 0))
    e.w32(w, 0x100, c.get('th', 0))
    return w, []


def gen_l2task(r):
    return dict(tid=r.choice([0xA73, 0xA73, 5]), blink=r.choice([0, 1]), th=r.choice([0, 9]))


TESTS4['l2task_oncreate'] = (0x10148e20, 40, b_l2task_obj, gen_l2task, {'noret': False})
TESTS4['l2task_ontimer'] = (0x10148e50, 37, lambda e, c: (b_l2task_obj(e, c)[0], [c['tid']]), gen_l2task, {'noret': False})
TESTS4['l2task_ondestroy'] = (0x10148e80, 43, b_l2task_obj, gen_l2task, {'noret': False})


if __name__ == '__main__':
    t_msn1.run_tests(TESTS4, FW, sys.argv)
