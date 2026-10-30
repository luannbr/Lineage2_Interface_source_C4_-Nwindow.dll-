#pragma once
// NCSSQMainEventWnd - the Seven Signs event list tab (UClass "CSSQMainEventWnd", 0xD4; new in C3): the tree of the
// events (NCSSQMainEventGaraTreeCtrl, 240 x 304) on the "ssq_back2" panel and a button that asks the console for
// the list again.

#include "nwindow/NCWnd.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCButton;
class NCSSQMainEventGaraTreeCtrl;

class NCSSQMainEventWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCSSQMainEventWnd, NCWnd, 0, 0x103033A0)

    UTexture* BackTex;               // +0xE8 "L2UI_CH3.SSQWnd.ssq_back2"
    NCSSQMainEventGaraTreeCtrl* TreeCtrl;   // +0xEC (8, 11, 240 x 304) (name ours)
    NCButton* RefreshButton;         // +0xF0 id 0xFF00F0A1, text 0x3AA (name ours)

    NCSSQMainEventWnd();                                             // 0x100fe160
    // ~NCSSQMainEventWnd                                            // 0x100fe1b0

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x100fcbd0
    virtual INT OnCreate();                                          // 37 0x100fe200
    virtual INT OnDestroy();                                         // 38 0x100fde00
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x100fde70
    virtual INT OnRefresh();                                         // 74 0x100fcc40 (name ours) asks for the list

    void ClearList();                                                // 0x100fcc30 (name ours) the tree emptied
};
NW_ASSERT_OFFSET(NCSSQMainEventWnd, TreeCtrl, 0xEC);
NW_ASSERT_OFFSET(NCSSQMainEventWnd, RefreshButton, 0xF0);
NW_ASSERT_SIZE(NCSSQMainEventWnd, 0xF4);

#pragma pack(pop)
