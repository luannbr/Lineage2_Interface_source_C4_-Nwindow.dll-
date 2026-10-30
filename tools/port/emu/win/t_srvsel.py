# Differential test of the server select window of the login screen (NCServerSelectWnd, its list control and the help page; the
# "ServerID keying" of wave B): stock against ours under unicorn, on the fake world of pw.py. The list control is a fake object
# with the stock layout (Rows at +0x218, Selected at +0x250), the rows are built with the stock layout (0x890 bytes, cells of
# 0x1A64), the list's own methods, the cells' setters, the console and the network handler are logging services.
#   python t_srvsel.py [cases] [seed] [function,...]
import sys, os, random, struct
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from pw import *
import pw

FN = {
    'listwp': 0x100ffd10, 'wp': 0x100ffd70, 'format': 0x100ffe30, 'listend': 0x100fffb0, 'clear': 0x10100020, 'notify': 0x10100050,
    'keydown': 0x10100060, 'timer': 0x10100090, 'leap': 0x101000b0, 'dyears': 0x101000f0, 'dmonths': 0x10100130, 'hours': 0x101001a0,
    'stamp': 0x101001d0, 'cacheold': 0x10100230, 'noping': 0x10100280, 'saveping': 0x101002e0, 'savetime': 0x10100330,
    'destroy': 0x101003a0, 'paint': 0x101004d0, 'add': 0x101005d0, 'setping': 0x10100b10, 'ok': 0x10100bf0, 'cancel': 0x10100d80,
    'quality': 0x10100df0, 'infopaint': 0x101011f0, 'infodestroy': 0x101012f0, 'sortcol': 0x101013a0, 'infocreate': 0x101015d0,
    'lctor': 0x101016d0, 'ldtor': 0x10101720, 'lctor2': 0x10101770, 'ctor': 0x101017a0, 'dtor': 0x10101810, 'create': 0x10101860,
    'ictor': 0x1009dfe0, 'idtor': 0x1009e030,
}
VOID = ('format', 'listend', 'clear', 'saveping', 'savetime', 'add', 'setping', 'ictor', 'idtor', 'dtor', 'ldtor')
STATIC = ('leap', 'dyears', 'dmonths', 'hours', 'stamp')     # no `this`
NARGS = {'listwp': 3, 'wp': 3, 'format': 5, 'notify': 1, 'keydown': 2, 'timer': 1, 'leap': 1, 'dmonths': 2, 'hours': 2, 'saveping': 2,
         'noping': 1, 'dyears': 1, 'setping': 2, 'infopaint': 1, 'paint': 1, 'sortcol': 1, 'lctor2': 4, 'add': 1}
CONF = {'stamp': dict(skip={0x101000f0, 0x10100130, 0x101001a0, 0x101000b0}),
        'dyears': dict(skip={0x101000b0}),         # IsLeapYear runs for real on both sides (a stub would never say "leap")
        'sortcol': dict(extra={0x10171439: ('wtol', 0, 0), 0x10171e4e: ('wcsicmp', 0, 0)}, syms={0x10171e4e: '__wcsicmp'})}
CELL = 0x1A64
ROW = 0x890
# stock callee pops that the db cannot tell (tail jumps)
pw.POP_OVERRIDE.update({0x10074e60: 4,      # NConsoleWnd::PlayInterfaceSound -> PlaySoundName
                        0x1002b420: 12,     # NCObject::SetTimer (id, ms, callback)
                        })
ADDS = {0x10171439: ('wtol', 0, 0)}
SERVERNAMES = 0x10273C34       # the table of names of NCsoft's servers (stock VA of the pointer array)


class SW(PW):
    def __init__(self, kind, funcs, skip=(), extra=None, syms=None):
        PW.__init__(self, kind, funcs, extra or {}, syms, skip)
        self.vt_net = self.make_vt('net', {181: 0, 263: 12, 264: 0}, nslots=270)
        self.vt_cfg = self.make_vt('cfg', CFG_POPS, {1: self.cfg_getint, 5: self.cfg_getstr, 10: self.cfg_setint, 12: self.cfg_set})
        self.vt_sel = self.class_vt('NCServerSelectWnd')
        self.vt_list = self.class_vt('NCServerSelectListCtrl')
        self.vt_login = self.class_vt('NCLoginWnd')
        self.vt_wnd = self.class_vt('NCWnd')
        self.engine = self.obj('engine', 0x100)
        self.net = self.obj('net', 0x100, self.vt_net)
        self.cfgobj = self.obj('cfg', 0x40, self.vt_cfg)
        self.god = self.obj('god', 0x300)
        self.login = self.wnd('login', self.vt_login, 0x300)
        self.write32(self.console + 0x54, self.engine)
        self.write32(self.engine + 0x64, self.net)
        self.write32(self.console + 0x3BE0, self.god)
        self.write32(self.god + 0xEC, self.login)
        self.write32(self.gvar('GConfig'), self.cfgobj)
        self.rets['NCListCtrl::GetRowCount'] = lambda e, t, a: e.read32(t + 0x21C)
        self.rets['NCListCtrl::SetSelected'] = self.l_setsel
        self.rets['NCListCtrl::GetSelectedRow'] = self.l_selrow
        self.rets['NListRow::NListRow'] = self.row_ctor
        self.rets['NCWnd::WindowProc'] = lambda e, t, a: e.case['basewp']
        self.rets['NCListCtrl::WindowProc'] = lambda e, t, a: e.case['basewp']
        self.rets['NCObject::FindTexture'] = lambda e, t, a: 0x9000 + sum(ord(ch) for ch in (e.wstr(a[0]) or '')) + a[1]
        self.rets['NCServerSelectWnd::HasNoPing'] = lambda e, t, a: e.case['noping'].get(a[0], 1)
        self.rets['NCServerSelectWnd::GetTimeStamp'] = lambda e, t, a: e.case['stamp']
        self.rets['NCServerSelectWnd::IsPingCacheOld'] = lambda e, t, a: e.case['cacheold']
        self.rets['NCWnd::AddChildWindow'] = self.add_child
        self.rets['NCButton::NCButton'] = lambda e, t, a: (e.write32(t, e.vt_wnd), t)[1]
        self.rets['NCServerSelectListCtrl::NCServerSelectListCtrl'] = lambda e, t, a: (e.write32(t, e.vt_list), t)[1]
        self.rets['NCHtmlViewer::NCHtmlViewer'] = lambda e, t, a: (e.write32(t, e.vt_wnd), t)[1]
        self.rets['NCObject::SetTimer'] = lambda e, t, a: 77
        self.rets['NCListCtrl::AddColumn'] = lambda e, t, a: 1
        self.rets['NConsoleWnd::PlayInterfaceSound'] = lambda e, t, a: 0
        self.cdecl_n['wtol'] = 1
        self.cdecl_n['wcsicmp'] = 2
        self.nolog.add('wtol')
        self.nolog.add('wcsicmp')
        self.rets['wtol'] = self.crt_wtol
        self.rets['wcsicmp'] = self.crt_wcsicmp
        self.nnew = 0
        self.created = []
        # the table of NCsoft's server names (0x10273C34): strings in the heap, the same in both emulators
        base = self.glob(SERVERNAMES)
        for i in range(41):
            self.write32(base + 4 * i, self.put_wstr('ncserver%d' % i))
        self.case = None

    def rets_cell(self):
        pass

    def crt_wtol(self, e, t, a):
        import re as _re
        m = _re.match("[ " + chr(9) + chr(10) + chr(13) + "]*([+-]?[0-9]+)", e.wstr(a[0]) or "")
        return (int(m.group(1)) & 0xFFFFFFFF) if m else 0

    def crt_wcsicmp(self, e, t, a):
        x = ''.join(chr(ord(ch) + 32) if 'A' <= ch <= 'Z' else ch for ch in e.wstr(a[0]))
        y = ''.join(chr(ord(ch) + 32) if 'A' <= ch <= 'Z' else ch for ch in e.wstr(a[1]))
        return 0 if x == y else (1 if x > y else 0xFFFFFFFF)

    # ---- services ---------------------------------------------------------------------------------------------------------
    def cfg_getint(self, e, t, a):
        key = (e.wstr(a[0]), e.wstr(a[1]))
        v = e.case['ini'].get(key)
        if v is None:
            return 0
        e.write32(a[2], v & 0xFFFFFFFF)
        return 1

    def cfg_getstr(self, e, t, a):
        key = (e.wstr(a[0]), e.wstr(a[1]))
        v = e.case['ini'].get(key)
        return e.put_wstr(v or '')

    def cfg_setint(self, e, t, a):
        return None

    def cfg_set(self, e, t, a):
        return None

    def l_setsel(self, e, t, a):
        e.write32(t + 0x250, a[0] & 0xFFFFFFFF)
        return 0

    def l_selrow(self, e, t, a):
        sel = e.rds32(t + 0x250)
        n = e.read32(t + 0x21C)
        if 0 <= sel < n:
            return e.read32(e.read32(t + 0x218) + 4 * sel)
        return 0

    def row_ctor(self, e, t, a):
        e.uc.mem_write(t, bytes(ROW))
        cells = e.obj('cells', CELL * a[0] + 16)
        e.write32(t, cells)
        e.write32(t + 0x804, 0xFFFFFFFF)
        e.write32(t + 0x808, 0xFFFFFFFF)
        return t

    def add_child(self, e, t, a):
        ent = e.obj('entry:' + str(e.nm(a[0])), 0x40)
        e.write32(ent + 4, a[0])
        e.write32(ent + 0xC, e.read32(t + 0x74))
        return ent

    def do_import(self, nm):
        if nm == '?GetTransientPackage@UObject@@SAPAVUPackage@@XZ':
            return 0x7777
        if nm.startswith('?StaticAllocateObject@UObject@@'):
            p = self.obj('new%d' % self.nnew, 0x400)
            self.nnew += 1
            self.created.append(p)
            return p
        if nm == '?ConditionalDestroy@UObject@@QAEHXZ':
            self.log.append(('ConditionalDestroy', self.nm(self.this())))
            return 1
        return PW.do_import(self, nm)

    # ---- the world ---------------------------------------------------------------------------------------------------------
    def mk_row(self, k, d):
        row = self.obj('row%d' % k, ROW)
        cells = self.obj('cells%d' % k, CELL * 5 + 16)
        self.write32(row, cells)
        self.write_wstr_at(row + 4, d['port'])
        self.write32(row + 0x804, d['id'] & 0xFFFFFFFF)
        self.write32(row + 0x808, d['ip'] & 0xFFFFFFFF)
        self.write32(row + 0x80C, 0xFFFFFFFF)
        for i in range(5):
            self.write32(cells + CELL * i + 0x1A48, 0xFFFFFFFF)
        self.write_wstr_at(cells + CELL * 1, d['name'])
        self.write32(cells + CELL * 1 + 0x1A48, d['users'] & 0xFFFFFFFF)
        self.write32(cells + CELL * 2 + 0x1A48, d['flags'])
        self.write_wstr_at(cells + CELL * 3, d['ping'])
        self.write_wstr_at(cells + CELL * 3 + 0x1248, d['age'])
        return row

    def world(self, c):
        self.case = c
        self.nnew = 0
        self.created = []
        self.pstacks = {}
        gd = self.gdata
        # the names the stock reads (FL2GameData: records of 0x1C bytes with an FString at +4)
        for i in range(0, 48):
            self.fs_set(gd + (i + 0x5554) * 0x1C + 4, c['names'].get(i, 'gdname%d' % i))
        for i in range(0, 6):
            self.fs_set(gd + (i + 0x561D) * 0x1C + 4, c['types'].get(i, 'gdtype%d' % i))
        self.fs_set(gd + 0x96314 + 0, 'SPECIAL')
        self.fs_set(gd + 0x962F8 + 0, 'FLAG4')
        self.write32(self.langvar, c['lang'])
        for k in ('LoadingScreenID',):
            pass
        self.write32(self.console + 0x3C64, 0xAAAA)
        self.write32(self.console + 0x3C68, 0xBBBB)
        self.gd_extra()

    def gd_extra(self):
        pass

    def page(self, c, snaps):
        w = self.wnd('select', self.vt_sel, 0x140)
        lst = self.wnd('list', self.vt_list, 0x280)
        self.write32(w + 0xEC, lst)
        for k, off in enumerate((0xF0, 0xF4, 0xF8)):        # the OK, Cancel and quality buttons (OnDestroy removes and frees the ones it has)
            if (c['stamp'] >> (k + 2)) & 1:
                self.write32(w + off, self.wnd('btn%d' % k, self.vt_wnd, 0x140, parent=w))
        self.write32(w + 0xFC, c['lastid'] & 0xFFFFFFFF)
        self.write32(w + 0x100, c['lastrow'] & 0xFFFFFFFF)
        self.write32(w + 0x104, c['pending'])
        self.write32(w + 0x10C, c['requested'])
        self.write32(w + 0x110, c['cacheold'])
        self.write32(w + 0x114, c['saved'])
        self.write32(w + 0x118, 0xD1D1)
        for i in range(16):
            self.uc.mem_write(w + 0x11C + i, bytes([c['bits'][i]]))
        rows = [self.mk_row(k, d) for k, d in enumerate(c['rows'])]
        data = self.obj('rowsdata', 4 * max(len(rows), 1) + 16)
        for i, r in enumerate(rows):
            self.write32(data + 4 * i, r)
        self.write32(lst + 0x218, data)
        self.write32(lst + 0x21C, len(rows))
        self.write32(lst + 0x220, len(rows))
        self.write32(lst + 0x250, c['sel'] & 0xFFFFFFFF)
        cols = self.obj('colsdata', 4 * 5 + 16)
        rr = random.Random(c['stamp'])
        for i in range(5):
            h = self.obj('hdr%d' % i, 0x240)
            self.write32(h + 0x230, rr.randint(0, 1))
            self.write32(h + 0x234, rr.randint(0, 1))
            self.write32(cols + 4 * i, h)
            snaps['hdr%d' % i] = (h, 0x240)
        self.write32(lst + 0x224, cols)
        self.write32(lst + 0x228, 5)
        self.write32(lst + 0x22C, 5)
        snaps['this'] = (w, 0x140)
        snaps['list'] = (lst, 0x280)
        snaps['rowsdata'] = (data, 4 * max(len(rows), 1) + 16)
        for k, r in enumerate(rows):
            snaps['row%d' % k] = (r, ROW)
            snaps['cells%d' % k] = (self.read32(r), CELL * 5)
        snaps['console'] = (self.console, 0x3D00)
        return w, lst, rows


def rand_row(r, k):
    ping = r.choice(['0', '5', '9', '10', '11', '49', '50', '51', '120', '9999', '9998', '', '7'])
    return dict(id=r.choice([k + 1, k + 1, r.randint(1, 40), r.randint(1, 200)]), ip=r.choice([0x0100007F, 0x0200007F, 0xC0A80001, 77]),
                port=r.choice(['7777', '2106', '7778']), ping=ping, age=r.choice(['', 'S462', 'S461', 'Teen', 'S539']),
                users=r.choice([0, 1, 5, 100, 3700, 5000, 0x7FFFFFFF]), flags=r.choice([0, 0, 1, 2, 4, 8, 10, 12]),
                name=r.choice(['alpha', 'Beta', 'gamma', '', 'delta']))


def rand_case(r):
    n = r.choice([0, 1, 2, 3, 5, 8, 12])
    rows = [rand_row(r, k) for k in range(n)]
    lastid = r.choice([0, 1, 2, 3, 5, 9, 13, 100])
    lastrow = r.choice([-1, 0, 1, 2, 3, 5, 9, 13, 100, lastid])
    sysnames = {}
    c = dict(rows=rows, lastid=lastid, lastrow=lastrow, pending=r.choice([0, 1, 2, 3, 7]), requested=r.randint(0, 1),
             cacheold=r.randint(0, 1), saved=r.randint(0, 1), bits=[r.choice([0, 0, 255, 1, 2, 4, 128, 0x55]) for _ in range(16)],
             sel=r.choice([-1, 0, 1, 2, 4, 20]), lang=r.choice([0, 1, 1, 2, 5]), names={}, types={}, basewp=r.choice([0, 0, 1]),
             ini={('URL', 'ServerAddr'): r.choice(['L2Server', 'l2server.ncsoft.co.kr', 'L2SERVER', 'example.com', None])},
             noping={}, stamp=r.randint(0, 100000), time=[r.choice([2004, 2005, 2006, 2000, 1900, 2400]), r.randint(1, 12), r.randint(1, 31),
                                                          r.randint(0, 23), r.randint(0, 59)],
             # AddServer's ten values: id, ip, port, age, pvp, users, max users, state, flags, type
             params=[r.choice([1, 2, 7, 9, 15, 16, 39, 40, 41, 99, 127, 200, -1, -9]), r.choice([0x0100007F, 77]), r.choice([7777, 2106]),
                     r.choice([0, 15, 18, 12]), r.choice([0, 1, 2]), r.choice([0, 1, 100, 1500, 3700, 3699, 0x12345, 70000]),
                     r.choice([0, -1, 1, 100, 2000, 3000, 4000, 0x12345]), r.choice([0, 1, 1, 2]), r.choice([0, 1, 2, 4, 8, 10, 12, 3]),
                     r.choice([0, 0, 1, 2, 3, 5, -1])],
             ids=[r.choice([0, 1, 2, 3])], ping=r.choice([0, 5, 9999, 9998, 12, 70000, -1]), ip=r.choice([0x0100007F, 77, 0xC0A80001, 5]),
             msg=r.choice([0x501, 0x501, 0x501, 0x111]), wid=r.choice([0xF007, 0xF017, 0xF020, 0xF018, 0xF021, 0xF019, 0x1234]),
             lp=r.choice([0, 1, 5]), vk=r.choice([0x0D, 0x0D, 0x41, 0x1B]), timer=r.choice([0xF019, 0xF019, 5, 0]),
             year=r.choice([2000, 1900, 2004, 2400, 1, 4, 100, 400, 2100, -1, -4, -100, -400]), month=r.choice([1, 2, 3, 6, 7, 12, 13, 14]),
             day=r.choice([1, 2, 31]), hour=r.choice([0, 5, 23]), years=r.choice([0, 1, 4, 5, 100, 400, 2005]),
             idx=r.choice([0, 1, 5, 9, 10, 39, 40, 41, 100]), flags=r.choice([0, 0, 2, 4, 8, 12, 10]), typ=r.choice([0, 0, 1, 3, 5]),
             tpair=(r.choice([0, 1, 5, 9, 10, 15]), r.choice([0, 1, 2])))
    # ini: s_info.ini values the stock reads
    c['ini'][('Date', 'Time')] = r.choice([None, c['stamp'], c['stamp'] - 12, c['stamp'] - 13, c['stamp'] + 5])
    for srv in (1, 2, 3, 5, 7, 9, 15, 16, 39, 40, 41, 99, 127, 200):
        c['ini'][(str(srv), 'Time')] = r.choice([None, 0, 25, 9999, 120])
    for k in range(1, 6):
        c['noping'][k] = r.randint(0, 1)
    c['cacheold_call'] = r.randint(0, 1)
    return c


def run(e, A, c, name):
    c = dict(c)
    e.world(c)
    snaps = {}
    args = []
    this = None
    if name in ('lctor', 'lctor2', 'ctor', 'dtor', 'ldtor', 'ictor', 'idtor'):
        size = {'lctor': 0x270, 'lctor2': 0x270, 'ldtor': 0x270, 'ctor': 0x140, 'dtor': 0x140, 'ictor': 0xF0, 'idtor': 0xF0}[name]
        w = e.obj('o', size + 16)
        e.uc.mem_write(w, bytes([0xCD]) * (size + 16))
        snaps['this'] = (w, size + 16)
        if name == 'lctor2':
            args = [100, 15, 5, 12]
        r = e.go(A[name], this=w, args=args, snaps=snaps)
        r['eax'] = 'this' if r['eax'] == w else r['eax']
        return r
    if name in STATIC:
        args = {'leap': [c['year']], 'dyears': [c['years']], 'dmonths': [c['year'], c['month']], 'hours': [c['day'], c['hour']],
                'stamp': []}[name]
        r = e.go(A[name], this=None, args=args, snaps=snaps)
        return r
    w, lst, rows = e.page(c, snaps)
    if name == 'listwp':
        this = lst
        args = [c['msg'], c['wid'], c['lp']]
    elif name == 'wp':
        args = [c['msg'], c['wid'], c['lp']]
    elif name == 'format':
        nm = e.obj('nameBuf', 0x200)
        nt = e.obj('nameTextBuf', 0x80)
        snaps['nameBuf'] = (nm, 0x200)
        snaps['nameTextBuf'] = (nt, 0x80)
        args = [c['tpair'][0] if False else c['idx'], c['flags'], c['typ'], nm, nt]
    elif name == 'notify':
        args = [c['lp']]
    elif name == 'keydown':
        args = [c['vk'], 0]
    elif name == 'timer':
        args = [c['timer']]
    elif name == 'saveping':
        args = [c['idx'], c['ping'] & 0xFFFFFFFF]
    elif name == 'noping':
        args = [c['idx']]
    elif name == 'setping':
        args = [c['ip'], c['ping'] & 0xFFFFFFFF]
    elif name == 'add':
        ps = e.obj('params', 0x40)
        e.pstacks[ps] = list(c['params'])
        args = [ps]
    elif name in ('paint', 'infopaint'):
        dev = e.obj('dev', 0x100)
        snaps['dev'] = (dev, 0x100)
        e.write32(w + 0x44, 10)
        e.write32(w + 0x48, 20)
        e.write32(w + 0x4C, f32(300.0))
        e.write32(w + 0x50, f32(330.0))
        e.write32(w + 0xE8, c['tex'] if 'tex' in c else (0x5555 if c['sel'] & 1 else 0))
        args = [dev]
    elif name == 'sortcol':
        this = lst
        args = [(c['stamp'] >> 3) % 5]
    r = e.go(A[name], this=this or w, args=args, snaps=snaps)
    if name in VOID:
        del r['eax']
    return r


def main():
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 100
    seed = int(sys.argv[2]) if len(sys.argv) > 2 else 1
    which = sys.argv[3].split(',') if len(sys.argv) > 3 else list(FN)
    total = 0
    lm = log_map()
    for name in which:
        funcs = [FN[name]]
        cf = CONF.get(name, {})
        es = SW('stock', funcs, cf.get('skip', ()), cf.get('extra'), cf.get('syms'))
        eo = SW('ours', funcs, cf.get('skip', ()), cf.get('extra'), cf.get('syms'))
        es.heap_mark, eo.heap_mark = es.heap_ptr, eo.heap_ptr
        if name == 'create':
            eo.alias_extra('??0NCListCtrl@@QAE@HHHHHH@Z', 'NCServerSelectListCtrl::NCServerSelectListCtrl', 24, 4)
        es.track(FN[name])
        r = random.Random(seed)
        cases = [rand_case(r) for _ in range(n)]
        As = {name: FN[name]}
        Ao = {name: lm[FN[name]]}
        total += run_tests(name, es, eo, lambda e, A, c: run(e, A, c, name), cases, As, Ao)
        cov_report(es, FN[name])
    print('TOTAL problems', total)


if __name__ == '__main__':
    main()
