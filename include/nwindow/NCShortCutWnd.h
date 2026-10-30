#pragma once
// NCShortCutWnd - the shortcut bar (UClass "CShortCutWnd", 0x294 bytes in C4 (C3: 0x270); desktop +0x15C in C4 (C3: +0x138)): Pages pages
// of Cols slots (12 x 10 in the C3 client: F1 .. F12), each an item, a skill or an action the server registered
// (NConsoleWnd hands them over: SetShortCut). F1 .. F12 (or 1 .. 0, -, = while the chat line needs
// Enter) or a click uses the slot of the current page; the arrows turn the pages, the corner
// button turns the bar between vertical (46 x 504) and horizontal (504 x 46), the game pad button
// shows the pad layout (160 x 82: four slots at a time, L / R hold). A slot dragged off the bar
// is deleted on the server. Each slot's key waits before it works again: until released,
// until its skill's cool time is over, or a fixed time (shots, attack, pick up).

#include "nwindow/NControl.h"
#include "nwindow/NCCoolTimeIcon.h"
#include "nwindow/NCItemWnd.h"        // NItemEntry: the slots are built on it
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

struct NItemInfo;
class NCPushButton;

// NWM_COMMAND ids of the bar's controls
enum
{
    NID_SHORTCUT_FRAME = (INT)0xFF000003,
    NID_SHORTCUT_NEXT = (INT)0xFF000004,
    NID_SHORTCUT_PREV = (INT)0xFF000005,
    NID_SHORTCUT_ROTATE = (INT)0xFF000008,
    NID_SHORTCUT_PAD = (INT)0xFF000009,      // new in C2 (the id the WindowProc tests; the button's own id is set in OnCreate)
};

// NShortCutItem::Type (names ours)
enum { SHORTCUT_Item = 1, SHORTCUT_Skill = 2, SHORTCUT_Action = 3, SHORTCUT_Macro = 4 /* new in C2 */, SHORTCUT_Recipe = 5 /* new in C4: the recipe icon is drawn over it */ };

// A slot of the bar (name ours; 0x1CC0 bytes in C4, 0x1AB4 in C3, 0x1A7C in C2; an array of Cols * Pages): an item entry (the item,
// skill, action or macro the slot holds) with the cool time icon and the slot's own fields. C4 offsets, read in the stock
// UpdateItem 0x10105ED0 (`cmp [slot + 0x1B18], 1`, `[slot + 0x1B1C]`, `[slot + 0x1CA0]`), the constructors 0x10108D90 / 0x10108DF0,
// CopyFrom 0x101056E0 and Clear 0x101057F0: the slot's kind is the entry's +0x1B18 (the field NItemEntry names Unknown1910 after
// its C3 offset; the entry's constructor copies it from NItemInfo +0x1918; Kind() below), ObjectID +0x1B1C, ID +0x1B20 (the item,
// skill, action or macro id), UnknownC18 +0x1B24 the skill's level, Count +0x1B28, Enchant +0x1B44, bStackable +0x1B4C,
// Unknown1998 +0x1BA4 the "asks first" flag, the tool tip +0x1BC0, then the slot's own fields below. (The C3 build had them at
// +0x1910 / +0x1914 / +0x1918 / +0x191C / +0x1920 / +0x193C / +0x1944 / +0x199C / +0x19B8 and the slot's fields from +0x1A34: every
// number in the field comments below that is not an assert is C3's name, the position is the assert's.)
struct NShortCutItem : public NItemEntry
{
    NCCoolTimeIcon CoolTime;       // +0x1C40 (C3 +0x1A34)
    INT Owner;                     // +0x1C98 (C3 +0x1A8C) items: 1 the player's inventory, 2 the pet's (UseShortCut); copied by CopyFrom, left by Clear
    INT Unknown1A90;               // +0x1C9C (C3 +0x1A90) NItemInfo +0x1970 (C3 +0x1768; the constructor stores it, 0x10108DF0), copied by CopyFrom, cleared by Clear
    UBOOL bUsed;                   // +0x1CA0 (C3 +0x1A94) the slot holds something (cleared by the constructors and by Clear)
    UBOOL bAutoShot;               // +0x1CA4 (C3 +0x1A98; name ours) new in C3: the shot is on auto use (SetShortCut sets it for a shot the
                                   //   auto list holds; the painters draw the toggle effect over it; OnRButtonDown flips it)
    BYTE _Unknown1A9C[0x1AAC - 0x1A9C];   // +0x1CA8 (C3 +0x1A9C) (not touched by the code seen)
    INT Unknown1AAC;               // +0x1CB8 (C3 +0x1AAC) (0 in the constructor, CopyFrom and Clear)
    INT Unknown1AB0;               // +0x1CBC (C3 +0x1AB0) (0 in the constructor, CopyFrom and Clear)

    NShortCutItem();                                                 // 0x10108D90 (the entry's defaults, the cool time icon, bUsed = 0)
    NShortCutItem(const NItemInfo& Info);                            // 0x10108DF0
    // ~NShortCutItem                                                // 0x10108E70 (the cool time's empty destructor 0x10043B50,
                                                                     //   then the entry's: its tool tip 0x10002E40 at +0x1BC0)
    __declspec(noinline) void CopyFrom(NShortCutItem* Src);          // 0x101056E0 the entry's Copy, then the slot's fields; the
                                                                     //   cool time stops
    __declspec(noinline) INT Clear();                                // 0x101057F0 the entry's Reset, the fields; the cool time stops
    // 0x10108E70 (name ours): what the stock's destructor does, see the .cpp; the compiler's own destructor is not to be used
    __declspec(noinline) void DestroyItem();

    // the slot's kind, SHORTCUT_* (the entry's Unknown1910, which is at +0x1B18 in this client: C4 CHECK, wave C debt2: the stock
    // UpdateItem 0x10105ED0 compares [slot + 0x1B18] with SHORTCUT_Item and the ObjectID at +0x1B1C with its argument)
    INT& Kind() { return Unknown1910; }
};
NW_ASSERT_OFFSET(NShortCutItem, CoolTime, 0x1C40);
NW_ASSERT_OFFSET(NShortCutItem, Owner, 0x1C98);
NW_ASSERT_OFFSET(NShortCutItem, bUsed, 0x1CA0);
NW_ASSERT_OFFSET(NShortCutItem, bAutoShot, 0x1CA4);
NW_ASSERT_OFFSET(NShortCutItem, Unknown1AAC, 0x1CB8);
NW_ASSERT_SIZE(NShortCutItem, 0x1CC0);

// When the key of a slot works again (name ours; 0x14 bytes, an array of Cols * Pages).
struct NShortCutKey
{
    DWORD Elapsed;                 // +0x00 ms since the last use
    DWORD Interval;                // +0x04 ms it waits (shots, attack: 200, pick up: 500)
    UBOOL bReady;                  // +0x08 the key uses the slot
    UBOOL bOnKeyUp;                // +0x0C ready again when the key is released
    UBOOL bOnCoolTime;             // +0x10 ready again when the cool time is over

    NShortCutKey() : Elapsed(0), Interval(0), bReady(1), bOnKeyUp(0), bOnCoolTime(0) {}

    // 0x100f4000 (name ours): the key that waits for its release is ready again (the mouse's release uses it like the
    // key's)
    __declspec(noinline) void OnKeyUp();
};
NW_ASSERT_SIZE(NShortCutKey, 0x14);

// Layout from the constructor (0x10108ED0), OnCreate (0x10109120), UpdateLayout (0x101058E0) and the painters (the C3 build had
// them at 0x100f6430, 0x100f6660 and 0x100f3150); the field offsets of the window below are the C4 ones (asserts at the end).
class NCShortCutWnd : public NControl
{
    NW_DECLARE_CLASS(NCShortCutWnd, NControl, 0, 0x10307210)

    INT TimerID;                           // +0x1F4 (name ours) SetTimer(0x1F9, 200 ms) in OnCreate   // C2: offset derived, no evidence
    INT bPadMode;                          // +0x1F8 (name ours) 0 the bar, 1 the game pad layout (OnPadClicked; the options
                                           //   window compares it with 1)
    NShortCutKey* Keys;                    // +0x1FC new NShortCutKey[Cols * Pages]
    NShortCutItem* Items;                  // +0x200 new NShortCutItem[Cols * Pages]
    UTexture* BackTex;                     // +0x204 "L2UI_CH3.ShortcutWnd.shortcut_backv" / "_back" (UpdateLayout), the pad's "joypad_back"
    UTexture* Unknown1E8;                  // +0x208 (0: the constructor clears it, nothing sets it)
    UTexture* IconBackTex;                 // +0x20C "L2UI_CH3.Etc.menu_outline" the frame of a slot
    UTexture* IconClickTex;                // +0x210 "L2UI_CH3.Etc.menu_outline_down" over the pressed one
    UTexture* ToggleTex;                   // +0x214 "L2_SkillTime.ToggleEffect.ToggleEffect001"
    UTexture* KeyTex[12];                  // +0x218 "L2UI_CH3.ShortcutWnd.shortcut_f01" .. "_f12": the key of each slot
    UTexture* RecipeIconTex;               // +0x248 "L2UI_ch3.ShortcutWnd.shortcut_recipeicon" (new in C4; loaded in OnCreate)
    NCPushButton* NextButton;              // +0x24C right / down arrow
    NCPushButton* PrevButton;              // +0x250 left / up arrow
    NCPushButton* RotateButton;            // +0x254 vertical / horizontal
    NCPushButton* PadButton;               // +0x258 "L2UI_CH3.ShortcutWnd.shortcut_joypad", hidden until the game pad is in use
    UBOOL bPressed;                        // +0x25C mouse button down on PressedSlot
    UBOOL bDragging;                       // +0x260
    UBOOL bHorizontal;                     // +0x264
    INT Cols;                              // +0x268 slots per page (12)
    INT Pages;                             // +0x26C (10)
    INT Page;                              // +0x270
    INT IconSize;                          // +0x274 (32)
    INT PressedSlot;                       // +0x278 -1 = none
    INT HoverSlot;                         // +0x27C slot under the mouse, -1 = none
    INT PadHold;                           // +0x280 (name ours) 0, 1 = left hold, 2 = right hold (SetPadHoldL / R)
    UTexture* PadHoldLTex;                 // +0x284 "L2UI_ch3.Joypad.joypad_L_HOLD"
    UTexture* PadHoldRTex;                 // +0x288 "L2UI_ch3.Joypad.joypad_R_HOLD"
    UTexture* PadLTex;                     // +0x28C "L2UI_ch3.Joypad.joypad_L"
    UTexture* PadRTex;                     // +0x290 "L2UI_ch3.Joypad.joypad_R"

    // the slot and its cool time (the stock Tick works on the cool time at +0x1A34)
    NShortCutItem* ItemAt(INT Index) { return &Items[Index]; }
    NCCoolTimeIcon* ItemCoolTime(INT Index) { return &Items[Index].CoolTime; }

    NCShortCutWnd(INT InCols = 12, INT InPages = 10, INT InIconSize = 32);          // 0x10108ed0 (ret 0xC)
    // ~NCShortCutWnd                                                // 0x101090d0

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10105830
    virtual INT OnCreate();                                          // 37 0x10109120
    virtual INT OnDestroy();                                         // 38 0x10109660
    virtual void ShowWindow();                                       // 48 0x101066f0
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x10107b20
    virtual INT OnDrawToolTip(NCGDevice* Device);                    // 63 0x101097e0
    virtual INT OnMouseMove(WPARAM wParam, LPARAM lParam);           // 67 0x10107c60
    virtual INT OnLButtonDown(WPARAM wParam, LPARAM lParam);         // 68 0x10107c00
    virtual INT OnLButtonUp(WPARAM wParam, LPARAM lParam);           // 69 0x10107df0
    virtual INT OnRButtonDown(WPARAM wParam, LPARAM lParam);         // 70 0x10106730 (the shot on / off the auto use)
    // names ours
    virtual void UpdateLayout();                                     // 75 0x101058e0 after a turn: the bar's size, the textures and
                                                                     //   the buttons of the vertical / horizontal layout
    virtual void SetPadHoldL(bool bHold);                            // 76 0x10106510 PadHold = bHold
    virtual void SetPadHoldR(bool bHold);                            // 77 0x10106530 PadHold = bHold ? 2 : 0
    virtual INT OnNextClicked();                                     // 78 0x10108d10
    virtual INT OnPrevClicked();                                     // 79 0x10108d30
    virtual INT OnRotateClicked();                                   // 80 0x10106300
    virtual INT OnPadClicked();                                      // 81 0x10106550 the game pad layout on / off

    // names ours; the console calls most of them (UpdateShortCut, ..., Tick, HookShortCutKey, ReloadGameTexts)
    INT UpdateItem(INT ObjectID);                                    // 0x10105ed0 inventory
    INT UpdatePetItem(INT ObjectID);                                 // 0x10105ff0 pet inventory
    __declspec(noinline) INT SlotAt(INT X, INT Y);                                        // 0x101060f0 -1 = none
    void ReloadNames();                                              // 0x10106330 new language: the name and the tool tip of every slot again
    INT PageSlot(INT Index);                                         // 0x101062d0
    INT DeleteShortCut(INT Slot);                                    // 0x10108500
    void SetPage(INT NewPage);                                       // 0x10108560
    void Tick(FLOAT DeltaTime);                                      // 0x101085d0
    INT OnShortCutKeyUp(UINT Key, LPARAM lParam);                    // 0x101086b0
    INT OnShortCutKeyDown(UINT Key, LPARAM lParam);                  // 0x10108860 (the console's HookShortCutKey calls it)
    void NextPage();                                                 // 0x10108c30
    void PrevPage();                                                 // 0x10108ca0
    void ClearShortCuts();                                           // 0x10108d50
    INT SetShortCut(NItemInfo* Info, INT Owner);                     // 0x10109a50 (the console's AddShortCutItem calls it)
    __declspec(noinline) INT SlotAtPoint(INT X, INT Y);                                   // 0x101062b0 the slot under a screen point by the layout (bPadMode): SlotAt, PadSlotAt or -1
    __declspec(noinline) INT PadSlotAt(INT X, INT Y);                                     // 0x101061f0 the game pad layout's four slots (PadHold * 4 + 0 .. 3), -1 none
    INT UpdateMacro(INT MacroID, INT Mode);                          // 0x10105c60 the console's UpdateMacroShortCut: Mode 2, the slots of the macro take its icon, name and tool tip; 0, they are deleted
    void PaintBar(NCGDevice* Device);                                // 0x10106860 the vertical / horizontal bar (OnPaint's)
    void PaintPad(NCGDevice* Device);                                // 0x10107400 the game pad layout (OnPaint's)
    UBOOL IsAutoShotSlot(INT ItemID);                                // 0x10105c20 (name ours) a used slot of this item that is on auto use
    virtual INT OnTimer(INT TimerID) ;  // 36 0x100f76f0 (the stock vtable of this class overrides it; defined in NNewVirtuals.gen.cpp)
};
NW_ASSERT_OFFSET(NCShortCutWnd, TimerID, 0x1F4);
NW_ASSERT_OFFSET(NCShortCutWnd, Keys, 0x1FC);
NW_ASSERT_OFFSET(NCShortCutWnd, BackTex, 0x204);
NW_ASSERT_OFFSET(NCShortCutWnd, IconBackTex, 0x20C);
NW_ASSERT_OFFSET(NCShortCutWnd, ToggleTex, 0x214);
NW_ASSERT_OFFSET(NCShortCutWnd, KeyTex, 0x218);
NW_ASSERT_OFFSET(NCShortCutWnd, NextButton, 0x24C);
NW_ASSERT_OFFSET(NCShortCutWnd, PadButton, 0x258);
NW_ASSERT_OFFSET(NCShortCutWnd, Cols, 0x268);
NW_ASSERT_OFFSET(NCShortCutWnd, HoverSlot, 0x27C);
NW_ASSERT_OFFSET(NCShortCutWnd, PadHoldRTex, 0x288);
NW_ASSERT_OFFSET(NCShortCutWnd, PadRTex, 0x290);
NW_ASSERT_SIZE(NCShortCutWnd, 0x294);

#pragma pack(pop)
