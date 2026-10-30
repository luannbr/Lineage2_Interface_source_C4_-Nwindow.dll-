/*=============================================================================
    L2ChatFrameCtrl.cpp - the L2 skin's conversation title bar (C3 only; C3 addresses). Stock: the constructors /
    destructor 0x101258a0 .. 0x1012593f. WindowProc (0x10119aa0), OnMinimize (0x101241e0) and OnClose (0x10124210) are
    ChatFrameCtrl's (the same stock functions, hooked in ChatFrameCtrl.cpp).
=============================================================================*/
#include "nwindow/L2ChatFrameCtrl.h"

// 0x1014a350 - the base constructor, then the vtable
L2ChatFrameCtrl::L2ChatFrameCtrl()
{
}
NW_HOOK_CTOR(0x1014a350, L2ChatFrameCtrl)

// 0x1014a3f0 (ret 0xC) - the frame's three arguments
L2ChatFrameCtrl::L2ChatFrameCtrl(INT InDecorated, INT InVertical, INT InSmall)
    : NCFrameCtrl(InDecorated, InVertical, InSmall)
{
}
NW_HOOK_CTOR_ARGS(0x1014a3f0, L2ChatFrameCtrl, (INT A, INT B, INT C), (A, B, C))

// 0x1014a3a0
NW_HOOK_DTOR(0x1014a3a0, L2ChatFrameCtrl)

// 0x10119aa0 (vslot 25) - ChatFrameCtrl's body (hooked there)
INT L2ChatFrameCtrl::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCFrameCtrl::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND)
    {
        if (wParam == NID_FRAME_MINIMIZE)
            return OnMinimize() != 0;
        if (wParam == NID_FRAME_CLOSE)
            return OnClose() != 0;
    }
    return 0;
}

// 0x101241e0 (vslot 71) - ChatFrameCtrl's body (hooked there): the window this bar belongs to hears the minimize button
INT L2ChatFrameCtrl::OnMinimize()
{
    NWndEntry* Parent = Entry->Parent;
    if (Parent && Parent->Wnd)
    {
        Parent->Wnd->WindowProc(NWM_COMMAND, NID_FRAME_MINIMIZE, 0);
        return 1;
    }
    return 0;
}

// 0x10124210 (vslot 70) - ChatFrameCtrl's body (hooked there): ... and the close button
INT L2ChatFrameCtrl::OnClose()
{
    NWndEntry* Parent = Entry->Parent;
    if (Parent && Parent->Wnd)
    {
        Parent->Wnd->WindowProc(NWM_COMMAND, NID_FRAME_CLOSE, 0);
        return 1;
    }
    return 0;
}

NW_CHECK_VTABLE(L2ChatFrameCtrl, 0x101dafe8, 79)
