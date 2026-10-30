#pragma once
// NCWarningWnd - the login screen's "Warning" box (UClass "CWarningWnd", 0xA8; NCLoginWnd
// creates it): a text in an NCNoticeInfoWnd and an OK button (or Enter) that asks for the
// server list. A copy of NCNoticeWnd's code: even its OnPaint guard is NCNoticeWnd's.

#include "nwindow/NCWnd.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCButton;
class NCNoticeInfoWnd;

class NCWarningWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCWarningWnd, NCWnd, 0, 0x102EA888)

    INT TimerResult;                 // +0xE8 SetTimer's (ShowWindow)
    INT Unknown98;                   // +0xEC (0) cleared by ShowWindow / HideWindow
    UTexture* BackTex;               // +0xF0 "L2UI.etcWnd.Announce_back" below the title bar
    NCNoticeInfoWnd* InfoWnd;        // +0xF4 the text
    NCButton* OKButton;              // +0xF8

    NCWarningWnd();                                                  // 0x100be6d0
    // ~NCWarningWnd                                                 // 0x1009b240 (C1 only: no C2 match) (C1 only: no C2 match)

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x100bd8c0
    virtual INT OnTimer(INT TimerID) { return 1; }                   // 36 0x100f76f0 (folded)
    virtual INT OnCreate();                                          // 37 0x100be770
    virtual INT OnDestroy();                                         // 38 0x100bdda0
    virtual void ShowWindow();                                       // 48 0x100bd9b0
    virtual void HideWindow();                                       // 49 0x100bd9e0
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x100bde70
    virtual INT OnKeyDown(WPARAM wParam, LPARAM lParam);             // 65 0x100bd970
    // names ours
    virtual INT OnOK();                                              // 74 0x100bd950 server list

    void SetText(const TCHAR* Text);                                 // 0x1009a820 (C1 only: no C2 match) (C1 only: no C2 match) empty: ignored
    void ClearText();                                                // 0x1009a840 (C1 only: no C2 match) (C1 only: no C2 match)
};
NW_ASSERT_OFFSET(NCWarningWnd, TimerResult, 0xE8);
NW_ASSERT_OFFSET(NCWarningWnd, BackTex, 0xF0);
NW_ASSERT_OFFSET(NCWarningWnd, OKButton, 0xF8);
NW_ASSERT_SIZE(NCWarningWnd, 0xFC);

#pragma pack(pop)