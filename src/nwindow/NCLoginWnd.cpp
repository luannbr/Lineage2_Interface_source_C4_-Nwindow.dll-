/*=============================================================================
    NCLoginWnd.cpp - the login screen. Stock: 0x10082a90 .. 0x10082cdd (0x10082c50 is a body
    the linker folded with NCSystemMenuWnd's), 0x100838c0 .. 0x1008462f (NCAuthWnd's functions sit in
    between).
=============================================================================*/
#include "nwindow/NCLoginWnd.h"
#include "nwindow/NCAuthWnd.h"
#include "nwindow/NCAnnounceWnd.h"
#include "nwindow/NCEulaWnd.h"
#include "nwindow/NCKeyCryptWarningDialogBox.h"
#include "nwindow/NCGodWnd.h"
#include "nwindow/NCNoticeWnd.h"
#include "nwindow/NCServerSelectWnd.h"
#include "nwindow/NKeyCrypt.h"
#include "nwindow/NCWarningWnd.h"
#include "nwindow/NCReplayListWnd.h"
#include "nwindow/NCButton.h"
#include "nwindow/NCCreditWnd.h"
#include "nwindow/NCMessageWnd.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NConsoleWnd.h"

// 0x1009f700 - only the logo is cleared; the rest is left as allocated (the stock reads the layout of the boxes it makes
// itself)
NCLoginWnd::NCLoginWnd()
{
    LogoTex = NULL;
}
NW_HOOK_CTOR(0x1009f700, NCLoginWnd)

// 0x1009f750
NW_HOOK_DTOR(0x1009f750, NCLoginWnd)

// (slot 25, WindowProc, is NCWnd's own body, 0x10034390: the stock vtable of this class points at it, so the class has no override)

// 0x1009f7a0 (vslot 37) - the logos by the country's names in localization.ini, the boxes centered on the window (the
// sizes halved as floats), the server window, the function (buttons) window in the lower right corner and, new in C4,
// the replay list under the server list; the key crypt box last, hidden. C4 addresses of the stock constructors:
// NCAuthWnd 0x1009F630, NCWarningWnd 0x100BE6D0, NCEulaWnd 0x100BE910, NCAnnounceWnd 0x100BEC00, NCKeyCryptWarningDialogBox
// 0x1009F590, NCReplayListWnd 0x100F7F30 (class at 0x102FFA78)
INT NCLoginWnd::OnCreate()
{
    NCWnd::OnCreate();
    FString LogoName;
    TCHAR Key[192];
    appSprintf(Key, TEXT("%s_LogoTexture"), GCountryName);
    if (!GConfig->GetString(GCountryName, Key, LogoName, TEXT("localization.ini")))
        LogoName = TEXT("L2Font.start_logo-k");
    LogoTex = FindTexture(*LogoName);
    appSprintf(Key, TEXT("%s_MiniLogoTexture"), GCountryName);
    if (!GConfig->GetString(GCountryName, Key, LogoName, TEXT("localization.ini")))
        LogoName = TEXT("L2Font.start_minilogo-k");
    MiniLogoTex = FindTexture(*LogoName);

    AuthWnd = new NCAuthWnd();
    AuthWnd->SetPosition((INT)(Width * 0.5f - 128.0f), (INT)(Height * 0.5f - 54.0f), 256, 108);
    CreateChild(AuthWnd, NWS_NOCLOSE | NWS_VISIBLE);
    ::nw::VirtualCall<void>(AuthWnd, 56, 149);                  // SetTitle by id: "Log In"
    AuthWnd->SetAnchor(5, 0, 0);

    WarningWnd = new NCWarningWnd();
    WarningWnd->SetPosition((INT)(Width * 0.5f - 273.0f), (INT)(Height * 0.5f - 212.0f), 546, 425);
    CreateChild(WarningWnd, NWS_NOFOCUSSEARCH | NWS_NOCLOSE | NWS_VISIBLE);
    ::nw::VirtualCall<void>(WarningWnd, 56, 538);               // "Warning"
    WarningWnd->SetAnchor(5, 0, 0);

    EulaWnd = new NCEulaWnd();
    EulaWnd->SetPosition((INT)(Width * 0.5f - 273.0f), (INT)(Height * 0.5f - 212.0f), 546, 425);
    CreateChild(EulaWnd, NWS_NOFOCUSSEARCH | NWS_NOCLOSE | NWS_VISIBLE);
    ::nw::VirtualCall<void>(EulaWnd, 56, 598);                  // the EULA's title
    EulaWnd->SetAnchor(5, 0, 0);

    AnnounceWnd = new NCAnnounceWnd();
    AnnounceWnd->SetPosition((INT)(Width * 0.5f - 273.0f), (INT)(Height * 0.5f - 212.0f), 546, 425);
    CreateChild(AnnounceWnd, NWS_NOFOCUSSEARCH | NWS_NOCLOSE | NWS_VISIBLE);
    AnnounceWnd->SetAnchor(5, 0, 0);

    ServerWnd = new NCLoginServerWnd();
    ServerWnd->SetPosition(GSEKScreenX / 2 - 158, GSEKScreenY / 2 - 271, 316, 542);
    CreateChild(ServerWnd, NWS_VISIBLE);
    ServerWnd->SetAnchor(5, 0, 0);

    FunctionWnd = new NCLoginFunctionWnd();
    FunctionWnd->SetPosition(GSEKScreenX - 154, GSEKScreenY - 200, 120, 138);
    CreateChild(FunctionWnd, NWS_VISIBLE);
    FunctionWnd->SetAnchor(9, -34, -10);

    ReplayListWnd = new NCReplayListWnd();
    ReplayListWnd->SetPosition(GSEKScreenX / 2 - 158, GSEKScreenY / 2 - 175, 316, 350);
    CreateChild(ReplayListWnd, NWS_VISIBLE);

    KeyCryptWnd = new NCKeyCryptWarningDialogBox();
    KeyCryptWnd->SetPosition(GSEKScreenX / 2 - 101, GSEKScreenY / 2 - 43, 205, 87);
    CreateChild(KeyCryptWnd, NWS_VISIBLE);
    KeyCryptWnd->HideWindow();
    FunctionWnd->SetTabOrder(AuthWnd->Entry, AuthWnd->Entry);
    return 1;
}
NW_HOOK_METHOD(0x1009f7a0, NCLoginWnd, OnCreate)

// a child window goes (repeated inline in the stock OnDestroy)
template <class T> static FORCEINLINE void DestroyChild(NCWnd* Parent, T*& Child)
{
    if (Child)
    {
        Parent->RemoveChildWindow(Child->Entry);
        if (Child)
            delete Child;
        Child = NULL;
    }
}

// 0x1009edf0 (vslot 38) - guarded; the children go in the order the stock has (the function window before the server list)
INT NCLoginWnd::OnDestroy()
{
    guard(NCLoginWnd::OnDestroy);
    DestroyChild(this, AuthWnd);
    DestroyChild(this, WarningWnd);
    DestroyChild(this, EulaWnd);
    DestroyChild(this, AnnounceWnd);
    DestroyChild(this, FunctionWnd);
    DestroyChild(this, ServerWnd);
    DestroyChild(this, KeyCryptWnd);
    DestroyChild(this, ReplayListWnd);
    NCWnd::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1009edf0, NCLoginWnd, OnDestroy)

// 0x1009e610 (vslot 48) - only the login box stays up (C4: the replay list hides too); the key crypt box is shown (and takes
// the focus) while the key crypt module (GL2UseKeyCrypt, and now GIsL2NetLogin) is on and not yet registered
void NCLoginWnd::ShowWindow()
{
    NCWnd::ShowWindow();
    WarningWnd->HideWindow();
    EulaWnd->HideWindow();
    AnnounceWnd->HideWindow();
    ServerWnd->HideWindow();
    ReplayListWnd->HideWindow();
    if (GL2UseKeyCrypt && GIsL2NetLogin)
    {
        if (!GConsole->KeyCrypt || !GConsole->KeyCrypt->Open() || !GConsole->KeyCrypt->RegisterMessage())
        {
            KeyCryptWnd->NewVirtual75();     // NCDialogBox::DoModal (slot 75)
            KeyCryptWnd->SetFocus();
        }
        else
            KeyCryptWnd->HideWindow();
    }
    else
        KeyCryptWnd->HideWindow();
}
NW_HOOK_METHOD(0x1009e610, NCLoginWnd, ShowWindow)

// 0x1009e6d0 (vslot 49) - with the key crypt module on, the driver is closed along with the screen
void NCLoginWnd::HideWindow()
{
    NCWnd::HideWindow();
    if (GL2UseKeyCrypt && GConsole->KeyCrypt)
        GConsole->KeyCrypt->Close();
}
NW_HOOK_METHOD(0x1009e6d0, NCLoginWnd, HideWindow)

// 0x1009e700 / 0x1009e780 (slots 74 and 75, stock placeholders in NNewVirtuals.gen.cpp, so hooked through a stand-in class
// that carries them as methods; names ours) - the two modes of the screen
struct NLoginWndModes : public NCLoginWnd
{
    __declspec(noinline) void ShowReplayMode();
    __declspec(noinline) void ShowLoginMode();
};

// the replay list mode: the login box, the warnings and the server list hide, the function window too, the replay list
// shows, the key crypt box hides
void NLoginWndModes::ShowReplayMode()
{
    AuthWnd->HideWindow();
    WarningWnd->HideWindow();
    EulaWnd->HideWindow();
    AnnounceWnd->HideWindow();
    ServerWnd->HideWindow();
    FunctionWnd->HideWindow();
    ReplayListWnd->ShowWindow();
    KeyCryptWnd->HideWindow();
}
NW_HOOK_METHOD(0x1009e700, NLoginWndModes, ShowReplayMode)

// the login mode: the login box and the function window show, the rest hides
void NLoginWndModes::ShowLoginMode()
{
    AuthWnd->ShowWindow();
    WarningWnd->HideWindow();
    EulaWnd->HideWindow();
    AnnounceWnd->HideWindow();
    ServerWnd->HideWindow();
    FunctionWnd->ShowWindow();
    ReplayListWnd->HideWindow();
    KeyCryptWnd->HideWindow();
}
NW_HOOK_METHOD(0x1009e780, NLoginWndModes, ShowLoginMode)

// 0x1009efd0 (vslot 62) - the small logo in the upper left corner (232 x 120), the big one centered at a twentieth of
// the height (neither is checked for NULL)
INT NCLoginWnd::OnPaint(NCGDevice* Device)
{
    guard(NCLoginWnd::OnPaint);
    Device->SetClip(ScreenX, ScreenY, Width, Height);
    Device->BeginPaint(ScreenX, ScreenY);
    Device->DrawTexture(0, 0, 232, 120, 0, 0, 232, 120, MiniLogoTex);
    Device->DrawTexture((INT)(Width * 0.5f - 128.0f), (INT)(Height * 0.05f), 256, 256, 0, 0, 256, 256, LogoTex);
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1009efd0, NCLoginWnd, OnPaint)

// (C2 only: "Options" and "Credits" were buttons of this window; C3 keeps the buttons in NCLoginFunctionWnd)

// 0x1009e800 - back to an empty login screen (failed login, server list cancelled...): the password, the warning and
// EULA texts and the server list
void NCLoginWnd::ResetWindows()
{
    AuthWnd->ClearPassword();
    WarningWnd->ClearText();
    EulaWnd->ClearText();
    ((NCLoginServerWnd*)ServerWnd)->ServerSelectWnd->ClearServers();
}
NW_HOOK_METHOD(0x1009e800, NCLoginWnd, ResetWindows)

NW_CHECK_VTABLE(NCLoginWnd, 0x101b8b98, 76)

/*-----------------------------------------------------------------------------
    NCLoginFunctionWnd (C3): the four buttons of the lower right corner.
-----------------------------------------------------------------------------*/

// 0x1009ddd0 - NCWnd's constructor and the vtable (the stock one returns this)
NCLoginFunctionWnd::NCLoginFunctionWnd()
{
}
NW_HOOK_CTOR(0x1009ddd0, NCLoginFunctionWnd)

// 0x1009de20
NW_HOOK_DTOR(0x1009de20, NCLoginFunctionWnd)

// 0x1009d360 (vslot 25) - the buttons' ids go to the five handlers (virtual slots 74, 77, 75, 76 and 78)
INT NCLoginFunctionWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCWnd::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND)
    {
        if (wParam == NID_LOGIN_NEWACCOUNT)
            return OnNewAccountBtnClick() != 0;
        if (wParam == NID_LOGIN_LOSTACCOUNT)
            return OnLostAccountBtnClick() != 0;
        if (wParam == NID_LOGIN_OPTIONS)
            return OnOptionsBtnClick() != 0;
        if (wParam == NID_LOGIN_CREDITS)
            return OnCreditsBtnClick() != 0;
        if (wParam == NID_LOGIN_REPLAY)
        {
            NewVirtual78();
            return 1;
        }
    }
    return 0;
}
NW_HOOK_METHOD(0x1009d360, NCLoginFunctionWnd, WindowProc)

// 0x1009d9a0 (vslot 37) - the backdrop and the five buttons, 96 x 23 each, one under the other 25 pixels apart from y 8
// (NCWnd::OnCreate is called); C4: the fifth button (replay) is new and the others moved up
INT NCLoginFunctionWnd::OnCreate()
{
    guard(NCLoginFunctionWnd::OnCreate);
    NCWnd::OnCreate();
    BackTex = FindTexture(TEXT("L2UI_CH3.LobbyWnd.lobby_menuback"));

    NewAccountButton = new NCButton(TEXT("L2UI_CH3.Button.BigButton"),
                                    TEXT("L2UI_CH3.Button.BigButton_down"), 0);
    NewAccountButton->ID = NID_LOGIN_NEWACCOUNT;
    NewAccountButton->SetPosition(13, 8, 96, 23);
    CreateChild(NewAccountButton, NWS_VISIBLE);
    NewAccountButton->SetTextID(154);

    LostAccountButton = new NCButton(TEXT("L2UI_CH3.Button.BigButton"),
                                     TEXT("L2UI_CH3.Button.BigButton_down"), 0);
    LostAccountButton->ID = NID_LOGIN_LOSTACCOUNT;
    LostAccountButton->SetPosition(13, 33, 96, 23);
    CreateChild(LostAccountButton, NWS_VISIBLE);
    LostAccountButton->SetTextID(155);

    OptionsButton = new NCButton(TEXT("L2UI_CH3.Button.BigButton"),
                                 TEXT("L2UI_CH3.Button.BigButton_down"), 0);
    OptionsButton->ID = NID_LOGIN_OPTIONS;
    OptionsButton->SetPosition(13, 58, 96, 23);
    CreateChild(OptionsButton, NWS_VISIBLE);
    OptionsButton->SetTextID(156);

    CreditsButton = new NCButton(TEXT("L2UI_CH3.Button.BigButton"),
                                 TEXT("L2UI_CH3.Button.BigButton_down"), 0);
    CreditsButton->ID = NID_LOGIN_CREDITS;
    CreditsButton->SetPosition(13, 83, 96, 23);
    CreateChild(CreditsButton, NWS_VISIBLE);
    CreditsButton->SetTextID(805);

    ReplayButton = new NCButton(TEXT("L2UI_CH3.Button.BigButton"),
                                TEXT("L2UI_CH3.Button.BigButton_down"), 0);
    ReplayButton->ID = NID_LOGIN_REPLAY;
    ReplayButton->SetPosition(13, 107, 96, 23);
    CreateChild(ReplayButton, NWS_VISIBLE);
    ReplayButton->SetTextID(1251);
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1009d9a0, NCLoginFunctionWnd, OnCreate)

// 0x1009d850 (vslot 38) - the five buttons go, then NCWnd's OnDestroy
INT NCLoginFunctionWnd::OnDestroy()
{
    guard(NCLoginFunctionWnd::OnDestroy);
    DestroyChild(this, NewAccountButton);
    DestroyChild(this, LostAccountButton);
    DestroyChild(this, OptionsButton);
    DestroyChild(this, CreditsButton);
    DestroyChild(this, ReplayButton);
    NCWnd::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1009d850, NCLoginFunctionWnd, OnDestroy)

// 0x1009d760 (vslot 62) - the backdrop (a 120 x 138 piece) stretched over the window
INT NCLoginFunctionWnd::OnPaint(NCGDevice* Device)
{
    guard(NCLoginFunctionWnd::OnPaint);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    Device->BeginPaint(ScreenX, ScreenY);
    if (BackTex)
        Device->DrawTexture(0, 0, (INT)Width, (INT)Height, 0, 0, 120, 138, BackTex);
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1009d760, NCLoginFunctionWnd, OnPaint)

// 0x1009d440 (slot 74) - the message window shows systemmsg 1186 (where to make an account). The four handlers share the
// guard name OnNewAccountBtnClick in the stock
INT NCLoginFunctionWnd::OnNewAccountBtnClick()
{
    guard(NCLoginFunctionWnd::OnNewAccountBtnClick);
    GConsole->ShowMessageWnd(*GL2GameData.SystemMsg[1186].Message);
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1009d440, NCLoginFunctionWnd, OnNewAccountBtnClick)

// 0x1009d4d0 (slot 77) - the same with systemmsg 1187 (where to recover a lost account)
INT NCLoginFunctionWnd::OnLostAccountBtnClick()
{
    guard(NCLoginFunctionWnd::OnNewAccountBtnClick);
    GConsole->ShowMessageWnd(*GL2GameData.SystemMsg[1187].Message);
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1009d4d0, NCLoginFunctionWnd, OnLostAccountBtnClick)

// 0x1009d560 (slot 75) - the options window (C4: NConsoleWnd 0x1006A350, which toggles it through the god window)
INT NCLoginFunctionWnd::OnOptionsBtnClick()
{
    guard(NCLoginFunctionWnd::OnNewAccountBtnClick);
    GConsole->ShowOptionWnd();                                      // 0x1006A350
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1009d560, NCLoginFunctionWnd, OnOptionsBtnClick)

// 0x1009d5e0 (slot 76) - the credits: the login screen and the message line of the god window go away, the credit window
// takes the screen and the console's StopIntroMusic (slot 346) follows
INT NCLoginFunctionWnd::OnCreditsBtnClick()
{
    guard(NCLoginFunctionWnd::OnNewAccountBtnClick);
    NCGodWnd* God = (NCGodWnd*)GConsole->GodWnd;
    God->LoginWnd->HideWindow();
    God->MessageWnd->HideWindow();
    GConsole->CreditWnd->ShowWindow();
    ((NCCreditWnd*)GConsole->CreditWnd)->Start();
    GConsole->CreditWnd->SetFocus();
    GConsole->StopIntroMusic();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1009d5e0, NCLoginFunctionWnd, OnCreditsBtnClick)

// 0x1009d6d0 (slot 78, a stock placeholder in NNewVirtuals.gen.cpp, so hooked through a stand-in class; name ours) - the
// replay list: the login screen's replay mode (its slot 74)
struct NLoginFunctionWndFns : public NCLoginFunctionWnd
{
    __declspec(noinline) INT OnReplayBtnClick();
};
INT NLoginFunctionWndFns::OnReplayBtnClick()
{
    guard(NCLoginFunctionWnd::OnReplayBtnClick);
    ((NCGodWnd*)GConsole->GodWnd)->LoginWnd->NewVirtual74();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1009d6d0, NLoginFunctionWndFns, OnReplayBtnClick)

NW_CHECK_VTABLE(NCLoginFunctionWnd, 0x101b8278, 79)

/*-----------------------------------------------------------------------------
    NCLoginServerWnd (C3): the server list and its help page.
-----------------------------------------------------------------------------*/

// 0x1009e080 - NCWnd's constructor and the vtable (the stock one returns this)
NCLoginServerWnd::NCLoginServerWnd()
{
}
NW_HOOK_CTOR(0x1009e080, NCLoginServerWnd)

// 0x1009e0d0
NW_HOOK_DTOR(0x1009e0d0, NCLoginServerWnd)

// 0x1009e120 (vslot 37) - the list (a 316 x 330 window of style 0x200002, titled by id 449) over the help page (316 x 205 at
// (0, 337), titled by id 857); NCWnd::OnCreate is not called. C4: the titles are set by id (slot 56), the list's
// constructor is 0x101017A0 and the page's is NCWnd's, inline, and the vtable
INT NCLoginServerWnd::OnCreate()
{
    guard(NCLoginServerWnd::OnCreate);
    ServerSelectWnd = new NCServerSelectWnd();
    ServerSelectWnd->SetPosition(0, 0, 316, 330);
    CreateChild(ServerSelectWnd, NWS_NOFOCUSSEARCH | NWS_VISIBLE);
    ::nw::VirtualCall<void>(ServerSelectWnd, 56, 449);

    ServerInfoWnd = new NCServerInfoWnd();
    ServerInfoWnd->SetPosition(0, 337, 316, 205);
    CreateChild(ServerInfoWnd, NWS_VISIBLE);
    ::nw::VirtualCall<void>(ServerInfoWnd, 56, 857);
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1009e120, NCLoginServerWnd, OnCreate)

// 0x1009df10 (vslot 38) - the list and the page go; NCWnd's OnDestroy is not called
INT NCLoginServerWnd::OnDestroy()
{
    guard(NCLoginServerWnd::OnDestroy);
    if (ServerSelectWnd)
    {
        RemoveChildWindow(ServerSelectWnd->Entry);
        delete ServerSelectWnd;
        ServerSelectWnd = NULL;
    }
    if (ServerInfoWnd)
    {
        RemoveChildWindow(ServerInfoWnd->Entry);
        delete ServerInfoWnd;
        ServerInfoWnd = NULL;
    }
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1009df10, NCLoginServerWnd, OnDestroy)

// 0x1009de80 (ret 4) - the window is shown, the list learns the server played last (the stock body is NCServerSelectWnd's
// SetLastServerID, shared with other classes' setters) and takes the focus
void NCLoginServerWnd::ReceiveServerList(INT LastServerID)
{
    guard(NCLoginServerWnd::ReceiveServerList);
    ShowWindow();
    ServerSelectWnd->SetLastServerID(LastServerID);   // 0x100efdd0, the setter the linker shared with NCRecipeBuyListWnd's
    ServerSelectWnd->SetFocus();
    unguard;
}
NW_HOOK_METHOD(0x1009de80, NCLoginServerWnd, ReceiveServerList)

NW_CHECK_VTABLE(NCLoginServerWnd, 0x101b8588, 74)
