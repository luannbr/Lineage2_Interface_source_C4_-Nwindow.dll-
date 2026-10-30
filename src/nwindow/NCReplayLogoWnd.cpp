/*=============================================================================
    NCReplayLogoWnd.cpp - the logos of the replay screens (new in C4; C4 addresses). Stock: OnCreate 0x100F8610, OnDestroy
    0x100F8750, OnPaint 0x100F87D0, the constructor 0x100F8990 and the destructor 0x100F89E0. The scalar deleting destructor
    (0x100F89B0) and InternalConstructor (0x100F8A30) stay stock.
=============================================================================*/
#include "nwindow/NCReplayLogoWnd.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NConsoleWnd.h"

// 0x100f8990 - the two textures come with OnCreate
NCReplayLogoWnd::NCReplayLogoWnd() : LogoTex(NULL), MiniLogoTex(NULL)
{
}
NW_HOOK_CTOR(0x100f8990, NCReplayLogoWnd)

// 0x100f89e0
NW_HOOK_DTOR(0x100f89e0, NCReplayLogoWnd)

// 0x10148f10 (vslot 25) - NControl's body, shared (hooked there)
#if NW_HOOK_MODE
NW_NAKED INT NCReplayLogoWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam) NW_STOCK(0x10148f10)
#else
INT NCReplayLogoWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return NCWnd::WindowProc(Msg, wParam, lParam) != 0;   // 0x10148f10: NControl's body
}
#endif

// 0x100f8610 (vslot 37) - the logos by the country's names in localization.ini (the replay's own, then the small one the
// login screen uses)
INT NCReplayLogoWnd::OnCreate()
{
    NCWnd::OnCreate();
    FString LogoName;
    TCHAR Key[256];
    appSprintf(Key, TEXT("%s_ReplayLogoTexture"), GCountryName);
    if (!GConfig->GetString(GCountryName, Key, LogoName, TEXT("localization.ini")))
        LogoName = TEXT("L2Font.replay_logo-k");
    LogoTex = FindTexture(*LogoName);
    appSprintf(Key, TEXT("%s_MiniLogoTexture"), GCountryName);
    if (!GConfig->GetString(GCountryName, Key, LogoName, TEXT("localization.ini")))
        LogoName = TEXT("L2Font.start_minilogo-k");
    MiniLogoTex = FindTexture(*LogoName);
    return 1;
}
NW_HOOK_METHOD(0x100f8610, NCReplayLogoWnd, OnCreate)

// 0x100f8750 (vslot 38, guard) - nothing of its own to free
INT NCReplayLogoWnd::OnDestroy()
{
    guard(NCReplayLogoWnd::OnDestroy);
    NCWnd::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100f8750, NCReplayLogoWnd, OnDestroy)

// 0x100f87d0 (vslot 62; the guard string is NCLoginWnd::OnPaint, copied from the login screen's paint) - the scale is the larger
// of the screen's height / 768 and width / 1024 (C4: the screen's size, GSEKScreenX / Y; the C3 code used the window's): the
// small logo (232 x 120) sits in the lower right corner of the screen at 0.8 of it and the replay logo (256 x 64) in the upper
// left one at 0.7 of it
INT NCReplayLogoWnd::OnPaint(NCGDevice* Device)
{
    guard(NCLoginWnd::OnPaint);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    Device->BeginPaint(ScreenX, ScreenY);
    FLOAT ScreenXf = (FLOAT)GSEKScreenX;
    FLOAT MiniScale = (FLOAT)(__max(GSEKScreenX * 0.0009765625, GSEKScreenY * 0.0013020833333333333) * 0.8);
    FLOAT LogoScale = (FLOAT)(__max(GSEKScreenX * 0.0009765625, GSEKScreenY * 0.0013020833333333333) * 0.7);
    Device->DrawTexture((INT)(ScreenXf - MiniScale * 256.0f), (INT)(GSEKScreenY - MiniScale * 120.0f),
                        (INT)(MiniScale * 232.0f), (INT)(MiniScale * 120.0f), 0, 0, 0xE8, 0x78, MiniLogoTex, 0xFF, 1);
    Device->DrawTexture((INT)(LogoScale * 30.0f), (INT)(LogoScale * 30.0f), (INT)(LogoScale * 256.0f),
                        (INT)(LogoScale * 64.0f), 0, 0, 0x100, 0x40, LogoTex, 0xFF, 1);
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100f87d0, NCReplayLogoWnd, OnPaint)

NW_CHECK_VTABLE(NCReplayLogoWnd, 0x101c89f0, 74)
