#pragma once
// NCRecipeBuyManufactureWnd - "Item Creation" for an offer of a recipe shop (UClass "CRecipeBuyManufactureWnd", 0x1B04):
// NCRecipeManufactureWnd's window (same product, ingredients, creator MP bar, outcome line and buttons; see that header for
// the common fields) for a customer of a recipe shop: Create asks the shop's owner to make the item
// (RequestRecipeShopMakeDo(ShopID, RecipeID)) and <PREV goes back to the shop's list (RequestRecipeShopSellList(ShopID)).
// The console opens it (NConsoleWnd::vfn131 0x10059330, the answer of RequestRecipeShopMakeInfo): the buy list hidden,
// Reset, the shop id, the creator's MP, the outcome, SetRecipe, the window placed where the list was, shown and focused.
//
// Differences from NCRecipeManufactureWnd (C3 layout from the constructor 0x100E2260 and the diff of the two OnCreate /
// OnPaint / SetRecipe bodies, which are identical but for these offsets): two new fields (+0x1B30 ShopID, +0x1B34 a number
// SetRecipe is given that the Create request carries) push the outcome to +0x1B38 and the four buttons to +0x1B3C.., the
// pressed flag of the tree button is at +0x1B4C, and the close button's id is 0xF8, not 0xFD.
//
// Desktop (NCConsole) +0x148.

#include "nwindow/NCRecipeManufactureItemWnd.h"
#include "nwindow/NCRecipeManufactureWnd.h"
#include "nwindow/NRecipeData.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCButton;

enum { NID_RECIPEBUYMAKE_CLOSE = 0xF8 };   // the close button (NCRecipeManufactureWnd's is 0xFD)

class NCRecipeBuyManufactureWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCRecipeBuyManufactureWnd, NCWnd, 0, 0x102FB6E8)

    NCRecipeManufactureItemWnd* IngredientWnd;  // +0xE8
    UTexture* BackTex;               // +0xEC
    UTexture* MPTex;                 // +0xF0
    UTexture* TreeTex;               // +0xF4 (see NCRecipeManufactureWnd.h)
    UTexture* TreeDownTex;           // +0xF8
    UTexture* TreeCurTex;            // +0xFC
    NItemEntry Product;              // +0x100
    INT RecipeID;                    // +0x1D40
    INT ProductID;                   // +0x1D44
    INT ProductCount;                // +0x1D48
    INT MPCost;                      // +0x1D4C
    INT SuccessRate;                 // +0x1D50
    INT CreatorMP;                   // +0x1D54
    INT CreatorMaxMP;                // +0x1D58
    INT ShopID;                      // +0x1D5C the shop's owner (the console writes it; the requests take it first)
    INT MakeParam;                   // +0x1D60 (name ours) SetRecipe's second argument; RequestRecipeShopMakeDo's third. Zeroed by the
                                     //   constructor and Reset
    INT Result;                      // +0x1D64 (-1) the last try: 0 failed, 1 succeeded
    NCButton* TreeButton;            // +0x1D68 id 0xFE
    NCButton* MakeButton;            // +0x1D6C id 0xF9
    NCButton* PrevButton;            // +0x1D70 id 0xFA
    NCButton* CloseButton;           // +0x1D74 id 0xF8
    UBOOL bTreePressed;              // +0x1D78 (name ours; new in C3) the mouse went down on the tree button

    NCRecipeBuyManufactureWnd();                                     // 0x100f3510
    // ~NCRecipeBuyManufactureWnd                                    // 0x100f3600

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x100efde0
    virtual INT OnCreate();                                          // 37 0x100f1d70
    virtual INT OnDestroy();                                         // 38 0x100f0e60
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x100f0f50
    virtual INT OnMouseMove(WPARAM wParam, LPARAM lParam);           // 67 0x100f17f0
    virtual INT OnLButtonDown(WPARAM wParam, LPARAM lParam);         // 68 0x100f17b0
    virtual INT OnLButtonUp(WPARAM wParam, LPARAM lParam);           // 69 0x100efeb0
    // names ours
    virtual INT OnPrev();                                            // 74 0x100eff20 hides the window, asks for the shop's list
    virtual INT OnClose();                                           // 75 0x10084ee0 (NCOptionWnd::OnCancel's body)
    virtual INT OnShowTree();                                        // 76 0x100f5df0 (hooked here; NCRecipeManufactureWnd shares it)
    virtual void NewVirtual77();  // 77 0x100eff60 (only this client has it; defined in NNewVirtuals.gen.cpp)
    INT OnMake();                                            // 0x100eff60 (slot 71 is NewVirtual71, which jumps to it) RequestRecipeShopMakeDo(ShopID, RecipeID, MakeParam)

    // names ours
    void Reset();                                                    // 0x100efed0 the numbers zeroed (not the outcome), the grid emptied
    void SetRecipe(NRecipeData* Rec, INT InMakeParam);               // 0x100f2220 (NCRecipeManufactureWnd::SetRecipe's code, ret 8)
    __declspec(noinline) UBOOL AddIngredient(NItemEntry* Item);      // 0x100efe90 (NCRecipeManufactureWnd's, shared body)
    UBOOL HitTree(INT X, INT Y);                                     // 0x100f1720 (ret 8; name ours)
};
NW_ASSERT_OFFSET(NCRecipeBuyManufactureWnd, TreeCurTex, 0xFC);
NW_ASSERT_OFFSET(NCRecipeBuyManufactureWnd, Product, 0x100);
NW_ASSERT_OFFSET(NCRecipeBuyManufactureWnd, ShopID, 0x1D5C);
NW_ASSERT_OFFSET(NCRecipeBuyManufactureWnd, Result, 0x1D64);
NW_ASSERT_OFFSET(NCRecipeBuyManufactureWnd, TreeButton, 0x1D68);
NW_ASSERT_OFFSET(NCRecipeBuyManufactureWnd, CloseButton, 0x1D74);
NW_ASSERT_OFFSET(NCRecipeBuyManufactureWnd, bTreePressed, 0x1D78);
NW_ASSERT_SIZE(NCRecipeBuyManufactureWnd, 0x1D7C);

#pragma pack(pop)
