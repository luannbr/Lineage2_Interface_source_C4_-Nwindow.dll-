#pragma once
// NCInvenItemWnd - a page of the inventory's items (UClass "CInvenItemWnd", 0x13C; the
// inventory has two: "Item" and "Quest", 6 x 4 icons of 32 pixels, 160 at most). An item is
// used with a double click or the right button; dragged inside the page it changes place,
// onto the paperdoll it is equipped, onto the shortcut bar it gets a shortcut, onto the pet
// window it is given to the pet, onto the inventory's trash / crystallize icons or the ground
// it asks first (the answer comes back through the dialog box itself, NDialogInfo::Owner / ID).
// C4: the release is OnLButtonUp, slot 69 (+0x114), 0x100976C0, proved against the stock with
// tools/port/emu/win/t_invdrop.py; the other handlers with t_invpage.py. The enchant scroll
// mode of C3 is gone from the release (the stock C4 function has no such branch).

#include "nwindow/NCItemWnd.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCInvenItemWnd : public NCItemWnd
{
    NW_DECLARE_CLASS(NCInvenItemWnd, NCItemWnd, 0, 0x102DB898)

    // C2 (sizeof 0x124): the four "question pending" flags of C1 are gone; the questions go through
    // NDialogInfo / NConsoleWnd::ShowDialog and the answers come back through the dialog box itself
    // C2: not placed (no evidence for where it went): UBOOL bEnchantMode;              // +0x148 (SetEnchantMode) the cursor stays the scroll's   // C2: offset derived, no evidence
    UBOOL bDblClicked;               // +0x168 the release after a double click moves nothing
    FVector DropLocation;            // +0x16C the ground point under the mouse (OnLButtonUp: UGameEngine::MouseHitLocation +0x21C)
    UTexture* OutlineDownTex;        // +0x178 "l2ui_ch3.InventoryWnd.Inventory_OutLine_down" (the 6-argument constructor)
    UTexture* OutlineTex;            // +0x17C "l2ui_ch3.InventoryWnd.Inventory_OutLine": the frames of the icons that are not selected

    NCInvenItemWnd();                                                // 0x10099790
    // C3: the cell gaps too (ret 0x18)
    NCInvenItemWnd(INT InColumns, INT InVisibleRows, INT InMaxItems, INT InCellSize, INT InGapX, INT InGapY); // 0x100997b0
    // ~NCInvenItemWnd                                               // 0x100782e0 (C1 only: no C2 match) (C1 only: no C2 match)

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10093ce0 (folded with NCPetInvenItemWnd's)
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x10095210
    virtual INT OnMouseMove(WPARAM wParam, LPARAM lParam);           // 67 0x10095430
    virtual INT OnLButtonDown(WPARAM wParam, LPARAM lParam);         // 68 0x10094e40
    virtual INT OnLButtonUp(WPARAM wParam, LPARAM lParam);           // 69 0x100976c0
    virtual INT OnRButtonDown(WPARAM wParam, LPARAM lParam);         // 70 0x10094be0
    // names ours
    virtual INT OnLButtonDblClick(WPARAM wParam, LPARAM lParam);     // 81 0x10094ed0 (new in C2) WM_LBUTTONDBLCLK

    // names ours
    UBOOL UpdateItem(NItemInfo* Info);                               // 0x100950e0 0: not here
    INT GetAdena();                                                  // 0x100951e0
    UBOOL AddItem(NItemInfo* Info);                                  // 0x10098170 0 when full
    UBOOL RemoveItem(INT ObjectID);                                  // 0x100989c0
    // NCPetInvenItemWnd's bodies (0x100b35b0, 0x100b3600): unhooked copies
    __declspec(noinline) UBOOL HasItem(INT ObjectID);
    __declspec(noinline) UBOOL FindItem(INT ObjectID, INT* Index);
};
// NW_ASSERT_OFFSET(NCInvenItemWnd, bEnchantMode, 0x148);   // C2: field not mapped
NW_ASSERT_OFFSET(NCInvenItemWnd, bDblClicked, 0x168);
NW_ASSERT_OFFSET(NCInvenItemWnd, DropLocation, 0x16C);
NW_ASSERT_SIZE(NCInvenItemWnd, 0x180);

#pragma pack(pop)