#pragma once
// Task - one entry of the messenger's task list (UClass "Task", 0xDC; C3 only): the invisible window that stands for a
// conversation window in TaskWnd's list. It blinks while the conversation has an unread message: a one-second timer
// toggles bBlink, and TaskBox paints the entry lit while both flags are on. Clicking the entry in the list opens the
// conversation (TaskBox::OnLButtonUp).

#include "nwindow/NCWnd.h"
#pragma pack(push, 4)

class Task : public NCWnd
{
    NW_DECLARE_CLASS(Task, NCWnd, 0, 0x1031C9B0)

    INT Unknown_C8;                  // +0xE8 (name ours) cleared by the constructor, never used
    NCWnd* ChatWindow;               // +0xEC (name ours) the conversation window (ChatWnd) the entry opens
    INT bAlert;                      // +0xF0 (name ours) a message is waiting: the entry blinks until it is clicked
    INT bBlink;                      // +0xF4 (name ours) flipped by the timer every second
    INT TimerHandle;                 // +0xF8 (name ours) what SetTimer(0xA15, 1000) returned; killed by OnDestroy

    Task();                                                          // 0x10143230
    // ~Task                                                         // 0x101432a0

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10148f10 (NControl's body, folded)
    virtual INT OnTimer(INT TimerID);                                // 36 0x1013e6e0
    virtual INT OnCreate();                                          // 37 0x1013e6b0
    virtual INT OnDestroy();                                         // 38 0x1013e710
};
NW_ASSERT_OFFSET(Task, ChatWindow, 0xEC);
NW_ASSERT_OFFSET(Task, bAlert, 0xF0);
NW_ASSERT_OFFSET(Task, bBlink, 0xF4);
NW_ASSERT_OFFSET(Task, TimerHandle, 0xF8);
NW_ASSERT_SIZE(Task, 0xFC);

#pragma pack(pop)
