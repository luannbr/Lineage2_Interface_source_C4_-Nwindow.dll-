#pragma once
// NCPetInventoryWnd - the Items page of the pet window (UClass "CPetInventoryWnd", 0x9C): the
// pet's inventory in an NCPetInvenItemWnd grid, 6 x 2 icons of 32 pixels (160 at most). The
// console fills it through NCPetWnd (AddInventoryItem / UpdateInventoryItem / ClearInventory).
//
// NCPetInvenItemWnd - that grid (UClass "CPetInvenItemWnd", 0x134): equipped items get a frame
// behind their icon; a double click or the right button uses an item (the pet equips / eats
// it), dragging it inside the grid changes its place, onto the player's inventory or paperdoll
// gives it back (how many, for a stack), onto the shortcut bar registers it. The tool tips are
// the inventory's (NItemToolTip.h).

#include "nwindow/NCItemWnd.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCPetInvenItemWnd : public NCItemWnd
{
    NW_DECLARE_CLASS(NCPetInvenItemWnd, NCItemWnd, 0, 0x102F2FC0)

    UTexture* EquippedTex;           // +0x168 "l2ui_ch3.PetWnd.petitem_click" over equipped items (C3 draws it over the icon)
    // the questions asked last (names ours; OnLButtonUp sets them)
    UBOOL bDropPending;              // +0x16C on the ground (nothing here sets it: the   // C2: offset derived, no evidence
                                     //   inventory's code, kept)
    UBOOL bToOwnerPending;           // +0x170 back to the player   // C2: offset derived, no evidence
    UBOOL bDblClicked;               // +0x174 the release after a double click moves nothing
    UTexture* OutlineDownTex;        // +0x178 "l2ui_ch3.InventoryWnd.Inventory_OutLine_down" (loaded, never drawn)
    UTexture* OutlineTex;            // +0x17C "l2ui_ch3.InventoryWnd.Inventory_OutLine" around every icon
    FVector DropLocation;            // +0x180 (never written here: the constructor leaves it)   // C2: offset derived, no evidence

    NCPetInvenItemWnd();                                             // 0x100decf0
    NCPetInvenItemWnd(INT InColumns, INT InVisibleRows, INT InMaxItems, INT InCellSize, INT InGapX, INT InGapY); // 0x100deda0
    // ~NCPetInvenItemWnd                                            // 0x100b6210 (C1 only: no C2 match) (C1 only: no C2 match)

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10093ce0
    virtual INT OnCreate();                                          // 37 0x100db120
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x100dbf50
    virtual INT OnLButtonDown(WPARAM wParam, LPARAM lParam);         // 68 0x100dc170
    virtual INT OnLButtonUp(WPARAM wParam, LPARAM lParam);           // 69 0x100de490
    virtual INT OnRButtonDown(WPARAM wParam, LPARAM lParam);         // 70 0x100dc290
    virtual void Clear();                                            // 78 0x100dea90
    // names ours
    virtual INT OnLButtonDblClick(WPARAM wParam, LPARAM lParam);     // 81 0x100dc200 WM_LBUTTONDBLCLK

    // names ours
    __declspec(noinline) UBOOL AddItem(NItemInfo* Info);             // 0x100de880 (C1 0x10093e50) 0 when full
    UBOOL UpdateItem(NItemInfo* Info);                               // 0x100dc2f0 0: not here
    __declspec(noinline) UBOOL RemoveItem(INT ObjectID);                                  // 0x100cf0e0 (C1 only: no C2 match) (NCTradeItemWnd's too)
    __declspec(noinline) UBOOL HasItem(INT ObjectID);                                     // 0x100dc3f0
    __declspec(noinline) UBOOL FindItem(INT ObjectID, INT* Index);                        // 0x100dc440
};
NW_ASSERT_OFFSET(NCPetInvenItemWnd, EquippedTex, 0x168);
NW_ASSERT_OFFSET(NCPetInvenItemWnd, OutlineTex, 0x17C);
NW_ASSERT_OFFSET(NCPetInvenItemWnd, DropLocation, 0x180);
NW_ASSERT_SIZE(NCPetInvenItemWnd, 0x18C);

class NCPetInventoryWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCPetInventoryWnd, NCWnd, 0, 0x102F3A20)

    UTexture* BackTex;               // +0xE8 "L2UI_CH3.PetWnd.Petinterface_back33"   // C2: offset derived, no evidence
    NCPetInvenItemWnd* Items;        // +0xEC

    NCPetInventoryWnd();                                             // 0x100dedf0
    // ~NCPetInventoryWnd                                            // 0x100b36a0 (C1 only: no C2 match) (C1 only: no C2 match)

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10148f10 (folded)
    virtual INT OnCreate();                                          // 37 0x100dee90
    virtual INT OnDestroy();                                         // 38 0x100dc490 (folded)
    virtual INT OnPaint(NCGDevice* Device);                          // C2: slot gone (was 50) (folded)

    // names ours (NCPetWnd passes the console's calls on)
    UBOOL AddItem(NItemInfo* Info);                                  // 0x100de980 (C1 0x10094010)
    UBOOL UpdateItem(INT Op, NItemInfo* Info);                       // 0x100deaf0 (C1 0x10094030) 1 add, 2 change,
                                                                     //   3 remove
    NItemEntry* GetItem(INT ObjectID);                               // 0x100dc5e0 NULL: none
    void ClearItems();                                               // 0x100b2b90 (C1 only: no C2 match) (C1 only: no C2 match) (C1 0x100bab50) (shared with
                                                                     //   NCTrainListWnd's Clear)
};
NW_ASSERT_OFFSET(NCPetInventoryWnd, Items, 0xEC);
NW_ASSERT_SIZE(NCPetInventoryWnd, 0xF0);

#pragma pack(pop)