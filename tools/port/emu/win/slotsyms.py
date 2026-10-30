# Helper of the inven lane's tests: the compiled member of a hooked function is a different piece of code from the hook's forwarder
# (log_map() gives the forwarder, which may have the member inlined), and our own code calls the member. GW patches the addresses it is
# told of, so a callee needs its decorated symbol too: found here from the name the smoke log gives the stock address.
import re, os
import emu as _emu


def read_names(logfile):
    names = {}
    for ln in open(logfile, encoding='latin-1'):
        m = re.match(r'^\S+\s+([0-9a-f]{8}) (.*?)\s+ours=([0-9a-f]+) ok', ln)
        if m:
            names[int(m.group(1), 16)] = m.group(2)
    return names


def read_symbols(mapfile):
    syms = {}
    for ln in open(mapfile, encoding='latin-1'):
        m = re.match(r'\s*[0-9a-f]{4}:[0-9a-f]{8}\s+(\S+)\s+([0-9a-f]{8})\s', ln)
        if m:
            syms.setdefault(m.group(1), int(m.group(2), 16))
    return syms


_CACHE = {}


def install_cache():
    """the smoke log is parsed once per process (GW parses it for every callee of every emulator otherwise: minutes in a mutation run)"""
    import gw, e4
    if getattr(gw, '_slot_cached', False):
        return
    orig = gw.log_map
    box = []

    def cached():
        if not box:
            box.append(orig())
        return box[0]
    gw.log_map = cached
    e4.log_map = cached
    gw._slot_cached = True


def auto_syms(callees, logfile, verbose=True):
    """{stock va: decorated symbol of the compiled member} for the callees whose name in the smoke log gives exactly one symbol"""
    key = (tuple(sorted(callees)), logfile)
    if key in _CACHE:
        return dict(_CACHE[key])
    names = read_names(logfile) if 'names' not in _CACHE else _CACHE['names']
    _CACHE['names'] = names
    syms = read_symbols(_emu.OURS_MAP) if 'syms' not in _CACHE else _CACHE['syms']
    _CACHE['syms'] = syms
    out = {}
    for va in callees:
        n = names.get(va)
        if not n:
            continue
        n = n.split('(')[0]
        cls, _, meth = n.partition('::')
        if not meth:
            continue
        if meth == cls:
            prefix = '??0' + cls + '@@'
        elif meth == '~' + cls:
            prefix = '??1' + cls + '@@'
        else:
            prefix = '?' + meth + '@' + cls + '@@'
        cands = [s for s in syms if s.startswith(prefix)]
        if len(cands) == 1:
            out[va] = cands[0]
        elif verbose:
            print('auto_syms: %x %s -> %d candidates %s' % (va, n, len(cands), cands[:3]))
    _CACHE[key] = dict(out)
    return out
