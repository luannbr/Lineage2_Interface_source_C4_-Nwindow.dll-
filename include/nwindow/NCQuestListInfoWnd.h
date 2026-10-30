#pragma once
// NCQuestListInfoWnd - the list of the quests the player can still take (UClass "CQuestListInfoWnd", 0x190; new in C4).
// A 586 x 237 list (NCListCtrl, five columns: name, region, level, type icon, NPC) under a 20 pixel title bar on the
// "questinfo_back" panel, a "?" icon at (578, 30) that shows a two line tool tip, and a button that
// marks the place of the selected quest's NPC: the tab of the main window that tracks a quest gets the NPC's name and
// position and the map shows the mark. The rows are the first step of every quest of the console's quest table
// (GL2GameData +0x3D28, 2000 records of 0xA0 bytes; FillList). Layout from OnCreate (0x100EC600) and the other bodies.

#include "nwindow/NCWnd.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCListCtrl;
class NCButton;

// NWM_COMMAND ids of the children (names ours)
enum
{
    NID_QUESTLIST_MOVE = 0xF00A0,    // the button
    NID_QUESTLIST_LIST = 0xF00A1,    // the list
};

class NCQuestListInfoWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCQuestListInfoWnd, NCWnd, 0, 0x102F8D50)

    NCListCtrl* ListCtrl;            // +0xE8 id 0xF00A1, at (7, 49), 586 x 237, 2000 rows of 17, 13 visible
    UTexture* BackTex;               // +0xEC "L2UI_CH3.QUESTWND.questinfo_back"
    NCButton* MoveButton;            // +0xF0 id 0xF00A0, text 0x35C, at (518, 297), 76 x 23 (name ours)
    UTexture* ToolTipBtnTex;         // +0xF4 "L2UI_CH3.QUESTWND.QuestWndToolTipBtn" the "?" icon
    UTexture* Icons[5];              // +0xF8 "L2UI_CH3.QUESTWND.QuestWndInfoIcon_1" .. 5 (the type column uses the first two)
    UBOOL bHover;                    // +0x10C the mouse is over the "?" icon (name ours)
    NToolTipInfo ToolTip;            // +0x110 the "?" icon's two lines with their icons (OnCreate)

    NCQuestListInfoWnd();                                            // 0x100ed9b0
    // ~NCQuestListInfoWnd                                           // 0x100eda40

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x100ea250
    virtual INT OnCreate();                                          // 37 0x100ec600
    virtual INT OnDestroy();                                         // 38 0x100eb960
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x100eb9d0
    virtual INT OnDrawToolTip(NCGDevice* Device);                    // 63 0x100ecfb0 (the db names it OnPostPaint)
    virtual INT OnMouseMove(WPARAM wParam, LPARAM lParam);           // 67 0x100ebb00
    // names ours
    virtual INT OnMoveBtnClick();                                    // 74 0x100ec140 (the db: vfn074) the selected quest's NPC is tracked

    // 0x100ea2b0 (anonymous in the db, name ours): one row for the first step of every quest in the table
    void FillList();
};
NW_ASSERT_OFFSET(NCQuestListInfoWnd, ListCtrl, 0xE8);
NW_ASSERT_OFFSET(NCQuestListInfoWnd, BackTex, 0xEC);
NW_ASSERT_OFFSET(NCQuestListInfoWnd, MoveButton, 0xF0);
NW_ASSERT_OFFSET(NCQuestListInfoWnd, ToolTipBtnTex, 0xF4);
NW_ASSERT_OFFSET(NCQuestListInfoWnd, Icons, 0xF8);
NW_ASSERT_OFFSET(NCQuestListInfoWnd, bHover, 0x10C);
NW_ASSERT_OFFSET(NCQuestListInfoWnd, ToolTip, 0x110);
NW_ASSERT_SIZE(NCQuestListInfoWnd, 0x190);

#pragma pack(pop)
