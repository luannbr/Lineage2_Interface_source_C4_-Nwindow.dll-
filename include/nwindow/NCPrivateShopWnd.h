#pragma once
// NCPrivateShopWnd - the private store (sell) window (UClass "CPrivateShopWnd", 0xD4), in two
// modes: setting one's own store up (Mode 1: the inventory on top, the items on sale with
// their prices below; Start, Message, Abort) or buying at someone's (Mode 0: the store's items
// on top, the ones chosen below; OK). The arrows and drags move items between the two grids
// (how many first, for a stack; the price first, going on sale), and the window adds up the
// price of the list that is sent. The console fills it (SetMode, the items, SetAdena); the
// desktop (NCConsole) keeps it at +0x130.
//
// NCPrivateShopItemWnd - its two grids (UClass "CPrivateShopItemWnd", 0x128): Side 0 is the
// store's list, Side 1 the player's side. An entry's count is the amount there (NItemEntry::
// TradeCount), its price NItemEntry::UnknownC20 (Unknown1CA8: the item's reference price, a
// price 5 times away from it asks first). The questions go through the desktop's dialog box,
// which answers with the NWM_STORE_* messages.

#include "nwindow/NCItemWnd.h"
#include "nwindow/NCFrameCtrl.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCButton;
class NCCheckBox;

// what the dialog box sends the store windows (NCDialogBox::OnOK / OnCancel), names ours
enum
{
    NWM_STORE_PRICE = (INT)0xF0000006,         // dialog 0x21: the price typed
    NWM_STORE_AMOUNT = (INT)0xF0000007,        // dialog 0x22: the amount typed
    NWM_STORE_PRICE_OK = (INT)0xF0000008,      // dialog 0x24: the unusual price confirmed
    NWM_STORE_PRICE_CANCEL = (INT)0xF0000009,  // dialog 0x24 cancelled
    NWM_STORE_BUY_OK = (INT)0xF000000A,        // dialog 0x25: buying at unusual prices confirmed
};

// NCPrivateShopFrameCtrl - the sell store's title bar (UClass "CPrivateShopFrameCtrl", 0x238; new in C3): the close
// button does the window's own close
class NCPrivateShopFrameCtrl : public NCFrameCtrl
{
    NW_DECLARE_CLASS(NCPrivateShopFrameCtrl, NCFrameCtrl, 0, 0x102F8820)

    NCPrivateShopFrameCtrl();                                        // 0x100e9600 (NCFrameCtrl's and this vtable)
    virtual INT OnClose();                                           // 76 0x100e6cd0
    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x100b14c0 (the stock vtable of this class overrides it; defined in NNewVirtuals.gen.cpp)
};
NW_ASSERT_SIZE(NCPrivateShopFrameCtrl, 0x26C);

class NCPrivateShopItemWnd : public NCItemWnd
{
    NW_DECLARE_CLASS(NCPrivateShopItemWnd, NCItemWnd, 0, 0x102F82F0)

    NCPrivateShopItemWnd* Partner;   // +0x168 the other grid (SetPartner), or the one an item   // C2: offset derived, no evidence
                                     //   was carried onto
    INT Mode;                        // +0x16C the store window's (SetMode)   // C2: offset derived, no evidence   // C2: offset derived, no evidence
    INT Side;                        // +0x170 0: the store's list, 1: the player's side (SetSide)   // C2: offset derived, no evidence   // C2: offset derived, no evidence
    INT Unknown124;                  // +0x174 cleared, never read here   // C2: offset derived, no evidence

    __declspec(noinline) NCPrivateShopItemWnd();                     // 0x100e96a0 (the stock OnCreate calls it: not inlined there)
    __declspec(noinline) NCPrivateShopItemWnd(INT InColumns, INT InVisibleRows, INT InMaxItems, INT InCellSize, INT InGapX, INT InGapY); // 0x100e9740
    // ~NCPrivateShopItemWnd                                         // 0x100e96f0

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x100e6970
    virtual INT OnDestroy();                                         // 38 0x100efac0 (shared)
    virtual INT OnLButtonUp(WPARAM wParam, LPARAM lParam);           // 69 0x100e8b60
    // names ours
    virtual INT OnLButtonDblClick(WPARAM wParam, LPARAM lParam);     // 81 0x100e8ee0
    virtual INT OnPriceCancel(WPARAM wParam, LPARAM lParam) { return 1; } // 82 0x100e6a50 (folded)
    virtual void NewVirtual83();  // 83 0x100e9330 = OnPrice below (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual84();  // 84 0x100e9550 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual85();  // 85 0x100e91f0 (only this client has it; defined in NNewVirtuals.gen.cpp)
    INT OnPrice(WPARAM wParam, LPARAM lParam);               // 0x100e9330: the body of slot 77 (called through the slot)
    INT OnAmount(WPARAM wParam, LPARAM lParam);              // 0x100e9550: slot 78
    INT OnPriceOK(WPARAM wParam, LPARAM lParam);             // 0x100e91f0: slot 79

    // names ours
    __declspec(noinline) void SetPartner(NCPrivateShopItemWnd* InPartner);                // 0x10088870 (C1 only: no C2 match) (hooked as
                                                                     //   NCTradeItemWnd::SetKind)
    __declspec(noinline) void SetMode(INT InMode);                                        // 0x100efad0 (shared with the
                                                                     //   NPC shops' grids)
    __declspec(noinline) void SetSide(INT InSide);                                        // 0x100a28d0 (C1 only: no C2 match) (shared)
    UBOOL AddItem(NItemInfo* Info);                                  // 0x100e7e80 0 when full
    __declspec(noinline) UBOOL AddEntry(NItemEntry* Src, INT Count);                      // 0x100e7fb0 a copy, 0 when full
    // to Partner; 1, or 0 when the list would pass the limit (OnAmount, OnPrice return it)
    __declspec(noinline) INT MoveItem(NItemEntry* Item, INT Count);                       // 0x100e8150
    // a package sale (the window's bPackage) moves the whole list at once; new in C4 (anonymous in the db, name ours)
    __declspec(noinline) void MoveAll();                                                  // 0x100e80a0
};
NW_ASSERT_OFFSET(NCPrivateShopItemWnd, Partner, 0x168);
NW_ASSERT_OFFSET(NCPrivateShopItemWnd, Side, 0x170);
NW_ASSERT_SIZE(NCPrivateShopItemWnd, 0x178);

// 4-byte packing: TotalPrice sits at +0xCC
#pragma pack(push, 4)
class NCPrivateShopWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCPrivateShopWnd, NCWnd, 0, 0x102F7DB8)

    NCPrivateShopItemWnd* PlayerItems;   // +0xE8 Side 1: the inventory (Mode 1), the items
                                         //   chosen (Mode 0)
    NCPrivateShopItemWnd* StoreItems;    // +0xEC Side 0: the items on sale
    INT Unknown9C;                       // +0xF0   // C2: offset derived, no evidence
    UTexture* BackTex;                   // +0xF4 "L2UI.StoreWnd.StoreBack"
    INT UnknownA4;                       // +0xF8   // C2: offset derived, no evidence
    NCButton* UpButton;                  // +0xFC
    NCButton* DownButton;                // +0x100
    NCButton* StartButton;               // +0x104 "Start" (Mode 1) / "OK" (Mode 0)
    NCButton* MessageButton;             // +0x108 Mode 1 only
    NCButton* AbortButton;               // +0x10C Mode 1 only
    NCWnd* UnknownBC;                    // +0x110 a child the destructor would remove; not made   // C2: offset derived, no evidence
    NCCheckBox* PackageCheck;            // +0x114 the package-sale check box (OnCreate: string 1198, id 0x1F0; shown with Mode 1 and
                                         //   told the flag by NCStoreModeWndExt::SetFlag 0x100e7600; Start sends its state)
    INT bPackage;                        // +0x118 the store sells everything as one package (SetFlag); a grid drop at someone's store then
                                         //   takes the whole list (NCPrivateShopItemWnd::MoveAll)
    INT Mode;                            // +0x11C 0: buying at someone's store, 1: one's own (SetMode)
    INT OwnerID;                         // +0x120 (-1) object id of the store's owner (Mode 0)
    INT Adena;                           // +0x124 the player's (SetAdena)   // C2: offset derived, no evidence
    SQWORD TotalPrice;                   // +0x128 the list's (UpdateTotal)   // C2: offset derived, no evidence
    INT ItemLimit;                       // +0x130 the "(n/limit)" shown beside the list's heading in Mode 1 (OnPaint); no code of this
                                         //   client writes it, so it stays 0 from the allocation
    NCPrivateShopFrameCtrl* ShopFrame;   // +0x134 the title bar (C3: made here, not the base's FrameCtrl)

    NCPrivateShopWnd();                                              // 0x100e9790
    // ~NCPrivateShopWnd                                             // 0x100e9840

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x100e6a60
    virtual INT OnCreate();                                          // 37 0x100e9890
    virtual INT OnDestroy();                                         // 38 0x100e72b0 (shared)
    virtual void ShowWindow();                                       // 48 0x100e6bd0 (shared)
    virtual void HideWindow();                                       // 49 0x100e6c50 (shared)
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x100e6d20
    // names ours
    virtual INT OnStart();                                           // 74 0x100e76d0
    virtual INT OnUp();                                              // 75 0x100e8650
    virtual INT OnDown();                                            // 76 0x100e8920
    virtual INT OnClose();                                           // 77 0x100e6c60
    virtual INT OnMessage();                                         // 78 0x100e7c20
    virtual INT OnAbort();                                           // 79 0x100e6ca0
    virtual void NewVirtual80();  // 80 0x100e7d00 (only this client has it; defined in NNewVirtuals.gen.cpp)
    INT OnBuyConfirmed(WPARAM wParam, LPARAM lParam);        // 0x100e7d00: the body of slot 74 (called through it)

    // names ours
    void SetAdena(INT InAdena);                                      // 0x101021f0 (shared with NCShopPreviewWnd::SetOwnerID: +0x124)
    void SetOwnerID(INT InOwnerID);                                  // 0x100e6b80 (the db's SetAdena of C3: it stores +0x120)
    __declspec(noinline) void Reset();                                                    // 0x100e6b90 (the buy window has its own, 0x100e3380)
    __declspec(noinline) void UpdateLayout();                                             // 0x100e7480
    void SetMode(INT InMode);                                        // 0x100e85d0
    void AddStoreItem(NItemInfo* Info);                              // 0x100e8610
    UBOOL AddPlayerItem(NItemInfo* Info);                            // 0x100e8640
    __declspec(noinline) UBOOL CheckTotal(INT ItemID, INT Count, INT Price);              // 0x100e7a40 (one body with the buy window's)
    __declspec(noinline) void UpdateTotal();                                              // 0x100e7b10
};
#pragma pack(pop)
NW_ASSERT_OFFSET(NCPrivateShopWnd, BackTex, 0xF4);
NW_ASSERT_OFFSET(NCPrivateShopWnd, PackageCheck, 0x114);
NW_ASSERT_OFFSET(NCPrivateShopWnd, ItemLimit, 0x130);
NW_ASSERT_OFFSET(NCPrivateShopWnd, bPackage, 0x118);
NW_ASSERT_OFFSET(NCPrivateShopWnd, Mode, 0x11C);
NW_ASSERT_OFFSET(NCPrivateShopWnd, OwnerID, 0x120);
NW_ASSERT_OFFSET(NCPrivateShopWnd, Adena, 0x124);
NW_ASSERT_OFFSET(NCPrivateShopWnd, TotalPrice, 0x128);
NW_ASSERT_OFFSET(NCPrivateShopWnd, ShopFrame, 0x134);
NW_ASSERT_SIZE(NCPrivateShopWnd, 0x138);

#pragma pack(pop)