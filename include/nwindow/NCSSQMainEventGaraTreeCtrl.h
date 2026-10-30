#pragma once
// NCSSQMainEventGaraTreeCtrl - the scrolling window of one NCSSQMainEventGaraTree (UClass "CSSQMainEventGaraTreeCtrl", 0x11C; new in C3):
// NCGaraTreeCtrl's code repeated with the tree at +0x114 like NCQuestGaraTreeCtrl's (most of the methods are the
// same stock bodies, shared by the linker: they are declared here as stock stubs).

#include "nwindow/NCGaraTreeCtrl.h"
#include "nwindow/NCSSQMainEventGaraTree.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

// One event of the Seven Signs event list as the console's packet reader fills it (name ours; 0xF0 bytes, from the tree
// builder 0x100EB1D0): the group, the event and the numbers the rows show, then two lists of nine texts (the names of the
// cabals' players shown under the details)
struct NSSQEventRecord
{
    INT Type;                        // +0x00 1: the row's text is the count in whole (not "(a b)")
    INT Group;                       // +0x04 the heading the event goes under
    INT Event;                       // +0x08 the event (SSQEventName's number)
    INT Value;                       // +0x0C the number shown after the event's name
    INT Have;                        // +0x10 the first number of the "(a b)" text and of the details
    INT Need;                        // +0x14 the second
    FString NamesA[9];               // +0x18 the first list of the details
    FString NamesB[9];               // +0x84 the second
};
NW_ASSERT_SIZE(NSSQEventRecord, 0xF0);

class NCSSQMainEventGaraTreeCtrl : public NCGaraTreeCtrl
{
    NW_DECLARE_CLASS(NCSSQMainEventGaraTreeCtrl, NCGaraTreeCtrl, 0, 0x10303E08)

    NCSSQMainEventGaraTree* SSQTree;   // +0x134 (id 0xA00) (name ours)
    NCGaraTVItem* RootItem;          // +0x138 (0) the entry the pages' root maker (0x100EB150) added: the groups go under it

    NCSSQMainEventGaraTreeCtrl();                                                       // 0x100fdfe0
    // ~NCSSQMainEventGaraTreeCtrl                                                      // 0x100fe040

    virtual INT OnCreate();                                          // 37 0x100fe090
    virtual INT OnDestroy();                                         // 38 0x100eb1f0 (NCQuestGaraTreeCtrl's body)
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x100fd860
    virtual INT OnMouseWheel(WPARAM wParam, LPARAM lParam);          // 73 0x100f5330 (NCQuestGaraTreeCtrl's body)
    virtual INT ScrollUp();                                          // 75 0x100fd9f0 (NCQuestGaraTreeCtrl's body)
    virtual INT ScrollDown();                                        // 76 0x100eb470 (NCQuestGaraTreeCtrl's body)
    virtual INT OnScrollBar(INT Value);                              // 77 0x100eb540 (NCQuestGaraTreeCtrl's body)
    virtual void ResetScroll();                                      // 78 0x100fd950 (shared by the two Seven Signs ones)
    virtual void ArrangeItem(NCGaraTVItem* Item);                    // 79 0x100ff3f0 (NCQuestGaraTreeCtrl's body)

    // name ours (the page's AddEvent, 0x100EB570, hands its record to it): the event's group is looked for under the root
    // and made (a heading with the +/- sign) when it is new; the event is added under it with a details entry under that;
    // the event's entry comes back
    NCGaraTVItem* AddEvent(const NSSQEventRecord* Record);           // 0x100fdb30 (ret 4)
    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x100ad790 (the stock vtable of this class overrides it; defined in NNewVirtuals.gen.cpp)
};
NW_ASSERT_OFFSET(NCSSQMainEventGaraTreeCtrl, SSQTree, 0x134);
NW_ASSERT_SIZE(NCSSQMainEventGaraTreeCtrl, 0x13C);

#pragma pack(pop)
