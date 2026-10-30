#pragma once
// NCNameCtrl - a control that writes a name in the width it has (UClass "CNameCtrl", 0x2A4, super NControl; C4 only): the name of a
// competitor of the olympiad windows (Kind 0: the text, cut with ".." when it is wider than the control, left aligned or centered by
// the Align) or an item's name (Kind 1: "+enchant name", the additional name in yellow, the crystal grade between quotes and the count
// after it, the name cut with ".." when the whole is wider than the control). The two painters are 0x1002CB30 and 0x1002C7C0
// (anonymous in the db). A tool tip can be shown under the mouse.
//
// Layout and virtuals from the stock constructor 0x1002D260, destructor 0x1002D310, OnPaint 0x1002CE20 and OnDrawToolTip 0x1002D390
// (the db says OnPostPaint).

#include "nwindow/NControl.h"
#pragma pack(push, 4)

class NCNameCtrl : public NControl
{
    NW_DECLARE_CLASS(NCNameCtrl, NControl, 0, 0x102C4EE8)

    INT Kind;                        // +0x1F4 which painter OnPaint calls: 0 sub_1002CB30, 1 sub_1002C7C0 (names ours)
    INT Align;                       // +0x1F8 0 left, 1 centered (sub_1002CB30)
    FString Name;                    // +0x1FC the name
    INT ItemID;                      // +0x208 Kind 1: the item (its additional name is looked up) (names ours)
    INT ItemType;                    // +0x20C Kind 1: 0 .. 3 (weapon, armor, jewel, etc.: the enchant shows)
    INT Enchant;                     // +0x210 Kind 1: the "+N" before the name (shown when > 0 and the type is 0 .. 3)
    INT Grade;                       // +0x214 Kind 1: the crystal grade 0 .. 4 (-1 none), written between quotes after the name
    INT Count;                       // +0x218 Kind 1: the amount, written "xN" (when > 0)
    DWORD Color;                     // +0x21C Kind 0: the text color
    NToolTipInfo ToolTip;            // +0x220 shown by OnDrawToolTip while bToolTip
    BYTE bToolTip;                   // +0x2A0 (1 from the constructor)

    NCNameCtrl();                                                    // 0x1002D260
    // ~NCNameCtrl                                                   // 0x1002D310

    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x1002CE20
    virtual INT OnDrawToolTip(NCGDevice* Device);                    // 63 0x1002D390 (the db says OnPostPaint)

    // the two painters (anonymous in the db; ret 4; names ours), out of line as the stock's
    __declspec(noinline) void PaintName(NCGDevice* Device);          // 0x1002CB30 Kind 0
    __declspec(noinline) void PaintItemName(NCGDevice* Device);      // 0x1002C7C0 Kind 1
};
NW_ASSERT_OFFSET(NCNameCtrl, Kind, 0x1F4);
NW_ASSERT_OFFSET(NCNameCtrl, Name, 0x1FC);
NW_ASSERT_OFFSET(NCNameCtrl, ItemID, 0x208);
NW_ASSERT_OFFSET(NCNameCtrl, Count, 0x218);
NW_ASSERT_OFFSET(NCNameCtrl, Color, 0x21C);
NW_ASSERT_OFFSET(NCNameCtrl, ToolTip, 0x220);
NW_ASSERT_OFFSET(NCNameCtrl, bToolTip, 0x2A0);
NW_ASSERT_SIZE(NCNameCtrl, 0x2A4);

#pragma pack(pop)
