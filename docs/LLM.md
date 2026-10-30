# Guide for AI coding agents (LLMs)

Read this before you touch the code. It condenses `README.en.md`, `docs/PLANO_C4.md`, `docs/CONVENCOES.md`,
`docs/notas/C4_Agentes.md`, `docs/notas/C4_Standalone.md` and `docs/notas/C4_EmuSA.md` (the PLANO/CONVENCOES notes
are in Portuguese; the C4 agent guides and the code comments are in English). When this file and the code disagree,
the code and the judges win.

## 1. What the code base is

A C++ rebuild of the Lineage II C4 (Scions of Destiny) `NWindow.dll` (the UI DLL; stock build TimeDateStamp
`0x43C73652`, VC++ 7.0 `13.00.9466`, md5 `afab3ae3fa4f85f7f4c323715e5155e3`, ImageBase `0x10000000`, 341 classes,
343 exports). One source, two CMake targets (configure with `-A Win32 -DNWD_GAME_SOURCES=ON`):

| Target | Define | Role |
|---|---|---|
| `NWindowDecomp` | `NW_HOOK_MODE=1` | hook DLL loaded by the stock `NWindow.dll`; each `NW_HOOK_*` puts a `jmp ours` at a stock entry (5,456 hooks) |
| `NWindowStandalone` | `NW_HOOK_MODE=0` | the whole `NWindow.dll` from our code; it logs in and enters the world in the game |

The tree was forked from the C3 project (which was forked from C2, itself from C1). In the port tools the "c1" side
(`C1_ROOT`, `--c1`, `c1_va`) is the **base** tree, our C3 project (`NWD_C3_ROOT`), and the "c2" side is the C4 image
in `orig/`. `analysis/c2|c3`, `db/c1|c2|c3`, `docs/PLANO_C2|C3.md`, `docs/notas/C2_*|C3_*` are the earlier
projects' data and notes, kept because the C4 tools and notes refer to them.

Key places:

| Path | Content |
|---|---|
| `include/nwindow/`, `src/nwindow/` | decompiled game code, **one class per file**, file = class name (the console is split: `NConsoleWnd*.cpp`) |
| `include/ue2/` | Core/Engine declarations (`UE2Core.h`, `UE2Engine.h`, generated vtables in `gen/*.vt.inc` by `tools/gen_engine_decls.py`) |
| `src/re/` | hook runtime: `NwHook.h` (macros), installer, log, ini, `NwCrtCompat.cpp`, `Prologues.gen.cpp` |
| `src/standalone/` | `NwStandalone.h` (empty hook macros, `NW_SLOT_IS`), `Classes.gen.cpp` (341 `NW_IMPLEMENT_CLASS`), `NwGlobals.cpp` (`tools/gen_globals.py`), `NWindow.def`, `NWindowPackage.cpp` (DllMain), `NwGameGuard.cpp`, `NwCrashLog.cpp`, `NwTrace.cpp` |
| `db/` | `functions.csv` (generated, never edit) + hand-written `names.csv`, `verify_notes.csv`, `verify_data_notes.csv`, `verify_args_notes.csv`, `rawoffs_ok.txt`, `stockargs_ok.txt`, `literals_ok.txt`, `vtslots_ok.txt`, `vslots_manual.csv`, `struct_sizes.csv`, `demote.csv`, `hand_owned.txt` (files the port generator must not overwrite) |
| `analysis/` | facts from the stock binary (`classes.json`/`classes.md`: size, parent, vtable of the 341 classes; `eh.json`; `engine_vtables.txt`, `engine/*.json`: Core/Engine vtables and layouts); `analysis/ghidra/` (not in git) = Ghidra export, `decomp/<va>.c` and `asm/<va>.asm` per function |
| `tools/port/` | the C3 -> C4 pipeline (`match`, `maps`, `layout`, `vslots`, `eligible`, `gen`), the port judges (`checks.py` and its `check_*`), `sa_sites.py` (what still reaches the stock image in the standalone), `map_addr.py` (crash log -> names), `emu/` (Unicorn differential harness) |
| `orig/` | the user's stock binaries (not in git; `orig/README.md`) |

Object model: everything is a UE2 `UObject` (0x34 bytes in this Core). `NCObject : UObject` → `NCCmdTarget` →
`NCWnd` (0xE8 bytes, 74 vtable slots: the window tree) → `NControl` (0x1F4, 75 slots: controls) and the game
windows. `NConsoleWnd : UL2ConsoleWnd` (0x3CA4 bytes, 448 vtable slots: the engine's network/UI callbacks; the
engine declares 425) owns the window manager (`NCVirtualWndMain`) and the in-game desktop `NCConsole` (0x2A0,
`GetDesktop()`), whose `OnCreate` builds every in-game window. Slots the engine does not name are `NewVirtual<N>`
placeholders (`NNewVirtuals.gen.cpp`); never renumber a slot by hand.

## 2. How a function is reimplemented (the loop)

1. Pick `todo` functions in `db/functions.csv` (few are left: the GameGuard library `NPG*` is out of scope by the
   owner's decision; `Md5::Transform` and helpers take arguments in registers). New work is mostly fixes and features.
2. Read `analysis/ghidra/decomp/<va>.c`, then **check the assembly** (`analysis/ghidra/asm/<va>.asm`,
   `python tools/disasm.py <va|Class::Method>`): Ghidra gets parameter counts wrong (check `ret N`), mixes
   parameters with the return address and does not know vtable slots. Helpers: `tools/xrefs.py`, `tools/vtable.py
   <Class>`, `tools/engine_vtables.py <Class>`; for the C3 counterpart `db/fmap.csv` / `db/amap.csv`.
3. Write it in `src/nwindow/<Class>.cpp`, UE2 style, address in a comment, hook macro right after:

   ```cpp
   // 0x1007d200 - a chat line from the server: the colour by the chat type
   UBOOL NConsoleWnd::Say2(L2ParamStack& Params)
   {
       ...
   }
   NW_HOOK_METHOD(0x1007d200, NConsoleWnd, Say2)
   ```

   Declarations carry `// <slot> 0x...` too; invented names carry `(name ours)`. Layouts:
   `NW_ASSERT_OFFSET(Class, Field, 0x..)` for every known field and `NW_ASSERT_SIZE(Class, 0x..)` = the class size in
   `analysis/classes.md`; unknown fields are `UnknownXX`. Each class has `NW_DECLARE_CLASS(Class, Super, Flags,
   StaticClassVA)` and `NW_CHECK_VTABLE(Class, StockVtableVA, Slots)`.
4. Run the cycle until green (section 4). Only after an in-game test: a line in `db/verified.csv`.

Hook macros (`src/re/NwHook.h`; empty in the standalone, `src/standalone/NwStandalone.h`):

| Macro | Use |
|---|---|
| `NW_HOOK_METHOD(va, Class, Method)` | virtual or non-virtual method (a non-virtual forwarder is installed, so a virtual never loops through the stock vtable) |
| `NW_HOOK_METHOD_SIG(va, Class, Method, Sig)` | overloaded method, `Sig = R (Class::*)(A...)` |
| `NW_HOOK_VARARGS(va, Class, Method, Sig)` | `...` method (cdecl, `this` on the stack); must not be virtual |
| `NW_HOOK_FUNC(va, Func)` | free/static function, same convention as stock |
| `NW_HOOK_CTOR(va, Class)`, `NW_HOOK_CTOR_ARGS(va, Class, (Params), (Args))`, `NW_HOOK_DTOR(va, Class)` | constructors / destructor |
| `NW_NAKED ... NW_STOCK(va)` | declared but not decompiled: jumps into stock code (hook mode); in the standalone it is a link error `_nw_stock_<va>` |
| `nw::StockCallAt<R>(va, obj, args...)`, `nw::Addr(va)`, `nw::Field<T>(obj, off)`, `nw::VirtualCall<R>(obj, slot, ...)` | scaffolding; `StockCallAt`/`Addr` have **no definition** in the standalone |
| `NW_GLOBAL(Type, va)` | a stock global (hook mode); the standalone defines it in `src/standalone/NwGlobals.cpp` |
| `NW_SLOT_IS("<decorated placeholder>", "<decorated target>")` | standalone only: a vtable slot that must hold the very function the stock slot holds (an alias at link time) |

Globals are declared in two branches: `#if NW_HOOK_MODE #define GX NW_GLOBAL(T, va) #else extern T GX; #endif`. If a
stock reference escapes `#if NW_HOOK_MODE`, the standalone link fails and names it: `python
tools/standalone_report.py` turns the link errors into a work list, `python tools/port/sa_sites.py` lists every site
that still reaches the stock image.

## 3. Traps (each one has bitten this project)

- **Never hook a scalar deleting destructor**: the stock one calls the (already hooked) destructor.
- **Virtual overloads are laid out in reverse declaration order** by MSVC: declare the higher slot first.
  `NW_CHECK_VTABLE`, `tools/port/vt_audit.py` and `verify_standalone` catch it. A virtual whose name or signature
  differs from the engine's declaration becomes a NEW slot at the end: use the engine's signature.
- **Calling conventions**: thiscall is called through `__fastcall` with a dummy `edx`; varargs members are cdecl;
  `ret N` in the stock assembly is the truth (`verify_hooks.py`, `check_stockargs`, `check_stubs`).
- **By-value non-trivial parameters** (`FString`, `NMacroInfo`, `NToolTipBits`) are destroyed by the callee: a typed
  call that copies them twice destroys them twice (`docs/notas/C4_Agentes.md` §7.3).
- **Register assumptions (`/GL`)**: a stock caller may keep using `ecx`/`edx`/`eax` across a direct call. `tools/regcheck.py`
  lists such callers: hook them together with the callee.
- **Addresses and offsets copied from C3 are wrong in C4** (the C4 classes grew: `UObject` 0x34, `NCWnd` 0xE8, the
  console 0x3CA4; slots were renumbered). `check_addrs`, `check_rawoffs`, `check_literals`, `check_vslots` and
  `verify_data` exist because each of these crashed the game once. A raw `nw::Field<T>(x, 0xNN)` must be in
  `db/rawoffs_ok.txt`, read against the C4 stock.
- **ICF folding**: identical functions share one address in the stock image; one hook serves all.
- **Engine imports** must produce exactly the exported decorated name; `TCHAR` is `unsigned short` (`/Zc:wchar_t-`).
- **Compiler flags are part of the ABI**: `/Zc:wchar_t- /GR- /EHa /arch:IA32 /Oy-`, static CRT; the standalone adds
  `/Ob1` (the stock kept small functions out of line). Keep `legacy_stdio_float_rounding.obj` and `src/re/NwCrtCompat.cpp`;
  never use UCRT `_wtol/_wtoi/atoi` (use `NWtol`/`NAtol`, `include/nwindow/NCrt.h`).
- **x87 arithmetic**: integer × float constant / integer must be written in `DOUBLE`, or results above 2^24 differ.
- **Plain C++ in game code**: no STL, lambdas, `auto` or new templates in `src/nwindow`; UE2 types, `guard/unguard`
  where the original has them, with the original string.
- **Faithful before pretty**: reproduce odd stock behaviour exactly (strings included: the emulator found five
  string/config divergences the other judges missed, `docs/notas/C4_EmuSA.md` §6).
- **Layout is frozen**: never add a field or a new virtual slot to a stock class (its size is registered with Core
  and the engine and stock code index into it). New state goes into globals of your own `.cpp` or into a subclass
  that adds no field (section 5).
- **Stand-in structs** (function holders such as `NConsoleFns.h`, `MsnUiStock.h`) are declared ONCE, in one header.
- **GameGuard**: the hook build still runs the stock library; the standalone has documented no-ops
  (`include/nwindow/NGameGuard.h`) and answers the login server's query itself (`NConsoleWnd::GameGuardQueryX`,
  `src/nwindow/NConsoleWndC2.cpp`). Do not try to port `NPG*`.
- **`#if NW_FEATURES` blocks** in `src/nwindow` are the C1 project's phase-6 call sites, inherited through the
  forks; nothing in this tree defines `NW_FEATURES` and `src/features/` does not exist here. Leave them alone or
  remove them in a separate change; do not use that switch for new work (section 5).

## 4. The judges and what "green" means

Set `NWD_CLIENT_SYSTEM` (a copy of the C4 `system` folder) and have `orig/` and `analysis/ghidra/` in place. Then:

- `python tools/cycle.py --game-source --no-install [--standalone]`: build → smoke (`nwd_smoke.exe`,
  `hooks: 5456 installed, 5456 registered, 0 errors`) → `verify_hooks.py` (0 unexplained) → `regcheck.py --game-src`
  (0 problems) → with `--standalone`: `verify_standalone.py --checks source`, `NWindowStandalone` build,
  `verify_standalone.py --dll`, `smoke_standalone.py` (9 stages, output identical to the stock's) → install (unless
  `--no-install`) → `build_db.py --game-src`.
- `tools\port\cycle.bat` (the agents' judge): the cycle above in hook mode, then `tools/port/verify_data.py`
  (`0 differ`), `tools/port/check_breakers.py`, `tools/port/checks.py` (`CHECKS: ALL OK`).
- `python tools/port/emu/run_suite.py --mode hook|sa --cases 30 --seed 1` (needs `pip install unicorn`, both builds and
  the smoke log; `tools/port/emu/win/pw.py` calls `undname.exe` from the VS 2022 MSVC folder named in `UNDNAME`):
  every result line `differ 0 raised 0`.

**Green = exit code 0 of each, with no new compiler warning.** A difference is never "fixed" by editing a notes file
without understanding it; a note says why the stock and ours differ and what was read to prove it is harmless.
`tools/difftest.py` still has C1 cases and refuses the C4 image. The cycle does **not** draw the UI: visual changes
need an in-game test by a human. `build_db.py` regenerates `db/functions.csv`, `db/verify_data.csv`,
`db/stub_check.csv` and `docs/PROGRESSO.md`: do not commit their churn from a partial run.

## 5. Adding a feature safely

This tree has no feature layer of its own (C1's `NWindowL2`/`src/features` was not carried over). The pattern that
was used for C4 in a separate experiment (extra shortcut bars and auto potions, not part of this source; described
in `docs/MODIFY_INTERFACE.md`):

1. A compile switch of your own (for example `NW_CUSTOM=1`) defined **only** on `nwd_standalone_objs` in
   `CMakeLists.txt`; the hook build never defines it, so the judges keep comparing the faithful code.
2. New code in files of its own (`src/custom/*.cpp`, headers in `include/nwindow/`), added to the standalone sources
   with a `file(GLOB ...)` next to `NWD_STANDALONE_SOURCES`.
3. In decompiled code only the call site, in `#if NW_CUSTOM ... #endif` with a `// custom: ...` comment; the faithful
   code must read the same without it.
4. Wiring points: `NCConsole::OnCreate` (after the stock windows) / `NCConsole::OnDestroy` (before them) /
   `NCConsole::Reset` (leaving the game); `NConsoleWnd::Tick` (every frame); `NConsoleWnd::HookShortCutKey` (keys);
   a window's `WindowProc` (`NWM_COMMAND`), `OnPaint`, `OnDrawToolTip`, `OnLButtonUp`; `NConsoleWnd::Say2` for chat.
5. A new window class that adds **no field** can reuse a stock `UClass` (`static UClass* StaticClass() { return
   NCWnd::StaticClass(); }` + its own `operator new` through `StaticAllocateObject`, `NW_ASSERT_SIZE` equal to the
   base): the stock class table, `verify_standalone` and `smoke_standalone` then see exactly the stock classes. A class
   with fields of its own needs a `UClass` of its own, i.e. a registration and an export the stock does not have:
   the judges would have to be taught to accept exactly that list (the C1 project did it with `NW_DECLARE_L2_CLASS`
   and `tests/standalone/l2_extras.csv`).
6. Settings: a file of your own next to `l2.exe` (the experiment used `system\NWindowCustom.ini`), never the game's
   `Option.ini`/`L2.ini` unless the stock code already reads that key.
7. Talking to the server: only the requests the stock client already sends (`RequestShortCutReg`, `RequestUseItem`,
   bypass...), with the same limits; the server does not change.
8. Prove it: the hook build unchanged (cycle green), the standalone links, `verify_standalone` and `smoke_standalone`
   still pass, a unit test of the engine-free logic, then an in-game test by the user in a copy of `system`.

## 6. Working rules

- **Stay faithful to the original first.** Reproduce, then improve in a separate, annotated change.
- **Prove every change with the judges**; never weaken a judge or a notes file to make a change pass.
- **Small commits**, one subject each, message saying what was checked.
- Never install into the folder the user plays from; in-game tests only with the user's consent, in a copy.
- Never commit game binaries (`orig/`, `*.dll`, `*.exe`, `*.dat`; see `.gitignore`).
- Keep the address comment and the `NW_HOOK_*` together; keep `(name ours)` until something confirms a name.
- When unsure about a stock behaviour, read the assembly again rather than guessing; write down what was verified
  and what was inferred.
- Machine paths come from options or `NWD_*` variables (README: "Machine paths"); never hard-code one.
