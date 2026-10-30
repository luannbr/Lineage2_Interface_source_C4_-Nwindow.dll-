#pragma once
// NCRecipeBookItemWnd - the recipe book's icon grid (UClass "CRecipeBookItemWnd", 0x148; new in C2). A plain
// NCItemWnd (6 x 7 icons, 160 recipes at most, a name line for the tool tip) whose entries are the recipes the
// character knows. Dragging a recipe onto the book's trash icon (NCRecipeBookWnd::HitTrash) asks to delete it
// (NDialogInfo ID 0x22, answered by the console: RequestRecipeItemDelete); a double click asks the server for the
// recipe's ingredients (RequestRecipeItemMakeInfo) and closes the grid.
//
// WindowProc is the stock body shared with NCInvenItemWnd (0x100CBA90).

#include "nwindow/NCItemWnd.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCRecipeBookItemWnd : public NCItemWnd
{
    NW_DECLARE_CLASS(NCRecipeBookItemWnd, NCItemWnd, 0, 0x102FF538)

    NCRecipeBookItemWnd();                                           // 0x100f5e90
    // C3: the item window's gaps are arguments too (ret 0x18): NCRecipeBookWnd::OnCreate makes it with (6, 8, 0xA0, 0x20, 5, 3)
    NCRecipeBookItemWnd(INT InColumns, INT InVisibleRows, INT InMaxItems, INT InCellSize, INT InGapX, INT InGapY); // 0x100f5f30
    // ~NCRecipeBookItemWnd                                          // 0x100f5ee0

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10093ce0 (shared with NCInvenItemWnd)
    virtual INT OnMouseMove(WPARAM wParam, LPARAM lParam);           // 67 0x100f3c00
    virtual INT OnLButtonDown(WPARAM wParam, LPARAM lParam);         // 68 0x100f3b90
    virtual INT OnLButtonUp(WPARAM wParam, LPARAM lParam);           // 69 0x100f3cf0
    // names ours
    virtual INT OnLButtonDblClick(WPARAM wParam, LPARAM lParam);     // 81 0x100f3f10 WM_LBUTTONDBLCLK

    // name ours (the console calls it through NCRecipeBookWnd::AddRecipe)
    __declspec(noinline) UBOOL AddItem(NItemInfo* Info);             // 0x100f58a0 0 when full
};
NW_ASSERT_SIZE(NCRecipeBookItemWnd, 0x168);

#pragma pack(pop)
