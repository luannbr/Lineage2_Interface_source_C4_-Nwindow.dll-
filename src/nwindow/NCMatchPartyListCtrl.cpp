/*=============================================================================
    NCMatchPartyListCtrl.cpp - the list of the members of a party room. C3 stock: 0x100a8b00 / 0x100a8ba0
    (constructors), 0x100a8b50 (destructor), 0x100a6630 / 0x100a6670 (the member searches); WindowProc and
    ShowWindow are the code of NCMatchListCtrl's (0x100a4c10, 0x100a50c0: the linker kept one copy).
=============================================================================*/
#include "nwindow/NCMatchPartyListCtrl.h"
#include "nwindow/NCButton.h"
#include "nwindow/NCSliderBar.h"

// 0x100b5450 (wave C; it was a forwarder to the stock constructor, left so by the slot-48 vtable judge: the ShowWindow below was C3's, see there) -
// NCListCtrl's with the scroll controls (bNoScrollBar 0), then this class' vtable
NCMatchPartyListCtrl::NCMatchPartyListCtrl()
    : NCListCtrl(0)
{
}
NW_HOOK_CTOR(0x100b5450, NCMatchPartyListCtrl)

// 0x100b54f0 (ret 0x10; wave C) - NCListCtrl's with the four numbers (and the header's defaults 0 / 0x11), then this class' vtable
NCMatchPartyListCtrl::NCMatchPartyListCtrl(INT InMaxRows, INT InVisibleRows, INT InMaxColumns, INT Unused)
    : NCListCtrl(InMaxRows, InVisibleRows, InMaxColumns, Unused, 0)
{
}
NW_HOOK_CTOR_ARGS(0x100b54f0, NCMatchPartyListCtrl, (INT A, INT B, INT C, INT D), (A, B, C, D))

// 0x100b54a0
NW_HOOK_DTOR(0x100b54a0, NCMatchPartyListCtrl)

// stock body shared with NCMatchListCtrl's (0x100a4c10): NCListCtrl's, as a flag
INT NCMatchPartyListCtrl::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return NCListCtrl::WindowProc(Msg, wParam, lParam) != 0;
}

// 0x100b18b0 (vslot 48; C4: a 5-byte `jmp 0x10022790`, a thunk of this class' own) - NCListCtrl's ShowWindow and nothing else. In C3 the stock
// shared NCMatchListCtrl's body (which hides the three scroll controls again); in C4 the party list has its own thunk, which does not, and the
// slot of the stock vtable is this thunk. Not hooked (a thunk that only jumps to a hooked function): the body below is what our vtable
// (the constructors above are hooked) runs
void NCMatchPartyListCtrl::ShowWindow()
{
    NCListCtrl::ShowWindow();
}

// 0x100b3120 (ret 4) - the index of the row whose first cell holds the object id, -1 when no row does
INT NCMatchPartyListCtrl::FindMemberIndex(INT ObjectID)
{
    for (INT i = 0; i < Rows.Num(); i++)
    {
        NListRow* Row = Rows(i);
        if (Row && MemberObjectID(Row) == ObjectID)
            return i;
    }
    return -1;
}
NW_HOOK_METHOD(0x100b3120, NCMatchPartyListCtrl, FindMemberIndex)

// 0x100b3160 (ret 4) - the row itself, NULL when no row has it
NListRow* NCMatchPartyListCtrl::FindMemberRow(INT ObjectID)
{
    for (INT i = 0; i < Rows.Num(); i++)
    {
        NListRow* Row = Rows(i);
        if (Row && MemberObjectID(Row) == ObjectID)
            return Row;
    }
    return NULL;
}
NW_HOOK_METHOD(0x100b3160, NCMatchPartyListCtrl, FindMemberRow)

NW_CHECK_VTABLE(NCMatchPartyListCtrl, 0x101bc1c8, 82)
