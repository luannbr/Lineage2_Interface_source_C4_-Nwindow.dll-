#pragma once
// NCRecipeGaraTreeCtrl - the scrolling frame of the recipe tree (UClass "CRecipeGaraTreeCtrl", 0x118; C2: NCRecipeTreeCtrl):
// NCGaraTreeCtrl's code repeated with the recipe tree at +0x114 (the base's +0x100 tree is never made). The tree is as wide
// as the frame less 15 pixels; the scroll bar moves the tree window up and down and the tree keeps how far in its
// ScrollOffset. AddRecipe fills the tree.

#include "nwindow/NCGaraTreeCtrl.h"
#include "nwindow/NCRecipeGaraTree.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCRecipeGaraTreeCtrl : public NCGaraTreeCtrl
{
    NW_DECLARE_CLASS(NCRecipeGaraTreeCtrl, NCGaraTreeCtrl, 0, 0x102FD610)

    NCRecipeGaraTree* RecipeTree;    // +0x134 (id 0xA00) (name ours)

    NCRecipeGaraTreeCtrl();                                          // 0x100f6e20
    // ~NCRecipeGaraTreeCtrl                                         // 0x100f6e70

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x100ad790 (NCSkillItemWnd's, shared)
    virtual INT OnCreate();                                          // 37 0x100f6ec0
    virtual INT OnDestroy();                                         // 38 0x100eb1f0 (NCQuestGaraTreeCtrl's, shared)
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x100f5130
    virtual INT OnMouseWheel(WPARAM wParam, LPARAM lParam);          // 73 0x100f5330 (shared)
    virtual INT ScrollUp();                                          // 75 0x100fd9f0 (shared)
    virtual INT ScrollDown();                                        // 76 0x100eb470 (shared)
    virtual INT OnScrollBar(INT Value);                              // 77 0x100f52b0
    virtual void ResetScroll();                                      // 78 0x100f5220 the tree emptied and back at the top
    virtual void ArrangeItem(NCGaraTVItem* Item);                    // 79 0x100ff3f0 (shared)

    // 0x100E4550 (ret 0x10; name ours): the recipe that makes ItemID at SuccessRate percent, as entries under Parent: the
    // item's entry (icon, name, description, how many are needed and how many the first inventory page holds) and, for
    // each ingredient, the same for the recipe that makes it at 100 percent (down to the items nobody makes). Without a
    // recipe (or with an empty record) the entry is the item's own record and a leaf.
    void AddRecipe(INT ItemID, INT SuccessRate, INT Need, NCGaraTVItem* Parent);
};
NW_ASSERT_OFFSET(NCRecipeGaraTreeCtrl, RecipeTree, 0x134);
NW_ASSERT_SIZE(NCRecipeGaraTreeCtrl, 0x138);

#pragma pack(pop)
