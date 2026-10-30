/*=============================================================================
    NCPawnViewr_Simulator.cpp - the pawn viewer's crowd simulator. Stock:
    0x10088770 .. 0x100887d8, 0x1008bc10 .. 0x1008c27d, 0x1008d150 .. 0x1008d935,
    0x1008de40 .. 0x1008e211, and UE2's appSeconds out of line at 0x10016620 (the pawn
    vector's std::vector code is NStdLib.cpp's; the OnDestroy body is NCLobbyWnd's).
=============================================================================*/
#include "nwindow/NCPawnViewrWnd.h"
#include "nwindow/NCButton.h"
#include "nwindow/NCEditBox.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NCMSViewrWnd.h"
#include "nwindow/NConsoleWnd.h"

#include <intrin.h>
#include <string.h>

// An object made as the stock code makes it: StaticAllocateObject of its class, then the constructor at Ctor of
// this client (the headers' constructors of these controls are not all safe to run)
// The standalone has no stock code: the constructor that runs is T's own (hooked at Ctor with the same arguments, so the
// hook build runs the same one through the stock address).
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

// NCEditBox::InsertText, 0x1000C250
static FORCEINLINE void EditInsertText(NCEditBox* Edit, const TCHAR* Text, UBOOL bAppend)
{
    Edit->InsertText(Text, bAppend);
}

// TArray<AActor*>::AddItem is one out-of-line function in the stock image (C4: the viewer's instance 0x100A5A00, thiscall
// on the array with the address of the element, ret 4): the pawn's skill targets are added by a call to it, not by the
// array's inlined body (NActorArrayAdd, NCMSViewrWnd.cpp)
static FORCEINLINE void AddMagicTarget(TArray<AActor*>* Targets, AActor** Target)
{
    reinterpret_cast<NActorArrayAdd*>(Targets)->AddItem(Target);
}


enum { NID_SIMULATOR_START = 0x113 };   // NWM_COMMAND id (name ours)
enum { STR_Start = 428 };               // sysstring line

// the weapons the new pawns hold (item ids)
enum { ITEM_BowWeapon = 273, ITEM_OneHandWeapon = 1 };

// the 18 default characters of the creation screen (0x101133b8, only the viewer reads it)
struct NDefaultCharacter
{
    INT Race;
    INT Sex;
    INT ClassID;
};
#if NW_HOOK_MODE
#define GDefaultCharacters (reinterpret_cast<const NDefaultCharacter*>(::nw::Addr(0x10263160)))
#else
extern const NDefaultCharacter GDefaultCharacters[];
#endif

// UE2's appSeconds (UnVcWin32.h): the time stamp counter in seconds, plus 2^24 (stock: one out-of-line
// copy at 0x10019A10, called from here)
static FORCEINLINE DOUBLE appSeconds()
{
    unsigned __int64 Cycles = __rdtsc();
    DWORD L = (DWORD)Cycles;
    DWORD H = (DWORD)(Cycles >> 32);
    return ((DOUBLE)L + 4294967296.0 * (DOUBLE)H) * GSecondsPerCycle + 16777216.0;
}

// the same clock for OnPaint (the stock expands appSeconds inline there; 0x10019A10 is the out-of-line copy OnStart calls)
static FORCEINLINE DOUBLE SecondsNow()
{
    return appSeconds();
}

// a label centered in a Width x 16 box at (X, Y) (inline in the stock OnPaint)
static FORCEINLINE void DrawLabel(NCGDevice* Device, const TCHAR* Text, INT BoxW, INT X, INT Y)
{
    SIZE Size;
    NCGDevice::GetTextExtent(&Size, Text, 0, 0, 0, 0, 0);
    Device->DrawText((BoxW - Size.cx) / 2 + X, (16 - Size.cy) / 2 + Y, 0xFFDCDCDC, Text, 0, 0, 0,
                     L2FONT_Normal, 0, 0, 0, 0, 0, 0);
}

// a child window goes (repeated inline in the stock OnDestroys)
template <class T> static FORCEINLINE void DestroyChild(NCWnd* Parent, T*& Child)
{
    if (Child)
    {
        Parent->RemoveChildWindow(Child->Entry);
        delete Child;
        Child = NULL;
    }
}

// 0x100d98f0 - the bow share's edit line is left as allocated
NCPawnViewr_Simulator::NCPawnViewr_Simulator()
{
    CountEdit = NULL;
    SkillEdit = NULL;
    CancelEdit = NULL;
    ReplaceEdit = NULL;
    StartButton = NULL;
    Pawns.First = NULL;
    Pawns.Last = NULL;
    Pawns.End = NULL;
}
NW_HOOK_CTOR(0x100d98f0, NCPawnViewr_Simulator)

// 0x100d9990 - the pawn vector's storage freed (its pawns are not destroyed)
NW_HOOK_DTOR(0x100d9990, NCPawnViewr_Simulator)

// 0x100d9480 - the pawn vector's destructor as the stock's unwind funclets of the simulator's frames call it: the storage freed
// (GMalloc->Free), the three pointers cleared
NW_HOOK_DTOR(0x100d9480, NSimPawnVector)


// 0x100d1d50 (vslot 25)
INT NCPawnViewr_Simulator::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCWnd::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND && wParam == NID_SIMULATOR_START)
        return OnStart() != 0;
    return 0;
}
NW_HOOK_METHOD(0x100d1d50, NCPawnViewr_Simulator, WindowProc)

// 0x100d7de0 (vslot 37) - the five edit lines and "Start"; the active skills of level 1 that have an animation
// (SkillIDs holds 2000, unchecked). The skill record is read by offset: this client's FL2MagicSkillData has the id at
// +0x18, the level at +0x1C, the operate type at +0x20 and the animation name at +0x40 (C3: +0x3C; the header has the C2 record)
INT NCPawnViewr_Simulator::OnCreate()
{
    NCWnd::OnCreate();
    CountEdit = NewStock<NCEditBox>(0x100112C0, 1, (const TCHAR*)NULL, 1, 64, 0);
    CountEdit->SetPosition(100, 30, 60, 16);
    CreateChild(CountEdit, NWS_VISIBLE);
    SkillEdit = NewStock<NCEditBox>(0x100112C0, 1, (const TCHAR*)NULL, 1, 64, 0);
    SkillEdit->SetPosition(100, 60, 60, 16);
    CreateChild(SkillEdit, NWS_VISIBLE);
    CancelEdit = NewStock<NCEditBox>(0x100112C0, 1, (const TCHAR*)NULL, 1, 64, 0);
    CancelEdit->SetPosition(100, 90, 60, 16);
    CreateChild(CancelEdit, NWS_VISIBLE);
    ReplaceEdit = NewStock<NCEditBox>(0x100112C0, 1, (const TCHAR*)NULL, 1, 64, 0);
    ReplaceEdit->SetPosition(100, 120, 60, 16);
    CreateChild(ReplaceEdit, NWS_VISIBLE);
    BowEdit = NewStock<NCEditBox>(0x100112C0, 1, (const TCHAR*)NULL, 1, 64, 0);
    BowEdit->SetPosition(70, 150, 30, 16);
    CreateChild(BowEdit, NWS_VISIBLE);
    StartButton = NewStock<NCButton>(0x10003340, TEXT("L2UI.Control.DefaultButton"),
                                     TEXT("L2UI.Control.DefaultButton_click"), 0);
    StartButton->ID = NID_SIMULATOR_START;
    StartButton->SetPosition(100, 180, 35, 16);
    CreateChild(StartButton, NWS_VISIBLE);
    StartButton->SetTextID(STR_Start);

    NumSkills = 0;
    NDataMap<FL2MagicSkillData>* Map = GL2GameData.SkillTable;
    for (INT i = 0; i < Map->Pairs.Num(); i++)
    {
        BYTE* Skill = (BYTE*)Map->Pairs(i).Value;
        if (!Skill)
            continue;
        FName None(TEXT("None"), FNAME_Add);
        if (*(FName*)(Skill + 0x40) != None && *(INT*)(Skill + 0x1C) == 1 && *(INT*)(Skill + 0x20) != 2)
            SkillIDs[NumSkills++] = *(INT*)(Skill + 0x18);
    }
    return 1;
}
NW_HOOK_METHOD(0x100d7de0, NCPawnViewr_Simulator, OnCreate)

// 0x100d5540 (vslot 38) - the five edit lines and the button; a body some other classes share (hooked once, here)
INT NCPawnViewr_Simulator::OnDestroy()
{
    DestroyChild(this, CountEdit);
    DestroyChild(this, SkillEdit);
    DestroyChild(this, CancelEdit);
    DestroyChild(this, ReplaceEdit);
    DestroyChild(this, BowEdit);
    DestroyChild(this, StartButton);
    NCWnd::OnDestroy();
    return 1;
}
NW_HOOK_METHOD(0x100d5540, NCPawnViewr_Simulator, OnDestroy)

// 0x100d4eb0 (vslot 62) - the labels ("PC count", "skill use %", "skill cancel %", "PC delete %", "bow %": single
// bytes of the Korean code page widened), then, while running, each pawn's turn: an action, else a cancel, else a
// replacement. The origin goes on before the clip.
INT NCPawnViewr_Simulator::OnPaint(NCGDevice* Device)
{
    guard(NCPawnViewr_Simulator::OnPaint);
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, Width, Height);
    // C4: the labels are real Unicode ("PC count:", "skill use %:", "skill stop %:", "PC delete %:", "bow %:" in Korean)
    DrawLabel(Device, TEXT("PC\xAC1C\xC218:"), 80, 20, 30);
    DrawLabel(Device, TEXT("\xC2A4\xD0AC\xC0AC\xC6A9%:"), 80, 20, 60);
    DrawLabel(Device, TEXT("\xC2A4\xD0AC\xC911\xB2E8%:"), 80, 20, 90);
    DrawLabel(Device, TEXT("PC\xC0AD\xC81C%:"), 80, 20, 120);
    DrawLabel(Device, TEXT("\xD65C%:"), 45, 25, 150);
    Device->EndPaint();
    Device->ReleaseClip();
    if (bRunning)
    {
        DeltaTime = SecondsNow() - LastTime;
        LastTime = DeltaTime + LastTime;
        for (INT i = 0; i < NumPawns; i++)
        {
            if (!TryAction(i) && !TryCancel(i))
                TryReplace(i);
        }
    }
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100d4eb0, NCPawnViewr_Simulator, OnPaint)

// 0x100d9640 (vslot 74) - the count and the percentages read; running (never, see bRunning) the crowd grows or
// shrinks, else it is made anew in reserved storage. Growing writes past the storage reserved.
INT NCPawnViewr_Simulator::OnStart()
{
    NSimPawn Spawned;
    if (!CountEdit->CommitText() || !SkillEdit->CommitText() || !CancelEdit->CommitText() ||
        !ReplaceEdit->CommitText() || !BowEdit->CommitText())
        return 1;
    INT Count = appAtoi(CountEdit->CommitText());
    SkillRate = appAtof(SkillEdit->CommitText());
    CancelRate = appAtof(CancelEdit->CommitText());
    ReplaceRate = appAtof(ReplaceEdit->CommitText());
    BowRate = appAtof(BowEdit->CommitText());
    if (bRunning)
    {
        LastTime = appSeconds();
        if (NumPawns < Count)
        {
            for (INT i = NumPawns; i < Count; i++)
                Pawns.First[i] = *SpawnPawn(&Spawned);
            NumPawns = Count;
            return 1;
        }
        for (INT i = Count; i < NumPawns; i++)
            GameLevel()->DestroyActor(Pawns.First[i].Pawn, 0);
        NumPawns = Count;
        return 1;
    }
    NumPawns = Count;
    SkillRate = appAtof(SkillEdit->CommitText());
    CancelRate = appAtof(CancelEdit->CommitText());
    ReplaceRate = appAtof(ReplaceEdit->CommitText());
    BowRate = appAtof(BowEdit->CommitText());
    LastTime = appSeconds();
    Pawns.Reserve(NumPawns);
    for (INT i = 0; i < NumPawns; i++)
        Pawns.First[i] = *SpawnPawn(&Spawned);
    return 1;
}
NW_HOOK_METHOD(0x100d9640, NCPawnViewr_Simulator, OnStart)

// 0x100d51f0 (vslot 75) - a random default character up to 300 units from the player (on the half circle of angles
// 0..pi) 50 higher, with a controller of its own: by the bow share one of six characters with item 273 in both hands,
// else any of the 18 with item 1 in the right hand. The stock indexes the six with a negated factor. The result goes
// through a hidden pointer (popped by the callee) and comes back in eax
NSimPawn* NCPawnViewr_Simulator::SpawnPawn(NSimPawn* Out)
{
    User Character;
    INT BowCharacters[6] = {0, 1, 4, 5, 7, 8};
    NSimPawn Result;
    INT Index;
    __stosd((DWORD*)&Character, 0, sizeof(User) / 4);
    if (appFrand() * 100.0f < BowRate)
    {
        Index = BowCharacters[-(INT)(appFrand() * -5.94f)];
        Result.Kind = 2;
        Character.Equipped[BODYSLOT_LRHand] = ITEM_BowWeapon;
    }
    else
    {
        Index = (INT)(appFrand() * 17.82f);
        Result.Kind = 1;
        Character.Equipped[BODYSLOT_RHand] = ITEM_OneHandWeapon;
    }
    Character.Race = GDefaultCharacters[Index].Race;
    Character.Sex = GDefaultCharacters[Index].Sex;
    Character.ClassID = GDefaultCharacters[Index].ClassID;
    ULevel* Level = GameLevel();
    FVector Location = NPawnView::PlayerPawn()->Location;
    Location.Z += 50.0f;
    DOUBLE Angle = appFrand() * 3.141592653589793;
    DOUBLE Distance = appFrand() * 300.0f;
    Location.X = (FLOAT)(appCos(Angle) * Distance + Location.X);
    Location.Y = (FLOAT)(appSin(Angle) * Distance + Location.Y);
    FName ControllerClass = FName(TEXT("Engine.PlayerController"), FNAME_Add);
    AController* Controller = (AController*)Level->SpawnActor(
        UObject::StaticLoadClass(AController::StaticClass(), NULL, *ControllerClass, NULL, 1, NULL),
        NAME_None, Location, FRotator(0, 0, 0), NULL, 1, 1, NULL, NULL);
    UClass*& PawnClass = Controller->PawnClass;
    PawnClass = UObject::StaticLoadClass(APawn::StaticClass(), NULL,
                                         Character.GetClassNamePointer(), NULL, 0, NULL);
    Controller->Pawn = (APawn*)Level->SpawnActor(
        PawnClass, NAME_None, Location, FRotator(0, 0, 0), NULL, 1, 0, Controller, NULL);
    Controller->Pawn->Controller = Controller;
    Controller->Role = ROLE_Authority;
    Controller->Pawn->Role = ROLE_Authority;
    Controller->Pawn->setPhysics(1, NULL, FVector(0.f, 0.f, 1.f));
    Character.Pawn = Controller->Pawn;
    Character.Unknown8 = 0;
    Character.SetPawnResource();
    APawn* Pawn = Controller->Pawn;
    Pawn->PlayAnim(0, Pawn->WaitAnimName[Pawn->WeaponType], 1.0f, 0.0f, 1, 0);
    Result.Pawn = Controller->Pawn;
    *Out = Result;
    return Out;
}
NW_HOOK_METHOD(0x100d51f0, NCPawnViewr_Simulator, SpawnPawn)

// 0x100d8260 (vslot 76) - by the skill share of the frame, a pawn not casting picks a skill (the first kind: any of
// the table; the bow kind: one of four, or an attack) and a random other pawn to use it on. The target is drawn again
// until it is another live pawn: with no other live pawn the loop never ends. Both random indexes use negated
// factors. Returns 0 when nothing was started (the paint then tries the cancel and the replacement)
INT NCPawnViewr_Simulator::TryAction(INT Index)
{
    INT BowSkills[6] = {56, 19, 24, 101, 0, 0};
    INT SkillID = -1;
    if (SkillRate * DeltaTime < appFrand() * 100.0f)
        return 0;
    NSimPawn& Sim = Pawns.First[Index];
    if (Sim.Kind == 0 || Sim.Pawn->ActionType == 5)
        return 0;
    if (Sim.Kind == 1)
        SkillID = SkillIDs[-(INT)(appFrand() * (DOUBLE)NumSkills * -0.99f)];
    else if (Sim.Kind == 2)
        SkillID = BowSkills[-(INT)(appFrand() * -5.94f)];
    INT Target;
    do
        Target = (INT)(appFrand() * (DOUBLE)NumPawns);
    while (Target == Index || Pawns.First[Target].Kind == 0 || Target >= NumPawns);

    if (SkillID == 0)
    {
        // an attack (the pawn's fields of the L2 action)
        APawn* Pawn = Pawns.First[Index].Pawn;
        APawn* TargetPawn = Pawns.First[Target].Pawn;
        Pawn->Controller->Unknown638 |= 2;
        Pawn->Controller->Unknown638 |= 4;
        Pawn->Controller->StopMove();
        Pawn->NActionStop();
        Pawn->ActionTarget = TargetPawn;
        Pawn->ActionType = (Pawn->WeaponType == 5) + 1;   // 5: a bow
        DWORD& Bits = Pawn->Unknown62C;
        Bits = (Bits & ~0xF) | ((((Target % 3 == 0) << 2) | (Target % 2 & 1)) << 1);
        Pawn->Unknown628 = 1;
        Pawn->Unknown63C.Empty(16);
        Pawn->Unknown634 = 0;
        Pawn->Unknown638 = 0;
        Pawn->Unknown5C0 = 1;
        return 1;
    }
    if (SkillID == -1)
        return 1;
    APawn* Pawn = Pawns.First[Index].Pawn;
    AActor* TargetPawn = Pawns.First[Target].Pawn;
    Pawn->Controller->StopMove();
    FL2MagicSkillData* Skill = GL2GameData.GetMSData(SkillID, 1);
    if (!Skill)
        return 0;
    FLOAT HitTime = *(FLOAT*)((BYTE*)Skill + 0x34);       // this client's record: the hit time at +0x34 (C3: +0x30)
    Pawn->SetMagicInfo(SkillID, 1, HitTime, HitTime, TargetPawn, 1.0f, NULL);
    AddMagicTarget(&Pawn->MagicTargets, &TargetPawn);
    if (Pawn->InitSkillProcess())
    {
        Pawn->ActionTarget = TargetPawn;
        Pawn->ActionType = 5;
        return 1;
    }
    Pawn->ActionType = 0;
    return 1;
}
NW_HOOK_METHOD(0x100d8260, NCPawnViewr_Simulator, TryAction)

// 0x100d8500 (vslot 77) - by the cancel share, the pawn's cast stops
INT NCPawnViewr_Simulator::TryCancel(INT Index)
{
    FLOAT Roll = appFrand() * 100.0f;
    if (CancelRate * DeltaTime > Roll)
    {
        Pawns.First[Index].Pawn->MagicCancel();
        return 1;
    }
    return 0;
}
NW_HOOK_METHOD(0x100d8500, NCPawnViewr_Simulator, TryCancel)

// 0x100d8550 (vslot 78) - by the replacement share, another pawn takes its place (the first one never goes); its
// controller is flagged unless it is the player's
INT NCPawnViewr_Simulator::TryReplace(INT Index)
{
    NSimPawn Spawned;
    if (Index == 0)
        return 0;
    FLOAT Roll = appFrand() * 100.0f;
    if (ReplaceRate * DeltaTime > Roll)
    {
        AController* Controller = Pawns.First[Index].Pawn->Controller;
        if (Controller != ViewActor())
            Controller->Unknown98 |= 4;
        GameLevel()->DestroyActor(Pawns.First[Index].Pawn, 0);
        Pawns.First[Index] = *SpawnPawn(&Spawned);
    }
    return 1;
}
NW_HOOK_METHOD(0x100d8550, NCPawnViewr_Simulator, TryReplace)

NW_CHECK_VTABLE(NCPawnViewr_Simulator, 0x101c3290, 79)
