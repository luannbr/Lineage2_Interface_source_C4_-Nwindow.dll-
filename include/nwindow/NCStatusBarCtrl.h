#pragma once
// NCStatusBarCtrl - the gauge of the player's status window ("ps_hpbar": HP, MP, CP; UClass "CStatusBarCtrl", 0x244;
// new in C3, the C2 window drew its bars by hand): a back, a fill as wide as Cur / MaxValue of the control, a lighter
// "ghost" strip after it for the amount just lost (a timer shrinks it) and an overlay; the label at the left and
// "Cur / MaxValue" in the middle. Cur follows Target by timer 0xFFF002 (the status window sets Target and starts it),
// the ghost's width by timer 0xFFF001.
//
// Layout from the constructor 0x101024d0, OnPaint 0x100fb3a0 and OnTimer 0x100fa3a0 (names ours).

#include "nwindow/NControl.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

enum
{
    NTIMER_STATUSBAR_GHOST = 0xFFF001,   // the ghost strip shrinks (name ours)
    NTIMER_STATUSBAR_VALUE = 0xFFF002,   // Cur moves towards Target (name ours)
};

class NCStatusBarCtrl : public NControl
{
    NW_DECLARE_CLASS(NCStatusBarCtrl, NControl, 0, 0x1030AB40)

    UTexture* FillTex;               // +0x1F4 the fill
    UTexture* BackTex;               // +0x1F8 the empty gauge
    UTexture* GhostTex;              // +0x1FC drawn at half strength after the fill (the amount just lost)
    UTexture* OverlayTex;            // +0x200 drawn over the fill when bOverlay
    INT TexWidth;                    // +0x204 the textures' size (the source rectangle of every draw)
    INT TexHeight;                   // +0x208
    INT MaxValue;                    // +0x20C the full gauge
    INT Cur;                         // +0x210 what the fill shows
    INT Target;                      // +0x214 what Cur moves to
    DOUBLE GhostScale;               // +0x218 starts at the ghost's timer; OnTimer 0xFFF001 lowers it by Rate * 0.666
    INT Rate;                        // +0x220
    DOUBLE GhostStep;                // +0x224 taken off the ghost's width on every tick of the ghost's timer
    DOUBLE Ghost;                    // +0x22C the ghost strip's amount (0: none; the timer is killed when it is spent)
    INT Unknown214;                  // +0x234 (0) nothing here reads it
    DOUBLE LastTime;                 // +0x238 appSeconds of the last StartAnim / UpdateTrail (name ours)
    UBOOL bOverlay;                  // +0x240 SetValues: set while Cur is below 3/5 of the maximum (the low warning)
    UBOOL bGhost;                    // +0x244 the ghost strip is drawn
    UBOOL bGrow;                     // +0x248 Cur goes up to Target (else down)
    UBOOL bReached;                  // +0x24C Cur is Target (the value timer is done)
    INT GhostTimer;                  // +0x250 timer id of the ghost's, 0 none
    INT ValueTimer;                  // +0x254 timer id of the value's, 0 none
    FString Text;                    // +0x258 the label drawn at (9, 2)

    NCStatusBarCtrl();                                               // 0x101162e0
    // the status window's (ret 0x1c): the label, the textures' size, the fill and back textures, the ghost and the
    // overlay ones (or NULL for none)
    NCStatusBarCtrl(const TCHAR* InLabel, INT InTexW, INT InTexH, const TCHAR* FillName, const TCHAR* BackName,
                    const TCHAR* GhostName, const TCHAR* OverlayName); // 0x101025e0
    // ~NCStatusBarCtrl                                              // 0x10116380 (the label, then the control)

    // names ours
    void SetValues(INT InCur, INT InMax);                            // 0x10112bb0 (ret 8) the player status window's
    __declspec(noinline) void ClearTrail();                          // 0x1010d040 no strip left (out of line, as SetValues calls it)
    __declspec(noinline) void UpdateTrail();                         // 0x1010e4a0 the strip shortened by the time passed
    // 0x1010e3e0 (ret 0x10, name ours) the strip starts: the bar moves to Value of Max over Time seconds (the player status
    // window's HP bar, NConsoleWnd::StartHPBarAnim); a non-virtual member, out of line like the stock one
    __declspec(noinline) void StartAnim(INT Value, INT Max, DOUBLE Time);

    virtual INT OnTimer(INT TimerID);                                // 36 0x1010d070
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x1010e070
    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10021e60 (the stock vtable of this class overrides it; defined in NNewVirtuals.gen.cpp)
};
NW_ASSERT_OFFSET(NCStatusBarCtrl, FillTex, 0x1F4);
NW_ASSERT_OFFSET(NCStatusBarCtrl, MaxValue, 0x20C);
NW_ASSERT_OFFSET(NCStatusBarCtrl, GhostScale, 0x218);
NW_ASSERT_OFFSET(NCStatusBarCtrl, Rate, 0x220);
NW_ASSERT_OFFSET(NCStatusBarCtrl, GhostStep, 0x224);
NW_ASSERT_OFFSET(NCStatusBarCtrl, Ghost, 0x22C);
NW_ASSERT_OFFSET(NCStatusBarCtrl, LastTime, 0x238);
NW_ASSERT_OFFSET(NCStatusBarCtrl, bOverlay, 0x240);
NW_ASSERT_OFFSET(NCStatusBarCtrl, GhostTimer, 0x250);
NW_ASSERT_OFFSET(NCStatusBarCtrl, Text, 0x258);
NW_ASSERT_SIZE(NCStatusBarCtrl, 0x264);

#pragma pack(pop)
