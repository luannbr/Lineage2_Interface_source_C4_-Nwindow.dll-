# STALE (C4, sa-emu agent `old`): the C4 NCNpcView_CharacterSelectWnd::OnSpawnTest (0x100C1410) is a different function (combo / edit box driven, NSpawnTestParam); OnSummon 0x100C0FF0 has no successor; C2 model, does not run.
"""Differential test of NCNpcView_CharacterSelectWnd::OnSpawnTest (stock 0x1009E230 against ours) and of OnSummon (0x1009DE10) for
comparison: the engine is a model (the viewport chain of the console, a ULevel whose SpawnActor, SingleLineCheck and FarMoveActor
are stubs logging their arguments bit by bit, the pawn's setPhysics and SetMeshes, StaticLoadClass, the User constructor and
SetPawnResource, FName's constructor, SpawnEnterEvent). Random player positions and rotations, a missing viewport, pawn or level,
a pawn class that does not load, and a ground trace that hits at a random try (or never, or with a hit close to the start) are run
by both; the logs and the objects written must be equal.
usage: python tools/port/emu/t_spawn.py <cases> <seed>   (needs build/bin/Release/NWindowDecomp.dll and its .map; NWD_OURS=<dir> tests a snapshot)"""
import sys, random, os, struct
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import emu as _emu
import stubs
from stubs import *
from t_recipe import RecipeEmu, sym

SLC = '?StaticLoadClass@UObject@@SAPAVUClass@@PAV2@PAV1@PBG2KPAVUPackageMap@@@Z'
stubs.IMPORT_POP.update({SLC: 0, '??0User@@QAE@XZ': 0, '?SetPawnResource@User@@QAEXXZ': 0,
                         '??0FName@@QAE@PBGW4EFindName@@@Z': 8, '?SpawnEnterEvent@APawn@@QAEXXZ': 0})
stubs.FN_POP.update({'LvSpawn': 0x34, 'LvTrace': 0x20, 'LvFarMove': 0x1C, 'PwPhysics': 0x14, 'PwMeshes': 0xC})

PAWN_SIZE = 0x1600


def fbits(x):
    return struct.unpack('<I', struct.pack('<f', x))[0]


class SpawnEmu(RecipeEmu):
    def __init__(self, kind, case):
        super().__init__(kind)
        self.case = case
        self.trace_calls = 0
        self.spawned = []
        self.load_ok = case['class_ok']
        # vtables
        self.pawn_vt = self.alloc(0x200 * 4)
        self.level_vt = self.alloc(0x100 * 4)
        for vt, slot, name, off in ((self.pawn_vt, 111, 'PwPhysics', 0xB000), (self.pawn_vt, 51, 'PwMeshes', 0xB010),
                                    (self.level_vt, 47, 'LvSpawn', 0xB020), (self.level_vt, 56, 'LvTrace', 0xB030),
                                    (self.level_vt, 41, 'LvFarMove', 0xB040)):
            addr = STUB_BASE + off
            self.uc.mem_write(addr, b'\xC3')
            self.stubs[addr] = ('fn:' + name, None)
            self.write32(vt + 4 * slot, addr)
        # the viewport chain: console+0x44 -> engine, +0x4C -> client, +0x2C -> viewports data, [0] -> viewport, +0x30 -> actor
        eng, cli, vpd, vp = self.alloc(0x100), self.alloc(0x100), self.alloc(0x10), self.alloc(0x100)
        for o in (eng, cli, vpd, vp):
            self.uc.mem_write(o, bytes(0x10))
        self.actor = self.alloc(0x1500)
        self.uc.mem_write(self.actor, bytes(0x1500))
        self.level = self.alloc(0x100)
        self.uc.mem_write(self.level, bytes(0x100))
        self.write32(self.level, self.level_vt)
        self.player_pawn = self.alloc(PAWN_SIZE)
        self.uc.mem_write(self.player_pawn, bytes(PAWN_SIZE))
        self.write32(self.player_pawn, self.pawn_vt)
        for i, v in enumerate(case['loc']):
            self.write32(self.player_pawn + 0x1B0 + 4 * i, fbits(v))
        for i, v in enumerate(case['rot']):
            self.write32(self.player_pawn + 0x1BC + 4 * i, v)
        if case['viewport']:
            self.write32(self.console + 0x44, eng)
            self.write32(eng + 0x4C, cli)
            self.write32(cli + 0x2C, vpd)
            self.write32(vpd, vp)
            self.write32(vp + 0x30, self.actor)
        else:
            self.write32(self.console + 0x44, eng)
            self.write32(eng + 0x4C, cli)
            self.write32(cli + 0x2C, vpd)
            self.write32(vpd, vp)
            self.write32(vp + 0x30, 0)
        if case['has_pawn']:
            self.write32(self.actor + 0x394, self.player_pawn)
        if case['has_level']:
            self.write32(self.actor + 0xD8, self.level)

    def tok(self, v):
        """a pointer made comparable between the two images: import stubs by name, heap objects by their offset"""
        if v in self.import_names:
            return ('imp', self.import_names[v])
        return v

    def do_import(self, nm):
        t = self.this()
        if nm == SLC:
            esp = self.reg(UC_X86_REG_ESP)
            base = self.read32(esp + 4)
            name = self.wstr(self.read32(esp + 12))
            flags = self.read32(esp + 20)
            self.log.append(('StaticLoadClass', self.tok(base), self.read32(esp + 8), name, self.read32(esp + 16), flags,
                             self.read32(esp + 24)))
            if name == 'LineageWarrior.LineagePawn' and not self.load_ok:
                return 0
            return self.static_str('class:' + name) if False else (0x6F000000 + (sum(ord(c) for c in name) & 0xFFFF))
        if nm == '??0User@@QAE@XZ':
            self.uc.mem_write(t, bytes(0x1F8))
            self.log.append(('User::User',))
            return t
        if nm == '?SetPawnResource@User@@QAEXXZ':
            self.log.append(('SetPawnResource', bytes(self.uc.mem_read(t, 0x30)).hex(), self.read32(t + 0x140)))
            return None
        if nm == '??0FName@@QAE@PBGW4EFindName@@@Z':
            s = self.wstr(self.arg(0))
            self.write32(t, 0x4000 + (sum(ord(c) for c in s) & 0xFFF))
            self.log.append(('FName', s, self.arg(1)))
            return t
        if nm == '?SpawnEnterEvent@APawn@@QAEXXZ':
            self.log.append(('SpawnEnterEvent', t))
            return None
        return super().do_import(nm)

    def do_fn(self, nm):
        t = self.this()
        esp = self.reg(UC_X86_REG_ESP)
        a = lambda i: self.read32(esp + 4 + 4 * i)
        if nm == 'LvSpawn':
            args = [a(i) for i in range(13)]
            self.log.append(('SpawnActor', t == self.level, self.tok(args[0]), args[1:8], args[8:11], [self.tok(x) for x in args[11:13]]))
            obj = self.alloc(PAWN_SIZE)
            self.uc.mem_write(obj, bytes(PAWN_SIZE))
            self.write32(obj, self.pawn_vt)
            self.spawned.append(obj)
            return obj
        if nm == 'LvTrace':
            hit = a(0)
            before = bytes(self.uc.mem_read(hit, 0x30)).hex()
            args = [a(i) for i in range(1, 8)]
            end = [self.read32(args[1] + 4 * i) for i in range(3)]
            start = [self.read32(args[2] + 4 * i) for i in range(3)]
            self.log.append(('SingleLineCheck', before, args[0], end, start, args[3], args[4:7]))
            k = self.trace_calls
            self.trace_calls += 1
            if k == self.case['hit_at']:
                self.write32(hit + 8, fbits(self.case['hit_loc'][0]))
                self.write32(hit + 12, fbits(self.case['hit_loc'][1]))
                self.write32(hit + 16, fbits(self.case['hit_loc'][2]))
                self.write32(hit + 0x24, fbits(self.case['hit_time']))
            return 1
        if nm == 'LvFarMove':
            self.log.append(('FarMoveActor', [a(i) for i in range(7)]))
            return 1
        if nm == 'PwPhysics':
            self.log.append(('setPhysics', t, [a(i) for i in range(5)]))
            return None
        if nm == 'PwMeshes':
            self.log.append(('SetMeshes', t, [a(i) for i in range(3)]))
            return None
        return super().do_fn(nm)


def rand_case(r):
    def f(lo, hi):
        return round(r.uniform(lo, hi), r.choice([0, 1, 3]))
    return {
        'loc': [f(-5000, 5000), f(-5000, 5000), f(-500, 500)],
        'rot': [r.randint(-70000, 70000), r.randint(0, 65535), r.randint(-100, 100)],
        'viewport': r.random() < 0.93, 'has_pawn': r.random() < 0.93, 'has_level': r.random() < 0.93,
        'class_ok': r.random() < 0.9,
        'hit_at': r.choice([0, 1, 2, 5, 17, 29, 30, 99, 99]),
        'hit_loc': [f(-5000, 5000), f(-5000, 5000), r.choice([f(-500, 500), f(-500, 500), 0.0])],
        'hit_time': r.choice([0.0, 0.05, 0.1, 0.15, 0.158, 0.16, 0.2, 0.5, 0.99, 1.0]),
    }


def run(kind, which, cases):
    outs = []
    for c in cases:
        e = SpawnEmu(kind, c)
        if kind == 'stock':
            fn = {'spawn': 0x1009E230}[which]
        else:
            fn = sym(e, '?OnSpawnTest@NCNpcView_CharacterSelectWnd@@')
        wnd = e.alloc(0x400)
        e.uc.mem_write(wnd, bytes(0x400))
        # pad the stack so the two images use the same addresses for the objects the function builds on it
        ret = e.call(fn, this=wnd, args=[])
        objs = [bytes(e.uc.mem_read(o, PAWN_SIZE)).hex() for o in e.spawned]
        outs.append((ret, list(e.log), objs))
    return outs


def main(n, seed):
    r = random.Random(seed)
    cases = [rand_case(r) for _ in range(n)]
    a = run('stock', 'spawn', cases)
    b = run('ours', 'spawn', cases)
    bad = 0
    spawned = sum(1 for x in a if any(l[0] == 'SpawnEnterEvent' for l in x[1]))
    for i, (x, y) in enumerate(zip(a, b)):
        if x != y:
            bad += 1
            if bad <= 3:
                print('case', i, cases[i])
                print(' ret', x[0], y[0])
                for j, (p, q) in enumerate(zip(x[1], y[1])):
                    if p != q:
                        print('  log differs at', j, '\n   stock', p, '\n   ours ', q)
                        break
                else:
                    print('  log lengths', len(x[1]), len(y[1]))
                for j, (p, q) in enumerate(zip(x[2], y[2])):
                    if p != q:
                        for off in range(0, len(p), 2):
                            if p[off:off + 2] != q[off:off + 2]:
                                print('  object', j, 'differs at byte 0x%x' % (off // 2), p[off:off + 8], q[off:off + 8])
                                break
                        break
    print('spawn cases', n, 'reached SpawnEnterEvent', spawned, 'differ', bad)


if __name__ == '__main__':
    main(int(sys.argv[1]), int(sys.argv[2]))
