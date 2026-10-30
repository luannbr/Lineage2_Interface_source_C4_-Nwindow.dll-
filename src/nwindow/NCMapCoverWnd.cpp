/*=============================================================================
    NCMapCoverWnd.cpp - the world map window around the NCMinimapWnd. C3 stock: 0x100abd90 (WindowProc), 0x100abe50
    (Minimize), 0x100ac630 / 0x100ac6d0 (paint, destroy), 0x100afa50 .. 0x100afbb0 (the commands, ShowWindow),
    0x100afe30 / 0x100afe80 (constructor, destructor) and OnCreate (0x100b05d0). KillFocus (0x100abeb0) is the
    NCMinimapWnd's body: it stays a stock stub here.
=============================================================================*/
#include "nwindow/NCMapCoverWnd.h"
#include "nwindow/NCButton.h"
#include "nwindow/NCComboBox.h"
#include "nwindow/NCFrameCtrl.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NCIcon.h"
#include "nwindow/NCMinimapWnd.h"
#include "nwindow/NMapBase24.h"
#include "nwindow/NConsoleWnd.h"

// NWM_COMMAND ids (names ours)
enum
{
    NID_MAPCOVER_FRAME = 0xB000F0,
    NID_MAPCOVER_MARK = 0xB000F5,
    NID_MAPCOVER_MYPOS = 0xB00F00,
    NID_MAPCOVER_PARTYLOC = 0xB00F01,    // new in C4
};

// sysstring lines of the buttons
enum
{
    STR_ShowMark = 886,
    STR_ShowMyPosition = 887,
    STR_PartyLocation = 1223,            // new in C4
};

// the player's world position as the stock reads it inline (the first viewport's controller's pawn; 0 without one)
static FORCEINLINE FVector MyLocation()
{
    FVector Result(0.0f, 0.0f, 0.0f);
    APlayerController* Controller = GConsole->Engine->Client->Viewports(0)->Actor;
    if (Controller && Controller->Pawn)
        Result = Controller->Pawn->Location;
    return Result;
}

// a child window goes (repeated inline in the stock OnDestroy)
template <class T> static FORCEINLINE void DestroyChild(NCWnd* Parent, T*& Child)
{
    if (Child)
    {
        Parent->RemoveChildWindow(Child->Entry);
        delete Child;
        Child = NULL;
    }
}

// 0x100bbd80 - NCWnd's constructor; the map's backdrop empty and the two party position fields at -1 (the stock stores them
// in this order: +0x100, +0xF0, the vtable, +0xEC)
NCMapCoverWnd::NCMapCoverWnd()
{
    PartyLocNext = -1;
    PartyLocShown = -1;
    BackTex = NULL;
}
NW_HOOK_CTOR(0x100bbd80, NCMapCoverWnd)

// 0x100bbde0 - nothing of its own to free (OnDestroy took the children)
NW_HOOK_DTOR(0x100bbde0, NCMapCoverWnd)

// 0x100b87e0 (vslot 25) - the three buttons' commands (C4: the third is the party position button, not the combo box)
INT NCMapCoverWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCWnd::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND)
    {
        if (wParam == NID_MAPCOVER_MARK)
            return OnShowMark() != 0;
        if (wParam == NID_MAPCOVER_MYPOS)
            return OnShowMyPosition() != 0;
        if (wParam == NID_MAPCOVER_PARTYLOC)
            return OnPartyLocBtnClick() != 0;
    }
    return 0;
}
NW_HOOK_METHOD(0x100b87e0, NCMapCoverWnd, WindowProc)

// 0x100b9010 (vslot 30) - the cursor lock is let go (NCObject::UnlockCursor), then NCWnd's; the body is hooked in NTownMapWnd.cpp
// (the hook build jumps to it, the standalone has its own copy)
#if NW_HOOK_MODE
NW_NAKED void NCMapCoverWnd::KillFocus() NW_STOCK(0x100b9010)
#else
void NCMapCoverWnd::KillFocus()
{
    UnlockCursor();
    NCWnd::KillFocus();
}
#endif

// 0x100b8880 (vslot 54) - minimizing: the window's own icon picture (the menu icon, text 0x1BF)
void NCMapCoverWnd::Minimize()
{
    if (Icon)
        Icon->SetIcon(TEXT("L2UI_CH3.MenuIcon.menuicon3"), 0x39D, 1);
    if (ScreenX < 0 && Icon)
        Icon->OffsetWindow(-ScreenX, 0);
    HideWindow();
    if (Icon)
    {
        Icon->ShowWindow();
        Icon->SetFocus();
    }
}
NW_HOOK_METHOD(0x100b8880, NCMapCoverWnd, Minimize)

// 0x100b9120 (vslot 62) - the background under the title bar (334 x 363)
INT NCMapCoverWnd::OnPaint(NCGDevice* Device)
{
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    Device->BeginPaint(ScreenX, ScreenY);
    if (BackTex)
        Device->DrawTexture(0, 20, 334, 363, 0, 0, 334, 363, BackTex, 0xFF, 1);
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
}
NW_HOOK_METHOD(0x100b9120, NCMapCoverWnd, OnPaint)

// 0x100b91c0 (vslot 38)
INT NCMapCoverWnd::OnDestroy()
{
    guard(NCMapCoverWnd::OnDestroy);
    DestroyChild(this, Map);
    DestroyChild(this, MarkButton);
    DestroyChild(this, MyPosButton);
    DestroyChild(this, PartyLocButton);
    NCWnd::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100b91c0, NCMapCoverWnd, OnDestroy)

// C2 CANDIDATE: the C2 function is not the same code as the C1 one; kept only while it matches the stock one (verify_data)
// 0x100bba20 (vslot 48) - shown where its icon was; the map goes to the player's place and forgets the continent's
// saved scroll
void NCMapCoverWnd::ShowWindow()
{
    NCWnd::ShowWindow();
    if (Icon)
    {
        SetPosition(Icon->ScreenX, Icon->ScreenY, (INT)Width, (INT)Height);
        Icon->SetPosition(0, 0, 0, 0);
        Icon->HideWindow();
    }
    if (Map)
        Map->CenterOnPlayer();
    if (Map)
    {
        Map->SavedScrollX = 0;
        Map->SavedScrollY = 0;
    }
}
NW_HOOK_METHOD(0x100bba20, NCMapCoverWnd, ShowWindow)

// 0x100bb9e0 (vslot 74, name ours) - the mark the console reported (the first one of MarkPos), while the map shows it
INT NCMapCoverWnd::OnShowMark()
{
    if (Map && Map->bShowMark && Map->MarkPos.Num() > 0)
        Map->ShowPosition(Map->MarkPos(0).X, Map->MarkPos(0).Y, 0, 1);
    return 1;
}
NW_HOOK_METHOD(0x100bb9e0, NCMapCoverWnd, OnShowMark)

// 0x100bb8f0 (vslot 75, name ours) - the map follows the player (the stock code does not test Map here)
INT NCMapCoverWnd::OnShowMyPosition()
{
    INT PX = (INT)MyLocation().X;
    INT PY = (INT)MyLocation().Y;
    INT PZ = (INT)MyLocation().Z;
    Map->ShowPosition(PX, PY, PZ, 1);
    return 1;
}
NW_HOOK_METHOD(0x100bb8f0, NCMapCoverWnd, OnShowMyPosition)

// 0x100b88e0 (slot 36, a stock placeholder in NNewVirtuals.gen.cpp, so hooked through a stand-in class) - the timer of the party
// position button runs out: it is killed and the shown position forgotten
struct NMapCoverFns : public NCMapCoverWnd
{
    __declspec(noinline) INT OnTimerBody(INT TimerID);
};
INT NMapCoverFns::OnTimerBody(INT TimerID)
{
    guard(NCMapCoverWnd::OnTimer);
    if (TimerID == 0xB01000 && PartyLocTimer != 0)
    {
        KillTimer(PartyLocTimer);
        PartyLocTimer = 0;
        PartyLocShown = -1;
    }
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100b88e0, NMapCoverFns, OnTimerBody)

// the party the server keeps (UL2NetHandler's slot 52 fills it: pairs of 0x18 bytes, a hash chain index, the key and the
// member's 16 bytes: id, x, y, z) as the stock holds it on the stack - the engine's TMap<INT, PartyInfo> (the same record as
// NCMinimapWnd::OnPaint's) - and the destructor the small path calls (name ours)
struct NMapCoverPartyMap
{
    BYTE* Pairs;
    INT Num;
    INT Max;
    INT* Hash;
    INT HashCount;
    __declspec(noinline) void Destruct();
};

// 0x100BB860 - the pairs array's destructor (NMapBase24.cpp hooks it): the hook build calls the stock function, the standalone
// the template's own
static FORCEINLINE void DestroyPartyPairs(void* Party)
{
#if NW_HOOK_MODE
    ::nw::StockCallAt<void>(0x100BB860, Party);
#else
    reinterpret_cast<TMapPair24Array*>(Party)->~TMapPair24Array();
#endif
}

// 0x100bbe30 (anonymous in the db) - the hash goes, the pairs are emptied (0x100BB860)
void NMapCoverPartyMap::Destruct()
{
    if (Hash)
        appFree(Hash);
    Hash = NULL;
    HashCount = 0;
    DestroyPartyPairs(this);
}
NW_HOOK_METHOD(0x100bbe30, NMapCoverPartyMap, Destruct)

// 0x100bcdb0 (vslot 76, name ours) - the next position of the party: the server's list (the engine's slot 52) is read, the
// index after the last one shown (wrapping) is marked as the one shown, the map goes to its place, and a timer of 2000 ms arms
// the reset of PartyLocShown (OnTimer). Nothing happens when nobody is in the party. The stock's iterator over the pairs
// walks the array in order; its tail (the hash rebuilt when something was removed) is never reached here
INT NCMapCoverWnd::OnPartyLocBtnClick()
{
    guard(NCMapCoverWnd::OnPartyLocBtnClick);
    NMapCoverPartyMap Party;
    Party.Pairs = NULL;
    Party.Num = 0;
    Party.Max = 0;
    Party.Hash = NULL;
    Party.HashCount = 8;
    reinterpret_cast<NMapBase24*>(&Party)->Rehash();   // 0x100BA050
    ::nw::VirtualCall<void>(::nw::Field<BYTE*>(GConsole->Engine, 0x64), 52, &Party);
    if (Party.Num == 0)
    {
        Party.Destruct();
        return 1;
    }
    INT Next = (PartyLocNext + 1) % Party.Num;
    PartyLocNext = Next;
    PartyLocShown = Next;
    INT Count = 0;
    for (INT k = 0; k < Party.Num; k++)
    {
        if (PartyLocNext == Count)
        {
            INT* Pair = (INT*)(Party.Pairs + k * 0x18 + 8);
            Map->ShowPosition(Pair[1], Pair[2], Pair[3], 1);
            break;
        }
        Count++;
    }
    if (PartyLocTimer != 0)
    {
        KillTimer(PartyLocTimer);
        PartyLocTimer = 0;
    }
    PartyLocTimer = SetTimer(0xB01000, 2000, 0);
    if (Party.Hash)
        appFree(Party.Hash);
    Party.Hash = NULL;
    Party.HashCount = 0;
    DestroyPartyPairs(&Party);
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100bcdb0, NCMapCoverWnd, OnPartyLocBtnClick)

// 0x100bd420 (vslot 37) - a title bar, the map under it and the three buttons along the bottom. C4: the third control is a
// button with the text 1223 at (89, 355) where C3 had the combo box of the continents; the texts are set by id
INT NCMapCoverWnd::OnCreate()
{
    NCWnd::OnCreate();
    BackTex = FindTexture(TEXT("L2UI_CH3.Minimap.MapBack"));

    FrameCtrl = new NCFrameCtrl(1, 0, 0);
    FrameCtrl->ID = NID_MAPCOVER_FRAME;
    FrameCtrl->SetPosition(0, 0, 334, 20);
    CreateChild(FrameCtrl, NWS_VISIBLE);

    Map = new NCMinimapWnd();
    Map->SetPosition(3, 21, 328, 328);
    CreateChild(Map, 0x22);

    MarkButton = new NCButton(TEXT("L2UI_CH3.Button.Btn1_Normal"), TEXT("L2UI_CH3.Button.Btn1_NormalOn"), 0);
    MarkButton->ID = NID_MAPCOVER_MARK;
    MarkButton->SetPosition(169, 355, 76, 23);
    CreateChild(MarkButton, NWS_VISIBLE);
    MarkButton->SetTextID(STR_ShowMark);

    MyPosButton = new NCButton(TEXT("L2UI_CH3.Button.Btn1_Normal"), TEXT("L2UI_CH3.Button.Btn1_NormalOn"), 0);
    MyPosButton->ID = NID_MAPCOVER_MYPOS;
    MyPosButton->SetPosition(249, 355, 76, 23);
    CreateChild(MyPosButton, NWS_VISIBLE);
    MyPosButton->SetTextID(STR_ShowMyPosition);

    PartyLocButton = new NCButton(TEXT("L2UI_CH3.Button.Btn1_Normal"), TEXT("L2UI_CH3.Button.Btn1_NormalOn"), 0);
    PartyLocButton->ID = NID_MAPCOVER_PARTYLOC;
    PartyLocButton->SetPosition(89, 355, 76, 23);
    CreateChild(PartyLocButton, NWS_VISIBLE);
    PartyLocButton->SetTextID(STR_PartyLocation);
    return 1;
}
NW_HOOK_METHOD(0x100bd420, NCMapCoverWnd, OnCreate)

NW_CHECK_VTABLE(NCMapCoverWnd, 0x101bda20, 77)
