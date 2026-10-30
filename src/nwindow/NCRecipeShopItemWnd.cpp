/*=============================================================================
    NCRecipeShopItemWnd.cpp - the item grids of the recipe shop's manage window (new in C2).
    Stock (C4): 0x100F1850 .. 0x100F1B00 (AddItem, AddCopy, MoveItem), 0x100F2560 .. 0x100F287B (the mouse handlers, the dialog
    answer), 0x100F2920 / 0x100F2970 / 0x100F29C0 (the constructors, the destructor). (C3: 0x100E0680 .. 0x100E1780.)
    WindowProc (0x10102860) and OnDestroy (0x100EFAC0) are bodies shared with NCShopItemWnd; SetTarget (0x10084BD0) and
    SetMode (0x100EFAD0) with the other grids (the `mov [ecx + 0x168 / 0x16C], eax` setters).
    The C3 offset the price question once carried (`Field(&Info, 0x18)`) is gone: AskPrice below writes NDialogInfo::Flag18
    (+0x18, the item's id at +0x1B20 of the entry) and bLargeAmount (+0x14), both read in the C4 stock 0x100F2700 (the stores
    `[Info + 0x14] = 1`, `[Info + 0x18] = [entry + 0x1B20]`).
    C4 PROOF (wave C, debt2, unicorn): AddItem, AddCopy and MoveItem run against the stock on random grids in
    tools/port/emu/win/t_shopwnd.py (rmove / rcopy / radd: the tool tip fills with their flags, the entries, the arrays, the calls;
    0 differences). The mouse handlers (they open the price box) have the judges' proof only.
=============================================================================*/
#include "nwindow/NCRecipeShopItemWnd.h"
#include "nwindow/NCConsole.h"
#include "nwindow/NCShopWnd.h"
#include "nwindow/NConsoleWnd.h"
#include "nwindow/NCDialogBox.h"
#include "nwindow/NDialogInfo.h"
#include "nwindow/NClasses.gen.h"
#include "nwindow/NItemC4.h"

enum { DIALOG_RECIPE_PRICE = 0x23 };  // NDialogInfo::ID (name ours): a price for the owner (the answer is OnDialogResult)
enum { MSG_PriceToBuy = 0x3c3 };      // systemmsg line the price box shows (C3: 0x142 for a double click; C4 shows 0x3c3 in all three)
enum { MSG_PriceToBuyDrop = 0x3c3 };  // the same box when the entry was dropped on the grid

// the price question (inline in the three stock callers): the entry's name goes through sprintf into a 64-character
// buffer that nothing reads afterwards, the box asks for a number and answers Owner. C3 puts the recipe's id in the
// info (+0x18, Flag18); C4 also lets the price reach the large limit (bLargeAmount, +0x14)
static FORCEINLINE void AskPrice(NCRecipeShopItemWnd* Owner, NItemEntry* Item, INT MsgID)
{
    TCHAR Name[64];
    appSprintf(Name, TEXT("%s"), Item->GetName());
    NDialogInfo Info;
    Info.ID = DIALOG_RECIPE_PRICE;
    Info.Type = 3;
    Info.Owner = Owner->Entry;
    Info.bMainBox = 1;
    Info.bInput = 1;
    Info.bLargeAmount = 1;
    Info.SetSystemMessage(MsgID);
    Info.Flag18 = AsC4(Item)->ID;
    GConsole->ShowDialog(&Info);
}

// 0x100f2920 - the other grid is none
NCRecipeShopItemWnd::NCRecipeShopItemWnd()
{
    Target = NULL;
}
NW_HOOK_CTOR(0x100f2920, NCRecipeShopItemWnd)

// 0x100f29c0 (ret 0x18) - the grid of the six arguments (columns, rows, most items, icon size, gaps)
NCRecipeShopItemWnd::NCRecipeShopItemWnd(INT InColumns, INT InVisibleRows, INT InMaxItems, INT InCellSize, INT InGapX, INT InGapY)
    : NCItemWnd(InColumns, InVisibleRows, InMaxItems, InCellSize, InGapX, InGapY)
{
    Target = NULL;
}
NW_HOOK_CTOR_ARGS(0x100f29c0, NCRecipeShopItemWnd, (INT InColumns, INT InVisibleRows, INT InMaxItems, INT InCellSize, INT InGapX, INT InGapY), (InColumns, InVisibleRows, InMaxItems, InCellSize, InGapX, InGapY))

// 0x100f2970
NW_HOOK_DTOR(0x100f2970, NCRecipeShopItemWnd)

// 0x10102860 (vslot 25, ret 0xC): the stock body is NCShopItemWnd's (hooked there): the double click and the dialog
// box's answer
#if NW_HOOK_MODE
NW_NAKED INT NCRecipeShopItemWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam) NW_STOCK(0x10102860)
#else
INT NCRecipeShopItemWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    // the one stock body of the grids (NCShopItemWnd's: NCItemWnd::WindowProc, then slots 81 and 82 of this object)
    return ((NCShopItemWnd*)this)->NCShopItemWnd::WindowProc(Msg, wParam, lParam);
}
#endif

// 0x100efac0 (vslot 38): the stock body is NCShopItemWnd's (hooked there)
#if NW_HOOK_MODE
NW_NAKED INT NCRecipeShopItemWnd::OnDestroy() NW_STOCK(0x100efac0)
#else
INT NCRecipeShopItemWnd::OnDestroy()
{
    NCItemWnd::OnDestroy();
    return 1;
}
#endif

// 0x10084BD0 (ret 4): the body NCDeliverItemWnd::SetTarget hooks (Target at +0x168)
#if NW_HOOK_MODE
NW_NAKED void NCRecipeShopItemWnd::SetTarget(NCRecipeShopItemWnd* InTarget) NW_STOCK(0x10084BD0)
#else
void NCRecipeShopItemWnd::SetTarget(NCRecipeShopItemWnd* InTarget)
{
    Target = InTarget;
}
#endif

// 0x100EFAD0 (ret 4): the body NCManorShopItemWnd::SetMode hooks (Mode at +0x16C)
#if NW_HOOK_MODE
NW_NAKED void NCRecipeShopItemWnd::SetMode(INT InMode) NW_STOCK(0x100EFAD0)
#else
void NCRecipeShopItemWnd::SetMode(INT InMode)
{
    Mode = InMode;
}
#endif

// 0x100f1850 - a new entry from the item record (icon from its name, "NWindow.BlackTexture" when missing, written back
// into Info), its TradeCount the record's count; the tool tip is the name line (NTT_ItemName), with the count line
// (NTTO_Count) on the Mode 0 grid. 0 when the grid is full.
UBOOL NCRecipeShopItemWnd::AddItem(NItemInfo* Info)
{
    if (MaxItems <= Items.Num())
        return 0;
    AsC4(Info)->Icon[0] = FindTexture(AsC4(Info)->IconName[0]);
    if (!AsC4(Info)->Icon[0])
        AsC4(Info)->Icon[0] = FindTexture(TEXT("NWindow.BlackTexture"));
    NItemEntry* Item = NewItemEntry(Info);
    AsC4(Item)->TradeCount = AsC4(Info)->Count;
    if (Mode == 0)
        AsC4(Item)->ToolTip.Fill(Item, 1, 0x20);
    else if (Mode == 1)
        AsC4(Item)->ToolTip.Fill(Item, 1, 0);
    Items.AddItem(Item);
    UpdateScrollRange();
    return 1;
}
NW_HOOK_METHOD(0x100f1850, NCRecipeShopItemWnd, AddItem)

// 0x100f1980 - a copy of another grid's entry (the move of a recipe into the Mode 0 grid), same tool tip as AddItem
UBOOL NCRecipeShopItemWnd::AddCopy(NItemEntry* Src)
{
    if (MaxItems <= Items.Num())
        return 0;
    NItemEntry* Item = (NItemEntry*)appMalloc(sizeof(NItemEntry), TEXT("new"));
    if (Item)
        Item = new ((void*)Item) NItemEntry;
    Item->Copy(Src);
    if (Mode == 0)
        AsC4(Item)->ToolTip.Fill(Item, 1, 0x20);
    else if (Mode == 1)
        AsC4(Item)->ToolTip.Fill(Item, 1, 0);
    Items.AddItem(Item);
    UpdateScrollRange();
    return 1;
}
NW_HOOK_METHOD(0x100f1980, NCRecipeShopItemWnd, AddCopy)

// 0x100f1a60 - the entry goes to the other grid: a copy into the Mode 0 grid unless it holds the recipe already (the ids
// are compared), out of this grid (not freed, as in the stock code) when the target is the Mode 1 grid
void NCRecipeShopItemWnd::MoveItem(NItemEntry* Item)
{
    if (Target)
    {
        UnselectAll();
        if (Target->Mode == 0)
        {
            for (INT i = 0; i < Target->Items.Num(); i++)
                if (AsC4(Target->Items(i))->ID == AsC4(Item)->ID)
                    goto Done;
            Target->AddCopy(Item);
        }
        else if (Target->Mode == 1)
            Items.Remove(Selected, 1);
Done:
        Selected = -1;
        UpdateScrollRange();
    }
}
NW_HOOK_METHOD(0x100f1a60, NCRecipeShopItemWnd, MoveItem)

// 0x100f2560 (vslot 69) - the carried entry dropped onto the other grid: the Mode 0 grid asks the price first, the Mode 1
// grid takes it back at once (C3: the cursor is left alone)
INT NCRecipeShopItemWnd::OnLButtonUp(WPARAM wParam, LPARAM lParam)
{
    INT X = LOWORD(lParam);
    INT Y = HIWORD(lParam);
    if (Selected >= 0 && Selected < Items.Num() && bDragging)
    {
        if (!IsInWindow(X, Y))
        {
            NWndEntry* E = FindWindowAt(X, Y);
            if (E && E->Wnd && E->Wnd->IsA(NClass::NCRecipeShopItemWnd()))
            {
                Target = (NCRecipeShopItemWnd*)E->Wnd;
                NItemEntry* Item = Items(Selected);
                if (Target->Mode == 0)
                    AskPrice(this, Item, MSG_PriceToBuyDrop);
                else if (Target->Mode == 1)
                    MoveItem(Item);
            }
        }
    }
    bDragging = 0;
    return 1;
}
NW_HOOK_METHOD(0x100f2560, NCRecipeShopItemWnd, OnLButtonUp)

// 0x100f2700 (vslot 81) - a double click moves the entry the same way (the target is the other grid already)
INT NCRecipeShopItemWnd::OnLButtonDblClick(WPARAM wParam, LPARAM lParam)
{
    bNoDrag = 1;
    Selected = HitTest(LOWORD(lParam), HIWORD(lParam), 0, 0);
    if (Selected >= 0 && Selected < Items.Num())
    {
        NItemEntry* Item = Items(Selected);
        if (Target->Mode == 0)
            AskPrice(this, Item, MSG_PriceToBuy);
        else if (Target->Mode == 1)
            MoveItem(Item);
    }
    bDragging = 0;
    return 1;
}
NW_HOOK_METHOD(0x100f2700, NCRecipeShopItemWnd, OnLButtonDblClick)

// 0x100f2850 (vslot 82) - the price typed (wParam) goes into the selected entry, which then moves to the other grid; the
// stock code reads Items(Selected) without a bounds check
INT NCRecipeShopItemWnd::OnDialogResult(WPARAM wParam, LPARAM lParam)
{
    NItemEntry* Item = Items(Selected);
    if (Item)
    {
        AsC4(Item)->UnknownC20 = (INT)wParam;
        MoveItem(Item);
    }
    return 1;
}
NW_HOOK_METHOD(0x100f2850, NCRecipeShopItemWnd, OnDialogResult)

NW_CHECK_VTABLE(NCRecipeShopItemWnd, 0x101c7048, 83)
