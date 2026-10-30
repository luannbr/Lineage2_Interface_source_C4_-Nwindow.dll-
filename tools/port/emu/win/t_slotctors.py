# Differential test of the constructors and destructors of the slot-window family (stock against ours under unicorn): the object is filled
# with a poison pattern, the function runs, the whole object is compared (the vtable pointer apart, which is compared with the class's
# vtable on each side) with the log of the callees (base constructors, FindTexture, the tool tip destructors, the TArray imports).
#   NCSkillItemWnd 0x100AE9D0 / 0x100AEA70, NCGMSkillItemWnd 0x1008FC30 / 0x1008FCD0, NCSkillWnd 0x100AEC30, NCGMSkillWnd 0x1008FD10,
#   NCGMInvenItemWnd 0x1008F6F0, NCGMPledgeWnd 0x10090460, NCHennaInfoWnd 0x10093A60 (ctor) / 0x10093B40 (dtor), NCManorShopWnd 0x100B0E70,
#   NActionItem 0x1004FF60 (reads an NItemInfo), NCGMQuestWnd 0x10090360 (dtor)
#   python t_slotctors.py [cases] [seed] [name,name,...]
import sys, os, random, struct, json
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import tip4          # clears the edit box services of e4 (its side effect is wanted)
from gw import *
import gw
import stubs
from slotsyms import auto_syms

stubs.IMPORT_POP.update({'?ConditionalDestroy@UObject@@QAEHXZ': 0, '??1UObject@@UAE@XZ': 0})

# name: (stock va, number of arguments (dwords), class, vtable of the stock class, kind)
FUNCS = {
    'skillitem0': (0x100AE9D0, 0, 'NCSkillItemWnd', 0x101B4448), 'skillitem6': (0x100AEA70, 6, 'NCSkillItemWnd', 0x101B4448),
    'gmskillitem0': (0x1008FC30, 0, 'NCGMSkillItemWnd', 0x101B4E30), 'gmskillitem6': (0x1008FCD0, 6, 'NCGMSkillItemWnd', 0x101B4E30),
    'skillwnd': (0x100AEC30, 0, 'NCSkillWnd', 0x101BAFE0), 'gmskillwnd': (0x1008FD10, 0, 'NCGMSkillWnd', 0x101B4F78),
    'gminven': (0x1008F6F0, 0, 'NCGMInvenItemWnd', 0x101B4A60), 'gmpledge': (0x10090460, 0, 'NCGMPledgeWnd', 0x101B5628),
    'henna': (0x10093A60, 0, 'NCHennaInfoWnd', 0x101B62C0), 'manorshop': (0x100B0E70, 0, 'NCManorShopWnd', 0x101BB550),
    'actionitem': (0x1004FF60, 1, 'NActionItem', 0),
    'henna_dtor': (0x10093B40, 0, 'NCHennaInfoWnd', 0x101B62C0), 'gmquest_dtor': (0x10090360, 0, 'NCGMQuestWnd', 0x101B53B0),
    # the page itself (its constructors are the callees of the GM page's, so they run in a world of their own: group B)
    'inven0': (0x10099790, 0, 'NCInvenItemWnd', 0x101B4300), 'inven6': (0x100997B0, 6, 'NCInvenItemWnd', 0x101B4300),
    'inven_dtor': (0x1008F380, 0, 'NCInvenItemWnd', 0x101B4300),
}
GROUP_B = ('inven0', 'inven6', 'inven_dtor')
CALLEES = {
    0x100201B0: ('NCItemWndCtor', 0, 1), 0x100202D0: ('NCItemWndCtor6', 0x18, 1), 0x1002B450: ('FindTexture', 8, 0),
    0x10036CE0: ('NCWndCtor', 0, 1), 0x1001F7F0: ('NItemEntryCtor', 0, 1), 0x10099790: ('InvenCtor', 0, 1),
    0x100508D0: ('PledgeCtor', 0, 1), 0x10105120: ('ShopCtor', 0, 1),
    0x10002FB0: ('NCWndDtor', 0, 0), 0x10002E40: ('ToolTipDtor', 0, 0),
}
CALLEES_B = {0x100201B0: CALLEES[0x100201B0], 0x100202D0: CALLEES[0x100202D0], 0x1002B450: CALLEES[0x1002B450], }
OURS_SYMS = {0x1002B450: '?FindTexture@NCObject@@QAEPAVUTexture@@PBGH@Z'}
VT_SYM = lambda cls: '??_7%s@@6B@' % cls
SIZES = {}
for v in json.load(open(os.path.join(ROOT, 'analysis', 'classes.json'), encoding='utf-8')):
    if isinstance(v, dict) and 'cpp' in v:
        SIZES[v['cpp']] = v['size']
SIZES['NActionItem'] = 0x1C48


class CW(GW):
    def __init__(self, kind, callees=None):
        callees = callees or CALLEES
        syms = {}
        if kind == 'ours':
            syms = auto_syms(callees, LOGFILE)
            syms.update({k: v for k, v in OURS_SYMS.items() if k in callees})
        GW.__init__(self, kind, callees, syms)
        self.rets['FindTexture'] = lambda e, t, a: 0x7777 + len(e.wstr(a[0]) or '')
        self.base = 0

    def do_import(self, nm):
        if nm == '?ConditionalDestroy@UObject@@QAEHXZ':
            self.log.append(('ConditionalDestroy', self.nm(self.this())))
            return 1
        if nm == '??1UObject@@UAE@XZ':
            self.log.append(('~UObject', self.nm(self.this())))
            return None
        return GW.do_import(self, nm)

    def dispatch(self, name):
        kind, nm = name.split(':', 1)
        if kind == 'cb' and nm in ('ToolTipDtor', 'NItemEntryCtor', 'QuestTreeDtor', 'NCWndDtor', 'ScrollWndDtor') and self.base:
            # the object's interior addresses as offsets
            va, pop, rthis = self.cb[nm]
            self.last_pop = pop
            t = self.this()
            self.log.append((nm, hex(t - self.base) if self.base <= t < self.base + 0x4000 else t))
            return t if rthis else 0
        return GW.dispatch(self, name)


def put_info(e, p, c):
    """an NItemInfo (C4, 0x19B4 bytes) for NActionItem's constructor"""
    e.uc.mem_write(p, bytes(0x19B4))
    e.write_wstr_at(p + 0x0, c['name'])
    e.write_wstr_at(p + 0x400, c['icon'])
    e.write_wstr_at(p + 0x900, c['desc'])
    e.write32(p + 0x1900, c['tex'])
    e.write32(p + 0x1914, c['param'])
    e.write32(p + 0x1920, c['id'])


def run(e, A, c):
    mark = len(e.log)
    name = c['f']
    va, nargs, cls, stockvt = A[name]
    size = SIZES[cls]
    obj = e.alloc(size + 16)
    e.uc.mem_write(obj, (bytes(size + 16) if name == 'inven_dtor' else bytes([0xCD]) * (size + 16)))
    e.objs[obj] = 'obj'
    e.base = obj
    args = []
    if name == 'actionitem':
        info = e.alloc(0x19B4)
        put_info(e, info, c)
        args = [info]
        e.objs[info] = 'info'
    elif nargs == 6:
        args = c['args']
    if name == 'inven_dtor':
        # the page's Items (+0x12C): the stock runs ~NCItemWnd for real, which empties the array
        n = c['n']
        data = e.alloc(16 * max(n, 1))
        e.write32(obj + 0x12C, data if n else 0)
        e.write32(obj + 0x130, n)
        e.write32(obj + 0x134, n + c['slack'])
        e.objs[data] = 'items'
    if name == 'gmquest_dtor':
        # the Counts array (+0x1A4): data, num, max
        n = c['n']
        data = e.alloc(16 * max(n, 1))
        e.write32(obj + 0x1A4, data if n else 0)
        e.write32(obj + 0x1A8, n)
        e.write32(obj + 0x1AC, n + c['slack'])
        e.objs[data] = 'counts'
    ret = e.call(va, this=obj, args=args)
    mem = bytes(e.uc.mem_read(obj, size + 16))
    words = struct.unpack('<%dI' % ((size + 16) // 4), mem)
    snap = [('vt' if i == 0 else w) for i, w in enumerate(words)]
    out = dict(ret=ret if ret != obj else 'this', log=e.log[mark:], obj=snap)
    if not name.endswith('_dtor') and stockvt:
        vtv = words[0]
        if e.kind == 'stock':
            out['vtable'] = 'ok' if vtv == stockvt else 'stock vtable %x' % vtv
        else:
            out['vtable'] = 'ok' if vtv == e.symbols[VT_SYM(cls)] else 'ours vtable %x' % vtv
    return out


def rand_case(r, f):
    c = dict(f=f)
    c['args'] = [r.randint(0, 40) for _ in range(6)]
    c['name'] = r.choice(['', 'Sit', 'Social Bow', 'A' * 40])
    c['icon'] = r.choice(['', 'icon.sit', 'L2UI.actionwnd.act_' + 'b' * 30])
    c['desc'] = r.choice(['', 'Sit down.', 'D' * 200])
    c['tex'] = r.randint(0, 1 << 30)
    c['param'] = r.choice([-1, 0, 5, 17])
    c['id'] = r.choice([-1, 0, 3, 28, 1000])
    c['n'] = r.choice([0, 1, 3, 8])
    c['slack'] = r.choice([0, 0, 4])
    return c


def main():
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 10
    seed = int(sys.argv[2]) if len(sys.argv) > 2 else 1
    which = sys.argv[3].split(',') if len(sys.argv) > 3 else list(FUNCS)
    es = CW('stock')
    eo = CW('ours')
    esb = CW('stock', CALLEES_B)
    eob = CW('ours', CALLEES_B)
    lm = log_map()
    for e_ in (es, eo, esb, eob):
        e_.heap_mark = e_.heap_ptr
    As = dict(FUNCS)
    Ao = {}
    missing = []
    for k, v in FUNCS.items():
        if v[0] in lm:
            Ao[k] = (lm[v[0]],) + v[1:]
        else:
            missing.append(k)
    r = random.Random(seed)
    total = 0
    for f in which:
        if f in missing:
            print('%-14s: not hooked in our build, skipped' % f)
            continue
        cases = [rand_case(r, f) for _ in range(n)]
        if f in GROUP_B:
            total += compare(f, esb, eob, run, cases, As, Ao)
        else:
            total += compare(f, es, eo, run, cases, As, Ao)
    print('TOTAL problems', total)
    sys.exit(1 if total else 0)


if __name__ == '__main__':
    main()
