/*=============================================================================
    NCPawnView_CharacterSelectWnd.cpp - the pawn viewer's character window and its
    animation list. Stock: 0x10088140 .. 0x100881ec, 0x10088450 .. 0x10088592,
    0x10089140 .. 0x100892cf, 0x10089f10 .. 0x1008a007, 0x1008c3c0 .. 0x1008c719,
    0x1008cd20 .. 0x1008d13f, 0x1008d960 .. 0x1008dc73; OnDestroy and five of the list's
    slots are bodies shared with other classes.
=============================================================================*/
#include "nwindow/NCPawnViewrWnd.h"
#include "nwindow/NCButton.h"
#include "nwindow/NCCheckBox.h"
#include "nwindow/NCEditBox.h"
#include "nwindow/NCComboBox.h"
#include "nwindow/NCFrameCtrl.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NConsoleWnd.h"

#include <intrin.h>

// NWM_COMMAND ids (names ours)
enum
{
    NID_CHARSELECT_FRAME = (INT)0xAFF12011,
    NID_CHARSELECT_BYACTION = 0x101,
    NID_CHARSELECT_NAME = 0x1F04,
    NID_CHARSELECT_ANINAME = 0x1F05,   // handled, no control sends it
    NID_CHARSELECT_NEW129 = 0x129,     // C4 (slot 77): the play button
    NID_CHARSELECT_NEW12A = 0x12A,     // C4 (slot 78): the stop button
    NID_CHARSELECT_SPEED = 0x12B,      // C4: the rate edit line
    NID_CHARSELECT_LOOP = 0x12C,       // C4: the loop check box
};

// UpdateAnimList's groups: the animations whose name holds these come first, in this order
static const TCHAR GAnimActionKeys[5][10] = {
    TEXT("run"), TEXT("walk"), TEXT("atk"), TEXT("wait"), TEXT("social")};

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

// a sequence of the animation set in this client: 0x44 bytes, the name first (UE2Engine.h has the C2 record, 0x40)
struct NAnimSeq
{
    FName Name;
    BYTE _Unknown4[0x44 - 0x4];
};

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

/*-----------------------------------------------------------------------------
    NCPawnView_CharacterSelect_List.
-----------------------------------------------------------------------------*/

// 0x100d8a40 - NCDebugWnd's default constructor, then this class's vtable
NCPawnView_CharacterSelect_List::NCPawnView_CharacterSelect_List()
{
}
NW_HOOK_CTOR(0x100d8a40, NCPawnView_CharacterSelect_List)

// 0x100d8ae0 (ret 8) - NCDebugWnd's default constructor, the size of the list and no animation playing
NCPawnView_CharacterSelect_List::NCPawnView_CharacterSelect_List(INT InMaxLines, INT InVisibleLines)
{
    MaxLines = InMaxLines;
    VisibleLines = InVisibleLines;
    PlayingIndex = -1;
}
NW_HOOK_CTOR_ARGS(0x100d8ae0, NCPawnView_CharacterSelect_List, (INT A, INT B), (A, B))


// stock body shared with NCNpcView_CharacterSelect_List's (0x1009c940, hooked there)
INT NCPawnView_CharacterSelect_List::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return NCDebugWnd::WindowProc(Msg, wParam, lParam) != 0;
}

// 0x100d19b0 (vslot 37)
INT NCPawnView_CharacterSelect_List::OnCreate()
{
    guard(NCPawnView_CharacterSelect_List::OnCreate);
    NCDebugWnd::OnCreate();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100d19b0, NCPawnView_CharacterSelect_List, OnCreate)

// 0x100d1ab0 (vslot 38)
INT NCPawnView_CharacterSelect_List::OnDestroy()
{
    guard(NCPawnView_CharacterSelect_List::OnDestroy);
    NCDebugWnd::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100d1ab0, NCPawnView_CharacterSelect_List, OnDestroy)

// 0x100d1a30 (vslot 62)
INT NCPawnView_CharacterSelect_List::OnPaint(NCGDevice* Device)
{
    guard(NCPawnView_CharacterSelect_List::OnPaint);
    NCDebugWnd::OnPaint(Device);
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100d1a30, NCPawnView_CharacterSelect_List, OnPaint)

// stock body shared with NCNpcView_CharacterSelect_List's (0x1007a730)
INT NCPawnView_CharacterSelect_List::OnMouseWheel(WPARAM wParam, LPARAM lParam)
{
    NCListBox::OnMouseWheel(wParam, lParam);
    return 1;
}

// 0x100d3880 - the animation of the selected line (its param is the sequence name) plays on the player's pawn, looped
// for a double click and once for the right button; on the line that plays already (the pawn's animation flag 0x10 set) it
// goes back to the idle animation of the pawn's weapon instead
__declspec(noinline) INT NCPawnView_CharacterSelect_List::PlayAnim(INT bLoop)
{
    guard(NCPawnView_CharacterSelect_List::PlayAnim);
    TCHAR* Name = (TCHAR*)GetItemParam(Selected);
    APawn* Pawn = NPawnView::PlayerPawn();
    if (PlayingIndex == Selected && (Pawn->AnimFlags & 0x10))
    {
        Pawn->AnimFlags &= ~0x10;
        Pawn->Unknown13D4 |= 1;
        Pawn->PlayAnim(0, Pawn->WaitAnimName[Pawn->WeaponType], 1.0f, 0.0f, 1, 0);
    }
    else
    {
        Pawn->AnimFlags |= 0x10;
        Pawn->Unknown13D4 &= ~1;
        Pawn->PlayAnim(0, FName(Name, FNAME_Add), Pawn->Unknown694, 0.0f, bLoop, 1);
        PlayingIndex = Selected;
    }
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100d3880, NCPawnView_CharacterSelect_List, PlayAnim)

// 0x100d6bd0 (vslot 78) - a double click plays the animation in a loop
INT NCPawnView_CharacterSelect_List::OnLButtonDblClick(WPARAM wParam, LPARAM lParam)
{
    return PlayAnim(1);
}
NW_HOOK_METHOD(0x100d6bd0, NCPawnView_CharacterSelect_List, OnLButtonDblClick)

// 0x100d6be0 (vslot 70) - the right button plays it once
INT NCPawnView_CharacterSelect_List::OnRButtonDown(WPARAM wParam, LPARAM lParam)
{
    return PlayAnim(0);
}
NW_HOOK_METHOD(0x100d6be0, NCPawnView_CharacterSelect_List, OnRButtonDown)

// stock body shared with NCNpcView_CharacterSelect_List's (0x1007a6f0)
INT NCPawnView_CharacterSelect_List::ScrollUp()
{
    NCListBox::ScrollUp();
    return 1;
}

// stock body shared with NCNpcView_CharacterSelect_List's (0x1007a700)
INT NCPawnView_CharacterSelect_List::ScrollDown()
{
    NCListBox::ScrollDown();
    return 1;
}

// stock body shared with NCNpcView_CharacterSelect_List's (0x1007a710)
INT NCPawnView_CharacterSelect_List::SetTopIndex(INT Index)
{
    NCListBox::SetTopIndex(Index);
    return 1;
}

// 0x100d8a90 - the vtable, ConditionalDestroy, NCDebugWnd
NW_HOOK_DTOR(0x100d8a90, NCPawnView_CharacterSelect_List)
// 0x100d8920 - the vtable, ConditionalDestroy, NCWnd
NW_HOOK_DTOR(0x100d8920, NCPawnView_CharacterSelectWnd)

NW_CHECK_VTABLE(NCPawnView_CharacterSelect_List, 0x101c2e68, 82)

/*-----------------------------------------------------------------------------
    NCPawnView_CharacterSelectWnd.
-----------------------------------------------------------------------------*/

// 0x100d88a0
NCPawnView_CharacterSelectWnd::NCPawnView_CharacterSelectWnd()
{
    TitleTex = NULL;
    BackTex = NULL;
    UnknownA8 = NULL;
    UnknownAC = NULL;
    UnknownB0 = NULL;
    UnknownB4 = 0;
    Selected = -1;
}
NW_HOOK_CTOR(0x100d88a0, NCPawnView_CharacterSelectWnd)


// 0x100d16a0 (vslot 25) - the name combo box and the animation combo box (nothing sends the second), the sort check box
// and the two C4 controls (0x129, 0x12A: slots 77 and 78)
INT NCPawnView_CharacterSelectWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCWnd::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND)
    {
        if (wParam == NID_CHARSELECT_NAME)
            return OnNameComboBox((INT)lParam) != 0;
        if (wParam == NID_CHARSELECT_ANINAME)
            return OnAniNameComboBox((INT)lParam) != 0;
        if (wParam == NID_CHARSELECT_BYACTION)
            return OnByActionCheck() != 0;
        if (wParam == NID_CHARSELECT_NEW129)
            return ::nw::VirtualCall<INT>(this, 77) != 0;
        if (wParam == NID_CHARSELECT_NEW12A)
            return ::nw::VirtualCall<INT>(this, 78) != 0;
    }
    return 0;
}
NW_HOOK_METHOD(0x100d16a0, NCPawnView_CharacterSelectWnd, WindowProc)

// 0x100d8e20 (vslot 37) - a title bar without buttons, the textures, the character combo, the animation list (500
// lines, 18 shown), "by action" (unchecked), and C4's play and stop buttons, loop check box and rate edit line under the list
INT NCPawnView_CharacterSelectWnd::OnCreate()
{
    guard(NCPawnView_CharacterSelectWnd::OnCreate);
    NCWnd::OnCreate();
    FrameCtrl = NewStock<NCFrameCtrl>(0x100126E0, 0, 0, 0);
    FrameID = NID_CHARSELECT_FRAME;
    FrameCtrl->ID = NID_CHARSELECT_FRAME;
    FrameCtrl->SetPosition(2, 3, (INT)(Width - 6.0f), 20);
    CreateChild(FrameCtrl, NWS_VISIBLE);
    TitleTex = FindTexture(TEXT("sek.cbui168"));
    BackTex = FindTexture(TEXT("sek.cbui144"));
    UnknownA8 = FindTexture(TEXT("sek.cbui67"));
    UnknownAC = FindTexture(TEXT("sek.cbui68"));
    UnknownB0 = FindTexture(TEXT("sek.cbui69"));

    NameCombo = NewStock<NCComboBox>(0x10006A10);
    NameCombo->ID = NID_CHARSELECT_NAME;
    NameCombo->SetPosition(10, 28, 129, 17);
    CreateChild(NameCombo, NWS_VISIBLE);

    AnimList = NewStock<NCPawnView_CharacterSelect_List>(0x100D8AE0, 500, 18);
    AnimList->SetPosition(10, 50, 185, 218);
    CreateChild(AnimList, NWS_VISIBLE);

    // "by action", C4: real Unicode
    ByActionCheck = NewStock<NCCheckBox>(0x10005720, TEXT("\xC561\xC158\xBCC4"), 0, 0, 0);
    ByActionCheck->ID = NID_CHARSELECT_BYACTION;
    ByActionCheck->SetPosition(145, 30, 50, 16);
    CreateChild(ByActionCheck, NWS_VISIBLE);
    ByActionCheck->bChecked = 0;

    PlayButton = NewStock<NCButton>(0x10003340, TEXT("L2UI.Control.DefaultButton"), TEXT("L2UI.Control.DefaultButton_click"), 0);
    PlayButton->ID = NID_CHARSELECT_NEW129;
    PlayButton->SetPosition(10, 275, 30, 16);
    CreateChild(PlayButton, NWS_VISIBLE);
    PlayButton->SetText(TEXT("\xC7AC\xC0DD"));                  // "play"

    StopButton = NewStock<NCButton>(0x10003340, TEXT("L2UI.Control.DefaultButton"), TEXT("L2UI.Control.DefaultButton_click"), 0);
    StopButton->ID = NID_CHARSELECT_NEW12A;
    StopButton->SetPosition(45, 275, 30, 16);
    CreateChild(StopButton, NWS_VISIBLE);
    StopButton->SetText(TEXT("\xC815\xC9C0"));                  // "stop"

    LoopCheck = NewStock<NCCheckBox>(0x10005720, TEXT("LOOP"), 0, 0, 0);
    LoopCheck->ID = NID_CHARSELECT_LOOP;
    LoopCheck->SetPosition(80, 275, 50, 16);
    CreateChild(LoopCheck, NWS_VISIBLE);
    LoopCheck->bChecked = 0;

    SpeedEdit = NewStock<NCEditBox>(0x100112C0, 1, (const TCHAR*)NULL, 1, 64, 0);
    SpeedEdit->ID = NID_CHARSELECT_SPEED;
    SpeedEdit->SetPosition(40, 295, 40, 16);
    CreateChild(SpeedEdit, NWS_VISIBLE);
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100d8e20, NCPawnView_CharacterSelectWnd, OnCreate)

// 0x100d31a0 (vslot 38) - the combo box, the list and C4's four controls go (the "by action" check box is left alone; the
// title bar is left to NCWnd::OnDestroy)
INT NCPawnView_CharacterSelectWnd::OnDestroy()
{
    DestroyChild(this, NameCombo);
    DestroyChild(this, AnimList);
    DestroyChild(this, PlayButton);
    DestroyChild(this, StopButton);
    DestroyChild(this, LoopCheck);
    DestroyChild(this, SpeedEdit);
    NCWnd::OnDestroy();
    return 1;
}
NW_HOOK_METHOD(0x100d31a0, NCPawnView_CharacterSelectWnd, OnDestroy)

// 0x100d2ef0 (vslot 62) - the background stretched 20 px past the right edge, the title strip; the clip goes on
// before the origin (the guard names NCCharacterSelectWnd)
INT NCPawnView_CharacterSelectWnd::OnPaint(NCGDevice* Device)
{
    guard(NCCharacterSelectWnd::OnPaint);
    Device->SetClip(ScreenX, ScreenY, Width, Height);
    Device->BeginPaint(ScreenX, ScreenY);
    if (BackTex)
        Device->DrawStretchedTex(0, 0, (INT)(Width + 20.0f), (INT)Height, BackTex);
    if (TitleTex)
    {
        INT TitleWidth = (INT)(Width - 4.0f);
        Device->DrawTexture(2, 3, TitleWidth, 20, 0, 0, TitleWidth, 20, TitleTex);
    }
    // C4: under the list, the frame the pawn's animation is at and its rate ("frame / of frames"); the play button reads
    // "play" while the pawn idles
    APawn* Pawn = NPawnView::PlayerPawn();
    FName Wait = Pawn->GetCurWaitAnimName();
    FName Playing = Pawn->GetCurAnimName(0);
    if (Playing == Wait)
        PlayButton->SetText(TEXT("\xC7AC\xC0DD"));                  // "play"
    FName Current = Pawn->GetCurAnimName(0);
    INT Sequence = ::nw::VirtualCall<INT>(Pawn->MeshInstance, 45, *(INT*)&Current);
    // (a float result comes back in st0: the call is written out, nw::VirtualCall does not inline for it)
    typedef FLOAT(__thiscall * FFramesFn)(void*, INT);
    FLOAT Frames = ((FFramesFn)(*(void***)Pawn->MeshInstance)[49])(Pawn->MeshInstance, Sequence);
    FLOAT Position = *(FLOAT*)(*(BYTE**)((BYTE*)Pawn->MeshInstance + 0x184) + 0x10);
    // the buffer is cleared inline in the stock code (a word, 63 dwords and a word)
    TCHAR Text[128];
    Text[0] = 0;
    __stosd((unsigned long*)(Text + 1), 0, 63);
    __stosw((unsigned short*)(Text + 127), 0, 1);
    appSprintf(Text, TEXT("%2.1f/%2.1f"), (DOUBLE)(Position * Frames), (DOUBLE)Frames);
    SIZE Size;
    NCGDevice::GetTextExtent(&Size, TEXT("1"), 0, 0, 0, 0, 0);
    Device->DrawText(85, (16 - Size.cy) / 2 + 295, 0xFFDCDCDC, TEXT("Frame:"), 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
    Device->DrawText(125, (16 - Size.cy) / 2 + 295, 0xFFDCDCDC, Text, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
    Device->DrawText(10, (16 - Size.cy) / 2 + 295, 0xFFDCDCDC, TEXT("\xC18D\xB3C4:"), 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);   // "speed:"
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100d2ef0, NCPawnView_CharacterSelectWnd, OnPaint)

// 0x100d7a70 (vslot 74) - another default character: a new PlayerController is spawned only to own the new pawn and
// carry its class (it is left in the level), the pawn of that class replaces the player's 30 units higher under the
// player's own controller, falls to the ground, is dressed as the record (its volumes set, the name combo kept on it)
// and plays its idle animation; then the animations list and the hair window's values follow
INT NCPawnView_CharacterSelectWnd::OnNameComboBox(INT Index)
{
    guard(NCPawnView_CharacterSelectWnd::OnNameComboBox);
    if (Index > 19 || Index == Selected)
        return 1;
    ULevel* Level = GameLevel();
    APlayerController* PlayerController = ViewActor();
    FName ControllerClass = FName(TEXT("Engine.PlayerController"), FNAME_Add);
    AController* pController = (AController*)Level->SpawnActor(
        UObject::StaticLoadClass(AController::StaticClass(), NULL, *ControllerClass, NULL, 1, NULL));
    if (!pController)
        appFailAssert("pController", "Src\\NPawnViewerWnd.cpp", 1606);
    User Character = NPawnView::Viewer()->Characters[Index];
    UClass*& PawnClass = pController->PawnClass;
    PawnClass = UObject::StaticLoadClass(APawn::StaticClass(), NULL,
                                         Character.GetClassNamePointer(), NULL, 0, NULL);
    if (!PawnClass)
        return 1;
    FVector Location = NPawnView::PlayerPawn()->Location;
    Location.Z += 30.0f;
    APawn* Pawn = (APawn*)Level->SpawnActor(PawnClass, NAME_None, Location, FRotator(0, 0, 0),
                                            NULL, 1, 1, pController, NULL);
    if (!Pawn)
        return 1;
    Level->DestroyActor(NPawnView::PlayerPawn(), 0);
    ViewActor()->Pawn = Pawn;
    Pawn->Controller = PlayerController;
    Pawn->Role = ROLE_Authority;
    Pawn->setPhysics(1, NULL, FVector(0.f, 0.f, 1.f));
    Selected = Index;
    NPawnView::Viewer()->Characters[Index].Pawn = Pawn;
    NPawnView::Viewer()->Characters[Selected].SetPawnResource();
    Pawn->SetVolumes();
    NameCombo->SetSelected(Selected);
    Pawn->PlayAnim(0, Pawn->GetCurWaitAnimName(), 1.0f, 0.0f, 1, 0);
    UpdateAnimList();
    NPawnView::Viewer()->HairChangeWnd->SyncFromCharacter();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100d7a70, NCPawnView_CharacterSelectWnd, OnNameComboBox)

// 0x100d32c0 (the body of NewVirtual77, the play button) - the animation of the chosen line plays at the rate in the rate line
// (the pawn's own when the line is empty); when it is the animation already playing, the button pauses it (the channel's
// rate to 0, the button reads "play") or resumes it at the rate (the button reads "wait"); else it plays from the start,
// looped when LOOP is checked
INT NCPawnView_CharacterSelectWnd::OnPlayAnim()
{
    TCHAR* Name = (TCHAR*)AnimList->GetItemParam(AnimList->Selected);
    APawn* Pawn = NPawnView::PlayerPawn();
    FLOAT Rate = Pawn->Unknown694;
    // the buffer is cleared inline in the stock code (a word, 63 dwords and a word)
    TCHAR Text[128];
    Text[0] = 0;
    __stosd((unsigned long*)(Text + 1), 0, 63);
    __stosw((unsigned short*)(Text + 127), 0, 1);
    appStrcpy(Text, SpeedEdit->CommitText());
    if (appStrcmp(Text, TEXT("")) != 0)
        Rate = appAtof(Text);
    FName Anim(Name, FNAME_Add);
    FName Playing = Pawn->GetCurAnimName(0);
    if (Playing == Anim)
    {
        MeshAnimChannel* Channel = (MeshAnimChannel*)static_cast<USkeletalMeshInstance*>(Pawn->MeshInstance)->Blends.GetData();
        if (Channel->AnimRate == 0.0f)
        {
            PlayButton->SetText(TEXT("\xB300\xAE30"));                   // "wait"
            Channel->AnimRate = Rate;
            return 1;
        }
        PlayButton->SetText(TEXT("\xC7AC\xC0DD"));                       // "play"
        Channel->AnimRate = 0.0f;
        return 1;
    }
    PlayButton->SetText(TEXT("\xB300\xAE30"));                           // "wait"
    Pawn->PlayAnim(0, Anim, Rate, 0.0f, LoopCheck->bChecked, 1);
    return 1;
}
NW_HOOK_METHOD(0x100d32c0, NCPawnView_CharacterSelectWnd, OnPlayAnim)

// 0x100d3450 (the body of NewVirtual78, the stop button) - the pawn plays its idle animation again
INT NCPawnView_CharacterSelectWnd::OnStopAnim()
{
    APawn* Pawn = NPawnView::PlayerPawn();
    Pawn->PlayAnim(0, Pawn->WaitAnimName[Pawn->WeaponType], 1.0f, 0.0f, 1, 0);
    return 1;
}
NW_HOOK_METHOD(0x100d3450, NCPawnView_CharacterSelectWnd, OnStopAnim)

// 0x100d7dd0 (vslot 76)
INT NCPawnView_CharacterSelectWnd::OnByActionCheck()
{
    UpdateAnimList();
    return 1;
}
NW_HOOK_METHOD(0x100d7dd0, NCPawnView_CharacterSelectWnd, OnByActionCheck)

// 0x100d5f80 - the animations of the player's pawn mesh into the list. C4: with the check box off they come in the mesh's
// order, with it on first those whose name holds each of five action keys (run, walk, atk, wait, social), then the rest;
// a line's param is the name itself (C3 had weapon keys for the unchecked state). The animation set's sequences are
// FMeshAnimSeq records of 0x74 bytes at +0x6C
__declspec(noinline) void NCPawnView_CharacterSelectWnd::UpdateAnimList()
{
    TArray<INT> Remaining;
    APawn* Pawn = NPawnView::PlayerPawn();
    if (!Pawn)
        return;
    UMesh* Mesh = Pawn->Mesh;
    if (!Mesh)
        return;
    Mesh->MeshGetInstance(Pawn);
    UMeshAnimation* Anim = static_cast<USkeletalMesh*>(Mesh)->DefaultAnim;
    TArray<FMeshAnimSeq>& Seqs = Anim->AnimSeqs;
    INT NumSequences = Seqs.Num();
    AnimList->Clear();
    if (!ByActionCheck->bChecked)
    {
        for (INT i = 0; i < NumSequences; i++)
        {
            FName Name = Seqs(i).Name;
            AnimList->AddString(*Name, 0xFFDCDCDC, (INT)*Name);
        }
    }
    else
    {
        for (INT i = 0; i < NumSequences; i++)
            Remaining.AddItem(i);
        for (INT k = 0; k < 5; k++)
        {
            for (INT i = 0; i < NumSequences; i++)
            {
                FName Name = Seqs(i).Name;
                INT Index = Remaining.FindItemIndex(i);
                if (Index != -1 && appStristr(*Name, GAnimActionKeys[k]) && AnimList)
                {
                    Remaining.Remove(Index);
                    AnimList->AddString(*Name, 0xFFDCDCDC, (INT)*Name);
                }
            }
        }
        for (INT i = 0; i < Remaining.Num(); i++)
            AnimList->AddString(*Seqs(Remaining(i)).Name, 0xFFDCDCDC, (INT)*Seqs(Remaining(i)).Name);
    }
}
NW_HOOK_METHOD(0x100d5f80, NCPawnView_CharacterSelectWnd, UpdateAnimList)

NW_CHECK_VTABLE(NCPawnView_CharacterSelectWnd, 0x101c2be8, 79)
