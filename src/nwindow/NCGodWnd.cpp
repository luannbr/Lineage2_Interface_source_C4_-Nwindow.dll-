/*=============================================================================
    NCGodWnd.cpp - the screen-sized top window of the game screen (C3 only; C3 addresses). Stock: 0x10085620 ..
    0x10085D8F (the small methods, OnDestroy, DefaultPosition, SetWindowSize, ShowWarningMessage), OnCreate 0x10085DD0
    .. 0x1008618B, the constructor 0x100861C0 and the destructor 0x10086250. The console calls the small methods by
    address (NConsoleWnd::ShowMessageWnd, ToggleOptionWnd, C3Virtual202 / 203, ...).
=============================================================================*/
#include "nwindow/NCGodWnd.h"
#include "nwindow/NCMessageWnd.h"
#include "nwindow/NCLoginWnd.h"
#include "nwindow/NCGaraDialogBox.h"
#include "nwindow/NCAuthWnd.h"
#include "nwindow/NCWarningWnd.h"
#include "nwindow/NCEulaWnd.h"
#include "nwindow/NCAnnounceWnd.h"
#include "nwindow/NCConsole.h"
#include "nwindow/NCOptionWnd.h"
#include "nwindow/NCRestartMenuWnd.h"
#include "nwindow/NCWarningDialogBox.h"
#include "nwindow/NConsoleWnd.h"
#include "nwindow/NGameData.h"

// The sizes and places of the children (the stock code repeats them inline)
enum
{
    GOD_MessageHeight = 52,
    GOD_RestartSize = 128,
    GOD_OptionWidth = 256,
    GOD_OptionHeight = 480,
    GOD_WarningWidth = 256,
    GOD_WarningHeight = 108,
    SYSSTR_OptionTitle = 146,   // GL2GameData + 0x66164
};

// A child window goes (repeated inline in the stock OnDestroy)
template <class T> static FORCEINLINE void DestroyChild(NCWnd* Parent, T*& Child)
{
    if (Child)
    {
        Parent->RemoveChildWindow(Child->Entry);
        delete Child;
        Child = NULL;
    }
}

// 0x10091590 - the five fields cleared, then the vtable
NCGodWnd::NCGodWnd()
    : UnknownC8(NULL), LoginWnd(NULL), MessageWnd(NULL), OptionWnd(NULL), WarningBox(NULL)
{
}
NW_HOOK_CTOR(0x10091590, NCGodWnd)

// 0x10091620
NW_HOOK_DTOR(0x10091620, NCGodWnd)

// 0x10148f10 (vslot 25) - NControl::WindowProc's body, which the stock vtables of these windows share (the C3 address was
// 0x10119f60): NCWnd's, normalized to 0 or 1
INT NCGodWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return NCWnd::WindowProc(Msg, wParam, lParam) != 0;
}

// 0x10090970 (guard: NCGodWnd::ReceiveServerList; ret 4) - the message line emptied (the console shows its own text while
// the server list is awaited), the options window hidden and the login screen back to its first state: its four windows
// (the account, the warning, the licence, the announcement) hidden, the fields cleared and the server list given the
// server played last
void NCGodWnd::ReceiveServerList(INT Param)
{
    guard(NCGodWnd::ReceiveServerList);
    if (MessageWnd)
        MessageWnd->ClearText();
    if (OptionWnd)
        OptionWnd->HideWindow();
    if (LoginWnd)
    {
        LoginWnd->ResetWindows();
        LoginWnd->AuthWnd->HideWindow();
        LoginWnd->WarningWnd->HideWindow();
        LoginWnd->EulaWnd->HideWindow();
        LoginWnd->AnnounceWnd->HideWindow();
        static_cast<NCLoginServerWnd*>(LoginWnd->ServerWnd)->ReceiveServerList(Param);
    }
    unguard;
}
NW_HOOK_METHOD(0x10090970, NCGodWnd, ReceiveServerList)

// 0x10090a80 (guard: NCGodWnd::RestartBegin) - the options window hidden
void NCGodWnd::RestartBegin()
{
    guard(NCGodWnd::RestartBegin);
    if (OptionWnd)
        OptionWnd->HideWindow();
    unguard;
}
NW_HOOK_METHOD(0x10090a80, NCGodWnd, RestartBegin)

// 0x10090b00 (guard: NCGodWnd::ShowOptionWnd) - the options window toggles: hidden with the close sound (6), shown
// with the open sound (5) and the focus
void NCGodWnd::ShowOptionWnd()
{
    guard(NCGodWnd::ShowOptionWnd);
    if (OptionWnd->IsShowWindow())
    {
        GConsole->PlayInterfaceSound(6);
        OptionWnd->HideWindow();
    }
    else
    {
        GConsole->PlayInterfaceSound(5);
        OptionWnd->ShowWindow();
        OptionWnd->SetFocus();
    }
    unguard;
}
NW_HOOK_METHOD(0x10090b00, NCGodWnd, ShowOptionWnd)

// 0x10090bd0 (guard: NCGodWnd::ShowMessage; ret 4) - the line's text is replaced by the one given (nothing for an empty
// one), and the line is shown with the focus when it was not
void NCGodWnd::ShowMessage(const TCHAR* Text)
{
    guard(NCGodWnd::ShowMessage);
    if (MessageWnd)
    {
        MessageWnd->ClearText();
        if (Text && *Text)
            MessageWnd->SetText(Text);
        if (!MessageWnd->IsShowWindow())
        {
            MessageWnd->ShowWindow();
            MessageWnd->SetFocus();
        }
    }
    unguard;
}
NW_HOOK_METHOD(0x10090bd0, NCGodWnd, ShowMessage)

// 0x10090ca0 (vslot 38, guard: NCGodWnd::OnDestroy) - the login screen, the options window, the message line, the warning
// box and the dialog box go; NCWnd's OnDestroy is not called
INT NCGodWnd::OnDestroy()
{
    guard(NCGodWnd::OnDestroy);
    DestroyChild(this, LoginWnd);
    DestroyChild(this, OptionWnd);
    DestroyChild(this, MessageWnd);
    DestroyChild(this, WarningBox);
    DestroyChild(this, GaraDialog);
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10090ca0, NCGodWnd, OnDestroy)

// 0x10090df0 (guard: NCGodWnd::DefaultPosition) - the windows back in their places for the size the window has now: the
// options window centered (256 wide: its place is set on the screen, from this window's origin) and the message line along
// the bottom edge (52 high; its width follows the window's). The login screen and the boxes keep their places
void NCGodWnd::DefaultPosition()
{
    guard(NCGodWnd::DefaultPosition);
    POINT Origin;
    Origin.x = ScreenX;
    Origin.y = ScreenY;
    FLOAT W = Width;
    FLOAT H = Height;
    if (OptionWnd)
        OptionWnd->SetScreenPos((INT)(W * 0.5f - 128.0f), (INT)(H * 0.5f - 200.0f), &Origin);
    if (MessageWnd)
    {
        MessageWnd->SetWindowSize(W, (FLOAT)GOD_MessageHeight);
        MessageWnd->SetScreenPos(0, (INT)(H - 52.0f), &Origin);
    }
    unguard;
}
NW_HOOK_METHOD(0x10090df0, NCGodWnd, DefaultPosition)

// 0x10090f20 (vslot 47, guard: NCGodWnd::SetWindowSize; ret 8) - NCWnd's, then the message line takes the width and the login
// screen the width and the height less the message line's
void NCGodWnd::SetWindowSize(FLOAT W, FLOAT H)
{
    guard(NCGodWnd::SetWindowSize);
    NCWnd::SetWindowSize(W, H);
    FLOAT NewHeight = Height;
    FLOAT NewWidth = Width;
    if (MessageWnd)
        MessageWnd->SetWindowSize(NewWidth, (FLOAT)GOD_MessageHeight);
    if (LoginWnd)
        LoginWnd->SetWindowSize(NewWidth, NewHeight - 52.0f);
    unguard;
}
NW_HOOK_METHOD(0x10090f20, NCGodWnd, SetWindowSize)

// 0x10090ff0 (guard: NCGodWnd::ShowWarningMessage; ret 8) - the warning box: marked as one that ends the client when
// asked to (bExit), and, with a text, raised modal (NCWarningDialogBox 0x10008500 sets the text, NCDialogBox's slot 75 shows it
// as the modal window with the focus)
void NCGodWnd::ShowWarningMessage(const TCHAR* Text, INT bExit)
{
    guard(NCGodWnd::ShowWarningMessage);
    if (bExit)
        WarningBox->bExit = 1;
    if (Text)
    {
        WarningBox->SetWarningMessage(Text);   // 0x10008500
        ::nw::VirtualCall<void>(WarningBox, 75);
    }
    unguard;
}
NW_HOOK_METHOD(0x10090ff0, NCGodWnd, ShowWarningMessage)

// 0x100910a0 (vslot 37, guard: NCGodWnd::OnCreate) - C4: the login screen and the dialog box (hidden; only for a client that is
// neither the test console nor the html test), the message line with its first text (shown, anchored at the bottom), the
// options window (hidden, with its title) and the warning box (hidden); NCWnd's OnCreate is not called
INT NCGodWnd::OnCreate()
{
    guard(NCGodWnd::OnCreate);
    FLOAT H = Height;
    FLOAT W = Width;
    if (!GIsOnlyL2Console && !GIsTestHtml)
    {
        LoginWnd = new NCLoginWnd;
        LoginWnd->SetPosition(0, 0, (INT)W, (INT)(H - 52.0f));
        CreateChild(LoginWnd, NWS_VISIBLE);
        LoginWnd->ShowWindow();

        GaraDialog = new NCGaraDialogBox;
        GaraDialog->SetPosition((INT)(W * 0.5f - 128.0f), (INT)(H * 0.5f - 54.0f), 256, 108);
        CreateChild(GaraDialog, NWS_VISIBLE);
        GaraDialog->HideWindow();
        GaraDialog->SetButtonTexts(0x8C, 0x8D);
        GaraDialog->SetAnchor(5, 0, 0);
    }

    MessageWnd = new NCMessageWnd;
    MessageWnd->SetPosition(0, (INT)(H - 52.0f), (INT)W, GOD_MessageHeight);
    CreateChild(MessageWnd, NWS_VISIBLE);
    MessageWnd->SetText(*::nw::Field<FString>(&GL2GameData, 0x7346C));
    MessageWnd->ShowWindow();
    MessageWnd->SetAnchor(8, 0, 0);

    OptionWnd = new NCOptionWnd((INT)(INT_PTR)UnknownC8);
    OptionWnd->SetPosition((INT)(W * 0.5f - 128.0f), (INT)(H * 0.5f - 240.0f), GOD_OptionWidth, GOD_OptionHeight);
    CreateChild(OptionWnd, 0x811002);
    OptionWnd->HideWindow();
    ::nw::VirtualCall<void>(OptionWnd, 56, SYSSTR_OptionTitle);   // NCWnd::SetTitleID (slot 56)

    WarningBox = new NCWarningDialogBox;
    WarningBox->SetPosition(GSEKScreenX / 2 - 128, GSEKScreenY / 2 - 54, GOD_WarningWidth, GOD_WarningHeight);
    CreateChild(WarningBox, NWS_VISIBLE);
    WarningBox->HideWindow();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100910a0, NCGodWnd, OnCreate)

NW_CHECK_VTABLE(NCGodWnd, 0x101b5d58, 74)
