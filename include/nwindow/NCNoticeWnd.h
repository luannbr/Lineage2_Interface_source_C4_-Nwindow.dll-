#pragma once
// NCNoticeWnd - the announcements box of the login screen (UClass "CNoticeWnd", 0xA4;
// NCLoginWnd +0x98): the text the login server sends, scrolling in an NCNoticeInfoWnd, and
// OK (or Enter), which goes on to the warning box when the notice says so (Type bit 0) or
// else asks for the server list.

#include "nwindow/NCScrollWnd.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCButton;

// NCNoticeInfoWnd - the scrolling text of the box (UClass "CNoticeInfoWnd", 0x200CC)
class NCNoticeInfoWnd : public NCScrollWnd
{
    NW_DECLARE_CLASS(NCNoticeInfoWnd, NCScrollWnd, 0, 0x102EADC0)

    UTexture* BackTex;               // +0x120 "l2ui_ch3.etc.textbackline" (C3: the stripes of the odd rows; OnCreate 0x100b09d0)
    TCHAR Text[65536];               // +0x124 (SetText 0x100b0a80 / ClearText 0x100b0ab0 / the constructor store here)

    NCNoticeInfoWnd();                                               // 0x100be620 (+0xFC = 16)
    // ~NCNoticeInfoWnd                                              // 0x100be680

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10014b90 (folded)
    virtual INT OnCreate();                                          // 37 0x100bd7d0
    virtual INT OnDestroy();                                         // 38 0x100bd800
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x100bdbd0

    // names ours
    void SetText(const TCHAR* InText);                               // 0x100bd880
    void ClearText();                                                // 0x100bd8b0
};
NW_ASSERT_OFFSET(NCNoticeInfoWnd, BackTex, 0x120);
NW_ASSERT_OFFSET(NCNoticeInfoWnd, Text, 0x124);
NW_ASSERT_SIZE(NCNoticeInfoWnd, 0x20124);

// C3: class NCNoticeWnd gone (no counterpart): class NCNoticeWnd : public NCWnd
// C3: class NCNoticeWnd gone (no counterpart): {
// C3: class NCNoticeWnd gone (no counterpart):     NW_DECLARE_CLASS(NCNoticeWnd, NCWnd, 0, 0x101C56B0)

// C3: class NCNoticeWnd gone (no counterpart):     INT Type;                        // +0x98 bit 0: the warning box comes next
// C3: class NCNoticeWnd gone (no counterpart):     UTexture* BackTex;               // +0x9C "L2UI.etcWnd.Announce_back"
// C3: class NCNoticeWnd gone (no counterpart):     NCNoticeInfoWnd* InfoWnd;        // +0xA0
// C3: class NCNoticeWnd gone (no counterpart):     NCButton* OKButton;              // +0xA4

// C3: class NCNoticeWnd gone (no counterpart):     NCNoticeWnd();                                                   // 0x1009acf0
// C3: class NCNoticeWnd gone (no counterpart):     // ~NCNoticeWnd                                                  // 0x1009ad40

// C3: class NCNoticeWnd gone (no counterpart):     virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x1009a7c0
// C3: class NCNoticeWnd gone (no counterpart):     virtual INT OnCreate();                                          // 36 0x1009ad90
// C3: class NCNoticeWnd gone (no counterpart):     virtual INT OnDestroy();                                         // 37 0x1009afa0
// C3: class NCNoticeWnd gone (no counterpart):     virtual INT OnPaint(NCGDevice* Device);                          // 55 0x1009b070
// C3: class NCNoticeWnd gone (no counterpart):     virtual INT OnKeyDown(WPARAM wParam, LPARAM lParam);             // 58 0x1009b190
// C3: class NCNoticeWnd gone (no counterpart):     virtual INT OnOK();                                              // 67 0x1009b150 (name ours)

// C3: class NCNoticeWnd gone (no counterpart):     // names ours
// C3: class NCNoticeWnd gone (no counterpart):     void SetText(const TCHAR* InText);                               // 0x1009a820
// C3: class NCNoticeWnd gone (no counterpart):     void ClearText();                                                // 0x1009a840
// C3: class NCNoticeWnd gone (no counterpart): };
// C3: class NCNoticeWnd gone (no counterpart): NW_ASSERT_OFFSET(NCNoticeWnd, BackTex, 0x9C);
// C3: class NCNoticeWnd gone (no counterpart): NW_ASSERT_OFFSET(NCNoticeWnd, OKButton, 0xA4);
// C3: class NCNoticeWnd gone (no counterpart): NW_ASSERT_SIZE(NCNoticeWnd, 0xA8);

#pragma pack(pop)