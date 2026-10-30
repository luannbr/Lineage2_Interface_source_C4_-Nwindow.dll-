/*=============================================================================
    NCBroadcastObserverPartyWnd.cpp - one player's row of a party list (C3 only; C3 addresses). Stock: OnCreate
    0x10115360, OnDestroy 0x101155A0, the mouse handlers 0x10115620 .. 0x10115780, GetUser 0x10115820, IsSelected
    0x101158C0, OnPaint 0x10115960, the constructor 0x10115F20 and the destructor 0x10116020.
=============================================================================*/
#include "nwindow/NCBroadcastObserverPartyWnd.h"
#include "nwindow/NCBroadcastObserverPartyListWnd.h"
#include "nwindow/NCBroadcastObserverWnd.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NConsoleWnd.h"

// 0x1012bd20 (ret 8) - the textures cleared; the slot kept within 0..8, the list kept
NCBroadcastObserverPartyWnd::NCBroadcastObserverPartyWnd(NCBroadcastObserverPartyListWnd* InParent, INT InSlot)
{
    for (INT i = 0; i < 3; i++)
    {
        BackTex[i] = NULL;
        DeadBackTex[i] = NULL;
        SelectTex[i] = NULL;
    }
    HpBarTex = NULL;
    MpBarTex = NULL;
    CpBarTex = NULL;
    HpBackTex = NULL;
    MpBackTex = NULL;
    CpBackTex = NULL;
    TeamTex = NULL;
    DeadTeamTex = NULL;
    Slot = InSlot;
    if (InSlot > 8)
        Slot = 8;
    ParentList = InParent;
    if (InSlot < 0)
        Slot = 0;
}
NW_HOOK_CTOR_ARGS(0x1012bd20, NCBroadcastObserverPartyWnd, (NCBroadcastObserverPartyListWnd* InParent, INT InSlot), (InParent, InSlot))

// 0x1012be20
NW_HOOK_DTOR(0x1012be20, NCBroadcastObserverPartyWnd)

// 0x10148f10 (vslot 25) - NControl::WindowProc's body, which the stock vtables of these windows share (the C3 address was
// 0x10119f60): NCWnd's, normalized to 0 or 1
INT NCBroadcastObserverPartyWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return NCWnd::WindowProc(Msg, wParam, lParam) != 0;
}

// 0x1012b140 (vslot 37; the guard string is NCBroadcastObserverWnd::OnCreate, copied from the parent window) - the banner
// pieces (alive, dead, selected), the bar textures and the team's emblem for the slot
INT NCBroadcastObserverPartyWnd::OnCreate()
{
    guard(NCBroadcastObserverWnd::OnCreate);
    NCWnd::OnCreate();
    for (INT i = 0; i < 3; i++)
    {
        BackTex[i] = FindTexture(*FString::Printf(TEXT("L2UI_CH3.BroadcastObs.br_party2_back%d"), i + 1));
        DeadBackTex[i] = FindTexture(*FString::Printf(TEXT("L2UI_CH3.BroadcastObs.br_dead2_back%d"), i + 1));
        SelectTex[i] = FindTexture(*FString::Printf(TEXT("L2UI_CH3.BroadcastObs.br_partyon_back%d"), i + 1));
    }
    HpBarTex = FindTexture(TEXT("L2UI_CH3.BroadcastObs.br_bar2_hp"));
    MpBarTex = FindTexture(TEXT("L2UI_CH3.BroadcastObs.br_bar2_mp"));
    CpBarTex = FindTexture(TEXT("L2UI_CH3.BroadcastObs.br_bar2_cp"));
    HpBackTex = FindTexture(TEXT("L2UI_CH3.BroadcastObs.br_bar2back_hp"));
    MpBackTex = FindTexture(TEXT("L2UI_CH3.BroadcastObs.br_bar2back_mp"));
    CpBackTex = FindTexture(TEXT("L2UI_CH3.BroadcastObs.br_bar2back_cp"));
    switch (ParentList->GetTeam())
    {
    case 0:
        TeamTex = FindTexture(*FString::Printf(TEXT("L2UI_CH3.BroadcastObs.br_party1_%d"), Slot + 1));
        break;
    case 1:
        TeamTex = FindTexture(*FString::Printf(TEXT("L2UI_CH3.BroadcastObs.br_party2_%d"), Slot + 1));
        break;
    }
    DeadTeamTex = FindTexture(TEXT("L2UI_CH3.BroadcastObs.br_party_X"));
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1012b140, NCBroadcastObserverPartyWnd, OnCreate)

// 0x1012b3a0 (vslot 38; the guard string is NCBroadcastObserverWnd::OnDestroy) - nothing of its own to free
INT NCBroadcastObserverPartyWnd::OnDestroy()
{
    guard(NCBroadcastObserverWnd::OnDestroy);
    NCWnd::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1012b3a0, NCBroadcastObserverPartyWnd, OnDestroy)

// 0x1012b420 (vslot 62, guard: NCBroadcastObserverPartyWnd::OnLButtonDown; ret 8) - the press goes to the parent window
// (the list) too, and the owner follows this row's player
INT NCBroadcastObserverPartyWnd::OnLButtonDown(WPARAM wParam, LPARAM lParam)
{
    guard(NCBroadcastObserverPartyWnd::OnLButtonDown);
    if (Entry && Entry->Parent && Entry->Parent->Wnd)
        Entry->Parent->Wnd->WindowProc(0x201, wParam, lParam);
    if (ParentList)
        ParentList->SetSelectedUser(Slot);
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1012b420, NCBroadcastObserverPartyWnd, OnLButtonDown)

// 0x1012b4e0 (vslot 63, guard: NCBroadcastObserverPartyWnd::OnLButtonUp; ret 8) - the release goes to the parent window
INT NCBroadcastObserverPartyWnd::OnLButtonUp(WPARAM wParam, LPARAM lParam)
{
    guard(NCBroadcastObserverPartyWnd::OnLButtonUp);
    if (Entry && Entry->Parent && Entry->Parent->Wnd)
        Entry->Parent->Wnd->WindowProc(0x202, wParam, lParam);
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1012b4e0, NCBroadcastObserverPartyWnd, OnLButtonUp)

// 0x1012b580 (vslot 61, guard: NCBroadcastObserverPartyWnd::OnMouseMove; ret 8) - the move goes to the parent window
INT NCBroadcastObserverPartyWnd::OnMouseMove(WPARAM wParam, LPARAM lParam)
{
    guard(NCBroadcastObserverPartyWnd::OnMouseMove);
    if (Entry && Entry->Parent && Entry->Parent->Wnd)
        Entry->Parent->Wnd->WindowProc(0x200, wParam, lParam);
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1012b580, NCBroadcastObserverPartyWnd, OnMouseMove)

// 0x1012b620 (guard: NCBroadcastObserverPartyWnd::GetUser) - the roster line of the row, through the list
NBroadcastUser* NCBroadcastObserverPartyWnd::GetUser()
{
    guard(NCBroadcastObserverPartyWnd::GetUser);
    if (!ParentList)
        return NULL;
    return ParentList->GetUser(Slot);
    unguard;
}
NW_HOOK_METHOD(0x1012b620, NCBroadcastObserverPartyWnd, GetUser)

// 0x1012b6c0 (guard: NCBroadcastObserverPartyWnd::IsSelected)
INT NCBroadcastObserverPartyWnd::IsSelected()
{
    guard(NCBroadcastObserverPartyWnd::IsSelected);
    if (!ParentList)
        return 0;
    return ParentList->IsSelected(Slot);
    unguard;
}
NW_HOOK_METHOD(0x1012b6c0, NCBroadcastObserverPartyWnd, IsSelected)

// 0x1012b760 (vslot 62; the guard string is NCBroadcastObserverWnd::OnPaint) - the banner (alpha 0x80, 0xFF when the row
// is selected; the dead frame for an HP of 0), the team's emblem (the dead one when he is), the class or name of the
// player (cut to fit) and the three bars (CP, HP, MP: 220 pixels full), the first team's rows laid out from the left,
// the second team's mirrored; the selected row gets its overlay
INT NCBroadcastObserverPartyWnd::OnPaint(NCGDevice* Device)
{
    guard(NCBroadcastObserverWnd::OnPaint);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    Device->BeginPaint(ScreenX, ScreenY);
    INT Alpha = 0x80;
    if (IsSelected())
        Alpha = 0xFF;
    NBroadcastUser* User = GetUser();
    UTexture** Back = BackTex;
    if (User && User->HP <= 0)
        Back = DeadBackTex;
    Device->DrawTexture(0, 0, 0x20, 0x80, 0, 0, 0x20, 0x80, Back[0], Alpha, 1);
    Device->DrawTexture(0x20, 0, (INT)(Width - 64.0f), 0x80, 0, 0, 0x20, 0x80, Back[1], Alpha, 1);
    Device->DrawTexture((INT)(Width - 32.0f), 0, 0x20, 0x80, 0, 0, 0x20, 0x80, Back[2], Alpha, 1);

    UTexture* Emblem = TeamTex;
    if (User && User->HP <= 0)
        Emblem = DeadTeamTex;
    INT Team = ParentList->GetTeam();
    INT X;
    if (Team == 0)
        X = 7;
    else if (Team == 1)
        X = 0xE7;
    else
        X = Alpha;
    Device->DrawTexture(X, 9, 0x40, 0x40, 0, 0, 0x40, 0x40, Emblem, Alpha, 1);
    if (Team == 0)
        X = 0x38;
    else if (Team == 1)
        X = 5;

    if (User)
    {
        DWORD Color = 0xFF000000;
        if (User->HP > 0)
            Color = 0xFFDCDCDC;
        FString Label;
        if (ParentList->IsShowClassOrName())
            Label = GConsole->GetClassNameString(User->ClassID);
        else
            Label = User->Name;
        SIZE Size;
        for (;;)
        {
            NCGDevice::GetTextExtent(&Size, *Label, 0, 0, 0, 0, 0, (L2FontType)7);
            if (!(Width - 61.0f < (FLOAT)Size.cx))
                break;
            Label = Label.Left(Label.Len() - 1);
        }
        if (Team == 0)
        {
            Device->DrawText(X, 5, Color, *Label, 0, 0, 0, (L2FontType)7, 0, 0, 0, 0, 0, 0, 0);
        }
        else if (Team == 1)
        {
            NCGDevice::GetTextExtent(&Size, *Label, 0, 0, 0, 0, 0, (L2FontType)7);
            Device->DrawText(X - Size.cx + 0xDC, 5, Color, *Label, 0, 0, 0, (L2FontType)7, 0, 0, 0, 0, 0, 0, 0);
        }
    }

    Device->DrawTexture(X, 0x26, 0xDC, 6, 0, 0, 8, 8, CpBackTex, Alpha, 1);
    Device->DrawTexture(X, 0x2E, 0xDC, 8, 0, 0, 8, 8, HpBackTex, Alpha, 1);
    Device->DrawTexture(X, 0x38, 0xDC, 8, 0, 0, 8, 8, MpBackTex, Alpha, 1);
    if (User)
    {
        INT CpWidth = User->CP * 0xDC / User->MaxCP;
        INT HpWidth = User->HP * 0xDC / User->MaxHP;
        INT MpWidth = User->MP * 0xDC / User->MaxMP;
        if (Team == 0)
        {
            Device->DrawTexture(X, 0x26, CpWidth, 6, 0, 0, 8, 8, CpBarTex, Alpha, 1);
            Device->DrawTexture(X, 0x2E, HpWidth, 8, 0, 0, 8, 8, HpBarTex, Alpha, 1);
            Device->DrawTexture(X, 0x38, MpWidth, 8, 0, 0, 8, 8, MpBarTex, Alpha, 1);
        }
        else if (Team == 1)
        {
            Device->DrawTexture(X - CpWidth + 0xDC, 0x26, CpWidth, 6, 0, 0, 8, 8, CpBarTex, Alpha, 1);
            Device->DrawTexture(X - HpWidth + 0xDC, 0x2E, HpWidth, 8, 0, 0, 8, 8, HpBarTex, Alpha, 1);
            Device->DrawTexture(X - MpWidth + 0xDC, 0x38, MpWidth, 8, 0, 0, 8, 8, MpBarTex, Alpha, 1);
        }
    }

    if (IsSelected())
    {
        Device->DrawTexture(0, 0, 0x20, 0x80, 0, 0, 0x20, 0x80, SelectTex[0], 0x80, 1);
        Device->DrawTexture(0x20, 0, (INT)(Width - 64.0f), 0x80, 0, 0, 0x20, 0x80, SelectTex[1], 0x80, 1);
        Device->DrawTexture((INT)(Width - 32.0f), 0, 0x20, 0x80, 0, 0, 0x20, 0x80, SelectTex[2], 0x80, 1);
    }
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1012b760, NCBroadcastObserverPartyWnd, OnPaint)

NW_CHECK_VTABLE(NCBroadcastObserverPartyWnd, 0x101d3e20, 74)
