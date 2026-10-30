#pragma once
// NCManorInfoSettingWnd - the base of the lord's tables (new in C4; UClass "CManorInfoSettingWnd", 0xF0): a title bar, the
// back picture and a list. NCManorCropInfoSettingWnd, NCManorSeedInfoSettingWnd and NCManorCropSellWnd derive from it in
// the stock code (they run its OnCreate / OnPaint / OnDestroy / WindowProc first); their headers keep NCWnd as the base and
// declare the two inherited fields, so the shared bodies are called by address there. Nothing creates this class itself.

#include "nwindow/NManorC4.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

// NWM_COMMAND id of the list (names ours); the title bar gets 0xD0000001
enum
{
    NID_MANORINFOSETTING_FRAME = (INT)0xD0000001,
    NID_MANORINFOSETTING_LIST = (INT)0xD0000011,
};

class NCManorInfoSettingWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCManorInfoSettingWnd, NCWnd, 0, 0x10317118)

    NCListCtrl* ListCtrl;            // +0xE8 id 0xD0000011, at (7, 73), 586 x 199, up to 1000 rows
    UTexture* BackTex;               // +0xEC "L2UI_CH3.Manor.manor2_back" (drawn from y = 20)

    NCManorInfoSettingWnd();                                         // 0x1005f550
    // ~NCManorInfoSettingWnd                                        // 0x1005f4d0

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10136ad0
    virtual INT OnCreate();                                          // 37 0x10136ce0
    virtual INT OnDestroy();                                         // 38 0x10136b30
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x10136be0
    // name ours: the list reports a double click on a row (the answer to NWM_COMMAND from the list); the base does nothing
    virtual INT OnListCtrl(INT Row);                                 // 74 0x100f76f0 (shared stub, ret 4)
};
NW_ASSERT_OFFSET(NCManorInfoSettingWnd, ListCtrl, 0xE8);
NW_ASSERT_OFFSET(NCManorInfoSettingWnd, BackTex, 0xEC);
NW_ASSERT_SIZE(NCManorInfoSettingWnd, 0xF0);

#pragma pack(pop)
