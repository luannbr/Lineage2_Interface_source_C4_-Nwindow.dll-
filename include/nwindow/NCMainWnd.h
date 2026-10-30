#pragma once
// NCMainWnd - the character window (UClass "CMainWnd", 0xAC; Alt+T, Alt+K, Alt+C, Alt+U): four
// pages under a row of tab buttons - status (NCDetailStatusWnd), skills (NCMagicSkillWnd),
// actions (NCActionWnd) and quests (NCQuestWnd) - and the title of the page shown. Minimized,
// its icon is the page's.
//
// Desktop (NCConsole) +0x94 is this window.

#include "nwindow/NCWnd.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCDetailStatusWnd;
class NCActionWnd;
class NCTabControl;

// NWM_COMMAND ids (name ours)
enum
{
    NID_MAIN_FRAME = 0xF2A6,
    NID_MAIN_TABS = 0xF2A7,           // lParam: the tab clicked
};

// the pages, in tab order (names ours)
enum
{
    MAINTAB_Status = 0,
    MAINTAB_Skills = 1,
    MAINTAB_Actions = 2,
    MAINTAB_Pledge = 3,             // new in C3 (NCPledgeWnd, SysString 439)
    MAINTAB_Quests = 4,
};

class NCMainWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCMainWnd, NCWnd, 0, 0x102D1C70)

    NCDetailStatusWnd* DetailWnd;    // +0xE8 tab 0
    NCActionWnd* ActionWnd;          // +0xEC tab 2
    NCWnd* PledgeWnd;                // +0xF0 tab 3 NCPledgeWnd (new in C3)
    NCWnd* SkillWnd;                 // +0xF4 tab 1 NCMagicSkillWnd
    NCWnd* QuestWnd;                 // +0xF8 tab 4 NCQuestGaraTreeWnd (C3; starts hidden)
    NCTabControl* TabCtrl;           // +0xFC
    UTexture* BackTex;               // +0x100 "L2UI.NWindow.mainwndback" (behind the tabs)

    NCMainWnd();                                                     // 0x1005fe50
    // ~NCMainWnd                                                    // 0x1004c830 (C1 only: no C2 match) (C1 only: no C2 match)

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x1005c370
    virtual INT OnCreate();                                          // 37 0x1005d8a0
    virtual INT OnDestroy();                                         // 38 0x1005d030
    virtual void ShowWindow();                                       // 48 0x100649f0
    virtual void Minimize();                                         // 54 0x1005d3c0
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x1005d320
    virtual INT OnTabChanged(INT Tab);                               // 74 0x1005c3d0 (name ours)

    // names ours
    void ShowTab(INT Tab);                                           // 0x1005d1b0 (shortcut keys)
    // 0x100565A0 (anonymous in the db): the clan page's tab button enabled again and the current page shown again (the
    // console calls it when the clan information arrives or goes); the window hides itself when it was not shown
    void UpdatePledgeTab();
    virtual void HideWindow();  // 49 0x100471a0 (the stock vtable of this class overrides it; defined in NNewVirtuals.gen.cpp)
};
NW_ASSERT_OFFSET(NCMainWnd, DetailWnd, 0xE8);
NW_ASSERT_OFFSET(NCMainWnd, TabCtrl, 0xFC);
NW_ASSERT_SIZE(NCMainWnd, 0x104);

#pragma pack(pop)