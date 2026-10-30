#pragma once
// NCAnnounceWnd - the announce box of the login screen (UClass "CAnnounceWnd", 0xA4; C2 only;
// NCLoginWnd +0xA8): a picture ("Announce_cn") and an OK button (or Enter) that shows the
// announcements (NCNoticeWnd, with the EULA text) when there is one, or else asks for the
// server list.

#include "nwindow/NCWnd.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCButton;

class NCAnnounceWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCAnnounceWnd, NCWnd, 0, 0x102EA350)

    UTexture* BackTex;               // +0xE8 "L2UI.etcWnd.Announce_cn" below the title bar   // C2: offset derived, no evidence
    NCButton* OKButton;              // +0xEC
    INT Type;                        // +0xF0 (name ours) set from outside; OK gives it to the notice box's Type   // C2: offset derived, no evidence

    NCAnnounceWnd();                                                 // 0x100bec00
    // ~NCAnnounceWnd                                                // 0x1009be20 (C1 only: no C2 match) (C1 only: no C2 match)

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x100bdb70
    virtual INT OnCreate();                                          // 37 0x100be520
    virtual INT OnDestroy();                                         // 38 0x100be2c0
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x100be370
    virtual INT OnKeyDown(WPARAM wParam, LPARAM lParam);             // 65 0x100be4b0
    virtual INT OnOK();                                              // 74 0x100be470 (name ours)
};
NW_ASSERT_OFFSET(NCAnnounceWnd, BackTex, 0xE8);
NW_ASSERT_OFFSET(NCAnnounceWnd, OKButton, 0xEC);
NW_ASSERT_OFFSET(NCAnnounceWnd, Type, 0xF0);
NW_ASSERT_SIZE(NCAnnounceWnd, 0xF4);

#pragma pack(pop)
