/*=============================================================================
    NCMessageWnd.cpp - the console's one-line message window. Stock: 0x10077fa0 .. 0x100781d0.
=============================================================================*/
#include "nwindow/NCMessageWnd.h"
#include "nwindow/NCGDevice.h"

#include <wchar.h>

// 0x100b86d0 - NCWnd's constructor is inlined in the stock code
NCMessageWnd::NCMessageWnd()
    : BackTex(NULL)
{
}
NW_HOOK_CTOR(0x100b86d0, NCMessageWnd)

// 0x100b8760
NW_HOOK_DTOR(0x100b8760, NCMessageWnd)


// 0x10004e90 (vslot 24) - stock body shared with NControl (hooked there)
INT NCMessageWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return NCWnd::WindowProc(Msg, wParam, lParam) != 0;
}

// C2 CANDIDATE: the C2 function is not the same code as the C1 one; kept only while it matches the stock one (verify_data)
// 0x100b8460 (vslot 37)
INT NCMessageWnd::OnCreate()
{
    NCWnd::OnCreate();
    BackTex = FindTexture(TEXT("sek.cbui141"));
    return 1;
}
NW_HOOK_METHOD(0x100b8460, NCMessageWnd, OnCreate)

// 0x100b8490 (vslot 38) - guarded; NCWnd's OnDestroy and nothing else
INT NCMessageWnd::OnDestroy()
{
    guard(NCMessageWnd::OnDestroy);
    NCWnd::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100b8490, NCMessageWnd, OnDestroy)

// 0x100b8510 (C1 0x10077fd0) - the text emptied: a lone tail jump to FString::Empty on the member at +0x9C
void NCMessageWnd::ClearText()
{
    Text.Empty();
}
NW_HOOK_METHOD(0x100b8510, NCMessageWnd, ClearText)

// 0x100b8520 - NULL keeps the text
void NCMessageWnd::SetText(const TCHAR* NewText)
{
    if (NewText)
        Text = NewText;
}
NW_HOOK_METHOD(0x100b8520, NCMessageWnd, SetText)

// 0x100b8540 (vslot 62) - the text is centered when it fits the window, else drawn from (50, 8) wrapped to the screen
// width; C3: the sizes are FLOAT, the text is 0xFFDCDCDC. The centering halves the text's size (signed) and takes it
// off half the window's
INT NCMessageWnd::OnPaint(NCGDevice* Device)
{
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    Device->BeginPaint(ScreenX, ScreenY);
    if (BackTex)
        Device->DrawTexture(0, 0, (INT)Width, (INT)Height, 0, 0, (INT)Width, (INT)Height, BackTex);
    SIZE Size;
    NCGDevice::GetTextExtent(&Size, *Text, 0, 0, 0, 0, 0, L2FONT_Normal);
    if (Text.Num())
    {
        if (Size.cx < (INT)Width)
            Device->DrawText((INT)(Width * 0.5f - (Size.cx / 2)), (INT)(Height * 0.5f - (Size.cy / 2)),
                             0xFFDCDCDC, *Text, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
        else
            Device->DrawText(50, 8, 0xFFDCDCDC, *Text, GSEKScreenX - 100, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
    }
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
}
NW_HOOK_METHOD(0x100b8540, NCMessageWnd, OnPaint)

NW_CHECK_VTABLE(NCMessageWnd, 0x101bd350, 74)
