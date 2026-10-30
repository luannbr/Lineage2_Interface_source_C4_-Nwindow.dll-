/*=============================================================================
    NCManorCropInfoSettingWnd.cpp - the crop prices of the lord's manor, to be set (new in C4).
    Stock (C4): 0x1005F610 (constructor), 0x10130780 .. 0x101322D0 (the handlers, OnDestroy, OnPaint, OnCreate, OnListCtrl,
    CalcDefaultPriceSum, NotifyChangeTomValue, OnShowCropSetting, the confirmations, WindowProc), 0x10130F30 (the copy of
    the list's row array).
=============================================================================*/
#include "nwindow/NCManorCropInfoSettingWnd.h"
#include "nwindow/NCManorInfoSettingWnd.h"
#include "nwindow/NCManorCropInfoChangeWnd.h"
#include "nwindow/NClasses.gen.h"
#include "nwindow/NDialogInfo.h"
#include "nwindow/NGameData.h"
#include "nwindow/NString.h"

// sysstring labels and system messages
enum
{
    STR_Change = 0x476,
    STR_SetToToday = 0x477,
    STR_ClearAll = 0x47F,
    STR_OK = 140,
    STR_Cancel = 141,
    MSG_ClearTable = 0x640,
    MSG_SetToToday = 0x641,
};

// dialog ids (names ours): the answers come back as NWM_MANORSET_CLEARCONFIRM / TODAYCONFIRM
enum
{
    DIALOG_MANOR_CLEAR = 0x56,
    DIALOG_MANOR_TODAY = 0x57,
};

// the NManorCropInfoSettingWnd.cpp the stock code reports a null row from
#define MANOR_SRC "Src\\NManorCropInfoSettingWnd.cpp"

// 0x10130f30 (ret 4; anonymous in the db) - the copy constructor of the list's array of rows (the console and the owners
// take a copy before they walk it)
typedef TArray<NListRow*> TManorRowArray;
NW_HOOK_CTOR_ARGS(0x10130f30, TManorRowArray, (const TManorRowArray& Other), (Other))

// the copy of the list's rows, made and destroyed by the array's copy constructor (0x10130F30) and destructor (0x10023580)
struct NManorRows
{
    TArray<NListRow*> Copy;
    NManorRows(TArray<NListRow*>* Source) : Copy(*Source) {}
    INT Num() { return Copy.Num(); }
    NListRow* Get(INT i) { return Copy(i); }
};

// 0x1005f610
NCManorCropInfoSettingWnd::NCManorCropInfoSettingWnd()
{
}
NW_HOOK_CTOR(0x1005f610, NCManorCropInfoSettingWnd)

// 0x1005f660 - ConditionalDestroy, then NCWnd's destructor (the stock one calls NCManorInfoSettingWnd's, which does the same)
NW_HOOK_DTOR(0x1005f660, NCManorCropInfoSettingWnd)

// 0x101322d0 (vslot 25) - the shared body first (the list's command); then the five buttons and the answers of the two
// questions
INT NCManorCropInfoSettingWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (((NCManorInfoSettingWnd*)this)->NCManorInfoSettingWnd::WindowProc(Msg, wParam, lParam))   // 0x10136AD0, the shared body
        return 1;
    if (Msg == NWM_COMMAND)
    {
        if (wParam == (WPARAM)NID_MANORSET_CHANGE)
            return OnChangeBtnClick() != 0;
        if (wParam == (WPARAM)NID_MANORSET_TODAY)
            return OnSetToTodayBtnClick() != 0;
        if (wParam == (WPARAM)NID_MANORSET_CLEAR)
            return OnClearBtnClick() != 0;
        if (wParam == (WPARAM)NID_MANORSET_OK)
            return OnOKBtnClick() != 0;
        if (wParam == (WPARAM)NID_MANORSET_CANCEL)
            return OnCancelBtnClick() != 0;
    }
    else if (Msg == (UINT)NWM_MANORSET_CLEARCONFIRM)
        return OnClearConfirmMsg(wParam, lParam) != 0;
    else if (Msg == (UINT)NWM_MANORSET_TODAYCONFIRM)
        return OnTodayConfirmMsg(wParam, lParam) != 0;
    return 0;
}
NW_HOOK_METHOD(0x101322d0, NCManorCropInfoSettingWnd, WindowProc)

// 0x10130780 - closes the window
INT NCManorCropInfoSettingWnd::OnCancelBtnClick()
{
    guard(NCManorCropInfoSettingWnd::OnCancelBtnClick);
    HideWindow();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10130780, NCManorCropInfoSettingWnd, OnCancelBtnClick)

// 0x10130800 (vslot 38) - the five buttons, then the shared body (the list, the base)
INT NCManorCropInfoSettingWnd::OnDestroy()
{
    guard(NCManorCropInfoSettingWnd::OnDestroy);
    ManorDestroyChild(this, ChangeButton);
    ManorDestroyChild(this, TodayButton);
    ManorDestroyChild(this, ClearButton);
    ManorDestroyChild(this, OKButton);
    ManorDestroyChild(this, CancelButton);
    ((NCManorInfoSettingWnd*)this)->NCManorInfoSettingWnd::OnDestroy();   // 0x10136B30
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10130800, NCManorCropInfoSettingWnd, OnDestroy)

// 0x10130950 (vslot 62) - the shared body draws the back texture; then the adena icon and the three boxes of the total,
// the manor's name, the label of the total and the total itself
INT NCManorCropInfoSettingWnd::OnPaint(NCGDevice* Device)
{
    guard(NCManorCropInfoSettingWnd::OnPaint);
    ((NCManorInfoSettingWnd*)this)->NCManorInfoSettingWnd::OnPaint(Device);   // 0x10136BE0
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    Device->BeginPaint(ScreenX, ScreenY);
    Device->DrawTexture(0x6B, 0x116, 0x10, 0xC, 0, 0, 0x10, 0xC, AdenaTex, 0xFF, 1);
    Device->DrawTexture(0x7E, 0x114, 8, 0x11, 0, 0, 8, 0x11, TextBoxTex[0], 0xFF, 1);
    Device->DrawTexture(0x86, 0x114, 0x46, 0x11, 0, 0, 8, 0x11, TextBoxTex[1], 0xFF, 1);
    Device->DrawTexture(0xCC, 0x114, 8, 0x11, 0, 0, 8, 0x11, TextBoxTex[2], 0xFF, 1);
    ManorDrawLabel(Device, 0x10, 0x2F, 0xFFDCDCDC, GConsole->GetManorName(ManorID));
    ManorDrawLabel(Device, 0x10, 0x117, 0xFFDCDCDC, *::nw::Field<FString>(&GL2GameData, 0x9BC98));
    FString Total = ManorMakeNumStr(TotalPrice);
    ManorDrawLabel(Device, 0x82, 0x117, 0xFFDCDCDC, *Total);
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10130950, NCManorCropInfoSettingWnd, OnPaint)

// 0x10130b60 - the selected row goes to the change window (the desktop's, +0x204): the crop name, the most it can take, the
// amount, the price and its kind, and the limits of the price; then the window comes up
INT NCManorCropInfoSettingWnd::OnChangeBtnClick()
{
    guard(NCManorCropInfoSettingWnd::OnChangeBtnClick);
    NCManorCropInfoChangeWnd* Change = (NCManorCropInfoChangeWnd*)ManorDesktopWnd(0x204);
    if (Change)
    {
        NListRow* Row = ListCtrl->GetSelectedRow();
        if (Row)
        {
            Change->SetCropName(FString(ManorCellText(Row, 0)));
            Change->SetMaxAmount(Row->Param2);
            Change->SetAmt(FString(ManorCellText(Row, 4)));
            Change->SetPriceText(FString(ManorCellText(Row, 5)));
            Change->SetPriceKind(appAtoi(ManorCellText(Row, 6)));
            const TCHAR* Cell8 = ManorCellText(Row, 8);
            const TCHAR* Cell7 = ManorCellText(Row, 7);
            INT Limit8 = appAtoi(Cell8);
            INT Limit7 = appAtoi(Cell7);
            Change->SetLimits(Limit7, Limit8);
            Change->NewVirtual75();   // DoModal
        }
    }
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10130b60, NCManorCropInfoSettingWnd, OnChangeBtnClick)

// 0x10130d70 - asks whether to set every price to today's (the answer comes back as NWM_MANORSET_TODAYCONFIRM)
INT NCManorCropInfoSettingWnd::OnSetToTodayBtnClick()
{
    guard(NCManorCropInfoSettingWnd::OnSetToTodayBtnClick);
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
NW_HOOK_METHOD(0x10130d70, NCManorCropInfoSettingWnd, OnSetToTodayBtnClick)

// 0x10130e50 - asks whether to clear the table
INT NCManorCropInfoSettingWnd::OnClearBtnClick()
{
    guard(NCManorCropInfoSettingWnd::OnClearBtnClick);
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
NW_HOOK_METHOD(0x10130e50, NCManorCropInfoSettingWnd, OnClearBtnClick)

// 0x10131710 - OK: the manor, the number of rows, and for each row its crop and the three numbers of the table (the
// amount, the price and the kind) go to the server (the console keeps the stack)
INT NCManorCropInfoSettingWnd::OnOKBtnClick()
{
    guard(NCManorCropInfoSettingWnd::OnOKBtnClick);
    NManorRows Rows(ListCtrl->GetRows());
    INT Count = Rows.Num();
    L2ParamStack* Params = new L2ParamStack(0x400);
    Params->PushBack((void*)(INT_PTR)ManorID);
    Params->PushBack((void*)(INT_PTR)Count);
    for (INT i = 0; i < Count; i++)
    {
        NListRow* pDataRecord = Rows.Get(i);
        NW_CHECK_AT(pDataRecord, MANOR_SRC, 349);
        INT Crop = pDataRecord->Param1;
        INT Amount = appAtoi(ManorCellText(pDataRecord, 4));
        INT Price = appAtoi(ManorCellText(pDataRecord, 5));
        INT Kind = appAtoi(ManorCellText(pDataRecord, 6));
        Params->PushBack((void*)(INT_PTR)Crop);
        Params->PushBack((void*)(INT_PTR)Amount);
        Params->PushBack((void*)(INT_PTR)Price);
        Params->PushBack((void*)(INT_PTR)(signed char)Kind);
    }
    GConsole->RequestSetCrop(*Params);   // 0x1006DD10
    HideWindow();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10131710, NCManorCropInfoSettingWnd, OnOKBtnClick)

// 0x101318e0 - the total of amount * price over the rows
void NCManorCropInfoSettingWnd::CalcDefaultPriceSum()
{
    guard(NCManorCropInfoSettingWnd::CalcDefaultPriceSum);
    TotalPrice = 0;
    NManorRows Rows(ListCtrl->GetRows());
    for (INT i = 0; i < Rows.Num(); i++)
    {
        NListRow* pDataRecord = Rows.Get(i);
        NW_CHECK_AT(pDataRecord, MANOR_SRC, 436);
        INT Amount = appAtoi(ManorCellText(pDataRecord, 4));
        INT Price = appAtoi(ManorCellText(pDataRecord, 5));
        TotalPrice += Price * Amount;
    }
    unguard;
}
NW_HOOK_METHOD(0x101318e0, NCManorCropInfoSettingWnd, CalcDefaultPriceSum)

// 0x101319f0 (ret 0xc) - the change window's answer for the selected row: the three numbers go to its cells; the total follows
void NCManorCropInfoSettingWnd::NotifyChangeTomValue(INT Amount, INT Price, INT Limit)
{
    guard(NCManorCropInfoSettingWnd::NotifyChangeTomValue);
    NListRow* Row = ListCtrl->GetSelectedRow();
    if (Row)
    {
        Row->Cells[4].SetText(appItoa(Amount));
        Row->Cells[5].SetText(appItoa(Price));
        Row->Cells[6].SetText(appItoa(Limit));
    }
    CalcDefaultPriceSum();
    unguard;
}
NW_HOOK_METHOD(0x101319f0, NCManorCropInfoSettingWnd, NotifyChangeTomValue)

// 0x10130fc0 (vslot 37) - the shared body (the back texture, the list), the adena icon, the three textures of the total's
// box, the five buttons and the seven columns of the list
INT NCManorCropInfoSettingWnd::OnCreate()
{
    guard(NCManorCropInfoSettingWnd::OnCreate);
    ((NCManorInfoSettingWnd*)this)->NCManorInfoSettingWnd::OnCreate();   // 0x10136CE0
    AdenaTex = FindTexture(TEXT("L2UI.WindowIcon.WindowIconAdena"));
    for (INT i = 0; i < 3; i++)
        TextBoxTex[i] = FindTexture(*FString::Printf(TEXT("L2UI_CH3.Etc.textbox%d"), i + 1));

    ChangeButton = ManorNewButton(0, 0);
    ChangeButton->ID = NID_MANORSET_CHANGE;
    ChangeButton->SetPosition(8, 0x129, 0, 0);
    CreateChild(ChangeButton, NWS_VISIBLE);
    ChangeButton->SetTextID(STR_Change);

    TodayButton = ManorNewButton(0, 0);
    TodayButton->ID = NID_MANORSET_TODAY;
    TodayButton->SetPosition(0x58, 0x129, 0, 0);
    CreateChild(TodayButton, NWS_VISIBLE);
    TodayButton->SetTextID(STR_SetToToday);

    ClearButton = ManorNewButton(0, 0);
    ClearButton->ID = NID_MANORSET_CLEAR;
    ClearButton->SetPosition(0xA8, 0x129, 0, 0);
    CreateChild(ClearButton, NWS_VISIBLE);
    ClearButton->SetTextID(STR_ClearAll);

    OKButton = ManorNewButton(0, 0);
    OKButton->ID = NID_MANORSET_OK;
    OKButton->SetPosition(0x1B6, 0x129, 0, 0);
    CreateChild(OKButton, NWS_VISIBLE);
    OKButton->SetTextID(STR_OK);

    CancelButton = ManorNewButton(0, 0);
    CancelButton->ID = NID_MANORSET_CANCEL;
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
    Column.TitleID = 0x47A;
    Column.bNumeric = 1;
    ListCtrl->AddColumn(1, (NListColumnInfo*)&Column);
    Column.Width = 0x46;
    Column.TitleID = 0x47B;
    Column.bNumeric = 1;
    ListCtrl->AddColumn(2, (NListColumnInfo*)&Column);
    Column.Width = 0x46;
    Column.TitleID = 0x47C;
    Column.bNumeric = 0;
    ListCtrl->AddColumn(3, (NListColumnInfo*)&Column);
    Column.Width = 0x46;
    Column.TitleID = 0x4C9;
    Column.bNumeric = 1;
    ListCtrl->AddColumn(4, (NListColumnInfo*)&Column);
    Column.Width = 0x46;
    Column.TitleID = 0x47D;
    Column.bNumeric = 1;
    ListCtrl->AddColumn(5, (NListColumnInfo*)&Column);
    Column.Width = 0x57;
    Column.TitleID = 0x47E;
    Column.bNumeric = 0;
    ListCtrl->AddColumn(6, (NListColumnInfo*)&Column);
    ManorID = -1;
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10130fc0, NCManorCropInfoSettingWnd, OnCreate)

// 0x10131690 (vslot 74, ret 4) - a double click on a row is the change button
INT NCManorCropInfoSettingWnd::OnListCtrl(INT Row)
{
    guard(NCManorCropInfoSettingWnd::OnListCtrl);
    OnChangeBtnClick();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10131690, NCManorCropInfoSettingWnd, OnListCtrl)

// 0x10131ad0 (ret 4) - the console's packet: the manor, the number of crops, then for each crop its id, a value, the seeds it
// is made from (a count, the ids) and the crops it gives (the same), ten numbers; a row of twelve cells
void NCManorCropInfoSettingWnd::OnShowCropSetting(L2ParamStack* Params)
{
    guard(NCManorCropInfoSettingWnd::OnShowCropSetting);
    ListCtrl->ClearRows();
    ManorID = (INT)Params->Top();
    INT Count = (INT)Params->Top();
    for (INT n = 0; n < Count; n++)
    {
        INT Crop = (INT)Params->Top();
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
        INT V9 = (INT)Params->Top();
        INT V10 = (INT)Params->Top();
        NListRow* Row = new NListRow(12);
        Row->Param1 = Crop;
        Row->Param2 = V1;
        Row->Param3 = V2;
        Row->Cells[0].SetText(GL2GameData.GetItemName(Crop));
        Row->Cells[1].SetText(*FString::Printf(TEXT("%d"), V5));
        Row->Cells[2].SetText(*FString::Printf(TEXT("%d"), V6));
        Row->Cells[3].SetText(*FString::Printf(TEXT("%d"), V7));
        Row->Cells[4].SetText(*FString::Printf(TEXT("%d"), V8));
        Row->Cells[5].SetText(*FString::Printf(TEXT("%d"), V9));
        Row->Cells[6].SetText(*FString::Printf(TEXT("%d"), V10));
        Row->Cells[7].SetText(*FString::Printf(TEXT("%d"), V3));
        Row->Cells[8].SetText(*FString::Printf(TEXT("%d"), V4));
        Row->Cells[9].SetText(*FString::Printf(TEXT("%d"), Value));
        Row->Cells[10].SetText(*First);
        Row->Cells[11].SetText(*Second);
        Row->ToolTip.FillMatchRow(Row, 0x100000);   // 0x10031B10
        ListCtrl->AddRow(Row);
    }
    CalcDefaultPriceSum();
    ShowWindow();
    unguard;
}
NW_HOOK_METHOD(0x10131ad0, NCManorCropInfoSettingWnd, OnShowCropSetting)

// 0x10132080 (ret 8) - the clear question was answered: the amount, the price and the kind of every row become "0"
INT NCManorCropInfoSettingWnd::OnClearConfirmMsg(WPARAM wParam, LPARAM lParam)
{
    guard(NCManorCropInfoSettingWnd::OnClearConfirmMsg);
    NManorRows Rows(ListCtrl->GetRows());
    for (INT i = 0; i < Rows.Num(); i++)
    {
        NListRow* pDataRecord = Rows.Get(i);
        NW_CHECK_AT(pDataRecord, MANOR_SRC, 390);
        pDataRecord->Cells[4].SetText(TEXT("0"));
        pDataRecord->Cells[5].SetText(TEXT("0"));
        pDataRecord->Cells[6].SetText(TEXT("0"));
    }
    CalcDefaultPriceSum();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10132080, NCManorCropInfoSettingWnd, OnClearConfirmMsg)

// 0x10132180 (ret 8) - the today question was answered: the three cells of every row are copied from the three before
// the amounts (the default amount, price and kind)
INT NCManorCropInfoSettingWnd::OnTodayConfirmMsg(WPARAM wParam, LPARAM lParam)
{
    guard(NCManorCropInfoSettingWnd::OnTodayConfirmMsg);
    NManorRows Rows(ListCtrl->GetRows());
    for (INT i = 0; i < Rows.Num(); i++)
    {
        NListRow* pDataRecord = Rows.Get(i);
        NW_CHECK_AT(pDataRecord, MANOR_SRC, 412);
        pDataRecord->Cells[4].SetText(ManorCellText(pDataRecord, 1));
        pDataRecord->Cells[5].SetText(ManorCellText(pDataRecord, 2));
        pDataRecord->Cells[6].SetText(ManorCellText(pDataRecord, 3));
    }
    CalcDefaultPriceSum();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10132180, NCManorCropInfoSettingWnd, OnTodayConfirmMsg)

NW_CHECK_VTABLE(NCManorCropInfoSettingWnd, 0x101ae058, 75)
