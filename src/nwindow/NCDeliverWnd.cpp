/*=============================================================================
    NCDeliverWnd.cpp - the package windows: the choice of the character and the package
    window with its item grids.
    C3 stock: 0x10079fe0 .. 0x1007bdb0 (the grids' 0x1007b330 .. 0x1007b760, MoveItem 0x1007ac30; AddItem 0x1007aa40,
    AddCopy 0x1007ab50 and AddInventoryItem 0x1007ad80 are called by address). WindowProc (0x100a2850), OnDestroy
    (0x100d7090) and SetTarget (0x10088870) of the grid, WindowProc (0x100f0070) of the package window and OnCancel
    (0x1007bee0) of the choice are copies of bodies other classes keep the addresses of.
=============================================================================*/
#include "nwindow/NCDeliverWnd.h"
#include "nwindow/NItemC4.h"
#include "nwindow/NCConsole.h"
#include "nwindow/NCButton.h"
#include "nwindow/NCComboBox.h"
#include "nwindow/NCDialogBox.h"
#include "nwindow/NCFrameCtrl.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NCStoreWnd.h"
#include "nwindow/NConsoleWnd.h"
#include "nwindow/NConsoleFns.h"
#include "nwindow/NDialogInfo.h"
#include "nwindow/NClasses.gen.h"

enum { DIALOG_DELIVER_AMOUNT = 3 };  // NDialogInfo::ID (name ours): an amount for the owner
enum { MSG_HowManyToMove = 72 };     // systemmsg "How many $s1(s) do you want to move?"
enum { MSG_ChooseReceiver = 664 };   // systemmsg "Please choose a person to receive."
enum { ITEM_Adena = 57 };
enum { SOUND_WindowClose = 6 };      // interface sound (name ours)
enum { DELIVER_FEE_PER_SLOT = 1000 };  // adena (name ours)
enum { MAX_DELIVER_TARGETS = 8 };

// sysstring labels
enum
{
    STR_Adena = 134,
    STR_Inventory = 138,
    STR_OK = 140,
    STR_Cancel = 141,
    STR_SendingList = 560,
    STR_SendingFee = 561,
};

// the package window
static NCDeliverWnd* DeliverWnd()
{
    return GDesktop->DeliverWnd;
}

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

// how many of a stack to move: the box (the whole stack at most, Unknown74) answers to Owner's OnDialogResult; the item's
// id goes in the field at +0x18 (C3: not the adena flag of C2). Inline in each stock caller; the name buffer is the stock
// frame's 64 characters
static FORCEINLINE void AskAmount(NCWnd* Owner, NItemEntry* Item)
{
    TCHAR Name[64];
    appSprintf(Name, TEXT("%s"), Item->GetName());
    NDialogInfo Info;
    Info.ID = DIALOG_DELIVER_AMOUNT;
    Info.Type = 3;
    Info.Owner = Owner->Entry;
    Info.bMainBox = 1;
    Info.bInput = 1;
    Info.SetSystemMessage(MSG_HowManyToMove);
    Info.SetButtons(Name, NULL);
    Info.Unknown74 = AsC4(Item)->Count;
    Info.Flag18 = AsC4(Item)->ID;
    GConsole->ShowDialog(&Info);
}

/*-----------------------------------------------------------------------------
    NCDeliverItemWnd.
-----------------------------------------------------------------------------*/

// 0x10086360 - the grid's own partner is none
NCDeliverItemWnd::NCDeliverItemWnd()
{
    Target = NULL;
}
NW_HOOK_CTOR(0x10086360, NCDeliverItemWnd)

// 0x10086400 - the grids of the package window: columns, rows, most items, cell size and the gaps
NCDeliverItemWnd::NCDeliverItemWnd(INT InColumns, INT InVisibleRows, INT InMaxItems, INT InCellSize, INT InGapX,
                                   INT InGapY)
    : NCItemWnd(InColumns, InVisibleRows, InMaxItems, InCellSize, InGapX, InGapY)
{
    Target = NULL;
}
NW_HOOK_CTOR_ARGS(0x10086400, NCDeliverItemWnd, (INT A, INT B, INT C, INT D, INT E, INT F), (A, B, C, D, E, F))

// 0x100863b0 - the base's (ConditionalDestroy, then NCItemWnd's)
NW_HOOK_DTOR(0x100863b0, NCDeliverItemWnd)


// 0x10102860 (vslot 25) - the stock body is shared with NCShopItemWnd::WindowProc, which keeps the address (hooked there): NCItemWnd's, then
// the double click (slot 81) and the dialog box's answer (slot 82). Hook build: the slot jumps to the stock address, which our hook owns
// (vt_slots proves a stub, not a longer body written on the class); the standalone has the body
#if NW_HOOK_MODE
NW_NAKED INT NCDeliverItemWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam) NW_STOCK(0x10102860)
#else
INT NCDeliverItemWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
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

// stock body shared with NCShopItemWnd::OnDestroy (0x100c46f0), which keeps the address
INT NCDeliverItemWnd::OnDestroy()
{
    NCItemWnd::OnDestroy();
    return 1;
}

// 0x10084bd0 (ret 4; the db names it NCDeliverWnd::SetTargetID) - the grid its items go to. One body for all the item
// grids with a target at +0x168 (NCManorShopItemWnd.cpp and NCRecipeShopItemWnd.cpp keep NW_STOCK stubs of the address);
// hooked here, in the lane the db gives the function to
void NCDeliverItemWnd::SetTarget(NCDeliverItemWnd* InTarget)
{
    Target = InTarget;
}
NW_HOOK_METHOD(0x10084bd0, NCDeliverItemWnd, SetTarget)


// 0x10085fd0 (vslot 63) - an item let go over the other grid (the one under the mouse becomes the target) is moved: one
// at a time, or asked how many for a stack of 2 or more
INT NCDeliverItemWnd::OnLButtonUp(WPARAM wParam, LPARAM lParam)
{
    if (Selected > -1 && Selected < Items.Num() && bDragging)
    {
        INT X = LOWORD(lParam);
        INT Y = HIWORD(lParam);
        if (!IsInWindow(X, Y))
        {
            NWndEntry* Found = FindWindowAt(X, Y);
            if (Found && Found->Wnd && Found->Wnd->IsA(NCDeliverItemWnd::StaticClass()))
            {
                Target = (NCDeliverItemWnd*)Found->Wnd;
                NItemEntry* Item = Items(Selected);
                if (!Item->IsStackable() || AsC4(Item)->TradeCount < 2)
                    MoveItem(Item, 1);
                else
                    AskAmount(this, Item);
            }
        }
    }
    bDragging = 0;
    return 1;
}
NW_HOOK_METHOD(0x10085fd0, NCDeliverItemWnd, OnLButtonUp)

// 0x100861a0 (vslot 75) - a double click moves the item under the mouse the same way (to the target it had)
INT NCDeliverItemWnd::OnLButtonDblClick(WPARAM wParam, LPARAM lParam)
{
    bNoDrag = 1;
    Selected = HitTest(LOWORD(lParam), HIWORD(lParam), 0, 0);
    if (Selected > -1 && Selected < Items.Num())
    {
        NItemEntry* Item = Items(Selected);
        if (!Item->IsStackable() || AsC4(Item)->TradeCount < 2)
            MoveItem(Item, 1);
        else
            AskAmount(this, Item);
    }
    bDragging = 0;
    return 1;
}
NW_HOOK_METHOD(0x100861a0, NCDeliverItemWnd, OnLButtonDblClick)

// 0x10086310 (vslot 76) - the amount typed (the stack at most) of the selected item is moved
INT NCDeliverItemWnd::OnDialogResult(WPARAM wParam, LPARAM lParam)
{
    NItemEntry* Item = Items(Selected);
    if (Item && wParam != 0)
    {
        if (AsC4(Item)->TradeCount <= (SQWORD)(INT)wParam)
            wParam = (WPARAM)(DWORD)AsC4(Item)->TradeCount;
        MoveItem(Item, (INT)wParam);
    }
    return 1;
}
NW_HOOK_METHOD(0x10086310, NCDeliverItemWnd, OnDialogResult)

// 0x100856b0 (ret 4) - an item of the package window's grid. The body is the console lane's (NConsoleWndMisc.cpp hooks it as
// NCDeliverItemWndExt::AddItem, a stand-in that is the same object; declared in NConsoleFns.h): this is the method's definition, which
// forwards to it
UBOOL NCDeliverItemWnd::AddItem(NItemInfo* Info)
{
    return ((NCDeliverItemWndExt*)this)->AddItem(Info);
}

// 0x100857c0 (ret 8; called by address) - a copy of Src with Count as its amount (0 when the grid is full): the entry
// made by the stock constructor, its tool tip filled for NTT_Item, the scroll range updated
UBOOL NCDeliverItemWnd::AddCopy(NItemEntry* Src, INT Count)
{
    if (Items.Num() >= MaxItems)
        return 0;
    NItemEntry* Item = (NItemEntry*)appMalloc(sizeof(NItemEntry), TEXT("new"));
    if (Item)
        Item = new ((void*)Item) NItemEntry;
    Item->Copy(Src);
    AsC4(Item)->TradeCount = Count;
    AsC4(Item)->ToolTip.Fill(Item, 2, 0);
    Items.AddItem(Item);
    UpdateScrollRange();
    return 1;
}
NW_HOOK_METHOD(0x100857c0, NCDeliverItemWnd, AddCopy)

// 0x100858a0 - Count of Item goes to the target grid (to the entry of the same item when it is a stack, else a copy), and
// leaves this one (the entry when it was the last, else its amount less Count); the fee follows
void NCDeliverItemWnd::MoveItem(NItemEntry* Item, INT Count)
{
    if (!Target)
        return;
    UnselectAll();
    UBOOL bStacked = 0;
    if (Item->IsStackable())
    {
        for (INT i = 0; i < Target->Items.Num(); i++)
        {
            if (AsC4(Target->Items(i))->ID == AsC4(Item)->ID)
            {
                AsC4(Target->Items(i))->TradeCount += Count;
                AsC4(Target->Items(i))->ToolTip.Fill(Target->Items(i), 2, 0);
                bStacked = 1;
                break;
            }
        }
    }
    if (!bStacked)
        Target->AddCopy(Item, Count);
    if (Item->IsStackable() && AsC4(Item)->TradeCount > (SQWORD)Count)
    {
        AsC4(Item)->TradeCount -= Count;
        AsC4(Item)->ToolTip.Fill(Item, 2, 0);
    }
    else
        Items.Remove(Selected, 1);
    DeliverWnd()->UpdateFee();
    Selected = -1;
    UpdateScrollRange();
}
NW_HOOK_METHOD(0x100858a0, NCDeliverItemWnd, MoveItem)

NW_CHECK_VTABLE(NCDeliverItemWnd, 0x101b30a8, 83)

/*-----------------------------------------------------------------------------
    NCDeliverWnd.
-----------------------------------------------------------------------------*/

// 0x10086440
NCDeliverWnd::NCDeliverWnd()
{
    InventoryItems = NULL;
    SendItems = NULL;
    BackTex = NULL;
    UpButton = NULL;
    DownButton = NULL;
    OKButton = NULL;
    CancelButton = NULL;
    UnknownB0 = NULL;
    Adena = 0;
    Fee = 0;
    TargetID = -1;
}
NW_HOOK_CTOR(0x10086440, NCDeliverWnd)

// 0x100864d0
NW_HOOK_DTOR(0x100864d0, NCDeliverWnd)

// 0x101028f0 (vslot 25) - the buttons (nothing sends NID_STORE_CLOSE): the stock body is shared with NCStoreWnd's and NCShopWnd's and
// keeps the address, which NCStoreWnd::WindowProc hooks (OnOK / OnCancel / OnUp / OnDown / OnClose through the slots 74 .. 78, as the stock does).
// Hook build: the slot jumps to the stock address, which our hook owns; the standalone has the body
#if NW_HOOK_MODE
NW_NAKED INT NCDeliverWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam) NW_STOCK(0x101028f0)
#else
INT NCDeliverWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCWnd::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND)
    {
        if (wParam == NID_STORE_OK)
            return OnOK() != 0;
        if (wParam == NID_STORE_CANCEL)
            return OnCancel() != 0;
        if (wParam == NID_STORE_UP)
            return ::nw::VirtualCall<INT>(this, 76) != 0;
        if (wParam == NID_STORE_DOWN)
            return ::nw::VirtualCall<INT>(this, 77) != 0;
        if (wParam == NID_STORE_CLOSE)
            return ::nw::VirtualCall<INT>(this, 78) != 0;
    }
    return 0;
}
#endif

// 0x10086520 (vslot 37) - the base's, the back, the title bar as wide as the window, the two grids (6 columns of 32 pixel
// cells, 3 rows below and 4 above, 5 / 3 apart; each the other's target), the arrows, OK and Cancel
INT NCDeliverWnd::OnCreate()
{
    NCWnd::OnCreate();
    BackTex = FindTexture(TEXT("L2UI_CH3.StoreWnd.Store1_back"));

    FrameCtrl = new NCFrameCtrl(1, 0, 0);
    FrameID = NID_STORE_FRAME;
    FrameCtrl->ID = NID_STORE_FRAME;
    FrameCtrl->SetPosition(0, 0, (INT)Width, 20);
    CreateChild(FrameCtrl, NWS_VISIBLE);

    SendItems = new NCDeliverItemWnd(6, 3, 160, 32, 5, 3);
    SendItems->SetPosition(9, 215, 239, 104);
    CreateChild(SendItems, NWS_VISIBLE | NWS_UNKNOWN20);

    InventoryItems = new NCDeliverItemWnd(6, 4, 160, 32, 5, 3);
    InventoryItems->SetPosition(9, 48, 239, 139);
    CreateChild(InventoryItems, NWS_VISIBLE | NWS_UNKNOWN20);

    SendItems->SetTarget(InventoryItems);
    InventoryItems->SetTarget(SendItems);

    UpButton = new NCButton(TEXT("L2UI_CH3.Button.UpButton"), TEXT("L2UI_CH3.Button.UpButton"), 0);
    UpButton->ID = NID_STORE_UP;
    UpButton->SetPosition(112, 194, 15, 15);
    CreateChild(UpButton, NWS_VISIBLE);

    DownButton = new NCButton(TEXT("L2UI_CH3.Button.DownButton"), TEXT("L2UI_CH3.Button.DownButton"), 0);
    DownButton->ID = NID_STORE_DOWN;
    DownButton->SetPosition(130, 194, 15, 15);
    CreateChild(DownButton, NWS_VISIBLE);

    OKButton = new NCButton(TEXT("L2UI_CH3.Button.Btn1_Normal"), TEXT("L2UI_CH3.Button.Btn1_NormalOn"), 0);
    OKButton->ID = NID_STORE_OK;
    OKButton->SetPosition(51, 372, 76, 23);
    CreateChild(OKButton, NWS_VISIBLE);
    OKButton->SetTextID(STR_OK);

    CancelButton = new NCButton(TEXT("L2UI_CH3.Button.Btn1_Normal"), TEXT("L2UI_CH3.Button.Btn1_NormalOn"), 0);
    CancelButton->ID = NID_STORE_CANCEL;
    CancelButton->SetPosition(131, 372, 76, 23);
    CreateChild(CancelButton, NWS_VISIBLE);
    CancelButton->SetTextID(STR_Cancel);
    return 1;
}
NW_HOOK_METHOD(0x10086520, NCDeliverWnd, OnCreate)

// 0x100852a0 (vslot 38) - the grids, the arrows and the buttons go (and the window of +0xE4, never created), then the base
INT NCDeliverWnd::OnDestroy()
{
    DestroyChild(this, InventoryItems);
    DestroyChild(this, SendItems);
    DestroyChild(this, UpButton);
    DestroyChild(this, DownButton);
    DestroyChild(this, OKButton);
    DestroyChild(this, CancelButton);
    DestroyChild(this, UnknownB0);
    NCWnd::OnDestroy();
    return 1;
}
NW_HOOK_METHOD(0x100852a0, NCDeliverWnd, OnDestroy)

// 0x10084c40 (vslot 48) - opens emptied (the console fills it after)
void NCDeliverWnd::ShowWindow()
{
    Clear();
    NCWnd::ShowWindow();
}
NW_HOOK_METHOD(0x10084c40, NCDeliverWnd, ShowWindow)

// 0x10084c50 (vslot 49)
void NCDeliverWnd::HideWindow()
{
    Clear();
    NCWnd::HideWindow();
}
NW_HOOK_METHOD(0x10084c50, NCDeliverWnd, HideWindow)

// 0x10084f10 (vslot 56) - the back below the title bar, the headings, and the fee and the adena right-aligned at x = 156
// (the labels) and x = 247 (the numbers, with their separators)
INT NCDeliverWnd::OnPaint(NCGDevice* Device)
{
    guard(NCDeliverWnd::OnPaint);
    TCHAR Text[132];
    TCHAR FeeText[128];
    TCHAR AdenaText[128];
    SIZE Size;
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    if (BackTex)
    {
        INT H = (INT)(Height - 20.0f);
        INT W = (INT)Width;
        Device->DrawTexture(0, 20, W, H, 0, 0, W, H, BackTex, 0xFF, 1);
    }
    Device->DrawText(11, 32, 0xFFDCDCDC, *SysString(STR_Inventory), 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
    Device->DrawText(11, 198, 0xFFDCDCDC, *SysString(STR_SendingList), 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
    NCGDevice::GetTextExtent(&Size, *SysString(STR_SendingFee), 0, 0, 0, 0, 0);
    Device->DrawText(156 - Size.cx, 331, 0xFFDCDCDC, *SysString(STR_SendingFee), 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
    NCGDevice::GetTextExtent(&Size, *SysString(STR_Adena), 0, 0, 0, 0, 0);
    Device->DrawText(156 - Size.cx, 350, 0xFFDCDCDC, *SysString(STR_Adena), 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
    appSprintf(Text, TEXT("%d"), Fee);
    FormatNumber(FeeText, Text);
    appSprintf(Text, TEXT("%s"), FeeText);
    NCGDevice::GetTextExtent(&Size, Text, 0, 0, 0, 0, 0);
    Device->DrawText(247 - Size.cx, 331, 0xFFDCDCDC, Text, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
    appSprintf(Text, TEXT("%d"), Adena);
    FormatNumber(AdenaText, Text);
    appSprintf(Text, TEXT("%s"), AdenaText);
    NCGDevice::GetTextExtent(&Size, Text, 0, 0, 0, 0, 0);
    Device->DrawText(247 - Size.cx, 350, 0xFFDCDCDC, Text, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10084f10, NCDeliverWnd, OnPaint)

// 0x100853f0 (vslot 68) - the list sent to the server: the character, how many entries have an amount, and for each its
// first field and its amount; then the window closes
INT NCDeliverWnd::OnOK()
{
    INT Count = 0;
    for (INT i = 0; i < SendItems->Items.Num(); i++)
    {
        NItemEntry* Item = SendItems->Items(i);
        if (AsC4(Item)->TradeCount > 0)
            Count++;
    }
    L2ParamStack Params(Count * 2 + 2);
    Params.PushBack((void*)(INT_PTR)TargetID);
    Params.PushBack((void*)(INT_PTR)Count);
    for (INT i = 0; i < SendItems->Items.Num(); i++)
    {
        NItemEntry* Item = SendItems->Items(i);
        if (AsC4(Item)->TradeCount > 0)
        {
            Params.PushBack((void*)(INT_PTR)AsC4(Item)->UnknownC34);
            Params.PushBack((void*)(INT_PTR)(INT)AsC4(SendItems->Items(i))->TradeCount);
        }
    }
    GConsole->RequestPackageSend(Params);
    HideWindow();
    return 1;
}
NW_HOOK_METHOD(0x100853f0, NCDeliverWnd, OnOK)

// 0x10084c60 (vslot 75)
INT NCDeliverWnd::OnCancel()
{
    Clear();
    HideWindow();
    return 1;
}
NW_HOOK_METHOD(0x10084c60, NCDeliverWnd, OnCancel)

// 0x10084cb0 (vslot 78) - the close of the window: emptied, hidden, with the window close sound
INT NCDeliverWnd::OnClose()
{
    Clear();
    HideWindow();
    GConsole->PlayInterfaceSound(SOUND_WindowClose);
    return 1;
}
NW_HOOK_METHOD(0x10084cb0, NCDeliverWnd, OnClose)

// 0x10085a00 (vslot 70) - the up arrow: the selected item of the lower grid goes up, one at a time or asked for a stack
INT NCDeliverWnd::OnUp()
{
    INT Sel = SendItems->GetSelected();
    if (Sel > -1 && Sel < SendItems->Items.Num())
    {
        NItemEntry* Item = SendItems->Items(Sel);
        if (Item)
        {
            if (!Item->IsStackable() || AsC4(Item)->TradeCount < 2)
                SendItems->MoveItem(Item, 1);
            else
                AskAmount(SendItems, Item);
        }
    }
    return 1;
}
NW_HOOK_METHOD(0x10085a00, NCDeliverWnd, OnUp)

// 0x10085b70 (vslot 71) - the down arrow: the same for the upper grid
INT NCDeliverWnd::OnDown()
{
    INT Sel = InventoryItems->GetSelected();
    if (Sel > -1 && Sel < InventoryItems->Items.Num())
    {
        NItemEntry* Item = InventoryItems->Items(Sel);
        if (Item)
        {
            if (!Item->IsStackable() || AsC4(Item)->TradeCount < 2)
                InventoryItems->MoveItem(Item, 1);
            else
                AskAmount(InventoryItems, Item);
        }
    }
    return 1;
}
NW_HOOK_METHOD(0x10085b70, NCDeliverWnd, OnDown)

// 0x100deca0 - the player's adena (the console sets it before the list); the stock body is one with
// NCRecipeBuyListWnd::SetAdena's (hooked there, which keeps the address)
void NCDeliverWnd::SetAdena(INT InAdena)
{
    Adena = InAdena;
}

// the character the package goes to (+0x110). The stock caller (NConsoleWnd::AddSendableItemList 0x1007AA10) calls 0x10084BF0, a 13-byte
// setter of +0x110 (`mov [ecx + 0x110], eax`) shared with other windows; the stub that stood here jumped to 0x10084BD0, which is
// NCDeliverItemWnd::SetTarget (`mov [ecx + 0x168], eax`, the grid's target): a store past the end of this window
void NCDeliverWnd::SetTargetID(INT InTargetID)
{
    TargetID = InTargetID;
}


// 0x10084c00 - kept out of line like the stock code (ShowWindow, HideWindow, OnCancel)
__declspec(noinline) void NCDeliverWnd::Clear()
{
    TargetID = -1;
    Adena = 0;
    Fee = 0;
    InventoryItems->Clear();
    SendItems->Clear();
}
NW_HOOK_METHOD(0x10084c00, NCDeliverWnd, Clear)

// 0x10084c80 - 1000 adena for each slot sent
void NCDeliverWnd::UpdateFee()
{
    Fee = 0;
    Fee = SendItems->Items.Num() * DELIVER_FEE_PER_SLOT;
}
NW_HOOK_METHOD(0x10084c80, NCDeliverWnd, UpdateFee)

// 0x100859f0 (ret 4) - an item of the player's inventory (the grid on top). The body is the console lane's (NConsoleWndMisc.cpp hooks it as
// NCDeliverWndExt::AddInventoryItem, a stand-in that is the same object; declared in NConsoleFns.h): this is the method's definition,
// which forwards to it
UBOOL NCDeliverWnd::AddInventoryItem(NItemInfo* Info)
{
    return ((NCDeliverWndExt*)this)->AddInventoryItem(Info);
}

NW_CHECK_VTABLE(NCDeliverWnd, 0x101b31f8, 79)

/*-----------------------------------------------------------------------------
    NCSelectDeliverWnd.
-----------------------------------------------------------------------------*/

// 0x10086a40 - the targets are left as allocated
NCSelectDeliverWnd::NCSelectDeliverWnd()
{
    NumTargets = 0;
    Selected = -1;
}
NW_HOOK_CTOR(0x10086a40, NCSelectDeliverWnd)

// 0x10086aa0
NW_HOOK_DTOR(0x10086aa0, NCSelectDeliverWnd)

// 0x10084ce0 (vslot 25) - the buttons and the combo box
INT NCSelectDeliverWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCWnd::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND)
    {
        if (wParam == NID_SELDELIVER_OK)
            return OnOK() != 0;
        if (wParam == NID_SELDELIVER_CANCEL)
            return OnCancel() != 0;
        if (wParam == NID_SELDELIVER_TARGETS)
            return OnSelect(lParam) != 0;
    }
    return 0;
}
NW_HOOK_METHOD(0x10084ce0, NCSelectDeliverWnd, WindowProc)

// 0x10085ce0 (vslot 37) - the base's, the title bar as wide as the window, the combo box of the characters, OK and
// Cancel, the back
INT NCSelectDeliverWnd::OnCreate()
{
    NCWnd::OnCreate();
    FrameCtrl = new NCFrameCtrl(1, 0, 0);
    FrameCtrl->ID = NID_SELDELIVER_FRAME;
    FrameCtrl->SetPosition(0, 0, (INT)Width, 20);
    CreateChild(FrameCtrl, NWS_VISIBLE);

    TargetCombo = new NCComboBox;
    TargetCombo->ID = NID_SELDELIVER_TARGETS;
    TargetCombo->SetPosition(39, 61, 178, 17);
    CreateChild(TargetCombo, NWS_VISIBLE);

    OKButton = new NCButton(TEXT("L2UI_ch3.button.btn1_normal"), TEXT("L2UI_ch3.button.btn1_normalon"), 0);
    OKButton->ID = NID_SELDELIVER_OK;
    OKButton->SetPosition(51, 172, 76, 23);
    CreateChild(OKButton, NWS_VISIBLE);
    OKButton->SetTextID(STR_OK);

    CancelButton = new NCButton(TEXT("L2UI_ch3.button.btn1_normal"), TEXT("L2UI_ch3.button.btn1_normalon"), 0);
    CancelButton->ID = NID_SELDELIVER_CANCEL;
    CancelButton->SetPosition(131, 172, 76, 23);
    CreateChild(CancelButton, NWS_VISIBLE);
    CancelButton->SetTextID(STR_Cancel);

    BackTex = FindTexture(TEXT("L2UI_ch3.DeliverWnd.Warehouse_back"));
    return 1;
}
NW_HOOK_METHOD(0x10085ce0, NCSelectDeliverWnd, OnCreate)

// 0x10085610 (vslot 38) - the combo box and the buttons go, then the base
INT NCSelectDeliverWnd::OnDestroy()
{
    DestroyChild(this, TargetCombo);
    DestroyChild(this, CancelButton);
    DestroyChild(this, OKButton);
    NCWnd::OnDestroy();
    return 1;
}
NW_HOOK_METHOD(0x10085610, NCSelectDeliverWnd, OnDestroy)

// 0x10084e20 (vslot 48) - opens emptied (the console adds the characters after)
void NCSelectDeliverWnd::ShowWindow()
{
    Clear();
    NCWnd::ShowWindow();
}
NW_HOOK_METHOD(0x10084e20, NCSelectDeliverWnd, ShowWindow)

// 0x10084e30 (vslot 49)
void NCSelectDeliverWnd::HideWindow()
{
    Clear();
    NCWnd::HideWindow();
}
NW_HOOK_METHOD(0x10084e30, NCSelectDeliverWnd, HideWindow)

// 0x10085540 (vslot 56) - the back stretched below the title bar and the heading ("Please choose a person to receive."); no
// guard frame
INT NCSelectDeliverWnd::OnPaint(NCGDevice* Device)
{
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    Device->BeginPaint(ScreenX, ScreenY);
    if (BackTex)
    {
        INT H = (INT)(Height - 20.0f);
        INT W = (INT)Width;
        Device->DrawStretchedTex(0, 20, W, H, 0, 0, W, H, BackTex, 0xFF);
    }
    Device->DrawText(11, 32, 0xFFDCDCDC, *GL2GameData.SystemMsg[MSG_ChooseReceiver].Message, 0, 0, 0, L2FONT_Normal, 0, 0,
                     0, 0, 0, 0);
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
}
NW_HOOK_METHOD(0x10085540, NCSelectDeliverWnd, OnPaint)

// 0x10084ef0 (vslot 74) - the combo box's choice
INT NCSelectDeliverWnd::OnSelect(LPARAM Index)
{
    Selected = (INT)Index;
    return 1;
}
NW_HOOK_METHOD(0x10084ef0, NCSelectDeliverWnd, OnSelect)

// 0x10084e40 (vslot 75) - with a character chosen, the server is asked what can be sent to it and the window closes
INT NCSelectDeliverWnd::OnOK()
{
    if (Selected >= 0)
    {
        L2ParamStack Params(1);
        Params.PushBack((void*)(INT_PTR)Targets[Selected].ID);
        GConsole->RequestPackageSendableItemList(Params);
        HideWindow();
    }
    return 1;
}
NW_HOOK_METHOD(0x10084e40, NCSelectDeliverWnd, OnOK)

// stock body shared with NCOptionWnd's (0x10059d00)
INT NCSelectDeliverWnd::OnCancel()
{
    HideWindow();
    return 1;
}

// 0x10084d80 - 8 at most; the rest are dropped (the name buffer holds 24 characters: a longer one overruns it)
void NCSelectDeliverWnd::AddTarget(INT ID, const TCHAR* Name)
{
    if (NumTargets >= MAX_DELIVER_TARGETS)
        return;
    Targets[NumTargets].ID = ID;
    appStrcpy(Targets[NumTargets].Name, Name);
    TargetCombo->AddItem((const TCHAR*)Targets[NumTargets].Name);   // NCComboBox::AddItem(const TCHAR*), 0x100068b0
    NumTargets++;
}
NW_HOOK_METHOD(0x10084d80, NCSelectDeliverWnd, AddTarget)

// 0x10084df0 - kept out of line like the stock code (ShowWindow, HideWindow)
__declspec(noinline) void NCSelectDeliverWnd::Clear()
{
    TargetCombo->ClearItems();
    Selected = -1;
    NumTargets = 0;
}
NW_HOOK_METHOD(0x10084df0, NCSelectDeliverWnd, Clear)

NW_CHECK_VTABLE(NCSelectDeliverWnd, 0x101b3450, 77)
