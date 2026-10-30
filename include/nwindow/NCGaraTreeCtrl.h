#pragma once
// NCGaraTreeCtrl - a scrolling window with one NCGaraTree inside (UClass "CGaraTreeCtrl", 0x114; new in C3): the
// tree is as wide as the window less the bar's 16 pixels and grows with its entries; the window's own scroll moves the
// tree up (NCGaraTree::ScrollOffset follows). NCQuestGaraTreeCtrl and the Seven Signs ones repeat the code with
// their own tree at +0x114 / +0x118.

#include "nwindow/NCGaraTree.h"
#include "nwindow/NCScrollWnd.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCGaraTreeCtrl : public NCScrollWnd
{
    NW_DECLARE_CLASS(NCGaraTreeCtrl, NCScrollWnd, 0, 0x102BEB10)

    NCGaraTree* Tree;                // +0x120 (id 0xA00001) (name ours)
    BYTE _Unknown104[0x114 - 0x104]; // +0x124 never touched   // C2: offset derived, no evidence

    NCGaraTreeCtrl();                                                // 0x10015e40
    // ~NCGaraTreeCtrl                                               // 0x10015e90

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10014b90 (folded with NCItemWnd's)
    virtual INT OnCreate();                                          // 37 0x10015ee0
    virtual INT OnDestroy();                                         // 38 0x10015250
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x10015300
    virtual INT OnMouseWheel(WPARAM wParam, LPARAM lParam);          // 73 0x100156d0
    virtual INT ScrollUp();                                          // 75 0x100154c0
    virtual INT ScrollDown();                                        // 76 0x10015580
    virtual INT OnScrollBar(INT Value);                              // 77 0x10015650
    // names ours
    virtual void ResetScroll();                                      // 78 0x10015800 the tree back at the top
    virtual void ArrangeItem(NCGaraTVItem* Item);                    // 79 0x100153f0 shows the entries that are inside
                                                                     //   the window, hides the others
};
NW_ASSERT_OFFSET(NCGaraTreeCtrl, Tree, 0x120);
NW_ASSERT_SIZE(NCGaraTreeCtrl, 0x134);

#pragma pack(pop)
