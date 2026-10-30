# Differential test (stock vs ours under unicorn) of the tree view entry of the C4 image, wave C lane debt2 (src/nwindow/NCTVItem.cpp):
#   NTVItemData::NTVItemData 0x10032BF0, operator= 0x10032C90, NCTVItem::NCTVItem 0x10033D60, Expand 0x10032E20, Collapse 0x10032E70,
#   AddChild 0x10033800, NTVItemArray::RemoveItem 0x10033D20, ~NTVItemArray 0x10033C90, DeleteAllChildren 0x10033E50,
#   DeleteChildItem 0x10033EB0, ~NCTVItem 0x10033DB0.
# Each case builds a random tree (entries of 0x2880 bytes with random contents; children arrays of random sizes with NULL holes) in
# the emulated heap, runs the function on both sides and compares every byte of every block the tree owns, the return value and the
# calls the body makes (GMalloc.Free of each deleted entry, in order). FArray::Add / Insert / Realloc / Remove are the emulator's.
#   python tools/port/emu/win/t_tvitem.py [cases] [seed]     (needs build/bin/Release/NWindowDecomp.dll + .map and the smoke log)
import sys, os, random, struct
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from gw import *
import stubs

ITEM = 0x2880
OFF = dict(Parent=0x285C, Data=0x2860, Num=0x2864, Max=0x2868, Level=0x286C, Expanded=0x2870, Visible=0x2878)
# name: (stock va, argument kinds)  (this is always ecx; kinds: 'item' a new entry, 'idx' an index or -1, 'ptr' a pointer to a pointer)
FUNCS = {
    'data_ctor': (0x10032BF0, ()),
    'data_assign': (0x10032C90, ('data',)),
    'item_ctor': (0x10033D60, ('level',)),
    'expand': (0x10032E20, ()),
    'collapse': (0x10032E70, ()),
    'addchild': (0x10033800, ('child', 'idx')),
    'removeitem': (0x10033D20, ('childptr',)),
    'array_dtor': (0x10033C90, ()),
    'delall': (0x10033E50, ()),
    'delchild': (0x10033EB0, ('child_of',)),
    'item_dtor': (0x10033DB0, ()),
}
stubs.IMPORT_POP.update({'?Insert@FArray@@QAEXHHH@Z': 12})
RET_MATTERS = {'data_ctor', 'data_assign', 'item_ctor', 'addchild', 'removeitem'}   # this / the child / the count removed


class CW(GW):
    def __init__(self, kind):
        GW.__init__(self, kind, {}, {})

    def dispatch(self, name):
        kind, nm = name.split(':', 1)
        if kind == 'imp' and nm == '?Insert@FArray@@QAEXHHH@Z':
            t = self.this()
            self.last_pop = 12
            idx, cnt, size = self.arg(0), self.arg(1), self.arg(2)
            data, num, mx = self.read32(t), self.read32(t + 4), self.read32(t + 8)
            num2 = num + cnt
            if num2 > mx:
                mx = num2 + 3 * num2 // 8 + 16
                nd = self.alloc(mx * size)
                if data:
                    self.uc.mem_write(nd, bytes(self.uc.mem_read(data, num * size)))
                data = nd
            tail = bytes(self.uc.mem_read(data + idx * size, (num - idx) * size)) if num > idx else b''
            self.uc.mem_write(data + (idx + cnt) * size, tail)
            self.uc.mem_write(data + idx * size, bytes(cnt * size))
            self.write32(t, data); self.write32(t + 4, num2); self.write32(t + 8, mx)
            return None
        return GW.dispatch(self, name)


def build_item(e, r, depth, blocks):
    """a random entry (every byte random) with a children array of random content; returns its address"""
    a = e.alloc(ITEM + 16)
    e.uc.mem_write(a, bytes(r.getrandbits(8) for _ in range(ITEM)))
    blocks.append(a)
    n = r.choice([0, 0, 1, 2, 3, 4]) if depth < 2 else 0
    mx = n + r.choice([0, 1, 3])
    arr = e.alloc(max(mx, 1) * 4 + 8)
    kids = []
    for i in range(n):
        if r.random() < 0.15:
            kids.append(0)
        else:
            kids.append(build_item(e, r, depth + 1, blocks))
    for i in range(max(mx, 1)):
        e.write32(arr + 4 * i, kids[i] if i < n else 0xDEAD0000 + i)
    e.write32(a + OFF['Data'], arr if mx else 0)
    e.write32(a + OFF['Num'], n)
    e.write32(a + OFF['Max'], mx)
    e.write32(a + OFF['Visible'], r.choice([0, 1]))
    e.write32(a + OFF['Expanded'], r.choice([0, 1]))
    return a


def kids_of(e, a):
    n = e.read32(a + OFF['Num'])
    d = e.read32(a + OFF['Data'])
    return [e.read32(d + 4 * i) for i in range(n)] if d and 0 <= n < 64 else []


def snap(e, blocks, arrays):
    out = []
    for b in blocks:
        out.append(('blk', bytes(e.uc.mem_read(b, ITEM)).hex()))
    for a, n in arrays:
        out.append(('arr', bytes(e.uc.mem_read(a, n)).hex()))
    return out


def run(e, A, c):
    mark = len(e.log)
    va, kinds = A[c['f']]
    r = random.Random(c['seed'])
    blocks = []
    if c['f'] in ('data_ctor', 'item_ctor'):
        root = e.alloc(ITEM + 16)
        e.uc.mem_write(root, bytes(r.getrandbits(8) for _ in range(ITEM + 16)))
        blocks.append(root)
    elif c['f'] == 'data_assign':
        root = e.alloc(ITEM + 16)
        e.uc.mem_write(root, bytes(r.getrandbits(8) for _ in range(ITEM + 16)))
        blocks.append(root)
    else:
        root = build_item(e, r, 0, blocks)
    args = []
    arrays = []
    for k in kinds:
        if k == 'data':
            src = e.alloc(ITEM + 16)
            # proper wide strings in the four text fields, random numbers elsewhere
            b = bytearray(r.getrandbits(8) for _ in range(ITEM))
            for off, n in ((0x10, 1024), (0x810, 2048), (0x1810, 1024), (0x2010, 1024)):
                s = ''.join(chr(r.choice([0x41, 0x61, 0xAC00, 0x3042, 0x7F]) + r.randint(0, 9)) for _ in range(r.choice([0, 1, 5, 40]))).encode('utf-16-le') + b'\0\0'
                b[off:off + len(s)] = s
            e.uc.mem_write(src, bytes(b))
            blocks.append(src)
            args.append(src)
        elif k == 'level':
            args.append(r.choice([-1, 0, 1, 5, 0x7FFFFFFF]) & 0xFFFFFFFF)
        elif k == 'idx':
            n = e.read32(root + OFF['Num'])
            args.append(r.choice([-1, -1, 0, n // 2, n]) & 0xFFFFFFFF)
        elif k == 'child':
            ch = 0 if r.random() < 0.15 else build_item(e, r, 2, blocks)
            args.append(ch)
        elif k in ('childptr', 'child_of'):
            kids = [x for x in kids_of(e, root) if x]
            if k == 'childptr':
                ch = r.choice(kids + [0, 0xBEEF0000])
                cell = e.alloc(8)
                e.write32(cell, ch)
                args.append(cell)
                arrays.append((cell, 4))
            else:
                ch = r.choice(kids + [0])
                args.append(ch)
        else:
            raise RuntimeError(k)
    this = root
    if c['f'] in ('removeitem', 'array_dtor'):
        this = root + OFF['Data'] - 0   # the children array (NTVItemArray) is a member at +0x2860
    ret = e.call(va, this=this, args=args)
    # every block reachable now plus the array data of the root
    d = e.read32(root + OFF['Data'])
    mx = e.read32(root + OFF['Max'])
    if d and 0 < mx < 64:
        arrays.append((d, mx * 4))
    if c['f'] not in RET_MATTERS:
        ret = None   # a void function: eax is whatever the last call left
    return dict(log=e.log[mark:], ret=ret, state=snap(e, blocks, arrays))


if __name__ == '__main__':
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 20
    seed = int(sys.argv[2]) if len(sys.argv) > 2 else 1
    es = CW('stock')
    eo = CW('ours')
    lm = log_map()
    es.heap_mark, eo.heap_mark = es.heap_ptr, eo.heap_ptr
    As = dict(FUNCS)
    Ao = {k: (lm[v[0]], v[1]) for k, v in FUNCS.items()}
    r = random.Random(seed)
    total = 0
    for f in FUNCS:
        cases = [dict(f=f, seed=r.randint(0, 10 ** 6)) for _ in range(n)]
        total += compare(f, es, eo, run, cases, As, Ao)
    print('TOTAL problems', total)
