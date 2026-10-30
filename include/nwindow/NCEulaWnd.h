#pragma once
// NCEulaWnd - the license agreement box of the login screen (UClass "CEulaWnd", 0xAC; C2 only;
// NCLoginWnd +0xA4): the text in an NCNoticeInfoWnd and two buttons, agree (or Enter), which
// goes on to the announcements (NCNoticeWnd, with the EULA text) when there is one or else
// asks for the server list, and the other one, which drops the connection and goes back to
// the login box.

#include "nwindow/NCWnd.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCButton;
class NCNoticeInfoWnd;

class NCEulaWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCEulaWnd, NCWnd, 0, 0x102E9E20)

    UTexture* BackTex;               // +0xE8 "L2UI.etcWnd.Announce_back" below the title bar
    NCNoticeInfoWnd* InfoWnd;        // +0xEC the text
    NCButton* AgreeButton;           // +0xF0 text 599 (name ours)
    NCButton* DisagreeButton;        // +0xF4 text 600 (name ours)
    INT Type;                        // +0xF8 (name ours) set from outside; agreeing gives it to the notice box's Type   // C2: offset derived, no evidence   // C2: offset derived, no evidence

    NCEulaWnd();                                                     // 0x100be910
    // ~NCEulaWnd                                                    // 0x1009b6a0 (C1 only: no C2 match) (C1 only: no C2 match)

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x100bda00
    virtual INT OnCreate();                                          // 37 0x100be9b0
    virtual INT OnDestroy();                                         // 38 0x100bdf70
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x100be070
    virtual INT OnKeyDown(WPARAM wParam, LPARAM lParam);             // 65 0x100bdb30
    virtual INT OnAgree();                                           // 74 0x100be170 (name ours)
    virtual INT OnDisagree();                                        // 75 0x100bdab0 (name ours)

    // names ours (the text box's, like NCWarningWnd's)
    void SetText(const TCHAR* Text);                                 // 0x100bda80 empty: ignored
    void ClearText();                                                // 0x100bdaa0
};
NW_ASSERT_OFFSET(NCEulaWnd, BackTex, 0xE8);
NW_ASSERT_OFFSET(NCEulaWnd, InfoWnd, 0xEC);
NW_ASSERT_OFFSET(NCEulaWnd, DisagreeButton, 0xF4);
NW_ASSERT_OFFSET(NCEulaWnd, Type, 0xF8);
NW_ASSERT_SIZE(NCEulaWnd, 0xFC);

#pragma pack(pop)
