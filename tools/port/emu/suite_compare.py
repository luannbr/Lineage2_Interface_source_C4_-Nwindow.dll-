"""Compares two runs of run_suite.py (hook mode vs standalone mode) from their output files.

  python tools/port/emu/suite_compare.py [--dir build/emu_suite] [--md]

Reads <dir>/hook/<test>.txt and <dir>/sa/<test>.txt (written by run_suite.py) and prints, per script, the number of function results, cases, differences and exceptions of
each mode, then the totals: the functions (result lines) that are clean in both modes are the ones PROVED in the standalone against the stock.
"""
import os, sys, glob, argparse

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import run_suite

ROOT = run_suite.ROOT


def load(d):
    out = {}
    for f in sorted(glob.glob(os.path.join(d, '*.txt'))):
        sid = os.path.splitext(os.path.basename(f))[0]
        txt = open(f, encoding='utf-8', errors='replace').read()
        status, res, skips = run_suite.classify(txt, 0)
        out[sid] = (status, res, skips)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--dir', default=os.path.join(ROOT, 'build', 'emu_suite'))
    ap.add_argument('--md', action='store_true')
    a = ap.parse_args()
    h, s = load(os.path.join(a.dir, 'hook')), load(os.path.join(a.dir, 'sa'))
    ids = sorted(set(h) | set(s))
    tot = dict(h_fun=0, h_cases=0, h_clean=0, s_fun=0, s_cases=0, s_clean=0, both_fun=0, both_cases=0, s_skip=0)
    rows = []
    for i in ids:
        hs, hr, hk = h.get(i, ('-', [], []))
        ss, sr, sk = s.get(i, ('-', [], []))
        hc = sum(n for _, n, _, _ in hr)
        sc = sum(n for _, n, _, _ in sr)
        hd = sum(d + r for _, _, d, r in hr)
        sd = sum(d + r for _, _, d, r in sr)
        hl = {l: (n, d + r) for l, n, d, r in hr}
        sl = {l: (n, d + r) for l, n, d, r in sr}
        hclean = {l for l, (n, d) in hl.items() if d == 0}
        sclean = {l for l, (n, d) in sl.items() if d == 0}
        both = hclean & sclean
        tot['h_fun'] += len(hr); tot['h_cases'] += hc; tot['h_clean'] += len(hclean)
        tot['s_fun'] += len(sr); tot['s_cases'] += sc; tot['s_clean'] += len(sclean)
        tot['both_fun'] += len(both); tot['both_cases'] += sum(sl[l][0] for l in both)
        tot['s_skip'] += len(sk)
        rows.append((i, hs, len(hr), hc, hd, ss, len(sr), sc, sd, len(both), len(sk)))
    if a.md:
        print('| script | hook | funcs | cases | diff | standalone | funcs | cases | diff | clean in both | skipped |')
        print('|---|---|---:|---:|---:|---|---:|---:|---:|---:|---:|')
        for r in rows:
            print('| %s | %s | %d | %d | %d | %s | %d | %d | %d | %d | %d |' % r)
    else:
        print('%-18s %-7s %5s %6s %5s  %-7s %5s %6s %5s  %5s %4s' % ('script', 'hook', 'funcs', 'cases', 'diff', 'sa', 'funcs', 'cases', 'diff', 'both', 'skip'))
        for r in rows:
            print('%-18s %-7s %5d %6d %5d  %-7s %5d %6d %5d  %5d %4d' % r)
    print()
    print('hook mode      : %d function results, %d cases, %d functions clean' % (tot['h_fun'], tot['h_cases'], tot['h_clean']))
    print('standalone mode: %d function results, %d cases, %d functions clean, %d skipped (no standalone function)' % (tot['s_fun'], tot['s_cases'], tot['s_clean'], tot['s_skip']))
    print('clean in both  : %d functions, %d standalone cases' % (tot['both_fun'], tot['both_cases']))


if __name__ == '__main__':
    main()
