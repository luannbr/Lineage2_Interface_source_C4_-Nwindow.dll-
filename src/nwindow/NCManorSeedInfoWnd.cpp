/*=============================================================================
    NCManorSeedInfoWnd.cpp - the seed prices of a manor (new in C4): the twin of NCManorCropInfoWnd.
    Stock (C4): 0x1013A570 .. 0x1013B540 (ShowWindow, the button and combo handlers, OnShowSeedInfo, OnDestroy, OnPaint,
    OnCreate, WindowProc), 0x10137980 (constructor), 0x101379D0 (destructor).
=============================================================================*/
#include "nwindow/NCManorSeedInfoWnd.h"
#include "nwindow/NCManorInfoDownWnd.h"
#include "nwindow/NClasses.gen.h"
#include "nwindow/NGameData.h"
#include "nwindow/NString.h"

// sysstring labels and system messages
enum
{
    STR_Buy = 0x45F,
    STR_ThisMonth = 0x45C,
    STR_NextMonth = 0x45D,
    MSG_ManorName = 0x645,
};

// 0x10137980 - NCWnd's constructor and the vtable (the stock one calls the base's NCManorInfoDownWnd constructor, which
// does the same)
NCManorSeedInfoWnd::NCManorSeedInfoWnd()
{
}
NW_HOOK_CTOR(0x10137980, NCManorSeedInfoWnd)

// 0x101379d0 - ConditionalDestroy, then NCWnd's destructor (the stock one calls NCManorInfoDownWnd's 0x10133430, which does the same)
NW_HOOK_DTOR(0x101379d0, NCManorSeedInfoWnd)

// 0x1013b540 (vslot 25) - the base's answer first; then the two combo boxes (their id 0xFFDDCCA1 / 0xFFDDCCA0) ask for
// another page, the buy button for the buy page
INT NCManorSeedInfoWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCWnd::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND)
    {
        if (wParam == (WPARAM)NID_MANORSEED_MANORCOMBO)
            return OnManorComboBox(lParam) != 0;
        if (wParam == (WPARAM)NID_MANORSEED_TIMECOMBO)
            return OnTimeComboBox(lParam) != 0;
        if (wParam == (WPARAM)NID_MANORSEED_BUY)
            return OnBuySeedBtnClick() != 0;
    }
    return 0;
}
NW_HOOK_METHOD(0x1013b540, NCManorSeedInfoWnd, WindowProc)

// 0x1013a570 (vslot 48) - the base's; on the player's own manor's page the buy button stays hidden
void NCManorSeedInfoWnd::ShowWindow()
{
    guard(NCManorSeedInfoWnd::ShowWindow);
    NCWnd::ShowWindow();
    if (bOwnManor == 1)
        BuyButton->HideWindow();
    unguard;
}
NW_HOOK_METHOD(0x1013a570, NCManorSeedInfoWnd, ShowWindow)

// 0x1013a610 - asks the server for the buy page ("manor_menu_select", ask 1)
INT NCManorSeedInfoWnd::OnBuySeedBtnClick()
{
    guard(NCManorSeedInfoWnd::OnBuySeedBtnClick);
    TCHAR Bypass[1024];
    appSprintf(Bypass, TEXT("manor_menu_select?ask=1&state=-1&time=0"));
    L2ParamStack Params(10);
    Params.PushBack((void*)Bypass);
    GConsole->RequestBypassToServer(Params);
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1013a610, NCManorSeedInfoWnd, OnBuySeedBtnClick)

// 0x1013a6e0 (ret 4) - the console's packet: whether this is the player's own manor, the manor, one value skipped, the
// number of seeds, then for each seed five values (the seed, the amount and prices, ...), the items it is made from and
// the ones it gives (a count each, then the ids; the names joined by commas), one list row of seven cells; then the manor
// combo box lists the console's manors (a record of 16 bytes: the id first) by name and shows the one of this window
void NCManorSeedInfoWnd::OnShowSeedInfo(L2ParamStack* Params)
{
    guard(NCManorSeedInfoWnd::OnShowSeedInfo);
    ListCtrl->ClearRows();
    bOwnManor = (BYTE)(INT)Params->Top();
    ManorID = (INT)Params->Top();
    Params->Top();
    INT Count = (INT)Params->Top();
    for (INT n = 0; n < Count; n++)
    {
        INT Seed = (INT)Params->Top();
        INT Value1 = (INT)Params->Top();
        INT Value2 = (INT)Params->Top();
        INT Value3 = (INT)Params->Top();
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
        NListRow* Row = new NListRow(7);
        Row->Cells[0].SetText(GL2GameData.GetItemName(Seed));
        Row->Cells[1].SetText(*FString::Printf(TEXT("%d"), Value1));
        Row->Cells[2].SetText(*FString::Printf(TEXT("%d"), Value2));
        Row->Cells[3].SetText(*FString::Printf(TEXT("%d"), Value3));
        Row->Cells[4].SetText(*FString::Printf(TEXT("%d"), Value4));
        Row->Cells[5].SetText(*First);
        Row->Cells[6].SetText(*Second);
        Row->ToolTip.FillMatchRow(Row, 0x20000);   // 0x10031B10
        ListCtrl->AddRow(Row);
    }
    ManorComboClear(ManorCombo);
    for (INT i = 0; i < GConsole->ManorList.Num(); i++)
    {
        INT Manor = GConsole->ManorList(i).ID;
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
    if (bOwnManor == 1)
        BuyButton->HideWindow();
    else
        BuyButton->ShowWindow();
    unguard;
}
NW_HOOK_METHOD(0x1013a6e0, NCManorSeedInfoWnd, OnShowSeedInfo)

// 0x1013ab90 - the manor the combo box shows
INT NCManorSeedInfoWnd::GetSelectedManorID()
{
    guard(NCManorSeedInfoWnd::GetSelectedManorID);
    return ManorComboData(ManorCombo, ManorCombo->Selected);
    unguard;
}
NW_HOOK_METHOD(0x1013ab90, NCManorSeedInfoWnd, GetSelectedManorID)

// 0x1013a600 (anonymous in the db) - the manor OnPaint names is the one shown
void NCManorSeedInfoWnd::SetPaintManor()
{
    PaintManorID = ManorID;
}
NW_HOOK_METHOD(0x1013a600, NCManorSeedInfoWnd, SetPaintManor)

// 0x1013ac10 (anonymous in the db; the crop page's calls land here too, the window layouts agree) - the period combo box's choice
INT NCManorSeedInfoWnd::GetTimeSelected()
{
    return TimeCombo->Selected;
}
NW_HOOK_METHOD(0x1013ac10, NCManorSeedInfoWnd, GetTimeSelected)

// 0x1013ac20 (vslot 38) - the three controls, then the shared body (the list, the base)
INT NCManorSeedInfoWnd::OnDestroy()
{
    guard(NCManorSeedInfoWnd::OnDestroy);
    ManorDestroyChild(this, ManorCombo);
    ManorDestroyChild(this, BuyButton);
    ManorDestroyChild(this, TimeCombo);
    MANOR_DOWN_BASE()->NCManorInfoDownWnd::OnDestroy();   // 0x101367B0
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1013ac20, NCManorSeedInfoWnd, OnDestroy)

// 0x1013ad20 (vslot 62) - the shared body draws the back texture, then the title (a text of the game data) and, unless this
// is the player's own manor's page, the name of the manor PaintManorID under it (system message 0x645 with the name)
INT NCManorSeedInfoWnd::OnPaint(NCGDevice* Device)
{
    guard(NCManorSeedInfoWnd::OnPaint);
    MANOR_DOWN_BASE()->NCManorInfoDownWnd::OnPaint(Device);   // 0x10136860
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    Device->BeginPaint(ScreenX, ScreenY);
    ManorDrawLabel(Device, 0x10, 0xF, 0xFFDCDCDC, *SysString(1113));
    if (bOwnManor != 1)
    {
        const TCHAR* ManorName = GConsole->GetManorName(PaintManorID);
        if (ManorName)
        {
            TCHAR Message[1024];
            L2ParamStack Name(10);
            Name.PushBack((void*)0);
            Name.PushBack((void*)ManorName);
            GConsole->AddSystemMessageParam(Name);
            L2ParamStack Text(10);
            Text.PushBack((void*)MSG_ManorName);
            ::nw::VirtualCall<void>(GConsole, 191, &Text, 1, Message, 0);
            ManorDrawLabel(Device, 0x10, 0xFA, 0xFFDCDCDC, Message);
        }
    }
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1013ad20, NCManorSeedInfoWnd, OnPaint)

// 0x1013aef0 - asks for the page of the manor and the period chosen (ask 3)
INT NCManorSeedInfoWnd::RequestData()
{
    guard(NCManorSeedInfoWnd::RequestData);
    TCHAR Bypass[1024];
    appSprintf(Bypass, TEXT("manor_menu_select?ask=3&state=%d&time=%d"), ManorComboData(ManorCombo, ManorCombo->Selected),
               TimeCombo->Selected);
    L2ParamStack Params(10);
    Params.PushBack((void*)Bypass);
    GConsole->RequestBypassToServer(Params);
    return 0;
    unguard;
}
NW_HOOK_METHOD(0x1013aef0, NCManorSeedInfoWnd, RequestData)

// 0x1013afd0 (vslot 37) - the shared body makes the back texture and the list; then the buy button, the period combo box
// (this month, next month) and the manor combo box, the list's four columns
INT NCManorSeedInfoWnd::OnCreate()
{
    guard(NCManorSeedInfoWnd::OnCreate);
    MANOR_DOWN_BASE()->NCManorInfoDownWnd::OnCreate();   // 0x10136960
    BuyButton = ManorNewButton(0, 0);
    BuyButton->ID = NID_MANORSEED_BUY;
    BuyButton->SetPosition(0x206, 0xF9, 0, 0);
    CreateChild(BuyButton, NWS_VISIBLE);
    BuyButton->SetTextID(STR_Buy);

    TimeCombo = new NCComboBox();
    TimeCombo->ID = NID_MANORSEED_TIMECOMBO;
    TimeCombo->SetPosition(0xA4, 0xC, 0x46, 0x11);
    CreateChild(TimeCombo, NWS_VISIBLE);
    ManorComboAddID(TimeCombo, STR_ThisMonth);
    ManorComboAddID(TimeCombo, STR_NextMonth);
    TimeCombo->SetSelected(0);

    ManorCombo = new NCComboBox();
    ManorCombo->ID = NID_MANORSEED_TIMECOMBO;
    ManorCombo->SetPosition(0x162, 0xC, 0x82, 0x11);
    CreateChild(ManorCombo, NWS_VISIBLE);

    ManorListSetColumns(ListCtrl, 4);
    NManorColumnInfo Column;
    ManorSetTabTextures(Column);
    Column.Width = 0x149;
    Column.TitleID = 0x460;
    Column.bNumeric = 0;
    ListCtrl->AddColumn(0, (NListColumnInfo*)&Column);
    Column.Width = 0x50;
    Column.TitleID = 0x461;
    Column.bNumeric = 1;
    ListCtrl->AddColumn(1, (NListColumnInfo*)&Column);
    Column.Width = 0x50;
    Column.TitleID = 0x462;
    Column.bNumeric = 1;
    ListCtrl->AddColumn(2, (NListColumnInfo*)&Column);
    Column.Width = 0x61;
    Column.TitleID = 0x463;
    Column.bNumeric = 1;
    ListCtrl->AddColumn(3, (NListColumnInfo*)&Column);
    bOwnManor = 0;
    PaintManorID = -1;
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1013afd0, NCManorSeedInfoWnd, OnCreate)

// 0x1013b440 (ret 4) - the manor combo box answers: the page of the new manor
INT NCManorSeedInfoWnd::OnManorComboBox(WPARAM lParam)
{
    guard(NCManorSeedInfoWnd::OnManorComboBox);
    RequestData();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1013b440, NCManorSeedInfoWnd, OnManorComboBox)

// 0x1013b4c0 (ret 4) - the same for the period combo box
INT NCManorSeedInfoWnd::OnTimeComboBox(WPARAM lParam)
{
    guard(NCManorSeedInfoWnd::OnTimeComboBox);
    RequestData();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1013b4c0, NCManorSeedInfoWnd, OnTimeComboBox)

// 0x100023c0 (vslot 74) - the shared stub of the extra slot (ret 4: it takes a row, as NCManorInfoSettingWnd's OnListCtrl, and
// does nothing); the standalone has its own empty body
#if NW_HOOK_MODE
NW_NAKED INT NCManorSeedInfoWnd::NewVirtual74(INT Row) NW_STOCK_AT(0x100023c0)
#else
INT NCManorSeedInfoWnd::NewVirtual74(INT Row)
{
    return 0;
}
#endif

NW_CHECK_VTABLE(NCManorSeedInfoWnd, 0x101d79d8, 75)
