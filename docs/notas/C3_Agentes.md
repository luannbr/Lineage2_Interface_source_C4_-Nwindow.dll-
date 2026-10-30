# C3: working guide for whoever ports functions by hand (agents and people)

What `tools/port` could not copy from the C2 source is ported here, function by function, in hook mode.
Written in English on purpose (agents read it better). docs/PLANO_C3.md is the project plan.

## 0. The situation in eight lines
- Target: the Lineage II **C3 (Rise of Darkness)** `orig/NWindow.dll` (PE stamp 0x4329071F, 2,732,052 bytes).
  Our code is `src/nwindow/*.cpp` + `include/nwindow/*.h` (one class per file, UE2 style, docs/CONVENCOES.md)
  and it runs as a hook DLL: every function we reimplement replaces the stock one at its **C3 address**;
  every function we have not is a stub `NW_NAKED ... NW_STOCK(0xVA)` that jumps into the stock code.
- The base of this tree is our finished **C2** port (read only, `NW_C1_ROOT`). In the tools' output the "C1"
  side is that C2 source and the "C2" side is the C3 binary: `c1_va` = address in the C2 client, `c2_va` =
  address in the C3 client. Everything in `src/` carries **C3 addresses** (comments and `NW_HOOK_*`).
- **The files of your lane are hand-owned** (listed in `db/hand_owned.txt`): `tools/port/gen.py` and
  `judge_loop.sh` must NOT be run in your worktree; you edit the files by hand. The coordinator keeps
  regenerating the files nobody owns.
- Ground truth is the C3 binary: `analysis/ghidra/asm/<va>.asm`, `analysis/ghidra/decomp/<va>.c` (Ghidra's
  parameter counts and slots are unreliable: check the `ret N`; `python tools/disasm.py <va|Name::Method>`).
- Your build is a private git worktree. Never run anything that writes to the game's own `system` folder.
- A function is done when the cycle and the checks say so, not when it compiles.

## 1. Setup (once)
    bash tools/port/new_worktree.sh <yourname>      # in the main tree: creates ..\wt\<yourname>, branch agent/<yourname>
Work only inside `<repo>\..\wt\<yourname>`. Run the cycle (about one minute):
    cmd //c "tools\port\cycle.bat <repo>\..\wt\<yourname>" > cycle.log 2>&1
It configures, builds, loads the patched C3 DLL outside the game, runs `verify_hooks`, `regcheck`, `verify_data`,
`check_breakers` and `tools/port/checks.py`. Read the end of `cycle.log` and the sections `=== verify` /
`=== regcheck`. Do not use `python -c`/heredocs for edits with backslashes or regexes: they collapse the
backslashes. Use the Edit/Write tools.

## 2. The work packet of a function
    python tools/port/packet.py NCButton::OnPaint        # by name, or: packet.py 10001ca0 (C3 va)
    python tools/port/packet.py --list NCButton          # every function of a class: status and why it was not copied
It prints (1) the map row (C2 va -> C3 va, similarity, category), (2) the C2 source unit, (3) the instruction diff
C2 -> C3 with `this` displacements named after the C3 fields, (4) Ghidra's decompile of the C3 function.
Category C = same shape, constants differ (`analysis/port/constants.csv`); D = the code changed: read the diff,
not the whole function. A C3 function with no C2 counterpart is decompiled from the asm (method: docs/PLANO.md).
Anonymous functions (`sub_XXXXXXXX`): `python tools/xrefs.py <va>` shows who calls them; give the name in
`db/names.csv` (`va,name,evidence`) when you know the class and the role.

## 3. How to port one function
In `src/nwindow/<File>.cpp` find the unit (`// 0xVA  NOT PORTED (...)` and the stub below it) and replace the
stub with the body:

    // 0x10031d80 (vslot 55) - what it does, in one line, English
    INT NCButton::OnPaint(NCGDevice* Device)
    {
        ...
    }
    NW_HOOK_METHOD(0x10031d80, NCButton, OnPaint)

`guard(Class::Method); ... unguard;` where the stock function has the guard frame (the string is in the
decompile: it is the original name). Hook macros: docs/CONVENCOES.md. A constructor needs every ancestor's
constructor ported, or the hook is not installed. Reproduce what the code does, odd parts included. Types: UE2
(`INT`, `UBOOL`, `TCHAR`, `FString`, `FLOAT`...). C3 changed many layouts and signatures against C2: **trust the
C3 binary, not the C2 body**.

## 4. Headers
`include/nwindow/<Class>.h` (yours) has the C3 offsets from the layout votes, with `BYTE _UnknownXXXX[n]`
fillers where nothing is known. When you need a field, derive its C3 offset and type from the C3 code (the
constructor stores, the instruction diff) and declare it; keep `NW_ASSERT_OFFSET` / `NW_ASSERT_SIZE` right (a
window class' real size is in `analysis/classes.json`: it already equals the header's assert). A virtual you
introduce sits at the C3 slot (`analysis/classes.json` has every vtable). Never leave a guessed offset without a
comment. **Structs the client grew** (NItemInfo 0x17AC, NItemEntry 0x1A34, NToolTipInfo 0x7C, NDialogInfo
0xA0, ...) are listed in `db/struct_sizes.csv`: a stack local of the old size dies on the /GS cookie. If you meet
another struct whose stock constructor or frame is bigger than the header says, tell the coordinator in the
report (do not edit shared headers that are not yours).

## 5. Proof (the cycle is the judge)
After each batch run the cycle. Everything must hold:
- smoke: `hooks: N installed, N registered, 0 errors`
- `=== verify`: your functions are `OK`, or `DIFF` with a cause you understood (the pop on return, the virtual
  slots called, the imports). Fix the code. Only if the difference is the compiler's (e.g. we inline what the
  stock calls) add one line to `db/verify_notes.csv` (`c3va,explanation`), specific, never generic.
- `=== regcheck`: 0 problems. A BREAKS line means a stock caller keeps a register alive across a call to a
  function we hooked: do NOT hook that callee, or port its callers too.
- `db/verify_data.csv`: for your functions `status` should be `ok`; offsets that both sides have but different
  (stock-only {58} ours-only {5c}) are layout bugs in a header: fix them. 8-bit-shift constants are benign
  (`db/verify_data_notes.csv`).
- `tools/port/checks.py` must end `CHECKS: ALL OK`. These are the failures the real game runs found, which no
  other judge sees: a hook that reaches an `NW_UNPORTED()` int3 (also behind a constructor's prologue); a hook
  declared `void`/`bool`/`INT` whose stock callers read `eax`/`st0` (**return what the stock function returns:
  GetLocalPos returns its argument**); a stub declared with other arguments than the stock `ret N`; a local
  struct smaller than the stock frame; a constructor that writes past the asserted size.
A function that cannot be made to pass stays a stub. Never delete a stub to hide an unresolved external.

## 6. Things that killed the game (read once)
1. Calling a function we only stubbed with the wrong argument count corrupts the stack (`ret N` of the stub's
   declaration must equal the stock `ret N`; `db/signature_changed.csv` lists the C3 changes: a ported function
   that calls one of those names stays a stub).
2. `NW_UNPORTED()` is an int3: never leave a body of it reachable. An unported constructor of a class we embed
   or construct calls the stock one: `::nw::StockCallAt<void>(0xVA, this, args)`.
3. Packet handlers (`NConsoleWnd::Receive*`, `Add*List`) pop `L2ParamStack` values in order: pop exactly what
   the C3 code pops, in the same order and with the same types (a short pop desynchronises the next packet).
4. Virtual slots only C3 has are placeholders (`NewVirtualN`, jump to the stock code): do not give them bodies
   that disagree with the slot's real meaning; name them in the header when you know.
5. Do not change a hook's calling convention (thiscall member, `__fastcall` ctor wrappers) or its `ret N`.
6. **Two C runtimes live in the process**: the stock NWindow.dll's (static) and ours. Heap is bridged
   (`operator new/delete`, `appMalloc` go through GMalloc), but a `FILE*`/descriptor that stock code opened
   must be read with the stock runtime: `::nw::StockFread(buf, size, count, file)` (stock `_fread` 0x1014dac0;
   add `StockFseek`/`StockFclose` the same way from 0x1014dc38/0x1014db58 when a port needs them). Our `fread`
   on a stock stream faults inside ntdll while taking a lock it never saw (the first wave-1 game run died in
   `FL2FontPage::Load` at engine start, History `UCanvas::UCanvas <- UViewport::UViewport`).
   `tools/port/check_crt.py` (part of checks.py) lists every direct call from our code into a stateful CRT entry.

## 6b. What the first game runs added to the judges (wave 2 gets them in checks.py)
- `tools/port/verify_args.py` (run by checks.py): for every call it compares the constant arguments the stock pushes
  (through registers too: `xor edi, edi ... push edi` is a 0) with what ours pushes. It found `NewFrameCtrl(1, 0, 0)`
  where the stock builds the frame with (0, 0, 0) (a second close button on the target window). When it names a call of
  yours, read the stock disassembly around that call: the value is almost always in a register set earlier. A
  difference that is only the compiler's goes in `db/verify_args_notes.csv` (`va,callee,explanation`).
- `check_crt.py`: our code must not touch a FILE* / descriptor of the stock C runtime (see 6.6).
- A hooked function that *calls a stock function you left as a stub* with a constant argument: the stub's declaration
  is the only place the argument count is written; check it against the stock `ret N`.
- Live call log, for the coordinator and for you if a function misbehaves in the game: `python tools/hook_ini.py log
  Class::Method 'Class::*' 1005d3a0 --logmax 40` writes `[call] va name this= ret= args=` lines (hooked or stock
  functions alike) to `system_nwtest/NWindowDecomp.log`; the names come from `NWindowDecomp.names`.

## 6c. Wave 3 additions (what is left: the C3-only windows, the helpers, the destructors)
- **Your lane** is in `db/lanes_wave3.json` (`classes`, `files`, `anonymous`): only edit those files. Lanes are
  file-disjoint on purpose; if you need a function that lives in another lane's file, leave it a stock stub and say so
  in the report (never edit another lane's file: the coordinator merges lane by lane).
- **Anonymous functions** (`sub_XXXXXXXX`): `db/anon_lane.csv` (`va,size,lane,why`) says which lane ports each one:
  the class whose methods call it, or for a shared helper the lane that calls it most; the others only call it as a
  stock stub (`::nw::StockCallAt<R>(0xVA, ...)` or a `NW_NAKED ... NW_STOCK(va)` declaration of the exact `ret N`).
  `python tools/port/anon_owner.py <va>` lists the callers. Read the callers to find what it is (a method of the class:
  `this` in ecx, a static helper, a callback) and name it: `NW_HOOK_METHOD(va, Class, Name)` with the method declared
  in the header (not virtual unless the vtable has it), or `NW_HOOK_FUNC(va, Name)` for a free function; add
  `va,Class::Name,note` to db/names.csv when you leave it a stub but know what it is. Several `sub_` of a class that
  are really *pieces of one function* (jump-table targets, no caller, 80-130 bytes each, in a row) are not
  functions: skip them, they come with their parent.
- **Destructors**: hook the real `~Class` (`NW_HOOK_DTOR(va, Class)`), never the `scalar deleting destructor` (the
  stock one calls the hooked destructor). The body is what the C++ compiler would generate for the members: check the
  stock destructor's calls (verify_hooks compares them) and give every member that is destroyed there its real type
  in the header (a `FString`, an `NArray`...), not a `BYTE _Unknown` filler, or the stock frees what ours leaks.
- Not in this wave (stay stock): messenger (Msn*, Switchboard*, ChatWnd, L2*, Friend*...), GameGuard NPG*,
  NCBroadcast* / NCReplay* / observer windows, and every `sub_` the lane list does not give you.
- A **game run found** (wave 2 report): a window's ctor that fills fields of a base the stock ctor never initialises
  reads garbage later. When you port a ctor, compare the stock ctor's stores one by one (the C3 disassembly) with
  yours: same offsets, same values, same order of base-ctor calls.

## 6d. Wave 4 additions (the C3-only families: broadcast / observer / replay and the messenger)
- **Lane lists**: `db/lanes_wave4.json` (`classes`, `files`, `anonymous` per lane) and `db/anon_lane4.csv`
  (`va,size,lane,why`; `why` = the owning class, `callers` or `neighbours` when only data tables or several lanes
  reach it: then it is a guess, check the callers with `python tools/port/anon_owner.py <va>`). Lanes: `bgm`
  (NCBroadcastGM*), `bobs` (observer windows, replay windows, NCObserverWnd, NCBenchMarkResultWnd, NCLoginWnd's
  replay paints), `dlg` (the messenger dialogs), `chat` (ChatWnd, ChatBox, Invite/Task windows, L2Chat*/L2Task*/
  L2WordWrapperWnd), `msn1` (Msn, MSNSocket, SwitchboardSession, DispatchProcess, FriendList and the protocol block of
  anonymous functions, roughly 0x10126000-0x10144000), `msn2` (MsnWnd, Msn trees, MessengerWnd,
  Login/Option/Status/Alarm/Loading/Version windows, L2Friend*), `core` (NConsoleWnd's leftovers, the small classes).
- **No C2 source and no existing file**: for each class of your lane create `include/nwindow/<Class>.h` and
  `src/nwindow/<Class>.cpp` yourself (one class per file; examples: `NCZoneTitleWnd`, `NCGodWnd`, `NCStatusBarCtrl`).
  `analysis/classes.json` has, per class: `cpp` (the C++ name; `name` drops the first letter), `size`, `super`, `ctor`,
  `vtable` and `vtable_slots` (the stock function behind every slot: your header must have the same number of
  virtuals, the ones you do not override stay placeholders `NewVirtualN` / inherited). The fields come from the stock
  constructor's stores and from the methods; keep `NW_ASSERT_OFFSET` / `NW_ASSERT_SIZE`. The new files are added to
  `db/hand_owned.txt` by the coordinator when he merges (do not edit that file); never run gen.py.
- **You hook methods, not necessarily constructors**: the stock code creates these windows (the stock NConsoleWnd
  code), so the object has the stock vtable and every virtual you hook is reached through it. Hook a constructor only
  when all the ancestors' constructors are ported; a hooked constructor puts OUR vtable in the object and
  `verify_hooks` then fails a vtable slot that does not reach the same target as the stock slot (the NCEditBox
  slot 35 / NCManorShopWnd 70-72 bugs of wave 3).
- **Messenger code and the C runtime**: the protocol code (`Msn*`, `SwitchboardSession`, `MSNSocket`) uses winsock
  through the DLL's imports (call them the way the stock does: same import, same arguments) and the CRT. Two C
  runtimes live in the process: only stateless CRT calls (`memcpy`, `strlen`, `sprintf`...); `strtok`, `rand`, `fopen`,
  `errno`... are stateful and `check_crt.py` rejects them (use the stock entry through `StockCallAt`). A function that
  runs on another thread (a thread entry point, a callback of a socket or timer) is only hooked when it touches nothing
  of ours that is not thread safe; thread entry points stay stock if in doubt.
- **Order of value**: the window virtuals first (OnCreate, OnPaint, the mouse and key handlers, the packet/event
  entry points), then their helpers, then the anonymous functions. Pieces of one function (jump table targets) are not
  functions. A function you cannot make the judges accept (a 6 KB parser, say) stays a stock stub: say so in the report
  with its address and size; do not burn the budget on it.
- Stay stock on purpose: the GameGuard `NPG*` (a vendor library) and every `scalar deleting destructor`.

## 6e. Wave 5 additions (the residue of wave 4: 122 functions, ~26 KB, almost all messenger)
- **Lane lists**: `db/lanes_wave5.json` (`classes`, `files`, `anonymous`) and `db/anon_lane5.csv` (`va,size,lane,why`), made by
  `tools/port/lanes_wave5.py`. Three agents: `msn5` = lane `msn1` (DispatchProcess, MSNSocket, SwitchboardSession, the protocol block of
  anonymous functions), `ui5` = lanes `chat` + `msn2` + `bobs` (ChatWnd, L2ChatWnd, MsnWnd and their anonymous helpers), `core5` = lane `core`
  (NConsoleWnd leftovers, NotificationProcess, NMsnPassport, MsnCommand, NHtmlParser, small classes, free functions). The files already exist
  (wave 4 wrote them) and are all in `db/hand_owned.txt`: edit them, do not create parallel files for the same class.
- What is left is what wave 4 could not make the judges accept or did not reach: big protocol state machines (SwitchboardSession 1 KB `sub_1013b070`,
  `sub_10126700` 2.2 KB), parsers and callbacks. Read the wave-4 notes of the class in the header/cpp comments first (they say why a function
  stayed a stub). A function that stays a stub after one honest attempt goes in the report with address, size and the judge line that failed.
- The NPG* family (GameGuard, vendor library) is out of scope, as are the 4 anonymous functions the lane table lists as stock.
- Lessons carried over: the judges do not check argument ORDER or branch polarity (re-read the stock stack/jumps for every `StockCallAt` with
  several arguments); two lanes that coordinate by address but not by type duplicate structs (use the existing header types, `NChatMsg.h`,
  `NMsnFriend`); several tiny `sub_` in a row with no caller are pieces of one function, not functions.

## 7. Git
You are on branch `agent/<yourname>`. Commit small and often, English messages, with the trailer the harness
gives you. **Commit only** the files of your lane (`src/nwindow/**`, `include/nwindow/**` that you own),
`db/verify_notes.csv`, `db/verify_data_notes.csv`, `db/names.csv`. The cycle regenerates `db/functions.csv`,
`db/verify_data.csv`, `db/stub_check.csv`, `docs/PROGRESSO.md`: `git checkout -- <file>` them before
committing. Do not push, do not merge: the coordinator merges your branch.

## 8. Report (under 60 lines)
Per file: functions ported (name, C3 va), functions tried and left as stubs and why, headers changed (fields
added with their evidence), new `db/verify_notes.csv` lines, the last cycle summary lines (`hooks: ...`,
`N ok, M explained, K unexplained`, regcheck, `CHECKS:`), and what the next person must know (a struct layout
you could not settle, a struct that grew, an Engine API that changed).

## 9. Cleaning up
Never delete a worktree by hand or with `git worktree remove`: `analysis/ghidra` and `ghidra/census` in it are
junctions to the main tree and a recursive delete empties the main tree. Use `bash tools/port/remove_worktree.sh <name>`.
