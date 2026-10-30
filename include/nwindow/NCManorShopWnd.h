#pragma once
// NCManorShopWnd - the manor's seed and crop shop (UClass "CManorShopWnd", 0x110; desktop +0xE4, NCConsole::NewWndE4):
// C2's copy of NCShopWnd (NCShopWnd.h): the same two grids with the arrows between them, the total and the player's
// adena, OK and Cancel, the same texts and requests. The class declares NCShopWnd as its super and then repeats the
// shop's fields (+0xD4 .. +0x10F, the shop's +0x98 .. +0xD3 moved by 0x3C): its constructor runs the shop's (which
// sets the inherited copies) and sets none of its own, and every body below works on the copies. The console's
// manor virtuals (NConsoleWnd 65..68: StartManorBuyList / StartManorSellList, AddManorBuyList / AddManorSellList)
// open it in one mode or the other (Clear, SetMode, SetListID, SetAdena) and fill the grids (AddShopItem /
// AddPlayerItem).
//
// What differs from the shop's code: the grids (NCManorShopItemWnd.h) move whole entries (the NPC's goods do not
// stay), the window procedure repeats the OK button's test after the shop's procedure (which has answered it: the
// repeat is never reached), and there is no ReloadTexts.

#include "nwindow/NCShopWnd.h"
#include "nwindow/NCManorShopItemWnd.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCManorShopWnd : public NCShopWnd
{
    NW_DECLARE_CLASS(NCManorShopWnd, NCShopWnd, 0, 0x102E4A40)

    // the fields repeat the shop's (and hide them: the inherited ones are the shop's, unused here)
    NCManorShopItemWnd* PlayerItems; // +0x124 SHOPSIDE_Player: below when buying, on top when selling (the shop's +0x98)
    NCManorShopItemWnd* ShopItems;   // +0x128 SHOPSIDE_Shop: on top when buying, below when selling (+0x9C)
    INT UnknownDC;                   // +0x12C (+0xA0)   // C2: offset derived, no evidence   // C2: offset derived, no evidence
    UTexture* BackTex;               // +0x130 "L2UI.StoreWnd.StoreBack" (+0xA4)
    INT UnknownE4;                   // +0x134 (+0xA8)   // C2: offset derived, no evidence   // C2: offset derived, no evidence
    NCButton* UpButton;              // +0x138 (+0xAC)
    NCButton* DownButton;            // +0x13C (+0xB0)
    NCButton* OKButton;              // +0x140 (+0xB4)
    NCButton* CancelButton;          // +0x144 (+0xB8)
    NCWnd* UnknownF8;                // +0x148 OnDestroy's, never created (+0xBC)
    INT Mode;                        // +0x14C SHOPMODE_* (+0xC0)
    INT ListID;                      // +0x150 (-1 after Clear) the NPC's list, sent back with the request (+0xC4)
    INT Adena;                       // +0x154 the player's (+0xC8)
    SQWORD TotalPrice;               // +0x158 UpdateTotalPrice (+0xCC)

    NCManorShopWnd();                                                // 0x100b0e70
    // ~NCManorShopWnd                                               // 0x100b0ec0

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x100af0f0
    virtual INT OnCreate();                                          // 37 0x100b0f10
    virtual INT OnDestroy();                                         // 38 0x100af990
    virtual void HideWindow();                                       // 49 0x100af1b0
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x100af210
    // names ours (the same slots as NCShopWnd's)
    virtual INT OnOK();                                              // 74 0x100af710
    virtual INT OnCancel();                                          // 75 0x100af1c0
    // slots 70, 71 and 72 are the shop's placeholders (NewVirtual70..72); the bodies below are hooked at their addresses
    virtual void NewVirtual76();                                     // 76 0x100b01f0 (this shop's own OnUp, not NCShopWnd's)
    virtual void NewVirtual77();                                     // 77 0x100b04b0
    virtual void NewVirtual78();                                     // 78 0x100af1e0
    INT OnUp();                                                      // 0x100b01f0: slot 70
    INT OnDown();                                                    // 0x100b04b0: slot 71
    INT OnClose();                                                   // 0x100af1e0: slot 72 (OnCancel's work and a sound)

    // names ours (the console calls most of these)
    void SetListID(INT InListID);                                    // 0x100af150
    void SetAdena(INT InAdena);                                      // 0x100af160
    __declspec(noinline) void Clear();                               // 0x100af170 grids emptied, list id -1, adena and total 0
    __declspec(noinline) void UpdateLayout();                        // 0x100afae0 the grids placed by mode
    __declspec(noinline) void UpdateTotalPrice();                    // 0x100afc60
    void SetMode(INT InMode);                                        // 0x100b0190 the grids' too, then the layout
    UBOOL AddShopItem(NItemInfo* Info);                              // 0x100b01d0
    UBOOL AddPlayerItem(NItemInfo* Info);                            // 0x100b01e0
};
NW_ASSERT_OFFSET(NCManorShopWnd, PlayerItems, 0x124);
NW_ASSERT_OFFSET(NCManorShopWnd, ShopItems, 0x128);
NW_ASSERT_OFFSET(NCManorShopWnd, BackTex, 0x130);
NW_ASSERT_OFFSET(NCManorShopWnd, UpButton, 0x138);
NW_ASSERT_OFFSET(NCManorShopWnd, UnknownF8, 0x148);
NW_ASSERT_OFFSET(NCManorShopWnd, Mode, 0x14C);
NW_ASSERT_OFFSET(NCManorShopWnd, ListID, 0x150);
NW_ASSERT_OFFSET(NCManorShopWnd, Adena, 0x154);
NW_ASSERT_OFFSET(NCManorShopWnd, TotalPrice, 0x158);
NW_ASSERT_SIZE(NCManorShopWnd, 0x160);

#pragma pack(pop)
