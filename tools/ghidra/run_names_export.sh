#!/usr/bin/env bash
# Apply db/functions.csv names to the Ghidra project, then export again (analysis/ghidra/).
# Run after tools/build_db.py whenever many functions got names.
# C3: the Ghidra project is the one tools/ghidra/run_full.sh made (ghidra/census/project, name NWindow_C3).
# Do not run it while another headless run holds that project.
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
GHIDRA="${GHIDRA:-${NWD_GHIDRA_HOME:-}}"   # the Ghidra install folder (the one with support/analyzeHeadless.bat)
[ -n "$GHIDRA" ] || { echo "$(basename "$0"): set NWD_GHIDRA_HOME to the Ghidra install folder (the one with support/analyzeHeadless.bat)" >&2; exit 1; }
export JAVA_HOME="${GHIDRA_JAVA_HOME:-C:\Program Files\Java\jdk-21}"
rm -rf "$ROOT/analysis/ghidra/decomp" "$ROOT/analysis/ghidra/asm"
"$GHIDRA/support/analyzeHeadless.bat" "$(cygpath -w "$ROOT/ghidra/census/project")" NWindow_C3 \
  -process NWindow.dll -noanalysis \
  -scriptPath "$(cygpath -w "$ROOT/tools/ghidra")" \
  -postScript NwdApplyNames.java "$(cygpath -w "$ROOT/db/functions.csv")" \
  -postScript NwdExport.java "$(cygpath -w "$ROOT/analysis/ghidra")" \
  -log "$(cygpath -w "$ROOT/analysis/ghidra/headless_names.log")" \
  -scriptlog "$(cygpath -w "$ROOT/analysis/ghidra/script_names.log")"
