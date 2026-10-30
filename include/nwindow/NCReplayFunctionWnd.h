#pragma once
// NCReplayFunctionWnd - four buttons one under the other over the lobby menu's backdrop (UClass "CReplayFunctionWnd",
// 0xDC; C3 only; NCReplayMenuWnd +0xC8), the same shape as NCLoginFunctionWnd. Their handlers are the virtual slots
// 68 .. 71 (names ours). Layout and virtuals from the stock constructor 0x100E77C0, OnCreate 0x100E7270, OnDestroy
// 0x100E69F0, OnPaint 0x100E6B20 and the handlers 0x100E64F0 .. 0x100E6630.

#include "nwindow/NCWnd.h"
#pragma pack(push, 4)

class NCButton;

class NCReplayFunctionWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCReplayFunctionWnd, NCWnd, 0, 0x10301478)

    UTexture* BackTex;                  // +0xE8 "L2UI_CH3.LobbyWnd.lobby_menuback" (120 x 138)
    NCButton* ReplayButton;             // +0xEC id 0xAFF22011, SysString 1023 (names ours)
    NCButton* BenchmarkButton;          // +0xF0 id 0xAFF22012, SysString 1022
    NCButton* OptionsButton;            // +0xF4 id 0xAFF22013, SysString 156
    NCButton* ExitButton;               // +0xF8 id 0xAFF22014, SysString 153

    NCReplayFunctionWnd();                                           // 0x100f9f20
    // ~NCReplayFunctionWnd                                          // 0x100f9f90

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x100f8d20 the ids go to the four handlers
    virtual INT OnCreate();                                          // 37 0x100f99a0
    virtual INT OnDestroy();                                         // 38 0x100f9220
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x100f9350
    virtual INT OnReplayBtnClick();                                  // 74 0x100f8dd0 (name ours) the replay starts (state 0)
    virtual INT OnBenchmarkBtnClick();                               // 75 0x100f8e00 (name ours) the benchmark starts
    virtual INT OnOptionsBtnClick();                                 // 76 0x100f8e60 (name ours) the options window
    virtual INT OnExitBtnClick();                                    // 77 0x1009e590 (name ours; folded with NCAuthWnd::OnCancel) the client ends
    virtual void ShowWindow();  // 48 0x100efc40 (the stock vtable of this class overrides it; defined in NNewVirtuals.gen.cpp)
    virtual void HideWindow();  // 49 0x100471a0 (the stock vtable of this class overrides it; defined in NNewVirtuals.gen.cpp)
};
NW_ASSERT_OFFSET(NCReplayFunctionWnd, BackTex, 0xE8);
NW_ASSERT_OFFSET(NCReplayFunctionWnd, ExitButton, 0xF8);
NW_ASSERT_SIZE(NCReplayFunctionWnd, 0xFC);

#pragma pack(pop)
