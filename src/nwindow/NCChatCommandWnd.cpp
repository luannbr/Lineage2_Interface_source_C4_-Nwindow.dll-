/*=============================================================================
    NCChatCommandWnd.cpp - the command channel's chat window (C4 only). Stock: 0x100522f0 (constructor), 0x10052340 (destructor),
    0x10052120 (OnCreate), 0x10051890 (OnDestroy), 0x10051340 (AddLine, anonymous in the db).
=============================================================================*/
#include "nwindow/NCChatCommandWnd.h"
#include "nwindow/NCListBox.h"
#include "nwindow/NCTextListBoxC4.h"

// NWM_COMMAND id of the list (names ours)
enum { NID_CHATCMD_LIST = 0xF0A0C1 };

// 0x100522f0 - NCWnd's constructor
NCChatCommandWnd::NCChatCommandWnd()
{
}
NW_HOOK_CTOR(0x100522f0, NCChatCommandWnd)

// 0x10052340
NW_HOOK_DTOR(0x10052340, NCChatCommandWnd)

// 0x10148f10 (vslot 25) - the stock body is the folded plain forward to NCWnd's (shared with other windows)
INT NCChatCommandWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return NCWnd::WindowProc(Msg, wParam, lParam) != 0;
}

// 0x10052120 (vslot 37) - the list of 200 lines, 8 shown (the third argument 15 is the one the list box ignores), as the last child
INT NCChatCommandWnd::OnCreate()
{
    NCWnd::OnCreate();
    ChatList = NewTextListBox(200, 8, 15, 1);
    ChatList->ID = NID_CHATCMD_LIST;
    ChatList->SetPosition(6, 5, 327, 134);
    CreateChild(ChatList, NWS_VISIBLE);
    ChatList->bAutoScroll = 1;
    ChatList->LineGap = 0;
    return 1;
}
NW_HOOK_METHOD(0x10052120, NCChatCommandWnd, OnCreate)

// 0x10051890 (vslot 38, guard) - the list goes (out of the children, deleted), then NCWnd's OnDestroy
INT NCChatCommandWnd::OnDestroy()
{
    guard(NCChatCommandWnd::OnDestroy);
    if (ChatList)
    {
        RemoveChildWindow(ChatList->Entry);
        delete ChatList;
        ChatList = NULL;
    }
    NCWnd::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10051890, NCChatCommandWnd, OnDestroy)

// 0x10051340 (ret 0x10) - the line at the end of the list, its Param -1
void NCChatCommandWnd::AddLine(const TCHAR* Text, DWORD Color, INT Unused1, INT Unused2)
{
    if (ChatList)
        ChatList->AddString(Text, Color, -1);
}
NW_HOOK_METHOD(0x10051340, NCChatCommandWnd, AddLine)

// 0x100efdc0 (folded in the stock; hooked in NConsoleWndC2.cpp) - the list box's Clear (virtual 77)
void NCChatCommandWnd::ClearList()
{
    ChatList->Clear();
}

NW_CHECK_VTABLE(NCChatCommandWnd, 0x101ab9c0, 74)
