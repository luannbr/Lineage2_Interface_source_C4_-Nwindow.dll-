#pragma once
// NCAutoLogWnd - a development screen that logs in by itself (UClass "CAutoLogWnd", 0xAC):
// black, a progress line, and on its first paint the login with the account and password of
// AutoLog.txt ([AutoLogOn] L2ID and L2Passwd, "zodiac" when missing; L2Slot is read too, and
// nothing uses it). Nothing in NWindow creates it, and nothing moves Step past the first line.

#include "nwindow/NCWnd.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

// what OnCreate reads from AutoLog.txt (names ours), and the progress lines: "1.Authorizing....",
// "2.Send infomations of character to server..", "3.Loading World......", "Can not connect to
// server" (name ours)
#if NW_HOOK_MODE
#define GAutoLogID       (reinterpret_cast<TCHAR*>(::nw::Addr(0x102CF250)))   // TCHAR[64]
#define GAutoLogPassword (reinterpret_cast<TCHAR*>(::nw::Addr(0x102CF1D0)))   // TCHAR[64]
#define GAutoLogSlot     NW_GLOBAL(INT, 0x102CF2D0)
#define GAutoLogMessages (reinterpret_cast<const TCHAR* const*>(::nw::Addr(0x1023C808)))// C3: four pointers 0x10200AF4 .. 0x10200B00 (OnPaint 0x1004b340)
#else
extern TCHAR GAutoLogID[64];
extern TCHAR GAutoLogPassword[64];
extern INT GAutoLogSlot;
extern const TCHAR* const GAutoLogMessages[];
#endif

class NCAutoLogWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCAutoLogWnd, NCWnd, 0, 0x102CF2D8)

    UTexture* BackTex;               // +0xE8 "NWindow.BlackTexture"
    UBOOL bLoginSent;                // +0xEC
    INT Unknown9C;                   // +0xF0 (0)
    INT Step;                        // +0xF4 lines 0..Step show; 3 shows the fourth alone
    INT TextX;                       // +0xF8 40% of the width   // C2: offset derived, no evidence
    INT TextY;                       // +0xFC a quarter of the height   // C2: offset derived, no evidence

    NCAutoLogWnd();                                                  // 0x10050f60
    // ~NCAutoLogWnd                                                 // 0x10050fc0

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10148f10 (folded)
    virtual INT OnCreate();                                          // 37 0x10050ca0
    virtual INT OnDestroy();                                         // 38 0x10050d90 (folded)
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x10050e10
};
NW_ASSERT_OFFSET(NCAutoLogWnd, BackTex, 0xE8);
NW_ASSERT_OFFSET(NCAutoLogWnd, Step, 0xF4);
NW_ASSERT_SIZE(NCAutoLogWnd, 0x100);

#pragma pack(pop)