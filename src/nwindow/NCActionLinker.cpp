/*=============================================================================
    NCActionLinker.cpp - an HTML link that runs an action. Stock: 0x1002fe50 .. 0x1002fed8,
    0x100306e0 .. 0x10030835 (OnConfirm's body is also NCFileLinker's OnLButtonDown).
=============================================================================*/
#include "nwindow/NCActionLinker.h"
#include "nwindow/NDialogInfo.h"
#include "nwindow/NCommandParser.h"
#include "nwindow/NConsoleWnd.h"

// 0x1004a520 - NControl's stores (inline in the stock constructor) and the code; Message is left as allocated
NCActionLinker::NCActionLinker(INT InAction)
{
    Action = InAction;
}
NW_HOOK_CTOR_ARGS(0x1004a520, NCActionLinker, (INT A), (A))

// 0x1004a590
NW_HOOK_DTOR(0x1004a590, NCActionLinker)


// 0x10047310 (vslot 25) - the confirmation from the dialog box (its virtual at slot 68)
INT NCActionLinker::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NControl::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_ACTIONLINK_CONFIRM)
        return OnConfirm(wParam, lParam) != 0;
    return 0;
}
NW_HOOK_METHOD(0x10047310, NCActionLinker, WindowProc)

// 0x1004baa0 (vslot 68) - with a message (C2: a "number;text" code that ExpandSystemMsgCode, 0x1003e5b0, turns into
// the message in place) the box asks first; else the code goes to the viewer
INT NCActionLinker::OnLButtonDown(WPARAM wParam, LPARAM lParam)
{
    if (Message[0])
    {
        ExpandSystemMsgCode(Message);
        NDialogInfo Info;
        Info.ID = 0x3C;   // C1: 0x38
        Info.bMainBox = 1;
        Info.bInput = 0;
        Info.Owner = Entry;
        Info.SetText(Message);
        Info.bLargeAmount = 1;   // set by the stock code, meaningless for a yes / no box
        GConsole->ShowDialog(&Info);
        return 1;
    }
    NotifyParent(Action);
    return 1;
}
NW_HOOK_METHOD(0x1004baa0, NCActionLinker, OnLButtonDown)

// 0x10047370 (vslot 75) - also NCFileLinker's slot 56 (identical body)
INT NCActionLinker::OnConfirm(WPARAM wParam, LPARAM lParam)
{
    NotifyParent(Action);
    return 1;
}
NW_HOOK_METHOD(0x10047370, NCActionLinker, OnConfirm)

NW_CHECK_VTABLE(NCActionLinker, 0x101a9d88, 76)
