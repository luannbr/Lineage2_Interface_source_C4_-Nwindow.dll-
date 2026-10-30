#pragma once
// ChatFrameCtrl - the title bar of a conversation window (UClass "ChatFrameCtrl", 0x238; C3 only, the messenger). A
// frame control whose buttons do not act on the window but tell the parent (NWM_COMMAND with the frame's minimize /
// close ids), and whose bar shows "<title> - <SysString 1002>" cut to the width.

#include "nwindow/NCFrameCtrl.h"
#pragma pack(push, 4)

class ChatFrameCtrl : public NCFrameCtrl
{
    NW_DECLARE_CLASS(ChatFrameCtrl, NCFrameCtrl, 0, 0x1031B4E0)

    ChatFrameCtrl();                                                 // 0x101426c0
    ChatFrameCtrl(INT InDecorated, INT InVertical, INT InSmall);     // 0x10142760 (anonymous in the db) (ret 0xC)
    // ~ChatFrameCtrl                                                // 0x10142710

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x1013e340 (shared with L2ChatFrameCtrl)
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x1013f430
    virtual INT OnClose();                                           // 76 0x10148d40 the parent hears NID_FRAME_CLOSE (shared)
    virtual INT OnMinimize();                                        // 77 0x10148d10 the parent hears NID_FRAME_MINIMIZE (shared)
};
NW_ASSERT_SIZE(ChatFrameCtrl, 0x26C);

#pragma pack(pop)
