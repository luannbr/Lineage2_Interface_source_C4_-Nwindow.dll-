#pragma once
// NCReplayListCtrl - the list of the replay files (UClass "CReplayListCtrl", 0x270; new in C4, the same size as NCListCtrl: nothing
// is added to its layout): the replay list window (NCReplayListWnd) shows the names of ..\REPLAY\*.L2R in it, 256 rows at most,
// fifteen visible, two columns (the number and the name). Its header does not sort (the SortByColumn slot is the folded
// "return 1"), and it passes the header's click on (NWM_COMMAND 0xF034) to that slot.
//
// Layout and virtuals from the stock constructors 0x100F7E60 / 0x100F7F00 and WindowProc 0x100F7690.

#include "nwindow/NCListCtrl.h"
#pragma pack(push, 4)

class NCReplayListCtrl : public NCListCtrl
{
    NW_DECLARE_CLASS(NCReplayListCtrl, NCListCtrl, 0, 0x102FFFA8)

    NCReplayListCtrl();                                              // 0x100f7e60 (NCListCtrl(0))
    NCReplayListCtrl(INT InMaxRows, INT InVisibleRows, INT InMaxColumns, INT Unused);   // 0x100f7f00 (ret 0x10; NCListCtrl(.., 0, 0x11))
    // ~NCReplayListCtrl                                             // 0x100f7eb0

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x100f7690
    virtual INT SortByColumn(INT Column);                            // 81 0x100f76f0 (folded: returns 1)
};
NW_ASSERT_SIZE(NCReplayListCtrl, 0x270);

#pragma pack(pop)
