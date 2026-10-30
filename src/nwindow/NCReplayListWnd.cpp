/*=============================================================================
    NCReplayListWnd.cpp - the list of the saved replays (new in C4; C4 addresses). Stock: WindowProc 0x100F7700, OnKeyDown
    0x100F77E0, the small slots 0x100F7820 / 0x100F7830, OnDestroy 0x100F7860, OnPaint 0x100F79E0, the play and delete handlers
    0x100F7AE0 / 0x100F7DE0, the list filling 0x100F7C70, ShowWindow 0x100F7DD0, the constructor 0x100F7F30, the destructor
    0x100F7F80 and OnCreate 0x100F7FD0. The scalar deleting destructor (0x100F7F50) and InternalConstructor (0x100F8600) stay
    stock. The anonymous 0x100F7B80 / 0x100F7C20 (TArray<FString>::Remove and its destructor) are the compiler's instances.
=============================================================================*/
#include "nwindow/NCReplayListWnd.h"
#include "nwindow/NCButton.h"
#include "nwindow/NCCheckBox.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NCGodWnd.h"
#include "nwindow/NCLoginWnd.h"
#include "nwindow/NCReplayListCtrl.h"
#include "nwindow/NConsoleWnd.h"

// NWM_COMMAND ids of the children (names ours)
enum
{
    NID_RLIST_LIST = 0xF030,
    NID_RLIST_PLAY = 0xF031,
    NID_RLIST_DELETE = 0xF032,
    NID_RLIST_CANCEL = 0xF033,
    NID_RLIST_CHECK1 = 0xF035,
    NID_RLIST_CHECK2 = 0xF036,
};

// sysstring lines of the window's texts (names ours)
enum
{
    STR_RLIST_ColumnNumber = 416,
    STR_RLIST_ColumnName = 1250,
    STR_RLIST_Check1 = 1249,
    STR_RLIST_Check2 = 1253,
    STR_RLIST_Play = 140,
    STR_RLIST_Delete = 1272,
    STR_RLIST_Cancel = 141,
};

// A child window goes (repeated inline in the stock OnDestroy)
template <class T> static FORCEINLINE void DestroyChild(NCWnd* Parent, T*& Child)
{
    if (Child)
    {
        Parent->RemoveChildWindow(Child->Entry);
        delete Child;
        Child = NULL;
    }
}

// 0x100f7f30 - the background texture comes with OnCreate
NCReplayListWnd::NCReplayListWnd()
{
    BackTex = NULL;
}
NW_HOOK_CTOR(0x100f7f30, NCReplayListWnd)

// 0x100f7f80
NW_HOOK_DTOR(0x100f7f80, NCReplayListWnd)

// 0x100f7700 (vslot 25) - NCWnd's, then the children's commands by id: the three buttons, the first check box and the list (a
// double click on a row, lParam is passed on)
INT NCReplayListWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCWnd::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND)
    {
        if (wParam == NID_RLIST_PLAY)
            return OnPlayBtnClick() != 0;
        if (wParam == NID_RLIST_DELETE)
            return OnDeleteBtnClick() != 0;
        if (wParam == NID_RLIST_CANCEL)
            return OnCancelBtnClick() != 0;
        if (wParam == NID_RLIST_CHECK1)
            return OnCheckClick() != 0;
        if (wParam == NID_RLIST_LIST)
            return OnListDblClick((INT)lParam) != 0;
    }
    return 0;
}
NW_HOOK_METHOD(0x100f7700, NCReplayListWnd, WindowProc)

// 0x100f77e0 (vslot 65) - Return plays the selected replay, Escape cancels; NCWnd's OnKeyDown follows either way
INT NCReplayListWnd::OnKeyDown(WPARAM wParam, LPARAM lParam)
{
    if (wParam == 0xD)
        OnPlayBtnClick();
    else if (wParam == 0x1B)
        OnCancelBtnClick();
    NCWnd::OnKeyDown(wParam, lParam);
    return 1;
}
NW_HOOK_METHOD(0x100f77e0, NCReplayListWnd, OnKeyDown)

// 0x100f7820 (vslot 74; name ours, ret 4) - a double click on a row plays it (the row is already selected)
INT NCReplayListWnd::OnListDblClick(INT Param)
{
    OnPlayBtnClick();
    return 1;
}
NW_HOOK_METHOD(0x100f7820, NCReplayListWnd, OnListDblClick)

// 0x100f7830 (vslot 77; name ours) - cancel: the god window's login screen takes its slot 75 call (back to where the replay
// menu came from)
INT NCReplayListWnd::OnCancelBtnClick()
{
    static_cast<NCGodWnd*>(GConsole->GodWnd)->LoginWnd->NewVirtual75();
    return 1;
}
NW_HOOK_METHOD(0x100f7830, NCReplayListWnd, OnCancelBtnClick)

// 0x100f7850 (vslot 78; name ours) - a body shared by many classes: returns 1 (the first check box needs nothing)
#if NW_HOOK_MODE
NW_NAKED INT NCReplayListWnd::OnCheckClick() NW_STOCK(0x100f7850)
#else
INT NCReplayListWnd::OnCheckClick()
{
    return 1;   // 0x100f7850: mov eax, 1 / ret
}
#endif

// 0x100f7860 (vslot 38, guard) - the six children go in order, then NCWnd's
INT NCReplayListWnd::OnDestroy()
{
    guard(NCReplayListWnd::OnDestroy);
    DestroyChild(this, ListCtrl);
    DestroyChild(this, Check1);
    DestroyChild(this, Check2);
    DestroyChild(this, PlayButton);
    DestroyChild(this, DeleteButton);
    DestroyChild(this, CancelButton);
    NCWnd::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100f7860, NCReplayListWnd, OnDestroy)

// 0x100f79e0 (vslot 62, guard) - the background texture over the whole window
INT NCReplayListWnd::OnPaint(NCGDevice* Device)
{
    guard(NCReplayListWnd::OnPaint);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    Device->BeginPaint(ScreenX, ScreenY);
    if (BackTex)
        Device->DrawTexture(0, 0, (INT)Width, (INT)Height, 0, 0, (INT)Width, (INT)Height, BackTex, 0xFF, 1);
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100f79e0, NCReplayListWnd, OnPaint)

// 0x100f7ae0 (vslot 75; name ours) - the selected replay's name goes to the replay manager with the two check boxes' states
INT NCReplayListWnd::OnPlayBtnClick()
{
    TCHAR Name[256];
    NListRow* Row = ListCtrl->GetSelectedRow();
    if (Row)
    {
        const TCHAR* Text = Row->Cells[1].Text;
        if (!Text[0])
            Text = NULL;
        appStrcpy(Name, Text);
        UBOOL bCheck1 = Check1->bChecked;
        UBOOL bCheck2 = Check2->bChecked;
        FL2ReplayManager* Manager = GConsole->Engine->ReplayManager;
        if (Manager)
            Manager->OnPublicLoad(FString(Name), bCheck1, bCheck2);
    }
    return 1;
}
NW_HOOK_METHOD(0x100f7ae0, NCReplayListWnd, OnPlayBtnClick)

// 0x100f7c70 (anonymous, name ours) - the files of ..\REPLAY\*.L2R become the rows: the number of the file and its name
// without ".L2R"
void NCReplayListWnd::Refresh()
{
    TCHAR Name[256];
    TArray<FString> Files;
    ::nw::VirtualCall<void>(GFileManager, 12, &Files, TEXT("..\\REPLAY\\*.L2R"), 1, 0);   // FFileManager::FindFiles
    ListCtrl->ClearRows();
    for (INT i = 0; i < Files.Num(); i++)
    {
        appStrcpy(Name, *Files(i));
        // sa-emu: the stock searches L"." (0x101A14A0, at 0x100F7CF2 / 0x100F7D05): the name is cut at its FIRST dot, not at ".L2R"
        if (appStrstr(Name, TEXT(".")))
            *appStrstr(Name, TEXT(".")) = 0;
        NListRow* Row = new NListRow(2);
        Row->Cells[0].SetNumber(i);
        Row->Cells[1].SetText(Name);
        ListCtrl->AddRow(Row);
    }
}
NW_HOOK_METHOD(0x100f7c70, NCReplayListWnd, Refresh)

// 0x100f7dd0 (vslot 48) - the list is filled again, then NCWnd's
void NCReplayListWnd::ShowWindow()
{
    Refresh();
    NCWnd::ShowWindow();
}
NW_HOOK_METHOD(0x100f7dd0, NCReplayListWnd, ShowWindow)

// 0x100f7de0 (vslot 76; name ours) - the selected replay's file is deleted and the list filled again
INT NCReplayListWnd::OnDeleteBtnClick()
{
    TCHAR Path[256];
    NListRow* Row = ListCtrl->GetSelectedRow();
    if (Row)
    {
        const TCHAR* Text = Row->Cells[1].Text;
        if (!Text[0])
            Text = NULL;
        appSprintf(Path, TEXT("%s%s%s"), TEXT("..\\REPLAY\\"), Text, TEXT(".L2R"));
        ::nw::VirtualCall<INT>(GFileManager, 5, Path, 0, 0);   // FFileManager::Delete
    }
    Refresh();
    return 1;
}
NW_HOOK_METHOD(0x100f7de0, NCReplayListWnd, OnDeleteBtnClick)

// 0x100f7fd0 (vslot 37) - the background, the list (256 rows, 15 visible, two columns: the number 32 wide and the name 268
// wide, 19 high) at (8, 11), the two check boxes under it and the three buttons below them
INT NCReplayListWnd::OnCreate()
{
    NListColumnInfo NumberColumn;
    NListColumnInfo NameColumn;
    NCWnd::OnCreate();
    BackTex = FindTexture(TEXT("L2UI_CH3.ReplayListWnd.replay_back"));

    ListCtrl = new NCReplayListCtrl(0x100, 0xF, 2, 0x13);
    ListCtrl->ID = NID_RLIST_LIST;
    ListCtrl->SetPosition(8, 0xB, 0x12C, 0x111);
    CreateChild(ListCtrl, NWS_VISIBLE);

    NumberColumn.Width = 0x20;
    NumberColumn.Height = 0x13;
    NumberColumn.SetTitleID(STR_RLIST_ColumnNumber);
    NumberColumn.SetNormalTex(TEXT("L2UI_CH3.ListCtrl.tab1"), 0);
    NumberColumn.SetPushedTex(TEXT("L2UI_CH3.ListCtrl.tab1_down"), 0);
    NumberColumn.SetNormalTex(TEXT("L2UI_CH3.ListCtrl.tab2"), 1);
    NumberColumn.SetPushedTex(TEXT("L2UI_CH3.ListCtrl.tab2_down"), 1);
    NumberColumn.SetNormalTex(TEXT("L2UI_CH3.ListCtrl.tab3"), 2);
    NumberColumn.SetPushedTex(TEXT("L2UI_CH3.ListCtrl.tab3_down"), 2);
    ListCtrl->AddColumn(0, &NumberColumn);

    NameColumn.Width = 0x10C;
    NameColumn.Height = 0x13;
    NameColumn.SetTitleID(STR_RLIST_ColumnName);
    NameColumn.SetNormalTex(TEXT("L2UI_CH3.ListCtrl.tab1"), 0);
    NameColumn.SetPushedTex(TEXT("L2UI_CH3.ListCtrl.tab1_down"), 0);
    NameColumn.SetNormalTex(TEXT("L2UI_CH3.ListCtrl.tab2"), 1);
    NameColumn.SetPushedTex(TEXT("L2UI_CH3.ListCtrl.tab2_down"), 1);
    NameColumn.SetNormalTex(TEXT("L2UI_CH3.ListCtrl.tab3"), 2);
    NameColumn.SetPushedTex(TEXT("L2UI_CH3.ListCtrl.tab3_down"), 2);
    ListCtrl->AddColumn(1, &NameColumn);

    Check1 = new NCCheckBox(STR_RLIST_Check1, 0, 0, 0);
    Check1->ID = NID_RLIST_CHECK1;
    Check1->SetPosition(0xB, 0x127, 0x8C, 0x10);
    CreateChild(Check1, NWS_VISIBLE);

    Check2 = new NCCheckBox(STR_RLIST_Check2, 0, 0, 0);
    Check2->ID = NID_RLIST_CHECK2;
    Check2->SetPosition(0xA1, 0x127, 0x8C, 0x10);
    CreateChild(Check2, NWS_VISIBLE);

    PlayButton = new NCButton(TEXT("L2UI_CH3.Button.Btn1_normal"), TEXT("L2UI_CH3.Button.Btn1_normalOn"), 0);
    PlayButton->ID = NID_RLIST_PLAY;
    PlayButton->SetPosition(0x29, 0x13D, 0x4C, 0x17);
    CreateChild(PlayButton, NWS_VISIBLE);
    PlayButton->SetTextID(STR_RLIST_Play);

    DeleteButton = new NCButton(TEXT("L2UI_CH3.Button.Btn1_normal"), TEXT("L2UI_CH3.Button.Btn1_normalOn"), 0);
    DeleteButton->ID = NID_RLIST_DELETE;
    DeleteButton->SetPosition(0x79, 0x13D, 0x4C, 0x17);
    CreateChild(DeleteButton, NWS_VISIBLE);
    DeleteButton->SetTextID(STR_RLIST_Delete);

    CancelButton = new NCButton(TEXT("L2UI_CH3.Button.Btn1_normal"), TEXT("L2UI_CH3.Button.Btn1_normalOn"), 0);
    CancelButton->ID = NID_RLIST_CANCEL;
    CancelButton->SetPosition(0xC9, 0x13D, 0x4C, 0x17);
    CreateChild(CancelButton, NWS_VISIBLE);
    CancelButton->SetTextID(STR_RLIST_Cancel);

    Refresh();
    return 1;
}
NW_HOOK_METHOD(0x100f7fd0, NCReplayListWnd, OnCreate)

NW_CHECK_VTABLE(NCReplayListWnd, 0x101c8740, 79)
