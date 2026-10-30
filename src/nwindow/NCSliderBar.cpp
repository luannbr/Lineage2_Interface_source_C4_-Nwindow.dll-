/*=============================================================================
    NCSliderBar.cpp - scroll bar thumb. Stock: 0x100217a0 .. 0x10021cd5.
=============================================================================*/
#include "nwindow/NCSliderBar.h"
#include "nwindow/NCGDevice.h"

// 0x1002c4e0 - no textures (the three-texture constructor is the anonymous 0x10029a80); Total and TrackLen are left as
// allocated. NControl's constructor is inlined in the stock one (the same stores)
NCSliderBar::NCSliderBar()
{
    ThumbSize = 0;
    ThumbPos = 0;
    GrabOffset = 0;
    Value = 0;
    Page = 0;
    bDragging = 0;
    ThumbTopTex = NULL;
    ThumbMidTex = NULL;
    ThumbBottomTex = NULL;
}
NW_HOOK_CTOR(0x1002c4e0, NCSliderBar)

// 0x1002c580
NW_HOOK_DTOR(0x1002c580, NCSliderBar)

// 0x1002c5d0 (ret 0xc) - the three textures by name
NCSliderBar::NCSliderBar(const TCHAR* TopTexName, const TCHAR* MidTexName, const TCHAR* BottomTexName)
{
    ThumbSize = 0;
    ThumbPos = 0;
    GrabOffset = 0;
    Value = 0;
    Page = 0;
    bDragging = 0;
    ThumbTopTex = FindTexture(TopTexName, 1);
    ThumbMidTex = FindTexture(MidTexName, 1);
    ThumbBottomTex = FindTexture(BottomTexName, 1);
}
NW_HOOK_CTOR_ARGS(0x1002c5d0, NCSliderBar, (const TCHAR* A, const TCHAR* B, const TCHAR* C), (A, B, C))


// stock body shared with NCButton's (0x100051f0)
INT NCSliderBar::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return NControl::WindowProc(Msg, wParam, lParam) != 0;
}

// 0x1002c360 (vslot 62) - the thumb only: a 15 x 8 cap on each end and the stretched middle between them
INT NCSliderBar::OnPaint(NCGDevice* Device)
{
    guard(NCSliderBar::OnPaint);
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
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
NW_HOOK_METHOD(0x1002c360, NCSliderBar, OnPaint)

// 0x1002c230 (vslot 67) - the thumb follows the mouse (its grab point stays under it), within the track; the parent is
// told the new Value (NWM_COMMAND). Division by TrackLen as in the stock code
INT NCSliderBar::OnMouseMove(WPARAM wParam, LPARAM lParam)
{
    if ((wParam & MK_LBUTTON) && bDragging)
    {
        INT Y = (SWORD)HIWORD(lParam) - ScreenY;
        INT Delta = Y - GrabOffset - ThumbPos;
        if (Delta != 0)
        {
            if (ThumbPos <= 0)
            {
                if (GrabOffset > Y)
                    return 1;
            }
            else if (ThumbPos >= TrackLen && Y > ThumbPos + GrabOffset)
                return 1;
        }
        if (Delta > 0)
        {
            if (ThumbPos >= TrackLen)
                return 1;
            ThumbPos += Delta;
            if (ThumbPos > TrackLen)
                ThumbPos = TrackLen;
            Value = (Total - Page) * ThumbPos / TrackLen;
        }
        else
        {
            if (Delta >= 0 || ThumbPos <= 0)
                return 1;
            ThumbPos += Delta;
            if (ThumbPos < 0)
                ThumbPos = 0;
            Value = (Total - Page) * ThumbPos / TrackLen;
        }
        NotifyParent(Value);
    }
    return 1;
}
NW_HOOK_METHOD(0x1002c230, NCSliderBar, OnMouseMove)

// 0x1002c1c0 (vslot 68) - grabs the thumb (where it was grabbed is kept); only a bar with something to scroll drags
INT NCSliderBar::OnLButtonDown(WPARAM wParam, LPARAM lParam)
{
    INT Y = (SWORD)HIWORD(lParam) - ScreenY;
    if (ThumbPos <= Y && Y <= ThumbPos + ThumbSize)
    {
        if (Total > Page)
            bDragging = 1;
        GrabOffset = Y - ThumbPos;
    }
    return 1;
}
NW_HOOK_METHOD(0x1002c1c0, NCSliderBar, OnLButtonDown)

// 0x1002c210 (vslot 69)
INT NCSliderBar::OnLButtonUp(WPARAM wParam, LPARAM lParam)
{
    bDragging = 0;
    GrabOffset = 0;
    ReleaseFocus();
    return 1;
}
NW_HOOK_METHOD(0x1002c210, NCSliderBar, OnLButtonUp)

// 0x1002c0a0 - the thumb is the share of the height that the page is of the total (at least 15); a bar with nothing to
// scroll hides itself, one that has shows itself when its parent is shown (and it is not)
void NCSliderBar::SetRange(INT InTotal, INT InPage)
{
    Total = InTotal;
    Page = InPage;
    if (InTotal > InPage)
    {
        ThumbSize = (INT)((DOUBLE)InPage * (DOUBLE)Height / (DOUBLE)InTotal);
        NCWnd* ParentWnd = Entry->Parent->Wnd;
        if (ParentWnd && ParentWnd->IsShowWindow() && !IsShowWindow())
            ShowWindow();
    }
    else
    {
        ThumbSize = (INT)Height;
        HideWindow();
    }
    if (ThumbSize < 15)
        ThumbSize = 15;
    TrackLen = (INT)(Height - (FLOAT)ThumbSize);
}
NW_HOOK_METHOD(0x1002c0a0, NCSliderBar, SetRange)

// 0x1002c150 - the range test looks at the old Value, not the new one (stock behaviour)
void NCSliderBar::SetValue(INT NewValue)
{
    if (Value > Total || Value < 0)
    {
        Value = 0;
        return;
    }
    Value = NewValue;
    if (Total > Page)
        ThumbPos = TrackLen * NewValue / (Total - Page);
    else
        ThumbPos = 0;
}
NW_HOOK_METHOD(0x1002c150, NCSliderBar, SetValue)

// 0x1002c330 - returns 0 (eax is the zero the stores used: NCListCtrl's callers read it)
INT NCSliderBar::Reset()
{
    ThumbPos = 0;
    Value = 0;
    Page = 0;
    ThumbSize = 0;
    GrabOffset = 0;
    bDragging = 0;
    return 0;
}
NW_HOOK_METHOD(0x1002c330, NCSliderBar, Reset)

// 0x1002c4c0 (ret 8) - the stock is a 5-byte `jmp SetRange` (the thunk the scroll windows and the tree controls call)
void NCSliderBar::SetScrollRange(INT InTotal, INT InPage)
{
    SetRange(InTotal, InPage);
}
NW_HOOK_METHOD(0x1002c4c0, NCSliderBar, SetScrollRange)

// 0x1002c4d0 (ret 4) - the stock is a 5-byte `jmp SetValue`
void NCSliderBar::SetScrollPos(INT NewValue)
{
    SetValue(NewValue);
}
NW_HOOK_METHOD(0x1002c4d0, NCSliderBar, SetScrollPos)

NW_CHECK_VTABLE(NCSliderBar, 0x101a6368, 75)
