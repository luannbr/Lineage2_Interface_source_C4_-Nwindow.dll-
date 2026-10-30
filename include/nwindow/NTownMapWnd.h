#pragma once
// NTownMapWnd - the map of a town (UClass "TownMapWnd", 0xD0; C2 only; NCConsole makes it): a
// 328 x 329 view into a 512 x 512 picture that the left button drags, with the player's place
// marked by an icon, inside the minimap's frame.

#include "nwindow/NCWnd.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NTownMapWnd : public NCWnd
{
    NW_DECLARE_CLASS(NTownMapWnd, NCWnd, 0, 0x102E93C0)

    UTexture* BackTex;               // +0xE8 "L2UI.MinimapWnd.MinimapBack": the frame, drawn last
    UTexture* MapTex;                // +0xEC the town's picture (set from outside) (name ours)
    UTexture* IconTex;               // +0xF0 "town_map.icon_i00": the player's mark
    INT IconX;                       // +0xF4 the mark's place in the picture (set from outside) (name ours)
    INT IconY;                       // +0xF8 (name ours)
    INT MapW;                        // +0xFC the picture's size (512) (name ours)
    INT MapH;                        // +0x100 (512)
    INT ViewW;                       // +0x104 the part shown (328 x 329) (name ours)
    INT ViewH;                       // +0x108
    INT DragX;                       // +0x10C where the drag last was, in the view (name ours)   // C2: offset derived, no evidence
    INT DragY;                       // +0x110   // C2: offset derived, no evidence
    INT ScrollX;                     // +0x114 the view's corner in the picture (name ours)
    INT ScrollY;                     // +0x118
    UBOOL bDragging;                 // +0x11C (name ours)

    NTownMapWnd();                                                   // 0x100bcb50
    // ~NTownMapWnd                                                  // 0x10099940 (C1 only: no C2 match) (C1 only: no C2 match)

    virtual void KillFocus();                                        // 30 0x100b9010 (NCMinimapWnd's body)
    virtual INT OnCreate();                                          // 37 0x100bb3c0
    virtual INT OnDestroy();                                         // 38 0x100b8cc0
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x100b9de0
    virtual INT OnMouseMove(WPARAM wParam, LPARAM lParam);           // 67 0x100b8ee0
    virtual INT OnLButtonDown(WPARAM wParam, LPARAM lParam);         // 68 0x100b8e20
    virtual INT OnLButtonUp(WPARAM wParam, LPARAM lParam);           // 69 0x100b8eb0

    void SetMap(const TCHAR* TexName, INT X, INT Y);                 // 0x100b8d40 (name ours) the picture and the mark's place
    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10148f10 (the stock vtable of this class overrides it; defined in NNewVirtuals.gen.cpp)
};
NW_ASSERT_OFFSET(NTownMapWnd, MapTex, 0xEC);
NW_ASSERT_OFFSET(NTownMapWnd, IconX, 0xF4);
NW_ASSERT_OFFSET(NTownMapWnd, MapW, 0xFC);
NW_ASSERT_OFFSET(NTownMapWnd, DragX, 0x10C);
NW_ASSERT_OFFSET(NTownMapWnd, ScrollX, 0x114);
NW_ASSERT_OFFSET(NTownMapWnd, bDragging, 0x11C);
NW_ASSERT_SIZE(NTownMapWnd, 0x120);

#pragma pack(pop)
