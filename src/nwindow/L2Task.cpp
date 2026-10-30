/*=============================================================================
    L2Task.cpp - an entry of the Lineage II skin's task list (C3 only; C3 addresses). Stock: OnCreate / OnTimer /
    OnDestroy 0x10124300 .. 0x1012438f and the constructor / destructor 0x10125de0 .. 0x10125f2f.
=============================================================================*/
#include "nwindow/L2Task.h"

enum { TIMER_L2TASK_BLINK = 0xA73 };

// 0x1014a8b0 - the base window's constructor, the name empty, the other fields too
L2Task::L2Task()
{
    ChatWindow = NULL;
    bAlert = 0;
    bBlink = 0;
    TimerHandle = 0;
}
NW_HOOK_CTOR(0x1014a8b0, L2Task)

// 0x1014a950 - the name goes, then the base
NW_HOOK_DTOR(0x1014a950, L2Task)

// 0x10119f60 (vslot 25) - NControl's body, shared (hooked there)
INT L2Task::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return NCWnd::WindowProc(Msg, wParam, lParam) != 0;
}

// 0x10148e20 (vslot 37) - the blink timer: once a second
INT L2Task::OnCreate()
{
    NCWnd::OnCreate();
    TimerHandle = SetTimer(TIMER_L2TASK_BLINK, 1000, 0);
    return 1;
}
NW_HOOK_METHOD(0x10148e20, L2Task, OnCreate)

// 0x10148e50 (vslot 36) - the blink flag flips
INT L2Task::OnTimer(INT TimerID)
{
    if (TimerID == TIMER_L2TASK_BLINK)
        bBlink = (bBlink == 0);
    return 1;
}
NW_HOOK_METHOD(0x10148e50, L2Task, OnTimer)

// 0x10148e80 (vslot 38) - the timer goes, then the base
INT L2Task::OnDestroy()
{
    if (TimerHandle)
    {
        KillTimer(TimerHandle);
        TimerHandle = 0;
    }
    NCWnd::OnDestroy();
    return 1;
}
NW_HOOK_METHOD(0x10148e80, L2Task, OnDestroy)

NW_CHECK_VTABLE(L2Task, 0x101db250, 74)
