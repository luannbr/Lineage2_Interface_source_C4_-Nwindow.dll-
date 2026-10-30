/*=============================================================================
    NCCandidateBox.cpp - IME candidate list. Stock: 0x100025a0 .. 0x100029d2,
    0x10002ac0 .. 0x1000336f, 0x10003a50 .. 0x10003afd, 0x10003b00 .. 0x10003d61 (the
    TArray<NImeText> destructor at 0x10003bb0 is the template's).
=============================================================================*/
#include "nwindow/NCCandidateBox.h"
#include "nwindow/NCButton.h"
#include "nwindow/NCSliderBar.h"
#include "nwindow/NCGDevice.h"

// 0x10004f50
NCCandidateBox::NCCandidateBox(INT InPageSize, INT InRowHeight)
{
    Selected = 0;
    PageStart = 0;
    UnknownE0 = 0;
    UnknownE8 = 0;
    Mode = 0;
    PageSize = InPageSize;
    RowHeight = InRowHeight;
    TextColor = 0xFF000000;
}
NW_HOOK_CTOR_ARGS(0x10004f50, NCCandidateBox, (INT A, INT B), (A, B))

// 0x10004ff0
NW_HOOK_DTOR(0x10004ff0, NCCandidateBox)

// 0x10003cc0 (vslot 25) - C3: only the mouse buttons are its own (the scroll controls are gone)
INT NCCandidateBox::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCWnd::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == WM_LBUTTONDOWN)
        return OnLButtonDown(wParam, lParam) != 0;
    if (Msg == WM_LBUTTONUP)
        return OnLButtonUp(wParam, lParam) != 0;
    return 0;
}
NW_HOOK_METHOD(0x10003cc0, NCCandidateBox, WindowProc)

// C2 CANDIDATE: the C2 function is not the same code as the C1 one; kept only while it matches the stock one (verify_data)
// 0x10003d40 (vslot 37) - C3: only the frame textures and the selection bar; no scroll controls any more
INT NCCandidateBox::OnCreate()
{
    NCWnd::OnCreate();
    LeftEnable1 = FindTexture(TEXT("L2UI.CandidateWnd.left_enable1"));
    LeftDisable1 = FindTexture(TEXT("L2UI.CandidateWnd.left_disable1"));
    LeftEnable2 = FindTexture(TEXT("L2UI.CandidateWnd.left_enable2"));
    LeftDisable2 = FindTexture(TEXT("L2UI.CandidateWnd.left_disable2"));
    Center1 = FindTexture(TEXT("L2UI.CandidateWnd.center1"));
    Center2 = FindTexture(TEXT("L2UI.CandidateWnd.center2"));
    Right1 = FindTexture(TEXT("L2UI.CandidateWnd.right1"));
    Right2 = FindTexture(TEXT("L2UI.CandidateWnd.right2"));
    WhiteTex = FindTexture(TEXT("NWindow.WhiteTexture"));
    return 1;
}
NW_HOOK_METHOD(0x10003d40, NCCandidateBox, OnCreate)

// 0x10004e10 (vslot 38) - the candidates go (C3: nothing else is owned)
INT NCCandidateBox::OnDestroy()
{
    Items.Empty();
    NCWnd::OnDestroy();
    return 1;
}
NW_HOOK_METHOD(0x10004e10, NCCandidateBox, OnDestroy)

// 0x10003f90 (vslot 43) - the point is inside the rectangle, edges included (the right and bottom edges as
// floats)
UBOOL NCCandidateBox::PtInRect(INT X, INT Y, NRectF Rect)
{
    if (X < Rect.X || (FLOAT)X > (FLOAT)Rect.X + Rect.W)
        return 0;
    if (Y < Rect.Y || (FLOAT)Y > (FLOAT)Rect.Y + Rect.H)
        return 0;
    return 1;
}
NW_HOOK_METHOD(0x10003f90, NCCandidateBox, PtInRect)

// 0x10003fe0 (vslot 68; name ours) - the box is at least 70 px wide: W and H in px
void NCCandidateBox::SetSize(INT W, INT H)
{
    if ((UINT)W < 70)
        Width = 70.0f;
    else
        Width = (FLOAT)(UINT)W;
    Height = (FLOAT)H;
}
NW_HOOK_METHOD(0x10003fe0, NCCandidateBox, SetSize)

// (slot 47, SetWindowSize, is NCWnd's own body, 0x10036dc0, not an override: the stock vtable of this class points at it)

// 0x100efc40 (vslot 48) - the 5-byte body the linker folded for every class that inherits NCWnd::ShowWindow (NCEditBox, NCCandidateBox,
// NCGMMagicSkillWnd, NCRecipeShopWnd, the replay windows...: their stubs jump here): `jmp NCWnd::ShowWindow`
void NCCandidateBox::ShowWindow()
{
    NCWnd::ShowWindow();
}
NW_HOOK_METHOD(0x100efc40, NCCandidateBox, ShowWindow)

// 0x10004190 (vslot 62) - frame for the mode (rows past Count get the "disable" strip), the
// selection bar, then the page. The text of row i is Items(PageStart + i) with i already
// counted from PageStart, as in the stock code, so pages after the first show the wrong
// candidates (or none).
INT NCCandidateBox::OnPaint(NCGDevice* Device)
{
    guard(NCCandidateBox::OnPaint);
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    switch (Mode)
    {
    case 0:
    case 1:
    case 2:
        if (LeftEnable1)
            Device->DrawTexture(0, 0, 11, 136, 0, 0, 11, 136, LeftEnable1);
        if (Count < PageSize && LeftDisable1)
            Device->DrawTexture(1, RowHeight * Count + 1, 9, (PageSize - Count) * RowHeight, 0,
                                RowHeight * Count, 9, (PageSize - Count) * RowHeight, LeftDisable1);
        if (Center1)
            Device->DrawTexture(11, 0, (INT)(Width - 15.0f), 136, 0, 0, 12, 136, Center1);
        if (Right1)
            Device->DrawTexture((INT)(Width - 4.0f), 0, 4, 136, 0, 0, 4, 136, Right1);
        break;
    case 3:
    case 4:
        if (LeftEnable2)
            Device->DrawTexture(0, 0, 11, 151, 0, 0, 11, 151, LeftEnable2);
        if (Count < PageSize && LeftDisable2)
            Device->DrawTexture(1, RowHeight * Count + 1, 9, (PageSize - Count) * RowHeight, 0,
                                RowHeight * Count, 9, (PageSize - Count) * RowHeight, LeftDisable2);
        if (Center2)
            Device->DrawTexture(11, 0, (INT)(Width - 15.0f), 151, 0, 0, 12, 136, Center2);
        if (Right2)
            Device->DrawTexture((INT)(Width - 4.0f), 0, 4, 151, 0, 0, 4, 136, Right2);
        break;
    case 5:
        if (LeftEnable2)
        {
            Device->DrawTexture(0, 0, 11, 1, 0, 0, 11, 1, LeftEnable2);
            Device->DrawTexture(0, 1, 11, 135, 0, 16, 11, 135, LeftEnable2);
            Device->DrawTexture(0, 135, 11, 15, 0, 1, 11, 15, LeftEnable2);
        }
        if (Count < PageSize && LeftDisable2)
        {
            Device->DrawTexture(1, RowHeight * Count + 1, 9, (PageSize - Count - 1) * RowHeight, 0,
                                (Count + 1) * RowHeight, 9, (PageSize - Count - 1) * RowHeight,
                                LeftDisable2);
            Device->DrawTexture(1, (PageSize - 1) * RowHeight + 1, 9, RowHeight, 0, 0, 9, RowHeight,
                                LeftDisable2);
        }
        if (Center2)
            Device->DrawTexture(11, 0, (INT)(Width - 15.0f), 151, 0, 0, 12, 151, Center2);
        if (Right2)
            Device->DrawTexture((INT)(Width - 4.0f), 0, 4, 151, 0, 0, 4, 151, Right2);
        break;
    }

    INT Line = 0;
    for (INT i = PageStart; i < PageStart + PageSize && i <= Count; i++, Line++)
    {
        INT Index = PageStart + i;
        if (i == Selected)
        {
            SIZE Size;
            NCGDevice::GetTextExtent(&Size, Items(Index).Text, 0, 0, 0, 0, 0, L2FONT_Normal);
            Device->DrawTexture(12, Selected % PageSize * RowHeight, Size.cx + 3, RowHeight, 0, 0,
                                Size.cx, RowHeight, WhiteTex);
            if (Items.Num() > PageStart + i)
                Device->DrawText(14, RowHeight * Line + 2, TextColor, Items(PageStart + i).Text, 0,
                                 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
        }
        else if (Items.Num() > Index)
        {
            Device->DrawText(14, RowHeight * Line + 2, 0xFFDCDCDC, Items(Index).Text, 0, 0, 0,
                             L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
        }
    }
    Device->ReleaseClip();
    Device->EndPaint();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10004190, NCCandidateBox, OnPaint)

// 0x10003e90 (vslot 65)
INT NCCandidateBox::OnKeyDown(WPARAM wParam, LPARAM lParam)
{
    switch (wParam)
    {
    case VK_NEXT:
        if (IsShowWindow())
            HideWindow();
        else
            ShowWindow();
        break;
    case VK_PRIOR:
        if (IsShowWindow())
            HideWindow();
        else
            ShowWindow();
        break;
    case VK_UP:
        SelectPrev();
        break;
    case VK_DOWN:
        SelectNext();
        break;
    }
    NCWnd::OnKeyDown(wParam, lParam);
    return 1;
}
NW_HOOK_METHOD(0x10003e90, NCCandidateBox, OnKeyDown)

// 0x100046f0 (vslot 68) - selects the row under the cursor (16-bit coordinates as in the
// stock code); the click is never passed on
INT NCCandidateBox::OnLButtonDown(WPARAM wParam, LPARAM lParam)
{
    _WORD X = (_WORD)(LOWORD(lParam) - (_WORD)ScreenX);
    _WORD Y = (_WORD)(HIWORD(lParam) - (_WORD)ScreenY);
    INT Row = 0;
    for (INT i = PageStart; i < PageStart + PageSize; i++, Row++)
    {
        if (i >= Count)
            break;
        NRectF Rect;
        Rect.X = MarginX;
        Rect.Y = RowHeight * Row + MarginY;
        Rect.W = Width - (FLOAT)(2 * MarginX);
        Rect.H = (FLOAT)RowHeight;
        if (PtInRect(X, Y, Rect))
        {
            Selected = i;
            break;
        }
    }
    return 1;
}
NW_HOOK_METHOD(0x100046f0, NCCandidateBox, OnLButtonDown)

// 0x10003f60 (vslot 69)
INT NCCandidateBox::OnLButtonUp(WPARAM wParam, LPARAM lParam)
{
    if (IsShowWindow())
        HideWindow();
    return 1;
}
NW_HOOK_METHOD(0x10003f60, NCCandidateBox, OnLButtonUp)

// 0x10003e10 - moving up past the top of a page shows the previous page
void NCCandidateBox::SelectPrev()
{
    if (Selected <= 0 || --Selected == 0)
        PageStart = 0;
    else if (Selected % PageSize == PageSize - 1)
        PageStart = Selected - PageSize + 1;
}
NW_HOOK_METHOD(0x10003e10, NCCandidateBox, SelectPrev)

// 0x10003e60 - moving down past the bottom of a page shows the next one
void NCCandidateBox::SelectNext()
{
    INT Next = Selected + 1;
    if (Next < Count)
    {
        Selected = Next;
        if (Next % PageSize == 0)
            PageStart = Next;
    }
}
NW_HOOK_METHOD(0x10003e60, NCCandidateBox, SelectNext)

// 0x10003f40 (vslot 69; new in C3, name ours) - the previous candidate (the up key's slot of the IMEs)
INT NCCandidateBox::OnSelectPrev()
{
    SelectPrev();
    return 1;
}
NW_HOOK_METHOD(0x10003f40, NCCandidateBox, OnSelectPrev)

// 0x10003f50 (vslot 70; new in C3, name ours) - the next candidate
INT NCCandidateBox::OnSelectNext()
{
    SelectNext();
    return 1;
}
NW_HOOK_METHOD(0x10003f50, NCCandidateBox, OnSelectNext)

// 0x10004020 (a 3-byte `ret 8` shared by many classes: the empty body of a function of two arguments; the IMEs call it, nothing happens)
void NCCandidateBox::SetScroll(INT Total, INT Value)
{
}

// 0x10004030 - the frame layout; modes 2..4 make the box 150 pixels high, the others 135
void NCCandidateBox::SetMode(INT InMode)
{
    Mode = InMode;
    if (InMode >= 2 && InMode <= 4)
        CallSetSize(100, 150);
    else
        CallSetSize(100, 135);
}
NW_HOOK_METHOD(0x10004030, NCCandidateBox, SetMode)

// 0x10004d60
void NCCandidateBox::AddItem(const TCHAR* Text, INT Flag)
{
    NImeText Item;
    Item.Text[0] = 0;
    Item.Flag = 0;
    if (Text)
        wcscpy(Item.Text, Text);
    Items(Items.Add()) = Item;
    Count++;
}
NW_HOOK_METHOD(0x10004d60, NCCandidateBox, AddItem)

// 0x10004de0
void NCCandidateBox::Clear()
{
    Items.Empty();
    Count = 0;
}
NW_HOOK_METHOD(0x10004de0, NCCandidateBox, Clear)

NW_CHECK_VTABLE(NCCandidateBox, 0x1019f8a8, 78)
