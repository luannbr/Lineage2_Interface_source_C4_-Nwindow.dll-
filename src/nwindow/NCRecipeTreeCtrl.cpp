/*=============================================================================
    NCRecipeTreeCtrl.cpp - the scrolling frame of the recipe tree (C2 only).
    Stock: 0x100c5d70 .. 0x100c5dc0 (constructor, destructor), 0x100c5e10 .. 0x100c6240 (the overrides), 0x100c62c0
    (OnMouseWheel), 0x100c6af0 (the tree fill).
    The constructor calls NCTreeCtrl's (0x1002a9a0, w2newui's): it is hooked only when that group's header is in the tree
    (NW_HAVE_NCTREECTRL_H, see NCRecipeTreeCtrl.h); until then the stock constructor runs (NCRecipeTreeWnd::OnCreate makes the
    control through it: StockCall).
=============================================================================*/
#include "nwindow/NCRecipeTreeCtrl.h"
#include "nwindow/NCConsole.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NCInventoryWnd.h"
#include "nwindow/NCInvenItemWnd.h"
#include "nwindow/NCSliderBar.h"
#include "nwindow/NRecipeData.h"
#include "nwindow/NClasses.gen.h"

// NWM id of the tree control (names ours)
enum { NID_RECIPETREE_TREE = 0xA00 };
// the node width is the recipe tree's: a scroll bar of 16 pixels at the right of the frame
enum { RECIPETREE_SCROLLBAR_W = 16 };
// the node icon size and the sure recipe rate of the ingredients (names ours)
enum { RECIPETREE_ICON = 32, RECIPE_RATE_SURE = 100 };

// what the recipe code reads of an entry's data (see NCRecipeTree.cpp)
static FORCEINLINE INT& DataNeed(NTVItemData& Data) { return Data.Unknown08; }
static FORCEINLINE INT& DataHave(NTVItemData& Data) { return Data.Unknown0C; }

/*-----------------------------------------------------------------------------
    NCRecipeTreeCtrl
-----------------------------------------------------------------------------*/

#ifdef NW_HAVE_NCTREECTRL_H
#endif












// C2: no vtable for NW_CHECK_VTABLE(NCRecipeTreeCtrl, 0x101243d0, 72)
