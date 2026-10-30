#pragma once
// NCQuestWnd - the quest window (UClass "CQuestWnd", 0xCC, 240 x 300; NCMainWnd creates it,
// NCGMQuestWnd derives from it): the accepted quests in a list and, below, two tab pages: the
// selected quest's description (NCQuestInfoWnd) and the quest items (NCQuestItemWnd). Opening
// it asks the server for the quests again; "Cancel" gives up the selected quest after asking.

#include "nwindow/NCWnd.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCButton;
class NCListBox;
class NCQuestInfoWnd;
class NCQuestItemWnd;
class NCTabControl;
struct NItemInfo;

class NCQuestWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCQuestWnd, NCWnd, 0, 0x102FA220)

    UTexture* TopTex;                // +0xE8 "L2UI.QuestWnd.questBack1" the top 115 pixels   // C2: offset derived, no evidence
    UTexture* InfoBackTex;           // +0xEC "L2UI.QuestWnd.questBack2" the rest, page 0   // C2: offset derived, no evidence
    UTexture* ItemBackTex;           // +0xF0 "L2UI.QuestWnd.questBack3" the rest, page 1   // C2: offset derived, no evidence
    UTexture* ItemPageIconTex;       // +0xF4 "sek.cbui303" 14 x 14 at (220, 127) on page 1   // C2: offset derived, no evidence
    NCListBox* QuestList;            // +0xF8 the quest names, the quest ids as parameters
    NCQuestInfoWnd* InfoWnd;         // +0xFC page 0 "Description"
    NCQuestItemWnd* ItemWnd;         // +0x100 page 1 "Item"
    NCTabControl* TabCtrl;           // +0x104
    NCButton* CancelButton;          // +0x108 page 0 only
    NCWnd* NewChild[2];              // +0x10C, +0xC0 new in C2 (NCPushButtons): [0] the arrow back (command 0xF9), [1] the arrow
                                     //   forward (0xF8) of the description page; shown with it, hidden on the item page (OnTab)
    NRectF DropRect;                 // +0x114 (160, 420, 27.0, 27.0; C3: the size is FLOAT) the rectangle where a quest item
                                     //   dropped asks to be destroyed (IsInDropZone): below the window, so a drop
                                     //   inside never hits it
    INT CurTab;                      // +0x124 page shown

    NCQuestWnd();                                                    // 0x100eef60
    // ~NCQuestWnd                                                   // 0x10073c50 (C1 only: no C2 match) (C1 only: no C2 match)

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x100edb60
    virtual INT OnCreate();                                          // 37 0x100ef050
    virtual INT OnDestroy();                                         // 38 0x100ee700
    virtual void ShowWindow();                                       // 48 0x100ef640
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x100ee8b0
    // names ours
    virtual UBOOL IsInDropZone(INT X, INT Y);                        // 74 0x100edd00
    virtual INT OnDialogResult(WPARAM wParam, LPARAM lParam);        // 75 0x100eeac0
    virtual INT OnCancel();                                          // 76 0x100eeaf0
    virtual INT OnSelectQuest(INT Index);                            // 77 0x100eebf0
    virtual INT OnTab(INT Page);                                     // 78 0x100edc60
    virtual INT OnNewButton1();                                      // 79 0x100eec10 (C2 only; name ours) the command 0xF8: next step
    virtual INT OnNewButton2();                                      // 80 0x100eec30 (C2 only; name ours) the command 0xF9: previous step

    // names ours: the console fills the window with these
    __declspec(noinline) void Clear();                               // 0x100ef610 (called by ShowWindow: out of line)
};
NW_ASSERT_OFFSET(NCQuestWnd, TopTex, 0xE8);
NW_ASSERT_OFFSET(NCQuestWnd, QuestList, 0xF8);
NW_ASSERT_OFFSET(NCQuestWnd, DropRect, 0x114);
NW_ASSERT_OFFSET(NCQuestWnd, CurTab, 0x124);
NW_ASSERT_SIZE(NCQuestWnd, 0x128);

#pragma pack(pop)