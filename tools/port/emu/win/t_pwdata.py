# Differential test of the small functions that STORE TO A GLOBAL of the NWindow image (stock against ours under unicorn, on the fake world of pw.py): the
# part of pw.py that compares the data writes of a call (PW.go: `data`). In the hook build the data is the whole stock image's data at NWBASE; in the
# standalone (NWD_SA=1) it is the named globals of src/standalone/NwGlobals.cpp, taken one by one at their stock addresses (pw.py data_snap / data_mask).
# None of the big functions of t_video / t_srvsel / t_gara / t_sys stores to a global itself, so this is the test that proves the data comparison
# of the standalone mode sees a store (the mutation runs are in the report of the sa-emu lane: a flipped constant, a removed store and a store to
# another global are all found; a store to an unnamed place of .data is not).
#   NConsoleWnd::LockCursor / UnlockCursor (GCursorLocked), NCWnd::KillFocus (GFocusEntry), NCWnd::OnKeyUp and NCEditBox::HandleKeyUp (GShiftDown),
#   NCTabControl::OnCreate (GNextTabGroupID)
#   python t_pwdata.py [cases] [seed] [lock,unlock,killfocus,onkeyup,editkeyup,tabcreate]
import sys, os, random, struct
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from pw import *
import pw

FN = {'lock': 0x1006a2f0, 'unlock': 0x1006a300, 'killfocus': 0x100347a0, 'onkeyup': 0x10034b60, 'editkeyup': 0x1000c210,
      'tabcreate': 0x1002d6a0}
CLS = {'lock': 'NConsoleWnd', 'unlock': 'NConsoleWnd', 'killfocus': 'NCWnd', 'onkeyup': 'NCWnd', 'editkeyup': 'NCEditBox', 'tabcreate': 'NCTabControl'}
VOID = ('lock', 'unlock', 'killfocus')
G = dict(shift=0x102c6bb8, focus=0x102c6bbc, cursor=0x102d26d4, tabgroup=0x1022a7f8)      # GShiftDown, GFocusEntry, GCursorLocked, GNextTabGroupID


class DW(PW):
    def __init__(self, kind, funcs, cls):
        PW.__init__(self, kind, funcs, {})
        self.vt_this = self.class_vt(cls, rets={33: lambda e, t, a: e.case['v33'], 28: lambda e, t, a: e.case['v33'], 26: lambda e, t, a: 1})
        self.vt_wnd = self.class_vt('NCWnd')
        self.case = None

    def world(self, c):
        self.case = c
        w = self.wnd('this', self.vt_this, 0x400)
        ent = self.read32(w + 0x74)
        child = self.wnd('child', self.vt_wnd, 0x300)
        self.write32(ent + 8, child)                                  # (the second entry of the chain: RemoveChildWindow calls through it)
        self.write32(w + 0x40, c['bfocus'])
        self.write32(w + 0x68, c['style'])
        self.write32(w + 0x2ac, c['sel'])
        self.write32(self.glob(G['shift']), c['shift'])
        self.write32(self.glob(G['focus']), {0: 0, 1: ent, 2: ent + 4, 3: child}[c['focus']])
        self.write32(self.glob(G['cursor']), c['cursor'])
        self.write32(self.glob(G['tabgroup']), c['tabgroup'])
        return w, ent, child


def rand_case(r):
    return dict(shift=r.choice([0, 1, 2]), focus=r.randint(0, 3), cursor=r.choice([0, 1, 2, 5]), tabgroup=r.choice([0, 1, 7, 0xFFFFFFFF]),
                key=r.choice([0x10, 0x10, 0x11, 0x41, 0]), v33=r.randint(0, 1), bfocus=r.choice([0, 1, 7]), style=r.choice([0, 0x40, 0x42, 3]),
                sel=r.choice([0, 1, 9]), lp=r.choice([0, 0x14B0001]))


def run(e, A, c, name):
    w, ent, child = e.world(c)
    args = []
    if name in ('onkeyup', 'editkeyup'):
        args = [c['key'], c['lp']]
    snaps = {'this': (w, 0x300), 'child': (child, 0x300), 'entry': (ent, 0x40)}
    r = e.go(A, this=w, args=args, snaps=snaps)
    if name in VOID:
        del r['eax']
    return r


def main():
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 100
    seed = int(sys.argv[2]) if len(sys.argv) > 2 else 1
    which = sys.argv[3].split(',') if len(sys.argv) > 3 else list(FN)
    lm = log_map()
    total = 0
    for name in which:
        try:
            ova = lm[FN[name]]
        except sa.NoStandaloneFunction as ex:
            sa.skip(name, ex)
            continue
        es = DW('stock', [FN[name]], CLS[name])
        eo = DW('ours', [FN[name]], CLS[name])
        es.heap_mark, eo.heap_mark = es.heap_ptr, eo.heap_ptr
        es.track(FN[name])
        r = random.Random(seed)
        cases = [rand_case(r) for _ in range(n)]
        total += run_tests(name, es, eo, lambda e, A, c: run(e, A, c, name), cases, FN[name], ova)
        cov_report(es, FN[name])
    print('TOTAL problems', total)


if __name__ == '__main__':
    main()
