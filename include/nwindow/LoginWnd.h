#pragma once
// LoginWnd - the messenger's log-in window (UClass "CLoginWnd", 0xE8; C3 only, no C2 source; not the game's NCLoginWnd):
// the account and the password boxes, the "log in" and "clear" buttons and the "version" one, over the messenger picture
// and the logo. The messenger window makes it (0x10139A40) and shows it until the sign-in is under way.
//
// Layout and virtuals from the stock constructor 0x10137620, OnCreate 0x101348C0, OnDestroy 0x101316D0, OnPaint 0x101317C0
// and WindowProc 0x10136530.

#include "nwindow/NCWnd.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class MsnWnd;
class NCEditBox;
class NCButton;

class LoginWnd : public NCWnd
{
    NW_DECLARE_CLASS(LoginWnd, NCWnd, 0, 0x10324BA0)

    NCEditBox* IdEdit;               // +0xE8 the account (100 characters at most), 151 x 17 at (92, 131) (name ours)
    NCEditBox* PasswordEdit;         // +0xEC id 0xA56, the password (16 characters at most, type 2), 151 x 17 at (92, 151) (name ours)
    NCButton* LoginButton;           // +0xF0 id 0xA09, 66 x 21 at (72, 176), system string 140 (name ours)
    NCButton* ClearButton;           // +0xF4 id 0xA0A, 66 x 21 at (142, 176), system string 141 (name ours)
    NCButton* VersionButton;         // +0xF8 id 0xA0B, 134 x 23 at (72, 204), system string 995 (name ours)
    MsnWnd* Owner;                   // +0xFC the messenger window that made it (the constructor's argument) (name ours)
    UTexture* BackTex;               // +0x100 "L2UI_CH3.Msn.messenger_back1"
    UTexture* LogoTex;               // +0x104 "L2UI_CH3.Msn.msnlogo"

    LoginWnd(MsnWnd* InOwner = NULL);                                       // 0x1015bf90 (ret 4); defaults = what the stock InternalConstructor pushes
    // ~LoginWnd                                                     // 0x1015c010

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x1015af20
    virtual INT OnCreate();                                          // 37 0x10159480
    virtual INT OnDestroy();                                         // 38 0x10156320
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x10156410

    // names ours (anonymous in the db)
    INT OnLoginButton();                                             // 0x101565e0 asks the owner to sign in with both texts
    INT OnClearButton();                                             // 0x10155690 empties the two boxes
    INT OnVersionButton();                                           // 0x101566c0 hides this window, the owner shows the version box
    INT OnEditNotify(INT Code);                                      // 0x10159900 (ret 4) code 1 (Enter in the password box) logs in
};
NW_ASSERT_OFFSET(LoginWnd, IdEdit, 0xE8);
NW_ASSERT_OFFSET(LoginWnd, PasswordEdit, 0xEC);
NW_ASSERT_OFFSET(LoginWnd, LoginButton, 0xF0);
NW_ASSERT_OFFSET(LoginWnd, ClearButton, 0xF4);
NW_ASSERT_OFFSET(LoginWnd, VersionButton, 0xF8);
NW_ASSERT_OFFSET(LoginWnd, Owner, 0xFC);
NW_ASSERT_OFFSET(LoginWnd, BackTex, 0x100);
NW_ASSERT_OFFSET(LoginWnd, LogoTex, 0x104);
NW_ASSERT_SIZE(LoginWnd, 0x108);

#pragma pack(pop)
