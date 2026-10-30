#pragma once
// NCVIPShopWnd - the multisell window of this client, called the VIP shop by the decompilation (UClass "CVIPShopWnd", 0x11C; the
// desktop's +0x140, title "Store"). A grid of the products (NCVIPShopItemWnd, the first item of each record, a plus icon over
// the ones that make several), the items the selected product gives (NCVIPShopItemInfoWnd: up to 50 item entries, each with the
// name control the page puts over its row, or the one item in full) and the items it costs (NCVIPShopNeededItemWnd), with OK
// (buy), Cancel (empties and hides the window) and the edit line for the amount (disabled for the products of the kind 0 and 1
// of the list: it holds "1"). The console fills it through NConsoleWnd's virtuals: Open (a list id, everything emptied), AddRecord
// for each item of each product, AddNeededItem for each item a product costs, then the window shown on the list (ShowList).
// OK asks the amount (dialog 0x51 with the list id and the product id) and the dialog's answer is the purchase. Hiding the
// window empties it.
//
// C4 against C3: the list is the item window above, the record is the console's item info (NItemInfoC4, 0x19B4 bytes) times the
// products that share one id, the pages are scroll windows whose rows are NCNameCtrl windows (name and tool tip of the item), and
// the amount is an edit line (the C3 window had no amount and had the list page as a window of its own). The layout is from
// OnCreate 0x100A3A20, the constructors (0x100A3560, 0x100A3790, 0x100A3850, 0x100A3960) and the functions below.

#include "nwindow/NCItemWnd.h"
#include "nwindow/NCVIPShopItemListBox.h"
#include "nwindow/NItemC4.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCButton;
class NCEditBox;

// NWM_COMMAND ids (names ours)
enum
{
    NID_VIPSHOP_FRAME = 0xF0E400,
    NID_VIPSHOP_LIST = 0xF0E401,
    NID_VIPSHOP_OK = 0xF0E402,
    NID_VIPSHOP_CANCEL = 0xF0E403,
    NID_VIPSHOP_COUNT = 0xF0E404,   // the amount's edit line
};

// An item a product costs (name ours; 0x318 bytes: Name is 256 characters; built on the stack by the console's virtual
// AddVIPNeedItemList and copied by NCVIPShopWnd::AddNeededItem, then by the needed items' page). Layout from AddNeededItem
// 0x100A2B40 (the inlined constructor and the copy), NCVIPShopWnd::ShowItem 0x100A2E90 and NCVIPShopNeededItemWnd::AddNeeded
// 0x100A2360.
struct NVIPNeededItem
{
    INT ItemID;                // +0x000
    TCHAR Name[256];           // +0x004 the item's
    INT Count;                 // +0x204 how many
    UTexture* Icon;            // +0x208 (AddNeededItem finds it by IconName)
    TCHAR IconName[128];       // +0x20C

    NVIPNeededItem()
    {
        Name[0] = 0;
        IconName[0] = 0;
        Count = 0;
        Icon = NULL;
        ItemID = 0;
        Enchant = 0;
        Grade = 0;
    }
    INT Enchant;       // +0x30C new in C3 (name ours): the "+N" of the name (the name control's fourth parameter)
    INT Grade;         // +0x310 new in C3 (name ours): the crystal grade, 0 .. 4 (the fifth)
    INT Type;          // +0x314 new in C3 (name ours): NItemEntry::Type (0 .. 3: the enchant shows; the third); the constructor
                       //   leaves it
};
NW_ASSERT_OFFSET(NVIPNeededItem, Name, 0x4);
NW_ASSERT_OFFSET(NVIPNeededItem, Count, 0x204);
NW_ASSERT_OFFSET(NVIPNeededItem, Icon, 0x208);
NW_ASSERT_OFFSET(NVIPNeededItem, IconName, 0x20C);
NW_ASSERT_SIZE(NVIPNeededItem, 0x318);

// A product of the window (name ours; 0x5053C bytes, made by the console's AddRecord, constructor 0x100A2D40): the list the
// product came in, how many items it gives, those items (the console's item infos: the first one's UnknownC90 is the id of the
// product) and the items it costs (never emptied by the window: Reset frees them one by one)
struct NVIPRecord
{
    INT ListID;                         // +0x000 (AddRecord stores the list there) 0 or 1: the amount can't be changed
    INT NumItems;                       // +0x004 the items the product gives (1 at first, one more for each record of the same product)
    BYTE Items[0x50530 - 8];            // +0x008 NItemInfoC4 [49 and a bit], 0x19B4 bytes each
    TArray<NVIPNeededItem*> Needed;     // +0x50530 (Data, Num at +0x50534, Max)

    NItemInfoC4* Item(INT Index) { return (NItemInfoC4*)(Items + Index * 0x19B4); }
    __declspec(noinline) void DestroyNeeded();                        // 0x100A3200 (name ours; a jump into the array's destructor 0x100A2CB0)
};
NW_ASSERT_OFFSET(NVIPRecord, Needed, 0x50530);
NW_ASSERT_SIZE(NVIPRecord, 0x5053C);

// the grid of the products (UClass "CVIPShopItemWnd", 0x16C): NCItemWnd's cells over the console's records; the ones that
// give several items carry a plus icon. A click selects the product and shows it on the other pages
class NCVIPShopItemWnd : public NCItemWnd
{
    NW_DECLARE_CLASS(NCVIPShopItemWnd, NCItemWnd, 0, 0x102E0178)

    UTexture* PlusTex;               // +0x168 "L2UI_CH3.MultiSellWnd.Multisell_plusicon"

    __declspec(noinline) NCVIPShopItemWnd();                         // 0x100A3560 (out of line, like the stock one's callers)
    __declspec(noinline) NCVIPShopItemWnd(INT InColumns, INT InVisibleRows, INT InMaxItems, INT InCellSize, INT InGapX, INT InGapY);  // 0x100A3640 (ret 0x18)
    // ~NCVIPShopItemWnd                                             // 0x100A35F0

    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x100A1870
    virtual INT OnLButtonDown(WPARAM wParam, LPARAM lParam);         // 68 0x100A36C0
};
NW_ASSERT_OFFSET(NCVIPShopItemWnd, PlusTex, 0x168);
NW_ASSERT_SIZE(NCVIPShopItemWnd, 0x16C);

// the 50 entries of the information page, built in order and destroyed in reverse as the stock's vector constructor and destructor
// (0x100A3850 / 0x100A38F0) do it: the constructor is NItemEntry's stock one (0x1001F7F0), the destructor's part of an entry is the tool
// tip's (entries are bytes here: ~NItemEntry is not ported)
struct NVIPEntryArray
{
    BYTE Data[50 * 0x1C40];
    NVIPEntryArray();
    ~NVIPEntryArray();
};

// the item page's information (UClass "CVIPShopItemInfoWnd", 0x58674): the product's items as item window entries, completed
// from the item table; one item is shown in full, several are a column of icons with a name control over each row
class NCVIPShopItemInfoWnd : public NCScrollWnd
{
    NW_DECLARE_CLASS(NCVIPShopItemInfoWnd, NCScrollWnd, 0, 0x102E06A8)

    NVIPEntryArray Entries;          // +0x120 NItemEntry [50] (the constructor 0x100A3850 builds them, the destructor 0x100A38F0 undoes them)
    INT NumEntries;                  // +0x585A0 how many of the entries are in use
    INT Unknown585A4;                // +0x585A4
    UTexture* IconBoxTex;            // +0x585A8 "L2ui_ch3.etc.iconbox", the frame under each icon (OnCreate 0x100A00F0)
    NCWnd* NameCtrls[50];            // +0x585AC the NCNameCtrl over each entry's row (more than one entry only)

    __declspec(noinline) NCVIPShopItemInfoWnd();                     // 0x100A3850
    // ~NCVIPShopItemInfoWnd                                         // 0x100A38F0

    NItemEntry* EntryAt(INT Index) { return (NItemEntry*)(Entries.Data + Index * 0x1C40); }   // (not Entry: NCWnd's)

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10014b90 (shared)
    virtual INT OnCreate();                                          // 37 0x100A00F0
    virtual INT OnDestroy();                                         // 38 0x100A2640
    virtual void ShowWindow();                                       // 48 0x100A0130 (the scroll back at the top)
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x100A1C50

    // names ours
    __declspec(noinline) void Clear();                               // 0x100A1F50 (called, not inlined, by the stock code)
    __declspec(noinline) void AddItem(INT Index, NItemEntry* Src);   // 0x100A26C0 (ret 8) Src copied as entry Index, completed from the item table
    __declspec(noinline) void MoveNames(INT DX, INT DY);             // 0x100A1E00 (ret 8) the name controls follow the scroll; those out of the window are hidden
    INT DrawItemInfo(NCGDevice* Device, NItemEntry* Entry, INT X, INT Y); // 0x100A0160 the Y under the texts
    virtual INT OnMouseWheel(WPARAM wParam, LPARAM lParam);  // 73 0x100a1f20 (ret 8; NCScrollWnd's, then MoveNames; NCVIPShopWnd.cpp)
    virtual INT ScrollUp();  // 75 0x100a1eb0 (the same; NCVIPShopWnd.cpp)
    virtual INT ScrollDown();  // 76 0x100a1ed0 (the same; NCVIPShopWnd.cpp)
    virtual INT OnScrollBar(INT Value);  // 77 0x100a1ef0 (ret 4; the same; NCVIPShopWnd.cpp)
};
NW_ASSERT_OFFSET(NCVIPShopItemInfoWnd, NumEntries, 0x585A0);
NW_ASSERT_OFFSET(NCVIPShopItemInfoWnd, IconBoxTex, 0x585A8);
NW_ASSERT_OFFSET(NCVIPShopItemInfoWnd, NameCtrls, 0x585AC);
NW_ASSERT_SIZE(NCVIPShopItemInfoWnd, 0x58674);

// the item page's needed items (UClass "CVIPShopNeededItemWnd", 0x204): icon, name control and "x count", 36 pixels a row
class NCVIPShopNeededItemWnd : public NCScrollWnd
{
    NW_DECLARE_CLASS(NCVIPShopNeededItemWnd, NCScrollWnd, 0, 0x102DFC48)

    TArray<NVIPNeededItem*> Items;   // +0x120 copies the window owns (freed by Clear / OnDestroy)
    INT Unknown12C;                  // +0x12C (the constructor 0x100A3790 leaves it)
    UTexture* IconBoxTex;            // +0x130 "L2ui_ch3.etc.iconbox", the frame under each icon (OnCreate 0x1009FFD0)
    NCWnd* NameCtrls[50];            // +0x134 the NCNameCtrl over each row
    INT Unknown1FC;                  // +0x1FC (Clear: 0)
    INT Unknown200;                  // +0x200 (Clear: 0)

    __declspec(noinline) NCVIPShopNeededItemWnd();                   // 0x100A3790
    // ~NCVIPShopNeededItemWnd                                       // 0x100A37F0

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10014b90 (shared)
    virtual INT OnCreate();                                          // 37 0x1009FFD0
    virtual INT OnDestroy();                                         // 38 0x100A2DC0
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x100A1A80

    // names ours
    __declspec(noinline) void Clear();                               // 0x100A25A0 the items freed, the name controls destroyed
    __declspec(noinline) void AddNeeded(const NVIPNeededItem* Src);  // 0x100A2360 (ret 4) Src copied, a name control over its row
    __declspec(noinline) void MoveNames(INT DX, INT DY);             // 0x100A0010 (ret 8) the name controls follow the scroll
    virtual INT OnMouseWheel(WPARAM wParam, LPARAM lParam);  // 73 0x100a00c0 (ret 8; NCScrollWnd's, then MoveNames; NCVIPShopWnd.cpp)
    virtual INT ScrollUp();  // 75 0x100a0050 (the same; NCVIPShopWnd.cpp)
    virtual INT ScrollDown();  // 76 0x100a0070 (the same; NCVIPShopWnd.cpp)
    virtual INT OnScrollBar(INT Value);  // 77 0x100a0090 (ret 4; the same; NCVIPShopWnd.cpp)
};
NW_ASSERT_OFFSET(NCVIPShopNeededItemWnd, Items, 0x120);
NW_ASSERT_OFFSET(NCVIPShopNeededItemWnd, IconBoxTex, 0x130);
NW_ASSERT_OFFSET(NCVIPShopNeededItemWnd, NameCtrls, 0x134);
NW_ASSERT_SIZE(NCVIPShopNeededItemWnd, 0x204);

class NCVIPShopWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCVIPShopWnd, NCWnd, 0, 0x102DF710)

    NCVIPShopItemWnd* ListWnd;       // +0xE8 the products
    NCVIPShopItemInfoWnd* InfoWnd;   // +0xEC what the selected product gives
    NCVIPShopNeededItemWnd* NeededWnd; // +0xF0 what it costs
    NCEditBox* CountEdit;            // +0xF4 the amount (id NID_VIPSHOP_COUNT)
    NCButton* OKButton;              // +0xF8
    NCButton* CancelButton;          // +0xFC
    UTexture* BackTex;               // +0x100 "L2UI_CH3.MultiSellWnd.multicell_back"
    INT Selected;                    // +0x104 the product shown (index in Records), -1 none
    INT ListID;                      // +0x108 (Open) sent back with the purchase
    INT Unknown10C;                  // +0x10C
    TArray<NVIPRecord*> Records;     // +0x110

    NCVIPShopWnd();                                                  // 0x100A3960
    // ~NCVIPShopWnd                                                 // 0x100A39C0

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x100a17f0
    virtual INT OnCreate();                                          // 37 0x100A3A20
    virtual INT OnDestroy();                                         // 38 0x100A4040
    virtual void HideWindow();                                       // 49 0x100A41D0
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x100A1FC0
    // names ours
    virtual INT OnOK();                                              // 74 0x100a21b0
    virtual INT OnCancel();                                          // 75 0x100a4000 (Reset and hide)

    // names ours (the console calls the last four)
    __declspec(noinline) void ShowList();                            // 0x100a2ab0 (the console lane's: the list built again)
    __declspec(noinline) void ShowItem();                            // 0x100a2e90 the item page of Records(Selected)
    __declspec(noinline) void Reset();                               // 0x100a3f40 (Open, OnCancel, HideWindow and OnDestroy call it)
    void AddNeededItem(INT ProductID, const NVIPNeededItem* Src);    // 0x100a2b40 (ret 8) (the console lane's)
    void Open(INT InListID);                                         // 0x100a4020 (ret 4) (the console lane's)
};
NW_ASSERT_OFFSET(NCVIPShopWnd, BackTex, 0x100);
NW_ASSERT_OFFSET(NCVIPShopWnd, Records, 0x110);
NW_ASSERT_SIZE(NCVIPShopWnd, 0x11C);

#pragma pack(pop)
