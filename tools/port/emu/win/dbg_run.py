# Debug helper: run one test case of a t_*.py table on one side and print the last instructions before an emulation error.
#   python dbg_run.py <module> <test> <stock|ours> [case index] [seed]
import sys, os, random, importlib
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from tr import *
from unicorn import UC_HOOK_CODE

mod = importlib.import_module(sys.argv[1])
test, kind = sys.argv[2], sys.argv[3]
idx = int(sys.argv[4]) if len(sys.argv) > 4 else 0
seed = int(sys.argv[5]) if len(sys.argv) > 5 else 1
mod.LM = log_map()
import t_msn1 as _t1
_t1.LM = mod.LM
TESTS = None
for nm in dir(mod):
    if nm.startswith('TESTS') and isinstance(getattr(mod, nm), dict) and test in getattr(mod, nm):
        TESTS = getattr(mod, nm)
RWC = None
for nm in ([os.environ['DBG_CLASS']] if os.environ.get('DBG_CLASS') else []) + ['R1', 'SW', 'CW', 'FW', 'MW', 'RW']:
    if hasattr(mod, nm):
        RWC = getattr(mod, nm)
        break
sva, size, build, gen = TESTS[test][:4]
r = random.Random(seed)
cases = [gen(r) for _ in range(idx + 1)]
c = cases[idx]
e = RWC(kind)
e.heap_mark = e.heap_ptr
e.reset_case(c)
e.shown = {}
e.pre_len = None
this, args = build(e, c)
A = (sva if kind == 'stock' else mod.LM[sva])
e.set_fn(A, size)
hist = []
md = Cs(CS_ARCH_X86, CS_MODE_32)


def hook(uc, address, sz, user):
    code = bytes(uc.mem_read(address, sz))
    ins = list(md.disasm(code, address))
    hist.append('%08x %s' % (address, ('%s %s' % (ins[0].mnemonic, ins[0].op_str)) if ins else code.hex()))
    if len(hist) > 60:
        hist.pop(0)


e.uc.hook_add(UC_HOOK_CODE, hook, None, 0x10000000, 0x10400000)
WATCH = set(int(x, 16) for x in os.environ.get('DBG_ADDRS', '').split(',') if x)


def watch(uc, address, sz, user):
    if address in WATCH:
        from unicorn.x86_const import UC_X86_REG_ESP
        esp = uc.reg_read(UC_X86_REG_ESP)
        print('at %x esp=%x  [esp]=%x [esp+4]=%x [esp+8]=%x' % (address, esp, e.read32(esp), e.read32(esp + 4), e.read32(esp + 8)))


if WATCH:
    e.uc.hook_add(UC_HOOK_CODE, watch, None, 0x10000000, 0x10400000)
print('case', c)
try:
    ret = e.call(A, this=this, args=args)
    print('ok ret', ret)
except Exception as ex:
    import traceback; traceback.print_exc()
    print('EXC', ex)
    print('\n'.join(hist[-40:]))
for ent in e.log[-12:]:
    print(ascii(ent)[:200])
