/*=============================================================================
    NCQuestWnd.cpp - the quest window. Stock: 0x100a0370 .. 0x100a04e8, 0x100a0fd0 ..
    0x100a13b6, 0x100a1580 .. 0x100a1656 and OnCreate (0x100a1890), plus the destructor
    (0x1005c6b0, among NCGMQuestWnd's code).
=============================================================================*/
#include "nwindow/NCQuestWnd.h"
#include "nwindow/NCButton.h"
#include "nwindow/NCDialogBox.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NCListBox.h"
#include "nwindow/NCQuestInfoWnd.h"
#include "nwindow/NCQuestItemWnd.h"
#include "nwindow/NCTabControl.h"
#include "nwindow/NConsoleWnd.h"
#include "nwindow/NDialogInfo.h"

// ids (names ours)
enum
{
    NID_QUEST_TAB = 0xF6,
    NID_QUEST_CANCEL = 0xF7,
    NID_QUEST_LIST = 0x1A03,
    NID_QUEST_NEW1 = 0xF8,            // new in C2
    NID_QUEST_NEW2 = 0xF9,
};
enum { DIALOG_CANCEL_QUEST = 30 };      // NDialogInfo::ID (name ours)
enum { TUTORIAL_QUEST_ITEMS = 0x2000 }; // the item page opened (name ours)

// sysstring
enum
{
    STR_AcceptedQuest = 324,
    STR_Description = 325,
    STR_Item = 326,
    STR_Cancel = 327,
};

// 0x100eef60 - C3: the drop rectangle is (160, 420) 27.0 x 27.0 (a FLOAT size)
NCQuestWnd::NCQuestWnd()
{
    QuestList = NULL;
    InfoWnd = NULL;
    ItemWnd = NULL;
    TabCtrl = NULL;
    DropRect.Y = 0;
    DropRect.X = 0;
    DropRect.W = 0.0f;
    DropRect.H = 0.0f;
    CurTab = 0;
    DropRect.W = 27.0f;
    DropRect.H = 27.0f;
    DropRect.X = 160;
    DropRect.Y = 420;
}
NW_HOOK_CTOR(0x100eef60, NCQuestWnd)


// 0x100edb60 (vslot 25) - C2 has no IsEnabled check of its own, and two more commands
INT NCQuestWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCWnd::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_DIALOG_RESULT)
        return OnDialogResult(wParam, lParam) != 0;
    if (Msg == NWM_COMMAND)
    {
        if (wParam == NID_QUEST_CANCEL)
            return OnCancel() != 0;
        if (wParam == NID_QUEST_LIST)
            return OnSelectQuest((INT)lParam) != 0;
        if (wParam == NID_QUEST_TAB)
            return OnTab((INT)lParam) != 0;
        if (wParam == NID_QUEST_NEW1)
            return OnNewButton1() != 0;
        if (wParam == NID_QUEST_NEW2)
            return OnNewButton2() != 0;
    }
    return 0;
}
NW_HOOK_METHOD(0x100edb60, NCQuestWnd, WindowProc)

// 0x100edc60 (vslot 78) - Cancel (and the two new children) only on the description page; the item page
// counts as a tutorial event
INT NCQuestWnd::OnTab(INT Page)
{
    if (Page == 0)
    {
        CurTab = 0;
        CancelButton->ShowWindow();
        NewChild[1]->ShowWindow();
        NewChild[0]->ShowWindow();
        return 1;
    }
    if (Page == 1)
    {
        CurTab = 1;
        CancelButton->HideWindow();
        NewChild[1]->HideWindow();
        NewChild[0]->HideWindow();
        GConsole->SendTutorialEvent(TUTORIAL_QUEST_ITEMS);
    }
    return 1;
}
NW_HOOK_METHOD(0x100edc60, NCQuestWnd, OnTab)

// 0x100edd00 (vslot 74) - edges included
UBOOL NCQuestWnd::IsInDropZone(INT X, INT Y)
{
    X -= ScreenX + DropRect.X;
    Y -= ScreenY + DropRect.Y;
    return X >= 0 && X <= (INT)DropRect.W && Y >= 0 && Y <= (INT)DropRect.H;
}
NW_HOOK_METHOD(0x100edd00, NCQuestWnd, IsInDropZone)

// 0x100ee700 (vslot 38) - C2 guards it
INT NCQuestWnd::OnDestroy()
{
    guard(NCQuestWnd::OnDestroy);
    if (QuestList)
    {
        RemoveChildWindow(QuestList->Entry);
        delete QuestList;
        QuestList = NULL;
    }
    if (InfoWnd)
    {
        RemoveChildWindow(InfoWnd->Entry);
        delete InfoWnd;
        InfoWnd = NULL;
    }
    if (ItemWnd)
    {
        RemoveChildWindow(ItemWnd->Entry);
        delete ItemWnd;
        ItemWnd = NULL;
    }
    if (TabCtrl)
    {
        RemoveChildWindow(TabCtrl->Entry);
        delete TabCtrl;
        TabCtrl = NULL;
    }
    if (CancelButton)
    {
        RemoveChildWindow(CancelButton->Entry);
        delete CancelButton;
        CancelButton = NULL;
    }
    if (NewChild[0])
    {
        RemoveChildWindow(NewChild[0]->Entry);
        delete NewChild[0];
        NewChild[0] = NULL;
    }
    if (NewChild[1])
    {
        RemoveChildWindow(NewChild[1]->Entry);
        delete NewChild[1];
        NewChild[1] = NULL;
    }
    NCWnd::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100ee700, NCQuestWnd, OnDestroy)

// 0x100eec10 (vslot 72, new in C2; name ours) - the command 0xF8: the arrow forward, the next step
INT NCQuestWnd::OnNewButton1()
{
    InfoWnd->NextStep();
    return 1;
}
NW_HOOK_METHOD(0x100eec10, NCQuestWnd, OnNewButton1)

// 0x100eec30 (vslot 73, new in C2; name ours) - the command 0xF9: the arrow back, the step before
INT NCQuestWnd::OnNewButton2()
{
    InfoWnd->PrevStep();
    return 1;
}
NW_HOOK_METHOD(0x100eec30, NCQuestWnd, OnNewButton2)

// 0x100ee8b0 (vslot 62) - BeginPaint comes before the clip here; the title is centered in the 105 pixels from
// x 22 (C3: its color is 0xFFDCDCDC)
INT NCQuestWnd::OnPaint(NCGDevice* Device)
{
    guard(NCQuestWnd::OnPaint);
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    if (TopTex)
        Device->DrawTexture(0, 0, 240, 115, 0, 0, 240, 115, TopTex);
    UTexture* PageTex = CurTab == 0 ? InfoBackTex : ItemBackTex;
    if (PageTex)
        Device->DrawTexture(0, 115, 240, 185, 0, 0, 240, 185, PageTex);
    if (ItemPageIconTex && CurTab)
        Device->DrawTexture(220, 127, 0, 14, 0, 0, 32, 32, ItemPageIconTex);   // the width is 0 in the stock code
    SIZE Size;
    NCGDevice::GetTextExtent(&Size, *SysString(STR_AcceptedQuest), 0, 0, 0, 0, 0);
    Device->DrawText((105 - Size.cx) / 2 + 22, 3, 0xFFDCDCDC, *SysString(STR_AcceptedQuest), 0, 0, 0,
                     L2FONT_Normal, 0, 0, 0, 0, 0, 0);
    if (QuestList)
    {
        // how many quests are accepted, of 15
        TCHAR CountText[16];
        appSprintf(CountText, TEXT("(%d/%d)"), QuestList->NumLines, 15);
        Device->DrawText(190, 3, 0xFFB09B79, CountText, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
    }
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100ee8b0, NCQuestWnd, OnPaint)

// 0x100eeac0 (vslot 75) - whatever the answer: the Cancel question only reports OK
INT NCQuestWnd::OnDialogResult(WPARAM wParam, LPARAM lParam)
{
    // the console's slot 399 by number: NConsoleWnd.h compiles its new virtuals 76 slots too high for now
    ::nw::VirtualCall<void>(GConsole, 433, QuestList->GetItemParam(QuestList->Selected));
    return 1;
}
NW_HOOK_METHOD(0x100eeac0, NCQuestWnd, OnDialogResult)

// 0x100eeaf0 (vslot 76) - asks before giving up the selected quest (systemmsg 182)
INT NCQuestWnd::OnCancel()
{
    guard(NCQuestWnd::OnCancel);
    if (QuestList->Selected >= 0 && QuestList->Selected < QuestList->NumLines
        && QuestList->GetItemParam(QuestList->Selected) >= 0)
    {
        NDialogInfo Info;
        Info.ID = DIALOG_CANCEL_QUEST;
        Info.bMainBox = 1;
        Info.bInput = 0;
        Info.Owner = Entry;
        Info.SetSystemMessage(182);  // "Do you want to quit the current quest?"
        GConsole->ShowDialog(&Info);
    }
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100eeaf0, NCQuestWnd, OnCancel)

// 0x100eebf0 (vslot 77) - the description page shows the quest picked in the list
INT NCQuestWnd::OnSelectQuest(INT Index)
{
    InfoWnd->ShowQuest(Index);
    return 1;
}
NW_HOOK_METHOD(0x100eebf0, NCQuestWnd, OnSelectQuest)

// 0x100ef610 - the list, the descriptions and the items go
void NCQuestWnd::Clear()
{
    QuestList->Clear();
    InfoWnd->ClearQuests();
    ::nw::VirtualCall<void>(ItemWnd, 78);   // NCItemWnd::Clear by number: NCScrollWnd.h shifts NCItemWnd's slots by one for now
}
NW_HOOK_METHOD(0x100ef610, NCQuestWnd, Clear)

// 0x100ef640 (vslot 48) - emptied, then filled again by the server's answer; the arrows come and go with the
// description page
void NCQuestWnd::ShowWindow()
{
    Clear();
    ::nw::VirtualCall<UBOOL>(GConsole, 432);   // RequestQuestList: by number (see OnDialogResult)
    NCWnd::ShowWindow();
    TabCtrl->ShowCurrentPage();
    if (CurTab)
    {
        CancelButton->HideWindow();
        NewChild[1]->HideWindow();
        NewChild[0]->HideWindow();
    }
    else
    {
        CancelButton->ShowWindow();
        NewChild[1]->ShowWindow();
        NewChild[0]->ShowWindow();
    }
}
NW_HOOK_METHOD(0x100ef640, NCQuestWnd, ShowWindow)

// 0x100ef000 - nothing of its own to free: the children are destroyed by OnDestroy
NW_HOOK_DTOR(0x100ef000, NCQuestWnd)

// 0x100ef050 (vslot 37) - NCWnd::OnCreate is not called (no title bar here); the list and both pages are created
// with the style bit 0x20. Everything is moved down by the title bar's height when the window has one
// (FrameCtrl), and the two arrows of the description page are the C2 additions
INT NCQuestWnd::OnCreate()
{
    INT Y = FrameCtrl ? 0x15 : 0;
    TopTex = FindTexture(TEXT("L2UI.QuestWnd.questBack1"));
    InfoBackTex = FindTexture(TEXT("L2UI.QuestWnd.questBack2"));
    ItemBackTex = FindTexture(TEXT("L2UI.QuestWnd.questBack3"));
    ItemPageIconTex = FindTexture(TEXT("sek.cbui303"));
    QuestList = new NCListBox(165, 5, 16);
    QuestList->ID = NID_QUEST_LIST;
    QuestList->SetPosition(17, Y + 31, 220, 74);
    CreateChild(QuestList, NWS_VISIBLE | NWS_UNKNOWN20);   // 0x22
    QuestList->TextX = 1;
    QuestList->TextY = 1;
    QuestList->LineGap = 1;
    InfoWnd = new NCQuestInfoWnd();
    InfoWnd->SetPosition(18, Y + 150, 220, 122);
    CreateChild(InfoWnd, NWS_VISIBLE | NWS_UNKNOWN20);
    ItemWnd = new NCQuestItemWnd(6, 4, 80, 32);   // 0x100dddd0
    ItemWnd->SetPosition(16, Y + 144, 220, 139);
    CreateChild(ItemWnd, NWS_VISIBLE | NWS_UNKNOWN20);
    TabCtrl = new NCTabControl();
    TabCtrl->ID = NID_QUEST_TAB;
    TabCtrl->SetPosition(15, Y + 117, 138, 15);
    CreateChild(TabCtrl, NWS_VISIBLE);
    TabCtrl->AddTab(STR_Description, InfoWnd, 69, 16, TEXT("L2UI.NWindow.Tab2"), TEXT("L2UI.NWindow.Tab1"),
                    0, -1);
    TabCtrl->AddTab(STR_Item, ItemWnd, 69, 16, TEXT("L2UI.NWindow.Tab2"), TEXT("L2UI.NWindow.Tab1"), 0,
                    -1);
    CancelButton = new NCButton(TEXT("L2UI.Control.Defaultbutton"), TEXT("L2UI.Control.Defaultbutton_click"), 0);
    CancelButton->ID = NID_QUEST_CANCEL;
    CancelButton->SetPosition(87, Y + 277, 67, 19);
    CreateChild(CancelButton, NWS_VISIBLE);
    CancelButton->SetTextID(STR_Cancel);
    // the arrows under the description: forward (command 0xF8) and back (0xF9)
    NCPushButton* Next = new NCPushButton(TEXT("L2UI.ShortcutWnd.RightBtn1"), TEXT("L2UI.ShortcutWnd.RightBtn2"));
    NewChild[1] = Next;
    Next->ID = NID_QUEST_NEW1;
    Next->SetPosition(183, Y + 279, 13, 13);
    CreateChild(Next, NWS_VISIBLE);
    Next->RepeatDelay = 180;
    NCPushButton* Prev = new NCPushButton(TEXT("L2UI.ShortcutWnd.LeftBtn1"), TEXT("L2UI.ShortcutWnd.LeftBtn2"));
    NewChild[0] = Prev;
    Prev->ID = NID_QUEST_NEW2;
    Prev->SetPosition(45, Y + 279, 13, 13);
    CreateChild(Prev, NWS_VISIBLE);
    Prev->RepeatDelay = 180;
    return 1;
}
NW_HOOK_METHOD(0x100ef050, NCQuestWnd, OnCreate)

NW_CHECK_VTABLE(NCQuestWnd, 0x101c67c8, 81)
