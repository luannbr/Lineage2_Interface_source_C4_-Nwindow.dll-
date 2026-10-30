/*=============================================================================
    NCManorShopItemWnd.cpp - the item grids of the manor's shop (a copy of NCShopItemWnd).
    Stock (C3): 0x100a3560 .. 0x100a3980 (AddItem, AddCopy, MoveItem, between the window's functions), 0x100a3f40 ..
    0x100a45ff (OnLButtonUp, OnLButtonDblClick, OnDialogResult, the constructors, the destructor). WindowProc
    (0x100a2850), OnDestroy (0x100d7090), SetTarget (0x10088870), SetMode (0x100de910) and SetSide (0x100a28d0) are
    copies of bodies other classes keep the addresses of (NCShopWnd.cpp hooks them).
=============================================================================*/
#include "nwindow/NCManorShopItemWnd.h"
#include "nwindow/NItemC4.h"
#include "nwindow/NCShopWnd.h"
#include "nwindow/NCManorShopWnd.h"
#include "nwindow/NCConsole.h"
#include "nwindow/NCDialogBox.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NClasses.gen.h"
#include "nwindow/NConsoleWnd.h"
#include "nwindow/NDialogInfo.h"

enum { DIALOG_SHOP_AMOUNT = 2 };     // NDialogInfo::ID (name ours): an amount for the owner (the shop's)
enum { MSG_HowManyToMove = 72 };     // systemmsg "How many $s1(s) do you want to move?"

// how many of a stack to move; the answer goes to Owner's OnDialogResult. Inline in each stock caller; the name buffer is
// the stock frame's 64 characters. The item's id goes in the info (Flag18, +0x18; the entry's ID is +0x1B20 in C4) and the
// box is given the most it may take (Unknown74: the entry's Count). The same as the shop's.
static FORCEINLINE void AskAmount(NCWnd* Owner, NItemEntry* Item, INT Max)
{
    TCHAR Name[64];
    appSprintf(Name, TEXT("%s"), Item->GetName());
    NDialogInfo Info;
    Info.ID = DIALOG_SHOP_AMOUNT;
    Info.Type = 3;
    Info.Owner = Owner->Entry;
    Info.bMainBox = 1;
    Info.bInput = 1;
    Info.SetSystemMessage(MSG_HowManyToMove);
    Info.SetButtons(Name, NULL);
    Info.Flag18 = AsC4(Item)->ID;
    Info.Unknown74 = Max;
    GConsole->ShowDialog(&Info);
}

// The tool tip of an entry of this grid, by its side and the grid's mode (inline in each stock caller): the shop's
// ladder with one constant changed (5 for the NPC's goods when buying, where the shop passes 7). Nothing for a side or
// mode out of range.
static FORCEINLINE void ManorFillToolTip(NCManorShopItemWnd* Wnd, NItemEntry* Item)
{
    INT Extra;
    if (Wnd->Side == SHOPSIDE_Shop)
    {
        if (Wnd->Mode == SHOPMODE_Buy)
            Extra = 5;
        else if (Wnd->Mode == SHOPMODE_Sell)
            Extra = 8;
        else
            return;
    }
    else if (Wnd->Side == SHOPSIDE_Player)
    {
        if (Wnd->Mode == SHOPMODE_Buy)
            Extra = 4;
        else if (Wnd->Mode == SHOPMODE_Sell)
            Extra = 8;
        else
            return;
    }
    else
        return;
    AsC4(Item)->ToolTip.Fill(Item, 2, Extra);
}

// 0x100b0d90
NCManorShopItemWnd::NCManorShopItemWnd()
{
    Target = NULL;
}
NW_HOOK_CTOR(0x100b0d90, NCManorShopItemWnd)

// 0x100b0e30 (ret 0x18) - the grid of the six arguments (columns, rows, most items, icon size, gaps)
NCManorShopItemWnd::NCManorShopItemWnd(INT InColumns, INT InVisibleRows, INT InMaxItems, INT InCellSize, INT InGapX,
                                       INT InGapY)
    : NCItemWnd(InColumns, InVisibleRows, InMaxItems, InCellSize, InGapX, InGapY)
{
    Target = NULL;
}
NW_HOOK_CTOR_ARGS(0x100b0e30, NCManorShopItemWnd, (INT A, INT B, INT C, INT D, INT E, INT F), (A, B, C, D, E, F))

// 0x100b0de0
NW_HOOK_DTOR(0x100b0de0, NCManorShopItemWnd)

// 0x10102860 (vslot 25) - the stock body is shared with NCShopItemWnd::WindowProc, which keeps the address (hooked there): NCItemWnd's, then
// the double click (slot 81) and the dialog box's answer (slot 82). The slot jumps to the stock address, which our hook owns
// (the standalone has its own copy of that body)
#if NW_HOOK_MODE
NW_NAKED INT NCManorShopItemWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam) NW_STOCK(0x10102860)
#else
INT NCManorShopItemWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCItemWnd::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == WM_LBUTTONDBLCLK)
        return OnLButtonDblClick(wParam, lParam) != 0;
    if (Msg == NWM_DIALOG_RESULT)
        return OnDialogResult(wParam, lParam) != 0;
    return 0;
}
#endif

// stock body shared with NCShopItemWnd::OnDestroy (0x100d7090, hooked there)
INT NCManorShopItemWnd::OnDestroy()
{
    NCItemWnd::OnDestroy();
    return 1;
}

// 0x10084bd0 (ret 4) - `mov [ecx + 0x168], eax`, the body NCDeliverItemWnd::SetTarget hooks; the standalone has its own copy
#if NW_HOOK_MODE
NW_NAKED void NCManorShopItemWnd::SetTarget(NCManorShopItemWnd* InTarget) NW_STOCK(0x10084bd0)
#else
void NCManorShopItemWnd::SetTarget(NCManorShopItemWnd* InTarget)
{
    Target = InTarget;
}
#endif

// 0x100efad0 (ret 4) - stock body shared with the other grids' setter of +0x14C
void NCManorShopItemWnd::SetMode(INT InMode)
{
    Mode = InMode;
}
NW_HOOK_METHOD(0x100efad0, NCManorShopItemWnd, SetMode)

// 0x101028e0 (ret 4) - `mov [ecx + 0x170], eax`, the body NCShopItemWnd::SetSide has; the standalone has its own copy
#if NW_HOOK_MODE
NW_NAKED void NCManorShopItemWnd::SetSide(INT InSide) NW_STOCK(0x101028e0)
#else
void NCManorShopItemWnd::SetSide(INT InSide)
{
    Side = InSide;
}
#endif

// 0x100b0750 (vslot 69) - the carried item dropped onto another manor grid goes there (that grid becomes the target):
// how many first for a stack (from the NPC's list to the purchases whatever its amount), else one
INT NCManorShopItemWnd::OnLButtonUp(WPARAM wParam, LPARAM lParam)
{
    INT X = LOWORD(lParam);
    INT Y = HIWORD(lParam);
    if (Selected >= 0 && Selected < Items.Num() && bDragging)
    {
        if (!IsInWindow(X, Y))
        {
            NWndEntry* E = FindWindowAt(X, Y);
            if (E && E->Wnd && E->Wnd->IsA(NCManorShopItemWnd::StaticClass()))
            {
                Target = (NCManorShopItemWnd*)E->Wnd;
                NItemEntry* Item = Items(Selected);
                if (Mode == SHOPMODE_Buy && Target->Side == SHOPSIDE_Player)
                {
                    if (Item->IsStackable())
                    {
                        AskAmount(this, Item, AsC4(Item)->Count);
                        bDragging = 0;
                        return 1;
                    }
                }
                else if (Item->IsStackable() && AsC4(Item)->TradeCount > 1)
                {
                    AskAmount(this, Item, AsC4(Item)->Count);
                    bDragging = 0;
                    return 1;
                }
                MoveItem(Item, 1);
            }
        }
    }
    bDragging = 0;
    return 1;
}
NW_HOOK_METHOD(0x100b0750, NCManorShopItemWnd, OnLButtonUp)

// 0x100b0a20 (vslot 81) - the item goes to the target: how many first for a stack (any stack when buying), else one
INT NCManorShopItemWnd::OnLButtonDblClick(WPARAM wParam, LPARAM lParam)
{
    bNoDrag = 1;
    Selected = HitTest(LOWORD(lParam), HIWORD(lParam), 0, 0);
    if (Selected >= 0 && Selected < Items.Num())
    {
        NItemEntry* Item = Items(Selected);
        if (Mode == SHOPMODE_Buy)
        {
            if (Item->IsStackable())
            {
                AskAmount(this, Item, AsC4(Item)->Count);
                bDragging = 0;
                return 1;
            }
        }
        else if (Item->IsStackable() && AsC4(Item)->TradeCount > 1)
        {
            AskAmount(this, Item, AsC4(Item)->Count);
            bDragging = 0;
            return 1;
        }
        MoveItem(Item, 1);
    }
    bDragging = 0;
    return 1;
}
NW_HOOK_METHOD(0x100b0a20, NCManorShopItemWnd, OnLButtonDblClick)

// 0x100b0c80 (vslot 82) - the amount typed (wParam; at most the entry's, in both modes) goes to the target
INT NCManorShopItemWnd::OnDialogResult(WPARAM wParam, LPARAM lParam)
{
    if (Selected < 0 || Selected >= Items.Num())
        return 1;
    NItemEntry* Item = Items(Selected);
    if (!Item)
        return 1;
    INT Count = (INT)wParam;
    if (Count)
    {
        if ((Mode == SHOPMODE_Sell || Mode == SHOPMODE_Buy) && AsC4(Item)->TradeCount <= (SQWORD)Count)
            Count = (INT)AsC4(Item)->TradeCount;
        MoveItem(Item, Count);
    }
    return 1;
}
NW_HOOK_METHOD(0x100b0c80, NCManorShopItemWnd, OnDialogResult)

// 0x100afd70 - a new entry, its icon from its name ("NWindow.BlackTexture" when missing, written back into Info), its
// amount the item's count
UBOOL NCManorShopItemWnd::AddItem(NItemInfo* Info)
{
    if (Items.Num() >= MaxItems)
        return 0;
    AsC4(Info)->Icon[0] = FindTexture(AsC4(Info)->IconName[0]);
    if (!AsC4(Info)->Icon[0])
        AsC4(Info)->Icon[0] = FindTexture(TEXT("NWindow.BlackTexture"));
    NItemEntry* Item = NewItemEntry(Info);
    AsC4(Item)->TradeCount = AsC4(Info)->Count;
    ManorFillToolTip(this, Item);
    Items.AddItem(Item);
    UpdateScrollRange();
    return 1;
}
NW_HOOK_METHOD(0x100afd70, NCManorShopItemWnd, AddItem)

// 0x100afec0 - a copy of another grid's entry holding Count
UBOOL NCManorShopItemWnd::AddCopy(NItemEntry* Src, INT Count)
{
    if (Items.Num() >= MaxItems)
        return 0;
    NItemEntry* Item = (NItemEntry*)appMalloc(sizeof(NItemEntry), TEXT("new"));
    if (Item)
        Item = new ((void*)Item) NItemEntry;
    Item->Copy(Src);
    AsC4(Item)->TradeCount = Count;
    ManorFillToolTip(this, Item);
    Items.AddItem(Item);
    UpdateScrollRange();
    return 1;
}
NW_HOOK_METHOD(0x100afec0, NCManorShopItemWnd, AddCopy)

// 0x100afff0 - the whole item goes to the target (the amount joins an entry of the same item there when it is a stack)
// and this entry loses it (an emptied one leaves the grid, not freed, as in the stock code); then the total of the shop
// window shown, else the manor shop's
void NCManorShopItemWnd::MoveItem(NItemEntry* Item, INT Count)
{
    if (!Target)
        return;
    UnselectAll();
    UBOOL bJoined = 0;
    if (Item->IsStackable())
    {
        for (INT i = 0; i < Target->Items.Num(); i++)
        {
            if (AsC4(Target->Items(i))->ID == AsC4(Item)->ID)
            {
                AsC4(Target->Items(i))->TradeCount += Count;
                AsC4(Target->Items(i))->ToolTip.Fill(Target->Items(i), 2, 0);
                bJoined = 1;
                break;
            }
        }
    }
    if (!bJoined)
        Target->AddCopy(Item, Count);
    if (Item->IsStackable() && (SQWORD)Count < AsC4(Item)->TradeCount)
    {
        AsC4(Item)->TradeCount -= Count;
        AsC4(Item)->ToolTip.Fill(Item, 2, 0);
    }
    else
        Items.Remove(Selected, 1);
    NCShopWnd* Shop = GDesktop->ShopWnd;
    if (Shop && Shop->IsEnabled() && Shop->IsShowWindow())
        Shop->UpdateTotalPrice();
    else
        ((NCManorShopWnd*)GDesktop->NewWndE4)->UpdateTotalPrice();   // 0x100afc60 (desktop +0x13C)
    Selected = -1;
    UpdateScrollRange();
}
NW_HOOK_METHOD(0x100afff0, NCManorShopItemWnd, MoveItem)

NW_CHECK_VTABLE(NCManorShopItemWnd, 0x101bb400, 83)
