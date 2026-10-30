#pragma once
// NCInventoryWnd - the inventory (UClass "CInventoryWnd", 0xF8; the desktop, NCConsole, creates
// it in OnCreate and keeps it at +0xBC): the paperdoll (NCEquipItemWnd) on top, two tab pages
// of items below ("Item" and "Quest", both NCInvenItemWnd: the quest items, NItemInfo::Param
// 3, go to the second), the adena and the weight bar at the bottom, between the crystallize
// icon (shown with the skill) and the trash icon; the item windows light those up while an
// item is carried over them. The console fills it through AddItem / UpdateItem / ClearItems.

#include "nwindow/NCConsole.h"
#include "nwindow/NConsoleWnd.h"
#include "nwindow/NGameData.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCButton;
class NCEquipItemWnd;
class NCInvenItemWnd;
class NCTabControl;
struct NItemEntry;
struct NItemInfo;

class NCInventoryWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCInventoryWnd, NCWnd, 0, 0x102DBDD0)

    // C4 layout (0x25C), from the constructor (0x10099900), OnCreate (0x10099A60) and the painters. C3 had the weight
    // bar's five textures here and a manufacture button; C4 moved the bar into a child window of its own (WeightWnd).
    UTexture* BackTex;               // +0xE8 "L2UI_CH3.InventoryWnd.Inventory_Back"
    UTexture* RecipeTex;             // +0xEC "L2UI_CH3.InventoryWnd.Inventory_recipe"
    UTexture* RecipeDragTex;         // +0xF0 "L2UI_CH3.InventoryWnd.Inventory_recipe_drag"
    UTexture* TrashTex;              // +0xF4 "L2UI_CH3.InventoryWnd.Inventory_trash"
    UTexture* TrashDragTex;          // +0xF8 "L2UI_CH3.InventoryWnd.Inventory_trash_drag"
    UTexture* OutlineTex;            // +0xFC "L2UI_CH3.Etc.menu_outline" (read by nobody ported: the manufacture button is gone)
    UTexture* OutlineDownTex;        // +0x100 "L2UI_CH3.Etc.menu_outline_down"
    UTexture* AdenaTex;              // +0x104 "L2UI.WindowIcon.WindowIconAdena"
    UTexture* WeightIconTex;         // +0x108 "L2UI.WindowIcon.Weight"
    NCInvenItemWnd* ItemWnd[2];      // +0x10C the pages: 0 "Item", 1 "Quest"
    NCEquipItemWnd* EquipWnd;        // +0x114
    NCTabControl* TabCtrl;           // +0x118 the two tabs (ID 0xFE001A)
    NCWnd* WeightWnd;                // +0x11C (C4, name ours) NCInvenWeightWnd, the weight bar (117, 372; 85 x 12)
    UBOOL bOverRecipe;               // +0x120 (name ours) an item is carried over the recipe icon (the equipment window's
                                     //   OnMouseMove sets it)
    UBOOL bOverTrash;                // +0x124 (name ours) ... over the trash icon
    INT TrashX;                      // +0x128 (208, 351)
    INT TrashY;                      // +0x12C
    FLOAT TrashW;                    // +0x130 (34.0)
    FLOAT TrashH;                    // +0x134 (34.0)
    INT RecipeX;                     // +0x138 (14, 351)
    INT RecipeY;                     // +0x13C
    FLOAT RecipeW;                   // +0x140 (34.0)
    FLOAT RecipeH;                   // +0x144 (34.0)
    NCButton* TrashButton;           // +0x148 hidden until the player may crystallize
    NCButton* RecipeButton;          // +0x14C shown with the crystallize flag (ShowWindow)
    TCHAR ItemOrderFile[128];        // +0x150 "..\iwo\<coded name>_<server>.dat" (ReadItemOrder)
    TArray<INT> ItemOrder;           // +0x250 the ids of the "Item" page's items as they stood when the window was last
                                     //   hidden (SaveItemOrder); RestoreItemOrder puts the page back in that order

    NCInventoryWnd();                                                // 0x10099900
    // ~NCInventoryWnd                                               // 0x10099a00 (ItemOrder, the base)

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10148f10
    virtual INT OnCreate();                                          // 37 0x10099a60
    virtual INT OnDestroy();                                         // 38 0x10098be0
    virtual void ShowWindow();                                       // 48 0x10098dc0
    virtual void HideWindow();                                       // 49 0x10098670
    virtual void Minimize();                                         // 54 0x10093ea0
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x10095870
    // OnMouseMove / OnLButtonDown / OnLButtonUp (slots 67 .. 69) are NCWnd's own: the stock vtable points at 0x10036240 / 0x10036350 / 0x10036410

    // names ours
    void AddHenna(INT HennaID, INT bUsable);                         // 0x10099000 the paperdoll's (names ours)
    void ClearHennas();                                              // 0x1009a5f0
    void ReloadTexts();                                              // 0x10095b40 names and tool tips
    NItemEntry* FindItem(INT ObjectID);                              // 0x10095cd0 paperdoll, "Item" page
    void ClearItems();                                               // 0x10098d90
    UBOOL AddItem(NItemInfo* Info);                                  // 0x10098270
    UBOOL UpdateItem(NItemInfo* Info, INT Change);                   // 0x10098e90 1 added, 2 changed,
                                                                     //   3 removed
    void SaveItemOrder();                                            // 0x100982c0 (name ours)
    void MoveLastToFront();                                          // 0x10098610 (name ours) the page's last item first
    void RestoreItemOrder();                                         // 0x10099010 (name ours)
    void ApplyItemOrder();                                           // 0x100991f0 (name ours) when the page has items
    void ReadItemOrder();                                            // 0x10098390 (name ours) the saved file back into ItemOrder
    UBOOL IsInTrash(INT X, INT Y);                                   // 0x10093f00 (ret 8; name ours) the trash icon's box, edges included
    UBOOL IsInRecipe(INT X, INT Y);                                  // 0x10093f60 (ret 8; name ours) the recipe icon's box
};
NW_ASSERT_OFFSET(NCInventoryWnd, BackTex, 0xE8);
NW_ASSERT_OFFSET(NCInventoryWnd, TrashTex, 0xF4);
NW_ASSERT_OFFSET(NCInventoryWnd, OutlineTex, 0xFC);
NW_ASSERT_OFFSET(NCInventoryWnd, AdenaTex, 0x104);
NW_ASSERT_OFFSET(NCInventoryWnd, ItemWnd, 0x10C);
NW_ASSERT_OFFSET(NCInventoryWnd, WeightWnd, 0x11C);
NW_ASSERT_OFFSET(NCInventoryWnd, bOverRecipe, 0x120);
NW_ASSERT_OFFSET(NCInventoryWnd, bOverTrash, 0x124);
NW_ASSERT_OFFSET(NCInventoryWnd, TrashX, 0x128);
NW_ASSERT_OFFSET(NCInventoryWnd, RecipeX, 0x138);
NW_ASSERT_OFFSET(NCInventoryWnd, TrashButton, 0x148);
NW_ASSERT_OFFSET(NCInventoryWnd, ItemOrderFile, 0x150);
NW_ASSERT_OFFSET(NCInventoryWnd, ItemOrder, 0x250);
NW_ASSERT_SIZE(NCInventoryWnd, 0x25C);

// The item table's search by the item id (NDataMap::FindRef, 0x1004dda0): the entry, NULL when
// there is none.
namespace NItemTable
{
    inline NViewItemData* Find(INT* ItemID)
    {
        return GL2GameData.ItemTable->FindRef(*ItemID);
    }
}

// a system line in the desktop's chat window
inline void DesktopSystemMessage(const TCHAR* Text)
{
    GDesktop->AddSystemText(Text, 0, 0xFFFFFFFF);
}

#pragma pack(pop)