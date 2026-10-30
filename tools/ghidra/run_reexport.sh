#!/usr/bin/env bash
# Re-open the existing Ghidra project, seed missed functions (analysis/seeds.txt), let the
# analyzer process them, then export again. Run after tools/gen_seeds.py.
# C3: the Ghidra project is the one tools/ghidra/run_full.sh made (ghidra/census/project, name NWindow_C3).
# Do not run it while another headless run holds that project.
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
GHIDRA="${GHIDRA:-${NWD_GHIDRA_HOME:-}}"   # the Ghidra install folder (the one with support/analyzeHeadless.bat)
[ -n "$GHIDRA" ] || { echo "$(basename "$0"): set NWD_GHIDRA_HOME to the Ghidra install folder (the one with support/analyzeHeadless.bat)" >&2; exit 1; }
export JAVA_HOME="${GHIDRA_JAVA_HOME:-C:\Program Files\Java\jdk-21}"
rm -rf "$ROOT/analysis/ghidra/decomp" "$ROOT/analysis/ghidra/asm"
"$GHIDRA/support/analyzeHeadless.bat" "$(cygpath -w "$ROOT/ghidra/census/project")" NWindow_C3 \
  -process NWindow.dll \
  -scriptPath "$(cygpath -w "$ROOT/tools/ghidra")" \
  -postScript NwdSeed.java "$(cygpath -w "$ROOT/analysis/seeds.txt")" \
  -postScript NwdExport.java "$(cygpath -w "$ROOT/analysis/ghidra")" \
  -log "$(cygpath -w "$ROOT/analysis/ghidra/headless2.log")" \
  -scriptlog "$(cygpath -w "$ROOT/analysis/ghidra/script2.log")"
