#pragma once
// NCSSQPreWinnerWnd - the previous period's winners list of the Seven Signs status page (UClass "CSSQPreWinnerWnd",
// 0x128; new in C3): a scrolling list, a block per seal: its name, the winner (a cabal's name, or a "-" line) and a
// text line of the system messages; every second row sits on the band texture. The console fills the three
// arrays (a seal id, the winning cabal and a system message id, one entry each).

#include "nwindow/NCScrollWnd.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCSSQPreWinnerWnd : public NCScrollWnd
{
    NW_DECLARE_CLASS(NCSSQPreWinnerWnd, NCScrollWnd, 0, 0x10302410)

    UTexture* LineTex;               // +0x120 "L2UI_CH3.Etc.TextBackline" (name ours)
    TArray<INT> Seals;               // +0x124 SSQSealName's argument (name ours)
    TArray<INT> Winners;             // +0x130 1 / 2 the winning cabal (SSQCabalName), anything else "no winner" (name ours)
    TArray<INT> Messages;            // +0x13C a system message id: its text under the seal's name (name ours)

    NCSSQPreWinnerWnd();                                             // 0x100fc1d0
    // ~NCSSQPreWinnerWnd                                            // 0x100fc290 (the three arrays)

    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x100fb930
    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10014b90 (the stock vtable of this class overrides it; defined in NNewVirtuals.gen.cpp)
};
NW_ASSERT_OFFSET(NCSSQPreWinnerWnd, LineTex, 0x120);
NW_ASSERT_OFFSET(NCSSQPreWinnerWnd, Seals, 0x124);
NW_ASSERT_OFFSET(NCSSQPreWinnerWnd, Winners, 0x130);
NW_ASSERT_OFFSET(NCSSQPreWinnerWnd, Messages, 0x13C);
NW_ASSERT_SIZE(NCSSQPreWinnerWnd, 0x148);

#pragma pack(pop)
