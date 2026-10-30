#pragma once
// NCSSQSealStatusGaraTreeCtrl - the scrolling window of one NCSSQSealStatusGaraTree (UClass "CSSQSealStatusGaraTreeCtrl", 0x11C; new in C3):
// NCGaraTreeCtrl's code repeated with the tree at +0x114 like NCQuestGaraTreeCtrl's (most of the methods are the
// same stock bodies, shared by the linker: they are declared here as stock stubs).

#include "nwindow/NCGaraTreeCtrl.h"
#include "nwindow/NCSSQSealStatusGaraTree.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

// One seal of the Seven Signs seal list as the console's packet reader fills it (name ours; 0x1C bytes, from the tree
// builder 0x100ECCA0)
struct NSSQSealRecord
{
    INT Unknown00;                   // +0x00 not read by the builder
    INT Seal;                        // +0x04 the seal (1 .. 3: SSQSealName's number)
    INT Owner;                       // +0x08 the cabal that holds it (1, 2; SSQCabalName's number)
    INT Low;                         // +0x0C the share of the cabal 2 when it is the owner (percent)
    INT High;                        // +0x10 the share of the owner's rival
    INT Total1;                      // +0x14 the first bar's value (percent)
    INT Total2;                      // +0x18 the second bar's value (percent)
};
NW_ASSERT_SIZE(NSSQSealRecord, 0x1C);

class NCSSQSealStatusGaraTreeCtrl : public NCGaraTreeCtrl
{
    NW_DECLARE_CLASS(NCSSQSealStatusGaraTreeCtrl, NCGaraTreeCtrl, 0, 0x10304870)

    NCSSQSealStatusGaraTree* SSQTree;   // +0x134 (id 0xFFF0A0F1) (name ours)
    NCGaraTVItem* RootItem;          // +0x138 (0) the entry the pages' root maker (0x100ECC20) added: the seals go under it

    NCSSQSealStatusGaraTreeCtrl();                                                       // 0x100ff930
    // ~NCSSQSealStatusGaraTreeCtrl                                                      // 0x100ff990

    virtual INT OnCreate();                                          // 37 0x100ff9e0
    virtual INT OnDestroy();                                         // 38 0x100eb1f0 (NCQuestGaraTreeCtrl's body)
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x100ff300
    virtual INT OnMouseWheel(WPARAM wParam, LPARAM lParam);          // 73 0x100f5330 (NCQuestGaraTreeCtrl's body)
    virtual INT ScrollUp();                                          // 75 0x100fd9f0 (NCQuestGaraTreeCtrl's body)
    virtual INT ScrollDown();                                        // 76 0x100eb470 (NCQuestGaraTreeCtrl's body)
    virtual INT OnScrollBar(INT Value);                              // 77 0x100eb540 (NCQuestGaraTreeCtrl's body)
    virtual void ResetScroll();                                      // 78 0x100fd950 (shared by the two Seven Signs ones)
    virtual void ArrangeItem(NCGaraTVItem* Item);                    // 79 0x100ff3f0 (NCQuestGaraTreeCtrl's body)

    // name ours (the page's AddEntry, 0x100ECFC0, hands its record to it): a heading under the root with the seal's
    // numbers and the seal's details entry (kind 2) under it; the details entry comes back
    NCGaraTVItem* AddEntry(const NSSQSealRecord* Record);            // 0x100ff540 (ret 4)
    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x100ad790 (the stock vtable of this class overrides it; defined in NNewVirtuals.gen.cpp)
};
NW_ASSERT_OFFSET(NCSSQSealStatusGaraTreeCtrl, SSQTree, 0x134);
NW_ASSERT_SIZE(NCSSQSealStatusGaraTreeCtrl, 0x13C);

#pragma pack(pop)
