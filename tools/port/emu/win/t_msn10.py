# Differential tests of the messenger's data layer (wave C, lane proof2): NMsnProtocol.cpp (the contact record NMsnFriend, the group record, the server
# address, the header map Add / Set, the MSG payload parser, the UTF-8 conversion, the list-bit tests); stock against ours under unicorn
# (t_msn1.py / tr.py).
#   python t_msn10.py [cases] [seed] [test,test,...]
import sys, os, random, struct
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import tr
from tr import *
import t_msn1
from t_msn1 import *
import t_msn3
from t_msn3 import person, fsargs
import t_msn5
from t_msn5 import CW, gen5
import t_msn8
from t_msn8 import BW

TESTS10 = {}
# DecodeMsnUrl(in, out) and MsnStrReplace(FString*, FString from, FString to): cdecl, the second with the strings by value
tr.ARGS_OVERRIDE.update({0x10168cc0: 2, 0x10168890: 7, 0x10152c00: 2})
tr.POP_OVERRIDE.update({0x10168cc0: 0, 0x10168890: 0, 0x10152c00: 0})


def crc_table():
    t = []
    for i in range(256):
        c = i
        for _ in range(8):
            c = (c >> 1) ^ 0xEDB88320 if c & 1 else c >> 1
        t.append(c)
    return t


CRC = crc_table()


def strihash(text):
    h = 0
    for ch in text:
        o = ord(ch)
        if 0x61 <= o <= 0x7A:
            o -= 0x20
        h = ((h >> 8) & 0x00FFFFFF) ^ CRC[(h ^ (o & 0xFF)) & 0xFF]
        h = ((h >> 8) & 0x00FFFFFF) ^ CRC[(h ^ (o >> 8)) & 0xFF]
    return h


class DW(BW):
    def __init__(self, kind):
        BW.__init__(self, kind)
        R = self.rules
        # the engine's CRC table (GCRCTable is an array the import points at: the cell of the harness is too small)
        for ee in self.pe.DIRECTORY_ENTRY_IMPORT:
            for imp in ee.imports:
                if imp.name and imp.name.decode() == '?GCRCTable@@3PAKA':
                    cell = self.alloc(1040)
                    self.uc.mem_write(cell, b''.join(struct.pack('<I', v) for v in CRC))
                    self.write32(imp.address, cell)
        R[0x10168cc0] = self.url_decode                                                            # DecodeMsnUrl(in, out)
        R[0x10168890] = self.str_replace                                                           # MsnStrReplace(&str, from, to)
        R[0x10152c00] = self.array_add1                                                            # the pointer array's Add(1, 4) helper
        R[0x1013fba0] = self.rehash                                                                # NMsnHeaderMap::Rehash
        R[0x1014be00] = self.header_new                                                            # NMsnHeaderNew: the map's Pairs.Add(1) (stock, ours inline)
        R[0x1014c100] = lambda e, ecx, a: int((e.wstr(a[0]) or '') in getattr(e, 'line_verbs', []))      # MsnLine::IsAsyncVerb(const FString&)
        R[0x1014c1a0] = lambda e, ecx, a: int((e.wstr(a[0]) or '').isdigit() and len(e.wstr(a[0]) or '') == 3 and e.wstr(a[0])[0] in '2345789')   # MsnLine::IsErrorCode
        self.transparent |= {0x1013e3c0, 0x1014bda0, 0x1014e600, 0x1014be80, 0x1014be20, 0x1014e620}

    def url_decode(self, e, ecx, a):
        t = (e.wstr(a[0]) or '') if a[0] else ''
        e.uc.mem_write(a[1], t.replace('%20', ' ').encode('utf-16le') + bytes(2))
        return 1

    def str_replace(self, e, ecx, a):
        # a = [FString* str, from (data, num, max), to (data, num, max)]
        frm, to = (e.wstr(a[1]) or '') if a[1] else '', (e.wstr(a[4]) or '') if a[4] else ''
        cur = e.fs_get(a[0]) or ''
        if frm:
            e.fs_set(a[0], cur.replace(frm, to))
        return 0

    def array_add1(self, e, ecx, a):
        size, arr = a[0], a[1]
        data, n, mx = e.read32(arr), e.read32(arr + 4), e.read32(arr + 8)
        if n + 1 > mx:
            nd = e.alloc(size * (n + 1 + 3) + 16)
            if n:
                e.uc.mem_write(nd, bytes(e.uc.mem_read(data, size * n)))
            e.write32(arr, nd)
            e.write32(arr + 8, n + 1 + 3)
            data = nd
        e.write32(arr + 4, n + 1)
        return data + n * size

    def header_new(self, e, ecx, a):
        # NMsnHeaderNew(): the map's (ecx) pair array grows by one; the address of the new pair
        return self.array_add1(e, ecx, [0x1C, ecx])

    def rehash(self, e, ecx, a):
        data, n, hc = e.read32(ecx), e.read32(ecx + 4), e.read32(ecx + 0x10)
        h = e.alloc(4 * hc + 16)
        for i in range(hc):
            e.write32(h + 4 * i, 0xFFFFFFFF)
        for i in range(n):
            key = e.fs_get(data + 0x1C * i + 4) or ''
            b = strihash(key) & (hc - 1)
            e.write32(data + 0x1C * i, e.read32(h + 4 * b))
            e.write32(h + 4 * b, i)
        e.write32(ecx + 0xC, h)
        return 0


def friend_obj(e, label, c, kind='f'):
    return person(e, label, name='nick' + c['pname'], status=c['pstatus'], f20=c['acct'], n1=c['pname'], n2=c['pdisp'], gid=c['gid'])


def gen10(r):
    return dict(rs=r.randint(0, 10 ** 6), pname=r.choice(['x@y', 'n%20m', '', 'plain']), pdisp=r.choice(['', 'shown']), pstatus=r.choice(['NLN', 'FLN', 'BSY', '']),
                acct=r.choice(['a@b', 'c@d', '', 'Z@Z']), gid=r.choice([0, 1, 7, -1]), text=r.choice(['new text', '', 'two words', 'a b c']),
                port=r.choice([0, 1863, 80]), lists=r.choice([0, 1, 2, 4, 8, 15, 0xF0, 0x7FFFFFFF]), n=r.choice([0, 1, 3, 6, 12]),
                keys=[r.choice(['Content-Type', 'x-a', 'X-A', 'MIME-Version', 'Invitation-Cookie', 'k%d' % r.randint(0, 40)]) for _ in range(20)],
                vals=[r.choice(['', 'v', 'text/plain', 'val ue']) for _ in range(20)], hc=r.choice([8, 8, 16, 2]),
                key=r.choice(['Content-Type', 'x-a', 'X-A', 'zz', 'k1']), val=r.choice(['v', '', 'new val']),
                utf=r.choice(['plain', '', 'caf\xc3\xa9', '\xe6\x97\xa5\xe6\x9c\xac', 'a' * 40]), nlines=r.choice([0, 1, 2, 3, 4, 6, 9]),
                mime=r.choice(['text/plain', 'text/x-msmsgscontrol', 'text/x-msmsgsprofile', 'text/x-msmsgsemailnotification', 'text/x-msmsgsinvite', 'text/other', 'text/plain; charset=UTF-8']),
                lines=[r.choice(['TypingUser: a@b', 'Invitation-Cookie: 1234', 'X-Foo: bar baz', 'novalue', 'Content-Type: text/plain', 'A: b: c', '', 'body line']) for _ in range(12)],
                mimever=r.choice(['MIME-Version: 1.0', 'MIME-Version:1.0', 'x', '']))


def reg(name, sva, size, build, noret=True, opts=None, gen=gen10):
    o = {'noret': noret}
    if opts:
        o.update(opts)

    def b(e, c):
        e.write32(e.console + 0x3BF4, e.new_obj('NCWnd', 'Desktop'))
        e.presence = ''
        e.shown = {}
        return build(e, c)
    TESTS10[name] = (sva, size, b, gen, o)


def fsv(e, *items):
    """the arguments of a stock function that takes FStrings by value (3 words each on the stack, the callee destroys them): a str item is one such string, any other item
    a plain argument. The standalone's version of such a constructor takes `const FString&` (the caller owns the string): ours is given a pointer to a private copy of the
    three words instead (standalone mode only; the hook build keeps the by-value hook)."""
    out = []
    for it in items:
        if isinstance(it, str):
            w = fsargs(e, it)
            if sa.ENABLED and e.kind == 'ours':
                p = e.alloc(16)
                for i, v in enumerate(w):
                    e.write32(p + 4 * i, v)
                out.append(p)
            else:
                out += w
        else:
            out.append(it)
    return out


def poison_obj(e, label, n):
    p = e.alloc(n + 16)
    e.uc.mem_write(p, bytes([0xCD]) * (n + 16))
    e.objs[p] = label
    e.reg_size[p] = n
    return p


# ---- NMsnFriend -----------------------------------------------------------------------------------------------------------------------
reg('NMsnFriend.ctor1', 0x10150d80, 186, lambda e, c: (poison_obj(e, 'Friend', 0x44), fsv(e, c['acct'])), opts={'alt': '??0NMsnFriend@@QAE@ABVFString@@@Z'})
reg('NMsnFriend.ctor2', 0x10150e40, 218, lambda e, c: (poison_obj(e, 'Friend', 0x44), fsv(e, c['acct'], c['pname'])), opts={'alt': '??0NMsnFriend@@QAE@ABVFString@@0@Z'})
reg('NMsnFriend.copy', 0x10150f20, 205, lambda e, c: (poison_obj(e, 'Friend', 0x44), [friend_obj(e, 'Src', c)]))
reg('NMsnFriend.assign', 0x10151070, 86, lambda e, c: (friend_obj(e, 'Dst', dict(c, acct='old', pname='old', pdisp='o', pstatus='FLN', gid=9)), [friend_obj(e, 'Src', c)]), noret=False)
reg('NMsnFriend.getdisplay', 0x101510d0, 105, lambda e, c: (friend_obj(e, 'Friend', c), [e.raw_obj('Result', 0x10)]), noret=False)
reg('NMsnFriend.setname', 0x10151290, 78, lambda e, c: (friend_obj(e, 'Friend', c), fsargs(e, c['text'])))
reg('NMsnFriend.setstatus', 0x101512e0, 92, lambda e, c: (friend_obj(e, 'Friend', c), fsargs(e, c['text'])))


def b_friend_dtor(e, c):
    p = friend_obj(e, 'Friend', c)
    return p, []


reg('NMsnFriend.dtor', 0x10150ff0, 121, b_friend_dtor)
for nm, va in (('FL', 0x10151170), ('AL', 0x10151180), ('BL', 0x10151190), ('RL', 0x101511a0)):
    reg('MsnListHas' + nm, va, 16, lambda e, c: (e.raw_obj('Dummy', 8), [c['lists'] & 0xFFFFFFFF]), noret=False)


# ---- NMsnGroup / NMsnServer -----------------------------------------------------------------------------------------------------------
def group_obj(e, label, c):
    g = e.raw_obj(label, 0x1C)
    e.fs_set(g, c['text'])
    e.fs_set(g + 0xC, c['pname'])
    e.w32(g, 0x18, c['gid'])
    return g


reg('NMsnGroup.ctor', 0x10152660, 120, lambda e, c: (poison_obj(e, 'Group', 0x1C), fsv(e, c['text'], c['gid'] & 0xFFFFFFFF)), opts={'alt': '??0NMsnGroup@@QAE@ABVFString@@H@Z'})
reg('NMsnGroup.copy', 0x101526e0, 114, lambda e, c: (poison_obj(e, 'Group', 0x1C), [group_obj(e, 'Src', c)]))
reg('NMsnGroup.dtor', 0x10152760, 78, lambda e, c: (group_obj(e, 'Group', c), []))
reg('NMsnGroup.SetName', 0x101527b0, 159, lambda e, c: (group_obj(e, 'Group', c), fsargs(e, c['text'])))
reg('NMsnServer.ctor0', 0x1014be20, 81, lambda e, c: (poison_obj(e, 'Server', 0x10), []))
reg('NMsnServer.ctor2', 0x1014be80, 106, lambda e, c: (poison_obj(e, 'Server', 0x10), fsv(e, c['text'], c['port'])), opts={'alt': '??0NMsnServer@@QAE@ABVFString@@H@Z'})


# ---- the header map --------------------------------------------------------------------------------------------------------------------
def strmap_obj(e, c, label='Map', room=4):
    n, hc = c['n'], c['hc']
    m = e.raw_obj(label, 0x14)
    data = e.alloc(0x1C * (n + room) + 16)
    e.uc.mem_write(data, bytes(0x1C * (n + room) + 16))
    h = e.raw_obj(label + 'Hash', 4 * hc)
    for i in range(hc):
        e.write32(h + 4 * i, 0xFFFFFFFF)
    for i in range(n):
        k = c['keys'][i]
        e.fs_set(data + 0x1C * i + 4, k)
        e.fs_set(data + 0x1C * i + 0x10, c['vals'][i])
        b = strihash(k) & (hc - 1)
        e.write32(data + 0x1C * i, e.read32(h + 4 * b))
        e.write32(h + 4 * b, i)
    e.write32(m, data)
    e.write32(m + 4, n)
    e.write32(m + 8, n + room)
    e.write32(m + 0xC, h)
    e.write32(m + 0x10, hc)
    return m


reg('NMsnStrStrPair.ctor', 0x1014bda0, 85, lambda e, c: (poison_obj(e, 'Pair', 0x1C), [e.put_wstr(c['key']), e.put_wstr(c['val'])]))
reg('NMsnStrStrMap.Add', 0x1014c520, 118, lambda e, c: (strmap_obj(e, c), [e.put_wstr(c['key']), e.put_wstr(c['val'])]), opts={'ignore': {0x1014be00}})   # (the stock's Add(1) helper, ours inline)
reg('NMsnStrStrMap.Set', 0x1014e4d0, 121, lambda e, c: (strmap_obj(e, c), [e.put_wstr(c['key']), e.put_wstr(c['val'])]))


# ---- the MSG payload parser -----------------------------------------------------------------------------------------------------------
def message_obj(e, c):
    m = e.raw_obj('Message', 0x58)
    for off in (0x04, 0x10, 0x1C, 0x3C, 0x48):
        e.fs_set(m + off, '')
    h = strmap_obj(dict(c, n=0), dict(c, n=0), 'Headers') if False else None
    hm = m + 0x28
    hc = c['hc']
    hh = e.raw_obj('MsgHash', 4 * hc)
    for i in range(hc):
        e.write32(hh + 4 * i, 0xFFFFFFFF)
    data = e.alloc(0x1C * 8 + 16)
    e.uc.mem_write(data, bytes(0x1C * 8 + 16))
    e.write32(hm, data)
    e.write32(hm + 4, 0)
    e.write32(hm + 8, 8)
    e.write32(hm + 0xC, hh)
    e.write32(hm + 0x10, hc)
    return m


reg('NMsnMessage.ParseHeader', 0x1014ec10, 296, lambda e, c: (message_obj(e, c), fsargs(e, c['lines'][c['n'] % 12])), noret=False)


def lines_arg(e, c):
    n = c['nlines']
    lines = [c['mimever'], c['mime']] + [c['lines'][i % 12] for i in range(max(n - 2, 0))]
    lines = lines[:n]
    data = e.alloc(4 * max(n, 1) + 16)
    for i, t in enumerate(lines):
        p = e.alloc(16)
        e.fs_set(p, t)
        e.write32(data + 4 * i, p)
    return [data if n else 0, n, n]


reg('NMsnMessage.Parse', 0x1014ed40, 653, lambda e, c: (message_obj(e, c), lines_arg(e, c)), opts={'ignore': {0x1008e8a0}})   # (the by-value array's destructor: the stock calls its own, ours the engine's)


# ---- MsnUtf8ToWide ---------------------------------------------------------------------------------------------------------------------
def b_utf8(e, c):
    inp = e.alloc(0x100)
    e.uc.mem_write(inp, c['utf'].encode('latin-1') + bytes(2))
    out = e.alloc(0x2100)
    e.uc.mem_write(out, bytes(0x2100))
    e.objs[out] = 'Out'
    e.reg_size[out] = 0x100
    return e.raw_obj('Dummy', 8), [inp, out]


reg('MsnUtf8ToWide', 0x10168c20, 52, b_utf8, noret=False)

# ---- MsnCommand / MsnLine ----------------------------------------------------------------------------------------------------------------
def cmd_obj(e, c, label='Cmd', size=0x3C, line=False):
    o = e.raw_obj(label, size)
    e.fs_set(o, c['verb'])
    e.w32(o, 0x0C, c['trid'])
    n = c['nargs']
    data = e.alloc(4 * max(n, 1) + 16)
    for i in range(n):
        p = e.alloc(16)
        e.fs_set(p, c['args'][i])
        e.write32(data + 4 * i, p)
    e.write32(o + 0x10, data if n else 0)
    e.write32(o + 0x14, n)
    e.write32(o + 0x18, n)
    if line:
        for off, texts in ((0x1C, c.get('verbs', [])), (0x28, c.get('codes', []))):
            k = len(texts)
            arr = e.alloc(4 * max(k, 1) + 16)
            for i, t in enumerate(texts):
                q = e.alloc(16)
                e.fs_set(q, t)
                e.write32(arr + 4 * i, q)
            e.write32(o + off, arr if k else 0)
            e.write32(o + off + 4, k)
            e.write32(o + off + 8, k)
        e.w32(o, 0x34, c['basync'])
        e.w32(o, 0x38, c['berror'])
    return o


def arr_strings(e, base):
    data, n = e.read32(base), e.read32(base + 4)
    out = []
    for i in range(n):
        p = e.read32(data + 4 * i)
        out.append(e.fs_get(p) if p else None)
    return out


def post_args(e, this, args):
    return arr_strings(e, this + 0x10)


def post_line(e, this, args):
    return (arr_strings(e, this + 0x10), arr_strings(e, this + 0x1C), arr_strings(e, this + 0x28))


def gen_cmd(r):
    CR, LF = chr(13), chr(10)
    d = gen10(r)
    d.update(verb=r.choice(['USR', 'XFR', 'MSG', '', 'NLN', '201', 'VER']), trid=r.choice([-1, 0, 7, 1234]), nargs=r.choice([0, 1, 2, 4]),
             args=[r.choice(['TWN', 'S', 'messenger.hotmail.com:1863', 'host:80', 'nocolon', ':99', '12', '-5', 'abc', '', 'a:b:c']) for _ in range(6)],
             verbs=[r.choice(['MSG', 'NLN', 'x']) for _ in range(r.choice([0, 1, 3]))], codes=[r.choice(['201', '911']) for _ in range(r.choice([0, 1, 2]))],
             idx=r.choice([0, 1, 2, 5]), basync=r.choice([0, 1]), berror=r.choice([0, 1]),
             raw=r.choice(['', 'MSG', 'AB', 'USR 5 TWN S ticket', 'NLN NLN a@b nick', 'LST N=a@b F=n C=1', '201 7', 'XFR 3 NS 1.2.3.4:1863 0 2.2.2.2:80', 'ABCD', 'ABC ',
                           'FOO 12', 'FOO 12 a', 'BYE a@b' + CR + LF + 'rest', 'USR 5  two  spaces', 'MSG a b' + CR + LF, '911 5']),
             text=r.choice(['USR 1 TWN' + CR + LF, 'a b c', 'a' + CR + LF + 'b c', ' lead', 'trail ', 'x  y', '', 'one']))
    return d


def regc(name, sva, size, build, noret=True, opts=None, gen=gen_cmd):
    reg(name, sva, size, build, noret=noret, opts=opts, gen=gen)


regc('MsnCommand.ctor0', 0x1014e600, 31, lambda e, c: (poison_obj(e, 'Cmd', 0x1C), []), opts={'post': lambda e, t, a: None})
regc('MsnCommand.copy', 0x1014e620, 214, lambda e, c: (poison_obj(e, 'Cmd', 0x1C), [cmd_obj(e, c, 'Src')]), opts={'post': post_args})
regc('MsnCommand.ctorverb', 0x10152c20, 98, lambda e, c: (poison_obj(e, 'Cmd', 0x1C), fsv(e, c['verb'])), opts={'alt': '??0MsnCommand@@QAE@ABVFString@@@Z'})
regc('MsnCommand.dtor', 0x1014e700, 161, lambda e, c: (cmd_obj(e, c), []), opts={'ignore': {0x1008e8a0}})   # (the by-value array's destructor: the stock's own, ours the engine's)
regc('MsnCommand.Release', 0x1014ebb0, 83, lambda e, c: (cmd_obj(e, c), []), opts={'ignore': {0x1008e8a0}})
regc('MsnCommand.GetArg', 0x1014bff0, 60, lambda e, c: (cmd_obj(e, c), [e.raw_obj('Result', 0x10), c['idx']]), noret=False)
regc('MsnCommand.GetArgInt', 0x1014c030, 141, lambda e, c: (cmd_obj(e, c), [c['idx']]), noret=False)
regc('MsnCommand.GetArgServer', 0x1014c240, 336, lambda e, c: (cmd_obj(e, c), [e.raw_obj('Result', 0x10), c['idx']]), noret=False)
regc('MsnCommand.ToString', 0x1014bef0, 254, lambda e, c: (cmd_obj(e, c), [e.raw_obj('Result', 0x10)]), noret=False)
regc('MsnCommand.GetArgCount', 0x1014bbc0, 4, lambda e, c: (cmd_obj(e, c), []), noret=False)
regc('MsnCommand.ParseArgs', 0x1014e070, 455, lambda e, c: (cmd_obj(e, c), fsargs(e, c['text'])), opts={'post': post_args})
regc('MsnLine.ctor0', 0x1014e7b0, 103, lambda e, c: (poison_obj(e, 'Line', 0x3C), []), opts={'post': post_line})
regc('MsnLine.ctorcmd', 0x1014e820, 39, lambda e, c: (poison_obj(e, 'Line', 0x3C), [cmd_obj(e, c, 'Src')]), opts={'post': post_args, 'alt': '??0MsnLine@@QAE@ABV0@@Z'})
regc('MsnLine.dtor', 0x1014e850, 304, lambda e, c: (cmd_obj(e, c, 'Line', 0x3C, True), []), opts={'ignore': {0x1008e8a0}})
regc('MsnLine.FillAsyncVerbs', 0x1014c5a0, 800, lambda e, c: (cmd_obj(e, c, 'Line', 0x3C, True), []), opts={'post': post_line})
regc('MsnLine.FillErrorCodes', 0x1014c8c0, 6050, lambda e, c: (cmd_obj(e, c, 'Line', 0x3C, True), []), opts={'post': post_line})


def b_lineparse(e, c):
    o = cmd_obj(e, dict(c, nargs=0), 'Line', 0x3C, True)
    # the tables the parser asks (the constructor of the stock fills them): the line's own AsyncVerbs / ErrorCodes through the stock's lookups
    e.line_verbs = ['MSG', 'NLN', 'FLN', 'BYE', 'OUT', 'BPR', 'LSG', 'LST', 'RNG', 'JOI']
    return o, fsargs(e, c['raw'])


regc('MsnLine.Parse', 0x1014e980, 558, b_lineparse, noret=False, opts={'post': lambda e, t, a: (post_args(e, t, a), e.r32(t, 0x0C), e.r32(t, 0x34), e.r32(t, 0x38), e.fs_get(t))})

# the stock's lookups of the line (ours calls the same stock functions by address): the unprompted verbs / the error numbers
DW.line_rules = True
# ---- NMsnLink / NMsnSession: the requests of the dialogs and the commands they build ------------------------------------------------------
tr.ARGS_OVERRIDE.update({0x10168c90: 2})
tr.POP_OVERRIDE.update({0x10168c90: 0})


def cmd_digest(e, base):
    """the words of a by-value command (verb FString, TrID, args TArray) as text"""
    verb = e.fs_get(base) if e.read32(base + 4) else ''
    data, n = e.read32(base + 0x10), e.read32(base + 0x14)
    args = []
    for i in range(min(n, 16)):
        p = e.read32(data + 4 * i)
        args.append(e.fs_get(p) if p else None)
    return (verb, e.r32(base, 0x0C), args)


class SW10(DW):
    def __init__(self, kind):
        DW.__init__(self, kind)
        R = self.rules
        R[0x1014e240] = self.add_arg                                                               # MsnCommand::AddArg(FString by value)
        R[0x1014e2e0] = self.add_arg_int                                                           # MsnCommand::AddArg(INT)
        R[0x101607e0] = self.send_command                                                          # NMsnSender::SendLine(command by value)
        R[0x10168c90] = self.url_encode                                                            # EncodeMsnUrl(in, out)
        self.transparent |= {0x10152c20, 0x1014e620, 0x1014ebb0, 0x1014e700}

    def push_arg(self, e, cmd, text):
        arr, n, mx = e.read32(cmd + 0x10), e.read32(cmd + 0x14), e.read32(cmd + 0x18)
        if n + 1 > mx:
            nd = e.alloc(4 * (n + 4) + 16)
            if n:
                e.uc.mem_write(nd, bytes(e.uc.mem_read(arr, 4 * n)))
            e.write32(cmd + 0x10, nd)
            e.write32(cmd + 0x18, n + 4)
            arr = nd
        q = e.alloc(16)
        e.fs_set(q, text)
        e.write32(arr + 4 * n, q)
        e.write32(cmd + 0x14, n + 1)

    def add_arg(self, e, ecx, a):
        self.push_arg(e, ecx, (e.wstr(a[0]) or '') if a[0] else '')
        return 0

    def add_arg_int(self, e, ecx, a):
        self.push_arg(e, ecx, str(a[0] - (1 << 32) if a[0] & 0x80000000 else a[0]))
        return 0

    def send_command(self, e, ecx, a):
        # the command is the by-value argument: rebuild its words (verb data/num/max, TrID, Args data/num/max)
        tmp = e.alloc(0x30)
        for i, w in enumerate(a[:7]):
            e.write32(tmp + 4 * i, w)
        # the copy has the layout verb +0, TrID +0xC, Args +0x10: the words were read in that order
        e.write32(tmp + 0x0C, a[3])
        e.write32(tmp + 0x10, a[4])
        e.write32(tmp + 0x14, a[5])
        e.write32(tmp + 0x18, a[6])
        e.log.append(('cmd', cmd_digest(e, tmp)))
        return 0

    def url_encode(self, e, ecx, a):
        t = (e.wstr(a[0]) or '') if a[0] else ''
        e.uc.mem_write(a[1], t.replace(' ', '%20').encode('utf-16le') + bytes(2))
        return 0


def session_obj(e, c):
    s = e.raw_obj('Session', 0x60)
    e.w32(s, 0x4C, c['trid'])
    return s


def regs(name, sva, size, build, noret=True, opts=None):
    # not compared: the array destructors of the by-value commands (the stock's own, ours the engine's), the extra copy / destructor of a by-value
    # contact record in ours' forwarders, and the address of the command's argument array (the digest of the command's content is)
    o = {'ignore': {0x1008e8a0, 0x10150f20, 0x10150ff0}, 'mask': {0x101607e0: {4}}}
    if opts:
        o.update(opts)
    reg(name, sva, size, build, noret=noret, opts=o, gen=gen_cmd)


regs('NMsnSession.AddFriend', 0x101640c0, 451, lambda e, c: (session_obj(e, c), fsargs(e, c['acct']) + [c['basync']]))
regs('NMsnSession.RemoveFriend', 0x10164e20, 256, lambda e, c: (session_obj(e, c), fsargs(e, c['acct']) + [c['basync']]))
regs('NMsnSession.MoveFriend', 0x10164480, 447, lambda e, c: (session_obj(e, c), [e.read32(friend_obj(e, 'Buddy', c) + 4 * i) for i in range(0x11)] + [c['idx'], c['gid'] & 0xFFFFFFFF]))
regs('NMsnSession.AddGroup', 0x10164640, 268, lambda e, c: (session_obj(e, c), fsargs(e, c['text'])))
regs('NMsnSession.RemoveGroup', 0x10164750, 155, lambda e, c: (session_obj(e, c), [c['gid'] & 0xFFFFFFFF]))
regs('NMsnSession.RenameGroup', 0x101647f0, 285, lambda e, c: (session_obj(e, c), [c['gid'] & 0xFFFFFFFF] + fsargs(e, c['text'])))


def link_obj(e, c):
    l = e.raw_obj('Link', 0x58)
    e.w32(l, 0x0C, session_obj(e, c) if c['hover'] else 0)
    return l


regs('NMsnLink.RequestAddFriend', 0x1014f290, 108, lambda e, c: (link_obj(e, dict(c, hover=c['basync'])), fsargs(e, c['acct']) + [c['berror']]))
regs('NMsnLink.RequestRemoveFriend', 0x1014f370, 108, lambda e, c: (link_obj(e, dict(c, hover=c['basync'])), fsargs(e, c['acct']) + [c['berror']]))
regs('NMsnLink.RequestMoveFriend', 0x1014f3e0, 114, lambda e, c: (link_obj(e, dict(c, hover=c['basync'])), [e.read32(friend_obj(e, 'Buddy', c) + 4 * i) for i in range(0x11)] + [c['idx'], c['gid'] & 0xFFFFFFFF]))
regs('NMsnLink.RequestAddGroup', 0x1014f460, 104, lambda e, c: (link_obj(e, dict(c, hover=c['basync'])), fsargs(e, c['text'])))
regs('NMsnLink.RequestRemoveGroup', 0x1014f4d0, 15, lambda e, c: (link_obj(e, dict(c, hover=c['basync'])), [c['gid'] & 0xFFFFFFFF]))
regs('NMsnLink.RequestRenameGroup', 0x1014f4e0, 109, lambda e, c: (link_obj(e, dict(c, hover=c['basync'])), [c['gid'] & 0xFFFFFFFF] + fsargs(e, c['text'])))

if __name__ == '__main__':
    t_msn1.run_tests(TESTS10, SW10, sys.argv)
