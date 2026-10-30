/*=============================================================================
    NCPartyRoomFrameCtrl.cpp - the title bar of the party room window. C3 stock: 0x100a7a90 (constructor),
    0x100a66b0 (OnClose), 0x100a7ae0 (destructor).
=============================================================================*/
#include "nwindow/NCPartyRoomFrameCtrl.h"
#include "nwindow/NCPartyRoomWnd.h"

// 0x100b43c0 - NCFrameCtrl's default constructor (decorated), then this class' vtable
NCPartyRoomFrameCtrl::NCPartyRoomFrameCtrl()
{
}
NW_HOOK_CTOR(0x100b43c0, NCPartyRoomFrameCtrl)

// 0x100b4410
NW_HOOK_DTOR(0x100b4410, NCPartyRoomFrameCtrl)

// 0x100b31a0 (vslot 76) - the room window (the parent) asks to dismiss or leave the room (when there is one), then
// NCFrameCtrl's close
INT NCPartyRoomFrameCtrl::OnClose()
{
    NCPartyRoomWnd* Room = static_cast<NCPartyRoomWnd*>(Entry->Parent->Wnd);
    if (Room)
        Room->RequestExit();
    return NCFrameCtrl::OnClose();
}
NW_HOOK_METHOD(0x100b31a0, NCPartyRoomFrameCtrl, OnClose)

NW_CHECK_VTABLE(NCPartyRoomFrameCtrl, 0x101bbc80, 79)
