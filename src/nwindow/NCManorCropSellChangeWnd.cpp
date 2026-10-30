/*=============================================================================
    NCManorCropSellChangeWnd.cpp - the box that sets how many of a crop the player sells to which manor (new in C4).
    Stock (C4): 0x1005F750 (constructor), 0x10133580 .. 0x10134760 (HideWindow, DoModal, the handlers, OnShowProcureCropDetail,
    OnDestroy, OnPaint, OnCreate, OnMaxBtnClick, OnOKBtnClick, WindowProc).
=============================================================================*/
#include "nwindow/NCManorCropSellChangeWnd.h"
#include "nwindow/NCManorCropSellWnd.h"
#include "nwindow/NClasses.gen.h"
#include "nwindow/NGameData.h"
#include "nwindow/NString.h"

// sysstring labels (names ours)
enum
{
    STR_Max = 0x486,
    STR_OK = 140,
    STR_Cancel = 141,
};

// the copy of the list's rows, made and destroyed by the array's copy constructor (0x10130F30; NCManorCropInfoSettingWnd.cpp hooks it) and
// destructor (0x10023580)
struct NManorSellRows
{
    TArray<NListRow*> Copy;
    NManorSellRows(TArray<NListRow*>* Source) : Copy(*Source) {}
    INT Num() { return Copy.Num(); }
    NListRow* Get(INT i) { return Copy(i); }
};

// 0x1005f750 - NCDialogBox's constructor, the vtable and the three empty strings
NCManorCropSellChangeWnd::NCManorCropSellChangeWnd()
{
}
NW_HOOK_CTOR(0x1005f750, NCManorCropSellChangeWnd)

// 0x1005f7f0 - ConditionalDestroy, the three strings, NCDialogBox's destructor
NW_HOOK_DTOR(0x1005f7f0, NCManorCropSellChangeWnd)

// 0x10134760 (vslot 25) - the base's answer first; then the three buttons
INT NCManorCropSellChangeWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCWnd::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND)
    {
        if (wParam == (WPARAM)NID_MANORSELL_MAX)
            return OnMaxBtnClick() != 0;
        if (wParam == (WPARAM)NID_MANORSELL_OK)
            return OnOKBtnClick() != 0;
        if (wParam == (WPARAM)NID_MANORSELL_CANCEL)
            return OnCancelBtnClick() != 0;
    }
    return 0;
}
NW_HOOK_METHOD(0x10134760, NCManorCropSellChangeWnd, WindowProc)

// 0x10133580 (vslot 49) - the modal window goes
void NCManorCropSellChangeWnd::HideWindow()
{
    guard(NCManorCropSellChangeWnd::HideWindow);
    NCDialogBox::HideModal();
    unguard;
}
NW_HOOK_METHOD(0x10133580, NCManorCropSellChangeWnd, HideWindow)

// 0x101335f0 (vslot 75) - the amount edit is emptied, the first manor chosen, the edit takes the focus; the window comes up modal
void NCManorCropSellChangeWnd::NewVirtual75()
{
    guard(NCManorCropSellChangeWnd::DoModal);
    AmountEdit->Left = TEXT("");
    ManorCombo->SetSelected(0);
    AmountEdit->SetFocus();
    NCDialogBox::DoModal();
    unguard;
}
NW_HOOK_METHOD(0x101335f0, NCManorCropSellChangeWnd, NewVirtual75)

// 0x101336a0
INT NCManorCropSellChangeWnd::OnCancelBtnClick()
{
    guard(NCManorCropSellChangeWnd::OnCancelBtnClick);
    NCDialogBox::HideModal();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x101336a0, NCManorCropSellChangeWnd, OnCancelBtnClick)

// 0x10133720 (ret 4) - the console's packet: one value skipped, the number of manors, then for each the manor, three
// numbers (the amount it wants, its price, its limit); a row of four cells (the manor's name and the numbers), and the manor
// goes into the combo box with its id; the window comes up
void NCManorCropSellChangeWnd::OnShowProcureCropDetail(L2ParamStack* Params)
{
    guard(NCManorCropSellChangeWnd::OnShowProcureCropDetail);
    ListCtrl->ClearRows();
    Params->Top();
    INT Count = (INT)Params->Top();
    ManorComboClear(ManorCombo);
    for (INT n = 0; n < Count; n++)
    {
        INT Manor = (INT)Params->Top();
        INT Value1 = (INT)Params->Top();
        INT Value2 = (INT)Params->Top();
        INT Value3 = (INT)Params->Top();
        const TCHAR* Name = GConsole->GetManorName(Manor);
        NListRow* Row = new NListRow(4);
        Row->Cells[0].SetText(Name);
        Row->Cells[1].SetText(*FString::Printf(TEXT("%d"), Value1));
        Row->Cells[2].SetText(*FString::Printf(TEXT("%d"), Value2));
        Row->Cells[3].SetText(*FString::Printf(TEXT("%d"), Value3));
        Row->Param1 = Manor;
        ListCtrl->AddRow(Row);
        ManorComboAddText(ManorCombo, Name, Manor);
    }
    NewVirtual75();
    unguard;
}
NW_HOOK_METHOD(0x10133720, NCManorCropSellChangeWnd, OnShowProcureCropDetail)

// 0x10133940 (vslot 38) - the three strings are emptied, the six controls go, then the base's
INT NCManorCropSellChangeWnd::OnDestroy()
{
    guard(NCManorCropSellChangeWnd::OnDestroy);
    Text1.Empty();
    Text2.Empty();
    Text3.Empty();
    ManorDestroyChild(this, MaxButton);
    ManorDestroyChild(this, OKButton);
    ManorDestroyChild(this, CancelButton);
    ManorDestroyChild(this, ManorCombo);
    ManorDestroyChild(this, AmountEdit);
    ManorDestroyChild(this, ListCtrl);
    NCDialogBox::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10133940, NCManorCropSellChangeWnd, OnDestroy)

// 0x10133af0 (vslot 62) - the back texture under the title bar, then the labels of the box (grey) with the owner's values
// (gold) beside the first three, and the two column headings of the list's table
INT NCManorCropSellChangeWnd::OnPaint(NCGDevice* Device)
{
    guard(NCManorCropSellChangeWnd::OnPaint);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    Device->BeginPaint(ScreenX, ScreenY);
    if (BackTex)
        Device->DrawTexture(0, 0x14, (INT)Width, (INT)(Height - 20.0f), 0, 0, (INT)Width, (INT)(Height - 20.0f), BackTex, 0xFF, 1);
    ManorDrawLabel(Device, 0xF, 0x26, 0xFFDCDCDC, *::nw::Field<FString>(&GL2GameData, 0x9BBCC));
    ManorDrawLabel(Device, 0x4C, 0x26, 0xFFB09B79, *Text1);
    ManorDrawLabel(Device, 0xF, 0x36, 0xFFDCDCDC, *::nw::Field<FString>(&GL2GameData, 0x9BC44));
    ManorDrawLabel(Device, 0x4C, 0x36, 0xFFB09B79, *Text2);
    ManorDrawLabel(Device, 0xF, 0x46, 0xFFDCDCDC, *::nw::Field<FString>(&GL2GameData, 0x9BC50));
    ManorDrawLabel(Device, 0x4C, 0x46, 0xFFB09B79, *Text3);
    ManorDrawLabel(Device, 0x4B, 0xDB, 0xFFDCDCDC, *::nw::Field<FString>(&GL2GameData, 0x9BD28));
    ManorDrawLabel(Device, 0x4B, 0xF4, 0xFFDCDCDC, *::nw::Field<FString>(&GL2GameData, 0x9BD4C));
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10133af0, NCManorCropSellChangeWnd, OnPaint)

// 0x10133d60 (vslot 37) - the base's OnCreate, the title bar, the back texture, the Max button (the small style), OK and
// Cancel, the amount edit, the combo box of the manors, and the list with its four columns
INT NCManorCropSellChangeWnd::OnCreate()
{
    guard(NCManorCropSellChangeWnd::OnCreate);
    NCWnd::OnCreate();
    FrameCtrl = ManorNewFrameCtrl(1, 0, 0);
    FrameID = (INT)0xF0000A40;
    FrameCtrl->ID = (INT)0xF0000A40;
    FrameCtrl->SetPosition(0, 0, (INT)Width, 0x14);
    CreateChild(FrameCtrl, NWS_VISIBLE);
    BackTex = FindTexture(TEXT("L2UI_CH3.Manor.manor3_back"));

    MaxButton = ManorNewButton(1, 0);
    MaxButton->ID = NID_MANORSELL_MAX;
    MaxButton->SetPosition(0xC4, 0xF0, 0, 0);
    CreateChild(MaxButton, NWS_VISIBLE);
    MaxButton->SetTextID(STR_Max);

    OKButton = ManorNewButton(0, 0);
    OKButton->ID = NID_MANORSELL_OK;
    OKButton->SetPosition(0x5B, 0x115, 0, 0);
    CreateChild(OKButton, NWS_VISIBLE);
    OKButton->SetTextID(STR_OK);

    CancelButton = ManorNewButton(0, 0);
    CancelButton->ID = NID_MANORSELL_CANCEL;
    CancelButton->SetPosition(0xAB, 0x115, 0, 0);
    CreateChild(CancelButton, NWS_VISIBLE);
    CancelButton->SetTextID(STR_Cancel);

    AmountEdit = new NCEditBox(0, NULL, 1, 0x108, 0);
    AmountEdit->ID = (INT)0xF0000A20;
    AmountEdit->SetPosition(0x7B, 0xF1, 0x44, 0x11);
    CreateChild(AmountEdit, NWS_VISIBLE);
    AmountEdit->SetMaxLength(10);

    ManorCombo = new NCComboBox();
    ManorCombo->ID = (INT)0xF0000A30;
    ManorCombo->SetPosition(0x7B, 0xD8, 0x8A, 0x11);
    CreateChild(ManorCombo, NWS_VISIBLE);
    ManorCombo->SetSelected(0);

    ListCtrl = ManorNewListCtrl(0x3E8, 5, 4, 0x10, 0, 0x12);
    ListCtrl->ID = (INT)0xF0000A00;
    ListCtrl->SetPosition(7, 0x5C, 0x142, 0x6D);
    CreateChild(ListCtrl, NWS_VISIBLE);
    ListCtrl->SetTextOffset(0, 3);

    NManorColumnInfo Column;
    ManorSetTabTextures(Column);
    Column.Width = 0x73;
    Column.TitleID = 0x481;
    Column.bNumeric = 0;
    ListCtrl->AddColumn(0, (NListColumnInfo*)&Column);
    Column.Width = 0x46;
    Column.TitleID = 0x482;
    Column.bNumeric = 1;
    ListCtrl->AddColumn(1, (NListColumnInfo*)&Column);
    Column.Width = 0x46;
    Column.TitleID = 0x466;
    Column.bNumeric = 1;
    ListCtrl->AddColumn(2, (NListColumnInfo*)&Column);
    Column.Width = 0x43;
    Column.TitleID = 0x467;
    Column.bNumeric = 0;
    ListCtrl->AddColumn(3, (NListColumnInfo*)&Column);
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10133d60, NCManorCropSellChangeWnd, OnCreate)

// 0x10134490 - Max: the amount of the crop the player has (NCManorCropSellWnd::GetMyCropCnt), at most what the manor
// selected in the combo box takes (its row's first number), goes into the amount edit
INT NCManorCropSellChangeWnd::OnMaxBtnClick()
{
    guard(NCManorCropSellChangeWnd::OnMaxBtnClick);
    INT Manor = ManorComboData(ManorCombo, ManorCombo->Selected);
    INT Limit = -1;
    NManorSellRows Rows(ListCtrl->GetRows());
    for (INT i = 0; i < Rows.Num(); i++)
    {
        NListRow* Row = Rows.Get(i);
        if (Row && Row->Param1 == Manor)
        {
            Limit = appAtoi(ManorCellText(Row, 1));
            break;
        }
    }
    if (Limit != -1)
    {
        INT Have = ::nw::Field<NCManorCropSellWnd*>(GConsole->Desktop, 0x1F8)->GetMyCropCnt();   // 0x10134E80
        INT Amount = Limit;
        if (Limit > Have)
            Amount = Have;
        AmountEdit->Left = FString::Printf(TEXT("%d"), Amount);
    }
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10134490, NCManorCropSellChangeWnd, OnMaxBtnClick)

// 0x101345e0 - OK: the amount typed, the manor chosen and its row's three numbers go to the sell window
// (NCManorCropSellWnd::NotifyChangeSellValue), this window closes
INT NCManorCropSellChangeWnd::OnOKBtnClick()
{
    guard(NCManorCropSellChangeWnd::OnOKBtnClick);
    INT Manor = ManorComboData(ManorCombo, ManorCombo->Selected);
    INT Value1 = 0;
    INT Value2 = 0;
    INT Value3 = 0;
    NManorSellRows Rows(ListCtrl->GetRows());
    for (INT i = 0; i < Rows.Num(); i++)
    {
        NListRow* Row = Rows.Get(i);
        if (Row && Row->Param1 == Manor)
        {
            Value1 = appAtoi(ManorCellText(Row, 1));
            Value2 = appAtoi(ManorCellText(Row, 2));
            Value3 = appAtoi(ManorCellText(Row, 3));
            break;
        }
    }
    INT Amount = appAtoi(*AmountEdit->Left);
    ::nw::Field<NCManorCropSellWnd*>(GConsole->Desktop, 0x1F8)->NotifyChangeSellValue(Amount, Manor, Value1, Value2, Value3);   // 0x10134800
    NCDialogBox::HideModal();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x101345e0, NCManorCropSellChangeWnd, OnOKBtnClick)

NW_CHECK_VTABLE(NCManorCropSellChangeWnd, 0x101ae2b8, 76)
