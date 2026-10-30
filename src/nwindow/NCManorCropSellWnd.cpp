/*=============================================================================
    NCManorCropSellWnd.cpp - the crops the player can sell to a manor (new in C4).
    Stock (C4): 0x10134800 .. 0x10135A10 (NotifyChangeSellValue, the buttons, OnShowSellCropList, GetMyCropCnt, OnDestroy,
    OnPaint, OnChangeBtnClick, OnCreate, OnListCtrl, OnSellBtnClick, WindowProc), 0x1005F6B0 (constructor), 0x1005F700
    (destructor).
=============================================================================*/
#include "nwindow/NCManorCropSellWnd.h"
#include "nwindow/NCManorCropSellChangeWnd.h"
#include "nwindow/NCManorInfoSettingWnd.h"
#include "nwindow/NClasses.gen.h"
#include "nwindow/NGameData.h"
#include "nwindow/NString.h"

// sysstring labels
enum
{
    STR_Change = 0x476,
    STR_Sell = 0x485,
    STR_Cancel = 0x8D,
};

// the copy of the list's rows, made and destroyed by the stock functions (the array's copy constructor 0x10130F30 and
// destructor 0x10023580); the standalone has the array's own (the same two functions, hooked in NCManorCropInfoSettingWnd.cpp
// and NTemplates.cpp)
#if NW_HOOK_MODE
struct NManorSellRows
{
    BYTE Storage[12];
    NManorSellRows(TArray<NListRow*>* Source) { ::nw::StockCallAt<void>(0x10130F30, this, Source); }
    ~NManorSellRows() { ::nw::StockCallAt<void>(0x10023580, this); }
    INT Num() { return ((INT*)Storage)[1]; }
    NListRow* Get(INT i) { return ((NListRow**)((INT*)Storage)[0])[i]; }
};
#else
struct NManorSellRows : public TArray<NListRow*>
{
    NManorSellRows(TArray<NListRow*>* Source) : TArray<NListRow*>(*Source) {}
    NListRow* Get(INT i) { return (*this)(i); }
};
#endif

// the shared bodies the stock class inherits from NCManorInfoSettingWnd (this header keeps NCWnd as the base and declares the
// two inherited fields, which sit where the base has them): called as that class' own, not virtually
#define MANOR_SETTING_BASE() (reinterpret_cast<NCManorInfoSettingWnd*>(this))

// an array of 4-byte values as the stock builds it: empty, with its storage released (FArray::Realloc) at once
struct NManorIntArray : public TArray<INT>
{
    NManorIntArray() { Empty(); }
};

// 0x1005f6b0 - NCWnd's constructor and the vtable
NCManorCropSellWnd::NCManorCropSellWnd()
{
}
NW_HOOK_CTOR(0x1005f6b0, NCManorCropSellWnd)

// 0x1005f700 - ConditionalDestroy, then NCWnd's destructor (the stock one calls NCManorInfoSettingWnd's, which does the same)
NW_HOOK_DTOR(0x1005f700, NCManorCropSellWnd)

// 0x10135a10 (vslot 25) - the shared body first (the list's command); then the three buttons
INT NCManorCropSellWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (MANOR_SETTING_BASE()->NCManorInfoSettingWnd::WindowProc(Msg, wParam, lParam))   // 0x10136AD0
        return 1;
    if (Msg == NWM_COMMAND)
    {
        if (wParam == (WPARAM)NID_MANORSELLCROP_CHANGE)
            return OnChangeBtnClick() != 0;
        if (wParam == (WPARAM)NID_MANORSELLCROP_SELL)
            return OnSellBtnClick() != 0;
        if (wParam == (WPARAM)NID_MANORSELLCROP_CANCEL)
            return OnCancelBtnClick() != 0;
    }
    return 0;
}
NW_HOOK_METHOD(0x10135a10, NCManorCropSellWnd, WindowProc)

// 0x10134960 - closes the window
INT NCManorCropSellWnd::OnCancelBtnClick()
{
    guard(NCManorCropSellWnd::OnCancelBtnClick);
    HideWindow();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10134960, NCManorCropSellWnd, OnCancelBtnClick)

// 0x10134f20 (vslot 38) - the three buttons, then the shared body (the list, the base)
INT NCManorCropSellWnd::OnDestroy()
{
    guard(NCManorCropSellWnd::OnDestroy);
    ManorDestroyChild(this, ChangeButton);
    ManorDestroyChild(this, SellButton);
    ManorDestroyChild(this, CancelButton);
    MANOR_SETTING_BASE()->NCManorInfoSettingWnd::OnDestroy();   // 0x10136B30
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10134f20, NCManorCropSellWnd, OnDestroy)

// 0x10135020 (vslot 62) - the shared body draws the back texture; nothing else
INT NCManorCropSellWnd::OnPaint(NCGDevice* Device)
{
    guard(NCManorCropSellWnd::OnPaint);
    MANOR_SETTING_BASE()->NCManorInfoSettingWnd::OnPaint(Device);   // 0x10136BE0
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    Device->BeginPaint(ScreenX, ScreenY);
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10135020, NCManorCropSellWnd, OnPaint)

// 0x101350e0 - the selected row's crop goes to the server (ask 9: the manors that buy it), and its three texts (the name, the
// seeds / crops it is made from and gives) to the sell change window (the desktop's, +0x1FC) before the answer comes
INT NCManorCropSellWnd::OnChangeBtnClick()
{
    guard(NCManorCropSellWnd::OnChangeBtnClick);
    NListRow* Row = ListCtrl->GetSelectedRow();
    if (Row)
    {
        TCHAR Bypass[1024];
        appSprintf(Bypass, TEXT("manor_menu_select?ask=9&state=%d&time=0"), Row->Param2);
        L2ParamStack Params(10);
        Params.PushBack((void*)Bypass);
        GConsole->RequestBypassToServer(Params);
        const TCHAR* Cell9 = ManorCellText(Row, 9);
        const TCHAR* Cell8 = ManorCellText(Row, 8);
        const TCHAR* Cell0 = ManorCellText(Row, 0);
        NCManorCropSellChangeWnd* Change = (NCManorCropSellChangeWnd*)::nw::Field<NCWnd*>(GConsole->Desktop, 0x1FC);
        Change->Text1 = Cell0;
        Change->Text2 = Cell8;
        Change->Text3 = Cell9;
    }
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x101350e0, NCManorCropSellWnd, OnChangeBtnClick)

// 0x10135230 (vslot 37) - the shared body (the title bar, the back picture, the list), the change, sell and cancel buttons
// and the seven columns of the list
INT NCManorCropSellWnd::OnCreate()
{
    guard(NCManorCropSellWnd::OnCreate);
    MANOR_SETTING_BASE()->NCManorInfoSettingWnd::OnCreate();   // 0x10136CE0

    ChangeButton = ManorNewButton(0, 0);
    ChangeButton->ID = NID_MANORSELLCROP_CHANGE;
    ChangeButton->SetPosition(8, 0x129, 0, 0);
    CreateChild(ChangeButton, NWS_VISIBLE);
    ChangeButton->SetTextID(STR_Change);

    SellButton = ManorNewButton(0, 0);
    SellButton->ID = NID_MANORSELLCROP_SELL;
    SellButton->SetPosition(0x1B6, 0x129, 0, 0);
    CreateChild(SellButton, NWS_VISIBLE);
    SellButton->SetTextID(STR_Sell);

    CancelButton = ManorNewButton(0, 0);
    CancelButton->ID = NID_MANORSELLCROP_CANCEL;
    CancelButton->SetPosition(0x206, 0x129, 0, 0);
    CreateChild(CancelButton, NWS_VISIBLE);
    CancelButton->SetTextID(STR_Cancel);

    ManorListSetColumns(ListCtrl, 7);
    NManorColumnInfo Column;
    ManorSetTabTextures(Column);
    Column.Width = 0x95;
    Column.TitleID = 0x464;
    Column.bNumeric = 0;
    ListCtrl->AddColumn(0, (NListColumnInfo*)&Column);
    Column.Width = 0x46;
    Column.TitleID = 0x481;
    Column.bNumeric = 0;
    ListCtrl->AddColumn(1, (NListColumnInfo*)&Column);
    Column.Width = 0x46;
    Column.TitleID = 0x482;
    Column.bNumeric = 1;
    ListCtrl->AddColumn(2, (NListColumnInfo*)&Column);
    Column.Width = 0x46;
    Column.TitleID = 0x466;
    Column.bNumeric = 1;
    ListCtrl->AddColumn(3, (NListColumnInfo*)&Column);
    Column.Width = 0x46;
    Column.TitleID = 0x467;
    Column.bNumeric = 0;
    ListCtrl->AddColumn(4, (NListColumnInfo*)&Column);
    Column.Width = 0x46;
    Column.TitleID = 0x483;
    Column.bNumeric = 1;
    ListCtrl->AddColumn(5, (NListColumnInfo*)&Column);
    Column.Width = 0x57;
    Column.TitleID = 0x484;
    Column.bNumeric = 1;
    ListCtrl->AddColumn(6, (NListColumnInfo*)&Column);
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10135230, NCManorCropSellWnd, OnCreate)

// 0x10135710 (vslot 74, ret 4) - a double click on a row is the change button
INT NCManorCropSellWnd::OnListCtrl(INT Row)
{
    guard(NCManorCropSellWnd::OnListCtrl);
    OnChangeBtnClick();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10135710, NCManorCropSellWnd, OnListCtrl)

// 0x10135790 - Sell: every row with an amount to sell (the seventh cell) is collected in four arrays (its third, second and
// first number, then the amount) and the count and the four numbers of each go to the server (the console keeps the stack's
// contents; the stack is deleted here); the window closes
INT NCManorCropSellWnd::OnSellBtnClick()
{
    guard(NCManorCropSellWnd::OnSellBtnClick);
    NManorIntArray Third;
    NManorIntArray Second;
    NManorIntArray First;
    NManorIntArray Amounts;
    NManorSellRows Rows(ListCtrl->GetRows());
    for (INT i = 0; i < Rows.Num(); i++)
    {
        NListRow* Row = Rows.Get(i);
        if (Row)
        {
            INT Amount = appAtoi(ManorCellText(Row, 6));
            if (Amount != 0)
            {
                Third(Third.Add()) = Row->Param3;
                Second(Second.Add()) = Row->Param2;
                First(First.Add()) = Row->Param1;
                Amounts(Amounts.Add()) = Amount;
            }
        }
    }
    INT Count = Third.Num();
    L2ParamStack* Params = new L2ParamStack(0x400);
    Params->PushBack((void*)(INT_PTR)Count);
    for (INT i = 0; i < Count; i++)
    {
        Params->PushBack((void*)(INT_PTR)Third(i));
        Params->PushBack((void*)(INT_PTR)Second(i));
        Params->PushBack((void*)(INT_PTR)First(i));
        Params->PushBack((void*)(INT_PTR)Amounts(i));
    }
    GConsole->RequestProcureCropList(*Params);
    if (Params)
    {
        Params->~L2ParamStack();
        appFree(Params);
    }
    HideWindow();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10135790, NCManorCropSellWnd, OnSellBtnClick)

// 0x10134e80 - how many of the selected crop the player has (the sixth cell), 0 without a selection
INT NCManorCropSellWnd::GetMyCropCnt()
{
    guard(NCManorCropSellWnd::GetMyCropCnt);
    INT Have = 0;
    NListRow* Row = ListCtrl->GetSelectedRow();
    if (Row)
        Have = appAtoi(ManorCellText(Row, 5));
    return Have;
    unguard;
}
NW_HOOK_METHOD(0x10134e80, NCManorCropSellWnd, GetMyCropCnt)

// 0x10134800 (ret 0x14) - the change window's answer for the selected row: the manor it sells to (its name in the second
// cell), the three numbers it gave and the amount to sell (the seventh cell)
void NCManorCropSellWnd::NotifyChangeSellValue(INT Amount, INT Manor, INT Value1, INT Value2, INT Value3)
{
    guard(NCManorCropSellWnd::NotifyChangeSellValue);
    NListRow* Row = ListCtrl->GetSelectedRow();
    Row->Param1 = Manor;
    Row->Cells[1].SetText(GConsole->GetManorName(Manor));
    Row->Cells[2].SetText(*FString::Printf(TEXT("%d"), Value1));
    Row->Cells[3].SetText(*FString::Printf(TEXT("%d"), Value2));
    Row->Cells[4].SetText(*FString::Printf(TEXT("%d"), Value3));
    Row->Cells[6].SetNumber(Amount);
    unguard;
}
NW_HOOK_METHOD(0x10134800, NCManorCropSellWnd, NotifyChangeSellValue)

// 0x101349e0 (ret 4) - the console's packet: one value skipped, the number of crops, then for each crop four values (the
// number the seeds / crops lists share, the crop, ...), the crops it is made from and gives (a count each, then the ids;
// the names joined by commas), five more values; a row of ten cells (the sixth is the amount to sell, "0")
void NCManorCropSellWnd::OnShowSellCropList(L2ParamStack* Params)
{
    guard(NCManorCropSellWnd::OnShowSellCropList);
    ListCtrl->ClearRows();
    Params->Top();
    INT Count = (INT)Params->Top();
    for (INT n = 0; n < Count; n++)
    {
        INT Value1 = (INT)Params->Top();
        INT Crop = (INT)Params->Top();
        INT Value3 = (INT)Params->Top();
        INT NumFirst = (INT)Params->Top();
        FString First(TEXT(""));
        FString Second(TEXT(""));
        for (INT i = 0; i < (signed char)NumFirst; i++)
        {
            INT Name = (INT)Params->Top();
            if (i != 0)
                First = First + TEXT(",");
            First = First + GL2GameData.GetItemName(Name);
        }
        BYTE NumSecond = (BYTE)(INT)Params->Top();
        for (INT i = 0; i < (signed char)NumSecond; i++)
        {
            INT Name = (INT)Params->Top();
            if (i != 0)
                Second = Second + TEXT(",");
            Second = Second + GL2GameData.GetItemName(Name);
        }
        INT Manor = (INT)Params->Top();
        INT Value5 = (INT)Params->Top();
        INT Value6 = (INT)Params->Top();
        INT Value7 = (INT)Params->Top();
        INT Value8 = (INT)Params->Top();
        NListRow* Row = new NListRow(10);
        Row->Cells[0].SetText(GL2GameData.GetItemName(Crop));
        Row->Cells[1].SetText(GConsole->GetManorName(Manor));
        Row->Cells[2].SetText(*FString::Printf(TEXT("%d"), Value5));
        Row->Cells[3].SetText(*FString::Printf(TEXT("%d"), Value6));
        Row->Cells[4].SetText(*FString::Printf(TEXT("%d"), (signed char)Value7));
        Row->Cells[5].SetText(*FString::Printf(TEXT("%d"), Value8));
        Row->Cells[6].SetText(TEXT("0"));
        Row->Param2 = Crop;
        Row->Param1 = Manor;
        Row->Param3 = Value1;
        Row->Cells[7].SetText(*FString::Printf(TEXT("%d"), Value3));
        Row->Cells[8].SetText(*First);
        Row->Cells[9].SetText(*Second);
        Row->ToolTip.FillMatchRow(Row, 0x400000);   // 0x10031B10
        ListCtrl->AddRow(Row);
    }
    ShowWindow();
    SetFocus();
    unguard;
}
NW_HOOK_METHOD(0x101349e0, NCManorCropSellWnd, OnShowSellCropList)

NW_CHECK_VTABLE(NCManorCropSellWnd, 0x101ae188, 75)
