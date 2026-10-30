/*=============================================================================
    NCManorCropInfoWnd.cpp - the crop prices of a manor (new in C4).
    Stock (C4): 0x101323E0 .. 0x10133570 (ShowWindow, the button and combo handlers, OnShowCropInfo, OnDestroy, OnPaint,
    OnCreate, WindowProc, constructor, destructor), 0x101334D0 (constructor).
=============================================================================*/
#include "nwindow/NCManorCropInfoWnd.h"
#include "nwindow/NCManorInfoDownWnd.h"
#include "nwindow/NClasses.gen.h"
#include "nwindow/NGameData.h"
#include "nwindow/NString.h"

// sysstring labels
enum
{
    STR_Sell = 0x46A,
    STR_ThisMonth = 0x45C,
    STR_NextMonth = 0x45D,
};

// 0x101334d0 - NCWnd's constructor and the vtable (the stock one calls the base's NCManorInfoDownWnd constructor, which
// does the same)
NCManorCropInfoWnd::NCManorCropInfoWnd()
{
}
NW_HOOK_CTOR(0x101334d0, NCManorCropInfoWnd)

// 0x10133520 - ConditionalDestroy, then NCWnd's destructor (the stock one calls NCManorInfoDownWnd's 0x10133430, which does the same)
NW_HOOK_DTOR(0x10133520, NCManorCropInfoWnd)

// 0x10133390 (vslot 25) - the base's answer first; then the two combo boxes (their id 0xFFDECCB1 / 0xFFDECCB0) ask for
// another page, the sell button for the sell page
INT NCManorCropInfoWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCWnd::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND)
    {
        if (wParam == (WPARAM)NID_MANORCROP_MANORCOMBO)
            return OnManorComboBox(lParam) != 0;
        if (wParam == (WPARAM)NID_MANORCROP_TIMECOMBO)
            return OnTimeComboBox(lParam) != 0;
        if (wParam == (WPARAM)NID_MANORCROP_SELL)
            return OnSellCropBtnClick() != 0;
    }
    return 0;
}
NW_HOOK_METHOD(0x10133390, NCManorCropInfoWnd, WindowProc)

// 0x101323e0 (vslot 48) - the base's; on the player's own manor's page the sell button stays hidden
void NCManorCropInfoWnd::ShowWindow()
{
    guard(NCManorCropInfoWnd::ShowWindow);
    NCWnd::ShowWindow();
    if (bOwnManor == 1)
        SellButton->HideWindow();
    unguard;
}
NW_HOOK_METHOD(0x101323e0, NCManorCropInfoWnd, ShowWindow)

// 0x10132470 - asks the server for the sell page ("manor_menu_select", ask 2)
INT NCManorCropInfoWnd::OnSellCropBtnClick()
{
    guard(NCManorCropInfoWnd::OnSellCropBtnClick);
    TCHAR Bypass[1024];
    appSprintf(Bypass, TEXT("manor_menu_select?ask=2&state=-1&time=0"));
    L2ParamStack Params(10);
    Params.PushBack((void*)Bypass);
    GConsole->RequestBypassToServer(Params);
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10132470, NCManorCropInfoWnd, OnSellCropBtnClick)

// 0x10132540 - the manor combo box lists the console's manors (a record of 16 bytes: the id first) by name, and shows
// the one of this window
void NCManorCropInfoWnd::ValidateManorComboBox()
{
    guard(NCManorCropInfoWnd::ValidateManorComboBox);
    ManorComboClear(ManorCombo);
    for (INT i = 0; i < ::nw::Field<INT>(GConsole, 0x3C24); i++)
    {
        INT Manor = *(INT*)(::nw::Field<BYTE*>(GConsole, 0x3C20) + i * 16);
        ManorComboAddText(ManorCombo, GConsole->GetManorName(Manor), Manor);
    }
    for (INT i = 0; i < ManorCombo->Items.Num(); i++)
    {
        if (ManorID == ManorComboData(ManorCombo, i))
        {
            ManorCombo->SetSelected(i);
            break;
        }
    }
    unguard;
}
NW_HOOK_METHOD(0x10132540, NCManorCropInfoWnd, ValidateManorComboBox)

// 0x10132630 (ret 4) - the console's packet: whether this is the player's own manor, the manor, one value skipped, the
// number of crops, then for each crop seven values (the crop, two prices, an amount, the type, ...), the seeds it comes
// from and the crops it gives (a count each, then the ids; the names joined by commas), one list row of eight cells
void NCManorCropInfoWnd::OnShowCropInfo(L2ParamStack* Params)
{
    guard(NCManorCropInfoWnd::OnShowCropInfo);
    ListCtrl->ClearRows();
    bOwnManor = (BYTE)(INT)Params->Top();
    ManorID = (INT)Params->Top();
    Params->Top();
    INT Count = (INT)Params->Top();
    for (INT n = 0; n < Count; n++)
    {
        INT Crop = (INT)Params->Top();
        INT Value1 = (INT)Params->Top();
        INT Value2 = (INT)Params->Top();
        INT Value3 = (INT)Params->Top();
        BYTE Type = (BYTE)(INT)Params->Top();
        INT Value4 = (INT)Params->Top();
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
        BYTE NumSecond = (BYTE)(INT)Params->Top();
        for (INT i = 0; i < (signed char)NumSecond; i++)
        {
            INT Item = (INT)Params->Top();
            if (i != 0)
                Second = Second + TEXT(",");
            Second = Second + GL2GameData.GetItemName(Item);
        }
        NListRow* Row = new NListRow(8);
        Row->Cells[0].SetText(GL2GameData.GetItemName(Crop));
        Row->Cells[1].SetText(*FString::Printf(TEXT("%d"), Value1));
        Row->Cells[2].SetText(*FString::Printf(TEXT("%d"), Value2));
        Row->Cells[3].SetText(*FString::Printf(TEXT("%d"), Value3));
        Row->Cells[4].SetText(*FString::Printf(TEXT("%d"), (signed char)Type));
        Row->Cells[5].SetText(*FString::Printf(TEXT("%d"), Value4));
        Row->Cells[6].SetText(*First);
        Row->Cells[7].SetText(*Second);
        Row->ToolTip.FillMatchRow(Row, 0x40000);   // 0x10031B10
        ListCtrl->AddRow(Row);
    }
    ValidateManorComboBox();
    if (bOwnManor == 1)
        SellButton->HideWindow();
    else
        SellButton->ShowWindow();
    unguard;
}
NW_HOOK_METHOD(0x10132630, NCManorCropInfoWnd, OnShowCropInfo)

// 0x10132ab0 - the manor the combo box shows
INT NCManorCropInfoWnd::GetSelectedManorID()
{
    guard(NCManorCropInfoWnd::GetSelectedManorID);
    return ManorComboData(ManorCombo, ManorCombo->Selected);
    unguard;
}
NW_HOOK_METHOD(0x10132ab0, NCManorCropInfoWnd, GetSelectedManorID)

// 0x10132b30 (vslot 38) - the three controls, then the shared body (the list, the base)
INT NCManorCropInfoWnd::OnDestroy()
{
    guard(NCManorCropInfoWnd::OnDestroy);
    ManorDestroyChild(this, ManorCombo);
    ManorDestroyChild(this, SellButton);
    ManorDestroyChild(this, TimeCombo);
    ((NCManorInfoDownWnd*)this)->NCManorInfoDownWnd::OnDestroy();   // 0x101367B0, the shared body (the list, the base)
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10132b30, NCManorCropInfoWnd, OnDestroy)

// 0x10132c30 (vslot 62) - the shared body draws the back texture, then the title (a text of the game data)
INT NCManorCropInfoWnd::OnPaint(NCGDevice* Device)
{
    guard(NCManorCropInfoWnd::OnPaint);
    ((NCManorInfoDownWnd*)this)->NCManorInfoDownWnd::OnPaint(Device);   // 0x10136860
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    Device->BeginPaint(ScreenX, ScreenY);
    ManorDrawLabel(Device, 0x10, 0xF, 0xFFDCDCDC, *::nw::Field<FString>(&GL2GameData, 0x9BB54));
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10132c30, NCManorCropInfoWnd, OnPaint)

// 0x10132d20 - asks for the page of the manor and the period chosen (ask 4)
INT NCManorCropInfoWnd::OnViewBtnClick()
{
    guard(NCManorCropInfoWnd::OnViewBtnClick);
    TCHAR Bypass[1024];
    appSprintf(Bypass, TEXT("manor_menu_select?ask=4&state=%d&time=%d"), ManorComboData(ManorCombo, ManorCombo->Selected),
               TimeCombo->Selected);
    L2ParamStack Params(10);
    Params.PushBack((void*)Bypass);
    GConsole->RequestBypassToServer(Params);
    return 0;
    unguard;
}
NW_HOOK_METHOD(0x10132d20, NCManorCropInfoWnd, OnViewBtnClick)

// 0x10132e00 (vslot 37) - the shared body makes the back texture and the list; then the sell button, the period combo box
// (this month, next month) and the manor combo box, the list's seven... five columns
INT NCManorCropInfoWnd::OnCreate()
{
    guard(NCManorCropInfoWnd::OnCreate);
    ((NCManorInfoDownWnd*)this)->NCManorInfoDownWnd::OnCreate();   // 0x10136960
    SellButton = ManorNewButton(0, 0);
    SellButton->ID = NID_MANORCROP_SELL;
    SellButton->SetPosition(0x206, 0xF9, 0, 0);
    CreateChild(SellButton, NWS_VISIBLE);
    SellButton->SetTextID(STR_Sell);

    TimeCombo = new NCComboBox();
    TimeCombo->ID = NID_MANORCROP_TIMECOMBO;
    TimeCombo->SetPosition(0xA4, 0xC, 0x46, 0x11);
    CreateChild(TimeCombo, NWS_VISIBLE);
    ManorComboAddID(TimeCombo, STR_ThisMonth);
    ManorComboAddID(TimeCombo, STR_NextMonth);
    TimeCombo->SetSelected(0);

    ManorCombo = new NCComboBox();
    ManorCombo->ID = NID_MANORCROP_TIMECOMBO;
    ManorCombo->SetPosition(0x162, 0xC, 0x82, 0x11);
    CreateChild(ManorCombo, NWS_VISIBLE);

    ManorListSetColumns(ListCtrl, 5);
    NManorColumnInfo Column;
    ManorSetTabTextures(Column);
    Column.Width = 0x10D;
    Column.TitleID = 0x464;
    Column.bNumeric = 0;
    ListCtrl->AddColumn(0, (NListColumnInfo*)&Column);
    Column.Width = 0x50;
    Column.TitleID = 0x482;
    Column.bNumeric = 1;
    ListCtrl->AddColumn(1, (NListColumnInfo*)&Column);
    Column.Width = 0x50;
    Column.TitleID = 0x465;
    Column.bNumeric = 1;
    ListCtrl->AddColumn(2, (NListColumnInfo*)&Column);
    Column.Width = 0x50;
    Column.TitleID = 0x466;
    Column.bNumeric = 1;
    ListCtrl->AddColumn(3, (NListColumnInfo*)&Column);
    Column.Width = 0x4D;
    Column.TitleID = 0x467;
    Column.bNumeric = 0;
    ListCtrl->AddColumn(3, (NListColumnInfo*)&Column);
    bOwnManor = 0;
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10132e00, NCManorCropInfoWnd, OnCreate)

// 0x10133290 (ret 4) - the manor combo box answers: the page of the new manor
INT NCManorCropInfoWnd::OnManorComboBox(WPARAM lParam)
{
    guard(NCManorCropInfoWnd::OnManorComboBox);
    OnViewBtnClick();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10133290, NCManorCropInfoWnd, OnManorComboBox)

// 0x10133310 (ret 4) - the same for the period combo box
INT NCManorCropInfoWnd::OnTimeComboBox(WPARAM lParam)
{
    guard(NCManorCropInfoWnd::OnTimeComboBox);
    OnViewBtnClick();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10133310, NCManorCropInfoWnd, OnTimeComboBox)

// 0x100023c0 (vslot 74) - the shared stub of the extra slot (ret 4: it takes a row, as NCManorInfoSettingWnd's OnListCtrl, and does
// nothing); the standalone has its own empty body (same form as NCManorInfoDownWnd's)
#if NW_HOOK_MODE
NW_NAKED INT NCManorCropInfoWnd::NewVirtual74(INT Row) NW_STOCK_AT(0x100023c0)
#else
INT NCManorCropInfoWnd::NewVirtual74(INT Row)
{
    return 0;
}
#endif

NW_CHECK_VTABLE(NCManorCropInfoWnd, 0x101d6700, 75)
