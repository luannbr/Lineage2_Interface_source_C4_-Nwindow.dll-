#pragma once
// NCChatCommandWnd - the command channel's chat window (UClass "CChatCommandWnd", 0xEC; C4 only): one text list box (an
// NCTextListBox of 200 lines, 8 shown) at (6, 5), 327 x 134, that the command channel window shows on its first tab and the
// console's Say fills with the channel's messages. The command channel window (NCCommandChannelWnd) makes it.
//
// Layout and virtuals from the stock constructor 0x100522F0, OnCreate 0x10052120 and OnDestroy 0x10051890; the WindowProc is
// the folded NControl one (0x10148F10).

#include "nwindow/NCWnd.h"
#pragma pack(push, 4)

class NCListBox;

class NCChatCommandWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCChatCommandWnd, NCWnd, 0, 0x102D0270)

    NCListBox* ChatList;             // +0xE8 an NCTextListBox, id 0xF0A0C1 (name ours)

    NCChatCommandWnd();                                              // 0x100522F0
    // ~NCChatCommandWnd                                             // 0x10052340

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10148F10 (folded, stays stock)
    virtual INT OnCreate();                                          // 37 0x10052120
    virtual INT OnDestroy();                                         // 38 0x10051890

    // 0x10051340 (anonymous in the db; ret 0x10, name ours): the line goes on the list (the last parameter -1); the two trailing
    // arguments are not read
    __declspec(noinline) void AddLine(const TCHAR* Text, DWORD Color, INT Unused1, INT Unused2);
    // 0x100EFDC0 (the stock's one folded function for every window that keeps a list box at +0xE8; hooked in NConsoleWndC2.cpp as
    // NRecipeBuyListWndExt::ClearList): the lines go. This is our copy, kept out of line so that the callers call it, as they do
    __declspec(noinline) void ClearList();
};
NW_ASSERT_OFFSET(NCChatCommandWnd, ChatList, 0xE8);
NW_ASSERT_SIZE(NCChatCommandWnd, 0xEC);

#pragma pack(pop)
