/*=============================================================================
    NCOlympiadControlWnd.cpp - the olympiad observer's control bar (C4 only). Stock: 0x1005fba0 (constructor), 0x1005fbf0
    (destructor), 0x1013c8b0 .. 0x1013ca20+ (the buttons, WindowProc, OnDestroy), 0x1013caf0 (OnPaint), 0x1013cbf0 (OnCreate, stock).
=============================================================================*/
#include "nwindow/NCOlympiadControlWnd.h"
#include "nwindow/NCButton.h"
#include "nwindow/NCFrameCtrl.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NConsoleWnd.h"

// NWM_COMMAND ids of the frame bar and the two buttons (names ours)
enum
{
    NID_OLYMPIAD_FRAME = 0xAADDCEA0,
    NID_OLYMPIAD_STOP = 0xAADDCEB0,
    NID_OLYMPIAD_OTHER = 0xAADDCEB1,
};

// 0x1005fba0 - NCWnd's constructor; the fields are left as allocated
NCOlympiadControlWnd::NCOlympiadControlWnd()
{
}
NW_HOOK_CTOR(0x1005fba0, NCOlympiadControlWnd)

// 0x1005fbf0
NW_HOOK_DTOR(0x1005fbf0, NCOlympiadControlWnd)

// 0x1013cbf0 (vslot 37, guard: the stock's string is NCManorInfoSettingWnd's, copied) - the frame bar (12 px wide, as high as the
// window), the background piece and the two buttons (SysString 611 and 1252)
INT NCOlympiadControlWnd::OnCreate()
{
    guard(NCManorInfoSettingWnd::OnCreate);
    NCWnd::OnCreate();
    FrameCtrl = new NCFrameCtrl(1, 1, 1);
    FrameID = NID_OLYMPIAD_FRAME;
    FrameCtrl->ID = NID_OLYMPIAD_FRAME;
    FrameCtrl->SetPosition(0, 0, 12, (INT)Height);
    CreateChild(FrameCtrl, NWS_VISIBLE);
    BackTex = FindTexture(TEXT("L2UI_CH3.Olympiad.olympiad_back"));
    StopObservingButton = new NCButton(0, 0);
    StopObservingButton->ID = NID_OLYMPIAD_STOP;
    StopObservingButton->SetPosition(19, 5, 0, 0);
    CreateChild(StopObservingButton, NWS_VISIBLE);
    StopObservingButton->SetTextID(611);
    OtherGameButton = new NCButton(0, 0);
    OtherGameButton->ID = NID_OLYMPIAD_OTHER;
    OtherGameButton->SetPosition(19, 29, 0, 0);
    CreateChild(OtherGameButton, NWS_VISIBLE);
    OtherGameButton->SetTextID(1252);
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1013cbf0, NCOlympiadControlWnd, OnCreate)

// 0x1013c8b0 (guard) - the console asks the server to end the observing
INT NCOlympiadControlWnd::OnStopObservingBtnClick()
{
    guard(NCOlympiadControlWnd::OnStopObservingBtnClick);
    GConsole->RequestOlympiadObserverEnd();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1013c8b0, NCOlympiadControlWnd, OnStopObservingBtnClick)

// 0x1013c930 (guard) - the console asks the server for the list of matches
INT NCOlympiadControlWnd::OnOtherGameBtnClick()
{
    guard(NCOlympiadControlWnd::OnOtherGameBtnClick);
    GConsole->RequestOlympiadMatchList();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1013c930, NCOlympiadControlWnd, OnOtherGameBtnClick)

// 0x1013c9b0 (vslot 25)
INT NCOlympiadControlWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCWnd::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND)
    {
        if (wParam == NID_OLYMPIAD_STOP)
            return OnStopObservingBtnClick() != 0;
        if (wParam == NID_OLYMPIAD_OTHER)
            return OnOtherGameBtnClick() != 0;
    }
    return 0;
}
NW_HOOK_METHOD(0x1013c9b0, NCOlympiadControlWnd, WindowProc)

// 0x1013ca20 (vslot 38, guard) - the two buttons go, then NCWnd's OnDestroy
INT NCOlympiadControlWnd::OnDestroy()
{
    guard(NCOlympiadControlWnd::OnDestroy);
    if (StopObservingButton)
    {
        RemoveChildWindow(StopObservingButton->Entry);
        delete StopObservingButton;
        StopObservingButton = NULL;
    }
    if (OtherGameButton)
    {
        RemoveChildWindow(OtherGameButton->Entry);
        delete OtherGameButton;
        OtherGameButton = NULL;
    }
    NCWnd::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1013ca20, NCOlympiadControlWnd, OnDestroy)

// 0x1013caf0 (vslot 62, guard) - the background piece from x = 12, as high as the window and 12 less wide
INT NCOlympiadControlWnd::OnPaint(NCGDevice* Device)
{
    guard(NCOlympiadControlWnd::OnPaint);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    Device->BeginPaint(ScreenX, ScreenY);
    if (BackTex)
        Device->DrawTexture(0xC, 0, (INT)(Width - 12.0f), (INT)Height, 0, 0, (INT)(Width - 12.0f), (INT)Height, BackTex, 0xFF, 1);
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1013caf0, NCOlympiadControlWnd, OnPaint)

NW_CHECK_VTABLE(NCOlympiadControlWnd, 0x101ae898, 74)
