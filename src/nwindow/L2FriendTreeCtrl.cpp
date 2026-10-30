/*=============================================================================
    L2FriendTreeCtrl.cpp - the scrolling tree of the Lineage II friend list (C3 only; C3 addresses). Stock: 0x10141B10 (the
    double click), 0x10141B60 (WindowProc), 0x10142210 .. 0x101425D0 (the virtuals), 0x10143A00 .. 0x10143AA0 (constructor,
    destructor, OnCreate).
=============================================================================*/
#include "nwindow/L2FriendTreeCtrl.h"
#include "nwindow/L2FriendTree.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NCSliderBar.h"

enum { NID_L2FRIENDTREE_TREE = 0xA01 };   // the inner tree's id (name ours)

// 0x101683c0 - NCTreeCtrl's constructor and one word cleared
L2FriendTreeCtrl::L2FriendTreeCtrl()
{
    Unknown114 = 0;
}
NW_HOOK_CTOR(0x101683c0, L2FriendTreeCtrl)

// 0x10168410
NW_HOOK_DTOR(0x10168410, L2FriendTreeCtrl)

// 0x10166b60 (vslot 25) - the control's own procedure, then a double click (0x203) is passed up; shared by MsnTreeCtrl
INT L2FriendTreeCtrl::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCScrollWnd::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == 0x203)
        return OnDoubleClick(wParam, lParam) != 0;
    return 0;
}
NW_HOOK_METHOD(0x10166b60, L2FriendTreeCtrl, WindowProc)

// 0x10166530 (name ours; ret 8) - the message goes on to the window the parent belongs to (nobody: 0)
INT L2FriendTreeCtrl::OnDoubleClick(WPARAM wParam, LPARAM lParam)
{
    NWndEntry* ParentEntry = Entry->Parent;
    if (ParentEntry)
    {
        NCWnd* ParentWnd = ParentEntry->Wnd;
        if (ParentWnd)
        {
            ParentWnd->WindowProc(0x203, wParam, lParam);
            return 1;
        }
    }
    return 0;
}
NW_HOOK_METHOD(0x10166530, L2FriendTreeCtrl, OnDoubleClick)

// C2 CANDIDATE: the C2 function is not the same code as the C1 one; kept only while it matches the stock one (verify_data)
// 0x10168460 (vslot 37) - NCScrollWnd's (not NCTreeCtrl's), then the friend tree, as wide as the window less the scroll bar
// (16), as high as the window
INT L2FriendTreeCtrl::OnCreate()
{
    NCScrollWnd::OnCreate();
    FriendTree = new L2FriendTree();
    FriendTree->ID = NID_L2FRIENDTREE_TREE;
    FriendTree->SetPosition(0, 0, (INT)(Width - 16.0f), (INT)Height);
    CreateChild(FriendTree, 0x22);
    return 1;
}
NW_HOOK_METHOD(0x10168460, L2FriendTreeCtrl, OnCreate)

// 0x10166bc0 (vslot 38)
INT L2FriendTreeCtrl::OnDestroy()
{
    if (FriendTree)
    {
        RemoveChildWindow(FriendTree->Entry);
        delete FriendTree;
        FriendTree = NULL;
    }
    NCScrollWnd::OnDestroy();
    return 1;
}
NW_HOOK_METHOD(0x10166bc0, L2FriendTreeCtrl, OnDestroy)

// 0x10166c00 (vslot 62) - the content height follows the tree (and 21 more)
INT L2FriendTreeCtrl::OnPaint(NCGDevice* Device)
{
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    SetContentHeightEx(FriendTree->ContentHeight + 21);
    Device->ReleaseClip();
    Device->EndPaint();
    return 1;
}
NW_HOOK_METHOD(0x10166c00, L2FriendTreeCtrl, OnPaint)

// 0x10166d70 (vslot 75) - one line up: the tree window moves down by a line
INT L2FriendTreeCtrl::ScrollUp()
{
    if (ScrollY < 0)
    {
        DeltaX = ScrollX;
        DeltaY = ScrollY;
        ScrollY += LineStep;
        if (0 < ScrollY)
            ScrollY = 0;
        ScrollBar->SetScrollPos(-ScrollY);
        DeltaX = DeltaX - ScrollX;
        DeltaY = DeltaY - ScrollY;
        FriendTree->MoveWindow(0, LineStep);
        FriendTree->ScrollOffset -= LineStep;
    }
    return 1;
}
NW_HOOK_METHOD(0x10166d70, L2FriendTreeCtrl, ScrollUp)

// 0x10166e30 (vslot 76) - one line down
INT L2FriendTreeCtrl::ScrollDown()
{
    if (MinScrollY < ScrollY)
    {
        DeltaX = ScrollX;
        DeltaY = ScrollY;
        ScrollY -= LineStep;
        if (ScrollY < MinScrollY)
            ScrollY = MinScrollY;
        ScrollBar->SetScrollPos(-ScrollY);
        DeltaX = DeltaX - ScrollX;
        DeltaY = DeltaY - ScrollY;
        FriendTree->MoveWindow(0, -LineStep);
        FriendTree->ScrollOffset += LineStep;
    }
    return 1;
}
NW_HOOK_METHOD(0x10166e30, L2FriendTreeCtrl, ScrollDown)

// 0x10166f00 (vslot 71) - the bar was moved to Value
INT L2FriendTreeCtrl::OnScrollBar(INT Value)
{
    DeltaX = ScrollX;
    DeltaY = ScrollY;
    ScrollY = -Value;
    DeltaY = DeltaY - ScrollY;
    DeltaX = DeltaX - ScrollX;
    FriendTree->MoveWindow(0, -DeltaY);
    FriendTree->ScrollOffset += DeltaY;
    return 1;
}
NW_HOOK_METHOD(0x10166f00, L2FriendTreeCtrl, OnScrollBar)

// 0x10166f80 (vslot 73) - three lines per notch of the wheel
INT L2FriendTreeCtrl::OnMouseWheel(WPARAM wParam, LPARAM lParam)
{
    SHORT Notches = (SHORT)((SHORT)HIWORD(wParam) / 120);
    DeltaX = ScrollX;
    DeltaY = ScrollY;
    if (Notches < 1)
    {
        if (MinScrollY < ScrollY)
        {
            ScrollY = ScrollY + LineStep * Notches * 3;
            if (ScrollY < MinScrollY)
                ScrollY = MinScrollY;
            ScrollBar->SetScrollPos(-ScrollY);
        }
    }
    else if (ScrollY < 0)
    {
        ScrollY = ScrollY + LineStep * Notches * 3;
        if (0 < ScrollY)
            ScrollY = 0;
        ScrollBar->SetScrollPos(-ScrollY);
    }
    DeltaY = DeltaY - ScrollY;
    DeltaX = DeltaX - ScrollX;
    FriendTree->MoveWindow(0, -DeltaY);
    FriendTree->ScrollOffset += DeltaY;
    return 1;
}
NW_HOOK_METHOD(0x10166f80, L2FriendTreeCtrl, OnMouseWheel)

// 0x10166ce0 (vslot 72) - the tree emptied and brought back to its place, the scroll at the top
void L2FriendTreeCtrl::Clear()
{
    FriendTree->Clear();
    NRectF R;
    R.X = 0;
    R.Y = 0;
    R.W = 0.0f;
    R.H = 0.0f;
    FriendTree->GetLocalRect(&R);
    FriendTree->MoveWindow(-R.X, -R.Y);
    FriendTree->ScrollOffset = 0;
    ScrollY = 0;
    ScrollX = 0;
    DeltaY = 0;
    DeltaX = 0;
    MinScrollY = 0;
    ScrollBar->ThumbPos = 0;                                // the slider's thumb back at the top (NCSliderBar +0x1F8 in C4)
}
NW_HOOK_METHOD(0x10166ce0, L2FriendTreeCtrl, Clear)

NW_CHECK_VTABLE(L2FriendTreeCtrl, 0x101dde38, 79)
