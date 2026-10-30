/*=============================================================================
    MessageDialog.cpp - the messenger's plain message box (C3 only). Stock: 0x101219A0 .. 0x10121B1B (WindowProc,
    OnDestroy, OnPaint), OnCreate 0x10123480, the constructor 0x10124000 and the destructor 0x101240A0.
=============================================================================*/
#include "nwindow/MessageDialog.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/MsnDlgC4.h"
#include "nwindow/MsnWnd.h"

// 0x10148b50 - the base constructor, the text empty (the member's), the button and the texture none
MessageDialog::MessageDialog()
    : OKButton(NULL), BackTex(NULL)
{
}
NW_HOOK_CTOR(0x10148b50, MessageDialog)

// the messenger window makes the box through this (MsnWnd.cpp cannot include the dialogs' headers: see MsnWnd.h)
NCWnd* MsnNewMessageDialog() { return new MessageDialog; }

// 0x10148bf0 - the text goes with the members, then NCDialogBox
NW_HOOK_DTOR(0x10148bf0, MessageDialog)

// 0x10146530 (vslot 25) - OK closes the box
INT MessageDialog::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCDialogBox::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND && (INT)wParam == NID_MESSAGE_OK)
        return OnOK() != 0;
    return 0;
}
NW_HOOK_METHOD(0x10146530, MessageDialog, WindowProc)

// 0x1007bee0 (name ours) - the body the linker folded with NCOptionWnd::OnCancel (HideWindow, return 1): called there
#if NW_HOOK_MODE
NW_NAKED INT MessageDialog::OnOK() NW_STOCK(0x10084EE0)
#else
INT MessageDialog::OnOK()   // the standalone: the folded body itself (NCOptionWnd::OnCancel 0x10084EE0)
{
    HideWindow();
    return 1;
}
#endif

// 0x10147fd0 (vslot 37) - the back texture and OK (90, 75). C4: FindTexture's second argument, the button's text is SetTextID
INT MessageDialog::OnCreate()
{
    NCDialogBox::OnCreate();
    BackTex = FindTexture(TEXT("L2UI_CH3.dialog.system_back"), 1);
    OKButton = MsnDlgButtonC4(this, NID_MESSAGE_OK, 90, 75, 140);
    return 1;
}
NW_HOOK_METHOD(0x10147fd0, MessageDialog, OnCreate)

// 0x10146590 (vslot 38) - the button goes, then NCDialogBox
INT MessageDialog::OnDestroy()
{
    DestroyDlgChild(this, OKButton);
    NCDialogBox::OnDestroy();
    return 1;
}
NW_HOOK_METHOD(0x10146590, MessageDialog, OnDestroy)

// 0x101465d0 (vslot 62) - the back texture over the box and, once there is a character, the text at (11, 16), wrapped to
// the width less 22
INT MessageDialog::OnPaint(NCGDevice* Device)
{
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    if (BackTex)
        Device->DrawTexture(0, 0, (INT)Width, (INT)Height, 0, 0, (INT)Width, (INT)Height, BackTex, 0xFF, 1);
    if (Message.Num() != 0 && Message.Num() - 1 > 0)
        Device->DrawText(11, 16, 0xFFDCDCDC, *Message, (INT)(Width - 22.0f), 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
    Device->ReleaseClip();
    Device->EndPaint();
    return 1;
}
NW_HOOK_METHOD(0x101465d0, MessageDialog, OnPaint)

NW_CHECK_VTABLE(MessageDialog, 0x101dabe0, 76)
