/*=============================================================================
    NCSSQMainBoard.cpp - the Seven Signs window. C3 stock: 0x100E9070 .. 0x100E9195 (WindowProc, ShowWindow,
    HideWindow, OnTab), 0x100E9630 (OnDestroy), 0x100E9760 (paint), 0x100E9E10 / 0x100E9E60 (constructor,
    destructor), 0x100E9EB0 (OnCreate).
=============================================================================*/
#include "nwindow/NCSSQMainBoard.h"
#include "nwindow/NCFrameCtrl.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NCSSQMainEventWnd.h"
#include "nwindow/NCSSQSealStatusListWnd.h"
#include "nwindow/NCSSQStatusWnd.h"
#include "nwindow/NCTabControl.h"
#include "nwindow/NSSQHelpers.h"

// NWM_COMMAND ids (names ours)
enum
{
    NID_SSQ_FRAME = 0xFFFA0F9,
    NID_SSQ_TABS = 0xFFFA0FF,
};

// sysstring lines of the tab titles
enum { STR_TabStatus = 0x338, STR_TabEvent = 0x339, STR_TabSeal = 0x33A };

enum { SOUND_WindowOpen = 5 };   // interface sound

// 0x100fc6c0 - the two "asked" flags clear (NCWnd's constructor and the vtable around them)
NCSSQMainBoard::NCSSQMainBoard()
{
    bEventAsked = 0;
    bSealAsked = 0;
}
NW_HOOK_CTOR(0x100fc6c0, NCSSQMainBoard)

// 0x100fc710
NW_HOOK_DTOR(0x100fc710, NCSSQMainBoard)

// 0x100fb800 (vslot 25) - the tab control reports the tab picked
INT NCSSQMainBoard::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCWnd::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND && wParam == NID_SSQ_TABS)
        return OnTab((INT)lParam) != 0;
    return 0;
}
NW_HOOK_METHOD(0x100fb800, NCSSQMainBoard, WindowProc)

// 0x100fb860 (vslot 48) - the first tab again, with the window's sound
void NCSSQMainBoard::ShowWindow()
{
    NCWnd::ShowWindow();
    TabCtrl->SetTab(0, 1);
    GConsole->PlayInterfaceSound(SOUND_WindowOpen);
}
NW_HOOK_METHOD(0x100fb860, NCSSQMainBoard, ShowWindow)

// 0x100fb890 (vslot 49) - the lists are asked for again next time; what the tabs show is cleared
void NCSSQMainBoard::HideWindow()
{
    bEventAsked = 0;
    bSealAsked = 0;
    StatusWnd->ResetStatus();
    EventWnd->ClearList();
    SealWnd->ClearList();
    NCWnd::HideWindow();
}
NW_HOOK_METHOD(0x100fb890, NCSSQMainBoard, HideWindow)

// 0x100fb8d0 (vslot 68, name ours) - the event tab (1) and the seal tab (2) ask for their lists once
INT NCSSQMainBoard::OnTab(INT Index)
{
    if (Index == 1)
    {
        if (!bEventAsked)
        {
            ((NConsoleSSQ*)GConsole)->RequestSSQStatus(2);
            bEventAsked = 1;
        }
        return 1;
    }
    if (Index == 2)
    {
        if (!bSealAsked)
        {
            ((NConsoleSSQ*)GConsole)->RequestSSQStatus(3);
            bSealAsked = 1;
        }
    }
    return 1;
}
NW_HOOK_METHOD(0x100fb8d0, NCSSQMainBoard, OnTab)

// a child window goes (repeated inline in the stock OnDestroy)
template <class T> static FORCEINLINE void DestroyChild(NCWnd* Parent, T*& Child)
{
    if (Child)
    {
        Parent->RemoveChildWindow(Child->Entry);
        if (Child)
            delete Child;
        Child = NULL;
    }
}

// 0x100fbec0 (vslot 38; the db names it NCMainWnd::OnDestroy: that is the guard string the stock code carries) - the
// three tab pages, then the tab control, then NCWnd's
INT NCSSQMainBoard::OnDestroy()
{
    guard(NCMainWnd::OnDestroy);
    DestroyChild(this, StatusWnd);
    DestroyChild(this, EventWnd);
    DestroyChild(this, SealWnd);
    DestroyChild(this, TabCtrl);
    NCWnd::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100fbec0, NCSSQMainBoard, OnDestroy)

// 0x100fbff0 (vslot 62) - the panel under the title bar (256 x 28)
INT NCSSQMainBoard::OnPaint(NCGDevice* Device)
{
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    Device->BeginPaint(ScreenX, ScreenY);
    if (BackTex)
        Device->DrawTexture(0, 20, 256, 28, 0, 0, 256, 28, BackTex, 0xFF, 1);
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
}
NW_HOOK_METHOD(0x100fbff0, NCSSQMainBoard, OnPaint)

// 0x100fc760 (vslot 37) - a title bar, the three tab pages (all at (0, 48), 256 x 353) and the tab control with its
// three tabs (74 x 23)
INT NCSSQMainBoard::OnCreate()
{
    NCWnd::OnCreate();
    BackTex = FindTexture(TEXT("L2UI_CH3.SSQWnd.SSQ_back"));

    FrameCtrl = new NCFrameCtrl(1, 0, 0);
    FrameCtrl->ID = NID_SSQ_FRAME;
    FrameCtrl->SetPosition(0, 0, 256, 20);
    CreateChild(FrameCtrl, NWS_VISIBLE);

    StatusWnd = new NCSSQStatusWnd();
    StatusWnd->SetPosition(0, 48, 256, 353);
    CreateChild(StatusWnd, NWS_VISIBLE);

    EventWnd = new NCSSQMainEventWnd();
    EventWnd->SetPosition(0, 48, 256, 353);
    CreateChild(EventWnd, NWS_VISIBLE);

    SealWnd = new NCSSQSealStatusListWnd();
    SealWnd->SetPosition(0, 48, 256, 353);
    CreateChild(SealWnd, NWS_VISIBLE);

    TabCtrl = new NCTabControl();
    TabCtrl->ID = NID_SSQ_TABS;
    TabCtrl->SetPosition(12, 25, 224, 41);
    CreateChild(TabCtrl, NWS_VISIBLE);
    TabCtrl->AddTab(STR_TabStatus, StatusWnd, 74, 23, TEXT("L2UI_CH3.TabButton.normal_tab_on"),
                    TEXT("L2UI_CH3.TabButton.normal_tab"), 1, -1);
    TabCtrl->AddTab(STR_TabEvent, EventWnd, 74, 23, TEXT("L2UI_CH3.TabButton.normal_tab_on"),
                    TEXT("L2UI_CH3.TabButton.normal_tab"), 1, -1);
    TabCtrl->AddTab(STR_TabSeal, SealWnd, 74, 23, TEXT("L2UI_CH3.TabButton.normal_tab_on"),
                    TEXT("L2UI_CH3.TabButton.normal_tab"), 1, -1);
    return 1;
}
NW_HOOK_METHOD(0x100fc760, NCSSQMainBoard, OnCreate)

NW_CHECK_VTABLE(NCSSQMainBoard, 0x101c9a58, 75)
