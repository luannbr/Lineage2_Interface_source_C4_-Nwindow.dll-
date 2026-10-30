/*=============================================================================
    NCPetWnd.cpp - the pet and summon windows and their Information and Action pages.
    Stock: 0x1008ee70 .. 0x1008ee90, 0x10090e10 .. 0x100916fb, 0x10091700 .. 0x10091823,
    0x10091fb0 .. 0x100938ca, 0x10093940 .. 0x10093a01, 0x100940b0 .. 0x100940ea,
    0x100940f0 .. 0x100941b1, 0x100bb540 .. 0x100bb57e.
=============================================================================*/
#include "nwindow/NCPetWnd.h"
#include "nwindow/NCSummonedWnd.h"
#include "nwindow/NCActionItemWnd.h"
#include "nwindow/NCButton.h"
#include "nwindow/NCFrameCtrl.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NCIcon.h"
#include "nwindow/NCPetInventoryWnd.h"
#include "nwindow/NCTabControl.h"
#include "nwindow/NConsoleWnd.h"
#include "nwindow/NDialogInfo.h"
#include "nwindow/NItemInfo.h"
#include "nwindow/NWndCreate.h"
#include "nwindow/NItemC4.h"

#include <stdio.h>
#include <wchar.h>

// the action categories the two Action pages list (FL2ActionData::Category)
enum { ACTION_Pet = 4, ACTION_Summon = 5 };

// back from the icon, at its place (inlined in the stock ShowWindows, name ours)
static FORCEINLINE void RestoreFromIcon(NCWnd* Wnd)
{
    if (Wnd->Icon)
    {
        Wnd->SetPosition(Wnd->Icon->ScreenX, Wnd->Icon->ScreenY, (INT)Wnd->Width, (INT)Wnd->Height);
        Wnd->Icon->SetPosition(0, 0, 0, 0);
        Wnd->Icon->HideWindow();
    }
}

// the stock code destroys its dialog record through one out-of-line destructor (0x10006e20); ours inlines the
// strings' destructors, so the function gets one of its own here
struct NDlg : public NDialogInfo
{
    __declspec(noinline) ~NDlg() {}
};

// every text of these windows goes out with the same trailing arguments
static FORCEINLINE void DrawString(NCGDevice* Device, INT X, INT Y, DWORD Color, const TCHAR* Text)
{
    Device->DrawText(X, Y, Color, Text, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
}

// a number centered on CenterX in the values' color (spelled out at each use in the stock code)
#if 0  // C2: does not compile against the C2 headers (db/demote.csv)
static FORCEINLINE void DrawNumber(NCGDevice* Device, TCHAR* Buf, INT CenterX, INT Y, INT Value)
{
    SIZE Size;
    appSprintf(Buf, TEXT("%d"), Value);
    NCGDevice::GetTextExtent(&Size, Buf, 0, 0, 0, 0, 0);
    DrawString(Device, CenterX - Size.cx / 2, Y, 0xFFB09B79, Buf);
}
#endif

// "now / max" around a slash centered at 76 (HP) and 181 (MP), in the large digits
#if 0  // C2: does not compile against the C2 headers (db/demote.csv)
static FORCEINLINE void DrawHPMP(NCGDevice* Device, TCHAR* Buf, INT Y, const PetInfo& Info)
{
    SIZE Size;
    appSprintf(Buf, TEXT("%s"), TEXT("/"));
    NCGDevice::GetTextExtent(&Size, Buf, 0, 0, 0, 0, 0);
    Device->DrawSpecialDigit(1, 76 - Size.cx / 2, Y, 0xFFFFFFFF, Buf);
    appSprintf(Buf, TEXT("%d"), Info.HP);
    NCGDevice::GetTextExtent(&Size, Buf, 0, 0, 0, 0, 0);
    Device->DrawSpecialDigit(1, 66 - Size.cx, Y, 0xFFFFFFFF, Buf);
    appSprintf(Buf, TEXT("%d"), Info.MaxHP);
    NCGDevice::GetTextExtent(&Size, Buf, 0, 0, 0, 0, 0);
    Device->DrawSpecialDigit(1, 82, Y, 0xFFFFFFFF, Buf);
    appSprintf(Buf, TEXT("%s"), TEXT("/"));
    NCGDevice::GetTextExtent(&Size, Buf, 0, 0, 0, 0, 0);
    Device->DrawSpecialDigit(1, 181 - Size.cx / 2, Y, 0xFFFFFFFF, Buf);
    appSprintf(Buf, TEXT("%d"), Info.MP);
    NCGDevice::GetTextExtent(&Size, Buf, 0, 0, 0, 0, 0);
    Device->DrawSpecialDigit(1, 171 - Size.cx, Y, 0xFFFFFFFF, Buf);
    appSprintf(Buf, TEXT("%d"), Info.MaxMP);
    NCGDevice::GetTextExtent(&Size, Buf, 0, 0, 0, 0, 0);
    Device->DrawSpecialDigit(1, 187, Y, 0xFFFFFFFF, Buf);
}
#endif

// ---- C3 paints: the pages are laid out differently from C2's (the helpers above were demoted)

// a number centered on CenterX in the values' color (spelled out at each use in the stock code)
static FORCEINLINE void DrawNumberC3(NCGDevice* Device, TCHAR* Buf, INT CenterX, INT Y, INT Value)
{
    SIZE Size;
    appSprintf(Buf, TEXT("%d"), Value);
    NCGDevice::GetTextExtent(&Size, Buf, 0, 0, 0, 0, 0);
    DrawString(Device, CenterX - Size.cx / 2, Y, 0xFFB09B79, Buf);
}

// The Information page of both windows (a copy of the code in each stock OnPaint): the combat values of the pet or summon
// in two columns of six rows 14 pixels apart, read again at each paint. C4 has a sixth row in each column: the last two
// values of the record (the fields new in C4, +0xAC and +0xB0; their labels are system strings 404 and 496).
static FORCEINLINE INT PaintInfoPageC4(NCWnd* Wnd, UTexture* BackTex, PetInfo& Info, NCGDevice* Device)
{
    Device->BeginPaint(Wnd->ScreenX, Wnd->ScreenY);
    Device->SetClip(Wnd->ScreenX, Wnd->ScreenY, (INT)Wnd->Width, (INT)Wnd->Height);
    if (BackTex)
        Device->DrawTexture(0, 0, (INT)Wnd->Width, (INT)Wnd->Height, 0, 0, (INT)Wnd->Width, (INT)Wnd->Height,
                            BackTex, 0xFF, 1);
    DrawString(Device, 17, 3, 0xFFA3A3A3, *GL2GameData.SysString[94]);     // "P.Atk."
    DrawString(Device, 17, 17, 0xFFA3A3A3, *GL2GameData.SysString[95]);    // "P.Def."
    DrawString(Device, 17, 31, 0xFFA3A3A3, *GL2GameData.SysString[96]);    // "Accuracy"
    DrawString(Device, 17, 45, 0xFFA3A3A3, *GL2GameData.SysString[113]);   // "Critical"
    DrawString(Device, 17, 59, 0xFFA3A3A3, *GL2GameData.SysString[111]);   // "Atk. Spd."
    DrawString(Device, 17, 73, 0xFFA3A3A3, *GL2GameData.SysString[404]);
    DrawString(Device, 132, 3, 0xFFA3A3A3, *GL2GameData.SysString[98]);    // "M.Atk."
    DrawString(Device, 132, 17, 0xFFA3A3A3, *GL2GameData.SysString[99]);   // "M.Def."
    DrawString(Device, 132, 31, 0xFFA3A3A3, *GL2GameData.SysString[97]);   // "Evasion"
    DrawString(Device, 132, 45, 0xFFA3A3A3, *GL2GameData.SysString[432]);  // "Speed"
    DrawString(Device, 132, 59, 0xFFA3A3A3, *GL2GameData.SysString[112]);  // "Casting Spd."
    DrawString(Device, 132, 73, 0xFFA3A3A3, *GL2GameData.SysString[496]);
    Info = GConsole->GetPetInfo();
    TCHAR Buf[64];
    DrawNumberC3(Device, Buf, 89, 3, Info.PAtk);
    DrawNumberC3(Device, Buf, 89, 17, Info.PDef);
    DrawNumberC3(Device, Buf, 89, 31, Info.Accuracy);
    DrawNumberC3(Device, Buf, 89, 45, Info.Critical);
    DrawNumberC3(Device, Buf, 89, 59, Info.PAtkSpeed);
    DrawNumberC3(Device, Buf, 89, 73, Info.UnknownAC);
    DrawNumberC3(Device, Buf, 214, 3, Info.MAtk);
    DrawNumberC3(Device, Buf, 214, 17, Info.MDef);
    DrawNumberC3(Device, Buf, 214, 31, Info.Evasion);
    DrawNumberC3(Device, Buf, 214, 45, Info.Speed);
    DrawNumberC3(Device, Buf, 214, 59, Info.CastSpeed);
    DrawNumberC3(Device, Buf, 214, 73, Info.UnknownB0_C4);
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
}

// "now / max" around a slash centered at 77, in the large digits (HP and MP rows of the summon's and pet's paints)
static FORCEINLINE void DrawValueRowC3(NCGDevice* Device, TCHAR* Buf, INT Y, INT Cur, INT Max)
{
    SIZE Size;
    appSprintf(Buf, TEXT("%s"), TEXT("/"));
    NCGDevice::GetTextExtent(&Size, Buf, 0, 0, 0, 0, 0);
    DrawDigit(Device, 1, 77 - Size.cx / 2, Y, 0xFFDCDCDC, Buf);
    appSprintf(Buf, TEXT("%d"), Cur);
    NCGDevice::GetTextExtent(&Size, Buf, 0, 0, 0, 0, 0);
    DrawDigit(Device, 1, 67 - Size.cx, Y, 0xFFDCDCDC, Buf);
    appSprintf(Buf, TEXT("%d"), Max);
    NCGDevice::GetTextExtent(&Size, Buf, 0, 0, 0, 0, 0);
    DrawDigit(Device, 1, 83, Y, 0xFFDCDCDC, Buf);
}

// a bar Max pixels wide at most (the stock code repeats the test for each bar)
static FORCEINLINE void DrawBar(NCGDevice* Device, INT X, INT Y, INT W, INT Max, UTexture* Tex)
{
    if (W > 0 && W <= Max)
        Device->DrawTexture(X, Y, W, 10, 0, 0, W, 10, Tex);
    else if (W > Max)
        Device->DrawTexture(X, Y, Max, 10, 0, 0, Max, 10, Tex);
}

// "<level> <name>" after the "Lv" label; a nameless pet or summon gets a NULL name, which
// appSprintf prints as "(null)", as in the stock code
#if 0  // C2: does not compile against the C2 headers (db/demote.csv)
static FORCEINLINE void DrawLevelName(NCGDevice* Device, const PetInfo& Info)
{
    TCHAR LevelText[64];
    DrawString(Device, 17, 30, 0xFFA3A3A3, *NCObject::SysString(88));   // "Lv"
    const TCHAR* Name = Info.Name;
    appSprintf(LevelText, TEXT("%d %s"), Info.Level, Name && *Name ? Name : NULL);
    DrawString(Device, 35, 30, 0xFFB09B79, LevelText);
}
#endif

// The Information page of both windows (one stock body, 0x10090f90): the combat values of the
// pet or summon, read again at each paint.
#if 0  // C2: does not compile against the C2 headers (db/demote.csv)
static FORCEINLINE INT PaintInfoPage(NCWnd* Wnd, UTexture* BackTex, PetInfo& Info,
                                     NCGDevice* Device)
{
    Device->BeginPaint(Wnd->ScreenX, Wnd->ScreenY);
    Device->SetClip(Wnd->ScreenX, Wnd->ScreenY, Wnd->Width, Wnd->Height);
    if (BackTex)
        Device->DrawTexture(0, 0, Wnd->Width, Wnd->Height, 0, 0, Wnd->Width, Wnd->Height,
                            BackTex);
    DrawString(Device, 17, 24, 0xFFA3A3A3, *NCObject::SysString(94));   // "P.Atk."
    DrawString(Device, 17, 41, 0xFFA3A3A3, *NCObject::SysString(95));   // "P.Def."
    DrawString(Device, 17, 58, 0xFFA3A3A3, *NCObject::SysString(96));   // "Accuracy"
    DrawString(Device, 17, 75, 0xFFA3A3A3, *NCObject::SysString(113));  // "Critical"
    DrawString(Device, 17, 92, 0xFFA3A3A3, *NCObject::SysString(111));  // "Atk. Spd."
    DrawString(Device, 122, 24, 0xFFA3A3A3, *NCObject::SysString(98));  // "M.Atk."
    DrawString(Device, 122, 41, 0xFFA3A3A3, *NCObject::SysString(99));  // "M.Def."
    DrawString(Device, 122, 58, 0xFFA3A3A3, *NCObject::SysString(97));  // "Evasion"
    DrawString(Device, 122, 75, 0xFFA3A3A3, *NCObject::SysString(432)); // "Speed"
    DrawString(Device, 122, 92, 0xFFA3A3A3, *NCObject::SysString(112)); // "Casting Spd."
    Info = GConsole->GetPetInfo();
    TCHAR Buf[64];
    DrawNumber(Device, Buf, 89, 24, Info.PAtk);
    DrawNumber(Device, Buf, 89, 41, Info.PDef);
    DrawNumber(Device, Buf, 89, 58, Info.Accuracy);
    DrawNumber(Device, Buf, 89, 75, Info.Critical);
    DrawNumber(Device, Buf, 89, 92, Info.PAtkSpeed);
    DrawNumber(Device, Buf, 206, 24, Info.MAtk);
    DrawNumber(Device, Buf, 206, 41, Info.MDef);
    DrawNumber(Device, Buf, 206, 58, Info.Evasion);
    DrawNumber(Device, Buf, 206, 75, Info.Speed);
    DrawNumber(Device, Buf, 206, 92, Info.CastSpeed);
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
}
#endif

// The Action page of both windows (one stock body, 0x10091680): the background; the icons are
// the grid's.
static FORCEINLINE INT PaintActionPage(NCWnd* Wnd, UTexture* BackTex, NCGDevice* Device)
{
    Device->BeginPaint(Wnd->ScreenX, Wnd->ScreenY);
    Device->SetClip(Wnd->ScreenX, Wnd->ScreenY, Wnd->Width, Wnd->Height);
    if (BackTex)
        Device->DrawTexture(0, 0, Wnd->Width, Wnd->Height, 0, 0, Wnd->Width, Wnd->Height,
                            BackTex);
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
}

// The icon grid of both Action pages (repeated in the stock OnCreates): 6 x 2 icons of 32
// pixels, 15 actions at most.
static FORCEINLINE NCActionItemWnd* CreateActionGrid(NCWnd* Parent)
{
    NCActionItemWnd* Grid = new NCActionItemWnd(6, 2, 15, 32);
    Grid->SetPosition(16, 29, 209, 69);
    Parent->CreateChild(Grid, NWS_VISIBLE | 0x20);   // 0x20: meaning unknown
    return Grid;
}

// The icon grid of the Action pages (C3): 6 x 2 icons of 32 pixels, 15 actions at most, at (18, 9)
static FORCEINLINE NCActionItemWnd* CreateActionGridC3(NCWnd* Parent)
{
    NCActionItemWnd* Grid = new NCActionItemWnd(6, 2, 15, 32);
    Grid->SetPosition(18, 9, 219, 69);
    Parent->CreateChild(Grid, NWS_VISIBLE | NWS_UNKNOWN20);   // 0x22
    return Grid;
}

// C2 action entry (FL2GameData::ActionTable, heap objects of actionname-*.dat): the fields of it that the
// engine header does not place, read here by offset (from the stock loops at 0x100b2fd0 and
// 0x100b50d0). FL2ActionEntry's IconName/Name (+0x1C/+0x28) are read the other way round here: the stock
// loop copies +0x1C into NItemInfo::Name and +0x28 into NItemInfo::IconName (the texture it looks up).
static FORCEINLINE INT ActionCategory(const FL2ActionEntry* E)   { return *(const INT*)((const BYTE*)E + 0x0C); }
static FORCEINLINE INT ActionParam(const FL2ActionEntry* E)      { return *(const INT*)((const BYTE*)E + 0x08); }
static FORCEINLINE const FArray& ActionPets(const FL2ActionEntry* E) { return *(const FArray*)((const BYTE*)E + 0x10); }
static FORCEINLINE const FString& ActionName(const FL2ActionEntry* E)  { return *(const FString*)((const BYTE*)E + 0x1C); }
static FORCEINLINE const FString& ActionIcon(const FL2ActionEntry* E)  { return *(const FString*)((const BYTE*)E + 0x28); }
static FORCEINLINE const FString& ActionDesc(const FL2ActionEntry* E)  { return *(const FString*)((const BYTE*)E + 0x34); }

// the grid's own functions (NCActionItemWnd.cpp): AddItem 0x10043660 (ret 4), ClearItems 0x100d8950
static void AddActionItem(NCActionItemWnd* Grid, NItemInfo* Info)
{
    Grid->AddItem(Info);
}
static void ClearActionItems(NCActionItemWnd* Grid)
{
    Grid->ClearItems();
}

// The actions of one category of actionname-*.dat into a grid, the pet's or the summon's own
// (two stock copies: the pet window's is part of its ShowWindow at 0x100b2fd0, the summon page's is
// 0x100b50d0). C2 filters them by the creature: PetInfo::Type must be PetType (the loop ends at the first
// action found while the record says otherwise), and an action lists the creatures it is for
// (ids offset by 1000000, or -2 for all). An icon that is not found shows black. The summon's copy skips
// entries whose first dword is 0; the pet's does not test it.
#if 0  // C2: does not compile against the C2 headers (db/demote.csv)
static FORCEINLINE void LoadActionCategoryC2(NCWnd* Wnd, NCActionItemWnd* Grid, INT Wanted, INT PetType,
                                             UBOOL bTestValid)
{
    for (INT i = 0; i < GL2GameData.ActionTable.Num(); i++)
    {
        FL2ActionEntry* Entry = GL2GameData.ActionTable(i);
        if (bTestValid && !Entry->bValid)
            continue;
        if (ActionCategory(Entry) != Wanted)
            continue;
        PetInfo Pet = GConsole->GetPetInfo();
        if (Pet.ObjectID == -1)
            return;
        if (Pet.Type != PetType)
            return;
        NItemInfo Info;
        Info.ID = Entry->ActionID;
        Info.Param = ActionParam(Entry);
        appStrcpy(Info.Name, *ActionName(Entry));
        appStrcpy(Info.IconName, *ActionIcon(Entry));
        appStrcpy(Info.Description, *ActionDesc(Entry));
        Info.Icon = Wnd->FindTexture(Info.IconName);
        if (!Info.Icon)
            Info.Icon = Wnd->FindTexture(TEXT("NWindow.BlackTexture"));
        const FArray& Pets = ActionPets(Entry);
        const INT* PetIDs = (const INT*)Pets.GetData();
        UBOOL bAll = PetIDs[0] == -2;
        UBOOL bFound = bAll;
        for (INT k = 0; k < Pets.Num(); k++)
            if (PetIDs[k] == Pet.UnknownA4 - 1000000)
                bFound = 1;
        if (bFound)
            AddActionItem(Grid, &Info);
    }
}
#endif

// The actions of one category of actionname-*.dat into a page's grid (C3: the pet page's loop is 0x100cc5f0, the
// summon page's 0x100ce230, the same code; the summon's also skips an entry whose first dword is 0). The entry
// (read by offset: +0x04 id, +0x08 parameter, +0x0C category, +0x10 / +0x14 the ids of the creatures it is for,
// +0x1C name, +0x28 icon texture, +0x34 description). The record of the player's pet or summon must be of PetType
// (the loop ends at the first action of the category found while it says otherwise); an action lists the creatures
// it is for (their ids offset by 1000000, or -2 for all). An icon that is not found shows black.
// (the stock code reads the table again for each field, so there is no local for the entry)
#define ACTION_ENTRY (GL2GameData.ActionTable(i))
template <class T> static FORCEINLINE void LoadActionsC3(T* Wnd, INT Category, INT PetType, UBOOL bTestValid)
{
    for (INT i = 0; i < GL2GameData.ActionTable.Num(); i++)
    {
        if (bTestValid && !ACTION_ENTRY->bValid)
            continue;
        if (nw::Field<INT>(ACTION_ENTRY, 0x0C) != Category)
            continue;
        PetInfo Pet = GConsole->GetPetInfo();
        if (Pet.ObjectID == -1)
            return;
        if (Pet.Type != PetType)
            return;
        NItemInfo Info;
        NItemInfoC4* Record = AsC4(&Info);   // (the shared NItemInfo.h has the C3 field order)
        Record->ID = ACTION_ENTRY->ActionID;
        Record->Param = nw::Field<INT>(ACTION_ENTRY, 0x08);
        appStrcpy(Record->Name, *nw::Field<FString>(ACTION_ENTRY, 0x1C));
        appStrcpy(Record->IconName[0], *nw::Field<FString>(ACTION_ENTRY, 0x28));
        appStrcpy(Record->Description, *nw::Field<FString>(ACTION_ENTRY, 0x34));
        Record->Icon[0] = Wnd->FindTexture(Record->IconName[0]);
        if (!Record->Icon[0])
            Record->Icon[0] = Wnd->FindTexture(TEXT("NWindow.BlackTexture"));
        const INT* IDs = nw::Field<INT*>(ACTION_ENTRY, 0x10);
        UBOOL bFound = IDs[0] == -2;   // (the stock code reads the first without looking at the count)
        INT NumIDs = nw::Field<INT>(ACTION_ENTRY, 0x14);
        for (INT k = 0; k < NumIDs; k++)
        {
            if (IDs[k] == Pet.UnknownA4 - 1000000)
                bFound = 1;
        }
        if (bFound)
            Wnd->Items->AddItem(&Info);   // (the grid is read where it is used)
    }
}
#undef ACTION_ENTRY

/*-----------------------------------------------------------------------------
    NCPetDetailStatusWnd.
-----------------------------------------------------------------------------*/

// 0x100deb90
NCPetDetailStatusWnd::NCPetDetailStatusWnd()
{
    BackTex = NULL;
    Pet.Name[0] = 0;
    Pet.UnknownB0 = 0;
    Pet.UnknownA8 = 0;
    Pet.ObjectID = -1;
}
NW_HOOK_CTOR(0x100deb90, NCPetDetailStatusWnd)

// 0x100dec00
NW_HOOK_DTOR(0x100dec00, NCPetDetailStatusWnd)

// stock body shared with NControl's (0x10004e90)
INT NCPetDetailStatusWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return NCWnd::WindowProc(Msg, wParam, lParam) != 0;
}

// C2 CANDIDATE: the C2 function is not the same code as the C1 one; kept only while it matches the stock one (verify_data)
// 0x100db070 (vslot 37)
INT NCPetDetailStatusWnd::OnCreate()
{
    NCWnd::OnCreate();
    BackTex = FindTexture(TEXT("L2UI_ch3.PetWnd.petinterface_back31"));
    return 1;
}
NW_HOOK_METHOD(0x100db070, NCPetDetailStatusWnd, OnCreate)

// 0x100db0a0 (vslot 38) - guarded; its own copy of the body
INT NCPetDetailStatusWnd::OnDestroy()
{
    guard(NCPetDetailStatusWnd::OnDestroy);
    NCWnd::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100db0a0, NCPetDetailStatusWnd, OnDestroy)

// 0x100db3a0 (vslot 62) - C4: one body for each page (the summon's is 0x100dd190)
INT NCPetDetailStatusWnd::OnPaint(NCGDevice* Device)
{
    guard(NCPetDetailStatusWnd::OnPaint);
    return PaintInfoPageC4(this, BackTex, Pet, Device);
    unguard;
}
NW_HOOK_METHOD(0x100db3a0, NCPetDetailStatusWnd, OnPaint)

NW_CHECK_VTABLE(NCPetDetailStatusWnd, 0x101c3b38, 74)

/*-----------------------------------------------------------------------------
    NCPetActionWnd.
-----------------------------------------------------------------------------*/

// 0x100dec50
NCPetActionWnd::NCPetActionWnd()
{
}
NW_HOOK_CTOR(0x100dec50, NCPetActionWnd)

// 0x100deca0
NW_HOOK_DTOR(0x100deca0, NCPetActionWnd)

// stock body shared with NControl's (0x10004e90)
INT NCPetActionWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return NCWnd::WindowProc(Msg, wParam, lParam) != 0;
}

// C2 CANDIDATE: the C2 function is not the same code as the C1 one; kept only while it matches the stock one (verify_data)
// 0x100de3c0 (vslot 37) - C3: the page's background and its grid of icons
INT NCPetActionWnd::OnCreate()
{
    NCWnd::OnCreate();
    BackTex = FindTexture(TEXT("L2UI_ch3.PetWnd.petinterface_back32"));
    Items = CreateActionGridC3(this);
    return 1;
}
NW_HOOK_METHOD(0x100de3c0, NCPetActionWnd, OnCreate)

// 0x100dbce0 (vslot 38) - one body for the windows whose only child is at +0x98: also
// NCSummonedActionWnd's, NCPetInventoryWnd's, NCGMActionWnd's, NCMagicSkillWnd's and
// NCTrainListWnd's
INT NCPetActionWnd::OnDestroy()
{
    DestroyChild(this, Items);
    NCWnd::OnDestroy();
    return 1;
}
NW_HOOK_METHOD(0x100dbce0, NCPetActionWnd, OnDestroy)

// 0x100dc540 (vslot 56) - the background (the summon page's is the same body: the linker folded them)
INT NCPetActionWnd::OnPaint(NCGDevice* Device)
{
    return PaintActionPage(this, BackTex, Device);
}
NW_HOOK_METHOD(0x100dc540, NCPetActionWnd, OnPaint)

// 0x100dbd90 (name ours) - the pet's actions (category 4), for a pet (record type 2)
void NCPetActionWnd::LoadActions()
{
    LoadActionsC3(this, 4, 2, 0);
}
NW_HOOK_METHOD(0x100dbd90, NCPetActionWnd, LoadActions)

NW_CHECK_VTABLE(NCPetActionWnd, 0x101c3c60, 74)

/*-----------------------------------------------------------------------------
    NCPetWnd.
-----------------------------------------------------------------------------*/

// 0x100def60 - the name button is left as allocated
NCPetWnd::NCPetWnd()
{
    BackTex = NULL;
    Pet.Name[0] = 0;
    Pet.UnknownB0 = 0;
    Pet.UnknownA8 = 0;
    Pet.ObjectID = -1;
    DetailWnd = NULL;
    ActionWnd = NULL;
    InventoryWnd = NULL;
    TabCtrl = NULL;
    bCanName = 1;
}
NW_HOOK_CTOR(0x100def60, NCPetWnd)

// 0x100deff0
NW_HOOK_DTOR(0x100deff0, NCPetWnd)

// 0x100db170 (vslot 25) - C2 has no IsEnabled check of its own
INT NCPetWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCWnd::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND && (INT)wParam == NID_PET_NAME)
        return OnName() != 0;
    return 0;
}
NW_HOOK_METHOD(0x100db170, NCPetWnd, WindowProc)

// 0x100db1d0 (vslot 56, ret 4; was a stock jump in NNewVirtuals.gen.cpp) - the title bar (the base's FrameCtrl, +0x7C) takes the
// SysString title; unlike NCWnd's (0x10034BC0) the stock has no null test: `mov ecx, [ecx + 0x7C]; jmp NCFrameCtrl::SetTitleID`
void NCPetWnd::SetFrameTitleID(INT ID)
{
    FrameCtrl->SetTitleID(ID);
}
NW_HOOK_METHOD(0x100db1d0, NCPetWnd, SetFrameTitleID)

// 0x100df040 (vslot 37) - C4: the title bar over the window's width, the bars' textures of the player's status window,
// the three pages (all hidden to hit tests: the tab control shows one) at (0, 138) and the tab control at (12, 115) with
// the three tabs, then the Name button
INT NCPetWnd::OnCreate()
{
    NCWnd::OnCreate();
    FrameCtrl = NewFrameCtrl(1, 0, 0);
    FrameCtrl->ID = NID_PET_FRAME;
    FrameCtrl->SetPosition(0, 0, (DWORD)(INT)Width, 20);
    CreateChild(FrameCtrl, NWS_VISIBLE);
    BackTex = FindTexture(TEXT("L2UI_CH3.PetWnd.Petinterface_back1"));
    BackTex2 = FindTexture(TEXT("L2UI_CH3.PetWnd.Petinterface_back2"));
    HPTex = FindTexture(TEXT("L2UI_CH3.PlayerStatusWnd.ps_HPbar"));
    MPTex = FindTexture(TEXT("L2UI_CH3.PlayerStatusWnd.ps_MPbar"));
    EXPTex = FindTexture(TEXT("L2UI_CH3.PlayerStatusWnd.ps_ExPbar"));
    WeightTex[0] = FindTexture(TEXT("L2UI_CH3.PlayerStatusWnd.ps_weightbar1"));
    WeightTex[1] = FindTexture(TEXT("L2UI_CH3.PlayerStatusWnd.ps_weightbar2"));
    WeightTex[2] = FindTexture(TEXT("L2UI_CH3.PlayerStatusWnd.ps_weightbar3"));
    WeightTex[3] = FindTexture(TEXT("L2UI_CH3.PlayerStatusWnd.ps_weightbar4"));
    FedTex = FindTexture(TEXT("L2UI_CH3.PlayerStatusWnd.ps_foodbar"));

    DetailWnd = NewWindowOf<NCPetDetailStatusWnd>(0x100DEB90);
    DetailWnd->SetPosition(0, 138, 256, 91);
    CreateChild(DetailWnd, NWS_NOHITTEST);
    ActionWnd = NewWindowOf<NCPetActionWnd>(0x100DEC50);
    ActionWnd->SetPosition(0, 138, 256, 91);
    CreateChild(ActionWnd, NWS_NOHITTEST);
    InventoryWnd = NewWindowOf<NCPetInventoryWnd>(0x100DEDF0);
    InventoryWnd->SetPosition(0, 138, 256, 91);
    CreateChild(InventoryWnd, NWS_NOHITTEST);

    TabCtrl = NewTabControl();
    TabCtrl->ID = NID_PET_TABS;
    TabCtrl->SetPosition(12, 115, 224, 22);
    CreateChild(TabCtrl, NWS_VISIBLE);
    AddTabC4(TabCtrl, 0x1EB, DetailWnd, 74, 23, TEXT("L2UI_Ch3.PetWnd.petinterface_tab2"),
             TEXT("L2UI_Ch3.PetWnd.petinterface_tab1"), 1, -1, 0);   // "Information"
    AddTabC4(TabCtrl, 0x1EC, ActionWnd, 74, 23, TEXT("L2UI_Ch3.PetWnd.petinterface_tab2"),
             TEXT("L2UI_Ch3.PetWnd.petinterface_tab1"), 1, -1, 0);  // "Action"
    AddTabC4(TabCtrl, 0x1ED, InventoryWnd, 74, 23, TEXT("L2UI_Ch3.PetWnd.petinterface_tab2"),
             TEXT("L2UI_Ch3.PetWnd.petinterface_tab1"), 0, -1, 0);  // "Items"

    NameButton = NewButton(TEXT("L2UI_Ch3.BUTTON.btn1_normal"), TEXT("L2UI_Ch3.BUTTON.btn1_normalon"), 0);
    NameButton->ID = NID_PET_NAME;
    NameButton->SetPosition(167, 28, 76, 23);
    CreateChild(NameButton, NWS_VISIBLE);
    NameButton->SetTextID(0x1F1);                                       // "Name"
    return 1;
}
NW_HOOK_METHOD(0x100df040, NCPetWnd, OnCreate)

// 0x100dc620 (vslot 38) - C2 guards it
INT NCPetWnd::OnDestroy()
{
    guard(NCPetWnd::OnDestroy);
    DestroyChild(this, DetailWnd);
    DestroyChild(this, ActionWnd);
    DestroyChild(this, InventoryWnd);
    DestroyChild(this, TabCtrl);
    DestroyChild(this, NameButton);
    NCWnd::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100dc620, NCPetWnd, OnDestroy)

// 0x100dc770 (vslot 62) - C4: the two background parts, the record read again, then the bars: HP, MP (85 pixels wide at
// most; the over-full branch of the MP bar draws a 10-pixel strip as the stock code does), food and exp (206), weight (85,
// the texture by the percentage: up to 50, 66.66, 80, above) and their texts (exp and food in percent, "now / max" for HP
// and MP, the SP, the weight percent), then "Lv" and "<level> <name>". The percentages are FLOAT here (C3: DOUBLE).
INT NCPetWnd::OnPaint(NCGDevice* Device)
{
    guard(NCPetWnd::OnPaint);
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    if (BackTex)
        Device->DrawTexture(0, 20, 256, 95, 0, 0, 256, 95, BackTex, 0xFF, 1);
    if (BackTex2)
        Device->DrawTexture(0, 115, 256, 23, 0, 0, 256, 23, BackTex2, 0xFF, 1);
    Pet = GConsole->GetPetInfo();
    INT HPW = 0;
    INT MPW = 0;
    INT ExpW = 0;
    INT WeightW = 0;
    INT FedW = 0;
    if (Pet.MaxHP)
        HPW = Pet.HP * 85 / Pet.MaxHP;
    if (Pet.MaxMP)
        MPW = Pet.MP * 85 / Pet.MaxMP;
    if (Pet.MaxLoad)
        WeightW = Pet.Load * 85 / Pet.MaxLoad;
    if (Pet.MaxFed)
        FedW = Pet.Fed * 206 / Pet.MaxFed;
    if (Pet.NextExp)
        ExpW = (INT)((FLOAT)(Pet.Exp - Pet.LevelExp) * 206.0f / (Pet.NextExp - Pet.LevelExp));
    if (HPW > 0 && HPW <= 85)
        Device->DrawTexture(35, 51, HPW, 12, 0, 0, 8, 12, HPTex, 0xFF, 1);
    else if (HPW > 85)
        Device->DrawTexture(35, 51, 85, 12, 0, 0, 8, 12, HPTex, 0xFF, 1);
    if (MPW > 0 && MPW <= 85)
        Device->DrawTexture(35, 65, MPW, 12, 0, 0, 8, 12, MPTex, 0xFF, 1);
    else if (MPW > 85)
        Device->DrawTexture(35, 65, 85, 10, 0, 0, 84, 10, MPTex, 0xFF, 1);
    if (FedW > 0 && FedW <= 206)
        Device->DrawTexture(35, 93, FedW, 12, 0, 0, 8, 12, FedTex, 0xFF, 1);
    else if (FedW > 206)
        Device->DrawTexture(35, 93, 206, 10, 0, 0, 8, 10, FedTex, 0xFF, 1);
    if (ExpW > 0 && ExpW <= 206)
        Device->DrawTexture(35, 79, ExpW, 12, 0, 0, ExpW, 12, EXPTex, 0xFF, 1);
    else if (ExpW > 206)
        Device->DrawTexture(35, 79, 206, 12, 0, 0, 8, 12, EXPTex, 0xFF, 1);
    FLOAT WeightPct = 0.0f;
    if (WeightW > 0)
    {
        WeightPct = (FLOAT)Pet.Load / Pet.MaxLoad * 100.0f;
        UTexture* Tex = NULL;
        if (WeightPct <= 50.0f)
            Tex = WeightTex[0];
        else if (WeightPct <= 66.66)
            Tex = WeightTex[1];
        else if (WeightPct <= 80.0f)
            Tex = WeightTex[2];
        else if (WeightPct > 80.0f)
            Tex = WeightTex[3];
        if (Tex)
            Device->DrawTexture(156, 51, WeightW, 12, 0, 0, 8, 12, Tex, 0xFF, 1);
    }
    TCHAR Buf[64];
    SIZE Size;
    _swprintf(Buf, TEXT("%.2f%s"), (DOUBLE)((FLOAT)Pet.Fed * 100.0f / Pet.MaxFed), TEXT("%"));
    NCGDevice::GetTextExtent(&Size, Buf, 0, 0, 0, 0, 0);
    DrawDigit(Device, 1, 138 - Size.cx / 2, 95, 0xFFB4B4B4, Buf);
    _swprintf(Buf, TEXT("%.2f%s"), (DOUBLE)((FLOAT)(Pet.Exp - Pet.LevelExp) * 100.0f / (Pet.NextExp - Pet.LevelExp)),
              TEXT("%"));
    NCGDevice::GetTextExtent(&Size, Buf, 0, 0, 0, 0, 0);
    DrawDigit(Device, 1, 138 - Size.cx / 2, 81, 0xFFB4B4B4, Buf);
    DrawValueRowC3(Device, Buf, 53, Pet.HP, Pet.MaxHP);
    DrawValueRowC3(Device, Buf, 67, Pet.MP, Pet.MaxMP);
    appSprintf(Buf, TEXT("%d"), Pet.SP);
    NCGDevice::GetTextExtent(&Size, Buf, 0, 0, 0, 0, 0);
    DrawString(Device, 198 - Size.cx / 2, 66, 0xFFB09B79, Buf);
    _swprintf(Buf, TEXT("%.2f%s"), (DOUBLE)WeightPct, TEXT("%"));
    NCGDevice::GetTextExtent(&Size, Buf, 0, 0, 0, 0, 0);
    DrawDigit(Device, 1, 198 - Size.cx / 2, 53, 0xFFDCDCDC, Buf);
    TCHAR LevelText[64];
    DrawString(Device, 15, 35, 0xFFA3A3A3, *GL2GameData.SysString[88]);   // "Lv"
    const TCHAR* Name = Pet.Name;
    appSprintf(LevelText, TEXT("%d %s"), Pet.Level, Name && *Name ? Name : NULL);
    DrawString(Device, 33, 35, 0xFFB09B79, LevelText);
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100dc770, NCPetWnd, OnPaint)

// 0x100dd030 (vslot 48) - the tab control shows its page, the Name button only while the pet can be named, the window
// back from its icon at the icon's place, the action page's icons listed again for this pet
void NCPetWnd::ShowWindow()
{
    NCWnd::ShowWindow();
    TabCtrl->ShowCurrentPage();
    if (bCanName)
        NameButton->ShowWindow();
    else
        NameButton->HideWindow();
    RestoreFromIcon(this);
    if (ActionWnd)
    {
        ActionWnd->Items->ClearItems();   // 0x100505A0: the grid's icons dropped (as NCSummonedWnd::ClearActions)
        ActionWnd->LoadActions();
    }
}
NW_HOOK_METHOD(0x100dd030, NCPetWnd, ShowWindow)

// 0x100db1e0 (vslot 54) - minimizing: the window's own icon picture (text 506), the icon moved back on the screen when
// the window was partly left of it, then the icon takes the focus
void NCPetWnd::Minimize()
{
    if (Icon)
        Icon->SetIcon(TEXT("L2UI_CH3.PetWnd.PetIcon"), 0x1FA, 1);
    if (ScreenX < 0 && Icon)
        Icon->OffsetWindow(-ScreenX, 0);
    HideWindow();
    if (Icon)
    {
        Icon->ShowWindow();
        Icon->SetFocus();
    }
}
NW_HOOK_METHOD(0x100db1e0, NCPetWnd, Minimize)

// 0x100dd0f0 (vslot 74) - asks for the pet's name (systemmsg 535); the dialog box sends it (dialog id 0x39)
INT NCPetWnd::OnName()
{
    NDlg Info;
    Info.ID = 0x39;
    Info.bMainBox = 1;
    Info.bInput = 1;
    Info.Type = 0;
    Info.Owner = Entry;
    Info.SetSystemMessage(0x217);
    GConsole->ShowDialog(&Info);
    return 1;
}
NW_HOOK_METHOD(0x100dd0f0, NCPetWnd, OnName)

// 0x100db240 - once named, the button goes away
void NCPetWnd::SetCanName(UBOOL bCan)
{
    bCanName = bCan;
    if (!bCan)
        NameButton->HideWindow();
}
NW_HOOK_METHOD(0x100db240, NCPetWnd, SetCanName)

// 0x100db260 - the Items page's list emptied: the stock body jumps to the page's slot 72, the shared forwarder 0x100fcc30,
// which calls slot 78 of the page's grid; the grid's Clear is called here
void NCPetWnd::ClearInventory()
{
    InventoryWnd->Items->Clear();
}
NW_HOOK_METHOD(0x100db260, NCPetWnd, ClearInventory)

// 0x100de9a0 (C1 0x100940b0) - hands the item to the pet inventory page (+0x170), returns 1
UBOOL NCPetWnd::AddInventoryItem(NItemInfo* Info)
{
    InventoryWnd->AddItem(Info);
    return 1;
}
NW_HOOK_METHOD(0x100de9a0, NCPetWnd, AddInventoryItem)

// 0x100deb70 (C1 0x100940d0) - the same for an update (Op, Info)
UBOOL NCPetWnd::UpdateInventoryItem(INT Op, NItemInfo* Info)
{
    InventoryWnd->UpdateItem(Op, Info);
    return 1;
}
NW_HOOK_METHOD(0x100deb70, NCPetWnd, UpdateInventoryItem)

NW_CHECK_VTABLE(NCPetWnd, 0x101c4040, 75)

/*-----------------------------------------------------------------------------
    NCSummonedDetailStatusWnd.
-----------------------------------------------------------------------------*/

// 0x100df560
NCSummonedDetailStatusWnd::NCSummonedDetailStatusWnd()
{
    BackTex = NULL;
    Summon.Name[0] = 0;
    Summon.UnknownB0 = 0;
    Summon.UnknownA8 = 0;
    Summon.ObjectID = -1;
}
NW_HOOK_CTOR(0x100df560, NCSummonedDetailStatusWnd)

// 0x100df5d0
NW_HOOK_DTOR(0x100df5d0, NCSummonedDetailStatusWnd)

// stock body shared with NControl's (0x10004e90)
INT NCSummonedDetailStatusWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return NCWnd::WindowProc(Msg, wParam, lParam) != 0;
}

// C2 CANDIDATE: the C2 function is not the same code as the C1 one; kept only while it matches the stock one (verify_data)
// 0x100db270 (vslot 37)
INT NCSummonedDetailStatusWnd::OnCreate()
{
    NCWnd::OnCreate();
    BackTex = FindTexture(TEXT("L2UI_ch3.summonwnd.summon_back31"));
    return 1;
}
NW_HOOK_METHOD(0x100db270, NCSummonedDetailStatusWnd, OnCreate)

// 0x100db2a0 (vslot 38) - guarded; the same body as NCPetDetailStatusWnd's, its own copy
INT NCSummonedDetailStatusWnd::OnDestroy()
{
    guard(NCSummonedDetailStatusWnd::OnDestroy);
    NCWnd::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100db2a0, NCSummonedDetailStatusWnd, OnDestroy)

// 0x100dd190 (vslot 62) - the same body
INT NCSummonedDetailStatusWnd::OnPaint(NCGDevice* Device)
{
    guard(NCSummonedDetailStatusWnd::OnPaint);
    return PaintInfoPageC4(this, BackTex, Summon, Device);
    unguard;
}
NW_HOOK_METHOD(0x100dd190, NCSummonedDetailStatusWnd, OnPaint)

NW_CHECK_VTABLE(NCSummonedDetailStatusWnd, 0x101c4560, 74)

/*-----------------------------------------------------------------------------
    NCSummonedActionWnd.
-----------------------------------------------------------------------------*/

// 0x100df620
NCSummonedActionWnd::NCSummonedActionWnd()
{
}
NW_HOOK_CTOR(0x100df620, NCSummonedActionWnd)

// 0x100df670
NW_HOOK_DTOR(0x100df670, NCSummonedActionWnd)

// stock body shared with NControl's (0x10004e90)
INT NCSummonedActionWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return NCWnd::WindowProc(Msg, wParam, lParam) != 0;
}

// C2 CANDIDATE: the C2 function is not the same code as the C1 one; kept only while it matches the stock one (verify_data)
// 0x100de9c0 (vslot 37) - C3: the same page as the pet's with its own background
INT NCSummonedActionWnd::OnCreate()
{
    NCWnd::OnCreate();
    BackTex = FindTexture(TEXT("L2UI_ch3.summonwnd.summon_back32"));
    Items = CreateActionGridC3(this);
    return 1;
}
NW_HOOK_METHOD(0x100de9c0, NCSummonedActionWnd, OnCreate)

// 0x100ddad0 (vslot 38) - the grid goes, then NCWnd's (same body as NCPetActionWnd's, its own copy)
INT NCSummonedActionWnd::OnDestroy()
{
    guard(NCSummonedActionWnd::OnDestroy);
    DestroyChild(this, Items);
    NCWnd::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100ddad0, NCSummonedActionWnd, OnDestroy)

// stock body shared with NCPetActionWnd's (0x10091680)
INT NCSummonedActionWnd::OnPaint(NCGDevice* Device)
{
    return PaintActionPage(this, BackTex, Device);
}

// 0x100ddb80 (name ours) - the summon's actions (category 5), for a summon (record type 1)
void NCSummonedActionWnd::LoadActions()
{
    LoadActionsC3(this, 5, 1, 1);
}
NW_HOOK_METHOD(0x100ddb80, NCSummonedActionWnd, LoadActions)

NW_CHECK_VTABLE(NCSummonedActionWnd, 0x101c4688, 74)

/*-----------------------------------------------------------------------------
    NCSummonedWnd.
-----------------------------------------------------------------------------*/

// 0x100df6c0
NCSummonedWnd::NCSummonedWnd()
{
    BackTex = NULL;
    DetailWnd = NULL;
    ActionWnd = NULL;
    TabCtrl = NULL;
    Summon.Name[0] = 0;
    Summon.UnknownB0 = 0;
    Summon.UnknownA8 = 0;
    Summon.ObjectID = -1;
}
NW_HOOK_CTOR(0x100df6c0, NCSummonedWnd)

// 0x100df740
NW_HOOK_DTOR(0x100df740, NCSummonedWnd)

// stock body shared with NControl's (0x10004e90)
INT NCSummonedWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return NCWnd::WindowProc(Msg, wParam, lParam) != 0;
}

// 0x100df790 (vslot 37) - C4: the title bar, the two background parts and the bars' textures of the player's status window,
// the Information and Action pages at (0, 110) and the tab control at (12, 87) with its two tabs
INT NCSummonedWnd::OnCreate()
{
    NCWnd::OnCreate();
    FrameCtrl = NewFrameCtrl(1, 0, 0);
    FrameCtrl->ID = NID_SUMMON_FRAME;
    FrameCtrl->SetPosition(0, 0, (DWORD)(INT)Width, 20);
    CreateChild(FrameCtrl, NWS_VISIBLE);
    BackTex = FindTexture(TEXT("L2UI_CH3.SummonWnd.summon_back1"));
    BackTex2 = FindTexture(TEXT("L2UI_CH3.SummonWnd.summon_back2"));
    HPTex = FindTexture(TEXT("L2UI_CH3.PlayerStatusWnd.ps_HPbar"));
    MPTex = FindTexture(TEXT("L2UI_CH3.PlayerStatusWnd.ps_MPbar"));

    DetailWnd = NewWindowOf<NCSummonedDetailStatusWnd>(0x100DF560);
    DetailWnd->SetPosition(0, 110, 256, 91);
    CreateChild(DetailWnd, NWS_NOHITTEST);
    ActionWnd = NewWindowOf<NCSummonedActionWnd>(0x100DF620);
    ActionWnd->SetPosition(0, 110, 256, 91);
    CreateChild(ActionWnd, NWS_NOHITTEST);

    TabCtrl = NewTabControl();
    TabCtrl->ID = NID_PET_TABS;
    TabCtrl->SetPosition(12, 87, 224, 22);
    CreateChild(TabCtrl, NWS_VISIBLE);
    AddTabC4(TabCtrl, 0x1EB, DetailWnd, 74, 23, TEXT("L2UI_Ch3.SummonWnd.summon_tab2"),
             TEXT("L2UI_Ch3.SummonWnd.summon_tab1"), 1, -1, 0);     // "Information"
    AddTabC4(TabCtrl, 0x1EC, ActionWnd, 74, 23, TEXT("L2UI_Ch3.SummonWnd.summon_tab2"),
             TEXT("L2UI_Ch3.SummonWnd.summon_tab1"), 0, -1, 0);     // "Action"
    return 1;
}
NW_HOOK_METHOD(0x100df790, NCSummonedWnd, OnCreate)

// 0x100db320 (vslot 54) - minimizing: the window's own icon picture (text 505), see NCPetWnd::Minimize
void NCSummonedWnd::Minimize()
{
    if (Icon)
        Icon->SetIcon(TEXT("L2UI_CH3.SummonWnd.SummonIcon"), 0x1F9, 1);
    if (ScreenX < 0 && Icon)
        Icon->OffsetWindow(-ScreenX, 0);
    HideWindow();
    if (Icon)
    {
        Icon->ShowWindow();
        Icon->SetFocus();
    }
}
NW_HOOK_METHOD(0x100db320, NCSummonedWnd, Minimize)

// 0x100ddd50 (vslot 38) - C2 guards it
INT NCSummonedWnd::OnDestroy()
{
    guard(NCSummonedWnd::OnDestroy);
    DestroyChild(this, DetailWnd);
    DestroyChild(this, ActionWnd);
    DestroyChild(this, TabCtrl);
    NCWnd::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100ddd50, NCSummonedWnd, OnDestroy)

// 0x100dde50 (vslot 62) - C4: two background parts, the bars 85 pixels wide (the over-full branch of the MP bar draws a
// 10-pixel strip as the stock code does), the HP and MP rows, then "Lv" and the "<level> <name>"
INT NCSummonedWnd::OnPaint(NCGDevice* Device)
{
    guard(NCSummonedWnd::OnPaint);
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    if (BackTex)
        Device->DrawTexture(0, 20, 256, 67, 0, 0, 256, 67, BackTex, 0xFF, 1);
    if (BackTex2)
        Device->DrawTexture(0, 87, 256, 23, 0, 0, 256, 23, BackTex2, 0xFF, 1);
    Summon = GConsole->GetPetInfo();
    INT HPW = 0;
    INT MPW = 0;
    if (Summon.MaxHP)
        HPW = Summon.HP * 85 / Summon.MaxHP;
    if (Summon.MaxMP)
        MPW = Summon.MP * 85 / Summon.MaxMP;
    if (HPW > 0 && HPW <= 85)
        Device->DrawTexture(35, 51, HPW, 12, 0, 0, 8, 12, HPTex, 0xFF, 1);
    else if (HPW > 85)
        Device->DrawTexture(35, 51, 85, 12, 0, 0, 8, 12, HPTex, 0xFF, 1);
    if (MPW > 0 && MPW <= 85)
        Device->DrawTexture(35, 65, MPW, 12, 0, 0, 8, 12, MPTex, 0xFF, 1);
    else if (MPW > 85)
        Device->DrawTexture(35, 65, 85, 10, 0, 0, 84, 10, MPTex, 0xFF, 1);
    TCHAR Buf[64];
    DrawValueRowC3(Device, Buf, 53, Summon.HP, Summon.MaxHP);
    DrawValueRowC3(Device, Buf, 67, Summon.MP, Summon.MaxMP);
    TCHAR LevelText[64];
    DrawString(Device, 15, 35, 0xFFA3A3A3, *GL2GameData.SysString[88]);   // "Lv"
    const TCHAR* Name = Summon.Name;
    appSprintf(LevelText, TEXT("%d %s"), Summon.Level, Name && *Name ? Name : NULL);
    DrawString(Device, 33, 35, 0xFFB09B79, LevelText);
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100dde50, NCSummonedWnd, OnPaint)

// 0x100de320 (vslot 48) - the tab control shows its page, the window back from its icon, the action page's icons listed
// again for this summon
void NCSummonedWnd::ShowWindow()
{
    NCWnd::ShowWindow();
    TabCtrl->ShowCurrentPage();
    RestoreFromIcon(this);
    if (ActionWnd)
    {
        ClearActions();
        ActionWnd->LoadActions();
    }
}
NW_HOOK_METHOD(0x100de320, NCSummonedWnd, ShowWindow)

// 0x100de3b0 (vslot 49) - the action page's icons are dropped with the window
void NCSummonedWnd::HideWindow()
{
    NCWnd::HideWindow();
    ClearActions();
}
NW_HOOK_METHOD(0x100de3b0, NCSummonedWnd, HideWindow)

// 0x100db380 (name ours) - the grid's own list of icons (0x1004ab00, a copy of the body other grids keep)
__declspec(noinline) void NCSummonedWnd::ClearActions()
{
    if (ActionWnd)
        ActionWnd->Items->ClearItems();   // 0x100505A0
}
NW_HOOK_METHOD(0x100db380, NCSummonedWnd, ClearActions)

NW_CHECK_VTABLE(NCSummonedWnd, 0x101c47b0, 74)
