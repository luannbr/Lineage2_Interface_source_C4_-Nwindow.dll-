# C2: guia de trabalho para quem porta funções à mão (agentes e pessoas)

O que o pipeline `tools/port/` não portou sozinho (docs/notas/C2_Porte.md) é portado aqui, função por
função, no modo gancho. Este guia é operacional; escrito em inglês de propósito (os agentes leem melhor).

## 0. The situation in six lines
- Target: the C2 `orig/NWindow.dll` (build 03/03/2005, patched in 8 places by whoever packaged it; we
  reconstruct what runs). Our code is `src/nwindow/*.cpp` + `include/nwindow/*.h` (one class per file,
  UE2 style, docs/CONVENCOES.md applies) and it runs as a hook DLL: every function we reimplement
  replaces the stock one at its C2 address; every function we have not is a stub
  `NW_NAKED ... NW_STOCK(0xVA)` that jumps into the stock code (or `NW_UNPORTED()` when no address is
  known) and the stock code keeps running.
- **The tree is FROZEN output** (tag `generated-v1`): `tools/port/gen.py` is NOT to be run again over
  it. You edit the files by hand.
- The C1 project (`<C1 repo>`, read only) is the reference for
  every body: the generator copied the bodies that were the same code; yours are the ones that changed.
- Ground truth for C2 behaviour is the C2 disassembly/decompile: `analysis/ghidra/asm/<va>.asm`,
  `analysis/ghidra/decomp/<va>.c` (Ghidra's output is unreliable on parameter counts and slots: check the
  `ret N`; `python tools/port/packet.py` prints both side by side).
- Your build is a private git worktree (below). Never run anything that writes to
  `C2_Client\system` (the game's own folder).
- Report facts, not hopes: a function is done when the cycle says so.

## 1. Setup (once)
    bash tools/port/new_worktree.sh <yourname>      # in the main tree: creates ..\wt\<yourname>, branch agent/<yourname>
Work only inside `<repo>\..\wt\<yourname>`. Run the cycle with
    cmd //c "tools\port\cycle.bat <repo>\..\wt\<yourname>" > cycle.log 2>&1
(about one minute: configure, build, smoke load of the patched C2 DLL outside the game, `verify_hooks`,
`regcheck`, `verify_data`; it sets up vcvars32 itself). Read the end of `cycle.log` and the sections
`=== verify` / `=== regcheck`. Do not use `python -c`/heredocs for edits with backslashes or regexes: use
the Edit/Write tools.

## 2. The work packet of a function
    python tools/port/packet.py NCButton::OnPaint        # by name, or: packet.py 10001ca0 (C2 va)
    python tools/port/packet.py --list NCButton          # every function of a class: status and why it was not copied
It prints (1) the map row (C1 va -> C2 va, similarity, category, how matched), (2) the C1 source unit, (3)
the instruction diff C1 -> C2 with `this` displacements named after the C2 fields, (4) Ghidra's decompile
of the C2 function. Category C = same shape, constants differ (`analysis/port/constants.csv` lists them per
instruction); D = the code changed. Read the diff, not the whole function: most of a D function is the C1
body unchanged, the port is the changed hunks. When the C2 function has no C1 counterpart (new code),
decompile it like the C1 project did (docs/PLANO.md, "Método").

## 3. How to port one function
In `src/nwindow/<File>.cpp` find the unit of the function (the `// 0xVA ... NOT PORTED (...)` comment and
the stub below it, or `NW_UNPORTED` for constructors) and replace it with the body, in this shape:

    // 0x1001ca0 (vslot 55) - what it does, in one line, English
    INT NCButton::OnPaint(NCGDevice* Device)
    {
        ...
    }
    NW_HOOK_METHOD(0x10001ca0, NCButton, OnPaint)

The address in the comment and in `NW_HOOK_*` is the **C2** address; `(vslot N)` is the C2 slot
(`python tools/port/packet.py` and the header say which). `guard(Class::Method); ... unguard;` where the C2
function has the guard frame (the string is in the decompile: it is the original name). Hook macros are in
docs/CONVENCOES.md ("Macros do modo gancho"). A constructor needs every ancestor's constructor ported or
the hook is not installed (the base constructor is called by C++): check `packet.py --list <Base>`.
If the C2 function is a plain forwarder/inline of something we have, say so in the comment.

Fidelity first (docs/CONVENCOES.md): reproduce what the C2 code does, including the odd; improvements are
separate, annotated changes. Types: UE2 (`INT`, `UBOOL`, `TCHAR`, `FString` where C2 holds one: **C2 changed
many `TCHAR[]` buffers into `FString`**, look at the constructor and the `FString::` imports in the diff).

## 4. Headers (layouts, virtuals)
`include/nwindow/<Class>.h` is C2-shaped by the generator but with holes: `BYTE _UnknownXXXX[n]` fillers
and `BYTE _C2Retyped_<name>[n]` where C2 changed a field's type or size, fields commented as "not placed"
or "offset derived, no evidence", `NewVirtual<slot>()` placeholders for virtuals only C2 has. When you need
a field, derive its C2 offset and type from the C2 code (the constructor stores, the instruction diff) and
declare it; then keep `NW_ASSERT_OFFSET` / `NW_ASSERT_SIZE` right (the C2 `sizeof` is in
`analysis/classes.json`). Never leave a guessed offset without saying so in a comment. A virtual you
introduce or rename must sit at the C2 slot (`analysis/classes.json` has every vtable). Engine classes are in
`include/ue2/` (C2 layouts verified by tools/ue2_check); `FL2GameData`, `User`, `NItemEntry`, `NListRow` are
known to be stale in places: the cycle's `verify_data` section names offsets the stock code uses that ours
does not.

## 5. Proof (the cycle is the judge)
After each batch run the cycle. Everything must hold:
- smoke: `hooks: N installed, N registered, 0 errors`
- `=== verify`: your functions are `OK`, or `DIFF` with a cause you understood. A DIFF is a difference in the pop
  on return, the virtual slots called or the imports called between our function and the stock one. Fix the
  code. Only if the difference is the compiler's (e.g. we inline what the stock calls) add one line to
  `db/verify_notes.csv` (`c2va,explanation`), specific, never generic.
- `=== regcheck`: 0 problems. A BREAKS line means a stock caller keeps a register alive across a call to a
  function we hooked; the fix is to NOT hook that callee (turn it back into its stub) or to port its callers too.
- `db/verify_data.csv` (run by the cycle): for your functions `status` should be `ok`. `diff` lists constants,
  field offsets and strings that the stock function uses and ours does not (or the reverse). Offsets that
  both sides have but different (stock-only {58} ours-only {5c}) are layout bugs in a header: fix them. Constants
  that differ by an 8-bit shift (0x20 vs 0x2000) are the compiler testing a byte instead of a dword: benign,
  note them in `db/verify_data_notes.csv`.
A function that cannot be made to pass stays a stub. Never delete a stub to hide an unresolved external:
keep the declaration defined.

## 6. Git
You are on branch `agent/<yourname>` in your worktree. Commit small and often, English messages, with the
trailer line the harness gives you. **Commit only** `src/nwindow/**`, `include/nwindow/**`,
`include/ue2/**` (only if your assignment says so), `db/verify_notes.csv`, `db/verify_data_notes.csv`,
`db/names.csv`. The cycle regenerates `db/functions.csv`, `db/verify_data.csv`, `docs/PROGRESSO.md` and
others: `git checkout -- <file>` them before committing (the coordinator regenerates them after merging).
Touch only the files of your assignment; a header another group needs (a base class) is changed only if
your assignment lists it. Do not push, do not merge: the coordinator merges your branch.

## 7. Report (under 60 lines)
Per file: functions ported (name, C2 va), functions you tried and left as stubs and why, headers changed
(fields added with their evidence), new `db/verify_notes.csv` lines, the last cycle summary line
(`hooks: ...`, `N ok, M explained, K unexplained`, regcheck), and anything the next person must know
(a struct layout you could not settle, an Engine API that changed).

## 8. Cleaning up
Never delete a worktree by hand or with `git worktree remove` directly: `analysis/ghidra` and `ghidra/census` in it are junctions to the main tree and a recursive delete empties the main tree. Use `bash tools/port/remove_worktree.sh <name>` (the coordinator does it after merging).

## 9. Additions from wave 1 (read before porting)
- The cycle now also runs `tools/port/check_breakers.py`: no hooked function may reach a function whose body is `NW_UNPORTED()` (an int3). A stub that a ported function calls must jump to the C2 function (`NW_NAKED ... NW_STOCK(0xC2VA)`) or be ported.
- C1 literal addresses are wrong in C2 everywhere (globals, tables, StockCall targets): take C2 addresses from the C2 asm; `db/amap.csv` maps C1->C2.
- The function map paired some C1 functions with the wrong C2 twin when C2 inserted new functions: a body that "verifies" can still be the wrong function; compare names (db/functions.csv guard names) and the virtual slot called.
- `NCObject::SysString(i)`, `NCObject::PlayInterfaceSound(id)` and the NCGDevice signatures (comment block in NCGDevice.h) are the C2 API; `GL2GameData.SysString[i]` in copied code is stale.
- Functions that exist only in C2 have no C1 body: decompile from the asm; for the large ones use the differential harness in `tools/port/emu/` (docs/notas/C2_Porte.md).
- New classes: docs/notas/C2_Onda2.md ("Como criar uma classe nova do C2").
- Do not commit generated cycle output (`db/functions.csv`, `db/verify_data.csv`, `docs/PROGRESSO.md`); the note tables (`db/verify_notes.csv`, `db/verify_data_notes.csv`, `db/names.csv`) merge by union.
