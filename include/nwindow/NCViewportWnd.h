#pragma once
// NCViewportWnd - a window the engine renders a camera view into (UClass "CViewportWnd", 0xF8; new in C4; the base of the
// fishing window's viewport): DrawViewport clips to the rectangle (OffsetX, OffsetY, ViewWidth x ViewHeight) inside the
// window and asks the canvas to draw its scene there (UCanvas::DrawCameraSceneNode, through the device's origin).

#include "nwindow/NCWnd.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCGDevice;

class NCViewportWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCViewportWnd, NCWnd, 0, 0x1030FE70)

    INT OffsetX;                // +0xE8 the picture's place inside the window (names ours; set by the owner)
    INT OffsetY;                // +0xEC
    FLOAT ViewWidth;            // +0xF0 its size
    FLOAT ViewHeight;           // +0xF4

    NCViewportWnd();                                                 // 0x10120ce0
    // ~NCViewportWnd                                                // 0x10120c60

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10148f10 (folded)
    virtual INT OnCreate();                                          // 37 0x100ef780 (shared)
    virtual INT OnDestroy();                                         // 38 0x1011ff20
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x100f76f0 (folded)
    virtual INT DrawViewport(NCGDevice* Device);                     // 74 0x101203a0 (name ours; new in C4)
};
NW_ASSERT_OFFSET(NCViewportWnd, OffsetX, 0xE8);
NW_ASSERT_OFFSET(NCViewportWnd, ViewHeight, 0xF4);
NW_ASSERT_SIZE(NCViewportWnd, 0xF8);

#pragma pack(pop)
