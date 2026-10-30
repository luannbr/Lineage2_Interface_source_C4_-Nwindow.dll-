/*=============================================================================
    NCHtmlSliderBar.cpp - the slider of the HTML pages. Stock: 0x10032fc0 .. 0x1003305f,
    0x10033060 .. 0x1003321e (WindowProc is a body the linker folded with the buttons').
=============================================================================*/
#include "nwindow/NCHtmlSliderBar.h"
#include "nwindow/NCHtmlViewer.h"
#include "nwindow/NCGDevice.h"

// 0x1004b0a0 - NCSliderBar's, nothing more
NCHtmlSliderBar::NCHtmlSliderBar()
{
}
NW_HOOK_CTOR(0x1004b0a0, NCHtmlSliderBar)

// 0x1004b0f0
NW_HOOK_DTOR(0x1004b0f0, NCHtmlSliderBar)

// C2 CANDIDATE: the C2 function is not the same code as the C1 one; kept only while it matches the stock one (verify_data)
// 0x1004b140 (anonymous in the stock image) - the three textures of the thumb by name
NCHtmlSliderBar::NCHtmlSliderBar(const TCHAR* TopTexName, const TCHAR* MidTexName, const TCHAR* BottomTexName)
{
    ThumbTopTex = FindTexture(TopTexName);
    ThumbMidTex = FindTexture(MidTexName);
    ThumbBottomTex = FindTexture(BottomTexName);
}
NW_HOOK_CTOR_ARGS(0x1004b140, NCHtmlSliderBar, (const TCHAR* A, const TCHAR* B, const TCHAR* C), (A, B, C))


// stock body shared with NCExButton's (0x100301f0)
INT NCHtmlSliderBar::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return NCSliderBar::WindowProc(Msg, wParam, lParam) != 0;
}

// 0x1004a2c0 (vslot 56) - NCSliderBar::OnPaint with the clip of the page (the first ancestor that is not a control);
// the origin goes on before the clip. The thumb's caps are 15 x 8, as the bar's own
INT NCHtmlSliderBar::OnPaint(NCGDevice* Device)
{
    guard(NCHtmlSliderBar::OnPaint);
    Device->BeginPaint(ScreenX, ScreenY);
    NCWnd* Owner = FirstNonControlParent(this);
    if (Owner)
        Device->SetClip(Owner->ScreenX, Owner->ScreenY, (INT)Owner->Width, (INT)Owner->Height);
    if (ThumbSize > 0)
    {
        if (ThumbTopTex)
            Device->DrawTexture(0, ThumbPos, 15, 8, 0, 0, 15, 8, ThumbTopTex, 0xFF, 1);
        if (ThumbBottomTex)
            Device->DrawTexture(0, ThumbPos + ThumbSize - 8, 15, 8, 0, 0, 15, 8, ThumbBottomTex, 0xFF, 1);
        if (ThumbMidTex)
            Device->DrawStretchedTex(0, ThumbPos + 8, 15, ThumbSize - 16, 0, 0, 15, 8, ThumbMidTex, 0xFF);
    }
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1004a2c0, NCHtmlSliderBar, OnPaint)

NW_CHECK_VTABLE(NCHtmlSliderBar, 0x101aa688, 75)
