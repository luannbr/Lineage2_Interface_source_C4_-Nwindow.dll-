/*=============================================================================
    NCWarningWnd.cpp - the login screen's warning box. Stock: 0x10079580 .. 0x100796df and
    0x10079d70 .. 0x1007a12f (NCNoticeInfoWnd and NCNoticeWnd sit in between, NCNoticeWnd.cpp).
=============================================================================*/
#include "nwindow/NCWarningWnd.h"
#include "nwindow/NCButton.h"
#include "nwindow/NCFrameCtrl.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NCNoticeWnd.h"
#include "nwindow/NConsoleWnd.h"

// ids (names ours)
enum
{
    NID_WARNING_OK = 0xF8,
    NID_WARNING_FRAME = 0xA000F9,
};
enum { TIMER_WARNING = 0xF030D1 };  // nothing handles it (OnTimer returns 1)
enum { SOUND_Click = 0 };

// 0x100be6d0
NCWarningWnd::NCWarningWnd()
{
    BackTex = NULL;
    Unknown98 = 0;
}
NW_HOOK_CTOR(0x100be6d0, NCWarningWnd)

// 0x100be720
NW_HOOK_DTOR(0x100be720, NCWarningWnd)

// 0x100bd8c0 (vslot 25) - C2: no enabled test any more
INT NCWarningWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCWnd::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND && wParam == NID_WARNING_OK)
        return OnOK() != 0;
    return 0;
}
NW_HOOK_METHOD(0x100bd8c0, NCWarningWnd, WindowProc)

// 0x100bd920 (the fmap row says 0x1009a820: that one is NCNoticeWnd's twin with the text window at +0xa0)
void NCWarningWnd::SetText(const TCHAR* Text)
{
    if (Text && *Text)
        InfoWnd->SetText(Text);
}
NW_HOOK_METHOD(0x100bd920, NCWarningWnd, SetText)

// 0x100bd940
void NCWarningWnd::ClearText()
{
    InfoWnd->ClearText();
}
NW_HOOK_METHOD(0x100bd940, NCWarningWnd, ClearText)

// 0x100bd950 (vslot 74)
INT NCWarningWnd::OnOK()
{
    ::nw::VirtualCall<void>(GConsole, 424);   // NConsoleWnd::RequestServerList (slot 390, vcall +0x618)
    return 1;
}
NW_HOOK_METHOD(0x100bd950, NCWarningWnd, OnOK)

// 0x100bd970 (vslot 65) - Enter works as OK, then the key goes on to NCWnd
INT NCWarningWnd::OnKeyDown(WPARAM wParam, LPARAM lParam)
{
    if (wParam == VK_RETURN)
    {
        ::nw::VirtualCall<void>(GConsole, 424);   // NConsoleWnd::RequestServerList
        PlayInterfaceSound(SOUND_Click);
    }
    NCWnd::OnKeyDown(wParam, lParam);
    return 1;
}
NW_HOOK_METHOD(0x100bd970, NCWarningWnd, OnKeyDown)

// 0x100bd9b0 (vslot 48) - a one-minute timer nothing listens to
void NCWarningWnd::ShowWindow()
{
    NCWnd::ShowWindow();
    TimerResult = SetTimer(TIMER_WARNING, 60000, NULL);
    Unknown98 = 0;
}
NW_HOOK_METHOD(0x100bd9b0, NCWarningWnd, ShowWindow)

// 0x100bd9e0 (vslot 49)
void NCWarningWnd::HideWindow()
{
    NCWnd::HideWindow();
    KillTimer(TIMER_WARNING);
    Unknown98 = 0;
}
NW_HOOK_METHOD(0x100bd9e0, NCWarningWnd, HideWindow)

// 0x100be770 (vslot 37) - C4: the lobby's back texture (looked up with the second argument), no title bar; the text window at
// (6, 6) of 536 x 375 and the OK button of the new skin centered under it (half the width less 38) at 396, 76 x 23
INT NCWarningWnd::OnCreate()
{
    NCWnd::OnCreate();
    BackTex = FindTexture(TEXT("L2UI_ch3.LobbyWnd.lobby3_back"), 1);

    InfoWnd = new NCNoticeInfoWnd();
    InfoWnd->SetPosition(6, 6, 536, 375);
    CreateChild(InfoWnd, NWS_VISIBLE);

    OKButton = new NCButton(TEXT("L2UI_ch3.button.btn1_normal"), TEXT("L2UI_ch3.button.btn1_normalon"), 0);
    OKButton->ID = NID_WARNING_OK;
    OKButton->SetPosition((INT)(Width * 0.5f - 38.0f), 396, 76, 23);
    CreateChild(OKButton, NWS_VISIBLE);
    OKButton->SetTextID(140);  // "OK"
    return 1;
}
NW_HOOK_METHOD(0x100be770, NCWarningWnd, OnCreate)

// 0x100bdda0 (vslot 38) - guarded in C2; the title bar goes with NCWnd::OnDestroy
INT NCWarningWnd::OnDestroy()
{
    guard(NCWarningWnd::OnDestroy);
    if (InfoWnd)
    {
        RemoveChildWindow(InfoWnd->Entry);
        delete InfoWnd;
        InfoWnd = NULL;
    }
    if (OKButton)
    {
        RemoveChildWindow(OKButton->Entry);
        delete OKButton;
        OKButton = NULL;
    }
    NCWnd::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100bdda0, NCWarningWnd, OnDestroy)

// 0x100bde70 (vslot 62) - C3: the back texture over the whole window (no title bar); the guard carries NCNoticeWnd's
// name in the stock code
INT NCWarningWnd::OnPaint(NCGDevice* Device)
{
    guard(NCNoticeWnd::OnPaint);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    Device->BeginPaint(ScreenX, ScreenY);
    if (BackTex)
        Device->DrawTexture(0, 0, (INT)Width, (INT)Height, 0, 0, (INT)Width, (INT)Height, BackTex);
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100bde70, NCWarningWnd, OnPaint)

NW_CHECK_VTABLE(NCWarningWnd, 0x101be660, 75)
