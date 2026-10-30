/*=============================================================================
    NCReplayPlayWnd.cpp - the playing screen of the replay (C3 only; C3 addresses). Stock: OnCreate 0x100E6650,
    OnDestroy 0x100E6790, OnPaint 0x100E6DC0, the constructor 0x100E7A00 and the destructor 0x100E7A50.
=============================================================================*/
#include "nwindow/NCReplayPlayWnd.h"
#include "nwindow/NCGDevice.h"

// 0x100fa170 - the two logos come with OnCreate
NCReplayPlayWnd::NCReplayPlayWnd() : LogoTex(NULL), MiniLogoTex(NULL)
{
}
NW_HOOK_CTOR(0x100fa170, NCReplayPlayWnd)

// 0x100fa1c0
NW_HOOK_DTOR(0x100fa1c0, NCReplayPlayWnd)

// 0x10119f60 (vslot 25) - NControl's body, shared (hooked there)
#if NW_HOOK_MODE
NW_NAKED INT NCReplayPlayWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam) NW_STOCK(0x10148f10)
#else
INT NCReplayPlayWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return NCWnd::WindowProc(Msg, wParam, lParam) != 0;   // 0x10148f10: NControl's body
}
#endif

// 0x100f8fc0 (vslot 38, guard: NCReplayPlayWnd::OnDestroy) - nothing of its own to free
INT NCReplayPlayWnd::OnDestroy()
{
    guard(NCReplayPlayWnd::OnDestroy);
    NCWnd::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100f8fc0, NCReplayPlayWnd, OnDestroy)

// C2 CANDIDATE: the C2 function is not the same code as the C1 one; kept only while it matches the stock one (verify_data)
// 0x100f8e80 (vslot 37) - the logos by the country's names in localization.ini (the replay's own, then the small one
// the login screen uses)
INT NCReplayPlayWnd::OnCreate()
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
NW_HOOK_METHOD(0x100f8e80, NCReplayPlayWnd, OnCreate)

// 0x100f95f0 (vslot 62; the guard string is NCLoginWnd::OnPaint, copied from the login screen's paint) - C4: the paint
// sets up the clip and draws nothing (the C3 code drew the small logo in the lower right corner and the replay logo in
// the upper left one here; both textures are still made by OnCreate)
INT NCReplayPlayWnd::OnPaint(NCGDevice* Device)
{
    guard(NCLoginWnd::OnPaint);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    Device->BeginPaint(ScreenX, ScreenY);
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100f95f0, NCReplayPlayWnd, OnPaint)

NW_CHECK_VTABLE(NCReplayPlayWnd, 0x101c9180, 74)
