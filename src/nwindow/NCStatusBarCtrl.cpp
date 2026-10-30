/*=============================================================================
    NCStatusBarCtrl.cpp - the gauge of the player's status window. C3 stock: 0x100fa3a0 (OnTimer), 0x100fb3a0 (OnPaint),
    0x101024d0 (constructor), 0x10102570 (destructor).
=============================================================================*/
#include "nwindow/NCStatusBarCtrl.h"
#include "nwindow/NCGDevice.h"

#include <intrin.h>

// NCGDevice::DrawSpecialDigit (0x10013350) takes a sixth argument in C4 (ret 0x18: the alpha, 0xFF here)
static FORCEINLINE void DrawDigits(NCGDevice* Device, UBOOL bLarge, INT X, INT Y, DWORD Color, TCHAR* Text)
{
    Device->DrawSpecialDigit(bLarge, X, Y, Color, Text, 0xFF);
}

// 0x101162e0 - NControl's fields (ID, texture, notify id -1, empty caption), the label built; the rest is left as
// allocated
NCStatusBarCtrl::NCStatusBarCtrl()
{
}
NW_HOOK_CTOR(0x101162e0, NCStatusBarCtrl)

// 0x101163f0 (ret 0x1c) - the constructor NCPlayerStatusWnd's bars are made with: NControl's fields, the label built,
// the fill and back textures found, the ghost and overlay ones when they are named (their flags say whether they are
// drawn), the textures' size, the values at 0 and the timers off
NCStatusBarCtrl::NCStatusBarCtrl(const TCHAR* InLabel, INT InTexW, INT InTexH, const TCHAR* FillName,
                                 const TCHAR* BackName, const TCHAR* GhostName, const TCHAR* OverlayName)
{
    FillTex = FindTexture(FillName);
    BackTex = FindTexture(BackName);
    if (GhostName)
    {
        GhostTex = FindTexture(GhostName);
        bGhost = 1;
    }
    else
        bGhost = 0;
    if (OverlayName)
    {
        OverlayTex = FindTexture(OverlayName);
        bOverlay = 1;
    }
    else
        bOverlay = 0;
    TexWidth = InTexW;
    TexHeight = InTexH;
    MaxValue = 0;
    Cur = 0;
    Unknown214 = 0;
    GhostTimer = 0;
    ValueTimer = 0;
    Text = InLabel;
}
NW_HOOK_CTOR_ARGS(0x101163f0, NCStatusBarCtrl,
                  (const TCHAR* A, INT B, INT C, const TCHAR* D, const TCHAR* E, const TCHAR* F, const TCHAR* G),
                  (A, B, C, D, E, F, G))

// 0x10116380 - ConditionalDestroy, the label, then the control
NW_HOOK_DTOR(0x10116380, NCStatusBarCtrl)

// 0x1010d040 (name ours) - no strip left: the ghost's scale, rate, step and amount cleared
void NCStatusBarCtrl::ClearTrail()
{
    GhostScale = 0.0;
    Rate = 0;
    GhostStep = 0.0;
    Ghost = 0.0;
}
NW_HOOK_METHOD(0x1010d040, NCStatusBarCtrl, ClearTrail)

// UE2's appSeconds (UnVcWin32.h): the time stamp counter in seconds, plus 2^24 (the stock expands it inline in StartAnim)
static FORCEINLINE DOUBLE InlineAppSeconds()
{
    unsigned __int64 Cycles = __rdtsc();
    DWORD L = (DWORD)Cycles;
    DWORD H = (DWORD)(Cycles >> 32);
    return ((DOUBLE)L + 4294967296.0 * (DOUBLE)H) * GSecondsPerCycle + 16777216.0;
}

// 0x1010e3e0 (ret 0x10; name ours) - the bar moves to Value of Max over Time seconds: the strip's scale starts from a point below the
// value (by 0.666 of half the maximum), the amount is that scale over the bar's length (Max * 0.666) times the time, the step is the time,
// the rate the maximum, and the start is stamped. C4: the same as the C3 one at the fields' new displacements (GhostScale +0x218,
// Rate +0x220, GhostStep +0x224, Ghost +0x22C, LastTime +0x238: the stock stores them in this order)
void NCStatusBarCtrl::StartAnim(INT Value, INT Max, DOUBLE Time)
{
    GhostScale = (DOUBLE)Value - (DOUBLE)(Max / 2) * 0.666;
    // the stock reads GhostScale back from the object (a double rounded from the x87 register, 53 bits), the optimizer would keep the
    // register's 64 bits: the volatile read makes the quotient the stock's to the last bit (t_statusbar.py: 73 of 400 cases were 1 ulp off)
    Ghost = *(volatile DOUBLE*)&GhostScale / ((DOUBLE)Max * 0.666) * Time;
    GhostStep = Time;
    Rate = Max;
    LastTime = InlineAppSeconds();
}
NW_HOOK_METHOD(0x1010e3e0, NCStatusBarCtrl, StartAnim)

// 0x1010e4a0 (name ours) - while a strip is left, it loses the time since the last call (the amount by the step per
// unit of Rate, 1.5 times as fast); once either is spent both are cleared. The time is stamped in any case
void NCStatusBarCtrl::UpdateTrail()
{
    if (GhostScale > 0.0 && Ghost >= 0.0)
    {
        DOUBLE Now = InlineAppSeconds();   // the stock calls its out-of-line copy (0x1001E860, NCoreMisc.cpp): the same expression
        DOUBLE Elapsed = Now - LastTime;
        GhostScale = GhostScale - Elapsed;
        Ghost = Ghost - GhostStep / (DOUBLE)Rate * Elapsed * 1.5015015015015014;
        if (GhostScale <= 0.0 || Ghost < 0.0)
        {
            GhostScale = 0.0;
            Ghost = 0.0;
        }
        LastTime = Now;
    }
}
NW_HOOK_METHOD(0x1010e4a0, NCStatusBarCtrl, UpdateTrail)

// 0x10112bb0 (ret 8; name ours) - the status window's new current and maximum: the low overlay shows while the current
// is below 3/5 of the maximum, the strip starts when the current changed, and at 0 or less it is cleared
void NCStatusBarCtrl::SetValues(INT InCur, INT InMax)
{
    bOverlay = InCur < InMax / 5 * 3;
    if (InCur != Cur)
        UpdateTrail();
    Cur = InCur;
    MaxValue = InMax;
    if (InCur <= 0)
        ClearTrail();
}
NW_HOOK_METHOD(0x10112bb0, NCStatusBarCtrl, SetValues)

// 0x1010d070 (vslot 36) - timer 0xFFF001: the ghost strip loses its step and, spent, its timer is killed; timer 0xFFF002: Cur
// moves to Target (a hundredfold step of the distance: it lands at once, the check below clamps it) and, once
// there, its timer is killed and the bar is done
INT NCStatusBarCtrl::OnTimer(INT TimerID)
{
    if (TimerID == NTIMER_STATUSBAR_GHOST)
    {
        GhostScale = GhostScale - (DOUBLE)Rate * 0.666;
        Ghost = Ghost - GhostStep;
        if (Ghost <= 0.0)
        {
            KillTimer(GhostTimer);
            GhostTimer = 0;
            Ghost = 0.0;
        }
    }
    else if (TimerID == NTIMER_STATUSBAR_VALUE)
    {
        INT To = Target;
        if (bReached == 0)
        {
            INT From = Cur;
            if (bGrow != 0)
                Cur = (INT)((DOUBLE)(To - From) * 100.0 + (DOUBLE)From);
            else
                Cur = (INT)((DOUBLE)From - (DOUBLE)(From - To) * 100.0);
        }
        else
            Cur = To;
        if (bGrow != 0)
        {
            if (Cur < To)
                return 1;
        }
        else
        {
            if (Cur > To)
                return 1;
        }
        Cur = To;
        KillTimer(ValueTimer);
        ValueTimer = 0;
        bReached = 1;
    }
    return 1;
}
NW_HOOK_METHOD(0x1010d070, NCStatusBarCtrl, OnTimer)

// 0x1010e070 (vslot 56) - the back, the fill (Cur / Max of the width), the ghost strip after it (half strength), the
// overlay over the fill; then the label and "Cur / Max" centered (the slash in the middle, 6 pixels of space on each
// side of it less 2)
INT NCStatusBarCtrl::OnPaint(NCGDevice* Device)
{
    guard(NCStatusBarCtrl::OnPaint);
    TCHAR Buffer[132];
    SIZE Size;
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    Device->BeginPaint(ScreenX, ScreenY);
    Device->DrawTexture(0, 0, (INT)Width, (INT)Height, 0, 0, TexWidth, TexHeight, BackTex, 0xFF, 1);
    INT FillW = 0;
    if (Cur != 0)
        FillW = (INT)((DOUBLE)Cur * Width / MaxValue);
    Device->DrawTexture(0, 0, FillW, (INT)Height, 0, 0, TexWidth, TexHeight, FillTex, 0xFF, 1);
    if (bGhost)
    {
        INT GhostW = 0;
        if (Ghost != 0.0)
            GhostW = (INT)(Width * Ghost / MaxValue);
        Device->DrawTexture(FillW, 0, GhostW, (INT)Height, 0, 0, TexWidth, TexHeight, GhostTex, 0x80, 1);
    }
    if (bOverlay)
        Device->DrawTexture(0, 0, FillW, (INT)Height, 0, 0, TexWidth, TexHeight, OverlayTex, 0xFF, 1);
    DrawDigits(Device, 1, 9, 2, 0xFFDCDCDC, (TCHAR*)*Text);
    appSprintf(Buffer, TEXT("%s"), TEXT("/"));
    NCGDevice::GetTextExtent(&Size, Buffer, 0, 0, 0, 0, 0);
    DrawDigits(Device, 1, (INT)(Width * 0.5f - Size.cx / 2), 2, 0xFFDCDCDC, Buffer);
    appSprintf(Buffer, TEXT("%d"), Cur);
    NCGDevice::GetTextExtent(&Size, Buffer, 0, 0, 0, 0, 0);
    DrawDigits(Device, 1, (INT)(Width * 0.5f - Size.cx - 6.0f - 2.0f), 2, 0xFFDCDCDC, Buffer);
    appSprintf(Buffer, TEXT("%d"), MaxValue);
    NCGDevice::GetTextExtent(&Size, Buffer, 0, 0, 0, 0, 0);
    DrawDigits(Device, 1, (INT)(Width * 0.5f + 6.0f - 2.0f), 2, 0xFFDCDCDC, Buffer);
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1010e070, NCStatusBarCtrl, OnPaint)

NW_CHECK_VTABLE(NCStatusBarCtrl, 0x101cdb10, 75)
