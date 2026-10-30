#pragma once
// NCZoneTitleWnd - the zone name that fades in over the screen when the player enters a new zone (UClass
// "CZoneTitleWnd", 0xF8; new in C3: the desktop NCConsole creates it, NConsoleWnd sets the three lines and ticks it).
// Up to three lines of text centered one under the other, each drawn twice (a black shadow and the cream text,
// alpha 0xFF at start) over the "fontdeco" texture; Tick (0x1010B4A0) adds the frame time to Elapsed and fades the
// two alphas, past the end it empties the lines and hides the window.

#include "nwindow/NCWnd.h"
#pragma pack(push, 4)

class NCZoneTitleWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCZoneTitleWnd, NCWnd, 0, 0x10310E10)

    FString Line1;                   // +0xE8 (the constructor empties the three; 0x1010B320 sets them)
    FString Line2;                   // +0xF4
    FString Line3;                   // +0x100
    FLOAT Elapsed;                   // +0x10C seconds since the lines were set (Tick)
    BYTE TextAlpha;                  // +0x110 alpha of the cream text and of the deco texture (0xFF at start)
    BYTE ShadowAlpha;                // +0x111 alpha of the black shadow (0xFF at start)
    UTexture* DecoTex;               // +0x114 "L2ui_ch3.etc.fontdeco": 256 x 256 behind the lines at (-100, -80)

    NCZoneTitleWnd();                                                // 0x10121970
    // ~NCZoneTitleWnd                                               // 0x10121a70

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10148f10 (NControl's body, folded)
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x101215f0

    // names ours
    INT GetTitleWidth();                                             // 0x10121270 width of the longest line (by length)
};
NW_ASSERT_OFFSET(NCZoneTitleWnd, Line1, 0xE8);
NW_ASSERT_OFFSET(NCZoneTitleWnd, Line2, 0xF4);
NW_ASSERT_OFFSET(NCZoneTitleWnd, Line3, 0x100);
NW_ASSERT_OFFSET(NCZoneTitleWnd, Elapsed, 0x10C);
NW_ASSERT_OFFSET(NCZoneTitleWnd, TextAlpha, 0x110);
NW_ASSERT_OFFSET(NCZoneTitleWnd, DecoTex, 0x114);
NW_ASSERT_SIZE(NCZoneTitleWnd, 0x118);

#pragma pack(pop)
