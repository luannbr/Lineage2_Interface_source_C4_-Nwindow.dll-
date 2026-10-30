#pragma once
// NCTabButton - a tab of the tab control (UClass "CTabButton", 0x2D8; C4 only): a radio button the control can drag off its bar. The
// control (NCTabControl, base lane) makes them with the stock constructor 0x10003B00 (normal and pushed texture names) and keeps them as
// NCRadioButtons; this header gives the class its own declaration for the functions that are ported here.
//
// Layout from the constructor 0x10003A40 and the methods: the bar state follows NCRadioButton's 0x2C0 bytes.
//
// The one-line accessors (anonymous in the db, wave C names ours; NCTabControl.cpp still calls them by address, which reaches the hooks):
// 0x10001EF0 (bSplit), 0x10001F00 (set bSplit), 0x10001F10 (set TabIndex), 0x10002000 (move the bar position by a delta); the
// two-argument constructor is 0x10003B00.

#include "nwindow/NCButton.h"
#pragma pack(push, 4)

class NCTabControl;

class NCTabButton : public NCRadioButton
{
    NW_DECLARE_CLASS(NCTabButton, NCRadioButton, 0, 0x102B5350)

    INT OrgX;                  // +0x2C0 (names ours) where the tab sits on the bar, screen coordinates (SetBounds stores the window's)
    INT OrgY;                  // +0x2C4
    FLOAT OrgW;                // +0x2C8 the size it had on the bar
    FLOAT OrgH;                // +0x2CC
    INT TabIndex;              // +0x2D0 (-1) the tab's place in the control (the control sets it)
    INT bSplit;                // +0x2D4 the tab was dragged off the bar

    NCTabButton();                                                   // 0x10003A40
    NCTabButton(const TCHAR* NormalTexName, const TCHAR* PushedTexName);   // 0x10003B00 (ret 8) the one the tab control makes the tabs with
    // ~NCTabButton                                                  // 0x10003AB0

    // names ours, non-virtual, out of line like the stock ones
    __declspec(noinline) INT GetSplit();                             // 0x10001EF0 bSplit
    __declspec(noinline) void SetSplit(INT bValue);                  // 0x10001F00 (ret 4)
    __declspec(noinline) void SetTabIndex(INT Index);                // 0x10001F10 (ret 4)
    __declspec(noinline) void MoveOrg(INT DX, INT DY);               // 0x10002000 (ret 8) the bar position follows the window

    virtual INT OnMouseMove(WPARAM wParam, LPARAM lParam);           // 67 0x10002030 the window drags, then the radio button's
    virtual INT OnLButtonDown(WPARAM wParam, LPARAM lParam);         // 68 0x10002A50 + the control selects this tab (SelectTab(TabIndex, 1))
    virtual INT OnLButtonUp(WPARAM wParam, LPARAM lParam);           // 69 0x10002B00 + the control is told the tab was let go (TabRelease)
    virtual INT OnMove(WPARAM wParam, LPARAM lParam);                // 72 0x10002BB0 + the control is told it is being dragged (TabMove)
    // new in this class (the tab control calls it for the tab it makes): the position as NCWnd's, then the bar state
    virtual void SetBounds(INT X, INT Y, DWORD W, DWORD H);          // 75 0x10002650 (name ours; the stock guard says SetPosition)

    // 0x10001F20 (ret 8, guard) - the tab goes back to its place on the bar: the offset of OrgX / OrgY from the window comes out and the
    // window is moved (virtual 51) by it
    void MoveToOrgPosition(INT* a_pX, INT* a_pY);
};
NW_ASSERT_OFFSET(NCTabButton, OrgX, 0x2C0);
NW_ASSERT_OFFSET(NCTabButton, TabIndex, 0x2D0);
NW_ASSERT_OFFSET(NCTabButton, bSplit, 0x2D4);
NW_ASSERT_SIZE(NCTabButton, 0x2D8);

#pragma pack(pop)
