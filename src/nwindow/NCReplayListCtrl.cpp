/*=============================================================================
    NCReplayListCtrl.cpp - the list of the replay files (new in C4; C4 addresses). Stock: WindowProc 0x100F7690, the
    constructors 0x100F7E60 (the default one) and 0x100F7F00 (four arguments), the destructor 0x100F7EB0. The scalar deleting
    destructor (0x100F7E80) and InternalConstructor (0x100F85F0) stay stock.
=============================================================================*/
#include "nwindow/NCReplayListCtrl.h"

// 0x100f7e60 - NCListCtrl's constructor with the scroll bar (0)
NCReplayListCtrl::NCReplayListCtrl() : NCListCtrl(0)
{
}
NW_HOOK_CTOR(0x100f7e60, NCReplayListCtrl)

// 0x100f7f00 (ret 0x10) - NCListCtrl's four argument constructor with no scroll bar flag (0) and the row height 0x11
NCReplayListCtrl::NCReplayListCtrl(INT InMaxRows, INT InVisibleRows, INT InMaxColumns, INT Unused)
    : NCListCtrl(InMaxRows, InVisibleRows, InMaxColumns, Unused, 0, 0x11)
{
}
NW_HOOK_CTOR_ARGS(0x100f7f00, NCReplayListCtrl, (INT A, INT B, INT C, INT D), (A, B, C, D))

// 0x100f7eb0
NW_HOOK_DTOR(0x100f7eb0, NCReplayListCtrl)

// 0x100f7690 (vslot 25) - NCListCtrl's, then the header's click (NWM_COMMAND 0xF034, lParam = the column) goes to the sort slot
INT NCReplayListCtrl::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCListCtrl::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND && wParam == 0xF034)
        return SortByColumn((INT)lParam) != 0;
    return 0;
}
NW_HOOK_METHOD(0x100f7690, NCReplayListCtrl, WindowProc)

// 0x100f76f0 (vslot 81) - a body shared by many classes: returns 1 (this list is not sorted)
#if NW_HOOK_MODE
NW_NAKED INT NCReplayListCtrl::SortByColumn(INT Column) NW_STOCK(0x100f76f0)
#else
INT NCReplayListCtrl::SortByColumn(INT Column)
{
    return 1;   // 0x100f76f0: mov eax, 1 / ret 4
}
#endif

NW_CHECK_VTABLE(NCReplayListCtrl, 0x101c85f8, 82)
