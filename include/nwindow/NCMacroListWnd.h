#pragma once
// NCMacroListWnd - the macro window (C2 only; UClass "CMacroListWnd", 0x61C4): the grid of the macros (NCMacroArrayWnd),
// the "(n/24)" counter, a button that opens the editor for a new macro and one that opens the macro help. It also keeps
// the macros themselves: NMacroEntry objects in a TArray (what the grid shows, the console's tick runs and the server's
// list fills, NConsoleWnd::vfn132 0x10066d70) and 24 slots of the data as the server sent it (NMacroSlot, what the
// editor shows and the save request is built from). C3: the two icons (edit, delete) draw a second texture while the
// mouse button is down on them and have a tool tip each.
//
// The desktop (NCConsole) keeps the three macro windows at +0x12C (this), +0x130 (NCMacroEditWnd) and +0x134
// (NCMacroInfoWnd) (NewWndF8, NewWndFC, NewWnd100; names ours; NCConsole.h declares them as NCWnd*).
//
// Layout from the C3 constructor 0x1009FC60, OnCreate 0x100A0C30, OnDestroy 0x1009FDC0, OnPaint 0x1009C550, OnMouseMove
// 0x1009B190 and the users of the slots (NCMacroInfoWnd, NCMacroEditWnd).

#include "nwindow/NCWnd.h"
#include "nwindow/NCConsole.h"
#include "nwindow/NMacroEntry.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCButton;
class NCMacroArrayWnd;

// the desktop's toggle of the macro editor window (NCConsole +0x130; 0x10053A70, anonymous in the db; thiscall with the
// desktop as `this`): hidden with the sound 6 when it shows, else shown, focused, sound 5. NCConsole.h is not this lane's,
// so the method lives in a helper class that derives from it (never constructed: a method of NCConsole for the hook)
struct NMacroDesktop : public NCConsole
{
    void ToggleMacroEditWindow();                                    // 0x10053A70
};
FORCEINLINE void ToggleMacroEditWindow(NCConsole* Desktop)
{
    ((NMacroDesktop*)Desktop)->ToggleMacroEditWindow();
}

// NWM_COMMAND ids of the macro window (names ours)
enum
{
    NID_MACROLIST_NEW = 0x100F,        // the button at the bottom left (text 0x28E): a new macro
    NID_MACROLIST_HELP = 0x100C,       // the button next to it (text 0x91): the macro help page
};

class NCMacroListWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCMacroListWnd, NCWnd, 0, 0x102E2B08)

    UTexture* BackTex;               // +0xE8 "l2ui_ch3.macrownd.macro2_back"
    UTexture* EditTex;               // +0xEC "L2UI_CH3.macrownd.macro_edit": the icon that opens the editor (drawn at EditX, EditY)
    UTexture* EditDragTex;           // +0xF0 "L2UI_CH3.macrownd.macro_edit_Drag": the same while a dragged macro is over it (bEditDragOver)
    UTexture* DeleteTex;             // +0xF4 "L2UI_CH3.InventoryWnd.Inventory_trash": the icon that deletes the macro (drawn at DeleteX, DeleteY)
    UTexture* DeleteDragTex;         // +0xF8 "L2UI_CH3.InventoryWnd.Inventory_trash_Drag": the same while bDeleteDragOver
    INT DeleteX;                     // +0xFC (0xD0) the delete icon's rectangle in the window: x, y, w, h
    INT DeleteY;                     // +0x100 (0x15B)
    FLOAT DeleteW;                   // +0x104 (34.0; C3: floats)
    FLOAT DeleteH;                   // +0x108 (34.0)
    INT EditX;                       // +0x10C (0xA9) the edit icon's rectangle
    INT EditY;                       // +0x110 (0x15B)
    FLOAT EditW;                     // +0x114 (34.0)
    FLOAT EditH;                     // +0x118 (34.0)
    UBOOL bEditDragOver;              // +0x11C (name ours) the edit icon is drawn with its Drag texture (set by NCMacroArrayWnd)
    UBOOL bDeleteDragOver;            // +0x120 (name ours) the same for the delete icon
    UBOOL bOverEdit;                 // +0x124 (name ours) the mouse is over the edit icon (OnMouseMove): its tool tip shows
    UBOOL bOverDelete;               // +0x128 (name ours) the same for the delete icon
    TArray<NMacroEntry*> Macros;     // +0x12C the macros the server sent (at most NMACRO_MaxMacros)
    NCMacroArrayWnd* Array;          // +0x138 the grid
    NMacroSlot Slots[NMACRO_MaxMacros]; // +0x13C the data as sent (id + NMacroInfo); the first dword of NConsoleWnd::vfn132's
                                     //   by-value argument is the slot's id
    INT Unknown60BC;                 // +0x60DC (not touched by the code seen)   // C2: offset derived, no evidence
    INT Selected;                    // +0x60E0 the slot being edited; Macros.Num() for a new macro
    NToolTipInfo EditToolTip;        // +0x60E4 the edit icon's tool tip (the macro's name text, style 0x400)
    NToolTipInfo DeleteToolTip;      // +0x6164 the delete icon's
    NCButton* NewButton;             // +0x61E4
    NCButton* HelpButton;            // +0x61E8

    NCMacroListWnd();                                                // 0x100ac320
    // ~NCMacroListWnd                                               // 0x100ac400

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x100a7720
    virtual INT OnCreate();                                          // 37 0x100ad360
    virtual INT OnDestroy();                                         // 38 0x100ac480
    virtual void ShowWindow();                                       // 48 0x100b71f0, the body of NCSystemMenuWnd::ShowWindow (shared, hooked there; a stub here)
    virtual void Minimize();                                         // 54 0x100a7970
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x100a8c30
    virtual INT OnDrawToolTip(NCGDevice* Device);                    // 63 0x100ac5f0 (the db's OnPostPaint)
    virtual INT OnMouseMove(WPARAM wParam, LPARAM lParam);           // 67 0x100a7860
    virtual INT OnLButtonUp(WPARAM wParam, LPARAM lParam) { return 1; }   // 69 0x100e6a50 (folded `return 1`)
    // names ours
    virtual INT OnNew();                                             // 74 0x100a8ec0
    virtual INT OnHelp();                                            // 75 0x100a7d00 (shared with NCMacroEditWnd's)

    // names ours: what the console's events call (jumps in NConsoleWnd's slots 217 / 218 / 219: the running macro's line waits
    // for the command's answer, and the event cuts the wait short)
    __declspec(noinline) void OnPickUpDone();                                             // 0x100a8730 slot 219: the line is /pickup
    __declspec(noinline) void OnTargetDone();                                             // 0x100a87c0 slot 218: the line is /target, /targetnext, /assist
    void OnSkillCoolTime(INT SkillID, INT TimeMS);                   // 0x100a8850 slot 217: the line is the skill (its cool time)
    __declspec(noinline) void SetIcon(INT Index);                                         // 0x100a8bc0
    __declspec(noinline) UBOOL HitDelete(INT X, INT Y);                                   // 0x100a77a0 (ret 8; anonymous in the db) (X, Y) in the delete icon's rectangle
    __declspec(noinline) UBOOL HitEdit(INT X, INT Y);                                     // 0x100a7800 (ret 8; anonymous in the db) in the edit icon's rectangle
    __declspec(noinline) void AddMacro(NMacroEntry* Macro);                               // 0x100ab4b0
};
NW_ASSERT_OFFSET(NCMacroListWnd, BackTex, 0xE8);
NW_ASSERT_OFFSET(NCMacroListWnd, DeleteX, 0xFC);
NW_ASSERT_OFFSET(NCMacroListWnd, EditX, 0x10C);
NW_ASSERT_OFFSET(NCMacroListWnd, bEditDragOver, 0x11C);
NW_ASSERT_OFFSET(NCMacroListWnd, bOverDelete, 0x128);
NW_ASSERT_OFFSET(NCMacroListWnd, Macros, 0x12C);
NW_ASSERT_OFFSET(NCMacroListWnd, Array, 0x138);
NW_ASSERT_OFFSET(NCMacroListWnd, Slots, 0x13C);
NW_ASSERT_OFFSET(NCMacroListWnd, Selected, 0x60E0);
NW_ASSERT_OFFSET(NCMacroListWnd, EditToolTip, 0x60E4);
NW_ASSERT_OFFSET(NCMacroListWnd, DeleteToolTip, 0x6164);
NW_ASSERT_OFFSET(NCMacroListWnd, HelpButton, 0x61E8);
NW_ASSERT_SIZE(NCMacroListWnd, 0x61EC);

#pragma pack(pop)
