/*=============================================================================
    NCCheckBox.cpp - check box. Stock: 0x10005230 .. 0x10005a62 (C3: 0x10004600 .. 0x10004b90).
=============================================================================*/
#include "nwindow/NCCheckBox.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NConsoleWnd.h"

#include <wchar.h>

// 0x10005720 - the label by text (an empty text leaves it empty, LabelID -1), a 12 x 12 box; the fourth argument is
// DrawText's fifth, which the label is drawn with. NControl's constructor is inlined in the stock one (the same stores);
// the disabled textures are left as allocated
NCCheckBox::NCCheckBox(const TCHAR* InLabel, UBOOL bInChecked, UBOOL bInLabelLeft, INT InTextParam)
{
    LabelID = -1;
    if (InLabel && *InLabel)
        Label = InLabel;
    BoxW = 12;
    BoxH = 12;
    CheckedTex = NULL;
    UncheckedTex = NULL;
    bDisabled = 0;
    bChecked = bInChecked;
    bLabelLeft = bInLabelLeft;
    TextParam = InTextParam;
}
NW_HOOK_CTOR_ARGS(0x10005720, NCCheckBox, (const TCHAR* A, UBOOL B, UBOOL C, INT D), (A, B, C, D))

// 0x100058c0 (ret 0x10) - the label by system string (the C3 "Check Box" fallback is gone: the string is drawn as it is)
NCCheckBox::NCCheckBox(INT InLabelID, UBOOL bInChecked, UBOOL bInLabelLeft, INT InTextParam)
{
    LabelID = InLabelID;
    BoxW = 12;
    BoxH = 12;
    CheckedTex = NULL;
    UncheckedTex = NULL;
    bDisabled = 0;
    bChecked = bInChecked;
    bLabelLeft = bInLabelLeft;
    TextParam = InTextParam;
}
NW_HOOK_CTOR_ARGS(0x100058c0, NCCheckBox, (INT A, UBOOL B, UBOOL C, INT D), (A, B, C, D))

// 0x10005840 - the label is an FString and the tool tip an NToolTipInfo (destroyed with the object)
NW_HOOK_DTOR(0x10005840, NCCheckBox)

// 0x10090b60 (vslot 25) - the stock body is NCButton's
INT NCCheckBox::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return NControl::WindowProc(Msg, wParam, lParam) != 0;
}

// 0x10005230 (vslot 37) - C4: NControl's is called (the Disable texture)
INT NCCheckBox::OnCreate()
{
    NControl::OnCreate();
    CheckedTex = FindTexture(TEXT("L2UI.Control.CheckBox_Checked"), 1);
    UncheckedTex = FindTexture(TEXT("L2UI.Control.CheckBox"), 1);
    UncheckedDisabledTex = FindTexture(TEXT("L2UI.Control.CheckBox_Unable"), 1);
    CheckedDisabledTex = FindTexture(TEXT("L2UI.Control.CheckBox_Checked_unable"), 1);
    return 1;
}
NW_HOOK_METHOD(0x10005230, NCCheckBox, OnCreate)

// 0x10005300 (vslot 38) - guarded in C2 and C3 (C1: NCButton's body)
INT NCCheckBox::OnDestroy()
{
    guard(NCCheckBox::OnDestroy);
    NControl::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10005300, NCCheckBox, OnDestroy)

// 0x100053b0 (vslot 62) - the box (the texture of its state: checked or not, disabled or not) and the label on its
// right, or its left (after the label's width and 4 pixels). The label is the SysString when LabelID >= 0; light grey
// (grey when disabled), drawn with TextParam
INT NCCheckBox::OnPaint(NCGDevice* Device)
{
    guard(NCCheckBox::OnPaint);
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    const TCHAR* Text;
    if (LabelID >= 0)
        Text = *SysString(LabelID);
    else
        Text = *Label;
    UTexture* Box;
    if (bDisabled)
        Box = bChecked ? CheckedDisabledTex : UncheckedDisabledTex;
    else
        Box = bChecked ? CheckedTex : UncheckedTex;
    DWORD Color = bDisabled ? 0xFF808080 : 0xFFDCDCDC;
    if (bLabelLeft)
    {
        SIZE Size;
        NCGDevice::GetTextExtent(&Size, Text, 0, 0, 0, 0, 0, L2FONT_Normal);
        Device->DrawTexture(Size.cx + 4, 0, BoxW, BoxH, 0, 0, BoxW, BoxH, Box, 0xFF, 1);
        Device->DrawText(0, 0, Color, Text, TextParam, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
    }
    else
    {
        Device->DrawTexture(0, 0, BoxW, BoxH, 0, 0, BoxW, BoxH, Box, 0xFF, 1);
        Device->DrawText(BoxW + 4, 0, Color, Text, TextParam, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
    }
    Device->ReleaseClip();
    Device->EndPaint();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100053b0, NCCheckBox, OnPaint)

// 0x10005990 (vslot 63) - the tool tip is the window skin's (the window's ToolTip by value, drawn at the window's
// corner), always: this window has no mouse-over test. The local rectangle of the desktop is asked for and not used
INT NCCheckBox::OnPostPaint(NCGDevice* Device)
{
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    NRectF Screen;
    Screen.X = 0;
    Screen.Y = 0;
    Screen.W = 0.0f;
    Screen.H = 0.0f;
    GConsole->Desktop->GetLocalRect(&Screen);
    if (ToolTipSkin)
    {
        NToolTipRect Wnd;
        Wnd.X = ScreenX;
        Wnd.Y = ScreenY;
        Wnd.W = Width;
        Wnd.H = Height;
        ToolTipSkin->Draw(Device, ToolTip, 0, 0, Wnd);
    }
    Device->ReleaseClip();
    Device->EndPaint();
    return 1;
}
NW_HOOK_METHOD(0x10005990, NCCheckBox, OnPostPaint)

// 0x10005380 (vslot 68)
INT NCCheckBox::OnLButtonDown(WPARAM wParam, LPARAM lParam)
{
    if (!bDisabled)
    {
        bChecked = !bChecked;
        NotifyParent(bChecked);
    }
    return 1;
}
NW_HOOK_METHOD(0x10005380, NCCheckBox, OnLButtonDown)

NW_CHECK_VTABLE(NCCheckBox, 0x1019fc90, 75)
