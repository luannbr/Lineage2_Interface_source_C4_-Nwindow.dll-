# Differential test of the switchboard session's body sender (stock 0x10160A60, ret 0x74: the command 0x1C + the message 0x58, both by
# value; `this` is the socket object: +4 the SOCKET, +0x4C the transaction counter) against ours (NMsnSender::SendMessageBody, MsnSend.cpp):
# stock vs ours under unicorn. The callees are logging stubs: the message's Serialize (0x1014C450, hidden FString result), the command's
# AddIntArg (0x1014E2E0), ToString (0x1014BEF0, hidden result), the release of the command (0x1014EBB0), the free of the message
# (0x1014EFE0), the UTF-8 conversion (0x10168C60, cdecl: the text into a 0x1040-byte buffer) and winsock's send / WSAGetLastError through
# the stock image's thunks (0x10170B32 / 0x10170B02). Compared: every call with its arguments, the bytes sent and their length, the
# counter, the by-value copies' words (the callee's own buffer: untouched).
#   python t_msnsend.py [cases] [seed]
import sys, os, random, struct
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from tip4 import *
import tip4
import sa

VA = 0x10160A60
CALLEES_X = {
    0x1014C450: ('Serialize', 4, 0), 0x1014E2E0: ('AddIntArg', 4, 0), 0x1014BEF0: ('ToString', 4, 0), 0x1014EBB0: ('CmdRelease', 0, 0),
    0x1014EFE0: ('MsgFree', 0, 0), 0x10170B32: ('send', 16, 0), 0x10170B02: ('WSAGetLastError', 0, 0),
}
CDECL_X = {0x10168C60: ('Utf8', 2)}
# (the compiled bodies the code calls directly: the hook registration of ToString / the releases is a wrapper at another address)
SYMS_X = {0x1014C450: '?Serialize@NMsnMessage@@QAE?AVFString@@XZ', 0x1014BEF0: '?ToString@MsnCommand@@QAE?AVFString@@XZ',
          0x1014EBB0: '??1NMsnLocalCommand@@QAE@XZ', 0x1014EFE0: '??1NMsnMessage@@QAE@XZ'}


class MS(TW):
    def __init__(self, kind):
        # the standalone inlines the releases of the two by-value arguments (the command's and the message's destructors): in its mode the stock's run for real
        # too (on the empty objects of run()), so that the frees compare; the other callees stay stubs
        callees = {va: v for va, v in CALLEES_X.items() if v[0] not in ('CmdRelease', 'MsgFree')} if sa.ENABLED else CALLEES_X
        TW.__init__(self, kind, extra_callees=callees, extra_cdecl=CDECL_X)
        self.payload = ''
        self.wire = ''
        self.sent = []
        self.short = False

    def thiscall(self, nm, t, a):
        if nm == 'Serialize':
            self.fs_set(a[0], self.payload)
            self.log.append(('Serialize', self.nm(t)))
            return a[0]
        if nm == 'ToString':
            self.fs_set(a[0], self.wire)
            self.log.append(('ToString', self.nm(t)))
            return a[0]
        if nm == 'AddIntArg':
            self.log.append(('AddIntArg', self.nm(t), a[0]))
            return None
        if nm in ('CmdRelease', 'MsgFree'):
            self.log.append((nm, self.nm(t)))
            return None
        if nm == 'send':
            data = bytes(self.uc.mem_read(a[1], a[2])) if 0 <= a[2] < 0x2000 else b'?'
            self.log.append(('send', a[0], data, a[2], a[3]))
            return a[2] - (1 if self.short else 0)
        if nm == 'WSAGetLastError':
            self.log.append(('WSAGetLastError',))
            return 10054
        return TW.thiscall(self, nm, t, a)

    def cdecl_call(self, nm, a):
        if nm == 'Utf8':
            text = self.wstr(a[0])
            data = text.encode('utf-8') + b'\0'
            self.uc.mem_write(a[1], data)
            self.log.append(('Utf8', text))
            return len(data)
        return TW.cdecl_call(self, nm, a)

    def do_import(self, nm):
        if nm in ('send', 'WSAGetLastError'):
            # the standalone calls Winsock as imports of ws2_32 (stdcall); the stock and the hook build call the image's thunks, which thiscall() answers: the same entry
            return self.thiscall(nm, 0, [self.arg(i) for i in range(4 if nm == 'send' else 0)])
        if nm == '?appMemset@@YAXPAXHH@Z':
            n = self.arg(2)
            self.uc.mem_write(self.arg(0), bytes([self.arg(1) & 0xFF]) * n)
            self.log.append(('appMemset', n))
            return None
        return TW.do_import(self, nm)


tip4.stubs.IMPORT_POP.update({'?appMemset@@YAXPAXHH@Z': 0, 'send': 16, 'WSAGetLastError': 0})
tip4.OURS_SYMS.update(SYMS_X)
TEXTS = ['', 'MIME-Version: 1.0\r\nContent-Type: text/plain; charset=UTF-8\r\n\r\nhello', 'x' * 300, 'café 中文', 'typing\r\n\r\n']
WIRES = ['MSG 12 N', 'MSG 7 U 59', 'MSG 100 N 1234']


def run(e, A, c):
    mark = len(e.log)
    e.payload = c['payload']
    e.wire = c['wire']
    e.short = c['short']
    this = e.alloc(0x60)
    e.uc.mem_write(this, bytes(0x60))
    e.write32(this + 4, c['sock'])
    e.write32(this + 0x4C, c['trid'])
    e.objs[this] = 'sock'
    words = []
    rnd = random.Random(c['seed'])
    for i in range(7 + 22):
        words.append(rnd.getrandbits(32))
    if sa.ENABLED:
        # the standalone inlines the releases of the by-value arguments, so the stock's run for real too (see MS): the arguments are real objects. The command: a verb,
        # the transaction id and two argument strings in a heap array (MsnCommand::Release empties the array: its buffer is freed, the strings are not); the message is empty
        t = e.alloc(16)
        e.fs_set(t, 'MSG')
        args = e.alloc(8)
        for k in range(2):
            fsp = e.alloc(16)
            e.fs_set(fsp, 'a%d' % k)
            e.write32(args + 4 * k, fsp)
        words = [e.read32(t), e.read32(t + 4), e.read32(t + 8), c['trid'], args, 2, 2] + [0] * 22
    e.call(A[0], this=this, args=words)
    lg = e.log[mark:]
    if not sa.ENABLED:
        # the hook build destroys the message argument before the command (the stock: the command, then the message); two independent releases at the end of the call,
        # so their order is not compared there (the standalone's inlined releases leave no entry to order: see the empty objects in the standalone's arguments)
        rel = [x for x in lg if x[0] in ('CmdRelease', 'MsgFree')]
        lg = [x for x in lg if x[0] not in ('CmdRelease', 'MsgFree')] + sorted(rel)
    return dict(log=lg, trid=e.read32(this + 0x4C))


def rand_case(r):
    return dict(payload=r.choice(TEXTS), wire=r.choice(WIRES), short=r.random() < 0.3, sock=r.choice([0, 0x1F4, 0xFFFF]),
                trid=r.choice([0, 1, 99, 0x7FFFFFFF]), seed=r.randint(0, 10 ** 9))


def main():
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 60
    seed = int(sys.argv[2]) if len(sys.argv) > 2 else 1
    es = MS('stock')
    eo = MS('ours')
    lm = log_map()
    es.heap_mark, eo.heap_mark = es.heap_ptr, eo.heap_ptr
    r = random.Random(seed)
    cases = [rand_case(r) for _ in range(n)]
    total = compare('SendMessageBody', es, eo, run, cases, (VA,), (lm[VA],))
    print('TOTAL problems', total)


if __name__ == '__main__':
    main()
