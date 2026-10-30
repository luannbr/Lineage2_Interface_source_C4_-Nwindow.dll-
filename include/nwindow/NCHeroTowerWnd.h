#pragma once
// NCHeroTowerWnd - the hero tower's window (UClass "CHeroTowerWnd", 0xFC; C4 only): a title bar, a list of the heroes (name, class,
// pledge, alliance, wins), a text box and a button for the hero's words, and a button that opens the selected hero's diary page.
// The desktop makes it (NCConsole::OnCreate); the server's hero list fills it (the console's slot calls OnHeroList).
//
// Layout and virtuals from the stock constructor 0x1005FA60, OnCreate 0x1013C2C0, OnDestroy 0x1013BF80, OnPaint 0x1013C0B0 and the
// methods.

#include "nwindow/NCWnd.h"
#pragma pack(push, 4)

class NCButton;
class NCEditBox;
class NCListCtrl;

class NCHeroTowerWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCHeroTowerWnd, NCWnd, 0, 0x103195A0)

    UTexture* BackTex;               // +0xE8 "L2UI_CH3.HeroTower.herotower1_back" (drawn from y = 20, under the title bar)
    NCListCtrl* HeroList;            // +0xEC id 0xDFDDCEB0, five columns: name, class, pledge, alliance, wins (names ours)
    NCEditBox* WordsEdit;            // +0xF0 the hero's words
    NCButton* WordsButton;           // +0xF4 id 0xDFDDCED0, SysString 1192
    NCButton* ContentButton;         // +0xF8 id 0xDFDDCEE0, SysString 1193

    NCHeroTowerWnd();                                                // 0x1005FA60
    // ~NCHeroTowerWnd                                               // 0x1005FAB0

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x1013C230
    virtual INT OnCreate();                                          // 37 0x1013C2C0
    virtual INT OnDestroy();                                         // 38 0x1013BF80
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x1013C0B0

    // names from the stock guard strings
    INT OnHeroWordsBtnClick();                                       // 0x1013BAF0 the words go to the server and the box empties
    INT OnShowContentBtnClick();                                     // 0x1013BBD0 the selected hero's diary page ("_diary?class=<id>&page=1")
    void OnHeroList(L2ParamStack& Params);                           // 0x1013BCB0 the list the server sends
    INT OnListCtrl(INT Row);                                         // 0x1013C1B0 a double click: the same as the diary button
};
NW_ASSERT_OFFSET(NCHeroTowerWnd, BackTex, 0xE8);
NW_ASSERT_OFFSET(NCHeroTowerWnd, WordsEdit, 0xF0);
NW_ASSERT_OFFSET(NCHeroTowerWnd, ContentButton, 0xF8);
NW_ASSERT_SIZE(NCHeroTowerWnd, 0xFC);

#pragma pack(pop)
