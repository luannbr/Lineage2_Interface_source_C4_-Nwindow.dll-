#pragma once
// NCMatchPartyListCtrl - the list of the members of a party room (UClass "CMatchPartyListCtrl", 0x24C; new in C3):
// NCPartyRoomWnd's. NCListCtrl's layout, the scroll controls hidden as the matching list's.
//
// Row data (NCPartyRoomWnd::SetMembers / UpdateMember): Cells[0] the name (Param1 the object id of the member),
// Cells[1] the class icon (no text; Param2 of the row the class id), Cells[2] the level range, Cells[3] the place,
// Cells[4] the role.

#include "nwindow/NCListCtrl.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCMatchPartyListCtrl : public NCListCtrl
{
    NW_DECLARE_CLASS(NCMatchPartyListCtrl, NCListCtrl, 0, 0x102E59D0)

    NCMatchPartyListCtrl();                                          // 0x100b5450
    NCMatchPartyListCtrl(INT InMaxRows, INT InVisibleRows, INT InMaxColumns, INT Unused); // 0x100b54f0 (ret 0x10)
    // ~NCMatchPartyListCtrl                                         // 0x100b54a0

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x100b1460 (the code of NCMatchListCtrl's)
    virtual void ShowWindow();                                       // 48 0x100b18b0 (the code of NCMatchListCtrl's)

    // names ours: the row of a member by his object id (the first cell's Param1), -1 / NULL when there is none
    INT FindMemberIndex(INT ObjectID);                               // 0x100b3120 (ret 4)
    NListRow* FindMemberRow(INT ObjectID);                           // 0x100b3160 (ret 4)
};
NW_ASSERT_SIZE(NCMatchPartyListCtrl, 0x270);

// The object id of a member (his row's first cell's first parameter) at its C4 offset: NListCell grew by 0x240 bytes in
// C4 (db/struct_sizes.csv), which the header's Param1 (C3's +0x1808) does not show yet.
inline INT& MemberObjectID(NListRow* Row)
{
    return *(INT*)((BYTE*)Row->Cells + 0x1A48);
}

#pragma pack(pop)
