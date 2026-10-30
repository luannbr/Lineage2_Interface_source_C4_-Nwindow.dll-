/*=============================================================================
    NCCountSpin.cpp - number with up / down buttons. Stock: 0x100050a0 .. 0x100051e9,
    0x100052b0 .. 0x1000572e (with the destructor).
=============================================================================*/
#include "nwindow/NCCountSpin.h"
#include "nwindow/NCButton.h"
#include "nwindow/NCGDevice.h"

#include <stdlib.h>
#include <wchar.h>

// button ids (names ours)
enum
{
    NID_COUNTSPIN_UP = 0x100A,
    NID_COUNTSPIN_DOWN = 0x100B,
};

// the code sent to the parent with each step (NWM_COMMAND)
enum
{
    COUNTSPIN_Up = 1,
    COUNTSPIN_Down = 2,
};

// 0x10007560 - the value starts at the minimum; the label is "Default" without a text
NCCountSpin::NCCountSpin(INT InMin, INT InMax, const TCHAR* InLabel)
{
    TextX = 5;
    TextY = 5;
    BackTex = NULL;
    UpButton = NULL;
    DownButton = NULL;
    bCancelStep = 0;
    Min = InMin;
    Max = InMax;
    Value = InMin;
    _itow(InMin, ValueText, 10);
    Label = InLabel ? InLabel : TEXT("Default");
}
NW_HOOK_CTOR_ARGS(0x10007560, NCCountSpin, (INT A, INT B, const TCHAR* C), (A, B, C))

// 0x10007680 - Label is an FString
NW_HOOK_DTOR(0x10007680, NCCountSpin)


// 0x10006d50 (vslot 25) - NControl's, then the buttons' clicks; C2: no enabled test
INT NCCountSpin::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NControl::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND)
    {
        if (wParam == NID_COUNTSPIN_UP)
            return OnUp() != 0;
        if (wParam == NID_COUNTSPIN_DOWN)
            return OnDown() != 0;
    }
    return 0;
}
NW_HOOK_METHOD(0x10006d50, NCCountSpin, WindowProc)

// 0x10006dd0 (vslot 75)
INT NCCountSpin::OnUp()
{
    if (Value < Max)
    {
        Value++;
        NotifyParent(COUNTSPIN_Up);
        if (bCancelStep)
        {
            Value--;
            bCancelStep = 0;
        }
        _itow(Value, ValueText, 10);
    }
    return 1;
}
NW_HOOK_METHOD(0x10006dd0, NCCountSpin, OnUp)

// 0x10006e30 (vslot 76)
INT NCCountSpin::OnDown()
{
    if (Value > Min)
    {
        Value--;
        NotifyParent(COUNTSPIN_Down);
        if (bCancelStep)
        {
            Value++;
            bCancelStep = 0;
        }
        _itow(Value, ValueText, 10);
    }
    return 1;
}
NW_HOOK_METHOD(0x10006e30, NCCountSpin, OnDown)

// 0x10006f00 (vslot 62) - the background, the label, the number 5 pixels after the label's width (what DrawText
// returns); C3 draws them in light grey
INT NCCountSpin::OnPaint(NCGDevice* Device)
{
    guard(NCCountSpin::OnPaint);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    Device->BeginPaint(ScreenX, ScreenY);
    if (BackTex)
        Device->DrawStretchedTex(0, 0, (INT)Width, (INT)Height, BackTex, 1, 0xFF);
    INT LabelW = Device->DrawText(TextX, TextY, 0xFFDCDCDC, *Label, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
    Device->DrawText(TextX + LabelW + 5, TextY, 0xFFDCDCDC, ValueText, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0,
                     0, 0, 0);
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10006f00, NCCountSpin, OnPaint)

// 0x10007300 (vslot 37) - the background and the two small push buttons (white over black) at the right end of the
// control: as tall as it is high minus 10 and half of that wide... (width = height - 10, height = half of it), the
// up one 3 pixels from the top, the down one below it; both repeat every 180 ms
INT NCCountSpin::OnCreate()
{
    NCWnd::OnCreate();
    BackTex = FindTexture(TEXT("NWindow.EditBack"));
    NRectF R;
    R.X = 0;
    R.Y = 0;
    R.W = 0.0f;
    R.H = 0.0f;
    GetLocalRect(&R);
    UpButton = new NCPushButton(TEXT("NWindow.WhiteTexture"), TEXT("NWindow.BlackTexture"));
    UpButton->ID = NID_COUNTSPIN_UP;
    UpButton->SetPosition((INT)(R.W - R.H + 5.0f), 3, (INT)(R.H - 10.0f), (INT)(0.5f * (R.H - 10.0f)));
    CreateChild(UpButton, NWS_VISIBLE);
    UpButton->RepeatDelay = 180;
    DownButton = new NCPushButton(TEXT("NWindow.WhiteTexture"), TEXT("NWindow.BlackTexture"));
    DownButton->ID = NID_COUNTSPIN_DOWN;
    FLOAT Half = 0.5f * (R.H - 10.0f);
    DownButton->SetPosition((INT)(R.W - R.H + 5.0f), (INT)(R.H - (Half + 5.0f) + 2.0f), (INT)(R.H - 10.0f),
                            (INT)Half);
    CreateChild(DownButton, NWS_VISIBLE);
    DownButton->RepeatDelay = 180;
    return 1;
}
NW_HOOK_METHOD(0x10007300, NCCountSpin, OnCreate)

// 0x10007050 (vslot 38) - guarded in C2
INT NCCountSpin::OnDestroy()
{
    guard(NCCountSpin::OnDestroy);
    if (UpButton)
    {
        RemoveChildWindow(UpButton->Entry);
        delete UpButton;
        UpButton = NULL;
    }
    if (DownButton)
    {
        RemoveChildWindow(DownButton->Entry);
        delete DownButton;
        DownButton = NULL;
    }
    NCWnd::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10007050, NCCountSpin, OnDestroy)

NW_CHECK_VTABLE(NCCountSpin, 0x101a0308, 77)
