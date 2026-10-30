#pragma once
// NCRecipeBookWnd - the recipe book (UClass "CRecipeBookWnd", 0x170; new in C2): the grid of the recipes the
// character knows (NCRecipeBookItemWnd, "(n/50)" in the corner) and a trash icon to forget one. The console fills
// it when the server sends the book (RequestRecipeBookOpen asks for it) through AddRecipe.

#include "nwindow/NCWnd.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCRecipeBookItemWnd;
struct NItemInfo;

class NCRecipeBookWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCRecipeBookWnd, NCWnd, 0, 0x102FF008)

    UTexture* BackTex;                // +0xE8 "L2UI_CH3.RecipeWnd.Recipe2_Back"
    UTexture* TrashTex;               // +0xEC "L2UI_CH3.InventoryWnd.Inventory_trash"
    UTexture* TrashActiveTex;         // +0xF0 "L2UI_CH3.InventoryWnd.Inventory_trash_Drag" while the drag is over it
    NToolTipInfo TrashToolTip;        // +0xF4 (C3) the trash icon's tool tip (SysString 890, style 0x400; OnCreate)
    NCRecipeBookItemWnd* Items;       // +0x174 (name ours) the grid
    INT TrashX;                       // +0x178 (name ours) the trash icon's place in the window (0xD0)
    INT TrashY;                       // +0x17C (0x15B)
    FLOAT TrashW;                     // +0x180 (34.0)
    FLOAT TrashH;                     // +0x184 (34.0)
    UBOOL bTrashActive;               // +0x188 (name ours) the grid's OnMouseMove sets it: a recipe is dragged over the icon
    UBOOL bOverTrash;                 // +0x18C (name ours; new in C3) the mouse is over the icon (OnMouseMove): its tool tip shows
    INT Unknown0BC;                   // +0x190 (name ours) the constructor and Clear zero it, nothing here reads it
    INT Kind;                         // +0x194 (name ours) the book's kind (the console's ReceiveRecipeBookItemList sets it with SetKind 0x100f37f0):
                                      //   0 titles the window with string 1215, 1 with string 1214; OnPaint picks the limit by it
    INT LimitKind1;                   // +0x198 (name ours) the most recipes the book holds when Kind is 1; no code seen writes it
    INT LimitOther;                   // +0x19C (name ours) the same for the other kind; no code seen writes it

    NCRecipeBookWnd();                                               // 0x100f5f70
    // ~NCRecipeBookWnd                                              // 0x100f6030

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10148f10 (NControl's, shared, hooked there; a stub here)
    virtual INT OnCreate();                                          // 37 0x100f6090
    virtual INT OnDestroy();                                         // 38 0x100f3f90
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x100f3fd0
    virtual INT OnDrawToolTip(NCGDevice* Device);                    // 63 0x100f6240 (the db's OnPostPaint)
    virtual INT OnMouseMove(WPARAM wParam, LPARAM lParam);           // 67 0x100f36c0

    // names ours (the console calls them: slots 123 and 124)
    void Clear();                                                    // 0x100f3770 the grid emptied
    UBOOL HitTrash(INT X, INT Y);                                    // 0x100f3790 (ret 8; sub_ in the db) the point is on the trash icon (the
                                                                     //   bounds are inclusive, the window's screen origin added)
    INT AddRecipe(NItemInfo* Info);                                  // 0x100f59a0 into the grid, always 1
};
NW_ASSERT_OFFSET(NCRecipeBookWnd, BackTex, 0xE8);
NW_ASSERT_OFFSET(NCRecipeBookWnd, TrashToolTip, 0xF4);
NW_ASSERT_OFFSET(NCRecipeBookWnd, Items, 0x174);
NW_ASSERT_OFFSET(NCRecipeBookWnd, bTrashActive, 0x188);
NW_ASSERT_OFFSET(NCRecipeBookWnd, bOverTrash, 0x18C);
NW_ASSERT_OFFSET(NCRecipeBookWnd, Unknown0BC, 0x190);
NW_ASSERT_OFFSET(NCRecipeBookWnd, Kind, 0x194);
NW_ASSERT_OFFSET(NCRecipeBookWnd, LimitOther, 0x19C);
NW_ASSERT_SIZE(NCRecipeBookWnd, 0x1A0);

#pragma pack(pop)
