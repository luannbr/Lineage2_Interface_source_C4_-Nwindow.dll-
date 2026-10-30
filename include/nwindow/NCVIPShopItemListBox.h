#pragma once
// NCVIPShopItemListBox - the item list of the VIP shop window (UClass "CVIPShopItemListBox",
// 0x1F4). Same rows as NCTrainListBox (its code is shared where identical): the icon, the name
// and, below it, the description, plus "X <count>" for type 5 entries.

#include "nwindow/NCListBox.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

// one item row (names ours; 0x518 bytes in C2, 0x318 in C1). The VIP shop window passes a copy of
// its own record (NVIPShopRecord, NCVIPShopWnd.h); the console builds one per product the server lists.
// C2 layout from the constructor (0x10087590 inlines it), AddEntry 0x10086d90 (the copy) and OnPaint
// 0x100861a0: Name and Desc are 256 characters now (C1: 128), IconName stays 128.
struct NVIPShopEntry
{
    INT ProductID;             // +0x000 what the purchase request names
    INT ItemID;                // +0x004 (C2: the paint asks the additional name of it)
    TCHAR Name[256];           // +0x008 the item's
    TCHAR Desc[256];           // +0x208
    UTexture* Icon;            // +0x408 (NCVIPShopWnd::AddRecord finds it by IconName)
    TCHAR IconName[128];       // +0x40C never copied by the list box
    INT BodyPart;              // +0x50C never copied by the list box
    INT Type;                  // +0x510 5: Count is shown
    INT Count;                 // +0x514 (after the name, as "x <count>")
    INT Enchant;               // +0x518 new in C3 (name ours): the "+N" of the name (the paint's 8th argument; AddEntry copies it)
    INT Grade;                 // +0x51C new in C3 (name ours): the crystal grade, 0 .. 4 (shown as `D` .. `S`; the paint's 9th)

    NVIPShopEntry()
    {
        Name[0] = 0;
        Desc[0] = 0;
        IconName[0] = 0;
        Icon = NULL;
        ProductID = 0;
        ItemID = 0;
        BodyPart = 0;
        Type = 0;
        Count = 0;
        Enchant = 0;
        Grade = 0;
    }
};
NW_ASSERT_OFFSET(NVIPShopEntry, Desc, 0x208);
NW_ASSERT_OFFSET(NVIPShopEntry, Icon, 0x408);
NW_ASSERT_OFFSET(NVIPShopEntry, IconName, 0x40C);
NW_ASSERT_OFFSET(NVIPShopEntry, Type, 0x510);
NW_ASSERT_OFFSET(NVIPShopEntry, Enchant, 0x518);
NW_ASSERT_OFFSET(NVIPShopEntry, Grade, 0x51C);
NW_ASSERT_SIZE(NVIPShopEntry, 0x520);

// An item's name line (0x10093a70, a cdecl function the list rows and the needed items' page share; name ours): at (X, Y)
// the name ("+Enchant Name" for the types 0 .. 3), the additional name of the item, the crystal grade `D` .. `S` and
// "x<Count>", cut with ".." to fit MaxWidth
class NCGDevice;
void DrawShopItemName(NCGDevice* Device, INT X, INT Y, INT MaxWidth, INT ItemID, const TCHAR* Name, INT Type, INT Enchant,
                      INT Grade, INT Count);

// C3: class NCVIPShopItemListBox gone (no counterpart): class NCVIPShopItemListBox : public NCListBox
// C3: class NCVIPShopItemListBox gone (no counterpart): {
// C3: class NCVIPShopItemListBox gone (no counterpart):     NW_DECLARE_CLASS(NCVIPShopItemListBox, NCListBox, 0, 0x10287928)

// C3: class NCVIPShopItemListBox gone (no counterpart):     TArray<NVIPShopEntry*> Entries;  // +0x220
// C3: class NCVIPShopItemListBox gone (no counterpart):     UTexture* IconBoxTex;            // +0x22C C3: "L2ui_ch3.etc.iconbox", the frame under each icon (OnCreate 0x10093a40)

// C3: class NCVIPShopItemListBox gone (no counterpart):     NCVIPShopItemListBox();                                          // 0x10097240
// C3: class NCVIPShopItemListBox gone (no counterpart):     NCVIPShopItemListBox(INT InMaxLines, INT InVisibleLines, INT Unused); // 0x10097300
// C3: class NCVIPShopItemListBox gone (no counterpart):     // ~NCVIPShopItemListBox                                         // 0x100972a0

// C3: class NCVIPShopItemListBox gone (no counterpart):     // stock bodies shared with NCTrainListBox (hooked there) except OnPaint
// C3: class NCVIPShopItemListBox gone (no counterpart):     virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x101241c0 (NCDebugWnd's)
// C3: class NCVIPShopItemListBox gone (no counterpart):     virtual INT OnCreate();                                          // 37 0x10093a40
// C3: class NCVIPShopItemListBox gone (no counterpart):     virtual INT OnDestroy();                                         // 38 0x10096310
// C3: class NCVIPShopItemListBox gone (no counterpart):     virtual INT OnPaint(NCGDevice* Device);                          // 56 0x100957b0
// C3: class NCVIPShopItemListBox gone (no counterpart):     virtual INT OnLButtonDown(WPARAM wParam, LPARAM lParam);         // 62 0x100862c0
// C3: class NCVIPShopItemListBox gone (no counterpart):     virtual INT HitTest(INT X, INT Y);                               // 69 0x10093db0 (the db names it NCTrainListBox's: not ours to hook)
// C3: class NCVIPShopItemListBox gone (no counterpart):     virtual void Clear();                                            // 71 0x100963e0 (the same body as NCHennaListBox's, folded; hooked there)

// C3: class NCVIPShopItemListBox gone (no counterpart):     // names ours
// C3: class NCVIPShopItemListBox gone (no counterpart):     void UpdateEntryScrollBar();                                     // 0x10086300 (shared with the other list boxes; hooked on NCHennaListBox)
// C3: class NCVIPShopItemListBox gone (no counterpart):     void AddEntry(const NVIPShopEntry* Src);                         // 0x10096460
// C3: class NCVIPShopItemListBox gone (no counterpart): };
// C3: class NCVIPShopItemListBox gone (no counterpart): NW_ASSERT_OFFSET(NCVIPShopItemListBox, Entries, 0x220);
// C3: class NCVIPShopItemListBox gone (no counterpart): NW_ASSERT_SIZE(NCVIPShopItemListBox, 0x230);

#pragma pack(pop)