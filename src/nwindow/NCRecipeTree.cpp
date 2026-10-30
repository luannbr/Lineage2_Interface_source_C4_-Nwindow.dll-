/*=============================================================================
    NCRecipeTree.cpp - the ingredient tree of the recipe information window (C2 only).
    Stock: 0x100c4830 (OnCreate), 0x100c5960 .. 0x100c59b0 (constructor, destructor), 0x100c5a00 (OnPaint), 0x100c5a90 (the
    entry drawing, vslot 71).
    The constructor calls NCTree's (0x1002a660, w2newui's): it is hooked only when that group's header is in the tree
    (NW_HAVE_NCTREE_H, see NCRecipeTree.h); until then the stock constructor runs (NCRecipeTreeCtrl::OnCreate makes the tree
    through it: StockCall).
=============================================================================*/
#include "nwindow/NCRecipeTree.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NClasses.gen.h"

// what the recipe code reads of an entry (NCTVItem's data record names the third and fourth dwords after nothing: here they
// are the number the recipe needs and the number the player has; +0x1A1C / +0x1A20 are the entry's place, see NCRecipeTree.h)
#if 0  // C2: does not compile against the C2 headers (db/demote.csv)
static FORCEINLINE INT& EntryNeed(NCTVItem* Item) { return Item->Data.Unknown08; }
static FORCEINLINE INT& EntryHave(NCTVItem* Item) { return Item->Data.Unknown0C; }

/*-----------------------------------------------------------------------------
    NCRecipeTree
-----------------------------------------------------------------------------*/

#ifdef NW_HAVE_NCTREE_H
#endif
#endif





// C2: no vtable for NW_CHECK_VTABLE(NCRecipeTree, 0x101242b0, 72)
