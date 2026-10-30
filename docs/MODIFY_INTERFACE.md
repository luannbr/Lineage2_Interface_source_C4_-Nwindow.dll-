# How to modify the interface

[Português](MODIFICAR_INTERFACE.md) · **English** · [Русский](MODIFY_INTERFACE.ru.md)

A practical guide to changing the C4 interface in this source: how the windows are built, where new code plugs in,
and step-by-step recipes (a new shortcut bar, a button, a window, a chat colour, a tool-tip line). Interface changes
go into the **standalone** build (`NWindowStandalone`) behind a compile switch of your own; hook mode and the plain
standalone stay faithful to the original, and that is where the judges compare with it.

## 1. How the windows are built

### The hierarchy

| Class | File | Role |
|---|---|---|
| `NCObject : UObject` (0x34 bytes) | `include/nwindow/NCObject.h` | base of everything; `FindTexture(TEXT("Package.Group.Name"))` loads a texture |
| `NCWnd` (0xE8 bytes, 74 vtable slots) | `include/nwindow/NCWnd.h` | every window: position (`LocalX/Y` relative to the parent, `ScreenX/Y`, `Width`, `Height`), style, children, focus, title bar (`FrameCtrl`), minimised icon |
| `NControl : NCWnd` (0x1F4, 75 slots) | `include/nwindow/NControl.h` | every control; has an `ID` and tells its parent with `NWM_COMMAND` (wParam = `ID`) |
| `NCButton`, `NCPushButton`, `NCExButton`, `NCRadioButton` | `include/nwindow/NCButton.h` ... | buttons with normal/pushed textures (`SetNormalTex`, `SetPushedTex`, `SetText`, `SetTextID`) |
| `NCEditBox`, `NCListBox`, `NCScrollWnd`, `NCComboBox`, `NCTabControl`, `NCSliderBar`, `NCProgressCtrl`, `NCListCtrl`, `NCMultiEdit`, `NCHtmlViewer`... | `include/nwindow/` | the other controls |
| `NC*Wnd` | `include/nwindow/` | the game windows (inventory, chat, shortcut bar, status...) |
| `NCVirtualWndMain` | `include/nwindow/NCVirtualWndMain.h` | owner of every window: registration, z-order, hit tests, focus, message dispatch, cursors |
| `NConsoleWnd : UL2ConsoleWnd` (0x3CA4, 448 slots) | `include/nwindow/NConsoleWnd.h` | the engine's console (network and UI events); `GConsole` |
| `NCConsole` (0x2A0) | `include/nwindow/NCConsole.h` | the in-game **desktop** (`GConsole->GetDesktop()`): parent of every in-game window, with a pointer to each (`ChatWnd` +0xF4, `InventoryWnd` +0x120, `SystemMenuWnd` +0x148, `ShortCutWnd` +0x15C...) |

The sizes are frozen: they are registered with Core (`UClass` size, `analysis/classes.json`) and the stock code and the
engine index into them. `NW_ASSERT_SIZE` / `NW_ASSERT_OFFSET` in every header keep them honest.

### Creating a window or a control

The pattern of the original code (`NCSystemMenuWnd::OnCreate` in `src/nwindow/NCSystemMenuWnd.cpp`):

```cpp
NCButton* Button = new NCButton(TEXT("L2UI_CH3.SystemMenuWnd.systemicon1"),
                                TEXT("L2UI_CH3.SystemMenuWnd.systemicon1_down"), 0);
Button->ID = NID_SOMETHING;          // what arrives in the parent's NWM_COMMAND
Button->SetPosition(13, 29, 36, 36); // relative to the parent (C4: a virtual with 3 more arguments, the screen pin)
CreateChild(Button, NWS_VISIBLE);    // registers it, makes the position absolute, sends WM_CREATE -> OnCreate
```

- `new` of a package class is UE2's `operator new` (the object is a `UObject`, allocated with the size of its
  registered `UClass`).
- `CreateChild(child, style)` (`NCWnd`, inline) = `AddChildWindow` + `AttachEntry` + screen position + `WM_CREATE`. The
  desktop registers its windows by hand (`NCConsole::OnCreate`: `AddChildWindow(W, style)`, `HideWindow`,
  `AttachEntry`).
- Styles (`NWS_*` in `NCWnd.h`): `NWS_VISIBLE`, `NWS_TOPMOST` (separate list, on top), `NWS_TITLEBAR` (`NCWnd::OnCreate`
  makes an `NCFrameCtrl` title bar), `NWS_MINIMIZABLE`, `NWS_NOCLOSE`, `NWS_CLIPTOPARENT`...
- Destroying: in the owner's `OnDestroy`, `RemoveChildWindow(Child->Entry); delete Child;` (see
  `NCSystemMenuWnd::OnDestroy`). `NCWnd::OnDestroy` only destroys the title bar and the icon.

### Messages, painting, tool tips

- Everything arrives through `WindowProc(Msg, wParam, lParam)` (slot 25) with Win32 message numbers; `NCWnd` dispatches
  to the virtuals (C4 slots): `OnTimer` (36), `OnCreate` (37), `OnDestroy` (38), `ShowWindow` (48), `HideWindow` (49),
  `MoveWindow` (51), `OnPaint(NCGDevice*)` (62), `OnDrawToolTip` (63), `OnChar` (64), `OnKeyDown` (65),
  `OnMouseMove` (67), `OnLButtonDown/Up` (68/69), `OnRButtonDown/Up` (70/71), `OnMove` (72), `OnMouseWheel` (73).
- A control tells its parent with `NWM_COMMAND` (`0x501`), `wParam` = the control's `ID`; the parent handles it in its
  `WindowProc` (example: `NCSystemMenuWnd::WindowProc`).
- Painting (`OnPaint`) uses the `NCGDevice` (`include/nwindow/NCGDevice.h`): `SetClip(ScreenX, ScreenY, W, H)`,
  `BeginPaint(ScreenX, ScreenY)` (from then on coordinates are relative), `DrawTexture(x, y, w, h, u, v, ul, vl, Tex)`,
  `DrawText(x, y, colour, text, ..., L2FONT_Normal, ...)`, `EndPaint`, `ReleaseClip`. Textures are loaded once in
  `OnCreate` with `FindTexture` (the client's `L2UI*` packages; C4 has the `L2UI_CH3.*` set). See
  `NCSystemMenuWnd::OnPaint`.
- Tool tip: `OnDrawToolTip(NCGDevice*)` of the window under the mouse, called after painting.

### Console and desktop

`NConsoleWnd` receives the server and game events and owns the `NCConsole` desktop. `NCConsole::OnCreate`
(`src/nwindow/NCConsole.cpp`) creates every game window (positions from the screen size), `NCConsole::OnDestroy`
destroys them, `NCConsole::Reset` runs when the player leaves the game. `NConsoleWnd::Tick` runs every frame;
`NConsoleWnd::HookShortCutKey` sees the keys first (F1..F12, Alt+letter, the "Enter to chat" mode's 1..=).

## 2. Where new code plugs in

This tree has no feature layer of its own. The `#if NW_FEATURES` blocks in `src/nwindow` are call sites of the C1
project's features, inherited through the forks: nothing here defines `NW_FEATURES` and `src/features/` does not exist,
so they compile to nothing. For C4, use the pattern that the separate experiment below used:

| Piece | Where | What it does |
|---|---|---|
| a switch of your own, e.g. `NW_CUSTOM=1` | `CMakeLists.txt`, `target_compile_definitions(nwd_standalone_objs ...)` | defined only for the standalone objects; the hook target never sees it, so `verify_hooks`/`checks.py` keep comparing faithful code |
| your sources | `src/custom/*.cpp` (+ a `file(GLOB NWD_CUSTOM_SOURCES ...)` added to `nwd_standalone_objs`), headers in `include/nwindow/` | the feature code, in the source's style |
| call sites | inside stock functions, `#if NW_CUSTOM ... #endif` with `// custom: ...` | the only change to decompiled code; the stock branch stays as it is |
| entry points | `NCConsole::OnCreate` (after the stock windows), `NCConsole::OnDestroy` (before them), `NCConsole::Reset`, `NConsoleWnd::Tick`, `NConsoleWnd::HookShortCutKey` | create / destroy / reset / per-frame work / keys |
| settings | a file of your own next to `l2.exe` (`GetModuleFileName` + `GetPrivateProfileInt/String`) | never the game's own ini files for new keys |
| a unit test | an `add_executable` of the engine-free logic (no game source, same flags) | what can be checked outside the game |

**Classes.** A new window class that adds **no field** can live under a stock `UClass`: `static UClass* StaticClass() {
return NCWnd::StaticClass(); }` (or the stock class it extends), an `operator new` that calls
`StaticAllocateObject(StaticClass(), ...)`, and `NW_ASSERT_SIZE` equal to the base's size. The class table, the exports,
`verify_standalone` and `smoke_standalone` then still see exactly the stock classes. State goes into statics of your
`.cpp`. A class with fields of its own needs its own `UClass` and export, which the standalone judges would reject
until they are taught that list (the C1 project did it with `NW_DECLARE_L2_CLASS` and `tests/standalone/l2_extras.csv`).

## 3. Recipe: new shortcut bars (as in Interlude/Essence)

This describes the approach implemented for C4 in a **separate experiment** (a development repository of its own,
branch `feature/barras-potions`, under `#if NW_CUSTOM`; it is **not** part of this source). It was built and passed the
standalone checks outside the game (`smoke_standalone` 9/9, `verify_standalone` OK, a unit test of its own); the
in-game test follows its own script.

**The facts it relies on.** The C4 bar is one `NCShortCutWnd` (`include/nwindow/NCShortCutWnd.h`, desktop `+0x15C`),
made in `NCConsole::OnCreate` with `new NCShortCutWnd(12, 10, 32)`: 12 slots × 10 pages, the 120 slots in `Items`
(`+0x200`) and the key states in `Keys` (`+0x1FC`), the page shown in `Page` (`+0x270`). A shortcut's slot is
`Page * 12 + position`, the flat number the server stores and sends back. The server accepts pages 0..11, so bars that
show pages 0..9 need no server change.

**The idea.** An extra bar **owns no slots**: `NCExtraShortCutWnd : NCShortCutWnd` adds no field, lives under the main
bar's `UClass` and shows another page of the same 120-slot table.

1. **Create** the set in `NCConsole::OnCreate`, after the stock windows (`NCustomFeatures::CreateWindows(this)` under
   `#if NW_CUSTOM`). In the extra bar's `OnCreate`, after the stock `OnCreate` made tables of its own, free them and
   point to the main bar's (`Items = Main->Items; Keys = Main->Keys;`); `Page` is the bar's own (default pages 1..5).
2. **Destroy** in `NCConsole::OnDestroy`, **before** the stock windows: the extra bar's `OnDestroy` sets `Items` and
   `Keys` to `NULL` first (they belong to the main bar; otherwise they are freed twice), then the stock `OnDestroy`.
3. **One clock.** Only the main bar ticks the cool times (`NConsoleWnd::Tick` calls its `Tick`); an extra bar only
   paints them. Six ticking bars would run the timers six times as fast.
4. **Follow the main bar** every frame (`NConsoleWnd::Tick` → the set's `Tick`): same orientation (`bHorizontal`,
   `UpdateLayout()` when it changes), stacked to the left of a vertical bar or above a horizontal one (the other side
   when the screen has no room), hidden when it hides. Dragging an extra bar moves the main bar (`MoveWindow` forwards)
   and the set follows.
5. **Drag and drop needs no change**: every drop site (inventory, paperdoll, actions, skills, macros, recipes, the bar
   itself) asks `IsA(NCShortCutWnd::StaticClass())` and then `SlotAtPoint(X, Y)`, which returns the global slot
   `Page * 12 + i`; the registration goes to the server with that slot. Clicking a slot uses the same global index.
6. **Data changes stay on the main bar**: `SetShortCut` / `DeleteShortCut` / `ClearShortCuts` / `UpdateItem` run only
   there (the console's `ReceiveShortCutList`, `AddShortCutItem`... already call only the main bar); the extra bars
   just repaint.
7. **Buttons.** An Expand button on the main bar (an `NCPushButton` child with a new `ID` after the stock ones,
   `0xFF00000A`; the bar's own go up to `NID_SHORTCUT_PAD` `0xFF000009`), handled in `NCShortCutWnd::WindowProc`'s
   `NWM_COMMAND` branch: one more bar per click, all hidden after the fifth. Each extra bar keeps the page arrows
   (`OnNextClicked`/`OnPrevClicked`, pages 0..9 in a circle); its turn and game-pad buttons are created (the stock
   layout expects them) but kept hidden.
8. **Keys.** In `NConsoleWnd::HookShortCutKey`, before the stock code: with the chat in "Enter to chat" mode, 1..0,-,=
   go to the first extra bar (they otherwise repeat F1..F12) and Q..P,[,] to the second, while those bars are shown.
   The labels are painted over the F-key textures in the extra bar's `OnPaint`.
9. **Saved** in the client's own ini (`system\NWindowCustom.ini`): how many bars are shown and each bar's page.

The same experiment added an **auto-potion** window (`NCAutoPotionWnd`, under `NCWnd`'s `UClass`, no field): one slot
per potion kind filled from the inventory, a right click switches it on, a 200 ms timer compares HP/MP/CP with a
per-character threshold and calls `NConsoleWnd::RequestUseItem(ObjectID)` after each kind's minimum interval; dropping
an inventory item on it is one `#if NW_CUSTOM` branch in `NCInvenItemWnd`'s release code.

## 4. Recipe: a new button in an existing window

Example: one more button in the system menu (`NCSystemMenuWnd`).

1. Your files (`src/custom/MyButton.cpp`, `include/nwindow/MyButton.h`): `MyButton_Create(NCWnd* Owner)`,
   `MyButton_OnCommand(NCWnd* Owner, WPARAM ID)`, `MyButton_Destroy(NCWnd* Owner)`; the button pointer is a static of
   the `.cpp` (never a new field of `NCSystemMenuWnd`).
2. Create it at the end of `NCSystemMenuWnd::OnCreate`, in `#if NW_CUSTOM`: `new NCButton(normal, pushed, 0)`, an `ID`
   no control of that window uses (see its `NID_SYSMENU_*` enum), `SetPosition` inside the window,
   `Owner->CreateChild(Button, NWS_VISIBLE)`.
3. Handle the click in the `NWM_COMMAND` branch of `NCSystemMenuWnd::WindowProc`:
   `if (MyButton_OnCommand(this, wParam)) return 1;` (in `#if NW_CUSTOM`).
4. Destroy it in `NCSystemMenuWnd::OnDestroy`, before `NCWnd::OnDestroy()`: `RemoveChildWindow(Button->Entry);
   delete Button;`.
5. If the window has to grow, change its `SetPosition` in `NCConsole::OnCreate` and the background in its `OnPaint`,
   always in `#if NW_CUSTOM`, leaving the stock branch intact.

## 5. Recipe: a new window

1. A class deriving from `NCWnd` (or a stock window) that adds **no field** (section 2): the state in statics of its
   `.cpp`, `NW_ASSERT_SIZE(NCMyWnd, 0xE8)` to prove it.
2. Override the virtuals you need: `OnCreate` (call `NCWnd::OnCreate()` first, load textures with `FindTexture`,
   create the title bar and the controls), `OnDestroy` (destroy the children, then `NCWnd::OnDestroy()`), `WindowProc`
   (`NWM_COMMAND`), `OnPaint`, `OnDrawToolTip`, `OnTimer`. Keep the slot comments of `NCWnd.h`: an override with a
   different signature becomes a new slot and breaks the class.
3. Create it from `NCConsole::OnCreate` (your entry point, after the stock windows) with the desktop's registration
   (`SetPosition`, `AddChildWindow(W, style)`, `AttachEntry`, `HideWindow`), and destroy it from `NCConsole::OnDestroy`
   before the stock windows. Show it with `ShowWindow()` and `SetHeadWindow(Entry)` (`BringChildToTop`).
4. Models: `NCSystemMenuWnd` (title bar `NCFrameCtrl(1, 0, 0)`, buttons, background, text) and the shortcut bar
   (`NCShortCutWnd`: textures, buttons, layouts by orientation, tool tip).

## 6. Recipe: a new chat colour

The server sends no colour: it sends the chat **type** (`Say2`), and the client picks the colour in
`NConsoleWnd::Say2` (`src/nwindow/NConsoleWndEvents.cpp`, stock `0x1007d200`). Types C4 does not know fall into the
last `else`, drawn `0xFFDCDCDC` on the general tab (type 0). NPC lines go through `NConsoleWnd::NpcSay` right below it.

1. Add your type before the last `else`, in `#if NW_CUSTOM`:
   `else if (Type == 18) GetDesktop()->Say(ObjectID, Name, Text, 0xFF408CFF, Type);` with the `#else` branch keeping
   the stock chain unchanged (or put the whole chain in both branches).
2. The last argument is the type the chat window files the line under (tabs and filters); a type the chat window does
   not know may not show in any tab but the general one: pass 0 then, as the stock `else` does.
3. Do not reuse types with side effects (TELL adds the speaker to the reply list; ANNOUNCEMENT replaces the name).
4. Changing the colour of a type C4 **already** colours means changing a faithful branch: do it under `#if NW_CUSTOM`
   inside `Say2`, keeping the original colour in the `#else` branch.

## 7. Recipe: a new line in the item tool tip

The item tool tip of the item grids is drawn by the routines of `include/nwindow/NItemToolTip.h` (inventory pages,
paperdoll, trade, warehouse, private stores, pet inventory): `SetTexts`/`SetStoreTexts` fill `ToolTipName` and
`ToolTipText` (the text that **sizes** the box) and `DrawLines` draws the lines, 13 pixels apart (`DrawTypeLines`, then
`DrawTailLines`, which ends with the description).

1. Add the line in both places, in `#if NW_CUSTOM`: in the sizing text (so the box grows) and in the drawing (for
   example in `DrawTailLines`, before the description, with `DrawLabel`/`DrawValue` and `Y += 13`).
2. The routines are templates instantiated by each grid with different parameters: check every copy you change.
3. Other windows' tool tips are their own `OnDrawToolTip` (slot 63): give the box the extra height where it is
   computed, then draw the line, both under the switch.

## 8. The build and test loop

```bat
cmake -S . -B build -A Win32 -DNWD_GAME_SOURCES=ON
cmake --build build --config Release --target NWindowStandalone
python tools\verify_standalone.py --dll build\standalone\Release\NWindow.dll
python tools\smoke_standalone.py --dll build\standalone\Release\NWindow.dll
python tools\cycle.py --game-source --no-install
```

1. Build `NWindowStandalone` until there is no error and no new warning.
2. `verify_standalone` and `smoke_standalone` must still pass: with classes that add no field the class table and the
   output compared with the original do not change. If a new window changes the smoke output, explain and adjust the
   comparison with a reason; never hide a difference.
3. Run the hook cycle: it proves the faithful code did not change (the switch is not defined there).
4. Test in the game: close the game, keep the `NWindow.dll` of a **copy** of the `system` folder aside and copy
   `build\standalone\Release\NWindow.dll` over it. The tools do not draw the interface: position, art and dragging only
   show in the game. On a crash, `NWindow_crash.log` next to `l2.exe` + `python tools\port\map_addr.py --log
   NWindow_crash.log`.

## 9. Safety rules

- Never add fields or new virtual slots to an original class: its size is registered with the engine and the original
  code indexes into it and its vtable. Overriding a slot the class already has is fine.
- In decompiled code, only the call site, under your switch with a `// custom: ...` comment; the stock branch stays as
  it is. The hook build never gets the switch.
- Check every texture name against the C4 client's packages before using it (an Interlude/Essence texture is often
  missing in C4).
- Same protocol as the original client: the server does not change; respect its packet limits (shortcut registrations
  sent too fast may be dropped).
- Settings in a file of your own, never new keys in the game's ini files.
- Test on a copy of the client, never in the folder you play from; no game binary goes into the repository.
- Small commits, one subject each, with the judges green.
