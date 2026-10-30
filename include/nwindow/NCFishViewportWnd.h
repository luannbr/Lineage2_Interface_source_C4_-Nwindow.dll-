#pragma once
// NCFishViewportWnd - the fishing window (UClass "CFishViewportWnd", 0x168, super NCViewportWnd; C4 only): the 3D view of the
// fishing (a viewport 252 x 254), the name of the fish and the damage dealt under it, a clock and the fish's life bar while a
// fight goes on, a flash while a skill's effect shows and a hint (the pumping or reeling skill's icon and text) blinking at the
// bottom. The server's fishing packets set the fields (the console); the window only paints them.
//
// Layout and virtuals from the stock constructor 0x10120D10, OnCreate 0x10120AF0, OnDestroy 0x1011FFA0 and OnPaint 0x101204A0 (the
// db spells the class of the paint "NCFishViewportwnd").

#include "nwindow/NCViewportWnd.h"
#pragma pack(push, 4)

class NCFishViewportWnd : public NCViewportWnd
{
    NW_DECLARE_CLASS(NCFishViewportWnd, NCViewportWnd, 0, 0x103103A0)

    UTexture* BackTex;               // +0xF8 "L2UI_CH3.FishingWnd.fishing_back" (256 x 256 from y = 20)
    UTexture* ClockTex;              // +0xFC "L2UI_CH3.FishingWnd.fishing_clockicon"
    UTexture* LifeTex;               // +0x100 "L2UI_CH3.FishingWnd.fishing_bar1" the fish's life left (names ours)
    UTexture* LifeBackTex;           // +0x104 "L2UI_CH3.FishingWnd.fishing_bar2" what is gone
    UTexture* EffectTex;             // +0x108 "L2UI_CH3.FishingWnd.fishing_effect"
    UTexture* HintTex[2];            // +0x10C "Icon.skill_i.skill1313", +0x110 "skill1314" the two hints' icons
    INT FishMaxLife;                 // +0x114 (the server's) the bar's full length
    INT FishLife;                    // +0x118
    INT Unknown11C;                  // +0x11C cleared with the rest
    INT TimeLeft;                    // +0x120 the clock's number
    INT bFighting;                   // +0x124 the clock and the life bar show
    INT EffectAlpha;                 // +0x128 the effect's alpha (the low byte)
    INT bEffect;                     // +0x12C the effect shows
    INT Unknown130;                  // +0x130
    INT Unknown134;                  // +0x134
    FString Message;                 // +0x138 the line under the picture, at x = 13
    INT Delta;                       // +0x144 the damage dealt: > 0 painted red, < 0 white, at the right edge
    FString DeltaText;               // +0x148 its text
    FLOAT Fade;                      // +0x154 the texts' alpha, 0 .. 1 (nothing painted at 0 or less)
    INT bMessageRed;                 // +0x158 the line is red, else white
    INT bHint;                       // +0x15C a hint is on
    FLOAT HintTimer;                 // +0x160 it blinks: shown for 0 .. 0.1, 0.2 .. 0.3 and from 0.4 on
    BYTE Unknown164;                 // +0x164
    BYTE bHintAlt;                   // +0x165 the second hint (icon, text)

    NCFishViewportWnd();                                             // 0x10120D10
    // ~NCFishViewportWnd                                            // 0x10120E30

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10021E60 (folded with NCButton's, stays stock)
    virtual INT OnCreate();                                          // 37 0x10120AF0
    virtual INT OnDestroy();                                         // 38 0x1011FFA0
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x101204A0
};
NW_ASSERT_OFFSET(NCFishViewportWnd, BackTex, 0xF8);
NW_ASSERT_OFFSET(NCFishViewportWnd, HintTex, 0x10C);
NW_ASSERT_OFFSET(NCFishViewportWnd, FishMaxLife, 0x114);
NW_ASSERT_OFFSET(NCFishViewportWnd, Message, 0x138);
NW_ASSERT_OFFSET(NCFishViewportWnd, Delta, 0x144);
NW_ASSERT_OFFSET(NCFishViewportWnd, DeltaText, 0x148);
NW_ASSERT_OFFSET(NCFishViewportWnd, Fade, 0x154);
NW_ASSERT_OFFSET(NCFishViewportWnd, HintTimer, 0x160);
NW_ASSERT_OFFSET(NCFishViewportWnd, bHintAlt, 0x165);
NW_ASSERT_SIZE(NCFishViewportWnd, 0x168);

#pragma pack(pop)
