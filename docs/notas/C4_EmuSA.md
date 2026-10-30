# C4 stage 5: the unicorn differential harness against the STANDALONE dll (lane sa-emu)

Written in English on purpose. The harness is `tools/port/emu` (stock vs ours under unicorn; `docs/notas/C4_Agentes.md` section 7.7 and the wave C proofs).
Until now `ours` was always the HOOK build (`build/bin/Release/NWindowDecomp.dll`). With `NWD_SA=1` it is the STANDALONE
(`build/standalone/Release/NWindow.dll` + `NWindow.map`, the same source compiled with `NW_HOOK_MODE=0`). Hook mode stays the default.

## 1. Running
    python tools/port/emu/run_suite.py --mode hook|sa [--cases N] [--seed S] [--jobs J] [--timeout SEC] [name-prefix ...]
    NWD_SA=1 python tools/port/emu/win/t_video.py 20 1          # one script, standalone mode
Needs both builds (`cmake --build` of `NWindowStandalone`, i.e. `python tools/standalone_report.py`, and the hook cycle, which also writes
`build/smoke_system/NWindowDecomp.log` that maps stock addresses to hook targets). `run_suite.py` writes every output to `build/emu_suite/<mode>/<test>.txt`
and prints CLEAN / DIFF / CRASH / SKIP / TIMEOUT per script (a result line is `<label> cases N differ D raised R`; `SKIP <label>: <why>` lines are counted apart).
Environment: `NWD_SA=1` standalone mode; `NWD_SA_LENIENT=1` collects every stock address without a counterpart instead of failing at the first
(printed as `SA-MISSING ...` at exit); `NWD_SA_NOTES=1` prints the callee stubs that could not be placed (`SA-UNSTUBBED`); `NWD_TR_CTORS=0` gives the old
stubbed constructors of tr.py.

## 2. How a test written for the hook build runs against the standalone (tools/port/emu/sa.py)
* **Functions.** The hook registration (smoke log: stock VA, `ours=<rva>`) gives, through the hook map, a decorated symbol; the standalone map gives the address of the
  same symbol. Half of the hooks are registered through hook-only wrappers (`??$Fwd<...>`, `?Run@NwCtor_N`, `?Run@NwDtor_N`): those are resolved by class, method name and the
  undecorated parameter list (dbghelp `UnDecorateSymbolName`). A destructor has no function of its own in the standalone (it is inlined in `??_G<Class>`, the scalar
  deleting destructor): the stock destructor maps to a 10-byte trampoline `push 0; call ??_G; xor eax,eax; ret` at `0x7E40xxxx`. `sa.func_map()` = 5,289 of the 5,456
  hooks (167 have no standalone function: template instances and `...Ext` stand-ins that the standalone inlines, constructor overloads by value, a few folded thunks);
  `sa.unmapped()` says why. `log_map()` (win/e4.py) returns `sa.LogMap`: a missing function raises `sa.NoStandaloneFunction` (print `sa.skip(label, ex)` and go on).
* **Globals.** The hook build reads the stock image's globals at `g_nwBase + rva` (the harness maps a zeroed region `NWBASE = 0x50000000` for that and the tests write the
  state there). The standalone has its own globals. `sa.SaUc` is a proxy around unicorn's `Uc` for the `ours` side: every `mem_write` / `mem_read` inside
  `[NWBASE, NWBASE + 4MB)` is translated from `stock VA = 0x10000000 + off` to the standalone object that holds it: the named globals of `src/standalone/NwGlobals.cpp`
  (`tools/gen_globals.py scan()` gives name -> stock VA, the map gives the symbol), the 341 `PrivateStaticClass` objects and the class vtables (`analysis/classes.json`).
  An access with no counterpart raises `sa.NoCounterpart` (or is recorded with `NWD_SA_LENIENT=1`). A push/ret logging stub for a callee that has no standalone function is skipped.
* **Pointer VALUES** (not accesses): a test that hands ours a stock pointer (a UClass for an IsA chain, a table) must use `e.gptr(stock_va)` (hook: `NWBASE + rva`;
  standalone: the object itself; an address with no object of ours gets a scratch block). The reverse, ours' pointer -> the stock address it stands for, is `e.stockptr(v)` /
  `sa.inv(v)` (used to log `StaticAllocateObject(class)` and to name pointers into data as the stock does).
* **Imports.** The standalone IAT is built like the hook DLL's (every import is a `ret` stub, the test's `do_import` answers). It also imports a few more engine functions and the
  Winsock functions directly (the hook build reached them through the stock image's thunks): stub them in the test, logging them like the stock thunk call.
* **Not run:** the dynamic initialisers of the standalone (347: the 341 `IMPLEMENT_CLASS` registrations need Core; 5 FString/TArray globals are zero-initialised anyway) and the CRT startup.
  The initial values of the globals are not copied from the stock any more (pw.py `load_stock_data`): the standalone's own data is the thing under test (`gen_globals.py --check` proves it equals the stock's).

## 3. Constructors (tr.py): why the baseline of the hook build changed too
The sa lanes converted `NewStock<T>(Ctor, args)` to `new (mem) T(args)`: the compiler inlines the constructor chain, so ours no longer calls `AddFriendDialog::AddFriendDialog` as a
function and the old harness (the stock constructor is a stub that gives the object a fake vtable) saw a different call sequence: 23 of the wave C tests differed in the HOOK build as
soon as the sa lanes merged (not a bug of the code: a harness limitation). `tr.py` now (default; `NWD_TR_CTORS=0` undoes it) makes every constructor of the db *transparent* on both sides
(it runs for real, its callees stay trapped) and overwrites the real vtable of every class (the stock's and ours) with the fake trapping vtable, so that a constructor that writes the real
vtable pointer still gives the object trapping virtuals. Non-UObject classes with a vtable (Msn...) show their vtable pointer as `vt` on both sides.

## 4. Triage rules (the lane brief)
1. **Harness limitation** (state the standalone reads from a global of ours that the test only set in the stock's memory, a missing import stub, an inlined callee the stock logs,
   a folded address ...): fix the harness; say it in the report.
2. **Real divergence** of the standalone code: minimal fix in `src`/`include` (hook build must stay at the baseline: 5456 hooks, 0 unexplained, `CHECKS: ALL OK` except `vt_audit`)
   or a report with the evidence (stock VA, test, expected/got). The agents of the lane do NOT edit `src`/`include` and do NOT build: they report.
3. **Cannot run**: write down why (no standalone function, test stale in the hook mode too, ...).

## 5. Result of the lane
`python tools/port/emu/run_suite.py --mode sa|hook --cases 30 --seed 1` on the tree with the five source fixes of section 6 (hook build: 5,456 hooks, `5171 ok, 285 explained, 0 unexplained`,
regcheck 0 problems, `CHECKS: ALL OK`; standalone: `smoke_standalone.py` all 9 stages with the stock's output, `verify_standalone.py` unchanged at 36 failures that belong to other lanes):
* **standalone mode: 127 suite entries (90 script files), 1,161 function results, 35,204 cases, every one `differ 0 raised 0`**; 8 functions are `SKIP` (they have no function of their own
  in the standalone: inlined or template instances), `t_misc_rect` and `t_w3ui_apply` are SKIP-only (C4 has no ScaleRect; NCVideoOptionWnd::Apply is proved by `win/t_video.py apply`).
* hook mode, same arguments: 1,164 function results, 35,294 cases, all clean (the hook build inlines `NDataMap::FindRef` in the two drop handlers `t_invdrop` / `t_equipdrop`, so the
  `FindByKey` service never ran; the world now gives the inlined walk a real hash table, `IW.fill_itemtable`, and the service call is logged in the standalone mode only).
  The tr family (`t_msn1..16`, `t_rest1`, `t_vip5`, `t_msnsend`, `t_msnfit`: 638 results, 19,140 cases per seed) is clean in both modes for seeds 1 and 2.
* Before the lane, with the same sweep at 10 cases: hook 46 clean of 112 scripts (23 differed since the sa lanes merged, 42 crashed), standalone had never been run.
* The suite runner also runs the older scripts that were written for the C2 client: 24 were repaired (C4 addresses/symbols; `t_md5_class` now proves `Md5::Transform/CopyWords/DecodeWords`
  that the hook build left stock, against hashlib and the stock), 13 are marked `# STALE` with their C4 successor (list in `run_suite.py`), `win/t_oldwnd.py` is the successor of the
  macro/recipe/shortcut/GM painter tests, `win/t_pwdata.py` exercises the global-store comparison of `pw.py`.
* Sensitivity (`tools/port/emu/mutate_sa.py`: one conditional jump of the standalone function flipped at a time, the script must notice): killed 6/6 (`NCChatFilterWnd::SetFilter`), 13/13
  (`NCGameOptionWnd::Apply`), 12/12 (`NKeyCrypt::Open`), 9/9 (`ExpandToken`), 14/14 (`OnKeyDown`), 8/10 (`OnSocketMessage`), 6/8 (`t_invdrop`), 15/16 (`t_video apply`: the survivor dies at 300 cases);
  a short run leaves mutants alive in the big painters (`t_gara`, `t_video`): use 300 cases for those. The `data` comparison of `pw.py` sees only the 94 named globals of NwGlobals, the
  341 UClass objects and the class vtables (dword granularity, changes only); none of the big pw scripts stores to a named global (`t_pwdata` does).

## 6. Real divergences found (all in code shared by the hook and the standalone build; the judges missed all five: verify_hooks drops vslot 0 and never compares the content of a string)
1. `ChatWnd::ChatWnd` 0x10143BA0, src/nwindow/ChatWnd.cpp: the stock calls `FConfigCache` vslot 0 = `GetBool(L"URL", L"MSNHideWarnMsg", &GMsnHideWarnMsg, NULL)` (0x10143D04); ours called
   `GetInt` (vslot 1), so a "True" setting read as 0 (the stock hides the first-message warning, ours showed it). Fixed with the same `GetBool` call `SwitchboardSession.cpp` makes.
2. `NCMultiEdit::OnKeyDown` 0x1002A190, VK_RETURN: the stock calls `SetText(L"\r\n")` (the string at 0x101A6288 is `0D 00 0A 00`), ours `SetText(TEXT("\n"))`: Enter in a multi-line edit lost its CR.
   Fixed on both lines. Found by `t_multi.py` (11 of 100 cases differ before the fix); found by the messenger test `t_msn3 msn_openchat` for item 1.

3. (static audit `tools/port/audit_strings.py`, 5,454 hooks: the content of every string literal and the vslot of calls through engine interface objects, stock against ours) `NCMultiEdit::OnCreate`
   0x10026830: the width sample passed to `GetTextExtent` is L"¬00" (Hangul GA, 0x101A60D4) in the stock and was the CP949 bytes read as Latin-1 (`"°¡"`) in ours, so CharW
   (+0x2AC) was measured on the wrong character (NCMultiEdit.cpp:1109).
4. `NCNpcView_CharacterSelectWnd::OnCreate` 0x100C27F0: the summon buttons' text is L"Á8C Ö58" (0x101BF694, with a space) in the stock, ours had no space (NCNpcViewrWnd.cpp:597 and :655).
5. `NCReplayListWnd::Refresh` 0x100F7C70: the stock cuts each replay file name at its first dot (`appStrstr(Name, L".")`, 0x101A14A0); ours looked for ".L2R", so "a.b.L2R" gave "a.b" instead of "a"
   (NCReplayListWnd.cpp:186). The audit also checks the vslot of the calls through GConfig/GMalloc/... (0 substitutions left: the ChatWnd GetBool/GetInt case is its test); its limits are in its header.

## 7. Harness limitations found (the classes, for whoever extends the harness)
Constructors/vtables (section 3); the stand-in region (SaUc, gptr, stockptr); linker-folded addresses (identical bodies folded to one address: `TR.fold`, `e9`'s PetAddGM pair); inlined helpers
the stock calls (made transparent on both sides: their callees and stores are still compared); template instances with no function of their own (`sa.OVERRIDES`, `alt` symbols, SKIP);
destructors (trampoline, eax zeroed); by-value `FString` arguments that the standalone takes by reference; Winsock/Imm/CRT imports the standalone calls directly; indeterminate words
(a by-value record's first word, the stack below the call: zeroed); infinite loops of the standalone's inlined `TMap` code on a table the test built without a hash array (the hook build trapped
the map functions, so the test never noticed: `t_msn6`/`t_msn7` timeouts); `NWBASE` seeding of tables the standalone already has (skipped).
Not run in the standalone mode: the CRT startup and the 347 dynamic initialisers (class registration needs Core), anything that needs a real Core/Engine, `emptyint` (`TArray<INT>::Empty`),
`t_itemdebt` group 2 (4 recipe constructors/destructors not in the smoke log), the AVX/ERMS paths of the CRT `memcpy`.
