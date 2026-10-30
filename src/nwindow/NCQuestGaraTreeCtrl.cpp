/*=============================================================================
    NCQuestGaraTreeCtrl.cpp - the scrolling window of the quest tree: NCGaraTreeCtrl's code with its own tree. C3
    stock: 0x100DB070 .. 0x100DB370 (paint, reset, scroll down), 0x100DC110 .. 0x100DC27F (constructor, destructor,
    OnCreate), 0x100E3ED0 (wheel), 0x100EAF80 (destroy), 0x100ECA00 .. 0x100ECC1D (arrange, scroll up, scroll bar).
=============================================================================*/
#include "nwindow/NCQuestGaraTreeCtrl.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NCSliderBar.h"

// 0x100ed120 - the base's, then the tree's root and the selected id empty
NCQuestGaraTreeCtrl::NCQuestGaraTreeCtrl()
{
    RootItem = NULL;
    SelectedID = 0;
    Unknown0C8 = 16;
}
NW_HOOK_CTOR(0x100ed120, NCQuestGaraTreeCtrl)

// 0x100ed180
NW_HOOK_DTOR(0x100ed180, NCQuestGaraTreeCtrl)

// C2 CANDIDATE: the C2 function is not the same code as the C1 one; kept only while it matches the stock one (verify_data)
// 0x100ed1d0 (vslot 37) - NCScrollWnd's (not NCGaraTreeCtrl's), then the quest tree (id 0xA00)
INT NCQuestGaraTreeCtrl::OnCreate()
{
    NCScrollWnd::OnCreate();
    QuestTree = new NCQuestGaraTree();
    QuestTree->ID = 0xA00;
    QuestTree->SetPosition(0, 0, (INT)(Width - 16.0f), (INT)Height);
    CreateChild(QuestTree, NWS_VISIBLE);
    return 1;
}
NW_HOOK_METHOD(0x100ed1d0, NCQuestGaraTreeCtrl, OnCreate)

// 0x100eb1f0 (vslot 38) - no guard frame in C4; the quest tree goes, then NCScrollWnd's OnDestroy
INT NCQuestGaraTreeCtrl::OnDestroy()
{
    if (QuestTree)
    {
        RemoveChildWindow(QuestTree->Entry);
        delete QuestTree;
        QuestTree = NULL;
    }
    NCScrollWnd::OnDestroy();
    return 1;
}
NW_HOOK_METHOD(0x100eb1f0, NCQuestGaraTreeCtrl, OnDestroy)

// 0x100eb230 (vslot 62) - the quests that were not clicked close; the scroll range follows the tree's height (without
// the base's 21), and the entries are put where they show
INT NCQuestGaraTreeCtrl::OnPaint(NCGDevice* Device)
{
    guard(NCQuestGaraTreeCtrl::OnPaint);
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    NCGaraTVItem* Root = QuestTree->Root;
    if (Root)
    {
        for (INT i = 0; i < Root->Children.Num(); i++)
        {
            NCGaraTVItem* Item = Root->Children(i);
            if (Item->bClicked == 0 && Item->bExpanded != 0)
                Item->bExpanded = 0;
        }
    }
    SetContentHeightEx(QuestTree->ContentHeight);
    if (QuestTree->Root)
        ArrangeItem(QuestTree->Root);
    Device->ReleaseClip();
    Device->EndPaint();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100eb230, NCQuestGaraTreeCtrl, OnPaint)

// 0x100ff3f0 (vslot 79) - the base's, the box of an entry against the quest tree's window
void NCQuestGaraTreeCtrl::ArrangeItem(NCGaraTVItem* Item)
{
    if (!Item)
        return;
    INT Right = Item->CopyW + ScrollX;
    INT Bottom = Item->CopyH + ScrollY;
    NRectF Local;
    Local.X = 0;
    Local.Y = 0;
    Local.W = 0.0f;
    Local.H = 0.0f;
    QuestTree->GetLocalRect(&Local);
    if (Right >= 0 && Right <= (INT)Local.W && Bottom >= 0 && Bottom <= (INT)Local.H)
        Item->ShowWindow();
    else
        Item->HideWindow();
    for (INT i = 0; i < Item->Children.Num(); i++)
        ArrangeItem(Item->Children(i));
}
NW_HOOK_METHOD(0x100ff3f0, NCQuestGaraTreeCtrl, ArrangeItem)

// 0x100fd9f0 (vslot 75)
INT NCQuestGaraTreeCtrl::ScrollUp()
{
    if (ScrollY < 0)
    {
        DeltaX = ScrollX;
        DeltaY = ScrollY;
        ScrollY += LineStep;
        if (ScrollY > 0)
            ScrollY = 0;
        ScrollBar->SetScrollPos(-ScrollY);
        DeltaX -= ScrollX;
        DeltaY -= ScrollY;
        QuestTree->MoveWindow(0, LineStep);
        QuestTree->ScrollOffset -= LineStep;
    }
    return 1;
}
NW_HOOK_METHOD(0x100fd9f0, NCQuestGaraTreeCtrl, ScrollUp)

// 0x100eb470 (vslot 76)
INT NCQuestGaraTreeCtrl::ScrollDown()
{
    if (ScrollY > MinScrollY)
    {
        DeltaX = ScrollX;
        DeltaY = ScrollY;
        ScrollY -= LineStep;
        if (ScrollY < MinScrollY)
            ScrollY = MinScrollY;
        ScrollBar->SetScrollPos(-ScrollY);
        DeltaX -= ScrollX;
        DeltaY -= ScrollY;
        QuestTree->MoveWindow(0, -LineStep);
        QuestTree->ScrollOffset += LineStep;
    }
    return 1;
}
NW_HOOK_METHOD(0x100eb470, NCQuestGaraTreeCtrl, ScrollDown)

// 0x100eb540 (vslot 77) - the slider's value rounded up to a multiple of 15 (a constant here, not LineStep) and
// stored without the sign change NCScrollWnd makes: as the stock code does
INT NCQuestGaraTreeCtrl::OnScrollBar(INT Value)
{
    DeltaX = ScrollX;
    DeltaY = ScrollY;
    ScrollY = ((Value + 14) / 15) * 15;
    DeltaX -= ScrollX;
    DeltaY -= ScrollY;
    QuestTree->MoveWindow(0, -DeltaY);
    QuestTree->ScrollOffset += DeltaY;
    return 1;
}
NW_HOOK_METHOD(0x100eb540, NCQuestGaraTreeCtrl, OnScrollBar)

// 0x100f5330 (vslot 73) - three line steps per notch (120), up or down
INT NCQuestGaraTreeCtrl::OnMouseWheel(WPARAM wParam, LPARAM lParam)
{
    SHORT Notches = (SHORT)((SHORT)(wParam >> 16) / 120);
    DeltaX = ScrollX;
    DeltaY = ScrollY;
    if (Notches > 0)
    {
        if (ScrollY < 0)
        {
            ScrollY += LineStep * Notches * 3;
            if (ScrollY > 0)
                ScrollY = 0;
            ScrollBar->SetScrollPos(-ScrollY);
        }
    }
    else
    {
        if (ScrollY > MinScrollY)
        {
            ScrollY += LineStep * Notches * 3;
            if (ScrollY < MinScrollY)
                ScrollY = MinScrollY;
            ScrollBar->SetScrollPos(-ScrollY);
        }
    }
    DeltaX -= ScrollX;
    DeltaY -= ScrollY;
    QuestTree->MoveWindow(0, -DeltaY);
    QuestTree->ScrollOffset += DeltaY;
    return 1;
}
NW_HOOK_METHOD(0x100f5330, NCQuestGaraTreeCtrl, OnMouseWheel)

// 0x100eb3c0 (vslot 78) - the quest tree is emptied (the selected entry forgotten) and put back at the origin of the
// window, the scroll starts over and the root entry is gone
void NCQuestGaraTreeCtrl::ResetScroll()
{
    QuestTree->Clear();
    QuestTree->SelectedItem = NULL;
    NRectF Local;
    Local.X = 0;
    Local.Y = 0;
    Local.W = 0.0f;
    Local.H = 0.0f;
    QuestTree->GetLocalRect(&Local);
    QuestTree->MoveWindow(-Local.X, -Local.Y);
    QuestTree->ScrollOffset = 0;
    ScrollY = 0;
    ScrollX = 0;
    DeltaY = 0;
    DeltaX = 0;
    MinScrollY = 0;
    ScrollBar->ThumbPos = 0;
    RootItem = NULL;
}
NW_HOOK_METHOD(0x100eb3c0, NCQuestGaraTreeCtrl, ResetScroll)

// 0x100eb360 (name ours) - the quest the tree has marked: the first entry of the current category's list whose
// bClicked is set gives its quest id; -1 when there is none
INT NCQuestGaraTreeCtrl::FindMarkedQuest()
{
    NCGaraTVItem* Category = QuestTree->Root;
    if (Category)
    {
        INT Num = Category->Children.Num();
        for (INT i = 0; i < Num; i++)
        {
            if (Category->Children(i)->bClicked)
                return Category->Children(i)->Data.QuestID;
        }
    }
    return -1;
}
NW_HOOK_METHOD(0x100eb360, NCQuestGaraTreeCtrl, FindMarkedQuest)

NW_CHECK_VTABLE(NCQuestGaraTreeCtrl, 0x101c6120, 80)
