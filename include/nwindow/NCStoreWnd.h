#pragma once
// NCStoreWnd - the warehouse window (UClass "CStoreWnd", 0xC8): two grids with the arrows
// between them, the fee and the player's adena under them, OK and Cancel. Items go from one
// grid to the other with the arrows, a drag or a double click (how many first, for a stack).
// Mode 0 deposits: the inventory on top, what goes into the warehouse below (30 adena a slot);
// mode 1 withdraws: the warehouse's items on top, what comes back below (no fee). OK sends the
// lower grid's list to the server. The console opens the window in one mode or the other
// (Clear, SetMode, SetAdena) and fills the grids (AddInventoryItem / AddStoredItem).
//
// NCStoreItemWnd - its grids (UClass "CStoreItemWnd", 0x124): an item goes to Target, the
// other grid (a drop onto another warehouse grid makes that one the target); an entry's count
// is the amount it holds (NItemEntry::TradeCount). The tool tips are the inventory's
// (NItemToolTip.h).
//
// Desktop (NCConsole) +0xCC is this window.
//
// NCGMStoreWnd - the same window for a GM looking at a character's warehouse (UClass
// "CGMStoreWnd", 0xC8): the arrows, OK and Cancel are disabled.

#include "nwindow/NCItemWnd.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCButton;

// NWM_COMMAND ids (names ours; NCShopWnd shares this window procedure)
enum
{
    NID_STORE_FRAME = 0xF3,
    NID_STORE_OK = 0xF4,
    NID_STORE_CANCEL = 0xF5,
    NID_STORE_UP = 0xF6,
    NID_STORE_DOWN = 0xF7,
    NID_STORE_CLOSE = 0xF8,          // handled, but no control of the window sends it
};

// NCStoreWnd::Mode (names ours)
enum
{
    STOREMODE_Deposit = 0,
    STOREMODE_Withdraw = 1,
};

class NCStoreItemWnd : public NCItemWnd
{
    NW_DECLARE_CLASS(NCStoreItemWnd, NCItemWnd, 0, 0x1030C008)

    NCStoreItemWnd* Target;          // +0x168 where its items go (the other grid)
    INT Mode;                        // +0x16C the window's (STOREMODE_*); the shop's grid reads   // C2: offset derived, no evidence   // C2: offset derived, no evidence   // C2: offset derived, no evidence
                                     //   its copy, this one does not
    INT Side;                        // +0x170 0 the warehouse's grid, 1 the inventory's (not   // C2: offset derived, no evidence   // C2: offset derived, no evidence
                                     //   read here either)

    NCStoreItemWnd();                                                // 0x10119510
    NCStoreItemWnd(INT InColumns, INT InVisibleRows, INT InMaxItems, INT InCellSize, INT InGapX, INT InGapY); // 0x101195b0
    // ~NCStoreItemWnd                                               // 0x10119560

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10102860 (shared)
    virtual INT OnDestroy();                                         // 38 0x100efac0 (shared)
    virtual INT OnLButtonUp(WPARAM wParam, LPARAM lParam);           // 69 0x10119170
    // names ours
    virtual INT OnLButtonDblClick(WPARAM wParam, LPARAM lParam);     // 81 0x10119340 WM_LBUTTONDBLCLK
    virtual INT OnDialogResult(WPARAM wParam, LPARAM lParam);        // 82 0x101194b0

    // names ours; the three setters are one body each with other grids' (shops, private
    // stores), the last two hooked here
    __declspec(noinline) void SetTarget(NCStoreItemWnd* InTarget);   // 0x10088870 (C1 only: no C2 match) (shared)
    __declspec(noinline) void SetMode(INT InMode);                                        // 0x100efad0 (shared with the shops' and the private stores' grids)
    __declspec(noinline) void SetSide(INT InSide);                   // 0x100a28d0 (C1 only: no C2 match) (shared)
    UBOOL AddItem(NItemInfo* Info);                                  // 0x101188b0 0 when full
    UBOOL AddCopy(NItemEntry* Src, INT Count);                       // 0x101189c0 0 when full
    void MoveItem(NItemEntry* Item, INT Count);                      // 0x10118aa0 to Target
};
NW_ASSERT_OFFSET(NCStoreItemWnd, Target, 0x168);
NW_ASSERT_OFFSET(NCStoreItemWnd, Side, 0x170);
NW_ASSERT_SIZE(NCStoreItemWnd, 0x174);

class NCStoreWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCStoreWnd, NCWnd, 0, 0x1030C538)

    NCStoreItemWnd* InventoryItems;  // +0xE8 the inventory's side: on top when depositing,
                                     //   below (what comes back) when withdrawing
    NCStoreItemWnd* StoredItems;     // +0xEC the warehouse's side: below (what goes in) when
                                     //   depositing, on top when withdrawing
    INT Unknown9C;                   // +0xF0   // C2: offset derived, no evidence
    UTexture* BackTex;               // +0xF4 "L2UI.StoreWnd.StoreBack"
    INT UnknownA4;                   // +0xF8   // C2: offset derived, no evidence
    NCButton* UpButton;              // +0xFC
    NCButton* DownButton;            // +0x100
    NCButton* OKButton;              // +0x104
    NCButton* CancelButton;          // +0x108
    NCWnd* UnknownB8;                // +0x10C OnDestroy's, never created   // C2: offset derived, no evidence
    INT Mode;                        // +0x110 STOREMODE_*
    INT Adena;                       // +0x114 the player's
    INT Fee;                         // +0x118 UpdateFee
    INT StoreType;                   // +0x11C C4 (name ours): the kind of warehouse, 1 .. 4 (NConsoleWnd::StartDepositList /
                                     //   StartWithdrawList give it first, 0x10117D40: the title is sysstring 0x4C0, 0x4C1, 0x4C2 or
                                     //   0x83); the paint shows the "(n/max)" count of the warehouse's grid for every kind but 4
    INT Capacity;                    // +0x120 C4 (name ours): how many items the warehouse of kind 1 holds (a packet of the console,
                                     //   NConsoleWnd::C4Virtual085); the other kinds show 200

    NCStoreWnd();                                                    // 0x101195f0
    // ~NCStoreWnd                                                   // 0x10073bd0 (C1 only: no C2 match) (C1 only: no C2 match)

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x101028f0 (shared)
    virtual INT OnCreate();                                          // 37 0x10119650
    virtual INT OnDestroy();                                         // 38 0x10118260 (shared)
    virtual void ShowWindow();                                       // 48 0x10118540
    virtual void HideWindow();                                       // 49 0x10117cb0
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x10117db0
    // names ours
    virtual INT OnOK();                                              // 74 0x10118550
    virtual INT OnCancel();                                          // 75 0x10117cc0
    virtual INT OnUp();                                              // 76 0x10118c10
    virtual INT OnDown();                                            // 77 0x10118ec0
    virtual INT OnClose();                                           // 78 0x10117d10 (OnCancel's body)

    // names ours (the console calls most of these)
    void SetMode(INT InMode);                                        // 0x10117c50 the grids' too
    void SetAdena(INT InAdena);                                      // 0x10116b20 (C1 only: no C2 match) (shared)
    void Clear();                                                    // 0x10117c80 grids emptied,
                                                                     //   adena and fee 0
    __declspec(noinline) void UpdateFee();                                                // 0x10117ce0
    INT GetMode();                                                   // 0x101189a0 (C1 only: no C2 match) (shared)
    __declspec(noinline) void UpdateLayout();                        // 0x101183b0 the grids placed
                                                                     //   by mode
    void ReloadTexts();                                              // 0x101187a0 item names and
                                                                     //   descriptions, tool tips
    UBOOL AddInventoryItem(NItemInfo* Info);                         // 0x10118bf0
    UBOOL AddStoredItem(NItemInfo* Info);                            // 0x10118c00
};
NW_ASSERT_OFFSET(NCStoreWnd, BackTex, 0xF4);
NW_ASSERT_OFFSET(NCStoreWnd, Mode, 0x110);
NW_ASSERT_SIZE(NCStoreWnd, 0x124);

class NCGMStoreWnd : public NCStoreWnd
{
    NW_DECLARE_CLASS(NCGMStoreWnd, NCStoreWnd, 0, 0x102D5B08)

    NCGMStoreWnd();                                                  // 0x100903c0
    // ~NCGMStoreWnd                                                 // 0x100766a0 (C1 only: no C2 match) (C1 only: no C2 match)

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10089210
    virtual INT OnCreate();                                          // 37 0x10089230
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x1008cb80
};
NW_ASSERT_SIZE(NCGMStoreWnd, 0x124);

#pragma pack(pop)