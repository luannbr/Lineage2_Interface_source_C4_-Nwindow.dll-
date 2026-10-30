# Differential tests of the NCChatFilterWnd functions (stock vs ours under unicorn)
#   python t_filter.py [cases] [seed] [setfilter,load,system,chat,ok,cancel,wproc,destroy,paint]
import sys, os, random, struct
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from gw import *
import gw
import stubs
from unicorn.x86_const import *

FUNCS = {'setfilter': (0x10055110, 13), 'load': (0x10055870, 0), 'system': (0x100553b0, 0), 'chat': (0x10055400, 0),
         'ok': (0x10055540, 0), 'cancel': (0x10053e30, 0), 'wproc': (0x10053d80, 3), 'destroy': (0x10054f10, 0),
         'paint': (0x10055020, 1)}
CALLEES = {0x10034390: ('NCWndWindowProc', 12, 0), 0x10035880: ('RemoveChild', 4, 0), 0x10036160: ('NCWndOnDestroy', 0, 0)}
OURS_SYMS = {0x10034390: '?WindowProc@NCWnd@@UAEHIIJ@Z', 0x10035880: '?RemoveChildWindow@NCWnd@@QAEXPAUNWndEntry@@@Z',
             0x10036160: '?OnDestroy@NCWnd@@UAEHXZ'}
gw.VPOP.update({3: 4, 49: 0, 74: 0, 75: 0, 76: 0, 77: 0, 62: 4})
CHAT_OFFS = (0x18C, 0x190, 0x194, 0x198, 0x19C)
SECTIONS = ['entire_tab', 'pledge_tab', 'party_tab', 'market_tab', 'ally_tab']
KEYS = ['system', 'chat', 'normal', 'shout', 'pledge', 'party', 'market', 'tell', 'damage', 'ally', 'useitems', 'hero']


class FW(GW):
    def __init__(self, kind):
        GW.__init__(self, kind, CALLEES, OURS_SYMS)
        self.case = None
        self.vrets[74] = lambda e, t, a: e.case['vret'][0]
        self.vrets[75] = lambda e, t, a: e.case['vret'][1]
        self.vrets[76] = lambda e, t, a: e.case['vret'][2]
        self.vrets[77] = lambda e, t, a: e.case['vret'][3]
        self.rets['NCWndWindowProc'] = lambda e, t, a: e.case['basewp']
        # GConfig: an object with GetBool (slot 0) and SetBool (slot 9)
        self.cfg_vt = self.alloc(0x100)
        self.uc.mem_write(self.cfg_vt, bytes(0x100))
        self.cfg_obj = self.alloc(16)
        self.write32(self.cfg_obj, self.cfg_vt)
        self.cfg_ptr = self.alloc(4)
        self.write32(self.cfg_ptr, self.cfg_obj)
        for slot, nm in ((0, 'GetBool'), (9, 'SetBool')):
            stub = STUB_BASE + 0xD800 + 16 * slot
            self.uc.mem_write(stub, bytes([0xC3]))
            self.stubs[stub] = ('cfg:' + nm, None)
            self.write32(self.cfg_vt + 4 * slot, stub)
        for ee in self.pe.DIRECTORY_ENTRY_IMPORT:
            for imp in ee.imports:
                nm = imp.name.decode() if imp.name else ''
                if nm == '?GConfig@@3PAVFConfigCache@@A':
                    self.write32(imp.address, self.cfg_ptr)

    def dispatch(self, name):
        kind, nm = name.split(':', 1)
        if kind == 'cfg':
            c = self.case
            a = self.sargs(4)
            sec, key = self.wstr(a[0]), self.wstr(a[1])
            if nm == 'GetBool':
                self.last_pop = 16
                found = c['ini'].get((sec, key))
                self.log.append(('GetBool', sec, key, self.wstr(a[3])))
                if found is None:
                    return 0
                self.write32(a[2], found)
                return 1
            self.last_pop = 16
            self.log.append(('SetBool', sec, key, a[2], self.wstr(a[3])))
            return 0
        return GW.dispatch(self, name)


def world(e, c):
    rnd = random.Random(c['seed'])
    e.case = c
    e.objs = {}
    e.shown = {}
    fw = e.window('fw', size=0x130, rnd=rnd)
    for i in range(13):
        if c['null'].get(i):
            e.write32(fw + 0xE8 + 4 * i, 0)
            continue
        cb = e.window('cb%d' % i, size=0x400, rnd=rnd)
        e.write32(cb + 0x294, c['checked'][i])
        e.write32(cb + 0x298, c['disabled'][i])
        e.write32(fw + 0xE8 + 4 * i, cb)
    e.write32(fw + 0x11C, e.window('ok', size=0x300, rnd=rnd))
    e.write32(fw + 0x120, e.window('cancel', size=0x300, rnd=rnd))
    e.write32(fw + 0x124, 0x7000)
    e.write32(fw + 0x4C, f32(256.0)); e.write32(fw + 0x50, f32(191.0))
    e.write32(fw + 0x44, 10); e.write32(fw + 0x48, 20)
    chat = e.window('chat', size=0x700, rnd=rnd)
    e.write32(chat + 0x1D0, c['curtab'])
    e.write32(chat + 0x1B4, c['bfilter'])
    for k, off in enumerate(CHAT_OFFS):
        lst = e.window('list%d' % k, size=0x300, rnd=rnd)
        for j in range(13):
            e.write32(lst + 0x244 + 4 * j, c['listfilter'][k][j])
        e.write32(chat + off, lst)
    pent = e.alloc(0x40)
    e.uc.mem_write(pent, bytes(0x40))
    e.write32(pent + 4, chat)
    ent = e.read32(fw + 0x74)
    e.write32(ent + 0xC, pent)
    e.objs[ent] = 'entry:fw'
    e.objs[pent] = 'entry:chat'
    e.fw, e.chat = fw, chat
    return fw


def run(e, A, c):
    mark = len(e.log)
    fw = world(e, c)
    va, nargs = A[c['f']]
    args = []
    dev = 0
    if c['f'] == 'setfilter':
        args = list(c['filter'])
    elif c['f'] == 'wproc':
        args = [c['msg'], c['wparam'], 0]
    elif c['f'] == 'paint':
        dev = e.window('dev', size=0x100)
        args = [dev]
    e.call(va, this=fw, args=args)
    devmem = e.snapshot(dev, 0x100) if c['f'] == 'paint' else None
    ret = e.reg(UC_X86_REG_EAX) if c['f'] in ('wproc', 'ok', 'cancel', 'system', 'chat', 'destroy', 'paint') else 0
    snap = e.snapshot(fw, 0x130)
    boxes = []
    for i in range(13):
        b = e.read32(fw + 0xE8 + 4 * i)
        boxes.append(e.snapshot(b, 0x2A0) if b else None)
    lists = [e.snapshot(e.read32(e.chat + off), 0x300) for off in CHAT_OFFS]
    chatmem = e.snapshot(e.chat, 0x200)
    return dict(log=e.log[mark:], ret=ret, fw=snap, boxes=boxes, lists=lists, chat=chatmem, dev=devmem)


def rand(r, f):
    c = dict(f=f, seed=r.randint(0, 10 ** 6), curtab=r.randint(0, 4), bfilter=r.randint(0, 1),
             checked=[r.randint(0, 1) for _ in range(13)], disabled=[r.randint(0, 1) for _ in range(13)],
             null={}, vret=[r.choice([0, 1, 5]) for _ in range(4)], basewp=r.choice([0, 0, 1, 7]),
             msg=r.choice([0x501, 0x501, 0x501, 0x500, 0x100]),
             wparam=r.choice([0xF0FF1458, 0xF0FF1459, 0xF0F01450, 0xF0F01452, 0xF0F01451, 5]),
             filter=[r.randint(0, 1) for _ in range(13)],
             listfilter=[[r.randint(0, 1) for _ in range(13)] for _ in range(5)])
    ini = {}
    for sec in SECTIONS:
        for key in KEYS:
            if r.random() < 0.6:
                ini[(sec, key)] = r.randint(0, 1)
    c['ini'] = ini
    return c


if __name__ == '__main__':
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 20
    seed = int(sys.argv[2]) if len(sys.argv) > 2 else 1
    which = sys.argv[3].split(',') if len(sys.argv) > 3 else list(FUNCS)
    es = FW('stock'); eo = FW('ours')
    lm = log_map()
    es.heap_mark, eo.heap_mark = es.heap_ptr, eo.heap_ptr
    As = {k: (v[0], v[1]) for k, v in FUNCS.items()}
    Ao = {k: (lm[v[0]], v[1]) for k, v in FUNCS.items()}
    r = random.Random(seed)
    total = 0
    for f in which:
        cases = [rand(r, f) for _ in range(n)]
        total += compare(f, es, eo, run, cases, As, Ao)
    print('TOTAL problems', total)
