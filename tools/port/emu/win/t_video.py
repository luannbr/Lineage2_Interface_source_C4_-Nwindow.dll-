# Differential test of the video options page NCVideoOptionWnd (the biggest rewrite of wave B): stock against ours under unicorn.
# The fake world (pw.py): GConsole -> Engine -> {Client (gamma, viewports), GRenDev}, the GConfig object, the combo / check boxes as
# fake windows (their virtual calls are logged), the render device answers from the case. Every call of the page (the callees,
# the virtual calls with their arguments, the config writes), the globals it writes, the objects it changes and the stack balance
# are compared.
#   python t_video.py [cases] [seed] [winproc,postproc,initmodes,resolution,lowerdetail,load,items,apply,clear,paint,destroy,create,show]
import sys, os, random, struct
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from pw import *
import pw

ROOT = fncalls.ROOT
FN = {'winproc': 0x100c3f90, 'postproc': 0x100c4050, 'initmodes': 0x100c40a0, 'show': 0x100c4210, 'paint': 0x100c4b40,
      'destroy': 0x100c5190, 'clear': 0x100c5580, 'resolution': 0x100c5650, 'load': 0x100c57a0, 'apply': 0x100c5e70,
      'lowerdetail': 0x100c6d60, 'items': 0x100c90c0, 'create': 0x100c99e0, 'ctor': 0x100cb2d0, 'dtor': 0x100cb330, 'toggle': 0x100c6d00}
VOID = ('toggle', 'dtor', 'initmodes', 'clear', 'load', 'items', 'apply', 'show')
NARGS = {'winproc': 3, 'postproc': 1, 'resolution': 1, 'paint': 1}
GCONSOLE = e4.GCONSOLE
COMBOS = dict(Resolution=0xE8, Refresh=0xEC, Gamma=0xF0, Texture=0xF4, Modeling=0xF8, Animation=0xFC, PostProcess=0x108,
              Terrain=0x10C, Pawn=0x110, Reflection=0x114, ActorLimit=0x120, Antialias=0x124, ScreenShot=0x128)
BOXES = dict(Shadow=0x100, GroundDetail=0x104, LowerDetail=0x118, TextureFilter=0x11C)
WSIZE = 0x140


class VW(PW):
    def __init__(self, kind, funcs):
        extra = {}
        PW.__init__(self, kind, funcs, extra)
        inc = os.path.join(ROOT, 'include/ue2/gen/URenderDevice.vt.inc')
        rd_rets = {27: self.rd_setres, 28: self.rd_setres6, 29: self.rd_getres, 30: self.rd_getrate, 43: lambda e, t, a: e.case['trilinear'],
                   47: lambda e, t, a: e.caps, 49: self.rd_params, 50: self.rd_shaders, 51: lambda e, t, a: e.case['multisample'],
                   52: lambda e, t, a: e.case['texfmt']}
        self.vt_rd = self.make_vt('rendev', vt_inc_pops(inc), rd_rets)
        self.vt_cfg = self.make_vt('cfg', CFG_POPS, {2: self.cfg_getfloat})
        self.vt_vp = self.make_vt('viewport', {30: 0}, {30: lambda e, t, a: e.case['fullscreen']})
        self.vt_eng = self.make_vt('engine', {29: 0})
        self.vt_combo = self.class_vt('NCComboBox')
        self.vt_check = self.class_vt('NCCheckBox')
        self.vt_video = self.class_vt('NCVideoOptionWnd', rets={76: None})
        self.engine = self.obj('engine', 0x100, self.vt_eng)
        self.client = self.obj('client', 0x100)
        self.vparr = self.obj('vparr', 16)
        self.vp0 = self.obj('viewport', 0x100, self.vt_vp)
        self.rendev = self.obj('rendev', 0x100, self.vt_rd)
        self.caps = self.obj('caps', 16)
        self.cfgobj = self.obj('cfg', 0x40, self.vt_cfg)
        self.write32(self.console + 0x54, self.engine)
        self.write32(self.engine + 0x58, self.client)
        self.write32(self.engine + 0x60, self.rendev)
        self.write32(self.client + 0x38, self.vparr)
        self.write32(self.client + 0x3C, 1)
        self.write32(self.vparr, self.vp0)
        self.write32(self.gvar('GConfig'), self.cfgobj)
        self.case = None
        self.cdecl_n['NCGDevice::GetTextExtent'] = 8
        self.rets['NCGDevice::GetTextExtent'] = self.gd_extent
        self.rets['NCGDevice::DrawText'] = lambda e, t, a: 6 * len(e.wstr(a[3]) or '')
        self.rets['NCWnd::WindowProc'] = lambda e, t, a: e.case['basewp']
        self.rets['NCObject::FindTexture'] = lambda e, t, a: 0x9000 + sum(ord(ch) for ch in (e.wstr(a[0]) or '')) + a[1]
        self.rets['NCComboBox::NCComboBox'] = lambda e, t, a: (e.write32(t, e.vt_combo), t)[1]
        self.rets['NCCheckBox::NCCheckBox'] = lambda e, t, a: (e.write32(t, e.vt_check), t)[1]
        self.rets['NCWnd::AddChildWindow'] = self.add_child
        self.nnew = 0
        self.created = []
        self.options = self.wnd('options', self.vt_video, 0x300)

    def gd_extent(self, e, t, a):
        text = e.wstr(a[1]) or ''
        e.write32(a[0], 6 * len(text))
        e.write32(a[0] + 4, 13)
        return 0

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
        return PW.do_import(self, nm)

    # ---- the render device -------------------------------------------------------------------------------------------------
    def rd_getres(self, e, t, a):
        modes = e.case['modes']
        n = min(a[0], len(modes))
        for i in range(n):
            e.write32(a[1] + 4 * i, modes[i][0])
            e.write32(a[2] + 4 * i, modes[i][1])
            e.write32(a[3] + 4 * i, modes[i][2])
        return n

    def rd_getrate(self, e, t, a):
        rates = e.case['rates'].get((a[1], a[2], a[3]), e.case['defrates'])
        n = min(a[0], len(rates))
        for i in range(n):
            e.write32(a[4] + 4 * i, rates[i])
        return n

    def rd_setres(self, e, t, a):
        e.case['setres_calls'] = e.case.get('setres_calls', 0) + 1
        return e.case['setres'][min(e.case['setres_calls'] - 1, len(e.case['setres']) - 1)]

    def rd_setres6(self, e, t, a):
        return 1

    def rd_params(self, e, t, a):
        for i, v in enumerate(e.case['params']):
            e.write32(a[i], v)
        return 1

    def rd_shaders(self, e, t, a):
        e.write32(a[0], e.case['vs'])
        e.write32(a[1], e.case['ps'])
        return None

    def cfg_getfloat(self, e, t, a):
        key = e.wstr(a[1])
        v = e.case['fog'].get(key)
        if v is None:
            return 0
        e.write32(a[2], f32(v))
        return 1

    # ---- the page ----------------------------------------------------------------------------------------------------------
    def page(self, c):
        """this + its children from the case"""
        w = self.wnd('video', self.vt_video, 0x200)
        self.kids = {}
        for nm, off in COMBOS.items():
            k = self.wnd(nm + 'Combo', self.vt_combo, 0x300)
            self.write32(w + off, k)
            self.kids[nm] = k
            self.write32(k + 0x240, c['sel'][nm] & 0xFFFFFFFF)
        for nm, off in BOXES.items():
            k = self.wnd(nm + 'Box', self.vt_check, 0x300)
            self.write32(w + off, k)
            self.kids[nm] = k
            self.write32(k + 0x294, c['chk'][nm])
            self.write32(k + 0x298, c['dis'][nm])
        return w

    def world(self, c):
        """the engine side of the case"""
        self.case = c
        self.write32(self.client + 0x84, f32(c['gamma']))
        self.write32(self.caps + 4, c['pixel'])
        self.setg('GSEKScreenX', c['x'])
        self.setg('GSEKScreenY', c['y'])
        self.setg('GColorBits', c['bits'])
        self.setg('GRefreshRate', c['rate'])
        for k, v in c['g'].items():
            self.setg(k, v)
        for k, v in c['gf'].items():
            self.setg(k, v, True)
        self.nnew = 0
        self.created = []
        for i, t in enumerate(c.get('empty', [])):
            self.fs_set(self.gdata + SYSSTR_BASE + 12 * t, '')

    def snaps(self, w):
        s = {'this': (w, WSIZE)}
        for nm, k in self.kids.items():
            s[nm] = (k, 0x300)
        s['client'] = (self.client, 0x100)
        return s


def rand_modes(r):
    n = r.choice([0, 1, 3, 6, 12])
    out = []
    for i in range(n):
        out.append((r.choice([640, 800, 1024, 1280]), r.choice([480, 600, 768, 1024]), r.choice([16, 32])))
    return out


def rand_case(r):
    modes = rand_modes(r)
    rates = {}
    for m in modes:
        rates[m] = [r.choice([60, 75, 85, 100, 120]) for _ in range(r.choice([0, 1, 2, 4]))]
    if modes and r.random() < 0.8:
        m = r.choice(modes)
        x, y, bits = m
    else:
        x, y, bits = r.choice([640, 1024]), r.choice([480, 768]), r.choice([16, 32])
    rr = rates.get((x, y, bits)) or [60, 75]
    rate = r.choice(rr + [60])
    g = dict(GL2KeepMinFrameRate=r.randint(0, 1), GL2UseTrilinear=r.randint(0, 1), GL2TextureDetail=r.randint(-1, 3),
             GL2UseReducePawnLOD=r.randint(0, 2), GL2UseSkipAnim=r.randint(0, 2), GL2RenderDeco=r.randint(0, 1),
             GL2PostProcessType=r.randint(-1, 7), GIsL2Projector=r.randint(0, 1), GIsL2WaterEffect=r.randint(0, 1),
             GIsL2WaterEffectType=r.randint(0, 3), GL2RenderActorLimitOpt=r.randint(0, 7), GL2Antialiasing=r.randint(0, 3),
             GL2RenderActorLimit=r.randint(0, 20000), GL2ScreenShotQuality=r.randint(0, 5), GL2FogStart=0, GL2FogEnd=0)
    gf = dict(GL2OriginPawnCR=r.choice([0.5, 1.0, 1.5, 1.75, 2.0, 2.5, 2.75, 3.0, 3.5]),
              GL2OriginTerrainCR=r.choice([3.0, 4.0, 5.0, 5.5, 6.0, 6.5, 7.0, 7.5, 8.0, 9.0]),
              GL2OriginStaticMeshCR=1.0, GL2OriginStaticMeshLodCR=1.0, GL2ActorCR=1.0)
    sel = {k: r.choice([-1, 0, 1, 2, 3, 4, 5, 6, 7]) for k in COMBOS}
    for k in ('Resolution', 'Refresh'):
        sel[k] = r.choice([-1, 0, 1, 2, 5, 11, 50, len(modes) - 1 if modes else 0, len(modes)])
    chk = {k: r.randint(0, 1) for k in BOXES}
    dis = {k: r.randint(0, 1) for k in BOXES}
    fog = {}
    for i in range(1, 6):
        if r.random() < 0.5:
            fog['StartRange%d' % i] = r.choice([0.1, 0.2, 0.3])
        if r.random() < 0.5:
            fog['EndRange%d' % i] = r.choice([1.1, 1.2, 1.3])
    return dict(modes=modes, rates=rates, defrates=r.choice([[], [60], [60, 75, 85]]), x=x, y=y, bits=bits, rate=rate,
                gamma=r.choice([0.2, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 1.1, 1.2, 1.5]), pixel=r.choice([0, 11, 12, 14, 20, 30]),
                vs=r.choice([0, 11, 19, 20, 30]), ps=r.choice([0, 12, 19, 20, 30]), multisample=r.choice([0, 1, 2, 3]),
                texfmt=r.randint(0, 1), trilinear=r.randint(0, 1), fullscreen=r.randint(0, 1),
                setres=[r.randint(0, 1) for _ in range(2)], params=[r.randint(0, 100) for _ in range(5)], fog=fog,
                g=g, gl=None, gf=gf, gamma_ok=1, sel=sel, chk=chk, dis=dis, index=r.choice([-1, 0, 1, 2, 3, 4, 5, 6, 9]),
                msg=r.choice([0x501, 0x501, 0x501, 0x111, 0x201]), wp=None, lp=r.choice([0, 1, 2, 3, 4, 5, 6, 9]),
                basewp=r.choice([0, 0, 1]), sx=r.choice([0, 100, 640]), sy=r.choice([0, 50, 300]),
                pw=r.choice([100.0, 244.5, 300.0]), ph=r.choice([100.0, 330.25]),
                empty=[r.choice([209, 351, 210, 211, 594, 607, 214, 215, 540, 352, 868, 979, 1229]) for _ in range(r.choice([0, 0, 1, 3]))],
                nullk={k: r.random() < 0.3 for k in list(COMBOS) + list(BOXES) + ['Unused0', 'Unused1', 'Unused2']})


def run(e, A, c, name):
    c = dict(c)
    e.world(c)
    snaps = None
    args = []
    if name in ('ctor', 'dtor'):
        # the constructor / destructor: the object is poison, the whole thing is compared (the vtable pointer excepted)
        w = e.obj('video', 0x140)
        e.uc.mem_write(w, bytes([0xCD]) * 0x140)
        r = e.go(A[name], this=w, args=[], snaps={'this': (w, 0x140)})
        r['vt'] = None
        r['eax'] = 'this' if r['eax'] == w else r['eax']
        return r
    if name == 'create':
        w = e.wnd('video', e.vt_video, 0x200, parent=e.options)
        snaps = {'this': (w, WSIZE)}
    elif name == 'show':
        parent = e.wnd('options', e.vt_video, 0x300)
        w = e.wnd('video', e.vt_video, 0x200, parent=parent)
        snaps = {'this': (w, WSIZE), 'parent': (parent, 0x300)}
    else:
        w = e.page(c)
        snaps = e.snaps(w)
        if name == 'destroy':
            for nm in list(COMBOS) + list(BOXES):
                if c['nullk'].get(nm):
                    off = (COMBOS.get(nm) or BOXES.get(nm))
                    e.write32(w + off, 0)
            for k in range(3):
                kid = 0 if c['nullk'].get('Unused%d' % k) else e.wnd('Unused%d' % k, e.vt_combo, 0x300)
                e.write32(w + 0x12C + 4 * k, kid)
                if kid:
                    snaps['Unused%d' % k] = (kid, 0x300)
    if name in ('postproc', 'resolution'):
        args = [c['index'] & 0xFFFFFFFF]
    elif name == 'winproc':
        args = [c['msg'], c['ids'], c['lp']]
    elif name == 'paint':
        dev = e.obj('dev', 0x100)
        snaps['dev'] = (dev, 0x100)
        e.write32(w + 0x44, c['sx'])
        e.write32(w + 0x48, c['sy'])
        e.write32(w + 0x4C, f32(c['pw']))
        e.write32(w + 0x50, f32(c['ph']))
        args = [dev]
    r = e.go(A[name], this=w, args=args, snaps=snaps)
    if name == 'create':
        r['mem'].update({'new%d' % k: e.snapshot(p, 0x300) for k, p in enumerate(e.created)})
        r['mem']['options'] = e.snapshot(e.options, 0x300)
    if name in VOID:
        del r['eax']
    return r


def main():
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 200
    seed = int(sys.argv[2]) if len(sys.argv) > 2 else 1
    which = sys.argv[3].split(',') if len(sys.argv) > 3 else ['toggle', 'ctor', 'dtor', 'winproc', 'postproc', 'initmodes', 'resolution', 'lowerdetail', 'load', 'items', 'apply', 'clear', 'paint', 'destroy', 'create', 'show']
    total = 0
    for name in which:
        funcs = [FN[name]]
        es = VW('stock', funcs)
        eo = VW('ours', funcs)
        es.heap_mark, eo.heap_mark = es.heap_ptr, eo.heap_ptr
        es.track(FN[name])
        lm = log_map()
        As = {name: FN[name]}
        Ao = {name: lm[FN[name]]}
        r = random.Random(seed)
        cases = [rand_case(r) for _ in range(n)]
        for c in cases:
            c['ids'] = r.choice([0x1F08, 0x1F14, 0x1F25, 0xF0F01465, 0x1F09, 0x1F00])
        total += run_tests(name, es, eo, lambda e, A, c: run(e, A, c, name), cases, As, Ao)
        cov_report(es, FN[name])
    print('TOTAL problems', total)


if __name__ == '__main__':
    main()
