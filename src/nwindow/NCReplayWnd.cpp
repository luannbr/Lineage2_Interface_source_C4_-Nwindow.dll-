/*=============================================================================
    NCReplayWnd.cpp - the replay screen (C3 only; C3 addresses). Stock: SetWindowSize 0x100E6240, ShowWindow
    0x100E62F0, OnDestroy 0x100E68F0, the constructor 0x100E7720, the destructor 0x100E7770 and OnCreate 0x100E7BF0.
=============================================================================*/
#include "nwindow/NCReplayWnd.h"
#include "nwindow/NCBenchMarkResultWnd.h"
#include "nwindow/NCConsole.h"
#include "nwindow/NCGodWnd.h"
#include "nwindow/NCReplayMenuWnd.h"
#include "nwindow/NCReplayPlayWnd.h"
#include "nwindow/NConsoleWnd.h"

// A child window goes (repeated inline in the stock OnDestroy)
template <class T> static FORCEINLINE void DestroyChild(NCWnd* Parent, T*& Child)
{
    if (Child)
    {
        Parent->RemoveChildWindow(Child->Entry);
        delete Child;
        Child = NULL;
    }
}

// 0x100f9e80 - the three screens come with OnCreate (the stock clears the first two)
NCReplayWnd::NCReplayWnd() : MenuWnd(NULL), PlayWnd(NULL)
{
}
NW_HOOK_CTOR(0x100f9e80, NCReplayWnd)

// 0x100f9ed0
NW_HOOK_DTOR(0x100f9ed0, NCReplayWnd)

// 0x10119f60 (vslot 25) - NControl's body, shared (hooked there)
#if NW_HOOK_MODE
NW_NAKED INT NCReplayWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam) NW_STOCK(0x10148f10)
#else
INT NCReplayWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return NCWnd::WindowProc(Msg, wParam, lParam) != 0;   // 0x10148f10: NControl's body
}
#endif

// 0x10003360 (vslot 56) - a body shared by many classes: returns 1
#if NW_HOOK_MODE
NW_NAKED INT NCReplayWnd::OnPaint(NCGDevice* Device) NW_STOCK(0x100f76f0)
#else
INT NCReplayWnd::OnPaint(NCGDevice* Device)
{
    return 1;   // 0x100f76f0: mov eax, 1 / ret 4
}
#endif

// 0x100f8a40 (vslot 43, guard: NCReplayWnd::SetWindowSize; ret 8) - the three screens take the size
void NCReplayWnd::SetWindowSize(FLOAT W, FLOAT H)
{
    guard(NCReplayWnd::SetWindowSize);
    NCWnd::SetWindowSize(W, H);
    MenuWnd->SetWindowSize(W, H);
    PlayWnd->SetWindowSize(W, H);
    BenchWnd->SetWindowSize(W, H);
    unguard;
}
NW_HOOK_METHOD(0x100f8a40, NCReplayWnd, SetWindowSize)

// a window the header only forward-declares (the pointer is the window itself: NCWnd is the first base of all of them)
static FORCEINLINE NCWnd* AsWnd(const void* Wnd)
{
    return (NCWnd*)Wnd;
}

// 0x100f8af0 (vslot 48) - the screens that fit the replay manager's state, while a replay runs (GL2ReplayMode):
//   1: the menu, with the god window up and its five children (the login screen, message line, options window, warning
//      box and the new gara dialog) hidden; the desktop hidden
//   5: the benchmark's result alone (the desktop hidden)
//   else: the playing screen; when the manager has its flag at +0x5244 (C3 +0x5074) the desktop shows with the chat
//         window and the target and player status, menu and shortcut windows hidden
// C4: the god window hides five children (+0xEC .. +0xFC; the C3 code hid three) and the desktop's windows moved
void NCReplayWnd::ShowWindow()
{
    NCWnd::ShowWindow();
    if (GL2ReplayMode != 0)
    {
        FL2ReplayManager* Manager = GConsole->Engine->ReplayManager;
        if (Manager)
        {
            INT State = Manager->GetState();
            if (State == 1)
            {
                MenuWnd->ShowWindow();
                NCGodWnd* GodWnd = static_cast<NCGodWnd*>(GConsole->GodWnd);
                GodWnd->ShowWindow();
                AsWnd(GodWnd->LoginWnd)->HideWindow();
                AsWnd(GodWnd->MessageWnd)->HideWindow();
                AsWnd(GodWnd->OptionWnd)->HideWindow();
                AsWnd(GodWnd->WarningBox)->HideWindow();
                AsWnd(GodWnd->GaraDialog)->HideWindow();
                PlayWnd->HideWindow();
                BenchWnd->HideWindow();
                GConsole->Desktop->HideWindow();
                return;
            }
            if (State == 5)
            {
                MenuWnd->HideWindow();
                PlayWnd->HideWindow();
                BenchWnd->ShowWindow();
                GConsole->Desktop->HideWindow();
                return;
            }
            MenuWnd->HideWindow();
            GConsole->GodWnd->HideWindow();
            PlayWnd->ShowWindow();
            BenchWnd->HideWindow();
            if (::nw::Field<INT>(GConsole->Engine->ReplayManager, 0x5244) != 0)
            {
                NCConsole* Desktop = GConsole->GetDesktop();
                Desktop->ShowWindow();
                AsWnd(Desktop->ChatWnd)->ShowWindow();
                AsWnd(Desktop->TargetStatusWnd)->HideWindow();
                AsWnd(Desktop->PlayerStatusWnd)->HideWindow();
                AsWnd(Desktop->MenuWnd)->HideWindow();
                AsWnd(Desktop->ShortCutWnd)->HideWindow();
            }
        }
    }
}
NW_HOOK_METHOD(0x100f8af0, NCReplayWnd, ShowWindow)

// 0x100f9120 (vslot 38, guard: NCReplayWnd::OnDestroy) - the three screens go, then NCWnd's
INT NCReplayWnd::OnDestroy()
{
    guard(NCReplayWnd::OnDestroy);
    DestroyChild(this, MenuWnd);
    DestroyChild(this, PlayWnd);
    DestroyChild(this, BenchWnd);
    NCWnd::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100f9120, NCReplayWnd, OnDestroy)

// C2 CANDIDATE: the C2 function is not the same code as the C1 one; kept only while it matches the stock one (verify_data)
// 0x100fa360 (vslot 37) - the menu (style 0x4002), the playing screen (0x400A: not hit tested) and the benchmark's
// result (0x4002), each the size of this window and hidden
INT NCReplayWnd::OnCreate()
{
    NCWnd::OnCreate();
    MenuWnd = new NCReplayMenuWnd;
    MenuWnd->SetPosition(0, 0, (INT)Width, (INT)Height);
    CreateChild(MenuWnd, NWS_NOCLOSE | NWS_VISIBLE);
    MenuWnd->HideWindow();

    PlayWnd = new NCReplayPlayWnd;
    PlayWnd->SetPosition(0, 0, (INT)Width, (INT)Height);
    CreateChild(PlayWnd, NWS_NOCLOSE | NWS_NOHITTEST | NWS_VISIBLE);
    PlayWnd->HideWindow();

    BenchWnd = new NCBenchMarkResultWnd;
    BenchWnd->SetPosition(0, 0, (INT)Width, (INT)Height);
    CreateChild(BenchWnd, NWS_NOCLOSE | NWS_VISIBLE);
    BenchWnd->HideWindow();
    return 1;
}
NW_HOOK_METHOD(0x100fa360, NCReplayWnd, OnCreate)

NW_CHECK_VTABLE(NCReplayWnd, 0x101c8da8, 74)
