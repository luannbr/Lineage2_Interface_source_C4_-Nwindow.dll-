#pragma once
// NCRecipeTreeCtrl - the scrolling frame of the recipe tree (UClass "CRecipeTreeCtrl", 0xE8): an NCTreeCtrl (a scroll window
// with a tree in it) that keeps its own NCRecipeTree in +0xE4 instead of NCTreeCtrl's +0xD0 (the C2 source duplicated the
// base class' code with the new tree; every override here is NCTreeCtrl's body on the field at +0xE4). The tree is made
// as wide as the frame less its slider (16 pixels); the scroll bar moves the tree window up and down (MoveWindow) and the
// tree keeps how far in its +0x1B8 (ScrollOffset). Paint resizes the frame's scroll range from what the tree measured
// (+0x1B4 + 21) and asks for the entries to be shown or hidden by their position (vslot 71).
//
// NCTreeCtrl belongs to another group (w2newui: nwindow/NCTreeCtrl.h); the stand-in this header kept for it until that header was in
// the tree (see NCRecipeTree.h) is gone.

#include "nwindow/NCScrollWnd.h"
#include "nwindow/NCRecipeTree.h"

#include "nwindow/NCTreeCtrl.h"

#pragma pack(push, 4)

// C3: class NCRecipeTreeCtrl gone (no counterpart): class NCRecipeTreeCtrl : public NCTreeCtrl
// C3: class NCRecipeTreeCtrl gone (no counterpart): {
// C3: class NCRecipeTreeCtrl gone (no counterpart):     NW_DECLARE_CLASS(NCRecipeTreeCtrl, NCTreeCtrl, 0, 0x101D71C0)

// C3: class NCRecipeTreeCtrl gone (no counterpart):     NCRecipeTree* RecipeTree;        // +0xE4 the tree control, id 0xA00, (0, 0) with the frame's width less 16 (name ours; NCTreeCtrl's own
// C3: class NCRecipeTreeCtrl gone (no counterpart):                                      //   Tree at +0xD0 stays NULL)

// C3: class NCRecipeTreeCtrl gone (no counterpart):     NCRecipeTreeCtrl();                                              // 0x100c5d70 (NCTreeCtrl's constructor 0x1002a9a0, the vtable, +0xCC = 16)
// C3: class NCRecipeTreeCtrl gone (no counterpart):     // ~NCRecipeTreeCtrl                                             // 0x100c5dc0

// C3: class NCRecipeTreeCtrl gone (no counterpart):     virtual INT OnCreate();                                          // 36 0x100c5e10
// C3: class NCRecipeTreeCtrl gone (no counterpart):     virtual INT OnDestroy();                                         // 37 0x100c5ed0
// C3: class NCRecipeTreeCtrl gone (no counterpart):     virtual INT OnPaint(NCGDevice* Device);                          // 55 0x100c5f10
// C3: class NCRecipeTreeCtrl gone (no counterpart):     virtual INT OnMouseWheel(WPARAM wParam, LPARAM lParam);          // 66 0x100c62c0
// C3: class NCRecipeTreeCtrl gone (no counterpart):     virtual INT ScrollUp();                                          // 67 0x100c60b0
// C3: class NCRecipeTreeCtrl gone (no counterpart):     virtual INT ScrollDown();                                        // 68 0x100c6170
// C3: class NCRecipeTreeCtrl gone (no counterpart):     virtual INT OnScrollBar(INT Value);                              // 69 0x100c6240
// C3: class NCRecipeTreeCtrl gone (no counterpart):     virtual void Clear();                                            // 70 0x100c5fa0 the tree emptied and the view back at the top
// C3: class NCRecipeTreeCtrl gone (no counterpart):     virtual void ShowVisible(NCTVItem* Item);                        // 71 0x100c6010 an entry shown when it is inside the frame, hidden when not; its children too

// C3: class NCRecipeTreeCtrl gone (no counterpart):     // 0x100c6af0 (ret 0x10; name ours): the recipe that makes ItemID at SuccessRate percent, as entries under Parent: the item's
// C3: class NCRecipeTreeCtrl gone (no counterpart):     // entry (icon, name, description, how many are needed and how many the first inventory page holds) and, for each
// C3: class NCRecipeTreeCtrl gone (no counterpart):     // ingredient, the same for the recipe that makes it at 100 percent (down to the items nobody makes). Without a
// C3: class NCRecipeTreeCtrl gone (no counterpart):     // recipe (or with an empty record) the entry is the item's own record and a leaf.
// C3: class NCRecipeTreeCtrl gone (no counterpart):     void AddRecipe(INT ItemID, INT SuccessRate, INT Need, NCTVItem* Parent);
// C3: class NCRecipeTreeCtrl gone (no counterpart): };
// C3: class NCRecipeTreeCtrl gone (no counterpart): NW_ASSERT_OFFSET(NCRecipeTreeCtrl, RecipeTree, 0xE4);
// C3: class NCRecipeTreeCtrl gone (no counterpart): NW_ASSERT_SIZE(NCRecipeTreeCtrl, 0xE8);

#pragma pack(pop)
