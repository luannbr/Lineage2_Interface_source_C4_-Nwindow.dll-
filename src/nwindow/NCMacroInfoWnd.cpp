/*=============================================================================
    NCMacroInfoWnd.cpp - the macro's additional information window (the description).
    Stock (C3): 0x1009B6F0 .. 0x1009B8AB (WindowProc, SetDescription, ShowWindow, OnOK), 0x1009E6B0 .. 0x1009E8B0
    (OnDestroy, OnPaint), 0x1009F620 (OnCreate), 0x100A0B40 .. 0x100A0BDF (constructor, destructor).
    HideWindow (0x100412D0) is NCChatWnd::HideWindow's body; OnCancel (0x1007BEE0) is the one NCOptionWnd::OnCancel hooks.
=============================================================================*/
#include "nwindow/NCMacroInfoWnd.h"
#include "nwindow/NCMacroListWnd.h"
#include "nwindow/NCButton.h"
#include "nwindow/NCConsole.h"
#include "nwindow/NCEditBox.h"
#include "nwindow/NCFrameCtrl.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NCMultiEdit.h"
#include "nwindow/NConsoleWnd.h"

enum
{
    MSG_MacroDescTooLong = 0x345,      // systemmsg (name ours): the description is longer than 32 characters
    COLOR_SystemMsg = 0xFFB09B79,
    MACRO_DescLength = 32,
};

// sysstring labels (SysString index = (stock offset - 0x65A8C) / 12; names ours)
enum
{
    STR_MacroDescription = 57,        // 0x65D38: the box's heading, at (11, 32)
    STR_OK = 140,
    STR_Cancel = 141,
};

// the desktop's macro list window (NCConsole.h keeps it as an NCWnd* at NewWndF8; name ours)
static NCMacroListWnd* DesktopList()
{
    return (NCMacroListWnd*)GConsole->GetDesktop()->NewWndF8;
}

// a child window goes (repeated inline in the stock OnDestroys)
template <class T> static FORCEINLINE void DestroyChild(NCWnd* Parent, T*& Child)
{
    if (Child)
    {
        Parent->RemoveChildWindow(Child->Entry);
        delete Child;
        Child = NULL;
    }
}

// 0x100ad270 - nothing but the window's
NCMacroInfoWnd::NCMacroInfoWnd()
{
}
NW_HOOK_CTOR(0x100ad270, NCMacroInfoWnd)

// 0x100ad2c0
NW_HOOK_DTOR(0x100ad2c0, NCMacroInfoWnd)

// 0x100a7dc0 (vslot 25) - the two buttons
INT NCMacroInfoWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCWnd::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND)
    {
        if (wParam == NID_MACROINFO_OK)
            return OnOK() != 0;
        if (wParam == NID_MACROINFO_CANCEL)
            return OnCancel() != 0;
    }
    return 0;
}
NW_HOOK_METHOD(0x100a7dc0, NCMacroInfoWnd, WindowProc)

// 0x100a7e40 (ret 4) - the description box emptied, then Text put in it
void NCMacroInfoWnd::SetDescription(const TCHAR* Text)
{
    DescEdit->Clear();
    if (Text != NULL && DescEdit != NULL)
        DescEdit->SetText(Text);
}
NW_HOOK_METHOD(0x100a7e40, NCMacroInfoWnd, SetDescription)

// 0x100a7e70 (vslot 48) - the description kept shows in the box (only when the macro window is there)
void NCMacroInfoWnd::ShowWindow()
{
    if (GConsole != NULL && GConsole->Desktop != NULL && DesktopList() != NULL)
    {
        NCWnd::ShowWindow();
        SetDescription(Description);
    }
}
NW_HOOK_METHOD(0x100a7e70, NCMacroInfoWnd, ShowWindow)

// 0x100a7eb0 (vslot 74) - OK: with a macro being edited (slot below 24), a description over 32 characters is refused (message
// 0x345); else the text is kept (no text clears it) and the window hides
INT NCMacroInfoWnd::OnOK()
{
    if (GConsole != NULL && GConsole->Desktop != NULL && DesktopList() != NULL && DesktopList()->Selected < NMACRO_MaxMacros)
    {
        if (DescEdit->GetText() != NULL && appStrlen(DescEdit->GetText()) > MACRO_DescLength)
        {
            GConsole->ShowSystemMessage(MSG_MacroDescTooLong, COLOR_SystemMsg);
            return 0;
        }
        if (DescEdit->GetText() != NULL)
            appStrcpy(Description, DescEdit->GetText());
        else
            appMemset(Description, 0, 0x42);
        HideWindow();
        return 1;
    }
    return 0;
}
NW_HOOK_METHOD(0x100a7eb0, NCMacroInfoWnd, OnOK)

// vslot 69 - HideWindow (the stock body, 0x1007bee0, is NCOptionWnd::OnCancel's, which hooks it)
INT NCMacroInfoWnd::OnCancel()
{
    HideWindow();
    return 1;
}

// 0x100abc90 (vslot 37) - the description box (id 0x1514, 238 x 77), the description cleared, the title bar (id 3) and the
// OK / Cancel buttons
INT NCMacroInfoWnd::OnCreate()
{
    DescEdit = new NCMultiEdit();
    DescEdit->ID = NID_MACROINFO_DESC;
    DescEdit->SetPosition(9, 48, 238, 77);
    CreateChild(DescEdit, NWS_VISIBLE);
    appMemset(Description, 0, 0x42);
    BackTex = FindTexture(TEXT("l2ui_ch3.macrownd.macro3_back"));

    FrameCtrl = new NCFrameCtrl(1, 0, 0);
    FrameCtrl->ID = 3;
    FrameCtrl->SetPosition(0, 0, 256, 20);
    CreateChild(FrameCtrl, NWS_VISIBLE);

    OKButton = new NCButton(TEXT("L2UI_ch3.button.btn1_normal"), TEXT("L2UI_ch3.button.btn1_normalon"), 0);
    OKButton->SetPosition(51, 132, 76, 23);
    OKButton->ID = NID_MACROINFO_OK;
    CreateChild(OKButton, NWS_VISIBLE);
    OKButton->SetTextID(0x8C);

    CancelButton = new NCButton(TEXT("L2UI_ch3.button.btn1_normal"), TEXT("L2UI_ch3.button.btn1_normalon"), 0);
    CancelButton->SetPosition(131, 132, 76, 23);
    CancelButton->ID = NID_MACROINFO_CANCEL;
    CreateChild(CancelButton, NWS_VISIBLE);
    CancelButton->SetTextID(0x8D);
    return 1;
}
NW_HOOK_METHOD(0x100abc90, NCMacroInfoWnd, OnCreate)

// 0x100aad20 (vslot 38)
INT NCMacroInfoWnd::OnDestroy()
{
    guard(NCMacroInfoWnd::OnDestroy);
    DestroyChild(this, DescEdit);
    DestroyChild(this, FrameCtrl);
    DestroyChild(this, OKButton);
    DestroyChild(this, CancelButton);
    NCWnd::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100aad20, NCMacroInfoWnd, OnDestroy)

// 0x100aae40 (vslot 56) - the background and the heading of the description box
INT NCMacroInfoWnd::OnPaint(NCGDevice* Device)
{
    guard(NCMacroInfoWnd::OnPaint);
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    if (BackTex)
        Device->DrawStretchedTex(0, 20, 256, 141, 0, 0, 256, 141, BackTex);
    Device->DrawText(11, 32, 0xFFDCDCDC, *SysString(STR_MacroDescription), 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
    Device->ReleaseClip();
    Device->EndPaint();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100aae40, NCMacroInfoWnd, OnPaint)

NW_CHECK_VTABLE(NCMacroInfoWnd, 0x101baa60, 76)
