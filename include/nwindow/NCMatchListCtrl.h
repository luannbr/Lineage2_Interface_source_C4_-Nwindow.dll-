#pragma once
// NCMatchListCtrl - the list of the party matching window (UClass "CMatchListCtrl", 0x24C): the rooms, five
// columns (number, title, type, level range, members). A double click on a row asks the server to join that room.
// The scroll controls stay hidden. Nothing is added to NCListCtrl's layout.
//
// Row data (NCMatchWnd::SetList): Cells[0] the room number (as text), Cells[1] the title, Cells[2] the type, Cells[3]
// "min-max", Cells[4] "now/max".

#include "nwindow/NCListCtrl.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCMatchListCtrl : public NCListCtrl
{
    NW_DECLARE_CLASS(NCMatchListCtrl, NCListCtrl, 0, 0x102E6430)

    NCMatchListCtrl();                                               // 0x100b4460
    NCMatchListCtrl(INT InMaxRows, INT InVisibleRows, INT InMaxColumns, INT Unused); // 0x100b4500 (ret 0x10)
    // ~NCMatchListCtrl                                              // 0x100b44b0

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x100b1460 (shared with NCMatchPartyListCtrl's)
    virtual void ShowWindow();                                       // 48 0x100b1480 (shared with NCMatchPartyListCtrl's)
    virtual INT OnLButtonDblClick(WPARAM wParam, LPARAM lParam);     // 77 0x100b20d0
};
NW_ASSERT_SIZE(NCMatchListCtrl, 0x270);

#pragma pack(pop)
