"""Runs the differential tests of tools/port/emu (and emu/win) in a mode and prints one line per test.

  python tools/port/emu/run_suite.py [--mode hook|sa] [--cases N] [--seed S] [--jobs J] [--timeout SEC] [--list] [--out DIR] [name ...]

The mode only chooses what `ours` is (see emu.py / sa.py): `hook` = build/bin/Release/NWindowDecomp.dll (the hook build, the default)
and `sa` = build/standalone/Release/NWindow.dll (the standalone). The tests themselves are the same files; sa mode is selected by the
environment variable NWD_SA=1 that this runner sets in the child processes.

A test is one command line (script + arguments). Every output line of the form `<label> cases N differ D raised R` (or `... exceptions X`)
is a result; a test is CLEAN when it prints at least one such line and every one has D = 0 and R/X = 0. The summary classes are:
  CLEAN  every result line has no difference and no exception
  DIFF   at least one result line has differences or exceptions
  CRASH  the script died (traceback / non-zero exit) before printing a result line, or printed none
  TIMEOUT
The full output of every test is written to <out>/<mode>/<test id>.txt.
"""
import sys, os, re, subprocess, time, argparse, concurrent.futures

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..', '..'))

# (id, script relative to tools/port/emu, extra arguments after "<cases> <seed>" or full argument template with {n} {s})
# a template that contains {n}/{s} replaces the default "<cases> <seed>" arguments
T = []


def add(script, *args, id=None):
    sid = id or (os.path.splitext(os.path.basename(script))[0] + ('_' + '_'.join(a.strip('{}') for a in args if re.match(r'^[a-z][a-z0-9]*$', a)) if args else ''))
    T.append((sid, script, list(args)))


# --- the C2 / C3 era suites (tools/port/emu) -------------------------------------------------------------------------------
# The scripts below were C2 models of C2 addresses and layouts. Repaired for C4 (stock vs hook / standalone): t_fmtsys t_md5 t_md5_class t_multi
# t_namectrl t_nkeycrypt t_num t_snoop t_viewitem, t_misc henna, t_w3ui (all but apply), t_w4a, t_w4c, t_html_*, t_trace (generic).
# STALE, not in the suite (they would only CRASH), with the C4 successor that proves the same functions:
#   t_edit (NCEditBox)                  -> win/e4.py (adapted from it)          t_fill (NToolTipInfo/Skin) -> win/t_tipfill, t_tiplayout, t_tipdraw, t_tipdraw2, t_tipmatch
#   t_lbup equip / inven                -> win/t_equipdrop.py / win/t_invdrop.py   t_vip (DrawItemInfo)      -> win/t_vipwnd.py draw
#   t_vipbox (ListBox/ListWnd/Wnd)      -> win/t_vipwnd.py (the C2 list classes do not exist in C4)
#   t_macro / t_recipe / t_shortcut / t_gm (painters) / t_equippaint -> win/t_oldwnd.py (generic t_sys world; the rows that run are in its OLD table)
#   t_equip (OnDrawToolTip), t_recipe2 (SetRecipe, tree fill), t_spawn (the C4 OnSpawnTest is another function), t_gm (OnCreate / buttons), the other
#   t_macro / t_shortcut functions, t_misc rect / limiter / expand, t_w3ui apply (-> win/t_video.py apply): no C4 successor yet, need a rewrite.
for s in ['t_entry', 't_fmtsys', 't_md5', 't_md5_class', 't_multi', 't_namectrl', 't_nkeycrypt', 't_num', 't_snoop', 't_viewitem']:
    add(s)
add('win/t_oldwnd')
add('t_misc', 'rect', '{n}', '{s}', id='t_misc_rect')
add('t_misc', 'henna', '{n}', '{s}', id='t_misc_henna')
for k in ('insert', 'find', 'color'):
    add('t_w4a', k, '{n}', '{s}', id='t_w4a_' + k)
for k in ('html', 'master', 'loading'):
    add('t_w4c', k, '{n}', '{s}', id='t_w4c_' + k)
for k in ('convert', 'wrap', 'send', 'connect', 'load', 'write', 'generate', 'sysmsg', 'token', 'apply'):
    add('t_w3ui', k, '{n}', '{s}', id='t_w3ui_' + k)
# the html tests (C4 again since sa-emu agent `old`): the page tree, the parser's tree building and the board viewer
for k in ('body', 'td', 'table'):
    add('t_html_parse', '{n}', '{s}', k, id='t_html_parse_' + k)
for w in ('layout', 'arrange', 'draw', 'height', 'release'):
    for top in ('table', 'block', 'item'):
        add('t_html_layout', '{n}', '{s}', w, top, id='t_html_layout_%s_%s' % (w, top))
for k in ('pieces', 'post', 'form', 'getvalue', 'action', 'button'):
    add('t_html_bbs', '{n}', '{s}', k, id='t_html_bbs_' + k)
# t_trace is a generic tool (stock va, ours symbol prefix, object size ...): a few C4 constructors as examples
add('t_trace', '0x1004BE50', '??0NKeyCrypt@@QAE@XZ', '4', '--cases', '{n}', '--seed', '{s}', id='t_trace_nkeycrypt')
add('t_trace', '0x10002D90', '??0NToolTipInfo@@QAE@XZ', '0x80', '--vtable', '--cases', '{n}', '--seed', '{s}', id='t_trace_tooltipinfo')
add('t_trace', '0x10030600', '?Clear@NToolTipInfo@@QAEXXZ', '0x80', '--vtable', '--cases', '{n}', '--seed', '{s}', id='t_trace_tooltipclear')
# --- the window suites (tools/port/emu/win) --------------------------------------------------------------------------------
for s in ['e4', 'e5', 'e6', 'e7', 'e8', 'e9', 'e10', 'e11', 'e12', 't_actionitem', 't_chat', 't_chatpaint', 't_console', 't_ctor',
          't_entities', 't_equipdrop', 't_filter', 't_gara', 't_html', 't_ime', 't_ime2', 't_ime3', 't_invdrop', 't_invpage',
          't_itemdebt', 't_itemwnd', 't_limiter', 't_memcpy', 't_msnfit', 't_msnsend', 't_nconsole_ctor', 't_shopwnd',
          't_shortcutitem', 't_skillitem', 't_slotctors', 't_srvsel', 't_statusbar', 't_sys', 't_taiwan', 't_timestr', 't_tipdraw',
          't_tipdraw2', 't_tipfill', 't_tiplayout', 't_tipmatch', 't_tvitem', 't_video', 't_vipwnd', 't_vipwnd2', 't_vipwnd3',
          't_vipwnd4', 't_wavec', 't_pwdata']:
    add('win/' + s)
for i in range(1, 17):
    add('win/t_msn%d' % i)
add('win/t_rest1')
add('win/t_vip5')

RESULT = re.compile(r'^(?P<label>.*?)\s*cases\s+(?P<n>\d+)\s+differ\s+(?P<d>\d+)(?:\s+raised\s+(?P<r>\d+)|\s+exceptions\s+(?P<x>\d+))?\s*$')
# the other spellings of a result: `t_entry: 20 cases, 0 differences`, `... cases N  differ D  raised R` (pw.run_tests prints two spaces)
RESULT2 = re.compile(r'^(?P<label>\S.*?):?\s+(?P<n>\d+) cases,? (?P<d>\d+) differences?\s*$')
SKIP = re.compile(r'^SKIP (?P<label>\S.*)$')
TOTAL = re.compile(r'^TOTAL problems (?P<n>\d+)\s*$')


def classify(out, rc):
    res = []
    skips = []
    total = None
    for ln in out.splitlines():
        ln = ln.strip()
        m = RESULT.match(ln) or RESULT2.match(ln)
        if m:
            d = int(m.group('d'))
            r = int((m.groupdict().get('r') or m.groupdict().get('x') or 0))
            res.append((m.group('label'), int(m.group('n')), d, r))
            continue
        m = SKIP.match(ln)
        if m:
            skips.append(m.group('label'))
            continue
        m = TOTAL.match(ln)
        if m:
            total = int(m.group('n'))
    tb = 'Traceback (most recent call last)' in out
    if not res:
        if skips and not tb:
            return 'SKIP', res, skips
        if total == 0 and not tb:
            return 'CLEAN?', res, skips
        return 'CRASH', res, skips
    if any(d or r for _, _, d, r in res):
        return 'DIFF', res, skips
    if tb:
        return 'CRASH', res, skips
    return 'CLEAN', res, skips


def run_one(t, mode, n, s, timeout, outdir):
    sid, script, args = t
    if not args:
        args = ['{n}', '{s}']
    argv = [a.replace('{n}', str(n)).replace('{s}', str(s)) for a in args]
    env = dict(os.environ, PYTHONIOENCODING='utf-8', CMP_LIMIT=os.environ.get('CMP_LIMIT', '2'))
    if mode == 'sa':
        env['NWD_SA'] = '1'
        if os.environ.get('NWD_SA_LENIENT'):
            env['NWD_SA_LENIENT'] = '1'
    else:
        env.pop('NWD_SA', None)
    cmd = [sys.executable, os.path.join(HERE, script + '.py')] + argv
    t0 = time.time()
    try:
        p = subprocess.run(cmd, capture_output=True, text=True, errors='replace', timeout=timeout, env=env, cwd=ROOT)
        out = p.stdout + (('\n--- stderr ---\n' + p.stderr) if p.stderr else '')
        rc = p.returncode
        status, res, skips = classify(p.stdout + p.stderr, rc)
    except subprocess.TimeoutExpired as e:
        out = (e.stdout.decode('utf-8', 'replace') if isinstance(e.stdout, bytes) else (e.stdout or '')) + '\n--- TIMEOUT ---'
        status, res, rc, skips = 'TIMEOUT', [], -1, []
    dt = time.time() - t0
    os.makedirs(os.path.join(outdir, mode), exist_ok=True)
    with open(os.path.join(outdir, mode, sid + '.txt'), 'w', encoding='utf-8') as f:
        f.write('$ ' + ' '.join(cmd) + '\n' + out)
    return sid, status, res, dt, out, skips


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--mode', default='hook', choices=['hook', 'sa'])
    ap.add_argument('--cases', type=int, default=30)
    ap.add_argument('--seed', type=int, default=1)
    ap.add_argument('--jobs', type=int, default=8)
    ap.add_argument('--timeout', type=int, default=900)
    ap.add_argument('--out', default=os.path.join(ROOT, 'build', 'emu_suite'))
    ap.add_argument('--list', action='store_true')
    ap.add_argument('names', nargs='*')
    a = ap.parse_args()
    tests = [t for t in T if not a.names or any(x == t[0] or t[0].startswith(x) for x in a.names)]
    if a.list:
        for sid, script, args in tests:
            print(sid, script, ' '.join(args))
        return
    summ = {}
    tot_cases = tot_funcs = tot_skips = 0
    rows = []
    with concurrent.futures.ThreadPoolExecutor(a.jobs) as ex:
        futs = [ex.submit(run_one, t, a.mode, a.cases, a.seed, a.timeout, a.out) for t in tests]
        for f in concurrent.futures.as_completed(futs):
            sid, status, res, dt, out, skips = f.result()
            nc = sum(n for _, n, _, _ in res)
            nd = sum(d for _, _, d, _ in res)
            nr = sum(r for _, _, _, r in res)
            tot_cases += nc
            tot_funcs += len(res)
            tot_skips += len(skips)
            summ.setdefault(status, []).append(sid)
            rows.append((sid, status, len(res), nc, nd, nr, dt))
            print('%-24s %-8s results %3d cases %6d differ %4d raised %4d skipped %3d (%.0fs)' % (sid, status, len(res), nc, nd, nr, len(skips), dt), flush=True)
    print('---- mode %s: %d scripts, %d function results, %d cases, %d skipped (no standalone function)' % (a.mode, len(tests), tot_funcs, tot_cases, tot_skips))
    for k in sorted(summ):
        print('  %-8s %3d: %s' % (k, len(summ[k]), ' '.join(sorted(summ[k]))))


if __name__ == '__main__':
    main()
