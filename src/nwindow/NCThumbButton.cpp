/*=============================================================================
    NCThumbButton.cpp - the handle of the slider control (C2 only). Stock: 0x100013f0 ..
    0x1000143d, 0x10002900 .. 0x10002a2f (between NCExButton's and NCPushButton's constructors).
=============================================================================*/
#include "nwindow/NCThumbButton.h"

// 0x100037c0 - NCButton's, only the vtable differs
NCThumbButton::NCThumbButton()
{
}
NW_HOOK_CTOR(0x100037c0, NCThumbButton)

// 0x10003860
NCThumbButton::NCThumbButton(const TCHAR* NormalTexName, const TCHAR* PushedTexName, UBOOL bInAcceptKey)
    : NCButton(NormalTexName, PushedTexName, bInAcceptKey)
{
}
NW_HOOK_CTOR_ARGS(0x10003860, NCThumbButton, (const TCHAR* A, const TCHAR* B, UBOOL C), (A, B, C))

// 0x10003810
NW_HOOK_DTOR(0x10003810, NCThumbButton)


// 0x10001840 (vslot 67) - dragged: the pointer's x goes to the slider
INT NCThumbButton::OnMouseMove(WPARAM wParam, LPARAM lParam)
{
    if (wParam & MK_LBUTTON)
        NotifyParent(LOWORD(lParam));
    return 1;
}
NW_HOOK_METHOD(0x10001840, NCThumbButton, OnMouseMove)

// 0x10001830 (ret 8; the body NCIME::OnImeEndComposition hooks: the dword at +0x1F4 set, 1) - the button is pressed (NCButton::State)
#if NW_HOOK_MODE
NW_NAKED INT NCThumbButton::OnLButtonDown(WPARAM wParam, LPARAM lParam) NW_STOCK(0x10001830)
#else
INT NCThumbButton::OnLButtonDown(WPARAM wParam, LPARAM lParam)
{
    State = 1;
    return 1;
}
#endif

// 0x10001860 (ret 8; the body NCIME::OnImeStartComposition hooks: the dword at +0x1F4 cleared, 1) - the button is let go
#if NW_HOOK_MODE
NW_NAKED INT NCThumbButton::OnLButtonUp(WPARAM wParam, LPARAM lParam) NW_STOCK(0x10001860)
#else
INT NCThumbButton::OnLButtonUp(WPARAM wParam, LPARAM lParam)
{
    State = 0;
    return 1;
}
#endif

NW_CHECK_VTABLE(NCThumbButton, 0x1019f030, 75)
