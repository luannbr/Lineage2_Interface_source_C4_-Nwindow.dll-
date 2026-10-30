#pragma once
// NRecipeData - what the recipe windows read of the engine's recipe records (FL2RecipeData, declared opaque in
// include/ue2/UE2Engine.h: FL2GameData::GetRecipeDataByIndex / GetRecipeDataBy2Condition return it). A view struct (name
// ours): use it through NGetRecipe*, which cast the engine's pointer.
//
// Layout derived from the stock readers: NCRecipeManufactureWnd::SetRecipe 0x100c7de0 (copies +0x1C, +0x28 .. +0x34 into
// the window and walks the materials), the recipe tree's fill sub_100c6e20 / sub_100c6af0 (the second searches the
// recipe that makes a material: GetRecipeDataBy2Condition(item id, 100) in its recursion), and the console's
// NConsoleWnd::vfn124 / 125 / 127 / 128 / 130 / 131 (0x10066870 ..: they read +0x1C, +0x20, +0x24, +0x28, +0x34).
// The meanings come from the windows that show the values (the C1 project's F6_Craft.md names the same labels: "MP cost",
// "Success Rate", "Number of Resulting Items", "Lv.").

#include "nwindow/NGameData.h"
#pragma pack(push, 4)

// One ingredient of a recipe: the item and how many (the element the materials array points to; NCRecipeManufactureWnd's
// SetRecipe copies them into an NItemEntry's ID (+0x1918) and its needed amount (UnknownC34 +0x1938)).
struct NRecipeMaterial
{
    INT ItemID;    // +0x00
    INT Count;     // +0x04
};
NW_ASSERT_SIZE(NRecipeMaterial, 8);

struct NRecipeData
{
    NViewItemData* ProductRec;             // +0x00 the item record of the product (the tree takes the icon, the name, the
                                           //   description and the grade of it: it is what FindRef(ProductID) finds); the readers
                                           //   treat 0 as "no recipe here"
    TArray<NRecipeMaterial*> Materials;    // +0x04 the ingredients (Data, Num, Max)
    BYTE _Unknown10[0x1C - 0x10];          // +0x10
    INT RecipeID;                          // +0x1C what the requests to the server take (RequestRecipeItemMakeSelf, ShopMakeDo)
    INT RecipeItemID;                      // +0x20 the item id of the recipe itself (the console looks up its name)
    INT Level;                             // +0x24 the recipe's level (the tree window's "Lv.N")
    INT ProductID;                         // +0x28 the item it makes (the item table's key)
    INT ProductCount;                      // +0x2C how many come out ("Number of Resulting Items")
    INT MPCost;                            // +0x30 "MP cost"
    INT SuccessRate;                       // +0x34 percent ("Success Rate"); the material search uses 100
};
NW_ASSERT_OFFSET(NRecipeData, Materials, 0x04);
NW_ASSERT_OFFSET(NRecipeData, RecipeID, 0x1C);
NW_ASSERT_OFFSET(NRecipeData, ProductID, 0x28);
NW_ASSERT_OFFSET(NRecipeData, SuccessRate, 0x34);

// FL2GameData::GetRecipeDataByIndex / GetRecipeDataBy2Condition through the view (forced inline: the stock callers
// call the import directly)
FORCEINLINE NRecipeData* NGetRecipeByIndex(INT Index)
{
    return reinterpret_cast<NRecipeData*>(GL2GameData.GetRecipeDataByIndex(Index));
}
FORCEINLINE NRecipeData* NGetRecipeBy2Condition(INT P1, INT P2)
{
    return reinterpret_cast<NRecipeData*>(GL2GameData.GetRecipeDataBy2Condition(P1, P2));
}

// The item table's search by id (the stock code does the load of the table and calls TMapBase::FindRef, 0x10061dc0), forced
// inline like the stock callers' own code
FORCEINLINE NViewItemData* NRecipeFindItem(INT* ItemID)
{
    return GL2GameData.ItemTable->FindRef(*ItemID);
}

// The crystal grade the recipe windows show of an item record (NViewItemData): by its class, weapons keep it at +0xBC,
// armors at +0x214, etc items at +0x8C (unnamed in NGameData.h); any other class leaves *Grade alone. The record's
// description is an FString at +0x48 here (NGameData.h declares an L2FName there: the C2 readers use FString::operator*).
FORCEINLINE void NItemRecordGrade(NViewItemData* Item, INT* Grade)
{
    if (Item->Class == ITEMCLASS_Weapon)
        *Grade = Item->CrystalGrade;
    else if (Item->Class == ITEMCLASS_Armor)
        *Grade = Item->ArmorCrystalGrade;
    else if (Item->Class == ITEMCLASS_Etc)
        *Grade = *(INT*)((BYTE*)Item + 0x8C);
}
FORCEINLINE FString& NItemRecordDescription(NViewItemData* Item)
{
    return *(FString*)((BYTE*)Item + 0x48);
}

#pragma pack(pop)
