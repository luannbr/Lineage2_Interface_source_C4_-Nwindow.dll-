#pragma once
// NCObserverWnd - the box that stays on the screen while the player watches a duel or siege
// from the observer camera (UClass "CObserverWnd", 0xD0; C2: 0xA0): a panel with a vertical title bar
// (C3: the NCFrameCtrl is 12 wide and as high as the window) and one button that asks the server to end the
// observation.
//
// C3 layout and virtuals from OnCreate 0x100AF700, OnDestroy 0x100AD6C0, OnPaint 0x100AD630, OnExit 0x100AC590.

#include "nwindow/NCWnd.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCButton;

class NCObserverWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCObserverWnd, NCWnd, 0, 0x102E8898)

    UTexture* BackTex;               // +0xE8 "L2UI_ch3.etc.broadcast_back" (C2: "L2UI.ObserverWnd.ObserverWndBack"), a 102 x 32 piece
    NCButton* ExitButton;            // +0xEC id 0x1012, text SysString 611 (name ours)

    NCObserverWnd();                                                 // 0x100bcc40
    // ~NCObserverWnd                                                // 0x100bcc90

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x100b9020
    virtual INT OnCreate();                                          // 37 0x100bb4a0
    virtual INT OnDestroy();                                         // 38 0x100b9fa0
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x100b9f10
    virtual INT OnExit();                                            // 74 0x100b9080 (name ours)
};
NW_ASSERT_OFFSET(NCObserverWnd, BackTex, 0xE8);
NW_ASSERT_OFFSET(NCObserverWnd, ExitButton, 0xEC);
NW_ASSERT_SIZE(NCObserverWnd, 0xF0);

#pragma pack(pop)
