/*=============================================================================
    L2ChatWnd.cpp - the L2 skin's conversation window (C3 only; C3 addresses). Stock: ShowWindow 0x10124240, the minimize /
    close handlers 0x10124260 / 0x10124280, SetTitle 0x101242b0, ClearAll 0x101242d0, OnDestroy 0x101245a0, OnPaint
    0x10124640, the Send handler 0x10125320, WindowProc 0x10125970, the constructor / destructor 0x10125a00 .. 0x10125b3f
    and OnCreate 0x10125bd0.
=============================================================================*/
#include "nwindow/L2ChatWnd.h"
#include "nwindow/L2FriendWnd.h"
#include "nwindow/L2ChatBox.h"
#include "nwindow/L2ChatFrameCtrl.h"
#include "nwindow/NChatUtil.h"
#include "nwindow/NCEditBox.h"
#include "nwindow/NCFrameCtrl.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NConsoleWnd.h"

enum
{
    NID_L2CHAT_FRAME = 0xA70,      // the title bar
    NID_L2CHAT_EDIT = 0xA71,       // the edit box
};

// 0x1014a4b0 - the base window's constructor, the name empty, the children empty (+0xD8 and +0xDC are 1)
L2ChatWnd::L2ChatWnd()
{
    Messenger = NULL;
    TitleBar = NULL;
    TextBox = NULL;
    EditBox = NULL;
    Unknown_D8 = 1;
    bHidden = 1;
}
NW_HOOK_CTOR(0x1014a4b0, L2ChatWnd)

// 0x1014a5d0 (ret 0x10) - the same, with the owner and the friend's name from the arguments (the name's copy is destroyed on return)
L2ChatWnd::L2ChatWnd(void* InMessenger, FString InFriendName)
{
    Messenger = InMessenger;
    FriendName = InFriendName;
    Unknown_D8 = 1;
    bHidden = 1;
    TitleBar = NULL;
    TextBox = NULL;
    EditBox = NULL;
}
NW_HOOK_CTOR_ARGS(0x1014a5d0, L2ChatWnd, (void* A, FString B), (A, B))

// 0x1014a560 - the name goes, then the base
NW_HOOK_DTOR(0x1014a560, L2ChatWnd)

// 0x1014a420 (vslot 25) - minimize (0xF0F0), close (0xF0F1) and the edit box's Enter (0xA71, lParam is passed on)
INT L2ChatWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCWnd::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND)
    {
        if (wParam == NID_FRAME_MINIMIZE)
            return OnMinimize() != 0;
        if (wParam == NID_FRAME_CLOSE)
            return OnClose() != 0;
        if (wParam == NID_L2CHAT_EDIT)
            return OnSend(lParam) != 0;
    }
    return 0;
}
NW_HOOK_METHOD(0x1014a420, L2ChatWnd, WindowProc)

// 0x10148d70 (vslot 48) - shown, no longer minimized
void L2ChatWnd::ShowWindow()
{
    NCWnd::ShowWindow();
    bHidden = 0;
}
NW_HOOK_METHOD(0x10148d70, L2ChatWnd, ShowWindow)

// 0x10034b90  NOT PORTED (not in eligible.csv (category D or not a game function)); the stock slot points straight at NCWnd::SetTitle, which
// the standalone class inherits
#if NW_HOOK_MODE
NW_NAKED void L2ChatWnd::SetTitle(const TCHAR* Title) NW_STOCK(0x10034b90)
#endif

// 0x10148d90 (name ours) - the minimize button: hidden and remembered as minimized
INT L2ChatWnd::OnMinimize()
{
    NCWnd::HideWindow();   // 0x10035ab0, called directly (kept out of line)
    bHidden = 1;
    return 1;
}
NW_HOOK_METHOD(0x10148d90, L2ChatWnd, OnMinimize)

// 0x10148db0 (name ours) - the close button: hidden, the messenger told (0x10139490: the window leaves its lists; it is
// called whether or not there is a messenger); the message is not marked as handled
INT L2ChatWnd::OnClose()
{
    NCWnd::HideWindow();   // 0x10035ab0
    ((L2FriendWnd*)Messenger)->OnChatClosed(this);      // L2FriendWnd::OnChatClosed 0x1015de00
    bHidden = 1;
    return 0;
}
NW_HOOK_METHOD(0x10148db0, L2ChatWnd, OnClose)

// 0x10148de0 (vslot 56, ret 4; was a stock jump in NNewVirtuals.gen.cpp) - the title bar of this window, TitleBar (+0x100: its own
// L2ChatFrameCtrl; the base's FrameCtrl at +0x7C stays empty), takes the SysString title. The stock is
// `mov ecx, [ecx + 0x100]; jmp NCFrameCtrl::SetTitleID (0x10011810)`, no null test
void L2ChatWnd::SetFrameTitleID(INT ID)
{
    TitleBar->SetTitleID(ID);
}
NW_HOOK_METHOD(0x10148de0, L2ChatWnd, SetFrameTitleID)

// 0x10148df0 (name ours) - the Send flag cleared, the talk and the edit box emptied
void L2ChatWnd::ClearAll()
{
    Unknown_D8 = 0;
    TextBox->Clear();
    EditBox->ClearText();
}
NW_HOOK_METHOD(0x10148df0, L2ChatWnd, ClearAll)

// 0x10125320 (name ours, ret 4) - the edit box's Enter (lParam 1) with a text: the messenger (a MessengerWnd) is asked for the
// friend (0x10131450, the name by value, ret 0xC); a friend that is online (+0xC) gets the text (0x101313b0, name and text by
// value, ret 0x18: the console is asked to send it) and it is shown under the player's own name; any other case shows the
// notice (SystemMsg 1327) and the text indented, both in red. The edit box is emptied
// 0x10149dd0 (name ours, ret 4) - the edit box's Enter (lParam 1) with a text: the messenger (a MessengerWnd) is asked for the
// friend (0x10131450, the name by value, ret 0xC); a friend that is online (+0xC) gets the text (0x101313b0, name and text by
// value, ret 0x18: the console is asked to send it) and it is shown under the player's own name; any other case shows the
// notice (SystemMsg 1327) and the text indented, both in red. The edit box is emptied
INT L2ChatWnd::OnSend(WPARAM lParam)
{
    if (lParam == 1 && EditBox->CommitText())
    {
        void* Friend = ((L2FriendWnd*)Messenger)->FindFriend(MsnStringCopy(FriendName));        // L2FriendWnd::FindFriend 0x101560C0
        if (Friend && ::nw::Field<INT>(Friend, 0xC))
        {
            ((L2FriendWnd*)Messenger)->SendWhisper(MsnStringCopy(FriendName), MsnStringArgOf(EditBox->CommitText()));   // L2FriendWnd::SendWhisper 0x10156020
            User* Me = GConsole->GetMyUser();
            if (Me)
                TextBox->AddMessage(Me->GetName(), EditBox->CommitText(), 0xFFDCDCDC);
        }
        else
        {
            TCHAR Line[1024];
            TextBox->AddMessage(NULL, *GL2GameData.SystemMsg[1327].Message, 0xFFE37171);
            appSprintf(Line, TEXT("   %s"), EditBox->CommitText());
            TextBox->AddMessage(NULL, Line, 0xFFE37171);
        }
        EditBox->ClearText();
    }
    return 1;
}
NW_HOOK_METHOD(0x10149dd0, L2ChatWnd, OnSend)

// 0x101562a0 (ret 0xC, was anonymous) - the friend's name replaced by the argument's (the copy is destroyed on return)
void L2ChatWnd::SetFriendName(FString Name)
{
    FriendName = Name;
}
typedef void (L2ChatWnd::*SigSetFriendName)(FString);
NW_HOOK_VARARGS(0x101562a0, L2ChatWnd, SetFriendName, SigSetFriendName)

// 0x1014a680 (vslot 37) - the back texture, the title bar (id 0xA70), the talk (200 lines, 14 shown) and the edit box
INT L2ChatWnd::OnCreate()
{
    NCWnd::OnCreate();
    BackTex = FindTexture(TEXT("L2UI_CH3.msn_back3"));

    TitleBar = new L2ChatFrameCtrl(1, 0, 0);
    FrameID = NID_L2CHAT_FRAME;
    TitleBar->ID = NID_L2CHAT_FRAME;
    TitleBar->SetPosition(0, 0, 0x100, 0x14);
    CreateChild(TitleBar, NWS_VISIBLE);

    TextBox = new L2ChatBox(200, 0xE, 0xF);
    TextBox->SetPosition(9, 0x60, 0xEE, 0xFC);
    CreateChild(TextBox, NWS_VISIBLE);

    EditBox = new NCEditBox(0, 0, 1, 0x12C0, 0);
    EditBox->ID = NID_L2CHAT_EDIT;
    EditBox->SetPosition(8, 0x168, 0xEF, 0x10);
    CreateChild(EditBox, NWS_VISIBLE);
    return 1;
}
NW_HOOK_METHOD(0x1014a680, L2ChatWnd, OnCreate)

// 0x101490d0 (vslot 38) - the three children go, then the base
INT L2ChatWnd::OnDestroy()
{
    NDestroyChild(this, TitleBar);
    NDestroyChild(this, TextBox);
    NDestroyChild(this, EditBox);
    NCWnd::OnDestroy();
    return 1;
}
NW_HOOK_METHOD(0x101490d0, L2ChatWnd, OnDestroy)

// 0x10149170 (vslot 62) - the back; the friend's name (when there is one) at (11, 54)
INT L2ChatWnd::OnPaint(NCGDevice* Device)
{
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    if (BackTex)
        Device->DrawTexture(0, 0x14, 0x100, 0x17D, 0, 0, 0x100, 0x17D, BackTex, 0xFF, 1);
    if (FriendName.Len() > 0)
        Device->DrawText(0xB, 0x36, 0xFFDCDCDC, *FriendName, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
    Device->ReleaseClip();
    Device->EndPaint();
    return 1;
}
NW_HOOK_METHOD(0x10149170, L2ChatWnd, OnPaint)

NW_CHECK_VTABLE(L2ChatWnd, 0x101db128, 74)
