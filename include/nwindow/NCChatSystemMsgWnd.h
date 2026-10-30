#pragma once
// NCChatSystemMsgWnd - the system message list of the chat window (UClass "CChatSystemMsgWnd", 0x288; C4 only): an NCChatListBox
// with a picture of its own behind it (the 128-high strip "chatting_system") that sits above the chat and shows the system messages
// (the chat window makes it with the 3-argument constructor). Showing it moves the chat's filter panel up by its height
// (RearrangePos).
//
// Layout and virtuals from the stock constructors 0x1005A580 / 0x1005A620, OnCreate 0x100540F0, OnDestroy 0x10052B50, ShowWindow
// 0x10056880, OnPaint 0x10056900 and RearrangePos 0x10053FD0.

#include "nwindow/NCChatListBox.h"
#pragma pack(push, 4)

class NCChatSystemMsgWnd : public NCChatListBox
{
    NW_DECLARE_CLASS(NCChatSystemMsgWnd, NCChatListBox, 0, 0x102D1738)

    UTexture* BackTex;               // +0x284 "L2UI_CH3.ChatWnd.chatting_system" (drawn from the top, 128 high)

    NCChatSystemMsgWnd();                                            // 0x1005A580
    NCChatSystemMsgWnd(INT InMaxLines, INT InVisibleLines, INT Unused);   // 0x1005A620 (ret 0xC)
    // ~NCChatSystemMsgWnd                                           // 0x1005A5D0

    virtual INT OnCreate();                                          // 37 0x100540F0
    virtual INT OnDestroy();                                         // 38 0x10052B50
    virtual void ShowWindow();                                       // 48 0x10056880
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x10056900

    // 0x10053FD0 (guard name): the desktop docks the three icons above the chat, then the chat's filter panel moves to where the
    // list is (above the chat, 0x141 higher while the list shows, else 0xBF)
    void RearrangePos();
};
NW_ASSERT_OFFSET(NCChatSystemMsgWnd, BackTex, 0x284);
NW_ASSERT_SIZE(NCChatSystemMsgWnd, 0x288);

#pragma pack(pop)
