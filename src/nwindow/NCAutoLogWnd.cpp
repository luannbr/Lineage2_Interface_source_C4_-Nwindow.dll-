/*=============================================================================
    NCAutoLogWnd.cpp - the self-login screen. Stock: 0x10037360 .. 0x1003762d; WindowProc
    and OnDestroy are bodies shared with other classes.
=============================================================================*/
#include "nwindow/NCAutoLogWnd.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NConsoleWnd.h"

// 0x10050f60
NCAutoLogWnd::NCAutoLogWnd()
{
    BackTex = NULL;
    bLoginSent = 0;
    Unknown9C = 0;
    Step = 0;
}
NW_HOOK_CTOR(0x10050f60, NCAutoLogWnd)

// 0x10050fc0
NW_HOOK_DTOR(0x10050fc0, NCAutoLogWnd)

// stock body shared with NControl's (0x10004e90)
INT NCAutoLogWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return NCWnd::WindowProc(Msg, wParam, lParam) != 0;
}

// C2 CANDIDATE: the C2 function is not the same code as the C1 one; kept only while it matches the stock one (verify_data)
// 0x10050ca0 (vslot 37) - the text's place (C3: 40% of the width and a quarter of the height, the sizes are FLOAT)
// and AutoLog.txt; NCWnd's OnCreate is not called
INT NCAutoLogWnd::OnCreate()
{
    BackTex = FindTexture(TEXT("NWindow.BlackTexture"));
    TextX = (INT)(Width * 4.0f * 0.1f);
    TextY = (INT)(Height * 0.25f);
    if (!GConfig->GetString(TEXT("AutoLogOn"), TEXT("L2ID"), GAutoLogID, 64, TEXT("AutoLog.txt")))
        appSprintf(GAutoLogID, TEXT("zodiac"));
    if (!GConfig->GetString(TEXT("AutoLogOn"), TEXT("L2Passwd"), GAutoLogPassword, 64,
                            TEXT("AutoLog.txt")))
        appSprintf(GAutoLogPassword, TEXT("zodiac"));
    if (!GConfig->GetInt(TEXT("AutoLogOn"), TEXT("L2Slot"), GAutoLogSlot, TEXT("AutoLog.txt")))
        GAutoLogSlot = 0;
    return 1;
}
NW_HOOK_METHOD(0x10050ca0, NCAutoLogWnd, OnCreate)

// 0x10050d90 (vslot 38) - guarded; NCWnd's OnDestroy and nothing else
INT NCAutoLogWnd::OnDestroy()
{
    guard(NCAutoLogWnd::OnDestroy);
    NCWnd::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10050d90, NCAutoLogWnd, OnDestroy)

// 0x10050e10 (vslot 62) - C3: the lines are drawn in 0xFFDCDCDC
INT NCAutoLogWnd::OnPaint(NCGDevice* Device)
{
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    Device->BeginPaint(ScreenX, ScreenY);
    if (BackTex)
        Device->DrawStretchedTex(0, 0, (INT)Width, (INT)Height, BackTex);
    if (Step < 3)
    {
        for (INT i = 0; i < Step + 1; i++)
            Device->DrawText(TextX, TextY + 12 * i, 0xFFDCDCDC, GAutoLogMessages[i], 0, 0, 0,
                             L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
    }
    else if (Step == 3)
    {
        Device->DrawText(TextX, TextY, 0xFFDCDCDC, GAutoLogMessages[3], 0, 0, 0, L2FONT_Normal,
                         0, 0, 0, 0, 0, 0, 0);
    }
    Device->EndPaint();
    Device->ReleaseClip();
    if (!bLoginSent && GConsole->RequestAuthLogin(GAutoLogID, GAutoLogPassword))
        bLoginSent = 1;
    return 1;
}
NW_HOOK_METHOD(0x10050e10, NCAutoLogWnd, OnPaint)

NW_CHECK_VTABLE(NCAutoLogWnd, 0x101ab690, 74)
