/*=============================================================================
    NCMatchWnd.cpp - party matching. C3 stock: 0x100a4c40 .. 0x100a4e00 (WindowProc, OnTimer, Minimize, ClearList,
    OnJoin), 0x100a57e0 .. 0x100a5f60 (OnDestroy, OnPaint, SetList, the page buttons), 0x100a7c00 .. 0x100a8a51
    (constructor, OnCreate). ShowWindow's and HideWindow's bodies are shared (0x100aa860 and 0x10030c00).
=============================================================================*/
#include "nwindow/NCMatchWnd.h"
#include "nwindow/NCMatchListCtrl.h"
#include "nwindow/NCMatchFrameCtrl.h"
#include "nwindow/NCPartyRoomMakingWnd.h"
#include "nwindow/NCButton.h"
#include "nwindow/NCComboBox.h"
#include "nwindow/NCIcon.h"
#include "nwindow/NColumnInfoC4.h"
#include "nwindow/NWinBStock.h"
#include "nwindow/NCConsole.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NConsoleWnd.h"

// one child of OnDestroy (repeated inline in the stock code)
template <class T> static FORCEINLINE void DestroyChild(NCWnd* Parent, T*& Child)
{
    if (Child)
    {
        Parent->RemoveChildWindow(Child->Entry);
        delete Child;
        Child = NULL;
    }
}

// a column of the list (the stock code spells out each with its own record, 0x4018 bytes in C4: six of them make the
// 0x18090 bytes of its frame)
static FORCEINLINE void AddMatchColumn(NCListCtrl* List, NColumnInfoC4& Info, INT Width, INT TitleID, UBOOL bNumeric)
{
    Info.Width = Width;
    Info.Height = 19;
    Info.bNumeric = bNumeric;
    Info.SetTitleID(TitleID);
    Info.SetNormalTex(TEXT("L2UI_CH3.ListCtrl.tab1"), 0);
    Info.SetTabDownTex(TEXT("L2UI_CH3.ListCtrl.tab1_down"), 0);
    Info.SetNormalTex(TEXT("L2UI_CH3.ListCtrl.tab2"), 1);
    Info.SetTabDownTex(TEXT("L2UI_CH3.ListCtrl.tab2_down"), 1);
    Info.SetNormalTex(TEXT("L2UI_CH3.ListCtrl.tab3"), 2);
    Info.SetTabDownTex(TEXT("L2UI_CH3.ListCtrl.tab3_down"), 2);
    List->AddColumn(0, (NListColumnInfo*)&Info);
}

// NCComboBox::AddSysStringItem(TextID, Value) (0x10006850, ret 8) and AddItem(TextID) (0x10006800, ret 4)
static FORCEINLINE void AddComboItem(NCComboBox* Combo, INT TextID, INT Value)
{
    Combo->AddSysStringItem(TextID, Value);
}
static FORCEINLINE void AddComboItem(NCComboBox* Combo, INT TextID)
{
    Combo->AddItem(TextID);
}

// the data word of the combo box's chosen item (0x10006410, ret 4: NTemplates.cpp's NComboItemFns::GetItemData); the standalone
// reads the record itself
static FORCEINLINE INT ComboItemData(NCComboBox* Combo, INT Index)
{
#if NW_HOOK_MODE
    return ::nw::StockCallAt<INT>(0x10006410, Combo, Index);
#else
    return NComboItemData(Combo, Index);
#endif
}

// one of the small buttons (the page buttons are 64 x 21, the others 76 x 23: spelled out at each use in the stock
// code)
static FORCEINLINE NCButton* MakeSmallButton(NCWnd* Parent, INT ID, INT X, INT Y, INT TextID)
{
    NCButton* Button = new NCButton(TEXT("L2UI_CH3.Button.SmallButton2"), TEXT("L2UI_CH3.Button.SmallButton2_down"), 0);
    Button->ID = ID;
    Button->SetPosition(X, Y, 0x40, 0x15);
    Parent->CreateChild(Button, NWS_VISIBLE);
    Button->SetTextID(TextID);
    return Button;
}
static FORCEINLINE NCButton* MakeWideButton(NCWnd* Parent, INT ID, INT X, INT Y, INT TextID)
{
    NCButton* Button = new NCButton(TEXT("L2UI_CH3.Button.Btn1_normal"), TEXT("L2UI_CH3.Button.Btn1_normalOn"), 0);
    Button->ID = ID;
    Button->SetPosition(X, Y, 0x4C, 0x17);
    Parent->CreateChild(Button, NWS_VISIBLE);
    Button->SetTextID(TextID);
    return Button;
}

// the request of the page buttons (spelled out in the stock code of the previous, the next and the search button):
// the page, the place chosen (its item's value) and the kind of room chosen; then four of the buttons wait for the
// timer
static FORCEINLINE void RequestPage(NCMatchWnd* Wnd, INT NewPage)
{
    L2ParamStack Params(10);
    Params.PushBack((void*)NewPage);
    Params.PushBack((void*)ComboItemData(Wnd->AreaCombo, Wnd->AreaCombo->Selected));
    Params.PushBack((void*)Wnd->TypeCombo->Selected);
    ((NConsoleForwardFns*)GConsole)->RequestListPartyWating(Params);   // 0x1006DF40
    Wnd->PrevButton->EnableWindow(0);
    Wnd->NextButton->EnableWindow(0);
    Wnd->SearchButton->EnableWindow(0);
    Wnd->JoinButton->EnableWindow(0);
}

// 0x100b4530 - NCWnd's, no page
NCMatchWnd::NCMatchWnd()
{
    Page = 0;
}
NW_HOOK_CTOR(0x100b4530, NCMatchWnd)

// 0x100b4580
NW_HOOK_DTOR(0x100b4580, NCMatchWnd)

// 0x100b14f0 (vslot 25) - the five buttons
INT NCMatchWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCWnd::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND)
    {
        if ((INT)wParam == NID_MATCH_PREV)
            return OnPrevPage() != 0;
        if ((INT)wParam == NID_MATCH_NEXT)
            return OnNextPage() != 0;
        if ((INT)wParam == NID_MATCH_SEARCH)
            return OnSearch() != 0;
        if ((INT)wParam == NID_MATCH_CREATE)
            return OnCreateRoom() != 0;
        if ((INT)wParam == NID_MATCH_JOIN)
            return OnJoin() != 0;
    }
    return 0;
}
NW_HOOK_METHOD(0x100b14f0, NCMatchWnd, WindowProc)

// 0x100b15d0 (vslot 36) - the 5 second timer: four of the buttons are enabled again
INT NCMatchWnd::OnTimer(INT InTimerID)
{
    if (InTimerID == TIMER_MatchButtons)
    {
        PrevButton->EnableWindow(1);
        NextButton->EnableWindow(1);
        SearchButton->EnableWindow(1);
        JoinButton->EnableWindow(1);
    }
    return 1;
}
NW_HOOK_METHOD(0x100b15d0, NCMatchWnd, OnTimer)

// 0x100b45d0 (vslot 37) - the timer, the title bar of its own class (the title is a system string id now), the
// background, the list of rooms (15 rows, six columns in C4: number, title, place, kind, level range, members), the five
// buttons and the two selection boxes (15 places in C4: the two new ones are texts 1247 and 1248)
INT NCMatchWnd::OnCreate()
{
    NColumnInfoC4 C1, C2, C3, C4, C5, C6;   // six records, as in the stock frame
    NCWnd::OnCreate();
    TimerID = SetTimer(TIMER_MatchButtons, 5000, NULL);
    MatchFrame = new NCMatchFrameCtrl();
    MatchFrame->ID = NID_MATCH_FRAME;
    MatchFrame->SetPosition(0, 0, 600, 20);
    CreateChild(MatchFrame, NWS_VISIBLE);
    MatchFrame->SetTitleID(389);   // 0x10011810: the title is a text id
    BackTex = FindTexture(TEXT("L2UI_CH3.PartyMatchWnd.PartyMatch1_Back"), 1);
    MatchList = new NCMatchListCtrl(15, 15, 6, 16);
    MatchList->SetPosition(7, 0x53, 0x249, 0x111);
    CreateChild(MatchList, NWS_VISIBLE | NWS_UNKNOWN20);   // 0x22
    MatchList->SetTextOffset(0, 3);
    AddMatchColumn(MatchList, C1, 0x22, 416, 1);
    AddMatchColumn(MatchList, C2, 0xF0, 413, 0);
    AddMatchColumn(MatchList, C3, 0x64, 408, 0);
    AddMatchColumn(MatchList, C4, 0x62, 1029, 0);
    AddMatchColumn(MatchList, C5, 0x48, 1030, 0);
    AddMatchColumn(MatchList, C6, 0x29, 1036, 0);

    PrevButton = MakeSmallButton(this, NID_MATCH_PREV, 0xE9, 0x16E, 1037);
    NextButton = MakeSmallButton(this, NID_MATCH_NEXT, 0x12F, 0x16E, 1038);
    SearchButton = MakeWideButton(this, NID_MATCH_SEARCH, 0x171, 0x32, 1039);
    CreateButton = MakeWideButton(this, NID_MATCH_CREATE, 0x1C0, 0x192, 1040);
    JoinButton = MakeWideButton(this, NID_MATCH_JOIN, 0x20B, 0x192, 1041);

    AreaCombo = new NCComboBox();
    AreaCombo->ID = NID_MATCH_AREA;
    AreaCombo->SetPosition(0x54, 0x34, 0x5A, 0x11);
    CreateChild(AreaCombo, NWS_VISIBLE | 0x10);   // 0x12: 0x10's meaning is not known
    TypeCombo = new NCComboBox();
    TypeCombo->ID = NID_MATCH_TYPE;
    TypeCombo->SetPosition(0x108, 0x34, 0x5A, 0x11);
    CreateChild(TypeCombo, NWS_VISIBLE | 0x10);
    // the place box: -2, -1 and the fifteen places (the value of the item is what the request carries)
    AddComboItem(AreaCombo, 1047, -2);
    AddComboItem(AreaCombo, 1046, -1);
    AddComboItem(AreaCombo, 1048, 1);
    AddComboItem(AreaCombo, 1049, 2);
    AddComboItem(AreaCombo, 1050, 3);
    AddComboItem(AreaCombo, 1051, 4);
    AddComboItem(AreaCombo, 1052, 5);
    AddComboItem(AreaCombo, 1053, 6);
    AddComboItem(AreaCombo, 1054, 7);
    AddComboItem(AreaCombo, 1055, 8);
    AddComboItem(AreaCombo, 1056, 9);
    AddComboItem(AreaCombo, 1057, 10);
    AddComboItem(AreaCombo, 1058, 11);
    AddComboItem(AreaCombo, 1059, 12);
    AddComboItem(AreaCombo, 1060, 13);
    AddComboItem(AreaCombo, 1247, 14);
    AddComboItem(AreaCombo, 1248, 15);
    AreaCombo->SetSelected(1);
    AddComboItem(TypeCombo, 1045);
    AddComboItem(TypeCombo, 1046);
    TypeCombo->SetSelected(1);
    return 1;
}
NW_HOOK_METHOD(0x100b45d0, NCMatchWnd, OnCreate)

// 0x100b21a0 (vslot 38) - nine of the controls go (C3 left the next page button and the title bar), then NCWnd's
INT NCMatchWnd::OnDestroy()
{
    guard(NCMatchWnd::OnDestroy);
    DestroyChild(this, AreaCombo);
    DestroyChild(this, TypeCombo);
    DestroyChild(this, MatchList);
    DestroyChild(this, PrevButton);
    DestroyChild(this, NextButton);
    DestroyChild(this, SearchButton);
    DestroyChild(this, CreateButton);
    DestroyChild(this, JoinButton);
    DestroyChild(this, MatchFrame);
    NCWnd::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100b21a0, NCMatchWnd, OnDestroy)

// stock body shared with NCSystemMenuWnd's (0x10078950) and other windows': shown again from its minimized icon, the
// window takes the icon's place and the icon goes away
void NCMatchWnd::ShowWindow()
{
    NCWnd::ShowWindow();
    if (Icon)
    {
        SetPosition(Icon->ScreenX, Icon->ScreenY, (INT)Width, (INT)Height);
        Icon->SetPosition(0, 0, 0, 0);
        Icon->HideWindow();
    }
}

// stock body shared with NCChatWnd's (0x1003dab0)
void NCMatchWnd::HideWindow()
{
    NCWnd::HideWindow();
}

// 0x100b1630 (vslot 54) - minimizing: the window's own icon picture (text 389), see NCPetWnd::Minimize
void NCMatchWnd::Minimize()
{
    if (Icon)
        Icon->SetIcon(TEXT("L2UI_CH3.PartyMatchWnd.PartyMatchIcon"), 0x185, 1);
    if (ScreenX < 0 && Icon)
        Icon->OffsetWindow(-ScreenX, 0);
    HideWindow();
    if (Icon)
    {
        Icon->ShowWindow();
        Icon->SetFocus();
    }
}
NW_HOOK_METHOD(0x100b1630, NCMatchWnd, Minimize)

// 0x100b23a0 (vslot 62) - the background (600 x 411 under the title bar) and the two labels of the selection boxes,
// right-aligned
INT NCMatchWnd::OnPaint(NCGDevice* Device)
{
    SIZE Size;
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    Device->BeginPaint(ScreenX, ScreenY);
    if (BackTex)
        Device->DrawTexture(0, 20, 600, 411, 0, 0, 600, 411, BackTex, 0xFF, 1);
    NCGDevice::GetTextExtent(&Size, *GL2GameData.SysString[1034], 0, 0, 0, 0, 0);
    Device->DrawText(0x50 - Size.cx, 0x36, 0xFFDCDCDC, *GL2GameData.SysString[1034], 0, 0, 0, L2FONT_Normal, 0, 0, 0,
                     0, 0, 0);
    NCGDevice::GetTextExtent(&Size, *GL2GameData.SysString[1035], 0, 0, 0, 0, 0);
    Device->DrawText(0x104 - Size.cx, 0x36, 0xFFDCDCDC, *GL2GameData.SysString[1035], 0, 0, 0, L2FONT_Normal, 0, 0, 0,
                     0, 0, 0);
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
}
NW_HOOK_METHOD(0x100b23a0, NCMatchWnd, OnPaint)

// 0x100b26a0 (vslot 74) - the previous page (the first when there is none)
INT NCMatchWnd::OnPrevPage()
{
    INT NewPage = 1;
    if (Page > 1)
        NewPage = Page - 1;
    RequestPage(this, NewPage);
    return 1;
}
NW_HOOK_METHOD(0x100b26a0, NCMatchWnd, OnPrevPage)

// 0x100b27a0 (vslot 75) - the next page
INT NCMatchWnd::OnNextPage()
{
    INT NewPage = Page + 1;
    RequestPage(this, NewPage);
    return 1;
}
NW_HOOK_METHOD(0x100b27a0, NCMatchWnd, OnNextPage)

// 0x100b2890 (vslot 76) - the first page
INT NCMatchWnd::OnSearch()
{
    RequestPage(this, 1);
    return 1;
}
NW_HOOK_METHOD(0x100b2890, NCMatchWnd, OnSearch)

// 0x100b2980 (vslot 77) - the room making window opens for a new room: 10 in the members box (an index: 12 members),
// the level range 5 either side of the player's (1 .. 78), the default title (system message 1398)
INT NCMatchWnd::OnCreateRoom()
{
    INT Min = 1;
    INT Max = 78;
    User* Me = GConsole->GetMyUser();
    if (Me)
    {
        INT Level = Me->Level;
        Max = Level + 5;
        if (Level > 5)
        {
            if (Level > 73)
                Max = 78;
            Min = Level - 5;
        }
    }
    static_cast<NCPartyRoomMakingWnd*>(GDesktop->PartyRoomMakingWnd)->SetData(0, *GL2GameData.SystemMsg[1398].Message, 10,
                                                                              Min, Max, 0);
    GDesktop->PartyRoomMakingWnd->ShowWindow();
    GDesktop->PartyRoomMakingWnd->SetFocus();
    return 1;
}
NW_HOOK_METHOD(0x100b2980, NCMatchWnd, OnCreateRoom)

// 0x100b2a20 (vslot 78) - asks to join any room (room 0, the place chosen); four of the buttons wait for the timer
INT NCMatchWnd::OnJoin()
{
    L2ParamStack Params(10);
    Params.PushBack((void*)0);
    Params.PushBack((void*)ComboItemData(AreaCombo, AreaCombo->Selected));
    ((NConsoleForwardFns*)GConsole)->RequestJoinPartyRoom(Params);   // 0x1006DF60
    PrevButton->EnableWindow(0);
    NextButton->EnableWindow(0);
    SearchButton->EnableWindow(0);
    JoinButton->EnableWindow(0);
    return 1;
}
NW_HOOK_METHOD(0x100b2a20, NCMatchWnd, OnJoin)

// 0x100b2520 (the console's slot 374) - a page of the list: the window is shown, the page number and the rooms as the
// packet carries them (the number, the title, the place, the level range, the members now / most, and a last text that
// goes to the third column: C4 rows have six cells)
void NCMatchWnd::SetList(L2ParamStack& Params)
{
    TCHAR Buf[128];
    ShowWindow();
    Page = (INT)Params.Top();
    INT Count = (INT)Params.Top();
    while (Count > 0)
    {
        NListRow* Row = new NListRow(6);
        Row->Cells[0].SetNumber((INT)Params.Top());
        Row->Cells[1].SetText((TCHAR*)Params.Top());
        Row->Cells[3].SetText(((NConsoleLeftFns*)GConsole)->GetPartyRoomAreaName((INT)Params.Top()));
        INT First = (INT)Params.Top();
        INT Second = (INT)Params.Top();
        appSprintf(Buf, TEXT("%d-%d"), First, Second);
        Row->Cells[4].SetText(Buf);
        First = (INT)Params.Top();
        Second = (INT)Params.Top();
        appSprintf(Buf, TEXT("%d/%d"), First, Second);
        Row->Cells[5].SetText(Buf);
        Row->Cells[2].SetText((TCHAR*)Params.Top());
        MatchList->AddRow(Row);
        Count--;
    }
}
NW_HOOK_METHOD(0x100b2520, NCMatchWnd, SetList)

// 0x100b1690 (the console's slot 375) - back to no page, the rows go
void NCMatchWnd::ClearList()
{
    Page = 0;
    MatchList->ClearRows();
}
NW_HOOK_METHOD(0x100b1690, NCMatchWnd, ClearList)

NW_CHECK_VTABLE(NCMatchWnd, 0x101bbf08, 79)
