#pragma once
// NCGMSnoopManager - the GM's snoop windows (UClass "CGMSnoopManager", 0xA8; C2 only;
// NCConsole makes it): four NCGMSnoopWnd, one per conversation listened to, all hidden until
// the console shows one of them.

#include "nwindow/NCWnd.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCGMSnoopWnd;

class NCGMSnoopManager : public NCWnd
{
    NW_DECLARE_CLASS(NCGMSnoopManager, NCWnd, 0, 0x102F5958)

    NCGMSnoopWnd* Snoops[4];         // +0xE8 (name ours)

    NCGMSnoopManager();                                              // 0x1005f2d0
    // ~NCGMSnoopManager                                             // 0x1004c6f0 (C1 only: no C2 match) (C1 only: no C2 match)

    virtual INT OnCreate();                                          // 37 0x100e3030
    virtual INT OnDestroy();                                         // 38 0x100e08a0
    virtual void ShowWindow();                                       // 48 0x100e00a0

    // names ours (the console's Snoop calls them)
    void AddLine(INT SnoopID, const TCHAR* Text, DWORD Color, INT Type, INT Unused); // 0x100e0960
    void OpenSnoop(INT SnoopID, const TCHAR* TargetName);            // 0x100e09c0
    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10148f10 (the stock vtable of this class overrides it; defined in NNewVirtuals.gen.cpp)
};
NW_ASSERT_OFFSET(NCGMSnoopManager, Snoops, 0xE8);
NW_ASSERT_SIZE(NCGMSnoopManager, 0xF8);

#pragma pack(pop)
