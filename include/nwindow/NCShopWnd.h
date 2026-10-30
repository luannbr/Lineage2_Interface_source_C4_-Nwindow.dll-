#pragma once
// NCShopWnd - an NPC's shop (UClass "CShopWnd", 0x124 bytes in C4; C3: 0xD0): two grids with the arrows between
// them, the total and the player's adena under them, OK and Cancel. Mode 0 buys: the NPC's
// goods on top (they stay there; a stack asks how many whatever its count), what is bought
// below; mode 1 sells: the inventory on top, what is sold below (at most what there is). The
// total is the price times the amount of each item below, in 64 bits (drawn with "%d": its
// low half). OK sends the lower grid's list to the server (RequestBuyItem /
// RequestSellItem). The console opens the window in one mode or the other (Clear, SetMode,
// SetListID, SetAdena) and fills the grids (AddShopItem / AddPlayerItem).
//
// NCShopItemWnd - its grids (UClass "CShopItemWnd", 0x174 bytes in C4; C3: 0x124): the warehouse's (NCStoreWnd.h)
// with the prices (NItemEntry::UnknownC20, +0x1B2C in C4) in the tool tips, and the rules of the mode and side
// when an item moves.
//
// Desktop (NCConsole) +0x134 is this window (C4; C3: +0xD0).

#include "nwindow/NCItemWnd.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCButton;

// NWM_COMMAND ids (names ours): NCStoreWnd's (NID_STORE_*), the window procedure is one body

// NCShopWnd::Mode, NCShopItemWnd::Mode and Side (names ours)
enum
{
    SHOPMODE_Buy = 0,
    SHOPMODE_Sell = 1,
    SHOPMODE_Preview = 2,            // C3 (name ours): the preview window's (NCShopPreviewWnd.h): the goods move between
                                     //   the lists - tried on, not bought - and leave the NPC's list while they are below
    SHOPSIDE_Shop = 0,               // the NPC's goods when buying, what is sold when selling
    SHOPSIDE_Player = 1,             // what is bought when buying, the inventory when selling
};

class NCShopItemWnd : public NCItemWnd
{
    NW_DECLARE_CLASS(NCShopItemWnd, NCItemWnd, 0, 0x103067B0)

    NCShopItemWnd* Target;           // +0x168 where its items go (the other grid)   // C2: offset derived, no evidence
    INT Mode;                        // +0x16C the window's (SHOPMODE_*)   // C2: offset derived, no evidence
    INT Side;                        // +0x170 SHOPSIDE_*   // C2: offset derived, no evidence   // C2: offset derived, no evidence

    NCShopItemWnd();                                                 // 0x10105040
    NCShopItemWnd(INT InColumns, INT InVisibleRows, INT InMaxItems, INT InCellSize, INT InGapX, INT InGapY); // 0x101050e0
    // ~NCShopItemWnd                                                // 0x10105090

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10102860 (shared with the other grids)
    virtual INT OnDestroy();                                         // 38 0x100efac0 (shared with the other grids)
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x10102aa0
    virtual INT OnLButtonUp(WPARAM wParam, LPARAM lParam);           // 69 0x10104ac0
    // slots 75 and 76 are placeholders (the stock WindowProc calls them); the bodies below are hooked at their addresses
    // names ours
    INT OnLButtonDblClick(WPARAM wParam, LPARAM lParam);     // 0x10104d70: slot 75, WM_LBUTTONDBLCLK
    INT OnDialogResult(WPARAM wParam, LPARAM lParam);        // 0x10104fe0: slot 76

    // names ours
    // C4 bodies, shared with the other grids' setters of the same field (one `mov [ecx + N], eax` each, found by scanning the C4
    // image for the pattern): Target +0x168 is 0x10084BD0 (hooked as NCDeliverItemWnd::SetTarget), Mode +0x16C is 0x100EFAD0
    // (NCManorShopItemWnd::SetMode), Side +0x170 is 0x101028E0 (hooked here). SetTarget / SetMode are plain C++ here (no hook)
    __declspec(noinline) void SetTarget(NCShopItemWnd* InTarget);    // 0x10084BD0 (shared)
    __declspec(noinline) void SetMode(INT InMode);                   // 0x100EFAD0 (shared)
    __declspec(noinline) void SetSide(INT InSide);                   // 0x101028E0 (shared; the hook is NCShopItemWnd::SetSide)
    UBOOL AddItem(NItemInfo* Info);                                  // 0x101039e0 0 when full
    UBOOL AddCopy(NItemEntry* Src, INT Count);                       // 0x10103b40 0 when full
    void MoveItem(NItemEntry* Item, INT Count);                      // 0x10103c70 to Target
    virtual void NewVirtual81();  // 81 0x10104d70 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual82();  // 82 0x10104fe0 (only this client has it; defined in NNewVirtuals.gen.cpp)
};
NW_ASSERT_OFFSET(NCShopItemWnd, Target, 0x168);
NW_ASSERT_OFFSET(NCShopItemWnd, Side, 0x170);
NW_ASSERT_SIZE(NCShopItemWnd, 0x174);

class NCShopWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCShopWnd, NCWnd, 0, 0x10306CE0)

    NCShopItemWnd* PlayerItems;      // +0xE8 SHOPSIDE_Player: below when buying, on top when
                                     //   selling
    NCShopItemWnd* ShopItems;        // +0xEC SHOPSIDE_Shop: on top when buying, below when
                                     //   selling
    INT Unknown9C;                   // +0xF0
    UTexture* BackTex;               // +0xF4 "L2UI.StoreWnd.StoreBack"
    INT UnknownA4;                   // +0xF8
    NCButton* UpButton;              // +0xFC
    NCButton* DownButton;            // +0x100
    NCButton* OKButton;              // +0x104
    NCButton* CancelButton;          // +0x108
    NCWnd* UnknownB8;                // +0x10C OnDestroy's, never created
    INT Mode;                        // +0x110 SHOPMODE_*   // C2: offset derived, no evidence
    INT ListID;                      // +0x114 (-1) the NPC's list, sent back with the request
    INT Adena;                       // +0x118 the player's
    SQWORD TotalPrice;               // +0x11C UpdateTotalPrice

    NCShopWnd();                                                     // 0x10105120
    // ~NCShopWnd                                                    // 0x100b0d10

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x101028f0 (shared)
    INT OnCreate();                                          // 37 0x10105190
    INT OnDestroy();                                         // 38 0x10118260 (C4; hooked in NCShopWnd.cpp)
    virtual void ShowWindow();                                       // 48 0x100efc40 (folded)
    virtual void HideWindow();                                       // 49 0x10102a30
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x10102cc0
    // names ours
    virtual INT OnOK();                                              // 74 0x10103350
    virtual INT OnCancel();                                          // 75 0x10102a40
    virtual void NewVirtual76();  // 76 0x10104560 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual77();  // 77 0x10104820 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual78();  // 78 0x10102a70 (only this client has it; defined in NNewVirtuals.gen.cpp)
    // slots 70, 71 and 72 are placeholders; the bodies below are hooked at their addresses (the stock WindowProc, the
    // one of NCStoreWnd, calls them through the slots)
    INT OnUp();                                              // 0x10104560: slot 70
    INT OnDown();                                            // 0x10104820: slot 71
    INT OnClose();                                           // 0x10102a70: slot 72 (OnCancel's work and a sound)

    // names ours (the console calls most of these)
    // C4: the body that stores +0x118 (Adena) is 0x101029E0 (hooked by the console's file as NCNumber118Ext::Set); the one at
    // 0x101029D0 stores +0x114 (ListID) and is the db's NCStoreWnd::SetAdena: SetListID. Both are plain C++ here (no hook)
    void SetAdena(INT InAdena);                                      // 0x101029E0 (+0x118)
    void SetListID(INT InListID);                                    // 0x101029D0 (+0x114, shared)
    void Clear();                                                    // 0x101029f0 grids emptied,
                                                                     //   list id -1, adena and
                                                                     //   total 0
    __declspec(noinline) void UpdateLayout();                        // 0x101031c0 the grids placed
                                                                     //   by mode
    __declspec(noinline) void UpdateTotalPrice();                                         // 0x101036f0 (the preview's too)
    void ReloadTexts();                                              // 0x10103800 item names and
                                                                     //   descriptions, tool tips
    INT GetMode();                                                   // 0x10102A60 (hooked in NCShopWnd.cpp: `mov eax, [ecx + 0x110]`)
    void SetMode(INT InMode);                                        // 0x10104500 the grids' too,
                                                                     //   then the layout
    UBOOL AddShopItem(NItemInfo* Info);                              // 0x10104540
    UBOOL AddPlayerItem(NItemInfo* Info);                            // 0x10104550
};
NW_ASSERT_OFFSET(NCShopWnd, BackTex, 0xF4);
NW_ASSERT_OFFSET(NCShopWnd, Mode, 0x110);
NW_ASSERT_OFFSET(NCShopWnd, TotalPrice, 0x11C);
NW_ASSERT_SIZE(NCShopWnd, 0x124);

#pragma pack(pop)