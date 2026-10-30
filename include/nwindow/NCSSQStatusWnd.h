#pragma once
// NCSSQStatusWnd - the Seven Signs status page (UClass "CSSQStatusWnd", 0x144; new in C3; the first tab of
// NCSSQMainBoard): the period's text and the cabals' numbers (the console fills +0xC8 .. +0x104 from the server's
// Seven Signs status, two pages: the current one, painted by PaintCurrent, and the previous period's winners, painted
// by PaintPrevious with the NCSSQPreWinnerWnd list under it). Two buttons: the first toggles the page, the second asks
// the console for the page again.

#include "nwindow/NCWnd.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCButton;
class NCSSQPreWinnerWnd;

class NCSSQStatusWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCSSQStatusWnd, NCWnd, 0, 0x10302940)

    INT Unknown0C8;                  // +0xE8 (PaintCurrent: 3 picks the text of the end of the period) (names ours)
    INT Cabal;                       // +0xEC the cabal that leads (SSQCabalName)
    INT Seal;                        // +0xF0 (SSQSealName)
    INT Number;                      // +0xF4 shown as "%d" and a unit text
    INT UnknownD8;                   // +0xF8
    INT UnknownDC;                   // +0xFC
    INT UnknownE0;                   // +0x100
    INT UnknownE4;                   // +0x104
    INT UnknownE8;                   // +0x108 shown as a share bar (x 1.5 pixels) of the cabal 1 on the current page
    INT UnknownEC;                   // +0x10C the same for the cabal 2
    INT UnknownF0;                   // +0x110 a number in the boxes
    INT UnknownF4;                   // +0x114 a number in the boxes
    INT UnknownF8;                   // +0x118 shown as "%d <text>" in the title line
    INT UnknownFC;                   // +0x11C a system message line when > 0 (SystemMsg[n])
    INT Unknown100;                  // +0x120 the same, second
    INT Unknown104;                  // +0x124 shown as "(%d<unit>)"
    NCSSQPreWinnerWnd* PreWinnerWnd; // +0x128 the previous winners' list (8, 103, 240 x 212)
    INT Mode;                        // +0x12C the page of PaintPrevious: 1 or 2 names the cabal, 0 none
    BYTE _Unknown110[0x114 - 0x110]; // +0x130 never touched   // C2: offset derived, no evidence
    UBOOL bShowPrevious;             // +0x134 the previous period's page is shown (name ours)
    NCButton* PageButton;            // +0x138 id 0xFF0F0F11 (text 0x3A9 / 0x3AB) (name ours)
    NCButton* RefreshButton;         // +0x13C id 0xFF0F0F12 (text 0x3AA) (name ours)   // C2: offset derived, no evidence
    UTexture* BackTex;               // +0x140 "L2UI_CH3.SSQWnd.SSQ_back1" (names ours)
    UTexture* BackTex2;              // +0x144 "...SSQ_back1_1"
    UTexture* Bar22Tex;              // +0x148 "...ssq_bar22"
    UTexture* Bar12Tex;              // +0x14C "...ssq_bar12"
    UTexture* Bar2BackTex;           // +0x150 "...ssq_bar2back"
    UTexture* Bar1BackTex;           // +0x154 "...ssq_bar1back"
    UTexture* TextBox1Tex;           // +0x158 "L2UI_CH3.Etc.TextBox1"
    UTexture* TextBox2Tex;           // +0x15C "...TextBox2"
    UTexture* TextBox3Tex;           // +0x160 "...TextBox3"

    NCSSQStatusWnd();                                                // 0x100fc310
    // ~NCSSQStatusWnd                                               // 0x100fc440

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x100fa5a0
    virtual INT OnCreate();                                          // 37 0x100fc490
    virtual void ShowWindow();                                       // 48 0x100fb7b0
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x100fbde0
    // names ours
    virtual INT OnTogglePage();                                      // 74 0x100fb6f0 (0xFF0F0F11)
    virtual INT OnRefresh();                                         // 75 0x100fb770 (0xFF0F0F12)

    void ResetStatus();                                              // 0x100fb690 the numbers cleared, page 1
    void PaintCurrent(NCGDevice* Device);                            // 0x100fa620 (ret 4, 3395 bytes)
    void PaintPrevious(NCGDevice* Device);                           // 0x100fb370 (ret 4)
    virtual INT OnDestroy();  // 38 0x100fbce0 (the stock vtable of this class overrides it; defined in NNewVirtuals.gen.cpp)
    // the body of slot 38, hooked by its address under this name (the slot's own definition is the generated stub of
    // NNewVirtuals.gen.cpp): the winners list and the two buttons go (name ours)
    INT OnDestroyBody();                                             // 0x100fbce0 (guard OnDestroy)
};
NW_ASSERT_OFFSET(NCSSQStatusWnd, Cabal, 0xEC);
NW_ASSERT_OFFSET(NCSSQStatusWnd, UnknownF8, 0x118);
NW_ASSERT_OFFSET(NCSSQStatusWnd, PreWinnerWnd, 0x128);
NW_ASSERT_OFFSET(NCSSQStatusWnd, Mode, 0x12C);
NW_ASSERT_OFFSET(NCSSQStatusWnd, bShowPrevious, 0x134);
NW_ASSERT_OFFSET(NCSSQStatusWnd, BackTex, 0x140);
NW_ASSERT_SIZE(NCSSQStatusWnd, 0x164);

#pragma pack(pop)
