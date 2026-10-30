# STALE (C4, sa-emu agent `old`): the C4 macro window painters are in win/t_oldwnd.py; NMacroEntry::Set / PushLines have no C4 successor (C2 model: int geometry, C2 offsets; C4 geometry is FLOAT); does not run.
"""Differential test of the macro windows' code (stock vs ours under unicorn): the OnPaint of NCMacroInfoWnd, NCMacroListWnd,
NCMacroEditWnd, NCMacroArrayWnd and NCMacroEditList, NCMacroArrayWnd::HitCell, NMacroEntry::Set, NCMacroEditList::ValidateLines
and NCMacroEditWnd::PushLines. Random windows (positions, sizes, textures, macros, short names, command lines) are run by both and
the logs of the stubbed services (DrawText, DrawTexture, the parser, the request's values ...), the device fields and the objects
written are compared.
usage: python tools/port/emu/t_macro.py <cases> <seed>   (needs build/bin/Release/NWindowDecomp.dll and its .map; NWD_OURS=<dir> tests a snapshot)"""
import sys, random, os, struct, re
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import emu as _emu
import stubs
from stubs import *
import t_recipe
from t_recipe import RecipeEmu, sym, rstr

# more services: the big digits of the line numbers (NCGDevice::DrawSpecialDigit), FString::Mid, the command parser (a model: the
# tokens of the line), the command table (GetCommandType: "/cN" is command N, "//" and "///" the builder codes), the skill table
# (GetMSCommandData: a name starting with B is unknown), L2ParamStack::PushBack (logged), appMemset
stubs.STOCK_FN.update({0x1000fb40: 'DrawSpecialDigit'})
stubs.FN_POP.update({'DrawSpecialDigit': 0x14, 'PtInRectV': 0x18, 'OnScrollBarV': 4})
stubs.OURS_FN.update({'DrawSpecialDigit': ('@NCGDevice@@', '?DrawSpecialDigit@')})
stubs.IMPORT_POP.update({'?Mid@FString@@QBE?AV1@HH@Z': 0xC, '??0FParser@@QAE@XZ': 0, '??1FParser@@QAE@XZ': 0,
                         '?SetSperator@FParser@@QAEXPAG@Z': 4, '?SetString@FParser@@QAEPAGPAG@Z': 4,
                         '?GetNextToken@FParser@@QAEPAGXZ': 0, '?GetCommandType@FL2GameData@@QAEHPAG@Z': 4,
                         '?GetMSCommandData@FL2GameData@@QAEPAUFL2MagicSkillCommandData@@PAG@Z': 4,
                         '?PushBack@L2ParamStack@@QAEHPAX@Z': 4, '?appMemset@@YAXPAXHH@Z': 0})


class MacroEmu(RecipeEmu):
    parsers = {}

    def do_import(self, nm):
        t = self.this()
        if nm == '??0FParser@@QAE@XZ':
            self.parsers[t] = []
            return t
        if nm in ('??1FParser@@QAE@XZ', '?SetSperator@FParser@@QAEXPAG@Z'):
            return None
        if nm == '?SetString@FParser@@QAEPAGPAG@Z':
            toks = [x for x in re.split('[ ,\t\n]+', self.wstr(self.arg(0))) if x]
            self.parsers[t] = toks[1:]
            return self.put_wstr(toks[0]) if toks else 0
        if nm == '?GetNextToken@FParser@@QAEPAGXZ':
            rest = self.parsers.get(t, [])
            return self.put_wstr(rest.pop(0)) if rest else 0
        if nm == '?GetCommandType@FL2GameData@@QAEHPAG@Z':
            tok = self.wstr(self.arg(0))
            self.log.append(('GetCommandType', tok))
            if tok.startswith('//'):
                return 0x10000001 if tok.startswith('///') else 0x10000000
            if tok.startswith('/c') and tok[2:].isdigit():
                return int(tok[2:])
            return -1
        if nm == '?GetMSCommandData@FL2GameData@@QAEPAUFL2MagicSkillCommandData@@PAG@Z':
            name = self.wstr(self.arg(0))
            self.log.append(('GetMSCommandData', name))
            if name.startswith('B'):
                return 0
            rec = self.alloc(16)
            self.write32(rec, 1000 + len(name))
            return rec
        if nm == '?PushBack@L2ParamStack@@QAEHPAX@Z':
            v = self.arg(0)
            self.log.append(('PushBack', ('ptr', self.wstr(v)) if v >= 0x60000000 else self.rds32(self.reg(UC_X86_REG_ESP) + 4)))
            return 0
        if nm == '?appMemset@@YAXPAXHH@Z':
            dst, c, n = self.arg(0), self.arg(1) & 0xFF, self.arg(2)
            self.uc.mem_write(dst, bytes([c]) * n)
            return None
        if nm == '?Mid@FString@@QBE?AV1@HH@Z':
            ret, start, count = self.arg(0), self.rds32(self.reg(UC_X86_REG_ESP) + 8), self.rds32(self.reg(UC_X86_REG_ESP) + 12)
            s = self.fs_get(t)
            self.fs_set(ret, s[start:start + count] if count >= 0 else s[start:])
            self.log.append(('Mid', start, count))
            return ret
        return super().do_import(nm)

    def do_fn(self, nm):
        a = self.arg
        if nm == 'DrawSpecialDigit':
            self.log.append(('DrawSpecialDigit', [self.rds32(self.reg(UC_X86_REG_ESP) + 4 + 4 * i) for i in range(4)], self.wstr(a(4))))
            return 6
        if nm == 'PtInRectV':
            x, y, rx, ry, rw, rh = [self.rds32(self.reg(UC_X86_REG_ESP) + 4 + 4 * i) for i in range(6)]
            self.log.append(('PtInRect', x, y, rx, ry, rw, rh))
            return 1 if (rx <= x < rx + rw and ry <= y < ry + rh) else 0
        if nm == 'OnScrollBarV':
            self.log.append(('OnScrollBar', self.rds32(self.reg(UC_X86_REG_ESP) + 4)))
            return 1
        return super().do_fn(nm)


def stub_at(e, off, name):
    addr = STUB_BASE + off
    e.uc.mem_write(addr, b'\xC3')
    e.stubs[addr] = ('fn:' + name, None)
    return addr


def code_returning(e, value):
    """a little function `mov eax, value; ret` in the emulated memory (a virtual method with a fixed answer)"""
    a = e.alloc(16)
    e.uc.mem_write(a, b'\xB8' + struct.pack('<I', value & 0xFFFFFFFF) + b'\xC3')
    return a


def geom(e, w, case):
    for off, v in ((0x44, case['sx']), (0x48, case['sy']), (0x4C, case['w']), (0x50, case['h'])):
        e.write32(w + off, v)


def rgeom(r):
    return {'sx': r.randint(0, 600), 'sy': r.randint(0, 400), 'w': r.randint(100, 300), 'h': r.randint(100, 400)}


def stext(r, n):
    return ''.join(r.choice('ABCDEFGH') for _ in range(r.randint(0, n)))


# ---- NCMacroInfoWnd::OnPaint --------------------------------------------------------------------------------------------
def info_setup(case):
    def setup(e):
        w = e.alloc(0xB0)
        e.uc.mem_write(w, bytes(0xB0))
        geom(e, w, case)
        e.write32(w + 0x98, case['back'])
        return w, [], (lambda e: None)
    return setup


def rand_info_case(r):
    c = rgeom(r)
    c['back'] = r.choice([0, 0x1111])
    return c


# ---- NCMacroListWnd::OnPaint --------------------------------------------------------------------------------------------
def list_setup(case):
    def setup(e):
        w = e.alloc(0x6100)
        e.uc.mem_write(w, bytes(0x6100))
        geom(e, w, case)
        for off, k in ((0x98, 'back'), (0x9C, 'edit'), (0xA0, 'del'), (0xA4, 'dx'), (0xA8, 'dy'), (0xB4, 'ex'), (0xB8, 'ey'), (0xC8, 'num')):
            e.write32(w + off, case[k])
        return w, [], (lambda e: None)
    return setup


def rand_list_case(r):
    c = rgeom(r)
    c.update({'back': r.choice([0, 0x1111]), 'edit': r.choice([0, 0x2222]), 'del': r.choice([0, 0x3333]), 'dx': r.randint(0, 200),
              'dy': r.randint(0, 300), 'ex': r.randint(0, 200), 'ey': r.randint(0, 300), 'num': r.randint(0, 24)})
    return c


# ---- NCMacroEditWnd::OnPaint --------------------------------------------------------------------------------------------
def edit_setup(case):
    def setup(e):
        ml = e.alloc(0x6100)
        e.uc.mem_write(ml, bytes(0x6100))
        e.write32(ml + 0x6078, case['sel'])
        e.write_wstr_at(ml + 0x134 + case['sel'] * 0x3FC, case['short'])
        e.write32(e.desktop + 0xF8, ml if case['list'] else 0)
        w = e.alloc(0xD0)
        e.uc.mem_write(w, bytes(0xD0))
        geom(e, w, case)
        e.write32(w + 0x98, case['back'])
        arr = e.alloc(28)
        for i in range(7):
            e.write32(arr + 4 * i, 0x7000 + i)
        e.write32(w + 0xA0, arr)
        e.write32(w + 0xA4, 7)
        e.write32(w + 0xA8, 7)
        e.write32(w + 0xBC, case['icon'])
        return w, [], (lambda e: None)
    return setup


def rand_edit_case(r):
    c = rgeom(r)
    c.update({'sel': r.randint(0, 23), 'short': stext(r, 5), 'list': r.random() < 0.9, 'back': r.choice([0, 0x1111]), 'icon': r.randint(0, 6)})
    return c


# ---- NCMacroArrayWnd::OnPaint -------------------------------------------------------------------------------------------
def array_setup(case):
    def setup(e):
        ml = e.alloc(0x6100)
        e.uc.mem_write(ml, bytes(0x6100))
        ents = []
        for m in case['macros']:
            if m is None:
                ents.append(0)
                continue
            p = e.alloc(0xA60)
            e.uc.mem_write(p, bytes(0xA60))
            e.write32(p + 0x5B8, m['icon'])
            e.write_wstr_at(p + 0xA48, m['short'])
            ents.append(p)
        d = e.alloc(4 * max(1, len(ents)))
        for i, p in enumerate(ents):
            e.write32(d + 4 * i, p)
        e.write32(ml + 0xC4, d)
        e.write32(ml + 0xC8, len(ents))
        e.write32(ml + 0xCC, len(ents))
        pe_ = e.alloc(0x10)
        e.uc.mem_write(pe_, bytes(0x10))
        e.write32(pe_ + 4, ml)
        ent = e.alloc(0x10)
        e.uc.mem_write(ent, bytes(0x10))
        e.write32(ent + 0xC, pe_)
        w = e.alloc(0x120)
        e.uc.mem_write(w, bytes(0x120))
        geom(e, w, case)
        e.write32(w + 0x58, ent)
        for off, k in ((0xF0, 'cols'), (0xF4, 'rows'), (0xF8, 'top'), (0x104, 'cell')):
            e.write32(w + off, case[k])
        return w, [], (lambda e: None)
    return setup


def rand_array_case(r):
    c = rgeom(r)
    n = r.randint(0, 30)
    c.update({'cols': r.choice([3, 6]), 'rows': r.choice([2, 4, 7]), 'top': r.randint(0, 2), 'cell': r.choice([32, 34]),
              'macros': [None if r.random() < 0.1 else {'icon': r.choice([0, 0x4444]), 'short': stext(r, 5)} for _ in range(n)]})
    return c


# ---- NCMacroEditList::OnPaint -------------------------------------------------------------------------------------------
def elist_setup(case):
    def setup(e):
        w = e.alloc(0x500)
        e.uc.mem_write(w, bytes(0x500))
        geom(e, w, case)
        vt = e.alloc(0x400)
        e.uc.mem_write(vt, bytes(0x400))
        e.write32(vt + 0x114, e.onscroll_stub)
        e.write32(w, vt)
        edits = []
        for i in range(12):
            o = e.alloc(0x200)
            e.uc.mem_write(o, bytes(0x200))
            ov = e.alloc(0x400)
            e.uc.mem_write(ov, bytes(0x400))
            e.write32(ov + 0x70, code_returning(e, 1 if i == case['focus'] else 0))
            e.write32(o, ov)
            edits.append(o)
        d = e.alloc(48)
        for i, o in enumerate(edits):
            e.write32(d + 4 * i, o)
        e.write32(w + 0xD0, d)
        e.write32(w + 0xD4, 12)
        e.write32(w + 0xD8, 12)
        e.write32(w + 0xE0, case['top'])
        return w, [], (lambda e: [e.read32(w + 0xE0)])
    return setup


def rand_elist_case(r):
    c = rgeom(r)
    c.update({'focus': r.randint(-1, 11), 'top': r.randint(0, 4)})
    return c


def elist_pre(e, kind):
    e.onscroll_stub = stub_at(e, 0x9200, 'OnScrollBarV')


# ---- NCMacroArrayWnd::HitCell -------------------------------------------------------------------------------------------
def hit_setup(case):
    def setup(e):
        w = e.alloc(0x120)
        e.uc.mem_write(w, bytes(0x120))
        geom(e, w, case)
        vt = e.alloc(0x400)
        e.uc.mem_write(vt, bytes(0x400))
        e.write32(vt + 0xA4, e.ptinrect_stub)
        e.write32(w, vt)
        return w, [case['x'], case['y']], (lambda e: None)
    return setup


def rand_hit_case(r):
    c = rgeom(r)
    c['x'] = c['sx'] + r.randint(-5, 270)
    c['y'] = c['sy'] + r.randint(-5, 270)
    return c


def hit_pre(e, kind):
    e.ptinrect_stub = stub_at(e, 0x9300, 'PtInRectV')


# ---- NMacroEntry::Set ---------------------------------------------------------------------------------------------------
def set_setup(case):
    def setup(e):
        p = e.alloc(0xA60)
        e.uc.mem_write(p, bytes(0xA60))
        info = bytearray(0x3F8)

        def put(off, s, n):
            b = s.encode('utf-16-le')[:2 * (n - 1)]
            info[off:off + len(b)] = b
        put(0, case['name'], 13)
        put(0x1A, case['desc'], 33)
        put(0x5C, case['short'], 5)
        info[0x66] = case['icon'] & 0xFF
        info[0x67] = case['nlines'] & 0xFF
        for i, ln in enumerate(case['lines']):
            o = 0x68 + 0x4C * i
            info[o] = ln['no'] & 0xFF
            info[o + 1] = ln['type'] & 0xFF
            info[o + 4:o + 8] = struct.pack('<i', ln['a'])
            info[o + 8] = ln['b'] & 0xFF
            tb = ln['text'].encode('utf-16-le')[:64]
            info[o + 0xA:o + 0xA + len(tb)] = tb
        words = list(struct.unpack('<254I', bytes(info)))
        return p, [case['id']] + words, (lambda e: bytes(e.uc.mem_read(p + 0x78, 0xA54 - 0x78)).hex())
    return setup


def rand_set_case(r):
    n = r.randint(0, 12)
    return {'id': r.randint(1, 99999), 'name': stext(r, 12), 'desc': stext(r, 32), 'short': stext(r, 4), 'icon': r.randint(0, 6),
            'nlines': n, 'lines': [{'no': r.randint(0, 12), 'type': r.randint(0, 7), 'a': r.randint(-3, 900), 'b': r.randint(0, 20),
                                    'text': stext(r, 30)} for _ in range(12)]}


# ---- NCMacroEditList::ValidateLines and NCMacroEditWnd::PushLines -------------------------------------------------------
def lines_env(e, case):
    """an editor list (+0xD0 twelve boxes whose CommitText answers a fixed text, +0xE8 the parser) and the shortcuts' array"""
    e.parsers = {}
    sc = e.alloc(0x210)
    e.uc.mem_write(sc, bytes(0x210))
    arr = e.alloc(0x1A7C * 100)
    e.uc.mem_write(arr, bytes(0x1A7C * 100))
    for i, ty in enumerate(case['sctypes']):
        e.write32(arr + 0x1A7C * i + 0x190C, ty)
    e.write32(sc + 0x1AC, arr)
    e.write32(e.desktop + 0x104, sc)
    lst = e.alloc(0x500)
    e.uc.mem_write(lst, bytes(0x500))
    edits = []
    for t in case['texts']:
        o = e.alloc(0x200)
        e.uc.mem_write(o, bytes(0x200))
        ov = e.alloc(0x400)
        e.uc.mem_write(ov, bytes(0x400))
        e.write32(ov + 0x110, code_returning(e, e.put_wstr(t) if t is not None else 0))
        e.write32(o, ov)
        edits.append(o)
    d = e.alloc(48)
    for i, o in enumerate(edits):
        e.write32(d + 4 * i, o)
    e.write32(lst + 0xD0, d)
    e.write32(lst + 0xD4, 12)
    e.write32(lst + 0xD8, 12)
    return lst


def validate_setup(case):
    def setup(e):
        lst = lines_env(e, case)
        return lst, [], (lambda e: None)
    return setup


def push_setup(case):
    def setup(e):
        lst = lines_env(e, case)
        w = e.alloc(0xD0)
        e.uc.mem_write(w, bytes(0xD0))
        e.write32(w + 0x9C, lst)
        params = e.alloc(16)
        return w, [params], (lambda e: None)
    return setup


def rand_text(r):
    k = r.random()
    if k < 0.15:
        return None
    if k < 0.35:
        return r.choice(['hello', 'good luck', 'x'])
    n = r.choice([0, 1, 4, 5, 12, 13, 14, 15, 21, 24, 25, 26, 27, 28, 29, 0x2D, 0x2E, 0x33, 0x34, 0x4B, 0x4C, 0x4D, 0x51, 0x52, 0x53])
    args = r.choice(['', ' 3', ' 0', ' 3 4', ' 11 2', ' 5 5', ' 1 10', ' Skill Name', ' Battle Cry', ' xyz'])
    return '/c%d%s' % (n, args)


def rand_lines_case(r):
    texts = []
    gap = r.random() < 0.3
    n = r.randint(0, 12)
    for i in range(12):
        if i < n:
            texts.append(rand_text(r) or 'filler')
        else:
            texts.append(rand_text(r) if gap and r.random() < 0.3 else None)
    return {'texts': texts, 'sctypes': [r.choice([0, 1, 2, 3, 4]) for _ in range(100)]}


def lines_pre(e, kind):
    # hook mode: our code reads the stock globals from the NWBASE area: the two command prefixes (pointers to "/" and the full width slash)
    if kind == 'ours':
        e.write32(NWBASE + 0x161E84, e.put_wstr('/'))
        e.write32(NWBASE + 0x161E88, e.put_wstr(chr(0xFF0F)))


# ---- the runner ---------------------------------------------------------------------------------------------------------
def run_plain(e, setup, fn):
    e.heap_ptr = e.heap_mark
    e.log.clear()
    this, args, probe = setup(e)
    e.log.clear()
    ret = e.call(fn, this=this, args=args)
    return (ret if getattr(e, 'check_ret', True) else None, list(e.log), probe(e))


def run_one(label, n, r, mk_case, setup_fn, stock_fn, ours_pre, pre=None, plain=False, check_ret=True):
    cases = [mk_case(r) for _ in range(n)]
    res = {}
    for kind in ('stock', 'ours'):
        e = MacroEmu(kind)
        e.check_ret = check_ret
        if pre:
            pre(e, kind)
        e.heap_mark = e.heap_ptr
        fn = stock_fn if kind == 'stock' else sym(e, ours_pre)
        res[kind] = []
        for c in cases:
            try:
                res[kind].append((run_plain if plain else t_recipe.run_generic)(e, setup_fn(c), fn))
            except Exception as ex:
                if os.environ.get('NWD_TRACE') and not getattr(e, 'traced', False):
                    import traceback
                    traceback.print_exc()
                    e.traced = True
                res[kind].append(('error', type(ex).__name__))
    bad = 0
    crashes = 0
    for i, (a, b) in enumerate(zip(res['stock'], res['ours'])):
        if a[0] == 'error' and b[0] == 'error':
            crashes += 1
            continue
        if a != b:
            bad += 1
            if bad <= 2:
                print(label, 'case', i, cases[i])
                print(' stock', str(a)[:1200])
                print(' ours ', str(b)[:1200])
    print(label, 'cases', n, 'differ', bad, '(both crash: %d)' % crashes)
    return bad


def main(n, seed):
    r = random.Random(seed)
    bad = 0
    bad += run_one('InfoWnd OnPaint', n, r, rand_info_case, info_setup, 0x1008EA90, '?OnPaint@NCMacroInfoWnd@@UAEHPAVNCGDevice@@@Z')
    bad += run_one('ListWnd OnPaint', n, r, rand_list_case, list_setup, 0x1008C810, '?OnPaint@NCMacroListWnd@@UAEHPAVNCGDevice@@@Z')
    bad += run_one('EditWnd OnPaint', n, r, rand_edit_case, edit_setup, 0x1008DBD0, '?OnPaint@NCMacroEditWnd@@UAEHPAVNCGDevice@@@Z')
    bad += run_one('ArrayWnd OnPaint', n, r, rand_array_case, array_setup, 0x1008CD70, '?OnPaint@NCMacroArrayWnd@@UAEHPAVNCGDevice@@@Z')
    bad += run_one('EditList OnPaint', n, r, rand_elist_case, elist_setup, 0x1008F2C0, '?OnPaint@NCMacroEditList@@UAEHPAVNCGDevice@@@Z',
                   pre=elist_pre)
    bad += run_one('ArrayWnd HitCell', n, r, rand_hit_case, hit_setup, 0x1008CAD0, '?HitCell@NCMacroArrayWnd@@', pre=hit_pre, plain=True)
    bad += run_one('NMacroEntry Set', n, r, rand_set_case, set_setup, 0x1008B380, '?Set@NMacroEntry@@', plain=True, check_ret=False)
    bad += run_one('EditList ValidateLines', n, r, rand_lines_case, validate_setup, 0x1008D9F0, '?ValidateLines@NCMacroEditList@@',
                   pre=lines_pre, plain=True)
    bad += run_one('EditWnd PushLines', n, r, rand_lines_case, push_setup, 0x1008DF00, '?PushLines@NCMacroEditWnd@@', pre=lines_pre,
                   plain=True, check_ret=False)
    return bad


if __name__ == '__main__':
    sys.exit(1 if main(int(sys.argv[1]), int(sys.argv[2])) else 0)
