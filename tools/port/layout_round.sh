#!/usr/bin/env bash
# One round of the layout fix: where do the headers put a field that the stock code reads elsewhere?
#
#   bash tools/port/layout_round.sh
#
# 1. keeps of db/demote.csv only what is not a verify_data / check_breakers demotion (those are the functions whose
#    displacements we want to SEE), generates, builds, runs the cycle and tools/port/verify_data.py (which now flags
#    them) and tools/port/disp_votes.py (which field sits at our displacement, where the stock reads it);
# 2. tools/port/disp_apply.py turns the unambiguous votes into lines of db/layout_manual.csv;
# 3. tools/port/layout.py re-solves the layouts with them as anchors, tools/port/eligible.py re-checks the pairs;
# 4. db/demote.csv is put back as it was.
# Repeat until disp_apply finds nothing new, then start tools/port/judge_loop.sh from a fresh db/demote.csv.
cd "$(dirname "$0")/../.." || exit 1
mkdir -p build
cp db/demote.csv build/demote_full.csv
python - <<'EOF'
import csv
rows = list(csv.reader(open("db/demote.csv", encoding="utf-8")))
keep = [rows[0]] + [r for r in rows[1:] if not (r[1].startswith(("verify_data", "check_breakers")))]
csv.writer(open("db/demote.csv", "w", encoding="utf-8", newline="")).writerows(keep)
print("demote.csv: %d rows kept of %d" % (len(keep) - 1, len(rows) - 1))
EOF
python tools/port/gen.py --cat-c --derived > build/layout_gen.log 2>&1 || { echo "gen FAILED"; tail -3 build/layout_gen.log; cp build/demote_full.csv db/demote.csv; exit 1; }
cmake -S . -B build > /dev/null 2>&1
cmake --build build --config Release --target NWindowDecomp --parallel 8 > build/layout_cc.log 2>&1
if grep -qE "error (C|LNK)[0-9]+" build/layout_cc.log; then
  echo "compile errors: demoting"; python tools/port/demote.py build/layout_cc.log --stage . | head -1
  cp db/demote.csv build/demote_keep.csv
  cp build/demote_full.csv db/demote.csv   # the new compile demotions are lost on purpose: run judge_loop.sh to settle them
  echo "run the compile loop first (build/compile_loop.sh) and repeat"; exit 1
fi
python tools/cycle.py --game-source --no-install > build/layout_cy.log 2>&1
python tools/port/verify_data.py | tail -1
python tools/port/disp_votes.py | head -1
python tools/port/disp_apply.py
python tools/port/layout.py | head -2
python tools/port/eligible.py | head -1
cp build/demote_full.csv db/demote.csv
