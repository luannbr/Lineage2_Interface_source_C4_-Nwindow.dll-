#!/usr/bin/env bash
# The generate -> compile -> judge -> demote loop of the C3/C4 port (docs/PLANO_C3.md, "Etapa 3/4"; docs/PLANO_C4.md).
# Each round: gen.py (everything but db/hand_owned.txt), build, then the first judge that fails demotes what it
# can (db/demote.csv) and the next round starts again; it ends when every judge passes and tools/port/checks.py
# (the checks no judge of the cycle does) has run.
#
#   bash tools/port/judge_loop.sh [rounds]        (from the repository root; log lines on stdout)
#
# Differences of verify_hooks / verify_data that the base client's (C3) notes explain are carried over
# (tools/port/carry_notes.py) instead of demoted; db/demote.csv starts as its header line for a fresh run.
# A failure in a hand-owned file cannot be demoted: the loop prints it and keeps going until `rounds` run out,
# so read the log, fix the file, and start again.
cd "$(dirname "$0")/../.." || exit 1
mkdir -p build
for i in $(seq 1 "${1:-16}"); do
  python tools/port/gen.py --cat-c --derived > build/judge_gen.log 2>&1 || { echo "round $i: gen FAILED"; tail -3 build/judge_gen.log; break; }
  u=$(grep "^units" build/judge_gen.log | cut -c1-90)
  cmake -S . -B build > /dev/null 2>&1       # the sources are a glob: files that came or went
  cmake --build build --config Release --target NWindowDecomp --parallel 8 > build/judge_cc_$i.log 2>&1
  if grep -qE "error (C|LNK)[0-9]+" build/judge_cc_$i.log; then
    echo "round $i: $u | compile: $(grep -cE 'error (C|LNK)[0-9]+' build/judge_cc_$i.log) error lines"
    python tools/port/demote.py build/judge_cc_$i.log --stage . 2>&1 | head -1
    continue
  fi
  python tools/cycle.py --game-source --no-install > build/judge_cy_$i.log 2>&1; rc=$?
  h=$(grep "hooks:" build/judge_cy_$i.log | tail -1 | cut -c14-60)
  if [ $rc -ne 0 ]; then
    if echo "$h" | grep -qE ", [1-9][0-9]* errors"; then
      echo "round $i: $u | $h | the smoke test reports hook errors (duplicate or unknown address): see build/smoke_system/NWindowDecomp.log"
      break
    fi
    v=$(grep -E "[0-9]+ ok, [0-9]+ explained" build/judge_cy_$i.log | tail -1)
    if echo "$v" | grep -q " 0 unexplained"; then
      echo "round $i: $u | $h | regcheck -> demote"; python tools/regcheck.py --game-src --demote 2>&1 | tail -1
    else
      echo "round $i: $u | $h | $v -> carry notes / demote_verify"
      python tools/port/carry_notes.py --log build/judge_cy_$i.log 2>&1 | tail -1
      python tools/port/demote_verify.py build/judge_cy_$i.log 2>&1 | tail -1
    fi
    continue
  fi
  d=$(python tools/port/verify_data.py 2>&1 | tail -1)
  python tools/port/check_breakers.py > build/judge_br_$i.log 2>&1; brc=$?
  echo "round $i: $h | cycle ok | $d | breakers rc=$brc"
  dok=0; echo "$d" | grep -q " 0 differ" && dok=1
  if [ $dok -eq 1 ] && [ $brc -eq 0 ]; then
    echo "ALL JUDGES PASS"
    # the constant arguments of the calls (a default the declaration gives that the stock does not pass): a copy that
    # differs goes back to the stock code
    python tools/port/verify_args.py --smoke --quiet > build/judge_args_$i.log 2>&1
    if grep -q "^DIFF" build/judge_args_$i.log; then
      echo "round $i: verify_args differs -> demote"; python tools/port/demote_args.py build/judge_args_$i.log | tail -1
      continue
    fi
    # what no judge above sees (the first runs in the game found these kinds): the arguments a stub declares against
    # the stock ret, a void hook whose stock callers read eax, a struct that grew, a constant argument ...
    python tools/port/checks.py > build/judge_checks_$i.log 2>&1
    grep -E "^(check_|size_audit|verify_args|CHECKS)" build/judge_checks_$i.log | cut -c1-200
    break
  fi
  if [ $dok -eq 0 ]; then
    python tools/port/carry_notes.py --data 2>&1 | tail -1
    python tools/port/demote_data.py 2>&1 | tail -1
  fi
  [ $brc -ne 0 ] && python tools/port/check_breakers.py --demote 2>&1 | tail -1
done
