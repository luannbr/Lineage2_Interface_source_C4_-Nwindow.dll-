#pragma once
// NColumnInfoC3 - a list column's record as this client lays it out. NListColumnInfo (NCListCtrl.h) still has the
// C2 layout (0x1814 bytes); in C3 it is 0x4814: the setters take a state index (the header button has three states,
// the textures "tab1" .. "tab3" of L2UI_CH3.ListCtrl) and two more texture strings follow. Evidence: the frame of
// NCPledgeWnd::OnCreate (0x12050 = four of them), SetTitleID 0x1001f4b0, SetNormalTex 0x1001f540 and SetPushedTex
// 0x1001f5e0 (ret 8: name, state), the zeroed words of the inline constructor, NCListCtrl::AddColumn 0x10021430.
// No setters: they called the C4 stock setters (0x10005BA0 ...), which write the C4 record, not this one; nothing uses this C3 record.

#include "nwindow/NWindow.h"

struct NColumnInfoC3
{
    INT Width;                     // +0x000
    INT Height;                    // +0x004 of the header; the rows start under it
    TCHAR Title[1024];             // +0x008 SetTitleID copies a sysstring here
    UBOOL bClickable;              // +0x808 (1) a click on the header sorts
    UBOOL bNumeric;                // +0x80C (0) sort by value instead of by text
    INT Unknown810;                // +0x810 (left as it was on the stack)
    TCHAR NormalTex[3][1024];      // +0x814 SetNormalTex(Name, State)
    TCHAR PushedTex[3][1024];      // +0x2014 SetPushedTex(Name, State)
    TCHAR Tex3814[1024];           // +0x3814 cleared by the inline constructor; no setter writes it (name ours)
    TCHAR Tex4014[1024];           // +0x4014 the same (name ours)

    // inlined in the owners
    NColumnInfoC3()
    {
        Title[0] = 0;
        bClickable = 1;
        bNumeric = 0;
        NormalTex[0][0] = 0;
        NormalTex[1][0] = 0;
        NormalTex[2][0] = 0;
        PushedTex[0][0] = 0;
        PushedTex[1][0] = 0;
        PushedTex[2][0] = 0;
        Tex3814[0] = 0;
        Tex4014[0] = 0;
    }
};
NW_ASSERT_OFFSET(NColumnInfoC3, Title, 0x8);
NW_ASSERT_OFFSET(NColumnInfoC3, bClickable, 0x808);
NW_ASSERT_OFFSET(NColumnInfoC3, NormalTex, 0x814);
NW_ASSERT_OFFSET(NColumnInfoC3, PushedTex, 0x2014);
NW_ASSERT_OFFSET(NColumnInfoC3, Tex4014, 0x4014);
NW_ASSERT_SIZE(NColumnInfoC3, 0x4814);
