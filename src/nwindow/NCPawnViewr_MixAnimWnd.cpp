/*=============================================================================
    NCPawnViewr_MixAnimWnd.cpp - the pawn viewer's page of combined animations. Stock:
    0x10088260 .. 0x1008844a, 0x10089620 .. 0x10089f06.
=============================================================================*/
#include "nwindow/NCPawnViewrWnd.h"
#include "nwindow/NCButton.h"
#include "nwindow/NCEditBox.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NConsoleWnd.h"

#include <stdlib.h>

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

// TArray<FName>::Empty is one out-of-line function in the stock image (C4: 0x1002EA50, thiscall on the array with the
// slack, ret 4): the pawn's animation steps are emptied by a call to it, not by the array's inlined body. The standalone
// compiles the template's member itself (the same behavior)
static FORCEINLINE void EmptyNameArray(TArray<FName>* Array)
{
#if NW_HOOK_MODE
    ::nw::StockCallAt<void>(0x1002EA50, Array, 0);
#else
    Array->Empty(0);
#endif
}


// NWM_COMMAND ids (names ours); the edit lines are 0xFF..0x101
enum
{
    NID_MIXANIM_TAKE1 = 0xFB,
    NID_MIXANIM_TAKE2 = 0xFC,
    NID_MIXANIM_TAKE3 = 0xFD,
    NID_MIXANIM_START = 0xFE,
    NID_MIXANIM_EDIT = 0xFF,
    NID_MIXANIM_SPEED = 0x128,          // C4
};

// sysstring lines
enum { STR_Start = 428, STR_Use = 443 };

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

// the animation selected in the character window's list into an edit line (inline in each
// "Use" button's handler, name ours)
static FORCEINLINE void TakeSelectedAnim(NCEditBox* Edit)
{
    NCPawnView_CharacterSelect_List* List = NPawnView::Viewer()->CharSelectWnd->AnimList;
    if (List && List->Selected >= 0)
    {
        Edit->ClearText();
        EditInsertText(Edit, (const TCHAR*)List->GetItemParam(List->Selected), 0);
    }
}

// 0x100d8970 - the controls are made in OnCreate (the step arrays are left to the allocator, as in the stock code)
NCPawnViewr_MixAnimWnd::NCPawnViewr_MixAnimWnd()
{
    TakeButtons[0] = NULL;
    TakeButtons[1] = NULL;
    TakeButtons[2] = NULL;
    StartButton = NULL;
    AnimEdits[0] = NULL;
    AnimEdits[1] = NULL;
    AnimEdits[2] = NULL;
    SpeedEdit = NULL;
}
NW_HOOK_CTOR(0x100d8970, NCPawnViewr_MixAnimWnd)

// 0x100d89f0
NW_HOOK_DTOR(0x100d89f0, NCPawnViewr_MixAnimWnd)

// 0x100d17e0 (vslot 25)
INT NCPawnViewr_MixAnimWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCWnd::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND)
    {
        if (wParam == NID_MIXANIM_TAKE1)
            return OnTake1() != 0;
        if (wParam == NID_MIXANIM_TAKE2)
            return OnTake2() != 0;
        if (wParam == NID_MIXANIM_TAKE3)
            return OnTake3() != 0;
        if (wParam == NID_MIXANIM_START)
            return OnStart() != 0;
    }
    return 0;
}
NW_HOOK_METHOD(0x100d17e0, NCPawnViewr_MixAnimWnd, WindowProc)

// 0x100d6570 (vslot 37) - "Use" beside each animation line (disabled: only the buttons fill them), the speed ("1.8")
// and "Start". C4: the last row is at y 120 (C3: 140) and the speed line has an id (0x128)
INT NCPawnViewr_MixAnimWnd::OnCreate()
{
    NCWnd::OnCreate();
    // the three "Use" buttons, written out in the stock code
    TakeButtons[0] = NewStock<NCButton>(0x10003340, TEXT("L2UI.Control.DefaultButton"),
                                        TEXT("L2UI.Control.DefaultButton_click"), 0);
    TakeButtons[0]->ID = NID_MIXANIM_TAKE1;
    TakeButtons[0]->SetPosition(20, 25, 28, 16);
    CreateChild(TakeButtons[0], NWS_VISIBLE);
    TakeButtons[0]->SetTextID(STR_Use);
    TakeButtons[1] = NewStock<NCButton>(0x10003340, TEXT("L2UI.Control.DefaultButton"),
                                        TEXT("L2UI.Control.DefaultButton_click"), 0);
    TakeButtons[1]->ID = NID_MIXANIM_TAKE2;
    TakeButtons[1]->SetPosition(20, 55, 28, 16);
    CreateChild(TakeButtons[1], NWS_VISIBLE);
    TakeButtons[1]->SetTextID(STR_Use);
    TakeButtons[2] = NewStock<NCButton>(0x10003340, TEXT("L2UI.Control.DefaultButton"),
                                        TEXT("L2UI.Control.DefaultButton_click"), 0);
    TakeButtons[2]->ID = NID_MIXANIM_TAKE3;
    TakeButtons[2]->SetPosition(20, 85, 28, 16);
    CreateChild(TakeButtons[2], NWS_VISIBLE);
    TakeButtons[2]->SetTextID(STR_Use);
    StartButton = NewStock<NCButton>(0x10003340, TEXT("L2UI.Control.DefaultButton"),
                                     TEXT("L2UI.Control.DefaultButton_click"), 0);
    StartButton->ID = NID_MIXANIM_START;
    StartButton->SetPosition(120, 120, 38, 16);
    CreateChild(StartButton, NWS_VISIBLE);
    StartButton->SetTextID(STR_Start);
    for (INT i = 0; i < 3; i++)
    {
        AnimEdits[i] = NewStock<NCEditBox>(0x100112C0, 1, (const TCHAR*)NULL, 1, 64, 0);
        AnimEdits[i]->ID = NID_MIXANIM_EDIT + i;
        AnimEdits[i]->SetPosition(75, 25 + 30 * i, 140, 16);
        CreateChild(AnimEdits[i], NWS_VISIBLE);
        AnimEdits[i]->EnableWindow(0);
    }
    SpeedEdit = NewStock<NCEditBox>(0x100112C0, 1, (const TCHAR*)NULL, 1, 64, 0);
    SpeedEdit->ID = NID_MIXANIM_SPEED;
    SpeedEdit->SetPosition(65, 120, 35, 16);
    CreateChild(SpeedEdit, NWS_VISIBLE);
    EditInsertText(SpeedEdit, TEXT("1.8"), 1);
    return 1;
}
NW_HOOK_METHOD(0x100d6570, NCPawnViewr_MixAnimWnd, OnCreate)

// 0x100d3700 (vslot 38) - C4 deletes the speed line too
INT NCPawnViewr_MixAnimWnd::OnDestroy()
{
    DestroyChild(this, TakeButtons[0]);
    DestroyChild(this, TakeButtons[1]);
    DestroyChild(this, TakeButtons[2]);
    DestroyChild(this, StartButton);
    DestroyChild(this, AnimEdits[0]);
    DestroyChild(this, AnimEdits[1]);
    DestroyChild(this, AnimEdits[2]);
    DestroyChild(this, SpeedEdit);
    NCWnd::OnDestroy();
    return 1;
}
NW_HOOK_METHOD(0x100d3700, NCPawnViewr_MixAnimWnd, OnDestroy)

// 0x100d35e0 (vslot 62) - "HITTIME" centered in a 50 x 16 box left of the speed; the origin goes on before the clip
INT NCPawnViewr_MixAnimWnd::OnPaint(NCGDevice* Device)
{
    guard(NCPawnViewr_MixAnimWnd::OnPaint);
    SIZE Size;
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, Width, Height);
    NCGDevice::GetTextExtent(&Size, TEXT("HITTIME"), 0, 0, 0, 0, 0);
    Device->DrawText((50 - Size.cx) / 2 + 10, (16 - Size.cy) / 2 + 120, 0xFFDCDCDC,
                     TEXT("HITTIME"), 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100d35e0, NCPawnViewr_MixAnimWnd, OnPaint)

// 0x100d1890 (vslot 75)
INT NCPawnViewr_MixAnimWnd::OnTake1()
{
    TakeSelectedAnim(AnimEdits[0]);
    return 1;
}
NW_HOOK_METHOD(0x100d1890, NCPawnViewr_MixAnimWnd, OnTake1)

// 0x100d18f0 (vslot 76)
INT NCPawnViewr_MixAnimWnd::OnTake2()
{
    TakeSelectedAnim(AnimEdits[1]);
    return 1;
}
NW_HOOK_METHOD(0x100d18f0, NCPawnViewr_MixAnimWnd, OnTake2)

// 0x100d1950 (vslot 77)
INT NCPawnViewr_MixAnimWnd::OnTake3()
{
    TakeSelectedAnim(AnimEdits[2]);
    return 1;
}
NW_HOOK_METHOD(0x100d1950, NCPawnViewr_MixAnimWnd, OnTake3)

// 0x100d69f0 (vslot 78) - the three animations as the steps of a skill of the player's pawn on the player's target
// (C3: SetMagicInfo's fields written here: the cast record at +0x4A8 cleared, the pawn's +0x354 array emptied, the speed,
// the position, a few constants and the three animation names at +0x52C), at the speed typed (read with the C
// runtime's _wtof, unlike the simulator's appAtof)
INT NCPawnViewr_MixAnimWnd::OnStart()
{
    if (!SpeedEdit->CommitText())
        return 1;
    FLOAT Speed = (FLOAT)_wtof(SpeedEdit->CommitText());
    if (!AnimEdits[0]->CommitText() || !AnimEdits[1]->CommitText() ||
        !AnimEdits[2]->CommitText() || !(Speed > 0.0f))
        return 1;
    APlayerController* Controller = ViewActor();
    AActor* Target = Controller->Target;
    APawn* Pawn = Controller->Pawn;
    BYTE* P = (BYTE*)Pawn;
    Pawn->Controller->StopMove();
    ((FNMagicInfo*)(P + 0x4C0))->Clear(1);
    EmptyNameArray((TArray<FName>*)(P + 0x364));
    *(FLOAT*)(P + 0x4E0) = Speed;
    *(FVector*)(P + 0x508) = *(FVector*)(P + 0x304);
    *(INT*)(P + 0x514) = 0;
    *(INT*)(P + 0x518) = 0x50000;
    *(INT*)(P + 0x4FC) = 9;
    *(AActor**)(P + 0x4F0) = Target;
    *(FLOAT*)(P + 0x4F4) = 1.0f;
    *(INT*)(P + 0x4EC) = 0;
    *(INT*)(P + 0x51C) = 0;
    *(INT*)(P + 0x504) = 3;
    *(FName*)(P + 0x550) = FName(AnimEdits[0]->CommitText(), FNAME_Add);
    *(FName*)(P + 0x554) = FName(AnimEdits[1]->CommitText(), FNAME_Add);
    *(FName*)(P + 0x558) = FName(AnimEdits[2]->CommitText(), FNAME_Add);
    if (Pawn->InitSkillProcess())
    {
        *(INT*)(P + 0x578) = 5;
        *(AActor**)(P + 0x588) = Target;
    }
    return 1;
}
NW_HOOK_METHOD(0x100d69f0, NCPawnViewr_MixAnimWnd, OnStart)

// 0x100d34a0 (vslot 68; name ours) - the animation Name of line Index: the pawn's mesh gives its rate and its number
// of frames; the time of the three lines (0.2 + frames / rate of each that has a rate) goes into the speed line as
// "%2.2f" (C3: new; nothing in the ported code calls it)
void NCPawnViewr_MixAnimWnd::SetStepAnim(const TCHAR* Name, INT Index)
{
    TCHAR Text[128];
    FMeshAnimSeq* Seq = ViewActor()->Pawn->FindSeq(FName(Name, FNAME_Add));
    if (Seq)
    {
        StepRate[Index] = *(FLOAT*)((BYTE*)Seq + 0x18);
        StepFrames[Index] = *(INT*)((BYTE*)Seq + 0x14);
    }
    TotalTime = 0.2f;
    if (StepRate[0] > 0.0f)
        TotalTime = (FLOAT)StepFrames[0] / StepRate[0] + 0.2f;
    if (StepRate[1] > 0.0f)
        TotalTime = (FLOAT)StepFrames[1] / StepRate[1] + TotalTime;
    if (StepRate[2] > 0.0f)
        TotalTime = (FLOAT)StepFrames[2] / StepRate[2] + TotalTime;
    SpeedEdit->ClearText();
    appSprintf(Text, TEXT("%2.2f"), (DOUBLE)TotalTime);
    SpeedEdit->ClearText();
    EditInsertText(SpeedEdit, Text, 1);
}
NW_HOOK_METHOD(0x100d34a0, NCPawnViewr_MixAnimWnd, SetStepAnim)

NW_CHECK_VTABLE(NCPawnViewr_MixAnimWnd, 0x101c2d28, 79)
