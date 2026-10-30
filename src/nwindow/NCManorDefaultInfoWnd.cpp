/*=============================================================================
    NCManorDefaultInfoWnd.cpp - the manor information page of the crops a manor can be asked to grow (new in C4).
    Stock (C4): 0x10135AB0 .. 0x10136300 (ShowWindow, the two buttons, WindowProc, OnShowManorDefaultInfo, OnDestroy,
    OnPaint, OnCreate), 0x10136700 (constructor), 0x10136750 (destructor).
=============================================================================*/
#include "nwindow/NCManorDefaultInfoWnd.h"
#include "nwindow/NCManorInfoDownWnd.h"
#include "nwindow/NClasses.gen.h"
#include "nwindow/NGameData.h"
#include "nwindow/NString.h"

// sysstring labels
enum
{
    STR_BuySeed = 0x45F,
    STR_SellCrop = 0x46A,
};

// 0x10136700 - NCWnd's constructor and the vtable (the stock one calls the base's NCManorInfoDownWnd constructor, which
// does the same)
NCManorDefaultInfoWnd::NCManorDefaultInfoWnd()
{
}
NW_HOOK_CTOR(0x10136700, NCManorDefaultInfoWnd)

// 0x10136750 - ConditionalDestroy, then NCWnd's destructor (the stock one calls NCManorInfoDownWnd's 0x10133430, which does the same)
NW_HOOK_DTOR(0x10136750, NCManorDefaultInfoWnd)

// 0x10135cf0 (vslot 25) - the base's answer first; then the two buttons ask for the buy and the sell pages
INT NCManorDefaultInfoWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCWnd::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND)
    {
        if (wParam == (WPARAM)NID_MANORDEFAULT_BUYSEED)
            return OnBuySeedBtnClick() != 0;
        if (wParam == (WPARAM)NID_MANORDEFAULT_SELLCROP)
            return OnSellCropBtnClick() != 0;
    }
    return 0;
}
NW_HOOK_METHOD(0x10135cf0, NCManorDefaultInfoWnd, WindowProc)

// 0x10135ab0 (vslot 48) - the base's; on the player's own manor's page both buttons stay hidden
void NCManorDefaultInfoWnd::ShowWindow()
{
    guard(NCManorDefaultInfoWnd::ShowWindow);
    NCWnd::ShowWindow();
    if (bOwnManor == 1)
    {
        BuySeedButton->HideWindow();
        SellCropButton->HideWindow();
    }
    unguard;
}
NW_HOOK_METHOD(0x10135ab0, NCManorDefaultInfoWnd, ShowWindow)

// 0x10135b50 - asks the server for the buy page ("manor_menu_select", ask 1)
INT NCManorDefaultInfoWnd::OnBuySeedBtnClick()
{
    guard(NCManorDefaultInfoWnd::OnBuySeedBtnClick);
    TCHAR Bypass[1024];
    appSprintf(Bypass, TEXT("manor_menu_select?ask=1&state=-1&time=0"));
    L2ParamStack Params(10);
    Params.PushBack((void*)Bypass);
    GConsole->RequestBypassToServer(Params);
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10135b50, NCManorDefaultInfoWnd, OnBuySeedBtnClick)

// 0x10135c20 - asks the server for the sell page (ask 2)
INT NCManorDefaultInfoWnd::OnSellCropBtnClick()
{
    guard(NCManorDefaultInfoWnd::OnSellCropBtnClick);
    TCHAR Bypass[1024];
    appSprintf(Bypass, TEXT("manor_menu_select?ask=2&state=-1&time=0"));
    L2ParamStack Params(10);
    Params.PushBack((void*)Bypass);
    GConsole->RequestBypassToServer(Params);
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10135c20, NCManorDefaultInfoWnd, OnSellCropBtnClick)

// 0x10135d60 (ret 4) - the console's packet: whether this is the player's own manor, the number of items, then for each
// item four values (the item, three numbers), the items it is made from and the ones it gives (a count each, then the ids;
// the names joined by commas), one list row of six cells; both buttons follow the first value
void NCManorDefaultInfoWnd::OnShowManorDefaultInfo(L2ParamStack* Params)
{
    guard(NCManorDefaultInfoWnd::OnShowManorDefaultInfo);
    ListCtrl->ClearRows();
    bOwnManor = (BYTE)(INT)Params->Top();
    INT Count = (INT)Params->Top();
    for (INT n = 0; n < Count; n++)
    {
        INT Item = (INT)Params->Top();
        INT Value1 = (INT)Params->Top();
        INT Value2 = (INT)Params->Top();
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
        NListRow* Row = new NListRow(6);
        Row->Cells[0].SetText(GL2GameData.GetItemName(Item));
        Row->Cells[1].SetText(*FString::Printf(TEXT("%d"), Value1));
        Row->Cells[2].SetText(*FString::Printf(TEXT("%d"), Value2));
        Row->Cells[3].SetText(*FString::Printf(TEXT("%d"), Value3));
        Row->Cells[4].SetText(*First);
        Row->Cells[5].SetText(*Second);
        Row->ToolTip.FillMatchRow(Row, 0x200000);   // 0x10031B10
        ListCtrl->AddRow(Row);
    }
    if (bOwnManor == 1)
    {
        BuySeedButton->HideWindow();
        SellCropButton->HideWindow();
    }
    else
    {
        BuySeedButton->ShowWindow();
        SellCropButton->ShowWindow();
    }
    unguard;
}
NW_HOOK_METHOD(0x10135d60, NCManorDefaultInfoWnd, OnShowManorDefaultInfo)

// 0x10136140 (vslot 38) - the two buttons, then the shared body (the list, the base)
INT NCManorDefaultInfoWnd::OnDestroy()
{
    guard(NCManorDefaultInfoWnd::OnDestroy);
    ManorDestroyChild(this, BuySeedButton);
    ManorDestroyChild(this, SellCropButton);
    MANOR_DOWN_BASE()->NCManorInfoDownWnd::OnDestroy();   // 0x101367B0
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10136140, NCManorDefaultInfoWnd, OnDestroy)

// 0x10136210 (vslot 62) - the shared body draws the back texture, then the title (a text of the game data)
INT NCManorDefaultInfoWnd::OnPaint(NCGDevice* Device)
{
    guard(NCManorDefaultInfoWnd::OnPaint);
    MANOR_DOWN_BASE()->NCManorInfoDownWnd::OnPaint(Device);   // 0x10136860
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    Device->BeginPaint(ScreenX, ScreenY);
    ManorDrawLabel(Device, 0x10, 0xF, 0xFFDCDCDC, *SysString(1131));
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10136210, NCManorDefaultInfoWnd, OnPaint)

// 0x10136300 (vslot 37) - the shared body makes the back texture and the list; then the buy and sell buttons and the list's
// six columns
INT NCManorDefaultInfoWnd::OnCreate()
{
    guard(NCManorDefaultInfoWnd::OnCreate);
    MANOR_DOWN_BASE()->NCManorInfoDownWnd::OnCreate();   // 0x10136960
    BuySeedButton = ManorNewButton(0, 0);
    BuySeedButton->ID = NID_MANORDEFAULT_BUYSEED;
    BuySeedButton->SetPosition(0x1B6, 0xF9, 0, 0);
    CreateChild(BuySeedButton, NWS_VISIBLE);
    BuySeedButton->SetTextID(STR_BuySeed);

    SellCropButton = ManorNewButton(0, 0);
    SellCropButton->ID = NID_MANORDEFAULT_SELLCROP;
    SellCropButton->SetPosition(0x206, 0xF9, 0, 0);
    CreateChild(SellCropButton, NWS_VISIBLE);
    SellCropButton->SetTextID(STR_SellCrop);

    ManorListSetColumns(ListCtrl, 6);
    NManorColumnInfo Column;
    ManorSetTabTextures(Column);
    Column.Width = 0xA9;
    Column.TitleID = 0x464;
    Column.bNumeric = 0;
    ListCtrl->AddColumn(0, (NListColumnInfo*)&Column);
    Column.Width = 0x32;
    Column.TitleID = 0x219;
    Column.bNumeric = 1;
    ListCtrl->AddColumn(1, (NListColumnInfo*)&Column);
    Column.Width = 0x50;
    Column.TitleID = 0x46C;
    Column.bNumeric = 1;
    ListCtrl->AddColumn(2, (NListColumnInfo*)&Column);
    Column.Width = 0x50;
    Column.TitleID = 0x46D;
    Column.bNumeric = 1;
    ListCtrl->AddColumn(3, (NListColumnInfo*)&Column);
    Column.Width = 0x5F;
    Column.TitleID = 0x46E;
    Column.bNumeric = 0;
    ListCtrl->AddColumn(4, (NListColumnInfo*)&Column);
    Column.Width = 0x70;
    Column.TitleID = 0x46F;
    Column.bNumeric = 0;
    ListCtrl->AddColumn(5, (NListColumnInfo*)&Column);
    bOwnManor = 0;
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10136300, NCManorDefaultInfoWnd, OnCreate)

// 0x100023c0 (vslot 74) - the shared stub of the extra slot (ret 4: it takes a row, as NCManorInfoSettingWnd's OnListCtrl, and
// does nothing); the standalone has its own empty body
#if NW_HOOK_MODE
NW_NAKED INT NCManorDefaultInfoWnd::NewVirtual74(INT Row) NW_STOCK_AT(0x100023c0)
#else
INT NCManorDefaultInfoWnd::NewVirtual74(INT Row)
{
    return 0;
}
#endif

NW_CHECK_VTABLE(NCManorDefaultInfoWnd, 0x101d7148, 75)
