#pragma once
// NCHeroTowerContentWnd - the hero tower's content window (UClass "CHeroTowerContentWnd", 0xF4; C4 only): a title bar, a background
// piece below it and two buttons at the bottom whose clicks do nothing in this client (WindowProc takes the two ids and answers 1).
// The desktop makes it (NCConsole::OnCreate).
//
// Layout and virtuals from the stock constructor 0x1005FB00, OnCreate 0x1013B830, OnDestroy 0x1013B660, OnPaint 0x1013B730.
// The db calls 0x1013B830 "NCManorInfoSettingWnd::OnCreate" (the original code copied that window's guard string; the manor lane
// lists it as its own: names.csv gives it the right name).

#include "nwindow/NCWnd.h"
#pragma pack(push, 4)

class NCButton;

class NCHeroTowerContentWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCHeroTowerContentWnd, NCWnd, 0, 0x10319068)

    UTexture* BackTex;               // +0xE8 "L2UI_CH3.HeroTower.herotower2_back" (drawn from y = 20, under the title bar)
    NCButton* FirstButton;           // +0xEC id 0xCFDDCEB0 at (78, 372) (names ours)
    NCButton* SecondButton;          // +0xF0 id 0xCFDDCEB1 at (158, 372)

    NCHeroTowerContentWnd();                                         // 0x1005FB00
    // ~NCHeroTowerContentWnd                                        // 0x1005FB50

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x1013B5F0
    virtual INT OnCreate();                                          // 37 0x1013B830
    virtual INT OnDestroy();                                         // 38 0x1013B660
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x1013B730

    // both buttons call one function of the stock image that answers 1 (0x100f7850, folded with the other empty handlers)
    __declspec(noinline) INT OnFirstBtnClick() { return 1; }
    __declspec(noinline) INT OnSecondBtnClick() { return 1; }
};
NW_ASSERT_OFFSET(NCHeroTowerContentWnd, BackTex, 0xE8);
NW_ASSERT_OFFSET(NCHeroTowerContentWnd, SecondButton, 0xF0);
NW_ASSERT_SIZE(NCHeroTowerContentWnd, 0xF4);

#pragma pack(pop)
