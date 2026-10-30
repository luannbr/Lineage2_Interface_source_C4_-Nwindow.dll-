# Runs every differential test of the wave C proof2 lane (the messenger windows and sockets, the rest1 windows, the VIP shop) and prints one line
# per test that does not end `differ 0 raised 0`, then the totals. The tests need build/bin/Release/NWindowDecomp.dll and
# build/smoke_system/NWindowDecomp.log (the cycle makes both).
#   python run_proof2.py [cases] [seed]
import sys, os, subprocess, re
HERE = os.path.dirname(os.path.abspath(__file__))
n = sys.argv[1] if len(sys.argv) > 1 else '60'
seed = sys.argv[2] if len(sys.argv) > 2 else '1'
MODS = ['t_msn1', 't_msn2', 't_msn3', 't_msn4', 't_msn5', 't_msn6', 't_msn7', 't_msn8', 't_msn9', 't_msn10', 't_msn11', 't_msn12', 't_msn13', 't_msn14', 't_msn15', 't_msn16', 't_rest1', 't_vip5']
env = dict(os.environ, CMP_LIMIT='0', CMP_CASEW='60', CMP_EXCW='120')
tests = clean = 0
bad = []
low = []
for m in MODS:
    out = subprocess.run([sys.executable, os.path.join(HERE, m + '.py'), n, seed], capture_output=True, text=True, env=env, errors='replace').stdout
    for ln in out.splitlines():
        cv = re.match(r'^   coverage (\S+): (\d+)/(\d+) instructions \((\d+)%\)', ln)
        if cv and int(cv.group(4)) < 90:
            low.append('%s: %s %s/%s (%s%%)' % (m, cv.group(1), cv.group(2), cv.group(3), cv.group(4)))
        mm = re.match(r'^(\S+) cases (\d+) differ (\d+) raised (\d+)$', ln)
        if mm:
            tests += 1
            if mm.group(3) == '0' and mm.group(4) == '0':
                clean += 1
            else:
                bad.append('%s: %s' % (m, ln))
print('tests', tests, 'clean', clean, 'with differences or errors', len(bad))
for b in bad:
    print('  ', b)
print('tests under 90 percent of the stock instructions reached:', len(low))
for b in low:
    print('  ', b)
