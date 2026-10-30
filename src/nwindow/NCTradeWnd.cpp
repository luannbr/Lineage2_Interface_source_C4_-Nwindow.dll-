/*=============================================================================
    NCTradeWnd.cpp - the trade window, its item grids and its title bar.
    Stock (C3): 0x10105870 (the title bar's close), 0x10105890 .. 0x10107340; 0x10088870 (SetKind, shared).
    NCTradeItemWnd's WindowProc (0x100a2850, NCShopWnd.cpp hooks it) and RemoveItem (0x100cf0e0, the pet's grid hooks
    it) are copies of bodies other grids keep the addresses of.
=============================================================================*/
#include "nwindow/NCTradeWnd.h"
#include "nwindow/NCButton.h"
#include "nwindow/NCConsole.h"
#include "nwindow/NCShopWnd.h"
#include "nwindow/NCDialogBox.h"
#include "nwindow/NCFrameCtrl.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NConsoleWnd.h"
#include "nwindow/NDialogInfo.h"
#include "nwindow/NClasses.gen.h"
#include "nwindow/NItemC4.h"

// NDialogInfo::ID (names ours): both ask the inventory grid for an amount (NCDialogBox::OnOK)
enum
{
    DIALOG_TRADE_ADD = 2,            // the down arrow's question
    DIALOG_TRADE_MOVE = 8,           // the grid's (item carried onto the offer, double click)
};

enum { MSG_HowManyToMove = 72 };     // systemmsg "How many $s1(s) do you want to move?"
enum { MSG_TooMany = 0x40C };        // systemmsg: more than the entry holds

// sysstring labels
enum
{
    STR_OK = 140,
    STR_Cancel = 141,
};

// NConsoleWnd::RequestAddTradeItem (0x10043e60, guard name): the item offered, by value
static FORCEINLINE void RequestAddTradeItem(NItemRequest Request)
{
    GConsole->RequestAddTradeItem(Request);
}

// how many of a stack to offer; the answer goes to Owner's OnDialogResult. Inline in each stock caller; the name buffer
// is the stock frame's 64 characters. C3 puts the item's id in the info (+0x18, the header names that field bProgress)
// and gives the box the most it may take (Unknown74: the entry's Count).
static FORCEINLINE void AskTradeAmount(NCWnd* Owner, INT DialogID, NItemEntry* Item)
{
    TCHAR Name[64];
    appSprintf(Name, TEXT("%s"), Item->GetName());
    NDialogInfo Info;
    Info.ID = DialogID;
    Info.Type = 3;
    Info.Owner = Owner->Entry;
    Info.bMainBox = 1;
    Info.bInput = 1;
    Info.SetSystemMessage(MSG_HowManyToMove);
    Info.SetButtons(Name, NULL);
    Info.Flag18 = AsC4(Item)->ID;
    Info.Unknown74 = AsC4(Item)->Count;
    GConsole->ShowDialog(&Info);
}

// delete of an item record: ~NItemEntry (0x10050030) and the memory back to GMalloc
static FORCEINLINE void DeleteEntry(NItemEntry* Item)
{
    Item->~NItemEntry();
    appFree(Item);
}

/*-----------------------------------------------------------------------------
    NCTradeFrameCtrl.
-----------------------------------------------------------------------------*/

// 0x10119d10 (vslot 56, ret 4) - the title given by a system string goes to the title bar of this window, TradeFrame (+0x11C; the
// base's FrameCtrl stays empty). The stock is `mov ecx, [ecx + 0x11C]; jmp NCFrameCtrl::SetTitleID`, no null test
void NCTradeWnd::SetFrameTitleID(INT ID)
{
    TradeFrame->SetTitleID(ID);
}
NW_HOOK_METHOD(0x10119d10, NCTradeWnd, SetFrameTitleID)

// 0x1011aef0 - NCFrameCtrl's default constructor, then this class's vtable
NCTradeFrameCtrl::NCTradeFrameCtrl()
{
}
NW_HOOK_CTOR(0x1011aef0, NCTradeFrameCtrl)

// 0x1011af40
NW_HOOK_DTOR(0x1011af40, NCTradeFrameCtrl)

NW_CHECK_VTABLE(NCTradeFrameCtrl, 0x101cec60, 79)

// 0x10119ba0 (vslot 76) - the close button of the bar answers the trade with "no" first, then closes the bar
INT NCTradeFrameCtrl::OnClose()
{
    GConsole->RequestTradeDone(0);
    return NCFrameCtrl::OnClose();
}
NW_HOOK_METHOD(0x10119ba0, NCTradeFrameCtrl, OnClose)


// a child window goes (repeated inline in the stock OnDestroys)
template <class T> static FORCEINLINE void DestroyChild(NCWnd* Parent, T*& Child)
{
    if (Child)
    {
        Parent->RemoveChildWindow(Child->Entry);
        delete Child;
        Child = NULL;
    }
}

/*-----------------------------------------------------------------------------
    NCTradeItemWnd.
-----------------------------------------------------------------------------*/

// 0x1011af90 - NCItemWnd's default constructor, then this class's vtable
NCTradeItemWnd::NCTradeItemWnd()
{
}
NW_HOOK_CTOR(0x1011af90, NCTradeItemWnd)

// 0x10020370 (vslot 63): the tool tip routine every item grid without one of its own inherits is NCItemWnd::OnDrawToolTip (NCItemWnd.cpp;
// stage 5: its body used to be here, on a stand-in struct derived from the grid)

// C2 CANDIDATE: the C2 function is not the same code as the C1 one; kept only while it matches the stock one (verify_data)
// 0x1011b030 (ret 0x18) - the grid of the six arguments (columns, rows, most items, icon size, gaps); the box of every
// icon is loaded here
NCTradeItemWnd::NCTradeItemWnd(INT InColumns, INT InVisibleRows, INT InMaxItems, INT InCellSize, INT InGapX, INT InGapY)
    : NCItemWnd(InColumns, InVisibleRows, InMaxItems, InCellSize, InGapX, InGapY)
{
    IconBoxTex = FindTexture(TEXT("L2ui_ch3.etc.iconbox"));
}
NW_HOOK_CTOR_ARGS(0x1011b030, NCTradeItemWnd, (INT A, INT B, INT C, INT D, INT E, INT F), (A, B, C, D, E, F))

// 0x1011afe0
NW_HOOK_DTOR(0x1011afe0, NCTradeItemWnd)

// 0x10102860 (vslot 25) - the stock body is shared with NCShopItemWnd::WindowProc, which keeps the address (hooked there): NCItemWnd's, then
// the double click (slot 81) and the dialog box's answer (slot 82). The slot jumps to the stock address, which our hook owns
#if NW_HOOK_MODE
NW_NAKED INT NCTradeItemWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam) NW_STOCK(0x10102860)
#else
INT NCTradeItemWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return ((NCShopItemWnd*)this)->NCShopItemWnd::WindowProc(Msg, wParam, lParam);
}
#endif

// the grid's kind (+148): one body with the other grids' setters of their +148 (NCStoreItemWnd::SetTarget, 0x10088870)
void NCTradeItemWnd::SetKind(INT InKind)
{
    Kind = InKind;
}


// 0x10119d90 (vslot 62) - every icon on its box (the grids' own texture), the selection frame over it, in the
// inventory grid only
INT NCTradeItemWnd::OnPaint(NCGDevice* Device)
{
    guard(NCTradeItemWnd::OnPaint);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    Device->BeginPaint(ScreenX, ScreenY);
    INT X = 1;
    INT Y = 1;
    for (INT Index = TopRow * Columns; Index < Items.Num(); Index++)
    {
        NItemEntry* Item = Items(Index);
        if (!Item)
            continue;
        if (Index % Columns == 0)
        {
            X = 1;
            if (Index != TopRow * Columns)
                Y = Y + 3 + CellSize;
            if ((CellSize + 1) * VisibleRows <= Y)
                break;
        }
        else
            X = X + 5 + CellSize;
        Device->DrawTexture(X - 1, Y - 1, 0x22, 0x22, 0, 0, 0x22, 0x22, IconBoxTex, 0xFF, 1);
        if (Item->GetIcon())
            Device->DrawTexture(X, Y, CellSize, CellSize, 0, 0, CellSize, CellSize, Item->GetIcon(), 0xFF, 1);
        if (Kind == TRADEGRID_Inventory && Item->IsSelected())
            Device->DrawTexture(X - 1, Y - 1, 0x22, 0x22, 0, 0, 0x22, 0x22, SelectTex, 0xFF, 1);
    }
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10119d90, NCTradeItemWnd, OnPaint)

// 0x10119f90 (vslot 67) - the item under the mouse; the carried item is the cursor, in the inventory grid only
INT NCTradeItemWnd::OnMouseMove(WPARAM wParam, LPARAM lParam)
{
    HoverIndex = HitTest(LOWORD(lParam), HIWORD(lParam), 0, 0);
    if (Kind == TRADEGRID_Inventory)
    {
        if (IsFocused() && bDragging && Selected >= 0 && Selected < Items.Num())
        {
            SetCursor(4, Items(Selected)->GetIcon());
            LockCursor();
        }
    }
    return 1;
}
NW_HOOK_METHOD(0x10119f90, NCTradeItemWnd, OnMouseMove)

// 0x1011a010 (vslot 69) - an inventory item carried onto the player's offer grid is offered: how many first for a stack,
// else one (nothing in the offer grids)
INT NCTradeItemWnd::OnLButtonUp(WPARAM wParam, LPARAM lParam)
{
    if (Kind == TRADEGRID_Inventory)
    {
        INT X = LOWORD(lParam);
        INT Y = HIWORD(lParam);
        if (Selected < 0 || Selected >= Items.Num())
            bDragging = 0;
        else
        {
            if (bDragging && !IsInWindow(X, Y))
            {
                NWndEntry* E = FindWindowAt(X, Y);
                if (E && E->Wnd && E->Wnd->IsA(NClass::NCTradeItemWnd()))
                {
                    if (((NCTradeItemWnd*)E->Wnd)->Kind == TRADEGRID_MyOffer)
                    {
                        NItemEntry* Item = Items(Selected);
                        if (Item->IsStackable() && AsC4(Item)->TradeCount > 1)
                        {
                            AskTradeAmount(this, DIALOG_TRADE_MOVE, Item);
                            bDragging = 0;
                            return 1;
                        }
                        NItemRequest Request;
                        Request.ObjectID = AsC4(Item)->ObjectID;
                        Request.Count = 1;
                        RequestAddTradeItem(Request);
                        UnselectAll();
                        Selected = -1;
                    }
                }
            }
            bDragging = 0;
        }
    }
    return 1;
}
NW_HOOK_METHOD(0x1011a010, NCTradeItemWnd, OnLButtonUp)

// 0x1011a250 (vslot 81) - the inventory item is offered: how many first for a stack, else one. In the offer grids
// nothing happens.
INT NCTradeItemWnd::OnLButtonDblClick(WPARAM wParam, LPARAM lParam)
{
    bNoDrag = 1;
    bDragging = 0;
    Selected = HitTest(LOWORD(lParam), HIWORD(lParam), 0, 0);
    if (Selected >= 0 && Selected < Items.Num() && Kind == TRADEGRID_Inventory)
    {
        NItemEntry* Item = Items(Selected);
        if (Item->IsStackable() && AsC4(Item)->TradeCount > 1)
            AskTradeAmount(this, DIALOG_TRADE_MOVE, Item);
        else
        {
            NItemRequest Request;
            Request.ObjectID = AsC4(Item)->ObjectID;
            Request.Count = 1;
            RequestAddTradeItem(Request);
            UnselectAll();
        }
    }
    bDragging = 0;
    return 1;
}
NW_HOOK_METHOD(0x1011a250, NCTradeItemWnd, OnLButtonDblClick)

// 0x1011a530 (vslot 82) - the amount typed (wParam) of the selected item is offered; more than the entry holds is
// refused with a system message. The stock code reads Items(Selected) without a bounds check.
INT NCTradeItemWnd::OnDialogResult(WPARAM wParam, LPARAM lParam)
{
    NItemEntry* Item = Items(Selected);
    INT Count = (INT)wParam;
    if (Item && Count)
    {
        if ((SQWORD)Count > AsC4(Item)->TradeCount)
        {
            GConsole->ShowSystemMessage(MSG_TooMany, 0xFFDCDCDC);
            return 1;
        }
        NItemRequest Request;
        Request.ObjectID = AsC4(Item)->ObjectID;
        Request.Count = Count;
        RequestAddTradeItem(Request);
        UnselectAll();
        Selected = -1;
    }
    return 1;
}
NW_HOOK_METHOD(0x1011a530, NCTradeItemWnd, OnDialogResult)

// 0x1011ada0 (vslot 78) - frees the items and drops the selection; unlike NCItemWnd's, the scroll position stays
void NCTradeItemWnd::Clear()
{
    for (INT i = 0; i < Items.Num(); i++)
        if (Items(i))
            DeleteEntry(Items(i));
    Items.Empty();
    Selected = -1;
}
NW_HOOK_METHOD(0x1011ada0, NCTradeItemWnd, Clear)

// 0x1011ac00 - an entry of the same object id that stacks gets the amount added (the full check comes first);
// otherwise a new entry, its icon from its name ("NWindow.BlackTexture" when missing, written back into Info), its
// count the amount
UBOOL NCTradeItemWnd::AddItem(NItemInfo* Info)
{
    if (Items.Num() >= MaxItems)
        return 0;
    for (INT i = 0; i < Items.Num(); i++)
    {
        NItemEntry* Old = Items(i);
        if (AsC4(Old)->ObjectID == AsC4(Info)->ObjectID && Old->IsStackable())
        {
            AsC4(Old)->TradeCount += AsC4(Info)->Count;
            AsC4(Old)->ToolTip.Fill(Old, 2, 0);
            return 1;
        }
    }
    AsC4(Info)->Icon[0] = FindTexture(AsC4(Info)->IconName[0]);
    if (!AsC4(Info)->Icon[0])
        AsC4(Info)->Icon[0] = FindTexture(TEXT("NWindow.BlackTexture"));
    NItemEntry* Item = NewItemEntry(Info);
    AsC4(Item)->TradeCount = AsC4(Info)->Count;
    AsC4(Item)->ToolTip.Fill(Item, 2, 0);
    Items.AddItem(Item);
    UpdateScrollRange();
    return 1;
}
NW_HOOK_METHOD(0x1011ac00, NCTradeItemWnd, AddItem)

// 0x1011a410 - the entry with the same object id takes Info's values and amount (through a new entry that is never
// freed, as in the stock code)
UBOOL NCTradeItemWnd::UpdateItem(NItemInfo* Info)
{
    for (INT i = 0; i < Items.Num(); i++)
    {
        NItemEntry* Old = Items(i);
        if (AsC4(Info)->ObjectID == AsC4(Old)->ObjectID)
        {
            AsC4(Info)->Icon[0] = FindTexture(AsC4(Info)->IconName[0]);
            if (!AsC4(Info)->Icon[0])
                AsC4(Info)->Icon[0] = FindTexture(TEXT("NWindow.BlackTexture"));
            NItemEntry* New = NewItemEntry(Info);
            AsC4(New)->TradeCount = AsC4(Info)->Count;
            AsC4(New)->ToolTip.Fill(New, 2, 0);
            Old->Copy(New);
            return 1;
        }
    }
    return 0;
}
NW_HOOK_METHOD(0x1011a410, NCTradeItemWnd, UpdateItem)

// 0x1011ae10 (anonymous in the db) - the entry of the object id goes and is freed; one body with the pet inventory's grid
// (NCPetInvenItemWnd::RemoveItem calls this address)
UBOOL NCTradeItemWnd::RemoveItem(INT ObjectID)
{
    for (INT i = 0; i < Items.Num(); i++)
    {
        if (AsC4(Items(i))->ObjectID == ObjectID)
        {
            NItemEntry* Item = Items(i);
            Items.Remove(i);
            if (Item)
                DeleteEntry(Item);
            return 1;
        }
    }
    return 0;
}
NW_HOOK_METHOD(0x1011ae10, NCTradeItemWnd, RemoveItem)

NW_CHECK_VTABLE(NCTradeItemWnd, 0x101ceda0, 83)

/*-----------------------------------------------------------------------------
    NCTradeWnd.
-----------------------------------------------------------------------------*/


// 0x1011b0b0 - the names are FStrings (built here); the title bar's pointer (+0xFC) is left as allocated
NCTradeWnd::NCTradeWnd()
{
    InventoryItems = NULL;
    MyItems = NULL;
    PartnerItems = NULL;
    BackTex = NULL;
    AddButton = NULL;
    OKButton = NULL;
    CancelButton = NULL;
}
NW_HOOK_CTOR(0x1011b0b0, NCTradeWnd)

// 0x1011b170 - the partner's and the player's names
NW_HOOK_DTOR(0x1011b170, NCTradeWnd)

// 0x10119bc0 (vslot 25) - the buttons
INT NCTradeWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCWnd::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND)
    {
        if (wParam == NID_TRADE_OK)
            return OnOK() != 0;
        if (wParam == NID_TRADE_CANCEL)
            return OnCancel() != 0;
        if (wParam == NID_TRADE_ADD)
            return OnAdd() != 0;
    }
    return 0;
}
NW_HOOK_METHOD(0x10119bc0, NCTradeWnd, WindowProc)

// 0x1011b1f0 (vslot 37) - the title bar (its own class, kept at +0x11C; as wide as the window), the grids (the inventory
// 6 x 4 icons, the offers 6 x 2 icons each; 160 items at most), the down arrow between the inventory and the player's
// offer, OK and Cancel
INT NCTradeWnd::OnCreate()
{
    NCWnd::OnCreate();
    BackTex = FindTexture(TEXT("L2UI_ch3.TradeWnd.Trade_Back"));

    TradeFrame = new NCTradeFrameCtrl();
    FrameID = NID_TRADE_FRAME;
    TradeFrame->ID = NID_TRADE_FRAME;
    TradeFrame->SetPosition(0, 0, (DWORD)(INT)Width, 20);
    CreateChild(TradeFrame, NWS_VISIBLE);

    InventoryItems = new NCTradeItemWnd(6, 4, 160, 32, 5, 3);
    InventoryItems->SetPosition(9, 35, 239, 139);
    CreateChild(InventoryItems, NWS_VISIBLE | 0x20);   // 0x20: meaning unknown
    InventoryItems->SetKind(TRADEGRID_Inventory);

    MyItems = new NCTradeItemWnd(6, 2, 160, 32, 5, 3);
    MyItems->SetPosition(9, 200, 239, 69);
    CreateChild(MyItems, NWS_VISIBLE | 0x20);
    MyItems->SetKind(TRADEGRID_MyOffer);

    PartnerItems = new NCTradeItemWnd(6, 2, 160, 32, 5, 3);
    PartnerItems->SetPosition(9, 295, 239, 69);
    CreateChild(PartnerItems, NWS_VISIBLE | 0x20);
    PartnerItems->SetKind(TRADEGRID_PartnerOffer);

    AddButton = new NCButton(TEXT("l2ui_ch3.button.downbutton"), TEXT("l2ui_ch3.button.downbutton"), 0);
    AddButton->ID = NID_TRADE_ADD;
    AddButton->SetPosition(140, 181, 15, 15);
    CreateChild(AddButton, NWS_VISIBLE);

    OKButton = new NCButton(TEXT("L2UI_ch3.button.btn1_normal"), TEXT("L2UI_ch3.button.btn1_normalon"), 0);
    OKButton->ID = NID_TRADE_OK;
    OKButton->SetPosition(51, 372, 76, 23);
    CreateChild(OKButton, NWS_VISIBLE);
    OKButton->SetTextID(STR_OK);

    CancelButton = new NCButton(TEXT("L2UI_ch3.button.btn1_normal"), TEXT("L2UI_ch3.button.btn1_normalon"), 0);
    CancelButton->ID = NID_TRADE_CANCEL;
    CancelButton->SetPosition(131, 372, 76, 23);
    CreateChild(CancelButton, NWS_VISIBLE);
    CancelButton->SetTextID(STR_Cancel);
    return 1;
}
NW_HOOK_METHOD(0x1011b1f0, NCTradeWnd, OnCreate)

// 0x1011a790 (vslot 38)
INT NCTradeWnd::OnDestroy()
{
    DestroyChild(this, InventoryItems);
    DestroyChild(this, MyItems);
    DestroyChild(this, PartnerItems);
    DestroyChild(this, AddButton);
    DestroyChild(this, OKButton);
    DestroyChild(this, CancelButton);
    DestroyChild(this, TradeFrame);
    NCWnd::OnDestroy();
    return 1;
}
NW_HOOK_METHOD(0x1011a790, NCTradeWnd, OnDestroy)

// 0x10119cb0 (vslot 48) - the grids' scroll ranges follow what they got while hidden
void NCTradeWnd::ShowWindow()
{
    NCWnd::ShowWindow();
    InventoryItems->UpdateScrollRange();
    MyItems->UpdateScrollRange();
    PartnerItems->UpdateScrollRange();
}
NW_HOOK_METHOD(0x10119cb0, NCTradeWnd, ShowWindow)

// stock body shared with NCChatWnd's (0x1003dab0)
void NCTradeWnd::HideWindow()
{
    NCWnd::HideWindow();
}

// 0x1011a620 (vslot 62) - the background under the title bar and the two names
INT NCTradeWnd::OnPaint(NCGDevice* Device)
{
    guard(NCTradeWnd::OnPaint);
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    if (BackTex)
    {
        INT H = (INT)(Height - 20.0f);
        INT W = (INT)Width;
        Device->DrawTexture(0, 20, W, H, 0, 0, W, H, BackTex, 0xFF, 1);
    }
    if (MyName.Num())
        Device->DrawText(11, 184, 0xFFDCDCDC, *MyName, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
    if (PartnerName.Num())
        Device->DrawText(11, 278, 0xFFDCDCDC, *PartnerName, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1011a620, NCTradeWnd, OnPaint)

// 0x10119d50 (vslot 74)
INT NCTradeWnd::OnOK()
{
    GConsole->RequestTradeDone(1);
    return 1;
}
NW_HOOK_METHOD(0x10119d50, NCTradeWnd, OnOK)

// 0x10119d70 (vslot 75)
INT NCTradeWnd::OnCancel()
{
    GConsole->RequestTradeDone(0);
    return 1;
}
NW_HOOK_METHOD(0x10119d70, NCTradeWnd, OnCancel)

// 0x1011a8e0 (vslot 76) - the down arrow offers the selected inventory item: how many first for a stack (the
// inventory grid asks and gets the answer), else one
INT NCTradeWnd::OnAdd()
{
    INT Sel = InventoryItems->GetSelected();
    if (Sel >= 0 && Sel < InventoryItems->Items.Num())
    {
        NItemEntry* Item = InventoryItems->Items(Sel);
        if (Item)
        {
            if (Item->IsStackable() && AsC4(Item)->TradeCount > 1)
                AskTradeAmount(InventoryItems, DIALOG_TRADE_ADD, Item);
            else
            {
                NItemRequest Request;
                Request.ObjectID = AsC4(Item)->ObjectID;
                Request.Count = 1;
                RequestAddTradeItem(Request);
            }
        }
    }
    return 1;
}
NW_HOOK_METHOD(0x1011a8e0, NCTradeWnd, OnAdd)

// 0x10119c60 - a trade starts or ends: the grids emptied, the partner's name cleared (the player's is set again by the
// start); C2 clears both names
void NCTradeWnd::Reset()
{
    InventoryItems->Clear();
    MyItems->Clear();
    PartnerItems->Clear();
    PartnerName.Empty();
    MyName.Empty();
}
NW_HOOK_METHOD(0x10119c60, NCTradeWnd, Reset)

// 0x10119ce0
void NCTradeWnd::SetMyName(const TCHAR* Name)
{
    if (Name && *Name)
        MyName = Name;
    else
        MyName.Empty();
}
NW_HOOK_METHOD(0x10119ce0, NCTradeWnd, SetMyName)

// 0x10119d20
void NCTradeWnd::SetPartnerName(const TCHAR* Name)
{
    if (Name && *Name)
        PartnerName = Name;
    else
        PartnerName.Empty();
}
NW_HOOK_METHOD(0x10119d20, NCTradeWnd, SetPartnerName)

// 0x1011ad70
UBOOL NCTradeWnd::AddInventoryItem(NItemInfo* Info)
{
    return InventoryItems->AddItem(Info);
}
NW_HOOK_METHOD(0x1011ad70, NCTradeWnd, AddInventoryItem)

// 0x1011ad80
UBOOL NCTradeWnd::AddMyItem(NItemInfo* Info)
{
    return MyItems->AddItem(Info);
}
NW_HOOK_METHOD(0x1011ad80, NCTradeWnd, AddMyItem)

// 0x1011ad90
UBOOL NCTradeWnd::AddPartnerItem(NItemInfo* Info)
{
    return PartnerItems->AddItem(Info);
}
NW_HOOK_METHOD(0x1011ad90, NCTradeWnd, AddPartnerItem)

// 0x1011ae80 (C2: an anonymous function of its own, C1 0x100ba3a0 was inline in the console) - a change of the player's
// inventory while trading: 1 add, 3 change, 2 remove (the object id of the item)
UBOOL NCTradeWnd::UpdateInventoryItem(INT Op, NItemInfo* Info)
{
    if (Op == 1)
    {
        InventoryItems->AddItem(Info);
        return 1;
    }
    if (Op == 3)
    {
        InventoryItems->UpdateItem(Info);
        return 1;
    }
    if (Op == 2)
        InventoryItems->RemoveItem(AsC4(Info)->ObjectID);
    return 1;
}
NW_HOOK_METHOD(0x1011ae80, NCTradeWnd, UpdateInventoryItem)

// one grid's entries get their name and description again from the item tables, and their
// tool tips (the stock code repeats this for each grid)
static FORCEINLINE void ReloadItemTexts(NCTradeItemWnd* Grid)
{
    for (INT i = 0; i < Grid->Items.Num(); i++)
    {
        NItemEntry* Item = Grid->Items(i);
        if (Item)
        {
            appStrcpy(AsC4(Item)->Name, GL2GameData.GetItemName(AsC4(Item)->ID));
            appStrcpy(AsC4(Item)->Description, GL2GameData.GetItemDescription(AsC4(Item)->ID));
            AsC4(Item)->ToolTip.Fill(Item, 2, 0);
        }
    }
}

// 0x1011aa70 - after the item texts were reloaded (NConsoleWnd::ReloadGameTexts)
void NCTradeWnd::ReloadTexts()
{
    ReloadItemTexts(InventoryItems);
    ReloadItemTexts(MyItems);
    ReloadItemTexts(PartnerItems);
}
NW_HOOK_METHOD(0x1011aa70, NCTradeWnd, ReloadTexts)

NW_CHECK_VTABLE(NCTradeWnd, 0x101ceef0, 77)
