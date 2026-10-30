/*=============================================================================
    NCWarningDialogBox.cpp - the small warning box of the console (C2 only). Stock: 0x100090e0 ..
    0x100094cf, and SetText at 0x10006e00 (between NCDialogBox's functions).
=============================================================================*/
#include "nwindow/NCWarningDialogBox.h"
#include "nwindow/NCButton.h"
#include "nwindow/NCGDevice.h"

// 0x1000b390 - NCDialogBox's constructor, the text empty (an FString), OK ends nothing yet
NCWarningDialogBox::NCWarningDialogBox()
{
    bExit = 0;
}
NW_HOOK_CTOR(0x1000b390, NCWarningDialogBox)


// 0x100827a0 (vslot 25) - the stock body is one with NCKeyCryptWarningDialogBox's (hooked there)
INT NCWarningDialogBox::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCDialogBox::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND && wParam == NID_DIALOG_OK)
        return OnOK() != 0;
    return 0;
}

// 0x10008500 (ret 4) - guarded: the message is kept as it comes
void NCWarningDialogBox::SetWarningMessage(const TCHAR* InText)
{
    guard(NCWarningDialogBox::SetWarningMessage);
    Text = InText;
    unguard;
}
NW_HOOK_METHOD(0x10008500, NCWarningDialogBox, SetWarningMessage)

// 0x1000afd0 (vslot 37) - C4: the popup and the warning icon of L2UI_ch3 (textures looked up with the second argument), the OK
// button of the new skin placed from the box's rectangle (centered: half its width less 38) at 75 with a size of 76 x 23, its
// text SysString 140 (SetTextID in C4)
INT NCWarningDialogBox::OnCreate()
{
    NCDialogBox::OnCreate();
    BackTex = FindTexture(TEXT("L2UI_ch3.dialog.system_back"), 1);
    IconTex = FindTexture(TEXT("L2UI_ch3.dialog.warningicon"), 1);
    NRectF Local;
    Local.X = 0;
    Local.Y = 0;
    Local.W = 0;
    Local.H = 0;
    GetLocalRect(&Local);

    OKButton = new NCButton(TEXT("L2UI_ch3.button.btn1_normal"), TEXT("L2UI_ch3.button.btn1_normalon"), 0);
    OKButton->ID = NID_DIALOG_OK;
    OKButton->SetPosition((INT)(Local.W * 0.5f - 38.0f), 75, 76, 23);
    OKButton->SetTextID(140);  // "OK"
    CreateChild(OKButton, NWS_VISIBLE);
    return 1;
}
NW_HOOK_METHOD(0x1000afd0, NCWarningDialogBox, OnCreate)

// 0x10008fa0 (vslot 38) - guarded in C2
INT NCWarningDialogBox::OnDestroy()
{
    guard(NCWarningDialogBox::OnDestroy);
    if (OKButton)
    {
        RemoveChildWindow(OKButton->Entry);
        delete OKButton;
        OKButton = NULL;
    }
    NCDialogBox::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10008fa0, NCWarningDialogBox, OnDestroy)

// 0x10008e40 (vslot 62) - C3: the popup stretched over the box, the warning icon at (11, 15) and the text at (41, 16)
// wrapped to 200 px, in 0xFFDCDCDC
INT NCWarningDialogBox::OnPaint(NCGDevice* Device)
{
    guard(NCWarningDialogBox::OnPaint);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    Device->BeginPaint(ScreenX, ScreenY);
    if (BackTex)
        Device->DrawTexture(0, 0, (INT)Width, (INT)Height, 0, 0, (INT)Width, (INT)Height, BackTex);
    if (IconTex)
        Device->DrawTexture(11, 15, 24, 24, 0, 0, 24, 24, IconTex);
    if (Text.Num() != 0)
        Device->DrawText(41, 16, 0xFFDCDCDC, *Text, 200, 0, 1, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10008e40, NCWarningDialogBox, OnPaint)

// 0x10009050 (vslot 76) - OK: a fatal warning ends the client, any other closes the box
INT NCWarningDialogBox::OnOK()
{
    if (bExit)
    {
        appRequestExit(0);
        return 1;
    }
    HideModal();
    bExit = 0;
    return 1;
}
NW_HOOK_METHOD(0x10009050, NCWarningDialogBox, OnOK)

// 0x1000b420 - the text goes, then NCDialogBox
NW_HOOK_DTOR(0x1000b420, NCWarningDialogBox)

NW_CHECK_VTABLE(NCWarningDialogBox, 0x101a12e0, 77)
