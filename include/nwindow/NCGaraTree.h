#pragma once
// NCGaraTree - a tree of NCGaraTVItem entries (UClass "CGaraTree", 0x20C bytes in C4 (C3: 0x1EC); new in C3): the root entry and, under it,
// the others, each a child window that the paint puts in place (DrawItem lays them out one under the other, the
// children of an expanded entry indented, and returns the height used). NCGaraTreeCtrl holds one inside a scrolling
// window; NCQuestGaraTree, NCRecipeGaraTree and the Seven Signs trees derive from it with their own DrawItem.
// The slots 75 .. 78 of the C4 vtable (0x101A2460) are the C2 NCTree's AddItem / DeleteItem / Clear / DrawItem (there they
// were 68 .. 71; C3: 69 .. 72).

#include "nwindow/NCGaraTVItem.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

enum { NID_GARATVITEM = 0xA00000 };   // every entry's id (name ours)

class NCGaraTree : public NControl
{
    NW_DECLARE_CLASS(NCGaraTree, NControl, 0, 0x102BE5E0)

    NCGaraTVItem* Root;              // +0x1F4 NULL until the first AddItem without a parent (name ours)
    BYTE _Unknown1D8[0x1E4 - 0x1D8]; // +0x1F8 never touched   // C2: offset derived, no evidence
    INT ContentHeight;               // +0x204 what DrawItem returned in the last paint (name ours)
    INT ScrollOffset;                // +0x208 how far NCGaraTreeCtrl moved it up (the paint's clip follows) (name ours)

    NCGaraTree();                                                    // 0x10015b70
    // ~NCGaraTree                                                   // 0x10015bf0

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10021e60 (folded with NCButton's)
    virtual void SetFocus();                                         // 29 0x10014af0 (the parent window takes it)
    virtual __declspec(noinline) INT OnDestroy();                    // 38 0x10014b10 (the stock subclasses call it)
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x10014fe0
    virtual INT OnLButtonDown(WPARAM wParam, LPARAM lParam);         // 68 0x10014ad0 (focus to the parent, 0)
    // names ours (the C2 NCTree's)
    virtual NCGaraTVItem* AddItem(NGaraTVItemData* Data, NCGaraTVItem* ParentItem);   // 75 0x10015c40 the root when
                                                                     //   ParentItem is NULL (only the first), else
                                                                     //   under ParentItem; NULL: not added
    virtual void DeleteItem(NCGaraTVItem** Item);                    // 76 0x10014f70 the entry and what is under it
    virtual void Clear();                                            // 77 0x10014ac0 everything
    virtual INT DrawItem(NCGDevice* Device, NCGaraTVItem* Item, INT X, INT Y);   // 78 0x100150e0 the height used
};
NW_ASSERT_OFFSET(NCGaraTree, Root, 0x1F4);
NW_ASSERT_OFFSET(NCGaraTree, ContentHeight, 0x204);
NW_ASSERT_OFFSET(NCGaraTree, ScrollOffset, 0x208);
NW_ASSERT_SIZE(NCGaraTree, 0x20C);

#pragma pack(pop)
