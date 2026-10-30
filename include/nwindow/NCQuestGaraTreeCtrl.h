#pragma once
// NCQuestGaraTreeCtrl - the scrolling window of the quest tree (UClass "CQuestGaraTreeCtrl", 0x120; new in C3):
// NCGaraTreeCtrl's code repeated with the quest tree at +0x114 (the base's +0x100 tree is never made).

#include "nwindow/NCGaraTreeCtrl.h"
#include "nwindow/NCQuestGaraTree.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCQuestGaraTreeCtrl : public NCGaraTreeCtrl
{
    NW_DECLARE_CLASS(NCQuestGaraTreeCtrl, NCGaraTreeCtrl, 0, 0x102F97B8)

    NCQuestGaraTree* QuestTree;      // +0x134 (id 0xA00) (name ours)
    NCGaraTVItem* RootItem;          // +0x138 the root entry NConsoleWnd makes at the start of a quest list (0x100DB380)
    INT SelectedID;                  // +0x13C the quest the console marked as selected (an entry's +0x1A08)

    NCQuestGaraTreeCtrl();                                           // 0x100ed120
    // ~NCQuestGaraTreeCtrl                                          // 0x100ed180

    virtual INT OnCreate();                                          // 37 0x100ed1d0
    virtual INT OnDestroy();                                         // 38 0x100eb1f0
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x100eb230
    virtual INT OnMouseWheel(WPARAM wParam, LPARAM lParam);          // 73 0x100f5330
    virtual INT ScrollUp();                                          // 75 0x100fd9f0
    virtual INT ScrollDown();                                        // 76 0x100eb470
    virtual INT OnScrollBar(INT Value);                              // 77 0x100eb540
    virtual void ResetScroll();                                      // 78 0x100eb3c0 (the base's with the quest tree)
    virtual void ArrangeItem(NCGaraTVItem* Item);                    // 79 0x100ff3f0

    // 0x100DB1A0 (name ours, from NCGMWnd.cpp's view struct): the id of the quest the tree has marked (the first entry
    // under the root whose bClicked is set), -1 when there is none
    __declspec(noinline) INT FindMarkedQuest();
    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x100ad790 (the stock vtable of this class overrides it; defined in NNewVirtuals.gen.cpp)
};
NW_ASSERT_OFFSET(NCQuestGaraTreeCtrl, QuestTree, 0x134);
NW_ASSERT_OFFSET(NCQuestGaraTreeCtrl, RootItem, 0x138);
NW_ASSERT_OFFSET(NCQuestGaraTreeCtrl, SelectedID, 0x13C);
NW_ASSERT_SIZE(NCQuestGaraTreeCtrl, 0x140);

#pragma pack(pop)
