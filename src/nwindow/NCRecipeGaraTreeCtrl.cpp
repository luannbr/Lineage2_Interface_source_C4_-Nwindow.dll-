/*=============================================================================
    NCRecipeGaraTreeCtrl.cpp - the scrolling frame of the recipe tree (C2: NCRecipeTreeCtrl): NCGaraTreeCtrl's code with
    its own tree at +0x114. C3 stock: 0x100E3CD0 .. 0x100E3ECC (paint, reset, scroll bar), 0x100E4550 (AddRecipe),
    0x100E5A00 .. 0x100E5B5F (constructor, destructor, OnCreate). The destroy, the wheel, the scrolls and the arrange
    of the quest tree's frame (0x100EAF80, 0x100E3ED0, 0x100ECAD0, 0x100DB2B0, 0x100ECA00) are the same code on the
    field at +0x114 and the stock vtable shares those bodies: written here as overrides without a hook of their own.
=============================================================================*/
#include "nwindow/NCRecipeGaraTreeCtrl.h"
#include "nwindow/NItemC4.h"
#include "nwindow/NConsoleWndC4.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NCSliderBar.h"
#include "nwindow/NCConsole.h"
#include "nwindow/NCInventoryWnd.h"
#include "nwindow/NCInvenItemWnd.h"
#include "nwindow/NRecipeData.h"

// 0x100f6e20 - the base's, and the same Unknown0C8 (16) again; the tree comes with OnCreate
NCRecipeGaraTreeCtrl::NCRecipeGaraTreeCtrl()
{
    Unknown0C8 = 16;
}
NW_HOOK_CTOR(0x100f6e20, NCRecipeGaraTreeCtrl)

// 0x100f6e70
NW_HOOK_DTOR(0x100f6e70, NCRecipeGaraTreeCtrl)

// 0x100A1050 (vslot 25) - NCSkillItemWnd's body (hooked there): NCScrollWnd's, as a flag
INT NCRecipeGaraTreeCtrl::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return NCScrollWnd::WindowProc(Msg, wParam, lParam) != 0;
}

// C2 CANDIDATE: the C2 function is not the same code as the C1 one; kept only while it matches the stock one (verify_data)
// 0x100f6ec0 (vslot 37) - NCScrollWnd's (not NCGaraTreeCtrl's), then the recipe tree (id 0xA00), the window's width less
// 15 pixels
INT NCRecipeGaraTreeCtrl::OnCreate()
{
    NCScrollWnd::OnCreate();
    RecipeTree = new NCRecipeGaraTree();
    RecipeTree->ID = 0xA00;
    RecipeTree->SetPosition(0, 0, (INT)(Width - 15.0f), (INT)Height);
    CreateChild(RecipeTree, NWS_VISIBLE);
    return 1;
}
NW_HOOK_METHOD(0x100f6ec0, NCRecipeGaraTreeCtrl, OnCreate)

// 0x100EAF80 (vslot 38, shared with NCQuestGaraTreeCtrl's, hooked there) - no guard here
INT NCRecipeGaraTreeCtrl::OnDestroy()
{
    if (RecipeTree)
    {
        RemoveChildWindow(RecipeTree->Entry);
        delete RecipeTree;
        RecipeTree = NULL;
    }
    NCScrollWnd::OnDestroy();
    return 1;
}

// 0x100f5130 (vslot 62) - the scroll range follows the tree's height (and 21), and the entries are put where they show
INT NCRecipeGaraTreeCtrl::OnPaint(NCGDevice* Device)
{
    guard(NCRecipeGaraTreeCtrl::OnPaint);
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    SetContentHeightEx(RecipeTree->ContentHeight + 21);
    if (RecipeTree->Root)
        ArrangeItem(RecipeTree->Root);
    Device->ReleaseClip();
    Device->EndPaint();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100f5130, NCRecipeGaraTreeCtrl, OnPaint)

// 0x100ECA00 (vslot 73, shared with NCQuestGaraTreeCtrl's) - an entry whose box (its size from the paint, at the scroll
// offset) is inside the window is shown, the others hidden; then the ones under it
void NCRecipeGaraTreeCtrl::ArrangeItem(NCGaraTVItem* Item)
{
    if (!Item)
        return;
    INT Right = Item->CopyW + ScrollX;
    INT Bottom = Item->CopyH + ScrollY;
    NRectF Local;
    Local.X = 0;
    Local.Y = 0;
    Local.W = 0.0f;
    Local.H = 0.0f;
    RecipeTree->GetLocalRect(&Local);
    if (Right >= 0 && Right <= (INT)Local.W && Bottom >= 0 && Bottom <= (INT)Local.H)
        Item->ShowWindow();
    else
        Item->HideWindow();
    for (INT i = 0; i < Item->Children.Num(); i++)
        ArrangeItem(Item->Children(i));
}

// 0x100ECAD0 (vslot 69, shared with NCQuestGaraTreeCtrl's)
INT NCRecipeGaraTreeCtrl::ScrollUp()
{
    if (ScrollY < 0)
    {
        DeltaX = ScrollX;
        DeltaY = ScrollY;
        ScrollY += LineStep;
        if (ScrollY > 0)
            ScrollY = 0;
        ScrollBar->SetScrollPos(-ScrollY);
        DeltaX -= ScrollX;
        DeltaY -= ScrollY;
        RecipeTree->MoveWindow(0, LineStep);
        RecipeTree->ScrollOffset -= LineStep;
    }
    return 1;
}

// 0x100DB2B0 (vslot 70, shared with NCQuestGaraTreeCtrl's)
INT NCRecipeGaraTreeCtrl::ScrollDown()
{
    if (ScrollY > MinScrollY)
    {
        DeltaX = ScrollX;
        DeltaY = ScrollY;
        ScrollY -= LineStep;
        if (ScrollY < MinScrollY)
            ScrollY = MinScrollY;
        ScrollBar->SetScrollPos(-ScrollY);
        DeltaX -= ScrollX;
        DeltaY -= ScrollY;
        RecipeTree->MoveWindow(0, -LineStep);
        RecipeTree->ScrollOffset += LineStep;
    }
    return 1;
}

// 0x100f52b0 (vslot 77) - the slider's value is the position as it comes; the tree moves by the difference
INT NCRecipeGaraTreeCtrl::OnScrollBar(INT Value)
{
    DeltaX = ScrollX;
    DeltaY = ScrollY;
    ScrollY = -Value;
    DeltaX -= ScrollX;
    DeltaY -= ScrollY;
    RecipeTree->MoveWindow(0, -DeltaY);
    RecipeTree->ScrollOffset += DeltaY;
    return 1;
}
NW_HOOK_METHOD(0x100f52b0, NCRecipeGaraTreeCtrl, OnScrollBar)

// 0x100E3ED0 (vslot 67, shared with NCQuestGaraTreeCtrl's) - three line steps per notch (120), up or down
INT NCRecipeGaraTreeCtrl::OnMouseWheel(WPARAM wParam, LPARAM lParam)
{
    SHORT Notches = (SHORT)((SHORT)(wParam >> 16) / 120);
    DeltaX = ScrollX;
    DeltaY = ScrollY;
    if (Notches > 0)
    {
        if (ScrollY < 0)
        {
            ScrollY += LineStep * Notches * 3;
            if (ScrollY > 0)
                ScrollY = 0;
            ScrollBar->SetScrollPos(-ScrollY);
        }
    }
    else
    {
        if (ScrollY > MinScrollY)
        {
            ScrollY += LineStep * Notches * 3;
            if (ScrollY < MinScrollY)
                ScrollY = MinScrollY;
            ScrollBar->SetScrollPos(-ScrollY);
        }
    }
    DeltaX -= ScrollX;
    DeltaY -= ScrollY;
    RecipeTree->MoveWindow(0, -DeltaY);
    RecipeTree->ScrollOffset += DeltaY;
    return 1;
}

// 0x100f5220 (vslot 72, name ours) - the tree is emptied and put back at the origin of the window, the scroll starts over
void NCRecipeGaraTreeCtrl::ResetScroll()
{
    RecipeTree->Clear();
    NRectF Local;
    Local.X = 0;
    Local.Y = 0;
    Local.W = 0.0f;
    Local.H = 0.0f;
    RecipeTree->GetLocalRect(&Local);
    RecipeTree->MoveWindow(-Local.X, -Local.Y);
    RecipeTree->ScrollOffset = 0;
    ScrollY = 0;
    ScrollX = 0;
    DeltaY = 0;
    DeltaX = 0;
    MinScrollY = 0;
    ScrollBar->ThumbPos = 0;
}
NW_HOOK_METHOD(0x100f5220, NCRecipeGaraTreeCtrl, ResetScroll)

// A local NGaraTVItemData (0x1E70 bytes) whose constructor is the one declared (0x10014BB0) and whose destructor is run by hand
// (0x10014D20, ret 0). The union keeps the compiler from destroying the member itself.
struct NGaraTVItemLocal
{
    union { NGaraTVItemData Data; };
    NGaraTVItemLocal() { new ((void*)&Data) NGaraTVItemData; }
    ~NGaraTVItemLocal() { Data.~NGaraTVItemData(); }
};

// 0x100f59c0 (ret 0x10) - see the header. The stock code has the two cases as two copies of the record (its frame holds
// both): with a recipe the entry's data is the product's record and the ingredients follow (each at 100 percent, the
// entry just made as their parent); without one the item's own record and no children. The number the player has is
// what the first inventory page holds of the item. The item record is the C4 one (icon name +0x3C, name +0x50,
// description +0x58)
void NCRecipeGaraTreeCtrl::AddRecipe(INT ItemID, INT SuccessRate, INT Need, NCGaraTVItem* Parent)
{
    NRecipeData* Rec = NGetRecipeBy2Condition(ItemID, SuccessRate);
    if (Rec && Rec->ProductRec)
    {
        NGaraTVItemLocal InfoLocal;
        NGaraTVItemData& Info = InfoLocal.Data;
        NViewItemDataC4* Item = (NViewItemDataC4*)Rec->ProductRec;
        Info.Icon = FindTexture(*Item->IconName);
        if (!Info.Icon)
            Info.Icon = FindTexture(TEXT("NWindow.BlackTexture"));
        Info.IconH = 32;
        Info.IconW = 32;
        appStrcpy(Info.Text, *Item->Name);
        appStrcpy(Info.Text2, *Item->Description);
        Info.IconSet = Need;
        NCInvenItemWnd* Page0 = GDesktop->InventoryWnd->ItemWnd[0];
        if (Page0)
        {
            for (INT i = 0; i < Page0->Items.Num(); i++)
                if (ItemID == AsC4(Page0->Items(i))->ID)
                    Info.LevelMin += AsC4(Page0->Items(i))->Count;
        }
        NCGaraTVItem* Node = RecipeTree->AddItem(&Info, Parent);
        for (INT i = 0; i < Rec->Materials.Num(); i++)
        {
            NRecipeMaterial* Material = Rec->Materials(i);
            if (Material)
                AddRecipe(Material->ItemID, 100, Material->Count, Node);
        }
    }
    else
    {
        NViewItemDataC4* Item = (NViewItemDataC4*)NItemTable::Find(&ItemID);
        if (!Item)
            return;
        NGaraTVItemLocal InfoLocal;
        NGaraTVItemData& Info = InfoLocal.Data;
        Info.Icon = FindTexture(*Item->IconName);
        if (!Info.Icon)
            Info.Icon = FindTexture(TEXT("NWindow.BlackTexture"));
        Info.IconH = 32;
        Info.IconW = 32;
        appStrcpy(Info.Text, *Item->Name);
        appStrcpy(Info.Text2, *Item->Description);
        Info.IconSet = Need;
        NCInvenItemWnd* Page0 = GDesktop->InventoryWnd->ItemWnd[0];
        if (Page0)
        {
            for (INT i = 0; i < Page0->Items.Num(); i++)
                if (ItemID == AsC4(Page0->Items(i))->ID)
                    Info.LevelMin += AsC4(Page0->Items(i))->Count;
        }
        RecipeTree->AddItem(&Info, Parent);
    }
}
NW_HOOK_METHOD(0x100f59c0, NCRecipeGaraTreeCtrl, AddRecipe)

NW_CHECK_VTABLE(NCRecipeGaraTreeCtrl, 0x101c8170, 80)
