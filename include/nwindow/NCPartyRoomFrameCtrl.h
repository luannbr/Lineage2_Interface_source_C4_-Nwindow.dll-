#pragma once
// NCPartyRoomFrameCtrl - the title bar of the party room window (UClass "CPartyRoomFrameCtrl", 0x238; new in C3):
// closing it first asks the server to leave the room (what the Close button of the room does too), then closes the
// window as NCFrameCtrl does.

#include "nwindow/NCFrameCtrl.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCPartyRoomFrameCtrl : public NCFrameCtrl
{
    NW_DECLARE_CLASS(NCPartyRoomFrameCtrl, NCFrameCtrl, 0, 0x102E54A0)

    NCPartyRoomFrameCtrl();                                          // 0x100b43c0 (NCFrameCtrl's default constructor)
    // ~NCPartyRoomFrameCtrl                                         // 0x100b4410

    virtual INT OnClose();                                           // 76 0x100b31a0
    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x100b14c0 (the stock vtable of this class overrides it; defined in NNewVirtuals.gen.cpp)
};
NW_ASSERT_SIZE(NCPartyRoomFrameCtrl, 0x26C);

#pragma pack(pop)
