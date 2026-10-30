# STALE (C4, sa-emu agent `old`): C4 NCEquipItemWnd::OnPaint (0x10094080) is tried by win/t_oldwnd.py (OLDWND_ALL=1; the generic world cannot build the equipped items yet); C2 model, does not run.
"""Differential test of NCEquipItemWnd::OnPaint (stock 0x1007b5c0 against ours) and HitTestHenna (0x1007bbd0): random
paperdolls (items with two or three icons, tattoos), the calls logged by the stubs and the result must be equal.
usage: python tools/port/emu/t_equippaint.py <cases> <seed>"""
import sys, random
import os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from t_fill import *
import stubs

stubs.STOCK_FN[0x10025290] = 'FindTexture'
stubs.FN_POP['FindTexture'] = 4
stubs.OURS_FN['FindTexture'] = ('@NCObject@@', '?FindTexture@')
stubs.FN_POP['HennaSlots'] = 0
stubs.FN_POP['PtInRect'] = 0x18


class EqEmu(TipEmu):
    def __init__(self, kind):
        super().__init__(kind)
        self.nslots = 0
        stub = STUB_BASE + 0x9200
        self.uc.mem_write(stub, b'\xC3')
        self.stubs[stub] = ('fn:HennaSlots', None)
        code = b'\x68' + struct.pack('<I', stub) + b'\xC3'
        va = 0x10060E90
        if kind == 'stock':
            self.uc.mem_write(va, code)
        else:
            self.uc.mem_write(NWBASE + (va - 0x10000000), code)

    def do_fn(self, nm):
        if nm == 'HennaSlots':
            self.log.append(('HennaSlots',))
            return u32(self.nslots)
        if nm == 'FindTexture':
            s = self.wstr(self.arg(0)) if self.arg(0) else None
            self.log.append(('FindTexture', s))
            return 0x7700 + (len(s) if s else 0)
        if nm == 'PtInRect':
            a = [self.rds32(self.reg(UC_X86_REG_ESP) + 4 + 4 * i) for i in range(6)]
            self.log.append(('PtInRect', a))
            return 1 if (a[0] >= a[2] and a[0] < a[2] + a[4] and a[1] >= a[3] and a[1] < a[3] + a[5]) else 0
        return super().do_fn(nm)


def make_wnd(e, A, case, ds):
    items = []
    for it in case['items']:
        p = make_item(e, it['item'])
        e.write32(p, it['icon'])
        e.write32(p + 4, it['icon2'])
        e.write32(p + 8, it['icon3'])
        e.write_wstr_at(p + 0x50C, it['s2'])
        e.write_wstr_at(p + 0x60C, it['s3'])
        e.write32(p + 0x1950, it['sel'])
        items.append(p)
    hennas = []
    for h in case['hennas']:
        p = e.alloc(0x1B40)
        e.uc.mem_write(p, bytes(0x1B40))
        e.write32(p, h['icon'])
        e.write32(p + 0x1938, h['usable'])
        for off, v in zip((0x1A28, 0x1A2C, 0x1A30, 0x1A34), h['box']):
            e.write32(p + off, v)
        hennas.append(p)

    def arr(ptrs):
        d = e.alloc(4 * max(1, len(ptrs)))
        for i, v in enumerate(ptrs):
            e.write32(d + 4 * i, v)
        return d
    wnd = e.alloc(0x260)
    e.uc.mem_write(wnd, bytes(0x260))
    vt = e.alloc(0x400)
    for i in range(0x100):
        e.write32(vt + 4 * i, 0)
    e.write32(vt + 0x10C, ds['getequip'])
    e.write32(vt + 0xA4, ds['ptinrect'])
    e.write32(wnd, vt)
    for off, v in ((0x44, case['sx']), (0x48, case['sy']), (0x4C, case['w']), (0x50, case['h'])):
        e.write32(wnd + off, v)
    e.write32(wnd + 0xA0, arr(items)); e.write32(wnd + 0xA4, len(items)); e.write32(wnd + 0xA8, len(items))
    e.write32(wnd + 0xAC, arr(hennas)); e.write32(wnd + 0xB0, len(hennas)); e.write32(wnd + 0xB4, len(hennas))
    e.write32(wnd + 0xF4, 0x1000); e.write32(wnd + 0xFC, 0x1002 if case['dual'] else 0)
    e.write32(wnd + 0x100, 0x1003 if case['hback'] else 0)
    e.write32(wnd + 0x108, case['bow'])
    e.write32(wnd + 0x120, case['cell'])
    return wnd


def main(n, seed):
    r = random.Random(seed)
    cases = []
    for _ in range(n):
        items = []
        for _ in range(r.randint(0, 7)):
            it = rand_item(r)
            it['ints'][0x1914] = r.randint(1, 99)
            it['ints'][0x1934] = r.choice([0, 1, 6, 6])
            items.append({'item': it, 'sel': r.choice([0, 1]), 'icon': r.choice([0, 0x2001, 0x2002]),
                          'icon2': r.randint(0, 3) * 0x100, 'icon3': r.randint(0, 3) * 0x111,
                          's2': r.choice(['', 'a']), 's3': r.choice(['', '', 'b'])})
        hennas = [{'icon': r.choice([0, 0x3001, 0x3002]), 'usable': r.choice([0, 1, 5]),
                   'box': [0x6D, 27 * i, 24, 24]} for i in range(r.randint(0, 4))]
        cases.append({'items': items, 'hennas': hennas, 'sx': r.randint(0, 600), 'sy': r.randint(0, 400), 'w': r.randint(100, 300),
                      'h': r.randint(100, 300), 'cell': r.choice([32, 32, 34]), 'dual': r.random() < 0.8, 'hback': r.random() < 0.8,
                      'bow': r.choice([0, 1]), 'nslots': r.choice([-1, 0, 1, 2, 3]), 'px': r.randint(0, 700), 'py': r.randint(0, 500)})
    res = {}
    for kind in ('stock', 'ours'):
        e = EqEmu(kind)
        syms = e.symbols if kind == 'ours' else None
        if kind == 'stock':
            fp, fh = 0x1007B5C0, 0x1007BBD0
        else:
            fp = [v for s, v in syms.items() if s.startswith('?OnPaint@NCEquipItemWnd@@')][0]
            fh = [v for s, v in syms.items() if s.startswith('?HitTestHenna@NCEquipItemWnd@@')][0]
        stub = STUB_BASE + 0x9000
        e.uc.mem_write(stub, b'\xC3')
        e.stubs[stub] = ('fn:GetEquipSlot', None)
        stub2 = STUB_BASE + 0x9010
        e.uc.mem_write(stub2, b'\xC3')
        e.stubs[stub2] = ('fn:PtInRect', None)
        ds = {'getequip': stub, 'ptinrect': stub2}
        e.heap_mark = e.heap_ptr
        outs = []
        for c in cases:
            e.heap_ptr = e.heap_mark
            e.log.clear()
            e.nslots = c['nslots']
            wnd = make_wnd(e, None, c, ds)
            dev = e.alloc(0x80)
            e.uc.mem_write(dev, bytes(0x80))
            ret = e.call(fp, this=wnd, args=[dev])
            dv = [e.read32(dev + o) for o in (0x2C, 0x30, 0x48, 0x4C, 0x5C)]
            log1 = list(e.log)
            e.log.clear()
            h = e.call(fh, this=wnd, args=[c['px'], c['py']])
            outs.append((ret, dv, log1, u32(h), list(e.log)))
        res[kind] = outs
    bad = 0
    for i, (a, b) in enumerate(zip(res['stock'], res['ours'])):
        if a != b:
            bad += 1
            if bad <= 3:
                print('case', i, cases[i]['nslots'])
                print(' stock', str(a)[:700])
                print(' ours ', str(b)[:700])
    print('equip paint cases', n, 'differ', bad)


if __name__ == '__main__':
    main(int(sys.argv[1]), int(sys.argv[2]))
