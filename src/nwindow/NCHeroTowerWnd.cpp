/*=============================================================================
    NCHeroTowerWnd.cpp - the hero tower's window (C4 only). Stock: 0x1005fa60 (constructor), 0x1005fab0 (destructor), 0x1013baf0 ..
    0x1013c230 (the methods, OnDestroy, OnPaint, WindowProc, OnCreate); 0x10021b90 and 0x10021bb0 (the list cell's
    picture slots, anonymous in the db, used only here).
=============================================================================*/
#include "nwindow/NCHeroTowerWnd.h"
#include "nwindow/NCButton.h"
#include "nwindow/NCEditBox.h"
#include "nwindow/NCFrameCtrl.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NCListCtrl.h"
#include "nwindow/NColumnInfoC4.h"
#include "nwindow/NConsoleWnd.h"

// NWM_COMMAND ids (names ours)
enum
{
    NID_HEROTOWER_FRAME = 0xDFDDCEA0,
    NID_HEROTOWER_LIST = 0xDFDDCEB0,
    NID_HEROTOWER_EDIT = 0xDFDDCEC0,
    NID_HEROTOWER_WORDS = 0xDFDDCED0,
    NID_HEROTOWER_CONTENT = 0xDFDDCEE0,
};

// A stand-in for the two functions of the list cell that only this window calls (the cell's sixteen pictures sit at +0x1004, 0x24
// bytes each)
struct NHeroListCellFns : public NListCell
{
    __declspec(noinline) void ClearImages();
    __declspec(noinline) void AddImage(UTexture* Tex, INT X, INT Y, INT W, INT H, INT U, INT V, INT UL, INT VL);
};

// 0x10021b90 - no picture on the cell
void NHeroListCellFns::ClearImages()
{
    for (INT i = 0; i < 16; i++)
        Images[i].Tex = NULL;
}
NW_HOOK_METHOD(0x10021b90, NHeroListCellFns, ClearImages)

// 0x10021bb0 (ret 0x24) - the picture goes in the first free slot (all taken: nothing)
void NHeroListCellFns::AddImage(UTexture* Tex, INT X, INT Y, INT W, INT H, INT U, INT V, INT UL, INT VL)
{
    for (INT i = 0; i < 16; i++)
    {
        if (Images[i].Tex == NULL)
        {
            Images[i].Tex = Tex;
            Images[i].X = X;
            Images[i].Y = Y;
            Images[i].W = W;
            Images[i].H = H;
            Images[i].U = U;
            Images[i].V = V;
            Images[i].UL = UL;
            Images[i].VL = VL;
            return;
        }
    }
}
NW_HOOK_METHOD(0x10021bb0, NHeroListCellFns, AddImage)

// 0x1005fa60 - NCWnd's constructor; the fields are left as allocated
NCHeroTowerWnd::NCHeroTowerWnd()
{
}
NW_HOOK_CTOR(0x1005fa60, NCHeroTowerWnd)

// 0x1005fab0
NW_HOOK_DTOR(0x1005fab0, NCHeroTowerWnd)

// a column of the list (the stock reuses one record, 0x4018 bytes, for the five: the first sets the height and the textures)
static FORCEINLINE void AddHeroColumn(NCListCtrl* List, NColumnInfoC4& Info, INT Index, INT Width, INT TitleID)
{
    Info.Width = Width;
    Info.SetTitleID(TitleID);
    List->AddColumn(Index, (NListColumnInfo*)&Info);
}

// 0x1013c2c0 (vslot 37, guard) - the title bar (as wide as the window, 20 px high), the background piece, the list of the heroes
// (1000 rows, 13 shown, five columns: name, class, pledge, alliance, wins), the words box (64 characters at most) and the two
// buttons (SysString 1192 and 1193)
INT NCHeroTowerWnd::OnCreate()
{
    guard(NCHeroTowerWnd::OnCreate);
    NColumnInfoC4 Column;
    NCWnd::OnCreate();
    FrameCtrl = new NCFrameCtrl(1, 0, 0);
    FrameID = NID_HEROTOWER_FRAME;
    FrameCtrl->ID = NID_HEROTOWER_FRAME;
    FrameCtrl->SetPosition(0, 0, (INT)Width, 20);
    CreateChild(FrameCtrl, NWS_VISIBLE);
    BackTex = FindTexture(TEXT("L2UI_CH3.HeroTower.herotower1_back"));

    HeroList = new NCListCtrl(1000, 13, 5, 16, 0, 18);
    HeroList->ID = NID_HEROTOWER_LIST;
    HeroList->SetPosition(7, 34, 586, 255);
    CreateChild(HeroList, NWS_VISIBLE);
    HeroList->SetTextOffset(0, 3);
    Column.Height = 19;
    Column.SetNormalTex(TEXT("L2UI_CH3.ListCtrl.tab1"), 0);
    Column.SetTabDownTex(TEXT("L2UI_CH3.ListCtrl.tab1_down"), 0);
    Column.SetNormalTex(TEXT("L2UI_CH3.ListCtrl.tab2"), 1);
    Column.SetTabDownTex(TEXT("L2UI_CH3.ListCtrl.tab2_down"), 1);
    Column.SetNormalTex(TEXT("L2UI_CH3.ListCtrl.tab3"), 2);
    Column.SetTabDownTex(TEXT("L2UI_CH3.ListCtrl.tab3_down"), 2);
    AddHeroColumn(HeroList, Column, 0, 0xA9, 393);
    AddHeroColumn(HeroList, Column, 1, 0x78, 391);
    AddHeroColumn(HeroList, Column, 2, 0x80, 604);
    AddHeroColumn(HeroList, Column, 3, 0x77, 605);
    AddHeroColumn(HeroList, Column, 4, 0x32, 1191);

    WordsEdit = new NCEditBox(0, NULL, 1, 0x108, 0);
    WordsEdit->ID = NID_HEROTOWER_EDIT;
    WordsEdit->SetPosition(25, 300, 405, 16);
    CreateChild(WordsEdit, NWS_VISIBLE);
    WordsEdit->SetMaxLength(64);

    WordsButton = new NCButton(0, 0);
    WordsButton->ID = NID_HEROTOWER_WORDS;
    WordsButton->SetPosition(0x1B8, 0x129, 0, 0);
    CreateChild(WordsButton, NWS_VISIBLE);
    WordsButton->SetTextID(1192);

    ContentButton = new NCButton(0, 0);
    ContentButton->ID = NID_HEROTOWER_CONTENT;
    ContentButton->SetPosition(0x206, 0x129, 0, 0);
    CreateChild(ContentButton, NWS_VISIBLE);
    ContentButton->SetTextID(1193);
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1013c2c0, NCHeroTowerWnd, OnCreate)

// 0x1013baf0 (guard) - the words typed go through the console's word filter to the server and the box empties
INT NCHeroTowerWnd::OnHeroWordsBtnClick()
{
    guard(NCHeroTowerWnd::OnHeroWordsBtnClick);
    TCHAR Words[1024];
    appStrcpy(Words, *WordsEdit->Left);
    GConsole->HasBadWord(Words);
    GConsole->RequestWriteHeroWords(FString(Words));
    WordsEdit->Left = TEXT("");
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1013baf0, NCHeroTowerWnd, OnHeroWordsBtnClick)

// 0x1013bbd0 (guard) - the selected hero's diary page goes to the server as a bypass (the row's Param1 is his class)
INT NCHeroTowerWnd::OnShowContentBtnClick()
{
    guard(NCHeroTowerWnd::OnShowContentBtnClick);
    NListRow* Row = HeroList->GetSelectedRow();
    if (Row)
    {
        TCHAR Bypass[1024];
        appSprintf(Bypass, TEXT("_diary?class=%d&page=1"), Row->Param1);
        L2ParamStack Params(10);
        Params.PushBack(Bypass);
        GConsole->RequestBypassToServer(Params);
    }
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1013bbd0, NCHeroTowerWnd, OnShowContentBtnClick)

// 0x1013bcb0 (guard) - the list the server sends: the rows go, then for each hero (name, class, pledge, pledge crest, alliance,
// alliance crest, wins) a row of five cells; the class is the row's Param1; the pledge cell carries the crests (the alliance's 8 x 12
// first, the pledge's 16 x 12 after it); the three texts the packet carries are freed. The window shows, and the words box and
// button only when the player is a hero (User +0x240)
void NCHeroTowerWnd::OnHeroList(L2ParamStack& Params)
{
    guard(NCHeroTowerWnd::OnHeroList);
    HeroList->ClearRows();
    INT Count = (INT)Params.Top();
    for (INT i = 0; i < Count; i++)
    {
        TCHAR* Name = (TCHAR*)Params.Top();
        INT ClassID = (INT)Params.Top();
        TCHAR* Pledge = (TCHAR*)Params.Top();
        INT PledgeID = (INT)Params.Top();
        TCHAR* Ally = (TCHAR*)Params.Top();
        INT AllyID = (INT)Params.Top();
        INT Wins = (INT)Params.Top();
        NListRow* Row = new NListRow(7);
        Row->Cells[0].SetText(Name);
        Row->Cells[1].SetText(GConsole->GetClassNameString(ClassID));
        Row->Cells[2].SetText(Pledge);
        Row->Cells[3].SetText(Ally);
        Row->Cells[4].SetText(*FString::Printf(TEXT("%d"), Wins));
        Row->Param1 = ClassID;
        NHeroListCellFns* PledgeCell = (NHeroListCellFns*)&Row->Cells[2];
        PledgeCell->ClearImages();
        UTexture* PledgeCrest = GConsole->GetPledgeCrestTexture(PledgeID);
        UTexture* AllyCrest = GConsole->GetAllyCrestTexture(AllyID);
        if (AllyCrest)
        {
            PledgeCell->AddImage(AllyCrest, 10, 0, 8, 12, 0, 4, 0, 0);
            if (PledgeCrest)
                PledgeCell->AddImage(PledgeCrest, 0x12, 0, 0x10, 12, 0, 4, 0, 0);
        }
        else if (PledgeCrest)
        {
            PledgeCell->AddImage(PledgeCrest, 10, 0, 0x10, 12, 0, 4, 0, 0);
        }
        appFree(Name);
        appFree(Pledge);
        appFree(Ally);
        HeroList->AddRow(Row);
    }
    ShowWindow();
    User* Me = GConsole->GetMyUser();
    if (Me)
    {
        if (::nw::Field<INT>(Me, 0x240) != 0)
        {
            WordsEdit->ShowWindow();
            WordsButton->ShowWindow();
        }
        else
        {
            WordsEdit->HideWindow();
            WordsButton->HideWindow();
        }
    }
    unguard;
}
NW_HOOK_METHOD(0x1013bcb0, NCHeroTowerWnd, OnHeroList)

// 0x1013bf80 (vslot 38, guard) - the list, the words box and the two buttons go (each: out of the children, deleted), then NCWnd's
// OnDestroy
INT NCHeroTowerWnd::OnDestroy()
{
    guard(NCHeroTowerWnd::OnDestroy);
    if (HeroList)
    {
        RemoveChildWindow(HeroList->Entry);
        delete HeroList;
        HeroList = NULL;
    }
    if (WordsEdit)
    {
        RemoveChildWindow(WordsEdit->Entry);
        delete WordsEdit;
        WordsEdit = NULL;
    }
    if (WordsButton)
    {
        RemoveChildWindow(WordsButton->Entry);
        delete WordsButton;
        WordsButton = NULL;
    }
    if (ContentButton)
    {
        RemoveChildWindow(ContentButton->Entry);
        delete ContentButton;
        ContentButton = NULL;
    }
    NCWnd::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1013bf80, NCHeroTowerWnd, OnDestroy)

// 0x1013c0b0 (vslot 62, guard) - the background piece from y = 20 down, as wide as the window and 20 less high
INT NCHeroTowerWnd::OnPaint(NCGDevice* Device)
{
    guard(NCHeroTowerWnd::OnPaint);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    Device->BeginPaint(ScreenX, ScreenY);
    if (BackTex)
        Device->DrawTexture(0, 0x14, (INT)Width, (INT)(Height - 20.0f), 0, 0, (INT)Width, (INT)(Height - 20.0f), BackTex, 0xFF, 1);
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1013c0b0, NCHeroTowerWnd, OnPaint)

// 0x1013c1b0 (guard, ret 4) - a double click on a row: the diary page of the selected hero
INT NCHeroTowerWnd::OnListCtrl(INT Row)
{
    guard(NCHeroTowerWnd::OnListCtrl);
    OnShowContentBtnClick();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1013c1b0, NCHeroTowerWnd, OnListCtrl)

// 0x1013c230 (vslot 25)
INT NCHeroTowerWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCWnd::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND)
    {
        if (wParam == NID_HEROTOWER_LIST)
            return OnListCtrl((INT)lParam) != 0;
        if (wParam == NID_HEROTOWER_WORDS)
            return OnHeroWordsBtnClick() != 0;
        if (wParam == NID_HEROTOWER_CONTENT)
            return OnShowContentBtnClick() != 0;
    }
    return 0;
}
NW_HOOK_METHOD(0x1013c230, NCHeroTowerWnd, WindowProc)

NW_CHECK_VTABLE(NCHeroTowerWnd, 0x101ae648, 74)
