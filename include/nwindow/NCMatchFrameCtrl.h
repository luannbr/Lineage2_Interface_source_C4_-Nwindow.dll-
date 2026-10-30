#pragma once
// NCMatchFrameCtrl - the title bar of the party matching window (UClass "CMatchFrameCtrl", 0x238; new in C3): an
// NCFrameCtrl that does nothing of its own: its close button closes the window as the base's does.

#include "nwindow/NCFrameCtrl.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCMatchFrameCtrl : public NCFrameCtrl
{
    NW_DECLARE_CLASS(NCMatchFrameCtrl, NCFrameCtrl, 0, 0x102E6960)

    NCMatchFrameCtrl();                                              // 0x100b4320 (NCFrameCtrl's default constructor)
    // ~NCMatchFrameCtrl                                             // 0x100b4370

    virtual INT OnClose();                                           // 76 0x100b14e0 (a thunk to NCFrameCtrl's)
    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x100b14c0 (the stock vtable of this class overrides it; defined in NNewVirtuals.gen.cpp)
};
NW_ASSERT_SIZE(NCMatchFrameCtrl, 0x26C);

#pragma pack(pop)
