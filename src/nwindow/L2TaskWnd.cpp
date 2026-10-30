/*=============================================================================
    L2TaskWnd.cpp - the L2 skin's task list window (C3 only; C3 addresses). Stock: OnDestroy 0x101254a0, OnCreate
    0x101260c0, the constructor / destructor 0x101263f0 .. 0x1012654f. OnPaint (0x1011b110) and the map's Rehash
    (0x10124a60) are TaskWnd's.
=============================================================================*/
#include "nwindow/L2TaskWnd.h"
#include "nwindow/L2Task.h"
#include "nwindow/L2TaskBox.h"
#include "nwindow/MsnUiStock.h"
#include "nwindow/MsnWndOps.h"   // NMsnTaskMapOps (stage 5, sa-verify: declared there once)
#include "nwindow/NChatUtil.h"
#include "nwindow/NCFrameCtrl.h"
#include "nwindow/NCGDevice.h"

enum { NID_L2TASK_FRAME = 0xA72 };

// 0x1014aec0 - the base window's constructor, the map empty (8 hash entries), no list
L2TaskWnd::L2TaskWnd()
{
    List = NULL;
}
NW_HOOK_CTOR(0x1014aec0, L2TaskWnd)

// 0x1014af60 - the map's destructor, then the base
NW_HOOK_DTOR(0x1014af60, L2TaskWnd)

// 0x10119f60 (vslot 25) - NControl's body, shared (hooked there)
INT L2TaskWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return NCWnd::WindowProc(Msg, wParam, lParam) != 0;
}

// 0x1014ab70 (vslot 37) - TaskWnd's, with the vertical title bar of id 0xA72 and an L2TaskBox
INT L2TaskWnd::OnCreate()
{
    NCWnd::OnCreate();
    BackTex1 = FindTexture(TEXT("L2UI_CH3.msn6_back1"));
    BackTex2 = FindTexture(TEXT("L2UI_CH3.msn6_back2"));
    BackTex3 = FindTexture(TEXT("L2UI_CH3.msn6_back3"));

    FrameCtrl = new NCFrameCtrl(1, 1, 1);
    FrameCtrl->ID = NID_L2TASK_FRAME;
    FrameCtrl->SetPosition(0, 0, 0xC, 0x84);
    CreateChild(FrameCtrl, NWS_VISIBLE);

    List = new L2TaskBox(300, 6, 0xF);
    List->SetPosition(0xE, 3, 0x9B, 0x7E);
    CreateChild(List, NWS_VISIBLE);
    return 1;
}
NW_HOOK_METHOD(0x1014ab70, L2TaskWnd, OnCreate)

// 0x10149f50 (vslot 38) - TaskWnd's: every L2Task window goes, the map starts again, the list goes; the base is not called
INT L2TaskWnd::OnDestroy()
{
    for (INT i = 0; i < Tasks.Pairs.Num(); i++)
    {
        NCWnd* Child = Tasks.Pairs(i).Value;
        if (Child)
        {
            RemoveChildWindow(Child->Entry);
            delete Child;
        }
    }
    Tasks.Pairs.Empty();
    Tasks.HashCount = 8;
    Tasks.Rehash();
    NDestroyChild(this, List);
    return 1;
}
NW_HOOK_METHOD(0x10149f50, L2TaskWnd, OnDestroy)

// 0x1014a030 (ret 4) - the L2Task of that conversation window (the map's Find, 0x10124a10) leaves the list (0x10124fc0), the
// map and the window's children, and is deleted
void L2TaskWnd::RemoveTask(NCWnd* ChatWindow)
{
    NCWnd* Task = ((NMsnTaskMapOps*)&Tasks)->FindTask(&ChatWindow);       // the map's Find, 0x101494e0
    if (Task)
    {
        ((NChatTaskListBox*)List)->RemoveTask(Task);                        // 0x10149a70
        Tasks.RemoveKey(ChatWindow);
        RemoveChildWindow(Task->Entry);
        delete Task;
    }
}
NW_HOOK_METHOD(0x1014a030, L2TaskWnd, RemoveTask)

// 0x1014a080 (ret 0xC) - the first L2Task whose name is that friend's (the name is the argument by value, destroyed on return)
NCWnd* L2TaskWnd::FindTask(FString Name)
{
    for (INT i = 0; i < Tasks.Pairs.Num(); i++)
    {
        L2Task* Task = static_cast<L2Task*>(Tasks.Pairs(i).Value);
        if (appStrcmp(*Task->Name, *Name) == 0)
            return Task;
    }
    return NULL;
}
typedef NCWnd* (L2TaskWnd::*SigFindTask)(FString);
NW_HOOK_VARARGS(0x1014a080, L2TaskWnd, FindTask, SigFindTask)

// 0x1011b110 (vslot 56) - TaskWnd's body (hooked there): the back, the top and bottom pieces as they are, the middle stretched
INT L2TaskWnd::OnPaint(NCGDevice* Device)
{
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    if (BackTex1)
        Device->DrawTexture(0xC, 0, 0xB4, 8, 0, 0, 0xB4, 8, BackTex1, 0xFF, 1);
    if (BackTex2)
        Device->DrawStretchedTex(0xC, 8, 0xB4, 0x74, 0, 0, 0xB4, 8, BackTex2, 0xFF);
    if (BackTex3)
        Device->DrawTexture(0xC, 0x7C, 0xB4, 8, 0, 0, 0xB4, 8, BackTex3, 0xFF, 1);
    Device->ReleaseClip();
    Device->EndPaint();
    return 1;
}

NW_CHECK_VTABLE(L2TaskWnd, 0x101db4c0, 74)
