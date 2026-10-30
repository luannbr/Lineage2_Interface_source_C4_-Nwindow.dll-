/*=============================================================================
    NCQuestListInfoWnd.cpp - the list of the quests the player can take (new in C4). Stock (C4): 0x100EA250 (WindowProc),
    0x100EA2B0 (the rows), 0x100EB960 .. 0x100EBB70 (OnDestroy, OnPaint, OnMouseMove), 0x100EC140 (the button),
    0x100EC600 (OnCreate), 0x100ECFB0 (the "?" icon's tool tip), 0x100ED9B0 / 0x100EDA10 / 0x100EDA40 (constructor, scalar
    deleting destructor, destructor).
=============================================================================*/
#include "nwindow/NCQuestListInfoWnd.h"
#include "nwindow/NCButton.h"
#include "nwindow/NCConsole.h"
#include "nwindow/NCFrameCtrl.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NCListCtrl.h"
#include "nwindow/NCMainWnd.h"
#include "nwindow/NCMapCoverWnd.h"
#include "nwindow/NCMinimapWnd.h"
#include "nwindow/NCQuestGaraTreeWnd.h"
#include "nwindow/NConsoleWnd.h"
#include "nwindow/NWinBStock.h"

// sysstring lines (names ours)
enum
{
    STR_QuestRangeUnit = 859,        // after the level when the quest has no upper limit ("%d %s")
    STR_QuestInfo1 = 861,            // the two lines of the "?" icon's tool tip
    STR_QuestInfo2 = 862,
    STR_QuestAnyLevel = 866,         // the level column when the quest has no lower limit
    STR_Move = 0x35C,                // the button's text
};

// the table's record as this window reads it: the fields FL2QuestData leaves in its _Unknown34 (names ours; offsets from the
// stock listings of 0x100EA2B0 and 0x100EC140), the size of the record is FL2QuestData's
struct NQuestListRecord
{
    INT bValid;                      // +0x00
    INT QuestID;                     // +0x04
    INT Step;                        // +0x08 (the list shows the first one)
    FString Name;                    // +0x0C
    BYTE _Unknown18[0x5C - 0x18];
    INT MinLevel;                    // +0x5C
    INT MaxLevel;                    // +0x60 (0: no upper limit)
    INT Type;                        // +0x64 0 .. 3: the icon of the type column, the number too
    BYTE _Unknown68[0x78 - 0x68];
    INT NpcID;                       // +0x78 the NPC of the quest (npcname-*.dat)
    INT Position[3];                 // +0x7C the NPC's place
    FString Region;                  // +0x88 the second column
    FString Summary;                 // +0x94 the row's own text
};
NW_ASSERT_OFFSET(NQuestListRecord, MinLevel, 0x5C);
NW_ASSERT_OFFSET(NQuestListRecord, NpcID, 0x78);
NW_ASSERT_OFFSET(NQuestListRecord, Region, 0x88);
NW_ASSERT_SIZE(NQuestListRecord, 0xA0);

// a child window goes (repeated inline in the stock OnDestroy)
template <class T> static FORCEINLINE void DestroyChild(NCWnd* Parent, T*& Child)
{
    if (Child)
    {
        Parent->RemoveChildWindow(Child->Entry);
        delete Child;
        Child = NULL;
    }
}

// the usual three tab textures of a list header, and one column added (the stock builds a new 0x4018 byte description on
// the stack for each: the header of the clan list and the manor windows do the same)
static FORCEINLINE void AddListColumn(NCListCtrl* List, INT Width, INT TitleID, UBOOL bNumeric)
{
    NListColumnInfo Column;
    Column.Width = Width;
    Column.Height = 0x13;
    Column.SetTitleID(TitleID);
    Column.SetNormalTex(TEXT("L2UI_CH3.ListCtrl.tab1"), 0);
    Column.SetPushedTex(TEXT("L2UI_CH3.ListCtrl.tab1_down"), 0);
    Column.SetNormalTex(TEXT("L2UI_CH3.ListCtrl.tab2"), 1);
    Column.SetPushedTex(TEXT("L2UI_CH3.ListCtrl.tab2_down"), 1);
    Column.SetNormalTex(TEXT("L2UI_CH3.ListCtrl.tab3"), 2);
    Column.SetPushedTex(TEXT("L2UI_CH3.ListCtrl.tab3_down"), 2);
    if (bNumeric)
        Column.bNumeric = 1;
    List->AddColumn(0, &Column);
}

// 0x100ed9b0 - NCWnd's constructor, the vtable, the tool tip's constructor and the flag
NCQuestListInfoWnd::NCQuestListInfoWnd()
{
    bHover = 0;
}
NW_HOOK_CTOR(0x100ed9b0, NCQuestListInfoWnd)

// 0x100eda40 - ConditionalDestroy, the tool tip's destructor, NCWnd's destructor
NW_HOOK_DTOR(0x100eda40, NCQuestListInfoWnd)

// 0x100ea250 (vslot 25) - the base's answer first; then the button's command
INT NCQuestListInfoWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCWnd::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND && wParam == (WPARAM)NID_QUESTLIST_MOVE)
        return OnMoveBtnClick() != 0;
    return 0;
}
NW_HOOK_METHOD(0x100ea250, NCQuestListInfoWnd, WindowProc)

// 0x100ea2b0 (anonymous in the db) - a row for the first step of every quest of the table: the name, the region, the level
// (from - to, from and "up", or the line for any level), the type (its number is kept, only the icon is drawn), the NPC;
// the row keeps the quest's id and its summary. A type the icons do not cover keeps the icon the previous row had (this window
// for the first one): the stock sets that register once, before the loop (0x100ea2d7: ebp = this; the loop head is 0x100ea2e0)
void NCQuestListInfoWnd::FillList()
{
    UTexture* Icon = (UTexture*)this;
    for (INT Offset = 0; Offset < 2000 * 0xA0; Offset += 0xA0)
    {
        NQuestListRecord* Quest = (NQuestListRecord*)((BYTE*)&GL2GameData + 0x3D28 + Offset);
        if (Quest->bValid != 0 && Quest->Step == 1)
        {
            NListRow* Row = new NListRow(5);
            Row->Cells[0].SetText(*Quest->Name);
            Row->Cells[1].SetText(*Quest->Region);
            TCHAR Level[128];
            if (Quest->MinLevel > 0)
            {
                if (Quest->MaxLevel > 0)
                    appSprintf(Level, TEXT("%d~%d"), Quest->MinLevel, Quest->MaxLevel);
                else
                    appSprintf(Level, TEXT("%d %s"), Quest->MinLevel, *GL2GameData.SysString[STR_QuestRangeUnit]);
            }
            else
                appSprintf(Level, TEXT("%s"), *GL2GameData.SysString[STR_QuestAnyLevel]);
            Row->Cells[2].SetText(Level);
            switch ((DWORD)Quest->Type)
            {
            case 0:
            case 2:
                Icon = Icons[0];
                break;
            case 1:
            case 3:
                Icon = Icons[1];
                break;
            }
            Row->Cells[3].SetParams(Quest->Type, -1, -1);
            Row->Cells[3].SetNumber(Quest->Type);
            Row->Cells[3].SetIcon(Icon, 15, 15);
            Row->Cells[3].bTextVisible = 0;
            Row->Cells[4].SetText(GL2GameData.GetNpcName(Quest->NpcID));
            Row->SetText(*Quest->Summary);
            Row->SetParams(Quest->QuestID, -1, -1);
            Row->ToolTip.FillMatchRow(Row, 0x10000);
            ListCtrl->AddRow(Row);
        }
    }
}
NW_HOOK_METHOD(0x100ea2b0, NCQuestListInfoWnd, FillList)

// 0x100eb960 (vslot 38) - the base first; then the list and the button
INT NCQuestListInfoWnd::OnDestroy()
{
    NCWnd::OnDestroy();
    DestroyChild(this, ListCtrl);
    DestroyChild(this, MoveButton);
    return 1;
}
NW_HOOK_METHOD(0x100eb960, NCQuestListInfoWnd, OnDestroy)

// 0x100eb9d0 (vslot 62) - the panel under the title bar and the "?" icon
INT NCQuestListInfoWnd::OnPaint(NCGDevice* Device)
{
    guard(NCQuestListInfoWnd::OnPaint);
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    if (BackTex)
    {
        INT H = (INT)(Height - 20.0f);
        INT W = (INT)Width;
        Device->DrawTexture(0, 20, W, H, 0, 0, W, H, BackTex, 0xFF, 1);
    }
    if (ToolTipBtnTex)
        Device->DrawTexture(0x242, 0x1E, 15, 15, 0, 0, 15, 15, ToolTipBtnTex, 0xFF, 1);
    Device->ReleaseClip();
    Device->EndPaint();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100eb9d0, NCQuestListInfoWnd, OnPaint)

// 0x100ebb00 (vslot 67) - the "?" icon (15 x 15 at (578, 30)) shows the tool tip while the mouse is over it
INT NCQuestListInfoWnd::OnMouseMove(WPARAM wParam, LPARAM lParam)
{
    NRectF Icon;
    Icon.X = ScreenX + 0x242;
    Icon.Y = ScreenY + 0x1E;
    Icon.W = 15.0f;
    Icon.H = 15.0f;
    bHover = PtInRect((SHORT)lParam, (SHORT)(lParam >> 16), Icon) != 0;
    return 1;
}
NW_HOOK_METHOD(0x100ebb00, NCQuestListInfoWnd, OnMouseMove)

// 0x100ec140 (vslot 74) - the selected quest: its NPC's name and place go to the main window's quest tab (SetToolTipInfo, the
// quest id as the fourth number), the console is told (its slot 376, 0x10082730), and the map shows the mark of the report
// with its list emptied
INT NCQuestListInfoWnd::OnMoveBtnClick()
{
    NListRow* Row = ListCtrl->GetSelectedRow();
    if (Row)
    {
        NQuestListRecord* Quest = (NQuestListRecord*)GL2GameData.GetQuestData(Row->Param1, 1);
        if (Quest)
        {
            ((NCQuestGaraTreeWnd*)GDesktop->MainWnd->QuestWnd)
                ->SetToolTipInfo(FString(GL2GameData.GetNpcName(Quest->NpcID)), Quest->Position[0], Quest->Position[1],
                                 Quest->Position[2], Quest->QuestID);
            ::nw::VirtualCall<void>(GConsole, 376, 1);
            NCConsole* Desktop = GConsole->GetDesktop();
            if (Desktop)
            {
                NCMapCoverWnd* Cover = (NCMapCoverWnd*)Desktop->MapCoverWnd;
                if (Cover)
                {
                    NCMinimapWnd* Map = Cover->Map;
                    if (Map)
                    {
                        Map->bShowMark = 1;
                        Map->MarkPos.Empty();
                    }
                }
            }
        }
    }
    return 1;
}
NW_HOOK_METHOD(0x100ec140, NCQuestListInfoWnd, OnMoveBtnClick)

// 0x100ecfb0 (vslot 63) - the "?" icon's tool tip, drawn by the window's skin
INT NCQuestListInfoWnd::OnDrawToolTip(NCGDevice* Device)
{
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    if (bHover && ToolTipSkin)
    {
        NToolTipRect Wnd;
        Wnd.X = ScreenX;
        Wnd.Y = ScreenY;
        Wnd.W = Width;
        Wnd.H = Height;
        ToolTipSkin->Draw(Device, ToolTip, 0x242, 0x1E, Wnd);
    }
    Device->ReleaseClip();
    Device->EndPaint();
    return 1;
}
NW_HOOK_METHOD(0x100ecfb0, NCQuestListInfoWnd, OnDrawToolTip)

// 0x100ec600 (vslot 37) - the panel, the "?" icon and the five info icons; a plain title bar; the list (five columns) with a
// row for every quest; the button; the tool tip's two lines with their icons. The id of the button is the one WindowProc
// answers to
INT NCQuestListInfoWnd::OnCreate()
{
    NCWnd::OnCreate();
    BackTex = FindTexture(TEXT("L2UI_CH3.QUESTWND.questinfo_back"));
    ToolTipBtnTex = FindTexture(TEXT("L2UI_CH3.QUESTWND.QuestWndToolTipBtn"));
    Icons[0] = FindTexture(TEXT("L2UI_CH3.QUESTWND.QuestWndInfoIcon_1"));
    Icons[1] = FindTexture(TEXT("L2UI_CH3.QUESTWND.QuestWndInfoIcon_2"));
    Icons[2] = FindTexture(TEXT("L2UI_CH3.QUESTWND.QuestWndInfoIcon_3"));
    Icons[3] = FindTexture(TEXT("L2UI_CH3.QUESTWND.QuestWndInfoIcon_4"));
    Icons[4] = FindTexture(TEXT("L2UI_CH3.QUESTWND.QuestWndInfoIcon_5"));

    FrameCtrl = new NCFrameCtrl(1, 0, 0);
    FrameCtrl->SetPosition(0, 0, (INT)Width, 20);
    CreateChild(FrameCtrl, NWS_VISIBLE);

    ListCtrl = new NCListCtrl(2000, 13, 5, 16, 0, 17);
    ListCtrl->ID = NID_QUESTLIST_LIST;
    ListCtrl->SetPosition(7, 49, 586, 237);
    CreateChild(ListCtrl, 0x22);
    AddListColumn(ListCtrl, 0xD4, 0x4B0, 0);
    AddListColumn(ListCtrl, 0x82, 0x4B1, 0);
    AddListColumn(ListCtrl, 0x34, 0x39A, 1);
    AddListColumn(ListCtrl, 0x28, 0x4B2, 0);
    AddListColumn(ListCtrl, 0x98, 0x4B3, 0);

    MoveButton = new NCButton(TEXT("L2UI_CH3.Button.btn1_normal"), TEXT("L2UI_CH3.Button.btn1_normalon"), 0);
    MoveButton->ID = NID_QUESTLIST_MOVE;
    MoveButton->SetPosition(518, 297, 76, 23);
    CreateChild(MoveButton, NWS_VISIBLE);
    MoveButton->SetTextID(STR_Move);

    // the two arrays' destructors are the stock out-of-line ones, called by address so the inline template code is not
    // repeated here (as in NCQuestGaraTreeWnd::OnCreate)
    BYTE NamesStorage[sizeof(TArray<FString*>)];
    BYTE IconsStorage[sizeof(TArray<UTexture*>)];
    TArray<FString*>& Names = *new ((void*)NamesStorage) TArray<FString*>();
    TArray<UTexture*>& TipIcons = *new ((void*)IconsStorage) TArray<UTexture*>();
    Names.AddItem(new FString(*GL2GameData.SysString[STR_QuestInfo1]));
    Names.AddItem(new FString(*GL2GameData.SysString[STR_QuestInfo2]));
    TipIcons.AddItem(FindTexture(TEXT("L2UI_CH3.QuestWnd.QuestWndInfoIcon_1")));
    TipIcons.AddItem(FindTexture(TEXT("L2UI_CH3.QuestWnd.QuestWndInfoIcon_2")));
#if NW_HOOK_MODE
    ::nw::StockCallAt<void>(0x10030670, &ToolTip, &Names, &TipIcons, 0x1000);
#else
    ((NToolTipInfoLinesView*)&ToolTip)->SetIconLines(&Names, &TipIcons, 0x1000);   // 0x10030670
#endif
    for (INT i = 0; i < Names.Num(); i++)
    {
        Names(i)->Empty();
        delete Names(i);
    }
    Names.Empty();
    TipIcons.Empty();
    FillList();
#if NW_HOOK_MODE
    ::nw::StockCallAt<void>(0x1008E8A0, &Names);
    ::nw::StockCallAt<void>(0x1001F6E0, &TipIcons);
#else
    Names.~TArray<FString*>();      // the stock's out-of-line destructors 0x1008E8A0 / 0x1001F6E0
    TipIcons.~TArray<UTexture*>();
#endif
    return 1;
}
NW_HOOK_METHOD(0x100ec600, NCQuestListInfoWnd, OnCreate)

NW_CHECK_VTABLE(NCQuestListInfoWnd, 0x101c62a0, 75)
