/*=============================================================================
    NCCommandInfoWnd.cpp - the command channel's information page (C4 only). Stock: 0x10052200 (constructor), 0x10052280
    (destructor), 0x10051020 (WindowProc), 0x10051270 (OnTimer), 0x100514e0 (OnDestroy), 0x10051610 (OnPaint), 0x100510c0 /
    0x10051170 / 0x100517e0 (the button handlers), 0x10051220 / 0x100512b0 (ClearInfo, RequestInfo; anonymous in the db),
    0x10051c50 (OnCreate).
=============================================================================*/
#include "nwindow/NCCommandInfoWnd.h"
#include "nwindow/NCButton.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NCListCtrl.h"
#include "nwindow/NColumnInfoC4.h"
#include "nwindow/NConsoleWnd.h"

// NWM_COMMAND ids of the three buttons and the timer that lets the refresh button work again (names ours)
enum
{
    NID_CMDINFO_REFRESH = 0xF0A0C7,
    NID_CMDINFO_OUST = 0xF0A0C8,
    NID_CMDINFO_LEAVE = 0xF0A0C9,
    TIMER_CMDINFO_REFRESH = 0x1F9,
};
// the user commands the server is sent through BypassUserCmd (names ours)
enum
{
    USERCMD_LeaveChannel = 0x60,
    USERCMD_ChannelInfo = 0x61,
};
// SysStrings of the information texts (OnPaint)
enum
{
    SYSSTR_CmdMaster = 1265,
    SYSSTR_CmdParties = 1266,
    SYSSTR_CmdMembers = 1267,
};

// 0x10052200 - NCWnd's constructor, the name empty
NCCommandInfoWnd::NCCommandInfoWnd()
{
}
NW_HOOK_CTOR(0x10052200, NCCommandInfoWnd)

// 0x10052280
NW_HOOK_DTOR(0x10052280, NCCommandInfoWnd)

// a column of the party list (the stock spells out each with its own record, 0x4018 bytes: two make the 0x8030 bytes of its frame)
static FORCEINLINE void AddPartyColumn(NCListCtrl* List, NColumnInfoC4& Info, INT Index, INT Width, INT TitleID)
{
    Info.Width = Width;
    Info.Height = 19;
    Info.SetTitleID(TitleID);
    Info.SetNormalTex(TEXT("L2UI_CH3.ListCtrl.tab1"), 0);
    Info.SetTabDownTex(TEXT("L2UI_CH3.ListCtrl.tab1_down"), 0);
    Info.SetNormalTex(TEXT("L2UI_CH3.ListCtrl.tab2"), 1);
    Info.SetTabDownTex(TEXT("L2UI_CH3.ListCtrl.tab2_down"), 1);
    Info.SetNormalTex(TEXT("L2UI_CH3.ListCtrl.tab3"), 2);
    Info.SetTabDownTex(TEXT("L2UI_CH3.ListCtrl.tab3_down"), 2);
    List->AddColumn(Index, (NListColumnInfo*)&Info);
}

// one of the three buttons (the small 66 x 21 kind), 65 x 20 at the right of the page
static FORCEINLINE NCButton* MakeInfoButton(NCCommandInfoWnd* Wnd, INT ID, INT Y, INT TextID)
{
    NCButton* Button = new NCButton(1, 0);
    Button->ID = ID;
    Button->SetPosition(244, Y, 65, 20);
    Wnd->CreateChild(Button, NWS_VISIBLE);
    Button->SetTextID(TextID);
    return Button;
}

// 0x10051c50 (vslot 37) - the list of the parties' leaders (1000 rows, 5 shown, two columns: the name, the number of members) and
// the three buttons (SysString 938, 681 and 1268) with their timer; nobody in the page yet
INT NCCommandInfoWnd::OnCreate()
{
    NColumnInfoC4 NameColumn, CountColumn;
    NCWnd::OnCreate();
    PartyList = new NCListCtrl(1000, 5, 2, 12, 0, 17);
    PartyList->SetPosition(24, 25, 207, 101);
    CreateChild(PartyList, NWS_VISIBLE);
    PartyList->SetTextOffset(0, 3);
    AddPartyColumn(PartyList, NameColumn, 0, 0xA0, 408);
    AddPartyColumn(PartyList, CountColumn, 1, 0x2F, 1036);
    RefreshButton = MakeInfoButton(this, NID_CMDINFO_REFRESH, 62, 938);
    OustButton = MakeInfoButton(this, NID_CMDINFO_OUST, 86, 681);
    LeaveButton = MakeInfoButton(this, NID_CMDINFO_LEAVE, 110, 1268);
    ClearInfo();
    RefreshTimer = SetTimer(TIMER_CMDINFO_REFRESH, 3000, NULL);
    return 1;
}
NW_HOOK_METHOD(0x10051c50, NCCommandInfoWnd, OnCreate)

// 0x10051020 (vslot 25)
INT NCCommandInfoWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCWnd::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND)
    {
        if (wParam == NID_CMDINFO_REFRESH)
            return OnRefreshBtnClick() != 0;
        if (wParam == NID_CMDINFO_OUST)
            return OnOustBtnClick() != 0;
        if (wParam == NID_CMDINFO_LEAVE)
            return OnLeaveBtnClick() != 0;
    }
    return 0;
}
NW_HOOK_METHOD(0x10051020, NCCommandInfoWnd, WindowProc)

// 0x10051220 (anonymous in the db) - nobody and nothing in the list, the refresh button on
void NCCommandInfoWnd::ClearInfo()
{
    MasterName.Empty();
    PartyCount = 0;
    MemberCount = 0;
    PartyList->ClearRows();
    bCanRefresh = 1;
    RefreshButton->EnableWindow(1);
}
NW_HOOK_METHOD(0x10051220, NCCommandInfoWnd, ClearInfo)

// 0x100512b0 (anonymous in the db) - the server is asked for the info of the channel (user command 0x61) while the refresh button
// works; the button is off until the timer
void NCCommandInfoWnd::RequestInfo()
{
    if (bCanRefresh)
    {
        L2ParamStack Params(10);
        Params.PushBack((void*)USERCMD_ChannelInfo);
        GConsole->BypassUserCmd(Params);
        bCanRefresh = 0;
        RefreshButton->EnableWindow(0);
    }
}
NW_HOOK_METHOD(0x100512b0, NCCommandInfoWnd, RequestInfo)

// 0x10051270 (vslot 36, ret 4) - three seconds after the start of the page the refresh button works again
INT NCCommandInfoWnd::OnTimer(INT TimerID)
{
    if (TimerID == TIMER_CMDINFO_REFRESH && !bCanRefresh)
    {
        bCanRefresh = 1;
        RefreshButton->EnableWindow(1);
    }
    return 1;
}
NW_HOOK_METHOD(0x10051270, NCCommandInfoWnd, OnTimer)

// 0x100514e0 (vslot 38, guard) - the three buttons and the list go (each: out of the children, deleted), then NCWnd's OnDestroy
INT NCCommandInfoWnd::OnDestroy()
{
    guard(NCCommandInfoWnd::OnDestroy);
    if (RefreshButton)
    {
        RemoveChildWindow(RefreshButton->Entry);
        delete RefreshButton;
        RefreshButton = NULL;
    }
    if (OustButton)
    {
        RemoveChildWindow(OustButton->Entry);
        delete OustButton;
        OustButton = NULL;
    }
    if (LeaveButton)
    {
        RemoveChildWindow(LeaveButton->Entry);
        delete LeaveButton;
        LeaveButton = NULL;
    }
    if (PartyList)
    {
        RemoveChildWindow(PartyList->Entry);
        delete PartyList;
        PartyList = NULL;
    }
    NCWnd::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100514e0, NCCommandInfoWnd, OnDestroy)

// 0x10051610 (vslot 62) - "master:" and the name, then "parties:" and "members:" with their numbers under it; each value starts
// where its label ends
INT NCCommandInfoWnd::OnPaint(NCGDevice* Device)
{
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    DWORD W = Device->DrawText(0x16, 9, 0xFFDCDCDC, *GL2GameData.SysString[SYSSTR_CmdMaster], 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
    Device->DrawText(W + 0x1B, 9, 0xFFB09B79, *MasterName, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
    W = Device->DrawText(0xF3, 0x1D, 0xFFDCDCDC, *GL2GameData.SysString[SYSSTR_CmdParties], 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
    Device->DrawText(W + 0xF8, 0x1D, 0xFFB09B79, appItoa(PartyCount), 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
    W = Device->DrawText(0xF3, 0x2C, 0xFFDCDCDC, *GL2GameData.SysString[SYSSTR_CmdMembers], 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
    Device->DrawText(W + 0xF8, 0x2C, 0xFFB09B79, appItoa(MemberCount), 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
    Device->ReleaseClip();
    Device->EndPaint();
    return 1;
}
NW_HOOK_METHOD(0x10051610, NCCommandInfoWnd, OnPaint)

// 0x100517e0 (vslot 74) - the refresh button: the request
INT NCCommandInfoWnd::OnRefreshBtnClick()
{
    RequestInfo();
    return 1;
}
NW_HOOK_METHOD(0x100517e0, NCCommandInfoWnd, OnRefreshBtnClick)

// 0x100510c0 (vslot 75) - the oust button: the leader selected in the list (the text of the row's first cell) is thrown out of the
// channel
INT NCCommandInfoWnd::OnOustBtnClick()
{
    NListRow* Row = PartyList->GetSelectedRow();
    if (Row)
    {
        FString Name(Row->Cells[0].Text);
        L2ParamStack Params(10);
        Params.PushBack((void*)*Name);
        GConsole->RequestExOustFromMPCC(Params);
    }
    return 1;
}
NW_HOOK_METHOD(0x100510c0, NCCommandInfoWnd, OnOustBtnClick)

// 0x10051170 (vslot 76) - the leave button: the player's party leaves the channel (user command 0x60)
INT NCCommandInfoWnd::OnLeaveBtnClick()
{
    L2ParamStack Params(10);
    Params.PushBack((void*)USERCMD_LeaveChannel);
    GConsole->BypassUserCmd(Params);
    return 1;
}
NW_HOOK_METHOD(0x10051170, NCCommandInfoWnd, OnLeaveBtnClick)

NW_CHECK_VTABLE(NCCommandInfoWnd, 0x101ab888, 77)
