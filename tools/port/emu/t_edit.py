# STALE (C4, sa-emu agent `old`): covered by win/e4.py (the C4 NCEditBox test adapted from this one); the C2 addresses and layouts below do not run.
# Differential test of NCEditBox's text editing (OnKeyDown, OnChar, InsertText, DeleteChar, ReformatNumber): the stock
# function and ours run side by side under unicorn on random edit box states, in the Korean/English clients and in the
# Thai one (GLanguageType 5), with the engine, the console and the Thai splitter stubbed alike on both sides (the
# service calls and their arguments are logged and compared, and so is every field the edit box owns afterwards).
# Written by the w2edit lane; see docs/notas/C2_Porte.md.
#   python tools/port/emu/t_edit.py [cases] [seed]
import sys, os, random
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import stubs
from stubs import *

STOCK_X = {0x10051BB0: 'GetIMEType', 0x10012C30: 'IsNotComposing', 0x10005820: 'NotifyParent',
           0x1002B980: 'WndOnKeyDown', 0x1000FBE0: 'MakeThai', 0x1000B190: 'ThaiWidth', 0x100E7A71: 'swprintf', 0x1000F6D0: 'DrawStretched9'}
OURS_X = {'GetIMEType': '?GetIMEType@NConsoleWnd@@QAEHXZ', 'IsNotComposing': '?IsNotComposing@NCIMEManager@@QAEHXZ',
          'NotifyParent': '?NotifyParent@NControl@@QAEHJ@Z', 'WndOnKeyDown': '?OnKeyDown@NCWnd@@UAEHIJ@Z',
          'MakeThai': '?MakeThaiCharArray@NCGDevice@@SAXPBGAAV?$TArray@UThaiCharacter@@@@PAG@Z',
          'ThaiWidth': '?GetWidth@ThaiCharacter@@QBEHPAVUCanvas@@H@Z',
          'DrawStretched9': '?DrawStretchedTex@NCGDevice@@QAEXHHHHHHHHPAVUTexture@@@Z'}
stubs.FN_POP.update({'GetIMEType': 0, 'IsNotComposing': 0, 'NotifyParent': 4, 'WndOnKeyDown': 8, 'MakeThai': 0,
                     'ThaiWidth': 8, 'swprintf': 0, 'DrawStretched9': 0x24})
stubs.IMPORT_POP.update({
    '?Mid@FString@@QBE?AV1@HH@Z': 12, '?Left@FString@@QBE?AV1@H@Z': 8, '?Right@FString@@QBE?AV1@H@Z': 8,
    '?Delete@FString@@QAEXHH@Z': 8, '??HFString@@QAE?AV0@ABV0@@Z': 8, '??HFString@@QAE?AV0@PBG@Z': 8,
    '??8FString@@QBEHPBG@Z': 4, '__stdio_common_vswprintf': 0, '?appMemset@@YAXPAXHH@Z': 0, '?appMemmove@@YAPAXPAXPBXH@Z': 0,
    '?Insert@FArray@@QAEXHHH@Z': 12, '?AddZeroed@FArray@@QAEHHH@Z': 8})

NCEDIT = {'paint': 0x1000CAD0, 'keydown': 0x1000D860, 'char': 0x1000C740, 'insert': 0x1000C250, 'delete': 0x1000C470,
          'reformat': 0x1000A1B0}
OURS_EDIT = {'paint': '?OnPaint@NCEditBox@@UAEHPAVNCGDevice@@@Z', 'keydown': '?OnKeyDown@NCEditBox@@UAEHIJ@Z', 'char': '?OnChar@NCEditBox@@UAEHIJ@Z',
             'insert': '?InsertText@NCEditBox@@QAEXPBGH@Z', 'delete': '?DeleteChar@NCEditBox@@QAEXH@Z',
             'reformat': '?ReformatNumber@NCEditBox@@QAEXXZ'}

_stub_wfmt = wfmt


def wfmt(emu, fmt, args):
    """stubs.wfmt plus %c (a wide character), which the edit box's OnChar uses"""
    if '%c' in fmt and fmt.replace('%c', '', 1).find('%') < 0:
        return fmt.replace('%c', chr(args[0] & 0xFFFF), 1)
    return _stub_wfmt(emu, fmt, args)


THAI_MARKS = set([0xE31] + list(range(0xE34, 0xE3B)) + list(range(0xE47, 0xE4F)))
F = dict(Width=0x4C, Height=0x50, bFocus=0x40, IMEManager=0x1A4, Type=0x1C4, U1C8=0x1C8, U1CC=0x1CC, bCanInsert=0x1D0,
         bCaretOn=0x1D4, U1D8=0x1D8, ViewStart=0x1F4, ExtraWidth=0x1F8, MaxWidth=0x1FC, ViewStart2=0x200, MaxLen=0x204,
         Left=0x208, Right=0x214, ThaiLeft=0x220, ThaiRight=0x22C, Comp=0x238, Reading=0x244, CaretX=0x250)


def thai_split(text):
    out = []
    for c in text:
        if out and ord(c) in THAI_MARKS and len(out[-1]) < 6:
            out[-1] += c
        else:
            out.append(c)
    return out


def cluster_words(cl):
    w = [0] * 10
    if len(cl) == 1:
        w[5] = ord(cl[0])
    else:
        w[1] = ord(cl[0])
        for k, ch in enumerate(cl[1:]):
            w[[2, 3, 0, 4, 6][k]] = ord(ch)
    return w


def char_w(c):
    o = ord(c)
    if o in THAI_MARKS:
        return 0
    if o >= 0xE00:
        return 7
    return 12 if (o >= 0x80 or c in 'MW@') else 6


class EditEmu(TipEmu):
    def __init__(self, kind):
        super().__init__(kind)
        self.ime_type = 1
        self.not_composing = 1
        self.langvar = self.alloc(16)
        for e in self.pe.DIRECTORY_ENTRY_IMPORT:
            for imp in e.imports:
                nm = imp.name.decode() if imp.name else ''
                if nm == '?GLanguageType@@3HA':
                    self.write32(imp.address, self.langvar)
        fns = {}
        if kind == 'stock':
            fns = dict(STOCK_X)
        else:
            for nm, sym in OURS_X.items():
                c = [v for k, v in self.symbols.items() if k == sym]
                if len(c) != 1:
                    raise RuntimeError('ours symbol %s: %s' % (sym, c))
                fns[c[0]] = nm
        n = 0
        for va, nm in fns.items():
            stub = STUB_BASE + 0x9000 + 16 * n
            n += 1
            self.uc.mem_write(stub, b'\xC3')
            self.stubs[stub] = ('fn:' + nm, None)
            self.uc.mem_write(va, b'\x68' + struct.pack('<I', stub) + b'\xC3')

    def set_lang(self, v):
        self.write32(self.langvar, v)

    # ---- the stubs -------------------------------------------------------------------------------------
    def do_import(self, nm):
        t = self.this()
        a = self.arg
        if nm in ('?Mid@FString@@QBE?AV1@HH@Z', '?Left@FString@@QBE?AV1@H@Z', '?Right@FString@@QBE?AV1@H@Z'):
            s = self.fs_get(t)
            ret = a(0)
            if nm.startswith('?Mid'):
                st, cnt = self.rds32(self.reg(UC_X86_REG_ESP) + 8), self.rds32(self.reg(UC_X86_REG_ESP) + 12)
                st = max(0, min(st, len(s)))
                r = s[st:st + max(0, cnt)] if cnt < 0x7FFFFFF0 else s[st:]
            elif nm.startswith('?Left'):
                c = max(0, min(self.rds32(self.reg(UC_X86_REG_ESP) + 8), len(s)))
                r = s[:c]
            else:
                c = max(0, min(self.rds32(self.reg(UC_X86_REG_ESP) + 8), len(s)))
                r = s[len(s) - c:]
            self.fs_set(ret, r)
            self.log.append((nm.split('@')[0], s, [self.rds32(self.reg(UC_X86_REG_ESP) + 8 + 4 * i) for i in range(2 if nm.startswith('?Mid') else 1)]))
            return ret
        if nm == '?Delete@FString@@QAEXHH@Z':
            s = self.fs_get(t)
            i, c = self.rds32(self.reg(UC_X86_REG_ESP) + 4), self.rds32(self.reg(UC_X86_REG_ESP) + 8)
            self.log.append(('Delete', s, i, c))
            if 0 <= i <= len(s) and c >= 0:
                self.fs_set(t, s[:i] + s[i + c:])
            return None
        if nm in ('??HFString@@QAE?AV0@ABV0@@Z', '??HFString@@QAE?AV0@PBG@Z'):
            ret = a(0)
            o = self.fs_get(a(1)) if 'ABV0' in nm else (self.wstr(a(1)) if a(1) else '')
            self.fs_set(ret, self.fs_get(t) + o)
            return ret
        if nm == '??8FString@@QBEHPBG@Z':
            return 1 if self.fs_get(t).lower() == self.wstr(a(0)).lower() else 0
        if nm == '__stdio_common_vswprintf':
            buf, fmt, argp = a(2), self.wstr(a(4)), a(6)
            args = [self.read32(argp + 4 * i) for i in range(4)]
            s = wfmt(self, fmt, args)
            self.write_wstr_at(buf, s)
            return len(s)
        return super().do_import(nm)

    def do_fn(self, nm):
        t = self.this()
        a = self.arg
        if nm == 'GetTextExtent':
            text = self.wstr(a(1))
            self.write32(a(0), sum(char_w(c) for c in text))
            self.write32(a(0) + 4, 13)
            self.log.append(('GetTextExtent', text))
            return 0
        if nm == 'GetIMEType':
            return self.ime_type
        if nm == 'IsNotComposing':
            return self.not_composing
        if nm == 'NotifyParent':
            self.log.append(('NotifyParent', a(0)))
            return 1
        if nm == 'WndOnKeyDown':
            self.log.append(('WndOnKeyDown', a(0), a(1)))
            return 1
        if nm == 'MakeThai':
            text = self.wstr(a(0))
            arr = a(1)
            cl = thai_split(text)
            data = self.alloc(max(1, len(cl)) * 20)
            for i, c in enumerate(cl):
                self.uc.mem_write(data + 20 * i, struct.pack('<10H', *cluster_words(c)))
            self.write32(arr, data); self.write32(arr + 4, len(cl)); self.write32(arr + 8, len(cl))
            self.log.append(('MakeThai', text, len(cl)))
            return None
        if nm == 'ThaiWidth':
            w = struct.unpack('<10H', bytes(self.uc.mem_read(t, 20)))
            r = sum(7 for x in w if x)
            self.log.append(('ThaiWidth', r, a(1)))
            return r
        if nm == 'DrawStretched9':
            self.log.append(('DrawStretched9', [self.rds32(self.reg(UC_X86_REG_ESP) + 4 + 4 * i) for i in range(8)], a(8)))
            return None
        if nm == 'swprintf':
            buf, fmt = a(0), self.wstr(a(1))
            s = wfmt(self, fmt, [self.stack_arg(2 + i) for i in range(4)])
            self.write_wstr_at(buf, s)
            return len(s)
        return super().do_fn(nm)


def addrs(e):
    if e.kind == 'stock':
        return NCEDIT
    out = {}
    for k, sym in OURS_EDIT.items():
        c = [v for s, v in e.symbols.items() if s == sym]
        assert len(c) == 1, (k, c)
        out[k] = c[0]
    return out


def vtable(e):
    if e.kind == 'stock':
        return 0x1010A418
    return [v for s, v in e.symbols.items() if s == '??_7NCEditBox@@6B@'][0]


def make_edit(e, st):
    p = e.alloc(0x270)
    e.uc.mem_write(p, bytes(0x270))
    e.write32(p, vtable(e))
    mgr = e.alloc(0x1C0)
    e.uc.mem_write(mgr, bytes(0x1C0))
    for k in ('Width', 'Height', 'bFocus', 'Type', 'U1C8', 'U1CC', 'bCanInsert', 'bCaretOn', 'U1D8', 'ViewStart',
              'ExtraWidth', 'MaxWidth', 'ViewStart2', 'MaxLen', 'CaretX'):
        e.write32(p + F[k], st[k] & 0xFFFFFFFF)
    e.write32(p + F['IMEManager'], mgr)
    e.write32(p + 0x44, st.get('ScreenX', 10)); e.write32(p + 0x48, st.get('ScreenY', 20))
    for off in (0x1A8, 0x1AC, 0x1B0, 0x1B4, 0x1B8, 0x1BC, 0x1C0):
        e.write32(p + off, st.get('tex', 0x7000) + off)
    for k in ('Left', 'Right', 'Comp', 'Reading'):
        e.fs_set(p + F[k], st[k])
    if st['lang'] == 5:
        for k, tk in (('Left', 'ThaiLeft'), ('Right', 'ThaiRight')):
            cl = thai_split(st[k])
            data = e.alloc(max(1, len(cl)) * 20)
            for i, c in enumerate(cl):
                e.uc.mem_write(data + 20 * i, struct.pack('<10H', *cluster_words(c)))
            e.write32(p + F[tk], data); e.write32(p + F[tk] + 4, len(cl)); e.write32(p + F[tk] + 8, len(cl))
    return p


def dump(e, p):
    d = {}
    for k in ('Left', 'Right', 'Comp', 'Reading'):
        d[k] = e.fs_get(p + F[k])
    for k in ('Type', 'bCanInsert', 'ViewStart', 'MaxWidth', 'ViewStart2', 'MaxLen', 'CaretX', 'bCaretOn'):
        d[k] = e.rds32(p + F[k])
    for k in ('ThaiLeft', 'ThaiRight'):
        data, num = e.read32(p + F[k]), e.read32(p + F[k] + 4)
        d[k] = [struct.unpack('<10H', bytes(e.uc.mem_read(data + 20 * i, 20))) for i in range(num)] if data else []
    return d


def run_case(e, A, c):
    mark = len(e.log)
    e.set_lang(c['lang'])
    e.ime_type = c['ime']
    e.not_composing = c['nc']
    p = make_edit(e, c['st'])
    op = c['op']
    if op == 'key':
        r = e.call(A['keydown'], this=p, args=[c['vk'], c['lp']])
    elif op == 'char':
        r = e.call(A['char'], this=p, args=[c['ch'], 0])
    elif op == 'insert':
        r = e.call(A['insert'], this=p, args=[e.put_wstr(c['text']), c['app']])
    elif op == 'delete':
        r = e.call(A['delete'], this=p, args=[c['idx'] & 0xFFFFFFFF])
    elif op == 'paint':
        dev = e.alloc(0x6C)
        e.uc.mem_write(dev, bytes(0x6C))
        r = e.call(A['paint'], this=p, args=[dev])
        c_dev = [e.rds32(dev + o) for o in (0x2C, 0x30, 0x48, 0x4C, 0x5C)]
        return {'ret': r, 'state': dump(e, p), 'log': e.log[mark:], 'dev': c_dev}
    else:
        r = e.call(A['reformat'], this=p)
    return {'ret': r if op in ('key', 'char') else None, 'state': dump(e, p), 'log': e.log[mark:]}


def rand_text(r, lang, digits=False, wide=True):
    if digits:
        alpha = '0123456789,'
    elif lang == 5:
        alpha = 'ab ' + ''.join(chr(x) for x in [0xE01, 0xE02, 0xE14, 0xE32, 0xE33, 0xE31, 0xE34, 0xE48, 0xE40, 0xE50])
    else:
        alpha = 'abcMW 01,' + ('가각' if wide else '')
    return ''.join(r.choice(alpha) for _ in range(r.choice([0, 0, 1, 2, 3, 5, 8, 12, 20])))


def rand_case(r):
    lang = r.choice([0, 1, 5, 5])
    typ = r.choice([0, 0, 1, 2, 3, 3])
    digits = typ == 3
    st = dict(Width=r.choice([30, 60, 100, 160]), Height=16, bFocus=r.randint(0, 1), Type=typ,
              U1C8=1 if lang == 1 else r.choice([0, 0, 1]), U1CC=1, bCanInsert=r.randint(0, 1), bCaretOn=r.randint(0, 1),
              U1D8=0, ViewStart=0, ExtraWidth=r.choice([0, 0, 20]), MaxWidth=0, ViewStart2=0,
              MaxLen=r.choice([0, 0, 0, 5, 12]), CaretX=r.randint(0, 120),
              Left=rand_text(r, lang, digits), Right=rand_text(r, lang, digits), Comp=rand_text(r, lang, False)[:3],
              Reading='', lang=lang)
    ll, rl = len(st['Left']), len(st['Right'])
    st['ViewStart'] = r.choice([0, 0, 0, 1, 2, ll // 2, ll]) if ll else 0
    st['ViewStart2'] = r.choice([0, st['ViewStart'], ll])
    ime = r.choice([1, 1, 9, 10, 13, 6]) if lang == 5 else r.choice([1, 1, 9, 10, 11, 12, 18])
    c = dict(lang=lang, ime=ime, nc=r.choice([1, 1, 1, 0, -1]), st=st)
    op = r.choice(['key', 'key', 'key', 'key', 'char', 'insert', 'delete', 'reformat', 'paint', 'paint'])
    if op == 'paint':
        # the language painters read the IME objects (not made here): only the clients without one
        c['lang'] = st['lang'] = r.choice([1, 5])
        if st['Type'] == 3:
            st['Type'] = r.choice([0, 1, 2])
        st['U1CC'] = r.choice([0, 1, 1])
        st['U1D8'] = r.choice([0, 0, 1])
        st['ScreenX'] = r.randint(0, 300); st['ScreenY'] = r.randint(0, 300)
        st['ExtraWidth'] = r.choice([0, 0, 25])
        st['Reading'] = r.choice(['', '', 'ab'])
    c['op'] = op
    if op == 'reformat' and not (st['Left'] or st['Right']):
        st['Left'] = rand_text(r, lang, True) or '12'
    if op == 'key':
        c['vk'] = r.choice([8, 8, 0xD, 0x23, 0x24, 0x25, 0x25, 0x26, 0x27, 0x27, 0x28, 0x2E, 0x2E, 0xE5, 0x41])
        c['lp'] = r.choice([0, 0, 0x14B0001, 0x14D0001])
    elif op == 'char':
        c['ch'] = r.choice([8, 0x20, 0x41, 0x30, 0x35, 0x22, 0x2C, 0x7F, 0xAC00, 5, 0xE01, 0xE33, 0xE31, 0xE48, 0xE50, 0xE32])
    elif op == 'insert':
        c['text'] = rand_text(r, lang, digits)
        c['app'] = r.randint(0, 1)
    elif op == 'delete':
        c['idx'] = r.randint(-1, ll + rl + len(st['Comp']) + 1)
    return c


def main(n, seed):
    r = random.Random(seed)
    cases = [rand_case(r) for _ in range(n)]
    es = EditEmu('stock')
    eo = EditEmu('ours')
    As, Ao = addrs(es), addrs(eo)
    es.heap_mark, eo.heap_mark = es.heap_ptr, eo.heap_ptr
    bad = 0
    err = 0
    for i, c in enumerate(cases):
        es.heap_ptr, eo.heap_ptr = es.heap_mark, eo.heap_mark
        try:
            a = run_case(es, As, c)
        except Exception as ex:
            a = {'exc': str(ex)[:200]}
        try:
            b = run_case(eo, Ao, c)
        except Exception as ex:
            b = {'exc': str(ex)[:200]}
        if 'exc' in a or 'exc' in b:
            err += 1
            if err <= 4:
                print('--- case %d raised' % i, c['op'], a.get('exc'), '|', b.get('exc'))
        elif a != b:
            bad += 1
            if bad <= 6:
                print('--- case', i, c['op'], ascii({k: v for k, v in c.items() if k != 'st'}))
                print(' state in:', ascii(c['st']))
                for k in a:
                    if a[k] != b[k]:
                        print(' differs:', k)
                        print('  stock:', ascii(a[k])[:900])
                        print('  ours :', ascii(b[k])[:900])
        es.log.clear(); eo.log.clear()
    print('cases', n, 'differ', bad, 'raised', err)


if __name__ == '__main__':
    main(int(sys.argv[1]) if len(sys.argv) > 1 else 500, int(sys.argv[2]) if len(sys.argv) > 2 else 1)
