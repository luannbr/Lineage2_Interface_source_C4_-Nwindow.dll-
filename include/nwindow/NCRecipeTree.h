#pragma once
// NCRecipeTree - the ingredient tree of the recipe information window (UClass "CRecipeTree", 0x1D8): an NCTree (the tree view
// control of the C2 NWindow: its entries are NCTVItem child windows; NCTree keeps the root in +0x1A4, what the last paint
// measured in +0x1B4 and the view offset in +0x1B8) that draws each entry as a 32 x 32 frame with the ingredient's icon, its
// name and "(have/need)", a plus / minus sign for an entry with children and a dimming square for an ingredient the player
// lacks. NCRecipeTreeCtrl (the scrolling frame) owns it; NCRecipeTreeWnd shows both.
//
// NCTree and NCTVItem belong to another group (w2newui: nwindow/NCTree.h, NCTVItem.h). Stage 5 (sa-verify): the stand-in this
// header kept for them until those headers were in the tree (a second NCTree / NTVItemData that never compiled, because
// __has_include always found the real ones) is gone.
//
// What this class adds (OnCreate 0x100c4830): the five textures of the entries. The entry data the recipe code fills (NCTVItem's
// NTVItemData): Unknown08 is how many the recipe needs, Unknown0C how many the player carries (names after their use here).

#include "nwindow/NCTree.h"

#pragma pack(push, 4)

// C3: class NCRecipeTree gone (no counterpart): class NCRecipeTree : public NCTree
// C3: class NCRecipeTree gone (no counterpart): {
// C3: class NCRecipeTree gone (no counterpart):     NW_DECLARE_CLASS(NCRecipeTree, NCTree, 0, 0x101D6258)

// C3: class NCRecipeTree gone (no counterpart):     UTexture* FrameTex;              // +0x1BC "L2UI.RecipeWnd.RecipeTreeIconBack": an entry with children, collapsed
// C3: class NCRecipeTree gone (no counterpart):     UTexture* FrameOpenTex;          // +0x1C0 "L2UI.RecipeWnd.RecipeTreeIconBack_click": expanded
// C3: class NCRecipeTree gone (no counterpart):     UTexture* FrameLeafTex;          // +0x1C4 "L2UI.RecipeWnd.RecipeTreeIconDisableBack": a leaf
// C3: class NCRecipeTree gone (no counterpart):     UTexture* PlusTex;               // +0x1C8 "L2UI.RecipeWnd.TreePlus"
// C3: class NCRecipeTree gone (no counterpart):     UTexture* MinusTex;              // +0x1CC "L2UI.RecipeWnd.TreeMinus"
// C3: class NCRecipeTree gone (no counterpart):     INT Unknown1D0;                  // +0x1D0 (name ours) not touched by this class
// C3: class NCRecipeTree gone (no counterpart):     INT Unknown1D4;                  // +0x1D4 (name ours) not touched by this class

// C3: class NCRecipeTree gone (no counterpart):     NCRecipeTree();                                                  // 0x100c5960
// C3: class NCRecipeTree gone (no counterpart):     // ~NCRecipeTree                                                 // 0x100c59b0

// C3: class NCRecipeTree gone (no counterpart):     virtual INT OnCreate();                                          // 36 0x100c4830
// C3: class NCRecipeTree gone (no counterpart):     virtual INT OnPaint(NCGDevice* Device);                          // 55 0x100c5a00
// C3: class NCRecipeTree gone (no counterpart):     // 71 0x100c5a90 (ret 0x10): draws Item at (X, Y) and its children below / beside it and returns the height used
// C3: class NCRecipeTree gone (no counterpart):     virtual INT DrawItem(NCGDevice* Device, NCTVItem* Item, INT X, INT Y);
// C3: class NCRecipeTree gone (no counterpart): };
// C3: class NCRecipeTree gone (no counterpart): NW_ASSERT_OFFSET(NCRecipeTree, FrameTex, 0x1BC);
// C3: class NCRecipeTree gone (no counterpart): NW_ASSERT_OFFSET(NCRecipeTree, MinusTex, 0x1CC);
// C3: class NCRecipeTree gone (no counterpart): NW_ASSERT_SIZE(NCRecipeTree, 0x1D8);

#pragma pack(pop)
