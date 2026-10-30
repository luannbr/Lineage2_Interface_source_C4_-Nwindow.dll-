/*=============================================================================
    NCMatchListCtrl.cpp - the list of the party matching window. C3 stock: 0x100a4c10 (WindowProc), 0x100a50c0
    (ShowWindow), 0x100a5720 (OnLButtonDblClick), 0x100a7b30 / 0x100a7bd0 (constructors), 0x100a7b80 (destructor).
=============================================================================*/
#include "nwindow/NCMatchListCtrl.h"
#include "nwindow/NCButton.h"
#include "nwindow/NCSliderBar.h"
#include "nwindow/NConsoleWnd.h"
#include "nwindow/NCConsole.h"
#include "nwindow/NWinBStock.h"

// 0x100b4460 - NCListCtrl's with the scroll controls (bNoScrollBar 0), then this class' vtable
NCMatchListCtrl::NCMatchListCtrl()
    : NCListCtrl(0)
{
}
NW_HOOK_CTOR(0x100b4460, NCMatchListCtrl)

// 0x100b4500 (ret 0x10; wave C: it was a forwarder to the stock constructor) - NCListCtrl's with the four numbers (the stock pushes 0 and 0x11 after
// them: bNoScrollBar and the row height, the header's defaults), then this class' vtable
NCMatchListCtrl::NCMatchListCtrl(INT InMaxRows, INT InVisibleRows, INT InMaxColumns, INT Unused)
    : NCListCtrl(InMaxRows, InVisibleRows, InMaxColumns, Unused, 0)
{
}
NW_HOOK_CTOR_ARGS(0x100b4500, NCMatchListCtrl, (INT A, INT B, INT C, INT D), (A, B, C, D))

// 0x100b44b0
NW_HOOK_DTOR(0x100b44b0, NCMatchListCtrl)

// 0x100b1460 (vslot 25) - NCListCtrl's, as a flag (the stock body is shared with NCMatchPartyListCtrl's)
INT NCMatchListCtrl::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return NCListCtrl::WindowProc(Msg, wParam, lParam) != 0;
}
NW_HOOK_METHOD(0x100b1460, NCMatchListCtrl, WindowProc)

// 0x100b1480 (vslot 48) - NCListCtrl's (0x1001fe80: NCWnd's and the scroll range), then the three scroll controls are
// hidden again (the stock body is shared with NCMatchPartyListCtrl's)
void NCMatchListCtrl::ShowWindow()
{
    NCListCtrl::ShowWindow();   // 0x10022790
    ScrollUpButton->HideWindow();
    ScrollDownButton->HideWindow();
    ScrollBar->HideWindow();
}
NW_HOOK_METHOD(0x100b1480, NCMatchListCtrl, ShowWindow)

// 0x100b20d0 (vslot 77, ret 8) - a double click on a room asks to join it (the room number is the first cell's text), with
// no further parameter
INT NCMatchListCtrl::OnLButtonDblClick(WPARAM wParam, LPARAM lParam)
{
    INT Index = CallHitTest(LOWORD(lParam), HIWORD(lParam));
    Selected = Index;
    HoverRow = Index;
    if (Index >= 0 && Index < Rows.Num())
    {
        NListRow* Row = Rows(Index);
        L2ParamStack Params(10);
        Params.PushBack((void*)appAtoi((const TCHAR*)Row->Cells));
        Params.PushBack((void*)0);
        ((NConsoleForwardFns*)GConsole)->RequestJoinPartyRoom(Params);   // 0x1006DF60
    }
    return 1;
}
NW_HOOK_METHOD(0x100b20d0, NCMatchListCtrl, OnLButtonDblClick)

NW_CHECK_VTABLE(NCMatchListCtrl, 0x101bbdc0, 82)
