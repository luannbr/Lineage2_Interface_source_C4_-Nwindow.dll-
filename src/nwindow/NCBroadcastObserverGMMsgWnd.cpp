/*=============================================================================
    NCBroadcastObserverGMMsgWnd.cpp - the GM's message banner of the broadcast observer screen (C3 only; C3
    addresses). Stock: 0x101136B0 .. 0x10113D4D (ShowWindow, SetMsg, OnCreate, OnDestroy, OnTimer, OnPaint, the
    constructor 0x10113BE0 and the destructor 0x10113CD0).
=============================================================================*/
#include "nwindow/NCBroadcastObserverGMMsgWnd.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NConsoleWnd.h"

enum
{
    TIMER_GMMsgHide = 0xB0000,    // ShowWindow starts it, OnTimer hides the banner (name ours)
    GMMSG_ShowMillis = 5000,
    SYSMSG_GMMsgDefault = 1431,   // GL2GameData + 0x5028C: the text OnCreate starts with
};

// 0x101299b0 (guard: the constructor's own name) - the three textures and the timer cleared, the text an empty string
NCBroadcastObserverGMMsgWnd::NCBroadcastObserverGMMsgWnd()
{
    guard(NCBroadcastObserverGMMsgWnd::NCBroadcastObserverGMMsgWnd);
    for (INT i = 0; i < 3; i++)
        PieceTex[i] = NULL;
    Msg = TEXT("");
    TimerID = 0;
    unguard;
}
NW_HOOK_CTOR(0x101299b0, NCBroadcastObserverGMMsgWnd)

// 0x10129aa0
NW_HOOK_DTOR(0x10129aa0, NCBroadcastObserverGMMsgWnd)

// 0x10148f10 (vslot 25) - NControl::WindowProc's body, which the stock vtables of these windows share (the C3 address was
// 0x10119f60): NCWnd's, normalized to 0 or 1
INT NCBroadcastObserverGMMsgWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return NCWnd::WindowProc(Msg, wParam, lParam) != 0;
}

// 0x10129490 (vslot 44, guard: NCBroadcastObserverGMMsgWnd::ShowWindow) - NCWnd's, then the five second timer starts
// over
void NCBroadcastObserverGMMsgWnd::ShowWindow()
{
    guard(NCBroadcastObserverGMMsgWnd::ShowWindow);
    NCWnd::ShowWindow();
    if (TimerID)
    {
        KillTimer(TimerID);
        TimerID = 0;
    }
    TimerID = SetTimer(TIMER_GMMsgHide, GMMSG_ShowMillis, 0);
    unguard;
}
NW_HOOK_METHOD(0x10129490, NCBroadcastObserverGMMsgWnd, ShowWindow)

// 0x10129540 (guard: NCBroadcastObserverGMMsgWnd::SetMsg; ret 0xC: the FString comes by value) - the banner's text
void NCBroadcastObserverGMMsgWnd::SetMsg(FString NewMsg)
{
    guard(NCBroadcastObserverGMMsgWnd::SetMsg);
    Msg = NewMsg;
    unguard;
}
NW_HOOK_METHOD(0x10129540, NCBroadcastObserverGMMsgWnd, SetMsg)

// 0x101295d0 (vslot 37, guard: NCBroadcastObserverGMMsgWnd::OnCreate) - the three pieces of the banner and the
// default text (the stock reads the system message 1431 at GL2GameData + 0x82F1C in C4: 0x722C8 + 0x30 * 1431 + 4)
INT NCBroadcastObserverGMMsgWnd::OnCreate()
{
    guard(NCBroadcastObserverGMMsgWnd::OnCreate);
    NCWnd::OnCreate();
    for (INT i = 0; i < 3; i++)
        PieceTex[i] = FindTexture(*FString::Printf(TEXT("L2UI_CH3.BroadcastObs.br_msg2_back%d"), i + 1));
    Msg = *GL2GameData.SystemMsg[SYSMSG_GMMsgDefault].Message;
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x101295d0, NCBroadcastObserverGMMsgWnd, OnCreate)

// 0x101296b0 (vslot 38, guard: NCBroadcastObserverGMMsgWnd::OnDestroy) - nothing of its own to free
INT NCBroadcastObserverGMMsgWnd::OnDestroy()
{
    guard(NCBroadcastObserverGMMsgWnd::OnDestroy);
    NCWnd::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x101296b0, NCBroadcastObserverGMMsgWnd, OnDestroy)

// 0x10129730 (vslot 36, guard: NCBroadcastObserverGMMsgWnd::OnTimer; ret 4) - the five seconds are over: the timer
// goes and the banner is hidden
INT NCBroadcastObserverGMMsgWnd::OnTimer(INT ID)
{
    guard(NCBroadcastObserverGMMsgWnd::OnTimer);
    if (ID == TIMER_GMMsgHide && TimerID)
    {
        KillTimer(TimerID);
        TimerID = 0;
        HideWindow();
    }
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10129730, NCBroadcastObserverGMMsgWnd, OnTimer)

// 0x101297d0 (vslot 62, guard: NCBroadcastObserverGMMsgWnd::OnPaint) - the three pieces (32 wide, 128 high; the middle
// one stretched between the ends), then the text centered across the window and 15 pixels from the top (C4: the C3
// code centered it vertically too), wrapped at the window's width, in font 7
INT NCBroadcastObserverGMMsgWnd::OnPaint(NCGDevice* Device)
{
    guard(NCBroadcastObserverGMMsgWnd::OnPaint);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    Device->BeginPaint(ScreenX, ScreenY);
    Device->DrawTexture(0, 0, 0x20, 0x80, 0, 0, 0x20, 0x80, PieceTex[0], 0xFF, 1);
    Device->DrawTexture(0x20, 0, (INT)(Width - 64.0f), 0x80, 0, 0, 0x20, 0x80, PieceTex[1], 0xFF, 1);
    Device->DrawTexture((INT)(Width - 32.0f), 0, 0x20, 0x80, 0, 0, 0x20, 0x80, PieceTex[2], 0xFF, 1);
    SIZE Size;
    NCGDevice::GetTextExtent(&Size, *Msg, (DWORD)(INT)Width, 0, 0, 0, 0, (L2FontType)7);
    Device->DrawText((INT)(Width * 0.5f - Size.cx / 2), 15, 0xFFDCDCDC, *Msg, (INT)Width, 0, 0, (L2FontType)7, 0, 0, 0,
                     0, 0, 0, 0);
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x101297d0, NCBroadcastObserverGMMsgWnd, OnPaint)

NW_CHECK_VTABLE(NCBroadcastObserverGMMsgWnd, 0x101d2e70, 74)
