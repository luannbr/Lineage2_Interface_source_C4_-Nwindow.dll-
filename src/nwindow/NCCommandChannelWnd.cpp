/*=============================================================================
    NCCommandChannelWnd.cpp - the command channel window (C4 only). Stock: 0x10052390 (constructor), 0x100523e0 (destructor),
    0x10051370 (WindowProc), 0x10051440 (the tab), 0x100514c0 (ShowWindow), 0x10051940 (OnDestroy), 0x10051a90 (OnPaint), 0x10051b70
    (the chat text), 0x100513f0 and 0x10051c20 (AddChatLine, Reset; anonymous in the db), 0x10052430 (OnCreate).
=============================================================================*/
#include "nwindow/NCCommandChannelWnd.h"
#include "nwindow/NCChatCommandWnd.h"
#include "nwindow/NCCheckBox.h"
#include "nwindow/NCCommandInfoWnd.h"
#include "nwindow/NCEditBox.h"
#include "nwindow/NCFrameCtrl.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NCTabControl.h"
#include "nwindow/NConsoleWnd.h"
#include "nwindow/MsnUiStock.h"   // NCCheckBoxTexturesView

// 0x10005290 (NCPetitionFeedBackWnd.cpp; a method of NCCheckBox, ret 0x10) - the four textures by name (checked, unchecked, unchecked
// unable, checked unable); a name left NULL leaves its texture as it is. NCCheckBoxTexturesView is declared once, in MsnUiStock.h; the
// function is defined and hooked in NCPetitionFeedBackWnd.cpp (stage 5, sa-verify: this file used to define a copy of its own)

// NWM_COMMAND ids (names ours)
enum
{
    NID_CMDCHANNEL_TABS = 0xF0A0C0,
    NID_CMDCHANNEL_EDIT = 0xF0A0C2,
    NID_CMDCHANNEL_CHECK = 0xF0A0C3,
    NID_CMDCHANNEL_FRAME = 0xF0A0C5,
};
// the chat kinds SendChatMsgToServer takes (names ours: the check box picks the command channel's or the party's)
enum
{
    CHAT_CommandChecked = 15,
    CHAT_CommandUnchecked = 16,
};
enum
{
    SYSMSG_ChatTooLong = 971,      // SystemMsg[971]
    SYSSTR_CmdChatTypeOn = 1227,   // the check box's tool tip, checked
    SYSSTR_CmdChatTypeOff = 1228,  // unchecked
    TOOLTIP_STYLE_CmdCheck = 0x400,
};

// 0x10052390 - NCWnd's constructor
NCCommandChannelWnd::NCCommandChannelWnd()
{
}
NW_HOOK_CTOR(0x10052390, NCCommandChannelWnd)

// 0x100523e0
NW_HOOK_DTOR(0x100523e0, NCCommandChannelWnd)

// 0x10052430 (vslot 37) - the back picture, the frame bar (12 px wide, as high as the window, without a close button; its id is not
// the window's FrameID), the text box and the check box under the pages, the chat and the information pages (as wide as the window
// less the bar, 140 high) and the tab control under them with its two tabs (SysString 444 and 491)
INT NCCommandChannelWnd::OnCreate()
{
    NCWnd::OnCreate();
    BackTex = FindTexture(TEXT("L2UI_ch3.PartyCommandWnd.Herochat_back"));
    FrameCtrl = new NCFrameCtrl(1, 1, 1);
    FrameCtrl->ID = NID_CMDCHANNEL_FRAME;
    FrameCtrl->SetPosition(0, 0, 12, (INT)Height);
    CreateChild(FrameCtrl, NWS_VISIBLE | NWS_NOCLOSE);

    ChatEdit = new NCEditBox(1, NULL, 1, 0xA28, 0);
    ChatEdit->ID = NID_CMDCHANNEL_EDIT;
    ChatEdit->SetPosition(35, 165, 308, 17);
    CreateChild(ChatEdit, NWS_VISIBLE);

    ChatTypeCheck = new NCCheckBox();
    ChatTypeCheck->ID = NID_CMDCHANNEL_CHECK;
    ChatTypeCheck->SetPosition(18, 166, 15, 15);
    CreateChild(ChatTypeCheck, NWS_VISIBLE | NWS_UNKNOWN20);
    ((NCCheckBoxTexturesView*)ChatTypeCheck)->SetTextures(TEXT("L2UI_ch3.PartyCommandWnd.Herochat_btn1"),
                                                          TEXT("L2UI_ch3.PartyCommandWnd.Herochat_btn2"), NULL, NULL);
    ChatTypeCheck->BoxW = 15;
    ChatTypeCheck->BoxH = 15;

    ChatWnd = new NCChatCommandWnd();
    ChatWnd->SetPosition(12, 0, (INT)(Width - 12.0f), 140);
    CreateChild(ChatWnd, NWS_VISIBLE);

    InfoWnd = new NCCommandInfoWnd();
    InfoWnd->SetPosition(12, 0, (INT)(Width - 12.0f), 140);
    CreateChild(InfoWnd, NWS_VISIBLE);

    TabCtrl = new NCTabControl();
    TabCtrl->ID = NID_CMDCHANNEL_TABS;
    TabCtrl->SetPosition(24, (INT)(Height - 46.0f), 128, 23);
    CreateChild(TabCtrl, NWS_VISIBLE);
    TabCtrl->AddTab(444, ChatWnd, 64, 23, TEXT("L2UI_CH3.PartyCommandWnd.herochat_tab2"),
                    TEXT("L2UI_CH3.PartyCommandWnd.herochat_tab1"), 0, -1, 0);
    TabCtrl->AddTab(491, InfoWnd, 64, 23, TEXT("L2UI_CH3.PartyCommandWnd.herochat_tab2"),
                    TEXT("L2UI_CH3.PartyCommandWnd.herochat_tab1"), 0, -1, 0);
    Unknown100 = 0;
    return 1;
}
NW_HOOK_METHOD(0x10052430, NCCommandChannelWnd, OnCreate)

// 0x10051370 (vslot 25)
INT NCCommandChannelWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCWnd::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND)
    {
        if (wParam == NID_CMDCHANNEL_EDIT)
            return OnChatEnter((INT)lParam) != 0;
        if (wParam == NID_CMDCHANNEL_TABS)
            return OnTab((INT)lParam) != 0;
    }
    return 0;
}
NW_HOOK_METHOD(0x10051370, NCCommandChannelWnd, WindowProc)

// 0x10051440 (vslot 75, ret 4) - the tab picked: tab 1 asks for the channel's info (RequestInfo, which does it only while the
// button works); the information page shows and the chat hides, or the other way round; the info is asked again for the tab 1
INT NCCommandChannelWnd::OnTab(INT Tab)
{
    if (Tab == 1)
        InfoWnd->RequestInfo();
    switch (Tab)
    {
    case 0:
        InfoWnd->HideWindow();
        ChatWnd->ShowWindow();
        break;
    case 1:
        InfoWnd->ShowWindow();
        ChatWnd->HideWindow();
        InfoWnd->RequestInfo();
        break;
    }
    return 1;
}
NW_HOOK_METHOD(0x10051440, NCCommandChannelWnd, OnTab)

// 0x100514c0 (vslot 48) - the window shows and so does the page of the tab picked
void NCCommandChannelWnd::ShowWindow()
{
    NCWnd::ShowWindow();
    TabCtrl->ShowCurrentPage();
}
NW_HOOK_METHOD(0x100514c0, NCCommandChannelWnd, ShowWindow)

// 0x100513f0 (ret 0x10; anonymous in the db) - the line goes on the chat tab
void NCCommandChannelWnd::AddChatLine(const TCHAR* Text, DWORD Color, INT Unused1, INT Unused2)
{
    if (ChatWnd)
        ChatWnd->AddLine(Text, Color, 0, 0);
}
NW_HOOK_METHOD(0x100513f0, NCCommandChannelWnd, AddChatLine)

// 0x10051c20 (anonymous in the db) - the text box and the chat's lines are cleared, Unknown100 too
void NCCommandChannelWnd::Reset()
{
    ChatEdit->ClearText();
    ChatWnd->ClearList();
    Unknown100 = 0;
}
NW_HOOK_METHOD(0x10051c20, NCCommandChannelWnd, Reset)

// 0x10051940 (vslot 38, guard) - the chat, the text box, the check box, the information page and the tabs go (each: out of the
// children, deleted), then NCWnd's OnDestroy
INT NCCommandChannelWnd::OnDestroy()
{
    guard(NCCommandChannelWnd::OnDestroy);
    if (ChatWnd)
    {
        RemoveChildWindow(ChatWnd->Entry);
        delete ChatWnd;
        ChatWnd = NULL;
    }
    if (ChatEdit)
    {
        RemoveChildWindow(ChatEdit->Entry);
        delete ChatEdit;
        ChatEdit = NULL;
    }
    if (ChatTypeCheck)
    {
        RemoveChildWindow(ChatTypeCheck->Entry);
        delete ChatTypeCheck;
        ChatTypeCheck = NULL;
    }
    if (InfoWnd)
    {
        RemoveChildWindow(InfoWnd->Entry);
        delete InfoWnd;
        InfoWnd = NULL;
    }
    if (TabCtrl)
    {
        RemoveChildWindow(TabCtrl->Entry);
        delete TabCtrl;
        TabCtrl = NULL;
    }
    NCWnd::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10051940, NCCommandChannelWnd, OnDestroy)

// 0x10051a90 (vslot 62, ret 4) - the back picture from x = 12, as high as the window and 12 less wide; the check box's tool tip says
// which chat the text goes to
INT NCCommandChannelWnd::OnPaint(NCGDevice* Device)
{
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    Device->DrawTexture(0xC, 0, (INT)(Width - 12.0f), (INT)Height, 0, 0, (INT)(Width - 12.0f), (INT)Height, BackTex, 0xFF, 1);
    ChatTypeCheck->ToolTip.SetText(   // 0x10032460
        ChatTypeCheck->bChecked ? *GL2GameData.SysString[SYSSTR_CmdChatTypeOn] : *GL2GameData.SysString[SYSSTR_CmdChatTypeOff],
        TOOLTIP_STYLE_CmdCheck);
    Device->ReleaseClip();
    Device->EndPaint();
    return 1;
}
NW_HOOK_METHOD(0x10051a90, NCCommandChannelWnd, OnPaint)

// 0x10051b70 (vslot 74, ret 4) - Key 1 (Enter): the text typed goes to the server, as the command channel's chat or the party's by
// the check box; 255 characters or more are not sent, a message says so. The box is emptied in every case
INT NCCommandChannelWnd::OnChatEnter(INT Key)
{
    if (Key == 1)
    {
        TCHAR* Text = (TCHAR*)ChatEdit->CommitText();
        if (Text)
        {
            if (appStrlen(Text) >= 255)
                AddChatLine(*GL2GameData.SystemMsg[SYSMSG_ChatTooLong].Message, 0xFFDCDCDC, 0, 0);
            else
                GConsole->SendChatMsgToServer(ChatTypeCheck->bChecked ? CHAT_CommandChecked : CHAT_CommandUnchecked, Text, NULL);
        }
        ChatEdit->ClearText();
    }
    return 1;
}
NW_HOOK_METHOD(0x10051b70, NCCommandChannelWnd, OnChatEnter)

NW_CHECK_VTABLE(NCCommandChannelWnd, 0x101abae8, 76)
