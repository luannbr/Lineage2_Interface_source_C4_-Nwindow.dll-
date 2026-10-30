#pragma once
// NCRadarWnd - the radar (UClass "CRadarWnd", 0xA0): the canvas draws the radar itself
// (NCGDevice::DrawRadar) and a mark per target. The console fills Targets by calling
// NCMinimapWnd::AddTarget / RemoveTarget on this window (the same array, at the same place),
// as it does for the map window.
//
// Desktop (NCConsole) +0x10C is the radar window.

#include "nwindow/NCMinimapWnd.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCRadarWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCRadarWnd, NCWnd, 0, 0x102FB1B8)

    TArray<FMinimapTarget> Targets;  // +0xE8 world coordinates

    NCRadarWnd();                                                    // 0x100ef9f0
    // ~NCRadarWnd                                                   // 0x100efa50

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10148f10 (folded)
    virtual INT OnCreate();                                          // 37 0x100ef780
    virtual INT OnDestroy();                                         // 38 0x100ef890
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x100ef790

    __declspec(noinline) void ClearTargets();                        // 0x1009a540 (C1 only: no C2 match) (C1 only: no C2 match) (C1 0x100a1d70) (name ours)
    // the map window's, folded (the console calls them on both windows)
    void AddTarget(INT X, INT Y, INT Z);                             // 0x10079230 (C1 only: no C2 match) (C1 only: no C2 match) (C1 only: no C2 match)
    void RemoveTarget(INT X, INT Y, INT Z);                          // 0x1009a4d0 (C1 only: no C2 match) (C1 only: no C2 match)
};
NW_ASSERT_OFFSET(NCRadarWnd, Targets, 0xE8);
NW_ASSERT_SIZE(NCRadarWnd, 0xF4);

#pragma pack(pop)