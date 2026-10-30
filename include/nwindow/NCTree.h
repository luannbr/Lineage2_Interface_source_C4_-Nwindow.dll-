#pragma once
// NCTree - a tree view (UClass "CTree", 0x1FC): the root NCTVItem and, under it, the others. They are no windows any
// more (C3): plain objects the paint puts in place (DrawItem lays them out one under the other, the children of an
// expanded entry indented, and returns the height used) and that a click on the entry's picture or text opens and
// closes. NCTreeCtrl holds one inside a scrolling window; the messenger's MsnTree (and its FriendTree) derive from it
// with their own DrawItem. The slots 69 .. 71 are the C2 NCTree's AddItem / Clear / DrawItem (there 68 .. 71, with
// DeleteItem between them: it is no slot in C3).

#include "nwindow/NCTVItem.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCGDevice;

class NCTree : public NControl
{
    NW_DECLARE_CLASS(NCTree, NControl, 0, 0x102C6158)

    NCTVItem* Root;                  // +0x1F4 NULL until the first AddItem without a parent (name ours)
    INT Unknown1D8;                  // +0x1F8 never touched   // C2: offset derived, no evidence
    NCTVItem* Unknown1DC;            // +0x1FC (0) cleared when the entry it holds is deleted
    NCTVItem* Unknown1E0;            // +0x200 (0) the same
    NCTVItem* Selected;              // +0x204 (0) the entry the last click chose (name ours)
    NCTVItem* Hover;                 // +0x208 (0) the entry under the mouse, or NULL (name ours)
    INT ContentHeight;               // +0x20C what DrawItem returned in the last paint (name ours)
    INT ScrollOffset;                // +0x210 how far NCTreeCtrl moved it up (the paint's clip follows) (name ours)
    UTexture* SelectTex;             // +0x214 "L2UI_CH3.ListCtrl.TextSelect": the selection bar's left part
    UTexture* SelectTex2;            // +0x218 "L2UI_CH3.ListCtrl.TextSelect2": the right end

    NCTree();                                                        // 0x100340d0
    // ~NCTree                                                       // 0x100341b0

    virtual INT OnDestroy();                                         // 38 0x10032b70 (the entries go)
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x10032f50
    virtual INT OnMouseMove(WPARAM wParam, LPARAM lParam);           // 67 0x100331d0
    virtual INT OnLButtonDown(WPARAM wParam, LPARAM lParam);         // 68 0x10033150
    // names ours (the C2 NCTree's)
    virtual NCTVItem* AddItem(NTVItemData* Data, NCTVItem* ParentItem);   // 75 0x10033f50 the root when ParentItem is
                                                                     //   NULL (only the first), else under ParentItem;
                                                                     //   NULL: not added
    virtual void Clear();                                            // 76 0x100340a0 everything
    virtual INT DrawItem(NCGDevice* Device, NCTVItem* Item, INT X, INT Y);   // 77 0x10033850 the height used

    void DeleteItem(NCTVItem** Item);                                // 0x10034010 (ret 4, name ours) the entry and what
                                                                     //   is under it; its place in the tree is cleared
    NCTVItem* FindItemAt(INT X, INT Y, NCTVItem* Item);              // 0x10033060 (ret 0xC, name ours) the entry whose
                                                                     //   picture or text holds the point (the first of
                                                                     //   those under Item that is visible), or NULL

    // NNewVirtuals.gen.cpp (generated) still defines the three slots as stock jumps under their old names: they are
    // the functions above now. Never called.
    // C4: void NewVirtual69();
    // C4: void NewVirtual70();
    // C4: void NewVirtual71();
    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10021e60 (the stock vtable of this class overrides it; defined in NNewVirtuals.gen.cpp)
    virtual INT OnDrawToolTip(NCGDevice* Device) ;  // 63 0x100f76f0 (the stock vtable of this class overrides it; defined in NNewVirtuals.gen.cpp)
};
NW_ASSERT_OFFSET(NCTree, Root, 0x1F4);
NW_ASSERT_OFFSET(NCTree, Selected, 0x204);
NW_ASSERT_OFFSET(NCTree, Hover, 0x208);
NW_ASSERT_OFFSET(NCTree, ContentHeight, 0x20C);
NW_ASSERT_OFFSET(NCTree, ScrollOffset, 0x210);
NW_ASSERT_OFFSET(NCTree, SelectTex, 0x214);
NW_ASSERT_SIZE(NCTree, 0x21C);

#pragma pack(pop)
