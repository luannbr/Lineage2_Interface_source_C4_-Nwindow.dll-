/*=============================================================================
    NCSSQMainEventWnd.cpp - the Seven Signs event list tab. C3 stock: 0x100EA2E0 .. 0x100EA362 (WindowProc, the
    list reset, the refresh command), 0x100EB490 (OnDestroy), 0x100EB4D0 (paint), 0x100EB7B0 / 0x100EB800
    (constructor, destructor), 0x100EB850 (OnCreate).
=============================================================================*/
#include "nwindow/NCSSQMainEventWnd.h"
#include "nwindow/NCButton.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NCSSQMainEventGaraTreeCtrl.h"
#include "nwindow/NSSQHelpers.h"

enum { NID_SSQEVENT_REFRESH = 0xFF00F0A1 };   // name ours
enum { STR_Refresh = 0x3AA };

// 0x100fe160 - the control is made by OnCreate
NCSSQMainEventWnd::NCSSQMainEventWnd()
{
    TreeCtrl = NULL;
}
NW_HOOK_CTOR(0x100fe160, NCSSQMainEventWnd)

// 0x100fe1b0
NW_HOOK_DTOR(0x100fe1b0, NCSSQMainEventWnd)

// 0x100fcbd0 (vslot 25) - the refresh button
INT NCSSQMainEventWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCWnd::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND && wParam == NID_SSQEVENT_REFRESH)
        return OnRefresh() != 0;
    return 0;
}
NW_HOOK_METHOD(0x100fcbd0, NCSSQMainEventWnd, WindowProc)

// 0x100fcc40 (vslot 68, name ours) - the console asks the server for the event list again
INT NCSSQMainEventWnd::OnRefresh()
{
    ((NConsoleSSQ*)GConsole)->RequestSSQStatus(2);
    return 1;
}
NW_HOOK_METHOD(0x100fcc40, NCSSQMainEventWnd, OnRefresh)

// 0x100EA340 (name ours) - the tree emptied (the control's reset)
void NCSSQMainEventWnd::ClearList()
{
    TreeCtrl->ResetScroll();
}
// 0x100EA340 is hooked once, by NForwardFns::ChildSlot72 (NTemplates.cpp: the same `mov ecx,[ecx+0xCC]; jmp [vtbl+0x120]`)

// 0x100fde00 (vslot 38) - no guard here; C4: the refresh button goes too
INT NCSSQMainEventWnd::OnDestroy()
{
    if (TreeCtrl)
    {
        RemoveChildWindow(TreeCtrl->Entry);
        delete TreeCtrl;
        TreeCtrl = NULL;
    }
    if (RefreshButton)
    {
        RemoveChildWindow(RefreshButton->Entry);
        delete RefreshButton;
        RefreshButton = NULL;
    }
    NCWnd::OnDestroy();
    return 1;
}
NW_HOOK_METHOD(0x100fde00, NCSSQMainEventWnd, OnDestroy)

// 0x100fde70 (vslot 62) - the panel over the whole window
INT NCSSQMainEventWnd::OnPaint(NCGDevice* Device)
{
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    if (BackTex)
        Device->DrawTexture(0, 0, (INT)Width, (INT)Height, 0, 0, (INT)Width, (INT)Height, BackTex, 0xFF, 1);
    Device->ReleaseClip();
    Device->EndPaint();
    return 1;
}
NW_HOOK_METHOD(0x100fde70, NCSSQMainEventWnd, OnPaint)

// 0x100fe200 (vslot 37) - the panel, the event control and the refresh button under it
INT NCSSQMainEventWnd::OnCreate()
{
    NCWnd::OnCreate();
    BackTex = FindTexture(TEXT("L2UI_CH3.SSQWnd.ssq_back2"));

    TreeCtrl = new NCSSQMainEventGaraTreeCtrl();
    TreeCtrl->SetPosition(8, 11, 240, 304);
    CreateChild(TreeCtrl, NWS_VISIBLE);

    RefreshButton = new NCButton(TEXT("L2ui_ch3.button.Btn1_normal"), TEXT("L2ui_ch3.button.Btn1_normalOn"), 0);
    RefreshButton->ID = NID_SSQEVENT_REFRESH;
    RefreshButton->SetPosition(91, 324, 76, 23);
    CreateChild(RefreshButton, NWS_VISIBLE);
    RefreshButton->SetTextID(STR_Refresh);
    return 1;
}
NW_HOOK_METHOD(0x100fe200, NCSSQMainEventWnd, OnCreate)

NW_CHECK_VTABLE(NCSSQMainEventWnd, 0x101c9fc8, 75)
