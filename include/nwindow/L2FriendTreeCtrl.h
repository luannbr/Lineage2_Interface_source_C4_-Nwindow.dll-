#pragma once
// L2FriendTreeCtrl - the scrolling tree of the Lineage II friend list (UClass "CL2FriendTreeCtrl", 0x11C; C3 only, no C2
// source): like MsnTreeCtrl, an NCTreeCtrl that keeps its own tree (L2FriendTree, at +0x118; the base's +0x100 stays empty)
// and repeats the base's virtuals on that field. L2FriendWnd makes it.
//
// Layout and virtuals from the stock constructor 0x10143A00, OnCreate 0x10143AA0, OnDestroy 0x10142210, OnPaint 0x10142250,
// Clear 0x10142330, ScrollUp / ScrollDown / OnScrollBar / OnMouseWheel 0x101423C0 .. 0x101425D0, WindowProc 0x10141B60.

#include "nwindow/NCTreeCtrl.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class L2FriendTree;

class L2FriendTreeCtrl : public NCTreeCtrl
{
    NW_DECLARE_CLASS(L2FriendTreeCtrl, NCTreeCtrl, 0, 0x10325B30)

    INT Unknown114;                  // +0x134 the constructor clears it; nothing uses it
    L2FriendTree* FriendTree;        // +0x138 id 0xA01, as wide as the control less the scroll bar (name ours)

    L2FriendTreeCtrl();                                              // 0x101683c0
    // ~L2FriendTreeCtrl                                             // 0x10168410

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10166b60 (also MsnTreeCtrl's)
    virtual INT OnCreate();                                          // 37 0x10168460
    virtual INT OnDestroy();                                         // 38 0x10166bc0
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x10166c00
    virtual INT OnMouseWheel(WPARAM wParam, LPARAM lParam);          // 73 0x10166f80
    virtual INT ScrollUp();                                          // 75 0x10166d70
    virtual INT ScrollDown();                                        // 76 0x10166e30
    virtual INT OnScrollBar(INT Value);                              // 77 0x10166f00
    virtual void Clear();                                            // 78 0x10166ce0

    // name ours (anonymous in the db; ret 8): a double click is passed up to the parent's parent (both tree controls)
    __declspec(noinline) INT OnDoubleClick(WPARAM wParam, LPARAM lParam);   // 0x10166530
};
NW_ASSERT_OFFSET(L2FriendTreeCtrl, FriendTree, 0x138);
NW_ASSERT_SIZE(L2FriendTreeCtrl, 0x13C);

#pragma pack(pop)
