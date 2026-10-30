#pragma once
// NCManorInfoUpWnd - the tab bar of the manor information window (new in C4; UClass "CManorInfoUpWnd", 0xF0): three tabs
// (seed info, crop info, default info), each shown over the page NCManorInfoWnd made for it. A click asks the server for the
// page ("manor_menu_select?ask=3 / 4 / 5"). The window belongs to NCManorInfoWnd (its parent window), which keeps the three
// pages; GetManorInfoDownWnd finds them through it.

#include "nwindow/NManorC4.h"
#include "nwindow/NCTabControl.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

// the id of the tab control (name ours; the window procedure answers it)
enum
{
    NID_MANORINFOUP_TABS = (INT)0xFF000001,
};

class NCManorInfoUpWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCManorInfoUpWnd, NCWnd, 0, 0x10317650)

    NCTabControl* TabCtrl;           // +0xE8 id 0xFF000001, at (12, 5), the size of this window; three tabs of 94 x 23
    UTexture* BackTex;               // +0xEC "L2UI_CH3.Manor.manor1_back1" (OnPaint draws it over the whole window)

    NCManorInfoUpWnd();                                              // 0x101374f0
    // ~NCManorInfoUpWnd                                             // 0x10137540

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10137590
    virtual INT OnCreate();                                          // 37 0x101371f0
    virtual INT OnDestroy();                                         // 38 0x10136ef0
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x10136fa0

    // names ours
    NCWnd* GetManorInfoDownWnd(INT Index);                           // 0x10137160 (ret 4) the page of tab 0..2 (NCManorInfoWnd's)
    INT OnTabClick(INT Tab);                                         // 0x101373a0 (ret 4) asks for the page of the tab clicked
};
NW_ASSERT_OFFSET(NCManorInfoUpWnd, TabCtrl, 0xE8);
NW_ASSERT_OFFSET(NCManorInfoUpWnd, BackTex, 0xEC);
NW_ASSERT_SIZE(NCManorInfoUpWnd, 0xF0);

#pragma pack(pop)
