#pragma once
// NCReplayLogoWnd - the logos of the replay screens (UClass "CReplayLogoWnd", 0xF0; new in C4): the replay logo (256 x 64, the upper
// left corner) and the small logo (232 x 120, the lower right one) of the country's localization.ini, drawn at a scale that follows the
// screen (the larger of the height / 768 and the width / 1024, times 0.7 and 0.8). The C3 replay play window drew them itself.
//
// Layout and virtuals from the stock constructor 0x100F8990, OnCreate 0x100F8610, OnDestroy 0x100F8750 and OnPaint 0x100F87D0 (the
// db names that one NCLoginWnd::OnPaint, after the guard string the stock copied from the login screen).

#include "nwindow/NCWnd.h"
#pragma pack(push, 4)

class NCReplayLogoWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCReplayLogoWnd, NCWnd, 0, 0x103004D8)

    UTexture* LogoTex;               // +0xE8 (names ours) "<country>_ReplayLogoTexture" of localization.ini (L2Font.replay_logo-k)
    UTexture* MiniLogoTex;           // +0xEC "<country>_MiniLogoTexture" (L2Font.start_minilogo-k)

    NCReplayLogoWnd();                                               // 0x100f8990
    // ~NCReplayLogoWnd                                              // 0x100f89e0

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10148f10 (NControl's body, folded)
    virtual INT OnCreate();                                          // 37 0x100f8610
    virtual INT OnDestroy();                                         // 38 0x100f8750
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x100f87d0
};
NW_ASSERT_OFFSET(NCReplayLogoWnd, LogoTex, 0xE8);
NW_ASSERT_OFFSET(NCReplayLogoWnd, MiniLogoTex, 0xEC);
NW_ASSERT_SIZE(NCReplayLogoWnd, 0xF0);

#pragma pack(pop)
