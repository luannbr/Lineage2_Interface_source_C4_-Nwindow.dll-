#pragma once
// NCReplayWnd - the replay screen (UClass "CReplayWnd", 0xD4; C3 only): the screen-sized window the console keeps at
// +0x39E0 while a replay (GL2ReplayMode) or the benchmark runs. It owns three screens and shows the ones that fit the
// replay manager's state (FL2ReplayManager::GetState): the menu (NCReplayMenuWnd: the replay's own menu, with the
// function window of four buttons), the playing screen (NCReplayPlayWnd: the logos over the replay) and the
// benchmark's result (NCBenchMarkResultWnd). Layout and virtuals from the stock constructor 0x100E7720, OnCreate
// 0x100E7BF0 and the small methods 0x100E6240 .. 0x100E68F0.

#include "nwindow/NCWnd.h"
#pragma pack(push, 4)

class NCReplayMenuWnd;
class NCReplayPlayWnd;
class NCBenchMarkResultWnd;

class NCReplayWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCReplayWnd, NCWnd, 0, 0x103019A8)

    NCReplayMenuWnd* MenuWnd;           // +0xE8 (name ours) the size of this window, hidden until the state asks
    NCReplayPlayWnd* PlayWnd;           // +0xEC (name ours) style 0x400A: not hit tested
    NCBenchMarkResultWnd* BenchWnd;     // +0xF0 (name ours) the benchmark's result page

    NCReplayWnd();                                                   // 0x100f9e80
    // ~NCReplayWnd                                                  // 0x100f9ed0

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10148f10 (NControl's body, folded)
    virtual INT OnCreate();                                          // 37 0x100fa360
    virtual INT OnDestroy();                                         // 38 0x100f9120
    virtual void SetWindowSize(FLOAT W, FLOAT H);                    // 47 0x100f8a40 the three screens follow
    virtual void ShowWindow();                                       // 48 0x100f8af0 the screens of the manager's state
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x100f76f0 (folded: returns 1)
    virtual void HideWindow();  // 49 0x100471a0 (the stock vtable of this class overrides it; defined in NNewVirtuals.gen.cpp)
};
NW_ASSERT_OFFSET(NCReplayWnd, MenuWnd, 0xE8);
NW_ASSERT_OFFSET(NCReplayWnd, BenchWnd, 0xF0);
NW_ASSERT_SIZE(NCReplayWnd, 0xF4);

#pragma pack(pop)
