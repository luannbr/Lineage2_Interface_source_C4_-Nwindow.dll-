/*=============================================================================
    NCBroadcastObserverTopWnd.cpp - the score bar of the broadcast observer screen (C3 only; C3 addresses). Stock:
    OnCreate 0x10117120, OnDestroy 0x10117260, GetScore 0x101172E0, GetTeamName 0x101173A0, ShowWindow 0x10117450,
    OnPaint 0x10117520, OnTimer 0x10117950, the constructor 0x10117AB0 and the destructor 0x10117B80.
=============================================================================*/
#include "nwindow/NCBroadcastObserverTopWnd.h"
#include "nwindow/NCBroadcastObserverWnd.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NConsoleWnd.h"

enum
{
    TIMER_TopSlideDown = 0xFE0000,   // names ours
    TIMER_TopSlideUp = 0xFE0001,
    TIMER_TopHold = 0xFE0002,
    TOP_HoldMillis = 5000,
    TOP_Height = 0x71,
};

// 0x1012d830 (ret 4) - the textures and the three timers cleared, the owner kept
NCBroadcastObserverTopWnd::NCBroadcastObserverTopWnd(NCBroadcastObserverWnd* InOwner)
{
    for (INT i = 0; i < 6; i++)
        BackTex[i] = NULL;
    for (INT i = 0; i < 10; i++)
        ScoreTex[i] = NULL;
    Owner = InOwner;
    ColonTex = NULL;
    SlideTimerID = 0;
    HoldTimerID = 0;
    EndTimerID = 0;
}
NW_HOOK_CTOR_ARGS(0x1012d830, NCBroadcastObserverTopWnd, (NCBroadcastObserverWnd* InOwner), (InOwner))

// 0x1012d900
NW_HOOK_DTOR(0x1012d900, NCBroadcastObserverTopWnd)

// 0x10148f10 (vslot 25) - NControl::WindowProc's body, which the stock vtables of these windows share (the C3 address was
// 0x10119f60): NCWnd's, normalized to 0 or 1
INT NCBroadcastObserverTopWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return NCWnd::WindowProc(Msg, wParam, lParam) != 0;
}

// 0x1012ce90 (vslot 37; the guard string is NCBroadcastObserverWnd::OnCreate, copied from the parent window) - the six
// pieces of the band, the ten digits and the colon
INT NCBroadcastObserverTopWnd::OnCreate()
{
    guard(NCBroadcastObserverWnd::OnCreate);
    NCWnd::OnCreate();
    BackTex[0] = FindTexture(TEXT("L2UI_CH3.BroadcastObs.br_top_back1"));
    BackTex[1] = FindTexture(TEXT("L2UI_CH3.BroadcastObs.br_top_back2"));
    BackTex[2] = FindTexture(TEXT("L2UI_CH3.BroadcastObs.br_top_back3"));
    BackTex[3] = FindTexture(TEXT("L2UI_CH3.BroadcastObs.br_top_back4"));
    BackTex[4] = FindTexture(TEXT("L2UI_CH3.BroadcastObs.br_top_back5"));
    BackTex[5] = FindTexture(TEXT("L2UI_CH3.BroadcastObs.br_top_back6"));
    for (INT i = 0; i < 10; i++)
        ScoreTex[i] = FindTexture(*FString::Printf(TEXT("L2UI_CH3.BroadcastObs.br_score%d"), i));
    ColonTex = FindTexture(TEXT("L2UI_CH3.BroadcastObs.br_scorecolon"));
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1012ce90, NCBroadcastObserverTopWnd, OnCreate)

// 0x1012cfe0 (vslot 38; the guard string is NCBroadcastObserverWnd::OnDestroy) - nothing of its own to free
INT NCBroadcastObserverTopWnd::OnDestroy()
{
    guard(NCBroadcastObserverWnd::OnDestroy);
    NCWnd::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1012cfe0, NCBroadcastObserverTopWnd, OnDestroy)

// 0x1012d060 (guard: NCBroadcastObserverTopWnd::GetScore; ret 4) - the digit of a team's score (the owner's score, a
// value out of 0..9 draws the zero); the zero digit for a team out of 0..1 or without an owner
UTexture* NCBroadcastObserverTopWnd::GetScore(INT Team)
{
    guard(NCBroadcastObserverTopWnd::GetScore);
    if (Team >= 0 && Team < 2 && Owner)
    {
        INT Score = Owner->GetTeamScore(Team);
        if (Score < 0 || Score > 9)
            Score = 0;
        return ScoreTex[Score];
    }
    return ScoreTex[0];
    unguard;
}
NW_HOOK_METHOD(0x1012d060, NCBroadcastObserverTopWnd, GetScore)

// 0x1012d120 (guard: NCBroadcastObserverTopWnd::GetTeamName; ret 8: the FString comes back by the hidden pointer) -
// the owner's name of the team, an empty string for a team out of 0..1 or without an owner
FString NCBroadcastObserverTopWnd::GetTeamName(INT Team)
{
    guard(NCBroadcastObserverTopWnd::GetTeamName);
    if (Team < 0 || Team >= 2 || !Owner)
        return FString(TEXT(""));
    return Owner->GetTeamName(Team);
    unguard;
}
NW_HOOK_METHOD(0x1012d120, NCBroadcastObserverTopWnd, GetTeamName)

// 0x1012d1d0 (vslot 44, guard: NCBroadcastObserverTopWnd::ShowWindow) - NCWnd's, then every timer goes and the slide
// down starts (one millisecond per step)
void NCBroadcastObserverTopWnd::ShowWindow()
{
    guard(NCBroadcastObserverTopWnd::ShowWindow);
    NCWnd::ShowWindow();
    if (SlideTimerID)
    {
        KillTimer(SlideTimerID);
        SlideTimerID = 0;
    }
    if (HoldTimerID)
    {
        KillTimer(HoldTimerID);
        HoldTimerID = 0;
    }
    if (EndTimerID)
    {
        KillTimer(EndTimerID);
        EndTimerID = 0;
    }
    SlideTimerID = SetTimer(TIMER_TopSlideDown, 1, 0);
    unguard;
}
NW_HOOK_METHOD(0x1012d1d0, NCBroadcastObserverTopWnd, ShowWindow)

// 0x1012d6d0 (vslot 36, guard: NCBroadcastObserverTopWnd::OnTimer; ret 4) - the slide: down 3 pixels a step until the
// bar is on the screen (then it takes the owner's width less 120 and waits five seconds), up 3 pixels a step after the
// wait until it is gone
INT NCBroadcastObserverTopWnd::OnTimer(INT ID)
{
    guard(NCBroadcastObserverTopWnd::OnTimer);
    if (ID == TIMER_TopSlideDown)
    {
        if (ScreenY < 0)
        {
            MoveWindow(0, 3);
            if (ScreenY < 0)
                return 1;
        }
        SetPosition(60, 0, (INT)(Owner->Width - 120.0f), TOP_Height);
        if (SlideTimerID)
        {
            KillTimer(SlideTimerID);
            SlideTimerID = 0;
        }
        HoldTimerID = SetTimer(TIMER_TopHold, TOP_HoldMillis, 0);
    }
    else if (ID == TIMER_TopSlideUp)
    {
        if ((FLOAT)ScreenY + Height > 0.0f)
            MoveWindow(0, -3);
        else if (EndTimerID)
        {
            KillTimer(EndTimerID);
            EndTimerID = 0;
        }
    }
    else if (ID == TIMER_TopHold)
    {
        if (HoldTimerID)
        {
            KillTimer(HoldTimerID);
            HoldTimerID = 0;
        }
        SlideTimerID = SetTimer(TIMER_TopSlideUp, 1, 0);
    }
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1012d6d0, NCBroadcastObserverTopWnd, OnTimer)

// 0x1012d2a0 (vslot 62; the guard string is NCBroadcastObserverWnd::OnPaint) - the band: the left end (256 x 128), the
// stretched middle (half of the width less 420), the next pieces, the same middle again and the right end; the two
// score digits around the colon in the middle (128 x 128) and each team's name under its digit (right aligned for the
// first, left aligned for the second), in font 4
INT NCBroadcastObserverTopWnd::OnPaint(NCGDevice* Device)
{
    guard(NCBroadcastObserverWnd::OnPaint);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    Device->BeginPaint(ScreenX, ScreenY);
    Device->DrawTexture(0, 0, 0x100, 0x80, 0, 0, 0x100, 0x80, BackTex[0], 0xFF, 1);
    INT Half = (INT)(Width * 0.5f - 420.0f);
    INT TextWidth = Half + 0x82;
    Device->DrawTexture(0x100, 0, Half, 0x80, 0, 0, 0x100, 0x80, BackTex[1], 0xFF, 1);
    INT X = Half + 0x100;
    Device->DrawTexture(X, 0, 0x40, 0x80, 0, 0, 0x40, 0x80, BackTex[2], 0xFF, 1);
    X += 0x40;
    Device->DrawTexture(X, 0, 0xC8, 0x80, 0, 0, 0x20, 0x80, BackTex[3], 0xFF, 1);
    X += 0xC8;
    INT SecondX = X;
    Device->DrawTexture(X, 0, 0x40, 0x80, 0, 0, 0x40, 0x80, BackTex[4], 0xFF, 1);
    X += 0x40;
    INT Half2 = (INT)(Width * 0.5f - 420.0f);
    Device->DrawTexture(X, 0, Half2, 0x80, 0, 0, 0x100, 0x80, BackTex[1], 0xFF, 1);
    X += Half2;
    Device->DrawTexture(X, 0, 0x100, 0x80, 0, 0, 0x100, 0x80, BackTex[5], 0xFF, 1);

    INT Center = (INT)(Width * 0.5f - 105.0f);
    Device->DrawTexture(Center, 0x1C, 0x80, 0x80, 0, 0, 0x80, 0x80, GetScore(0), 0xFF, 1);
    Device->DrawTexture((INT)(Width * 0.5f - 51.0f), 0x12, 0x80, 0x80, 0, 0, 0x80, 0x80, ColonTex, 0xFF, 1);
    Device->DrawTexture((INT)(Width * 0.5f + 5.0f), 0x1C, 0x80, 0x80, 0, 0, 0x80, 0x80, GetScore(1), 0xFF, 1);

    SIZE Size;
    NCGDevice::GetTextExtent(&Size, *GetTeamName(0), 0, 0, 0, 0, 0, (L2FontType)4);
    Device->DrawText(Center - Size.cx, 0x28, 0xFFFF3737, *GetTeamName(0), TextWidth, 0, 0, (L2FontType)4,
                     0, 0, 0, 0, 0, 0, 0);
    NCGDevice::GetTextExtent(&Size, *GetTeamName(1), 0, 0, 0, 0, 0, (L2FontType)4);
    Device->DrawText(SecondX, 0x28, 0xFFDCDCDC, *GetTeamName(1), TextWidth, 0, 0, (L2FontType)4,
                     0, 0, 0, 0, 0, 0, 0);
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1012d2a0, NCBroadcastObserverTopWnd, OnPaint)

NW_CHECK_VTABLE(NCBroadcastObserverTopWnd, 0x101d4e18, 74)
