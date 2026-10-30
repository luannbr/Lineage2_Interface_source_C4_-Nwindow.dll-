/*=============================================================================
    NCLoadingWnd.cpp - the loading screen. Stock: 0x10068910 .. 0x10068b5d (WindowProc
    and OnCreate are bodies the linker folded with other classes).
=============================================================================*/
#include "nwindow/NCLoadingWnd.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NConsoleWnd.h"

// 0x1009a850
NCLoadingWnd::NCLoadingWnd()
{
    LoadingTex = NULL;
}
NW_HOOK_CTOR(0x1009a850, NCLoadingWnd)

// 0x1009a8a0
NW_HOOK_DTOR(0x1009a8a0, NCLoadingWnd)

// stock body shared with NControl's (0x10004e90)
INT NCLoadingWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return NCWnd::WindowProc(Msg, wParam, lParam) != 0;
}

// 0x1009a7b0 (vslot 62) - the picture (a 1024 x 768 source) over the whole window; no guard
INT NCLoadingWnd::OnPaint(NCGDevice* Device)
{
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    Device->BeginPaint(ScreenX, ScreenY);
    if (LoadingTex)
        Device->DrawStretchedTex(0, 0, (INT)Width, (INT)Height, 0, 0, 1024, 768, LoadingTex, 0xFF);
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
}
NW_HOOK_METHOD(0x1009a7b0, NCLoadingWnd, OnPaint)

// 0x1009a600 - ScreenID is NConsoleWnd +0x39C4, set when a server is chosen from its age rating
// (NCServerSelectWnd::OnOK): 10001 all ages, 10002 15+, 10003 18+. Only the Korean client uses it; the others read
// "<country>_LoadingTexture" from localization.ini (C4: the same body as C3)
void NCLoadingWnd::SetLoadingTexture(INT ScreenID)
{
    FString Name;
    if (GLanguageType == 0)
    {
        switch (ScreenID)
        {
        case 10001:
            LoadingTex = FindTexture(TEXT("L2Font.loading02-k"));
            break;
        case 10002:
            LoadingTex = FindTexture(TEXT("L2Font.loading03-k"));
            break;
        case 10003:
            LoadingTex = FindTexture(TEXT("L2Font.loading04-k"));
            break;
        default:
            LoadingTex = FindTexture(TEXT("L2Font.loading02-k"));
            break;
        }
    }
    else
    {
        TCHAR Key[256];
        appSprintf(Key, TEXT("%s_LoadingTexture"), GCountryName);
        if (!GConfig->GetString(GCountryName, Key, Name, TEXT("localization.ini")))
            Name = TEXT("L2Font.loading02-k");
        LoadingTex = FindTexture(*Name);
    }
}
NW_HOOK_METHOD(0x1009a600, NCLoadingWnd, SetLoadingTexture)

// 0x1009a700 (ret 0xC; new in C4, name ours) - the picture by name: the name given (copied to a buffer), "L2Font.loading02-k"
// when it is empty. The by-value text is destroyed here
void NCLoadingWnd::SetTextureName(FString Name)
{
    TCHAR Buffer[256];
    const TCHAR* Use;
    if (Name == TEXT(""))
        Use = TEXT("L2Font.loading02-k");
    else
    {
        appStrcpy(Buffer, *Name);
        Use = Buffer;
    }
    LoadingTex = FindTexture(Use);
}
NW_HOOK_METHOD(0x1009a700, NCLoadingWnd, SetTextureName)

NW_CHECK_VTABLE(NCLoadingWnd, 0x101b71e8, 74)
