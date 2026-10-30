#!/usr/bin/env bash
# Remove an agent worktree WITHOUT touching the main tree's Ghidra export: new_worktree.sh links
# analysis/ghidra and ghidra/census into the worktree with junctions, and `git worktree remove` (or any
# recursive delete) goes THROUGH a junction and empties the target (it happened once: the main tree's
# export had to be regenerated). The junctions are dropped first (rmdir removes only the link).
# usage: bash tools/port/remove_worktree.sh <name>
set -e
NAME="$1"
[ -n "$NAME" ] || { echo "usage: remove_worktree.sh <name>"; exit 2; }
MAIN="$(cd "$(dirname "$0")/../.." && pwd)"
WT="$(dirname "$MAIN")/wt/$NAME"
[ -d "$WT" ] || { echo "no $WT"; exit 0; }
for j in analysis/ghidra ghidra/census; do
  [ -e "$WT/$j" ] && cmd //c "rmdir $(cygpath -w "$WT/$j")" || true
done
cd "$MAIN"
git worktree remove --force "$WT"
git branch -D "agent/$NAME" 2>/dev/null || true
echo "removed $WT"
