#pragma once
// NCCalcWnd - the calculator (UClass "CCalcWnd", 0xF0; C2 only): a title bar, the display
// (NCCalcEditCtrl) and a keypad of 19 buttons whose clicks are sent to the display as typed
// characters, plus a close button. The window takes the keyboard focus for its display.

#include "nwindow/NCWnd.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCButton;
class NCCalcEditCtrl;

class NCCalcWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCCalcWnd, NCWnd, 0, 0x1030EED0)

    UTexture* BackTex;               // +0xE8 "l2ui.calcwnd.calcwnd_back"
    NCCalcEditCtrl* Display;         // +0xEC (name ours)
    NCButton* Keys[19];              // +0xF0 the keypad, NotifyID = the key's number (name ours):
                                     //   0 7, 1 8, 2 9, 3 +, 4 clear entry, 5 4, 6 5, 7 6, 8 -, 9 clear all,
                                     //   10 1, 11 2, 12 3, 13 *, 14 backspace, 15 0, 16 00, 17 /, 18 =
    NCButton* CloseButton;           // +0x13C text 646 (name ours)

    NCCalcWnd();                                                     // 0x1011faf0
    // ~NCCalcWnd                                                    // 0x100ded00 (C1 only: no C2 match) (C1 only: no C2 match)

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x1011ec30
    virtual void SetFocus();                                         // 29 0x1011ecb0
    virtual INT OnCreate();                                          // 37 0x1011fb90
    virtual INT OnDestroy();                                         // 38 0x1011f030
    virtual void ShowWindow();                                       // 48 0x1011f970
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x1011f310
    virtual INT OnKey(INT Key);                                      // 74 0x1011f140 (name ours)
    virtual INT OnClose();                                           // 75 0x1011f950 (name ours)

    // name ours
    __declspec(noinline) void Reset();                               // 0x1011f420 the display's Reset
};
NW_ASSERT_OFFSET(NCCalcWnd, BackTex, 0xE8);
NW_ASSERT_OFFSET(NCCalcWnd, Display, 0xEC);
NW_ASSERT_OFFSET(NCCalcWnd, Keys, 0xF0);
NW_ASSERT_OFFSET(NCCalcWnd, CloseButton, 0x13C);
NW_ASSERT_SIZE(NCCalcWnd, 0x140);

#pragma pack(pop)
