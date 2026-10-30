/*=============================================================================
    NCRecipeGaraTree.cpp - the ingredient tree of the recipe information window (C2: NCRecipeTree). C3 stock:
    0x100E26A0 .. 0x100E2710 (OnCreate, OnDestroy), 0x100E38E0 .. 0x100E3CC7 (paint, the entry drawing),
    0x100E5960 .. 0x100E59B0 (constructor, destructor).
=============================================================================*/
#include "nwindow/NCRecipeGaraTree.h"
#include "nwindow/NCGDevice.h"

// what the recipe code reads of an entry (the record's third and fourth dwords: the number the recipe needs and the number
// the player has; CopyW / CopyH are the entry's place for NCRecipeGaraTreeCtrl's visibility test)
static FORCEINLINE INT& EntryNeed(NCGaraTVItem* Item) { return Item->Data.IconSet; }
static FORCEINLINE INT& EntryHave(NCGaraTVItem* Item) { return Item->Data.LevelMin; }

// 0x100f6d80  stays stock in hook mode: the stock one is NCGaraTree's constructor (0x10015b70) and the vtable 0x101C8030 (18 bytes;
// NCGaraTree.cpp has the base constructor, hooked), and the object of a hooked constructor of this class would be built twice.
// The standalone constructor is the compiler's: the base one, then our vtable
NCRecipeGaraTree::NCRecipeGaraTree()
{
#if NW_HOOK_MODE
    ::nw::StockCallAt<void>(0x100f6d80, this);
#endif
    // the standalone: NCGaraTree()'s real body (0x10015b70) and this class's vtable are all the stock constructor does
}

// 0x100f6dd0
NW_HOOK_DTOR(0x100f6dd0, NCRecipeGaraTree)

// 0x10041760 (vslot 25) - the stock body is the one the linker shared with NCExButton's: NControl's, as a flag
INT NCRecipeGaraTree::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return NControl::WindowProc(Msg, wParam, lParam) != 0;
}

// C2 CANDIDATE: the C2 function is not the same code as the C1 one; kept only while it matches the stock one (verify_data)
// 0x100f3a40 (vslot 37) - NControl's, then the five entry textures
INT NCRecipeGaraTree::OnCreate()
{
    NControl::OnCreate();
    FrameTex = FindTexture(TEXT("L2UI.RecipeWnd.RecipeTreeIconBack"));
    FrameOpenTex = FindTexture(TEXT("L2UI.RecipeWnd.RecipeTreeIconBack_click"));
    FrameLeafTex = FindTexture(TEXT("L2UI.RecipeWnd.RecipeTreeIconDisableBack"));
    PlusTex = FindTexture(TEXT("L2UI.RecipeWnd.TreePlus"));
    MinusTex = FindTexture(TEXT("L2UI.RecipeWnd.TreeMinus"));
    return 1;
}
NW_HOOK_METHOD(0x100f3a40, NCRecipeGaraTree, OnCreate)

// 0x100f3ac0 (vslot 38) - NCGaraTree's (the entries go); no guard here
INT NCRecipeGaraTree::OnDestroy()
{
    NCGaraTree::OnDestroy();
    return 1;
}
NW_HOOK_METHOD(0x100f3ac0, NCRecipeGaraTree, OnDestroy)

// 0x100f4d40 (vslot 62) - the clip is the window's top moved by the view offset, 246 high (the frame's inner height); the
// tree is drawn from the root at (15, 5) and the window takes the height that came out (the scroll frame reads it)
INT NCRecipeGaraTree::OnPaint(NCGDevice* Device)
{
    guard(NCRecipeGaraTree::OnPaint);
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScrollOffset + ScreenY, (INT)Width, 246);
    ContentHeight = DrawItem(Device, Root, 15, 5);
    SetWindowSize(Width, (FLOAT)ContentHeight);
    Device->ReleaseClip();
    Device->EndPaint();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100f4d40, NCRecipeGaraTree, OnPaint)

// C2 CANDIDATE: the C2 function is not the same code as the C1 one; kept only while it matches the stock one (verify_data)
// 0x100f4e30 (vslot 72, ret 0x10) - the entry is placed at (X, Y) (its window rectangle: wider by 16 with children; the
// place is kept in CopyW / CopyH for NCRecipeGaraTreeCtrl's visibility test), its frame and icon drawn: expanded, the open
// frame and a minus sign at (X - 14, Y + 10); collapsed with children, the frame and a plus sign; a leaf, the disabled
// frame. Then the name at (icon width + X + 5, Y + 4) and below it "(have/need)" with a dimming square over the icon when
// the player has less than the recipe needs (the root has neither). The children follow: expanded, each one drawn under
// the previous (their column starts half an icon to the right; the heights add up), and the height used is returned;
// collapsed, they are hidden. An entry without children returns its own height (icon + 5); no entry, 0.
INT NCRecipeGaraTree::DrawItem(NCGDevice* Device, NCGaraTVItem* Item, INT X, INT Y)
{
    TCHAR Text[32];
    if (!Item)
        return 0;
    Item->CopyW = X;
    Item->CopyH = Y;
    if (Item->Children.Num() > 0)
        Item->SetPosition(X, Y, Item->Data.Width + 16, Item->Data.Height);
    else
        Item->SetPosition(X, Y, Item->Data.Width, Item->Data.Height);
    Device->DrawTexture(X, Y, Item->Data.IconW, Item->Data.IconH, 0, 0, Item->Data.IconW, Item->Data.IconH,
                        Item->Data.Icon, 0xFF, 1);
    if (Item->bExpanded)
    {
        Device->DrawTexture(X, Y, 32, 32, 0, 0, 32, 32, FrameOpenTex, 0xFF, 1);
        Device->DrawTexture(X - 14, Y + 10, 12, 12, 0, 0, 12, 12, MinusTex, 0xFF, 1);
    }
    else if (Item->Children.Num() > 0)
    {
        Device->DrawTexture(X, Y, 32, 32, 0, 0, 32, 32, FrameTex, 0xFF, 1);
        Device->DrawTexture(X - 14, Y + 10, 12, 12, 0, 0, 12, 12, PlusTex, 0xFF, 1);
    }
    else
        Device->DrawTexture(X, Y, 32, 32, 0, 0, 32, 32, FrameLeafTex, 0xFF, 1);
    Device->DrawText(Item->Data.IconW + X + 5, Y + 4, 0xFFDCDCDC, Item->Data.Text, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0,
                     0, 0, 0);
    if (Item != Root)
    {
        appSprintf(Text, TEXT("(%d/%d)"), EntryHave(Item), EntryNeed(Item));
        Device->DrawText(Item->Data.IconW + X + 5, Y + 18, 0xFFDCDCDC, Text, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0,
                         0);
        if (EntryHave(Item) < EntryNeed(Item))
            Device->DrawTexture(X, Y, 32, 32, 0, 0, 32, 32, FindTexture(TEXT("NWindow.ChatBack")), 0xFF, 1);
    }
    INT Used = Item->Data.IconH + 5;
    if (Item->Children.Num() > 0)
    {
        if (Item->bExpanded)
        {
            for (INT i = 0; i < Item->Children.Num(); i++)
                Used += DrawItem(Device, Item->Children(i), Item->Data.IconW / 2 + X, Used + Y);
            return Used;
        }
        for (INT i = 0; i < Item->Children.Num(); i++)
            Item->Children(i)->HideWindow();
    }
    return Used;
}
NW_HOOK_METHOD(0x100f4e30, NCRecipeGaraTree, DrawItem)

NW_CHECK_VTABLE(NCRecipeGaraTree, 0x101c8030, 79)
