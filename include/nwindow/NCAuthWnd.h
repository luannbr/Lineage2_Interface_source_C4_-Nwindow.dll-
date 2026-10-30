#pragma once
// NCAuthWnd - the login box of the login screen (UClass "CAuthWnd", 0xB4): account and password
// lines, "Log In" and "Exit" (NCLoginWnd creates it). Log In, or Enter in the password line,
// checks that both lines are filled and asks NConsoleWnd::RequestAuthLogin - once, until the
// box is shown again or its 3 s timer fires. Exit quits the game.

#include "nwindow/NCWnd.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCEditBox;
class NCButton;

class NCAuthWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCAuthWnd, NCWnd, 0, 0x102DF1E0)

    INT Mode;                        // +0xE8 (new in C2, name ours) 0 account and password, 1 the card key
                                     //   (the security card: only the password line is used, with the
                                     //   prompt of systemmsg 1237 for CardPos)
    INT CardPos;                     // +0xEC (new in C2, name ours) the argument of systemmsg 1237 (Mode 1)
    INT CardID;                      // +0xF0 (new in C2, name ours) first argument of RequestAuthCardKeyLogin
    INT Unknown94;                   // +0xF4 (0, never used)   // C2: offset derived, no evidence
    UTexture* BackTex;               // +0xF8 "L2UI.LobbyWnd.loginBack"   // C2: offset derived, no evidence
    NCEditBox* AccountEdit;          // +0xFC
    NCEditBox* PasswordEdit;         // +0x100
    NCButton* OKButton;              // +0x104 "Log In"
    NCButton* CancelButton;          // +0x108 "Exit"
    INT TimerResult;                 // +0x10C what SetTimer returned (never read)   // C2: offset derived, no evidence
    UBOOL bRequested;                // +0x110 a login request went out

    NCAuthWnd();                                                     // 0x1009f630
    // ~NCAuthWnd                                                    // 0x100830b0 (C1 only: no C2 match) (C1 only: no C2 match)

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x1009e320
    virtual INT OnTimer(INT TimerID);                                // 36 0x1009e5f0
    virtual INT OnCreate();                                          // 37 0x1009f230
    virtual INT OnDestroy();                                         // 38 0x1009ecc0
    virtual void ShowWindow();                                       // 48 0x1009e5b0
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x1009ea90
    // names ours
    virtual INT OnOK();                                              // 74 0x1009e3d0
    virtual INT OnCancel();                                          // 75 0x1009e590 Exit
    virtual INT OnPasswordNotify(INT Code);                          // 76 0x100bf260 Enter in the password line (the body is shared)

    void ClearPassword();                                            // 0x1009e3c0 (name ours)
    void ResetInput();                                               // (C2 only: 0x10057cd0 there; no such function here, nothing calls it) back to the account and password lines
    void SetCardKey(INT InCardID, INT InCardPos);                    // 0x1006e9c0 (name ours) the card key prompt (Mode 1)
};
NW_ASSERT_OFFSET(NCAuthWnd, Mode, 0xE8);
NW_ASSERT_OFFSET(NCAuthWnd, CardID, 0xF0);
NW_ASSERT_OFFSET(NCAuthWnd, BackTex, 0xF8);
NW_ASSERT_OFFSET(NCAuthWnd, AccountEdit, 0xFC);
NW_ASSERT_OFFSET(NCAuthWnd, OKButton, 0x104);
NW_ASSERT_OFFSET(NCAuthWnd, bRequested, 0x110);
NW_ASSERT_SIZE(NCAuthWnd, 0x114);

#pragma pack(pop)