# What t_vipwnd*.py (wave B, items3) do not cover of the C4 multisell ("VIP shop") windows: the constructors and destructors of the four classes,
# the two OnCreate and the info page's ShowWindow, the shop's WindowProc; stock against ours under unicorn with the call tracer (t_msn1.py /
# tr.py). Everything else of NCVIPShopWnd.cpp is proved by t_vipwnd.py .. t_vipwnd4.py.
#   python t_vip5.py [cases] [seed] [test,test,...]
import sys, os, random, struct
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import tr
from tr import *
import t_msn1
from t_msn1 import *
import t_msn5
from t_msn5 import CW, gen5, mk, make_args
import t_rest1

SRC = 'src/nwindow/NCVIPShopWnd.cpp'
t_rest1.REST1_FILES.clear()
for cn in ('NCVIPShopWnd', 'NCVIPShopItemWnd', 'NCVIPShopNeededItemWnd', 'NCVIPShopItemInfoWnd'):
    t_rest1.REST1_FILES[cn] = SRC
# the methods the t_vipwnd tests already prove are left to them
DONE = {'OnPaint', 'DrawItemInfo', 'AddItem', 'AddNeeded', 'Clear', 'OnDestroy', 'MoveNames', 'ScrollUp', 'ScrollDown', 'OnScrollBar', 'OnMouseWheel',
        'OnOK', 'ShowItem', 'Reset', 'OnCancel', 'HideWindow', 'OnLButtonDown', 'DestroyNeeded', 'SetItem'}
CL = t_rest1.build()
t_msn5.CLASSES.update(CL)
TESTSV = {}


class VW(CW):
    def __init__(self, kind):
        CW.__init__(self, kind)
        # the bases' destructors the stock calls and ours expands (see t_rest1.py)
        self.transparent |= {0x10002fb0, 0x10002d00, 0x10002e30, 0x10002350, 0x100022c0, 0x10015950, 0x100a2cb0, 0x10050030}


for cls, (cva, csz, ckinds, methods) in CL.items():
    for (mname, va, size, kind) in methods:
        if mname in DONE:
            continue
        TESTSV['%s.%s' % (cls, mname)] = (va, size, mk(cls, mname, va, size, kind), gen5, {'noret': kind not in ('C', 'D', 'P', 'W', 'T')})

    def bctor(e, c, cls=cls, ckinds=ckinds):
        return poison(e, cls, cls), make_args(e, ckinds)
    TESTSV['%s.ctor' % cls] = (cva, csz, bctor, lambda r: {}, {'noret': True})

    def bdtor(e, c, cls=cls, cva=cva, csz=csz, ckinds=ckinds):
        p = poison(e, cls, cls, 0)
        e.run_real(cva, csz, p, make_args(e, ckinds))
        e.pre_len = len(e.log)
        return p, []
    for va2, (sz2, nm2, cl2) in tr.functions().items():
        if nm2 == '%s::~%s' % (cls, cls):
            TESTSV['%s.dtor' % cls] = (va2, sz2, bdtor, lambda r: {}, {'noret': True})


# the second constructor of the item window (six arguments, passed on to NCItemWnd's)
def b_ctor2(e, c):
    return poison(e, 'NCVIPShopItemWnd', 'NCVIPShopItemWnd'), [c['a%d' % i] for i in range(6)]


def gen_ctor2(r):
    return {'a%d' % i: r.choice([0, 1, 7, 100]) for i in range(6)}


TESTSV['NCVIPShopItemWnd.ctor2'] = (0x100a3640, 120, b_ctor2, gen_ctor2, {'noret': True})

if __name__ == '__main__':
    t_msn1.run_tests(TESTSV, VW, sys.argv)
