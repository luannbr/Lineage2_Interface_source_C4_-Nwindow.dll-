/*=============================================================================
    NCPrivateBuyWnd.cpp - the private store (buy) window and its grids. Stock (C3): 0x100d3c30 ..
    0x100d6fa0 (the grids, the window and its title bar), the bodies it shares with the sell window
    (NCPrivateShopWnd.cpp) are hooked there or here: ShowWindow 0x100d71d0, HideWindow 0x100d7250,
    UpdateLayout 0x100d79c0 (hooked there), CheckTotal 0x100d7ea0.
=============================================================================*/
#include "nwindow/NCPrivateBuyWnd.h"
#include "nwindow/NItemC4.h"
#include "nwindow/NCButton.h"
#include "nwindow/NCConsole.h"
#include "nwindow/NCFrameCtrl.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NConsoleWnd.h"
#include "nwindow/NDialogInfo.h"
#include "nwindow/NItemToolTip.h"

// NWM_COMMAND ids (names ours; the sell window's)
enum
{
    NID_BUYSTORE_ABORT = 0xF0,
    NID_BUYSTORE_FRAME = 0xF3,
    NID_BUYSTORE_START = 0xF4,
    NID_BUYSTORE_UP = 0xF6,
    NID_BUYSTORE_DOWN = 0xF7,
    NID_BUYSTORE_CLOSE = 0xF8,
    NID_BUYSTORE_MESSAGE = 0xF9,
};

// NDialogInfo::ID of the questions (names ours)
enum
{
    DIALOG_BUYSTORE_PRICE = 0x2A,           // C1: 0x26 (the five ids moved up by four)
    DIALOG_BUYSTORE_AMOUNT = 0x2B,
    DIALOG_BUYSTORE_MESSAGE = 0x2C,
    DIALOG_BUYSTORE_PRICE_CAUTION = 0x2D,
    DIALOG_BUYSTORE_SELL_CAUTION = 0x2E,
};

// systemmsg
enum
{
    MSG_MoveHowMany = 72,       // "How many $s1(s) do you want to move?"
    MSG_StoreMessage = 334,     // "Please enter store message."
    MSG_PriceCaution = 569,     // "Caution--the item price greatly differs from the shop's
                                //   standard price. Do you wish to continue?"
    MSG_BuyHowMany = 570,       // "How many $s1 (s) do you wish to purchase?"
    MSG_BuyHowMany2 = 571,      // "How many $s1 (s) do you want to purchase?"
    MSG_SetBuyPrice = 585,      // "Please decide on the price."
    MSG_TooManyWanted = 1036,   // the store wants fewer than typed
};

// sysstring
enum
{
    STR_Adena = 134,
    STR_ItemsOnSale = 137,
    STR_Inventory = 138,
    STR_OK = 140,
    STR_Price = 142,
    STR_PriceSell = 143,
    STR_Message = 384,
    STR_Abort = 385,
    STR_Start = 428,
    STR_WishList = 502,
    STR_ForSale = 503,
};

// a child window goes (repeated inline in the stock OnDestroys)
template <class T> static FORCEINLINE void DestroyStoreChild(NCWnd* Parent, T*& Child)
{
    if (Child)
    {
        Parent->RemoveChildWindow(Child->Entry);
        delete Child;
        Child = NULL;
    }
}

// a price 5 times away from the item's reference price, either way (the reference 0: none). C4 entry layout through the
// view (NItemC4.h): UnknownC20 +0x1B2C is the price, Unknown1CA8 +0x1BB4 the reference
static FORCEINLINE UBOOL IsUnusualPrice(NItemEntry* Item)
{
    return AsC4(Item)->Unknown1CA8 > 0
        && (AsC4(Item)->UnknownC20 <= AsC4(Item)->Unknown1CA8 / 5 || AsC4(Item)->UnknownC20 >= AsC4(Item)->Unknown1CA8 * 5);
}

// the desktop's box asks how many of the item (NWM_BUYSTORE_AMOUNT answers the grid); the most it takes is Max. The
// item's id goes with it (Flag18, the dialog's +0x18; the entry's ID is +0x1B20 in C4)
static FORCEINLINE void AskAmount(NCWnd* Grid, NItemEntry* Item, INT MsgID, INT Max)
{
    TCHAR Name[64];
    appSprintf(Name, TEXT("%s"), Item->GetName());
    NDialogInfo Info;
    Info.ID = DIALOG_BUYSTORE_AMOUNT;
    Info.Type = 3;
    Info.Owner = Grid->Entry;
    Info.bMainBox = 1;
    Info.bInput = 1;
    Info.SetSystemMessage(MsgID);
    Info.SetButtons(Name, NULL);
    Info.Flag18 = AsC4(Item)->ID;
    Info.Unknown74 = Max;
    GConsole->ShowDialog(&Info);
}

// ... the price offered, up to 100,000,000 (NWM_BUYSTORE_PRICE answers)
static FORCEINLINE void AskPrice(NCWnd* Grid, NItemEntry* Item)
{
    NDialogInfo Info;
    Info.ID = DIALOG_BUYSTORE_PRICE;
    Info.Type = 3;
    Info.Owner = Grid->Entry;
    Info.bMainBox = 1;
    Info.bInput = 1;
    Info.SetSystemMessage(MSG_SetBuyPrice);
    Info.Flag18 = AsC4(Item)->ID;
    Info.bLargeAmount = 1;
    GConsole->ShowDialog(&Info);
}

// the tool tip of an entry by the grid's side: the player's side (1) and the list's (0) read different columns (the
// flags 0x802 / 8 are the buy store's). The tool tip sits at +0x1BC0 of the C4 entry
static FORCEINLINE void PrivateBuyToolTip(NItemEntry* Item, INT Side)
{
    if (Side == 1)
        AsC4(Item)->ToolTip.Fill(Item, 2, 0);
    else if (Side == 0)
        AsC4(Item)->ToolTip.Fill(Item, 0x802, 8);
}

// the entry of the partner's list a stack joins: the same item id (inline in the stock MoveItem)
static FORCEINLINE NItemEntry* FindBuyStackIn(NCPrivateBuyItemWnd* Grid, NItemEntry* Item)
{
    for (INT i = 0; i < Grid->Items.Num(); i++)
        if (AsC4(Grid->Items(i))->ID == AsC4(Item)->ID)
            return Grid->Items(i);
    return NULL;
}

/*-----------------------------------------------------------------------------
    NCPrivateBuyFrameCtrl.
-----------------------------------------------------------------------------*/

// 0x100e3ab0 (vslot 76) - closing from the bar: the window's own close (a store of one's own is told to quit) when the
// bar sits in the desktop's buy window, then the frame's
INT NCPrivateBuyFrameCtrl::OnClose()
{
    NWndEntry* Parent = Entry->Parent;
    if (Parent)
    {
        NCWnd* W = Parent->Wnd;
        if (W == (NCWnd*)GDesktop->PrivateBuyWnd)
        {
            if (((NCPrivateBuyWnd*)W)->Mode == 1)
                GConsole->RequestPrivateStoreQuitBuy();
            ((NCPrivateBuyWnd*)Entry->Parent->Wnd)->Reset();
        }
    }
    return NCFrameCtrl::OnClose();
}
NW_HOOK_METHOD(0x100e3ab0, NCPrivateBuyFrameCtrl, OnClose)

// 0x100e6030 - NCFrameCtrl's constructor and this class's vtable
NCPrivateBuyFrameCtrl::NCPrivateBuyFrameCtrl()
{
}
NW_HOOK_CTOR(0x100e6030, NCPrivateBuyFrameCtrl)

// a title bar made as the stock code does: the allocation, then the constructor (0x100e6030, hooked above); the standalone runs
// the C++ constructor
static NCPrivateBuyFrameCtrl* NewBuyFrameCtrl()
{
#if NW_HOOK_MODE
    void* Mem = NCPrivateBuyFrameCtrl::operator new(sizeof(NCPrivateBuyFrameCtrl));
    if (!Mem)
        return NULL;
    ::nw::StockCallAt<void>(0x100E6030, Mem);
    return (NCPrivateBuyFrameCtrl*)Mem;
#else
    return new NCPrivateBuyFrameCtrl;
#endif
}

/*-----------------------------------------------------------------------------
    NCPrivateBuyItemWnd.
-----------------------------------------------------------------------------*/

// C2 CANDIDATE: the C2 function is not the same code as the C1 one; kept only while it matches the stock one (verify_data)
// 0x100e60d0 - NCItemWnd's 5 x 5; the cover texture ("NWindow.Chat"), no partner
NCPrivateBuyItemWnd::NCPrivateBuyItemWnd()
{
    CoverTex = FindTexture(TEXT("NWindow.Chat"));
    Partner = NULL;
    Unknown128 = 0;
}
NW_HOOK_CTOR(0x100e60d0, NCPrivateBuyItemWnd)

// C2 CANDIDATE: the C2 function is not the same code as the C1 one; kept only while it matches the stock one (verify_data)
// 0x100e61c0
NCPrivateBuyItemWnd::NCPrivateBuyItemWnd(INT InColumns, INT InVisibleRows, INT InMaxItems, INT InCellSize, INT InGapX, INT InGapY)
    : NCItemWnd(InColumns, InVisibleRows, InMaxItems, InCellSize, InGapX, InGapY)
{
    CoverTex = FindTexture(TEXT("NWindow.Chat"));
    Partner = NULL;
    Unknown128 = 0;
}
NW_HOOK_CTOR_ARGS(0x100e61c0, NCPrivateBuyItemWnd, (INT A, INT B, INT C, INT D, INT E, INT F), (A, B, C, D, E, F))

// 0x100e6170
NW_HOOK_DTOR(0x100e6170, NCPrivateBuyItemWnd)

// 0x100e3160 (vslot 25) - the double click and the dialog box's answers (the price, the amount, the unusual price
// confirmed or not)
INT NCPrivateBuyItemWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCItemWnd::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == WM_LBUTTONDBLCLK)
        return OnLButtonDblClick(wParam, lParam) != 0;
    if (Msg == NWM_BUYSTORE_PRICE_CANCEL)
        return OnPriceCancel(wParam, lParam) != 0;
    if (Msg == NWM_BUYSTORE_PRICE)
        return OnPrice(wParam, lParam) != 0;
    if (Msg == NWM_BUYSTORE_AMOUNT)
        return OnAmount(wParam, lParam) != 0;
    if (Msg == NWM_BUYSTORE_PRICE_OK)
        return OnPriceOK(wParam, lParam) != 0;
    return 0;
}
NW_HOOK_METHOD(0x100e3160, NCPrivateBuyItemWnd, WindowProc)

// stock body shared with NCDeliverItemWnd's (0x100d7090)
INT NCPrivateBuyItemWnd::OnDestroy()
{
    NCItemWnd::OnDestroy();
    return 1;
}

// 0x100e34c0 (vslot 62) - NCItemWnd's icons (the cells 5 and 3 pixels apart here); at someone's store the items the
// player lacks (count 0) get a cover, its texture looked up again each time
INT NCPrivateBuyItemWnd::OnPaint(NCGDevice* Device)
{
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    Device->BeginPaint(ScreenX, ScreenY);
    INT X = 1;
    INT Y = 1;
    for (INT i = TopRow * Columns; i < Items.Num(); i++)
    {
        NItemEntry* Item = Items(i);
        if (!Item)
            continue;
        if (i % Columns == 0)
        {
            X = 1;
            if (i != TopRow * Columns)
                Y += CellSize + 3;
            if (VisibleRows * (CellSize + 1) <= Y)
                break;
        }
        else
            X += CellSize + 5;
        if (Mode != 0)
        {
            if (Item->IsSelected())
                Device->DrawTexture(X - 1, Y - 1, 34, 34, 0, 0, 34, 34, SelectTex, 0xFF, 1);
            if (Item->GetIcon())
                Device->DrawTexture(X, Y, CellSize, CellSize, 0, 0, CellSize, CellSize, Item->GetIcon(), 0xFF, 1);
        }
        else if (AsC4(Item)->Count == 0)
        {
            CoverTex = FindTexture(TEXT("NWindow.ChatBack"));
            if (Item->IsSelected())
                Device->DrawTexture(X - 1, Y - 1, 34, 34, 0, 0, 34, 34, SelectTex, 0xFF, 1);
            if (Item->GetIcon())
                Device->DrawTexture(X, Y, CellSize, CellSize, 0, 0, CellSize, CellSize, Item->GetIcon(), 0xFF, 1);
            Device->DrawTexture(X, Y, CellSize, CellSize, 0, 0, CellSize, CellSize, CoverTex, 0xFF, 1);
        }
        else
        {
            if (Item->IsSelected())
                Device->DrawTexture(X - 1, Y - 1, 34, 34, 0, 0, 34, 34, SelectTex, 0xFF, 1);
            if (Item->GetIcon())
                Device->DrawTexture(X, Y, CellSize, CellSize, 0, 0, CellSize, CellSize, Item->GetIcon(), 0xFF, 1);
        }
    }
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
}
NW_HOOK_METHOD(0x100e34c0, NCPrivateBuyItemWnd, OnPaint)

// 0x100e5910 (vslot 69) - the drop of the carried item: at someone's store an item the player lacks stays; going on the
// wish list asks the price, a stack coming off it how many (the arrow is no longer put back here)
INT NCPrivateBuyItemWnd::OnLButtonUp(WPARAM wParam, LPARAM lParam)
{
    INT X = LOWORD(lParam);
    INT Y = HIWORD(lParam);
    if (Selected >= 0 && Selected < Items.Num() && bDragging)
    {
        if (!IsInWindow(X, Y))
        {
            NWndEntry* E = FindWindowAt(X, Y);
            if (E && E->Wnd && E->Wnd->IsA(NCPrivateBuyItemWnd::StaticClass()))
            {
                Partner = (NCPrivateBuyItemWnd*)E->Wnd;
                NItemEntry* Item = Items(Selected);
                if (Mode == 0)
                {
                    if (AsC4(Item)->Count > 0)
                    {
                        if (Item->IsStackable())
                            AskAmount(this, Item, MSG_MoveHowMany, (INT)AsC4(Item)->TradeCount);
                        else
                            MoveItem(Item, 1);
                    }
                }
                else if (Partner->Side == 0)
                {
                    TCHAR Name[64];
                    appSprintf(Name, TEXT("%s"), Item->GetName());
                    AskPrice(this, Item);
                }
                else if (Item->IsStackable() && AsC4(Item)->Count > 1)
                    AskAmount(this, Item, MSG_BuyHowMany2, (INT)AsC4(Item)->TradeCount);
                else
                    MoveItem(Item, 1);
            }
        }
    }
    bDragging = 0;
    return 1;
}
NW_HOOK_METHOD(0x100e5910, NCPrivateBuyItemWnd, OnLButtonUp)

// 0x100e5c90 (vslot 81) - OnLButtonUp's moves, onto Partner (a stack coming off the list asks how many whatever its
// count)
INT NCPrivateBuyItemWnd::OnLButtonDblClick(WPARAM wParam, LPARAM lParam)
{
    bNoDrag = 1;
    Selected = HitTest(LOWORD(lParam), HIWORD(lParam), 0, 0);
    if (Selected >= 0 && Selected < Items.Num())
    {
        NItemEntry* Item = Items(Selected);
        if (Mode == 0)
        {
            if (AsC4(Item)->Count > 0)
            {
                if (Item->IsStackable())
                    AskAmount(this, Item, MSG_MoveHowMany, (INT)AsC4(Item)->TradeCount);
                else
                    MoveItem(Item, 1);
            }
        }
        else if (Partner->Side == 0)
        {
            TCHAR Name[64];
            appSprintf(Name, TEXT("%s"), Item->GetName());
            AskPrice(this, Item);
        }
        else if (Item->IsStackable())
            AskAmount(this, Item, MSG_BuyHowMany2, (INT)AsC4(Item)->TradeCount);
        else
            MoveItem(Item, 1);
    }
    bDragging = 0;
    return 1;
}
NW_HOOK_METHOD(0x100e5c90, NCPrivateBuyItemWnd, OnLButtonDblClick)

// 0x100e38c0 (vslot 83) - the price offered for the selected item (setting one's own store up): it takes it; a price 5
// times away from the reference asks again, then how many are wanted (always; 0: the box stays)
INT NCPrivateBuyItemWnd::OnPrice(WPARAM wParam, LPARAM lParam)
{
    NItemEntry* Item = Items(Selected);
    INT Price = (INT)wParam;
    if (!Item)
        return 1;
    if (!Price)
        return 1;
    if (Mode == 1 && Price > 0)
    {
        INT Reference = AsC4(Item)->Unknown1CA8;
        AsC4(Item)->UnknownC20 = Price;
        if (Reference < 1 || (Reference / 5 < Price && Price < Reference * 5))
        {
            AskAmount(this, Item, MSG_BuyHowMany, AsC4(Item)->Count);
            return 0;
        }
        NDialogInfo Info;
        Info.Owner = Entry;
        Info.ID = DIALOG_BUYSTORE_PRICE_CAUTION;
        Info.bMainBox = 1;
        Info.bInput = 0;
        Info.SetSystemMessage(MSG_PriceCaution);
        GConsole->ShowDialog(&Info);
        return 0;
    }
    return 1;
}
NW_HOOK_METHOD(0x100e38c0, NCPrivateBuyItemWnd, OnPrice)

// 0x100e5fb0 (vslot 84) - the amount typed: that many of the selected item move (setting up; at someone's store no more
// than it wants, a system message says so); the move's own answer is the result
INT NCPrivateBuyItemWnd::OnAmount(WPARAM wParam, LPARAM lParam)
{
    NItemEntry* Item = Items(Selected);
    INT Amount = (INT)wParam;
    if (Item && Amount)
    {
        if (Mode == 1 && Amount > 0)
        {
            if (Amount > 100000000)
                Amount = 100000000;
            Unknown128 = 0;
            return MoveItem(Item, Amount);
        }
        if (Mode == 0 && Amount > 0)
        {
            if (Amount <= AsC4(Item)->UnknownC34)
                return MoveItem(Item, Amount);
            GConsole->ShowSystemMessage(MSG_TooManyWanted, 0xFFDCDCDC);
        }
    }
    return 1;
}
NW_HOOK_METHOD(0x100e5fb0, NCPrivateBuyItemWnd, OnAmount)

// 0x100e37c0 (vslot 85) - the unusual price confirmed: how many are wanted
INT NCPrivateBuyItemWnd::OnPriceOK(WPARAM wParam, LPARAM lParam)
{
    AskAmount(this, Items(Selected), MSG_BuyHowMany, AsC4(Items(Selected))->Count);
    return 0;
}
NW_HOOK_METHOD(0x100e37c0, NCPrivateBuyItemWnd, OnPriceOK)

// the partner (the stock body is 0x100de910, one with the other grids' setters of the field at +0x14C)
void NCPrivateBuyItemWnd::SetPartner(NCPrivateBuyItemWnd* InPartner)
{
    Partner = InPartner;
}

// the stock body is 0x100a28d0, one with the other grids' setters of the field at +0x150
void NCPrivateBuyItemWnd::SetMode(INT InMode)
{
    Mode = InMode;
}

// 0x100e3240
void NCPrivateBuyItemWnd::SetSide(INT InSide)
{
    Side = InSide;
}
NW_HOOK_METHOD(0x100e3240, NCPrivateBuyItemWnd, SetSide)

// 0x100e4bf0 - the icon comes from its name ("NWindow.BlackTexture" when missing), written back into Info (+0x1900 in
// C4); the whole count is there (64 bits)
UBOOL NCPrivateBuyItemWnd::AddItem(NItemInfo* Info)
{
    if (Items.Num() >= MaxItems)
        return 0;
    AsC4(Info)->Icon[0] = FindTexture(AsC4(Info)->IconName[0]);
    if (!AsC4(Info)->Icon[0])
        AsC4(Info)->Icon[0] = FindTexture(TEXT("NWindow.BlackTexture"));
    NItemEntry* Item = NewItemEntry(Info);
    AsC4(Item)->TradeCount = AsC4(Info)->Count;
    PrivateBuyToolTip(Item, Side);
    Items.AddItem(Item);
    UpdateScrollRange();
    return 1;
}
NW_HOOK_METHOD(0x100e4bf0, NCPrivateBuyItemWnd, AddItem)

// 0x100e4d20 - a copy of Src with Count there
UBOOL NCPrivateBuyItemWnd::AddEntry(NItemEntry* Src, INT Count)
{
    if (Items.Num() >= MaxItems)
        return 0;
    NItemEntry* Item = (NItemEntry*)appMalloc(sizeof(NItemEntry), TEXT("new"));
    if (Item)
        Item = new ((void*)Item) NItemEntry;
    Item->Copy(Src);
    AsC4(Item)->TradeCount = Count;
    PrivateBuyToolTip(Item, Side);
    Items.AddItem(Item);
    UpdateScrollRange();
    return 1;
}
NW_HOOK_METHOD(0x100e4d20, NCPrivateBuyItemWnd, AddEntry)

// 0x100e4e20 - Count of the selected Item to Partner. Setting one's own store up, the catalog keeps its item (it first
// checks that the wish list's total stays within two billion): a stack joins the same item on the wish list (taking this
// price), single items replace the entries of the same item and enchant there (re-reading the entry that slides into the
// removed one's place) and come as one entry each, 100 at most; off the list the entry just shrinks or leaves. At
// someone's store a stack joins the same item on the other side or comes as a copy, the amount wanted moving along, and
// shrinks or leaves (not freed).
INT NCPrivateBuyItemWnd::MoveItem(NItemEntry* Item, INT Count)
{
    if (!Partner)
        return 1;
    UnselectAll();
    if (Mode == 1)
    {
        if (Partner->Side == 0)
        {
            if (!GDesktop->PrivateBuyWnd->CheckTotal(AsC4(Item)->ObjectID, Count, Item->GetUnknownC20()))
                return 0;
            if (!Item->IsStackable())
            {
                for (INT i = 0; i < Partner->Items.Num(); i++)
                {
                    NItemEntry* Listed = Partner->Items(i);
                    if (AsC4(Listed)->ID == AsC4(Item)->ID && AsC4(Listed)->Enchant == AsC4(Item)->Enchant)
                    {
                        Partner->Items.Remove(i, 1);
                        i--;
                    }
                }
                if (Count > 100)
                    Count = 100;
                for (; Count > 0; Count--)
                    Partner->AddEntry(Item, 1);
            }
            else
            {
                NItemEntry* Same = FindBuyStackIn(Partner, Item);
                if (Same)
                {
                    AsC4(Same)->TradeCount += Count;
                    AsC4(Same)->UnknownC20 = AsC4(Item)->UnknownC20;
                    PrivateBuyToolTip(Same, Partner->Side);
                }
                else
                    Partner->AddEntry(Item, Count);
            }
        }
        else if (Partner->Side == 1)
        {
            if (Item->IsStackable() && Count < AsC4(Item)->TradeCount)
            {
                AsC4(Item)->TradeCount -= Count;
                PrivateBuyToolTip(Item, Side);
            }
            else
                Items.Remove(Selected, 1);
        }
    }
    else if (Mode == 0 && (Partner->Side == 0 || Partner->Side == 1))
    {
        NItemEntry* Same = NULL;
        if (Item->IsStackable())
            Same = FindBuyStackIn(Partner, Item);
        if (Same)
        {
            AsC4(Same)->TradeCount += Count;
            AsC4(Same)->UnknownC34 += Count;
            PrivateBuyToolTip(Same, Partner->Side);
        }
        else
            Partner->AddEntry(Item, Count);
        if (Item->IsStackable() && Count < AsC4(Item)->TradeCount)
        {
            AsC4(Item)->UnknownC34 -= Count;
            AsC4(Item)->TradeCount -= Count;
            PrivateBuyToolTip(Item, Side);
        }
        else
            Items.Remove(Selected, 1);
    }
    GDesktop->PrivateBuyWnd->UpdateTotal();
    Selected = -1;
    UpdateScrollRange();
    return 1;
}
NW_HOOK_METHOD(0x100e4e20, NCPrivateBuyItemWnd, MoveItem)

// 0x100e6080 - the vtable, ConditionalDestroy, NCFrameCtrl
NW_HOOK_DTOR(0x100e6080, NCPrivateBuyFrameCtrl)

NW_CHECK_VTABLE(NCPrivateBuyFrameCtrl, 0x101c51a8, 79)

NW_CHECK_VTABLE(NCPrivateBuyItemWnd, 0x101c52e8, 86)

/*-----------------------------------------------------------------------------
    NCPrivateBuyWnd.
-----------------------------------------------------------------------------*/

// 0x100e6250
NCPrivateBuyWnd::NCPrivateBuyWnd()
{
    PlayerItems = NULL;
    StoreItems = NULL;
    Unknown9C = 0;
    BackTex = NULL;
    UnknownA4 = 0;
    UpButton = NULL;
    DownButton = NULL;
    StartButton = NULL;
    MessageButton = NULL;
    AbortButton = NULL;
    UnknownBC = NULL;
    Adena = 0;
    OwnerID = -1;
}
NW_HOOK_CTOR(0x100e6250, NCPrivateBuyWnd)

// 0x100e62f0
NW_HOOK_DTOR(0x100e62f0, NCPrivateBuyWnd)

// 0x100e3250 (vslot 25) - the buttons and the unusual prices confirmed
INT NCPrivateBuyWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCWnd::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND)
    {
        if (wParam == NID_BUYSTORE_START)
            return OnStart() != 0;
        if (wParam == NID_BUYSTORE_UP)
            return OnUp() != 0;
        if (wParam == NID_BUYSTORE_DOWN)
            return OnDown() != 0;
        if (wParam == NID_BUYSTORE_CLOSE)
            return OnClose() != 0;
        if (wParam == NID_BUYSTORE_MESSAGE)
            return OnMessage() != 0;
        if (wParam == NID_BUYSTORE_ABORT)
            return OnAbort() != 0;
    }
    else if (Msg == NWM_BUYSTORE_SELL_OK)
        return OnSellConfirmed(wParam, lParam) != 0;
    return 0;
}
NW_HOOK_METHOD(0x100e3250, NCPrivateBuyWnd, WindowProc)

// 0x100e33c0 (vslot 48) - the window shows; setting one's own store up the Abort and Message buttons show and the start
// button says Start, at someone's store they hide and it says OK (Mode is at +0x114 here, the sell window's +0x11C)
void NCPrivateBuyWnd::ShowWindow()
{
    NCWnd::ShowWindow();
    if (Mode == 1)
    {
        AbortButton->ShowWindow();
        MessageButton->ShowWindow();
        StartButton->SetTextID(STR_Start);
    }
    else if (Mode == 0)
    {
        AbortButton->HideWindow();
        MessageButton->HideWindow();
        StartButton->SetTextID(STR_OK);
    }
}
NW_HOOK_METHOD(0x100e33c0, NCPrivateBuyWnd, ShowWindow)

// 0x100e3450 (vslot 77) - closing one's own store also tells the server (NetHandler slot 204); the close sound
INT NCPrivateBuyWnd::OnClose()
{
    if (Mode == 1)
        GConsole->RequestPrivateStoreBuyManageCancel();
    Reset();
    HideWindow();
    GConsole->PlayInterfaceSound(6);
    return 1;
}
NW_HOOK_METHOD(0x100e3450, NCPrivateBuyWnd, OnClose)

// 0x100e3490 (vslot 79) - Abort: the store closes (NetHandler slot 205)
INT NCPrivateBuyWnd::OnAbort()
{
    GConsole->RequestPrivateStoreQuitBuy();
    Reset();
    HideWindow();
    return 1;
}
NW_HOOK_METHOD(0x100e3490, NCPrivateBuyWnd, OnAbort)

// 0x100e3b00 (vslot 62) - the texts of the two grids, the amount of the list and the player's adena; the clip is the
// window's, the texture (0, 20). Setting one's own store up the wish list's heading also shows "(n/limit)" (new in C4)
INT NCPrivateBuyWnd::OnPaint(NCGDevice* Device)
{
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    if (BackTex)
        Device->DrawTexture(0, 20, (INT)Width, (INT)(Height - 20.0f), 0, 0, (INT)Width, (INT)(Height - 20.0f), BackTex, 0xFF, 1);
    SIZE Size;
    if (Mode == 0)
    {
        Device->DrawText(11, 32, 0xFFDCDCDC, *SysString(STR_ForSale), 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
        Device->DrawText(11, 198, 0xFFDCDCDC, *SysString(STR_ItemsOnSale), 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
        NCGDevice::GetTextExtent(&Size, *SysString(STR_PriceSell), 0, 0, 0, 0, 0);
        Device->DrawText(156 - Size.cx, 331, 0xFFDCDCDC, *SysString(STR_PriceSell), 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
        NCGDevice::GetTextExtent(&Size, *SysString(STR_Adena), 0, 0, 0, 0, 0);
        Device->DrawText(156 - Size.cx, 350, 0xFFDCDCDC, *SysString(STR_Adena), 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
    }
    else if (Mode == 1)
    {
        Device->DrawText(11, 32, 0xFFDCDCDC, *SysString(STR_Inventory), 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
        Device->DrawText(11, 198, 0xFFDCDCDC, *SysString(STR_WishList), 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
        NCGDevice::GetTextExtent(&Size, *SysString(STR_Price), 0, 0, 0, 0, 0);
        Device->DrawText(156 - Size.cx, 331, 0xFFDCDCDC, *SysString(STR_Price), 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
        NCGDevice::GetTextExtent(&Size, *SysString(STR_Adena), 0, 0, 0, 0, 0);
        Device->DrawText(156 - Size.cx, 350, 0xFFDCDCDC, *SysString(STR_Adena), 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
        // "(n/limit)": the wish list's size and the limit, right-aligned in the heading line (5 pixels in from the right edge)
        TCHAR CountText[128];
        appSprintf(CountText, TEXT("(%d/%d)"), StoreItems->Items.Num(), ItemLimit);
        NCGDevice::GetTextExtent(&Size, CountText, 0, 0, 0, 0, 0);
        Device->DrawText((INT)(Width - Size.cx - 5.0f), 198, 0xFFB09B79, CountText, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
    }

    // "%d" takes the total's low half and its high one is the next argument, as in the stock code
    TCHAR Buf[132];
    TCHAR Total[128];
    TCHAR AdenaText[128];
    appSprintf(Buf, TEXT("%d"), TotalPrice);
    FormatNumber(Total, Buf);
    appSprintf(Buf, TEXT("%s"), Total);
    NCGDevice::GetTextExtent(&Size, Buf, 0, 0, 0, 0, 0);
    Device->DrawText(247 - Size.cx, 331, 0xFFDCDCDC, Buf, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
    appSprintf(Buf, TEXT("%d"), Adena);
    FormatNumber(AdenaText, Buf);
    appSprintf(Buf, TEXT("%s"), AdenaText);
    NCGDevice::GetTextExtent(&Size, Buf, 0, 0, 0, 0, 0);
    Device->DrawText(247 - Size.cx, 350, 0xFFDCDCDC, Buf, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
}
NW_HOOK_METHOD(0x100e3b00, NCPrivateBuyWnd, OnPaint)

// 0x100e4090 (vslot 38) - the children go (each one out of the window's list, then deleted)
INT NCPrivateBuyWnd::OnDestroy()
{
    DestroyStoreChild(this, PlayerItems);
    DestroyStoreChild(this, StoreItems);
    DestroyStoreChild(this, UpButton);
    DestroyStoreChild(this, DownButton);
    DestroyStoreChild(this, StartButton);
    DestroyStoreChild(this, UnknownBC);
    DestroyStoreChild(this, MessageButton);
    DestroyStoreChild(this, AbortButton);
    DestroyStoreChild(this, BuyFrame);
    NCWnd::OnDestroy();
    return 1;
}
NW_HOOK_METHOD(0x100e4090, NCPrivateBuyWnd, OnDestroy)

// 0x100e4410 (vslot 74) - Start: the store opens with the wish list (its count, then the item id, enchant, the sixth-number
// field of the entry (+0x1B84, new in C4), count and price of each). OK at someone's store: the items chosen are sold (the
// owner's object id first, then the count, and per item the object id, item id, enchant, +0x1B84, count and price),
// unusual prices asking first
INT NCPrivateBuyWnd::OnStart()
{
    if (Mode == 1)
    {
        INT Count = 0;
        for (INT i = 0; i < StoreItems->Items.Num(); i++)
            if (AsC4(StoreItems->Items(i))->TradeCount > 0)
                Count++;
        L2ParamStack Params(Count * 5 + 1);
        Params.PushBack((void*)(INT_PTR)Count);
        for (INT i = 0; i < StoreItems->Items.Num(); i++)
        {
            NItemEntry* Item = StoreItems->Items(i);
            if (AsC4(Item)->TradeCount > 0)
            {
                Params.PushBack((void*)(INT_PTR)AsC4(Item)->ID);
                Params.PushBack((void*)(INT_PTR)AsC4(Item)->Enchant);
                Params.PushBack((void*)(INT_PTR)AsC4(Item)->Unknown1C7C);
                Params.PushBack((void*)(INT_PTR)AsC4(Item)->TradeCount);
                Params.PushBack((void*)(INT_PTR)AsC4(Item)->UnknownC20);
            }
        }
        GConsole->RequestSetPrivateStoreListBuy(Params);
        Reset();
        HideWindow();
    }
    else if (Mode == 0)
    {
        INT Count = 0;
        for (INT i = 0; i < PlayerItems->Items.Num(); i++)
            if (AsC4(PlayerItems->Items(i))->TradeCount > 0)
                Count++;
        L2ParamStack Params(Count * 6 + 2);
        Params.PushBack((void*)(INT_PTR)OwnerID);
        Params.PushBack((void*)(INT_PTR)Count);
        UBOOL bUsual = 1;
        for (INT i = 0; i < PlayerItems->Items.Num(); i++)
        {
            NItemEntry* Item = PlayerItems->Items(i);
            if (AsC4(Item)->TradeCount > 0)
            {
                if (IsUnusualPrice(Item))
                    bUsual = 0;
                Params.PushBack((void*)(INT_PTR)AsC4(Item)->ObjectID);
                Params.PushBack((void*)(INT_PTR)AsC4(Item)->ID);
                Params.PushBack((void*)(INT_PTR)AsC4(Item)->Enchant);
                Params.PushBack((void*)(INT_PTR)AsC4(Item)->Unknown1C7C);
                Params.PushBack((void*)(INT_PTR)AsC4(Item)->TradeCount);
                Params.PushBack((void*)(INT_PTR)AsC4(Item)->UnknownC20);
            }
        }
        if (bUsual)
        {
            GConsole->RequestPrivateStoreSell(Params);
            Reset();
            HideWindow();
        }
        else
        {
            NDialogInfo Info;
            Info.ID = DIALOG_BUYSTORE_SELL_CAUTION;
            Info.Owner = Entry;
            Info.bMainBox = 1;
            Info.bInput = 0;
            Info.SetSystemMessage(MSG_PriceCaution);
            GConsole->ShowDialog(&Info);
        }
    }
    return 1;
}
NW_HOOK_METHOD(0x100e4410, NCPrivateBuyWnd, OnStart)

// 0x100e4810 (anonymous in the db) - the total the list comes to: at someone's store the items chosen (PlayerItems), setting
// one's own up the wish list (StoreItems), each one's price times its amount
void NCPrivateBuyWnd::UpdateTotal()
{
    TotalPrice = 0;
    if (Mode == 0)
    {
        for (INT i = 0; i < PlayerItems->Items.Num(); i++)
            TotalPrice += (SQWORD)PlayerItems->Items(i)->GetUnknownC20() * AsC4(PlayerItems->Items(i))->TradeCount;
    }
    else if (Mode == 1)
    {
        for (INT i = 0; i < StoreItems->Items.Num(); i++)
            TotalPrice += (SQWORD)StoreItems->Items(i)->GetUnknownC20() * AsC4(StoreItems->Items(i))->TradeCount;
    }
}
NW_HOOK_METHOD(0x100e4810, NCPrivateBuyWnd, UpdateTotal)

// 0x100e4920 (vslot 78) - Message: the store's message, typed in the desktop's box (dialog 0x2c hands it to the console);
// the box starts with the message the player's pawn has now (the FString at +0x16A8 of the pawn in C4)
INT NCPrivateBuyWnd::OnMessage()
{
    NDialogInfo Info;
    Info.ID = DIALOG_BUYSTORE_MESSAGE;
    Info.Owner = PlayerItems->Entry;
    Info.bMainBox = 1;
    Info.bInput = 1;
    Info.SetSystemMessage(MSG_StoreMessage);
    Info.bLargeAmount = 0;
    Info.Type = 0;
    const TCHAR* Text = NULL;
    User* Me = GConsole->GetMyUser();
    if (Me && Me->Pawn)
        Text = *nw::Field<FString>(Me->Pawn, 0x16A8);   // C4: the stock's lea ecx,[eax+0x16A8] (C3: +0x1628)
    Info.SetText60(Text);
    GConsole->ShowDialog(&Info);
    return 1;
}
NW_HOOK_METHOD(0x100e4920, NCPrivateBuyWnd, OnMessage)

// 0x100e4a00 (vslot 80) - selling at the unusual prices confirmed (the same request as OnStart's, without the check)
INT NCPrivateBuyWnd::OnSellConfirmed(WPARAM wParam, LPARAM lParam)
{
    if (Mode == 0)
    {
        INT Count = 0;
        for (INT i = 0; i < PlayerItems->Items.Num(); i++)
            if (AsC4(PlayerItems->Items(i))->TradeCount > 0)
                Count++;
        L2ParamStack Params(Count * 6 + 2);
        Params.PushBack((void*)(INT_PTR)OwnerID);
        Params.PushBack((void*)(INT_PTR)Count);
        for (INT i = 0; i < PlayerItems->Items.Num(); i++)
        {
            NItemEntry* Item = PlayerItems->Items(i);
            if (AsC4(Item)->TradeCount > 0)
            {
                Params.PushBack((void*)(INT_PTR)AsC4(Item)->ObjectID);
                Params.PushBack((void*)(INT_PTR)AsC4(Item)->ID);
                Params.PushBack((void*)(INT_PTR)AsC4(Item)->Enchant);
                Params.PushBack((void*)(INT_PTR)AsC4(Item)->Unknown1C7C);
                Params.PushBack((void*)(INT_PTR)AsC4(Item)->TradeCount);
                Params.PushBack((void*)(INT_PTR)AsC4(Item)->UnknownC20);
            }
        }
        GConsole->RequestPrivateStoreSell(Params);
    }
    Reset();
    HideWindow();
    return 1;
}
NW_HOOK_METHOD(0x100e4a00, NCPrivateBuyWnd, OnSellConfirmed)

// 0x100e5390 - both grids take the mode; they move to its places
void NCPrivateBuyWnd::SetMode(INT InMode)
{
    Mode = InMode;
    StoreItems->SetMode(Mode);
    PlayerItems->SetMode(Mode);
    UpdateLayout();
}
NW_HOOK_METHOD(0x100e5390, NCPrivateBuyWnd, SetMode)

// 0x100e53d0 - one's own list adds up again
void NCPrivateBuyWnd::AddStoreItem(NItemInfo* Info)
{
    StoreItems->AddItem(Info);
    if (Mode == 1)
        UpdateTotal();
}
NW_HOOK_METHOD(0x100e53d0, NCPrivateBuyWnd, AddStoreItem)

// 0x100e5400
UBOOL NCPrivateBuyWnd::AddPlayerItem(NItemInfo* Info)
{
    return PlayerItems->AddItem(Info);
}
NW_HOOK_METHOD(0x100e5400, NCPrivateBuyWnd, AddPlayerItem)

// 0x100e5410 (vslot 75) - the up arrow: the selected item of the lower grid goes up (a stack of more than one asks how
// many; at someone's store an item the player lacks stays)
INT NCPrivateBuyWnd::OnUp()
{
    if (Mode == 0)
    {
        INT Sel = PlayerItems->GetSelected();
        if (Sel < 0)
            return 1;
        if (Sel >= PlayerItems->Items.Num())
            return 1;
        NItemEntry* Item = PlayerItems->Items(Sel);
        if (!Item)
            return 1;
        if (AsC4(Item)->Count < 1)
            return 1;
        if (Item->IsStackable() && AsC4(Item)->TradeCount > 1)
        {
            AskAmount(PlayerItems, Item, MSG_MoveHowMany, (INT)AsC4(Item)->TradeCount);
            return 1;
        }
        PlayerItems->MoveItem(Item, 1);
    }
    else
    {
        if (Mode != 1)
            return 1;
        INT Sel = StoreItems->GetSelected();
        if (Sel < 0)
            return 1;
        if (Sel >= StoreItems->Items.Num())
            return 1;
        NItemEntry* Item = StoreItems->Items(Sel);
        if (!Item)
            return 1;
        if (Item->IsStackable() && AsC4(Item)->TradeCount > 1)
        {
            AskAmount(StoreItems, Item, MSG_BuyHowMany2, (INT)AsC4(Item)->TradeCount);
            return 1;
        }
        StoreItems->MoveItem(Item, 1);
    }
    return 1;
}
NW_HOOK_METHOD(0x100e5410, NCPrivateBuyWnd, OnUp)

// 0x100e56e0 (vslot 76) - the down arrow: at someone's store the selected item comes down (a stack asks how many; one the
// player lacks stays); setting one's own up, the price first
INT NCPrivateBuyWnd::OnDown()
{
    if (Mode == 0)
    {
        INT Sel = StoreItems->GetSelected();
        if (Sel < 0)
            return 1;
        if (Sel >= StoreItems->Items.Num())
            return 1;
        NItemEntry* Item = StoreItems->Items(Sel);
        if (!Item)
            return 1;
        if (AsC4(Item)->Count < 1)
            return 1;
        if (!Item->IsStackable())
        {
            StoreItems->MoveItem(Item, 1);
            return 1;
        }
        AskAmount(StoreItems, Item, MSG_MoveHowMany, (INT)AsC4(Item)->TradeCount);
    }
    else
    {
        if (Mode != 1)
            return 1;
        INT Sel = PlayerItems->GetSelected();
        if (Sel < 0)
            return 1;
        if (Sel >= PlayerItems->Items.Num())
            return 1;
        NItemEntry* Item = PlayerItems->Items(Sel);
        if (!Item)
            return 1;
        AskPrice(PlayerItems, Item);
    }
    return 1;
}
NW_HOOK_METHOD(0x100e56e0, NCPrivateBuyWnd, OnDown)

// 0x100e6340 (vslot 37) - NCPrivateShopWnd::OnCreate's with the buy grids (160 items at most, the cells 5 and 3 pixels
// apart): the base's, the back, the title bar, the two grids (the store's list below, the player's side on top), the
// arrows and the Start, Message and Abort buttons
INT NCPrivateBuyWnd::OnCreate()
{
    NCWnd::OnCreate();
    BackTex = FindTexture(TEXT("L2UI_CH3.StoreWnd.Store1_back"));

    BuyFrame = NewBuyFrameCtrl();
    FrameID = NID_BUYSTORE_FRAME;
    BuyFrame->ID = NID_BUYSTORE_FRAME;
    BuyFrame->SetPosition(0, 0, (INT)Width, 20);
    CreateChild(BuyFrame, NWS_VISIBLE);

    StoreItems = new NCPrivateBuyItemWnd(6, 3, 160, 32, 5, 3);
    StoreItems->SetPosition(9, 215, 239, 139);
    CreateChild(StoreItems, NWS_VISIBLE | NWS_UNKNOWN20);
    StoreItems->SetSide(0);

    PlayerItems = new NCPrivateBuyItemWnd(6, 4, 160, 32, 5, 3);
    PlayerItems->SetPosition(9, 48, 239, 104);
    CreateChild(PlayerItems, NWS_VISIBLE | NWS_UNKNOWN20);
    PlayerItems->SetSide(1);

    StoreItems->SetPartner(PlayerItems);
    PlayerItems->SetPartner(StoreItems);

    UpButton = new NCButton(TEXT("L2UI_CH3.Button.UpButton"), TEXT("L2UI_CH3.Button.UpButton"), 0);
    UpButton->ID = NID_BUYSTORE_UP;
    UpButton->SetPosition(112, 194, 15, 15);
    CreateChild(UpButton, NWS_VISIBLE);

    DownButton = new NCButton(TEXT("L2UI_CH3.Button.DownButton"), TEXT("L2UI_CH3.Button.DownButton"), 0);
    DownButton->ID = NID_BUYSTORE_DOWN;
    DownButton->SetPosition(130, 194, 15, 15);
    CreateChild(DownButton, NWS_VISIBLE);

    StartButton = new NCButton(TEXT("L2UI_CH3.Button.Btn1_Normal"), TEXT("L2UI_CH3.Button.Btn1_NormalOn"), 0);
    StartButton->ID = NID_BUYSTORE_START;
    StartButton->SetPosition(164, 372, 76, 23);
    CreateChild(StartButton, NWS_VISIBLE);
    StartButton->SetTextID(STR_Start);

    MessageButton = new NCButton(TEXT("L2UI_CH3.Button.Btn1_Normal"), TEXT("L2UI_CH3.Button.Btn1_NormalOn"), 0);
    MessageButton->ID = NID_BUYSTORE_MESSAGE;
    MessageButton->SetPosition(87, 372, 76, 23);
    CreateChild(MessageButton, NWS_VISIBLE);
    MessageButton->SetTextID(STR_Message);

    AbortButton = new NCButton(TEXT("L2UI_CH3.Button.Btn1_Normal"), TEXT("L2UI_CH3.Button.Btn1_NormalOn"), 0);
    AbortButton->ID = NID_BUYSTORE_ABORT;
    AbortButton->SetPosition(10, 372, 76, 23);
    CreateChild(AbortButton, NWS_VISIBLE);
    AbortButton->SetTextID(STR_Abort);
    return 1;
}
NW_HOOK_METHOD(0x100e6340, NCPrivateBuyWnd, OnCreate)

// 0x100e3440 (vslot 49) - also the sell window's
void NCPrivateBuyWnd::HideWindow()
{
    Reset();
    NCWnd::HideWindow();
}
NW_HOOK_METHOD(0x100e3440, NCPrivateBuyWnd, HideWindow)

// stock body shared with NCPrivateShopWnd's (0x100d71c0)
void NCPrivateBuyWnd::SetAdena(INT InAdena)
{
    Adena = InAdena;
}

// stock body shared with NCShopWnd::SetAdena (0x100efa00); the console sets it
void NCPrivateBuyWnd::SetOwnerID(INT InOwnerID)
{
    OwnerID = InOwnerID;
}

// 0x100e3380 (named NCPrivateShopWnd::Reset in the db) - the store is forgotten: owner none, adena and total 0, both grids
// emptied. C4 has one of its own for each window (the sell window's is 0x100e6b90)
void NCPrivateBuyWnd::Reset()
{
    OwnerID = -1;
    Adena = 0;
    TotalPrice = 0;
    PlayerItems->Clear();
    StoreItems->Clear();
}
NW_HOOK_METHOD(0x100e3380, NCPrivateBuyWnd, Reset)

// 0x100e7a40 - the one body of both windows (the sell window's CheckTotal, NCPrivateShopWnd.cpp, is the hooked C++ method):
// the grids call it on the desktop's window of their kind. The hook build jumps to the sell window's hook, the standalone has its
// own copy of that body: adding Count times Price must stay within two billion, and so must the total of the list (the entry
// of ItemID at the new price)
#if NW_HOOK_MODE
NW_NAKED UBOOL NCPrivateBuyWnd::CheckTotal(INT ItemID, INT Count, INT Price) NW_STOCK(0x100e7a40)
#else
UBOOL NCPrivateBuyWnd::CheckTotal(INT ItemID, INT Count, INT Price)
{
    INT Sum = 0;
    INT Total = 0;
    for (INT i = 0; i < Count; i++)
    {
        Sum += Price;
        if (Sum > 2000000000)
            return 0;
        if (Sum < 0)
            return 0;
    }
    if (StoreItems)
    {
        for (INT i = 0; i < StoreItems->Items.Num(); i++)
        {
            NItemEntry* Entry = StoreItems->Items(i);
            if (AsC4(Entry)->ID == ItemID)
                Total += (INT)AsC4(Entry)->TradeCount * Price;
            else
                Total += Entry->GetUnknownC20() * (INT)AsC4(Entry)->TradeCount;
        }
    }
    return Sum + Total <= 2000000000 && Sum + Total >= 0;
}
#endif

// 0x100e4290 (anonymous in the db) - the grids placed by mode (the sell window's 0x100e7480 with this window's Mode): the
// store's list on top at someone's store, below when setting one's own up; the two grids are 139 and 104 pixels tall with
// 4 and 3 rows
void NCPrivateBuyWnd::UpdateLayout()
{
    // the stock code fills { LocalX, LocalY, LocalW, LocalH }; SetScreenPos reads x and y
    NRectF Origin;
    if (Mode == 0)
    {
        StoreItems->UpdateScrollRange();
        PlayerItems->UpdateScrollRange();
        Origin.X = 0;
        Origin.Y = 0;
        *(INT*)&Origin.W = 0;
        *(INT*)&Origin.H = 0;
        GetLocalRect(&Origin);
        StoreItems->SetScreenPos(9, 48, (POINT*)&Origin);
        PlayerItems->SetScreenPos(9, 215, (POINT*)&Origin);
        ::nw::VirtualCall<void>(StoreItems, 74, 239, 139);
        ::nw::VirtualCall<void>(PlayerItems, 74, 239, 104);
        PlayerItems->SetVisibleRows(3);
        StoreItems->SetVisibleRows(4);
        return;
    }
    if (Mode == 1)
    {
        StoreItems->UpdateScrollRange();
        PlayerItems->UpdateScrollRange();
        Origin.X = 0;
        Origin.Y = 0;
        *(INT*)&Origin.W = 0;
        *(INT*)&Origin.H = 0;
        GetLocalRect(&Origin);
        StoreItems->SetScreenPos(9, 215, (POINT*)&Origin);
        PlayerItems->SetScreenPos(9, 48, (POINT*)&Origin);
        ::nw::VirtualCall<void>(StoreItems, 74, 239, 104);
        ::nw::VirtualCall<void>(PlayerItems, 74, 239, 139);
        StoreItems->SetVisibleRows(3);
        PlayerItems->SetVisibleRows(4);
    }
}
NW_HOOK_METHOD(0x100e4290, NCPrivateBuyWnd, UpdateLayout)

NW_CHECK_VTABLE(NCPrivateBuyWnd, 0x101c5440, 81)
