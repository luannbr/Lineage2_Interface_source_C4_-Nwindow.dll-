#pragma once
// NHtmlAnalyzer - an empty scroll window (UClass "HtmlAnalyzer", 0x140): four point deques
// (like NCGDevice's origin stack) and a few fields no code of NWindow reads. Its paint only
// runs the base one; nothing in NWindow creates it.

#include "nwindow/NCScrollWnd.h"
#include "nwindow/NStdLib.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NHtmlAnalyzer : public NCScrollWnd
{
    NW_DECLARE_CLASS(NHtmlAnalyzer, NCScrollWnd, 0, 0x102C7100)

    INT UnknownCC;                   // +0x120 (0)
    INT UnknownD0;                   // +0x124 (0)
    INT UnknownD4;                   // +0x128 (0)
    NPointDeque Points1;             // +0x12C
    NPointDeque Points2;             // +0x140
    INT Unknown100;                  // +0x154 (0)
    INT Unknown104;                  // +0x158 (0)
    INT Unknown108;                  // +0x15C (5)
    NPointDeque Points3;             // +0x160
    NPointDeque Points4;             // +0x174
    INT Unknown134[3];               // +0x188 never set   // C2: offset derived, no evidence   // C2: offset derived, no evidence   // C2: offset derived, no evidence

    NHtmlAnalyzer();                                                 // 0x10037f30
    // ~NHtmlAnalyzer                                                // 0x10038000

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10014b90 (shared)
    virtual INT OnCreate();                                          // 37 0x100edae0 (shared)
    virtual INT OnDestroy();                                         // 38 0x10037e20 (shared)
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x10037e30
};
NW_ASSERT_OFFSET(NHtmlAnalyzer, Points1, 0x12C);
NW_ASSERT_OFFSET(NHtmlAnalyzer, Unknown108, 0x15C);
NW_ASSERT_OFFSET(NHtmlAnalyzer, Points4, 0x174);
NW_ASSERT_SIZE(NHtmlAnalyzer, 0x194);

#pragma pack(pop)