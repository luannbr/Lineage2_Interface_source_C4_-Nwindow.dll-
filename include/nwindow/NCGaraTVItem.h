#pragma once
// NCGaraTVItem - one entry of the "Gara" tree views (UClass "CGaraTVItem", 0x20E0; new in C3): the quest list of the
// main window, the recipe lists and the Seven Signs windows are made of them. NCGaraTree puts every entry in place at
// each paint (DrawItem lays them out one under the other, the children of an expanded entry indented) and draws its
// picture and its text; the entry itself paints nothing but a clip. A click toggles bExpanded (outside the small box
// HitX / HitY / HitW / HitH of the +/- sign it comes back: the entry only takes the focus) and, in the quest tree,
// makes the entry the tree's selected one.
// NCRecipeGaraTree, NCQuestGaraTree and the Seven Signs trees use the same entries with their own data.

#include "nwindow/NControl.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCGDevice;

// The record a tree is given for an entry and keeps a copy of at +0x1D4 of the entry (name ours; 0x1EB0 bytes, from
// the stock constructor 0x10012890, destructor 0x10012A00 and the member-wise copy 0x100125B0). The quest tree reads
// what its rows need from the arrays: the names, the item ids, the counts and the texts of the steps.
struct NGaraTVItemData
{
    UTexture* Icon;                  // +0x000 the picture drawn left of the text (the collapsed one)
    INT Kind;                        // +0x004 (-1 in a new record) the quest tree's row kind: 1 a group, 2 a quest, 3
                                     //   the quest's details (name ours)
    INT IconSet;                     // +0x008 (0) 0 .. 3: which two of the five info icons a quest row shows (name ours)
    INT LevelMin;                    // +0x00C (0) the quest's level range (name ours)
    INT LevelMax;                    // +0x010 (0)
    INT Unknown14;                   // +0x014 left as allocated
    INT Unknown18;                   // +0x018 left as allocated
    TCHAR Text[1024];                // +0x01C the entry's text (its extent is measured with it)
    TCHAR Text2[2048];               // +0x81C the details' text of a kind 3 row (newlines converted when drawn)
    INT Width;                       // +0x181C set by AddItem: the icon's width + 2 + the text's
    INT Height;                      // +0x1820 set by AddItem: the icon's height
    INT IconW;                       // +0x1824 the picture's size
    INT IconH;                       // +0x1828
    INT Unknown182C;                 // +0x182C
    INT Unknown1830;                 // +0x1830
    INT QuestID;                     // +0x1834 (-1) the quest the row belongs to (name ours)
    INT Count;                       // +0x1838 how many entries of the item arrays below are used
    UTexture* ExpandedIcon;          // +0x183C the picture of an expanded group / quest (name ours)
    UTexture* ItemIcon[50];          // +0x1840 the details' item rows: their pictures (names ours),
    FString ItemName[50];            // +0x1908 their names,
    INT ItemID[50];                  // +0x1B60 their item ids (compared with the inventory's),
    INT ItemOwned[50];               // +0x1C28 (0) how many of each the player has (counted at paint),
    INT ItemNeeded[50];              // +0x1CF0 (0) how many the quest wants (0: any, < 0: more than |n|)
    BYTE bTextCount;                 // +0x1DB8 (0) the counts show a text instead of the owned number (name ours)
    FString MapPlace;                // +0x1DBC the quest's place on the map, for the tool tip (name ours)
    INT MapPos[3];                   // +0x1DC8 (its position)
    INT Unknown1DD4;                 // +0x1DD4
    FString Strings1DD8[9];          // +0x1DD8
    FString Strings1E44[9];          // +0x1E44

    __declspec(noinline) NGaraTVItemData();                         // 0x10012890 (out of line: the stock callers
                                                                     //   call it, also for their own locals)
    __declspec(noinline) ~NGaraTVItemData();                        // 0x10012A00
    __declspec(noinline) NGaraTVItemData& operator=(const NGaraTVItemData& Other);   // 0x100125B0
};
NW_ASSERT_OFFSET(NGaraTVItemData, Text, 0x1C);
NW_ASSERT_OFFSET(NGaraTVItemData, Text2, 0x81C);
NW_ASSERT_OFFSET(NGaraTVItemData, Width, 0x181C);
NW_ASSERT_OFFSET(NGaraTVItemData, IconW, 0x1824);
NW_ASSERT_OFFSET(NGaraTVItemData, Count, 0x1838);
NW_ASSERT_OFFSET(NGaraTVItemData, ItemIcon, 0x1840);
NW_ASSERT_OFFSET(NGaraTVItemData, ItemName, 0x1908);
NW_ASSERT_OFFSET(NGaraTVItemData, ItemID, 0x1B60);
NW_ASSERT_OFFSET(NGaraTVItemData, ItemOwned, 0x1C28);
NW_ASSERT_OFFSET(NGaraTVItemData, ItemNeeded, 0x1CF0);
NW_ASSERT_OFFSET(NGaraTVItemData, bTextCount, 0x1DB8);
NW_ASSERT_OFFSET(NGaraTVItemData, MapPlace, 0x1DBC);
NW_ASSERT_OFFSET(NGaraTVItemData, Strings1DD8, 0x1DD8);
NW_ASSERT_OFFSET(NGaraTVItemData, Strings1E44, 0x1E44);
NW_ASSERT_SIZE(NGaraTVItemData, 0x1EB0);

class NCGaraTVItem : public NControl
{
    NW_DECLARE_CLASS(NCGaraTVItem, NControl, 0, 0x102BE0B0)

    NGaraTVItemData Data;            // +0x1F4 the copy of the record
    NCGaraTVItem* ParentItem;        // +0x20A4 the entry this one is under (name ours; read by OnLButtonDown, never
                                     //   written here)
    TArray<NCGaraTVItem*> Children;  // +0x20A8 the entries under this one
    INT Unknown2094;                 // +0x20B4 (0)
    INT Unknown2098;                 // +0x20B8 (0)
    INT Unknown209C;                 // +0x20BC (0)
    INT Unknown20A0;                 // +0x20C0 (0)
    INT Level;                       // +0x20C4 the constructor's argument (-1)
    UBOOL bExpanded;                 // +0x20C8 the children are shown (name ours)
    UBOOL bClicked;                  // +0x20CC set when a click opened it: the quest list closes the others (name ours)
    INT Unknown20B0;                 // +0x20D0 (0)
    INT Unknown20B4;                 // +0x20D4 (0)
    INT Unknown20B8;                 // +0x20D8 (0)
    INT Unknown20BC;                 // +0x20DC (0)
    INT HitX;                        // +0x20E0 the box of the +/- sign (name ours)
    INT HitY;                        // +0x20E4
    FLOAT HitW;                      // +0x20E8
    FLOAT HitH;                      // +0x20EC
    INT Unknown20D0;                 // +0x20F0 (0)
    INT Unknown20D4;                 // +0x20F4 (0)
    INT CopyW;                       // +0x20F8 the window's size at OnCreate (name ours)
    INT CopyH;                       // +0x20FC

    NCGaraTVItem(INT InLevel = -1);                                  // 0x100159d0 (ret 4)
    // ~NCGaraTVItem                                                 // 0x10015b00 (the children's array, then the data)

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10021e60 (folded with NCButton's)
    virtual INT OnCreate();                                          // 37 0x100148a0
    virtual INT OnDestroy() { return 1; }                            // 38 0x100f7850 (folded)
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x10014dc0
    virtual INT OnMouseMove(WPARAM wParam, LPARAM lParam) { return 1; }   // 67 0x100e6a50 (folded)
    virtual INT OnLButtonDown(WPARAM wParam, LPARAM lParam);         // 68 0x10014e30
    virtual INT OnLButtonUp(WPARAM wParam, LPARAM lParam) { return 1; }   // 69 0x100e6a50 (folded)

    // name ours: the child goes to the end of Children and is handed back (NULL: nothing)
    NCGaraTVItem* AddChildItem(NCGaraTVItem* Child);                 // 0x10015890 (ret 4)
};
NW_ASSERT_OFFSET(NCGaraTVItem, Data, 0x1F4);
NW_ASSERT_OFFSET(NCGaraTVItem, ParentItem, 0x20A4);
NW_ASSERT_OFFSET(NCGaraTVItem, Children, 0x20A8);
NW_ASSERT_OFFSET(NCGaraTVItem, Level, 0x20C4);
NW_ASSERT_OFFSET(NCGaraTVItem, bExpanded, 0x20C8);
NW_ASSERT_OFFSET(NCGaraTVItem, bClicked, 0x20CC);
NW_ASSERT_OFFSET(NCGaraTVItem, HitX, 0x20E0);
NW_ASSERT_OFFSET(NCGaraTVItem, HitW, 0x20E8);
NW_ASSERT_OFFSET(NCGaraTVItem, CopyW, 0x20F8);
NW_ASSERT_SIZE(NCGaraTVItem, 0x2100);

#pragma pack(pop)
