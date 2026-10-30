#pragma once
// NCThumbButton - the handle of an NCSliderCtrl (UClass "CThumbButton", 0x264, a plain button
// in size; C2 only): pressing it only marks it pressed (no sound, no hit test), and moving
// the mouse with the left button held reports the pointer's x to the parent (NWM_COMMAND with
// the button's ID).

#include "nwindow/NCButton.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCThumbButton : public NCButton
{
    NW_DECLARE_CLASS(NCThumbButton, NCButton, 0, 0x102B6820)

    NCThumbButton();                                                 // 0x100037c0
    NCThumbButton(const TCHAR* NormalTexName, const TCHAR* PushedTexName, UBOOL bInAcceptKey); // 0x10003860
    // ~NCThumbButton                                                // 0x10003810

    virtual INT OnMouseMove(WPARAM wParam, LPARAM lParam);           // 67 0x10001840
    virtual INT OnLButtonDown(WPARAM wParam, LPARAM lParam);         // 68 0x10001830
    virtual INT OnLButtonUp(WPARAM wParam, LPARAM lParam);           // 69 0x10001860
    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x100e9f60 (the stock vtable of this class overrides it; defined in NNewVirtuals.gen.cpp)
};
NW_ASSERT_SIZE(NCThumbButton, 0x2B4);

#pragma pack(pop)
