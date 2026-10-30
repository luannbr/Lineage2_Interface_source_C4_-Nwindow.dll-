#pragma once
// NCReplayMenuWnd - the replay's menu screen (UClass "CReplayMenuWnd", 0xD8; C3 only; NCReplayWnd +0xC8): a
// full-screen backdrop with the function window (the four buttons) in the lower right corner.
// Layout and virtuals from the stock constructor 0x100E7880, OnCreate 0x100E7930, OnDestroy 0x100E6C10 and OnPaint
// 0x100E6CC0.

#include "nwindow/NCWnd.h"
#pragma pack(push, 4)

class NCReplayFunctionWnd;

class NCReplayMenuWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCReplayMenuWnd, NCWnd, 0, 0x10300F40)

    NCReplayFunctionWnd* FunctionWnd;   // +0xE8 (name ours) 120 x 138, 154 x 236 from the lower right corner
    INT UnknownCC;                      // +0xEC (name ours) cleared by the constructor, never used
    INT UnknownD0;                      // +0xF0 (name ours) the same
    UTexture* BackTex;                  // +0xF4 (name ours) "L2UI_CH3.BenchMarkWnd.BenchMark_Title" (1024 x 768 piece)

    NCReplayMenuWnd();                                               // 0x100f9fe0
    // ~NCReplayMenuWnd                                              // 0x100fa040

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10148f10 (folded)
    virtual INT OnCreate();                                          // 37 0x100fa090
    virtual INT OnDestroy();                                         // 38 0x100f9440
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x100f94f0
    virtual void ShowWindow();  // 48 0x100efc40 (the stock vtable of this class overrides it; defined in NNewVirtuals.gen.cpp)
    virtual void HideWindow();  // 49 0x100471a0 (the stock vtable of this class overrides it; defined in NNewVirtuals.gen.cpp)
};
NW_ASSERT_OFFSET(NCReplayMenuWnd, FunctionWnd, 0xE8);
NW_ASSERT_OFFSET(NCReplayMenuWnd, BackTex, 0xF4);
NW_ASSERT_SIZE(NCReplayMenuWnd, 0xF8);

#pragma pack(pop)
