# Differential test of NCConsole::OnCreate (stock 0x10060240, 18351 bytes: every window of the in-game interface is made there), stock against
# ours under unicorn, on the call tracer of tr.py (the same harness as t_msn1.py / t_rest1.py).
#   python e7.py [cases] [seed]        (SHOWLOG=stock|ours prints the call log of the first case)
#
# History: this test used to stub the window constructors by address (the table was read off the lifted source: StockCallAt<void>(ctor, ...)).
# The sa lanes converted those calls to `new (mem) T(args)`, which the compiler inlines (the table became empty and the STOCK side then ran
# the real constructors against a bare memory: "Invalid memory fetch" at the first one), so since the C4 stage 5 it runs on tr.py, which runs
# the constructors for real on both sides and compares what they leave (the call log, every registered object, the return value).
# The console's own memory (first 0x3C0 bytes) and the objects it makes are compared.
import sys, os, re
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import tr
from tr import *
import t_msn1
from t_msn1 import *
import stubs
import sa

# the engine imports OnCreate reaches: the parser object (NCommandParser's base), the user object, the window objects' no-argument constructors
stubs.IMPORT_POP.update({'??0FParser@@QAE@XZ': 0, '??1FParser@@QAE@XZ': 0, '?SetSperator@FParser@@QAEXPAG@Z': 4})
CTOR0 = re.compile(r'^\?\?0\w+@@QAE@XZ$')
DTOR0 = re.compile(r'^\?\?1\w+@@QAE@XZ$')

GNEXTWND = 0x102C6BC0       # the next window number (NCWnd::OnCreate's too)
SEPARATORS = 0x10239610     # GCommandSeparators, L" ,\t\n": FParser::SetSperator's argument (NCommandParser::NCommandParser)


class CR(RW):
    def __init__(self, kind):
        RW.__init__(self, kind)
        for nm in self.import_names.values():
            if CTOR0.match(nm) or DTOR0.match(nm):
                stubs.IMPORT_POP.setdefault(nm, 0)
        if kind == 'ours' and not sa.ENABLED:
            # hook build: our NCommandParser reads GCommandSeparators through g_nwBase (the stand-in of the stock image's data); the standalone
            # has its own global, initialised like the stock's (gen_globals.py --check)
            self.uc.mem_write(NWBASE + (SEPARATORS - 0x10000000), ' ,\t\n'.encode('utf-16-le') + bytes(2))

    def do_import(self, nm):
        if CTOR0.match(nm):
            return self.this()
        if DTOR0.match(nm):
            return None
        if nm == '?SetSperator@FParser@@QAEXPAG@Z':
            self.log.append(('FParser::SetSperator', self.nm(self.this()), self.wstr(self.arg(0))))
            return None
        return RW.do_import(self, nm)


def b_console_oncreate(e, c):
    con = e.console
    e.uc.mem_write(con, bytes(0x4000))
    e.write32(con, e.class_vt('NCConsole'))
    e.objs[con] = 'console'
    e.reg_size[con] = 0x3C0
    e.cls_objs.add(con)
    e.wf(con, 0x4C, c['w'])
    e.wf(con, 0x50, c['h'])
    e.w32(con, 0x3BB4, c['ui'])
    e.write32(e.glob(GNEXTWND), c['num'])
    e.write32(e.gvars['?GIsOnlyL2Console@@3HA'], c['only'])
    e.write32(e.gvars['?GIsTestHtml@@3HA'], c['test'])
    return con, []


def gen_console(r):
    return dict(w=r.choice([800.0, 1024.0, 1280.0, 1600.0]), h=r.choice([600.0, 768.0, 1024.0, 900.0]), num=r.randint(0, 50),
                only=r.randint(0, 1), test=r.randint(0, 1), ui=r.randint(0, 1))


TESTS7 = {'console_oncreate': (0x10060240, 18351, b_console_oncreate, gen_console)}


def main():
    argv = sys.argv if len(sys.argv) > 2 else (sys.argv + ['8', '1'])[:3]
    t_msn1.run_tests(TESTS7, CR, argv)


if __name__ == '__main__':
    main()
