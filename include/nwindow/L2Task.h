#pragma once
// L2Task - Task's Lineage II skin (UClass "L2Task", 0xE4; C3 only, the messenger): the entry that stands for a
// conversation in L2TaskWnd's list. The same blinking Task as the generic one, with the name of the friend it is
// about (an FString at +0xC8) in front of the other fields, and the blink timer 0xA73.

#include "nwindow/NCWnd.h"
#pragma pack(push, 4)

class L2Task : public NCWnd
{
    NW_DECLARE_CLASS(L2Task, NCWnd, 0, 0x10321270)

    FString Name;                    // +0xE8 (name ours) the friend: L2TaskWnd looks an entry up by it (0x101255D0)
    NCWnd* ChatWindow;               // +0xF4 (name ours) the conversation window (L2ChatWnd) the entry opens
    INT bAlert;                      // +0xF8 (name ours) a message is waiting: the entry blinks until it is clicked
    INT bBlink;                      // +0xFC (name ours) flipped by the timer every second
    INT TimerHandle;                 // +0x100 (name ours) what SetTimer(0xA73, 1000) returned

    L2Task();                                                        // 0x1014a8b0
    // ~L2Task                                                       // 0x1014a950

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10148f10 (NControl's body, folded)
    virtual INT OnTimer(INT TimerID);                                // 36 0x10148e50
    virtual INT OnCreate();                                          // 37 0x10148e20
    virtual INT OnDestroy();                                         // 38 0x10148e80
};
NW_ASSERT_OFFSET(L2Task, Name, 0xE8);
NW_ASSERT_OFFSET(L2Task, ChatWindow, 0xF4);
NW_ASSERT_OFFSET(L2Task, bAlert, 0xF8);
NW_ASSERT_OFFSET(L2Task, bBlink, 0xFC);
NW_ASSERT_OFFSET(L2Task, TimerHandle, 0x100);
NW_ASSERT_SIZE(L2Task, 0x104);

#pragma pack(pop)
