# C4: working guide for whoever ports functions by hand (stage 4)

Written in English on purpose (agents read it better). Read `docs/notas/C3_Agentes.md` first (sections 2 to 6e): the method, the macros,
the judges and the things that killed the game are the same. This file says what is different in the C4 tree. `docs/PLANO_C4.md`
("Etapa 3") is the plan and the result of the pipeline stage that produced the code you start from.

## 0. The situation in ten lines
- Target: the Lineage II **C4 (Scions of Destiny)** `orig/NWindow.dll` (md5 afab3ae3..., PE stamp 0x43C73652, 3,112,980 bytes). Our code is
  `src/nwindow/*.cpp` + `include/nwindow/*.h`, a hook DLL: every function we reimplement replaces the stock one at its **C4 address**; every
  function we have not is a `NW_NAKED ... NW_STOCK(0xVA)` stub (or a `StockCallAt` forwarder) that jumps into the stock code.
- The base of this tree is our finished **C3** port (`<C3 repo>`, read only, confirmed in game). In
  the tools' output the "C1" side is the C3 tree/binary and the "C2" side is the C4 binary: `c1_va` = address in the C3 client, `c2_va` =
  address in the C4 client. Everything in `src/` carries **C4 addresses**. Messages that say "does not compile against the C2 headers" are
  old wording: they mean "the C3 body did not compile/verify against the C4 headers".
- Stage 3 (tools/port/gen.py + judge_loop.sh) already copied the C3 bodies that prove to be the same function in the C4 image: 2,846 hooks,
  23 % of the code, all judges green. **What is left is yours** (about 1.0 MB in 16 lanes): category D (the code changed), candidates that
  did not match, units the judges demoted, C4-only methods and the 36 classes that are new in C4.
- **The files of your lane are hand-owned** (`db/hand_owned.txt`): never run `tools/port/gen.py` or `judge_loop.sh` in your worktree; you edit the
  files by hand.
- Ground truth is the C4 binary: `analysis/ghidra/asm/<va>.asm`, `analysis/ghidra/decomp/<va>.c` (Ghidra's parameter counts and slots are
  unreliable: check the `ret N`; `python tools/disasm.py <va|Name::Method>`).
- **Nothing of the C4 build has run in the game yet.** The judges are all there is. Be conservative: a function you cannot prove stays stock.
- Your build is a private git worktree. Never run anything that writes to a game `system` folder (the project installs only into
  `C4_Client\system_nwtest`, and only the coordinator does it).

## 1. Setup
    bash tools/port/new_worktree.sh <yourname>      # in the main tree: ..\wt\<yourname>, branch agent/<yourname>   (the coordinator already did it)
Work only inside `<repo>\..\wt\<yourname>`. The cycle (a few minutes; run it in the background and read the end of the log):
    cmd //c "tools\port\cycle.bat <repo>\..\wt\<yourname>" > cycle.log 2>&1
It configures, builds, loads the patched C4 DLL outside the game, runs `verify_hooks`, `regcheck`, `verify_data`, `check_breakers` and
`tools/port/checks.py` (which now also runs `vt_audit.py`: the vtable the headers build must equal the stock one slot by slot). Use the
Edit/Write tools for files with backslashes or regexes, never `python -c`/heredocs.

## 2. Your lane and your packet
- `db/lanes_s4.json` (`classes`, `files`, `anonymous` per lane; read it with the Read tool) and `db/anon_lane_s4.csv`
  (`va,size,lane,why`) list what is yours. Only edit the files of your lane (`src/nwindow/<Class>.cpp`, `include/nwindow/<Class>.h`) and **create new
  files for the classes that have none** (the C4-only classes: one class per file, UE2 style; `analysis/classes.json` has `cpp`, `size`, `super`,
  `ctor`, `vtable`, `vtable_slots` per class; examples of hand-made classes: `NCZoneTitleWnd`, `NCStatusBarCtrl`, `NCWnd`). The coordinator
  appends new files to `db/hand_owned.txt` when he merges (do not edit that file).
- Never edit a file of another lane, nor a shared header that is not yours (the `ue2` headers, `NCWnd.h`, `NControl.h` ... if they belong to
  `base`). If you need a change there, leave the function a stock stub and say so in the report.
- The list of what is still to do in your classes: `python tools/port/packet.py --list <Class>` (status of every function and why the pipeline
  did not copy it). The packet of one function: `python tools/port/packet.py <Class::Method|c4 va>` prints the map row, the **C3 source unit**
  (the body that worked in the C3 game), the instruction diff C3 -> C4 and Ghidra's decompile of the C4 function.
- Three kinds of work, in order of value per hour:
  1. **Same shape, constants/fields/slots moved** (the judge named it in the stub's `NOT PORTED (...)` comment: `verify_data` displacements,
     `verify` vcalls, `candidate` constants): the C3 body is right, the header or a slot number is not. Fix the header field (derive the C4 offset
     from the C4 code) or use the C4 slot/constant, remove the stub, re-run. One header fix often flips several functions.
  2. **Category D** (the code changed): read the instruction diff, port from the C4 asm, using the C3 body as a guide to meaning and names.
  3. **C4-only** (no C3 counterpart, no stub in the file): decompile from the C4 asm, add the method to the class header, name it (`db/names.csv`
     `va,name,evidence` when you only know what it is).
- Anonymous functions (`sub_XXXXXXXX`): `python tools/port/anon_owner.py <va>` lists the callers; several tiny `sub_` in a row with no caller are
  pieces of one function, not functions. Name what you port: `NW_HOOK_METHOD(va, Class, Name)` or `NW_HOOK_FUNC(va, Name)`.

## 3. What changed in the C4 image (the lessons of stage 3)
- `UObject` is 0x34 (C3: 0x28); `NCObject` 0x34, `NCCmdTarget`, `NCWnd` **0xE8 with 74 vtable slots** (C3: 0xC8, 68), `NControl` 75 slots,
  `NConsoleWnd` **448 slots**, `UL2ConsoleWnd` 425. Every window class moved by +0x20 or more; the derived layouts in `db/layout.json` are votes, not
  proof: when a judge says a displacement differs, the header is wrong, not the judge.
- Slots: the inherited slots of a class follow its parent's map; `NCWnd` virtuals `OnPaint/OnDrawToolTip/OnChar/OnMouseWheel` are 62/63/64/73;
  `SetPosition` is virtual with 7 arguments (the C3 callers pass 4; see `db/candidate_calls.csv`). `db/vslots.json` and `db/vslots_manual.csv` are
  the maps (read-only for you). A virtual that the stock vtable has and your header does not declare as that slot is a `vt_audit` failure.
- `UL2ConsoleWnd` (Engine) slots without a known name are `NewVirtual<N>() = 0` in `include/ue2/gen/UL2ConsoleWnd.vt.inc`; the signature table is
  `tools/ue2_data/l2consolewnd_slots_c4.csv`. If you learn a name or signature there, report it (shared file).
- Structs that grew: `db/struct_sizes.csv` (column `insert`), e.g. `NItemInfo`, `NToolTipInfo` (+4 at 0x18). A local of the old size dies on the /GS cookie.
- C3-era assumptions to distrust: `FindTexture(Name)` has a second argument (1); several `SetText(*SysString(n))` became `SetTextID(n)`; Engine
  and Core of the C4 client are newer (27/05/2006) than the NWindow (13/01/2006), so an Engine function may differ from what the C3 headers say.
- GameGuard (`NPG*`) stays stock, always. Never hook a `scalar deleting destructor`.

## 4. Proof
Same as C3 guide section 5 (smoke, `=== verify`, `=== regcheck`, `verify_data`, `verify_args`, `checks.py` must end `CHECKS: ALL OK`). The baseline of
the tree you start from is `hooks: 5387 installed, 5387 registered, 0 errors` and `CHECKS: ALL OK` (wave C; it was 2846 when wave A started, 4224 for wave B): your branch must never go below that. Notes
for a difference that is the compiler's: `db/verify_notes.csv`, `db/verify_data_notes.csv`, `db/verify_args_notes.csv` (specific, never generic).
A function that cannot be made to pass stays a stub: report it with its address, size and the judge line that failed; do not burn the budget on a
6 KB parser.

## 5. Git
You are on `agent/<yourname>`. Commit small and often, English messages, with the trailer the harness gives you. Commit only your lane's files and
`db/verify_notes.csv`, `db/verify_data_notes.csv`, `db/verify_args_notes.csv`, `db/names.csv`. The cycle regenerates `db/functions.csv`, `db/verify_data.csv`,
`db/stub_check.csv`, `docs/PROGRESSO.md`: `git checkout -- <file>` them before committing. Do not push, do not merge: the coordinator merges your branch.
Never delete a worktree by hand (junctions to the main tree): `bash tools/port/remove_worktree.sh <name>`.

## 6. Report (under 60 lines)
Per file: functions ported (name, C4 va), functions tried and left as stubs and why (address, size, judge line), headers changed (fields added with
their evidence), shared-file changes you needed but did not make (vslots, struct sizes, ue2 slot names), new notes lines, the last cycle summary
(`hooks: ...`, `N ok, M explained, K unexplained`, regcheck, `CHECKS:`), and what the next person must know.

## 7. Wave B: what wave A taught us (read this before you touch a line)
Wave A (8 lanes, 1,378 hooks) is merged: the tree you start from has 4,224 hooks = 62.5 % of the code (`docs/notas/C4_Levantamento.md`). The 4,224 build
has **not run in the game yet**; the 2,846 and 3,617 builds entered the world, and every crash on the way was one of the classes of bug below.
The judges you run in the cycle now include the two that were written to catch them: do not fight them, read what they print.

1. **C3 numbers inside a C4 body.** A raw `nw::Field<T>(x, 0xNN)` or `(BYTE*)x + 0xNN` copied from a C3 body is wrong whenever the object moved
   (+0x20 for everything under `NCWnd`, +0x0C under `UObject`). `ReceiveMacroListInfo` memset the heap over other windows with C3 offsets and the
   client died ten minutes later in an unrelated paint. `verify_data` cannot see it when the hook is a forwarder (a by-value parameter) and the real body
   sits behind it, and an inherited `C3:` note in `db/verify_data_notes.csv` silences it. Rules: (a) prefer a typed header field to a raw offset, and
   derive its C4 displacement from the C4 asm (`python tools/disasm.py <va>`), not from the C3 header; (b) `python tools/port/check_rawoffs.py` fails on
   every raw-offset line that is not in `db/rawoffs_ok.txt`: read EACH new line against the C4 stock function (the same displacement must be there),
   then `--update`; never `--update` unread; (c) do not copy an old `C3:` note, write a new one with C4 evidence.
2. **Stock calls with the wrong number of arguments.** `NewStock<NCCheckBox>(0x100058C0, id, 0, 0)` gave the constructor (it pops 16 bytes) three
   dwords, and the next store went to NULL (crash in `NCGMSnoopWnd::OnCreate`). `python tools/port/check_stockargs.py` compares every literal
   `StockCallAt/StockCall/NewStock/NewStockObj` with the stock `ret N`; a mismatch is a bug unless the argument is wider than a dword (then list it in
   `db/stockargs_ok.txt` with `--update` after reading it).
3. **By-value non-trivial parameters are destroyed by the CALLEE (MSVC).** A hook with an `FString`/`NToolTipBits`/`NMacroInfo` by value must not be
   `naked` (C3068) and must not re-enter our own code twice: copy the argument with the stock copy constructor and call the stock function by address
   (`__fastcall` with a dummy `edx`); the pattern is in `src/nwindow/NToolTip.cpp` (`NToolTipSkin::Draw/DrawFull`). Known latent bug for the messenger
   lanes: `NMsnFriend(const FString&)` forwards `&FString` where the stock pops the FString by value.
4. **`NW_UNPORTED_NORET()` is now `__debugbreak()`** (it used to be `__assume(0)`, which made the optimizer silently delete the code of every caller and
   hid the breaker from `check_breakers`). A hooked function that can reach an unported stub is reported by `check_breakers`; either port the callee in
   your lane or leave the caller stock. **13 hooks were switched off for exactly this reason** (their `NW_HOOK_*` line is commented with the marker
   `C4 stage 4: NOT HOOKED while the function reaches an unported stub`): `AddFriendDialog`, `AddGroupDialog`, `RenameGroupDialog`, `ChatWnd::RefreshRoster`,
   `Msn::FindFriend`, `NMsnNotifyCommands::SendBlock`, `NotificationProcess::ApplyList/OnMessageImpl`, `NCBroadcastObserverWnd::NotifyMagicSkillUse`,
   `NCQuestGaraTreeWnd::OnMarkCheckBox`. If your lane ports their callees, switch them back on (remove the marker, uncomment) and prove them again.
5. **One address, one hook.** A second `NW_HOOK_*` on an address that already has one is a smoke error (`duplicate hook`). Seven hooks of nconsole's
   stand-in classes were commented out for it (`NCChatWndAllyExt`, `NCListsWndExt`, `NCPartyRoomMembersExt`, `NCAbnormalStatusWndExt` ...): if you port the
   real class and its address is already hooked in another file, say so in the report; do not delete the other file's hook yourself.
6. **C4 layouts already measured** (use them, do not re-derive): `NCWnd` 0xE8, `UObject` 0x34, `NConsoleWnd` 448 slots, the desktop macro window
   `NewWndF8` is at +0x150 of the desktop, `NCMacroListWnd` Macros +0x12C / Slots +0x13C (stride 0x3FC), the pawn's private-store message `FString` at
   +0x16A8 (buy) / +0x169C (sell), `GConsole+0x3C00` is the petition conversation window (the `NConsoleWnd.h` comment says +0x3C04: wrong), `NListCell` tail
   `bTextVisible` +0x1A60, `NItemInfo` 0x19B4, `NItemEntry` 0x1C40 (ToolTip at +0x1BC0), `NToolTipInfo` 0x80, column record `NColumnInfoC4` 0x4018 (headers
   `NItemC4.h`, `NItemEntryC4.h`, `NManorC4.h`). `NItemInfo.h` / `NCItemWnd.h` are still C3-shaped (debt of wave A): if your lane needs them, use the C4
   headers and report the mismatch. Other open header debts: `NCGDevice::DrawSpecialDigit` has a 6th argument, `NCTabControl::AddTab` takes 9, `NCButton` has
   C4-only constructors, `NCComboBox::AddItem` addresses.
7. **Proof beyond the judges.** `tools/port/emu/` is a unicorn differential harness (stock vs ours on the same inputs); the `t_*.py` files are examples
   (docs/notas/C2_Porte.md). Use it for anything with a loop, a parser or arithmetic (HTML parser, tooltip layout, `Md5`, the IME): the judges prove shape,
   not behaviour. A function you cannot prove stays a stock stub: say so in the report, an honest dead end is better than a hook that corrupts the heap.
8. **Do not rerun a cycle while another `checks.py` of your tree is still running** (the DLL stays locked and the link fails with LNK1104); wait for the
   `CHECKS:` line. Python scripts with backslashes or regexes go through the Edit/Write tools, never a shell heredoc (it collapses the backslashes).
9. **Reports**: list every raw-offset or stock-call line you added and the C4 evidence, every header field you added, every hook you could not keep, and the
   judge summary lines. Update `db/verify_notes.csv`, `db/verify_data_notes.csv`, `db/verify_args_notes.csv`, `db/names.csv`, `db/rawoffs_ok.txt` and
   `db/stockargs_ok.txt` on your branch (they are line-based, the coordinator merges them).

## 8. Wave C: the rest, the debts, the proof (read sections 0 to 7 first, then this)
Wave B (8 lanes) is merged: the tree you start from has **5,387 hooks = 92.19 % of the code** (`docs/notas/C4_Levantamento_B.md`: read its
"Dividas" and "Verificacao" sections). Wave C is smaller in code and larger in care. What is left of the code is the hard rest (about 20 KB: every item
was already tried by a wave-B agent and left stock for a reason, which the survey and `python tools/port/packet.py <Class::Method>` tell), and the
5,387 build has **never run in the game** (only builds of at most 3,617 hooks entered the world). So the lanes are: `ime`, `inven`, `misc`
(close the rest), `debt1`, `debt2` (pay the debts), `audit` (the `check_literals` triage), `proof1`, `proof2` (prove, against the stock, the
windows that only the judges have seen). An honest dead end is fine: a function you cannot prove stays stock, with the reason and the judge line in
your report. A hook that is wrong costs a crash in the game; a stub costs nothing.

### 8.1 What wave B found that no judge could see (every one is a class of bug: look for its siblings in your files)
1. **Tiny functions folded by the linker are ambiguous in the C3 -> C4 address map.** The Winsock `jmp [IAT]` thunks and the `mov eax,[ecx+N]; ret`
   list getters of the messenger were remapped to a neighbour. Any address literal of a function under ~20 bytes: `python tools/disasm.py <va>` and read
   it. C4 Winsock thunks: WSAGetLastError `0x10170B02`, connect B08, htons B0E, gethostbyname B14, inet_addr B1A, socket B20, WSAStartup B26, recv B2C,
   send B32, WSAAsyncSelect B38, htonl B3E, closesocket B44. Messenger list getters: friends `0x1016EC20`, allow `0x10151140`, block `0x10005B90`,
   reverse `0x10151150`, groups `0x10151160`. `_Xlen` `0x10170BC9`, `_Xran` `0x10170B71`, `__CxxThrowException` `0x10171169`.
2. **A virtual slot written by hand** must be read off the stock call site (`call [reg+N*4]`). `NCGDevice::DrawPledgeCrest/DrawAllyCrest` called the
   NetHandler slots 43/44, the stock calls 45/46 (`NConsoleWnd::GetPledgeCrestTex/GetAllyCrestTex`, `0x1006A900/0x1006A910`). `verify_hooks` saw it only
   because the vcall sets differed; it cannot see a wrong slot when both sides call "a" virtual.
3. **Whose `this`?** `NMsnHeaderMap::HasHeader/GetHeader` have the message as `this`, not the map. A header that makes a method a member of the wrong
   class compiles and passes `verify_hooks` when the offsets agree by accident.
4. **A base address of a table is a C3 number until proved**: `GL2GameData` SystemMsg is at `+0x722C8` (stride 0x30, text at +4; the C3 `0x3F638` was
   wrong), SysString `+0x9871C` (stride 12), quest table `+0x3D28` (2000 x 0xA0). `GConsole+0x3C68` is the special-server flag and `+0x3C6C` the party item
   rule (the header names `PartyItemDistribution` are swapped: debt1 fixes it). Pawn flags/team/slot `+0x1744/+0x1748/+0x174C`, ReplayManager `+0x5244`.
5. **The judges themselves were wrong in places**: `verify_data.py` read the import table as UTF-16 (hundreds of false differences, fixed),
   `check_breakers.py` held C3 RVAs (fixed). If a judge is silent about something you know is risky, read the stock yourself.
6. Pending macros: `MSN2_AFTER_MSN1_*` and `MSN1_AFTER_MSN2_*` are gone (plain `NW_HOOK_*` now); `MSN2_DUP_*` stay OFF (`MSN2_HOOK_DUPLICATES=0`).
   No hook is switched off any more (`grep "NOT HOOKED" src` is empty): do not add one.

### 8.2 Rules of the wave
- Same as before: private worktree, never a `system` folder, no GameGuard (`NPG*` stays stock, forever), never hook a `scalar deleting destructor`, never
  run `tools/port/gen.py`/`judge_loop.sh`, never `python tools/build_db.py` alone (the cycle does it), the Edit/Write tools for any Python/regex with
  backslashes, `tools/port/remove_worktree.sh` only (the coordinator removes worktrees), commit small and often with the trailer the harness gives you.
- **Files**: edit only the files of your lane (named below); a shared header or a file of another lane that you need to change: leave it, say so in the
  report (the exceptions are the files the brief hands you explicitly). A new class gets a new file. `db/rawoffs_ok.txt` and `db/stockargs_ok.txt` are
  line-based: use `--update` AFTER reading the new lines against the stock, as in 8.1/section 7.
- Every hook you add: the cycle must end `0 unexplained` and `CHECKS: ALL OK`; and every body with a loop, a parser, arithmetic or a state machine needs
  a differential proof in `tools/port/emu` (stock vs ours on the same inputs; look at `tools/port/emu/win/t_*.py`: they show the pattern for windows,
  ctors, paints, the IME, the tooltip, the VIP window, `Md5`). Put the new `t_*.py` files you write in `tools/port/emu/win/` and commit them.
- `python tools/port/check_literals.py <file.cpp>` (read the head of the script): run it on every file you touch; a literal it flags is a bug until
  you have read the stock function and shown otherwise.

### 8.3 The lanes
**ime** (files: `NCTaiwanIME.cpp`, `NCEditBox.cpp`, `NCCandidateBox.cpp`, `Util.cpp`, their headers). The Taiwan IME (a locale class the other IMEs
`NCChineseIME/NCJapaneseIME/NCKoreanIME/NCThaiIME` share a base with, already ported and proved with `tools/port/emu/win/t_ime.py`, `t_ime2.py`,
`cov_ime.py`): `NCTaiwanIME::GetImeId 0x10016BD0` (361 B), `ReadReadingString 0x10016D40` (567), `OnInputLangChange 0x10017040` (29), `Deactivate 0x10017060`
(93), `OnCreate 0x1001A2B0` (205), `OnDestroy 0x1001A380` (169), **`OnImeComposition 0x1001A460` (3,124)**, `ReadCandList 0x1001B2B0` (221), `OnImeNotify 0x1001C950`
(456); plus `NCEditBox::WindowProc 0x10015FE0` (315), `NCCandidateBox::ShowWindow 0x100EFC40` (5), `Util::GetLanguageID 0x101685F0` (223) and
`HasNoFinalConsonant 0x10064AD0` (54, name it). Extend the IME harness to the Taiwan class (the imm32 calls go through the stock's own IAT) and prove each
function before it is a hook; the order of value is `OnCreate/OnDestroy/Deactivate`, the readers, then `OnImeComposition`.

**inven** (files: `NCInvenItemWnd.cpp` and the classes below). **`NCInvenItemWnd 0x100976C0` (2,701 B; the db calls it `OnLButtonDown`, find the real slot and
name)**: the drag-and-drop of an inventory item over about eight window classes; it uses the C4 `NItemInfo` 0x19B4 / `NItemEntry` 0x1C40 headers and the
raw offsets of other windows (`check_rawoffs` will make you read every one). `tools/port/emu/win/t_lbup.py`-style tests exist for neighbours
(`t_small.py`, `t_ctor.py`); write the proof first for the branches the drop can take. The slot-window family, with its constructors/destructors that wave B left
stock: `NActionItem::NActionItem 0x1004FF60` (197), `NItemEntry::IsSelected 0x1001E590` (7, folded: check the map), `NCSkillItemWnd 0x100AE9D0 / 0x100AEA70`,
`NCSkillWnd 0x100AEC30`, `NCGMSkillItemWnd 0x1008FC30 / 0x1008FCD0`, `NCGMSkillWnd 0x1008FD10`, `NCGMInvenItemWnd 0x1008F6F0`, `NCGMPledgeWnd 0x10090460`,
`NCGMQuestWnd` dtor `0x10090360`, `NCHennaInfoWnd` ctor/dtor `0x10093A60 / 0x10093B40`, `NCManorShopWnd 0x100B0E70`. Find out why each was left stock
(`packet.py --list <Class>`): an inherited-constructor / `InternalConstructor` chain is a reason to leave it, a by-value argument is a reason for the
forwarder pattern.

**misc** (files: the file of each class named; check the owner of every address with `python tools/port/anon_owner.py`). The rest, by value:
(1) `NToolTipSkin::Draw 0x10032860` (784) and `DrawFull 0x10032560` (762): the nine-slice frame; by-value `NToolTipBits` forwarders (pattern in `NToolTip.cpp`),
the C3 body is the same shape; proof with `t_tipdraw.py`. (2) `SwitchboardSession::SendMessageBody 0x10160A60` (466; today `NMsnSender::SendMessageBody` in `MsnSend.cpp`
is an `NW_UNPORTED_NORET` stub, by-value command 0x1C + message 0x74) and `MsnFitLine 0x10168A10` / `MsnFitText 0x10168B10`. (3) `NCBroadcastObserverWnd`
`0x1012DDC0`, `0x1012DDD0`, `0x1012F078`. (4) `NWndUtil::MakeTimeStr 0x10037BC0` (228): the hook line is commented out in `NWndUtil.cpp` because `check_frames`
mis-reads the stock frame; read the stock, fix `check_frames`' `KNOWN_FRAMES` entry with evidence (you own that edit) and turn the hook on. (5) `Md5::CopyWords 0x1014B150`,
`DecodeWords 0x1014B190`, `Transform 0x1014B1D0` (2,228 B; dead code in hook mode: shims with register arguments failed `regcheck`); `t_md5.py`/`t_md5_class.py` exist; only
if a form of the hook passes all the judges. (6) `appMemcpy 0x10001CB0`, `NStrihash 0x1013E3C0`, `NMapBase24_Relax 0x100BB820`, `NCStatusBarCtrl::StartAnim 0x1010E3E0` (184).
(7) The tiny ones, last: `NCChatListBox::OnDestroy/SetWindowSize 0x10052910/0x10052AD0`, `NCTrainListBox` ctors `0x1011E300/0x1011E3C0`, `NCMatchListCtrl 0x100B4500`,
`NCMatchPartyListCtrl 0x100B18B0/0x100B5450/0x100B54F0`, `NCRecipeBuyListWnd::OnDestroy 0x1011DC90`, `NCRecipeListBox::OnDestroy 0x1011DDC0`, the recipe dtors
`0x100F3600/0x100F6870/0x100F7030` and ctor `0x100F6D80`, `NCMinimapWnd::KillFocus 0x100B9010`, `NCPawnSelectWnd::GetSelected 0x1009B6A0`, `NCBBSHtmlViewer::OnTimer 0x1003B430`,
`NCAbnormalStatusWnd::HideWindow 0x100471A0`, `NCWnd::OnRButtonDown 0x100023B0`, `NCWnd::GetPosFlags 0x1014FC10`, `NCCmdTarget::OnTimer 0x100023D0`, `NHtmlObject` dtor `0x1003AC40`
(each is 4 to 100 bytes and many are folded duplicates: decide with `disasm`, hook only those that are real separate functions with a real caller). (8) The anonymous `sub_`
pieces (`db/anon_lane_s4.csv`, `anon_owner.py`): several tiny `sub_` in a row with no caller are pieces of one function, not functions; classify each (piece / data / real
function) in your report; port the real ones (`sub_10003C00` of `NCConsole::OnCreate`, `sub_10006850` of `NCMatchWnd::OnCreate`, `sub_1002E860` of `NCChatWnd::OnCreate`,
the `NCTabControl` helpers `0x10001EF0..0x10003B00`).

**debt1** (the console and the virtuals; you own `NConsoleWnd*.cpp/.h`, `NCConsole.cpp`, `UL2ConsoleWnd`, `NNewVirtuals.gen.cpp`, `include/ue2` `URenderDevice`).
(1) `NConsoleWnd.h`: `PartyItemDistribution`/`GetPartyItemDistribution` are swapped (`+0x3C68` special-server flag, `+0x3C6C` party item rule; the getter `0x100826A0`
returns the flag): rename with the C4 evidence and fix every user (`grep`), then `check_rawoffs`. (2) `NConsoleWnd` ctor/dtor `0x100824F0 / 0x10082900` (355 / 162 B) and
`UL2ConsoleWnd::RequestSendL2Friend... / RequestSendMsnChat... 0x1006CA80 / 0x1006CA90`: port them if they are provable, else report. (3) The redundant stand-ins:
the `NW_STOCK` stub of `SetTeamState` in `NConsoleWndMisc.cpp` (~line 744) and the constructor `StockCallAt` lines in `NCConsole.cpp`
(`0x101285F0 / 0x101229B0 / 0x1012F5D0 / 0x101299B0 / 0x1012CD70`): the real constructors are hooked in wave B, so these became dead weight (and a `StockCallAt` of a hooked
function goes through our hook: check that before and after). (4) `NNewVirtuals.gen.cpp`: slot 56 of `L2ChatWnd` (`L2ChatWnd::vfn056 0x10148DE0`, 11 B), `NCPetWnd::vfn056 0x100DB1D0` (8 B),
`NCTradeWnd`/`NCRecipeShopWnd`, VIP scroll virtuals (the stock jumps are hooked through stand-in `Ext` structs: give each a name and a real declaration, or document why not).
(5) `include/ue2` `URenderDevice` slot 27 `SetRes`: the stock takes viewport + 5 ints, the header declares 6 (find every caller; the `ue2_check` run must stay ALL OK).
(6) `NToolTipInfo::SetText` header stub has C3 `0x1002C4D0` (C4 `0x10032460`).

**debt2** (items, comments and the db; you own `NItem*.cpp/.h`, `NCItemWnd.h`, `NCShopWnd.cpp`, `NCGaraTree.cpp`, `NCRecipeShopItemWnd.cpp`, `NCTVItem.h`, `NShortCutItem*`,
`NHtmlLayout.h`, `NMsnDlgCommon.h`, `NCLoginWnd.h`, `NSSQHelpers.h`, and all the `db/*.csv` notes files). (1) `NItemEntry::~NItemEntry 0x10050030` as a real hook (frees three
destructors that call a stub) and the stand-in constructors `NCShopWnd()` / `NCGaraTree()`: replace them with the real ones. (2) `NCRecipeShopItemWnd.cpp` still has `Field(&Info,0x18)`
(C3). (3) `NCTVItem.h` is C3-shaped; `NItemEntry::FillToolTip` (`NCItemWnd.h`) uses the C3 offset `0x19B8` (C4 tip at `+0x1BC0`); `NShortCutItem::Kind` is a C3 field; `NHtmlLayout.h` is
in no lane file list. (4) The comments with C3 numbers: `NMsnDlgCommon.h:24` (`+0x3F638`), `NCLoginWnd.h`, `NSSQHelpers.h`. (5) The db: misnames `0x100F87D0 / 0x100F95F0` are
`NCReplayLogoWnd::OnPaint` / `NCReplayPlayWnd::OnPaint`, `0x1013B830 / 0x1013CBF0 / 0x1013D920 / 0x1013E000` are hero tower / olympiad `OnCreate` (`db/names.csv` with evidence);
the 15 `conv` functions of the new classes that are `todo` in `db/functions.csv` (decide: real hook or by-convention); the 13 `verify_data` rows without a note (the guard names of
sys1: Login / Option / GameOption / NoticeInfo / MapCover / QuestListInfo; each note must carry C4 evidence, not a copied `C3:`). (6) The standalone tables `analysis/classes.json`
/ `Classes.gen.cpp` lack the new classes (`NCQuestListInfoWnd`, `NCReplay*`, the manor family, olympiad, hero tower, command channel): add them if the generator allows it without
touching the hook build.

**audit** (you own `tools/port/check_literals.py`, `tools/port/checks.py`, a new `db/literals_ok.txt`; you may FIX a source line when you prove a bug, in the smallest possible
commit, one commit per bug, and list each in the report because the file belongs to nobody now and another lane may touch it). `python tools/port/check_literals.py --all` (80 s)
flags 527 hook units (of 5,394) that use a literal address the stock function does not use. Most are vtable constants in constructors, header helpers whose name collides,
`NStl*` helpers and the like; some are bugs of the Winsock/getter class (8.1 point 1). Triage ALL of them: for each, read the stock function and decide *false positive (why)*
or *bug*; fix the bugs; write the reviewed ones to `db/literals_ok.txt` (same idea as `db/rawoffs_ok.txt`: key `file|unit` and a one-word reason), teach the script a `--update`
and make it a judge of `checks.py` that fails on a flagged literal not in the list. Improve the script where the false positives have a pattern (a vtable literal in a hooked ctor is
checked against the vtable of the class). Then do the same hunt for the other classes of 8.1 with a script of your own if one can be mechanical: (a) every tiny (<= 20 B) stock
function used by address in a body: its first bytes against what the body assumes; (b) every `VirtualCall<...>(obj, N, ...)` / `NW_VCALL` with a literal slot against the stock's
`call [reg+N*4]` of the same function.

**proof1** and **proof2** (the in-game risk: judge-proven only; you may edit the file of the window you prove, minimal fixes, one commit per bug, listed in the report; new
`t_*.py` in `tools/port/emu/win/`). The method is a differential run in the unicorn harness of the stock function against ours on the same inputs and window layouts, comparing
memory writes and the calls made (see `t_vipwnd*.py`, `t_ctor.py`, `t_entry.py`, `t_tipdraw.py`); where the harness cannot run it, a line-by-line read of the stock asm against the source, written
in the report with the evidence. Report every divergence, fix the real ones.
- **proof1** (the system windows, wave B lane sys1/sys2): `NCVideoOptionWnd` (the biggest rewrite: OnCreate, the apply/OK path, the combo fills, resolution changes), the server select window
  (`ServerID` keying, `NCServerSelectWnd`), `NCGaraDialogBox::OnOK` (every dialog type: trade / party invite / drop-destroy item / quest delete `0x1E` / clan crest `0x53/0x54`),
  `NCLoginWnd`, `NCGameOptionWnd`, `NCNoticeWnd` (map cover, quest list info, credits).
- **proof2** (messenger and items, lanes msn1/msn2/items3/rest1): the messenger windows that run on the first entry into the world (`MsnWnd`, `L2FriendWnd`, `L2TaskWnd`, `MessengerWnd`:
  `OnCreate`/`OnPaint`/the roster fill, the Winsock senders `MsnSend.cpp`/`MSNSocket.cpp`), then rest1's olympiad / hero tower / command channel / chat list boxes / fish viewport
  paints and `OnCreate`s, then the VIP/multisell windows (`t_vipwnd*.py` already prove part).

### 8.4 Report (under 70 lines)
Per function: ported / proved how / left stock and why (address, size, judge line); every raw offset or stock-call line you added with its C4 evidence; every header field you changed;
every bug you found (file, line, address, what the stock does, what we did), whether fixed; the tests you added (`t_*.py`); the notes lines; the last cycle summary (`hooks: N installed...`,
`N ok, M explained, K unexplained`, regcheck, `CHECKS:`); and what the next person must know.
