"""Mutation test of the harness in the standalone mode: is a test of function X really sensitive to the standalone's code of X?

  python tools/port/emu/mutate_sa.py <stock va of the function, hex> <script> [script args ...] [--max N] [--jobs J]

Copies build/standalone/Release/NWindow.dll, flips ONE conditional jump (jcc rel8 / rel32: the opposite condition) of the standalone function that holds the stock
address (sa.func_map), and runs the test script in the standalone mode (NWD_SA=1, NWD_SA_DLL = the mutated copy). The mutant is KILLED when the script prints a difference
or an exception, SURVIVED when it does not (the branch is not reached by the cases, or the flip is equivalent). Prints one line per mutant and the score.
"""
import sys, os, re, struct, subprocess, shutil, tempfile, concurrent.futures

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import sa
from capstone import Cs, CS_ARCH_X86, CS_MODE_32
import pefile


def function_extent(addr):
    ssym, sbyva, rows = sa.parse_map(sa.SA_MAP)
    starts = sorted({r[0] for r in rows})
    import bisect
    i = bisect.bisect_right(starts, addr)
    end = starts[i] if i < len(starts) else addr + 0x400
    return addr, end


def jcc_sites(dll, lo, hi):
    pe = pefile.PE(dll)
    base = pe.OPTIONAL_HEADER.ImageBase
    out = []
    for s in pe.sections:
        a0 = base + s.VirtualAddress
        if a0 <= lo < a0 + s.Misc_VirtualSize:
            code = s.get_data()[lo - a0:hi - a0]
            fo = s.PointerToRawData + (lo - a0)
            for ins in Cs(CS_ARCH_X86, CS_MODE_32).disasm(code, lo):
                b = bytes(ins.bytes)
                off = fo + (ins.address - lo)
                if len(b) == 2 and 0x70 <= b[0] <= 0x7F:
                    out.append((ins.address, off, ins.mnemonic, b[0] ^ 1))
                elif len(b) == 6 and b[0] == 0x0F and 0x80 <= b[1] <= 0x8F:
                    out.append((ins.address, off + 1, ins.mnemonic, b[1] ^ 1))
            break
    return out


def run_mutant(tmp, dll, site, cmd, env0):
    va, off, mn, newb = site
    copy = os.path.join(tmp, 'NWindow_%08x.dll' % va)
    shutil.copy(dll, copy)
    with open(copy, 'r+b') as f:
        f.seek(off)
        f.write(bytes([newb]))
    env = dict(env0, NWD_SA='1', NWD_SA_DLL=copy, NWD_SA_MAP=sa.SA_MAP, PYTHONIOENCODING='utf-8')
    p = subprocess.run([sys.executable] + cmd, capture_output=True, text=True, errors='replace', env=env, cwd=sa.ROOT, timeout=900)
    out = p.stdout + p.stderr
    killed = bool(re.search(r'differ\s+[1-9]|raised\s+[1-9]|exceptions\s+[1-9]|Traceback', out)) or 'DIFF' in out
    return va, mn, killed


def main():
    args = sys.argv[1:]
    mx, jobs = 40, 3
    if '--max' in args:
        i = args.index('--max'); mx = int(args[i + 1]); del args[i:i + 2]
    if '--jobs' in args:
        i = args.index('--jobs'); jobs = int(args[i + 1]); del args[i:i + 2]
    sva = int(args[0], 16)
    cmd = [os.path.join(HERE, args[1] if args[1].endswith('.py') else args[1] + '.py')] + args[2:]
    fm = sa.func_map()
    if sva not in fm:
        sys.exit('stock %08x has no standalone function: %s' % (sva, sa.unmapped().get(sva)))
    lo, hi = function_extent(fm[sva])
    sites = jcc_sites(sa.SA_DLL, lo, hi)
    print('function %08x -> standalone %08x..%08x: %d conditional jumps' % (sva, lo, hi, len(sites)))
    if len(sites) > mx:
        step = len(sites) / float(mx)
        sites = [sites[int(i * step)] for i in range(mx)]
    tmp = tempfile.mkdtemp(prefix='sa_mut_')
    # the unmutated standalone must pass the script, or a 'kill' proves nothing
    env0 = dict(os.environ, NWD_SA='1', PYTHONIOENCODING='utf-8')
    base = subprocess.run([sys.executable] + cmd, capture_output=True, text=True, errors='replace', env=env0, cwd=sa.ROOT, timeout=900)
    bo = base.stdout + base.stderr
    if re.search(r'differ\s+[1-9]|raised\s+[1-9]|exceptions\s+[1-9]|Traceback', bo) or 'DIFF' in bo:
        sys.exit('the unmutated standalone does not pass this script:\n' + bo[-1500:])
    killed = 0
    surv = []
    try:
        with concurrent.futures.ThreadPoolExecutor(jobs) as ex:
            futs = [ex.submit(run_mutant, tmp, sa.SA_DLL, s, cmd, os.environ) for s in sites]
            for f in concurrent.futures.as_completed(futs):
                va, mn, k = f.result()
                print('  %08x %-4s %s' % (va, mn, 'KILLED' if k else 'survived'), flush=True)
                if k:
                    killed += 1
                else:
                    surv.append(va)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    print('mutants %d killed %d survived %d%s' % (len(sites), killed, len(surv), ('  survivors: ' + ' '.join('%08x' % v for v in sorted(surv))) if surv else ''))


if __name__ == '__main__':
    main()
