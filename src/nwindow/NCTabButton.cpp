/*=============================================================================
    NCTabButton.cpp - the tab buttons of NCTabControl (C4 only). Stock: 0x10001f20 .. 0x10001fc0 (MoveToOrgPosition), 0x10002030,
    0x10002650, 0x10002a50 .. 0x10002c2f (the mouse and move handlers), 0x10003a40 (constructor) and 0x10003ab0 (destructor). The
    two-argument constructor (0x10003b00) and the accessors (0x10001ef0 .. 0x10001f10, 0x10002000) are the base lane's anonymous
    functions; the checked cast to the tab control (0x100029f0) is ours.
=============================================================================*/
#include "nwindow/NCTabButton.h"
#include "nwindow/NCTabControl.h"

// 0x100029f0 (cdecl; anonymous in the db) - UObject's checked cast to NCTabControl, out of line in the stock image: the object comes
// back whatever the class is, and a wrong one is logged through GError ("Cast of %s to %s failed")
static __declspec(noinline) NCTabControl* __cdecl CastToTabControl(NCWnd* Obj)
{
    return CastChecked<NCTabControl>(Obj);
}
NW_HOOK_FUNC(0x100029f0, CastToTabControl)

// the parent window as the tab control
static FORCEINLINE NCTabControl* TabControlOf(NCWnd* Wnd)
{
    return CastToTabControl(Wnd->Entry->Parent->Wnd);
}

// 0x10003a40 - NCRadioButton's constructor, then the bar state: nothing, no place in the control, not split
NCTabButton::NCTabButton()
{
    OrgY = 0;
    OrgX = 0;
    OrgW = 0.0f;
    OrgH = 0.0f;
    bSplit = 0;
    TabIndex = -1;
}
NW_HOOK_CTOR(0x10003a40, NCTabButton)

// 0x10003b00 (ret 8; anonymous in the db) - the constructor NCTabControl::AddTab makes the tabs with (the textures' names): the radio
// button's two-argument constructor (0x100039C0, the textures set), then the same bar state as the one above
NCTabButton::NCTabButton(const TCHAR* NormalTexName, const TCHAR* PushedTexName) : NCRadioButton(NormalTexName, PushedTexName)
{
    OrgY = 0;
    OrgX = 0;
    OrgW = 0.0f;
    OrgH = 0.0f;
    bSplit = 0;
    TabIndex = -1;
}
NW_HOOK_CTOR_ARGS(0x10003b00, NCTabButton, (const TCHAR* A, const TCHAR* B), (A, B))

// 0x10003ab0
NW_HOOK_DTOR(0x10003ab0, NCTabButton)

// 0x10001ef0 (anonymous in the db; name ours) - the tab was dragged off the bar
INT NCTabButton::GetSplit()
{
    return bSplit;
}
NW_HOOK_METHOD(0x10001ef0, NCTabButton, GetSplit)

// 0x10001f00 (ret 4; name ours)
void NCTabButton::SetSplit(INT bValue)
{
    bSplit = bValue;
}
NW_HOOK_METHOD(0x10001f00, NCTabButton, SetSplit)

// 0x10001f10 (ret 4; name ours) - the tab's place in the control
void NCTabButton::SetTabIndex(INT Index)
{
    TabIndex = Index;
}
NW_HOOK_METHOD(0x10001f10, NCTabButton, SetTabIndex)

// 0x10002000 (ret 8; name ours) - the bar position the tab goes back to moves with the window
void NCTabButton::MoveOrg(INT DX, INT DY)
{
    OrgX += DX;
    OrgY += DY;
}
NW_HOOK_METHOD(0x10002000, NCTabButton, MoveOrg)

// 0x10001f20 (ret 8) - the tab goes back to its place on the bar: how far OrgX / OrgY are from the window comes out, and the
// window is moved by it (NCWnd's virtual 51)
void NCTabButton::MoveToOrgPosition(INT* a_pX, INT* a_pY)
{
    guard(NCTabButton::MoveToOrgPosition);
    NW_CHECK_AT(a_pX, "Src\\NButton.cpp", 0x2db);
    NW_CHECK_AT(a_pY, "Src\\NButton.cpp", 0x2dc);
    *a_pX = OrgX - ScreenX;
    *a_pY = OrgY - ScreenY;
    MoveWindow(*a_pX, *a_pY);
    unguard;
}
NW_HOOK_METHOD(0x10001f20, NCTabButton, MoveToOrgPosition)

// 0x10002030 (vslot 67)
INT NCTabButton::OnMouseMove(WPARAM wParam, LPARAM lParam)
{
    guard(NCTabButton::OnMouseMove);
    NCWnd::OnMouseMove(wParam, lParam);
    NCRadioButton::OnMouseMove(wParam, lParam);
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10002030, NCTabButton, OnMouseMove)

// 0x10002650 (vslot 75; the stock guard says NCTabButton::SetPosition) - NCWnd's SetPosition with the default pin (0, 0, 3), then the
// bar position and size are the window's
void NCTabButton::SetBounds(INT X, INT Y, DWORD W, DWORD H)
{
    guard(NCTabButton::SetPosition);
    NCWnd::SetPosition(X, Y, W, H, 0, 0, 3);
    OrgY = ScreenY;
    OrgX = ScreenX;
    OrgW = Width;
    OrgH = Height;
    unguard;
}
NW_HOOK_METHOD(0x10002650, NCTabButton, SetBounds)

// 0x10002a50 (vslot 68) - the window and the radio button first, then the control selects the tab (virtual 76: SelectTab)
INT NCTabButton::OnLButtonDown(WPARAM wParam, LPARAM lParam)
{
    guard(NCTabButton::OnLButtonDown);
    NCWnd::OnLButtonDown(wParam, lParam);
    NCRadioButton::OnLButtonDown(wParam, lParam);
    TabControlOf(this)->SelectTab(TabIndex, 1);
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10002a50, NCTabButton, OnLButtonDown)

// 0x10002b00 (vslot 69) - the window's, then the control is told the tab was let go (virtual 78)
INT NCTabButton::OnLButtonUp(WPARAM wParam, LPARAM lParam)
{
    guard(NCTabButton::OnLButtonUp);
    NCWnd::OnLButtonUp(wParam, lParam);
    ::nw::VirtualCall<INT>(TabControlOf(this), 78, TabIndex);
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10002b00, NCTabButton, OnLButtonUp)

// 0x10002bb0 (vslot 72) - the window's WM_MOVE, then the control is told where the tab is (virtual 77, with the new x and y from
// lParam)
INT NCTabButton::OnMove(WPARAM wParam, LPARAM lParam)
{
    guard(NCTabButton::OnMove);
    NCWnd::OnMove(wParam, lParam);
    ::nw::VirtualCall<INT>(TabControlOf(this), 77, TabIndex, (INT)(SWORD)LOWORD(lParam), (INT)(SWORD)HIWORD(lParam));
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10002bb0, NCTabButton, OnMove)

NW_CHECK_VTABLE(NCTabButton, 0x1019f290, 76)
