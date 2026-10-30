#pragma once
// NCPrivateBuyWnd - the private store (buy) window (UClass "CPrivateBuyWnd", 0xD4): the sell
// window's twin (NCPrivateShopWnd.h, the same fields at the same places; several bodies are
// shared) for a store that buys. Setting one's own up (Mode 1): the inventory on top as a
// catalog, the wish list below with the prices offered and the amounts wanted; at someone's
// (Mode 0): the items the store wants on top, the ones sold to it below. The desktop keeps it
// at +0x134.
//
// NCPrivateBuyItemWnd - its two grids (UClass "CPrivateBuyItemWnd", 0x12C), Side and Mode as in
// NCPrivateShopItemWnd; NItemEntry::UnknownC34 is the amount the store still wants. At
// someone's store the inventory's items the player lacks (count 0) are covered.

#include "nwindow/NCItemWnd.h"
#include "nwindow/NCFrameCtrl.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCButton;

// what the dialog box sends the buy store windows (NCDialogBox::OnOK), names ours
enum
{
    NWM_BUYSTORE_PRICE = (INT)0xF000000B,      // dialog 0x26: the price typed
    NWM_BUYSTORE_AMOUNT = (INT)0xF000000C,     // dialog 0x27: the amount typed
    NWM_BUYSTORE_PRICE_OK = (INT)0xF000000D,   // dialog 0x29: the unusual price confirmed
    NWM_BUYSTORE_PRICE_CANCEL = (INT)0xF000000E, // never sent
    NWM_BUYSTORE_SELL_OK = (INT)0xF000000F,    // dialog 0x2A: selling at unusual prices confirmed
};

// NCPrivateBuyFrameCtrl - the buy store's title bar (UClass "CPrivateBuyFrameCtrl", 0x238; new in C3): the close button
// does the window's own close (the store is told to quit when it is one's own, the lists are emptied)
class NCPrivateBuyFrameCtrl : public NCFrameCtrl
{
    NW_DECLARE_CLASS(NCPrivateBuyFrameCtrl, NCFrameCtrl, 0, 0x102F7888)

    NCPrivateBuyFrameCtrl();                                         // 0x100e6030 (NCFrameCtrl's and this vtable)
    virtual INT OnClose();                                           // 76 0x100e3ab0
    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x100b14c0 (the stock vtable of this class overrides it; defined in NNewVirtuals.gen.cpp)
};
NW_ASSERT_SIZE(NCPrivateBuyFrameCtrl, 0x26C);

class NCPrivateBuyItemWnd : public NCItemWnd
{
    NW_DECLARE_CLASS(NCPrivateBuyItemWnd, NCItemWnd, 0, 0x102F7358)

    UTexture* CoverTex;              // +0x168 "NWindow.Chat", then "NWindow.ChatBack" (OnPaint)
                                     //   over the items the player lacks
    NCPrivateBuyItemWnd* Partner;    // +0x16C the other grid (SetPartner), or the one an item
                                     //   was carried onto
    INT Mode;                        // +0x170 the store window's (SetMode)
    INT Side;                        // +0x174 0: the store's list, 1: the player's side (SetSide)
    INT Unknown128;                  // +0x178 cleared, never read here

    __declspec(noinline) NCPrivateBuyItemWnd();                      // 0x100e60d0 (the stock OnCreate calls it: not inlined there)
    __declspec(noinline) NCPrivateBuyItemWnd(INT InColumns, INT InVisibleRows, INT InMaxItems, INT InCellSize, INT InGapX, INT InGapY); // 0x100e61c0
    // ~NCPrivateBuyItemWnd                                          // 0x100e6170

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x100e3160
    virtual INT OnDestroy();                                         // 38 0x100efac0 (shared)
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x100e34c0
    virtual INT OnLButtonUp(WPARAM wParam, LPARAM lParam);           // 69 0x100e5910
    // names ours
    virtual INT OnLButtonDblClick(WPARAM wParam, LPARAM lParam);     // 81 0x100e5c90
    virtual INT OnPriceCancel(WPARAM wParam, LPARAM lParam) { return 1; } // 82 0x100e6a50 (folded)
    virtual INT OnPrice(WPARAM wParam, LPARAM lParam);               // 83 0x100e38c0
    virtual INT OnAmount(WPARAM wParam, LPARAM lParam);              // 84 0x100e5fb0
    virtual INT OnPriceOK(WPARAM wParam, LPARAM lParam);             // 85 0x100e37c0

    // names ours
    __declspec(noinline) void SetPartner(NCPrivateBuyItemWnd* InPartner);                 // 0x100efad0 (shared with the
                                                                     //   NPC shops' grids)
    __declspec(noinline) void SetMode(INT InMode);                   // 0x100a28d0 (C1 only: no C2 match) (shared: the stock callers call it, [this+0x150] = InMode)
    __declspec(noinline) void SetSide(INT InSide);                   // 0x100e3240 (called by the stock OnCreate)
    UBOOL AddItem(NItemInfo* Info);                                  // 0x100e4bf0 0 when full
    __declspec(noinline) UBOOL AddEntry(NItemEntry* Src, INT Count);                      // 0x100e4d20 a copy, 0 when full
    // to Partner; 1, or 0 when the wish list would pass the limit (OnAmount returns it)
    __declspec(noinline) INT MoveItem(NItemEntry* Item, INT Count);                       // 0x100e4e20
};
NW_ASSERT_OFFSET(NCPrivateBuyItemWnd, Partner, 0x16C);
NW_ASSERT_OFFSET(NCPrivateBuyItemWnd, Side, 0x174);
NW_ASSERT_SIZE(NCPrivateBuyItemWnd, 0x17C);

// 4-byte packing: TotalPrice sits at +0xCC
#pragma pack(push, 4)
class NCPrivateBuyWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCPrivateBuyWnd, NCWnd, 0, 0x102F6E20)

    NCPrivateBuyItemWnd* PlayerItems;    // +0xE8 Side 1: the inventory (Mode 1), the items sold
                                         //   (Mode 0)
    NCPrivateBuyItemWnd* StoreItems;     // +0xEC Side 0: the items wanted
    INT Unknown9C;                       // +0xF0   // C2: offset derived, no evidence
    UTexture* BackTex;                   // +0xF4 "L2UI.StoreWnd.StoreBack"
    INT UnknownA4;                       // +0xF8   // C2: offset derived, no evidence
    NCButton* UpButton;                  // +0xFC
    NCButton* DownButton;                // +0x100
    NCButton* StartButton;               // +0x104 "Start" (Mode 1) / "OK" (Mode 0)
    NCButton* MessageButton;             // +0x108 Mode 1 only
    NCButton* AbortButton;               // +0x10C Mode 1 only
    NCWnd* UnknownBC;                    // +0x110 a child the destructor would remove; not made   // C2: offset derived, no evidence
    INT Mode;                            // +0x114 0: selling to someone's store, 1: one's own
    INT OwnerID;                         // +0x118 (-1) object id of the store's owner (Mode 0)
    INT Adena;                           // +0x11C the player's (SetAdena)   // C2: offset derived, no evidence
    SQWORD TotalPrice;                   // +0x120 the list's (UpdateTotal)   // C2: offset derived, no evidence
    INT ItemLimit;                       // +0x128 the "(n/limit)" shown beside the wish list's heading in Mode 1 (OnPaint); no code of this
                                         //   client writes it, so it stays 0 from the allocation
    NCPrivateBuyFrameCtrl* BuyFrame;     // +0x12C the title bar (C3: made here, not the base's FrameCtrl)

    NCPrivateBuyWnd();                                               // 0x100e6250
    // ~NCPrivateBuyWnd                                              // 0x100e62f0

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x100e3250
    virtual INT OnCreate();                                          // 37 0x100e6340
    virtual INT OnDestroy();                                         // 38 0x100e4090
    virtual void ShowWindow();                                       // 48 0x100e33c0
    virtual void HideWindow();                                       // 49 0x100e3440
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x100e3b00
    // names ours
    virtual INT OnStart();                                           // 74 0x100e4410
    virtual INT OnUp();                                              // 75 0x100e5410
    virtual INT OnDown();                                            // 76 0x100e56e0
    virtual INT OnClose();                                           // 77 0x100e3450
    virtual INT OnMessage();                                         // 78 0x100e4920
    virtual INT OnAbort();                                           // 79 0x100e3490
    virtual INT OnSellConfirmed(WPARAM wParam, LPARAM lParam);       // 80 0x100e4a00

    // names ours
    void SetAdena(INT InAdena);                                      // 0x100e3370 (hooked as NCNumber11CExt::Set: +0x11C)
    void SetOwnerID(INT InOwnerID);                                  // 0x101029e0 (hooked as NCNumber118Ext::Set: +0x118)
    __declspec(noinline) void Reset();                                                    // 0x100e3380 (the sell window has its own, 0x100e6b90)
    __declspec(noinline) void UpdateLayout();                                             // 0x100e4290 (the sell window's is 0x100e7480)
    void SetMode(INT InMode);                                        // 0x100e5390
    void AddStoreItem(NItemInfo* Info);                              // 0x100e53d0
    UBOOL AddPlayerItem(NItemInfo* Info);                            // 0x100e5400
    __declspec(noinline) UBOOL CheckTotal(INT ItemID, INT Count, INT Price);              // 0x100e7a40 (name ours; the sell window's body, hooked there; the grids call it on the desktop's buy window)
    __declspec(noinline) void UpdateTotal();                                              // 0x100e4810 (anonymous in the db; the sell window's is 0x100e7b10)
};
#pragma pack(pop)
NW_ASSERT_OFFSET(NCPrivateBuyWnd, BackTex, 0xF4);
NW_ASSERT_OFFSET(NCPrivateBuyWnd, Mode, 0x114);
NW_ASSERT_OFFSET(NCPrivateBuyWnd, TotalPrice, 0x120);
NW_ASSERT_OFFSET(NCPrivateBuyWnd, ItemLimit, 0x128);
NW_ASSERT_OFFSET(NCPrivateBuyWnd, BuyFrame, 0x12C);
NW_ASSERT_SIZE(NCPrivateBuyWnd, 0x130);

#pragma pack(pop)