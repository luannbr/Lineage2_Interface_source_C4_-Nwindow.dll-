/*=============================================================================
    NCObserverWnd.cpp - the observer mode box (C3 addresses). Stock: WindowProc 0x100AC530, OnExit 0x100AC590, OnPaint
    0x100AD630, OnDestroy 0x100AD6C0, OnCreate 0x100AF700, the constructor 0x100AFFC0 and the destructor 0x100B0010.
=============================================================================*/
#include "nwindow/NCObserverWnd.h"
#include "nwindow/NCButton.h"
#include "nwindow/NCFrameCtrl.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NConsoleWnd.h"

// ids (names ours)
enum
{
    NID_OBSERVER_EXIT = 0x1012,
    NID_OBSERVER_FRAME = 0xB000F0,
};
enum { SYSSTR_ObserverExit = 611 };   // the button's text (name ours)

// 0x100bcc40 - the button comes with OnCreate
NCObserverWnd::NCObserverWnd()
{
    BackTex = NULL;
    ExitButton = NULL;
}
NW_HOOK_CTOR(0x100bcc40, NCObserverWnd)

// 0x100bcc90
NW_HOOK_DTOR(0x100bcc90, NCObserverWnd)

// 0x100b9020 (vslot 25)
INT NCObserverWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCWnd::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND && wParam == NID_OBSERVER_EXIT)
        return OnExit() != 0;
    return 0;
}
NW_HOOK_METHOD(0x100b9020, NCObserverWnd, WindowProc)

// 0x100b9080 (vslot 68; name ours) - the exit button: the server is asked to end the observation (an empty packet)
INT NCObserverWnd::OnExit()
{
    L2ParamStack Params(10);
    ::nw::VirtualCall<void>(GConsole, 439, &Params);   // NConsoleWnd::RequestObserverEndPacket (the header's slot number is C2's)
    return 1;
}
NW_HOOK_METHOD(0x100b9080, NCObserverWnd, OnExit)

// 0x100b9fa0 (vslot 38, guard: NCObserverWnd::OnDestroy) - the exit button goes, then NCWnd's
INT NCObserverWnd::OnDestroy()
{
    guard(NCObserverWnd::OnDestroy);
    if (ExitButton)
    {
        RemoveChildWindow(ExitButton->Entry);
        delete ExitButton;
        ExitButton = NULL;
    }
    NCWnd::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100b9fa0, NCObserverWnd, OnDestroy)

// 0x100b9f10 (vslot 62) - the backdrop piece (102 x 32) 12 pixels in; the title bar draws itself
INT NCObserverWnd::OnPaint(NCGDevice* Device)
{
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    Device->BeginPaint(ScreenX, ScreenY);
    if (BackTex)
        Device->DrawTexture(12, 0, 0x66, 0x20, 0, 0, 0x66, 0x20, BackTex, 0xFF, 1);
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
}
NW_HOOK_METHOD(0x100b9f10, NCObserverWnd, OnPaint)

// 0x100bb4a0 (vslot 37) - a title bar 12 wide and as high as the window (the frame is NCWnd's FrameCtrl), the
// backdrop texture and the exit button (76 x 23 at (19, 5)); C4: the button's text is set by its sysstring number
// (SetTextID 611) instead of by the string
INT NCObserverWnd::OnCreate()
{
    NCWnd::OnCreate();
    NRectF LocalRect = { 0, 0, 0.0f, 0.0f };
    GetLocalRect(&LocalRect);

    FrameCtrl = new NCFrameCtrl(1, 1, 1);
    FrameCtrl->ID = NID_OBSERVER_FRAME;
    FrameCtrl->SetPosition(0, 0, 12, (INT)Height);
    CreateChild(FrameCtrl, NWS_VISIBLE);

    BackTex = FindTexture(TEXT("L2UI_ch3.etc.broadcast_back"));

    ExitButton = new NCButton(TEXT("L2UI_ch3.button.btn1_normal"), TEXT("L2UI_ch3.button.btn1_normalon"), 0);
    ExitButton->ID = NID_OBSERVER_EXIT;
    ExitButton->SetPosition(19, 5, 76, 23);
    CreateChild(ExitButton, NWS_VISIBLE);
    ExitButton->SetTextID(SYSSTR_ObserverExit);
    return 1;
}
NW_HOOK_METHOD(0x100bb4a0, NCObserverWnd, OnCreate)

NW_CHECK_VTABLE(NCObserverWnd, 0x101bdd38, 75)
