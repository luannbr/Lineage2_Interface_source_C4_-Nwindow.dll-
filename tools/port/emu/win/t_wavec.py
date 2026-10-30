# Differential test of the small functions of wave C (misc lane): stock vs ours under unicorn, on random objects.
#   NCTabButton (ctor 0x10003B00 and the four accessors 0x10001EF0 / F00 / F10 / 0x10002000), NCTabControl(INT) 0x1002E860, NCEffectButton ctor
#   0x10003C00, the folded getters and empties (0x10005B90 / 0x1014FC10 / 0x100023B0 / 0x100023D0), NCPawnSelectWnd::GetSelected 0x1009B6A0,
#   NTownMapWnd/NCMapCoverWnd KillFocus 0x100B9010, NCBBSHtmlViewer::OnTimer 0x1003B430, the party matching list controls' constructors
#   (0x100B4500 / 0x100B5450 / 0x100B54F0), NCChatListBox OnDestroy / SetWindowSize (0x10052910 / 0x10052AD0), NCTrainWnd::OnDestroy 0x1011DC90,
#   NCTrainListBox::OnDestroy 0x1011DDC0, NCComboBox::AddSysStringItem 0x10006850, NCPetitionWnd::AddMessage 0x100DFC60, NSearchMapIterator::Relax
#   0x10150230, the pointer array's new element 0x10152C00 and Remove 0x1001F3F0.
# Every callee is a logging stub (stock: patched at its VA; ours: at our compiled copy and at the hook's address); the object is a poisoned
# buffer; compared: the calls (with the arguments, objects by name), the object's words and the result.
#   python t_wavec.py [cases] [seed] [function,function...]
import sys, os, random, struct
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from gw import *
import gw
import sa

nosa = {}
# standalone mode: the stock function that the hook registration names TPtrArray::Remove is TArray<void*>::Remove in the standalone (typedef), which
# sa.func_map() does not find by class name; the compiled template instance is taken by its symbol
SA_SYMS = {'remove4': '?Remove@?$TArray@PAX@@QAEXHH@Z'}
gw.VPOP[76] = 12
gw.VPOP[3] = 4

FUNCS = {
    'tabctor': 0x10003B00, 'tabget': 0x10001EF0, 'tabset': 0x10001F00, 'tabidx': 0x10001F10, 'taborg': 0x10002000, 'tabcc': 0x1002E860,
    'effbtn': 0x10003C00, 'get08': 0x10005B90, 'get50': 0x1014FC10, 'empty8': 0x100023B0, 'empty4': 0x100023D0, 'getsel': 0x1009B6A0,
    'killfocus': 0x100B9010, 'bbstimer': 0x1003B430, 'matchctor4': 0x100B4500, 'partyctor0': 0x100B5450, 'partyctor4': 0x100B54F0,
    'chatdestroy': 0x10052910, 'chatsetsize': 0x10052AD0, 'traindestroy': 0x1011DC90, 'trainlbdestroy': 0x1011DDC0,
    'combo': 0x10006850, 'petmsg': 0x100DFC60, 'relax': 0x10150230, 'newelem': 0x10152C00, 'remove4': 0x1001F3F0,
    'trainctor0': 0x1011E300, 'trainctor3': 0x1011E3C0,
}
# callee stock va: (name, bytes the callee pops, returns this)
CALLEES = {
    0x100039C0: ('RadioCtor2', 8, 1), 0x10036CE0: ('NCWndCtor', 0, 1), 0x10003410: ('ButtonCtor4', 0x10, 1),
    0x10023970: ('ListCtrlCtor1', 4, 1), 0x10023B10: ('ListCtrlCtor6', 0x18, 1), 0x1002B590: ('UnlockCursor', 0, 0),
    0x100347A0: ('WndKillFocus', 0, 0), 0x10021490: ('TextListOnDestroy', 0, 0), 0x10036DC0: ('WndSetSize', 8, 0),
    0x10035880: ('RemoveChildWindow', 4, 0), 0x10036160: ('WndOnDestroy', 0, 0), 0x10021060: ('ListBoxOnDestroy', 0, 0),
    0x10005BA0: ('ItemSetData', 4, 0), 0x10089D40: ('MapRehash', 0, 0), 0x100218E0: ('ListBoxCtor0', 0, 1), 0x10021980: ('ListBoxCtor3', 0xC, 1),
}
OURS_SYMS = {
    0x100039C0: '??0NCRadioButton@@QAE@PBG0@Z', 0x10036CE0: '??0NCWnd@@QAE@XZ', 0x10003410: '??0NCButton@@QAE@PBG00H@Z',
    0x10023970: '??0NCListCtrl@@QAE@H@Z', 0x10023B10: '??0NCListCtrl@@QAE@HHHHHH@Z', 0x1002B590: '?UnlockCursor@NCObject@@QAEXXZ',
    0x100347A0: '?KillFocus@NCWnd@@UAEXXZ', 0x10021490: '?OnDestroy@NCTextListBox@@UAEHXZ', 0x10036DC0: '?SetWindowSize@NCWnd@@UAEXMM@Z',
    0x10035880: '?RemoveChildWindow@NCWnd@@QAEXPAUNWndEntry@@@Z', 0x10036160: '?OnDestroy@NCWnd@@UAEHXZ',
    0x10021060: '?OnDestroy@NCListBox@@UAEHXZ', 0x10089D40: '?Rehash@NMsnIntMap@@QAEXXZ', 0x100218E0: '??0NCListBox@@QAE@XZ',
    0x10021980: '??0NCListBox@@QAE@HHH@Z',
}
SIZES = {'tabctor': 0x2D8, 'tabget': 0x2D8, 'tabset': 0x2D8, 'tabidx': 0x2D8, 'taborg': 0x2D8, 'tabcc': 0x21C, 'effbtn': 0x2E4, 'get08': 0x40,
         'get50': 0x80, 'empty8': 0x10, 'empty4': 0x10, 'getsel': 0x1100, 'killfocus': 0x100, 'bbstimer': 0x200, 'matchctor4': 0x270,
         'partyctor0': 0x270, 'partyctor4': 0x270, 'chatdestroy': 0x290, 'chatsetsize': 0x290, 'traindestroy': 0x100, 'trainlbdestroy': 0x260,
         'combo': 0x250, 'petmsg': 0x100, 'relax': 0x10, 'newelem': 0x10, 'remove4': 0x10, 'trainctor0': 0x254, 'trainctor3': 0x254}


class WC(GW):
    def __init__(self, kind):
        GW.__init__(self, kind, CALLEES, OURS_SYMS)
        self.rets['TextListOnDestroy'] = lambda e, t, a: 1
        self.rets['WndOnDestroy'] = lambda e, t, a: 1
        self.rets['ListBoxOnDestroy'] = lambda e, t, a: 1

    def dispatch(self, name):
        if name == 'cb:ItemSetData':
            # the folded setter of the dword at +8: really done, NOT logged any more (transparent): the current sources inline it
            # (`Item.Data = Data` in NCComboBox::AddSysStringItem, hook build and standalone alike), so only the stock would log the call;
            # the store is still compared (the first items of the array are in res['items'])
            self.last_pop = 4
            t = self.this()
            a = self.sargs(1)
            self.write32(t + 8, a[0])
            return None
        return GW.dispatch(self, name)


def poison(r, size):
    b = bytearray(r.randbytes(size))
    return bytes(b)


def run(e, A, c):
    mark = len(e.log)
    f = c['f']
    rnd = random.Random(c['seed'])
    size = SIZES[f]
    obj = e.alloc(size + 16)
    e.uc.mem_write(obj, poison(rnd, size + 16))
    e.objs[obj] = 'obj'
    args = []
    extra = {}
    if f == 'tabctor':
        args = [e.put_wstr('normal'), e.put_wstr('pushed')]
    elif f in ('tabset', 'tabidx'):
        args = [c['v'] & 0xFFFFFFFF]
    elif f == 'taborg':
        args = [c['v'] & 0xFFFFFFFF, c['w'] & 0xFFFFFFFF]
    elif f == 'tabcc':
        args = [c['v'] & 0xFFFFFFFF]
    elif f == 'effbtn':
        args = [c['v'] & 0xFFFFFFFF, e.put_wstr('n'), e.put_wstr('p'), e.put_wstr('o'), c['w'] & 0xFFFFFFFF, c['x'] & 0xFFFFFFFF]
    elif f == 'empty8':
        args = [c['v'], c['w']]
    elif f == 'empty4':
        args = [c['v']]
    elif f == 'bbstimer':
        args = [c['timer'] & 0xFFFFFFFF]
    elif f in ('matchctor4', 'partyctor4'):
        args = [c['v'], c['w'], c['x'], c['y']]
    elif f == 'trainctor3':
        args = [c['v'] & 0xFFFFFFFF, c['w'] & 0xFFFFFFFF, c['x'] & 0xFFFFFFFF]
    elif f == 'chatsetsize':
        args = [f32(c['fw']), f32(c['fh'])]
    elif f == 'traindestroy':
        ch = []
        for off in (0xE8, 0xEC):
            if c['child%x' % off]:
                w = e.window('child%x' % off, size=0x100)
                ch.append(w)
                e.write32(obj + off, w)
            else:
                e.write32(obj + off, 0)
    elif f == 'trainlbdestroy':
        n = c['n']
        data = e.alloc(4 * max(n, 1) + 16)
        for i in range(n):
            e.write32(data + 4 * i, c['ents'][i] and e.alloc(16))
        e.write32(obj + 0x240, data if n else 0)
        e.write32(obj + 0x244, n)
        e.write32(obj + 0x248, n + c['slack'])
    elif f == 'combo':
        data = e.alloc(12 * 8 + 16)
        num = c['num']
        e.write32(obj + 0x1FC, data)
        e.write32(obj + 0x200, num)
        e.write32(obj + 0x204, 8)
        args = [c['v'] & 0xFFFFFFFF, c['w'] & 0xFFFFFFFF]
    elif f == 'petmsg':
        lst = e.window('list', size=0x100) if c['list'] else 0
        e.write32(obj + 0xF0, lst)
        args = [e.put_wstr(c['text']), c['v'] & 0xFFFFFFFF, c['type'] & 0xFFFFFFFF, 0]
    elif f == 'relax':
        m = e.alloc(0x20)
        e.uc.mem_write(m, bytes(0x20))
        e.write32(m + 4, c['num'])
        e.write32(m + 0x10, c['hash'])
        e.objs[m] = 'map'
        e.write32(obj, m)
        e.write32(obj + 4, m)
        e.write32(obj + 8, 3)
        e.write32(obj + 0xC, c['removed'])
        extra['map'] = m
    elif f in ('newelem', 'remove4'):
        data = e.alloc(4 * 16 + 16)
        arr = e.alloc(16)
        for i in range(c['num']):
            e.write32(data + 4 * i, 0x1000 + i)
        e.write32(arr, data)
        e.write32(arr + 4, c['num'])
        e.write32(arr + 8, c['max'])
        e.objs[arr] = 'array'
        extra['arr'] = arr
        if f == 'newelem':
            args = [4, arr]
        else:
            args = [c['v'] & 0xFFFFFFFF, c['w'] & 0xFFFFFFFF]
            obj = arr
    this = obj
    if f == 'newelem':
        this = None
    raised = None
    try:
        r = e.call(A[f], this=this, args=args, conv='cdecl' if f == 'newelem' else 'thiscall')
    except RuntimeError as ex:
        raised = 'assert' if 'appFailAssert' in str(ex) else str(ex)[:80]
        r = None
    res = dict(log=e.log[mark:], obj=e.snapshot(obj, size), raised=raised)
    if f in ('get08', 'get50', 'empty8', 'empty4', 'getsel', 'bbstimer', 'chatdestroy', 'traindestroy', 'trainlbdestroy', 'killfocus'):
        res['ret'] = r
    if f == 'newelem' and raised is None:
        a = extra['arr']
        res['ret'] = (r - e.read32(a)) // 4
        res['arr'] = [e.read32(a + k) for k in (4, 8)]
    if f == 'remove4':
        res['arr'] = [e.read32(obj + k) for k in (4, 8)]
    if f == 'relax':
        res['map'] = [e.read32(extra['map'] + k) for k in (4, 0x10)]
    if f == 'combo':
        res['items'] = [e.read32(obj + 0x1FC + 4 * k) for k in (1, 2)] + [bytes(e.uc.mem_read(e.read32(obj + 0x1FC), 12 * 3)) if e.read32(obj + 0x1FC) else b'']
    return res


def rand_case(r, f):
    c = dict(f=f, seed=r.randint(0, 10 ** 9))
    c['v'] = r.choice([0, 1, -1, 5, 0x7FFFFFFF, r.randint(-1000, 1000)])
    c['w'] = r.choice([0, 1, -1, 500, r.randint(-1000, 1000)])
    c['x'] = r.choice([0, 1, r.randint(-5, 5)])
    c['y'] = r.choice([0, 3, 17])
    c['timer'] = r.choice([0xFF032113, 0xFF032112, 0, 1, 0xFF032114, -1])
    c['fw'] = r.choice([0, 100.5, 348.0, -3.25])
    c['fh'] = r.choice([0, 15.0, 200.25])
    c['child%x' % 0xE8] = r.randint(0, 1)
    c['child%x' % 0xEC] = r.randint(0, 1)
    c['n'] = r.choice([0, 0, 1, 2, 5])
    c['ents'] = [r.randint(0, 1) for _ in range(5)]
    c['slack'] = r.choice([0, 3])
    c['num'] = r.choice([0, 1, 3, 6])
    c['hash'] = r.choice([8, 16, 64, 1024, 9, 40, 1000])
    c['removed'] = r.choice([0, 1, 5])
    c['list'] = r.randint(0, 1)
    c['text'] = r.choice(['', 'hello', 'a petition line'])
    c['type'] = r.choice([0, 1, 5, 6, 7, -6])
    if f == 'newelem':
        c['num'] = r.choice([0, 1, 5, 15])
        c['max'] = c['num'] + r.choice([0, 3])
    if f == 'remove4':
        c['num'] = r.choice([0, 1, 5, 10])
        c['max'] = c['num'] + 2
        if r.random() < 0.7:
            c['v'] = r.randint(0, c['num'])
            c['w'] = r.randint(0, c['num'] - c['v'])
        else:   # the three check()s: Index < 0, Index > Num, Index + Count > Num
            c['v'] = r.choice([0, 1, 3, 5, 10, -1, 11])
            c['w'] = r.choice([0, 1, 2, 5, 11])
    if f == 'combo':
        c['num'] = r.choice([0, 1, 3])
        c['v'] = r.choice([-1, -5, 0, 1, 641, 1048])
    return c


def main():
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 40
    seed = int(sys.argv[2]) if len(sys.argv) > 2 else 1
    which = sys.argv[3].split(',') if len(sys.argv) > 3 else list(FUNCS)
    es = WC('stock')
    eo = WC('ours')
    lm = log_map()
    es.heap_mark, eo.heap_mark = es.heap_ptr, eo.heap_ptr
    r = random.Random(seed)
    As = dict(FUNCS)
    Ao = {}
    for k, v in FUNCS.items():
        try:
            Ao[k] = lm[v]
        except sa.NoStandaloneFunction as ex:       # (standalone mode) a template instance / function the standalone inlines
            sym = SA_SYMS.get(k)
            if sym and sym in sa.parse_map(sa.SA_MAP)[0]:
                Ao[k] = sa.parse_map(sa.SA_MAP)[0][sym]
            else:
                Ao[k] = None
                nosa[k] = ex
    total = 0
    for f in which:
        cases = [rand_case(r, f) for _ in range(n)]      # (drawn also for a skipped function: the other functions get the same cases as before)
        if Ao[f] is None:
            sa.skip(f, nosa[f])
            continue
        total += compare(f, es, eo, run, cases, As, Ao)
    print('TOTAL problems', total)


if __name__ == '__main__':
    main()
