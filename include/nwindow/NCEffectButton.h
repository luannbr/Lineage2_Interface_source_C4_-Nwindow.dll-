#pragma once
// NCEffectButton - the glowing button of the desktop's left corner (UClass "CEffectButton", 0x2E4; C4 only): an NCButton that blinks
// its "over" face every half second, and once started (StartTimers, which NConsoleWnd's tutorial code reaches) grows and fades a
// glow around it. Its click hands its Param on: the tutorial's question mark (Kind 0), the quest window (Kind 1: the quest is selected
// in the quest tree) or the mail board (Kind 2: tab 5), and hides the button. The desktop makes three: "TutorialBtn" (+0x17C),
// "ConsoleBtn.QuestBtn" (+0x180) and "ConsoleBtn.MailBtn" (+0x184).
//
// Layout and virtuals from the stock constructor 0x10003B50, OnCreate 0x100020C0, OnTimer 0x10002120, OnPaint 0x10002700 and the
// click 0x10002830 (the db calls it OnLButtonDown; it is slot 69, OnLButtonUp); the timers' start is StartTimers / SetQuestion
// (0x10002210 / 0x10002280, hooked in NConsoleWndEvents.cpp on a stand-in struct).

#include "nwindow/NCButton.h"
#pragma pack(push, 4)

class NCEffectButton : public NCButton
{
    NW_DECLARE_CLASS(NCEffectButton, NCButton, 0, 0x102B6D50)

    INT BlinkInterval;               // +0x2B4 the blink timer's period in ms (500; the constructor)
    INT BlinkTimer;                  // +0x2B8 SetTimer's result for the blink timer (StartTimers)
    UBOOL bBlinkOn;                  // +0x2BC OnTimer(NTIMER_EFFECT_BLINK) flips it; OnPaint draws the over face while it is set
    INT Kind;                        // +0x2C0 0 the tutorial's question mark, 1 the quest window, 2 the mail board (names ours)
    INT Param;                       // +0x2C4 the question or the quest the click passes on
    UTexture* GlowTex[2];            // +0x2C8 "l2ui_ch3.etc.glowbtn1" (128 x 128 around the button), "glowbtn2" (the growing ring)
    INT FadeTimer;                   // +0x2D0 the 10 ms timer of the glow's alpha (StartTimers)
    INT GrowTimer;                   // +0x2D4 the 50 ms timer of the ring's size
    INT Alpha;                       // +0x2D8 0 .. 255, the glow's
    INT RingSize;                    // +0x2DC the ring's size in percent of 128 (50 at the start, +5 per tick)
    BYTE bFadeIn;                    // +0x2E0 the alpha goes up (set by StartTimers; the fade out ends the timers)
    BYTE bGlowing;                   // +0x2E1 the glow is drawn (StartTimers sets it, the end of the fade out clears it)

    NCEffectButton();                                                // 0x10003B50
    // 0x10003C00 (ret 0x18; anonymous in the db, NCConsole::OnCreate makes the three buttons with it): the kind, the three texture names
    // (normal, pushed, over: NCButton's 0x10003410) and the blink period; the last argument is NCButton's Flags (0)
    NCEffectButton(INT InKind, const TCHAR* NormalTexName, const TCHAR* PushedTexName, const TCHAR* OverTexName, INT InBlinkInterval,
                   INT InFlags);
    // ~NCEffectButton                                               // 0x10003BB0

    virtual INT OnTimer(INT TimerID);                                // 36 0x10002120
    virtual INT OnCreate();                                          // 37 0x100020C0
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x10002700
    virtual INT OnLButtonUp(WPARAM wParam, LPARAM lParam);           // 69 0x10002830
};
NW_ASSERT_OFFSET(NCEffectButton, BlinkInterval, 0x2B4);
NW_ASSERT_OFFSET(NCEffectButton, Kind, 0x2C0);
NW_ASSERT_OFFSET(NCEffectButton, GlowTex, 0x2C8);
NW_ASSERT_OFFSET(NCEffectButton, Alpha, 0x2D8);
NW_ASSERT_OFFSET(NCEffectButton, bFadeIn, 0x2E0);
NW_ASSERT_SIZE(NCEffectButton, 0x2E4);

#pragma pack(pop)
