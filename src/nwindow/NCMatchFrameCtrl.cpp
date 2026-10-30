/*=============================================================================
    NCMatchFrameCtrl.cpp - the title bar of the party matching window. C3 stock: 0x100a79f0 (constructor),
    0x100a4c30 (OnClose), 0x100a7a40 (destructor).
=============================================================================*/
#include "nwindow/NCMatchFrameCtrl.h"

// 0x100b4320 - NCFrameCtrl's default constructor (decorated), then this class' vtable
NCMatchFrameCtrl::NCMatchFrameCtrl()
{
}
NW_HOOK_CTOR(0x100b4320, NCMatchFrameCtrl)

// 0x100b4370
NW_HOOK_DTOR(0x100b4370, NCMatchFrameCtrl)

// 0x100b14e0 (vslot 76) - a thunk: NCFrameCtrl's OnClose
INT NCMatchFrameCtrl::OnClose()
{
    return NCFrameCtrl::OnClose();
}
NW_HOOK_METHOD(0x100b14e0, NCMatchFrameCtrl, OnClose)

NW_CHECK_VTABLE(NCMatchFrameCtrl, 0x101bbb40, 79)
