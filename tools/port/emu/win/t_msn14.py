# Differential tests of the messenger's installed-version check (wave C, lane proof2): MessengerWnd.cpp's MsnGetFileVersion, MsnGetMessengerVersion and
# MsnCheckVersion (the registry and the version resource of the installed MSN messenger, modelled under the stock and ours); stock against ours under
# unicorn (t_msn1.py / tr.py).
#   python t_msn14.py [cases] [seed] [test,test,...]
import sys, os, random, struct
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import tr
from tr import *
import t_msn1
from t_msn1 import *
import t_msn10
from t_msn10 import DW, poison_obj

TESTS14 = {}
tr.stubs.IMPORT_POP.update({'??0FBlowFish@@QAE@XZ': 0, '?InitializeBlowfish@FBlowFish@@QAEFQAEF@Z': 8, '?BlowfishDecrypt@FBlowFish@@QAEXPAEH@Z': 8, 'StrCpyNW': 12, 'PathAppendW': 8, 'RegOpenKeyExW': 20, 'RegQueryValueExW': 24, 'RegCloseKey': 4, 'LocalAlloc': 8, 'LocalFree': 4,
                            'wnsprintfW': 0, 'GetFileVersionInfoSizeW': 8, 'GetFileVersionInfoW': 16, 'VerQueryValueW': 16})
# the stock's thunks of the version DLL (jmp [import]) and its wcslen
tr.ARGS_OVERRIDE.update({0x10152c00: 2, 0x10168c00: 2, 0x10168cf0: 3, 0x10170afc: 2, 0x10170af6: 4, 0x10170af0: 4, 0x1017187d: 1})
tr.POP_OVERRIDE.update({0x10152c00: 0, 0x10168c00: 0, 0x10168cf0: 0, 0x10170afc: 8, 0x10170af6: 16, 0x10170af0: 16, 0x1017187d: 0})
VERSION_API = {'GetFileVersionInfoSizeW': 0x10170afc, 'GetFileVersionInfoW': 0x10170af6, 'VerQueryValueW': 0x10170af0}


class VW14(DW):
    def __init__(self, kind):
        DW.__init__(self, kind)
        for nm, va in VERSION_API.items():
            self.rules[va] = (lambda n: (lambda e, ecx, a: e.api(n, a)))(nm)
        self.rules[0x1017187d] = lambda e, ecx, a: len(e.wstr(a[0]) or '')
        self.transparent |= {0x10168c00, 0x10151070, 0x10150ff0}
        self.rules[0x10152c00] = self.array_add1                                                   # the pointer array's Add(1, 4) helper (cdecl)                                                           # MsnAnsiToWide: the stock's own (ours inlines the API call)
        self.rules[0x10168cf0] = self.get_file_version                                             # MsnGetFileVersion (when trapped, for GetMessengerVersion's test)

    def ansi_to_wide(self, e, ecx, a):
        raw = b''
        i = 0
        while i < 0x100:
            ch = e.uc.mem_read(a[0] + i, 1)[0]
            if not ch:
                break
            raw += bytes([ch])
            i += 1
        e.uc.mem_write(a[1], raw.decode('latin-1').encode('utf-16le') + bytes(2))
        return len(raw) + 1

    def get_file_version(self, e, ecx, a):
        if not getattr(e, 'gfv_on', False):
            return 0
        for p, v in ((a[1], e.fv_ms), (a[2], e.fv_ls)):
            if p:
                e.write32(p, v)
        return 1

    def api(self, nm, a):
        """the model of the system calls (shared by the stock's thunks / direct imports and ours)"""
        w = lambda p: (self.wstr(p) or '') if p else None
        if nm == 'GetFileVersionInfoSizeW':
            self.log.append(('api', nm, w(a[0])))
            if a[1]:
                self.write32(a[1], 0)
            return self.fv_size
        if nm == 'GetFileVersionInfoW':
            self.log.append(('api', nm, w(a[0]), a[2]))
            if self.fv_ok:
                self.write32(a[3] + 8, self.fv_ms)
                self.write32(a[3] + 0xC, self.fv_ls)
            return self.fv_ok
        if nm == 'VerQueryValueW':
            self.log.append(('api', nm, w(a[1])))
            if self.vq_ok:
                self.write32(a[2], a[0])
                self.write32(a[3], self.vq_len)
            return self.vq_ok
        if nm == 'RegOpenKeyExW':
            self.log.append(('api', nm, a[0], w(a[1]), a[2], a[3]))
            if self.reg_open:
                self.write32(a[4], 0x77)
                return 0
            return 2
        if nm == 'RegQueryValueExW':
            self.log.append(('api', nm, a[0], w(a[1]), a[2], self.read32(a[5])))
            raw = self.reg_value.encode('utf-16le') + bytes(2)
            self.write32(a[3], self.reg_type)
            n = min(len(raw), self.read32(a[5]))
            self.uc.mem_write(a[4], raw[:n])
            self.write32(a[5], len(raw))
            return 0
        if nm == 'RegCloseKey':
            self.log.append(('api', nm, a[0]))
            return 0
        if nm == 'StrCpyNW':
            self.log.append(('api', nm, w(a[1]), a[2]))
            t = (w(a[1]) or '')[:max(a[2] - 1, 0)]
            self.uc.mem_write(a[0], t.encode('utf-16le') + bytes(2))
            return a[0]
        if nm == 'PathAppendW':
            self.log.append(('api', nm, w(a[0]), w(a[1])))
            cur = w(a[0]) or ''
            new = cur + ('' if cur.endswith(chr(92)) else chr(92)) + (w(a[1]) or '')
            self.uc.mem_write(a[0], new.encode('utf-16le') + bytes(2))
            return 1
        if nm == 'LocalAlloc':
            self.log.append(('api', nm, a[0], a[1]))
            p = self.alloc(max(a[1], 16))
            self.uc.mem_write(p, bytes(max(a[1], 16)))
            return p
        if nm == 'LocalFree':
            self.log.append(('api', nm))
            return 0
        raise RuntimeError('api ' + nm)

    def do_import(self, nm):
        if nm == '??0FBlowFish@@QAE@XZ':
            return self.this()
        if nm == '?InitializeBlowfish@FBlowFish@@QAEFQAEF@Z':            # (key, length): the key is remembered in the object (the schedule)
            t, key, ln = self.this(), self.arg(0), self.arg(1)
            kb = bytes(self.uc.mem_read(key, ln))
            self.log.append(('blow_init', kb, ln))
            self.write32(t, sum(kb) & 0xFFFFFFFF)
            return None
        if nm == '?BlowfishDecrypt@FBlowFish@@QAEXPAEH@Z':               # (buffer, length): a keyed byte transform
            t, buf, ln = self.this(), self.arg(0), self.arg(1)
            k = self.read32(t)
            raw = bytes(self.uc.mem_read(buf, ln))
            out = bytes((b ^ ((k + i * 7) & 0xFF)) for i, b in enumerate(raw))
            self.log.append(('blow_dec', raw, ln))
            self.uc.mem_write(buf, out)
            return None
        if nm in ('GetFileVersionInfoSizeW', 'GetFileVersionInfoW', 'VerQueryValueW', 'RegOpenKeyExW', 'RegQueryValueExW', 'RegCloseKey', 'StrCpyNW', 'PathAppendW',
                  'LocalAlloc', 'LocalFree'):
            n = {'GetFileVersionInfoSizeW': 2, 'GetFileVersionInfoW': 4, 'VerQueryValueW': 4, 'RegOpenKeyExW': 5, 'RegQueryValueExW': 6, 'RegCloseKey': 1, 'StrCpyNW': 3,
                 'PathAppendW': 2, 'LocalAlloc': 2, 'LocalFree': 1}[nm]
            return self.api(nm, [self.arg(i) for i in range(n)])
        if nm == 'wnsprintfW':                      # (buffer, count, format, ...): cdecl
            from stubs import wfmt
            fmt = self.wstr(self.arg(2)) or ''
            self.log.append(('api', nm, fmt, [self.arg(3), self.arg(4), self.arg(5), self.arg(6)], self.arg(1)))
            text = wfmt(self, fmt, [self.arg(3 + i) for i in range(8)])
            text = text[:max(self.arg(1) - 1, 0)]
            self.uc.mem_write(self.arg(0), text.encode('utf-16le') + bytes(2))
            return len(text)
        return DW.do_import(self, nm)


def gen14(r):
    cmds = ['"C:\\Program Files\\MSN Messenger\\msnmsgr.exe"', 'C:\\msn\\msnmsgr.exe', '"C:\\x y\\a.exe" /embedding', 'a', '', '"', 'C:\\' + 'p' * 250 + '.exe']
    return dict(fv_size=r.choice([0, 0x200]), fv_ok=r.choice([0, 1, 1]), vq_ok=r.choice([0, 1, 1]), vq_len=r.choice([0, 0x34]),
                ms=r.choice([0x00060002, 0x00060001, 0x0006000A, 0x00050000, 0]), ls=r.choice([0x00000000 | 205, 0x00CD0000 | 1, 0xFFFF0001, 205, 99]),
                reg_open=r.choice([0, 1, 1, 1]), reg_type=r.choice([1, 1, 1, 4, 0]), reg_value=r.choice(cmds), plist=r.choice([0, 1]),
                path=r.choice(['C:\\a.exe', '', 'x']), nullms=r.choice([0, 0, 1]), nullls=r.choice([0, 0, 1]))


def reg(name, sva, size, build, noret=True, opts=None):
    o = {'noret': noret, 'ignore': {0x1017187d}}
    if opts:
        o.update(opts)

    def b(e, c):
        e.write32(e.console + 0x3BF4, e.new_obj('NCWnd', 'Desktop'))
        e.presence = ''
        e.shown = {}
        e.fv_size, e.fv_ok, e.vq_ok, e.vq_len = c['fv_size'], c['fv_ok'], c['vq_ok'], c['vq_len']
        e.fv_ms, e.fv_ls = c['ms'], c['ls']
        e.reg_open, e.reg_type, e.reg_value = c['reg_open'], c['reg_type'], c['reg_value']
        return build(e, c)
    TESTS14[name] = (sva, size, b, gen14, o)


def outbuf(e, n=0x400):
    p = e.alloc(n + 16)
    e.uc.mem_write(p, bytes([0xCD]) * n)
    e.objs[p] = 'Out'
    e.reg_size[p] = 0x100
    return p


def b_getfile(e, c):
    path = e.put_wstr(c['path'])
    ms = e.raw_obj('Ms', 8) if not c['nullms'] else 0
    ls = e.raw_obj('Ls', 8) if not c['nullls'] else 0
    e.w32(ms, 0, 0x1234) if ms else None
    e.w32(ls, 0, 0x4321) if ls else None
    return e.raw_obj('Dummy', 8), [path, ms, ls]


reg('MsnGetFileVersion', 0x10168cf0, 220, b_getfile, noret=False)
def b_getmsgr(e, c):
    e.gfv_on = c['plist'] == 1
    return e.raw_obj('Dummy', 8), [outbuf(e)]


reg('MsnGetMessengerVersion', 0x10169130, 422, b_getmsgr, noret=False)


def b_check(e, c):
    e.gfv_on = c['plist'] == 1
    e.write32(e.glob(0x10322c68), 0x55)
    return e.raw_obj('Dummy', 8), []


def b_decipher(e, c):
    return e.raw_obj('Dummy', 8), [outbuf(e, 0x2100), outbuf(e, 0x2100)]


reg('MsnDecipherProduct', 0x10169010, 286, b_decipher)
reg('MsnCheckVersion', 0x10156160, 55, b_check, opts={'post': lambda e, t, a: e.read32(e.glob(0x10322c68))})

# ---- the small leftovers: the group's getters, the move dialog's person, the pointer arrays' copy constructors -------------------------------------------------
import t_msn3
from t_msn10 import friend_obj, gen10


def gen_small(r):
    d = gen14(r)
    d.update(gen10(r))
    d.update(npt=r.choice([0, 1, 3, 6]))
    return d


def regs(name, sva, size, build, noret=True, opts=None):
    o = {'noret': noret}
    if opts:
        o.update(opts)

    def b(e, c):
        e.write32(e.console + 0x3BF4, e.new_obj('NCWnd', 'Desktop'))
        e.presence = ''
        e.shown = {}
        return build(e, c)
    TESTS14[name] = (sva, size, b, gen_small, o)


def group14(e, c):
    g = e.raw_obj('Group', 0x1C)
    e.fs_set(g, c['pname'])
    e.fs_set(g + 0xC, 'old')
    e.w32(g, 0x18, c['gid'])
    return g


regs('NMsnGroup.GetName', 0x10152860, 78, lambda e, c: (group14(e, c), []), noret=False)
regs('NMsnGroup.GetId', 0x101528b0, 4, lambda e, c: (group14(e, c), []), noret=False)


def b_setperson(e, c):
    d = e.raw_obj('Dialog', 0x240)
    p = friend_obj(e, 'Old', dict(c, acct='old', pname='old', pdisp='', pstatus='FLN', gid=9))
    e.uc.mem_write(d + 0x1F8, bytes(e.uc.mem_read(p, 0x44)))
    new = friend_obj(e, 'New', c)
    return d, [e.read32(new + 4 * i) for i in range(0x11)]


regs('NMoveGroupDialogSetter.SetPerson', 0x10156250, 79, b_setperson, opts={'objmask': {}})


def ptr_array(e, c):
    n = c['npt']
    arr = e.raw_obj('SrcArr', 0xC)
    data = e.alloc(4 * max(n, 1) + 16)
    for i in range(n):
        e.write32(data + 4 * i, 0x6000 + 0x10 * i)
    e.write32(arr, data if n else 0)
    e.write32(arr + 4, n)
    e.write32(arr + 8, n)
    return arr


def post_ptrs(e, this, args):
    data, n = e.read32(this), e.read32(this + 4)
    return [e.read32(data + 4 * i) for i in range(n)], e.read32(this + 8) >= n


for nm, va in (('TMsnStrPtrArray.copy', 0x10153370), ('NTVItemArray.copy', 0x101546c0)):
    regs(nm, va, 134, lambda e, c: (poison_obj(e, 'DstArr', 0xC), [ptr_array(e, c)]), noret=False, opts={'post': post_ptrs})

if __name__ == '__main__':
    t_msn1.run_tests(TESTS14, VW14, sys.argv)
