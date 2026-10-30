#pragma once
// NCRecipeBuyListWnd - what a dwarf's recipe shop offers, seen by the customer (UClass "CRecipeBuyListWnd", 0xB8; new
// in C2; the desktop's NCConsole +0x144). The console opens it when the server sends the shop's list: the list box of
// the recipes (NCRecipeListBox: name, price, success rate), the dwarf's MP bar ("Creator MP") and the player's
// adena. Clicking a recipe asks the server for its ingredients (RequestRecipeShopMakeInfo: the shop owner and the
// recipe), which opens the manufacture window (NCRecipeBuyManufactureWnd, the desktop's +0x148) over this one.

#include "nwindow/NCWnd.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCButton;
class NCRecipeListBox;
struct NRecipeRow;

enum { NID_RECIPEBUYLIST_FRAME = 0xFF, NID_RECIPEBUYLIST_LIST = 0x1A05, NID_RECIPEBUYLIST_CLOSE = 0x1019 };   // NWM_COMMAND ids (names ours)

class NCRecipeBuyListWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCRecipeBuyListWnd, NCWnd, 0, 0x102FBC20)

    NCRecipeListBox* RecipeList;     // +0xE8 (name ours) id 0x1A05
    NCButton* CloseButton;           // +0xEC (name ours; new in C3) id 0x1019, label 0x286; OnPaint puts it at (91, 372) again
    UTexture* BackTex;               // +0xF0 "L2UI_CH3.RecipeWnd.Recipe3_Back"
    UTexture* MPTex;                 // +0xF4 "L2UI_CH3.PlayerStatusWnd.ps_mpbar" (the bar's texture)
    BYTE _UnknownD8[0xDC - 0xD8];    // +0xF8 (not touched by the code seen)   // C2: offset derived, no evidence
    INT ShopOwnerID;                 // +0xFC (-1) (name ours) whose shop it is (stock 0x100dec90, which the console calls)
    INT MP;                          // +0x100 (name ours) the dwarf's MP (the console writes it)
    INT MaxMP;                       // +0x104 (the console writes it)
    INT Adena;                       // +0x108 the player's adena (stock 0x100deca0, which the console calls)

    NCRecipeBuyListWnd();                                            // 0x100f31e0
    // ~NCRecipeBuyListWnd                                           // 0x100f3250

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x100efd20
    virtual INT OnCreate();                                          // 37 0x100f32a0
    virtual INT OnDestroy();                                         // 38 0x1011dc90 (NCTrainWnd's body, shared; a stub here)
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x100f0b80
    // names ours
    virtual INT OnClose();                                           // 74 0x100efda0 (hides the window with the sound; shared with NCRecipeManufactureWnd's slot 69)
    virtual INT OnSelectRecipe(LPARAM Index);                        // 75 0x100f0dc0 a recipe of the list was clicked
                                                                     //   (the list box's NWM_COMMAND code; not read)
    // not virtual: kept so that NNewVirtuals.gen.cpp (generated: the placeholder of slot 68, OnClose's) still compiles
    // C4: void NewVirtual68();

    // names ours (the console calls them)
    void ClearList();                                                // 0x100C11B0 (C1 only: no C2 match) (C1 only: no C2 match) the list box's Clear
    void AddRecipe(const NRecipeRow* Src);                           // 0x100f1d60 the list box's AddEntry (a tail call)
    void SetShopOwner(INT InOwnerID);                                // 0x100efdd0
    void SetAdena(INT InAdena);                                      // 0x100DECA0 (C1 only: no C2 match)
};
NW_ASSERT_OFFSET(NCRecipeBuyListWnd, RecipeList, 0xE8);
NW_ASSERT_OFFSET(NCRecipeBuyListWnd, CloseButton, 0xEC);
NW_ASSERT_OFFSET(NCRecipeBuyListWnd, BackTex, 0xF0);
NW_ASSERT_OFFSET(NCRecipeBuyListWnd, ShopOwnerID, 0xFC);
NW_ASSERT_OFFSET(NCRecipeBuyListWnd, Adena, 0x108);
NW_ASSERT_SIZE(NCRecipeBuyListWnd, 0x10C);

#pragma pack(pop)
