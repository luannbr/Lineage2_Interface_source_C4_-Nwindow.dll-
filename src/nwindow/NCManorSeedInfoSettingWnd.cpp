/*=============================================================================
    NCManorSeedInfoSettingWnd.cpp - the seed prices of the lord's manor, to be set (new in C4): the twin of
    NCManorCropInfoSettingWnd with five columns.
    Stock (C4): 0x1005F570 (constructor), 0x1005F5C0 (destructor), 0x10138B80 .. 0x1013A460 (the handlers, OnDestroy,
    OnPaint, OnCreate, OnListCtrl, CalcDefaultPriceSum, NotifyChangeTomValue, OnShowSeedSetting, the confirmations,
    WindowProc).
=============================================================================*/
#include "nwindow/NCManorSeedInfoSettingWnd.h"
#include "nwindow/NCManorInfoSettingWnd.h"
#include "nwindow/NCManorSeedInfoChangeWnd.h"
#include "nwindow/NClasses.gen.h"
#include "nwindow/NDialogInfo.h"
#include "nwindow/NGameData.h"
#include "nwindow/NString.h"

// sysstring labels and system messages
enum
{
    STR_Change = 0x476,
    STR_SetToToday = 0x477,
    STR_ClearAll = 0x478,
    STR_OK = 140,
    STR_Cancel = 141,
    MSG_ClearTable = 0x640,
    MSG_SetToToday = 0x641,
};

// dialog ids (names ours): the answers come back as NWM_MANORSEEDSET_CLEARCONFIRM / TODAYCONFIRM
enum
{
    DIALOG_MANOR_CLEAR = 0x56,
    DIALOG_MANOR_TODAY = 0x57,
};

// the file the stock code reports a null row from
#define MANOR_SRC "Src\\NManorSeedInfoSettingWnd.cpp"

// the copy of the list's rows, made and destroyed by the stock functions (the array's copy constructor 0x10130F30 and
// destructor 0x10023580; NCManorCropInfoSettingWnd.cpp hooks the first); the standalone has the array's own
#if NW_HOOK_MODE
struct NManorSeedRows
{
    BYTE Storage[12];
    NManorSeedRows(TArray<NListRow*>* Source) { ::nw::StockCallAt<void>(0x10130F30, this, Source); }
    ~NManorSeedRows() { ::nw::StockCallAt<void>(0x10023580, this); }
    INT Num() { return ((INT*)Storage)[1]; }
    NListRow* Get(INT i) { return ((NListRow**)((INT*)Storage)[0])[i]; }
};
#else
struct NManorSeedRows : public TArray<NListRow*>
{
    NManorSeedRows(TArray<NListRow*>* Source) : TArray<NListRow*>(*Source) {}
    NListRow* Get(INT i) { return (*this)(i); }
};
#endif

// the shared bodies the stock class inherits from NCManorInfoSettingWnd (this header keeps NCWnd as the base and declares the
// two inherited fields, which sit where the base has them): called as that class' own, not virtually
#define MANOR_SETTING_BASE() (reinterpret_cast<NCManorInfoSettingWnd*>(this))

// 0x1005f570
NCManorSeedInfoSettingWnd::NCManorSeedInfoSettingWnd()
{
}
NW_HOOK_CTOR(0x1005f570, NCManorSeedInfoSettingWnd)

// 0x1005f5c0 - ConditionalDestroy, then NCWnd's destructor (the stock one calls NCManorInfoSettingWnd's, which does the same)
NW_HOOK_DTOR(0x1005f5c0, NCManorSeedInfoSettingWnd)

// 0x1013a460 (vslot 25) - the shared body first (the list's command); then the five buttons and the answers of the two
// questions
INT NCManorSeedInfoSettingWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (MANOR_SETTING_BASE()->NCManorInfoSettingWnd::WindowProc(Msg, wParam, lParam))   // 0x10136AD0
        return 1;
    if (Msg == NWM_COMMAND)
    {
        if (wParam == (WPARAM)NID_MANORSEEDSET_CHANGE)
            return OnChangeBtnClick() != 0;
        if (wParam == (WPARAM)NID_MANORSEEDSET_TODAY)
            return OnSetToTodayBtnClick() != 0;
        if (wParam == (WPARAM)NID_MANORSEEDSET_CLEAR)
            return OnClearBtnClick() != 0;
        if (wParam == (WPARAM)NID_MANORSEEDSET_OK)
            return OnOKBtnClick() != 0;
        if (wParam == (WPARAM)NID_MANORSEEDSET_CANCEL)
            return OnCancelBtnClick() != 0;
    }
    else if (Msg == (UINT)NWM_MANORSEEDSET_CLEARCONFIRM)
        return OnClearConfirmMsg(wParam, lParam) != 0;
    else if (Msg == (UINT)NWM_MANORSEEDSET_TODAYCONFIRM)
        return OnTodayConfirmMsg(wParam, lParam) != 0;
    return 0;
}
NW_HOOK_METHOD(0x1013a460, NCManorSeedInfoSettingWnd, WindowProc)

// 0x10138b80 - closes the window
INT NCManorSeedInfoSettingWnd::OnCancelBtnClick()
{
    guard(NCManorSeedInfoSettingWnd::OnCancelBtnClick);
    HideWindow();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10138b80, NCManorSeedInfoSettingWnd, OnCancelBtnClick)

// 0x10138c00 (vslot 38) - the five buttons, then the shared body (the list, the base)
INT NCManorSeedInfoSettingWnd::OnDestroy()
{
    guard(NCManorSeedInfoSettingWnd::OnDestroy);
    ManorDestroyChild(this, ChangeButton);
    ManorDestroyChild(this, TodayButton);
    ManorDestroyChild(this, ClearButton);
    ManorDestroyChild(this, OKButton);
    ManorDestroyChild(this, CancelButton);
    MANOR_SETTING_BASE()->NCManorInfoSettingWnd::OnDestroy();   // 0x10136B30
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10138c00, NCManorSeedInfoSettingWnd, OnDestroy)

// 0x10138d50 (vslot 62) - the shared body draws the back texture; then the adena icon and the three boxes of the total,
// the manor's name, the label of the total and the total itself
INT NCManorSeedInfoSettingWnd::OnPaint(NCGDevice* Device)
{
    guard(NCManorSeedInfoSettingWnd::OnPaint);
    MANOR_SETTING_BASE()->NCManorInfoSettingWnd::OnPaint(Device);   // 0x10136BE0
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    Device->BeginPaint(ScreenX, ScreenY);
    Device->DrawTexture(0x6B, 0x116, 0x10, 0xC, 0, 0, 0x10, 0xC, AdenaTex, 0xFF, 1);
    Device->DrawTexture(0x7E, 0x114, 8, 0x11, 0, 0, 8, 0x11, TextBoxTex[0], 0xFF, 1);
    Device->DrawTexture(0x86, 0x114, 0x46, 0x11, 0, 0, 8, 0x11, TextBoxTex[1], 0xFF, 1);
    Device->DrawTexture(0xCC, 0x114, 8, 0x11, 0, 0, 8, 0x11, TextBoxTex[2], 0xFF, 1);
    ManorDrawLabel(Device, 0x10, 0x2F, 0xFFDCDCDC, GConsole->GetManorName(ManorID));
    ManorDrawLabel(Device, 0x10, 0x117, 0xFFDCDCDC, *SysString(1141));
    FString Total = ManorMakeNumStr(TotalPrice);
    ManorDrawLabel(Device, 0x82, 0x117, 0xFFDCDCDC, *Total);
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10138d50, NCManorSeedInfoSettingWnd, OnPaint)

// 0x10138f60 - the selected row goes to the change window (the desktop's, +0x200): the seed name, the most it can take
// (SetMaxAmount, 0x1000C810: folded with NCEditBox::SetType), the amount, the price and the limits of the price; then the
// window comes up
INT NCManorSeedInfoSettingWnd::OnChangeBtnClick()
{
    guard(NCManorSeedInfoSettingWnd::OnChangeBtnClick);
    NCManorSeedInfoChangeWnd* Change = (NCManorSeedInfoChangeWnd*)ManorDesktopWnd(0x200);
    if (Change)
    {
        NListRow* Row = ListCtrl->GetSelectedRow();
        if (Row)
        {
            Change->SetSeedName(FString(ManorCellText(Row, 0)));
#if NW_HOOK_MODE
            ::nw::StockCallAt<void>(0x1000C810, Change, Row->Param2);   // NCManorSeedInfoChangeWnd::SetMaxAmount
#else
            Change->MaxAmount = Row->Param2;                            // 0x1000C810: mov [ecx + 0x214], arg
#endif
            Change->SetAmt(FString(ManorCellText(Row, 3)));
            Change->SetPrice(FString(ManorCellText(Row, 4)));
            const TCHAR* Cell6 = ManorCellText(Row, 6);
            const TCHAR* Cell5 = ManorCellText(Row, 5);
            INT Limit6 = appAtoi(Cell6);
            INT Limit5 = appAtoi(Cell5);
            Change->SetLimits(Limit5, Limit6);
            Change->NewVirtual75();   // DoModal
        }
    }
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10138f60, NCManorSeedInfoSettingWnd, OnChangeBtnClick)

// 0x10139140 - asks whether to set every price to today's (the answer comes back as NWM_MANORSEEDSET_TODAYCONFIRM)
INT NCManorSeedInfoSettingWnd::OnSetToTodayBtnClick()
{
    guard(NCManorSeedInfoSettingWnd::OnSetToTodayBtnClick);
    NDialogInfo Info;
    Info.ID = DIALOG_MANOR_TODAY;
    Info.bMainBox = 1;
    Info.bInput = 0;
    Info.Owner = Entry;
    Info.SetSystemMessage(MSG_SetToToday);
    GConsole->ShowDialog(&Info);
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10139140, NCManorSeedInfoSettingWnd, OnSetToTodayBtnClick)

// 0x10139220 - asks whether to clear the table
INT NCManorSeedInfoSettingWnd::OnClearBtnClick()
{
    guard(NCManorSeedInfoSettingWnd::OnClearBtnClick);
    NDialogInfo Info;
    Info.ID = DIALOG_MANOR_CLEAR;
    Info.bMainBox = 1;
    Info.bInput = 0;
    Info.Owner = Entry;
    Info.SetSystemMessage(MSG_ClearTable);
    GConsole->ShowDialog(&Info);
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10139220, NCManorSeedInfoSettingWnd, OnClearBtnClick)

// 0x10139300 (vslot 37) - the shared body (the back texture, the list), the adena icon, the three textures of the total's
// box, the five buttons and the five columns of the list
INT NCManorSeedInfoSettingWnd::OnCreate()
{
    guard(NCManorSeedInfoSettingWnd::OnCreate);
    MANOR_SETTING_BASE()->NCManorInfoSettingWnd::OnCreate();   // 0x10136CE0
    AdenaTex = FindTexture(TEXT("L2UI.WindowIcon.WindowIconAdena"));
    for (INT i = 0; i < 3; i++)
        TextBoxTex[i] = FindTexture(*FString::Printf(TEXT("L2UI_CH3.Etc.textbox%d"), i + 1));

    ChangeButton = ManorNewButton(0, 0);
    ChangeButton->ID = NID_MANORSEEDSET_CHANGE;
    ChangeButton->SetPosition(8, 0x129, 0, 0);
    CreateChild(ChangeButton, NWS_VISIBLE);
    ChangeButton->SetTextID(STR_Change);

    TodayButton = ManorNewButton(0, 0);
    TodayButton->ID = NID_MANORSEEDSET_TODAY;
    TodayButton->SetPosition(0x58, 0x129, 0, 0);
    CreateChild(TodayButton, NWS_VISIBLE);
    TodayButton->SetTextID(STR_SetToToday);

    ClearButton = ManorNewButton(0, 0);
    ClearButton->ID = NID_MANORSEEDSET_CLEAR;
    ClearButton->SetPosition(0xA8, 0x129, 0, 0);
    CreateChild(ClearButton, NWS_VISIBLE);
    ClearButton->SetTextID(STR_ClearAll);

    OKButton = ManorNewButton(0, 0);
    OKButton->ID = NID_MANORSEEDSET_OK;
    OKButton->SetPosition(0x1B6, 0x129, 0, 0);
    CreateChild(OKButton, NWS_VISIBLE);
    OKButton->SetTextID(STR_OK);

    CancelButton = ManorNewButton(0, 0);
    CancelButton->ID = NID_MANORSEEDSET_CANCEL;
    CancelButton->SetPosition(0x206, 0x129, 0, 0);
    CreateChild(CancelButton, NWS_VISIBLE);
    CancelButton->SetTextID(STR_Cancel);

    ManorListSetColumns(ListCtrl, 5);
    NManorColumnInfo Column;
    ManorSetTabTextures(Column);
    Column.Width = 0xF9;
    Column.TitleID = 0x460;
    Column.bNumeric = 0;
    ListCtrl->AddColumn(0, (NListColumnInfo*)&Column);
    Column.Width = 0x50;
    Column.TitleID = 0x471;
    Column.bNumeric = 1;
    ListCtrl->AddColumn(1, (NListColumnInfo*)&Column);
    Column.Width = 0x50;
    Column.TitleID = 0x472;
    Column.bNumeric = 1;
    ListCtrl->AddColumn(2, (NListColumnInfo*)&Column);
    Column.Width = 0x50;
    Column.TitleID = 0x473;
    Column.bNumeric = 1;
    ListCtrl->AddColumn(3, (NListColumnInfo*)&Column);
    Column.Width = 0x61;
    Column.TitleID = 0x474;
    Column.bNumeric = 1;
    ListCtrl->AddColumn(4, (NListColumnInfo*)&Column);
    ManorID = -1;
    TotalPrice = 0;
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10139300, NCManorSeedInfoSettingWnd, OnCreate)

// 0x10139970 (vslot 74, ret 4) - a double click on a row is the change button
INT NCManorSeedInfoSettingWnd::OnListCtrl(INT Row)
{
    guard(NCManorSeedInfoSettingWnd::OnListCtrl);
    OnChangeBtnClick();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10139970, NCManorSeedInfoSettingWnd, OnListCtrl)

// 0x101399f0 - OK: the manor, the number of rows, and for each row its seed and the two numbers of the table (the amount
// and the price) go to the server (the stack is deleted afterwards)
INT NCManorSeedInfoSettingWnd::OnOKBtnClick()
{
    guard(NCManorSeedInfoSettingWnd::OnOKBtnClick);
    NManorSeedRows Rows(ListCtrl->GetRows());
    INT Count = Rows.Num();
    L2ParamStack* Params = new L2ParamStack(0x400);
    Params->PushBack((void*)(INT_PTR)ManorID);
    Params->PushBack((void*)(INT_PTR)Count);
    for (INT i = 0; i < Count; i++)
    {
        NListRow* pDataRecord = Rows.Get(i);
        NW_CHECK_AT(pDataRecord, MANOR_SRC, 332);
        Params->PushBack((void*)(INT_PTR)pDataRecord->Param1);
        Params->PushBack((void*)(INT_PTR)appAtoi(ManorCellText(pDataRecord, 3)));
        Params->PushBack((void*)(INT_PTR)appAtoi(ManorCellText(pDataRecord, 4)));
    }
    GConsole->RequestSetSeed(*Params);
    HideWindow();
    if (Params)   // unlike the crop window, which leaves the stack to the console
    {
        Params->~L2ParamStack();
        appFree(Params);
    }
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x101399f0, NCManorSeedInfoSettingWnd, OnOKBtnClick)

// 0x10139b90 - the total of amount * default price (the third number of the row) over the rows
void NCManorSeedInfoSettingWnd::CalcDefaultPriceSum()
{
    guard(NCManorSeedInfoSettingWnd::CalcDefaultPriceSum);
    TotalPrice = 0;
    NManorSeedRows Rows(ListCtrl->GetRows());
    for (INT i = 0; i < Rows.Num(); i++)
    {
        NListRow* pDataRecord = Rows.Get(i);
        NW_CHECK_AT(pDataRecord, MANOR_SRC, 413);
        TotalPrice += pDataRecord->Param3 * appAtoi(ManorCellText(pDataRecord, 3));
    }
    unguard;
}
NW_HOOK_METHOD(0x10139b90, NCManorSeedInfoSettingWnd, CalcDefaultPriceSum)

// 0x10139c80 (ret 8) - the change window's answer for the selected row: the two numbers go to its cells; the total follows
void NCManorSeedInfoSettingWnd::NotifyChangeTomValue(INT Amount, INT Price)
{
    guard(NCManorSeedInfoSettingWnd::NotifyChangeTomValue);
    NListRow* Row = ListCtrl->GetSelectedRow();
    Row->Cells[3].SetText(appItoa(Amount));
    Row->Cells[4].SetText(appItoa(Price));
    CalcDefaultPriceSum();
    unguard;
}
NW_HOOK_METHOD(0x10139c80, NCManorSeedInfoSettingWnd, NotifyChangeTomValue)

// 0x10139d40 (ret 4) - the console's packet: the manor, the number of seeds, then for each seed its id, a value, the items it
// is made from (a count, the ids) and the ones it gives (the same), eight numbers; a row of ten cells
void NCManorSeedInfoSettingWnd::OnShowSeedSetting(L2ParamStack* Params)
{
    guard(NCManorSeedInfoSettingWnd::OnShowSeedSetting);
    ListCtrl->ClearRows();
    ManorID = (INT)Params->Top();
    INT Count = (INT)Params->Top();
    for (INT n = 0; n < Count; n++)
    {
        INT Seed = (INT)Params->Top();
        INT Value = (INT)Params->Top();
        INT NumFirst = (INT)Params->Top();
        FString First(TEXT(""));
        FString Second(TEXT(""));
        for (INT i = 0; i < (signed char)NumFirst; i++)
        {
            INT Item = (INT)Params->Top();
            if (i != 0)
                First = First + TEXT(",");
            First = First + GL2GameData.GetItemName(Item);
        }
        INT NumSecond = (INT)Params->Top();
        for (INT i = 0; i < (signed char)NumSecond; i++)
        {
            INT Item = (INT)Params->Top();
            if (i != 0)
                Second = Second + TEXT(",");
            Second = Second + GL2GameData.GetItemName(Item);
        }
        INT V1 = (INT)Params->Top();
        INT V2 = (INT)Params->Top();
        INT V3 = (INT)Params->Top();
        INT V4 = (INT)Params->Top();
        INT V5 = (INT)Params->Top();
        INT V6 = (INT)Params->Top();
        INT V7 = (INT)Params->Top();
        INT V8 = (INT)Params->Top();
        NListRow* Row = new NListRow(10);
        Row->Cells[0].SetText(GL2GameData.GetItemName(Seed));
        Row->Cells[1].SetText(*FString::Printf(TEXT("%d"), V5));
        Row->Cells[2].SetText(*FString::Printf(TEXT("%d"), V6));
        Row->Cells[3].SetText(*FString::Printf(TEXT("%d"), V7));
        Row->Cells[4].SetText(*FString::Printf(TEXT("%d"), V8));
        Row->Param1 = Seed;
        Row->Param2 = V1;
        Row->Param3 = V2;
        Row->Cells[5].SetText(*FString::Printf(TEXT("%d"), V3));
        Row->Cells[6].SetText(*FString::Printf(TEXT("%d"), V4));
        Row->Cells[7].SetText(*FString::Printf(TEXT("%d"), Value));
        Row->Cells[8].SetText(*First);
        Row->Cells[9].SetText(*Second);
        Row->ToolTip.FillMatchRow(Row, 0x80000);   // 0x10031B10
        ListCtrl->AddRow(Row);
    }
    CalcDefaultPriceSum();
    ShowWindow();
    unguard;
}
NW_HOOK_METHOD(0x10139d40, NCManorSeedInfoSettingWnd, OnShowSeedSetting)

// 0x1013a250 (ret 8) - the clear question was answered: the amount and the price of every row become "0"
INT NCManorSeedInfoSettingWnd::OnClearConfirmMsg(WPARAM wParam, LPARAM lParam)
{
    guard(NCManorSeedInfoSettingWnd::OnClearConfirmMsg);
    NManorSeedRows Rows(ListCtrl->GetRows());
    for (INT i = 0; i < Rows.Num(); i++)
    {
        NListRow* pDataRecord = Rows.Get(i);
        NW_CHECK_AT(pDataRecord, MANOR_SRC, 369);
        pDataRecord->Cells[3].SetText(TEXT("0"));
        pDataRecord->Cells[4].SetText(TEXT("0"));
    }
    CalcDefaultPriceSum();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1013a250, NCManorSeedInfoSettingWnd, OnClearConfirmMsg)

// 0x1013a340 (ret 8) - the today question was answered: the two cells of every row are copied from the two before the
// amounts (the default amount and price)
INT NCManorSeedInfoSettingWnd::OnTodayConfirmMsg(WPARAM wParam, LPARAM lParam)
{
    guard(NCManorSeedInfoSettingWnd::OnTodayConfirmMsg);
    NManorSeedRows Rows(ListCtrl->GetRows());
    for (INT i = 0; i < Rows.Num(); i++)
    {
        NListRow* pDataRecord = Rows.Get(i);
        NW_CHECK_AT(pDataRecord, MANOR_SRC, 390);
        pDataRecord->Cells[3].SetText(ManorCellText(pDataRecord, 1));
        pDataRecord->Cells[4].SetText(ManorCellText(pDataRecord, 2));
    }
    CalcDefaultPriceSum();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1013a340, NCManorSeedInfoSettingWnd, OnTodayConfirmMsg)

NW_CHECK_VTABLE(NCManorSeedInfoSettingWnd, 0x101adf28, 75)
