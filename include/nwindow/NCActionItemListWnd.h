#pragma once
// NCActionItemListWnd - the Basic page of the action window (UClass "CActionItemListWnd",
// 0xA4): three icon grids, filled from actionname-*.dat by category (1 basic, 2 party,
// 3 social), under the labels "Party" and "Social".

#include "nwindow/NCWnd.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCActionItemWnd;

class NCActionItemListWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCActionItemListWnd, NCWnd, 0, 0x102CECA0)

    NCActionItemWnd* BasicGrid;            // +0xE8 6 x 2   // C2: offset derived, no evidence
    NCActionItemWnd* PartyGrid;            // +0xEC 6 x 1   // C2: offset derived, no evidence
    NCActionItemWnd* SocialGrid;           // +0xF0 6 x 3   // C2: offset derived, no evidence
    UTexture* BackTex;                     // +0xF4 "L2UI.ActionWnd.ActionBack"   // C2: offset derived, no evidence

    NCActionItemListWnd();                                           // 0x10050600
    // ~NCActionItemListWnd                                          // 0x10040d20 (C1 only: no C2 match) (C1 only: no C2 match)

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10148f10 (folded)
    virtual INT OnCreate();                                          // 37 0x100506a0
    virtual INT OnDestroy();                                         // 38 0x1004d3a0
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x1004d2a0

    void LoadActions();                                              // 0x10050100 (name ours; stock)
    void ClearGrids();                                               // 0x100508a0 (name ours)
};
NW_ASSERT_OFFSET(NCActionItemListWnd, BasicGrid, 0xE8);
NW_ASSERT_OFFSET(NCActionItemListWnd, BackTex, 0xF4);
NW_ASSERT_SIZE(NCActionItemListWnd, 0xF8);

#pragma pack(pop)