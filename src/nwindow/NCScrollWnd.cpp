/*=============================================================================
    NCScrollWnd.cpp - vertically scrolling window. Stock (C3): 0x10028ab0 .. 0x100295f8 (the constructor 0x100294f0,
    the controls' thunks 0x10029970 / 0x10029980 jump into NCSliderBar), OnPaint (0x10003360, folded).
=============================================================================*/
#include "nwindow/NCScrollWnd.h"
#include "nwindow/NCButton.h"
#include "nwindow/NCSliderBar.h"

// 0x1002c040 - the controls and MinScrollY come with OnCreate / SetContentHeight
NCScrollWnd::NCScrollWnd()
{
    ScrollY = 0;
    ScrollX = 0;
    DeltaY = 0;
    DeltaX = 0;
    LineStep = 15;
    Unknown0C8 = 15;
    PageStep = 100;
}
NW_HOOK_CTOR(0x1002c040, NCScrollWnd)

// 0x10015950 - nothing of its own to free (the controls are removed by OnDestroy)
NW_HOOK_DTOR(0x10015950, NCScrollWnd)

// 0x1002b5a0 (vslot 25) - its own controls report here
INT NCScrollWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCWnd::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND)
    {
        if ((WPARAM)ScrollUpID == wParam)
            return ScrollUp() != 0;
        if ((WPARAM)ScrollDownID == wParam)
            return ScrollDown() != 0;
        if ((WPARAM)ScrollBarID == wParam)
            return OnScrollBar((INT)lParam) != 0;
    }
    return 0;
}
NW_HOOK_METHOD(0x1002b5a0, NCScrollWnd, WindowProc)

// 0x1002bd60 (vslot 37) - up / down buttons and the slider along the right edge (C3: 15 wide, the CH3 textures, the
// size from GetLocalRect)
INT NCScrollWnd::OnCreate()
{
    NCWnd::OnCreate();
    NRectF R;
    R.X = 0;
    R.Y = 0;
    R.W = 0.0f;
    R.H = 0.0f;
    GetLocalRect(&R);

    ScrollUpButton = new NCPushButton(TEXT("L2UI_CH3.ScrollBar.ScrollBarUpBtn"),
                                      TEXT("L2UI_CH3.ScrollBar.ScrollBarUpOnBtn"));
    ScrollUpID = GNextScrollCtrlID++;
    ScrollUpButton->ID = ScrollUpID;
    ScrollUpButton->SetPosition((INT)(R.W - 15.0f), 0, 15, 15);
    CreateChild(ScrollUpButton, NWS_VISIBLE);

    ScrollDownButton = new NCPushButton(TEXT("L2UI_CH3.ScrollBar.ScrollBarDownBtn"),
                                        TEXT("L2UI_CH3.ScrollBar.ScrollBarDownOnBtn"));
    ScrollDownID = GNextScrollCtrlID++;
    ScrollDownButton->ID = ScrollDownID;
    ScrollDownButton->SetPosition((INT)(R.W - 15.0f), (INT)(R.H - 15.0f), 15, 15);
    CreateChild(ScrollDownButton, NWS_VISIBLE);

    ScrollBar = new NCSliderBar(TEXT("L2UI_CH3.ScrollBar.SliderBarTop"), TEXT("L2UI_CH3.ScrollBar.SliderBarCenter"),
                                TEXT("L2UI_CH3.ScrollBar.SliderBarBottom"));
    ScrollBarID = GNextScrollCtrlID++;
    ScrollBar->ID = ScrollBarID;
    ScrollBar->SetPosition((INT)(R.W - 15.0f), 15, 15, (INT)(R.H - 30.0f));
    CreateChild(ScrollBar, NWS_VISIBLE);
    return 1;
}
NW_HOOK_METHOD(0x1002bd60, NCScrollWnd, OnCreate)

// 0x1002b880 (vslot 38) - C2 guards it
INT NCScrollWnd::OnDestroy()
{
    guard(NCScrollWnd::OnDestroy);
    if (ScrollUpButton)
    {
        RemoveChildWindow(ScrollUpButton->Entry);
        delete ScrollUpButton;
        ScrollUpButton = NULL;
    }
    if (ScrollDownButton)
    {
        RemoveChildWindow(ScrollDownButton->Entry);
        delete ScrollDownButton;
        ScrollDownButton = NULL;
    }
    if (ScrollBar)
    {
        RemoveChildWindow(ScrollBar->Entry);
        delete ScrollBar;
        ScrollBar = NULL;
    }
    NCWnd::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1002b880, NCScrollWnd, OnDestroy)

// C2 CANDIDATE: the C2 function is not the same code as the C1 one; kept only while it matches the stock one (verify_data)
// 0x1002b640 (vslot 68, name ours) - the size (from unsigned numbers), then the controls follow the right edge (size
// 0 x 0 keeps theirs)
void NCScrollWnd::SetSize(DWORD W, DWORD H)
{
    NCWnd::SetWindowSize((FLOAT)W, (FLOAT)H);
    ScrollUpButton->SetPosition(W - 15, 0, 0, 0);
    ScrollDownButton->SetPosition(W - 15, H - 15, 0, 0);
    ScrollBar->SetPosition(W - 15, 15, 15, H - 30);
}
NW_HOOK_METHOD(0x1002b640, NCScrollWnd, SetSize)

// 0x100f76f0 (vslot 62) - `return 1` shared with NCCandidateBox and others
NW_HOOK_METHOD(0x100f76f0, NCScrollWnd, OnPaint)

// 0x1002b980 (vslot 65) - the base handler runs twice, as in the stock code
INT NCScrollWnd::OnKeyDown(WPARAM wParam, LPARAM lParam)
{
    DeltaX = ScrollX;
    INT OldY = ScrollY;
    DeltaY = OldY;
    UBOOL bHandled = 0;
    switch (wParam)
    {
    case VK_PRIOR:
        if (ScrollY < 0)
        {
            ScrollY += PageStep;
            if (ScrollY > 0)
                ScrollY = 0;
        }
        bHandled = 1;
        break;
    case VK_NEXT:
        if (ScrollY > MinScrollY)
        {
            ScrollY -= PageStep;
            if (ScrollY < MinScrollY)
                ScrollY = MinScrollY;
        }
        bHandled = 1;
        break;
    case VK_UP:
        if (ScrollY < 0)
        {
            ScrollY += LineStep;
            if (ScrollY > 0)
                ScrollY = 0;
        }
        bHandled = 1;
        break;
    case VK_DOWN:
        if (ScrollY > MinScrollY)
        {
            ScrollY -= LineStep;
            if (ScrollY < MinScrollY)
                ScrollY = MinScrollY;
        }
        bHandled = 1;
        break;
    }
    if (ScrollY != OldY)
        ScrollBar->SetScrollPos(-ScrollY);
    DeltaY -= ScrollY;
    DeltaX -= ScrollX;
    NCWnd::OnKeyDown(wParam, lParam);
    return NCWnd::OnKeyDown(wParam, lParam) ? 1 : bHandled;
}
NW_HOOK_METHOD(0x1002b980, NCScrollWnd, OnKeyDown)

// 0x1002bc60 (vslot 73) - three lines per wheel notch (120)
INT NCScrollWnd::OnMouseWheel(WPARAM wParam, LPARAM lParam)
{
    SWORD Notches = (SWORD)((SWORD)HIWORD(wParam) / 120);
    DeltaX = ScrollX;
    DeltaY = ScrollY;
    if (Notches > 0)
    {
        if (ScrollY < 0)
        {
            ScrollY += LineStep * Notches * 3;
            if (ScrollY > 0)
                ScrollY = 0;
            ScrollBar->SetScrollPos(-ScrollY);
        }
    }
    else if (ScrollY > MinScrollY)
    {
        ScrollY += LineStep * Notches * 3;
        if (ScrollY < MinScrollY)
            ScrollY = MinScrollY;
        ScrollBar->SetScrollPos(-ScrollY);
    }
    DeltaX -= ScrollX;
    DeltaY -= ScrollY;
    return 1;
}
NW_HOOK_METHOD(0x1002bc60, NCScrollWnd, OnMouseWheel)

// 0x1002bac0 (vslot 75)
INT NCScrollWnd::ScrollUp()
{
    if (ScrollY < 0)
    {
        DeltaX = ScrollX;
        DeltaY = ScrollY;
        ScrollY += LineStep;
        if (ScrollY > 0)
            ScrollY = 0;
        ScrollBar->SetScrollPos(-ScrollY);
        DeltaX -= ScrollX;
        DeltaY -= ScrollY;
        return 1;
    }
    DeltaY = 0;
    return 1;
}
NW_HOOK_METHOD(0x1002bac0, NCScrollWnd, ScrollUp)

// 0x1002bb50 (vslot 76)
INT NCScrollWnd::ScrollDown()
{
    if (ScrollY > MinScrollY)
    {
        DeltaX = ScrollX;
        DeltaY = ScrollY;
        ScrollY -= LineStep;
        if (ScrollY < MinScrollY)
            ScrollY = MinScrollY;
        ScrollBar->SetScrollPos(-ScrollY);
        DeltaX -= ScrollX;
        DeltaY -= ScrollY;
        return 1;
    }
    DeltaY = 0;
    return 1;
}
NW_HOOK_METHOD(0x1002bb50, NCScrollWnd, ScrollDown)

// 0x1002bbf0 (vslot 77) - the slider's value is the scroll position, rounded up to a whole line step (C3)
INT NCScrollWnd::OnScrollBar(INT Value)
{
    DeltaX = ScrollX;
    DeltaY = ScrollY;
    INT Step = LineStep;
    ScrollY = -(((Step - 1 + Value) / Step) * Step);
    DeltaX -= ScrollX;
    DeltaY -= ScrollY;
    return 1;
}
NW_HOOK_METHOD(0x1002bbf0, NCScrollWnd, OnScrollBar)

// 0x1002b750 - MinScrollY = Height - content height (0 when it all fits); C3: Height is a FLOAT
void NCScrollWnd::SetContentHeight(INT ContentHeight)
{
    INT H = (INT)Height;
    if (ContentHeight > H)
    {
        MinScrollY = (INT)(Height - (FLOAT)ContentHeight);
        ScrollBar->SetScrollRange(ContentHeight, H);
    }
    else
    {
        MinScrollY = 0;
        ScrollBar->SetScrollRange(0, H);
    }
}
NW_HOOK_METHOD(0x1002b750, NCScrollWnd, SetContentHeight)

// 0x1002b7b0 (name ours) - when the thumb (size + position, +0x1F4 / +0x1F8 of the bar) already passes the window's
// height less 30, the content is dropped: the slider's own handler runs with 0 and the bar goes back to 0; else the
// range is set from the whole content height (the shorter case gets the real height too, unlike SetContentHeight)
void NCScrollWnd::SetContentHeightEx(INT ContentHeight)
{
    NCSliderBar* Bar = ScrollBar;
    INT H = (INT)Height;
    INT Size = Bar->ThumbSize;
    if (Bar->ThumbPos + Size > H - 30)
    {
        OnScrollBar(0);
        MinScrollY = 0;
        Bar->SetScrollPos(0);
        return;
    }
    if (ContentHeight > H)
    {
        MinScrollY = (INT)(Height - (FLOAT)ContentHeight);
        Bar->SetScrollRange(ContentHeight, H);
    }
    else
    {
        MinScrollY = 0;
        Bar->SetScrollRange(ContentHeight, H);
    }
}
NW_HOOK_METHOD(0x1002b7b0, NCScrollWnd, SetContentHeightEx)

// 0x1002b6e0 (name ours) - the slider and both scroll buttons follow one flag: shown, or hidden
void NCScrollWnd::ShowBars(INT bShow)
{
    if (bShow)
    {
        ScrollBar->ShowWindow();
        ScrollUpButton->ShowWindow();
        ScrollDownButton->ShowWindow();
        return;
    }
    ScrollBar->HideWindow();
    ScrollUpButton->HideWindow();
    ScrollDownButton->HideWindow();
}
NW_HOOK_METHOD(0x1002b6e0, NCScrollWnd, ShowBars)

// 0x1002b850 - the scroll buttons are created without auto-repeat (as NCListBox's)
void NCScrollWnd::SetRepeatDelay(INT Delay)
{
    if (ScrollUpButton && ScrollDownButton)
    {
        ScrollUpButton->RepeatDelay = Delay;
        ScrollDownButton->RepeatDelay = Delay;
    }
}
NW_HOOK_METHOD(0x1002b850, NCScrollWnd, SetRepeatDelay)

NW_CHECK_VTABLE(NCScrollWnd, 0x101a21f8, 78)
