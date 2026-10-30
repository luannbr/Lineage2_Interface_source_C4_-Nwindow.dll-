# Differential test of NCChatWnd::AddChatMessage (stock vs ours under unicorn) - the lists, the petition windows and
# CaptureMessage are fake objects whose virtual methods log their calls
#   python e5.py [cases] [seed]
import sys, os, random, struct
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import e4
from e4 import *

STOCK5 = {0x100dfe90: 'PetAddGM', 0x100dfc60: 'PetAddPlayer'}
stubs.FN_POP.update({'PetAddGM': 0x10, 'PetAddPlayer': 0x10, 'ListAddString': 0xC, 'ChatCapture': 0xC,
                     'WndIsShow': 0, 'WndShow': 0, 'WndFocus': 0})

CHAT = dict(Lists=0x18C, SystemList=0x1A0)
FILT = 0x244


class E5(E4):
    def __init__(self, kind):
        E4.__init__(self, kind)
        self.flags = {}
        for e in self.pe.DIRECTORY_ENTRY_IMPORT:
            for imp in e.imports:
                nm = imp.name.decode() if imp.name else ''
                if nm in ('?GL2TextCapture@@3HA', '?GL2SystemMsgWndDamage@@3HA', '?GL2SystemMsgWndExpendableItem@@3HA',
                          '?GL2SystemMsgWnd@@3HA'):
                    v = self.alloc(16)
                    self.flags[nm] = v
                    self.write32(imp.address, v)
        # fake vtables (every slot a stub that logs which slot was called)
        self.objs = {}
        self.n_stub = 0
        self.vt_list = self.make_vtable({76: ('ListAddString',)})
        self.vt_chat = self.make_vtable({77: ('ChatCapture',)})
        self.vt_wnd = self.make_vtable({41: ('WndIsShow',), 48: ('WndShow',), 29: ('WndFocus',)})
        self.show_ret = 0
        # petition functions of the stock: stubbed at their VA (stock) or at NWBASE+rva (ours, StockCallAt)
        for va, nm in STOCK5.items():
            stub = STUB_BASE + 0xA000 + 16 * self.n_stub
            self.n_stub += 1
            self.uc.mem_write(stub, bytes([0xC3]))
            self.stubs[stub] = ('fn:' + nm, None)
            where = va if self.kind == 'stock' else NWBASE + (va - 0x10000000)
            self.uc.mem_write(where, b'\x68' + struct.pack('<I', stub) + b'\xC3')

    def make_vtable(self, slots):
        vt = self.alloc(0x400)
        self.uc.mem_write(vt, bytes(0x400))
        for slot, (nm,) in slots.items():
            stub = STUB_BASE + 0xB000 + 16 * self.n_stub
            self.n_stub += 1
            self.uc.mem_write(stub, bytes([0xC3]))
            self.stubs[stub] = ('fn:' + nm, None)
            self.write32(vt + 4 * slot, stub)
        return vt

    def obj_name(self, p):
        return self.objs.get(p, '?%x' % p)

    def do_fn(self, nm):
        t = self.this()
        a = self.arg
        if nm == 'ListAddString':
            self.log.append(('AddString', self.obj_name(t), self.wstr(a(0)), a(1), self.rds32(self.reg(UC_X86_REG_ESP) + 12)))
            return None
        if nm == 'ChatCapture':
            self.log.append(('Capture', self.wstr(a(0)), a(1), a(2)))
            return None
        if nm in ('PetAddGM', 'PetAddPlayer'):
            self.log.append((nm, self.obj_name(t), self.wstr(a(0)), a(1), a(2), a(3)))
            return None
        if nm == 'WndIsShow':
            self.log.append(('IsShow', self.obj_name(t)))
            return self.show_ret
        if nm == 'WndShow':
            self.log.append(('Show', self.obj_name(t)))
            return None
        if nm == 'WndFocus':
            self.log.append(('Focus', self.obj_name(t)))
            return None
        return E4.do_fn(self, nm)


def make_chat(e, st):
    e.objs = {}
    chat = e.alloc(0x300)
    e.uc.mem_write(chat, bytes(0x300))
    e.write32(chat, e.vt_chat)
    e.objs[chat] = 'chat'
    for i in range(6):
        if st['lists'][i]:
            o = e.alloc(0x300)
            e.uc.mem_write(o, bytes(0x300))
            e.write32(o, e.vt_list)
            for j in range(13):
                e.write32(o + FILT + 4 * j, st['filters'][i][j])
            e.objs[o] = 'list%d' % i
            e.write32(chat + (0x18C + 4 * i if i < 5 else 0x1A0), o)
    gm = e.alloc(0x100); e.uc.mem_write(gm, bytes(0x100)); e.write32(gm, e.vt_wnd); e.objs[gm] = 'gm'
    pw = e.alloc(0x100); e.uc.mem_write(pw, bytes(0x100)); e.write32(pw, e.vt_wnd); e.objs[pw] = 'player'
    desk = e.alloc(0x400); e.uc.mem_write(desk, bytes(0x400))
    e.write32(desk + 0x10C, gm)
    e.write32(e.console + 0x3BF4, desk)
    e.write32(e.console + 0x3C00, pw)
    e.show_ret = st['gm_shown']
    for nm, v in st['flags'].items():
        e.write32(e.flags[nm], v)
    return chat


def run5(e, A, c):
    mark = len(e.log)
    chat = make_chat(e, c['st'])
    text = e.put_wstr(c['text'])
    r = e.call(A['addchat'], this=chat, args=[text, c['color'], c['type'], c['sub']])
    return dict(log=e.log[mark:])


def rand5(r):
    st = dict(lists=[r.random() < 0.85 for _ in range(6)],
              filters=[[r.randint(0, 1) for _ in range(13)] for _ in range(6)],
              gm_shown=r.randint(0, 1),
              flags={'?GL2TextCapture@@3HA': r.choice([0, 1, 2, 2, 3]),
                     '?GL2SystemMsgWndDamage@@3HA': r.randint(0, 1),
                     '?GL2SystemMsgWndExpendableItem@@3HA': r.randint(0, 1),
                     '?GL2SystemMsgWnd@@3HA': r.randint(0, 1)})
    return dict(st=st, text='msg%d' % r.randint(0, 99), color=r.choice([0xFFDCDCDC, 0xFFB09B79, 0xFF123456]),
                type=r.choice(list(range(0, 21)) + [255]), sub=r.choice(list(range(0, 10)) + [-1]), op='addchat')


def main(n, seed):
    A5 = {'addchat': 0x10052f40}
    r = random.Random(seed)
    cases = [rand5(r) for _ in range(n)]
    es = E5('stock'); eo = E5('ours')
    lm = log_map()
    Ao = {'addchat': lm[0x10052f40]}
    es.heap_mark, eo.heap_mark = es.heap_ptr, eo.heap_ptr
    bad = err = 0
    seen = set()
    for i, c in enumerate(cases):
        es.heap_ptr, eo.heap_ptr = es.heap_mark, eo.heap_mark
        try:
            a = run5(es, A5, c)
        except Exception as ex:
            a = {'exc': str(ex)[:300]}
        try:
            b = run5(eo, Ao, c)
        except Exception as ex:
            b = {'exc': str(ex)[:300]}
        if 'exc' in a or 'exc' in b:
            err += 1
            if err <= 5:
                print('--- case %d raised' % i, c['type'], c['sub'], a.get('exc'), '|', b.get('exc'))
        elif a != b:
            bad += 1
            if bad <= 6:
                print('--- case', i, 'type', c['type'], 'sub', c['sub'])
                print(' state:', ascii(c['st'])[:700])
                print('  stock:', ascii(a['log'])[:900])
                print('  ours :', ascii(b['log'])[:900])
        else:
            seen.add((c['type'], c['sub'] if c['type'] == 255 or c['type'] == 0 and False else c['sub']))
        es.log.clear(); eo.log.clear()
    print('cases', n, 'differ', bad, 'raised', err)


if __name__ == '__main__':
    main(int(sys.argv[1]) if len(sys.argv) > 1 else 500, int(sys.argv[2]) if len(sys.argv) > 2 else 1)
