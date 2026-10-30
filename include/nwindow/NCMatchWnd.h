#pragma once
// NCMatchWnd - the party matching window (UClass "CMatchWnd", 0xF8): the rooms players have opened to look for a
// party (NCMatchListCtrl: number, title, type, level range, members), a page at a time. Two selection boxes
// (the place and the kind of room), buttons for the previous and the next page, a new search, a new room (the
// NCPartyRoomMakingWnd) and a quick join. The console fills it (SetList, ClearList); the buttons ask the server
// and stay disabled until a 5 second timer re-enables them.
//
// C3 layout from OnCreate (0x100a7ca0) and its users. The C2 window (invite / options / whisper / targeting) is
// gone with its slots.
//
// Desktop (NCConsole) +0x140 is this window.

#include "nwindow/NCWnd.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCMatchListCtrl;
class NCMatchFrameCtrl;
class NCComboBox;
class NCButton;

// NWM_COMMAND ids of the controls (names ours)
enum
{
    NID_MATCH_PREV = 0xF2,       // vslot 68
    NID_MATCH_NEXT = 0xF3,       // vslot 69
    NID_MATCH_SEARCH = 0xF4,     // vslot 70
    NID_MATCH_CREATE = 0xF5,     // vslot 71
    NID_MATCH_JOIN = 0xF6,       // vslot 72
    NID_MATCH_FRAME = 0xF7,
    NID_MATCH_AREA = 0xF8,       // the place selection box
    NID_MATCH_TYPE = 0xF9,       // the room kind selection box
};

// the window's timer: the buttons wake up again (SetTimer 5000)
enum { TIMER_MatchButtons = 0x1F8 };

class NCMatchWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCMatchWnd, NCWnd, 0, 0x102E4F70)

    UTexture* BackTex;               // +0xE8 "L2UI_CH3.PartyMatchWnd.PartyMatch1_Back"   // C2: offset derived, no evidence
    NCComboBox* AreaCombo;           // +0xEC (names ours) the place: -2, -1, then 1 .. 13 (the item values)
    NCComboBox* TypeCombo;           // +0xF0 the kind of room (two items)
    NCMatchListCtrl* MatchList;      // +0xF4 five columns
    NCButton* PrevButton;            // +0xF8 id 0xF2
    NCButton* NextButton;            // +0xFC id 0xF3
    NCButton* SearchButton;          // +0x100 id 0xF4
    NCButton* CreateButton;          // +0x104 id 0xF5   // C2: offset derived, no evidence
    NCButton* JoinButton;            // +0x108 id 0xF6
    INT TimerID;                     // +0x10C SetTimer(0x1F8, 5000)   // C2: offset derived, no evidence
    INT Page;                        // +0x110 the page the list shows (the console sets it with the list; 0 = none)
    NCMatchFrameCtrl* MatchFrame;    // +0x114 id 0xF7, the title bar (the window's own FrameCtrl is not used)   // C2: offset derived, no evidence

    NCMatchWnd();                                                    // 0x100b4530
    // ~NCMatchWnd                                                   // 0x100b4580

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x100b14f0
    virtual INT OnTimer(INT InTimerID);                              // 36 0x100b15d0
    virtual INT OnCreate();                                          // 37 0x100b45d0
    virtual INT OnDestroy();                                         // 38 0x100b21a0
    virtual void ShowWindow();                                       // 48 0x100b71f0 (folded)
    virtual void HideWindow();                                       // 49 0x10035ab0 (folded)
    virtual void Minimize();                                         // 54 0x100b1630
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x100b23a0
    // the buttons (names ours): each asks the console for a page of the list and disables four of the buttons
    virtual INT OnPrevPage();                                        // 74 0x100b26a0 (db: OnInvite)
    virtual INT OnNextPage();                                        // 75 0x100b27a0
    virtual INT OnSearch();                                          // 76 0x100b2890 page 1
    virtual INT OnCreateRoom();                                      // 77 0x100b2980 opens the room making window
    virtual INT OnJoin();                                            // 78 0x100b2a20 asks to join any room (id 0)

    // the console's calls (names ours)
    void SetList(L2ParamStack& Params);                              // 0x100b2520 (the console's slot 374)
    void ClearList();                                                // 0x100b1690 (the console's slot 375)
};
NW_ASSERT_OFFSET(NCMatchWnd, BackTex, 0xE8);
NW_ASSERT_OFFSET(NCMatchWnd, AreaCombo, 0xEC);
NW_ASSERT_OFFSET(NCMatchWnd, MatchList, 0xF4);
NW_ASSERT_OFFSET(NCMatchWnd, JoinButton, 0x108);
NW_ASSERT_OFFSET(NCMatchWnd, TimerID, 0x10C);
NW_ASSERT_OFFSET(NCMatchWnd, Page, 0x110);
NW_ASSERT_OFFSET(NCMatchWnd, MatchFrame, 0x114);
NW_ASSERT_SIZE(NCMatchWnd, 0x118);

#pragma pack(pop)
