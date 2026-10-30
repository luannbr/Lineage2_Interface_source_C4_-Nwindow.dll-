#pragma once
// L2ChatFrameCtrl - ChatFrameCtrl's Lineage II skin (UClass "L2ChatFrameCtrl", 0x238; C3 only, the messenger): the title
// bar of L2ChatWnd. Its buttons tell the parent like ChatFrameCtrl's; the bar itself is painted by NCFrameCtrl.

#include "nwindow/NCFrameCtrl.h"
#pragma pack(push, 4)

class L2ChatFrameCtrl : public NCFrameCtrl
{
    NW_DECLARE_CLASS(L2ChatFrameCtrl, NCFrameCtrl, 0, 0x103217A0)

    L2ChatFrameCtrl();                                               // 0x1014a350
    L2ChatFrameCtrl(INT InDecorated, INT InVertical, INT InSmall);   // 0x1014a3f0 (anonymous in the db) (ret 0xC)
    // ~L2ChatFrameCtrl                                              // 0x1014a3a0

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x1013e340 (ChatFrameCtrl's, shared)
    virtual INT OnClose();                                           // 76 0x10148d40 (ChatFrameCtrl's, shared)
    virtual INT OnMinimize();                                        // 77 0x10148d10 (ChatFrameCtrl's, shared)
};
NW_ASSERT_SIZE(L2ChatFrameCtrl, 0x26C);

#pragma pack(pop)
