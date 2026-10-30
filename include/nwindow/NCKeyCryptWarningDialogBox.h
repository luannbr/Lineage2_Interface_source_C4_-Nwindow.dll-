#pragma once
// NCKeyCryptWarningDialogBox - the keyboard protection warning of the login screen (UClass
// "CKeyCryptWarningDialogBox", 0x1AC; C2 only; NCLoginWnd +0xB0): nProtect's keyboard
// protection (npkcrypt) is not running; a box with system message 1239 and an OK button that
// closes it.

#include "nwindow/NCDialogBox.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCButton;

class NCKeyCryptWarningDialogBox : public NCDialogBox
{
    NW_DECLARE_CLASS(NCKeyCryptWarningDialogBox, NCDialogBox, 0, 0x102DECB0)

    UTexture* BackTex;               // +0x1F4 "L2UI.EtcWnd.Popup"
    NCButton* OKButton;              // +0x1F8 id 0x1004, centered, text SysString 140

    NCKeyCryptWarningDialogBox();                                    // 0x1009f590
    // ~NCKeyCryptWarningDialogBox                                   // 0x10082d30 (C1 only: no C2 match) (C1 only: no C2 match)

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x100084a0
    virtual INT OnCreate();                                          // 37 0x1009f100
    virtual INT OnDestroy();                                         // 38 0x1009e8a0
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x1009e950
    virtual INT OnOK();                                              // 76 0x1009e310 (name ours)
};
NW_ASSERT_OFFSET(NCKeyCryptWarningDialogBox, BackTex, 0x1F4);
NW_ASSERT_OFFSET(NCKeyCryptWarningDialogBox, OKButton, 0x1F8);
NW_ASSERT_SIZE(NCKeyCryptWarningDialogBox, 0x1FC);

#pragma pack(pop)
