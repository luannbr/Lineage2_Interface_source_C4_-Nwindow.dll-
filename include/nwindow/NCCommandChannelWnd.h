#pragma once
// NCCommandChannelWnd - the command channel window (UClass "CCommandChannelWnd", 0x104; C4 only; a command channel, MPCC, is
// several parties under one leader): a back picture, two tabs (the channel's chat and its information) and, under the chat, the
// text box where the player types and a check box that picks the kind of chat. The console makes it (NCConsole +0x264), says the
// channel's chat through AddChatLine and resets it when the channel closes.
//
// Layout and virtuals from the stock constructor 0x10052390, OnCreate 0x10052430, OnDestroy 0x10051940, OnPaint
// 0x10051A90, WindowProc 0x10051370, ShowWindow 0x100514C0 and the two handlers (new virtuals 74 and 75).

#include "nwindow/NCWnd.h"
#pragma pack(push, 4)

class NCChatCommandWnd;
class NCCheckBox;
class NCCommandInfoWnd;
class NCEditBox;
class NCTabControl;

class NCCommandChannelWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCCommandChannelWnd, NCWnd, 0, 0x102CF810)

    UTexture* BackTex;               // +0xE8 "L2UI_ch3.PartyCommandWnd.Herochat_back"
    NCChatCommandWnd* ChatWnd;       // +0xEC the channel's chat (tab 0) (names ours)
    NCCommandInfoWnd* InfoWnd;       // +0xF0 the channel's information (tab 1)
    NCTabControl* TabCtrl;           // +0xF4 id 0xF0A0C0 (NWM_COMMAND = the tab), two tabs "herochat_tab1" / "2"
    NCEditBox* ChatEdit;             // +0xF8 id 0xF0A0C2 (Enter = the text is sent)
    NCCheckBox* ChatTypeCheck;       // +0xFC the kind of chat: checked 15, else 16 (the tool tip tells which)
    INT Unknown100;                  // +0x100 (OnCreate and the reset clear it; nothing reads it)

    NCCommandChannelWnd();                                           // 0x10052390
    // ~NCCommandChannelWnd                                          // 0x100523E0

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10051370
    virtual INT OnCreate();                                          // 37 0x10052430
    virtual INT OnDestroy();                                         // 38 0x10051940
    virtual void ShowWindow();                                       // 48 0x100514C0
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x10051A90
    // names ours
    virtual INT OnChatEnter(INT Key);                                // 74 0x10051B70 Key 1: the text typed goes to the server (or, too long, a message)
    virtual INT OnTab(INT Tab);                                      // 75 0x10051440 the tab picked: 0 the chat, 1 the information (and its request)

    // 0x100513F0 (anonymous in the db; ret 0x10, name ours): a line for the chat tab (the two trailing arguments are not read)
    __declspec(noinline) void AddChatLine(const TCHAR* Text, DWORD Color, INT Unused1, INT Unused2);
    // 0x10051C20 (anonymous in the db; name ours): the typed text and the chat's lines go, Unknown100 is cleared (NCConsole::Reset,
    // OnExCloseMPCC)
    void Reset();
};
NW_ASSERT_OFFSET(NCCommandChannelWnd, BackTex, 0xE8);
NW_ASSERT_OFFSET(NCCommandChannelWnd, ChatTypeCheck, 0xFC);
NW_ASSERT_SIZE(NCCommandChannelWnd, 0x104);

#pragma pack(pop)
