/*=============================================================================
    NCServerSelectWnd.cpp - the server list of the login screen and its list control.
    Stock: 0x100a1e60 .. 0x100a24c7, 0x100a2500 .. 0x100a39cf (OnDestroy is a body the
    linker folded with other classes).
=============================================================================*/
#include "nwindow/NCServerSelectWnd.h"
#include "nwindow/NCLoginWnd.h"
#include "nwindow/NCAuthWnd.h"
#include "nwindow/NCButton.h"
#include "nwindow/NCEditBox.h"
#include "nwindow/NCFrameCtrl.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NConsoleWnd.h"
#include "nwindow/NConsoleWndLogin.h"
#include "nwindow/NCrt.h"
#include "nwindow/NCHtmlViewer.h"
#include "nwindow/NCListCtrl.h"

// An object made as the stock code makes it: StaticAllocateObject of its class, then the constructor at Ctor of
// this client (the standalone: the class's own constructor, which `new` runs after the same allocation)
template <class T, class... A> static FORCEINLINE T* NewStock(DWORD Ctor, A... a)
{
#if NW_HOOK_MODE
    void* Mem = T::operator new(sizeof(T));
    return Mem ? ::nw::StockCallAt<T*>(Ctor, Mem, a...) : NULL;
#else
    (void)Ctor;
    return new T(a...);
#endif
}


#include <wchar.h>
#include <math.h>

// NWM_COMMAND ids, timer id (names ours)
enum
{
    NID_SERVER_OK = 0xF007,
    NID_SERVER_FRAME = 0xF008,
    NID_SERVER_CANCEL = 0xF017,
    NID_SERVER_LIST = 0xF018,
    TIMER_SERVER_RETRY = 0xF019,    // 3 s: the next OK may log in again
    NID_SERVER_QUALITY = 0xF020,
    NID_SERVERLIST_SORT = 0xF021,   // no control of the list sends it
};

// interface sounds (FL2NetNotify::OnInterfacePlaySound)
enum { SOUND_Click = 0, SOUND_ClickDisabled = 2 };

// ping of a server that did not answer, or was not asked
enum { PING_None = 9999 };

// NConsoleWnd::SetLoadingTexture's screens, from the server's age rating (names ours)
enum
{
    LOADING_AllAges = 10001,
    LOADING_Over15 = 10002,
    LOADING_Over18 = 10003,
};

// names of NCsoft's servers 0..14, shown instead of sysstring 506 + id when the login server
// is theirs
#if NW_HOOK_MODE
#define GServerNames (reinterpret_cast<const TCHAR* const*>(::nw::Addr(0x10273C34)))   // C4: the stock indexes the table at 0x10273C34 with the server's id (FormatServerLabel 0x100FFF1D); C3 0x10233F80
#else
extern const TCHAR* const GServerNames[];
#endif

// The cells of a row (C2: five columns; C1 had four, with the ping in 2 and the type in 3): the number, the name,
// the load (the PvP text in Text2, the server flags in Param1, users in Cells[1]), the ping (the age limit in
// Text2) and the icon column
enum
{
    CELL_Number = 0,
    CELL_Name = 1,
    CELL_Load = 2,
    CELL_Ping = 3,
    CELL_Icon = 4,
};

// the server flags (the login server's last field)
enum { SERVERFLAG_Sandglass = 2, SERVERFLAG_Flag4 = 4, SERVERFLAG_Special = 8 };

// the tables of 0x1C-byte records at +0x95548 (server names, by server id from 1) and +0x96B28 (server type names, by type from
// 0), an FString at +4 in each, and the names of the special servers at +0x96314 / +0x962F8 (C4; C3: +0x628B8, +0x63E9C,
// +0x63684, +0x63668; FL2GameData in include/ue2 does not place them)
static FORCEINLINE const TCHAR* GameDataString(INT Offset)
{
    return **(FString*)((BYTE*)&GL2GameData + Offset);
}

// the console's login window and loading screen
#if 0  // C2: does not compile against the C2 headers (db/demote.csv)
static NCLoginWnd* ConsoleLoginWnd() { return GConsole->LoginWnd; }
// (C3: the stock writes it at NConsoleWnd +0x39C4, where the header has KeyCrypt)
static INT& ConsoleLoadingScreen() { return *(INT*)((BYTE*)GConsole + 0x39C4); }

/*-----------------------------------------------------------------------------
    NCServerSelectListCtrl.
-----------------------------------------------------------------------------*/

#endif
// 0x101016d0 - NCListCtrl's, with the scroll controls
NCServerSelectListCtrl::NCServerSelectListCtrl()
    : NCListCtrl(0)
{
}
NW_HOOK_CTOR(0x101016d0, NCServerSelectListCtrl)

// 0x10101770 (ret 0x10) - the rows, the visible rows, the columns and the unused one as NCListCtrl's constructor takes
// them
NCServerSelectListCtrl::NCServerSelectListCtrl(INT InMaxRows, INT InVisibleRows, INT InMaxColumns, INT Unused)
    : NCListCtrl(InMaxRows, InVisibleRows, InMaxColumns, Unused, 0)
{
}
NW_HOOK_CTOR_ARGS(0x10101770, NCServerSelectListCtrl, (INT InMaxRows, INT InVisibleRows, INT InMaxColumns, INT Unused),
                  (InMaxRows, InVisibleRows, InMaxColumns, Unused))


// 0x10101720
NW_HOOK_DTOR(0x10101720, NCServerSelectListCtrl)

// 0x100ffd10 (vslot 25)
INT NCServerSelectListCtrl::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCListCtrl::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND && wParam == NID_SERVERLIST_SORT)
        return SortByColumn((INT)lParam) != 0;
    return 0;
}
NW_HOOK_METHOD(0x100ffd10, NCServerSelectListCtrl, WindowProc)

// 0x101013a0 (vslot 81) - NCListCtrl::SortByColumn's code (five-column rows), then the row flagged "special" (bit 8 of
// the load cell's Param1) goes last. C4: the window's LastServerRow is a server id now, so the sort no longer keeps it
// on its row; the selected row is read inline (NCListCtrl::GetSelectedRow's test)
INT NCServerSelectListCtrl::SortByColumn(INT Column)
{
    NListRow* SelRow = NULL;
    if (Selected >= 0 && Selected < Rows.Num())
        SelRow = Rows(Selected);
    for (INT i = 0; i < Rows.Num(); i++)
    {
        for (INT j = 0; j < Rows.Num() - i && j < Rows.Num() - 1; j++)
        {
            const TCHAR* A = Rows(j)->Cells[Column].Text;
            if (!*A)
                continue;
            const TCHAR* B = Rows(j + 1)->Cells[Column].Text;
            if (!*B)
                continue;
            NCHeaderCtrl* Header = Columns(Column);
            UBOOL bSwap;
            if (Header->bNumeric)
            {
                if (Header->bAscending)
                    bSwap = NWtol(A) > NWtol(B);
                else
                    bSwap = NWtol(A) < NWtol(B);
            }
            else
            {
                if (Header->bAscending)
                    bSwap = _wcsicmp(A, B) > 0;
                else
                    bSwap = _wcsicmp(A, B) < 0;
            }
            if (bSwap)
            {
                NListRow* Temp = Rows(j);
                Rows(j) = Rows(j + 1);
                Rows(j + 1) = Temp;
            }
        }
    }
    INT NumRows = Rows.Num();
    for (INT i = 0; i < NumRows; i++)
    {
        if (Rows(i)->Cells[CELL_Load].Param1 & SERVERFLAG_Special)
        {
            NListRow* Temp = Rows(i);
            Rows(i) = Rows(NumRows - 1);
            Rows(Rows.Num() - 1) = Temp;
            break;
        }
    }
    Columns(Column)->bAscending = !Columns(Column)->bAscending;
    if (SelRow)
    {
        for (Selected = 0; Selected < Rows.Num(); Selected++)
        {
            if (Rows(Selected) == SelRow)
                break;
        }
    }
    return 1;
}
NW_HOOK_METHOD(0x101013a0, NCServerSelectListCtrl, SortByColumn)

NW_CHECK_VTABLE(NCServerSelectListCtrl, 0x101cac58, 82)

/*-----------------------------------------------------------------------------
    NCServerSelectWnd.
-----------------------------------------------------------------------------*/

// 0x101017a0
NCServerSelectWnd::NCServerSelectWnd()
{
    BackTex = NULL;
    LastServerID = 0;
    bRequested = 0;
    PendingPings = 0;
    LastServerRow = -1;
}
NW_HOOK_CTOR(0x101017a0, NCServerSelectWnd)

// 0x10101810
NW_HOOK_DTOR(0x10101810, NCServerSelectWnd)

// 0x100ffd70 (vslot 25) - C2 no longer refuses the messages of a disabled window here
INT NCServerSelectWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCWnd::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND)
    {
        if (wParam == NID_SERVER_OK)
            return OnOK() != 0;
        if (wParam == NID_SERVER_CANCEL)
            return OnCancel() != 0;
        if (wParam == NID_SERVER_QUALITY)
            return SortByQuality() != 0;
        if (wParam == NID_SERVER_LIST)
            return OnListNotify((INT)lParam) != 0;
    }
    return 0;
}
NW_HOOK_METHOD(0x100ffd70, NCServerSelectWnd, WindowProc)

// 0x10100090 (vslot 36)
INT NCServerSelectWnd::OnTimer(INT TimerID)
{
    if (TimerID == TIMER_SERVER_RETRY)
        bRequested = 0;
    return 1;
}
NW_HOOK_METHOD(0x10100090, NCServerSelectWnd, OnTimer)

// the three caps of a header, normal and pushed (the stock code spells them out at each column)
static FORCEINLINE void SetServerColumnCaps(NListColumnInfo* Column)
{
    Column->SetNormalTex(TEXT("L2UI_CH3.ListCtrl.tab1"), 0);
    Column->SetPushedTex(TEXT("L2UI_CH3.ListCtrl.tab1_down"), 0);
    Column->SetNormalTex(TEXT("L2UI_CH3.ListCtrl.tab2"), 1);
    Column->SetPushedTex(TEXT("L2UI_CH3.ListCtrl.tab2_down"), 1);
    Column->SetNormalTex(TEXT("L2UI_CH3.ListCtrl.tab3"), 2);
    Column->SetPushedTex(TEXT("L2UI_CH3.ListCtrl.tab3_down"), 2);
}

// 0x10101860 (vslot 37) - the backdrop, the 3 s timer and the list (100 rows, 15 visible, 5 columns; 300 x 274 at
// (8, 10), the rows' text 3 pixels down) with its five columns and the three buttons under it. No title bar control here;
// every header is 19 high and made of the three caps of "tab1".."tab3". C4: the column titles are system string ids
// (SetTitleID: 416, 451, 452, 601, 602) and the buttons are built with the C4 NCButton constructor
INT NCServerSelectWnd::OnCreate()
{
    NCWnd::OnCreate();
    BackTex = FindTexture(TEXT("L2UI_CH3.Serverselectwnd.Serverselect_back"));
    TimerResult = SetTimer(TIMER_SERVER_RETRY, 3000, NULL);

    ServerList = new NCServerSelectListCtrl(100, 15, 5, 12);
    ServerList->ID = NID_SERVER_LIST;
    ServerList->SetPosition(8, 10, 300, 274);
    CreateChild(ServerList, NWS_VISIBLE);
    ServerList->SetTextOffset(0, 3);

    NListColumnInfo NumberColumn;
    NumberColumn.Width = (GLanguageType == 2) ? 0 : 32;
    NumberColumn.Height = 19;
    if (GLanguageType != 2)
        NumberColumn.SetTitleID(416);
    SetServerColumnCaps(&NumberColumn);
    ServerList->AddColumn(0, &NumberColumn);

    NListColumnInfo NameColumn;
    NameColumn.Width = (GLanguageType != 2) ? 111 : 143;
    NameColumn.Height = 19;
    NameColumn.SetTitleID(451);              // "Name"
    SetServerColumnCaps(&NameColumn);
    ServerList->AddColumn(1, &NameColumn);

    NListColumnInfo StatusColumn;
    StatusColumn.Width = 59;
    StatusColumn.Height = 19;
    StatusColumn.SetTitleID(452);            // "Status"
    SetServerColumnCaps(&StatusColumn);
    ServerList->AddColumn(2, &StatusColumn);

    NListColumnInfo PingColumn;
    PingColumn.Width = 49;
    PingColumn.Height = 19;
    PingColumn.bNumeric = 1;
    PingColumn.SetTitleID(601);              // "Ping"
    SetServerColumnCaps(&PingColumn);
    ServerList->AddColumn(3, &PingColumn);

    NListColumnInfo TypeColumn;
    TypeColumn.Width = 49;
    TypeColumn.Height = 19;
    TypeColumn.SetTitleID(602);              // "Type"
    SetServerColumnCaps(&TypeColumn);
    ServerList->AddColumn(4, &TypeColumn);

    OKButton = new NCButton(TEXT("L2UI_CH3.Button.Btn1_normal"), TEXT("L2UI_CH3.Button.Btn1_normalOn"), 0);
    OKButton->ID = NID_SERVER_OK;
    OKButton->SetPosition(121, 297, 76, 23);
    CreateChild(OKButton, NWS_VISIBLE);
    OKButton->SetTextID(140);                // "OK"

    CancelButton = new NCButton(TEXT("L2UI_CH3.Button.Btn1_normal"), TEXT("L2UI_CH3.Button.Btn1_normalOn"), 0);
    CancelButton->ID = NID_SERVER_CANCEL;
    CancelButton->SetPosition(201, 297, 76, 23);
    CreateChild(CancelButton, NWS_VISIBLE);
    CancelButton->SetTextID(141);            // "Cancel"

    QualityButton = new NCButton(TEXT("L2UI_CH3.Button.Btn1_normal"), TEXT("L2UI_CH3.Button.Btn1_normalOn"), 0);
    QualityButton->ID = NID_SERVER_QUALITY;
    QualityButton->SetPosition(41, 297, 76, 23);
    CreateChild(QualityButton, NWS_VISIBLE);
    QualityButton->SetTextID(603);           // "Quality"

    bPingCacheOld = IsPingCacheOld();
    bPingSaved = 0;
    UnknownC4 = 0;
    return 1;
}
NW_HOOK_METHOD(0x10101860, NCServerSelectWnd, OnCreate)


// 0x101003a0 (vslot 38) - C2: a body of its own (guarded)
INT NCServerSelectWnd::OnDestroy()
{
    guard(NCServerSelectWnd::OnDestroy);
    if (ServerList)
    {
        RemoveChildWindow(ServerList->Entry);
        delete ServerList;
        ServerList = NULL;
    }
    if (OKButton)
    {
        RemoveChildWindow(OKButton->Entry);
        delete OKButton;
        OKButton = NULL;
    }
    if (CancelButton)
    {
        RemoveChildWindow(CancelButton->Entry);
        delete CancelButton;
        CancelButton = NULL;
    }
    if (QualityButton)
    {
        RemoveChildWindow(QualityButton->Entry);
        delete QualityButton;
        QualityButton = NULL;
    }
    NCWnd::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x101003a0, NCServerSelectWnd, OnDestroy)

// 0x101004d0 (vslot 62) - C3: no title bar, the back covers the whole window; the clip goes on before BeginPaint
INT NCServerSelectWnd::OnPaint(NCGDevice* Device)
{
    guard(NCServerSelectWnd::OnPaint);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    Device->BeginPaint(ScreenX, ScreenY);
    if (BackTex)
        Device->DrawTexture(0, 0, (INT)Width, (INT)Height, 0, 0, (INT)Width, (INT)Height, BackTex);
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x101004d0, NCServerSelectWnd, OnPaint)

// 0x10100060 (vslot 65) - Enter logs in
INT NCServerSelectWnd::OnKeyDown(WPARAM wParam, LPARAM lParam)
{
    if (wParam == VK_RETURN)
        OnOK();
    NCWnd::OnKeyDown(wParam, lParam);
    return 1;
}
NW_HOOK_METHOD(0x10100060, NCServerSelectWnd, OnKeyDown)

// 0x10100bf0 (vslot 74) - log in to the selected server (address, port and id from its row); its age rating (the
// ping cell's Text2) picks the Korean loading screen. C4: the "special" bit of the server's flags (the load cell's Param1)
// goes to the console too (+0x3C68), and the function has no guard
INT NCServerSelectWnd::OnOK()
{
    NListRow* Row = ServerList->GetSelectedRow();
    if (Row)
    {
        const TCHAR* Age = Row->Cells[CELL_Ping].Text2;
        INT Screen = LOADING_AllAges;
        if (Age)
        {
            if (!appStrcmp(Age, *GL2GameData.SysString[462]))       // "Over 15 years"
                Screen = LOADING_Over15;
            else if (!appStrcmp(Age, *GL2GameData.SysString[461]))  // "Over 18 years"
                Screen = LOADING_Over18;
        }
        GConsole->LoadingScreenID = Screen;
        GConsole->SpecialServer = Row->Cells[CELL_Load].Param1 & SERVERFLAG_Special;
        L2ParamStack Params(10);
        INT ServerID = Row->Param1;
        INT IP = Row->Param2;
        INT Port = appAtoi(Row->Text);
        Params.PushBack((void*)ServerID);
        Params.PushBack((void*)IP);
        Params.PushBack((void*)Port);
        if (!bRequested)
        {
            GConsole->RequestServerLogin(Params);
            SavePingTime();
            bRequested = 1;
        }
        GConsole->PlayInterfaceSound(SOUND_Click);
    }
    else
        GConsole->PlayInterfaceSound(SOUND_ClickDisabled);
    PendingPings = 0;
    return 1;
}
NW_HOOK_METHOD(0x10100bf0, NCServerSelectWnd, OnOK)

// 0x10100d80 (vslot 75) - back to the login box (C4: the login screen is the god window's child, LoginGroup)
INT NCServerSelectWnd::OnCancel()
{
    GConsole->Engine->NetHandler->AuthReconnect();
    ((NCLoginWnd*)LoginGroup(GConsole))->ResetWindows();
    LoginGroup(GConsole)->ShowWindow();
    GConsole->SetState(L2CS_Login);
    PendingPings = 0;
    SavePingTime();
    return 1;
}
NW_HOOK_METHOD(0x10100d80, NCServerSelectWnd, OnCancel)

// Bubble sort of rows First..Last by users (Cells[1].Param1, compared unsigned); inlined three
// times in SortByQuality in the stock code (name ours).
static void SortRowsByUsers(TArray<NListRow*>& Rows, INT First, INT Last)
{
    for (INT i = First; i < Last + 1; i++)
    {
        for (INT j = First; j < Last; j++)
        {
            if ((DWORD)Rows(j)->Cells[1].Param1 > (DWORD)Rows(j + 1)->Cells[1].Param1)
            {
                NListRow* Temp = Rows(j);
                Rows(j) = Rows(j + 1);
                Rows(j + 1) = Temp;
            }
        }
    }
}

// 0x10100df0 (vslot 76) - "Quality": the server played last goes to the top (swapped with the first row), the
// others are sorted by ping (the fourth cell), then each ping class (under 10 ms, 10 to 50, over 50) by users;
// unanswered servers (9999 ms) stay last, unsorted. Ends by moving the first row flagged "special" (bit 8 of its
// load cell's Param1) to the end. C4: LastServerRow is the server's id (AddServer): the row is searched by its Param1,
// and the first row is swapped (and selected) even when none matches
INT NCServerSelectWnd::SortByQuality()
{
    INT NumFast = 0;
    INT NumMid = 0;
    INT NumSlow = 0;
    TArray<NListRow*>& Rows = ServerList->Rows;
    INT LastRow = 0;
    for (INT i = 0; i < ServerList->GetRowCount(); i++)
    {
        if (Rows(i)->Param1 == LastServerRow)
        {
            LastRow = i;
            break;
        }
    }
    if (LastRow < ServerList->GetRowCount())
    {
        NListRow* Temp = Rows(0);
        Rows(0) = Rows(LastRow);
        Rows(LastRow) = Temp;
        ServerList->SetSelected(0);
    }
    for (INT i = 1; i < Rows.Num(); i++)
    {
        for (INT j = 1; j < Rows.Num() - i && j < Rows.Num() - 1; j++)
        {
            if (appAtoi(Rows(j)->Cells[CELL_Ping].Text) > appAtoi(Rows(j + 1)->Cells[CELL_Ping].Text))
            {
                NListRow* Temp = Rows(j);
                Rows(j) = Rows(j + 1);
                Rows(j + 1) = Temp;
            }
        }
    }
    for (INT i = 1; i < Rows.Num(); i++)
    {
        if (appAtoi(Rows(i)->Cells[CELL_Ping].Text) < 10)
            NumFast++;
        else if (appAtoi(Rows(i)->Cells[CELL_Ping].Text) > 50 && appAtoi(Rows(i)->Cells[CELL_Ping].Text) < PING_None)
            NumSlow++;
        else if (appAtoi(Rows(i)->Cells[CELL_Ping].Text) < PING_None)
            NumMid++;
    }
    SortRowsByUsers(Rows, 1, NumFast);
    SortRowsByUsers(Rows, NumFast + 1, NumFast + NumMid);
    SortRowsByUsers(Rows, NumFast + NumMid + 1, NumFast + NumMid + NumSlow);
    INT NumRows = Rows.Num();
    if (NumRows > 1)
    {
        for (INT i = 1; i < Rows.Num(); i++)
        {
            if (Rows(i)->Cells[CELL_Load].Param1 & SERVERFLAG_Special)
            {
                NListRow* Temp = Rows(i);
                Rows(i) = Rows(NumRows - 1);
                Rows(Rows.Num() - 1) = Temp;
                break;
            }
        }
    }
    return 1;
}
NW_HOOK_METHOD(0x10100df0, NCServerSelectWnd, SortByQuality)

// 0x10100050 (vslot 77) - double click on a row
INT NCServerSelectWnd::OnListNotify(INT Row)
{
    OnOK();
    return 1;
}
NW_HOOK_METHOD(0x10100050, NCServerSelectWnd, OnListNotify)

// (the stock body is shared with other classes' setters, 0x100dec90; the C3 map put NCLoginServerWnd::ReceiveServerList
// at 0x10091ae0 by mistake) - from the login server's list (NConsoleWnd::ReceiveServerList)
void NCServerSelectWnd::SetLastServerID(INT ServerID)
{
    LastServerID = ServerID;
}

// C2 CANDIDATE: the C2 function is not the same code as the C1 one; kept only while it matches the stock one (verify_data)
// 0x100fffb0 - NConsoleWnd::EndServerList: the last server gets selected, then the list is
// ordered
void NCServerSelectWnd::OnServerListEnd()
{
    if (LastServerRow >= 0 && LastServerRow < ServerList->GetRowCount())
        ServerList->SetSelected(LastServerRow);
    else
        LastServerRow = 0;   // 0x100fffdd: a server id that is not a row number (or none) is forgotten
    GConsole->Engine->NetHandler->EndServerPingList();
    if (bPingSaved)
        UnknownC4 = 1;
    SortByQuality();
}
NW_HOOK_METHOD(0x100fffb0, NCServerSelectWnd, OnServerListEnd)

// 0x10100020 - NCLoginWnd::ResetWindows
void NCServerSelectWnd::ClearServers()
{
    ServerList->ClearRows();
    LastServerID = 0;
    LastServerRow = -1;
    PendingPings = 0;
}
NW_HOOK_METHOD(0x10100020, NCServerSelectWnd, ClearServers)

// 0x100ffe30 (ret 0x14; this is not used) - the number text ("127" for a "special" server, "1" for a flag 4 one, "0N" under
// ten, else "N") and the name of the row about to be added: the game data's own for flags 8 and 4, NCsoft's servers' names
// (by id, up to 40) when the login server is theirs, else the game data's by id; a server type puts "[type]" in front of
// it. C4: Index is the server's id (it was the row number), and the tables moved (GameDataString)
__declspec(noinline) void NCServerSelectWnd::FormatServerLabel(INT Index, INT Flags, INT TypeIndex, TCHAR* Name,
                                                               TCHAR* NameText)
{
    TCHAR Tmp[128];
    if (Flags & SERVERFLAG_Special)
        appSprintf(NameText, TEXT("%03d"), 0x7F);
    else if (Flags & SERVERFLAG_Flag4)
        appSprintf(NameText, TEXT("%d"), 1);
    else if (Index < 10)
        appSprintf(NameText, TEXT("0%d"), Index);
    else
        appSprintf(NameText, TEXT("%d"), Index);
    const TCHAR* ServerName;
    if (Flags & SERVERFLAG_Special)
        ServerName = GameDataString(0x96314);
    else if (Flags & SERVERFLAG_Flag4)
        ServerName = GameDataString(0x962F8);
    else if (Index <= 40 &&
             (!appStricmp(GConfig->GetStr(TEXT("URL"), TEXT("ServerAddr"), NULL), TEXT("L2Server")) ||
              !appStricmp(GConfig->GetStr(TEXT("URL"), TEXT("ServerAddr"), NULL), TEXT("l2server.ncsoft.co.kr"))))
        ServerName = GServerNames[Index];
    else
        ServerName = GameDataString((Index + 0x5554) * 0x1C);
    appSprintf(Name, TEXT("%s"), ServerName);
    if (TypeIndex > 0)
    {
        appStrncpy(Tmp, Name, appStrlen(Name) + 1);
        appSprintf(Name, TEXT("[%s]%s"), GameDataString((TypeIndex + 0x561D) * 0x1C), Tmp);
    }
}
NW_HOOK_METHOD(0x100ffe30, NCServerSelectWnd, FormatServerLabel)

// 0x101005d0 - one server of the login server's list (NConsoleWnd::AddServerList): id, address, port, age limit,
// PvP, users, max users, state (0 down, 1 up), flags and the server type, in that order. Cells: the number (not for
// the special server, nor in language 2), the name (red when the server's bit is set in the window's bit array), the load
// (PvP in Text2, the flags in Param1; users and max in Cells[1]'s params; colored by the state and the load), the ping
// (age limit in Text2) and a sandglass in the icon column for flag 2. The saved ping is shown when there is a fresh
// one, else the server is asked (one more answer awaited). For other states the load text is left as it was on the
// stack, as in the stock code. C4: everything is keyed by the server's id now (the label, the bit of the array, the saved
// ping, the ping request), LastServerRow keeps the id of the server played last, and there is no guard
void NCServerSelectWnd::AddServer(L2ParamStack& Params)
{
    INT ServerID = (INT)Params.Top();
    INT IP = (INT)Params.Top();
    INT Port = (INT)Params.Top();
    INT AgeLimit = (INT)Params.Top();
    INT bPvP = (INT)Params.Top();
    INT Users = (INT)Params.Top();
    INT MaxUsers = (INT)Params.Top();
    INT State = (INT)Params.Top();
    INT Flags = (INT)Params.Top();
    INT TypeIndex = (INT)Params.Top();
    TCHAR LoadText[32];
    TCHAR NameText[32];
    TCHAR AgeText[32];
    TCHAR Name[96];
    TCHAR PingText[32];
    TCHAR PvPText[32];

    NListRow* Row = new NListRow(5);
    if (ServerID == LastServerID)
        LastServerRow = ServerID;
    FormatServerLabel(ServerID, Flags, TypeIndex, Name, NameText);
    if (ServerID / 8 < 16)
    {
        BYTE Bit = (BYTE)pow(2.0, (DOUBLE)(ServerID % 8));
        if (RowBits[ServerID / 8] & Bit)
            Row->Cells[CELL_Name].SetColor(0xFFFF0000);
    }
    if (!(Flags & SERVERFLAG_Special) && GLanguageType != 2)
        Row->Cells[CELL_Number].SetText(NameText);
    Row->Cells[CELL_Name].SetText(Name);
    Row->Cells[CELL_Ping].SetText(TEXT("9999"));

    if (State == 0)
    {
        appSprintf(LoadText, TEXT("%s"), *GL2GameData.SysString[456]);
        Row->Cells[CELL_Load].SetColor(0xFFFF0000);
    }
    else if (State == 1)
    {
        if ((SWORD)MaxUsers > 0)
        {
            FLOAT Load = (FLOAT)(SWORD)Users / (SWORD)MaxUsers * 100.f;
            if (Load < 80.f && (SWORD)Users < 3700)
            {
                if (Load > 50.f)
                {
                    appSprintf(LoadText, TEXT("%s"), *GL2GameData.SysString[457]);
                    Row->Cells[CELL_Load].SetColor(0xFFBFAE64);
                }
                else
                {
                    appSprintf(LoadText, TEXT("%s"), *GL2GameData.SysString[466]);
                    Row->Cells[CELL_Load].SetColor(0xFF64A3D9);
                }
            }
            else
            {
                appSprintf(LoadText, TEXT("%s"), *GL2GameData.SysString[458]);
                Row->Cells[CELL_Load].SetColor(0xFFFF8080);
            }
        }
        else
        {
            appSprintf(LoadText, TEXT("%s"), *GL2GameData.SysString[457]);
            Row->Cells[CELL_Load].SetColor(0xFFBFAE64);
        }

        if (!bPingCacheOld && !HasNoPing(ServerID))
        {
            INT Ping = PING_None;
            GConfig->GetInt(appItoa(ServerID), TEXT("Time"), Ping, TEXT("s_info.ini"));
            if (Ping == PING_None)
                appSprintf(PingText, TEXT("9999"));
            else
                appSprintf(PingText, TEXT("%d"), Ping);
            Row->Cells[CELL_Ping].SetText(PingText);
        }
        else
        {
            Row->Cells[CELL_Ping].SetText(TEXT("9999"));
            GConsole->Engine->NetHandler->AddServerPingList(ServerID, IP, Port);
            PendingPings++;
        }
    }

    Row->Cells[CELL_Load].SetText(LoadText);
    if (bPvP == 1)
        appSprintf(PvPText, TEXT("%s"), *GL2GameData.SysString[459]);      // "On"
    else
        appSprintf(PvPText, TEXT("%s"), *GL2GameData.SysString[460]);      // "Off"
    if (AgeLimit == 18)
        appSprintf(AgeText, TEXT("%s"), *GL2GameData.SysString[461]);      // "Over 18 years"
    else if (AgeLimit == 15)
        appSprintf(AgeText, TEXT("%s"), *GL2GameData.SysString[462]);      // "Over 15 years"
    else
        appSprintf(AgeText, TEXT("%s"), *GL2GameData.SysString[539]);      // "Teen"
    if (Flags & SERVERFLAG_Sandglass)
        Row->Cells[CELL_Icon].SetIcon(FindTexture(TEXT("symbol.sandglass")), 10, 12);
    Row->SetParams(ServerID, IP, -1);
    Row->SetText(appItoa(Port));
    Row->Cells[CELL_Name].SetParams((SWORD)Users, (SWORD)MaxUsers, -1);
    Row->Cells[CELL_Load].SetText2(PvPText);
    Row->Cells[CELL_Load].SetParams(Flags, -1, -1);
    Row->Cells[CELL_Ping].SetText2(AgeText);
    ServerList->AddRow(Row);
}
NW_HOOK_METHOD(0x101005d0, NCServerSelectWnd, AddServer)

// 0x10100b10 - a ping answer (NConsoleWnd vslot 260): the rows of that address show it and save it; the list is
// ordered again when the last awaited answer arrives
void NCServerSelectWnd::SetPing(INT IP, INT Ping)
{
    TCHAR Text[32];
    if (!ServerList)
        return;
    for (INT i = 0; i < ServerList->Rows.Num(); i++)
    {
        NListRow* Row = ServerList->Rows(i);
        if (Row->Param2 == IP && Row->Cells)
        {
            if (Ping == PING_None)
                appSprintf(Text, TEXT("9999"));
            else
                appSprintf(Text, TEXT("%d"), Ping);
            Row->Cells[CELL_Ping].SetText(Text);
            SavePing(Row->Param1, Ping);   // C4: the section is the server's id (AddServer / HasNoPing read it by id); C3 had id - 1
            if (--PendingPings == 0)
            {
                SortByQuality();
                PendingPings = 0;
            }
        }
    }
}
NW_HOOK_METHOD(0x10100b10, NCServerSelectWnd, SetPing)

// 0x10100230 - no saved time, or more than 12 units of GetTimeStamp old. Not inlined: the stock OnCreate calls it
__declspec(noinline) UBOOL NCServerSelectWnd::IsPingCacheOld()
{
    INT Stamp;
    if (GConfig->GetInt(TEXT("Date"), TEXT("Time"), Stamp, TEXT("s_info.ini")))
        return Stamp + 12 < GetTimeStamp();
    return 1;
}
NW_HOOK_METHOD(0x10100230, NCServerSelectWnd, IsPingCacheOld)

// 0x10100280 - no saved ping for the server, or 9999
UBOOL NCServerSelectWnd::HasNoPing(INT ServerIndex)
{
    INT Ping = 0;
    if (GConfig->GetInt(appItoa(ServerIndex), TEXT("Time"), Ping, TEXT("s_info.ini")))
        return Ping == PING_None;
    return 1;
}
NW_HOOK_METHOD(0x10100280, NCServerSelectWnd, HasNoPing)

// 0x101002e0 - not inlined: the stock callers call it
__declspec(noinline) void NCServerSelectWnd::SavePing(INT ServerIndex, INT Ping)
{
    TCHAR Section[8];
    appSprintf(Section, TEXT("%d"), ServerIndex);
    GConfig->SetInt(Section, TEXT("Time"), Ping, TEXT("s_info.ini"));
    bPingSaved = 1;
}
NW_HOOK_METHOD(0x101002e0, NCServerSelectWnd, SavePing)

// 0x10100330 - after new pings were asked: when they were taken. Not inlined: the stock callers call it.
__declspec(noinline) void NCServerSelectWnd::SavePingTime()
{
    if (bPingCacheOld)
    {
        GConfig->SetInt(TEXT("Date"), TEXT("Time"), GetTimeStamp(), TEXT("s_info.ini"));
        bPingCacheOld = 0;
    }
}
NW_HOOK_METHOD(0x10100330, NCServerSelectWnd, SavePingTime)

// 0x101000b0 - not inlined: the stock DaysInYears and DaysInMonths call it
__declspec(noinline) UBOOL NCServerSelectWnd::IsLeapYear(INT Year)
{
    return (Year % 4 == 0 && Year % 100 != 0) || Year % 400 == 0;
}
NW_HOOK_FUNC(0x101000b0, NCServerSelectWnd::IsLeapYear)

// 0x101000f0 - days of the years 1 .. Years
INT NCServerSelectWnd::DaysInYears(INT Years)
{
    INT Days = 0;
    for (INT i = 1; i <= Years; i++)
        Days += IsLeapYear(i) ? 366 : 365;
    return Days;
}
NW_HOOK_FUNC(0x101000f0, NCServerSelectWnd::DaysInYears)

// 0x10100130 - days of the months before Month (January = 1)
INT NCServerSelectWnd::DaysInMonths(INT Year, INT Month)
{
    INT February = 28 + (IsLeapYear(Year) != 0);
    INT Days = 0;
    for (INT i = 1; i < Month; i++)
    {
        if (i == 1 || i == 3 || i == 5 || i == 7 || i == 8 || i == 10 || i == 12)
            Days += 31;
        else if (i == 2)
            Days += February;
        else
            Days += 30;
    }
    return Days;
}
NW_HOOK_FUNC(0x10100130, NCServerSelectWnd::DaysInMonths)

// 0x101001a0 - hours of the days before Day, plus Hour
INT NCServerSelectWnd::HoursInDays(INT Day, INT Hour)
{
    INT Hours = 0;
    if (Day > 1)
        Hours = (Day - 1) * 24;
    return Hours + Hour;
}
NW_HOOK_FUNC(0x101001a0, NCServerSelectWnd::HoursInDays)

// 0x101001d0 - "now" for the ping cache: the days of the past years and of the months before
// last month plus the hours of this month, added as they are (the stock code mixes units)
INT NCServerSelectWnd::GetTimeStamp()
{
    INT Year, Month, Day, Hour, Min;
    appLocalTime(Year, Month, Day, Hour, Min);
    INT Stamp = DaysInMonths(Year, Month - 1);
    Stamp += DaysInYears(Year - 1);
    return HoursInDays(Day, Hour) + Stamp;
}
NW_HOOK_FUNC(0x101001d0, NCServerSelectWnd::GetTimeStamp)

NW_CHECK_VTABLE(NCServerSelectWnd, 0x101cada0, 78)

/*-----------------------------------------------------------------------------
    NCServerInfoWnd (C3): the help page under the server list.
-----------------------------------------------------------------------------*/

// 0x1009dfe0 - NCWnd's constructor and the vtable (the stock one returns this)
NCServerInfoWnd::NCServerInfoWnd()
{
}
NW_HOOK_CTOR(0x1009dfe0, NCServerInfoWnd)

// 0x1009e030
NW_HOOK_DTOR(0x1009e030, NCServerInfoWnd)

// C2 CANDIDATE: the C2 function is not the same code as the C1 one; kept only while it matches the stock one (verify_data)
// 0x101015d0 (vslot 37) - the backdrop and the HTML viewer, inside it 7 pixels from every side; the page is not loaded in
// the Thai mode (its word wrapper does that later). NCWnd::OnCreate is called; no guard
INT NCServerInfoWnd::OnCreate()
{
    NCWnd::OnCreate();
    BackTex = FindTexture(TEXT("L2UI_CH3.Serverselectwnd.Serverinfo_back"));

    Viewer = NewStock<NCHtmlViewer>(0x1003edf0);
    Viewer->SetPosition(7, 7, (INT)(Width - 14.0f), (INT)(Height - 14.0f));
    CreateChild(Viewer, NWS_VISIBLE);
    if (!GL2ThaiHtmlMode)
        Viewer->LoadHtmlFile(TEXT("..\\l2text\\server_help.htm"));
    return 1;
}
NW_HOOK_METHOD(0x101015d0, NCServerInfoWnd, OnCreate)

// 0x101012f0 (vslot 38) - NCWnd's OnDestroy, then the viewer goes
INT NCServerInfoWnd::OnDestroy()
{
    guard(NCServerInfoWnd::OnDestroy);
    NCWnd::OnDestroy();
    if (Viewer)
    {
        RemoveChildWindow(Viewer->Entry);
        delete Viewer;
        Viewer = NULL;
    }
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x101012f0, NCServerInfoWnd, OnDestroy)

// 0x101011f0 (vslot 62) - the backdrop stretched over the window, when there is one
INT NCServerInfoWnd::OnPaint(NCGDevice* Device)
{
    guard(NCServerInfoWnd::OnPaint);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    Device->BeginPaint(ScreenX, ScreenY);
    if (BackTex)
        Device->DrawTexture(0, 0, (INT)Width, (INT)Height, 0, 0, (INT)Width, (INT)Height, BackTex);
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x101011f0, NCServerInfoWnd, OnPaint)

NW_CHECK_VTABLE(NCServerInfoWnd, 0x101b8460, 74)
