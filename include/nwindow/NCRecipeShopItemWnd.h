#pragma once
// NCRecipeShopItemWnd - the two item grids of the dwarven recipe shop's manage window (NCRecipeShopWnd.h;
// UClass "CRecipeShopItemWnd", 0x170 bytes in C4 (C3: 0x150); new in C2). Each grid is an NCItemWnd of recipes whose items the player
// moves to the other grid (drag and drop, or a double click): the upper grid ("Recipe List", Mode 1) holds the
// recipe book, the lower one ("Production List", Mode 0) the recipes put up for sale, each with its price.
// Moving into the Mode 0 grid asks the price first (a number dialog, NDialogInfo ID 0x23, answered through
// OnDialogResult), moving into the Mode 1 grid just takes the entry out of the lower grid.
//
// The entries are NItemEntry records (NCItemWnd.h); an entry's UnknownC20 (+0x1B2C in C4: OnDialogResult 0x100F2850 stores the
// typed price there) is the price the shop sends, TradeCount (+0x1B54, 64 bits: AddItem 0x100F1850 stores `cdq` of the
// NItemInfo's dword at +0x1924, the Count) the recipe's second number as the NItemInfo gave it.
//
// C4 addresses: WindowProc (0x10102860) and OnDestroy (0x100EFAC0) are the stock bodies shared with NCShopItemWnd
// (NCShopWnd.cpp hooks them); SetTarget (0x10084BD0, `mov [ecx + 0x168], eax`) and SetMode (0x100EFAD0, `[ecx + 0x16C]`) are
// one body each with the other grids' (NCDeliverWnd.cpp / NCManorShopItemWnd.cpp hook them): here they are stubs that enter them.
// (The C3 image had them at 0x100A2850, 0x100D7090, 0x10088870 and 0x100DE910.)

#include "nwindow/NCItemWnd.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCRecipeShopItemWnd : public NCItemWnd
{
    NW_DECLARE_CLASS(NCRecipeShopItemWnd, NCItemWnd, 0, 0x102FC150)

    NCRecipeShopItemWnd* Target;     // +0x168 (name ours) the other grid (SetTarget 0x10084BD0), cleared by the constructors
    INT Mode;                        // +0x16C (name ours) 0: the grid that takes a price, 1: the grid that gives the
                                     //   entry back; set by OnCreate (SetMode 0x100EFAD0), never by the constructors

    NCRecipeShopItemWnd();                                           // 0x100f2920
    // C3: the item window's gaps are arguments too (ret 0x18): OnCreate makes both grids with (6, 4, 0xA0, 0x20, 5, 3)
    NCRecipeShopItemWnd(INT InColumns, INT InVisibleRows, INT InMaxItems, INT InCellSize, INT InGapX, INT InGapY); // 0x100f29c0
    // ~NCRecipeShopItemWnd                                          // 0x100f2970

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10102860 (shared with NCShopItemWnd)
    virtual INT OnDestroy();                                         // 38 0x100efac0 (shared with NCShopItemWnd)
    virtual INT OnLButtonUp(WPARAM wParam, LPARAM lParam);           // 69 0x100f2560
    // names ours
    virtual INT OnLButtonDblClick(WPARAM wParam, LPARAM lParam);     // 81 0x100f2700 WM_LBUTTONDBLCLK
    virtual INT OnDialogResult(WPARAM wParam, LPARAM lParam);        // 82 0x100f2850 the price typed

    // names ours (the console calls AddItem through NCRecipeShopWnd's two forwarders)
    __declspec(noinline) UBOOL AddItem(NItemInfo* Info);             // 0x100f1850 0 when full
    __declspec(noinline) UBOOL AddCopy(NItemEntry* Src);             // 0x100f1980 0 when full
    __declspec(noinline) void MoveItem(NItemEntry* Item);            // 0x100f1a60 to Target
    void SetTarget(NCRecipeShopItemWnd* InTarget);                   // 0x10084BD0 (ret 4; shared: NCDeliverItemWnd::SetTarget hooks it)
    void SetMode(INT InMode);                                        // 0x100EFAD0 (ret 4; shared: NCManorShopItemWnd::SetMode hooks it)
};
NW_ASSERT_OFFSET(NCRecipeShopItemWnd, Target, 0x168);
NW_ASSERT_OFFSET(NCRecipeShopItemWnd, Mode, 0x16C);
NW_ASSERT_SIZE(NCRecipeShopItemWnd, 0x170);

#pragma pack(pop)
