/*=============================================================================
    LoginWnd.cpp - the messenger's log-in window (C3 only; C3 addresses). Stock: 0x10130A20 (clear), 0x101316D0 (OnDestroy),
    0x101317C0 (OnPaint), 0x10131990 (log in), 0x10131A70 (version), 0x101348C0 (OnCreate), 0x10134D40 (the boxes' notice),
    0x10136530 (WindowProc), 0x10137620 / 0x101376A0 (constructor, destructor).
=============================================================================*/
#include "nwindow/LoginWnd.h"
#include "nwindow/MsnWnd.h"
#include "nwindow/NCButton.h"
#include "nwindow/NCEditBox.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NGameData.h"

enum
{
    LOGIN_PasswordID = 0xA56,
    LOGIN_LoginID = 0xA09,
    LOGIN_ClearID = 0xA0A,
    LOGIN_VersionID = 0xA0B,
    SYSSTR_Login = 140,
    SYSSTR_Clear = 141,
    SYSSTR_Version = 995,
    SYSSTR_LoginTitle = 1003,
    SYSSTR_LoginHint1 = 1004,
    SYSSTR_LoginHint2 = 1005,
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

// 0x1015bf90 - the owner is the argument, everything else is empty
LoginWnd::LoginWnd(MsnWnd* InOwner)
{
    IdEdit = NULL;
    PasswordEdit = NULL;
    LoginButton = NULL;
    ClearButton = NULL;
    VersionButton = NULL;
    BackTex = NULL;
    LogoTex = NULL;
    Owner = InOwner;
}
NW_HOOK_CTOR_ARGS(0x1015bf90, LoginWnd, (MsnWnd* A), (A))

// 0x1015c010
NW_HOOK_DTOR(0x1015c010, LoginWnd)

// 0x1015af20 (vslot 25) - the boxes and the three buttons report to it
INT LoginWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCWnd::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND)
    {
        if (wParam == LOGIN_PasswordID)
            return OnEditNotify((INT)lParam) != 0;
        if (wParam == LOGIN_LoginID)
            return OnLoginButton() != 0;
        if (wParam == LOGIN_ClearID)
            return OnClearButton() != 0;
        if (wParam == LOGIN_VersionID)
            return OnVersionButton() != 0;
    }
    return 0;
}
NW_HOOK_METHOD(0x1015af20, LoginWnd, WindowProc)

// 0x10159480 (vslot 37) - NCWnd's, the textures, the two boxes and the three buttons; the tab key walks this window, the
// account, the password, "log in", "clear" and back
INT LoginWnd::OnCreate()
{
    NCWnd::OnCreate();
    BackTex = FindTexture(TEXT("L2UI_CH3.Msn.messenger_back1"));
    LogoTex = FindTexture(TEXT("L2UI_CH3.Msn.msnlogo"));

    IdEdit = new NCEditBox(6, NULL, 1, 0x80, 1);                // C4: type 6 and the last argument 1 (C3: 0, 0)
    IdEdit->SetPosition(0x5C, 0x83, 0x97, 0x11);
    CreateChild(IdEdit, NWS_VISIBLE);
    IdEdit->SetMaxLength(100);

    PasswordEdit = new NCEditBox(2, NULL, 1, 0x80, 1);          // C4: the last argument 1 (C3: 0)
    PasswordEdit->ID = LOGIN_PasswordID;
    PasswordEdit->SetPosition(0x5C, 0x97, 0x97, 0x11);
    CreateChild(PasswordEdit, NWS_VISIBLE);
    PasswordEdit->SetMaxLength(16);

    LoginButton = new NCButton(TEXT("L2UI_CH3.Button.SmallButton2"), TEXT("L2UI_CH3.Button.SmallButton2_down"), 0);
    LoginButton->ID = LOGIN_LoginID;
    LoginButton->SetPosition(0x48, 0xB0, 0x42, 0x15);
    CreateChild(LoginButton, NWS_VISIBLE);
    LoginButton->SetTextID(SYSSTR_Login);

    ClearButton = new NCButton(TEXT("L2UI_CH3.Button.SmallButton2"), TEXT("L2UI_CH3.Button.SmallButton2_down"), 0);
    ClearButton->ID = LOGIN_ClearID;
    ClearButton->SetPosition(0x8E, 0xB0, 0x42, 0x15);
    CreateChild(ClearButton, NWS_VISIBLE);
    ClearButton->SetTextID(SYSSTR_Clear);

    VersionButton = new NCButton(TEXT("L2UI_CH3.Button.bigbutton3"), TEXT("L2UI_CH3.Button.bigbutton3_down"), 0);
    VersionButton->ID = LOGIN_VersionID;
    VersionButton->SetPosition(0x48, 0xCC, 0x86, 0x17);
    CreateChild(VersionButton, NWS_VISIBLE);
    VersionButton->SetTextID(SYSSTR_Version);

    SetTabOrder(IdEdit->Entry, ClearButton->Entry);
    IdEdit->SetTabOrder(PasswordEdit->Entry, Entry);
    PasswordEdit->SetTabOrder(LoginButton->Entry, IdEdit->Entry);
    LoginButton->SetTabOrder(ClearButton->Entry, PasswordEdit->Entry);
    ClearButton->SetTabOrder(Entry, LoginButton->Entry);
    return 1;
}
NW_HOOK_METHOD(0x10159480, LoginWnd, OnCreate)

// 0x10156320 (vslot 38) - the boxes and the buttons go (in the order they were made), then NCWnd's
INT LoginWnd::OnDestroy()
{
    DestroyChild(this, IdEdit);
    DestroyChild(this, PasswordEdit);
    DestroyChild(this, LoginButton);
    DestroyChild(this, ClearButton);
    DestroyChild(this, VersionButton);
    NCWnd::OnDestroy();
    return 1;
}
NW_HOOK_METHOD(0x10156320, LoginWnd, OnDestroy)

// 0x10156410 (vslot 56) - the picture (256 x 353), the logo at (95, 278), the title centered at y 109 and the two lines of
// the hint at (16, 134) and (16, 155)
INT LoginWnd::OnPaint(NCGDevice* Device)
{
    SIZE Size;
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    if (BackTex)
        Device->DrawTexture(0, 0, 0x100, 0x161, 0, 0, 0x100, 0x161, BackTex, 0xFF, 1);
    if (LogoTex)
        Device->DrawTexture(0x5F, 0x116, 0x8E, 0x17, 0, 0, 0x8E, 0x17, LogoTex, 0xFF, 1);
    NCGDevice::GetTextExtent(&Size, *MSN_SYSSTR(SYSSTR_LoginTitle), 0, 0, 0, 0, 0, (L2FontType)0);
    Device->DrawText((INT)(Width * 0.5f - (FLOAT)(Size.cx / 2)), 0x6D, 0xFFDCDCDC, *MSN_SYSSTR(SYSSTR_LoginTitle),
                     0, 0, 0, (L2FontType)0, 0, 0, 0, 0, 0, 0, 0);
    Device->DrawText(0x10, 0x86, 0xFFDCDCDC, *MSN_SYSSTR(SYSSTR_LoginHint1), 0, 0, 0, (L2FontType)0, 0, 0, 0, 0, 0, 0, 0);
    Device->DrawText(0x10, 0x9B, 0xFFDCDCDC, *MSN_SYSSTR(SYSSTR_LoginHint2), 0, 0, 0, (L2FontType)0, 0, 0, 0, 0, 0, 0, 0);
    Device->ReleaseClip();
    Device->EndPaint();
    return 1;
}
NW_HOOK_METHOD(0x10156410, LoginWnd, OnPaint)

// 0x101565e0 (name ours; anonymous in the db) - with both boxes filled and an owner, the owner is asked to sign in with
// the two texts; then both boxes are emptied (a failed sign-in leaves nothing of the password)
INT LoginWnd::OnLoginButton()
{
    if (Owner && IdEdit->CommitText() && PasswordEdit->CommitText())
        Owner->StartLogin(MsnStringArgOf(IdEdit->CommitText()), MsnStringArgOf(PasswordEdit->CommitText()));
    IdEdit->ClearText();
    PasswordEdit->ClearText();
    return 1;
}
NW_HOOK_METHOD(0x101565e0, LoginWnd, OnLoginButton)

// 0x10155690 (name ours; anonymous in the db) - both boxes emptied
INT LoginWnd::OnClearButton()
{
    IdEdit->ClearText();
    PasswordEdit->ClearText();
    return 1;
}
NW_HOOK_METHOD(0x10155690, LoginWnd, OnClearButton)

// 0x101566c0 (name ours; anonymous in the db) - this window goes and the owner shows the version box
INT LoginWnd::OnVersionButton()
{
    HideWindow();
    Owner->ShowVersionWnd();
    return 1;
}
NW_HOOK_METHOD(0x101566c0, LoginWnd, OnVersionButton)

// 0x10159900 (name ours; anonymous in the db; ret 4) - the password box's notice 1 (Enter) logs in
INT LoginWnd::OnEditNotify(INT Code)
{
    if (Code == 1)
        OnLoginButton();
    return 1;
}
NW_HOOK_METHOD(0x10159900, LoginWnd, OnEditNotify)

NW_CHECK_VTABLE(LoginWnd, 0x101dc9f8, 74)
