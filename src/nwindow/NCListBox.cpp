/*=============================================================================
    NCListBox.cpp - scrolling list of text lines. Stock (C3): 0x1001e4b0 .. 0x1001f3aa.
=============================================================================*/
#include "nwindow/NCListBox.h"
#include "nwindow/NCButton.h"
#include "nwindow/NCSliderBar.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NConsoleWnd.h"
#include "nwindow/NString.h"

// 0x100218e0 - NControl's constructor is inlined; the lists are 10 lines of at most 100
NCListBox::NCListBox()
{
    BackTex = NULL;
    HighlightTex = NULL;
    ScrollUpButton = NULL;
    ScrollDownButton = NULL;
    ScrollBar = NULL;
    TopIndex = 0;
    NumLines = 0;
    LineGap = 0;
    bAutoScroll = 0;
    TextX = 0;
    TextY = 0;
    MaxLines = 100;
    VisibleLines = 10;
    Selected = -1;
}
NW_HOOK_CTOR(0x100218e0, NCListBox)

// 0x10021980 - the third argument is never read
NCListBox::NCListBox(INT InMaxLines, INT InVisibleLines, INT Unused)
{
    BackTex = NULL;
    HighlightTex = NULL;
    MaxLines = InMaxLines;
    ScrollUpButton = NULL;
    ScrollDownButton = NULL;
    ScrollBar = NULL;
    TopIndex = 0;
    NumLines = 0;
    LineGap = 0;
    bAutoScroll = 0;
    TextX = 0;
    TextY = 0;
    VisibleLines = InVisibleLines;
    Selected = -1;
}
NW_HOOK_CTOR_ARGS(0x10021980, NCListBox, (INT A, INT B, INT C), (A, B, C))


// 0x100204e0 (vslot 25) - its own buttons and slider report here
INT NCListBox::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NControl::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == WM_LBUTTONDOWN)
        return OnLButtonDown(wParam, lParam) != 0;
    if (Msg == WM_LBUTTONDBLCLK)
        return OnLButtonDblClick(wParam, lParam) != 0;
    if (Msg == NWM_COMMAND)
    {
        if ((INT)wParam == NID_LIST_SCROLLUP)
            return ScrollUp() != 0;
        if ((INT)wParam == NID_LIST_SCROLLDOWN)
            return ScrollDown() != 0;
        if ((INT)wParam == NID_LIST_SCROLLBAR)
            return SetTopIndex((INT)lParam) != 0;
    }
    return 0;
}
NW_HOOK_METHOD(0x100204e0, NCListBox, WindowProc)

// 0x10020d80 (vslot 37) - up / down buttons (15 x 15) and the slider along the right edge, positions from the
// local rectangle (inside it). C4: the highlight is the list's selection bar (two textures)
INT NCListBox::OnCreate()
{
    NControl::OnCreate();
    HighlightTex = FindTexture(TEXT("L2UI_CH3.ListCtrl.TextSelect"), 1);
    HighlightTex2 = FindTexture(TEXT("L2UI_CH3.ListCtrl.TextSelect2"), 1);
    LineTex = FindTexture(TEXT("L2UI_ch3.etc.textbackline"), 1);
    NRectF Rect;
    Rect.X = 0;
    Rect.Y = 0;
    Rect.W = 0.0f;
    Rect.H = 0.0f;
    GetLocalRect(&Rect);

    ScrollUpButton = new NCPushButton(TEXT("L2UI_CH3.ScrollBar.ScrollBarUpBtn"),
                                      TEXT("L2UI_CH3.ScrollBar.ScrollBarUpOnBtn"));
    ScrollUpButton->ID = NID_LIST_SCROLLUP;
    ScrollUpButton->SetPosition((INT)(Rect.W - 15.0f), 0, 15, 15);
    CreateChild(ScrollUpButton, NWS_VISIBLE);

    ScrollDownButton = new NCPushButton(TEXT("L2UI_CH3.ScrollBar.ScrollBarDownBtn"),
                                        TEXT("L2UI_CH3.ScrollBar.ScrollBarDownOnBtn"));
    ScrollDownButton->ID = NID_LIST_SCROLLDOWN;
    ScrollDownButton->SetPosition((INT)(Rect.W - 15.0f), (INT)(Rect.H - 15.0f), 15, 15);
    CreateChild(ScrollDownButton, NWS_VISIBLE);

    ScrollBar = new NCSliderBar(TEXT("L2UI_CH3.ScrollBar.SliderBarTop"), TEXT("L2UI_CH3.ScrollBar.SliderBarCenter"),
                                TEXT("L2UI_CH3.ScrollBar.SliderBarBottom"));
    ScrollBar->ID = NID_LIST_SCROLLBAR;
    ScrollBar->SetPosition((INT)(Rect.W - 15.0f), 15, 15, (INT)(Rect.H - 30.0f));
    CreateChild(ScrollBar, NWS_VISIBLE);

    InitScrollBar();
    return 1;
}
NW_HOOK_METHOD(0x10020d80, NCListBox, OnCreate)

// 0x10021850 - the lines (TListItemArray, 0x100217c0) go, then NControl's destructor
NW_HOOK_DTOR(0x10021850, NCListBox)

// 0x10021060 (vslot 38) - the controls, then the lines
INT NCListBox::OnDestroy()
{
    guard(NCListBox::OnDestroy);
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
    for (INT i = 0; i < Items.Num(); i++)
    {
        NListItem Item = Items(i);
        if (Item.Text)
            delete Item.Text;
    }
    Items.Empty();
    NControl::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10021060, NCListBox, OnDestroy)

// 0x100208f0 (vslot 62) - the selected line gets a grey bar (the width less 16 and 2) and light grey text; the
// others their own color
INT NCListBox::OnPaint(NCGDevice* Device)
{
    guard(NCListBox::OnPaint);
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    if (BackTex)
        Device->DrawTexture(0, 0, (INT)Width, (INT)Height, 0, 0, (INT)Width, (INT)Height, BackTex, 0xFF, 1);
    INT Line = 0;
    for (INT i = TopIndex; i < VisibleLines + TopIndex && i <= NumLines && i < Items.Num(); i++, Line++)
    {
        NListItem Item = Items(i);
        if (i == Selected)
        {
            Device->DrawRect(0, (LineGap + 12) * Line, (INT)(Width - 16.0f - 2.0f), LineGap + 12, 0xFF646464);
            Device->DrawText(TextX, (LineGap + 12) * Line + TextY, 0xFFDCDCDC, Item.Text->GetAt(0), 0, 0, 0,
                             L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
        }
        else
        {
            DWORD Color = Item.Text->Color;
            Device->DrawText(TextX, (LineGap + 12) * Line + TextY, Color, Item.Text->GetAt(0), 0, 0, 0,
                             L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
        }
    }
    Device->ReleaseClip();
    Device->EndPaint();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100208f0, NCListBox, OnPaint)

// 0x100206a0 (vslot 68)
INT NCListBox::OnLButtonDown(WPARAM wParam, LPARAM lParam)
{
    Selected = HitTest(LOWORD(lParam), HIWORD(lParam));
    if (Selected >= 0 && Selected < Items.Num())
        NotifyParent(Selected);
    return 1;
}
NW_HOOK_METHOD(0x100206a0, NCListBox, OnLButtonDown)

// 0x100206e0 (vslot 73) - one line per wheel notch (120)
INT NCListBox::OnMouseWheel(WPARAM wParam, LPARAM lParam)
{
    SWORD Notches = (SWORD)((SWORD)HIWORD(wParam) / 120);
    INT Delta = Notches;
    if (Notches > 0)
    {
        if (TopIndex > Delta)
            TopIndex -= Delta;
        else
            TopIndex = 0;
    }
    else if (NumLines >= VisibleLines)
    {
        if (NumLines - TopIndex + Delta > VisibleLines)
            TopIndex -= Delta;
        else
            TopIndex = NumLines - VisibleLines;
    }
    else
    {
        if (NumLines - TopIndex + Delta > VisibleLines)
            TopIndex -= Delta;
        else
            TopIndex = 0;
    }
    ScrollBar->SetValue(TopIndex);
    return 1;
}
NW_HOOK_METHOD(0x100206e0, NCListBox, OnMouseWheel)

// 0x100205c0 (vslot 75) - selects the line under the point; returns Selected (unchanged when no line is hit) or
// -1 outside the box
INT NCListBox::HitTest(INT X, INT Y)
{
    INT RX = X - ScreenX;
    INT RY = Y - ScreenY;
    if (RX < 0)
        return -1;
    INT W = (INT)Width;
    if (RX > W)
        return -1;
    if (RY < 0)
        return -1;
    if (RY > (INT)Height)
        return -1;
    INT Line = 0;
    for (INT i = TopIndex; i < VisibleLines + TopIndex && i < NumLines; i++, Line++)
    {
        if (RX < W && RY >= (LineGap + 12) * Line + TextY && RY < (Line + 1) * (LineGap + 12) + TextY)
        {
            Selected = i;
            return Selected;
        }
    }
    return Selected;
}
NW_HOOK_METHOD(0x100205c0, NCListBox, HitTest)

// 0x10021260 (vslot 70) - when full, the oldest line is dropped
void NCListBox::AddString(const TCHAR* Text, DWORD Color, INT Param)
{
    NString* Str = new NString(Text);
    Str->Color = Color;
    if (NumLines < MaxLines)
    {
        NumLines++;
        if (bAutoScroll && NumLines > VisibleLines)
            TopIndex++;
    }
    else
    {
        NString* Oldest = Items(0).Text;
        if (Oldest)
        {
            delete Oldest;
            Items(0).Text = NULL;
        }
        Items.Remove(0, 1);
    }
    INT i = Items.Add();
    Items(i).Text = Str;
    Items(i).Param = Param;
    Items(i).Unknown8 = 0;
    UpdateScrollBar();
}
NW_HOOK_METHOD(0x10021260, NCListBox, AddString)

// 0x100211c0 (vslot 77)
void NCListBox::Clear()
{
    for (INT i = 0; i < Items.Num(); i++)
    {
        NString* Text = Items(i).Text;
        if (Text)
            delete Text;
    }
    Items.Empty();
    ScrollBar->Reset();
    TopIndex = 0;
    NumLines = 0;
    Selected = -1;
}
NW_HOOK_METHOD(0x100211c0, NCListBox, Clear)

// 0x1000bd60 (vslot 72) - folded `return 1`, hooked as NCIcon::OnLButtonDblClick

// 0x10020790 (vslot 79)
INT NCListBox::ScrollUp()
{
    if (NumLines > VisibleLines)
    {
        if (--TopIndex < 0)
            TopIndex = 0;
        ScrollBar->SetValue(TopIndex);
    }
    return 1;
}
NW_HOOK_METHOD(0x10020790, NCListBox, ScrollUp)

// 0x100207d0 (vslot 80)
INT NCListBox::ScrollDown()
{
    if (NumLines > VisibleLines)
    {
        INT Last = NumLines - VisibleLines;
        if (++TopIndex > Last)
            TopIndex = Last;
        ScrollBar->SetValue(TopIndex);
    }
    return 1;
}
NW_HOOK_METHOD(0x100207d0, NCListBox, ScrollDown)

// 0x10020820 (vslot 81) - from the slider
INT NCListBox::SetTopIndex(INT Index)
{
    TopIndex = Index;
    return 1;
}
NW_HOOK_METHOD(0x10020820, NCListBox, SetTopIndex)

// 0x10020840 - the range follows the lines (kept at its old value while there are none)
void NCListBox::UpdateScrollBar()
{
    if (Items.Num() > 0)
        NumLines = Items.Num();
    ScrollBar->SetRange(NumLines, VisibleLines);
    ScrollBar->SetValue(TopIndex);
}
NW_HOOK_METHOD(0x10020840, NCListBox, UpdateScrollBar)

// 0x10020b30 - the scroll buttons are created without auto-repeat, so this has no effect
void NCListBox::SetRepeatDelay(INT Delay)
{
    if (ScrollUpButton && ScrollDownButton)
    {
        ScrollUpButton->RepeatDelay = Delay;
        ScrollDownButton->RepeatDelay = Delay;
    }
}
NW_HOOK_METHOD(0x10020b30, NCListBox, SetRepeatDelay)

// 0x10020b60 - the range uses TopIndex as the total (0 at creation), as in the stock code
void NCListBox::InitScrollBar()
{
    SetRepeatDelay(200);
    ScrollBar->SetRange(TopIndex, VisibleLines);
    ScrollBar->SetValue(TopIndex);
}
NW_HOOK_METHOD(0x10020b60, NCListBox, InitScrollBar)

// 0x10020ba0 (ret 4) - -1 past the last line (a negative index is not looked at)
INT NCListBox::GetItemParam(INT Index)
{
    if (Index >= Items.Num())
        return -1;
    return Items(Index).Param;
}
NW_HOOK_METHOD(0x10020ba0, NCListBox, GetItemParam)

// 0x10020880 (vslot 44) - C3: shown, the scroll bar follows the lines held
void NCListBox::ShowWindow()
{
    NCWnd::ShowWindow();
    UpdateScrollBar();
}
NW_HOOK_METHOD(0x10020880, NCListBox, ShowWindow)

NW_CHECK_VTABLE(NCListBox, 0x101a5968, 82)
