#pragma once
// NCQuestGaraTreeWnd - the quest tab of the main window (UClass "CQuestGaraTreeWnd", 0x180; new in C3; NCMainWnd
// creates it as tab 4, starting hidden): the accepted quests in an NCQuestGaraTreeCtrl on the "QuestWndBack" panel
// (256 x 335) with the count "(n/15)" and a "?" icon, a "Cancel" button and a check box that keeps the selected
// quest's place marked on the map. Opening it asks the server for the quests again (NConsoleWnd slot 398).
// Slots 69 and 70 (the button's and the check box's commands) are named NCGMQuestWnd::OnDialogResult / OnDestroy in
// the db: they are this class'.

#include "nwindow/NCWnd.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCButton;
class NCCheckBox;
class NCQuestGaraTreeCtrl;

class NCQuestGaraTreeWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCQuestGaraTreeWnd, NCWnd, 0, 0x102F9280)

    UTexture* BackTex;               // +0xE8 "L2UI_CH3.QUESTWND.QuestWndBack"
    NCQuestGaraTreeCtrl* TreeCtrl;   // +0xEC the quests (7, 27, 242 x 274) (name ours)
    NCButton* CancelButton;          // +0xF0 id 0xFD, text 0x181 (name ours)
    NCCheckBox* MarkCheckBox;        // +0xF4 id 0xFF00FF23, text 0x35C, checked at the start (name ours)
    UTexture* ToolTipBtnTex;         // +0xF8 "L2UI_CH3.QUESTWND.QuestWndToolTipBtn" the "?" icon
    BYTE bDC;                        // +0xFC (0) set with bDD when the selected quest has no map place (name ours)
    BYTE bDD;                        // +0xFD (0)
    INT NumQuests;                   // +0x100 the count shown in "(n/15)" (the console fills it) (name ours)
    FString ToolTipText;             // +0x104 (SetToolTipInfo)
    INT ToolTipInfo[4];              // +0x110 (SetToolTipInfo's four numbers; [3] is the quest id GetTrackedQuestID returns)
    UBOOL bShowToolTip;              // +0x120 the mouse is over the "?" icon (name ours)
    NToolTipInfo ToolTip;            // +0x124 the list of the five info icons and their texts (OnCreate)

    NCQuestGaraTreeWnd();                                            // 0x100ed2a0
    // ~NCQuestGaraTreeWnd                                           // 0x1008f590

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x100ea070
    virtual INT OnCreate();                                          // 37 0x100ed330
    virtual INT OnDestroy();                                         // 38 0x100eb650
    virtual void ShowWindow();                                       // 48 0x100ea110
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x100ec230
    virtual INT OnDrawToolTip(NCGDevice* Device);                    // 63 0x100ed8e0 (the db names it OnPostPaint)
    virtual INT OnMouseMove(WPARAM wParam, LPARAM lParam);           // 67 0x100eb8e0
    // names ours
    virtual void ClearList();                                        // 74 0x100ea0f0 the tree emptied, the count 0
    virtual INT OnCancelQuest();                                     // 75 0x100eb6f0 (db: NCGMQuestWnd::OnDialogResult)
    virtual INT OnMarkCheckBox();                                    // 76 0x100ec5e0 (db: NCGMQuestWnd::OnDestroy)

    // 0x100DA3C0 (ret 0x1C, name ours): the tool tip of the selected quest's map place
    void SetToolTipInfo(FString Text, INT A, INT B, INT C, INT D);
    // 0x10116090 (name ours): the quest id of the map place SetToolTipInfo was given last (its fourth number)
    INT GetTrackedQuestID();
    // 0x100DB5D0 (name ours): the map's mark off (the desktop's map window, its map's bShowMark) and the two flags
    // cleared; NCConsole::Reset calls it on the main window's quest tab
    __declspec(noinline) void ClearMark();
};
NW_ASSERT_OFFSET(NCQuestGaraTreeWnd, BackTex, 0xE8);
NW_ASSERT_OFFSET(NCQuestGaraTreeWnd, TreeCtrl, 0xEC);
NW_ASSERT_OFFSET(NCQuestGaraTreeWnd, ToolTipBtnTex, 0xF8);
NW_ASSERT_OFFSET(NCQuestGaraTreeWnd, bDC, 0xFC);
NW_ASSERT_OFFSET(NCQuestGaraTreeWnd, NumQuests, 0x100);
NW_ASSERT_OFFSET(NCQuestGaraTreeWnd, ToolTipText, 0x104);
NW_ASSERT_OFFSET(NCQuestGaraTreeWnd, ToolTipInfo, 0x110);
NW_ASSERT_OFFSET(NCQuestGaraTreeWnd, bShowToolTip, 0x120);
NW_ASSERT_OFFSET(NCQuestGaraTreeWnd, ToolTip, 0x124);
NW_ASSERT_SIZE(NCQuestGaraTreeWnd, 0x1A4);

#pragma pack(pop)
