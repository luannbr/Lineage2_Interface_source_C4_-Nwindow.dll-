# Differential tests of the messenger's Winsock side (wave C, lane proof2): MsnSend.cpp (NMsnSender::SendLine / SendRaw) and MSNSocket.cpp
# (NMsnSock: Connect / Close / ReadLine / ReceiveBytes; MSNSocket: OnConnect / Connect / OnSocketMessage / the senders of the handshake),
# stock against ours under unicorn. The Winsock thunks of the stock image (0x10170B02..0x10170B44) are trapped on both sides and answered
# by the case; every byte sent is part of the log.
#   python t_msn6.py [cases] [seed] [test,test,...]
import sys, os, random, struct
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import tr
from tr import *
import t_msn1
from t_msn1 import *
import t_msn2
from t_msn2 import *
import t_msn3
from t_msn3 import *

TESTS6 = {}
CRLF = chr(13) + chr(10)
MSNSOCKET_VT = 0x101DD840          # C4 (the C3 one is 0x101AB138)


def put_fs(e, p, text):
    e.fs_set(p, text)
    return p


def zero_rec(e, p, n):
    e.uc.mem_write(p, bytes(n))
    return p


def copy_base(e, dst, src):
    e.uc.mem_write(dst, bytes(0x3C))
    e.uc.mem_write(dst, bytes(e.uc.mem_read(src, 0x1C)))
    return dst


def ansi(text):
    return text.encode('utf-8', 'replace')


class SW(MW):
    def __init__(self, kind):
        MW.__init__(self, kind)
        R = self.rules
        self.transparent |= {0x10163330, 0x10149ae0, 0x1008e8a0}
        R_ = self.rules
        # NMsnWndIntMap::SetWnd (0x10151770, the request table's Set): ours inlines it, so it runs for real in the stock too (the table is a valid TMap, see sockobj)
        self.fold(0x10164a40, 0x1014a210)          # NMsnIntMap::~NMsnIntMap and NTaskMap::~NTaskMap are one body of ours (folded by the linker): reported as the first
        self.transparent.add(0x10151770)                                       # (its pair array grows through NewStringArrayElement, 0x10148f30: a 27-byte helper that ours expands, FArray::Add)
        self.transparent.add(0x10148f30)
        if sa.ENABLED:
            self.transparent.add(0x101495b0)         # TMsnIntPairArray::Remove: a function in the hook build, inlined in the standalone
        self.transparent |= {0x10153370, 0x10152c00}                                 # TArray<FString*>::TArray(const TArray&) and its Add helper: real code
        R_[0x10165cf0] = lambda e, ecx, a: (e.fs_set(ecx + 0x1C, e.c.get('payload', '')), 0)[1]   # NMsnSock::ReceiveBytes: the payload lands in Line           # the destructors of the 12-byte pair arrays (the stock has two copies, ours calls one)
        R[0x10170b02] = lambda e, ecx, a: e.c.get('wsaerr', 0)                      # WSAGetLastError
        R[0x10170b08] = lambda e, ecx, a: e.c.get('conn', 0) & 0xFFFFFFFF           # connect
        R[0x10170b0e] = lambda e, ecx, a: ((a[0] & 0xFF) << 8 | (a[0] >> 8) & 0xFF)  # htons
        R[0x10170b14] = self.r_hostbyname                                          # gethostbyname
        R[0x10170b1a] = lambda e, ecx, a: e.c.get('inet', 0xFFFFFFFF)              # inet_addr
        R[0x10170b20] = lambda e, ecx, a: e.c.get('sockret', 0x44) & 0xFFFFFFFF    # socket
        R[0x10170b2c] = self.r_recv                                                # recv
        R[0x10170b32] = self.r_send                                                # send
        R[0x10170b38] = lambda e, ecx, a: e.c.get('sel', 0) & 0xFFFFFFFF           # WSAAsyncSelect
        R[0x10170b44] = lambda e, ecx, a: 0                                        # closesocket
        R[0x10168c60] = self.r_utf8                                                # the UTF-8 encoder (text, buffer)
        R[0x10168be0] = self.r_utf8                                                # MsnWideToAnsi (text, buffer)
        R[0x1014c0c0] = lambda e, ecx, a: e.c.get('cmdlen', 5)                     # MsnCommand::GetLength
        R[0x1014bef0] = self.r_tostring                                            # MsnCommand::ToString(out)
        for va, txt in ((0x101685f0, 'en'), (0x10168730, 'Windows'), (0x101687b0, '5.1'), (0x10168810, 'x86')):
            R[va] = (lambda t: lambda e, ecx, a: put_fs(e, a[0], t))(txt)          # the strings of the CVR line (hidden FString return)
        R[0x10152c90] = lambda e, ecx, a: put_fs(e, a[0], 'account@x')               # Msn::GetAccount (hidden FString return)
        R[0x10165c30] = lambda e, ecx, a: e.c.get('rl', 0)                         # NMsnSock::ReadLine (in OnSocketMessage)
        R[0x1014e7b0] = lambda e, ecx, a: zero_rec(e, ecx, 0x3C)                   # MsnLine::MsnLine()
        R[0x1014e620] = lambda e, ecx, a: copy_base(e, ecx, a[0])                  # MsnCommand::MsnCommand(const MsnCommand&)
        self.transparent |= {0x1014e820}                                           # MsnLine::MsnLine(const MsnLine&): base copy, then zeroes
        R[0x1014e980] = self.r_parse                                               # MsnLine::Parse(text by value)
        R[0x101514a0] = lambda e, ecx, a: e.c.get('req', 0)                        # the pending table's FindRef(&key)
        # MsnLine::IsError / IsAsync (the flags at +0x38 / +0x34): getters of the stock that ours does not call (it reads the fields); the parse rule writes the flags
        R[0x10088580] = lambda e, ecx, a: e.r32(ecx, 0x38)
        R[0x1014bbd0] = lambda e, ecx, a: e.r32(ecx, 0x34)

    def reset_case(self, c):
        MW.reset_case(self, c)
        self.rx = bytearray(c.get('rx', b''))
        self.hostent = 0

    def r_parse(self, e, ecx, a):
        e.w32(ecx, 0xC, e.c.get('trid2', 7))               # the transaction id the line carries
        e.w32(ecx, 0x34, e.c.get('basync', 0))            # the flags the getters read
        e.w32(ecx, 0x38, e.c.get('berror', 0))
        return e.c.get('parse', 1)

    def r_hostbyname(self, e, ecx, a):
        if not e.c.get('host', 1):
            return 0
        h = e.raw_obj('hostent', 0x20)
        addr = e.raw_obj('haddr', 8)
        e.uc.mem_write(addr, bytes([10, 1, 2, 3]))
        lst = e.raw_obj('haddrlist', 8)
        e.write32(lst, addr)
        e.uc.mem_write(h + 0xA, struct.pack('<h', 4))
        e.write32(h + 0xC, lst)
        return h

    def r_recv(self, e, ecx, a):
        n = a[2]
        mode = e.c.get('recvmode', 0)
        if mode == 1:
            return 0xFFFFFFFF
        if not e.rx:
            return 0
        chunk = bytes(e.rx[:n])
        del e.rx[:n]
        e.uc.mem_write(a[1], chunk)
        return len(chunk)

    def r_send(self, e, ecx, a):
        data = bytes(e.uc.mem_read(a[1], a[2])) if a[2] < 0x2000 else b'?'
        e.log.append(('sent', data))
        return (a[2] + e.c.get('sendshort', 0)) & 0xFFFFFFFF

    def r_utf8(self, e, ecx, a):
        text = e.wstr(a[0]) or ''
        e.uc.mem_write(a[1], text.encode('utf-8', 'replace') + b'\0')
        return 0

    def r_tostring(self, e, ecx, a):
        e.fs_set(a[0], e.c.get('wire', 'USR 5 TWN I x'))
        return a[0]


def sockobj(e, c):
    """a MSNSocket (0x64 bytes) with the vtable of the base class and a state"""
    p = e.raw_obj('MSNSocket', 0x64)
    vt = e.vt_from_stock(MSNSOCKET_VT, 11)
    e.write32(p, vt)
    e.w32(p, 4, c.get('h', 0x44))
    e.fs_set(p + 4 + 0x1C, c.get('line', ''))
    e.w32(p, 0x38, e.raw_obj('Msn', 0x58))
    e.fs_set(p + 0x3C, c.get('host_s', 'server.example'))
    e.w32(p, 0x48, c.get('port', 1863))
    e.w32(p, 0x4C, c.get('trid', 7))
    # the request table is a valid TMap<INT,INT> (pairs, hash chains): ours inlines the insertion and walks the chains (a table without its hash array sent it round in circles)
    intmap_fill(e, p + 0x50, [(1 + i, i + 1) for i in range(c.get('npend', 0))], 8, 0, 1)
    e.write32(e.glob(0x102c39f0), 0x1234)           # GViewportHWnd
    return p


def vt_from_stock(self, vtaddr, n):
    key = ('vt', vtaddr)
    if key not in self.cls_vt:
        base, mem = stock_bytes()
        vt = self.perm_next
        self.perm_next += 4 * (n + 4)
        self.uc.mem_write(vt, bytes(4 * (n + 4)))
        for i in range(n):
            va = struct.unpack('<I', bytes(mem[vtaddr - base + 4 * i:vtaddr - base + 4 * i + 4]))[0]
            self.write32(vt + 4 * i, self.vstub(va))
        self.cls_vt[key] = vt
    return self.cls_vt[key]


SW.vt_from_stock = vt_from_stock


def gen6(r):
    rx = r.choice([b'', b'VER 1 MSNP8 CVR0\r\n', b'abc', b'x\ny\n', b'MSG a b 5\r\nhello', 'café\n'.encode('utf-8')])
    return dict(h=r.choice([0, 0x44, 9]), wsaerr=r.choice([0, 0x2733, 10054]), conn=r.choice([0, -1]), inet=r.choice([0xFFFFFFFF, 0x0100007F]),
                sockret=r.choice([0x44, -1, 5]), sel=r.choice([0, 0, -1]), host=r.choice([0, 1]), cmdlen=r.choice([5, 0, -1]),
                wire=r.choice(['USR 5 TWN I x', 'VER 3 MSNP8 CVR0', 'café x', '']), sendshort=r.choice([0, 0, -1, 2]), rx=rx,
                recvmode=r.choice([0, 0, 1]), line=r.choice(['', 'old line']), trid=r.choice([1, 7, 99]), npend=r.choice([0, 1, 3]),
                text=r.choice(['mail.example.com', '10.0.0.1', '']), port=r.choice([1863, 80]), count=r.choice([0, 3, 5, 12]),
                lp=r.choice([0x00000001, 0x00000020, 0x00010001, 0x00200001, 0x00000002, 0x00000000]), wp=r.choice([0, 5]),
                req=r.choice([0, 1, 2, 3, 4]), berror=r.choice([0, 1]), basync=r.choice([0, 1]), os=r.choice([0, 1, 2]),
                cmd=[r.randint(0, 0xFFFF) for _ in range(7)], reason=r.choice(['a', 'bb']), parse=r.choice([0, 1, 1]), trid2=r.choice([1, 7, 9]),
                kept=r.choice([0, 1]), rl=r.choice([0, 5, 5]),
                payload=r.choice(['', 'hello', 'a' + CRLF + 'b' + CRLF, CRLF + CRLF + 'x', 'x' + CRLF, 'MIME: 1' + CRLF + 'Content: x' + CRLF + CRLF + 'body']),
                verb=r.choice(['MSG', 'MSG', 'NLN', '']))


def cmdargs(e, c):
    """the command by value: 0x1C bytes (verb, transaction id, an empty argument array)"""
    p = e.raw_obj('cmd', 0x1C)
    e.fs_set(p, c.get('wire', 'USR')[:3] or 'USR')
    e.w32(p, 0xC, c.get('trid2', 7))
    return [e.read32(p + 4 * i) for i in range(7)]


def sock_in(sva, size, argfn=None, noret=True, standalone=False, cases=gen6, name=None):
    def b(e, c):
        e.write32(e.glob(0x102c39f0), 0x1234)
        e.write32(e.glob(0x102c39f4), e.console)
        e.write32(e.console + 0x54, e.raw_obj('Engine', 0x80))
        e.write32(e.r32(e.console, 0x54) + 0x64, e.raw_obj('NetHandler', 0x40))
        if not hasattr(e, 'fake_vt54'):             # one permanent vtable per emulator (a new one per case used up the permanent area)
            e.fake_vt54 = e.perm_vt({54: 0x1fff0036})
        e.write32(e.r32(e.r32(e.console, 0x54), 0x64), e.fake_vt54)
        e.write32(e.gcell['?GRunningOS@@3W4ERunningOS@@A'], c.get('os', 0)) if '?GRunningOS@@3W4ERunningOS@@A' in e.gcell else None
        p = sockobj(e, c)
        this = p + 4 if standalone else p
        return this, (argfn(e, c) if argfn else [])
    return (sva, size or tr.functions()[sva][0], b, cases, {'noret': noret})


TESTS6['sock_connect'] = sock_in(0x10165ae0, None, lambda e, c: [e.put_wstr(c['text']), c['port']], noret=False, standalone=True)
TESTS6['sock_close'] = sock_in(0x10165d80, None, None, standalone=True)
TESTS6['sock_readline'] = sock_in(0x10165c30, None, None, noret=False, standalone=True)
# MsnUtf8ToWide takes no `this`: the stock leaves whatever it had, ours the object (as in sock_recvbytes)
TESTS6['sock_readline'] = TESTS6['sock_readline'][:4] + (dict(TESTS6['sock_readline'][4], nothis={0x10168c20}),)
TESTS6['sock_recvbytes'] = sock_in(0x10165cf0, None, lambda e, c: [c['count']], noret=True, standalone=True)
# ecx is not an argument of recv / MsnUtf8ToWide: the stock leaves whatever it had, ours the object: not compared
TESTS6['sock_recvbytes'] = TESTS6['sock_recvbytes'][:4] + (dict(TESTS6['sock_recvbytes'][4], nothis={0x10170b2c, 0x10168c20}),)   # the stock returns the FString& of its last assignment in eax, ours 0: no caller reads it (OnAsyncImpl, ProcessMSG)
TESTS6['sock_dtor'] = sock_in(0x10166260, None, None, standalone=True)
TESTS6['sock_ctor'] = (0x10165a90, 38, lambda e, c: (poison_raw(e, 'NMsnSock', 0x34), []), lambda r: {}, {'noret': True})
TESTS6['ms_onconnect'] = sock_in(0x10160c40, None, lambda e, c: [c['wp'], c['lp']], noret=False)
TESTS6['ms_connect'] = sock_in(0x10160fc0, None, None, noret=False)
TESTS6['ms_register'] = sock_in(0x10163460, None, lambda e, c: [c['req']])
TESTS6['ms_remove'] = sock_in(0x10163480, None, lambda e, c: [c['trid']])
TESTS6['ms_onsocketmsg'] = sock_in(0x10163490, None, lambda e, c: [c['wp'], c['lp']])
# (the getters of the two flags; and the flags of the line copies the handlers get by value: MsnLine's copy constructor does not write +0x34 / +0x38, so the words
# of those copies (arguments 13 and 14 of the line; 14 and 15 after the request number of OnReplyIgnore) are indeterminate)
TESTS6['ms_onsocketmsg'] = TESTS6['ms_onsocketmsg'][:4] + (dict(TESTS6['ms_onsocketmsg'][4], ignore={0x10088580, 0x1014bbd0},
                                                                mask={0x10160fe0: {13, 14}, 0x10164f20: {13, 14}, 0x10153c70: {13, 14}, 0x10164b70: {14, 15}}),)
TESTS6['ms_sendline'] = sock_in(0x101607e0, None, cmdargs)
TESTS6['ms_sendraw'] = sock_in(0x10160950, None, cmdargs)


def poison_raw(e, label, n):
    p = e.alloc(n + 16)
    e.uc.mem_write(p, bytes([0xCD]) * (n + 16))
    e.objs[p] = label
    e.reg_size[p] = n
    return p


def lineargs(e, c):
    """a MsnLine by value: 0x3C bytes"""
    p = e.raw_obj('line', 0x3C)
    e.fs_set(p, c.get('verb', 'MSG'))
    e.w32(p, 0xC, c.get('trid2', 7))
    return [e.read32(p + 4 * i) for i in range(15)]


TESTS6['ms_sendver'] = sock_in(0x10163680, None, None)
TESTS6['ms_sendcvr'] = sock_in(0x10163750, None, lineargs)
TESTS6['ms_sendusrds'] = sock_in(0x101638d0, None, lineargs)
TESTS6['ms_sendusrns'] = sock_in(0x101639d0, None, lineargs)
TESTS6['ms_onerror'] = sock_in(0x10160fe0, None, lineargs)
TESTS6['ms_onasync'] = sock_in(0x10164f20, None, lineargs)
TESTS6['ms_onasync'] = TESTS6['ms_onasync'][:4] + ({'noret': True},)
TESTS6['ms_ignorecmd'] = sock_in(0x10153c70, None, lineargs)
TESTS6['ms_ignorereply'] = sock_in(0x10164b70, None, lambda e, c: [c['req']] + lineargs(e, c))


# MSNSocket::Construct (0x10164ab0, ret 0x18) takes the server by value (four dwords: the host FString and the port); the standalone's constructor
# (??0MSNSocket@@QAE@PAVMsn@@ABUNMsnServer@@H@Z, the compiler inlines the stock's body into it) takes a reference to it
SA_SOCK_CTOR = '??0MSNSocket@@QAE@PAVMsn@@ABUNMsnServer@@H@Z'


def construct_args(e, msn, host, port, trid):
    srv = e.alloc(16)
    e.fs_set(srv, host)
    e.write32(srv + 0xC, port)
    if sa.ENABLED and e.kind == 'ours':
        t_msn1.ALT[0x10164ab0] = sa.parse_map(sa.SA_MAP)[0][SA_SOCK_CTOR]
        return [msn, srv, trid]
    return [msn, e.read32(srv), e.read32(srv + 4), e.read32(srv + 8), e.read32(srv + 0xC), trid]


def b_construct(e, c):
    p = poison_raw(e, 'MSNSocket', 0x64)
    return p, construct_args(e, e.raw_obj('Msn', 0x58), c.get('host_s', 'srv.example'), c.get('port', 1863), c['trid'])


TESTS6['ms_construct'] = (0x10164ab0, 182, b_construct, gen6, {'noret': False, 'alt': SA_SOCK_CTOR})


def b_destroybase(e, c):
    p = poison_raw(e, 'MSNSocket', 0x64)
    e.run_real(0x10164ab0, 182, p, construct_args(e, e.raw_obj('Msn', 0x58), '', 0, 5))
    e.pre_len = len(e.log)
    e.w32(p, 0x38, e.raw_obj('Msn', 0x58) if c.get('npend') else 0)
    return p, []


TESTS6['ms_destroybase'] = (0x10164b80, 0x10164b80 and 70, b_destroybase, gen6, {'noret': True})


# NMsnIntMap: the request table (TMap<INT,INT>): Rehash, Remove, the destructor, the pair array's destructor and Remove
def intmap_obj(e, c):
    m = e.raw_obj('IntMap', 0x14)
    intmap_fill(e, m, c.get('pairs', []), c.get('hc', 8), c.get('slack', 0), c.get('hash', 1))
    return m


def gen_map(r):
    n = r.choice([0, 1, 3, 9, 20])
    return dict(pairs=[(r.choice([1, 2, 3, 8, 9, 17, 100, -1]), r.randint(0, 9)) for _ in range(n)], hc=r.choice([8, 16, 32, 64]),
                slack=r.choice([0, 0, 5]), hash=r.choice([0, 1, 1]), key=r.choice([1, 2, 3, 8, 9, 17, 100, -1, 55]), idx=r.choice([0, 1, 2]))


TESTS6['map_rehash'] = (0x10089d40, None, lambda e, c: (intmap_obj(e, c), []), gen_map, {'noret': True})
TESTS6['map_remove'] = (0x10151830, None, lambda e, c: (intmap_obj(e, c), [e.raw_obj('key', 4) and e.alloc(4) and 0]), gen_map, {'noret': False})
TESTS6['map_dtor'] = (0x10164a40, None, lambda e, c: (intmap_obj(e, c), []), gen_map, {'noret': True})


def b_map_remove(e, c):
    m = intmap_obj(e, c)
    k = e.alloc(4)
    e.write32(k, c['key'] & 0xFFFFFFFF)
    return m, [k]


TESTS6['map_remove'] = (0x10151830, None, b_map_remove, gen_map, {'noret': False})


def b_pairs_remove(e, c):
    m = intmap_obj(e, c)
    n = e.r32(m, 4)
    if n == 0:
        m = intmap_obj(e, dict(c, pairs=[(1, 1)]))
        n = 1
    return m, [min(c['idx'], max(n - 1, 0)), 1]


# (the standalone has no function that the hook registration maps these two to: TArray<NMsnIntPair>'s Remove / destructor are template instances)
TESTS6['pairs_remove'] = (0x101495b0, None, b_pairs_remove, gen_map, {'noret': True, 'alt': '?Remove@?$TArray@UNMsnIntPair@@@@QAEXHH@Z'})
TESTS6['pairs_dtor'] = (0x10163330, None, lambda e, c: (intmap_obj(e, c), []), gen_map, {'noret': True, 'alt': '??1?$TArray@UNMsnIntPair@@@@QAE@XZ'})


# the handshake senders end to end: the command is built, copied and sent for real (the senders, the command's methods and the string
# building run as code on both sides; only the Winsock calls and the heap are the world), so the bytes sent are compared
class SW2(SW):
    def __init__(self, kind):
        SW.__init__(self, kind)
        self.transparent |= {0x101607e0, 0x10152c20, 0x1014e600, 0x1014e240, 0x1014bef0, 0x1014c0c0, 0x1014ebb0, 0x1014e700, 0x1014e620,
                             0x10160950, 0x1014e7b0, 0x1014e980, 0x1014e820, 0x1014e850, 0x1008e8a0, 0x1015ae00}
        for va in (0x1014c0c0, 0x1014bef0, 0x1014e600, 0x10152c20, 0x1014e620):
            self.rules.pop(va, None)


E2E = {}
for nm in ('ms_sendver', 'ms_sendcvr', 'ms_sendusrds', 'ms_sendusrns', 'ms_sendline', 'ms_sendraw'):
    t = TESTS6[nm]
    o = dict(t[4])
    o['no_heap_log'] = True         # ours makes one more copy of the command (an extra pair of allocations); the bytes sent are what counts
    E2E[nm + '_e2e'] = t[:4] + (o,)


# the structural variants (the senders trapped) differ only by ours' extra copy of the command; the end-to-end ones are the proof
for nm in ('ms_sendver', 'ms_sendcvr', 'ms_sendusrds', 'ms_sendusrns', 'ms_sendline', 'ms_sendraw'):
    TESTS6.pop(nm)

if __name__ == '__main__':
    names = sys.argv[3].split(',') if len(sys.argv) > 3 else None
    a1 = sys.argv[:3] + ([','.join(n for n in names if n in TESTS6)] if names else [])
    a2 = sys.argv[:3] + ([','.join(n for n in names if n in E2E)] if names else [])
    total = 0
    if not names or a1[3:] and a1[3]:
        total += t_msn1.run_tests(TESTS6, SW, a1)
    if not names or a2[3:] and a2[3]:
        total += t_msn1.run_tests(E2E, SW2, a2)
    print('ALL problems', total)
