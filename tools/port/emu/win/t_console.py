# Differential tests of NCConsole's ToggleInterface, CloseWindows, Reset, ToggleMinimap, ToggleBoard, AddSystemMessage,
# AddSystemText and DockAboveChat (stock vs ours under unicorn)
import sys, os, random, struct
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from gw import *

FUNCS = {'toggle': (0x1005b640, 2), 'close': (0x1005b790, 0), 'reset': (0x1005ba30, 0), 'minimap': (0x1005bcf0, 1),
         'board': (0x1005bd70, 0), 'sysmsg': (0x1005c250, 2), 'systext': (0x1005c2b0, 3), 'dock': (0x1005cf40, 0)}
CALLEES = {
    0x100efc20: ('ClearLists', 0, 0), 0x100e6b90: ('PrivShop6b90', 0, 0), 0x100e3380: ('PrivShopReset', 0, 0),
    0x100b1b40: ('PartyRoomExit', 0, 0), 0x10069ab0: ('QuitBuy', 0, 0), 0x100698d0: ('QuitSell', 0, 0),
    0x10068100: ('RecipeShopListSet', 0, 0), 0x10066890: ('TradeDone', 4, 0), 0x100dfed0: ('PetitionClear', 0, 0),
    0x10007cc0: ('GaraReset', 0, 0), 0x10051c20: ('Empty264', 0, 0), 0x10116a80: ('AbnormalClearAll', 0, 0),
    0x10108d50: ('ClearShortCuts', 0, 0), 0x100ef9d0: ('ClearTargets', 0, 0), 0x100ec100: ('MainWnd0xF8', 0, 0),
    0x100bd7c0: ('MapCoverReset', 0, 0), 0x100b1690: ('MatchClearList', 0, 0), 0x100576a0: ('ChatClearAll', 0, 0),
    0x10050b00: ('ResetPledgeWnd', 0, 0), 0x1004dc40: ('PledgeReset', 0, 0), 0x10074e60: ('PlaySound', 4, 0),
    0x10068d60: ('RequestShowBoard', 4, 0), 0x1002dd70: ('SetTab', 8, 0), 0x10052f40: ('AddChatMessage', 16, 0),
    0x10035e50: ('SetScreenPos', 12, 0), 0x10035650: ('GetLocalRect', 4, 0),
}
if not sa.ENABLED:
    # hook build: our NCConsole::Reset has NMapCoverWndReset::Reset inlined and calls the compiled NCMinimapWnd::ClearTargets (stock: 0x100bd7c0 jumps
    # to 0x100bd3d0): both sides run that code for real here, with the list-empty callee 0x100bcd30 (ours reaches it at NWBASE + rva) stubbed
    # on both sides. The standalone keeps the call out of line: MapCoverReset stays a stub there.
    del CALLEES[0x100bd7c0]
    CALLEES[0x100bcd30] = ('MinimapListEmpty', 4, 0)
# hook build: our own code calls the compiled member of a callee (e.g. NMapCoverWndReset::Reset), not the hook's forwarder that log_map gives:
# patched through the decorated symbol too. The standalone has no forwarders (sa.func_map gives the member).
import sa, slotsyms
OURS_SYMS = {} if sa.ENABLED else slotsyms.auto_syms(list(CALLEES), LOGFILE, verbose=False)
stubs.IMPORT_POP.update({'??0L2ParamStack@@QAE@H@Z': 4, '??1L2ParamStack@@QAE@XZ': 0})
WIN_OFFS = list(range(0xC8, 0x290, 4))


def pointfix(e, a):
    return [a[0], a[1], (e.rds32(a[2]), e.rds32(a[2] + 4)) if a[2] else None]


class CW(GW):
    def __init__(self, kind):
        GW.__init__(self, kind, CALLEES, OURS_SYMS)
        self.argfix = {'SetScreenPos': pointfix}
        def rect(e, t, a):
            r = (0, 0, f32(1024), f32(768)) if e.objs.get(t) == 'console' else (20, 500, f32(348), f32(187))
            e.log.append(('rectfor', e.objs.get(t)))
            for i, v in enumerate(r):
                e.write32(a[0] + 4 * i, v)
            return 0
        self.rets['GetLocalRect'] = rect

    def do_import(self, nm):
        if nm == '??0L2ParamStack@@QAE@H@Z':
            return self.this()
        if nm == '??1L2ParamStack@@QAE@XZ':
            return None
        return GW.do_import(self, nm)


def world(e, c):
    rnd = random.Random(c['seed'])
    e.objs = {}
    e.shown = {}
    con = e.console
    e.uc.mem_write(con, bytes(0x4000))
    e.objs[con] = 'console'
    e.write32(con + 0x4C, f32(1024)); e.write32(con + 0x50, f32(768))
    e.write32(con, e.vt)
    e.shown[con] = rnd.randint(0, 1)
    for off in WIN_OFFS:
        w = e.window('w%x' % off, rnd=rnd)
        # sub-objects at 0xE8 .. 0x100 (the windows' own children)
        for so in range(0xE8, 0x104, 4):
            sub = e.window('s%x_%x' % (off, so), size=0x400, rnd=rnd)
            e.write32(w + so, sub)
        e.write32(con + off, w)
        e.shown[w] = rnd.randint(0, 1)
    # the chat window: its system list (+0x1A0)
    chat = e.window('chat', size=0x400, rnd=rnd)
    sl = e.window('syslist', rnd=rnd)
    e.write32(chat + 0x1A0, sl)
    e.shown[sl] = rnd.randint(0, 1)
    e.write32(con + 0xF4, chat)
    e.shown[chat] = rnd.randint(0, 1)
    # GConsole's own fields
    god = e.window('god', rnd=rnd)
    gw_ = e.window('godwin', rnd=rnd)
    e.write32(god + 0xF4, gw_)
    e.shown[gw_] = rnd.randint(0, 1)
    e.write32(con + 0x3BE0, god)
    pw = e.window('petition', rnd=rnd) if c['petition'] else 0
    e.write32(con + 0x3C00, pw)
    if pw:
        e.shown[pw] = rnd.randint(0, 1)
    # the strings of the system messages
    for i in (1, 2, 5, 9):
        base = e.gdata + 0x722C8 + i * 0x30 + 4
        e.fs_set(base, c['msgs'][i] if i in c['msgs'] else '')
    return con


def run(e, A, c):
    mark = len(e.log)
    con = world(e, c)
    # the stack below the call is zeroed: the functions pass the address of an uninitialised local (L2ParamStack) to a stub that prints it, so
    # the garbage of the previous function (which differs between the stock and ours) would show
    e.uc.mem_write(STACK_TOP - 0x3000, bytes(0x2000))
    which = c['f']
    va, nargs = A[which]
    args = c['args']
    if which in ('systext',):
        args = [e.put_wstr(args[0]) if args[0] is not None else 0] + list(args[1:])
    r = e.call(va, this=con, args=list(args))
    snap = e.snapshot(con, 0x290)
    # the sub-objects the functions write into
    subs = []
    for off in (0x170, 0x174):
        w = e.read32(con + off)
        for so in (0xE8, 0xF8):
            sub = e.read32(w + so)
            subs.append(e.snapshot(sub, 0x200))
    return dict(log=e.log[mark:], console=snap, subs=subs)


def rand(r, f):
    c = dict(seed=r.randint(0, 10 ** 6), f=f, petition=r.randint(0, 1), msgs={1: 'one', 2: '', 5: 'five', 9: 'nine'})
    if f == 'toggle':
        c['args'] = [r.randint(0, 1), r.randint(0, 1)]
    elif f == 'minimap':
        c['args'] = [r.randint(0, 5)]
    elif f == 'sysmsg':
        c['args'] = [r.choice([1, 2, 5, 9, 3]), r.choice([0xFFB09B79, 0xFFDCDCDC])]
    elif f == 'systext':
        c['args'] = [r.choice([None, '', 'hello']), r.randint(0, 7), 0xFFDCDCDC]
    else:
        c['args'] = []
    return c


if __name__ == '__main__':
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 40
    seed = int(sys.argv[2]) if len(sys.argv) > 2 else 1
    which = sys.argv[3].split(',') if len(sys.argv) > 3 else list(FUNCS)
    es = CW('stock'); eo = CW('ours')
    lm = log_map()
    es.heap_mark, eo.heap_mark = es.heap_ptr, eo.heap_ptr
    As = {k: v[0] for k, v in FUNCS.items()}
    As = {k: (v[0], v[1]) for k, v in FUNCS.items()}
    Ao = {k: (lm[v[0]], v[1]) for k, v in FUNCS.items()}
    r = random.Random(seed)
    total = 0
    for f in which:
        cases = [rand(r, f) for _ in range(n)]
        total += compare(f, es, eo, run, cases, As, Ao)
    print('TOTAL problems', total)
