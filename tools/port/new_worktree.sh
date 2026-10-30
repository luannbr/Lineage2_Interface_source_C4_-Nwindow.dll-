#!/usr/bin/env bash
# A private worktree for one agent: its own branch (agent/<name>), its own build directory, the
# reference binaries copied in and the Ghidra export linked (junction) from the main tree.
# usage: bash tools/port/new_worktree.sh <name>        -> <repo>/../wt/<name>
set -e
NAME="$1"
[ -n "$NAME" ] || { echo "usage: new_worktree.sh <name>"; exit 2; }
MAIN="$(cd "$(dirname "$0")/../.." && pwd)"
WT="$(dirname "$MAIN")/wt/$NAME"
cd "$MAIN"
if [ -d "$WT" ]; then echo "$WT exists"; exit 0; fi
mkdir -p "$(dirname "$WT")"
git worktree add -q -b "agent/$NAME" "$WT" HEAD
mkdir -p "$WT/orig"
cp "$MAIN"/orig/*.dll "$MAIN"/orig/*.exe "$WT/orig/" 2>/dev/null || true
# the Ghidra export (decompiled C and disassembly of every C3 function, ~300 MB): a junction, not a copy
mkdir -p "$WT/analysis"
cmd //c "mklink /J $(cygpath -w "$WT/analysis/ghidra") $(cygpath -w "$MAIN/analysis/ghidra")" > /dev/null
mkdir -p "$WT/ghidra"
cmd //c "mklink /J $(cygpath -w "$WT/ghidra/census") $(cygpath -w "$MAIN/ghidra/census")" > /dev/null
echo "worktree $WT on branch agent/$NAME"
