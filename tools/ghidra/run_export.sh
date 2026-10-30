#!/usr/bin/env bash
# Import orig/NWindow.dll into a private Ghidra project (ghidra/, gitignored), run the full
# auto-analysis and export every function to analysis/ghidra/. Takes a while; run in background.
# C2: tools/ghidra/run_full.sh does this (plus the census and the seeding rounds that find the
# functions reached only through vtables); this plain import skips the seeding.
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
GHIDRA="${GHIDRA:-${NWD_GHIDRA_HOME:-}}"   # the Ghidra install folder (the one with support/analyzeHeadless.bat)
[ -n "$GHIDRA" ] || { echo "$(basename "$0"): set NWD_GHIDRA_HOME to the Ghidra install folder (the one with support/analyzeHeadless.bat)" >&2; exit 1; }
export JAVA_HOME="${GHIDRA_JAVA_HOME:-C:\Program Files\Java\jdk-21}"
mkdir -p "$ROOT/ghidra" "$ROOT/analysis/ghidra"
"$GHIDRA/support/analyzeHeadless.bat" "$(cygpath -w "$ROOT/ghidra")" NWindow_Decomp \
  -import "$(cygpath -w "$ROOT/orig/NWindow.dll")" -overwrite \
  -scriptPath "$(cygpath -w "$ROOT/tools/ghidra")" \
  -postScript NwdExport.java "$(cygpath -w "$ROOT/analysis/ghidra")" \
  -analysisTimeoutPerFile 7200 \
  -log "$(cygpath -w "$ROOT/analysis/ghidra/headless.log")" \
  -scriptlog "$(cygpath -w "$ROOT/analysis/ghidra/script.log")"
