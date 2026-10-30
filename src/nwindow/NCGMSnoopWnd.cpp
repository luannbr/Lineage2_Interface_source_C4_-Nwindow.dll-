/*=============================================================================
    NCGMSnoopWnd.cpp - the GM's snoop windows, and the manager that holds four of them (C3 addresses). Stock:
    0x100d05d0 .. 0x100d0680 (WindowProc, Reset, OnClose, the manager's ShowWindow), 0x100d0ed0 .. 0x100d13e0,
    0x100d2b60 .. 0x100d2c00 (constructors, destructor), OnCreate at 0x100d3420, the manager's constructor,
    destructor and OnCreate at 0x10056720 .. 0x10056770, 0x100d3b00.
=============================================================================*/
#include "nwindow/NCGMSnoopWnd.h"
#include "nwindow/NCGMSnoopManager.h"
#include "nwindow/NCButton.h"
#include "nwindow/NCCheckBox.h"
#include "nwindow/NCFrameCtrl.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NCListBox.h"
#include "nwindow/NCTextListBoxC4.h"
#include "nwindow/NConsoleWnd.h"
#include "nwindow/NDialogInfo.h"

// ids (names ours)
enum
{
    NID_GMSNOOP_LIST = 0xFCA0B1,
    NID_GMSNOOP_CLOSE = 0xFCA0B4,
    NID_GMSNOOP_FRAME = 0xFCA0B5,
    NID_GMSNOOP_FILTER = (INT)0xF0F01450,   // + the filter's number
};

// the seven check boxes: system message of the label and place (names ours)
struct NSnoopFilterDesc
{
    INT LabelID;
    INT X;
    INT Y;
};
static const NSnoopFilterDesc GSnoopFilters[7] = {
    { 0x16A, 15, 240 }, { 0x168, 84, 240 }, { 0x169, 153, 240 }, { 0x167, 222, 240 },
    { 0x16B, 15, 254 }, { 0x17F, 84, 254 }, { 0x250, 153, 254 },
};

// sysstring of the title of a snoop window: "<this> - <the player's name>" (name ours)
enum { SYSSTR_SnoopTitle = 693 };
// system message when all four windows are in use (name ours) and the box's id (the plain notice)
enum { MSG_SnoopWindowsFull = 0x322, DIALOG_Notice = 0x1F };

// a child window goes (repeated inline in the stock OnDestroys)
template <class T> static FORCEINLINE void DestroyChild(NCWnd* Parent, T*& Child)
{
    if (Child)
    {
        Parent->RemoveChildWindow(Child->Entry);
        delete Child;
        Child = NULL;
    }
}

/*-----------------------------------------------------------------------------
    NCGMSnoopWnd
-----------------------------------------------------------------------------*/

// 0x100e2f90 - NCWnd's constructor and the vtable
NCGMSnoopWnd::NCGMSnoopWnd()
{
}
NW_HOOK_CTOR(0x100e2f90, NCGMSnoopWnd)

// 0x100e2fe0
NW_HOOK_DTOR(0x100e2fe0, NCGMSnoopWnd)

// 0x100dfff0 (vslot 25)
INT NCGMSnoopWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCWnd::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND && wParam == NID_GMSNOOP_CLOSE)
        return OnClose() != 0;
    return 0;
}
NW_HOOK_METHOD(0x100dfff0, NCGMSnoopWnd, WindowProc)

// 0x100e0050 - the conversation emptied (NCListBox::Clear, vslot 77)
void NCGMSnoopWnd::Reset()
{
    List->Clear();
    SnoopID = -1;
}
NW_HOOK_METHOD(0x100e0050, NCGMSnoopWnd, Reset)

// 0x100e0070 (vslot 74) - the window hides, the server is told the snoop is over, the window is reset
INT NCGMSnoopWnd::OnClose()
{
    HideWindow();
    GConsole->RequestGMSnoopEnd(SnoopID);
    Reset();
    return 1;
}
NW_HOOK_METHOD(0x100e0070, NCGMSnoopWnd, OnClose)

// 0x100e0790 (ret 0x10) - a line of the chat, when its kind is shown: the type picks the check box (0 the sixth, 1 the fourth,
// 2 the fifth, 3 the third, 4 the second, 8 the first and 9 the seventh; 5 .. 7 and the rest nowhere), the line goes in the
// list in the color it came with
void NCGMSnoopWnd::AddLine(const TCHAR* Text, DWORD Color, INT Type, INT Unused)
{
    INT Filter;
    switch (Type)
    {
    case 0:
        Filter = 5;
        break;
    case 1:
        Filter = 3;
        break;
    case 2:
        Filter = 4;
        break;
    case 3:
        Filter = 2;
        break;
    case 4:
        Filter = 1;
        break;
    case 8:
        Filter = 0;
        break;
    case 9:
        Filter = 6;
        break;
    default:
        return;
    }
    if (Filters[Filter]->bChecked)
        List->AddString(Text, Color, -1);
}
NW_HOOK_METHOD(0x100e0790, NCGMSnoopWnd, AddLine)

// 0x100e1b50 (vslot 37) - the petition window's look: the "Petitionback" panel, the title bar (21 high, 300 wide), the
// conversation (an NCTextListBox: 200 lines kept, 16 shown, a bare list that follows the last line), the close button
// (text 141) and the seven kinds' check boxes under it, all checked in C4
INT NCGMSnoopWnd::OnCreate()
{
    NCWnd::OnCreate();
    BackTex = FindTexture(TEXT("L2UI.etcwnd.Petitionback"), 1);
    FrameCtrl = new NCFrameCtrl(1, 0, 0);
    FrameCtrl->ID = NID_GMSNOOP_FRAME;
    FrameCtrl->SetPosition(0, 0, 300, 21);
    CreateChild(FrameCtrl, NWS_VISIBLE);
    List = NewTextListBox(200, 16, 12, 1);
    List->ID = NID_GMSNOOP_LIST;
    List->SetPosition(15, 38, 282, 195);
    CreateChild(List, NWS_NOHITTEST);
    List->bAutoScroll = 1;
    List->LineGap = 0;
    CloseButton = new NCButton(TEXT("L2UI.control.DefaultButton"), TEXT("L2UI.control.DefaultButton_click"), 2);
    CloseButton->ID = NID_GMSNOOP_CLOSE;
    CloseButton->SetPosition(110, 270, 64, 19);
    CreateChild(CloseButton, NWS_VISIBLE);
    CloseButton->SetTextID(141);
    for (INT i = 0; i < 7; i++)
    {
        Filters[i] = new NCCheckBox(GSnoopFilters[i].LabelID, 0, 0, 0);   // four arguments: the stock pops 0x10 (three left the frame short and the next store went to a NULL)
        Filters[i]->ID = NID_GMSNOOP_FILTER + i;
        Filters[i]->SetPosition(GSnoopFilters[i].X, GSnoopFilters[i].Y, 64, 12);
        CreateChild(Filters[i], NWS_VISIBLE);
    }
    for (INT i = 0; i < 7; i++)
        Filters[i]->bChecked = 1;
    SnoopID = -1;
    return 1;
}
NW_HOOK_METHOD(0x100e1b50, NCGMSnoopWnd, OnCreate)

// 0x100e05e0 (vslot 38) - guarded
INT NCGMSnoopWnd::OnDestroy()
{
    guard(NCGMSnoopWnd::OnDestroy);
    DestroyChild(this, List);
    DestroyChild(this, CloseButton);
    for (INT i = 0; i < 7; i++)
        DestroyChild(this, Filters[i]);
    NCWnd::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100e05e0, NCGMSnoopWnd, OnDestroy)

// 0x100e06f0 (vslot 62) - the panel under the 21-pixel title bar (300 x 280); the texture is not checked
INT NCGMSnoopWnd::OnPaint(NCGDevice* Device)
{
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    if (BackTex)
        Device->DrawTexture(0, 21, 300, 280, 0, 0, 300, 280, BackTex, 0xFF, 1);
    Device->ReleaseClip();
    Device->EndPaint();
    return 1;
}
NW_HOOK_METHOD(0x100e06f0, NCGMSnoopWnd, OnPaint)

/*-----------------------------------------------------------------------------
    NCGMSnoopManager
-----------------------------------------------------------------------------*/

// 0x1005f2d0 - NCWnd's constructor and the vtable
NCGMSnoopManager::NCGMSnoopManager()
{
}
NW_HOOK_CTOR(0x1005f2d0, NCGMSnoopManager)

// 0x1005f320
NW_HOOK_DTOR(0x1005f320, NCGMSnoopManager)

// 0x100e00a0 (vslot 48) - shown, but its four windows stay hidden
void NCGMSnoopManager::ShowWindow()
{
    NCWnd::ShowWindow();
    for (INT i = 0; i < 4; i++)
        Snoops[i]->HideWindow();
}
NW_HOOK_METHOD(0x100e00a0, NCGMSnoopManager, ShowWindow)

// 0x100e3030 (vslot 37) - four snoop windows (300 x 301 at (15, 38), style 0x4008), each hidden
INT NCGMSnoopManager::OnCreate()
{
    NCWnd::OnCreate();
    for (INT i = 0; i < 4; i++)
    {
        Snoops[i] = new NCGMSnoopWnd;
        Snoops[i]->SetPosition(15, 38, 300, 301);
        CreateChild(Snoops[i], 0x4008);
        Snoops[i]->HideWindow();
    }
    return 1;
}
NW_HOOK_METHOD(0x100e3030, NCGMSnoopManager, OnCreate)

// 0x100e08a0 (vslot 38) - guarded
INT NCGMSnoopManager::OnDestroy()
{
    guard(NCGMSnoopManager::OnDestroy);
    for (INT i = 0; i < 4; i++)
        DestroyChild(this, Snoops[i]);
    NCWnd::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100e08a0, NCGMSnoopManager, OnDestroy)

// 0x100e0960 (name ours; the console's Snoop calls it with five values) - a line for the windows that listen to
// SnoopID: each shows it and shows itself
void NCGMSnoopManager::AddLine(INT SnoopID, const TCHAR* Text, DWORD Color, INT Type, INT Unused)
{
    for (INT i = 0; i < 4; i++)
    {
        if (Snoops[i]->SnoopID == SnoopID)
        {
            Snoops[i]->AddLine(Text, Color, Type, Unused);
            Snoops[i]->ShowWindow();
        }
    }
}
NW_HOOK_METHOD(0x100e0960, NCGMSnoopManager, AddLine)

// 0x100e09c0 - a window already listening to SnoopID: nothing; else the first free one (SnoopID -1) takes it and its title
// is "<sysstring 693> - <TargetName>"; none free: a notice (system message 0x322)
void NCGMSnoopManager::OpenSnoop(INT SnoopID, const TCHAR* TargetName)
{
    INT i;
    for (i = 0; i < 4; i++)
        if (Snoops[i]->SnoopID == SnoopID)
            return;
    for (i = 0; i < 4; i++)
    {
        if (Snoops[i]->SnoopID == -1)
        {
            Snoops[i]->SnoopID = SnoopID;
            TCHAR Title[128];
            appSprintf(Title, TEXT("%s - %s"), *GL2GameData.SysString[SYSSTR_SnoopTitle], TargetName);
            Snoops[i]->SetTitle(Title);
            return;
        }
    }
    NDialogInfo Info;
    Info.ID = DIALOG_Notice;
    Info.bMainBox = 1;
    Info.bInput = 0;
    Info.SetSystemMessage(MSG_SnoopWindowsFull);
    GConsole->ShowDialog(&Info);
}
NW_HOOK_METHOD(0x100e09c0, NCGMSnoopManager, OpenSnoop)

NW_CHECK_VTABLE(NCGMSnoopWnd, 0x101c4f18, 75)
NW_CHECK_VTABLE(NCGMSnoopManager, 0x101ada70, 74)
