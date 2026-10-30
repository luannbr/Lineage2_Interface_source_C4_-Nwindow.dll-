/*=============================================================================
    LoadingWnd.cpp - the messenger's "please wait" box (C3 only; C3 addresses). Stock: 0x10130A50 (OnCreate), 0x10130A80
    (the mode), 0x10131C00 (OnPaint), 0x101377A0 / 0x10137810 (constructor, destructor).
=============================================================================*/
#include "nwindow/LoadingWnd.h"
#include "nwindow/MsnWnd.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NGameData.h"

enum
{
    SYSMSG_SigningIn = 1324,
    SYSMSG_SigningOut = 1358,
};

// 0x1015c110 - the owner is the argument, the mode is "signing in", the picture not found yet
LoadingWnd::LoadingWnd(MsnWnd* InOwner)
{
    Owner = InOwner;
    bSigningIn = 1;
    BackTex = NULL;
}
NW_HOOK_CTOR_ARGS(0x1015c110, LoadingWnd, (MsnWnd* A), (A))

// 0x1015c180
NW_HOOK_DTOR(0x1015c180, LoadingWnd)

// 0x10119f60 (vslot 25) - NControl's body, shared (hooked there)
#if NW_HOOK_MODE
NW_NAKED INT LoadingWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam) NW_STOCK(0x10148f10)
#else
INT LoadingWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)   // the standalone: NControl::WindowProc's body (0x10148f10)
{
    return NCWnd::WindowProc(Msg, wParam, lParam) != 0;
}
#endif

// C2 CANDIDATE: the C2 function is not the same code as the C1 one; kept only while it matches the stock one (verify_data)
// 0x101556c0 (vslot 37) - NCWnd's, then the picture
INT LoadingWnd::OnCreate()
{
    NCWnd::OnCreate();
    BackTex = FindTexture(TEXT("L2UI_CH3.Msn.messenger_back1"));
    return 1;
}
NW_HOOK_METHOD(0x101556c0, LoadingWnd, OnCreate)

// 0x10156890 (vslot 56) - the picture (256 x 353), the little icon at (43, 144) and the line of text at (83, 160)
INT LoadingWnd::OnPaint(NCGDevice* Device)
{
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    if (BackTex)
        Device->DrawTexture(0, 0, 0x100, 0x161, 0, 0, 0x100, 0x161, BackTex, 0xFF, 1);
    Device->DrawTexture(0x2B, 0x90, 0x27, 0x21, 0, 0, 0x27, 0x21, FindTexture(TEXT("L2UI_CH3.Msn.msnlogin01"), 1), 0xFF, 1);
    const FString& Line = bSigningIn ? MSN_SYSMSG(SYSMSG_SigningIn)
                                     : MSN_SYSMSG(SYSMSG_SigningOut);
    Device->DrawText(0x53, 0xA0, 0xFFDCDCDC, *Line, 0, 0, 0, (L2FontType)0, 0, 0, 0, 0, 0, 0, 0);
    Device->ReleaseClip();
    Device->EndPaint();
    return 1;
}
NW_HOOK_METHOD(0x10156890, LoadingWnd, OnPaint)

// 0x101556f0 (name ours; anonymous in the db; ret 4) - the mode is set and the box shown (NCWnd's, not the virtual)
void LoadingWnd::ShowWithMode(UBOOL bInSigningIn)
{
    bSigningIn = bInSigningIn;
    NCWnd::ShowWindow();
}
NW_HOOK_METHOD(0x101556f0, LoadingWnd, ShowWithMode)

NW_CHECK_VTABLE(LoadingWnd, 0x101dcc48, 74)
