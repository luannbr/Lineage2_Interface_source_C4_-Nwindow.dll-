/*=============================================================================
    VersionWnd.cpp - the messenger's version box (C3 only; C3 addresses). Stock: 0x10131A90 (OnPaint), 0x10131BE0 (the
    OK button), 0x10134D60 (WindowProc), 0x10134DC0 (OnCreate), 0x101376F0 / 0x10137750 (constructor, destructor).
=============================================================================*/
#include "nwindow/VersionWnd.h"
#include "nwindow/MsnWnd.h"
#include "nwindow/NCButton.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NGameData.h"
#include "nwindow/NMsnDlgC4.h"

enum
{
    VERSION_OKButtonID = 0xA54,
    SYSSTR_VersionOK = 1008,
    SYSMSG_Version1 = 1310,     // the three lines of the box
    SYSMSG_Version2 = 1311,
    SYSMSG_Version3 = 1312,
};

// 0x1015c060 - the owner is the argument; the button and the picture start empty
VersionWnd::VersionWnd(MsnWnd* InOwner)
{
    Owner = InOwner;
    OKButton = NULL;
    BackTex = NULL;
}
NW_HOOK_CTOR_ARGS(0x1015c060, VersionWnd, (MsnWnd* A), (A))

// 0x1015c0c0
NW_HOOK_DTOR(0x1015c0c0, VersionWnd)

// 0x10159920 (vslot 25) - the button reports to the box: it hides itself and gives the log-in window back
INT VersionWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCWnd::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND && wParam == VERSION_OKButtonID)
        return OnOKButton() != 0;
    return 0;
}
NW_HOOK_METHOD(0x10159920, VersionWnd, WindowProc)

// 0x10159980 (vslot 37) - NCWnd's, the picture, and the OK button (96 x 23 at (79, 274))
INT VersionWnd::OnCreate()
{
    NCWnd::OnCreate();
    BackTex = FindTexture(TEXT("L2UI_CH3.Msn.messenger_back1"));
    OKButton = new NCButton(TEXT("L2UI_CH3.Button.bigbutton"), TEXT("L2UI_CH3.Button.bigbutton_down"), 0);
    OKButton->ID = VERSION_OKButtonID;
    OKButton->SetPosition(79, 274, 96, 23);
    CreateChild(OKButton, NWS_VISIBLE);
    OKButton->SetTextID(SYSSTR_VersionOK);   // C4: the text by its number (push 0x3F0, 0x100014A0)
    return 1;
}
NW_HOOK_METHOD(0x10159980, VersionWnd, OnCreate)

// 0x100eb490 (vslot 38) - NCSSQMainEventWnd's body, shared (hooked there): the control at +0xCC goes, then NCWnd's
#if NW_HOOK_MODE
NW_NAKED INT VersionWnd::OnDestroy() NW_STOCK(0x101566e0)
#else
INT VersionWnd::OnDestroy()   // the standalone: that body (0x101566e0 reads the control at +0xEC, here the OK button)
{
    DestroyDlgChild(this, OKButton);
    NCWnd::OnDestroy();
    return 1;
}
#endif

// 0x10156720 (vslot 56) - the picture (256 x 353), then the three lines of text in one block at (18, 44), wrapped at
// the window's width less 36
INT VersionWnd::OnPaint(NCGDevice* Device)
{
    TCHAR Text[2048];
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    if (BackTex)
        Device->DrawTexture(0, 0, 0x100, 0x161, 0, 0, 0x100, 0x161, BackTex, 0xFF, 1);
    Text[0] = 0;
    appSprintf(Text, TEXT("%s%s\r\n\r\n\r\n"), Text, MsnDlgText(SYSMSG_Version1));
    appSprintf(Text, TEXT("%s%s\r\n\r\n\r\n"), Text, MsnDlgText(SYSMSG_Version2));
    appSprintf(Text, TEXT("%s%s"), Text, MsnDlgText(SYSMSG_Version3));
    Device->DrawText(0x12, 0x2C, 0xFFDCDCDC, Text, (DWORD)(INT)(Width - 36.0f), 0, 0, (L2FontType)0, 0, 0, 0, 0, 0, 0, 0);
    Device->ReleaseClip();
    Device->EndPaint();
    return 1;
}
NW_HOOK_METHOD(0x10156720, VersionWnd, OnPaint)

// 0x10156870 (name ours; anonymous in the db) - the box goes and the owner shows the log-in window again
INT VersionWnd::OnOKButton()
{
    HideWindow();
    Owner->ShowLoginWnd();
    return 1;
}
NW_HOOK_METHOD(0x10156870, VersionWnd, OnOKButton)

NW_CHECK_VTABLE(VersionWnd, 0x101dcb20, 74)
