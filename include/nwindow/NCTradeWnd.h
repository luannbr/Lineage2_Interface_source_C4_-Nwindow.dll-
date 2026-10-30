#pragma once
// NCTradeWnd - the trade window (UClass "CTradeWnd", 0x100): the player's inventory on top,
// the player's offer and the partner's under it with their names, a down arrow that offers the
// selected inventory item, OK and Cancel (both answer the server: RequestTradeDone). The
// console fills it: Reset and the names when a trade starts, the three grids from the server's
// lists, UpdateInventoryItem for changes to the inventory; the texts again after a language
// change (ReloadTexts).
//
// NCTradeItemWnd - those grids (UClass "CTradeItemWnd", 0x11C). Its UClass names NCWnd as the
// super, but its constructors call NCItemWnd's and its vtable inherits NCItemWnd's bodies. In
// the inventory grid (Kind 0) an item is offered by carrying it onto the offer grid or by a
// double click (how many first, for a stack); the other two grids only show their items. An
// entry's count there is the amount offered (NItemEntry +0xC48); the tool tips are the
// inventory's (NItemToolTip.h).
//
// Desktop (NCConsole) +0xE0 is this window.

#include "nwindow/NCItemWnd.h"
#include "nwindow/NCFrameCtrl.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCButton;

// NWM_COMMAND ids (names ours)
enum
{
    NID_TRADE_FRAME = 0xF3,
    NID_TRADE_OK = 0xF4,
    NID_TRADE_CANCEL = 0xF5,
    NID_TRADE_ADD = 0xF6,            // the down arrow
};

// NCTradeItemWnd::Kind (names ours)
enum
{
    TRADEGRID_Inventory = 0,
    TRADEGRID_MyOffer = 1,
    TRADEGRID_PartnerOffer = 2,
};

// NCTradeFrameCtrl - the trade window's title bar (UClass "CTradeFrameCtrl", 0x238; new in C3): the close button
// answers the trade with "no" before it closes the bar
class NCTradeFrameCtrl : public NCFrameCtrl
{
    NW_DECLARE_CLASS(NCTradeFrameCtrl, NCFrameCtrl, 0, 0x1030D4D0)

    NCTradeFrameCtrl();                                              // 0x1011aef0 (NCFrameCtrl's and this vtable)
    virtual INT OnClose();                                           // 76 0x10119ba0
    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x100b14c0 (the stock vtable of this class overrides it; defined in NNewVirtuals.gen.cpp)
};
NW_ASSERT_SIZE(NCTradeFrameCtrl, 0x26C);

class NCTradeItemWnd : public NCItemWnd
{
    NW_DECLARE_CLASS(NCTradeItemWnd, NCItemWnd, 0, 0x1030CA70)

    INT Kind;                        // +0x168 TRADEGRID_* (SetKind; the constructors leave it)
    UTexture* IconBoxTex;            // +0x16C "L2ui_ch3.etc.iconbox" behind every icon (the 6-argument constructor loads it)

    NCTradeItemWnd();                                                // 0x1011af90
    NCTradeItemWnd(INT InColumns, INT InVisibleRows, INT InMaxItems, INT InCellSize, INT InGapX, INT InGapY); // 0x1011b030
    // ~NCTradeItemWnd                                               // 0x1011afe0

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10102860 (shared)
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x10119d90
    virtual INT OnMouseMove(WPARAM wParam, LPARAM lParam);           // 67 0x10119f90
    virtual INT OnLButtonUp(WPARAM wParam, LPARAM lParam);           // 69 0x1011a010
    virtual void Clear();                                            // 78 0x1011ada0
    // names ours
    virtual INT OnLButtonDblClick(WPARAM wParam, LPARAM lParam);     // 81 0x1011a250 WM_LBUTTONDBLCLK
    virtual INT OnDialogResult(WPARAM wParam, LPARAM lParam);        // 82 0x1011a530

    // names ours
    __declspec(noinline) void SetKind(INT InKind);                   // 0x10088870 (C1 only: no C2 match) (one body with the shop windows' grids')
    UBOOL AddItem(NItemInfo* Info);                                  // 0x1011ac00 0 when full
    UBOOL UpdateItem(NItemInfo* Info);                               // 0x1011a410 0: not here
    __declspec(noinline) UBOOL RemoveItem(INT ObjectID);             // 0x100cf0e0 (C1 only: no C2 match) (the pet's grid's body, shared)
};
NW_ASSERT_OFFSET(NCTradeItemWnd, Kind, 0x168);
NW_ASSERT_SIZE(NCTradeItemWnd, 0x170);

class NCTradeWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCTradeWnd, NCWnd, 0, 0x1030CFA0)

    NCTradeItemWnd* InventoryItems;  // +0xE8 6 x 4 icons, 80 at most
    NCTradeItemWnd* MyItems;         // +0xEC 6 icons
    NCTradeItemWnd* PartnerItems;    // +0xF0 6 icons
    UTexture* BackTex;               // +0xF4 "L2UI.storeWnd.TradeBack"
    NCButton* AddButton;             // +0xF8
    NCButton* OKButton;              // +0xFC
    NCButton* CancelButton;          // +0x100
    FString MyName;                  // +0x104 C2: an FString (C1: TCHAR[20])
    FString PartnerName;             // +0x110 over the partner's (Reset clears it)
    NCTradeFrameCtrl* TradeFrame;    // +0x11C the title bar (C3: made here, the base's FrameCtrl stays empty)

    NCTradeWnd();                                                    // 0x1011b0b0
    // ~NCTradeWnd                                                   // 0x1011b170

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10119bc0
    virtual INT OnCreate();                                          // 37 0x1011b1f0
    virtual INT OnDestroy();                                         // 38 0x1011a790
    virtual void ShowWindow();                                       // 48 0x10119cb0
    virtual void HideWindow();                                       // 49 0x100471a0 (folded)
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x1011a620
    // names ours
    virtual INT OnOK();                                              // 74 0x10119d50
    virtual INT OnCancel();                                          // 75 0x10119d70
    virtual INT OnAdd();                                             // 76 0x1011a8e0 the down arrow

    // names ours (the console's trade handlers call these)
    void Reset();                                                    // 0x10119c60
    void SetMyName(const TCHAR* Name);                               // 0x10119ce0
    void SetPartnerName(const TCHAR* Name);                          // 0x10119d20
    UBOOL AddInventoryItem(NItemInfo* Info);                         // 0x1011ad70
    UBOOL AddMyItem(NItemInfo* Info);                                // 0x1011ad80
    UBOOL AddPartnerItem(NItemInfo* Info);                           // 0x1011ad90
    UBOOL UpdateInventoryItem(INT Op, NItemInfo* Info);              // 0x1011ae80 1 add, 3 change,
                                                                     //   2 remove
    void ReloadTexts();                                              // 0x1011aa70 item names and
                                                                     //   descriptions, tool tips
    virtual void SetFrameTitleID(INT ID);                            // 56 0x10119d10 (ret 4) TradeFrame (+0x11C) takes the SysString title (NCWnd's slot)
};
NW_ASSERT_OFFSET(NCTradeWnd, BackTex, 0xF4);
NW_ASSERT_OFFSET(NCTradeWnd, MyName, 0x104);
NW_ASSERT_OFFSET(NCTradeWnd, PartnerName, 0x110);
NW_ASSERT_SIZE(NCTradeWnd, 0x120);

#pragma pack(pop)