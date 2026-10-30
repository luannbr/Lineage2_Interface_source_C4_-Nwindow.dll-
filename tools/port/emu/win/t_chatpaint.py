# Differential tests of NCChatWnd::OnPaint and NCChatWnd::ShowWindow (stock vs ours under unicorn)
#   python t_chatpaint.py [cases] [seed] [paint,show]
import sys, os, random, struct
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from gw import *
import gw
import stubs
from unicorn.x86_const import *

FUNCS = {'paint': (0x100544a0, 1), 'show': (0x10053be0, 0)}
CALLEES = {0x1002e4a0: ('IsSplit', 4, 0), 0x10035650: ('GetLocalRect', 4, 0), 0x1002b450: ('FindTexture', 8, 0),
           0x10001530: ('SetNormalTex', 4, 0), 0x10001570: ('SetPushedTex', 4, 0), 0x10159280: ('GetPeerName', 4, 0),
           0x100345a0: ('NCWndShowWindow', 0, 0)}
OURS_SYMS = {0x1002e4a0: None, 0x10035650: '?GetLocalRect@NCWnd@@QAEPAUNRectF@@PAU2@@Z', 0x1002b450: '?FindTexture@NCObject@@QAEPAVUTexture@@PBGH@Z',
             0x10001530: '?SetNormalTex@NCButton@@QAEHPBG@Z', 0x10001570: '?SetPushedTex@NCButton@@QAEHPBG@Z',
             0x10159280: None, 0x100345a0: '?ShowWindow@NCWnd@@UAEXXZ'}
gw.VPOP.update({41: 0, 48: 0, 49: 0, 40: 28})
CHAT_OFFS = (0x18C, 0x190, 0x194, 0x198, 0x19C, 0x1A0)
TEXOFFS = (0xE8, 0xEC, 0xF0, 0xF4, 0xF8, 0xFC, 0x100, 0x104, 0x108, 0x10C, 0x110, 0x114, 0x118, 0x11C, 0x120, 0x124, 0x128, 0x12C, 0x130)
STATUS = ['NLN', 'BRB', 'IDL', 'AWY', 'LUN', 'BSY', 'PHN', 'FLN', 'HDN', '', 'XYZ', 'nln']


class PW(GW):
    def __init__(self, kind, test):
        callees = dict(CALLEES)
        syms = {k: v for k, v in OURS_SYMS.items() if v}
        if test == 'paint':
            callees.pop(0x100345a0)
        GW.__init__(self, kind, callees, syms)
        self.case = None
        self.rets['IsSplit'] = lambda e, t, a: e.case['split'].get(e.objs.get(a[0], a[0]), 0)
        self.rets['FindTexture'] = lambda e, t, a: 0x9000 + len(e.wstr(a[0]) or '')
        self.rets['GetPeerName'] = self.peer
        self.rets['GetLocalRect'] = self.rect

    def rect(self, e, t, a):
        for i, v in enumerate((0, 0, f32(100.0), f32(e.case['recth']))):
            e.write32(a[0] + 4 * i, v)
        return 0

    def peer(self, e, t, a):
        e.fs_set(a[0], e.case['status'])
        return a[0]


def world(e, c):
    rnd = random.Random(c['seed'])
    e.case = c
    e.objs = {}
    e.shown = {}
    e.set_lang(c['lang'])
    e.ime_type = c['ime']
    chat = e.window('chat', size=0x700, rnd=rnd)
    for k, off in enumerate(TEXOFFS):
        e.write32(chat + off, 0 if c['texnull'][k] else 0x6000 + 0x100 * k)
    e.write32(chat + 0x44, c['sx']); e.write32(chat + 0x48, c['sy'])
    e.write32(chat + 0x4C, f32(348.0)); e.write32(chat + 0x50, f32(c['height']))
    edit = e.window('edit', size=0x300, rnd=rnd)
    e.write32(edit + 0x240, c['f1f0'])
    mgr = e.window('mgr', size=0x300, rnd=rnd) if c['hasmgr'] else 0
    if mgr:
        ime = e.alloc(0x6300)
        e.uc.mem_write(ime, bytes(0x6300))
        e.objs[ime] = 'ime'
        comp = e.window('compbox', size=0x300, rnd=rnd) if c['hascomp'] else 0
        e.write32(ime + 0x425C, comp)
        e.write32(mgr + 0x200, ime if c['hasime'] else 0)
    e.write32(edit + 0x1F4, mgr)
    e.write32(chat + 0x134, edit if c['hasedit'] else 0)
    e.write32(chat + 0x1AC, e.window('msnbtn', size=0x300, rnd=rnd))
    tab = e.window('tab', size=0x300, rnd=rnd)
    e.write32(chat + 0x1A4, tab)
    for k, off in enumerate(CHAT_OFFS):
        lst = e.window('list%d' % k, size=0x300, rnd=rnd)
        e.write32(chat + off, lst)
        e.shown[lst] = c['shown'][k]
    e.shown[e.read32(chat + 0x1B0)] = 0
    flt = e.window('filter', size=0x300, rnd=rnd)
    e.write32(chat + 0x1B0, flt)
    e.shown[flt] = c['shown'][0]
    e.write32(chat + 0x1B4, 0)
    # the desktop and the messenger window
    con = e.console
    e.uc.mem_write(con, bytes(0x4000))
    e.objs[con] = 'console'
    desk = e.window('desktop', size=0x300, rnd=rnd)
    e.write32(con + 0x3540, desk if c['hasdesk'] else 0)
    e.write32(desk + 0x1B8, e.window('messenger', size=0x300, rnd=rnd) if c['hasmsn'] else 0)
    # GL2SystemMsgWnd is an engine import (a pointer variable)
    for nm, v in e.gvars.items():
        if 'GL2SystemMsgWnd' in nm:
            e.write32(v, c['sysmsgwnd'])
    e.chat = chat
    return chat


def run(e, A, c):
    mark = len(e.log)
    chat = world(e, c)
    va, nargs = A[c['f']]
    dev = 0
    args = []
    if c['f'] == 'paint':
        dev = e.window('dev', size=0x100)
        args = [dev]
    e.call(va, this=chat, args=args)
    ret = e.reg(UC_X86_REG_EAX) if c['f'] == 'paint' else 0
    devmem = e.snapshot(dev, 0x100) if dev else None
    return dict(log=e.log[mark:], ret=ret, chat=e.snapshot(chat, 0x400), dev=devmem,
                lists=[e.snapshot(e.read32(chat + off), 0x100) for off in CHAT_OFFS],
                btn=e.snapshot(e.read32(chat + 0x1AC), 0x100))


def rand(r, f):
    c = dict(f=f, seed=r.randint(0, 10 ** 6), lang=r.choice([0, 1, 2, 3, 4, 4, 5]),
             ime=r.choice([-1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 8, 9, 10, 11, 12, 13]), height=r.choice([187.0, 190.0, 250.5, 300.0, 400.0, 502.0]),
             texnull=[r.random() < 0.15 for _ in TEXOFFS], sx=r.randint(0, 900), sy=r.randint(0, 700), f1f0=r.randint(0, 1),
             hasmgr=r.random() < 0.8, hascomp=r.random() < 0.8, hasime=r.random() < 0.8, hasedit=r.random() < 0.9,
             hasdesk=r.random() < 0.8, hasmsn=r.random() < 0.8, status=r.choice(STATUS), recth=r.choice([15.0, 15.5, 40.0]),
             shown=[r.randint(0, 1) for _ in range(6)], sysmsgwnd=r.choice([0, 0x4444]))
    c['split'] = {}
    for k in range(6):
        c['split']['list%d' % k] = r.randint(0, 1)
    return c


if __name__ == '__main__':
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 20
    seed = int(sys.argv[2]) if len(sys.argv) > 2 else 1
    which = sys.argv[3].split(',') if len(sys.argv) > 3 else list(FUNCS)
    lm = log_map()
    r = random.Random(seed)
    total = 0
    for f in which:
        es = PW('stock', f); eo = PW('ours', f)
        es.heap_mark, eo.heap_mark = es.heap_ptr, eo.heap_ptr
        As = {k: (v[0], v[1]) for k, v in FUNCS.items()}
        Ao = {k: (lm[v[0]], v[1]) for k, v in FUNCS.items()}
        cases = [rand(r, f) for _ in range(n)]
        total += compare(f, es, eo, run, cases, As, Ao)
    print('TOTAL problems', total)
