# C4 stage 5: the standalone NWindow.dll - working guide

Written in English on purpose (agents read it better). Read `docs/notas/C4_Agentes.md` (sections 3, 4, 5, 7) first: the judges, the git rules and the
classes of bug that killed the game are the same. The C2 project already did this stage; its story and its decisions are in
`docs/notas/C2_Fase5.md` (read the sections "Decisoes", "nProtect e NKeyCrypt no standalone" and "Resultado"). The C3 project never did it.

## 0. What the goal is
The user wants to **abandon the original `NWindow.dll` completely**, as the C2 project did: a `NWindow.dll` built only from our source
(`NW_HOOK_MODE=0`, no hook runtime, nothing of the stock image in memory), loaded by the game in place of the stock one.
Decisions that stay as they were (user, 2026): **GameGuard (`NPG*`, the nProtect static library) is out**: in the standalone it is the documented
no-op of `include/nwindow/NGameGuard.h` / `src/standalone/NwGameGuard.cpp`; NKeyCrypt is real code with `LoadLibrary`/`GetProcAddress` by ordinal.

Pipeline, in order: (1) every file compiles with `NW_HOOK_MODE=0` (done at the start of this stage, except `Classes.gen.cpp`), (2) the link closes with
0 unresolved symbols (this stage's work), (3) `tools/verify_standalone.py` (PE, imports, resources, exports, classes, vtables vs the stock) and
`tools/smoke_standalone.py` (loads outside the game, the same output as the stock) pass, (4) differential tests against the stock (emulator),
(5) the owner tests it in the game (only the coordinator installs: `C4_Client\system_nwtest`, never a `system` folder).

## 1. The state you start from (tag `s5-prep`)
- The hook build is unchanged in behaviour: 5,456 hooks, `hooks: 5456 installed, 5456 registered, 0 errors`, `verify 0 unexplained`, `CHECKS: ALL OK`.
  **Your branch must keep it that way** (a hooked function that now calls a typed function instead of `StockCallAt` is expected to show up as an
  "explained" difference: add a specific line to `db/verify_notes.csv`).
- The standalone objects compile except `src/standalone/Classes.gen.cpp` (13 errors: 12 classes with no default constructor, the `NMsnCommand` defined in
  two headers). Prep fixed: `StockCallAt`/`TraceCall`/`StockFread` for the standalone (`src/standalone/NwStandalone.h`), the 31 console slots that the engine
  declares pure (placeholders in `NNewVirtuals.gen.cpp`), `NCMinimapWnd`'s class pointer.
- What stops the link is the *sites* below. `python tools/port/sa_sites.py` lists them (about 1,720 in 229 files); a site is one unresolved symbol or one
  `int3` the standalone would execute:

| construct (standalone view) | what it is | what it must become |
|---|---|---|
| `nw::StockCallAt<R>(va, this, ...)` (about 1,090) | thiscall into the stock image | a typed call of OUR function that holds that address |
| `NW_NAKED ... NW_STOCK(_AT)(va)` (about 466; 265 of them are the `NewVirtual*` placeholders) | a body that jumps into the stock image | a real body |
| `nw::Addr(va)` (141) | a stock function pointer or a stock table | our function pointer / our table (the data ones: lane `sa-globals-rest`) |
| `NW_GLOBAL(T, va)` | a stock global variable | our global (lane `sa-globals-rest` defines them in `NwGlobals.cpp`) |
| `NW_UNPORTED` / `_NORET` (24) | a `__debugbreak()` where something was never ported | the real code, or proof that nothing reaches it |

`sa_sites.py` prints for every site what the database knows about the target: `done <Class::Method>` = we already have that function (936 of the
distinct targets are `done`, so most of the work is mechanical), `todo` / `lib` / `data/none` = real work (a few dozen; the lane `sa-globals-rest` owns the `todo`).

## 2. The method for each construct
1. **Find our function** that holds the address: `python tools/port/sa_sites.py --files <file> --list` (the `done` name), then
   `grep -rn "0x1014E620" src include` (the `NW_HOOK_METHOD(va, Class, Name)` / `NW_HOOK_FUNC(va, Name)` / `NW_HOOK_CTOR` line) or `db/functions.csv`
   (`src` column). It is a method, a free function, a constructor/destructor, or a forwarder that itself sits on a placeholder (then follow it to the real body).
2. **Check the convention before you convert** (this is where the game died before): the stock `ret N` (`python tools/disasm.py <va>`; `analysis/ghidra/asm/<va>.asm`)
   against the parameters of our function; `this` in ecx; stdcall vs cdecl for free functions; a by-value non-trivial parameter (`FString`, `NMacroInfo`,
   `NToolTipBits`) is **destroyed by the callee**, so a typed call that copies it twice destroys it twice (see `C4_Agentes.md` §7.3 and `src/nwindow/NToolTip.cpp`).
   `python tools/port/check_stockargs.py` (a judge of `checks.py`) only knows the literals that are left; once a site is a typed call the compiler checks the count, you check the meaning.
3. **Convert in both modes** unless the target is a function that really stays in the stock image (only the GameGuard entry points, which already go through
   `NGameGuard.h`). A call of our own function from our code is the same call in the hook build (the stock entry jumps to our function anyway), and the hook judges then
   check it. If one site cannot be shared, split it with `#if NW_HOOK_MODE ... #else ... #endif`; the hook branch keeps the old, verified code.
4. **Template out-of-line copies**: many `StockCallAt` targets are the stock's non-inlined copy of a template member (`TArray<...>::Remove`, `Empty`, `Rehash`, a
   `~TMapPair24Array`). The comment on the line says so. The standalone compiles the template itself: call the member (`Targets.Remove(i, 1)`); the behaviour is the
   same. If the stock copy differs from the template (a different growth rule, a hash size), port the copy as a function of ours and call that.
5. **`NW_STOCK` placeholders** (a method that exists in the header only so the vtable has a slot): keep the existing line for the hook build and give the standalone a real body.
   When the stock function is already ported under another name (`NConsoleWnd::AddCharacterInfo` for slot 36), the body forwards to it with the slot's arguments
   (the signatures are in `tools/ue2_data/l2consolewnd_slots_c4.csv` and in the real method). When the stock function is empty or folded, the body is empty
   (check the stock bytes: a 1-byte `ret` or `ret N` that pops arguments is NOT the same as `{}` for the callers; the vtable check `vt_audit` compares slots).
6. **Never** leave an `int3`/`__debugbreak` body that something can reach; never invent behaviour: a function you cannot prove is reported, not guessed.
7. Raw offsets, `nw::Field<T>(x, 0xNN)` copied from C3, wrong argument counts: `C4_Agentes.md` §7 still applies to every line you touch (`check_rawoffs.py`,
   `check_stockargs.py`, `verify_data`).

## 2b. Recurring patterns (what the sites of the first list really are)
- **`NewStock<T>(Ctor, args)` / `NewPage(ClassVA, CtorVA)` / `NewFrameCtrl`** (about 30 files; `StockCallAt<T*>(Ctor, Mem, ...)` with a *dynamic* address): the stock allocated the object
  (`StaticAllocateObject` of the class) and ran the stock constructor by address. In the standalone the constructor is ours: allocate the same way (`T::operator new` /
  `UObject::StaticAllocateObject(T::StaticClass())`) and construct with `new (Mem) T(args)` of the real C++ constructor. Check that the class's constructor is a real body
  (`NW_HOOK_CTOR(va, T)` with code), **not itself a forwarder to the stock constructor** (some are: "its stock constructor is ret 4", "a forwarder to the stock one"): then the
  constructor is the real work of that site (port it from the stock asm and keep the hook build working: NW_HOOK_CTOR over the real body).
- **`*(void**)this = nw::Addr(0x101DD86C)`** (a stock vtable written by hand in a hooked constructor): hook build only (`#if NW_HOOK_MODE`); in the standalone the compiler writes our vtable
  (and `NW_CHECK_VTABLE` / `vt_audit` / `verify_standalone` prove it equals the stock one slot by slot).
- **`(UClass*)nw::Addr(0x102DA3D8)`** (the stock's `PrivateStaticClass` object): `T::StaticClass()` of the class that lives at that address (`analysis/classes.json`: `static_class` -> `cpp`).
- **`lib` targets** (`send`, `recv`, `connect`, `WSAGetLastError`, `htons`, `inet_addr`, `gethostbyname`, `socket`, `NPKGetLoadStartup`...): the stock image's own import thunks. Call the real import
  (`ws2_32` is on the standalone link line) or, for NKeyCrypt, `NKeyCrypt::GetLoadStartup/LoadAtStartup` (`src/nwindow/NKeyCrypt.cpp` exists; the C2 project converted `NCGameOptionWnd`'s
  three helpers the same way).
- **`dynamic` targets** (`MSNVA_Wnd_NotifyAddFriend`, `Function`, `VA` in a macro): a constant or a macro hides the address; resolve it to the method it names and call that.
- **`Addr` of a table** (`0x102a5b70` Md5's PADDING, `0x101DBEE4`, ...): a stock data table. Lane `sa-globals-rest` provides our copy (initial values from the image) and `nw::Addr(va)` for it;
  if the table is small and read-only, a `static const` in your file is fine and simpler (then the site is yours).
- **GameGuard** (`NPGameLibApi::SendAuthQuery` 0x10169780, `CheckNPGameMon` 0x10169760, `InitGameMon_part` 0x1006BD60): through `NGameGuard.h` (original in the hook build, the documented
  no-op in the standalone); lane `sa-globals-rest` owns that header, the callers' lanes use it.
- **A vtable slot that must hold the stock function itself** (`tools/verify_standalone.py` vtables compares, slot by slot, the function name and `ret N` the stock slot points to with the
  ones ours points to): a `NewVirtual<N>` placeholder, or an override whose stock slot points straight at the base's function (the linker folded it), must not be a thunk or a
  forwarder with its own body. In the standalone branch (`#else` of `#if NW_HOOK_MODE`) delete the body and write `NW_SLOT_IS("<decorated placeholder>", "<decorated target>")`
  (`src/standalone/NwStandalone.h`; the decorated names are in `build\standalone\Release\NWindow.map`): the linker makes the placeholder an alias of the target. A forwarder written
  as `void NewVirtualN() { Member(); }` also loses the member's `INT` result in eax. The hook branch is left as it is.
- **A stand-in struct (a function holder: `struct NConsoleLeftFns : public NConsoleWnd { INT GetClassGroup(INT); ... }`) is declared ONCE**, in a header every file that calls it
  can include, with the union of the members the callers use; the .cpp that defines the members includes that header and defines them out of line (hook line unchanged). Two
  headers (or a header and a .cpp) with the same struct name break `tools/port/vt_audit.py`, whose translation unit includes every header: the compiler's error lands inside
  its own layout report and classes look as if they had lost slots (vt_audit now says so instead). Owners today: `NConsoleFns.h` (the console's small functions, NCConsoleExt,
  NCDeliverWndExt, NCVIPShopWndExt, NSystemMsgFns, NCIconFlashView), `MsnUiStock.h` (neutral messenger ones), `MsnProtoOps.h` / `MsnWndOps.h` (the ones with a protocol base), `NMsnGroup.h`.

## 3. Setup and proof
    bash tools/port/new_worktree.sh <lane>          # the coordinator already did it: <repo>\..\wt\<lane>, branch agent/<lane>
Work only inside your worktree. Each worktree has its own `build\`. Two builds:
- the hook cycle (a few minutes, run it in the background and read the end of the log; this is the judge you must keep green):
      cmd //c "tools\port\cycle.bat <repo>\..\wt\<lane>" > cycle.log 2>&1
  `hooks: 5456 installed, 5456 registered, 0 errors`, `N ok, M explained, 0 unexplained`, regcheck 0 problems, `CHECKS: ALL OK`. Do not start a second cycle while one runs in the
  same worktree (they share `build\`).
- the standalone objects (compile only; the link is the coordinator's after the merge): from a `vcvars32` shell
      cmake -S . -B build -DNWD_GAME_SOURCES=ON
      cmake --build build --config Release --target nwd_standalone_objs -- /m
  The first build of a worktree compiles the whole set (several minutes, 330 files); after that MSBuild is incremental (only the files you changed and the files that include a header you changed
  are rebuilt). English tool messages: set `VSLANG=1033`. A file that fails is rebuilt on every run, so `src/standalone/Classes.gen.cpp` (13 errors, lane `sa-registry`) will always show:
  errors in files that are not yours are not yours, read only the ones whose path is in your lane. Redirect the output to a file and grep it (`grep " error " log | grep -i <yourfile>`).
Your finish line, per lane (all three, in this order):
1. `python tools/port/sa_sites.py --files "<your files, comma separated globs>" --total` prints 0 (or every remaining site is in your report with address, size and the reason);
2. your files compile in the standalone with no new error;
3. the hook cycle ends as above.
Use the Edit/Write tools for files with backslashes or regexes (a bash heredoc into `python -c` eats backslashes). Never run `tools/build_db.py` alone (it needs `--game-src`; the cycle does it).

## 4. Rules (same as every wave)
- Never touch a game `system` folder; never install anything. The coordinator merges; you never merge or push. Never remove a worktree by hand (`bash tools/port/remove_worktree.sh <name>`).
- Edit only the files of your lane (`db/lanes_s5.json`: the files listed, and the header/cpp that share their class name). A header of another lane, `ue2` headers, `NCWnd.h`, `NControl.h`, `NWindow.h`:
  if you need a change there, make the smallest hunk and list it in the report (the coordinator resolves conflicts; two lanes touching the same line is the only real risk).
- `src/nwindow/NNewVirtuals.gen.cpp` has two owners (`sa-vslots-console`: the `NConsoleWnd::NewVirtual` lines; `sa-vslots-other`: every other line). Edit your lines in place; do not reorder, reformat or regenerate the file.
- Commit small and often on `agent/<lane>`, English messages, with the trailer the harness gives you. Commit only your files and `db/verify_notes.csv`, `db/verify_data_notes.csv`,
  `db/verify_args_notes.csv`, `db/names.csv`, `db/rawoffs_ok.txt`, `db/stockargs_ok.txt`. The cycle regenerates `db/functions.csv`, `db/verify_data.csv`, `db/stub_check.csv`, `docs/PROGRESSO.md`: `git checkout -- <file>` them before committing.
- The session scratchpad is shared by all agents: name every temporary file with your lane.
- A site that needs more than about half an hour of investigation: leave it, list it in the report (address, what you tried), and go on.

## 5. Report (under 60 lines)
Sites converted (count by kind), sites left and why (file:line, target address, what blocked), headers changed outside your lane (file, hunk, why), judge lines added
(`db/verify_notes.csv` ...), the last cycle summary (`hooks: ...`, `N ok, M explained, K unexplained`, regcheck, `CHECKS:`), the result of `sa_sites.py --total` and of the
standalone compile of your files, and what the coordinator must know to merge.
