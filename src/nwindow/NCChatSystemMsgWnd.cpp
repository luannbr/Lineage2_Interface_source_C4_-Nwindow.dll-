/*=============================================================================
    NCChatSystemMsgWnd.cpp - the system message list of the chat window (C4 only). Stock: 0x1005a580 / 0x1005a620 (constructors),
    0x1005a5d0 (destructor), 0x100540f0 (OnCreate), 0x10052b50 (OnDestroy), 0x10056880 (ShowWindow), 0x10056900 (OnPaint),
    0x10053fd0 (RearrangePos).
=============================================================================*/
#include "nwindow/NCChatSystemMsgWnd.h"
#include "nwindow/NCButton.h"
#include "nwindow/NCChatFilterWnd.h"
#include "nwindow/NCChatWnd.h"
#include "nwindow/NCConsole.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NCSliderBar.h"
#include "nwindow/NConsoleWnd.h"

// 0x1005a580 - NCChatListBox's constructor
NCChatSystemMsgWnd::NCChatSystemMsgWnd()
{
}
NW_HOOK_CTOR(0x1005a580, NCChatSystemMsgWnd)

// 0x1005a620 (ret 0xC) - NCChatListBox's three-argument constructor (it appears in the db as NCChatListBox's)
NCChatSystemMsgWnd::NCChatSystemMsgWnd(INT InMaxLines, INT InVisibleLines, INT Unused)
    : NCChatListBox(InMaxLines, InVisibleLines, Unused)
{
}
NW_HOOK_CTOR_ARGS(0x1005a620, NCChatSystemMsgWnd, (INT A, INT B, INT C), (A, B, C))

// 0x1005a5d0
NW_HOOK_DTOR(0x1005a5d0, NCChatSystemMsgWnd)

// 0x100540f0 (vslot 37, guard) - NCListBox's OnCreate, the strip, the scroll controls at the left (5 px in: the up button at 5, the
// down button 19 above the foot, the bar from 20 down to 39 above the foot) and the text 23 px in, 14 down
INT NCChatSystemMsgWnd::OnCreate()
{
    guard(NCChatSystemMsgWnd::OnCreate);
    NCListBox::OnCreate();
    BackTex = FindTexture(TEXT("L2UI_CH3.ChatWnd.chatting_system"));
    ScrollUpButton->SetPosition(5, 5, 15, 15);
    ScrollDownButton->SetPosition(5, (INT)(Height - 19.0f), 15, 15);
    ScrollBar->SetPosition(5, 20, 15, (INT)(Height - 39.0f));
    TextX = 23;
    TextY = 14;
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100540f0, NCChatSystemMsgWnd, OnCreate)

// 0x10052b50 (vslot 38, guard)
INT NCChatSystemMsgWnd::OnDestroy()
{
    guard(NCChatSystemMsgWnd::OnDestroy);
    NCChatListBox::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10052b50, NCChatSystemMsgWnd, OnDestroy)

// 0x10053fd0 (guard) - the desktop docks the icons above the chat, then the chat's filter panel moves by the distance between the
// chat and the panel less 0x141 (the list shows) or 0xBF
void NCChatSystemMsgWnd::RearrangePos()
{
    guard(NCChatSystemMsgWnd::RearrangePos);
    NCConsole* Desk = GConsole->GetDesktop();
    if (Desk)
    {
        INT ChatY = Desk->ChatWnd->ScreenY;
        Desk->DockAboveChat();
        NCChatFilterWnd* Filter = GConsole->GetDesktop()->ChatWnd->FilterWnd;
        if (Filter)
        {
            INT Delta = ChatY - Filter->ScreenY;
            if (IsShowWindow())
                GConsole->GetDesktop()->ChatWnd->FilterWnd->MoveWindow(0, Delta - 0x141);
            else
                GConsole->GetDesktop()->ChatWnd->FilterWnd->MoveWindow(0, Delta - 0xBF);
        }
    }
    unguard;
}
NW_HOOK_METHOD(0x10053fd0, NCChatSystemMsgWnd, RearrangePos)

// 0x10056880 (vslot 48, guard) - NCWnd's ShowWindow (not the list box's: the scroll controls stay), then the panel moves
void NCChatSystemMsgWnd::ShowWindow()
{
    guard(NCChatSystemMsgWnd::ShowWindow);
    NCWnd::ShowWindow();
    RearrangePos();
    unguard;
}
NW_HOOK_METHOD(0x10056880, NCChatSystemMsgWnd, ShowWindow)

// 0x10056900 (vslot 62, guard) - the strip (as wide as the window, 128 of its height drawn), then the list's own paint
INT NCChatSystemMsgWnd::OnPaint(NCGDevice* Device)
{
    guard(NCChatSystemMsgWnd::OnPaint);
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    if (BackTex)
    {
        INT W = (INT)Width;
        Device->DrawTexture(0, 0, W, (INT)Height, 0, 0, W, 0x80, BackTex, 0xFF, 1);
    }
    Device->ReleaseClip();
    Device->EndPaint();
    NCChatListBox::OnPaint(Device);
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10056900, NCChatSystemMsgWnd, OnPaint)

NW_CHECK_VTABLE(NCChatSystemMsgWnd, 0x101aca78, 83)
