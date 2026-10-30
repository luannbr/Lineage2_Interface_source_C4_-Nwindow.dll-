#pragma once
// NCTreeCtrl - a scrolling tree view (UClass "CTreeCtrl", 0xE4 in C2, 0x114 in C3): an NCScrollWnd with an
// NCTree (Tree) inside it, 16 pixels narrower than the window (the scroll bar's place). Scrolling
// moves the tree window and keeps its ScrollOffset (the tree's paint clips with it). The messenger's
// MsnTreeCtrl is built on it.

#include "nwindow/NCScrollWnd.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCTree;
class NCTVItem;

class NCTreeCtrl : public NCScrollWnd
{
    NW_DECLARE_CLASS(NCTreeCtrl, NCScrollWnd, 0, 0x102C6688)

    NCTree* Tree;                    // +0x120 id 0xA00001 (name ours)
    BYTE _UnknownD4[0xE4 - 0xD4];    // +0x124 never touched   // C2: offset derived, no evidence   // C2: offset derived, no evidence

    NCTreeCtrl();                                                    // 0x10034200
    // ~NCTreeCtrl                                                   // 0x1002a9f0 (C1 only: no C2 match) (C1 only: no C2 match)

    virtual INT OnCreate();                                          // 37 0x100342a0
    virtual INT OnDestroy();                                         // 38 0x100332a0
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x10033350
    virtual INT OnMouseWheel(WPARAM wParam, LPARAM lParam);          // 73 0x10033640
    virtual INT ScrollUp();                                          // 75 0x10033430
    virtual INT ScrollDown();                                        // 76 0x100334f0
    virtual INT OnScrollBar(INT Value);                              // 77 0x100335c0
    virtual void Clear();                                            // 78 0x10033770 (name ours) the tree emptied, scrolled back
    // C3: ShowVisible is gone (the tree's own paint shows and hides its entries); NCRecipeTreeCtrl's header keeps its own
    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10014b90 (the stock vtable of this class overrides it; defined in NNewVirtuals.gen.cpp)
};
NW_ASSERT_OFFSET(NCTreeCtrl, Tree, 0x120);
NW_ASSERT_SIZE(NCTreeCtrl, 0x134);

#pragma pack(pop)
