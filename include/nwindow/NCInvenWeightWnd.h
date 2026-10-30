#pragma once
// NCInvenWeightWnd - the weight bar of the inventory window (UClass "CInvenWeightWnd", 0xFC; C4 only): a bar 85 pixels wide at most,
// filled by the weight carried over the weight the player can carry, in one of five colors by how full it is, and the percentage
// above it; the tool tip on top shows "[carried]/[limit]". The inventory window makes it.
//
// Layout and virtuals from the stock constructor 0x10099840, OnCreate 0x10093D40, OnDestroy 0x10093E20, OnPaint 0x10095630 and
// OnDrawToolTip (63) 0x10098A50.

#include "nwindow/NCWnd.h"
#pragma pack(push, 4)

class NCInvenWeightWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCInvenWeightWnd, NCWnd, 0, 0x102DA3D8)

    UTexture* WeightTex[5];          // +0xE8 "L2UI_CH3.PlayerStatusWnd.PS_weightbar1" .. "5": up to 50 %, 66.6 %, 80 %, 100 %, over 100 %

    NCInvenWeightWnd();                                              // 0x10099840
    // ~NCInvenWeightWnd                                             // 0x100998B0

    virtual INT OnCreate();                                          // 37 0x10093D40
    virtual INT OnDestroy();                                         // 38 0x10093E20
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x10095630
    virtual INT OnDrawToolTip(NCGDevice* Device);                    // 63 0x10098A50 (the db says OnPostPaint)
};
NW_ASSERT_OFFSET(NCInvenWeightWnd, WeightTex, 0xE8);
NW_ASSERT_SIZE(NCInvenWeightWnd, 0xFC);

#pragma pack(pop)
