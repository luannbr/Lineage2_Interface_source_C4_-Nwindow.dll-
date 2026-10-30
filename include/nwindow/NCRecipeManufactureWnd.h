#pragma once
// NCRecipeManufactureWnd - "Item Creation" for the player's own recipe (UClass "CRecipeManufactureWnd", 0x1AFC): the
// product (icon, name, grade, "MP cost", "Success Rate", "Number of Resulting Items"), the ingredients in a grid below
// ("Ingredient(s)"; dimmed when the player lacks them), the creator's MP bar, the outcome of the last try and the count
// of the product the player keeps, with the buttons Create, <PREV (back to the recipe book) and Close, and a tree
// button that opens NCRecipeTreeWnd. The console opens it with the recipe's record, the player's MP and the outcome
// (NConsoleWnd::vfn125, the answer of RequestRecipeItemMakeInfo): Reset, SetRecipe, then the numbers.
//
// Desktop (NCConsole) +0x13C. NCRecipeBuyManufactureWnd (0x1B04) is the same window for a recipe shop's offer: the same
// code with two more fields (the shop's owner and one unnamed) and another close id; it is in NCRecipeBuyManufactureWnd.h
// and both live in src/nwindow/NCRecipeManufactureWnd.cpp.
//
// Layout from the C3 constructor 0x100E53B0, OnCreate 0x100E54F0, OnDestroy 0x100E2F20, OnPaint 0x100E3010 and the console's
// writers. The product is an NItemEntry used for its strings, icon and grade only: Icon (+0xE0), Name (+0xEC), IconName
// (+0x4EC), Description (+0x7EC), CrystalGrade (+0x1A3C). C3: the tree button is framed with a texture that changes while
// the mouse is over / down on it (three textures at +0xD4 .. +0xDC, the mouse handlers).

#include "nwindow/NCRecipeManufactureItemWnd.h"
#include "nwindow/NRecipeData.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCButton;
class NCRecipeGaraTreeCtrl;

// "Recipe Information", the ingredient tree of a recipe (UClass "CRecipeGaraTreeWnd", 0x1B14; the desktop's NCConsole +0x170;
// C2: NCRecipeTreeWnd): the recipe making windows' tree button fills and shows it. The tree itself is the control at +0xCC
// (NCRecipeGaraTreeCtrl, which belongs to the tree classes' group: it is kept as an NCWnd and used through its vtable).
// Layout from the C3 constructor 0x100E5B60, OnCreate 0x100E5C60, OnDestroy 0x100E4000, OnPaint 0x100E4070 and SetRecipe
// 0x100E4860. The product is an NItemEntry used for its strings, icon and grade only: Icon (+0xD0), Name (+0xDC), IconName
// (+0x4DC), Description (+0x7DC), CrystalGrade (+0x1A2C).
enum
{
    NID_RECIPETREE_FRAME = 0xFF,     // the title bar (NControl::ID)
    NID_RECIPETREE_CLOSE = 0xFD,     // the Close button -> OnClose (vslot 68)
};

class NCRecipeGaraTreeWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCRecipeGaraTreeWnd, NCWnd, 0, 0x102FEAD8)

    UTexture* BackTex;               // +0xE8 "L2UI_CH3.RecipeWnd.Recipe5_Back"
    NCWnd* TreeCtrl;                 // +0xEC (name ours) the tree control, 0xF2 x 0xFE at (7, 108); its vslot 72 empties it
    NItemEntry Product;              // +0xF0 the product: the icon, the strings and the grade (see above)
    INT MPCost;                      // +0x1D30 (name ours) the recipe's MP cost
    INT SuccessRate;                 // +0x1D34 percent
    INT Level;                       // +0x1D38 the recipe's level ("Lv.N")
    NCButton* CloseButton;           // +0x1D3C id 0xFD, text 646

    NCRecipeGaraTreeWnd();                                           // 0x100f6f90
    // ~NCRecipeGaraTreeWnd                                          // 0x100f7030

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x100f3ad0
    virtual INT OnCreate();                                          // 37 0x100f7090
    virtual INT OnDestroy();                                         // 38 0x100f5460
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x100f54d0
    virtual INT OnClose();                                           // 74 0x100f3b30 (name ours) the tree emptied, the window hidden

    // names ours
    __declspec(noinline) void Reset();                               // 0x100f3b60 the tree emptied and the numbers zeroed
    // 0x100E4860 (ret 8): the recipe of the given index shown: its numbers, its product's name, icon, description and grade, and
    // the tree control filled with (product, success rate, 0, 0)
    __declspec(noinline) void SetRecipe(INT RecipeIndex, INT Unused);
};
NW_ASSERT_OFFSET(NCRecipeGaraTreeWnd, TreeCtrl, 0xEC);
NW_ASSERT_OFFSET(NCRecipeGaraTreeWnd, Product, 0xF0);
NW_ASSERT_OFFSET(NCRecipeGaraTreeWnd, MPCost, 0x1D30);
NW_ASSERT_OFFSET(NCRecipeGaraTreeWnd, CloseButton, 0x1D3C);
NW_ASSERT_SIZE(NCRecipeGaraTreeWnd, 0x1D40);

// NWM_COMMAND ids of the recipe making windows (names ours; the buy window closes with 0xF8)
enum
{
    NID_RECIPEMAKE_FRAME = 0xFF,      // the title bar (NControl::ID)
    NID_RECIPEMAKE_CREATE = 0xF9,     // -> OnMake (vslot 70)
    NID_RECIPEMAKE_PREV = 0xFA,       // -> OnPrev (vslot 67)
    NID_RECIPEMAKE_CLOSE = 0xFD,      // -> OnClose (vslot 68); NCRecipeBuyManufactureWnd: 0xF8
    NID_RECIPEMAKE_TREE = 0xFE,       // -> OnShowTree (vslot 69)
};

class NCRecipeManufactureWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCRecipeManufactureWnd, NCWnd, 0, 0x102FE078)

    NCRecipeManufactureItemWnd* IngredientWnd;  // +0xE8 the ingredients, 6 x 3 cells at (10, 141) 0xEE x 0x67
    UTexture* BackTex;               // +0xEC "L2UI_CH3.RecipeWnd.Recipe4_Back"
    UTexture* MPTex;                 // +0xF0 "L2UI_CH3.PlayerStatusWnd.ps_mpbar" (the creator's MP bar)
    UTexture* TreeTex;               // +0xF4 (name ours; new in C3) "L2UI_CH3.Etc.menu_outline": the tree button's frame
    UTexture* TreeDownTex;           // +0xF8 "L2UI_CH3.Etc.menu_outline_down": the mouse over / down on the button
    UTexture* TreeCurTex;            // +0xFC the one of the two that is drawn (TreeTex at first)
    NItemEntry Product;              // +0x100 the product: the icon, the strings and the grade (see above)
    INT RecipeID;                    // +0x1D40 what the requests take (NRecipeData::RecipeID)
    INT ProductID;                   // +0x1D44 the item made: the count kept is the sum of the inventory's stacks of it
    INT ProductCount;                // +0x1D48 "Number of Resulting Items"
    INT MPCost;                      // +0x1D4C "MP cost"
    INT SuccessRate;                 // +0x1D50 percent
    INT CreatorMP;                   // +0x1D54 the player's MP (the bar's numerator), set by the console
    INT CreatorMaxMP;                // +0x1D58 its maximum (the bar is 152 pixels at most)
    INT Result;                      // +0x1D5C (-1) the last try: 0 failed, 1 succeeded (OnPaint shows the system message
                                     //   960 / 959 with the product's name); the console sets it
    UBOOL bTreePressed;              // +0x1D60 (name ours; new in C3) the mouse went down on the tree button (OnLButtonUp clears it)
    NCButton* TreeButton;            // +0x1D64 id 0xFE, "L2UI.RecipeWnd.RecipeTreeBtn" at (206, 71) 32 x 32, tool tip 662
    NCButton* MakeButton;            // +0x1D68 id 0xF9 "Create" (sysstring 645) at (11, 372) 76 x 23
    NCButton* PrevButton;            // +0x1D6C id 0xFA "<PREV" (543) at (91, 372)
    NCButton* CloseButton;           // +0x1D70 id 0xFD "Close" (646) at (171, 372)
    INT Kind;                        // +0x1D74 (name ours) the recipe book it came from, 0 or 1 (the console's ReceiveRecipeItemMakeInfo
                                     //   sets it with SetField1D74 0x100F3940): <PREV asks for that book again

    NCRecipeManufactureWnd();                                        // 0x100f6790
    // ~NCRecipeManufactureWnd                                       // 0x100f6870

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x100f3830
    virtual INT OnCreate();                                          // 37 0x100f68d0
    virtual INT OnDestroy();                                         // 38 0x100f4380
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x100f4470
    virtual INT OnMouseMove(WPARAM wParam, LPARAM lParam);           // 67 0x100f4cf0 over the tree button: the pressed frame
    virtual INT OnLButtonDown(WPARAM wParam, LPARAM lParam);         // 68 0x100f4cb0
    virtual INT OnLButtonUp(WPARAM wParam, LPARAM lParam);           // 69 0x100f38e0
    // names ours
    virtual INT OnPrev();                                            // 74 0x100f3950 hides the window and asks for the recipe book
    virtual INT OnClose();                                           // 75 0x100efda0 (NCRecipeBuyListWnd's body, shared: hides, plays the sound)
    virtual INT OnShowTree();                                        // 76 0x100f5df0 (the buy window's, shared)
    virtual INT OnMake();                                            // 77 0x100f3a20 RequestRecipeItemMakeSelf(RecipeID)

    // names ours
    void Reset();                                                    // 0x100f3900 the numbers zeroed, the grid emptied
    // 0x100E5FA0: shows Rec (the same code as the buy window's 0x100E0FF0): the numbers, the product's strings and icon, and
    // an entry per ingredient with how many of it the player carries
    void SetRecipe(NRecipeData* Rec);
    // 0x100DED60 (shared with the buy window): the entry goes to the ingredient grid
    __declspec(noinline) UBOOL AddIngredient(NItemEntry* Item);
    // 0x100E37C0 (ret 8; sub_ in the db): (X, Y) inside the tree button's rectangle (name ours)
    UBOOL HitTree(INT X, INT Y);
};
NW_ASSERT_OFFSET(NCRecipeManufactureWnd, TreeCurTex, 0xFC);
NW_ASSERT_OFFSET(NCRecipeManufactureWnd, Product, 0x100);
NW_ASSERT_OFFSET(NCRecipeManufactureWnd, RecipeID, 0x1D40);
NW_ASSERT_OFFSET(NCRecipeManufactureWnd, CreatorMP, 0x1D54);
NW_ASSERT_OFFSET(NCRecipeManufactureWnd, Result, 0x1D5C);
NW_ASSERT_OFFSET(NCRecipeManufactureWnd, bTreePressed, 0x1D60);
NW_ASSERT_OFFSET(NCRecipeManufactureWnd, TreeButton, 0x1D64);
NW_ASSERT_OFFSET(NCRecipeManufactureWnd, CloseButton, 0x1D70);
NW_ASSERT_OFFSET(NCRecipeManufactureWnd, Kind, 0x1D74);
NW_ASSERT_SIZE(NCRecipeManufactureWnd, 0x1D78);

#pragma pack(pop)
