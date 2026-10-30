#pragma once
// NCItemEnchantWnd - the window that asks which item an enchant scroll is for (new in C4; UClass "CItemEnchantWnd",
// 0xF8, desktop-created by the console's SelectItemToEnchant): a title bar, one grid of the items that can take the
// scroll (NCItemEnchantItemWnd, 6 x 9 cells of 32 pixels, 250 at most; its own click handler selects only), and the
// OK / Cancel buttons. OK sends the selected item's object id (NConsoleWnd::RequestEnchantItem), Cancel sends -1; both
// close the window. Closing it empties the grid.
//
// The stock vtable (0x101B6ED0, 76 slots) overrides WindowProc, OnCreate, OnDestroy, HideWindow and OnPaint and adds two
// slots, 74 (OnOK) and 75 (OnCancel), names ours. The grid's class overrides OnLButtonDown and WindowProc (NCSkillItemWnd's
// shared body).

#include "nwindow/NCItemWnd.h"
#include "nwindow/NCButton.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

// the grid of the candidate items (UClass "CItemEnchantItemWnd", 0x168 = NCItemWnd's: no fields of its own): a click
// selects the item under the mouse and nothing else (no drag, no tool tip change)
class NCItemEnchantItemWnd : public NCItemWnd
{
    NW_DECLARE_CLASS(NCItemEnchantItemWnd, NCItemWnd, 0, 0x102DA908)

    NCItemEnchantItemWnd();                                          // 0x1009a0e0
    NCItemEnchantItemWnd(INT InColumns, INT InVisibleRows, INT InMaxItems, INT InCellSize, INT InGapX, INT InGapY); // 0x1009a180 (anonymous in the db)
    // ~NCItemEnchantItemWnd                                         // 0x1009a130

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x100ad790 (NCSkillItemWnd's, shared)
    virtual INT OnLButtonDown(WPARAM wParam, LPARAM lParam);         // 68 0x10095d40
};
NW_ASSERT_SIZE(NCItemEnchantItemWnd, 0x168);

class NCItemEnchantWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCItemEnchantWnd, NCWnd, 0, 0x102DB368)

    NCItemEnchantItemWnd* ItemWnd;   // +0xE8 the candidates (6 x 9 cells) at (9, 48), 240 x 318
    NCButton* OKButton;              // +0xEC id 0xF0F012 at (51, 372), text 140
    NCButton* CancelButton;          // +0xF0 id 0xF0F013 at (131, 372), text 141
    UTexture* BackTex;               // +0xF4 "L2UI_CH3.InventoryWnd.itemenchant_back"

    NCItemEnchantWnd();                                              // 0x1009a1c0
    // ~NCItemEnchantWnd                                             // 0x1009a210

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10093fc0
    virtual INT OnCreate();                                          // 37 0x1009a260
    virtual INT OnDestroy();                                         // 38 0x10095ed0
    virtual void HideWindow();                                       // 49 0x10094070
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x10095db0
    // names ours
    virtual INT OnOK();                                              // 74 0x10095f70 asks the console for the selected item
    virtual INT OnCancel();                                          // 75 0x10094040 asks for none (-1) and closes
};
NW_ASSERT_OFFSET(NCItemEnchantWnd, ItemWnd, 0xE8);
NW_ASSERT_OFFSET(NCItemEnchantWnd, BackTex, 0xF4);
NW_ASSERT_SIZE(NCItemEnchantWnd, 0xF8);

#pragma pack(pop)
