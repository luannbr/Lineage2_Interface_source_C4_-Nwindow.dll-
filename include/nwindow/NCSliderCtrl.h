#pragma once
// NCSliderCtrl - a slider (UClass "CSliderCtrl", 0x1BC; C2 only): a black bar with an
// NCThumbButton that follows the mouse while it is dragged; Pos runs from 0 to Max over the
// control's width. Whoever creates it sets Max, Pos and the thumb's size before OnCreate;
// nothing in this class reports the position to its parent.

#include "nwindow/NControl.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCThumbButton;

class NCSliderCtrl : public NControl
{
    NW_DECLARE_CLASS(NCSliderCtrl, NControl, 0, 0x102C49B8)

    UTexture* BackTex;               // +0x1F4 "NWindow.BlackTexture"
    NCThumbButton* Thumb;            // +0x1F8 "NWindow.MenuHighlight" / "NWindow.WhiteTexture"
    INT Max;                         // +0x1FC (name ours) the largest Pos   // C2: offset derived, no evidence   // C2: offset derived, no evidence
    INT Pos;                         // +0x200 (name ours) the thumb's place, 0..Max   // C2: offset derived, no evidence   // C2: offset derived, no evidence
    INT ThumbW;                      // +0x204 (name ours)   // C2: offset derived, no evidence   // C2: offset derived, no evidence
    INT ThumbH;                      // +0x208 (name ours)   // C2: offset derived, no evidence   // C2: offset derived, no evidence

    NCSliderCtrl();                                                  // 0x1002d1a0
    // ~NCSliderCtrl                                                 // 0x10026270 (C1 only: no C2 match) (C1 only: no C2 match)

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x1002c6a0
    virtual INT OnCreate();                                          // 37 0x1002cee0
    virtual INT OnDestroy();                                         // 38 0x1002cce0
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x1002cd80
    virtual INT OnThumbMove(INT X);                                  // 75 0x1002c700 (name ours)
};
NW_ASSERT_OFFSET(NCSliderCtrl, BackTex, 0x1F4);
NW_ASSERT_OFFSET(NCSliderCtrl, Thumb, 0x1F8);
NW_ASSERT_OFFSET(NCSliderCtrl, Max, 0x1FC);
NW_ASSERT_OFFSET(NCSliderCtrl, ThumbH, 0x208);
NW_ASSERT_SIZE(NCSliderCtrl, 0x20C);

#pragma pack(pop)
