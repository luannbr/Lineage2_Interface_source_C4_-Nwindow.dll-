#!/usr/bin/env bash
# The whole Ghidra pass over orig/NWindow.dll in one go: import + auto-analysis, the function
# census with the seeding rounds (tools/c2/ghidra_census.sh: function starts reached only through
# vtables / relocated tables), then ONE export of every function (decompile + disassembly) into
# analysis/ghidra/ (tools/ghidra/NwdExport.java).
# usage: tools/ghidra/run_full.sh [rounds=3]
# The Ghidra project lives in ghidra/ (gitignored). Takes a while; run it in the background.
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
ROUNDS="${1:-3}"
GHIDRA="${GHIDRA:-${NWD_GHIDRA_HOME:-}}"   # the Ghidra install folder (the one with support/analyzeHeadless.bat)
[ -n "$GHIDRA" ] || { echo "$(basename "$0"): set NWD_GHIDRA_HOME to the Ghidra install folder (the one with support/analyzeHeadless.bat)" >&2; exit 1; }
export JAVA_HOME="${GHIDRA_JAVA_HOME:-C:\Program Files\Java\jdk-21}"
W() { cygpath -w "$1"; }
mkdir -p "$ROOT/analysis/ghidra"

# 1. import, analysis, census, seeds (project: ghidra/census/project, name NWindow_C4)
bash "$ROOT/tools/c2/ghidra_census.sh" NWindow_C4 "$ROOT/orig/NWindow.dll" "$ROOT/ghidra/census" "$ROUNDS"

# 2. one full export on the seeded project
rm -rf "$ROOT/analysis/ghidra/decomp" "$ROOT/analysis/ghidra/asm"
"$GHIDRA/support/analyzeHeadless.bat" "$(W "$ROOT/ghidra/census/project")" NWindow_C4 \
  -process NWindow.dll \
  -scriptPath "$(W "$ROOT/tools/ghidra")" \
  -postScript NwdExport.java "$(W "$ROOT/analysis/ghidra")" \
  -log "$(W "$ROOT/analysis/ghidra/headless_export.log")" \
  -scriptlog "$(W "$ROOT/analysis/ghidra/script_export.log")" > "$ROOT/analysis/ghidra/export.stdout" 2>&1
echo "export done: $(ls "$ROOT/analysis/ghidra/asm" | wc -l) functions"
