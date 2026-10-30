#pragma once
// NCHtmlSliderBar - a scroll bar thumb inside an HTML page (UClass "CHtmlSliderBar", 0x1CC):
// NCSliderBar clipped to its first ancestor that is not a control (the scroll bar of an HTML
// multi-line edit box).

#include "nwindow/NCSliderBar.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCHtmlSliderBar : public NCSliderBar
{
    NW_DECLARE_CLASS(NCHtmlSliderBar, NCSliderBar, 0, 0x102CD2A8)

    NCHtmlSliderBar();                                               // 0x1004b0a0
    NCHtmlSliderBar(const TCHAR* TopTexName, const TCHAR* MidTexName,
                    const TCHAR* BottomTexName);                     // 0x10033060
    // ~NCHtmlSliderBar                                              // 0x1003d800 (C1 only: no C2 match) (C1 only: no C2 match)

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x100e9f60 (shared)
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x1004a2c0
};
NW_ASSERT_SIZE(NCHtmlSliderBar, 0x220);

#pragma pack(pop)