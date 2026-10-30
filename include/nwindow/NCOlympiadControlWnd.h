#pragma once
// NCOlympiadControlWnd - the olympiad observer's control bar (UClass "COlympiadControlWnd", 0xF4; C4 only): a title bar and two buttons
// on a background piece, shown while the player watches an olympiad match: "stop observing" and "other game" (the list of matches).
// The desktop makes it (NCConsole::OnCreate) and the console shows it.
//
// Layout and virtuals from the stock constructor 0x1005FBA0, OnCreate 0x1013CBF0, OnDestroy 0x1013CA20, OnPaint 0x1013CAF0.
// The db calls 0x1013CBF0 "NCManorInfoSettingWnd::OnCreate" because the original code copied that window's guard string (the
// manor lane lists it as its own: names.csv gives it the right name).

#include "nwindow/NCWnd.h"
#pragma pack(push, 4)

class NCButton;

class NCOlympiadControlWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCOlympiadControlWnd, NCWnd, 0, 0x10319AD8)

    UTexture* BackTex;               // +0xE8 "L2UI_CH3.Olympiad.olympiad_back" (drawn from x = 12, 12 less wide than the window)
    NCButton* StopObservingButton;   // +0xEC id 0xAADDCEB0 at (19, 5), SysString 611
    NCButton* OtherGameButton;       // +0xF0 id 0xAADDCEB1 at (19, 29), SysString 1252

    NCOlympiadControlWnd();                                          // 0x1005FBA0
    // ~NCOlympiadControlWnd                                         // 0x1005FBF0

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x1013C9B0
    virtual INT OnCreate();                                          // 37 0x1013CBF0
    virtual INT OnDestroy();                                         // 38 0x1013CA20
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x1013CAF0

    // names from the stock guard strings
    INT OnStopObservingBtnClick();                                   // 0x1013C8B0 the console's RequestOlympiadObserverEnd
    INT OnOtherGameBtnClick();                                       // 0x1013C930 the console's RequestOlympiadMatchList
};
NW_ASSERT_OFFSET(NCOlympiadControlWnd, BackTex, 0xE8);
NW_ASSERT_OFFSET(NCOlympiadControlWnd, OtherGameButton, 0xF0);
NW_ASSERT_SIZE(NCOlympiadControlWnd, 0xF4);

#pragma pack(pop)
