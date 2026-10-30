#pragma once
// NColumnInfoC4 - a list column's record as the C4 client lays it out (0x4018 bytes; NColumnInfoC3.h has the C3 layout,
// 0x4814, whose constructor and setters do not fit this client). Evidence: the frame of NCPledgeWnd::OnCreate (0x10060 =
// four of them), NListColumnInfo::SetNormalTex 0x10021CB0 (the string of state 0, 1, 2 at +0x18, +0x818, +0x1018),
// NCLVColumn::SetTabDownTex 0x10021D40 (the strings at +0x1818 + 0x800 * state), sub_10005BA0 (stores the argument at +8:
// the column's title is a system string id now, not a copy of the string), the owners' inline constructor (-1, 1, 0 at
// +8, +0xC, +0x10 and a zero first character in each of the eight strings), NCListCtrl::AddColumn 0x10023EE0.

#include "nwindow/NWindow.h"
#include "nwindow/NCListCtrl.h"

struct NColumnInfoC4
{
    INT Width;                     // +0x000
    INT Height;                    // +0x004 of the header; the rows start under it
    INT TitleID;                   // +0x008 the system string of the title (-1 none) (C3: the string itself)
    UBOOL bClickable;              // +0x00C (1) a click on the header sorts (name ours, as in C3)
    UBOOL bNumeric;                // +0x010 (0) sort by value instead of by text (name ours)
    INT Unknown14;                 // +0x014 (left as it was on the stack)
    TCHAR NormalTex[3][1024];      // +0x018 SetNormalTex(Name, State)
    TCHAR TabDownTex[3][1024];     // +0x1818 SetTabDownTex(Name, State) (the pushed states)
    TCHAR Tex3018[1024];           // +0x3018 cleared by the inline constructor; no setter writes it (name ours)
    TCHAR Tex3818[1024];           // +0x3818 the same (name ours)

    // inlined in the owners
    NColumnInfoC4()
    {
        TitleID = -1;
        bClickable = 1;
        bNumeric = 0;
        NormalTex[0][0] = 0;
        NormalTex[1][0] = 0;
        NormalTex[2][0] = 0;
        TabDownTex[0][0] = 0;
        TabDownTex[1][0] = 0;
        TabDownTex[2][0] = 0;
        Tex3018[0] = 0;
        Tex3818[0] = 0;
    }
    // The three setters are NListColumnInfo's (NCListCtrl.cpp; the same 0x4018-byte record, NCListCtrl.h asserts its offsets)
    void SetTitleID(INT TextID)                          // 0x10005BA0 (ret 4)
    {
        reinterpret_cast<NListColumnInfo*>(this)->SetTitleID(TextID);
    }
    void SetNormalTex(const TCHAR* Name, INT State)      // 0x10021CB0 (ret 8)
    {
        reinterpret_cast<NListColumnInfo*>(this)->SetNormalTex(Name, State);
    }
    void SetTabDownTex(const TCHAR* Name, INT State)     // 0x10021D40 (ret 8)
    {
        reinterpret_cast<NListColumnInfo*>(this)->SetPushedTex(Name, State);
    }
};
NW_ASSERT_OFFSET(NColumnInfoC4, NormalTex, 0x18);
NW_ASSERT_OFFSET(NColumnInfoC4, TabDownTex, 0x1818);
NW_ASSERT_OFFSET(NColumnInfoC4, Tex3818, 0x3818);
NW_ASSERT_SIZE(NColumnInfoC4, 0x4018);
