#pragma once
// NCXMasSealWnd - a small window of the Christmas event (UClass "CXMasSealWnd", 0xA4; C2 only,
// NCConsole::OnCreate makes it): a "MinimapBack" panel with a title bar, and a second picture
// drawn over it that nothing in this class sets (the console fills it).

#include "nwindow/NCWnd.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCXMasSealWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCXMasSealWnd, NCWnd, 0, 0x103108D8)

    INT Unknown98;                   // +0xE8 (0) cleared by the constructor; SetItem keeps the item's id here
    UTexture* BackTex;               // +0xEC "L2UI.MinimapWnd.MinimapBack"   // C2: offset derived, no evidence
    UTexture* SealTex;               // +0xF0 (name ours) drawn at (3, 22) over the back; set from outside

    NCXMasSealWnd();                                                 // 0x10121140
    // ~NCXMasSealWnd                                                // 0x10121190

    virtual INT OnCreate();                                          // 37 0x10121020
    virtual INT OnDestroy();                                         // 38 0x10120ed0
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x10120f50

    void SetItem(INT ItemID);                                        // 0x101210f0 (name ours) the seal's item: its picture
    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10148f10 (the stock vtable of this class overrides it; defined in NNewVirtuals.gen.cpp)
};
NW_ASSERT_OFFSET(NCXMasSealWnd, BackTex, 0xEC);
NW_ASSERT_OFFSET(NCXMasSealWnd, SealTex, 0xF0);
NW_ASSERT_SIZE(NCXMasSealWnd, 0xF4);

#pragma pack(pop)
