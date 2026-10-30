# Differential test of NCGaraDialogBox::OnOK (0x10009600, 6,255 bytes: every dialog type of the desktop's message box: amounts, drop /
# destroy an item, trade / party / clan / alliance / friend answers, the clan crest 0x53 / 0x54, the quest delete 0x1E ...) and
# OnCancel (0x10007e00): stock against ours under unicorn.
# The console's requests that are 14-byte tail jumps into the network handler (0x100684f0 ...) are NOT stubbed: they run, so the net
# handler's slot that each dialog reaches is compared end to end (the identity of a tiny function is where wave B found its bugs).
# Everything else the stock calls is a logging service identified by its stock address (ours: by its hook / symbol).
#   python t_gara.py [cases] [seed] [ok,cancel]
import sys, os, random, struct
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from pw import *
import pw

ROOT = fncalls.ROOT
FN = {'ok': 0x10009600, 'cancel': 0x10007e00}
pw.POP_OVERRIDE.update({0x10074e60: 4})        # NConsoleWnd::PlayInterfaceSound -> PlaySoundName
THUNK = bytes.fromhex('8b4154' '8b4864' '8b11' 'ffa2')    # mov eax,[ecx+54]; mov ecx,[eax+64]; mov edx,[ecx]; jmp [edx+N]
GP = 0x2B0


def thunks_of(funcs):
    out = set()
    for f in funcs:
        direct, imports, vcalls = fncalls.scan(f)
        for t in direct:
            b = bytes(fncalls.MEM[t - fncalls.BASE:t - fncalls.BASE + len(THUNK)])
            if b == THUNK:
                out.add(t)
    return out


class GV(PW):
    def __init__(self, kind, funcs):
        sk = thunks_of(funcs)
        PW.__init__(self, kind, funcs, {0x1017143e: ('wtoi64', 0, 0)}, skip=sk)
        self.cdecl_n['wtoi64'] = 1
        if sa.ENABLED:
            # the standalone links its own copy of the CRT's _wtoi64 (nothing to trap: it runs for real in ours), so the stock's call to
            # 0x1017143e must not be logged either: transparent on both sides (the results of the parse are still compared, through the
            # sizes / texts the dialogs show; the stock side answers with wtoi64() below, ours with the real CRT function)
            self.nolog.add('wtoi64')
        stubs.IMPORT_POP.update({'WideCharToMultiByte': 32, '?GetName@User@@QAEPAGXZ': 0, '?KeyDown@UInput@@QAEEH@Z': 4,
                                 '?appRequestExit@@YAXH@Z': 0})
        net_pops = vt_inc_pops(os.path.join(ROOT, 'include/ue2/gen/UL2NetHandler.vt.inc'))
        self.vt_net = self.make_vt('net', net_pops, nslots=300)
        self.vt_con = self.class_vt('NConsoleWnd')
        self.vt_gara = self.class_vt('NCGaraDialogBox')
        self.vt_edit = self.class_vt('NCEditBox', rets={75: self.edit_commit})
        self.vt_prog = self.class_vt('NCProgressCtrl')
        self.vt_owner = self.class_vt('NCWnd')
        self.vt_vp = self.make_vt('viewport', {})
        self.write32(self.console, self.vt_con)
        self.engine = self.obj('engine', 0x1000)
        self.net = self.obj('net', 0x100, self.vt_net)
        self.client = self.obj('client', 0x100)
        self.vparr = self.obj('vparr', 16)
        self.vp0 = self.obj('viewport', 0x100)
        self.input = self.obj('input', 0x100)
        self.user = self.obj('user', 0x200)
        self.pawn = self.obj('pawn', 0x800)
        self.actor = self.obj('actor', 0x200)
        self.desktop = self.obj('desktop', 0x100)
        self.write32(self.console + 0x54, self.engine)
        self.write32(self.engine + 0x64, self.net)
        self.write32(self.engine + 0x58, self.client)
        self.write32(self.client + 0x38, self.vparr)
        self.write32(self.vparr, self.vp0)
        self.write32(self.user + 0x158, self.pawn)
        self.rets['NConsoleWnd::GetMyUser'] = lambda e, t, a: e.user
        self.rets['NConsoleWnd::GetSelectedActor'] = lambda e, t, a: e.actor if e.case['actor'] else 0
        self.rets['NConsoleWnd::HasInvalidChars'] = lambda e, t, a: e.case['invalid']
        self.rets['NConsoleWnd::IsValidName'] = lambda e, t, a: e.case['valid']
        self.rets['NControl::SendTo'] = lambda e, t, a: e.case['send']
        self.rets['NConsoleLeftFns::GetEtcItemStackKind'] = lambda e, t, a: e.case['stack']
        self.rets['NDialogInfo::NDialogInfo'] = self.dlg_ctor
        self.rets['NItemRequest::NItemRequest'] = self.req_ctor
        self.rets['NConsoleWnd::ShowDialog'] = self.show_dialog
        self.rets['wtoi64'] = self.wtoi64
        self.rets['NConsoleWnd::RegisterPledgeCrest'] = lambda e, t, a: 0
        self.case = None

    def edit_commit(self, e, t, a):
        s = e.case['commit']
        return e.put_wstr(s) if s is not None else 0

    def wtoi64(self, e, t, a):
        s = e.wstr(a[0]) or ''
        import re as _re
        m = _re.match(r'\s*([+-]?\d+)', s)
        v = int(m.group(1)) if m else 0
        v &= 0xFFFFFFFFFFFFFFFF
        e.uc.reg_write(UC_X86_REG_EDX, v >> 32)
        return v & 0xFFFFFFFF

    def dlg_ctor(self, e, t, a):
        e.uc.mem_write(t, bytes(0xA0))
        e.write32(t, 0xFFFFFFFF)
        e.write32(t + 4, 3)
        e.write32(t + 0xC, 1)
        return t

    def req_ctor(self, e, t, a):
        e.uc.mem_write(t, bytes(0x30))
        return t

    def show_dialog(self, e, t, a):
        e.log.append(('ShowDialog dump', e.snapshot(a[0], 0xA0)))
        return 0

    def do_import(self, nm):
        t = self.this()
        if nm == 'WideCharToMultiByte':
            src = self.wstr(self.arg(2)) or ''
            dst = self.arg(4)
            data = src.encode('cp1252', 'replace') + b'\0'
            if dst:
                self.uc.mem_write(dst, data[:self.arg(5)])
            return len(data)
        if nm == '?GetName@User@@QAEPAGXZ':
            return self.static_str('name%d' % (self.case['nameid']))
        if nm == '?KeyDown@UInput@@QAEEH@Z':
            return self.case['shift']
        if nm == '?appRequestExit@@YAXH@Z':
            self.log.append(('appRequestExit', self.arg(0)))
            return None
        return PW.do_import(self, nm)

    def world(self, c):
        self.case = c
        self.pstacks = {}
        self.write32(self.pawn + 0x5F4, c['pawnflags'])
        self.write32(self.pawn + 0x690, c['pawnflags'] ^ 1)
        self.write32(self.actor + 8, c['actor8'])
        for i in range(0, 0x80, 4):
            self.write32(self.engine + 0x800 + i, 0x1000 + i)
        self.write32(self.console + 0x3C00, 0x7777)

    def page(self, c):
        w = self.wnd('gara', self.vt_gara, GP)
        prog = self.wnd('progress', self.vt_prog, 0x100)
        edit = self.wnd('edit', self.vt_edit, 0x300)
        ent = self.obj('ownerentry', 0x40)
        ownerwnd = self.wnd('ownerwnd', self.vt_owner, 0x300)
        self.write32(ent + 4, ownerwnd)
        self.write32(w + 0x23C, prog)
        self.write32(w + 0x240, edit)
        self.write32(w + 0x244, c['id'])
        self.write32(w + 0x248, 3)
        self.write32(w + 0x24C, ent if c['owner'] else 0)
        self.write32(w + 0x250, c['input'])
        self.write32(w + 0x254, c['large'])
        self.write32(w + 0x258, c['flag238'])
        self.write32(w + 0x25C, c['progress'])
        self.fs_set(w + 0x26C, c['paramtext'])
        self.write32(w + 0x278, c['p1'])
        self.write32(w + 0x27C, c['p2'])
        for i in range(3):
            self.write32(w + 0x280 + 4 * i, 0x3F000000 + i)
        if not c['edit']:
            self.write32(w + 0x240, 0)
        return w, edit

    def snaps(self, w, edit):
        s = {'ownerwnd': (self.read32(self.read32(w + 0x24C) + 4) if self.read32(w + 0x24C) else 0, 0x300), 'this': (w, GP), 'console': (self.console, 0x3D00), 'engine': (self.engine, 0x1000), 'user': (self.user, 0x200),
             'pawn': (self.pawn, 0x800), 'actor': (self.actor, 0x200)}
        if edit:
            s['edit'] = (edit, 0x300)
        return s


IDS = list(range(0, 0x58))
if os.environ.get('GARA_IDS'):
    IDS = [int(x, 0) for x in os.environ['GARA_IDS'].split(',')]


def rand_case(r):
    commit = r.choice([None, '', '5', '1', '1000', '999999', '1000001', '2000000000', '2000000001', '99999999999', '-5', 'abc',
                       'Name', 'LongNameThatIsMoreThanSixteenChars', '12', '0', '4294967296', '4294967295', '2147483648'])
    return dict(id=r.choice(IDS + IDS + [0x1E, 0x53, 0x54, 0xA, 7, 4, 5]), owner=r.choice([1, 1, 0]), input=r.randint(0, 1),
                large=r.randint(0, 1), flag238=r.randint(0, 5), progress=r.randint(0, 1), paramtext=r.choice(['', 'Pname', 'x y']),
                p1=r.choice([0, 7, 1234567]), p2=r.choice([0, 3, 99]), edit=1, commit=commit,
                send=r.randint(0, 1), stack=r.choice([0, 1, 2, 3]), invalid=r.randint(0, 1), valid=r.randint(0, 1),
                pawnflags=r.choice([0, 1, 2, 3, 0x10]), actor=r.randint(0, 1), actor8=r.choice([0, 0, 1]), shift=r.randint(0, 1),
                nameid=r.randint(0, 3))


def run(e, A, c, name):
    c = dict(c)
    if os.environ.get('GARA_LONG'):
        c.update(invalid=0, valid=1, commit='LongNameThatIsMoreThanSixteenChars', actor=1, actor8=0)
    if os.environ.get('GARA_SHORT'):
        c.update(invalid=0, valid=1, commit='Name', actor=1, actor8=0)       # a valid short name: the nickname request (0x10009c47)
    e.world(c)
    w, edit = e.page(c)
    r = e.go(A[name], this=w, args=[], snaps=e.snaps(w, edit if c['edit'] else 0))
    return r


def main():
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 1000
    seed = int(sys.argv[2]) if len(sys.argv) > 2 else 1
    which = sys.argv[3].split(',') if len(sys.argv) > 3 else ['ok', 'cancel']
    lm = log_map()
    total = 0
    for name in which:
        funcs = [FN[name]]
        es = GV('stock', funcs)
        eo = GV('ours', funcs)
        es.heap_mark, eo.heap_mark = es.heap_ptr, eo.heap_ptr
        es.track(FN[name])
        r = random.Random(seed)
        cases = [rand_case(r) for _ in range(n)]
        total += run_tests(name, es, eo, lambda e, A, c: run(e, A, c, name), cases, {name: FN[name]}, {name: lm[FN[name]]})
        cov_report(es, FN[name])
    print('TOTAL problems', total)


if __name__ == '__main__':
    main()
