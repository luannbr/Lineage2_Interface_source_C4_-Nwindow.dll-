"""Differential test of the item records (C4 layout): NItemInfo::NItemInfo (0x1004CD30), NItemEntry::NItemEntry() (0x1001F7F0),
NItemEntry::NItemEntry(NItemInfo*) (0x1001F960), Reset (0x1001E5E0), Copy (0x1001FE00), MakeDisplayName (0x1001E470) and the
small getters, stock against ours on random records: every byte of the record is compared (the tool tip object at +0x1BC0 is built by
a stub on both sides: NToolTipInfo's constructor 0x10002D90 and its assignment 0x1001FCC0 only return).
usage: python tools/port/emu/t_entry.py <cases> <seed>   (needs build/bin/Release/NWindowDecomp.dll + .map)"""
import sys, os, random, struct
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from emu import *

INFO_SIZE, ENTRY_SIZE = 0x19B4, 0x1C40
STOCK = {'info_ctor': 0x1004CD30, 'entry_ctor': 0x1001F7F0, 'entry_ctor_info': 0x1001F960, 'reset': 0x1001E5E0,
         'copy': 0x1001FE00, 'mkname': 0x1001E470, 'getname': 0x1001E460, 'getadd': 0x1001E510, 'getdesc': 0x1001E530,
         'stackable': 0x1001E5B0, 'sel': 0x1001E580, 'unsel': 0x1001E5A0}
OURS = {'info_ctor': '??0NItemInfo@@QAE@XZ', 'entry_ctor': '??0NItemEntry@@QAE@XZ', 'entry_ctor_info': '??0NItemEntry@@QAE@PAUNItemInfo@@@Z',
        'reset': '?Reset@NItemEntry@@', 'copy': '?Copy@NItemEntry@@', 'mkname': '?MakeDisplayName@NItemEntry@@',
        'getname': '?GetName@NItemEntry@@', 'getadd': '?GetAdditionalName@NItemEntry@@', 'getdesc': '?GetDescription@NItemEntry@@',
        'stackable': '?IsStackable@NItemEntry@@', 'sel': '?Select@NItemEntry@@', 'unsel': '?Unselect@NItemEntry@@'}
STUB_RET = {0x10002D90: bytes([0xC3]), 0x1001FCC0: bytes([0xC2, 0x04, 0x00])}   # NToolTipInfo's constructor and assignment: nothing happens on either side


class EntryEmu(Emu):
    def __init__(self, kind):
        super().__init__(kind)
        self.last_pop = 0
        self.f = {}
        if kind == 'stock':
            self.f = dict(STOCK)
            for va, code in STUB_RET.items():
                self.uc.mem_write(va, code)
        else:
            for k, pre in OURS.items():
                c = [v for s, v in self.symbols.items() if s.startswith(pre)]
                assert len(c) == 1, (k, c)
                self.f[k] = c[0]
            for va, code in STUB_RET.items():
                try:
                    self.uc.mem_write(NWBASE + va - 0x10000000, code)
                except sa.NoCounterpart:
                    pass   # standalone: no function at that stock address (the assignment is patched through its symbol below)
            # the tool tip's copy assignment is compiled code in ours (NToolTip.cpp) that Copy calls directly: stubbed like the hook address
            for sym, addr in self.symbols.items():
                if sym.startswith('??4NToolTipInfo@@'):
                    self.uc.mem_write(addr, bytes([0x8B, 0xC1, 0xC2, 0x04, 0x00]))   # mov eax, ecx; ret 4

    def dispatch(self, name):
        kind, nm = name.split(':', 1)
        assert kind == 'imp', name
        self.last_pop = 0
        a = self.stack_arg
        if nm == '??0FString@@QAE@XZ':
            t = self.reg(UC_X86_REG_ECX)
            self.uc.mem_write(t, bytes(12))
            return t
        if nm in ('??1FString@@QAE@XZ', '??1FArray@@QAE@XZ'):
            return 0
        if nm == '?appStrcpy@@YAPAGPAGPBG@Z':
            self.write_wstr_at(a(0), self.wstr(a(1)))
            return a(0)
        if nm == '?appStrcat@@YAPAGPAGPBG@Z':
            self.write_wstr_at(a(0), self.wstr(a(0)) + self.wstr(a(1)))
            return a(0)
        if nm == '?appStrlen@@YAHPBG@Z':
            return len(self.wstr(a(0)))
        if nm == '?appSprintf@@YAHPAGPBGZZ':
            fmt = self.wstr(a(1))
            assert fmt == '+%d', fmt
            s = '+%d' % self.rds32(self.reg(UC_X86_REG_ESP) + 12)
            self.write_wstr_at(a(0), s)
            return len(s)
        raise RuntimeError('import ' + nm)


def rnd_wstr(r, n):
    k = r.choice([0, 0, 1, 3, 10, n - 1])
    return ''.join(chr(r.choice([0x41, 0x61, 0x2B, 0x31, 0xAC00, 0x3042, 0x7F]) + r.randint(0, 20)) for _ in range(k))


def make_info(e, r, seed_bytes):
    p = e.alloc(INFO_SIZE)
    e.uc.mem_write(p, seed_bytes)
    return p


def info_bytes(r):
    b = bytearray(r.getrandbits(8) for _ in range(INFO_SIZE))
    # the string parts: proper wide strings
    for off, n in ((0, 256), (0x200, 256), (0x400, 128), (0x500, 128), (0x600, 128), (0x700, 128), (0x800, 128), (0x900, 2048)):
        s = rnd_wstr(r, n).encode('utf-16-le') + b'\0\0'
        b[off:off + len(s)] = s
    for off in (0x1914, 0x1918, 0x192C, 0x1938, 0x193C, 0x1974, 0x19A4, 0x1994):
        if r.random() < 0.5:
            b[off:off + 4] = struct.pack('<i', -1)
    return bytes(b)


def entry_bytes(r):
    b = bytearray(r.getrandbits(8) for _ in range(ENTRY_SIZE))
    for off, n in ((0x14, 256), (0x214, 256), (0x414, 128), (0x514, 128), (0x614, 128), (0x714, 128), (0x814, 128), (0x914, 2048), (0x1914, 256)):
        s = rnd_wstr(r, n).encode('utf-16-le') + b'\0\0'
        b[off:off + len(s)] = s
    b[0x1B14:0x1B18] = struct.pack('<i', r.choice([-1, 0, 1, 2, 3, 5]))
    b[0x1B18:0x1B1C] = struct.pack('<i', r.choice([-1, 0, 7, 0xD, 0xE, 3]))
    b[0x1B44:0x1B48] = struct.pack('<i', r.choice([0, 1, 5, 12, 99]))
    b[0x1B4C:0x1B50] = struct.pack('<i', r.choice([0, 1, 2, 3, 4]))
    return bytes(b)


def compare(what, a, b, skip=((0x1BC0, 0x1C40),), limit=ENTRY_SIZE):
    bad = []
    for i in range(min(len(a), len(b), limit)):
        if a[i] != b[i] and not any(lo <= i < hi for lo, hi in skip):
            bad.append(i)
    if bad:
        offs = sorted(set(i & ~3 for i in bad))
        print('DIFF %s at %s' % (what, ' '.join('+0x%X' % o for o in offs[:16])))
        return 1
    return 0


def main():
    cases = int(sys.argv[1]) if len(sys.argv) > 1 else 50
    seed = int(sys.argv[2]) if len(sys.argv) > 2 else 1
    r = random.Random(seed)
    S, O = EntryEmu('stock'), EntryEmu('ours')
    bad = 0
    for c in range(cases):
        # 1. NItemInfo::NItemInfo on a dirty record
        ib = info_bytes(r)
        out = []
        for e in (S, O):
            p = make_info(e, r, ib)
            e.call(e.f['info_ctor'], this=p)
            out.append(bytes(e.uc.mem_read(p, INFO_SIZE)))
        # the two bytes after the word at +0x1998 are left alone by the constructor
        bad += compare('NItemInfo ctor', out[0], out[1], skip=(), limit=INFO_SIZE)
        # 2. NItemEntry::NItemEntry() on a dirty record
        eb = entry_bytes(r)
        out = []
        for e in (S, O):
            p = e.alloc(ENTRY_SIZE)
            e.uc.mem_write(p, eb)
            e.call(e.f['entry_ctor'], this=p)
            out.append(bytes(e.uc.mem_read(p, ENTRY_SIZE)))
        bad += compare('NItemEntry ctor', out[0], out[1])
        # 3. NItemEntry::NItemEntry(NItemInfo*) from a random Info
        ib = info_bytes(r)
        out = []
        for e in (S, O):
            ip = make_info(e, r, ib)
            p = e.alloc(ENTRY_SIZE)
            e.uc.mem_write(p, eb)
            e.call(e.f['entry_ctor_info'], this=p, args=[ip])
            out.append(bytes(e.uc.mem_read(p, ENTRY_SIZE)))
        bad += compare('NItemEntry(Info)', out[0], out[1])
        # 4. Reset
        out = []
        for e in (S, O):
            p = e.alloc(ENTRY_SIZE)
            e.uc.mem_write(p, eb)
            e.call(e.f['reset'], this=p)
            out.append(bytes(e.uc.mem_read(p, ENTRY_SIZE)))
        bad += compare('Reset', out[0], out[1])
        # 5. Copy src -> dst
        sb = entry_bytes(r)
        out = []
        for e in (S, O):
            sp = e.alloc(ENTRY_SIZE)
            e.uc.mem_write(sp, sb)
            p = e.alloc(ENTRY_SIZE)
            e.uc.mem_write(p, eb)
            e.call(e.f['copy'], this=p, args=[sp])
            out.append((bytes(e.uc.mem_read(p, ENTRY_SIZE)), bytes(e.uc.mem_read(sp, ENTRY_SIZE))))
        bad += compare('Copy dst', out[0][0], out[1][0])
        bad += compare('Copy src', out[0][1], out[1][1])
        # 6. the display name and the small getters
        res = []
        for e in (S, O):
            p = e.alloc(ENTRY_SIZE)
            e.uc.mem_write(p, sb)
            row = [e.wstr(e.call(e.f['mkname'], this=p)), bytes(e.uc.mem_read(p, ENTRY_SIZE))]
            for k in ('getname', 'getadd', 'getdesc'):
                v = e.call(e.f[k], this=p)
                row.append((v - p) if v else 0)
            row.append(e.call(e.f['stackable'], this=p) & 0xFF)
            res.append(row)
        if res[0][0] != res[1][0] or res[0][2:] != res[1][2:]:
            print('DIFF display name / getters: %r vs %r; %r vs %r' % (res[0][0], res[1][0], res[0][2:], res[1][2:]))
            bad += 1
        bad += compare('MakeDisplayName memory', res[0][1], res[1][1])
    print('t_entry: %d cases, %d differences' % (cases, bad))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
