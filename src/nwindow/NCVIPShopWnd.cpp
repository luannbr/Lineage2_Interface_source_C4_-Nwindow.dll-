/*=============================================================================
    NCVIPShopWnd.cpp - the multisell window (the decompilation's VIP shop) and its pages. C4 stock: 0x1009ffd0 .. 0x100a4280
    (the pages' paints, scrolling and destruction at 0x1009ffd0 .. 0x100a2dc0, the window's OnPaint / OnOK / ShowItem and
    OnCreate at 0x100a1fc0 .. 0x100a3f40, constructors and destructors at 0x100a3560 .. 0x100a39c0), the console lane's
    AddRecord / AddNeededItem / ShowList / Open (NConsoleWndAnon.cpp) and NCNameCtrl's SetItem (0x1002d020).
=============================================================================*/
#include "nwindow/NCVIPShopWnd.h"
#include "nwindow/NCButton.h"
#include "nwindow/NCConsole.h"
#include "nwindow/NCEditBox.h"
#include "nwindow/NCFrameCtrl.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NCGaraDialogBox.h"
#include "nwindow/NCInventoryWnd.h"
#include "nwindow/NCNameCtrl.h"
#include "nwindow/NCSliderBar.h"
#include "nwindow/NClasses.gen.h"
#include "nwindow/NConsoleWnd.h"
#include "nwindow/NConsoleFns.h"
#include "nwindow/NConsoleWndC4.h"
#include "nwindow/NDialogInfo.h"
#include "nwindow/NGameData.h"
#include "nwindow/NLeftHelpers.h"

// sysstring
enum
{
    STR_Store = 136,
    STR_OK = 140,
    STR_Cancel = 141,
    STR_List = 562,
    STR_ItemInformation = 564,
    STR_ItemsRequired = 565,
    STR_Amount = 808,              // the label of the amount's edit line
};

enum { MSG_MultisellAmount = 0x567 };   // systemmsg of the amount's dialog
enum { DIALOG_VIPSHOP_AMOUNT = 0x51 };  // NDialogInfo::ID (name ours): the answer is the purchase

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

// NCFrameCtrl(Decorated, Vertical, Small): 0x100126E0 (ret 0xc), run on the object that `new` allocates
static NCFrameCtrl* NewFrameCtrl(INT A, INT B, INT C)
{
    return new NCFrameCtrl(A, B, C);
}

// NCNameCtrl (UClass 0x102C4EE8, 0x2A4 bytes, constructor 0x1002D260): the control the pages put over a row to show the item's name
// with its tool tip
static NCWnd* NewNameCtrl()
{
    return new NCNameCtrl;
}

// the NItemEntry the stock builds on its stack (the constructor 0x1001F7F0) and the end of its life: the tool tip's members go
// (the rest of an entry is plain data); entries are bytes here because ~NItemEntry is not ported
struct NLocalEntry
{
    BYTE Data[0x1C40];

    NLocalEntry() { new ((void*)Data) NItemEntry; }   // 0x1001F7F0
    ~NLocalEntry() { AsC4((NItemEntry*)Data)->ToolTip.~NToolTipInfo(); }
    NItemEntryC4* operator->() { return (NItemEntryC4*)Data; }
    operator NItemEntry*() { return (NItemEntry*)Data; }
};

typedef TArray<NVIPNeededItem*> TVIPNeededItemArray;

/*-----------------------------------------------------------------------------
    NCNameCtrl::SetItem (0x1002d020) - the name control of a row: set from the parameters of a stack (the name, then for the
    mode 1 the item's id, type, enchant, crystal grade and amount); the mode 0 shows the name in the tool tip, the mode 1 fills
    it with an item entry made of the parameters
-----------------------------------------------------------------------------*/

struct NCNameCtrlView
{
    BYTE _Unknown000[0x1F4];
    INT Mode;                  // +0x1F4
    INT Flag;                  // +0x1F8
    FString Name;              // +0x1FC
    INT ItemID;                // +0x208 (mode 1)
    INT Type;                  // +0x20C
    INT Enchant;               // +0x210
    INT Grade;                 // +0x214
    INT Count;                 // +0x218
    BYTE _Unknown21C[4];
    NToolTipInfo ToolTip;      // +0x220

    __declspec(noinline) void SetItem(L2ParamStack* Params, INT InMode, INT InFlag);   // 0x1002D020 (ret 0xC)
};
NW_ASSERT_OFFSET(NCNameCtrlView, Mode, 0x1F4);
NW_ASSERT_OFFSET(NCNameCtrlView, Name, 0x1FC);
NW_ASSERT_OFFSET(NCNameCtrlView, ItemID, 0x208);
NW_ASSERT_OFFSET(NCNameCtrlView, ToolTip, 0x220);

void NCNameCtrlView::SetItem(L2ParamStack* Params, INT InMode, INT InFlag)
{
    Flag = InFlag;
    Mode = InMode;
    Name = (const TCHAR*)Params->Top();
    if (Mode == 0)
        ToolTip.SetText(*Name, 0x400);
    else if (Mode == 1)
    {
        ItemID = (INT)(INT_PTR)Params->Top();
        Type = (INT)(INT_PTR)Params->Top();
        Enchant = (INT)(INT_PTR)Params->Top();
        Grade = (INT)(INT_PTR)Params->Top();
        Count = (INT)(INT_PTR)Params->Top();
        NLocalEntry Entry;
        appStrcpy(Entry->Name, *Name);
        Entry->Type = Type;
        Entry->Enchant = Enchant;
        Entry->ID = ItemID;
        Entry->Count = Count;
        Entry->CrystalGrade = Grade;
        ToolTip.Fill(Entry, 2, 0);
    }
}
NW_HOOK_METHOD(0x1002d020, NCNameCtrlView, SetItem)

/*-----------------------------------------------------------------------------
    NVIPRecord and the arrays of the needed items
-----------------------------------------------------------------------------*/

// 0x100a2cb0 - the destructor of the array of needed items (the pointers are freed by the owner): the elements go, then the
// array's memory
NW_HOOK_DTOR(0x100a2cb0, TVIPNeededItemArray)

// 0x100a3200 - a jump into the destructor above for the record's array
void NVIPRecord::DestroyNeeded()
{
    Needed.~TVIPNeededItemArray();
}
NW_HOOK_METHOD(0x100a3200, NVIPRecord, DestroyNeeded)

/*-----------------------------------------------------------------------------
    NCVIPShopItemWnd - the grid of the products
-----------------------------------------------------------------------------*/

// 0x100a3560 - the plus icon's texture; the rest is NCItemWnd's
NCVIPShopItemWnd::NCVIPShopItemWnd()
{
    PlusTex = FindTexture(TEXT("L2UI_CH3.MultiSellWnd.Multisell_plusicon"));
}
NW_HOOK_CTOR(0x100a3560, NCVIPShopItemWnd)

// 0x100a3640 (ret 0x18) - the grid's size, then the plus icon's texture
NCVIPShopItemWnd::NCVIPShopItemWnd(INT InColumns, INT InVisibleRows, INT InMaxItems, INT InCellSize, INT InGapX, INT InGapY)
    : NCItemWnd(InColumns, InVisibleRows, InMaxItems, InCellSize, InGapX, InGapY)
{
    PlusTex = FindTexture(TEXT("L2UI_CH3.MultiSellWnd.Multisell_plusicon"));
}
NW_HOOK_CTOR_ARGS(0x100a3640, NCVIPShopItemWnd, (INT A, INT B, INT C, INT D, INT E, INT F), (A, B, C, D, E, F))

// 0x100a35f0 - the members' destructors are NCItemWnd's
NW_HOOK_DTOR(0x100a35f0, NCVIPShopItemWnd)

// 0x100a1870 (vslot 62; the guard's name is the class's own) - the first item's icon of each product from the page's first row,
// the selection frame behind a selected one and the plus icon over the products that give several items (the shop's records
// are NCVIPShopWnd::Records, the desktop's +0x140)
INT NCVIPShopItemWnd::OnPaint(NCGDevice* Device)
{
    guard(NCVIPShopItemWnd::OnPaint);
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
                Y = Y + (CellGapY + CellSize);
            if ((CellSize + 1) * VisibleRows <= Y)
                break;
        }
        else
            X = X + (CellGapX + CellSize);
        if (Item->IsSelected())
            Device->DrawTexture(X - 1, Y - 1, 0x22, 0x22, 0, 0, 0x22, 0x22, SelectTex, 0xFF, 1);
        if (Item->GetIcon())
            Device->DrawTexture(X, Y, CellSize, CellSize, 0, 0, CellSize, CellSize, Item->GetIcon(), 0xFF, 1);
        if (GDesktop->VIPShopWnd->Records(Index)->NumItems > 1)
            Device->DrawTexture(X, Y, CellSize, CellSize, 0, 0, CellSize, CellSize, PlusTex, 0xFF, 1);
    }
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100a1870, NCVIPShopItemWnd, OnPaint)

// 0x100a36c0 (vslot 68) - a click on a product selects it (the amount's dialog of another product goes away) and shows it on
// the other pages
INT NCVIPShopItemWnd::OnLButtonDown(WPARAM wParam, LPARAM lParam)
{
    NCGaraDialogBox* Box = (NCGaraDialogBox*)GDesktop->MainDialogBox;
    if (Box->IsShowWindow())
    {
        Box = (NCGaraDialogBox*)GDesktop->MainDialogBox;
        if (Box->DialogID == DIALOG_VIPSHOP_AMOUNT)
            Box->HideWindow();
    }
    INT Index = HitTest(LOWORD(lParam), HIWORD(lParam), 0, 0);
    Selected = Index;
    HoverIndex = Index;
    if (Index >= 0 && Index < Items.Num())
    {
        UnselectAll();
        Items(Selected)->Select();
        NCVIPShopWnd* Shop = (NCVIPShopWnd*)Entry->Parent->Wnd;
        Shop->Selected = Selected;
        Shop->ShowItem();
        bDragging = 0;
    }
    return 1;
}
NW_HOOK_METHOD(0x100a36c0, NCVIPShopItemWnd, OnLButtonDown)

NW_CHECK_VTABLE(NCVIPShopItemWnd, 0x101b8f80, 81)

/*-----------------------------------------------------------------------------
    NCVIPShopItemInfoWnd
-----------------------------------------------------------------------------*/

// the entries one by one with the stock's constructor, and back to front with the tool tip's destructor
NVIPEntryArray::NVIPEntryArray()
{
    for (INT i = 0; i < 50; i++)
        new ((void*)(Data + i * 0x1C40)) NItemEntry;   // 0x1001F7F0
}
NVIPEntryArray::~NVIPEntryArray()
{
    for (INT i = 49; i >= 0; i--)
        AsC4((NItemEntry*)(Data + i * 0x1C40))->ToolTip.~NToolTipInfo();
}

// 0x100a3850 - the scroll window's constructor, the 50 entries (NVIPEntryArray), none in use
NCVIPShopItemInfoWnd::NCVIPShopItemInfoWnd()
{
    NumEntries = 0;
}
NW_HOOK_CTOR(0x100a3850, NCVIPShopItemInfoWnd)

// 0x100a38f0 - the entries' tool tips go (back to front), then the scroll window
NW_HOOK_DTOR(0x100a38f0, NCVIPShopItemInfoWnd)

// stock body shared with NCItemWnd's (0x10093f30, hooked there): the base's answer as a plain 0 / 1
INT NCVIPShopItemInfoWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return NCScrollWnd::WindowProc(Msg, wParam, lParam) != 0;
}

// 0x100a00f0 (vslot 37) - the frame texture of the icons; the row controls start empty
INT NCVIPShopItemInfoWnd::OnCreate()
{
    NCScrollWnd::OnCreate();
    IconBoxTex = FindTexture(TEXT("L2ui_ch3.etc.iconbox"));
    for (INT i = 0; i < 50; i++)
        NameCtrls[i] = NULL;
    return 1;
}
NW_HOOK_METHOD(0x100a00f0, NCVIPShopItemInfoWnd, OnCreate)

// 0x100a2640 (vslot 38)
INT NCVIPShopItemInfoWnd::OnDestroy()
{
    guard(NCVIPShopItemInfoWnd::OnDestroy);
    NCScrollWnd::OnDestroy();
    Clear();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100a2640, NCVIPShopItemInfoWnd, OnDestroy)

// 0x100a0130 (vslot 48) - shown at the top of the text (scroll position and bar reset)
void NCVIPShopItemInfoWnd::ShowWindow()
{
    ScrollX = 0;
    ScrollY = 0;
    ScrollBar->SetValue(0);
    NCWnd::ShowWindow();
}
NW_HOOK_METHOD(0x100a0130, NCVIPShopItemInfoWnd, ShowWindow)

// 0x100a1f50 - the entries reset, the name controls destroyed, nothing in use
void NCVIPShopItemInfoWnd::Clear()
{
    for (INT i = 0; i < 50; i++)
        EntryAt(i)->Reset();
    for (INT i = 0; i < 50; i++)
        DestroyChild(this, NameCtrls[i]);
    NumEntries = 0;
}
NW_HOOK_METHOD(0x100a1f50, NCVIPShopItemInfoWnd, Clear)

// 0x100a1e00 (ret 8) - the name controls follow the scroll (DX, -DY) and the ones that are no longer in the window are hidden
void NCVIPShopItemInfoWnd::MoveNames(INT DX, INT DY)
{
    for (INT i = 0; i < 50; i++)
    {
        NCWnd* Name = NameCtrls[i];
        if (Name)
        {
            Name->MoveWindow(DX, -DY);
            NRectF Rect;
            Rect.X = 0;
            Rect.Y = 0;
            Rect.W = 0;
            Rect.H = 0;
            Name->GetLocalRect(&Rect);
            if (Rect.Y > (INT)Height || Rect.Y + (INT)Rect.H < 0)
                Name->HideWindow();
            else
                Name->ShowWindow();
        }
    }
}
NW_HOOK_METHOD(0x100a1e00, NCVIPShopItemInfoWnd, MoveNames)

// the four virtuals of the scrolling (vslots 73, 75, 76, 77; the stock's own overrides of NCScrollWnd's): the scroll window's step
// (a direct call of NCScrollWnd's, 0x1002BAC0 / 0x1002BAD0 ... in the stock), then the name controls follow it by DeltaY
// (+0xF4); all four return 1
// 0x100a1eb0
INT NCVIPShopItemInfoWnd::ScrollUp()
{
    NCScrollWnd::ScrollUp();
    MoveNames(0, DeltaY);
    return 1;
}
NW_HOOK_METHOD(0x100a1eb0, NCVIPShopItemInfoWnd, ScrollUp)
// 0x100a1ed0
INT NCVIPShopItemInfoWnd::ScrollDown()
{
    NCScrollWnd::ScrollDown();
    MoveNames(0, DeltaY);
    return 1;
}
NW_HOOK_METHOD(0x100a1ed0, NCVIPShopItemInfoWnd, ScrollDown)
// 0x100a1ef0 (ret 4)
INT NCVIPShopItemInfoWnd::OnScrollBar(INT Value)
{
    NCScrollWnd::OnScrollBar(Value);
    MoveNames(0, DeltaY);
    return 1;
}
NW_HOOK_METHOD(0x100a1ef0, NCVIPShopItemInfoWnd, OnScrollBar)
// 0x100a1f20 (ret 8)
INT NCVIPShopItemInfoWnd::OnMouseWheel(WPARAM wParam, LPARAM lParam)
{
    NCScrollWnd::OnMouseWheel(wParam, lParam);
    MoveNames(0, DeltaY);
    return 1;
}
NW_HOOK_METHOD(0x100a1f20, NCVIPShopItemInfoWnd, OnMouseWheel)

// 0x100a1c50 (vslot 62; the guard's name is the class's own) - one item in full, several as a column of icons (the names are
// the controls over them); the height of the contents sizes the scroll range
INT NCVIPShopItemInfoWnd::OnPaint(NCGDevice* Device)
{
    guard(NCVIPShopItemInfoWnd::OnPaint);
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    Device->PushOrigin(ScrollX, ScrollY);
    if (NumEntries == 1)
        SetContentHeight(DrawItemInfo(Device, EntryAt(0), 10, 4));
    else if (NumEntries > 1)
    {
        Device->PushOrigin(ScrollX, ScrollY);
        INT i;
        for (i = 0; i < NumEntries; i++)
        {
            Device->DrawTexture(10, i * 0x24 + 5, 0x22, 0x22, 0, 0, 0x22, 0x22, IconBoxTex, 0xFF, 1);
            Device->DrawTexture(11, i * 0x24 + 6, 0x20, 0x20, 0, 0, 0x20, 0x20, EntryAt(i)->Icon, 0xFF, 1);
        }
        SetContentHeight(i * 0x24 + 5);
        Device->PopOrigin();
    }
    Device->PopOrigin();
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100a1c50, NCVIPShopItemInfoWnd, OnPaint)

// 0x100a26c0 (ret 8) - the entry Index is Src copied and completed from the item table (nothing more when the item is not in it):
// name, additional name, icon's name, description, and by the item's class the weapon's, the armor's or the etc item's values;
// the type, body part and enchant stay Src's. A name control over the row shows the item's name with its tool tip
void NCVIPShopItemInfoWnd::AddItem(INT Index, NItemEntry* Src)
{
    if (Index >= 50)
        return;
    NumEntries++;
    NItemEntry* Item = EntryAt(Index);
    NItemEntryC4* E = AsC4(Item);
    Item->Copy(Src);
    NViewItemDataC4* Data = (NViewItemDataC4*)NItemTable::Find((INT*)&E->ID);
    if (E->ID < 0 || !Data)
        return;
    INT Class = Data->Class;
    appStrcpy(E->Name, GL2GameData.GetItemName(E->ID));
    if (GConsole->GetItemAdditionalName(E->ID))
        appStrcpy(E->AdditionalName, GConsole->GetItemAdditionalName(E->ID));
    appStrcpy(E->IconName[0], *Data->IconName);
    appStrcpy(E->Description, GL2GameData.GetItemDescription(E->ID));
    E->Type = AsC4(Src)->Type;
    E->BodyPart = AsC4(Src)->BodyPart;
    E->Enchant = AsC4(Src)->Enchant;
    if (Class == ITEMCLASS_C4_Weapon)
    {
        E->Weight = Data->Weight;
        E->UnknownC2C = Data->Unknown48;
        E->UnknownC30 = Data->WeaponType;
        E->Unknown1C80 = Data->PAtk;
        E->Unknown1C84 = Data->MAtk;
        E->Unknown1C90 = Data->ShieldPDef;
        E->Unknown1C94 = Data->ShieldRate;
        E->Unknown1C58 = Data->Unknown40;
        E->CrystalGrade = Data->CrystalGrade;
        E->Unknown1C60 = Data->Unknown90;
        E->Unknown1C64 = Data->UnknownA4;
        E->Unknown1C68 = Data->UnknownA8;
        E->Unknown1C6C = Data->AttackSpeed;
        E->Unknown1C70 = Data->UnknownBC;
        E->Unknown1C78 = Data->Evasion;
        E->Unknown1CA0 = Data->SoulShots;
        E->Unknown1CA4 = Data->SpiritShots;
    }
    else if (Class == ITEMCLASS_C4_Armor)
    {
        E->Weight = Data->Weight;
        E->UnknownC2C = Data->Unknown48;
        E->Unknown1C58 = Data->Unknown40;
        E->CrystalGrade = Data->ArmorCrystalGrade;
        E->SubType = Data->ArmorType;
        E->Unknown1C78 = Data->ArmorEvasion;
        E->Unknown1C88 = Data->PDef;
        E->Unknown1C8C = Data->MDef;
        E->Unknown1C9C = Data->MPBonus;
    }
    else if (Class == ITEMCLASS_C4_Etc)
    {
        E->Weight = Data->Weight;
        E->UnknownC2C = Data->Unknown48;
        E->Unknown1C58 = Data->Unknown40;
        E->bStackable = Data->bStackable;
        if (Data->EtcType == 2 || Data->EtcType == 0x10)
            E->Unknown1C98 = 1;
    }
    NCWnd* Name = NewNameCtrl();
    NameCtrls[Index] = Name;
    Name->SetPosition(0x2E, Index * 0x24 + 10, 0xB4, 0xE);
    Name->AttachEntry(AddChildWindow(Name, 0x22));
    L2ParamStack Params(10);
    Params.PushBack(E->Name);
    Params.PushBack((void*)(INT_PTR)E->ID);
    Params.PushBack((void*)(INT_PTR)E->Type);
    Params.PushBack((void*)(INT_PTR)E->Enchant);
    Params.PushBack((void*)(INT_PTR)E->CrystalGrade);
    Params.PushBack((void*)(INT_PTR)E->Count);
    ((NCNameCtrlView*)Name)->SetItem(&Params, 1, 0);
}
NW_HOOK_METHOD(0x100a26c0, NCVIPShopItemInfoWnd, AddItem)

// the label of a row of the item's information, in the stock's own steps (inline in each row): "label" grey at (X, Y), " : "
// white right after it; returns the X of the value (past "label : ")
static FORCEINLINE INT DrawStatLabel(NCGDevice* Device, INT X, INT Y, INT LabelID, TCHAR* Buf)
{
    SIZE Size;
    appSprintf(Buf, TEXT("%s"), *NCObject::SysString(LabelID));
    Device->DrawText(X, Y, 0xFFA3A3A3, Buf, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
    NCGDevice::GetTextExtent(&Size, Buf, 0, 0, 0, 0, 0, L2FONT_Normal);
    appSprintf(Buf, TEXT(" : "));
    Device->DrawText(Size.cx + X, Y, 0xFFDCDCDC, Buf, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
    appSprintf(Buf, TEXT("%s : "), *NCObject::SysString(LabelID));
    NCGDevice::GetTextExtent(&Size, Buf, 0, 0, 0, 0, 0, L2FONT_Normal);
    return Size.cx + X;
}

// the value of a row, gold, at the X DrawStatLabel gave
static FORCEINLINE void DrawStatValue(NCGDevice* Device, INT X, INT Y, TCHAR* Buf)
{
    Device->DrawText(X, Y, 0xFFB09B79, Buf, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
}

// 0x100a0160 - the item's information page from (X, Y): the icon in its frame, then by the kind of the item (Type 0 weapon, 1 armor,
// 2 jewelry, 3 and 5 etc): the slot (and the weapon's kind) from x 46 and its rows (P.Atk, M.Atk, speed, soulshots, spiritshots /
// P.Def with the enchant, M.Def, MP bonus / shield and evasion) from X, the weight, and the description (wrapped at 200 pixels);
// returns the Y under the last line. C4: the name is not drawn (the name control over the page is); the rows start at x 46
INT NCVIPShopItemInfoWnd::DrawItemInfo(NCGDevice* Device, NItemEntry* Entry, INT X, INT Y)
{
    TCHAR Buf[2046];
    SIZE CharSize;
    SIZE Size;
    if (!Entry)
        return Y;
    NItemEntryC4* E = AsC4(Entry);
    Device->DrawTexture(X - 1, Y - 1, 0x22, 0x22, 0, 0, 0x22, 0x22, IconBoxTex, 0xFF, 1);
    Device->DrawTexture(X, Y, 0x20, 0x20, 0, 0, 0x20, 0x20, E->Icon[0], 0xFF, 1);
    NCGDevice::GetCharSize(&CharSize, 'A', 0);
    FString Desc;
    GConsole->ConvertNewLines((TCHAR*)Entry->GetDescription(), Desc);
    FString SlotText;
    GConsole->GetItemSlotText(E->Type, E->BodyPart, SlotText, E->SubType);
    INT LineY = Y + 8 + CharSize.cy;
    INT LabelX = 0x2E;
    INT NextY;
    INT VX;
    if (E->Type == 0)
    {
        appSprintf(Buf, TEXT("%s / %s"), GConsole->GetWeaponTypeName(E->UnknownC30), *SlotText);
        Device->DrawText(0x2E, LineY, 0xFFB09B79, Buf, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
        NextY = LineY + 20;
        LabelX = X;
        VX = DrawStatLabel(Device, X, NextY, 94, Buf);
        appSprintf(Buf, TEXT("%d"), ((NConsoleLeftFns*)GConsole)->GetEnchantedPAtk(E->UnknownC30, E->BodyPart, E->CrystalGrade,
                                                                       E->Enchant, E->Unknown1C80));   // 0x100741B0
        DrawStatValue(Device, VX, NextY, Buf);
        LineY = LineY + 24 + CharSize.cy;
        VX = DrawStatLabel(Device, X, LineY, 98, Buf);
        appSprintf(Buf, TEXT("%d"), ((NConsoleLeftFns*)GConsole)->GetEnchantedMAtk(E->UnknownC30, E->BodyPart, E->CrystalGrade,
                                                                       E->Enchant, E->Unknown1C84));   // 0x10074250
        DrawStatValue(Device, VX, LineY, Buf);
        LineY = LineY + 4 + CharSize.cy;
        VX = DrawStatLabel(Device, X, LineY, 111, Buf);
        appSprintf(Buf, TEXT("%s"), GConsole->GetAttackSpeedText(E->Unknown1C6C));
        DrawStatValue(Device, VX, LineY, Buf);
        LineY = LineY + 4 + CharSize.cy;
        VX = DrawStatLabel(Device, X, LineY, 404, Buf);
        appSprintf(Buf, TEXT("X %d"), E->Unknown1CA0);
        DrawStatValue(Device, VX, LineY, Buf);
        NextY = LineY + 4 + CharSize.cy;
        VX = DrawStatLabel(Device, X, NextY, 496, Buf);
        appSprintf(Buf, TEXT("X %d"), E->Unknown1CA4);
        DrawStatValue(Device, VX, NextY, Buf);
        NextY = NextY + 4 + CharSize.cy;
    }
    else if (E->Type == 1)
    {
        appSprintf(Buf, TEXT("%s"), *SlotText);
        Device->DrawText(0x2E, LineY, 0xFFB09B79, Buf, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
        NextY = LineY + 20;
        LabelX = X;
        if ((E->BodyPart & 0x100) == 0 && (char)E->BodyPart >= 0)
        {
            if (!GConsole->IsMagicArmor(E->ID))
            {
                VX = DrawStatLabel(Device, X, NextY, 95, Buf);
                appSprintf(Buf, TEXT("%d"), GConsole->GetEnchantedDefense(E->CrystalGrade, E->Enchant, E->Unknown1C88));
                DrawStatValue(Device, VX, NextY, Buf);
                NextY = NextY + 4 + CharSize.cy;
            }
            else
            {
                VX = DrawStatLabel(Device, X, NextY, 388, Buf);
                appSprintf(Buf, TEXT("%d"), E->Unknown1C9C);
                DrawStatValue(Device, VX, NextY, Buf);
                LineY = LineY + 24 + CharSize.cy;
                VX = DrawStatLabel(Device, X, LineY, 95, Buf);
                appSprintf(Buf, TEXT("%d"), GConsole->GetEnchantedDefense(E->CrystalGrade, E->Enchant, E->Unknown1C88));
                DrawStatValue(Device, VX, LineY, Buf);
                NextY = LineY + 4 + CharSize.cy;
            }
        }
        else
        {
            VX = DrawStatLabel(Device, X, NextY, 95, Buf);
            appSprintf(Buf, TEXT("%d"), GConsole->GetEnchantedDefense(E->CrystalGrade, E->Enchant, E->Unknown1C90));
            DrawStatValue(Device, VX, NextY, Buf);
            LineY = LineY + 24 + CharSize.cy;
            VX = DrawStatLabel(Device, X, LineY, 317, Buf);
            appSprintf(Buf, TEXT("%d"), E->Unknown1C94);
            DrawStatValue(Device, VX, LineY, Buf);
            LineY = LineY + 4 + CharSize.cy;
            VX = DrawStatLabel(Device, X, LineY, 97, Buf);
            appSprintf(Buf, TEXT("%d"), E->Unknown1C78);
            DrawStatValue(Device, VX, LineY, Buf);
            NextY = LineY + 4 + CharSize.cy;
        }
    }
    else
    {
        NextY = LineY;
        if (E->Type == 2)
        {
            appSprintf(Buf, TEXT("%s"), *SlotText);
            Device->DrawText(0x2E, LineY, 0xFFB09B79, Buf, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
            INT RowY = LineY + 20;
            LabelX = X;
            VX = DrawStatLabel(Device, X, RowY, 99, Buf);
            appSprintf(Buf, TEXT("%d"), GConsole->GetEnchantedDefense(E->CrystalGrade, E->Enchant, E->Unknown1C8C));
            DrawStatValue(Device, VX, RowY, Buf);
            NextY = LineY + 24 + CharSize.cy;
        }
    }
    switch (E->Type)
    {
    case 0:
    case 1:
    case 2:
        VX = DrawStatLabel(Device, LabelX, NextY, 52, Buf);
        appSprintf(Buf, TEXT("%d"), E->Weight);
        DrawStatValue(Device, VX, NextY, Buf);
        appSprintf(Buf, TEXT("%s"), *Desc);
        if (Buf[0] == 0)
            return NextY;
        NextY = NextY + 4 + CharSize.cy;
        Device->DrawText(LabelX, NextY, 0xFFB2BECF, Buf, 200, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
        NCGDevice::GetTextExtent(&Size, Buf, 200, 0, 0, 0, 0, L2FONT_Normal);
        NextY = NextY + Size.cy;
        return NextY;
    case 3:
        appSprintf(Buf, TEXT("%s"), *SlotText);
        Device->DrawText(LabelX, NextY, 0xFFB09B79, Buf, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
        appSprintf(Buf, TEXT("%s"), *Desc);
        if (Buf[0] == 0)
            return NextY;
        NextY = NextY + 4 + CharSize.cy;
        Device->DrawText(X, NextY, 0xFFB2BECF, Buf, 200, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
        NCGDevice::GetTextExtent(&Size, Buf, 200, 0, 0, 0, 0, L2FONT_Normal);
        NextY = NextY + Size.cy;
        return NextY;
    case 5:
        VX = DrawStatLabel(Device, LabelX, NextY, 52, Buf);
        appSprintf(Buf, TEXT("%d"), E->Weight);
        DrawStatValue(Device, VX, NextY, Buf);
        break;
    }
    appSprintf(Buf, TEXT("%s"), *Desc);
    if (Buf[0] != 0)
    {
        NextY = NextY + 4 + CharSize.cy;
        Device->DrawText(X, NextY, 0xFFB2BECF, Buf, 200, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
        NCGDevice::GetTextExtent(&Size, Buf, 200, 0, 0, 0, 0, L2FONT_Normal);
        NextY = NextY + Size.cy;
    }
    return NextY;
}
NW_HOOK_METHOD(0x100a0160, NCVIPShopItemInfoWnd, DrawItemInfo)

NW_CHECK_VTABLE(NCVIPShopItemInfoWnd, 0x101b9258, 78)

/*-----------------------------------------------------------------------------
    NCVIPShopNeededItemWnd
-----------------------------------------------------------------------------*/

// 0x100a3790 - the items' array is empty
NCVIPShopNeededItemWnd::NCVIPShopNeededItemWnd()
{
}
NW_HOOK_CTOR(0x100a3790, NCVIPShopNeededItemWnd)

// 0x100a37f0 - Items' destructor is TArray<T*>'s (0x100a2cb0)
NW_HOOK_DTOR(0x100a37f0, NCVIPShopNeededItemWnd)

// stock body shared with NCItemWnd's (0x10093f30, hooked there): the base's answer as a plain 0 / 1
INT NCVIPShopNeededItemWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return NCScrollWnd::WindowProc(Msg, wParam, lParam) != 0;
}

// 0x1009ffd0 (vslot 37) - the frame texture of the icons; the row controls start empty
INT NCVIPShopNeededItemWnd::OnCreate()
{
    NCScrollWnd::OnCreate();
    IconBoxTex = FindTexture(TEXT("L2ui_ch3.etc.iconbox"));
    for (INT i = 0; i < 50; i++)
        NameCtrls[i] = NULL;
    return 1;
}
NW_HOOK_METHOD(0x1009ffd0, NCVIPShopNeededItemWnd, OnCreate)

// 0x100a0010 (ret 8) - the name controls follow the scroll (DX, -DY)
void NCVIPShopNeededItemWnd::MoveNames(INT DX, INT DY)
{
    for (INT i = 0; i < 50; i++)
    {
        if (NameCtrls[i])
            NameCtrls[i]->MoveWindow(DX, -DY);
    }
}
NW_HOOK_METHOD(0x100a0010, NCVIPShopNeededItemWnd, MoveNames)

// the four virtuals of the scrolling (vslots 73, 75, 76, 77; the stock's own overrides of NCScrollWnd's): the scroll window's step
// (a direct call of NCScrollWnd's in the stock), then the name controls follow it by DeltaY (+0xF4); all four return 1
// 0x100a0050
INT NCVIPShopNeededItemWnd::ScrollUp()
{
    NCScrollWnd::ScrollUp();
    MoveNames(0, DeltaY);
    return 1;
}
NW_HOOK_METHOD(0x100a0050, NCVIPShopNeededItemWnd, ScrollUp)
// 0x100a0070
INT NCVIPShopNeededItemWnd::ScrollDown()
{
    NCScrollWnd::ScrollDown();
    MoveNames(0, DeltaY);
    return 1;
}
NW_HOOK_METHOD(0x100a0070, NCVIPShopNeededItemWnd, ScrollDown)
// 0x100a0090 (ret 4)
INT NCVIPShopNeededItemWnd::OnScrollBar(INT Value)
{
    NCScrollWnd::OnScrollBar(Value);
    MoveNames(0, DeltaY);
    return 1;
}
NW_HOOK_METHOD(0x100a0090, NCVIPShopNeededItemWnd, OnScrollBar)
// 0x100a00c0 (ret 8)
INT NCVIPShopNeededItemWnd::OnMouseWheel(WPARAM wParam, LPARAM lParam)
{
    NCScrollWnd::OnMouseWheel(wParam, lParam);
    MoveNames(0, DeltaY);
    return 1;
}
NW_HOOK_METHOD(0x100a00c0, NCVIPShopNeededItemWnd, OnMouseWheel)

// 0x100a1a80 (vslot 62; the guard's name has a lower case n) - each needed item: its icon in the frame and "x count"; the name
// control is over the rest of the row; the height of the rows sizes the scroll range
INT NCVIPShopNeededItemWnd::OnPaint(NCGDevice* Device)
{
    TCHAR Text[2050];
    guard(NCVIPShopneededItemWnd::OnPaint);
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    if (Items.Num() > 0)
    {
        Device->PushOrigin(ScrollX, ScrollY);
        INT i;
        for (i = 0; i < Items.Num(); i++)
        {
            Device->DrawTexture(10, i * 0x24 + 5, 0x22, 0x22, 0, 0, 0x22, 0x22, IconBoxTex, 0xFF, 1);
            Device->DrawTexture(11, i * 0x24 + 6, 0x20, 0x20, 0, 0, 0x20, 0x20, Items(i)->Icon, 0xFF, 1);
            appSprintf(Text, TEXT("x %d"), Items(i)->Count);
            Device->DrawText(0x2E, i * 0x24 + 0x19, 0xFFDCDCDC, Text, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
        }
        SetContentHeight(i * 0x24 + 5);
        Device->PopOrigin();
    }
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100a1a80, NCVIPShopNeededItemWnd, OnPaint)

// 0x100a2dc0 (vslot 38) - the items go (the stock walks the array until it is empty, which it is already: Reset emptied it), the
// scroll window, the name controls
INT NCVIPShopNeededItemWnd::OnDestroy()
{
    guard(NCVIPShopNeededItemWnd::OnDestroy);
    for (INT i = 0; i < Items.Num(); i++)
    {
        if (Items(i))
            appFree(Items(i));
    }
    Items.Empty();
    NCScrollWnd::OnDestroy();
    Clear();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100a2dc0, NCVIPShopNeededItemWnd, OnDestroy)

// 0x100a25a0 - the items freed and the array emptied, the name controls destroyed
void NCVIPShopNeededItemWnd::Clear()
{
    for (INT i = 0; i < Items.Num(); i++)
    {
        if (Items(i))
            appFree(Items(i));
    }
    Items.Empty();
    for (INT i = 0; i < 50; i++)
        DestroyChild(this, NameCtrls[i]);
    Unknown1FC = 0;
    Unknown200 = 0;
}
NW_HOOK_METHOD(0x100a25a0, NCVIPShopNeededItemWnd, Clear)

// 0x100a2360 (ret 4) - Src copied to the end of the items, a name control over its row (the same row of the page: 36 pixels
// from y 10)
void NCVIPShopNeededItemWnd::AddNeeded(const NVIPNeededItem* Src)
{
    void* Mem = GMalloc->Malloc(sizeof(NVIPNeededItem), TEXT("new"));
    NVIPNeededItem* Copy = Mem ? ::new (Mem) NVIPNeededItem : NULL;
    Copy->ItemID = Src->ItemID;
    appStrcpy(Copy->Name, Src->Name);
    Copy->Icon = Src->Icon;
    Copy->Count = Src->Count;
    Copy->Enchant = Src->Enchant;
    Copy->Grade = Src->Grade;
    Copy->Type = Src->Type;
    Items.AddItem(Copy);
    INT Num = Items.Num();
    NCWnd* Name = NewNameCtrl();
    NameCtrls[Num - 1] = Name;
    Name->SetPosition(0x2E, (Num - 1) * 0x24 + 10, 0xAA, 0xE);
    CreateChild(Name, 0x22);
    L2ParamStack Params(10);
    Params.PushBack(Items(Num - 1)->Name);
    Params.PushBack((void*)(INT_PTR)Items(Num - 1)->ItemID);
    Params.PushBack((void*)(INT_PTR)Items(Num - 1)->Type);
    Params.PushBack((void*)(INT_PTR)Items(Num - 1)->Enchant);
    Params.PushBack((void*)(INT_PTR)Items(Num - 1)->Grade);
    Params.PushBack((void*)0);
    ((NCNameCtrlView*)Name)->SetItem(&Params, 1, 0);
}
NW_HOOK_METHOD(0x100a2360, NCVIPShopNeededItemWnd, AddNeeded)

NW_CHECK_VTABLE(NCVIPShopNeededItemWnd, 0x101b9120, 78)

/*-----------------------------------------------------------------------------
    NCVIPShopWnd
-----------------------------------------------------------------------------*/

// 0x100a3960 - the records' array is empty
NCVIPShopWnd::NCVIPShopWnd()
{
}
NW_HOOK_CTOR(0x100a3960, NCVIPShopWnd)

// 0x100a39c0 - Records' destructor is TArray<T*>'s: the records are not freed
NW_HOOK_DTOR(0x100a39c0, NCVIPShopWnd)

// 0x100a17f0 (vslot 25) - C2: no IsEnabled test
INT NCVIPShopWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCWnd::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND)
    {
        if (wParam == NID_VIPSHOP_OK)
            return OnOK() != 0;
        if (wParam == NID_VIPSHOP_CANCEL)
            return OnCancel() != 0;
    }
    return 0;
}
NW_HOOK_METHOD(0x100a17f0, NCVIPShopWnd, WindowProc)

// 0x100a4040 (vslot 38; the guard's name is the class's own) - everything emptied, the children go (the list, the pages, the
// two buttons and the edit line, in that order)
INT NCVIPShopWnd::OnDestroy()
{
    guard(NCVIPShopWnd::OnDestroy);
    NCWnd::OnDestroy();
    Reset();
    DestroyChild(this, ListWnd);
    DestroyChild(this, InfoWnd);
    DestroyChild(this, NeededWnd);
    DestroyChild(this, OKButton);
    DestroyChild(this, CancelButton);
    DestroyChild(this, CountEdit);
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100a4040, NCVIPShopWnd, OnDestroy)

// 0x100a1fc0 (vslot 62; the guard's name has a lower case p) - the window's back under the frame, the labels of the pages and
// the amount's right aligned to x 164
INT NCVIPShopWnd::OnPaint(NCGDevice* Device)
{
    SIZE LabelSize;
    guard(NCVIPShopWnd::Onpaint);
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    INT H = (INT)(Height - 20.0f);
    INT W = (INT)Width;
    Device->DrawTexture(0, 0x14, W, H, 0, 0, W, H, BackTex, 0xFF, 1);
    Device->DrawText(0xB, 0x20, 0xFFDCDCDC, *SysString(STR_List), 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
    Device->DrawText(0x10B, 0x20, 0xFFDCDCDC, *SysString(STR_ItemInformation), 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
    Device->DrawText(0x10B, 0xCB, 0xFFDCDCDC, *SysString(STR_ItemsRequired), 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
    NCGDevice::GetTextExtent(&LabelSize, *SysString(STR_Amount), 0, 0, 0, 0, 0, L2FONT_Normal);
    Device->DrawText(0xA4 - LabelSize.cx, 0x17A, 0xFFDCDCDC, *SysString(STR_Amount), 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100a1fc0, NCVIPShopWnd, OnPaint)

// 0x100a21b0 (vslot 74) - the amount asked in a dialog (id 0x51) that carries the list and the product, with the amount typed so far
INT NCVIPShopWnd::OnOK()
{
    if (Selected >= 0 && Selected < Records.Num())
    {
        NDialogInfo Dialog;
        Dialog.ID = DIALOG_VIPSHOP_AMOUNT;
        Dialog.bMainBox = 1;
        Dialog.bInput = 0;
        Dialog.Owner = NULL;
        Dialog.SetParams(ListID, Records(Selected)->Item(0)->UnknownC90);
        Dialog.SetParamText(CountEdit->CommitText());
        Dialog.SetSystemMessage(MSG_MultisellAmount);
        GConsole->ShowDialog(&Dialog);
    }
    return 1;
}
NW_HOOK_METHOD(0x100a21b0, NCVIPShopWnd, OnOK)

// 0x100a3f40 - nothing selected, the list and the pages emptied, the records and the items they cost freed
void NCVIPShopWnd::Reset()
{
    Selected = -1;
    ListID = 0;
    ListWnd->Clear();
    InfoWnd->Clear();
    NeededWnd->Clear();
    for (INT i = 0; i < Records.Num(); i++)
    {
        NVIPRecord* Record = Records(i);
        if (Record)
        {
            for (INT k = 0; k < Record->Needed.Num(); k++)
            {
                if (Record->Needed(k))
                    appFree(Record->Needed(k));
            }
            Record->DestroyNeeded();
            appFree(Record);
        }
    }
    Records.Empty();
}
NW_HOOK_METHOD(0x100a3f40, NCVIPShopWnd, Reset)

// 0x100a4000 (vslot 75) - Cancel: everything emptied and the window hidden
INT NCVIPShopWnd::OnCancel()
{
    Reset();
    HideWindow();
    return 1;
}
NW_HOOK_METHOD(0x100a4000, NCVIPShopWnd, OnCancel)

// 0x100a41d0 (vslot 49) - hiding the window empties it
void NCVIPShopWnd::HideWindow()
{
    NCWnd::HideWindow();
    Reset();
}
NW_HOOK_METHOD(0x100a41d0, NCVIPShopWnd, HideWindow)

// 0x100a2e90 - the pages of the selected product: the items it costs (a copy of each into the needed items' page) and the items it
// gives (each as an entry built from the record's item info into the information page); the amount's edit line is "1", changeable
// only for the lists of the kind 1
void NCVIPShopWnd::ShowItem()
{
    InfoWnd->Clear();
    NeededWnd->Clear();
    if (Selected < 0)
        return;
    NVIPRecord* Record = Records(Selected);
    for (INT i = 0; i < Record->Needed.Num(); i++)
    {
        NVIPNeededItem Needed;
        NVIPNeededItem* Src = Record->Needed(i);
        appStrcpy(Needed.Name, Src->Name);
        Needed.ItemID = Src->ItemID;
        Needed.Count = Src->Count;
        Needed.Icon = Src->Icon;
        Needed.Enchant = Src->Enchant;
        Needed.Grade = Src->Grade;
        Needed.Type = Src->Type;
        NeededWnd->AddNeeded(&Needed);
        Record = Records(Selected);
    }
    for (INT k = 0; k < Record->NumItems; k++)
    {
        NLocalEntry Entry;
        NItemInfoC4* Item = Record->Item(k);
        Entry->ID = Item->ID;
        Entry->Type = Item->Param;
        Entry->BodyPart = Item->BodyPart;
        Entry->Icon[0] = Item->Icon[0];
        Entry->Count = Item->Count;
        Entry->Enchant = Item->Enchant;
        Entry->CrystalGrade = Item->UnknownC34[7];
        InfoWnd->AddItem(k, Entry);
        Record = Records(Selected);
    }
    if (Record->ListID == 0)
    {
        CountEdit->ClearText();
        CountEdit->InsertText(TEXT("1"), 1);
        CountEdit->EnableWindow(0);
    }
    else if (Record->ListID == 1)
    {
        CountEdit->ClearText();
        CountEdit->InsertText(TEXT("1"), 1);
        CountEdit->EnableWindow(1);
    }
}
NW_HOOK_METHOD(0x100a2e90, NCVIPShopWnd, ShowItem)

// The three bodies below are the console lane's (NConsoleWndAnon.cpp hooks the addresses on a stand-in struct of that name, declared in
// NConsoleFns.h; `this` is the window): the standalone slots call them there

// 0x100a2ab0  the console lane's (NConsoleWndAnon.cpp: the list built again)
#if NW_HOOK_MODE
NW_NAKED void NCVIPShopWnd::ShowList() NW_STOCK(0x100a2ab0)
#else
void NCVIPShopWnd::ShowList()
{
    ((NCVIPShopWndExt*)this)->Refresh();
}
#endif

// 0x100a2b40  the console lane's (NConsoleWndAnon.cpp)
#if NW_HOOK_MODE
NW_NAKED void NCVIPShopWnd::AddNeededItem(INT ProductID, const NVIPNeededItem* Src) NW_STOCK(0x100a2b40)
#else
void NCVIPShopWnd::AddNeededItem(INT ProductID, const NVIPNeededItem* Src)
{
    ((NCVIPShopWndExt*)this)->AddNeededItem(ProductID, Src);
}
#endif

// 0x100a4020  the console lane's (NConsoleWndAnon.cpp)
#if NW_HOOK_MODE
NW_NAKED void NCVIPShopWnd::Open(INT InListID) NW_STOCK(0x100a4020)
#else
void NCVIPShopWnd::Open(INT InListID)
{
    ((NCVIPShopWndExt*)this)->Open(InListID);
}
#endif

// 0x100a3a20 (vslot 37) - the frame, the grid of the products, the two pages, the buttons, the edit line of the amount, the back
INT NCVIPShopWnd::OnCreate()
{
    FrameCtrl = NewFrameCtrl(1, 0, 0);
    FrameID = NID_VIPSHOP_FRAME;
    FrameCtrl->ID = NID_VIPSHOP_FRAME;
    FrameCtrl->SetPosition(0, 0, (DWORD)(INT)Width, 0x14);
    CreateChild(FrameCtrl, NWS_VISIBLE);
    FrameCtrl->SetTitleID(STR_Store);

    ListWnd = new NCVIPShopItemWnd(6, 9, 0x50, 0x20, 5, 3);
    ListWnd->SetPosition(9, 0x30, 0xF0, 0x13A);
    CreateChild(ListWnd, NWS_VISIBLE | 0x20);

    InfoWnd = new NCVIPShopItemInfoWnd;
    InfoWnd->SetPosition(0x106, 0x2D, 0xF4, 0x96);
    CreateChild(InfoWnd, NWS_VISIBLE);

    NeededWnd = new NCVIPShopNeededItemWnd;
    NeededWnd->SetPosition(0x106, 0xD9, 0xF4, 0x96);
    CreateChild(NeededWnd, NWS_VISIBLE);

    OKButton = new NCButton(TEXT("L2UI_CH3.Button.btn1_normal"), TEXT("L2UI_CH3.Button.btn1_normalon"), 0);
    OKButton->ID = NID_VIPSHOP_OK;
    OKButton->SetPosition(0x133, 0x174, 0x4C, 0x17);
    CreateChild(OKButton, NWS_VISIBLE);
    OKButton->SetTextID(STR_OK);

    CancelButton = new NCButton(TEXT("L2UI_CH3.Button.btn1_normal"), TEXT("L2UI_CH3.Button.btn1_normalon"), 0);
    CancelButton->ID = NID_VIPSHOP_CANCEL;
    CancelButton->SetPosition(0x183, 0x174, 0x4C, 0x17);
    CreateChild(CancelButton, NWS_VISIBLE);
    CancelButton->SetTextID(STR_Cancel);

    CountEdit = new NCEditBox(3, NULL, 1, 0, 0);
    CountEdit->ID = NID_VIPSHOP_COUNT;
    CountEdit->SetPosition(0xA5, 0x176, 0x3F, 0x11);
    CreateChild(CountEdit, NWS_VISIBLE);

    BackTex = FindTexture(TEXT("L2UI_CH3.MultiSellWnd.multicell_back"));
    return 1;
}
NW_HOOK_METHOD(0x100a3a20, NCVIPShopWnd, OnCreate)

NW_CHECK_VTABLE(NCVIPShopWnd, 0x101b9390, 76)
