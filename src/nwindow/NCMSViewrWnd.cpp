/*=============================================================================
    NCMSViewrWnd.cpp - the skill viewer, its character selection and skill grids. Stock (C3):
    0x10097bc0 .. 0x1009a3c0 and 0x1009adcc (the selection window's OnDestroy is NCTrainWnd's
    body, the grids' WindowProc NCNpcView_InvenItemWnd's: unhooked copies here; the
    TArray<AActor*>::AddItem instance 0x1011b200 is left out).
=============================================================================*/
#include "nwindow/NCMSViewrWnd.h"
#include "nwindow/NCButton.h"
#include "nwindow/NCCheckBox.h"
#include "nwindow/NCComboBox.h"
#include "nwindow/NCEditBox.h"
#include "nwindow/NCFrameCtrl.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NCTabControl.h"
#include "nwindow/NItemInfo.h"
#include "nwindow/NString.h"

#include <stdio.h>
#include <string.h>
#include <wchar.h>

// An object made as the stock code makes it: StaticAllocateObject of its class, then its constructor. Ctor is the stock
// constructor's address (tools/port/check_stockargs.py compares the argument count with its `ret N`). The hook build calls
// that constructor; the standalone has no stock code, the constructor that runs is T's own (every one used here is hooked at
// that address with the same arguments)
template <class T, class... A> static FORCEINLINE T* NewStock(DWORD Ctor, A... a)
{
#if NW_HOOK_MODE
    void* Mem = T::operator new(sizeof(T));
    return Mem ? ::nw::StockCallAt<T*>(Ctor, Mem, a...) : NULL;
#else
    (void)Ctor;
    return new T(a...);
#endif
}

// 0x100a5a00 - TArray<AActor*>::AddItem's instance shared by the viewers (thiscall on the array, the address of the element, ret 4):
// one slot added, the pointer put in it. A stand-in class carries the hook (the caller passes the array as `this`); the
// viewers add to the pawn's skill targets by a call to it, not by the array's inlined body (declared in NCMSViewrWnd.h)
__declspec(noinline) void NActorArrayAdd::AddItem(AActor** Item)
{
    FArray* Array = reinterpret_cast<FArray*>(this);
    INT Index = Array->Add(1, 4);
    ((AActor**)Array->GetData())[Index] = *Item;
}
NW_HOOK_METHOD(0x100a5a00, NActorArrayAdd, AddItem)

static FORCEINLINE void AddMagicTarget(TArray<AActor*>* Targets, AActor** Target)
{
    reinterpret_cast<NActorArrayAdd*>(Targets)->AddItem(Target);
}

// the fields of an item window's entry this client's code reads: the header's NItemEntry is C3's, where the ID is 0x208
// bytes closer to the start (0x1918)
static FORCEINLINE INT& EntryID(NItemEntry* Entry) { return *(INT*)((BYTE*)Entry + 0x1B20); }

// UObject::StaticLoadClass flags (UE2)
enum { LOAD_NoFail = 0x1 };

// sysstring texts (names ours): the selection window's labels, the search's tabs and check
// boxes, "search"
enum
{
    STR_MSViewTitle = 157,
    STR_MSViewLabel1 = 50,
    STR_MSViewLabel2 = 88,
    STR_MSViewLabel3 = 90,
    STR_MSViewLabel4 = 91,
    STR_MSViewLabel5 = 92,
    STR_MSViewLabel6 = 101,
    STR_MSViewRace0 = 170,               // the character names: the races (170 .. 174),
    STR_MSViewFighter = 175,             //   then fighter or mystic, male or female
    STR_MSViewMale = 177,
    STR_MSViewActive = 120,
    STR_MSViewPassive = 121,
    STR_MSViewMagic = 122,
    STR_MSViewSearch = 420,
};

// The characters the viewer offers (race, sex, class id; 18 entries at 0x10218270 for the 18
// names of AddAllCharacter, which clears 19: the last one is whatever follows the table).
struct NMSViewCharacter
{
    INT Race;
    INT Sex;
    INT ClassID;
};
#if NW_HOOK_MODE
#define GMSViewCharacters (reinterpret_cast<NMSViewCharacter*>(::nw::Addr(0x10255608)))
#else
extern NMSViewCharacter GMSViewCharacters[];
#endif

// A level of a skill as this client lays it out: the engine header's FL2MagicSkillData is C2's, where these fields
// sit elsewhere. The offsets are the viewers' own reads (NConsoleWndData.cpp has the same view for the getters).
struct NSkillDataC3
{
    FString Name;                       // +0x00
    FString Description;                // +0x0C
    INT SkillID;                        // +0x18
    INT Level;                          // +0x1C
    INT OperateType;                    // +0x20 2: passive, 3: toggle, else active
    BYTE _Unknown24[0x30 - 0x24];
    INT bInstant;                       // +0x30 (name ours; C3 +0x2C) set: the skill is cast with the magic info path
    FLOAT HitTime;                      // +0x34 (C3 +0x30)
    BYTE _Unknown38[0x3C - 0x38];
    INT bMagic;                         // +0x3C shown as "Magic" instead (C3: +0x38)
    BYTE _Unknown40[0x44 - 0x40];
    L2FName EffectName;                 // +0x44 (C3 +0x40) the script's skill visual effect; "none" when there is none
};
NW_ASSERT_OFFSET(NSkillDataC3, SkillID, 0x18);
NW_ASSERT_OFFSET(NSkillDataC3, OperateType, 0x20);
NW_ASSERT_OFFSET(NSkillDataC3, bInstant, 0x30);
NW_ASSERT_OFFSET(NSkillDataC3, HitTime, 0x34);
NW_ASSERT_OFFSET(NSkillDataC3, bMagic, 0x3C);
NW_ASSERT_OFFSET(NSkillDataC3, EffectName, 0x44);

// the index of an L2FName (0: no name; the type keeps it private)
static FORCEINLINE INT NameIndex(const L2FName& Name)
{
    return *(const INT*)&Name;
}

// a dword of an NItemInfo by offset (this client moved the fields after +0x178C up by 4: NItemInfo.h only grew at
// the end)
static FORCEINLINE INT& InfoInt(NItemInfo* Info, DWORD Offset)
{
    return *(INT*)((BYTE*)Info + Offset);
}

// NCItemWnd::Clear (slot 78 of this client)
static FORCEINLINE void ClearGrid(NCMSView_InvenItemWnd* Grid)
{
    Grid->Clear();
}

// The text of line Index of a combo box (0x10005B60 on the line; the lines are 12 bytes at +0x1FC: a game string id or an
// NString)
static FORCEINLINE TCHAR* ComboItemText(NCComboBox* Combo, INT Index)
{
    BYTE* Line = *(BYTE**)((BYTE*)Combo + 0x1FC) + Index * 12;
    return const_cast<TCHAR*>(reinterpret_cast<NComboItem*>(Line)->GetText());   // 0x10005B60
}

// NCComboBox::ClearItems (0x100067D0) / AddItem(const TCHAR*) (0x100068B0) of this client
static FORCEINLINE void ComboClear(NCComboBox* Combo)
{
    Combo->ClearItems();
}
static FORCEINLINE void ComboAdd(NCComboBox* Combo, const TCHAR* Text)
{
    Combo->AddItem(Text);
}

template <class T> static FORCEINLINE void DestroyChild(NCWnd* Parent, T*& Child)
{
    if (Child)
    {
        Parent->RemoveChildWindow(Child->Entry);
        delete Child;
        Child = NULL;
    }
}

/*-----------------------------------------------------------------------------
    NCMSView_InvenItemWnd.
-----------------------------------------------------------------------------*/

// 0x100a6700 - NCInvenItemWnd's default constructor (0x10099790; ours is a forwarder that would run NCItemWnd's constructor
// first, so it is called as the stock function and not as a C++ base), then the class' vtable, and `this` back. The C++
// constructor stays a forwarder to this function; the body is hooked on a stand-in that has no C++ base. The standalone
// has no stock functions to call: the constructor is the C++ one (NCInvenItemWnd's default constructor, then our vtable)
#if NW_HOOK_MODE
struct NMSGridConstruct
{
    void* Construct()
    {
        ::nw::StockCallAt<void>(0x10099790, this);
        *(void**)this = ::nw::Addr(0x101B9B00);
        return this;
    }
};
NW_HOOK_METHOD(0x100a6700, NMSGridConstruct, Construct)

NCMSView_InvenItemWnd::NCMSView_InvenItemWnd()
{
    ::nw::StockCallAt<void>(0x100a6700, this);
}
#else
NCMSView_InvenItemWnd::NCMSView_InvenItemWnd() : NCInvenItemWnd()
{
}
#endif

// 0x100a67a0 (ret 0x10) - the 4-argument constructor (columns, visible rows, max items, cell size): the grid of six
// arguments with both gaps 1; the skill viewer builds its four grids with it
NCMSView_InvenItemWnd::NCMSView_InvenItemWnd(INT InColumns, INT InVisibleRows, INT InMaxItems, INT InCellSize)
    : NCInvenItemWnd(InColumns, InVisibleRows, InMaxItems, InCellSize, 1, 1)
{
}
NW_HOOK_CTOR_ARGS(0x100a67a0, NCMSView_InvenItemWnd, (INT A, INT B, INT C, INT D), (A, B, C, D))

// stock body shared with the NPC viewer's and the pawn viewer's grids (0x10097bc0, hooked in NCNpcViewrWnd.cpp); C3
// no longer refuses the messages of a disabled window here
INT NCMSView_InvenItemWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCInvenItemWnd::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == WM_LBUTTONDBLCLK)
        return OnLButtonDblClick(wParam, lParam) != 0;
    return 0;
}

// 0x100a41e0 (vslot 67) - only the item under the mouse
INT NCMSView_InvenItemWnd::OnMouseMove(WPARAM wParam, LPARAM lParam)
{
    guard(NCMSView_InvenItemWnd::OnMouseMove);
    HoverIndex = HitTest(LOWORD(lParam), HIWORD(lParam));
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100a41e0, NCMSView_InvenItemWnd, OnMouseMove)

// 0x100a5a20 (vslot 70) - with a character picked (0 .. 19), the skill under the mouse (level 1) is cast by the
// player's pawn at the controller's target (the pawn itself without one). A skill with its flag at +0x2C goes the
// magic info way: the pawn's controller stops moving, the action is stopped, the magic info is set (hit time at
// +0x30), the target added to the pawn's list, and when the skill process starts the pawn's ActionType is 5 and its
// action target the target. Without the flag the skill visual effect named at +0x40 (found, or loaded with the flags
// 0x2000) is triggered at the target; no name, or none loaded: the pawn spawns its own transient effect for the
// skill
INT NCMSView_InvenItemWnd::OnRButtonDown(WPARAM wParam, LPARAM lParam)
{
    guard(NCMSView_InvenItemWnd::OnRButtonDown);
    Selected = HitTest(LOWORD(lParam), HIWORD(lParam));
    if (Selected < 0)
        return 1;
    if (Selected >= Items.Num())
        return 1;
    INT Character = MSViewer()->CharSelectWnd->Selected;
    if (Character < 0)
        return 1;
    if (Character > 19)
        return 1;
    INT SkillID = EntryID(Items(Selected));
    NSkillDataC3* Data = (NSkillDataC3*)GL2GameData.GetMSData(SkillID, 1);
    APlayerController* Controller = ViewActor();
    AActor* Target = Controller->Target;
    APawn* Pawn = Controller->Pawn;
    if (!Data)
        return 1;
    if (!Target)
        Target = Pawn;
    if (Data->bInstant)
    {
        Pawn->Controller->StopMove();
        Pawn->NActionStop();
        Pawn->SetMagicInfo(SkillID, 1, Data->HitTime, Data->HitTime, Target, 1.0f, (FL2MagicSkillData*)Data);
        AddMagicTarget(&Pawn->MagicTargets, &Target);
        *(INT*)((BYTE*)Pawn + 0x500) = 1;
        if (!Pawn->InitSkillProcess())
            return 1;
        Pawn->ActionType = 5;
        Pawn->ActionTarget = Target;
        return 1;
    }
    FName None(TEXT("none"), FNAME_Add);
    if (NameIndex(Data->EffectName) != *(INT*)&None)
    {
        USkillVisualEffect* Effect = Cast<USkillVisualEffect>(
            UObject::StaticFindObject(reinterpret_cast<UClass*>(USkillVisualEffect::StaticClass()), ANY_PACKAGE,
                                      *Data->EffectName, 0));
        if (!Effect)
        {
            Effect = Cast<USkillVisualEffect>(
                UObject::StaticLoadObject(reinterpret_cast<UClass*>(USkillVisualEffect::StaticClass()), NULL,
                                          *Data->EffectName, NULL, 0x2000, NULL));
            if (!Effect)
            {
                Pawn->SpawnNTransientEffect(CastToPawn(Target), SkillID);
                return 1;
            }
        }
        Effect->TriggerTransientEffect(Pawn, Target, SkillID);
    }
    else
        Pawn->SpawnNTransientEffect(CastToPawn(Target), SkillID);
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100a5a20, NCMSView_InvenItemWnd, OnRButtonDown)

// 0x100a4280 (vslot 81) - as the right button
INT NCMSView_InvenItemWnd::OnLButtonDblClick(WPARAM wParam, LPARAM lParam)
{
    guard(NCMSView_InvenItemWnd::OnLButtonDblClick);
    bDragging = 0;
    Selected = HitTest(LOWORD(lParam), HIWORD(lParam));
    UnselectAll();
    OnRButtonDown(wParam, lParam);
    bDragging = 0;
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100a4280, NCMSView_InvenItemWnd, OnLButtonDblClick)

/*-----------------------------------------------------------------------------
    NCMSView_CharacterSelectWnd.
-----------------------------------------------------------------------------*/

// 0x100a68f0 - the textures are looked up in OnCreate; no character shown
NCMSView_CharacterSelectWnd::NCMSView_CharacterSelectWnd()
{
    TitleTex = NULL;
    BackTex = NULL;
    Unknown67Tex = NULL;
    Unknown68Tex = NULL;
    Unknown69Tex = NULL;
    UnknownB0 = 0;
    Selected = -1;
}
NW_HOOK_CTOR(0x100a68f0, NCMSView_CharacterSelectWnd)

// 0x100a4e00 (vslot 25) - C3 no longer refuses the messages of a disabled window here
INT NCMSView_CharacterSelectWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCWnd::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND)
    {
        if (wParam == NID_MSVIEW_SELECT_NAME)
            return OnNameComboBox(lParam) != 0;
        if (wParam == NID_MSVIEW_SELECT_ANIM)
            return OnAniNameComboBox(lParam) != 0;
    }
    return 0;
}
NW_HOOK_METHOD(0x100a4e00, NCMSView_CharacterSelectWnd, WindowProc)

// 0x100a5c60 (vslot 37) - the title strip frame (6 pixels short of the width) and the two combo boxes under it
INT NCMSView_CharacterSelectWnd::OnCreate()
{
    guard(NCMSView_CharacterSelectWnd::OnCreate);
    NCWnd::OnCreate();
    FrameCtrl = NewStock<NCFrameCtrl>(0x100126E0, 0, 0, 0);
    FrameID = NID_MSVIEW_SELECT_FRAME;
    FrameCtrl->ID = NID_MSVIEW_SELECT_FRAME;
    FrameCtrl->SetPosition(2, 3, (INT)(Width - 6.0f), 20);
    CreateChild(FrameCtrl, NWS_VISIBLE);
    TitleTex = FindTexture(TEXT("sek.cbui168"));
    BackTex = FindTexture(TEXT("sek.cbui144"));
    Unknown67Tex = FindTexture(TEXT("sek.cbui67"));
    Unknown68Tex = FindTexture(TEXT("sek.cbui68"));
    Unknown69Tex = FindTexture(TEXT("sek.cbui69"));
    NameCombo = NewStock<NCComboBox>(0x10006A10);
    NameCombo->ID = NID_MSVIEW_SELECT_NAME;
    NameCombo->SetPosition(41, 28, 129, 18);
    CreateChild(NameCombo, NWS_VISIBLE);
    AniCombo = NewStock<NCComboBox>(0x10006A10);
    AniCombo->ID = NID_MSVIEW_SELECT_ANIM;
    AniCombo->SetPosition(41, 52, 129, 18);
    CreateChild(AniCombo, NWS_VISIBLE);
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100a5c60, NCMSView_CharacterSelectWnd, OnCreate)

// 0x100a5490 (vslot 38) - the combo boxes (the title bar is left to NCWnd::OnDestroy)
INT NCMSView_CharacterSelectWnd::OnDestroy()
{
    guard(NCMSView_CharacterSelectWnd::OnDestroy);
    DestroyChild(this, NameCombo);
    DestroyChild(this, AniCombo);
    NCWnd::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100a5490, NCMSView_CharacterSelectWnd, OnDestroy)

// 0x100a5210 (vslot 62) - the background and the title strip (4 pixels short of the width), the title and six labels
INT NCMSView_CharacterSelectWnd::OnPaint(NCGDevice* Device)
{
    guard(NCMSView_CharacterSelectWnd::OnPaint);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    Device->BeginPaint(ScreenX, ScreenY);
    if (BackTex)
        Device->DrawTexture(0, 0, (INT)Width, (INT)Height, 0, 0, (INT)Width, (INT)Height, BackTex);
    if (TitleTex)
        Device->DrawTexture(2, 3, (INT)(Width - 4.0f), 20, 0, 0, (INT)(Width - 4.0f), 20, TitleTex);
    Device->DrawText(23, 8, 0xFFDCDCDC, *GL2GameData.SysString[STR_MSViewTitle], 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0,
                     0, 0);
    Device->DrawText(8, 32, 0xFFA3A3A3, *GL2GameData.SysString[STR_MSViewLabel1], 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0,
                     0, 0);
    Device->DrawText(8, 72, 0xFFA3A3A3, *GL2GameData.SysString[STR_MSViewLabel2], 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0,
                     0, 0);
    Device->DrawText(8, 100, 0xFFA3A3A3, *GL2GameData.SysString[STR_MSViewLabel3], 0, 0, 0, L2FONT_Normal, 0, 0, 0,
                     0, 0, 0);
    Device->DrawText(107, 100, 0xFFA3A3A3, *GL2GameData.SysString[STR_MSViewLabel4], 0, 0, 0, L2FONT_Normal, 0, 0,
                     0, 0, 0, 0);
    Device->DrawText(8, 114, 0xFFA3A3A3, *GL2GameData.SysString[STR_MSViewLabel5], 0, 0, 0, L2FONT_Normal, 0, 0, 0,
                     0, 0, 0);
    Device->DrawText(6, 128, 0xFFA3A3A3, *GL2GameData.SysString[STR_MSViewLabel6], 0, 0, 0, L2FONT_Normal, 0, 0, 0,
                     0, 0, 0);
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100a5210, NCMSView_CharacterSelectWnd, OnPaint)

// 0x100a5560 (vslot 74) - another character (0..19, not the one shown) becomes the player's pawn: a player controller
// spawned for it, the pawn class loaded by the character's class name, the pawn spawned 30 units above the player's,
// which it replaces (the old one destroyed, the controller kept the viewport's); then its idle animation looped and
// its animations into the combo box (the animation set is not checked for NULL here)
INT NCMSView_CharacterSelectWnd::OnNameComboBox(INT Index)
{
    guard(NCMSView_CharacterSelectWnd::OnNameComboBox);
    if (Index > 19 || Index == Selected)
        return 1;
    FName ControllerClassName = NAME_None;
    ULevel* Level = GameLevel();
    APlayerController* Viewer = ViewActor();
    ControllerClassName = FName(TEXT("Engine.PlayerController"), FNAME_Add);
    AController* Controller = (AController*)Level->SpawnActor(
        UObject::StaticLoadClass(AController::StaticClass(), NULL, *ControllerClassName, NULL, LOAD_NoFail, NULL));
    if (!Controller)
        appFailAssert("pController", "Src\\NMSViewerWnd.cpp", 1039);
    User Character = MSViewer()->Characters[Index];
    Controller->PawnClass = UObject::StaticLoadClass(APawn::StaticClass(), NULL, Character.GetClassNamePointer(), NULL,
                                                     0, NULL);
    if (!Controller->PawnClass)
        return 1;
    FVector Location = ViewActor()->Pawn->Location;
    Location.Z += 30.0f;
    APawn* Pawn = (APawn*)Level->SpawnActor(Controller->PawnClass, NAME_None, Location, FRotator(0, 0, 0), NULL, 1, 1,
                                            Controller, NULL);
    if (!Pawn)
        return 1;
    Level->DestroyActor(ViewActor()->Pawn, 0);
    ViewActor()->Pawn = Pawn;
    Pawn->Controller = Viewer;
    Pawn->Role = ROLE_Authority;
    Pawn->setPhysics(1, NULL, FVector(0.f, 0.f, 1.f));
    Selected = Index;
    MSViewer()->Characters[Index].Pawn = Pawn;
    MSViewer()->Characters[Selected].SetPawnResource();
    // the idle animation of the pawn's weapon
    Pawn->PlayAnim(0, Pawn->GetWaitAnimName(), 1.0f, 0.f, 1, 0);
    APawn* ViewPawn = ViewActor()->Pawn;
    if (ViewPawn)
    {
        UMesh* Mesh = ViewPawn->Mesh;
        if (Mesh)
        {
            Mesh->MeshGetInstance(ViewPawn);
            UMeshAnimation* Anim = static_cast<USkeletalMesh*>(Mesh)->DefaultAnim;
            // C4: the sequences are at +0x6C of the set, 0x74 bytes each, their count at +0x70
            INT Num = *(INT*)((BYTE*)Anim + 0x70);
            BYTE* Seqs = *(BYTE**)((BYTE*)Anim + 0x6C);
            FName Sequence = NAME_None;
            ComboClear(AniCombo);
            for (INT i = 0; i < Num; i++)
            {
                Sequence = *(FName*)(Seqs + i * 0x74);
                ComboAdd(AniCombo, *Sequence);
            }
        }
    }
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100a5560, NCMSView_CharacterSelectWnd, OnNameComboBox)

// 0x100a5900 (vslot 75) - the animation picked plays looped on the player's pawn. Items(Index) is read up to
// Index == Num (">=" in the stock code).
INT NCMSView_CharacterSelectWnd::OnAniNameComboBox(INT Index)
{
    guard(NCMSView_CharacterSelectWnd::OnAniNameComboBox);
    // the combo box's Items array (the header keeps it as filler): C4 has 12-byte lines at +0x1FC, their count at +0x200
    if (*(INT*)((BYTE*)AniCombo + 0x200) >= Index)
    {
        TCHAR* Name = ComboItemText(AniCombo, Index);
        APawn* Pawn = ViewActor()->Pawn;
        Pawn->AnimFlags |= 0x10;
        Pawn->Unknown13D4 &= ~1;
        Pawn->PlayAnim(0, FName(Name, FNAME_Add), 1.0f, 0.f, 1, 0);
    }
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100a5900, NCMSView_CharacterSelectWnd, OnAniNameComboBox)

/*-----------------------------------------------------------------------------
    NCMSViewrWnd.
-----------------------------------------------------------------------------*/

// 0x100a67d0 - the tabs, the textures and the level are made in OnCreate; the nineteen User records are built by
// their constructors, the grids cleared (the selection window's pointer is not)
NCMSViewrWnd::NCMSViewrWnd() : TabCtrl(NULL), BackTex(NULL), HtmlBackTex(NULL), Level(NULL)
{
    ItemWnd[0] = NULL;
    ItemWnd[1] = NULL;
    ItemWnd[2] = NULL;
    ItemWnd[3] = NULL;
}
NW_HOOK_CTOR(0x100a67d0, NCMSViewrWnd)

// 0x100a4340 (vslot 25) - the two buttons search; Enter in the ID or name line reports through the edit's id into
// slot 70 / 71, bodies shared with other classes (code 1: the other search). C3 no longer refuses the messages of a
// disabled window here.
INT NCMSViewrWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCWnd::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND)
    {
        if (wParam == NID_MSVIEW_SEARCHNAME)
            return OnSearchName() != 0;
        if (wParam == NID_MSVIEW_SEARCHID)
            return OnSearchID() != 0;
        if (wParam == NID_MSVIEW_IDEDIT)
            return ::nw::VirtualCall<INT>(this, 76, (INT)lParam) != 0;
        if (wParam == NID_MSVIEW_NAMEEDIT)
            return ::nw::VirtualCall<INT>(this, 77, (INT)lParam) != 0;
    }
    return 0;
}
NW_HOOK_METHOD(0x100a4340, NCMSViewrWnd, WindowProc)

// 0x100bf470 (slot 70 here, slot 74 of NCNpcView_CharacterSelectWnd: one body, both call slot 69) - Enter (code 1) in
// the ID line searches by ID
INT NCMSViewrWnd::OnIDEdit(INT Code)
{
    if (Code == 1)
        OnSearchID();
    return 1;
}
NW_HOOK_METHOD(0x100bf470, NCMSViewrWnd, OnIDEdit)

// (C3: the stock body of slot 71 is NCAuthWnd::OnPasswordNotify's, 0x100b2360) - Enter (code 1) in the name line
// searches by name
INT NCMSViewrWnd::OnNameEdit(INT Code)
{
    if (Code == 1)
        OnSearchName();
    return 1;
}

// 0x100a69f0 (vslot 37) - the title bar, the name and ID search lines, the three kinds' check boxes (the passive one
// off) and the two search buttons, the four grids under their tabs ("magic", "active", "passive", "search"), the
// selection window (top-level), then the tables loaded and the first character shown. NCWnd::OnCreate is not called.
// C4: the check boxes are made from a sysstring id (4 arguments) and the tabs take one more argument
INT NCMSViewrWnd::OnCreate()
{
    guard(NCMSViewrWnd::OnCreate);
    FrameCtrl = NewStock<NCFrameCtrl>(0x100126E0, 1, 0, 0);
    FrameID = NID_MSVIEW_FRAME;
    FrameCtrl->ID = NID_MSVIEW_FRAME;
    FrameCtrl->SetPosition(0, 0, (INT)Width, 25);
    CreateChild(FrameCtrl, NWS_VISIBLE);
    HtmlBackTex = FindTexture(TEXT("L2UI.etcwnd.NPCHtml_BACK"));
    BackTex = FindTexture(TEXT("L2UI.SkillWnd.SkillBack"));

    NameEdit = NewStock<NCEditBox>(0x100112C0, 1, (const TCHAR*)NULL, 1, 64, 0);
    NameEdit->ID = NID_MSVIEW_NAMEEDIT;
    NameEdit->SetPosition(55, 30, 65, 16);
    CreateChild(NameEdit, NWS_VISIBLE);
    IDEdit = NewStock<NCEditBox>(0x100112C0, 1, (const TCHAR*)NULL, 1, 64, 0);
    IDEdit->ID = NID_MSVIEW_IDEDIT;
    IDEdit->SetPosition(55, 55, 65, 16);
    CreateChild(IDEdit, NWS_VISIBLE);

    MagicCheck = NewStock<NCCheckBox>(0x100058C0, STR_MSViewMagic, 0, 0, 0);
    MagicCheck->SetPosition(15, 80, 65, 16);
    CreateChild(MagicCheck, NWS_VISIBLE);
    MagicCheck->bChecked = 1;
    ActiveCheck = NewStock<NCCheckBox>(0x100058C0, STR_MSViewActive, 0, 0, 0);
    ActiveCheck->SetPosition(80, 80, 65, 16);
    CreateChild(ActiveCheck, NWS_VISIBLE);
    ActiveCheck->bChecked = 1;
    PassiveCheck = NewStock<NCCheckBox>(0x100058C0, STR_MSViewPassive, 0, 0, 0);
    PassiveCheck->SetPosition(145, 80, 65, 16);
    CreateChild(PassiveCheck, NWS_VISIBLE);
    PassiveCheck->bChecked = 0;

    SearchNameButton = NewStock<NCButton>(0x10003340, TEXT("L2UI.Control.DefaultButton"),
                                          TEXT("L2UI.Control.DefaultButton_click"), 0);
    SearchNameButton->ID = NID_MSVIEW_SEARCHNAME;
    SearchNameButton->SetPosition(140, 30, 32, 16);
    CreateChild(SearchNameButton, NWS_VISIBLE);
    SearchNameButton->SetTextID(STR_MSViewSearch);
    SearchIDButton = NewStock<NCButton>(0x10003340, TEXT("L2UI.Control.DefaultButton"),
                                        TEXT("L2UI.Control.DefaultButton_click"), 0);
    SearchIDButton->ID = NID_MSVIEW_SEARCHID;
    SearchIDButton->SetPosition(140, 55, 32, 16);
    CreateChild(SearchIDButton, NWS_VISIBLE);
    SearchIDButton->SetTextID(STR_MSViewSearch);

    ItemWnd[0] = NewStock<NCMSView_InvenItemWnd>(0x100A67A0, 6, 7, 80, 32);
    ItemWnd[0]->SetPosition(16, 133, 220, 255);
    CreateChild(ItemWnd[0], NWS_VISIBLE | 0x20);
    ItemWnd[1] = NewStock<NCMSView_InvenItemWnd>(0x100A67A0, 6, 7, 80, 32);
    ItemWnd[1]->SetPosition(16, 133, 220, 255);
    CreateChild(ItemWnd[1], NWS_VISIBLE | 0x20);
    ItemWnd[2] = NewStock<NCMSView_InvenItemWnd>(0x100A67A0, 6, 7, 80, 32);
    ItemWnd[2]->SetPosition(16, 133, 220, 255);
    CreateChild(ItemWnd[2], NWS_VISIBLE | 0x20);
    ItemWnd[3] = NewStock<NCMSView_InvenItemWnd>(0x100A67A0, 6, 7, 80, 32);
    ItemWnd[3]->SetPosition(16, 133, 220, 255);
    CreateChild(ItemWnd[3], NWS_VISIBLE | 0x20);

    TabCtrl = NewStock<NCTabControl>(0x1002E750);
    TabCtrl->ID = NID_MSVIEW_TAB;
    TabCtrl->SetPosition(15, 102, 138, 16);
    CreateChild(TabCtrl, NWS_VISIBLE);
    // NCTabControl::AddTab (0x1002DED0, 9 arguments: a trailing 0)
    TabCtrl->AddTab((INT)STR_MSViewMagic, (NCWnd*)ItemWnd[0], 35, 16, TEXT("L2UI.NWindow.Tab2"), TEXT("L2UI.NWindow.Tab1"), 0, -1, 0);
    TabCtrl->AddTab((INT)STR_MSViewActive, (NCWnd*)ItemWnd[1], 35, 16, TEXT("L2UI.NWindow.Tab2"), TEXT("L2UI.NWindow.Tab1"), 0, -1, 0);
    TabCtrl->AddTab((INT)STR_MSViewPassive, (NCWnd*)ItemWnd[2], 35, 16, TEXT("L2UI.NWindow.Tab2"), TEXT("L2UI.NWindow.Tab1"), 0, -1, 0);
    TabCtrl->AddTab((INT)STR_MSViewSearch, (NCWnd*)ItemWnd[3], 33, 16, TEXT("L2UI.NWindow.Tab2"), TEXT("L2UI.NWindow.Tab1"), 0, -1, 0);

    for (INT i = 0; i < 19; i++)
        Characters[i].Pawn = NULL;
    CharSelectWnd = NewStock<NCMSView_CharacterSelectWnd>(0x100A68F0);
    CharSelectWnd->SetPosition(34, 34, 208, 161);
    CreateTopWindow(CharSelectWnd, NWS_VISIBLE | NWS_NOHITTEST);
    AddAllItem();
    AddAllCharacter();
    Level = GameLevel();
    CharSelectWnd->OnNameComboBox(0);
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100a69f0, NCMSViewrWnd, OnCreate)

// 0x100a5080 (vslot 38) - the selection window deleted (top-level, not cleared); the search
// controls are left to NCWnd::OnDestroy
INT NCMSViewrWnd::OnDestroy()
{
    guard(NCMSViewrWnd::OnDestroy);
    DestroyChild(this, ItemWnd[0]);
    DestroyChild(this, ItemWnd[1]);
    DestroyChild(this, ItemWnd[2]);
    DestroyChild(this, ItemWnd[3]);
    DestroyChild(this, TabCtrl);
    DestroyChild(this, FrameCtrl);
    delete CharSelectWnd;
    NCWnd::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100a5080, NCMSViewrWnd, OnDestroy)

// 0x100a4cc0 (vslot 48) - with the selection window
void NCMSViewrWnd::ShowWindow()
{
    guard(NCMSViewrWnd::ShowWindow);
    if (CharSelectWnd)
        CharSelectWnd->ShowWindow();
    NCWnd::ShowWindow();
    TabCtrl->ShowCurrentPage();
    unguard;
}
NW_HOOK_METHOD(0x100a4cc0, NCMSViewrWnd, ShowWindow)

// 0x100a4c30 (vslot 49)
void NCMSViewrWnd::HideWindow()
{
    guard(NCMSViewrWnd::HideWindow);
    if (CharSelectWnd)
        CharSelectWnd->HideWindow();
    NCWnd::HideWindow();
    unguard;
}
NW_HOOK_METHOD(0x100a4c30, NCMSViewrWnd, HideWindow)

// 0x100a4eb0 (vslot 62) - the grids' background, the search's, and the search lines' labels ("skill name:", "skill
// ID:" in Korean) centered in 55 x 16 before them (the origin goes on before the clip)
INT NCMSViewrWnd::OnPaint(NCGDevice* Device)
{
    guard(NCMSViewrWnd::OnPaint);
    SIZE Size;
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    if (BackTex)
        Device->DrawTexture(0, 100, 240, 300, 0, 0, 240, 300, BackTex);
    if (HtmlBackTex)
        Device->DrawStretchedTex(0, 0, 240, 100, 0, 0, 300, 60, HtmlBackTex, 0xFF);
    // C4: the labels are real Unicode now ("skill name:" and "skill ID:" in Korean; C3 kept the ANSI bytes)
    NCGDevice::GetTextExtent(&Size, TEXT("\xC2A4\xD0AC\xBA85\x003A"), 0, 0, 0, 0, 0);
    Device->DrawText((55 - Size.cx) / 2 + 5, (16 - Size.cy) / 2 + 30, 0xFFDCDCDC,
                     TEXT("\xC2A4\xD0AC\xBA85\x003A"), 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
    NCGDevice::GetTextExtent(&Size, TEXT("\xC2A4\xD0AC") TEXT("ID:"), 0, 0, 0, 0, 0);
    Device->DrawText((55 - Size.cx) / 2 + 5, (16 - Size.cy) / 2 + 55, 0xFFDCDCDC,
                     TEXT("\xC2A4\xD0AC") TEXT("ID:"), 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100a4eb0, NCMSViewrWnd, OnPaint)

// the kind of a skill for the search (Type: its name in Korean, real Unicode in C4): 0 when the search leaves it out;
// kinds) keep the Type of the skill before them
static FORCEINLINE UBOOL SearchKind(NCMSViewrWnd* Wnd, NSkillDataC3* Data, TCHAR* Type)
{
    if (Data->bMagic)
    {
        if (!Wnd->MagicCheck->bChecked)
            return 0;
        appStrcpy(Type, TEXT("\xB9C8\xBC95"));                                           // "magic"
    }
    else if (Data->OperateType >= 0 && Data->OperateType <= 1)
    {
        if (!Wnd->ActiveCheck->bChecked)
            return 0;
        appStrcpy(Type, TEXT("\xC561\xD2F0\xBE0C"));                                    // "active"
    }
    else if (Data->OperateType == 2)
    {
        if (!Wnd->PassiveCheck->bChecked)
            return 0;
        appStrcpy(Type, TEXT("\xD328\xC2DC\xBE0C"));                                    // "passive"
    }
    return 1;
}

// A skill found by a search into the fourth grid (level 1 only): the icon of its level (or "NWindow.Darkyellow"),
// "ID = n\nTYPE = kind" as its description.
static FORCEINLINE void AddFoundSkill(NCMSViewrWnd* Wnd, INT SkillID, INT Level, const TCHAR* Type)
{
    NItemInfo Info;
    if (Level != 1)
        return;
    wcscpy(Info.IconName, GConsole->GetSkillIconName(SkillID, 1));
    InfoInt(&Info, 0x1900) = (INT)Wnd->FindTexture(GConsole->GetSkillIconName(SkillID, 1));
    if (GConsole->GetSkillIconName(SkillID, 1))
        InfoInt(&Info, 0x1900) = (INT)Wnd->FindTexture(GConsole->GetSkillIconName(SkillID, 1));
    if (!InfoInt(&Info, 0x1900))
        InfoInt(&Info, 0x1900) = (INT)Wnd->FindTexture(TEXT("NWindow.Darkyellow"));
    if (GConsole->GetSkillName(SkillID, 1))
    {
        appSprintf((TCHAR*)((BYTE*)&Info + 0x900), TEXT("ID = %d\nTYPE = %s"), SkillID, Type);
        appStrcpy(Info.Name, GConsole->GetSkillName(SkillID, 1));
    }
    InfoInt(&Info, 0x1920) = SkillID;
    InfoInt(&Info, 0x1930) = 3;
    Wnd->AddItem(3, &Info);
    Wnd->ItemWnd[3]->MaxItems++;
}

// 0x100a61a0 (vslot 74) - the skills whose name holds the text, of the kinds checked
INT NCMSViewrWnd::OnSearchName()
{
    const TCHAR* Text = NameEdit->CommitText();
    if (Text && appStrlen(Text) > 0)
    {
        ClearGrid(ItemWnd[3]);
        NDataMap<FL2MagicSkillData>* Map = GL2GameData.SkillTable;
        TCHAR Type[20];
        for (INT i = 0; i < Map->Pairs.Num(); i++)
        {
            NSkillDataC3* Data = (NSkillDataC3*)Map->Pairs(i).Value;
            if (!Data)
                continue;
            INT SkillID = Data->SkillID;
            INT Level = Data->Level;
            if (!Data->Name.Num() || !appStrstr(*Data->Name, Text) || !SearchKind(this, Data, Type))
                continue;
            AddFoundSkill(this, SkillID, Level, Type);
        }
        TabCtrl->SetTab(3, 1);
    }
    return 1;
}
NW_HOOK_METHOD(0x100a61a0, NCMSViewrWnd, OnSearchName)

// 0x100a6460 (vslot 75) - the skill of the id typed, of the kinds checked
INT NCMSViewrWnd::OnSearchID()
{
    const TCHAR* Text = IDEdit->CommitText();
    if (Text && appStrlen(Text) > 0)
    {
        INT ID = appAtoi(Text);
        ClearGrid(ItemWnd[3]);
        NDataMap<FL2MagicSkillData>* Map = GL2GameData.SkillTable;
        TCHAR Type[20];
        for (INT i = 0; i < Map->Pairs.Num(); i++)
        {
            NSkillDataC3* Data = (NSkillDataC3*)Map->Pairs(i).Value;
            if (!Data)
                continue;
            INT SkillID = Data->SkillID;
            INT Level = Data->Level;
            if (SkillID != ID || !SearchKind(this, Data, Type))
                continue;
            AddFoundSkill(this, SkillID, Level, Type);
        }
        TabCtrl->SetTab(3, 1);
    }
    return 1;
}
NW_HOOK_METHOD(0x100a6460, NCMSViewrWnd, OnSearchID)

// 0x100a4d60 - into a tab's grid, unless the info's +0x19A0 is set (C3 +0x1798, C2 +0xC8C; the NItemInfo "worn" flag)
INT NCMSViewrWnd::AddItem(INT Tab, NItemInfo* Info)
{
    guard(NCMSViewrWnd::AddItem);
    if (InfoInt(Info, 0x19A0) == 0 && Tab >= 0 && Tab < 4 && ItemWnd[Tab])
        ItemWnd[Tab]->AddItem(Info);
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100a4d60, NCMSViewrWnd, AddItem)

// 0x100a4400 - the characters: 19 records cleared and given the race, sex and class of the table (see
// GMSViewCharacters), then 18 names "race sex class" in the selection window
void NCMSViewrWnd::AddAllCharacter()
{
    guard(NCMSViewrWnd::AddAllCharacter);
    for (INT i = 0; i < 19; i++)
    {
        memset(&Characters[i], 0, sizeof(User));
        Characters[i].Race = GMSViewCharacters[i].Race;
        Characters[i].Sex = GMSViewCharacters[i].Sex;
        Characters[i].ClassID = GMSViewCharacters[i].ClassID;
    }
    // the 18 names "race sex class" (fighter / mystic, male / female for each race), written out one by one as in the
    // stock code; the combo box's AddItem(const TCHAR*) is 0x100068b0 in this client
    TCHAR Name[104];
#define MS_ADD_NAME(Race, Sex, Type) \
    appSprintf(Name, TEXT("%s%s%s"), *GL2GameData.SysString[STR_MSViewRace0 + (Race)], \
               *GL2GameData.SysString[STR_MSViewMale + (Sex)], *GL2GameData.SysString[STR_MSViewFighter + (Type)]); \
    ComboAdd(CharSelectWnd->NameCombo, Name)
    MS_ADD_NAME(0, 0, 0);
    MS_ADD_NAME(0, 1, 0);
    MS_ADD_NAME(0, 0, 1);
    MS_ADD_NAME(0, 1, 1);
    MS_ADD_NAME(1, 0, 0);
    MS_ADD_NAME(1, 1, 0);
    MS_ADD_NAME(1, 0, 1);
    MS_ADD_NAME(1, 1, 1);
    MS_ADD_NAME(2, 0, 0);
    MS_ADD_NAME(2, 1, 0);
    MS_ADD_NAME(2, 0, 1);
    MS_ADD_NAME(2, 1, 1);
    MS_ADD_NAME(3, 0, 0);
    MS_ADD_NAME(3, 1, 0);
    MS_ADD_NAME(3, 0, 1);
    MS_ADD_NAME(3, 1, 1);
    MS_ADD_NAME(4, 0, 0);
    MS_ADD_NAME(4, 1, 0);
#undef MS_ADD_NAME
    unguard;
}
NW_HOOK_METHOD(0x100a4400, NCMSViewrWnd, AddAllCharacter)

// 0x100a5f40 - the level-1 skills into the first three grids by kind (magic, active, passive and toggles), their
// capacity raised with each; the icon of their level (its name copied without a NULL check) or "NWindow.Darkyellow",
// "ID = n" as their description. C4: the fields of the item record sit 0x208 later (Description +0x900, Icon +0x1900, ID
// +0x1920, BodyPart +0x1930) and the skill's magic flag at +0x3C
void NCMSViewrWnd::AddAllItem()
{
    guard(NCMSViewrWnd::AddAllItem);
    NDataMap<FL2MagicSkillData>* Map = GL2GameData.SkillTable;
    for (INT i = 0; i < Map->Pairs.Num(); i++)
    {
        NSkillDataC3* Data = (NSkillDataC3*)Map->Pairs(i).Value;
        if (!Data)
            continue;
        INT SkillID = Data->SkillID;
        INT Level = Data->Level;
        NItemInfo Info;
        if (Level != 1)
            continue;
        wcscpy(Info.IconName, GConsole->GetSkillIconName(SkillID, 1));
        InfoInt(&Info, 0x1900) = (INT)FindTexture(GConsole->GetSkillIconName(SkillID, 1));
        if (GConsole->GetSkillIconName(SkillID, 1))
            InfoInt(&Info, 0x1900) = (INT)FindTexture(GConsole->GetSkillIconName(SkillID, 1));
        if (!InfoInt(&Info, 0x1900))
            InfoInt(&Info, 0x1900) = (INT)FindTexture(TEXT("NWindow.Darkyellow"));
        if (GConsole->GetSkillName(SkillID, 1))
        {
            swprintf((TCHAR*)((BYTE*)&Info + 0x900), TEXT("ID = %d"), SkillID);
            wcscpy(Info.Name, GConsole->GetSkillName(SkillID, 1));
        }
        InfoInt(&Info, 0x1920) = SkillID;
        NCMSView_InvenItemWnd* Grid;
        if (Data->bMagic)
        {
            InfoInt(&Info, 0x1930) = 0;
            AddItem(0, &Info);
            Grid = ItemWnd[0];
        }
        else if (Data->OperateType != 0 && Data->OperateType != 1)
        {
            InfoInt(&Info, 0x1930) = 2;
            AddItem(2, &Info);
            Grid = ItemWnd[2];
        }
        else
        {
            InfoInt(&Info, 0x1930) = 1;
            AddItem(1, &Info);
            Grid = ItemWnd[1];
        }
        Grid->MaxItems++;
    }
    unguard;
}
NW_HOOK_METHOD(0x100a5f40, NCMSViewrWnd, AddAllItem)

// 0x100a6750 - the vtable, ConditionalDestroy, NCInvenItemWnd
NW_HOOK_DTOR(0x100a6750, NCMSView_InvenItemWnd)
// 0x100a68a0 - the vtable, ConditionalDestroy, NCWnd
NW_HOOK_DTOR(0x100a68a0, NCMSViewrWnd)
// 0x100a6970 - the vtable, ConditionalDestroy, NCWnd
NW_HOOK_DTOR(0x100a6970, NCMSView_CharacterSelectWnd)

// 0x100a4e80 - Cast<USkillVisualEffect> (the item window's right click on a skill effect)
static USkillVisualEffect* CastToSkillVisualEffect(UObject* Object)
{
    if (Object && Object->IsA(USkillVisualEffect::StaticClass()))
        return (USkillVisualEffect*)Object;
    return NULL;
}
NW_HOOK_FUNC(0x100a4e80, CastToSkillVisualEffect)

NW_CHECK_VTABLE(NCMSView_InvenItemWnd, 0x101b9b00, 82)
NW_CHECK_VTABLE(NCMSView_CharacterSelectWnd, 0x101b9d80, 76)
NW_CHECK_VTABLE(NCMSViewrWnd, 0x101b9c48, 78)
