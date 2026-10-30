/*=============================================================================
    NCTabControl.cpp - tabs and their pages. Stock: 0x1002d6a0 .. 0x1002e900 (C4: the tabs are NCTabButtons, which can be
    dragged off the bar when Flags says so).
=============================================================================*/
#include "nwindow/NCTabControl.h"
#include "nwindow/NCButton.h"
#include "nwindow/NCTabButton.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NConsoleWnd.h"

// interface sound of a tab change
enum { SOUND_Tab = 5 };

/*-----------------------------------------------------------------------------
    The tab buttons. NCTabButton (0x102B5350: an NCRadioButton with the bar state at +0x2C0..+0x2D4, NCTabButton.h) is the class
    of every tab; the control keeps them as NCRadioButtons and reaches the five small members of NCTabButton through a cast.
-----------------------------------------------------------------------------*/

// the tab button: NCTabButton(normal and pushed texture names), 0x10003B00
static FORCEINLINE NCRadioButton* NewTabButton(const TCHAR* NormalTexName, const TCHAR* PushedTexName)
{
    return new NCTabButton(NormalTexName, PushedTexName);
}

// 0x10001EF0 - the tab is dragged off the bar (the dword at +0x2D4)
static FORCEINLINE INT TabIsSplit(NCRadioButton* Tab)
{
    return ((NCTabButton*)Tab)->GetSplit();
}

// 0x10001F00
static FORCEINLINE void TabSetSplit(NCRadioButton* Tab, INT bSplit)
{
    ((NCTabButton*)Tab)->SetSplit(bSplit);
}

// 0x10001F10 - the tab's place on the bar (the dword at +0x2D0)
static FORCEINLINE void TabSetIndex(NCRadioButton* Tab, INT Index)
{
    ((NCTabButton*)Tab)->SetTabIndex(Index);
}

// 0x10002000 - the bar position the tab goes back to (+0x2C0, +0x2C4) moves with the window
static FORCEINLINE void TabMoveOrg(NCRadioButton* Tab, INT DX, INT DY)
{
    ((NCTabButton*)Tab)->MoveOrg(DX, DY);
}

// 0x10001F20 (NCTabButton::MoveToOrgPosition, ret 8) - the tab goes back to its place on the bar; the page's position comes out
static FORCEINLINE void TabMoveToOrg(NCRadioButton* Tab, INT* X, INT* Y)
{
    ((NCTabButton*)Tab)->MoveToOrgPosition(X, Y);
}

// 0x1002e750 - NControl's constructor is inlined in the stock one; the tab group id is set by OnCreate. The arrays are
// zeroed by their own constructors
NCTabControl::NCTabControl()
{
    NextTabX = 0;
    CurrentPage = NULL;
    Flags = 0;
}
NW_HOOK_CTOR(0x1002e750, NCTabControl)

// 0x1002e860 (ret 4; anonymous in the db) - the same body with the Flags given (the stock stores the argument at +0x218 where the
// one above stores 0; the chat window's tab control is made with it)
NCTabControl::NCTabControl(INT InFlags)
{
    NextTabX = 0;
    CurrentPage = NULL;
    Flags = InFlags;
}
NW_HOOK_CTOR_ARGS(0x1002e860, NCTabControl, (INT A), (A))

// 0x1002e7f0 - Pages and Tabs are destroyed with the object (TArray<T*> destructor 0x10022980)
NW_HOOK_DTOR(0x1002e7f0, NCTabControl)

// 0x10148f10 (vslot 25) - the stock slot is NControl's body (the tabs report to the control with SelectTab, not with a message)
INT NCTabControl::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return NControl::WindowProc(Msg, wParam, lParam) != 0;
}

// 0x1002d6a0 (vslot 37)
INT NCTabControl::OnCreate()
{
    NControl::OnCreate();
    GroupID = GNextTabGroupID++;
    return 1;
}
NW_HOOK_METHOD(0x1002d6a0, NCTabControl, OnCreate)

// 0x1002e390 (vslot 38) - the tab buttons are ours, the pages are not
INT NCTabControl::OnDestroy()
{
    guard(NCTabControl::OnDestroy);
    for (INT i = 0; i < Tabs.Num(); i++)
    {
        if (Tabs(i))
        {
            RemoveChildWindow(Tabs(i)->Entry);
            delete Tabs(i);
            Tabs(i) = NULL;
        }
        delete Tabs(i);  // NULL by now: the stock code deletes twice
    }
    Tabs.Empty();
    Pages.Empty();
    NControl::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1002e390, NCTabControl, OnDestroy)

// 0x1002d7a0 (ret 8, guard) - the tab goes grey (or comes back); with Flags the draggable style goes with it
void NCTabControl::SetDisable(INT Index, INT bDisable)
{
    guard(NCTabControl::SetDisable);
    if (Tabs.Num() > 0)
    {
        NCRadioButton* Tab = Tabs(Index);
        if (bDisable)
        {
            Tab->EnableWindow(0);
            if (Flags & 1)
                Tab->SetStyle(Tab->NCWnd::GetStyle() & ~NWS_DRAGABLE);
        }
        else
        {
            Tab->EnableWindow(1);
            if (Flags & 1)
                Tab->SetStyle(Tab->NCWnd::GetStyle() | NWS_DRAGABLE);
        }
    }
    unguard;
}
NW_HOOK_METHOD(0x1002d7a0, NCTabControl, SetDisable)

// 0x1002d880 (ret 4, guard) - the tab is dragged off the bar
INT NCTabControl::IsSplit(INT Index)
{
    guard(NCTabControl::IsSplit);
    if (Index >= 0 && Index < Tabs.Num())
        return TabIsSplit(Tabs(Index)) != 0;
    return 0;
    unguard;
}
NW_HOOK_METHOD(0x1002d880, NCTabControl, IsSplit)

// 0x1002e4a0 (ret 4, guard; the stock name is IsSplit too) - the tab of this page is dragged off the bar (0 when it is no page of
// ours). The linker's analysis stops this function at 0x1002e4d0: the loop head, 0x1002e4d0 .. 0x1002e541, is its second half
INT NCTabControl::IsPageSplit(NCWnd* Page)
{
    guard(NCTabControl::IsSplit);
    for (INT i = 0; i < Pages.Num(); i++)
    {
        if (Page == Pages(i))
            return IsSplit(i);
    }
    return 0;
    unguard;
}
NW_HOOK_METHOD(0x1002e4a0, NCTabControl, IsPageSplit)

// 0x1002d930 (guard) - every tab that was dragged off goes back to its place on the bar, its page to where that puts it; the
// owner is told (NotifyParent with the number of tabs and the index)
void NCTabControl::DefaultPosition()
{
    guard(NCTabControl::DefaultPosition);
    for (INT i = 0; i < Tabs.Num(); i++)
    {
        if (IsSplit(i))
        {
            NCRadioButton* Tab = Tabs(i);
            if (Tab)
            {
                INT X = 0;
                INT Y = 0;
                TabSetSplit(Tab, 0);
                TabMoveToOrg(Tab, &X, &Y);
                Pages(i)->MoveWindow(X, Y);
                NotifyParent(Tabs.Num() + i);
            }
        }
    }
    unguard;
}
NW_HOOK_METHOD(0x1002d930, NCTabControl, DefaultPosition)

// 0x1002da20 (vslot 77, ret 0xC, guard OnTabMove) - a tab is dragged (X, Y: where its page goes): the first time it is taken off
// the bar, another tab of the bar (the first one left) is selected for the bar and then this one; its page follows the mouse.
// Nothing happens unless Flags says the tabs can be dragged. 0 for a tab that does not exist or is disabled
INT NCTabControl::TabMove(INT Index, INT X, INT Y)
{
    guard(NCTabControl::OnTabMove);
    if (Tabs.Num() < 0 || Index >= Tabs.Num() || !Tabs(Index)->IsEnabled())
        return 0;
    if (Flags & 1)
    {
        NCRadioButton* Tab = Tabs(Index);
        if (!TabIsSplit(Tab))
        {
            TabSetSplit(Tab, 1);
            for (INT i = 0; i < Pages.Num(); i++)
            {
                if (!IsSplit(i))
                {
                    SelectTab(i, 1);
                    break;
                }
            }
            SelectTab(Index, 1);
        }
        Pages(Index)->MoveWindow(X, Y);
    }
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1002da20, NCTabControl, TabMove)

// 0x1002db50 (vslot 78, ret 4, guard OnTabRelease) - the dragged tab is let go: when it touches the bar again (a corner of its
// rectangle, or of the bar position it came from, is inside the other) it goes back to the bar and its page with it
INT NCTabControl::TabRelease(INT Index)
{
    guard(NCTabControl::OnTabRelease);
    if (Tabs.Num() < 0 || Index >= Tabs.Num() || !Tabs(Index)->IsEnabled())
        return 0;
    NCRadioButton* Tab = Tabs(Index);
    NToolTipRect Rect;
    Rect.X = Tab->ScreenX;
    Rect.Y = Tab->ScreenY;
    Rect.W = Tab->Width;
    Rect.H = Tab->Height;
    if (IsSplit(Index))
    {
        if (Rect.ContainsCornerOf(::nw::Field<NToolTipRect>(Tab, 0x2C0)))
        {
            INT X = 0;
            INT Y = 0;
            TabSetSplit(Tab, 0);
            TabMoveToOrg(Tab, &X, &Y);
            SelectTab(Index, 1);
            Pages(Index)->MoveWindow(X, Y);
            NotifyParent(Tabs.Num() + Index);
        }
    }
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1002db50, NCTabControl, TabRelease)

// 0x1002dcc0 (vslot 39, ret 8, guard OnMove) - the window and its subtree move; the bar positions the tabs go back to move with it
void NCTabControl::MoveTabs(INT DX, INT DY)
{
    guard(NCTabControl::OnMove);
    NCWnd::MoveChildWindow(DX, DY);
    for (INT i = 0; i < Tabs.Num(); i++)
        TabMoveOrg(Tabs(i), DX, DY);
    unguard;
}
NW_HOOK_METHOD(0x1002dcc0, NCTabControl, MoveTabs)

// 0x1002dd70 - SelectTab without sound nor focus; every page is hidden first. The second argument asks for the owner to be told
// (NotifyParent)
void NCTabControl::SetTab(INT Index, BYTE bNotify)
{
    if (Tabs.Num() >= 0 && Index < Tabs.Num() && Tabs(Index)->IsEnabled())
    {
        for (INT i = 0; i < Pages.Num(); i++)
            Pages(i)->HideWindow();
        CurrentPage = Pages(Index);
        if (CurrentPage)
            CurrentPage->ShowWindow();
        for (INT i = 0; i < Tabs.Num(); i++)
            Tabs(i)->SetReleased(1);
        Tabs(Index)->SetReleased(0);
        if (bNotify)
            NotifyParent(Index);
    }
}
NW_HOOK_METHOD(0x1002dd70, NCTabControl, SetTab)

/*-----------------------------------------------------------------------------
    Adding tabs. The two stock overloads repeat the same code around the label.
-----------------------------------------------------------------------------*/

// 0x1002ded0 (ret 0x24) - the label is a SysString; a tab of W x H 0 is as large as the label plus (10, 5). The tab is created
// (and given its place on the bar, its index and the label) after the child registration; bDragable gives it NWS_DRAGABLE
INT NCTabControl::AddTab(INT TextID, NCWnd* Page, INT W, INT H, const TCHAR* NormalTexName, const TCHAR* PushedTexName, INT Gap, INT ToolTipID, INT bDragable)
{
    const TCHAR* Label = *SysString(TextID);
    if (!Label)
        return -1;
    if (Tabs.Num() == 0)
        CurrentPage = Page;
    if (W == 0 || H == 0)
    {
        SIZE Size;
        NCGDevice::GetTextExtent(&Size, *SysString(TextID), 0, 0, 0, 0, 0);
        W = Size.cx + 10;
        H = Size.cy + 5;
    }
    NCRadioButton* Tab;
    if (NormalTexName && PushedTexName)
        Tab = NewTabButton(NormalTexName, PushedTexName);
    else
        Tab = NewTabButton(TEXT("NWindow.BlackTexture"), TEXT("NWindow.Darkyellow"));
    Tab->ID = GroupID;
    CreateChild(Tab, bDragable ? (NWS_DRAGABLE | 0x32) : 0x22);
    ::nw::VirtualCall<void>(Tab, 75, NextTabX, 0, W, H);
    TabSetIndex(Tab, Tabs.Num());
    Tab->SetTextID(TextID);
    Tab->TextOffsetX = 2;
    Tab->TextOffsetY = 1;
    Tab->SetToolTip(ToolTipID, 1);
    if (Tabs.Num() == 0)
    {
        CurrentPage = Page;
        Tab->SetReleased(0);
    }
    else
    {
        Tab->SetReleased(1);
        Tab->HideWindow();
    }
    NextTabX += W + Gap;
    Tabs.AddItem(Tab);
    Pages.AddItem(Page);
    return Tabs.Num() - 1;
}
NW_HOOK_METHOD_SIG(0x1002ded0, NCTabControl, AddTab, INT (NCTabControl::*)(INT, NCWnd*, INT, INT, const TCHAR*, const TCHAR*, INT, INT, INT))

// 0x1002e120 (ret 0x20) - the label is the text itself; the tab gets its place on the bar before the child registration
INT NCTabControl::AddTab(const TCHAR* Text, NCWnd* Page, INT W, INT H, const TCHAR* NormalTexName, const TCHAR* PushedTexName, INT Gap, INT ToolTipID)
{
    if (!Text)
        return -1;
    if (Tabs.Num() == 0)
        CurrentPage = Page;
    if (W == 0 || H == 0)
    {
        SIZE Size;
        NCGDevice::GetTextExtent(&Size, Text, 0, 0, 0, 0, 0);
        W = Size.cx + 10;
        H = Size.cy + 5;
    }
    NCRadioButton* Tab;
    if (NormalTexName && PushedTexName)
        Tab = NewTabButton(NormalTexName, PushedTexName);
    else
        Tab = NewTabButton(TEXT("NWindow.BlackTexture"), TEXT("NWindow.Darkyellow"));
    Tab->ID = GroupID;
    ::nw::VirtualCall<void>(Tab, 75, NextTabX, 0, W, H);
    CreateChild(Tab, 0x22);
    TabSetIndex(Tab, Tabs.Num());
    Tab->SetText(Text);
    Tab->TextOffsetX = 2;
    Tab->TextOffsetY = 1;
    Tab->SetToolTip(ToolTipID, 1);
    if (Tabs.Num() == 0)
    {
        CurrentPage = Page;
        Tab->SetReleased(0);
    }
    else
    {
        Tab->SetReleased(1);
        Tab->HideWindow();
    }
    NextTabX += W + Gap;
    Tabs.AddItem(Tab);
    Pages.AddItem(Page);
    return Tabs.Num() - 1;
}
NW_HOOK_METHOD_SIG(0x1002e120, NCTabControl, AddTab, INT (NCTabControl::*)(const TCHAR*, NCWnd*, INT, INT, const TCHAR*, const TCHAR*, INT, INT))

// 0x1002e330 (vslot 75) - the pages of the tabs dragged off the bar are shown and the others hidden, then the current page is
// shown
void NCTabControl::ShowCurrentPage()
{
    for (INT i = 0; i < Pages.Num(); i++)
    {
        if (Pages(i))
        {
            if (IsSplit(i))
                Pages(i)->ShowWindow();
            else
                Pages(i)->HideWindow();
        }
    }
    if (CurrentPage)
        CurrentPage->ShowWindow();
}
NW_HOOK_METHOD(0x1002e330, NCTabControl, ShowCurrentPage)

// 0x1002e550 (vslot 76, ret 8) - the tab is chosen: the sound when the page changes; for a page that is on the bar the other
// pages (those on the bar) are hidden and their tabs released and this one is the current page; the page is shown, its tab
// pressed and both are focused (with Flags the page comes to the top of its parent); the owner is told when asked, and the sound
// plays again
INT NCTabControl::SelectTab(INT Index, INT bNotify)
{
    if (Tabs.Num() >= 0 && Index < Tabs.Num() && Tabs(Index)->IsEnabled())
    {
        if (CurrentPage != Pages(Index))
            GConsole->PlayInterfaceSound(SOUND_Tab);
        if (!IsPageSplit(Pages(Index)))
        {
            for (INT i = 0; i < Pages.Num(); i++)
            {
                if (!IsSplit(i))
                {
                    if (Pages(i))
                        Pages(i)->HideWindow();
                    Tabs(i)->SetReleased(1);
                }
            }
            CurrentPage = Pages(Index);
        }
        NCWnd* Page = Pages(Index);
        if (Page)
        {
            Page->ShowWindow();
            Tabs(Index)->SetReleased(0);
            Page->SetFocus();
            if (Flags & 1)
            {
                Tabs(Index)->SetFocus();
                NWndEntry* PageEntry = Page->Entry;
                if (PageEntry->Parent)
                    PageEntry->Parent->Wnd->SetHeadWindow(PageEntry);
            }
        }
        if (bNotify)
            NotifyParent(Index);
        GConsole->PlayInterfaceSound(SOUND_Tab);
    }
    return 1;
}
NW_HOOK_METHOD(0x1002e550, NCTabControl, SelectTab)

// 0x1002e720 (ret 4, the hidden return pointer) - a copy of the tab buttons' array (the copy constructor 0x1002de40)
TArray<NCRadioButton*> NCTabControl::GetTabs()
{
    return Tabs;
}
NW_HOOK_METHOD(0x1002e720, NCTabControl, GetTabs)

NW_CHECK_VTABLE(NCTabControl, 0x101a6980, 79)
