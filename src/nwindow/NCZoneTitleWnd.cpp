/*=============================================================================
    NCZoneTitleWnd.cpp - the zone name over the screen (C3 only). Stock: 0x1010b320 .. 0x1010bc1d; the setter
    (0x1010b320) and Tick (0x1010b4a0, with 0x1010b470, the clamp) stay stock: the console calls them by address.
=============================================================================*/
#include "nwindow/NCZoneTitleWnd.h"
#include "nwindow/NCGDevice.h"

// 0x10121970 - the lines empty, both alphas full, the deco texture found (the base window's constructor runs first;
// C4: the texture lookup takes the fallback flag, the header's default)
NCZoneTitleWnd::NCZoneTitleWnd()
{
    Line1.Empty();
    Line2.Empty();
    Line3.Empty();
    Elapsed = 0.0f;
    TextAlpha = 0xFF;
    ShadowAlpha = 0xFF;
    DecoTex = FindTexture(TEXT("L2ui_ch3.etc.fontdeco"));
}
NW_HOOK_CTOR(0x10121970, NCZoneTitleWnd)

// 0x10121a70 - the three lines go in reverse
NW_HOOK_DTOR(0x10121a70, NCZoneTitleWnd)

// 0x10119f60 (vslot 25) - NControl's body, shared (folded) by the classes that do not override it
INT NCZoneTitleWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return NCWnd::WindowProc(Msg, wParam, lParam) != 0;
}

// 0x10121270 (name ours) - the width of the longest line: the longer one by characters (the first wins a tie with
// the second and the third, the second a tie with the third), measured in font 4
INT NCZoneTitleWnd::GetTitleWidth()
{
    INT Len1 = Line1.Num() ? Line1.Num() - 1 : 0;
    INT Len2 = Line2.Num() ? Line2.Num() - 1 : 0;
    INT Len3 = Line3.Num() ? Line3.Num() - 1 : 0;
    SIZE Size;
    if (Len2 > Len1)
    {
        if (Len2 < Len3)
            NCGDevice::GetTextExtent(&Size, *Line3, 0, 0, 0, 0, 0, (L2FontType)4);
        else
            NCGDevice::GetTextExtent(&Size, *Line2, 0, 0, 0, 0, 0, (L2FontType)4);
    }
    else
    {
        if (Len1 < Len3)
            NCGDevice::GetTextExtent(&Size, *Line3, 0, 0, 0, 0, 0, (L2FontType)4);
        else
            NCGDevice::GetTextExtent(&Size, *Line1, 0, 0, 0, 0, 0, (L2FontType)4);
    }
    return Size.cx;
}
NW_HOOK_METHOD(0x10121270, NCZoneTitleWnd, GetTitleWidth)

// 0x101215f0 (vslot 62) - guarded. The deco texture (256 x 256 at (-100, -80), the text alpha), then each line
// centered on the width of the longest one in font 4: a shadow at (+3, +3 from its place) and the text, the lines
// one cell under the other (the first line's height + 5)
INT NCZoneTitleWnd::OnPaint(NCGDevice* Device)
{
    guard(NCZoneTitleWnd::OnPaint);
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    Device->DrawTexture(-100, -80, 0x100, 0x100, 0, 0, 0x100, 0x100, DecoTex, TextAlpha, 0);
    DWORD TextColor = ((DWORD)TextAlpha << 24) | 0xF0F0DC;
    DWORD ShadowColor = (DWORD)ShadowAlpha << 24;
    if (Line1.Num() != 0 && Line1.Num() - 1 > 0)
    {
        SIZE Size1;
        NCGDevice::GetTextExtent(&Size1, *Line1, 0, 0, 0, 0, 0, (L2FontType)4);
        Device->DrawText(GetTitleWidth() / 2 - Size1.cx / 2 + 3, 3, ShadowColor, *Line1, 0, 0, 0,
                         (L2FontType)4, 0, 0, 0, 0, 0, 0, 0);
        Device->DrawText(GetTitleWidth() / 2 - Size1.cx / 2, 0, TextColor, *Line1, 0, 0, 0,
                         (L2FontType)4, 0, 0, 0, 0, 0, 0, 0);

        SIZE Size2;
        NCGDevice::GetTextExtent(&Size2, *Line2, 0, 0, 0, 0, 0, (L2FontType)4);
        Device->DrawText(GetTitleWidth() / 2 - Size2.cx / 2 + 3, Size1.cy + 3, ShadowColor, *Line2, 0, 0, 0,
                         (L2FontType)4, 0, 0, 0, 0, 0, 0, 0);
        Device->DrawText(GetTitleWidth() / 2 - Size2.cx / 2, Size1.cy + 5, TextColor, *Line2, 0, 0, 0,
                         (L2FontType)4, 0, 0, 0, 0, 0, 0, 0);

        SIZE Size3;
        NCGDevice::GetTextExtent(&Size3, *Line3, 0, 0, 0, 0, 0, (L2FontType)4);
        Device->DrawText(GetTitleWidth() / 2 - Size3.cx / 2 + 3, Size1.cy * 2 + 6, ShadowColor, *Line3, 0, 0, 0,
                         (L2FontType)4, 0, 0, 0, 0, 0, 0, 0);
        Device->DrawText(GetTitleWidth() / 2 - Size3.cx / 2, Size1.cy * 2 + 10, TextColor, *Line3, 0, 0, 0,
                         (L2FontType)4, 0, 0, 0, 0, 0, 0, 0);
    }
    Device->ReleaseClip();
    Device->EndPaint();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x101215f0, NCZoneTitleWnd, OnPaint)

NW_CHECK_VTABLE(NCZoneTitleWnd, 0x101d11c8, 74)
