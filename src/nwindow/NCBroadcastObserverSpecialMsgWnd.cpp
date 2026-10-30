/*=============================================================================
    NCBroadcastObserverSpecialMsgWnd.cpp - the big banners of the broadcast observer screen (C3 only; C3
    addresses). Stock: ShowWindow 0x10116A60, OnCreate 0x10116B30, OnDestroy 0x10116C70, OnTimer 0x10116CF0, OnPaint
    0x10116D90 (its jump table at 0x10116FD4), the constructor 0x10117000 and the destructor 0x101170C0.
=============================================================================*/
#include "nwindow/NCBroadcastObserverSpecialMsgWnd.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NConsoleWnd.h"

enum
{
    TIMER_SpecialMsgHide = 0xC0000,   // ShowWindow starts it, OnTimer hides the banner (name ours)
};

// 0x1012cd70 - the textures and the timer cleared, Mode 2 (the start picture)
NCBroadcastObserverSpecialMsgWnd::NCBroadcastObserverSpecialMsgWnd()
{
    BandTex[0] = BandTex[1] = BandTex[2] = NULL;
    StartTex = NULL;
    FinishTex = NULL;
    GameOverTex = NULL;
    TimerID = 0;
    Mode = SPECMSG_Start;
    CountTex[0] = CountTex[1] = CountTex[2] = CountTex[3] = CountTex[4] = NULL;
}
NW_HOOK_CTOR(0x1012cd70, NCBroadcastObserverSpecialMsgWnd)

// 0x1012ce30
NW_HOOK_DTOR(0x1012ce30, NCBroadcastObserverSpecialMsgWnd)

// 0x10148f10 (vslot 25) - NControl::WindowProc's body, which the stock vtables of these windows share (the C3 address was
// 0x10119f60): NCWnd's, normalized to 0 or 1
INT NCBroadcastObserverSpecialMsgWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return NCWnd::WindowProc(Msg, wParam, lParam) != 0;
}

// 0x1012c7d0 (vslot 48, guard: NCBroadcastObserverSpecialMsgWnd::ShowWindow) - NCWnd's, then the hide timer starts over:
// one second for the start picture, five for the others
void NCBroadcastObserverSpecialMsgWnd::ShowWindow()
{
    guard(NCBroadcastObserverSpecialMsgWnd::ShowWindow);
    NCWnd::ShowWindow();
    if (TimerID)
    {
        KillTimer(TimerID);
        TimerID = 0;
    }
    TimerID = SetTimer(TIMER_SpecialMsgHide, Mode == SPECMSG_Start ? 1000 : 5000, 0);
    unguard;
}
NW_HOOK_METHOD(0x1012c7d0, NCBroadcastObserverSpecialMsgWnd, ShowWindow)

// 0x1012c890 (vslot 37; the guard string is NCBroadcastObserverWnd::OnCreate, copied from the parent window) - the
// band's three pieces, the start / finish / game over pictures and the five count pictures
INT NCBroadcastObserverSpecialMsgWnd::OnCreate()
{
    guard(NCBroadcastObserverWnd::OnCreate);
    NCWnd::OnCreate();
    for (INT i = 0; i < 3; i++)
        BandTex[i] = FindTexture(*FString::Printf(TEXT("L2UI_CH3.BroadcastObs.br_msg1_back%d"), i + 1));
    StartTex = FindTexture(TEXT("L2UI_CH3.BroadcastObs.br_msg1_start"));
    FinishTex = FindTexture(TEXT("L2UI_CH3.BroadcastObs.br_msg1_finish"));
    GameOverTex = FindTexture(TEXT("L2UI_CH3.BroadcastObs.br_msg1_gameover"));
    for (INT i = 0; i < 5; i++)
        CountTex[i] = FindTexture(*FString::Printf(TEXT("L2UI_CH3.BroadcastObs.br_msg1_count%d"), i + 1));
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1012c890, NCBroadcastObserverSpecialMsgWnd, OnCreate)

// 0x1012c9e0 (vslot 38; the guard string is NCBroadcastObserverWnd::OnDestroy) - nothing of its own to free
INT NCBroadcastObserverSpecialMsgWnd::OnDestroy()
{
    guard(NCBroadcastObserverWnd::OnDestroy);
    NCWnd::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1012c9e0, NCBroadcastObserverSpecialMsgWnd, OnDestroy)

// 0x1012ca60 (vslot 36, guard: NCBroadcastObserverSpecialMsgWnd::OnTimer; ret 4) - the banner's time is over: the timer
// goes and the window is hidden
INT NCBroadcastObserverSpecialMsgWnd::OnTimer(INT ID)
{
    guard(NCBroadcastObserverSpecialMsgWnd::OnTimer);
    if (ID == TIMER_SpecialMsgHide && TimerID)
    {
        KillTimer(TimerID);
        TimerID = 0;
        HideWindow();
    }
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1012ca60, NCBroadcastObserverSpecialMsgWnd, OnTimer)

// 0x1012cb00 (vslot 56, guard: NCBroadcastObserverSpecialMsgWnd::OnPaint) - the band (three pieces, 32 wide and 512
// high, the middle one stretched), then the picture of the Mode centered 32 pixels down
INT NCBroadcastObserverSpecialMsgWnd::OnPaint(NCGDevice* Device)
{
    guard(NCBroadcastObserverSpecialMsgWnd::OnPaint);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    Device->BeginPaint(ScreenX, ScreenY);
    Device->DrawTexture(0, 0, 0x20, 0x200, 0, 0, 0x20, 0x200, BandTex[0], 0xFF, 1);
    Device->DrawTexture(0x20, 0, (INT)(Width - 64.0f), 0x200, 0, 0, 0x20, 0x200, BandTex[1], 0xFF, 1);
    Device->DrawTexture((INT)(Width - 32.0f), 0, 0x20, 0x200, 0, 0, 0x20, 0x200, BandTex[2], 0xFF, 1);
    switch (Mode)
    {
    case 1:
        Device->DrawTexture((INT)(Width * 0.5f - 256.0f), 0x20, 0x200, 0x100, 0, 0, 0x200, 0x100, FinishTex, 0xFF, 1);
        break;
    case 2:
        Device->DrawTexture((INT)(Width * 0.5f - 256.0f), 0x20, 0x200, 0x200, 0, 0, 0x200, 0x200, StartTex, 0xFF, 1);
        break;
    case 3:
        Device->DrawTexture((INT)(Width * 0.5f - 256.0f), 0x20, 0x200, 0x100, 0, 0, 0x200, 0x100, GameOverTex, 0xFF, 1);
        break;
    case 4:
        Device->DrawTexture((INT)(Width * 0.5f - 128.0f), 0x20, 0x100, 0x100, 0, 0, 0x100, 0x100, CountTex[0], 0xFF, 1);
        break;
    case 5:
        Device->DrawTexture((INT)(Width * 0.5f - 128.0f), 0x20, 0x100, 0x100, 0, 0, 0x100, 0x100, CountTex[1], 0xFF, 1);
        break;
    case 6:
        Device->DrawTexture((INT)(Width * 0.5f - 128.0f), 0x20, 0x100, 0x100, 0, 0, 0x100, 0x100, CountTex[2], 0xFF, 1);
        break;
    case 7:
        Device->DrawTexture((INT)(Width * 0.5f - 128.0f), 0x20, 0x100, 0x100, 0, 0, 0x100, 0x100, CountTex[3], 0xFF, 1);
        break;
    case 8:
        Device->DrawTexture((INT)(Width * 0.5f - 128.0f), 0x20, 0x100, 0x100, 0, 0, 0x100, 0x100, CountTex[4], 0xFF, 1);
        break;
    }
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1012cb00, NCBroadcastObserverSpecialMsgWnd, OnPaint)

NW_CHECK_VTABLE(NCBroadcastObserverSpecialMsgWnd, 0x101d4860, 74)
