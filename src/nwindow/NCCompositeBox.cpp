/*=============================================================================
    NCCompositeBox.cpp - IME composition window. Stock (C2): 0x10003090 .. 0x10003fff.
=============================================================================*/
#include "nwindow/NCCompositeBox.h"
#include "nwindow/NCGDevice.h"

// 0x10005050 - the background texture, the first character, the caret and its flag cleared
NCCompositeBox::NCCompositeBox()
{
    BackTex = NULL;
    CaretPos = 0;
    bShowCaret = 0;
    FirstChar = 0;
}
NW_HOOK_CTOR(0x10005050, NCCompositeBox)

// C2 CANDIDATE: the C2 function is not the same code as the C1 one; kept only while it matches the stock one (verify_data)
// 0x10005160 (ret 0x14) - the Chinese IME's composition window (NCChineseIME::OnCreate): the caret flag as given, the
// first character and the caret at 0, the place and size set; BackTex is left as allocated
NCCompositeBox::NCCompositeBox(INT InW, INT InH, INT InX, INT InY, UBOOL bInShowCaret)
{
    bShowCaret = bInShowCaret;
    FirstChar = 0;
    CaretPos = 0;
    SetPosition(InX, InY, InW, InH);
}
NW_HOOK_CTOR_ARGS(0x10005160, NCCompositeBox, (INT W, INT H, INT X, INT Y, UBOOL B), (W, H, X, Y, B))

// 0x100050f0 - Text is an FString
NW_HOOK_DTOR(0x100050f0, NCCompositeBox)


// 0x10043ef0 (vslot 25) - the stock body is NControl's (C2: NCWnd's alone, no enabled test)
INT NCCompositeBox::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return NCWnd::WindowProc(Msg, wParam, lParam) != 0;
}

// C2 CANDIDATE: the C2 function is not the same code as the C1 one; kept only while it matches the stock one (verify_data)
// 0x10004070 (vslot 37)
INT NCCompositeBox::OnCreate()
{
    NCWnd::OnCreate();
    BackTex = FindTexture(TEXT("NWindow.BlackTexture"));
    Text.Empty();
    return 1;
}
NW_HOOK_METHOD(0x10004070, NCCompositeBox, OnCreate)

// 0x10004110 (vslot 38) - guarded; NCWnd's OnDestroy and nothing else
INT NCCompositeBox::OnDestroy()
{
    guard(NCCompositeBox::OnDestroy);
    NCWnd::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10004110, NCCompositeBox, OnDestroy)

// 0x10004810 (vslot 62) - the width drawn is unsigned in the stock code; C3 draws in light grey and the clip comes
// after the origin
INT NCCompositeBox::OnPaint(NCGDevice* Device)
{
    guard(NCCompositeBox::OnPaint);
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    if (BackTex)
        Device->DrawStretchedTex(0, 0, (INT)Width, (INT)Height, BackTex, 1, 0xFF);
    DWORD X = 0;
    INT i;
    for (i = FirstChar; i < Text.Len(); i++)
    {
        if (i == CaretPos && bShowCaret)
            Device->DrawText(X + 1, 3, 0xFFDCDCDC, TEXT("|"), 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
        X += Device->DrawText(X + 3, 3, 0xFFDCDCDC, *Text.Mid(i, 1), 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0,
                              0, 0);
        if (X > 97)
            break;
    }
    if (i == CaretPos && bShowCaret)
        Device->DrawText(X + 1, 3, 0xFFDCDCDC, TEXT("|"), 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
    Device->ReleaseClip();
    Device->EndPaint();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10004810, NCCompositeBox, OnPaint)

// 0x100040a0
void NCCompositeBox::CaretLeft()
{
    if (CaretPos > 0)
        CaretPos--;
    if (FirstChar > CaretPos)
        FirstChar = CaretPos;
}
NW_HOOK_METHOD(0x100040a0, NCCompositeBox, CaretLeft)

// 0x10004a10 - measures with a device created (and destroyed) just for the call, as in the stock code (its stock
// constructor is run on the object the class allocates)
void NCCompositeBox::CaretRight()
{
    if (Text.Len() > CaretPos)
        CaretPos++;
    NCGDevice* Device = new NCGDevice();
    SIZE Size;
    NCGDevice::GetTextExtent(&Size, *Text.Mid(FirstChar, CaretPos - FirstChar), 0, 0, 0, 0, 0, L2FONT_Normal);
    while (Size.cx > 97)
    {
        FirstChar++;
        NCGDevice::GetTextExtent(&Size, *Text.Mid(FirstChar, CaretPos - FirstChar), 0, 0, 0, 0, 0, L2FONT_Normal);
    }
    if (Device)
        delete Device;
}
NW_HOOK_METHOD(0x10004a10, NCCompositeBox, CaretRight)

// 0x10004b80 - scrolls back to the start, then forward until the caret fits
void NCCompositeBox::SetText(const TCHAR* InText)
{
    if (!InText)
        return;
    FirstChar = 0;
    Text = FString(InText);
    NCGDevice* Device = new NCGDevice();
    SIZE Size;
    NCGDevice::GetTextExtent(&Size, *Text.Mid(FirstChar, CaretPos - FirstChar), 0, 0, 0, 0, 0, L2FONT_Normal);
    while (Size.cx > 97)
    {
        FirstChar++;
        NCGDevice::GetTextExtent(&Size, *Text.Mid(FirstChar, CaretPos - FirstChar), 0, 0, 0, 0, 0, L2FONT_Normal);
    }
    if (Device)
        delete Device;
}
NW_HOOK_METHOD(0x10004b80, NCCompositeBox, SetText)

// 0x100040d0 - a copy of the composition string
FString NCCompositeBox::GetText()
{
    return Text;
}
NW_HOOK_METHOD(0x100040d0, NCCompositeBox, GetText)

// 0x10004100
void NCCompositeBox::SetShowCaret(UBOOL bShow)
{
    bShowCaret = bShow;
}
NW_HOOK_METHOD(0x10004100, NCCompositeBox, SetShowCaret)

NW_CHECK_VTABLE(NCCompositeBox, 0x1019f9e0, 74)
