#pragma once
// NCQuestItemWnd - the quest window's "Item" page (UClass "CQuestItemWnd", 0x118): the quest
// items, 6 x 4 icons of 32 pixels (at most 80). The selected item's tool tip lists its name
// (enchant, count), slot and description. An item dropped on the quest window's drop zone
// asks whether to destroy it (how many, when stacked).

#include "nwindow/NCItemWnd.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCQuestItemWnd : public NCItemWnd
{
    NW_DECLARE_CLASS(NCQuestItemWnd, NCItemWnd, 0, 0x102FAC88)

    NCQuestItemWnd();                                                // 0x100eee90
    NCQuestItemWnd(INT InColumns, INT InVisibleRows, INT InMaxItems, INT InCellSize); // 0x100eef30
    // ~NCQuestItemWnd                                               // 0x100eeee0

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x100edaf0
    virtual INT OnCreate();                                          // 37 0x100edb50
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x100ee240
    virtual INT OnLButtonUp(WPARAM wParam, LPARAM lParam);           // 69 0x100ee400
    virtual void DrawItemToolTip(NCGDevice* Device, NItemEntry* Item, INT X, INT Y); // 80 0x1001eb90
    // names ours
    virtual INT OnDialogResult(WPARAM wParam, LPARAM lParam);        // 81 0x100ee640
};
NW_ASSERT_SIZE(NCQuestItemWnd, 0x168);

#pragma pack(pop)