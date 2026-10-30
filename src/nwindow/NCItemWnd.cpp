/*=============================================================================
    NCItemWnd.cpp - grid of item icons, base of the item windows, and NItemEntry, the record
    of each item. Stock (C4): 0x1001E460 .. 0x10020370 (NItemEntry's getters and Reset, the grid's methods, NItemEntry's
    constructors and Copy, NCItemWnd's constructors and destructor), plus WindowProc (0x10014B90), ~NItemEntry (0x10050030) and
    the folded getters 0x10151140 / 0x10151150 / 0x10151160. (The C3 image had them at 0x10015B80 .. 0x100178E0.)
    C4 PROOF (unicorn, stock against ours on random objects): the entry's constructors, Reset, Copy, MakeDisplayName and getters in
    tools/port/emu/t_entry.py; ~NItemEntry and NCItemWnd's destructor path in tools/port/emu/win/t_itemdebt.py; the grid's HitTest,
    GetSelected, GetItemCount, OnMouseWheel, InitScrollBar, UpdateScrollRange, OnScrollBar, ScrollUp / Down, OnLButtonDown,
    OnMouseMove, UnselectAll and Clear, and OnPaint, OnCreate, OnDestroy, ShowWindow and HideWindow too, in
    tools/port/emu/win/t_itemwnd.py (19 functions, 300 random grids each, 0 differences; the device, the textures, the slider and the
    desktop's message boxes are logging services on both sides). The constructors and HitTestSkill have the judges' proof only.
=============================================================================*/
#include "nwindow/NCItemWnd.h"
#include "nwindow/NCConsole.h"
#include "nwindow/NCDialogBox.h"
#include "nwindow/NCCoolTimeIcon.h"
#include "nwindow/NCEditBox.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NItemC4.h"
#include "nwindow/NCMacroEditList.h"
#include "nwindow/NCMacroEditWnd.h"
#include "nwindow/NCSkillItemWnd.h"
#include "nwindow/NCSliderBar.h"
#include "nwindow/NClasses.gen.h"
#include "nwindow/NConsoleWnd.h"
#if NW_FEATURES
// phase 6: ItemHoverTip (not in NWindow_C1: the hover tip of the item grids in the recent style)
#include "features/ItemHoverTip.h"
#endif

enum { SOUND_PickUp = 3 };  // an item picked up (name ours)
enum { SOUND_CoolTimeEnd = 9 };  // a cool time ended (name ours)

// the desktop's message box (NDialogInfo::bMainBox) and the second one
static FORCEINLINE NCDialogBox* MainDialogBox()
{
    return GDesktop->MainDialogBox;
}
static FORCEINLINE NCDialogBox* SecondDialogBox()
{
    return GDesktop->SecondDialogBox;
}

/*-----------------------------------------------------------------------------
    NItemEntry
-----------------------------------------------------------------------------*/

// 0x1001f7f0 - the defaults: every number 0 but the -1s; the tool tip object is built first (NToolTipInfo's constructor,
// 0x10002d90). C4: five icon dwords (+0x00 .. +0x10) and five icon names
NItemEntry::NItemEntry()
{
    new ((void*)&ToolTip) NToolTipInfo();
    Name[0] = 0;
    AdditionalName[0] = 0;
    IconName[0] = 0;
    Unknown20C[0] = 0;
    Unknown30C[0] = 0;
    IconName4[0] = 0;
    IconName5[0] = 0;
    Description[0] = 0;
    Icon = NULL;
    Unknown004 = 0;
    Unknown008 = 0;
    Unknown00C = 0;
    Unknown010 = 0;
    TradeCount = 0;
    bSelected = 0;
    ObjectID = 0;
    ID = 0;
    Count = 0;
    UnknownC20 = 0;
    BodyPart = 0;
    Weight = 0;
    Unknown1C80 = 0;
    Unknown1C84 = 0;
    Unknown1C88 = 0;
    Unknown1C8C = 0;
    Unknown1C90 = 0;
    Unknown1C94 = 0;
    Unknown1C58 = 0;
    CrystalGrade = 0;
    Unknown1C60 = 0;
    Unknown1C64 = 0;
    Unknown1C68 = 0;
    Unknown1C6C = 0;
    Unknown1C70 = 0;
    Unknown1C78 = 0;
    Unknown1C7C = 0;
    Enchant = 0;
    UnknownC3C = 0;
    bStackable = 0;
    bEquipped = 0;
    Unknown1C98 = 0;
    Unknown1998 = 0;
    Unknown1C9C = 0;
    Unknown1CA0 = 0;
    Unknown1CA4 = 0;
    Unknown1CA8 = 0;
    Unknown19AC = 0;
    DisplayName[0] = 0;
    Type = -1;
    Unknown1910 = -1;
    UnknownC18 = -1;
    UnknownC2C = -1;
    UnknownC30 = -1;
    SubType = -1;
    UnknownC34 = -1;
    Unknown19B4 = -1;
}
NW_HOOK_CTOR(0x1001f7f0, NItemEntry)

// 0x1001f960 - the fields do not keep NItemInfo's order; Unknown1C60 / Unknown1C64 (NItemInfo +0x1960 / +0x1964) stay
// uninitialized, and so does the display name past its first character
NItemEntry::NItemEntry(NItemInfo* Src)
{
    new ((void*)&ToolTip) NToolTipInfo();
    Icon = NULL;
    Unknown004 = 0;
    Unknown008 = 0;
    Unknown00C = 0;
    Unknown010 = 0;
    Name[0] = 0;
    AdditionalName[0] = 0;
    IconName[0] = 0;
    Unknown20C[0] = 0;
    Unknown30C[0] = 0;
    IconName4[0] = 0;
    IconName5[0] = 0;
    Description[0] = 0;
    DisplayName[0] = 0;
    TradeCount = 0;
    bSelected = 0;

    Icon = Src->Icon;
    Unknown004 = Src->UnknownC04;
    Unknown008 = Src->UnknownC08;
    Unknown00C = Src->Icon4;
    Unknown010 = Src->Icon5;
    appStrcpy(Name, Src->Name);
    appStrcpy(AdditionalName, Src->AdditionalName);
    appStrcpy(IconName, Src->IconName);
    appStrcpy(Unknown20C, Src->Unknown200);
    appStrcpy(Unknown30C, Src->Unknown300);
    appStrcpy(IconName4, Src->IconName4);
    appStrcpy(IconName5, Src->IconName5);
    appStrcpy(Description, Src->Description);
    Type = Src->Param;
    Unknown1910 = Src->Unknown1710;
    ObjectID = Src->ObjectID;
    ID = Src->ID;
    Count = Src->Count;
    UnknownC20 = Src->UnknownC1C;
    UnknownC18 = Src->UnknownC20;
    BodyPart = Src->BodyPart;
    Weight = Src->UnknownC28;
    UnknownC2C = Src->UnknownC2C;
    UnknownC30 = Src->UnknownC30;
    Unknown1C80 = Src->UnknownC34[0];
    Unknown1C84 = Src->UnknownC34[1];
    Unknown1C88 = Src->UnknownC34[2];
    Unknown1C8C = Src->UnknownC34[3];
    Unknown1C90 = Src->UnknownC34[4];
    Unknown1C94 = Src->UnknownC34[5];
    Unknown1C58 = Src->UnknownC34[6];
    CrystalGrade = Src->UnknownC34[7];
    Unknown1C68 = Src->UnknownC34[10];
    Unknown1C6C = Src->UnknownC34[11];
    Unknown1C70 = Src->UnknownC34[12];
    SubType = Src->SubType;
    Unknown1C78 = Src->UnknownC6C;
    Unknown1C7C = Src->UnknownC70;
    Enchant = Src->Enchant;
    UnknownC3C = Src->UnknownC84;
    bStackable = Src->bStackable;
    bEquipped = Src->UnknownC8C;
    UnknownC34 = Src->UnknownC90;
    Unknown1C98 = Src->UnknownC94;
    Unknown1998 = Src->Unknown17A0;
    Unknown1C9C = Src->UnknownC78;
    Unknown1CA0 = Src->UnknownC7C;
    Unknown1CA4 = Src->UnknownC80;
    Unknown1CA8 = Src->UnknownC98;
    Unknown19AC = Src->Unknown1788;
    Unknown19B4 = Src->New178C;
}
NW_HOOK_CTOR_ARGS(0x1001f960, NItemEntry, (NItemInfo* A), (A))

// 0x10050030 - the tool tip's destructor on the entry's tool tip (a tail jump into NToolTipInfo's, 0x10002e40, in the
// stock body; the union keeps the compiler from building and destroying it by itself). A real hook, not a stand-in: verify OK,
// and proved against the stock under unicorn (tools/port/emu/win/t_itemdebt.py: the call of 0x10002E40 on entry + 0x1BC0, the
// object untouched; with it the destructors of the windows that hold an NItemEntry member are no longer blocked by a stub)
NItemEntry::~NItemEntry()
{
    ToolTip.~NToolTipInfo();
}
NW_HOOK_DTOR(0x10050030, NItemEntry)

// 0x1001e460
const TCHAR* NItemEntry::GetName()
{
    const TCHAR* S = Name;
    return S && *S ? S : NULL;
}
NW_HOOK_METHOD(0x1001e460, NItemEntry, GetName)

// 0x1001e470 - the name the tool tip and the lists show: "+N" (the enchant, when above 0 and the type in Unknown1910 is
// not 7, 0xD or 0xE), the name, "-" and the additional name
const TCHAR* NItemEntry::MakeDisplayName()
{
    TCHAR* Out = DisplayName;
    Out[0] = 0;
    if (Enchant > 0 && Unknown1910 != 7 && Unknown1910 != 0xD && Unknown1910 != 0xE)
        appSprintf(Out, TEXT("+%d"), Enchant);
    const TCHAR* S = Name;
    if (S && *S)
        appStrcat(Out, S);
    const TCHAR* Add = AdditionalName;
    if (Add && *Add)
    {
        appStrcat(Out, TEXT("-"));
        appStrcat(Out, Add);
    }
    Out[appStrlen(Out) + 1] = 0;
    return Out;
}
NW_HOOK_METHOD(0x1001e470, NItemEntry, MakeDisplayName)

// 0x1001e510
const TCHAR* NItemEntry::GetAdditionalName()
{
    const TCHAR* S = AdditionalName;
    return S && *S ? S : NULL;
}
NW_HOOK_METHOD(0x1001e510, NItemEntry, GetAdditionalName)

// 0x1001e530
const TCHAR* NItemEntry::GetDescription()
{
    const TCHAR* S = Description;
    return S && *S ? S : NULL;
}
NW_HOOK_METHOD(0x1001e530, NItemEntry, GetDescription)

// 0x10151140 / 0x10151150 / 0x10151160 (names ours) - `mov eax, [ecx + 4]` / `[ecx + 0xC]` / `[ecx + 0x10]`: the second,
// fourth and fifth icon dwords. The linker folded each body with every class whose dwords are read the same way, so the
// hooks serve them all. (The first dword's getter, 0x1016ec20, is 3 bytes: too short for a hook; the third's, 0x10005b90,
// belongs to the base lane.)
INT NItemEntry::ReadDword04()
{
    return Unknown004;
}
NW_HOOK_METHOD(0x10151140, NItemEntry, ReadDword04)

INT NItemEntry::ReadDword0C()
{
    return Unknown00C;
}
NW_HOOK_METHOD(0x10151150, NItemEntry, ReadDword0C)

INT NItemEntry::ReadDword10()
{
    return Unknown010;
}
NW_HOOK_METHOD(0x10151160, NItemEntry, ReadDword10)

// 0x1001e550
INT NItemEntry::GetUnknownC20()
{
    return UnknownC20;
}
NW_HOOK_METHOD(0x1001e550, NItemEntry, GetUnknownC20)

// 0x1001e560
INT NItemEntry::GetUnknownC34()
{
    return UnknownC34;
}
NW_HOOK_METHOD(0x1001e560, NItemEntry, GetUnknownC34)

// 0x1001e570
void NItemEntry::SetUnknownC34(INT Value)
{
    UnknownC34 = Value;
}
NW_HOOK_METHOD(0x1001e570, NItemEntry, SetUnknownC34)

// 0x1001fe00 - the texts and the icons only when set in Src (the second to fifth icon textures all look at IconName's first
// character: the stock code does, whatever the intent was); the display name is Src's own (built), the tool tip object is
// copied whole (NToolTipInfo's copy assignment, 0x1001fcc0)
void NItemEntry::Copy(NItemEntry* Src)
{
    if (Src->GetIcon())
        Icon = Src->GetIcon();
    if (Src->Unknown004)
        Unknown004 = Src->Unknown004;
    if (Src->Unknown008)
        Unknown008 = Src->Unknown008;
    if (Src->Unknown00C)
        Unknown00C = Src->Unknown00C;
    if (Src->Unknown010)
        Unknown010 = Src->Unknown010;
    if (Src->GetName())
        appStrcpy(Name, Src->GetName());
    else
        Name[0] = 0;
    const TCHAR* SrcIconName = Src->IconName;
    if (SrcIconName && *SrcIconName)
        appStrcpy(IconName, SrcIconName);
    if (Src->Unknown20C && *SrcIconName)
        appStrcpy(Unknown20C, Src->Unknown20C);
    if (Src->Unknown30C && *SrcIconName)
        appStrcpy(Unknown30C, Src->Unknown30C);
    if (Src->IconName4 && *SrcIconName)
        appStrcpy(IconName4, Src->IconName4);
    if (Src->IconName5 && *SrcIconName)
        appStrcpy(IconName5, Src->IconName5);
    if (Src->GetAdditionalName())
        appStrcpy(AdditionalName, Src->GetAdditionalName());
    else
        AdditionalName[0] = 0;
    if (Src->GetDescription())
        appStrcpy(Description, Src->GetDescription());
    else
        Description[0] = 0;
    if (Src->MakeDisplayName())
        appStrcpy(DisplayName, Src->MakeDisplayName());
    else
        DisplayName[0] = 0;
    Type = Src->Type;
    Unknown1910 = Src->Unknown1910;
    ObjectID = Src->ObjectID;
    ID = Src->ID;
    Count = Src->Count;
    UnknownC20 = Src->UnknownC20;
    UnknownC18 = Src->UnknownC18;
    BodyPart = Src->BodyPart;
    Weight = Src->Weight;
    UnknownC2C = Src->UnknownC2C;
    UnknownC30 = Src->UnknownC30;
    Enchant = Src->Enchant;
    UnknownC3C = Src->UnknownC3C;
    bStackable = Src->bStackable;
    bEquipped = Src->bEquipped;
    UnknownC34 = Src->UnknownC34;
    bSelected = Src->bSelected;
    Unknown1C58 = Src->Unknown1C58;
    CrystalGrade = Src->CrystalGrade;
    Unknown1C60 = Src->Unknown1C60;
    Unknown1C64 = Src->Unknown1C64;
    Unknown1C68 = Src->Unknown1C68;
    Unknown1C6C = Src->Unknown1C6C;
    Unknown1C70 = Src->Unknown1C70;
    SubType = Src->SubType;
    Unknown1C80 = Src->Unknown1C80;
    Unknown1C84 = Src->Unknown1C84;
    Unknown1C88 = Src->Unknown1C88;
    Unknown1C8C = Src->Unknown1C8C;
    Unknown1C90 = Src->Unknown1C90;
    Unknown1C94 = Src->Unknown1C94;
    Unknown1C78 = Src->Unknown1C78;
    Unknown1C98 = Src->Unknown1C98;
    Unknown1998 = Src->Unknown1998;
    Unknown1C9C = Src->Unknown1C9C;
    Unknown1CA0 = Src->Unknown1CA0;
    Unknown1CA4 = Src->Unknown1CA4;
    Unknown1CA8 = Src->Unknown1CA8;
    Unknown1C7C = Src->Unknown1C7C;
    Unknown19AC = Src->Unknown19AC;
    Unknown19B4 = Src->Unknown19B4;
    ToolTip = Src->ToolTip;   // NToolTipInfo's copy assignment
}
NW_HOOK_METHOD(0x1001fe00, NItemEntry, Copy)

// NItemEntry::FillToolTip is inline in NCItemWnd.h (NToolTipInfo::Fill)

// 0x1001e580
void NItemEntry::Select()
{
    bSelected = 1;
}
NW_HOOK_METHOD(0x1001e580, NItemEntry, Select)

// 0x1001e590 (7 bytes: mov eax, [ecx + 0x1B5C]; ret) - naked, like Unselect: the paints that call it (OnPaint of the item
// windows) keep ecx/edx across the call in the stock code, so the body must touch eax only
NW_NAKED UBOOL NItemEntry::IsSelected()
{
    __asm
    {
        mov eax, dword ptr [ecx + 0x1B5C]
        ret
    }
}
NW_HOOK_METHOD(0x1001e590, NItemEntry, IsSelected)

// 0x1001e5a0 - naked: the stock loop of UnselectAll counts in eax across the call, so eax must come back untouched;
// bSelected is at +0x1B5C (NW_ASSERT_OFFSET)
NW_NAKED INT NItemEntry::Unselect()
{
    __asm
    {
        mov dword ptr [ecx + 0x1B5C], 0
        ret
    }
}
NW_HOOK_METHOD(0x1001e5a0, NItemEntry, Unselect)

// 0x1001e5b0 - the field is a kind (1, 2 or 3: the count shows), not a flag
UBOOL NItemEntry::IsStackable()
{
    return bStackable == 1 || bStackable == 2 || bStackable == 3;
}
NW_HOOK_METHOD(0x1001e5b0, NItemEntry, IsStackable)

// 0x1001e5d0 (name ours)
INT NItemEntry::GetUnknown19AC()
{
    return Unknown19AC;
}
NW_HOOK_METHOD(0x1001e5d0, NItemEntry, GetUnknown19AC)

// 0x1001e5e0 - the constructor's defaults, but the tool tip object, TradeCount, bSelected, Unknown1CA8, Unknown19AC
// and Unknown19B4 are kept
void NItemEntry::Reset()
{
    Name[0] = 0;
    AdditionalName[0] = 0;
    IconName[0] = 0;
    Unknown20C[0] = 0;
    Unknown30C[0] = 0;
    IconName4[0] = 0;
    IconName5[0] = 0;
    Description[0] = 0;
    Icon = NULL;
    Unknown004 = 0;
    Unknown008 = 0;
    Unknown00C = 0;
    Unknown010 = 0;
    Type = -1;
    Unknown1910 = -1;
    ObjectID = 0;
    ID = 0;
    Count = 0;
    UnknownC20 = 0;
    UnknownC18 = -1;
    BodyPart = 0;
    Weight = 0;
    UnknownC2C = -1;
    UnknownC30 = -1;
    Unknown1C80 = 0;
    Unknown1C84 = 0;
    Unknown1C88 = 0;
    Unknown1C8C = 0;
    Unknown1C90 = 0;
    Unknown1C94 = 0;
    Unknown1C58 = 0;
    CrystalGrade = 0;
    Unknown1C60 = 0;
    Unknown1C64 = 0;
    Unknown1C68 = 0;
    Unknown1C6C = 0;
    Unknown1C70 = 0;
    SubType = -1;
    Unknown1C78 = 0;
    Unknown1C7C = 0;
    Enchant = 0;
    UnknownC3C = 0;
    bStackable = 0;
    bEquipped = 0;
    UnknownC34 = -1;
    Unknown1C98 = 0;
    Unknown1998 = 0;
    Unknown1C9C = 0;
    Unknown1CA0 = 0;
    Unknown1CA4 = 0;
    DisplayName[0] = 0;
}
NW_HOOK_METHOD(0x1001e5e0, NItemEntry, Reset)

// 0x10008580 (the stock keeps it among NDialogInfo's: a small record's constructor; some callers build the request
// inline) - Unknown02 is left as it is
NItemRequest::NItemRequest()
{
    *(WORD*)&Type = 0xFFFF;
    ObjectID = 0;
    ItemID = 0;
    Count = 0;
    Unknown10 = 0;
    Unknown14 = 0;
    BodyPart = 0;
    Enchant = 0;
    Unknown1C = 0;
    bEquipped = 0;
    Unknown24 = 0;
    Unknown28 = 0;
    Unknown2C = -1;
}
NW_HOOK_CTOR(0x10008580, NItemRequest)

/*-----------------------------------------------------------------------------
    NCItemWnd
-----------------------------------------------------------------------------*/

// 0x100201b0 - 5 x 5 icons of 32 pixels (the cell gaps are left as allocated)
NCItemWnd::NCItemWnd()
{
    BackTex = NULL;
    SelectTex = NULL;
    ToolTipTex = NULL;
    Columns = 5;
    VisibleRows = 5;
    TopRow = 0;
    NumRows = 0;
    bDragging = 0;
    bNoDrag = 0;
    MaxItems = 25;
    CellSize = 32;
    Unknown0C8 = 16;
    Selected = -1;
    HoverIndex = -1;
}
NW_HOOK_CTOR(0x100201b0, NCItemWnd)

// 0x10020270 (96 bytes) - the vtable store 0x101A5520, ConditionalDestroy, Items' destructor (TItemEntryArray::~TItemEntryArray
// 0x1001F6E0), then NCScrollWnd::~NCScrollWnd (0x10015950); NCShopItemWnd's destructor 0x10105090 calls it and is proved against
// the stock by tools/port/emu/win/t_itemdebt.py
NW_HOOK_DTOR(0x10020270, NCItemWnd)

// 0x100202d0 - NumRows and bNoDrag are left as allocated
NCItemWnd::NCItemWnd(INT InColumns, INT InVisibleRows, INT InMaxItems, INT InCellSize, INT InGapX, INT InGapY)
{
    BackTex = NULL;
    SelectTex = NULL;
    ToolTipTex = NULL;
    Columns = InColumns;
    MaxItems = InMaxItems;
    TopRow = 0;
    bDragging = 0;
    VisibleRows = InVisibleRows;
    Selected = -1;
    HoverIndex = -1;
    CellGapX = InGapX;
    CellSize = InCellSize;
    Unknown0C8 = 16;
    CellGapY = InGapY;
}
NW_HOOK_CTOR_ARGS(0x100202d0, NCItemWnd, (INT A, INT B, INT C, INT D, INT E, INT F), (A, B, C, D, E, F))


// 0x10014b90 (vslot 25) - the base's answer as a plain 0 / 1
INT NCItemWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return NCScrollWnd::WindowProc(Msg, wParam, lParam) != 0;
}
NW_HOOK_METHOD(0x10014b90, NCItemWnd, WindowProc)

// C2 CANDIDATE: the C2 function is not the same code as the C1 one; kept only while it matches the stock one (verify_data)
// 0x1001ed90 (vslot 37)
INT NCItemWnd::OnCreate()
{
    NCScrollWnd::OnCreate();
    BackTex = FindTexture(TEXT("NWindow.BlackTexture"));
    SelectTex = FindTexture(TEXT("l2ui.nwindow.item_click"));
    ToolTipTex = FindTexture(TEXT("L2UI.tooltip.mini-tool01"));
    InitScrollBar();
    return 1;
}
NW_HOOK_METHOD(0x1001ed90, NCItemWnd, OnCreate)

// 0x1001ea20 - the range starts as TopRow rows, as in the stock code
void NCItemWnd::InitScrollBar()
{
    SetRepeatDelay(200);
    ScrollBar->SetRange(TopRow, VisibleRows);
    ScrollBar->SetValue(TopRow);
}
NW_HOOK_METHOD(0x1001ea20, NCItemWnd, InitScrollBar)

// 0x1001e710 (vslot 38)
INT NCItemWnd::OnDestroy()
{
    guard(NCItemWnd::OnDestroy);
    Clear();
    NCScrollWnd::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1001e710, NCItemWnd, OnDestroy)

// 0x1001ead0 (vslot 48) - shown, the scroll range follows what was added while it was hidden
void NCItemWnd::ShowWindow()
{
    NCWnd::ShowWindow();
    UpdateScrollRange();
}
NW_HOOK_METHOD(0x1001ead0, NCItemWnd, ShowWindow)

// 0x1001e960 (vslot 69) - the base's release (the mouse button state), answered 1
INT NCItemWnd::OnLButtonUp(WPARAM wParam, LPARAM lParam)
{
    NCWnd::OnLButtonUp(wParam, lParam);
    return 1;
}
NW_HOOK_METHOD(0x1001e960, NCItemWnd, OnLButtonUp)

// 0x1001e980 (vslot 49) - closes the desktop's message boxes this window opened (C3 tests the desktop and the box)
void NCItemWnd::HideWindow()
{
    if (GDesktop && GDesktop->SecondDialogBox)
    {
        if (GDesktop->SecondDialogBox->GetOwner() == Entry)
            GDesktop->SecondDialogBox->HideWindow();
    }
    if (GDesktop && GDesktop->MainDialogBox)
    {
        if (GDesktop->MainDialogBox->GetOwner() == Entry)
            GDesktop->MainDialogBox->HideWindow();
    }
    NCWnd::HideWindow();
}
NW_HOOK_METHOD(0x1001e980, NCItemWnd, HideWindow)

// 0x1001e790 - the cells count CellSize + the gaps; the right and bottom edges belong to the last column / row. The
// offsets move the window's origin (C3 callers pass 0, 0)
INT NCItemWnd::HitTest(INT X, INT Y, INT OffsetX, INT OffsetY)
{
    X -= ScreenX + OffsetX;
    Y -= ScreenY + OffsetY;
    if (X < 0)
        return -1;
    INT CellW = CellGapX + CellSize;
    if (X > Columns * CellW)
        return -1;
    if (Y < 0)
        return -1;
    INT CellH = CellGapY + CellSize;
    if (Y > VisibleRows * CellH)
        return -1;
    INT Col = X / CellW;
    if (Col >= Columns)
        Col--;
    INT Row = Y / CellH;
    if (Row >= VisibleRows)
        Row--;
    return (TopRow + Row) * Columns + Col;
}
NW_HOOK_METHOD(0x1001e790, NCItemWnd, HitTest)

// 0x1001e830
INT NCItemWnd::GetSelected()
{
    if (Selected >= 0 && Selected < Items.Num())
        return Selected;
    return -1;
}
NW_HOOK_METHOD(0x1001e830, NCItemWnd, GetSelected)

// 0x1001e850
INT NCItemWnd::GetItemCount()
{
    return Items.Num();
}
NW_HOOK_METHOD(0x1001e850, NCItemWnd, GetItemCount)

// 0x1001e8b0 (vslot 73) - a row per notch
INT NCItemWnd::OnMouseWheel(WPARAM wParam, LPARAM lParam)
{
    SWORD Notches = (SWORD)((SWORD)HIWORD(wParam) / 120);
    if (Notches > 0)
    {
        if (TopRow > Notches)
            TopRow -= Notches;
        else
            TopRow = 0;
    }
    else if (NumRows - TopRow + Notches > VisibleRows)
        TopRow -= Notches;
    else if (NumRows >= VisibleRows)
        TopRow = NumRows - VisibleRows;
    else
        TopRow = 0;
    ScrollBar->SetValue(TopRow);
    return 1;
}
NW_HOOK_METHOD(0x1001e8b0, NCItemWnd, OnMouseWheel)

// 0x1001ea10 - the stock body is shared with other classes' one-field setters
void NCItemWnd::SetVisibleRows(INT Rows)
{
    VisibleRows = Rows;
}
NW_HOOK_METHOD(0x1001ea10, NCItemWnd, SetVisibleRows)

// 0x1001ea60 - rows for the items; TopRow comes back when the last row would leave the view
void NCItemWnd::UpdateScrollRange()
{
    NumRows = Items.Num() / Columns;
    if (Items.Num() % Columns)
        NumRows++;
    if (NumRows >= VisibleRows && NumRows - TopRow < VisibleRows)
        TopRow = NumRows - VisibleRows;
    ScrollBar->SetRange(NumRows, VisibleRows);
    ScrollBar->SetValue(TopRow);
}
NW_HOOK_METHOD(0x1001ea60, NCItemWnd, UpdateScrollRange)

// 0x1001eae0 (vslot 77) - NCScrollWnd's OnScrollBar overridden (TopRow = Value)
INT NCItemWnd::OnScrollBar(INT Value)
{
    TopRow = Value;
    return 1;
}
NW_HOOK_METHOD(0x1001eae0, NCItemWnd, OnScrollBar)

// 0x1001eb00 (vslot 75)
INT NCItemWnd::ScrollUp()
{
    if (NumRows > VisibleRows)
    {
        if (--TopRow < 0)
            TopRow = 0;
        ScrollBar->SetValue(TopRow);
    }
    return 1;
}
NW_HOOK_METHOD(0x1001eb00, NCItemWnd, ScrollUp)

// 0x1001eb40 (vslot 76)
INT NCItemWnd::ScrollDown()
{
    if (NumRows > VisibleRows)
    {
        TopRow++;
        if (TopRow > NumRows - VisibleRows)
            TopRow = NumRows - VisibleRows;
        ScrollBar->SetValue(TopRow);
    }
    return 1;
}
NW_HOOK_METHOD(0x1001eb40, NCItemWnd, ScrollDown)

// 0x1001eb90 (vslot 80) - `ret 0x10` shared with other empty four-argument methods
void NCItemWnd::DrawItemToolTip(NCGDevice* Device, NItemEntry* Item, INT X, INT Y)
{
}
NW_HOOK_METHOD(0x1001eb90, NCItemWnd, DrawItemToolTip)

// 0x1001edf0 (vslot 62) - the clip is the window's rectangle; the first cell sits at (1, 1) and the cells are
// CellSize + the gaps apart; the rows stop at VisibleRows * (CellSize + 1) pixels
INT NCItemWnd::OnPaint(NCGDevice* Device)
{
    guard(NCItemWnd::OnPaint);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    Device->BeginPaint(ScreenX, ScreenY);
    INT X = 1;
    INT Y = 1;
    for (INT i = TopRow * Columns; i < Items.Num(); i++)
    {
        NItemEntry* Item = Items(i);
        if (!Item)
            continue;
        if (i % Columns == 0)
        {
            X = 1;
            if (i != TopRow * Columns)
                Y += CellGapY + CellSize;
            if (VisibleRows * (CellSize + 1) <= Y)
                break;
        }
        else
            X += CellGapX + CellSize;
        if (Item->IsSelected())
            Device->DrawTexture(X - 1, Y - 1, 34, 34, 0, 0, 34, 34, SelectTex, 0xFF, 1);
        if (Item->GetIcon())
            Device->DrawTexture(X, Y, CellSize, CellSize, 0, 0, CellSize, CellSize, Item->GetIcon(), 0xFF, 1);
    }
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1001edf0, NCItemWnd, OnPaint)

// 0x10020370 (vslot 63; the db used to name it NCItemWndToolTipExt::DrawToolTip / NCTradeItemWnd::OnPostPaint: it is this grid's
// OnDrawToolTip, the tool tip routine every item grid without one of its own inherits) - the tip of the item under the mouse beside
// its cell (the cells' step, the window's gaps and the skin's rectangle of the window): the title box of one that is not selected, the
// full box of a selected one. The skin takes the tip by value (NToolTipSkin::Draw / DrawFull are the stock functions behind
// forwarders; the copy is NToolTipInfo's). The entry's tool tip is at +0x1BC0 (the C3 offset +0x19B8 that made this unit fail
// verify_data is gone). Stage 5 (sa-verify): the body lives here, on the class, instead of on a stand-in struct hooked from
// NCTradeWnd.cpp, so that the slot holds the very function the stock slot holds (verify_standalone vtables)
INT NCItemWnd::OnDrawToolTip(NCGDevice* Device)
{
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    if (HoverIndex >= 0 && HoverIndex < Items.Num())
    {
        INT Rel = HoverIndex - TopRow * Columns;
        INT Row = Rel / Columns;
        INT Col = Rel % Columns;
        INT X = (CellGapX + CellSize) * Col;
        INT Y = (CellGapY + CellSize) * Row;
        NItemEntry* Item = Items(HoverIndex);
        NToolTipRect Wnd;
        Wnd.X = ScreenX;
        Wnd.Y = ScreenY;
        Wnd.W = Width;
        Wnd.H = Height;
        if (!Item->IsSelected())
        {
            if (ToolTipSkin)
                ToolTipSkin->Draw(Device, AsC4(Item)->ToolTip, X, Y, Wnd);
        }
        else if (ToolTipSkin)
            ToolTipSkin->DrawFull(Device, AsC4(Item)->ToolTip, X, Y, Wnd);
    }
    Device->ReleaseClip();
    Device->EndPaint();
    return 1;
}
NW_HOOK_METHOD(0x10020370, NCItemWnd, OnDrawToolTip)

// 0x1001efb0 (vslot 68) - selects the item under the mouse and picks it up (unless bNoDrag, which lasts one
// click); the desktop's message box closes, and keeps the focus if still up.
INT NCItemWnd::OnLButtonDown(WPARAM wParam, LPARAM lParam)
{
    INT Index = HitTest(LOWORD(lParam), HIWORD(lParam), 0, 0);
    Selected = Index;
    HoverIndex = Index;
    if (Index >= 0 && Index < Items.Num())
    {
        UnselectAll();
        Items(Selected)->Select();
        if (!bNoDrag)
        {
            if (MainDialogBox()->IsShowWindow())
            {
                MainDialogBox()->Reset();
                MainDialogBox()->HideWindow();
            }
            bDragging = 1;
            GConsole->PlayInterfaceSound(SOUND_PickUp);
        }
        else
            bNoDrag = 0;
        if (MainDialogBox()->IsShowWindow())
            MainDialogBox()->SetFocus();
    }
    return 1;
}
NW_HOOK_METHOD(0x1001efb0, NCItemWnd, OnLButtonDown)

// 0x1001f0c0 (vslot 67) - while an item is carried the cursor is its icon (type 4: a color cursor built from the
// texture); C3 no longer puts the arrow back
INT NCItemWnd::OnMouseMove(WPARAM wParam, LPARAM lParam)
{
    HoverIndex = HitTest(LOWORD(lParam), HIWORD(lParam), 0, 0);
    if (IsFocused() && bDragging && Selected >= 0 && Selected < Items.Num())
    {
        SetCursor(4, Items(Selected)->GetIcon());
        LockCursor();
    }
    return 1;
}
NW_HOOK_METHOD(0x1001f0c0, NCItemWnd, OnMouseMove)

// 0x1001f130 (vslot 79)
void NCItemWnd::UnselectAll()
{
    for (INT i = 0; i < Items.Num(); i++)
        Items(i)->Unselect();
}
NW_HOOK_METHOD(0x1001f130, NCItemWnd, UnselectAll)

// 0x1001fd80 (vslot 78) - the entries are deleted (~NItemEntry 0x10050030, then the memory goes back to GMalloc)
void NCItemWnd::Clear()
{
    for (INT i = 0; i < Items.Num(); i++)
        if (Items(i))
        {
            Items(i)->~NItemEntry();   // the stock calls the destructor and gives the memory back to GMalloc by hand
            appFree(Items(i));
        }
    Items.Empty();
    TopRow = 0;
    NumRows = 0;
    Selected = -1;
    HoverIndex = -1;
}
NW_HOOK_METHOD(0x1001fd80, NCItemWnd, Clear)

NW_CHECK_VTABLE(NCItemWnd, 0x101a5520, 81)

/*-----------------------------------------------------------------------------
    The skill grid's functions the stock code names after NCItemWnd (guard strings): they work on
    NCSkillItemWnd's fields (the icon frames, the toggle animation, the cool time icons), a class
    of another lane's file, so they are methods of this class (names ours) that read their object
    as the derived class, and the cell offset of a point is the item window's 0x100a10a0
-----------------------------------------------------------------------------*/

// 0x100ad7e0 (name ours) - the item under a point of the skill grid: like HitTest but the cells are CellSize + 5 wide and
// CellSize + 3 high whatever the gaps say (the skill grid never sets them); -1 outside
INT NCItemWnd::HitTestSkill(INT X, INT Y)
{
    X -= ScreenX;
    Y -= ScreenY;
    if (X < 0)
        return -1;
    INT CellW = CellSize + 5;
    if (X > CellW * Columns)
        return -1;
    if (Y < 0)
        return -1;
    INT CellH = CellSize + 3;
    if (Y > CellH * VisibleRows)
        return -1;
    X /= CellW;
    if (X >= Columns)
        X--;
    Y /= CellH;
    if (Y >= VisibleRows)
        Y--;
    return (TopRow + Y) * Columns + X;
}
NW_HOOK_METHOD(0x100ad7e0, NCItemWnd, HitTestSkill)


