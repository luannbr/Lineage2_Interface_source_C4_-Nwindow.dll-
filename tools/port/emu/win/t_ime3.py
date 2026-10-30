# The small functions of the IME lane (stock vs ours under unicorn), next to t_taiwan.py (NCTaiwanIME):
#   langid   Util::GetLanguageID 0x101685F0, every one of the 65,536 language ids (the high word of the LCID varies)
#   imgr     NCIMEManager::WindowProc 0x10015FE0 (the db calls it NCEditBox::WindowProc): the NIM_* messages to the slots 77..84
#   nofinal  HasNoFinalConsonant 0x10064AD0 (the character in AX, not a hook: checked against the inline function of NConsoleWnd.h
#            that its three stock callers carry) for every 16-bit code
#   showwin  the folded 5-byte ShowWindow 0x100EFC40 (jmp NCWnd::ShowWindow)
#   python t_ime3.py [cases] [seed] [langid,imgr,nofinal,showwin]      (the old order [tests] [cases] [seed] still works)
import sys, os, random, struct, re
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from gw import *
import gw
import stubs
from unicorn.x86_const import *
import emu as _emu


def map_symbols():
    out = {}
    for ln in open(_emu.OURS_MAP, encoding='latin-1'):
        m = re.match(r'\s*[0-9a-f]{4}:[0-9a-f]{8}\s+(\S+)\s+([0-9a-f]{8})\s', ln)
        if m:
            out.setdefault(m.group(1), int(m.group(2), 16))
    return out


MAP = map_symbols()
stubs.IMPORT_POP.update({'GetUserDefaultLCID': 0, '?Printf@FString@@SA?AV1@PBGZZ': 0})
gw.VPOP.update({k: 8 for k in range(77, 85)})
gw.VPOP.update({48: 0})


# ---- Util::GetLanguageID ------------------------------------------------------------------------------------------------
class LW(GW):
    def __init__(self, kind):
        GW.__init__(self, kind, {}, {})
        self.case = None

    def do_import(self, nm):
        a = self.arg
        if nm == 'GetUserDefaultLCID':
            self.log.append(('GetUserDefaultLCID',))
            return self.case['lcid']
        if nm == '?Printf@FString@@SA?AV1@PBGZZ':
            ret = a(0)
            fmt = self.wstr(a(1))
            v = a(2)
            self.log.append(('Printf', fmt, v))
            self.fs_set(ret, fmt.replace('%04x', '%04x' % v))
            return ret
        return GW.do_import(self, nm)


def run_langid(e, A, c):
    mark = len(e.log)
    e.case = c
    res = e.alloc(16)
    e.uc.mem_write(res, bytes(16))
    e.call(A['langid'], args=[res])
    return dict(log=e.log[mark:], ret=e.reg(UC_X86_REG_EAX) - res, text=e.fs_get(res), esp=e.reg(UC_X86_REG_ESP) - (STACK_TOP - 0x1000) + 4,
                regs=[e.reg(r) for r in (UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP)])


# ---- NCIMEManager::WindowProc -------------------------------------------------------------------------------------------
class MW(GW):
    def __init__(self, kind):
        GW.__init__(self, kind, {}, {})
        self.case = None
        for slot in range(77, 85):
            self.vrets[slot] = lambda e, t, a: e.case['vret']


def run_imgr(e, A, c):
    mark = len(e.log)
    e.case = c
    e.basewp = c['basewp']
    w = e.window('mgr', size=0x300, rnd=random.Random(c['seed']))
    e.call(A['imgr'], this=w, args=[c['msg'], c['wp'], c['lp']])
    return dict(log=e.log[mark:], ret=e.reg(UC_X86_REG_EAX), esp=e.reg(UC_X86_REG_ESP) - (STACK_TOP - 0x1000),
                regs=[e.reg(r) for r in (UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP)], obj=e.snapshot(w, 0x300))


# ---- the folded ShowWindow ----------------------------------------------------------------------------------------------
class SW(GW):
    def __init__(self, kind):
        callees = {0x100345a0: ('NCWndShowWindow', 0, 0)}
        syms = {0x100345a0: '?ShowWindow@NCWnd@@UAEXXZ'} if kind == 'ours' else {}
        GW.__init__(self, kind, callees, syms)
        self.case = None


def run_showwin(e, A, c):
    mark = len(e.log)
    e.case = c
    w = e.window('wnd', size=0x300, rnd=random.Random(c['seed']))
    e.call(A['showwin'], this=w, args=[])
    return dict(log=e.log[mark:], esp=e.reg(UC_X86_REG_ESP) - (STACK_TOP - 0x1000),
                regs=[e.reg(r) for r in (UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP)], obj=e.snapshot(w, 0x300))


def main():
    argv = sys.argv[1:]
    # the order of the other tests (and of run_suite.py) is `<cases> <seed> [tests]`; the old order `[tests] [cases] [seed]` still works
    if argv and not argv[0].isdigit():
        which = argv[0].split(',')
        argv = argv[1:]
    else:
        which = argv[2].split(',') if len(argv) > 2 else ['langid', 'imgr', 'nofinal', 'showwin']
    n = int(argv[0]) if len(argv) > 0 else 300
    seed = int(argv[1]) if len(argv) > 1 else 1
    lm = log_map()
    r = random.Random(seed)
    total = 0
    if 'langid' in which:
        es, eo = LW('stock'), LW('ours')
        es.heap_mark, eo.heap_mark = es.heap_ptr, eo.heap_ptr
        As = {'langid': 0x101685f0}
        Ao = {'langid': lm[0x101685f0]}
        lcids = list(range(0x10000)) if n >= 0x10000 else sorted(r.sample(range(0x10000), n) + [0x401, 0x404, 0x409, 0x816, 0xC04, 0xC0A, 0x804, 0x424, 0x41F, 0x41D])
        cases = [dict(lcid=(r.randint(0, 0xFFFF) << 16) | lc, seed=lc) for lc in lcids]
        total += compare('langid', es, eo, run_langid, cases, As, Ao)
        # what the stock answers for every id, as a table
        seen = {}
        for c in cases[:0x10000]:
            es.heap_ptr = es.heap_mark
            out = run_langid(es, As, c)
            seen[c['lcid'] & 0xFFFF] = out['text']
            es.log.clear()
        keep = sorted(k for k, v in seen.items() if v == '0x%04x' % k and k != 0x409)
        print('langid: ids kept by the stock (besides 0x409):', ' '.join('%x' % k for k in keep) if len(seen) == 0x10000 else '(sampled)')
    if 'imgr' in which:
        es, eo = MW('stock'), MW('ours')
        es.heap_mark, eo.heap_mark = es.heap_ptr, eo.heap_ptr
        As = {'imgr': 0x10015fe0}
        Ao = {'imgr': lm[0x10015fe0]}
        msgs = [0xF0000012, 0xF0000013, 0xF0000014, 0xF0000015, 0xF0000016, 0xF0000017, 0xF0000018, 0xF0000019, 0xF000001A, 0xF000001B,
                0xF0000011, 0x100, 0x200, 0x203, 1, 0]
        cases = [dict(seed=r.randint(0, 10 ** 6), msg=r.choice(msgs), wp=r.choice([0, 1, 5, 0x41, 0xFFFFFFFF]), lp=r.choice([0, 1, 7, 0xFFFF0001]),
                      basewp=r.choice([0, 0, 0, 1]), vret=r.choice([0, 1, 5, 0xFFFFFFFF, 0x80000000])) for _ in range(n)]
        total += compare('imgr', es, eo, run_imgr, cases, As, Ao)
    if 'nofinal' in which:
        es = E4('stock')
        bad = 0
        thunks = es.alloc(16 * 0x10000)   # one thunk per code: unicorn keeps the translated block of code written over
        for ch in range(0x10000):
            thunk = thunks + 16 * ch
            es.uc.mem_write(thunk, b'\xB8' + struct.pack('<I', ch) + b'\xE9' + struct.pack('<i', 0x10064ad0 - (thunk + 10)))
            es.call(thunk)
            got = es.reg(UC_X86_REG_EAX)
            want = 1 if (ch < 0xAC00 or ch > 0xD7A3) else int((ch - 0xAC00) % (21 * 28) % 28 == 0)
            if got != want:
                bad += 1
                if bad < 5:
                    print('nofinal differs at %04x stock %d model %d' % (ch, got, want))
        print('nofinal 65536 codes, differ', bad)
        total += bad
    if 'showwin' in which:
        es, eo = SW('stock'), SW('ours')
        es.heap_mark, eo.heap_mark = es.heap_ptr, eo.heap_ptr
        As = {'showwin': 0x100efc40}
        Ao = {'showwin': lm[0x100efc40]}
        cases = [dict(seed=r.randint(0, 10 ** 6)) for _ in range(20)]
        total += compare('showwin', es, eo, run_showwin, cases, As, Ao)
    print('TOTAL problems', total)


if __name__ == '__main__':
    main()
