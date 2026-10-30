/*=============================================================================
    NCBroadcastObserverPlayerWnd.cpp - the followed player of a team (C3 only; C3 addresses). Stock: RefreshBuffWnd
    0x101160A0, GetUser 0x10116120, OnDestroy 0x101161B0, SetUserIndex 0x10116230, OnPaint 0x101162C0, OnCreate
    0x101166E0, the constructor 0x10116920 and the destructor 0x101169F0.
=============================================================================*/
#include "nwindow/NCBroadcastObserverPlayerWnd.h"
#include "nwindow/NCBroadcastObserverBuffWnd.h"
#include "nwindow/NCBroadcastObserverWnd.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NConsoleWnd.h"

// 0x1012c690 (ret 8) - the textures and the effects window cleared, no player shown yet
NCBroadcastObserverPlayerWnd::NCBroadcastObserverPlayerWnd(NCBroadcastObserverWnd* InOwner, INT InTeam)
{
    for (INT i = 0; i < 3; i++)
    {
        BackTex[i] = NULL;
        DeadBackTex[i] = NULL;
    }
    HpBarTex = NULL;
    MpBarTex = NULL;
    CpBarTex = NULL;
    HpBackTex = NULL;
    MpBackTex = NULL;
    CpBackTex = NULL;
    BuffWnd = NULL;
    Team = InTeam;
    Owner = InOwner;
    UserSlot = -1;
}
NW_HOOK_CTOR_ARGS(0x1012c690, NCBroadcastObserverPlayerWnd, (NCBroadcastObserverWnd* InOwner, INT InTeam), (InOwner, InTeam))

// 0x1012c760
NW_HOOK_DTOR(0x1012c760, NCBroadcastObserverPlayerWnd)

// 0x10148f10 (vslot 25) - NControl::WindowProc's body, which the stock vtables of these windows share (the C3 address was
// 0x10119f60): NCWnd's, normalized to 0 or 1
INT NCBroadcastObserverPlayerWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return NCWnd::WindowProc(Msg, wParam, lParam) != 0;
}

// 0x1012be90 (guard: NCBroadcastObserverPlayerWnd::RefreshBuffWnd) - the effects window reads the player's lists again
void NCBroadcastObserverPlayerWnd::RefreshBuffWnd()
{
    guard(NCBroadcastObserverPlayerWnd::RefreshBuffWnd);
    if (BuffWnd)
        BuffWnd->Refresh();
    unguard;
}
NW_HOOK_METHOD(0x1012be90, NCBroadcastObserverPlayerWnd, RefreshBuffWnd)

// 0x1012bf10 (guard: NCBroadcastObserverPlayerWnd::GetUser) - the roster line of the shown player (NULL without an owner)
NBroadcastUser* NCBroadcastObserverPlayerWnd::GetUser()
{
    guard(NCBroadcastObserverPlayerWnd::GetUser);
    if (!Owner)
        return NULL;
    return Owner->GetUser(Team, UserSlot);
    unguard;
}
NW_HOOK_METHOD(0x1012bf10, NCBroadcastObserverPlayerWnd, GetUser)

// 0x1012bfa0 (guard: NCBroadcastObserverPlayerWnd::SetUserIndex; ret 4) - another player of the team (a slot out of 0..8
// is ignored), the effects window refreshed
void NCBroadcastObserverPlayerWnd::SetUserIndex(INT Slot)
{
    guard(NCBroadcastObserverPlayerWnd::SetUserIndex);
    if (Slot >= 0 && Slot < 9)
    {
        UserSlot = Slot;
        RefreshBuffWnd();
    }
    unguard;
}
NW_HOOK_METHOD(0x1012bfa0, NCBroadcastObserverPlayerWnd, SetUserIndex)

// 0x1012c430 (vslot 37; the guard string is NCBroadcastObserverWnd::OnCreate, copied from the parent window) - the effects
// window (400 x 80, anchored to the upper left corner for the first team and the upper right for the second), the
// banner pieces and the bar textures
INT NCBroadcastObserverPlayerWnd::OnCreate()
{
    guard(NCBroadcastObserverWnd::OnCreate);
    NCWnd::OnCreate();
    BuffWnd = new NCBroadcastObserverBuffWnd(this);
    CreateChild(BuffWnd, NWS_VISIBLE);
    if (Team == 0)
    {
        BuffWnd->SetPosition(0, 0, 0x190, 0x50);
        BuffWnd->SetAnchor(7, 10, 0);
    }
    else if (Team == 1)
    {
        BuffWnd->SetPosition(0, 0, 0x190, 0x50);
        BuffWnd->SetAnchor(9, -10, 0);
    }
    for (INT i = 0; i < 3; i++)
    {
        BackTex[i] = FindTexture(*FString::Printf(TEXT("L2UI_CH3.BroadcastObs.br_party1_back%d"), i + 1));
        DeadBackTex[i] = FindTexture(*FString::Printf(TEXT("L2UI_CH3.BroadcastObs.br_dead1_back%d"), i + 1));
    }
    HpBarTex = FindTexture(TEXT("L2UI_CH3.BroadcastObs.br_bar1_hp"));
    MpBarTex = FindTexture(TEXT("L2UI_CH3.BroadcastObs.br_bar1_mp"));
    CpBarTex = FindTexture(TEXT("L2UI_CH3.BroadcastObs.br_bar1_cp"));
    HpBackTex = FindTexture(TEXT("L2UI_CH3.BroadcastObs.br_bar1back_hp"));
    MpBackTex = FindTexture(TEXT("L2UI_CH3.BroadcastObs.br_bar1back_mp"));
    CpBackTex = FindTexture(TEXT("L2UI_CH3.BroadcastObs.br_bar1back_cp"));
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1012c430, NCBroadcastObserverPlayerWnd, OnCreate)

// 0x1012c030 (vslot 38; the guard string is NCBroadcastObserverWnd::OnDestroy) - the effects window goes, then NCWnd's
INT NCBroadcastObserverPlayerWnd::OnDestroy()
{
    guard(NCBroadcastObserverWnd::OnDestroy);
    if (BuffWnd)
    {
        RemoveChildWindow(BuffWnd->Entry);
        delete BuffWnd;
        BuffWnd = NULL;
    }
    NCWnd::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1012c030, NCBroadcastObserverPlayerWnd, OnDestroy)

// 0x1012c0e0 (vslot 62; the guard string is NCBroadcastObserverWnd::OnPaint) - the banner (the dead frame for an HP of 0),
// the "Lv<level> <class>" line and the name, both left aligned (C4: the first team's layout for both teams; the C3 right
// aligned second team branch is gone from the stock), the three bars (CP, HP, MP: 500 pixels full) over their backs
INT NCBroadcastObserverPlayerWnd::OnPaint(NCGDevice* Device)
{
    guard(NCBroadcastObserverWnd::OnPaint);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    Device->BeginPaint(ScreenX, ScreenY);
    NBroadcastUser* User = GetUser();
    UTexture** Back = BackTex;
    if (User && User->HP <= 0)
        Back = DeadBackTex;
    Device->DrawTexture(0, 0, 0x20, 0x100, 0, 0, 0x20, 0x100, Back[0], 0xFF, 1);
    Device->DrawTexture(0x20, 0, (INT)(Width - 64.0f), 0x100, 0, 0, 0x20, 0x100, Back[1], 0xFF, 1);
    Device->DrawTexture((INT)(Width - 32.0f), 0, 0x20, 0x100, 0, 0, 0x20, 0x100, Back[2], 0xFF, 1);
    if (User)
    {
        FString Label = FString::Printf(TEXT("Lv%d %s"), User->Level, GConsole->GetClassNameString(User->ClassID));
        Device->DrawText(0x14, 0xA, 0xFFDCDCDC, *Label, 0, 0, 0, (L2FontType)7, 0, 0, 0, 0, 0, 0, 0);
        Device->DrawText(0x14, 0x28, 0xFFFFD23C, *User->Name, (INT)Width, 0, 0, (L2FontType)4, 0, 0, 0, 0, 0, 0, 0);
    }
    Device->DrawTexture(0xA, 0x62, 0x1F4, 0xC, 0, 0, 8, 0x10, CpBackTex, 0xFF, 1);
    Device->DrawTexture(0xA, 0x70, 0x1F4, 0xC, 0, 0, 8, 0x10, HpBackTex, 0xFF, 1);
    Device->DrawTexture(0xA, 0x7E, 0x1F4, 0xC, 0, 0, 8, 0x10, MpBackTex, 0xFF, 1);
    if (User)
    {
        Device->DrawTexture(0xA, 0x62, User->CP * 0x1F4 / User->MaxCP, 0xC, 0, 0, 8, 0x10, CpBarTex, 0xFF, 1);
        Device->DrawTexture(0xA, 0x70, User->HP * 0x1F4 / User->MaxHP, 0xC, 0, 0, 8, 0x10, HpBarTex, 0xFF, 1);
        Device->DrawTexture(0xA, 0x7E, User->MP * 0x1F4 / User->MaxMP, 0xC, 0, 0, 8, 0x10, MpBarTex, 0xFF, 1);
    }
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1012c0e0, NCBroadcastObserverPlayerWnd, OnPaint)

NW_CHECK_VTABLE(NCBroadcastObserverPlayerWnd, 0x101d43d0, 74)
