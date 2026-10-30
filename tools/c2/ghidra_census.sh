#!/usr/bin/env bash
# Function census of any DLL in a private Ghidra project, with the project's seeding method:
# auto-analysis, then rounds of tools/c2/gen_seeds.py + tools/ghidra/NwdSeed.java (function starts
# reached only through vtables / relocated tables), each followed by an export of the function list.
# usage: tools/c2/ghidra_census.sh <name> <dll> <outdir> [rounds=3]
#   <outdir>/funcs.tsv  va size ranges name src thunk nparams callconv   (tools/ghidra/NwdFuncs.java)
# The Ghidra project goes to <outdir>/project (not under git). Takes a few minutes (NWindow) to ~15 (Engine).
set -e
NAME="$1"; DLL="$2"; OUT="$3"; ROUNDS="${4:-3}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
GHIDRA="${GHIDRA:-${NWD_GHIDRA_HOME:-}}"   # the Ghidra install folder (the one with support/analyzeHeadless.bat)
[ -n "$GHIDRA" ] || { echo "$(basename "$0"): set NWD_GHIDRA_HOME to the Ghidra install folder (the one with support/analyzeHeadless.bat)" >&2; exit 1; }
export JAVA_HOME="${GHIDRA_JAVA_HOME:-C:\Program Files\Java\jdk-21}"
mkdir -p "$OUT/project"
W() { cygpath -w "$1"; }
SCRIPTS="$(W "$ROOT/tools/ghidra")"
"$GHIDRA/support/analyzeHeadless.bat" "$(W "$OUT/project")" "$NAME" \
  -import "$(W "$DLL")" -overwrite -scriptPath "$SCRIPTS" \
  -postScript NwdFuncs.java "$(W "$OUT")" -analysisTimeoutPerFile 7200 \
  -log "$(W "$OUT/ghidra.log")" > "$OUT/ghidra.stdout" 2>&1
for r in $(seq 1 "$ROUNDS"); do
  python "$ROOT/tools/c2/gen_seeds.py" "$DLL" "$OUT/funcs.tsv" "$OUT/seeds$r.txt"
  "$GHIDRA/support/analyzeHeadless.bat" "$(W "$OUT/project")" "$NAME" -process "$(basename "$DLL")" \
    -scriptPath "$SCRIPTS" -postScript NwdSeed.java "$(W "$OUT/seeds$r.txt")" \
    -postScript NwdFuncs.java "$(W "$OUT")" -log "$(W "$OUT/ghidra.log")" > "$OUT/ghidra_seed$r.stdout" 2>&1
  grep -E "seeded|functions:" "$OUT/ghidra_seed$r.stdout" || true
done
python "$ROOT/tools/c2/gen_seeds.py" "$DLL" "$OUT/funcs.tsv" "$OUT/seeds_left.txt"
