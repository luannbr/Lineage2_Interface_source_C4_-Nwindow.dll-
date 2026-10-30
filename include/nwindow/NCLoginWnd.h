#pragma once
// NCLoginWnd - the login screen (UClass "CLoginWnd", 0x110 in C4 (C3: 0xCC); the god window holds it: NCGodWnd +0xEC, stored by
// NCGodWnd::OnCreate 0x100910A0 right after `new` and the constructor 0x1009F700): the game logo, the login box (NCAuthWnd), the
// warning, EULA and announce boxes, the server list (NCServerSelectWnd), the key crypt warning box, the replay list (new in C4) and
// the buttons in the lower right corner (NCLoginFunctionWnd): "New Account" and "Lost Account" (created disabled), "Options",
// "Credits" and (C4) "Replay".

#include "nwindow/NCWnd.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCAuthWnd;
class NCWarningWnd;
class NCEulaWnd;
class NCAnnounceWnd;
class NCKeyCryptWarningDialogBox;

class NCLoginWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCLoginWnd, NCWnd, 0, 0x102DE778)

    // C4 layout from OnCreate (0x1009F7A0: every store of [this + 0xE8 .. 0x10C]) and OnDestroy (0x1009EDF0): the notice box is
    // gone (C2), the server list sits in a window of its own (NCLoginServerWnd, new in C3) and the buttons in another
    // (NCLoginFunctionWnd, new in C3); C4 adds the replay list. The constructor 0x1009F700 clears only the logo (+0x108)
    NCAuthWnd* AuthWnd;              // +0xE8 "Log In" (title SysString 149)
    NCWarningWnd* WarningWnd;        // +0xEC "Warning" (title SysString 538)
    NCEulaWnd* EulaWnd;              // +0xF0 (title SysString 598)
    NCAnnounceWnd* AnnounceWnd;      // +0xF4
    NCWnd* ServerWnd;                // +0xF8 NCLoginServerWnd (0xF0 bytes in C4; its +0xE8 is the NCServerSelectWnd*)
    NCWnd* FunctionWnd;              // +0xFC NCLoginFunctionWnd (0x100 bytes in C4: the buttons)
    NCKeyCryptWarningDialogBox* KeyCryptWnd; // +0x100 NCKeyCryptWarningDialogBox
    NCWnd* ReplayListWnd;            // +0x104 NCReplayListWnd (C4; 316 x 350 under the server list, hidden by ShowWindow)
    UTexture* LogoTex;               // +0x108 localization.ini "<country>_LogoTexture"
    UTexture* MiniLogoTex;           // +0x10C localization.ini "<country>_MiniLogoTexture"

    NCLoginWnd();                                                    // 0x1009f700
    // ~NCLoginWnd                                                   // 0x1009F750

    // WindowProc (slot 25, 0x10034390) is NCWnd's own: the stock vtable of this class points at it
    virtual INT OnCreate();                                          // 37 0x1009f7a0
    virtual INT OnDestroy();                                         // 38 0x1009edf0
    virtual void ShowWindow();                                       // 48 0x1009e610
    virtual void HideWindow();                                       // 49 0x1009e6d0
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x1009efd0

    void ResetWindows();                                             // 0x1009e800 (name ours)
    virtual void NewVirtual74();  // 74 0x1009e700 (only this client has it; defined in NNewVirtuals.gen.cpp; hooked as ShowReplayMode in NCLoginWnd.cpp)
    virtual void NewVirtual75();  // 75 0x1009e780 (only this client has it; defined in NNewVirtuals.gen.cpp; hooked as ShowLoginMode in NCLoginWnd.cpp)
};
NW_ASSERT_OFFSET(NCLoginWnd, AuthWnd, 0xE8);
NW_ASSERT_OFFSET(NCLoginWnd, ServerWnd, 0xF8);
NW_ASSERT_OFFSET(NCLoginWnd, KeyCryptWnd, 0x100);
NW_ASSERT_OFFSET(NCLoginWnd, ReplayListWnd, 0x104);
NW_ASSERT_OFFSET(NCLoginWnd, LogoTex, 0x108);
NW_ASSERT_SIZE(NCLoginWnd, 0x110);


// NWM_COMMAND ids of the function window's buttons (names ours)
enum
{
    NID_LOGIN_OPTIONS = (INT)0xAFF12011,
    NID_LOGIN_NEWACCOUNT = (INT)0xAFF12012,
    NID_LOGIN_CREDITS = (INT)0xAFF12014,
    NID_LOGIN_LOSTACCOUNT = (INT)0xAFF12015,
    NID_LOGIN_REPLAY = (INT)0xAFF12016,       // new in C4 (name ours)
};

class NCButton;
class NCServerSelectWnd;
class NCServerInfoWnd;

// NCLoginFunctionWnd - the five buttons in the lower right corner of the login screen (UClass "CLoginFunctionWnd",
// 0x100 in C4 (C3: 0xDC); NCLoginWnd +0xFC in C4 (C3: +0xDC), new in C3: C2 had the buttons in NCLoginWnd itself). "New Account"
// and "Lost Account" show the systemmsg lines 1186 and 1187 (the addresses to visit) in the message window, "Options" toggles
// the options window, "Credits" hides the login screen and starts the credits and "Replay" (C4) shows the replay list. The same
// shape as NCLobbyFunctionWnd (it is the window the lobby's guard names were copied from).
class NCLoginFunctionWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCLoginFunctionWnd, NCWnd, 0, 0x102DDD18)

    UTexture* BackTex;               // +0xE8 "L2UI_CH3.LobbyWnd.lobby_menuback" (120 x 138)
    NCButton* NewAccountButton;      // +0xEC id 0xAFF12012, sysstring 154 (names ours)
    NCButton* LostAccountButton;     // +0xF0 id 0xAFF12015, sysstring 155
    NCButton* OptionsButton;         // +0xF4 id 0xAFF12011, sysstring 156
    NCButton* CreditsButton;         // +0xF8 id 0xAFF12014, sysstring 805
    NCButton* ReplayButton;          // +0xFC id 0xAFF12016, sysstring 1251 (new in C4, name ours): the replay list

    NCLoginFunctionWnd();                                            // 0x1009ddd0 (NCWnd's constructor and the vtable)
    // ~NCLoginFunctionWnd                                           // 0x1009de20

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x1009d360
    virtual INT OnCreate();                                          // 37 0x1009d9a0
    virtual INT OnDestroy();                                         // 38 0x1009d850
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x1009d760
    // the four guards carry the same name in the stock code (OnNewAccountBtnClick); names of the slots ours
    virtual INT OnNewAccountBtnClick();                              // 74 0x1009d440 systemmsg 1186 in the message window
    virtual INT OnOptionsBtnClick();                                 // 75 0x1009d560 the options window
    virtual INT OnCreditsBtnClick();                                 // 76 0x1009d5e0 the credits
    virtual INT OnLostAccountBtnClick();                             // 77 0x1009d4d0 systemmsg 1187 in the message window
    virtual void NewVirtual78();  // 78 0x1009d6d0 OnReplayBtnClick (only this client has it; defined in NNewVirtuals.gen.cpp; hooked as a free function in NCLoginWnd.cpp)
};
NW_ASSERT_OFFSET(NCLoginFunctionWnd, BackTex, 0xE8);
NW_ASSERT_OFFSET(NCLoginFunctionWnd, CreditsButton, 0xF8);
NW_ASSERT_OFFSET(NCLoginFunctionWnd, ReplayButton, 0xFC);
NW_ASSERT_SIZE(NCLoginFunctionWnd, 0x100);

// NCLoginServerWnd - the server list and the help page under it, one window (UClass "CLoginServerWnd", 0xF0 in C4 (C3: 0xD0);
// NCLoginWnd +0xF8 in C4 (C3: +0xD8), new in C3): the list (NCServerSelectWnd, titled with sysstring 449) over the page of
// ..\l2text\server_help.htm (NCServerInfoWnd, sysstring 857). The console's ReceiveServerList lands here.
class NCLoginServerWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCLoginServerWnd, NCWnd, 0, 0x102DE248)

    NCServerSelectWnd* ServerSelectWnd;   // +0xE8 316 x 330 at the top left corner
    NCServerInfoWnd* ServerInfoWnd;       // +0xEC 316 x 205 at (0, 337), under it (names ours)

    NCLoginServerWnd();                                              // 0x1009e080 (NCWnd's constructor and the vtable)
    // ~NCLoginServerWnd                                             // 0x1009e0d0

    virtual INT OnCreate();                                          // 37 0x1009e120
    virtual INT OnDestroy();                                         // 38 0x1009df10

    // (name ours; ret 4) the server the account played on last goes to the list, which is shown (the window too)
    void ReceiveServerList(INT LastServerID);                        // 0x1009de80
};
NW_ASSERT_OFFSET(NCLoginServerWnd, ServerSelectWnd, 0xE8);
NW_ASSERT_OFFSET(NCLoginServerWnd, ServerInfoWnd, 0xEC);
NW_ASSERT_SIZE(NCLoginServerWnd, 0xF0);
#pragma pack(pop)
