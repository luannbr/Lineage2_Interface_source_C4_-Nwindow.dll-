/*=============================================================================
    NCRecipeBookItemWnd.cpp - the recipe book's icon grid (new in C2).
    Stock (C3): 0x100E27E0 .. 0x100E2B3F (the mouse handlers), 0x100E4440 (AddItem), 0x100E4B20 .. 0x100E4BF1
    (constructors, destructor). WindowProc (0x100CBA90) is the body shared with NCInvenItemWnd.
=============================================================================*/
#include "nwindow/NCRecipeBookItemWnd.h"
#include "nwindow/NItemC4.h"
#include "nwindow/NCRecipeBookWnd.h"
#include "nwindow/NCShortCutWnd.h"
#include "nwindow/NCConsole.h"
#include "nwindow/NConsoleWnd.h"
#include "nwindow/NDialogInfo.h"
#include "nwindow/NClasses.gen.h"

enum { DIALOG_RECIPE_DELETE = 0x22 };  // NDialogInfo::ID (name ours): delete the recipe (Param1 its object id), the answer is the console's
enum { MSG_DeleteRecipe = 0x4a };      // systemmsg line the box shows

// 0x100f5e90 - NCItemWnd's constructor and this class's vtable
NCRecipeBookItemWnd::NCRecipeBookItemWnd()
{
}
NW_HOOK_CTOR(0x100f5e90, NCRecipeBookItemWnd)

// 0x100f5f30 (ret 0x18) - the grid of the six arguments (columns, rows, most items, icon size, gaps)
NCRecipeBookItemWnd::NCRecipeBookItemWnd(INT InColumns, INT InVisibleRows, INT InMaxItems, INT InCellSize, INT InGapX, INT InGapY)
    : NCItemWnd(InColumns, InVisibleRows, InMaxItems, InCellSize, InGapX, InGapY)
{
}
NW_HOOK_CTOR_ARGS(0x100f5f30, NCRecipeBookItemWnd, (INT A, INT B, INT C, INT D, INT E, INT F), (A, B, C, D, E, F))

// 0x100f5ee0
NW_HOOK_DTOR(0x100f5ee0, NCRecipeBookItemWnd)

// 0x100cba90 (vslot 25, ret 0xC): the stock body is NCInvenItemWnd's (hooked there): the double click. The standalone has its own
// copy of that body
#if NW_HOOK_MODE
NW_NAKED INT NCRecipeBookItemWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam) NW_STOCK(0x10093ce0)
#else
INT NCRecipeBookItemWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCItemWnd::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == WM_LBUTTONDBLCLK)
        return OnLButtonDblClick(wParam, lParam) != 0;
    return 0;
}
#endif

// 0x100f58a0 - a new entry from the item record (icon from its name, "NWindow.BlackTexture" when missing, written back
// into Info), the tool tip its name line (NTT_ItemName). 0 when the grid is full.
UBOOL NCRecipeBookItemWnd::AddItem(NItemInfo* Info)
{
    if (Items.Num() >= MaxItems)
        return 0;
    AsC4(Info)->Icon[0] = FindTexture(AsC4(Info)->IconName[0]);
    if (!AsC4(Info)->Icon[0])
        AsC4(Info)->Icon[0] = FindTexture(TEXT("NWindow.BlackTexture"));
    NItemEntry* Item = NewItemEntry(Info);
    AsC4(Item)->ToolTip.Fill(Item, 1, 0);
    Items.AddItem(Item);
    UpdateScrollRange();
    return 1;
}
NW_HOOK_METHOD(0x100f58a0, NCRecipeBookItemWnd, AddItem)

// 0x100f3b90 (vslot 68) - a click picks the recipe up
INT NCRecipeBookItemWnd::OnLButtonDown(WPARAM wParam, LPARAM lParam)
{
    Selected = HitTest(LOWORD(lParam), HIWORD(lParam), 0, 0);
    HoverIndex = Selected;
    if (Selected >= 0 && Selected < Items.Num())
    {
        UnselectAll();
        Items(Selected)->Select();
        bDragging = 1;
        return 1;
    }
    return 1;
}
NW_HOOK_METHOD(0x100f3b90, NCRecipeBookItemWnd, OnLButtonDown)

// 0x100f3c00 (vslot 67) - the hovered recipe; with the window focused and a recipe carried its icon is the cursor, and the
// parent book learns whether the cursor is over its trash icon (C3: the cursor is not given back here)
INT NCRecipeBookItemWnd::OnMouseMove(WPARAM wParam, LPARAM lParam)
{
    INT X = LOWORD(lParam);
    INT Y = HIWORD(lParam);
    HoverIndex = HitTest(X, Y, 0, 0);
    if (IsFocused())
    {
        if (bDragging && Selected >= 0 && Selected < Items.Num())
        {
            SetCursor(4, Items(Selected)->GetIcon());
            LockCursor();
        }
        NCRecipeBookWnd* Book = (NCRecipeBookWnd*)Entry->Parent->Wnd;
        NRectF Trash;
        Trash.X = Book->TrashX + Book->ScreenX;
        Trash.Y = Book->TrashY + Book->ScreenY;
        Trash.W = Book->TrashW;
        Trash.H = Book->TrashH;
        Book->bTrashActive = PtInRect(X, Y, Trash) != 0;
    }
    return 1;
}
NW_HOOK_METHOD(0x100f3c00, NCRecipeBookItemWnd, OnMouseMove)

// 0x100f3cf0 (vslot 69) - the recipe dropped on the trash icon of the book asks to be deleted (a yes / no box, ID 0x22,
// Param1 the recipe's object id); new in C4: dropped on a slot of the shortcut bar (the carried recipe only) it is
// registered there (type 5, the slot, the recipe's object id, 1)
INT NCRecipeBookItemWnd::OnLButtonUp(WPARAM wParam, LPARAM lParam)
{
    INT X = LOWORD(lParam);
    INT Y = HIWORD(lParam);
    if (Selected >= 0 && Selected < Items.Num())
    {
        NWndEntry* E = FindWindowAt(X, Y);
        if (E)
        {
            NCWnd* W = E->Wnd;
            if (W && W->IsA(NCRecipeBookWnd::StaticClass()))
            {
                NCRecipeBookWnd* Book = (NCRecipeBookWnd*)W;
                if (Book->HitTrash(X, Y))
                {
                    TCHAR Name[64];
                    appSprintf(Name, TEXT("%s"), Items(Selected)->GetName());
                    NDialogInfo Info;
                    Info.Owner = Entry;
                    Info.ID = DIALOG_RECIPE_DELETE;
                    Info.bMainBox = 1;
                    Info.SetSystemMessage(MSG_DeleteRecipe);
                    Info.SetButtons(Name, NULL);
                    Info.SetParams(AsC4(Items(Selected))->ObjectID, -1);
                    GConsole->ShowDialog(&Info);
                }
            }
            else if (bDragging && W && W->IsA(NCShortCutWnd::StaticClass()))
            {
                INT Slot = ((NCShortCutWnd*)W)->SlotAtPoint(X, Y);
                if (Slot >= 0)
                {
                    L2ParamStack Params(10);
                    Params.PushBack((void*)5);   // a recipe
                    Params.PushBack((void*)(INT_PTR)Slot);
                    Params.PushBack((void*)(INT_PTR)AsC4(Items(Selected))->ObjectID);
                    Params.PushBack((void*)1);
                    GConsole->RequestShortCutReg(Params);
                }
            }
        }
    }
    bDragging = 0;
    return 1;
}
NW_HOOK_METHOD(0x100f3cf0, NCRecipeBookItemWnd, OnLButtonUp)

// 0x100f3f10 (vslot 81) - a double click asks the server for the recipe's ingredients (C3 also closed the grid here: the
// C4 code only clears the selection)
INT NCRecipeBookItemWnd::OnLButtonDblClick(WPARAM wParam, LPARAM lParam)
{
    Selected = HitTest(LOWORD(lParam), HIWORD(lParam), 0, 0);
    if (Selected >= 0 && Selected < Items.Num())
    {
        NItemEntry* Item = Items(Selected);
        if (Item)
            GConsole->RequestRecipeItemMakeInfo(AsC4(Item)->ObjectID);
    }
    UnselectAll();
    Selected = -1;
    bDragging = 0;
    return 1;
}
NW_HOOK_METHOD(0x100f3f10, NCRecipeBookItemWnd, OnLButtonDblClick)

NW_CHECK_VTABLE(NCRecipeBookItemWnd, 0x101c7ac0, 82)
