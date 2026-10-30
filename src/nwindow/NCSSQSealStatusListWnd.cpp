/*=============================================================================
    NCSSQSealStatusListWnd.cpp - the Seven Signs seal list tab. C3 stock: 0x100EBAF0 .. 0x100EBB72 (WindowProc, the
    list reset, the refresh command), 0x100ECDF0 (OnDestroy), 0x100ECEC0 (paint), 0x100ED200 / 0x100ED250
    (constructor, destructor), 0x100ED2A0 (OnCreate).
=============================================================================*/
#include "nwindow/NCSSQSealStatusListWnd.h"
#include "nwindow/NCButton.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NCSSQSealStatusGaraTreeCtrl.h"
#include "nwindow/NSSQHelpers.h"

enum { NID_SSQSEAL_REFRESH = 0xFF00F0A0 };   // name ours
enum { STR_Refresh = 0x3AA };

// 0x100ffab0 - NCWnd's alone
NCSSQSealStatusListWnd::NCSSQSealStatusListWnd()
{
}
NW_HOOK_CTOR(0x100ffab0, NCSSQSealStatusListWnd)

// 0x100ffb00
NW_HOOK_DTOR(0x100ffb00, NCSSQSealStatusListWnd)

// 0x100fe4e0 (vslot 25) - the refresh button
INT NCSSQSealStatusListWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCWnd::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND && wParam == NID_SSQSEAL_REFRESH)
        return OnRefresh() != 0;
    return 0;
}
NW_HOOK_METHOD(0x100fe4e0, NCSSQSealStatusListWnd, WindowProc)

// 0x100fe550 (vslot 68, name ours) - the console asks the server for the seal list again
INT NCSSQSealStatusListWnd::OnRefresh()
{
    ((NConsoleSSQ*)GConsole)->RequestSSQStatus(3);
    return 1;
}
NW_HOOK_METHOD(0x100fe550, NCSSQSealStatusListWnd, OnRefresh)

// 0x100fe540 (name ours) - the tree emptied (the control's reset)
void NCSSQSealStatusListWnd::ClearList()
{
    TreeCtrl->ResetScroll();
}
NW_HOOK_METHOD(0x100fe540, NCSSQSealStatusListWnd, ClearList)

// 0x100ff690 (vslot 38) - NCWnd's first, then the control and the button go
INT NCSSQSealStatusListWnd::OnDestroy()
{
    guard(NCSSQSealStatusListWnd::OnDestroy);
    NCWnd::OnDestroy();
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
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100ff690, NCSSQSealStatusListWnd, OnDestroy)

// 0x100ff760 (vslot 62) - the panel over the whole window
INT NCSSQSealStatusListWnd::OnPaint(NCGDevice* Device)
{
    guard(NCSSQSealStatusListWnd::OnPaint);
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    if (BackTex)
        Device->DrawTexture(0, 0, (INT)Width, (INT)Height, 0, 0, (INT)Width, (INT)Height, BackTex, 0xFF, 1);
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100ff760, NCSSQSealStatusListWnd, OnPaint)

// 0x100ffb50 (vslot 37) - the panel, the seal control and the refresh button under it
INT NCSSQSealStatusListWnd::OnCreate()
{
    NCWnd::OnCreate();
    BackTex = FindTexture(TEXT("L2UI_CH3.SSQWnd.ssq_back2"));

    TreeCtrl = new NCSSQSealStatusGaraTreeCtrl();
    TreeCtrl->SetPosition(9, 11, 238, 304);
    CreateChild(TreeCtrl, NWS_VISIBLE);

    RefreshButton = new NCButton(TEXT("L2UI_CH3.BUTTON.Btn1_normal"), TEXT("L2UI_CH3.BUTTON.Btn1_normalOn"), 0);
    RefreshButton->ID = NID_SSQSEAL_REFRESH;
    RefreshButton->SetPosition(91, 324, 76, 23);
    CreateChild(RefreshButton, NWS_VISIBLE);
    RefreshButton->SetTextID(STR_Refresh);
    return 1;
}
NW_HOOK_METHOD(0x100ffb50, NCSSQSealStatusListWnd, OnCreate)

NW_CHECK_VTABLE(NCSSQSealStatusListWnd, 0x101ca600, 75)
