# Differential tests of the messenger windows (wave C, lane proof2): the stock function against ours under unicorn, with every call
# that leaves the function trapped and logged by tr.py (target, this, arguments) and every registered object compared at the end.
#   python t_msn1.py [cases] [seed] [test,test,...]      (SHOWLOG=stock|ours prints the call log of the first case)
# The objects are fake windows made with the stock class sizes and vtables of analysis/classes.json; the layouts below are the C4 ones
# (read off the stock asm: NCWnd 0xE8, Entry +0x74, Icon +0x3C).
import sys, os, random, struct
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import tr
from tr import *
from unicorn import UC_HOOK_CODE

W = dict(ScreenX=0x44, ScreenY=0x48, Width=0x4C, Height=0x50, Entry=0x74, Icon=0x3C, Style=0x68)
SYSSTR = 0x9871C
SYSMSG = 0x722C8
LM = None


ALT = {}      # stock va -> the standalone address the test under way compares it with (opts['alt'])


def fn_addr(e, sva):
    return sva if e.kind == 'stock' else ALT.get(sva) or LM[sva]


def add_child(e, ecx, a):
    ent = e.raw_obj('entry:%s' % e.nm(a[0]), 0x40)
    par = e.raw_obj('pentry', 0x40)
    pw = e.raw_obj('parentwnd', 0x40)
    e.write32(ent + 0xC, par)
    e.write32(par + 4, pw)
    return ent


def get_status(e, ecx, a):
    e.fs_set(a[0], e.presence)
    return a[0]


def display_name(e, ecx, a):
    """NMsnFriend::GetDisplayName(out): the nickname (or the second name) of the record in `out`, which it returns (ours returns its own out
    pointer, so a stub that answered 0 made the caller destroy the stale FString of that stack slot a second time)"""
    e.fs_set(a[0], 'disp:' + (e.fs_get(ecx + 0x2C) or e.fs_get(ecx + 0x38) or ''))
    return a[0]


def text_extent(e, ecx, a):
    text = e.wstr(a[1]) or ''
    e.write32(a[0], 6 * len(text))
    e.write32(a[0] + 4, 13)
    return 6 * len(text)


def char_size(e, ecx, a):
    e.write32(a[0], 6)
    e.write32(a[0] + 4, 13)
    return 0


class RW(TR):
    def __init__(self, kind):
        TR.__init__(self, kind)
        self.rules[0x1002b450] = lambda e, ecx, a: 0x9000 + len(e.wstr(a[0]) or '')      # NCObject::FindTexture(name, 1)
        self.rules[0x10035840] = add_child                                              # NCWnd::AddChildWindow(child, flags)
        self.rules[0x10150c40] = lambda e, ecx, a: ecx                                  # the protocol object's constructor
        self.rules[0x1014f1d0] = get_status                                             # Msn::GetStatus(out)
        self.rules[0x101510d0] = display_name                                           # NMsnFriend::GetDisplayName(out)
        self.rules[0x100345e0] = lambda e, ecx, a: e.shown.get(ecx, 0)                  # NCWnd::IsShowWindow
        self.rules[0x10012960] = text_extent                                            # NCGDevice::GetTextExtent
        self.rules[0x100134b0] = char_size                                              # NCGDevice::GetCharSize
        # helpers the stock code calls and ours inlines or hooks: they run for real on both sides
        self.transparent = {0x100021f0, 0x1000b120, 0x10003080, 0x100217c0, 0x10014b90}
        if self.ctors_real:
            self.transparent.add(0x10150c40)      # Msn::Construct (the protocol object's constructor, 319 bytes): ours builds the object inline, with its real vtable
        for i in range(0, 3000):
            self.fs_set(self.gdata + SYSSTR + 12 * i, 'S%d' % i)
        for i in range(0, 2000):
            self.fs_set(self.gdata + SYSMSG + 0x30 * i + 4, 'M%d' % i)
        self.presence = ''
        # mutation test of the harness itself: MUT=va:hexbytes patches the STOCK code (the differences must show)
        if os.environ.get('MUT') and kind == 'stock':
            va, hx = os.environ['MUT'].split(':')
            self.uc.mem_write(int(va, 16), bytes.fromhex(hx))

    def reset_case(self, c):
        self.ps = {}
        self.objs = {}
        self.reg_size = {}
        self.cls_objs = set()
        self.presence = c.get('presence', '')
        self.rand_n = 0

    def win(self, cls, label, w=256.0, h=353.0, sx=100, sy=50):
        p = self.new_obj(cls, label)
        self.w32(p, W['ScreenX'], sx)
        self.w32(p, W['ScreenY'], sy)
        self.wf(p, W['Width'], w)
        self.wf(p, W['Height'], h)
        ent = self.raw_obj('entry:' + label, 0x40)
        self.write32(p + W['Entry'], ent)
        return p

    def device(self):
        return self.raw_obj('Device', 0x100)


PRESENCE = ['NLN', 'BRB', 'IDL', 'AWY', 'LUN', 'BSY', 'PHN', 'FLN', 'HDN', '', 'XYZ']


def messenger(e, c, icon=True, msn=True):
    """a MessengerWnd the way OnCreate leaves it: the pages, the tabs, the icon"""
    w = e.win('MessengerWnd', 'MessengerWnd', c.get('w', 256.0), c.get('h', 400.0), c.get('sx', 100), c.get('sy', 50))
    mp = e.win('MsnWnd', 'MsnPage')
    e.w32(w, 0xE8, mp)
    e.w32(w, 0xEC, e.win('L2FriendWnd', 'FriendPage'))
    e.w32(w, 0xF0, e.win('NCTabControl', 'Tabs'))
    e.w32(w, 0xF4, 0x7777 if c.get('back', 1) else 0)
    if msn and c.get('msn', 1):
        m = e.raw_obj('Msn', 0x58)
        e.w32(mp, 0xE8, m)
    lg = e.win('LoginWnd', 'LoginWindow')
    e.w32(lg, 0xE8, e.win('NCEditBox', 'IdEdit'))
    e.w32(mp, 0x170, lg)
    if icon and c.get('icon', 1):
        ic = e.win('NCIcon', 'Icon', 32, 32, c.get('icx', 0), 0)
        e.w32(ic, 0x214, c.get('tip', 1))
        e.w32(w, W['Icon'], ic)
    return w


def gen_msn(r):
    return dict(w=r.choice([256.0, 276.5]), h=r.choice([400.0, 353.0]), sx=r.choice([100, 0, -30]), sy=r.choice([50, 0]),
                back=r.choice([0, 1]), msn=r.choice([0, 1, 1]), icon=r.choice([0, 1, 1]), tip=r.choice([0, 1, 5]),
                icx=r.choice([0, 20, -8]), presence=r.choice(PRESENCE))


def b_mw_oncreate(e, c):
    return e.new_obj('MessengerWnd', 'MessengerWnd'), []


def poison(e, cls, label, fill=0xCD):
    n = tr.classes()[cls]['size']
    p = e.alloc(n + 16)
    e.uc.mem_write(p, bytes([fill]) * (n + 16))
    e.objs[p] = label
    e.reg_size[p] = n
    e.cls_objs.add(p)
    return p


def b_mw_ctor(e, c):
    return poison(e, 'MessengerWnd', 'MessengerWnd'), []


def b_mw_dtor(e, c):
    p = poison(e, 'MessengerWnd', 'MessengerWnd')
    e.run_real(0x1015df10, 44, p)
    e.pre_len = len(e.log)
    return p, []


def b_mw_ondestroy(e, c):
    w = messenger(e, c)
    for off in (0xE8, 0xEC, 0xF0):
        if not c.get('kids', [1, 1, 1])[(off - 0xE8) // 4]:
            e.w32(w, off, 0)
    return w, []


def b_mw_onpaint(e, c):
    return messenger(e, c), [e.device()]


def b_mw_minimize(e, c):
    c['icon'] = 1
    return messenger(e, c, icon=True), []


def b_mw_show(e, c):
    return messenger(e, c), []


def b_mw_setfocus(e, c):
    w = messenger(e, c)
    e.shown[e.r32(e.r32(w, 0xE8), 0x170)] = c.get('loginshown', 0)
    return w, []


def b_mw_isicon(e, c):
    return messenger(e, c), []


def b_mw_seticon(e, c):
    return messenger(e, c), [e.put_wstr(c.get('name', 'icon.name'))]


def gen_kids(r):
    d = gen_msn(r)
    d['kids'] = [r.choice([0, 1, 1]) for _ in range(3)]
    d['loginshown'] = r.choice([0, 1])
    return d


TESTS = {
    # name: (stock va, size, builder, case generator)
    'mw_oncreate': (0x10160240, 776, b_mw_oncreate, lambda r: {}),
    'mw_ctor': (0x1015df10, 44, b_mw_ctor, lambda r: {}),
    'mw_dtor': (0x1015df70, 80, b_mw_dtor, lambda r: {}),
    'mw_ondestroy': (0x10158c90, 159, b_mw_ondestroy, gen_kids),
    'mw_onpaint': (0x10158d30, 614, b_mw_onpaint, gen_msn),
    'mw_minimize': (0x10158fa0, 509, b_mw_minimize, gen_msn, {'noret': 1}),
    'mw_show': (0x101591a0, 123, b_mw_show, gen_msn, {'noret': 1}),
    'mw_setfocus': (0x10159220, 58, b_mw_setfocus, gen_kids, {'noret': 1}),
    'mw_isicon': (0x10159260, 26, b_mw_isicon, gen_msn),
    'mw_seticon': (0x10156130, 48, b_mw_seticon, gen_msn, {'noret': 1}),
}


def b_msn_oncreate(e, c):
    w = e.win('MsnWnd', 'MsnWnd')
    desk = e.new_obj('NCWnd', 'Desktop')
    e.write32(e.console + 0x3BF4, desk)
    return w, []


TESTS['msn_oncreate'] = (0x1015e3b0, 4171, b_msn_oncreate, lambda r: {})


def run_real(self, sva, size, this, args=()):
    self.set_fn(fn_addr(self, sva), size)
    return self.call(fn_addr(self, sva), this=this, args=list(args))


TR.run_real = run_real


EMUS = {}


def print_cov(name, sva, cov):
    """how much of the stock function the cases ran (the instructions of Ghidra's listing that were executed)"""
    import re as _re
    asm = os.path.join(tr.ROOTP, 'analysis', 'ghidra', 'asm', '%08x.asm' % sva)
    if not os.path.exists(asm):
        return
    addrs = []
    for ln in open(asm, encoding='utf-8', errors='replace'):
        m = _re.match(r'^([0-9a-f]{8})\s+[0-9a-f]+\s+(\S+)', ln)
        if m:
            addrs.append((int(m.group(1), 16), m.group(2)))
    ran = [a for a, _ in addrs if a in cov]
    miss = [a for a, mn in addrs if a not in cov]
    pct = 100.0 * len(ran) / max(1, len(addrs))
    print('   coverage %s: %d/%d instructions (%.0f%%)%s' % (name, len(ran), len(addrs), pct,
          ('  not run: ' + ' '.join('%x' % a for a in miss[:14]) + (' ...' if len(miss) > 14 else '')) if miss else ''))


def run_tests(TESTS, RWclass, argv):
    global LM
    n = int(argv[1]) if len(argv) > 1 else 20
    seed = int(argv[2]) if len(argv) > 2 else 1
    which = argv[3].split(',') if len(argv) > 3 else list(TESTS)
    LM = log_map()
    total = 0
    for name in which:
        sva, size, build, gen = TESTS[name][:4]
        if size is None:
            size = tr.functions()[sva][0]
        opts = TESTS[name][4] if len(TESTS[name]) > 4 else {}
        # standalone mode: a stock function the standalone does not have as a function (inlined / template instance) is tested against the compiled symbol the test
        # names in opts['alt'] (same source function, another name or signature), else it is a SKIP line; opts['sa_only'] tests exist only in the standalone
        if opts.get('sa_only') and not sa.ENABLED:
            continue
        if sa.ENABLED and opts.get('alt_force'):           # compare with the named symbol even when the hook registration maps the stock function to another (e.g. the real constructor)
            ours_addr = sa.parse_map(sa.SA_MAP)[0].get(opts['alt'])
            if ours_addr is None:
                sa.skip(name, 'no standalone symbol %s' % opts['alt'])
        else:
            ours_addr = sa_pick(LM, sva, name, opts.get('alt')) if sa.ENABLED else LM[sva]
        if ours_addr is None:
            continue
        ALT[sva] = ours_addr
        if '::~' in tr.functions().get(sva, (0, '', ''))[1] and sa.ENABLED:
            # the standalone runs a destructor body through the scalar deleting destructor (flags 0), which returns `this`: a destructor's eax is not a result
            opts = dict(opts, noret=1)
        r = random.Random(seed)
        if RWclass not in EMUS:
            es, eo = RWclass('stock'), RWclass('ours')
            es.heap_mark, eo.heap_mark = es.heap_ptr, eo.heap_ptr
            EMUS[RWclass] = (es, eo)
        es, eo = EMUS[RWclass]
        cases = [gen(r) for _ in range(n)]

        def run(e, A, c, build=build, size=size, opts=opts):
            e.reset_case(c)
            e.shown = {}
            e.pre_len = None
            e.log.clear()
            e.freed = set()
            e.entry_sva = 0
            # the stack below the call starts zeroed: a by-value record whose first word the callee never writes (NMsnFriend copies) would otherwise carry the
            # residue of the previous test, which differs between the stock and ours (msn_openchat after msn_showmove: ChatWnd+0x964)
            e.uc.mem_write(STACK_TOP - 0x1000 - 0x4000, bytes(0x4000))
            this, args = build(e, c)
            e.set_fn(A[0], A[1])
            e.entry_sva = sva
            mark = e.pre_len if e.pre_len is not None else 0
            e.freed = set()
            ret = e.call(A[0], this=this, args=args)
            lg = e.log[mark:]
            ign = opts.get('ignore')
            if ign:
                lg = [x for x in lg if not (x[0] == 'call' and int(x[1], 16) in ign)]
            mk_ = opts.get('mask')
            if mk_:
                def masked(x):
                    if x[0] == 'call' and int(x[1], 16) in mk_:
                        args = list(x[4])
                        for ix in mk_[int(x[1], 16)]:
                            if ix < len(args):
                                args[ix] = '*'
                        return x[:4] + (args,)
                    return x
                lg = [masked(x) for x in lg]
            nt = opts.get('nothis')
            if nt:
                lg = [(x[:3] + ('-',) + x[4:]) if (x[0] == 'call' and int(x[1], 16) in nt) else x for x in lg]
            if opts.get('no_heap_log'):
                lg = [x for x in lg if not (isinstance(x[0], str) and x[0].startswith('GMalloc'))]
            if opts.get('ignore_alloc'):
                lg = [x for x in lg if x[0] != 'StaticAllocateObject']
            res = dict(log=lg, ret=(None if opts.get('noret') else ret), objs=e.dump_objs())
            om = opts.get('objmask')
            if om:                         # words of dead memory (a destroyed object's fields) that the stock and ours leave differently: not compared
                for lab, idxs in om.items():
                    w = res['objs'].get(lab)
                    if w:
                        for ix in idxs:
                            if ix < len(w):
                                w[ix] = '-'
            if opts.get('post'):
                res['post'] = opts['post'](e, this, args)
            if os.environ.get('SHOWLOG') and e.kind == os.environ.get('SHOWLOG') and not getattr(e, 'shown_once', False):
                e.shown_once = True
                print('---- %s log (%d entries) ret=%s' % (e.kind, len(res['log']), ret))
                for ent in res['log']:
                    print('   ', ascii(ent)[:220])
            return res
        es.cov = set()
        if not getattr(es, 'cov_hooked', False):
            es.cov_hooked = True

            def covhook(uc, address, sz, user, es=es):
                es.cov.add(address)
            es.uc.hook_add(UC_HOOK_CODE, covhook, None, 0x10000000, 0x10170000)
        total += compare(name, es, eo, run, cases, (sva, size), (ours_addr, size))
        print_cov(name, sva, es.cov)
    print('TOTAL problems', total)
    return total


def main():
    run_tests(TESTS, RW, sys.argv)


if __name__ == '__main__':
    main()
