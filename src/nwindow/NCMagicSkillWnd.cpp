/*=============================================================================
    NCMagicSkillWnd.cpp - the skill window and its GM copy (C3 addresses). Stock: 0x1005d050, 0x100a1170,
    0x100a1bd0 .. 0x100a1f7b, 0x100a26b0 .. 0x100a2750 .. 0x100a281b (WindowProc is NControl's body 0x10119f60,
    OnDestroy NCPetActionWnd's), NCGMMagicSkillWnd's 0x1007e2e0, 0x1007f880 .. 0x1007f9db, 0x10084cd0 ..
    0x10084ec9.
=============================================================================*/
#include "nwindow/NCMagicSkillWnd.h"
#include "nwindow/NCSkillWnd.h"
#include "nwindow/NCSkillItemWnd.h"
#include "nwindow/NCFrameCtrl.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NConsoleWnd.h"

#include "nwindow/NItemEntryC4.h"
#include <wchar.h>

// the stock code calls the CRT's wcscpy; the intrinsic would copy inline
#pragma function(wcscpy)

// 0x10068060 (sub_10068060, a method of the console, ret 4) - NetHandler 0x29C (slot 167): the skill list request. The
// console's RequestSkillList (0x10068040) is NetHandler 0x268, the skill USE request; the window asks for its list with
// this one (the stage 3 map paired the C3 function with 0x10068040 by shape; the C4 ShowWindow calls this one). The method
// is declared on a view of NConsoleWnd (its header is not this lane's)
struct NConsoleWndSkillListView : public NConsoleWnd
{
    __declspec(noinline) void RequestSkillListC3(L2ParamStack& Params)
    {
        nw::VirtualCall<void>(Engine->NetHandler, 0x29C / 4, &Params);
    }
};
NW_HOOK_METHOD(0x10068060, NConsoleWndSkillListView, RequestSkillListC3)

/*-----------------------------------------------------------------------------
    NCMagicSkillWnd
-----------------------------------------------------------------------------*/

// 0x100aef40 - NCWnd's constructor and nothing else (BackTex and SkillWnd are left as allocated)
NCMagicSkillWnd::NCMagicSkillWnd()
{
}
NW_HOOK_CTOR(0x100aef40, NCMagicSkillWnd)

// 0x100aef90
NW_HOOK_DTOR(0x100aef90, NCMagicSkillWnd)

// 0x10148f10 (vslot 25) - NControl::WindowProc's body, which the stock vtables of these windows share (the C3 address was
// 0x10119f60): NCWnd's, normalized to 0 or 1
INT NCMagicSkillWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return NCWnd::WindowProc(Msg, wParam, lParam) != 0;
}

// C2 CANDIDATE: the C2 function is not the same code as the C1 one; kept only while it matches the stock one (verify_data)
// 0x100aefe0 (vslot 37) - the background and the skill list, as big as the window (C3: the window's own size)
INT NCMagicSkillWnd::OnCreate()
{
    NCWnd::OnCreate();
    BackTex = FindTexture(TEXT("L2UI_CH3.SkillWnd.skill_back"));
    SkillWnd = new NCSkillWnd;
    SkillWnd->ID = NID_MAGICSKILL_LIST;
    SkillWnd->SetPosition(0, 0, (INT)Width, (INT)Height);
    CreateChild(SkillWnd, NWS_VISIBLE | 0x20);
    return 1;
}
NW_HOOK_METHOD(0x100aefe0, NCMagicSkillWnd, OnCreate)

// 0x100ae350 (vslot 38) - the skill list goes (same body as NCPetActionWnd's, its own copy); BackTex is left
INT NCMagicSkillWnd::OnDestroy()
{
    guard(NCMagicSkillWnd::OnDestroy);
    if (SkillWnd)
    {
        RemoveChildWindow(SkillWnd->Entry);
        delete SkillWnd;
        SkillWnd = NULL;
    }
    NCWnd::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100ae350, NCMagicSkillWnd, OnDestroy)

// 0x100ae4a0 (vslot 48) - only when hidden: the lists empty, the server is asked for them (0x1005d050)
void NCMagicSkillWnd::ShowWindow()
{
    if (!IsShowWindow())
    {
        ClearSkills();
        L2ParamStack Params(10);
        static_cast<NConsoleWndSkillListView*>(GConsole)->RequestSkillListC3(Params);
        NCWnd::ShowWindow();
    }
}
NW_HOOK_METHOD(0x100ae4a0, NCMagicSkillWnd, ShowWindow)

// 0x100ae520 (vslot 49)
void NCMagicSkillWnd::HideWindow()
{
    ClearSkills();
    NCWnd::HideWindow();
}
NW_HOOK_METHOD(0x100ae520, NCMagicSkillWnd, HideWindow)

// 0x100ae400 (vslot 62) - the background over the whole window; C3: the clip is one pixel taller than the window
INT NCMagicSkillWnd::OnPaint(NCGDevice* Device)
{
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)(Height + 1.0f));
    Device->BeginPaint(ScreenX, ScreenY);
    if (BackTex)
        Device->DrawTexture(0, 0, (INT)Width, (INT)Height, 0, 0, (INT)Width, (INT)Height, BackTex, 0xFF, 1);
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
}
NW_HOOK_METHOD(0x100ae400, NCMagicSkillWnd, OnPaint)

// 0x100ad890
void NCMagicSkillWnd::ClearSkills()
{
    SkillWnd->ClearLists();
}
NW_HOOK_METHOD(0x100ad890, NCMagicSkillWnd, ClearSkills)

// a list's skills take the new name and description and make their tool tip again (repeated
// inline in the stock ReloadSkillTexts); the entry is the C4 one (NItemEntryC4.h: the skill's id at +0x1B20, its level at
// +0x1B24, the name at +0x14, the description at +0x914, the tool tip object at +0x1BC0)
static FORCEINLINE void ReloadListTexts(NCSkillItemWnd* List)
{
    for (INT i = 0; i < List->Items.Num(); i++)
    {
        NItemEntry* Item = List->Items(i);
        if (!Item)
            continue;
        if (GConsole->GetSkillName(EntryID(Item), EntryLevel(Item)))
            wcscpy(EntryName(Item), GConsole->GetSkillName(EntryID(Item), EntryLevel(Item)));
        if (GConsole->GetSkillDesc(EntryID(Item), EntryLevel(Item)))
            wcscpy(EntryDescription(Item), GConsole->GetSkillDesc(EntryID(Item), EntryLevel(Item)));
        EntryToolTip(Item)->Fill(Item, 4, 0);   // NToolTipInfo::Fill (flags 4)
    }
}

// 0x100ae530 - after the language changed
void NCMagicSkillWnd::ReloadSkillTexts()
{
    ReloadListTexts(SkillWnd->ActiveList);
    ReloadListTexts(SkillWnd->PassiveList);
}
NW_HOOK_METHOD(0x100ae530, NCMagicSkillWnd, ReloadSkillTexts)

NW_CHECK_VTABLE(NCMagicSkillWnd, 0x101bb110, 74)

/*-----------------------------------------------------------------------------
    NCGMMagicSkillWnd
-----------------------------------------------------------------------------*/

// 0x10090020
NCGMMagicSkillWnd::NCGMMagicSkillWnd()
{
}
NW_HOOK_CTOR(0x10090020, NCGMMagicSkillWnd)

// 0x10090070
NW_HOOK_DTOR(0x10090070, NCGMMagicSkillWnd)

// 0x10148f10 (vslot 25) - NControl::WindowProc's body, which the stock vtables of these windows share (the C3 address was
// 0x10119f60): NCWnd's, normalized to 0 or 1
INT NCGMMagicSkillWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return NCWnd::WindowProc(Msg, wParam, lParam) != 0;
}

// 0x100900c0 (vslot 37) - a title bar (20 high, the window is 256 wide), the GM's skill list under it and the background
INT NCGMMagicSkillWnd::OnCreate()
{
    NCWnd::OnCreate();
    FrameCtrl = new NCFrameCtrl(1, 0, 0);
    FrameCtrl->ID = NID_GMMAGICSKILL_FRAME;
    FrameCtrl->SetPosition(0, 0, 256, 20);
    CreateChild(FrameCtrl, NWS_VISIBLE);
    SkillWnd = new NCGMSkillWnd;
    SkillWnd->ID = NID_MAGICSKILL_LIST;
    SkillWnd->SetPosition(0, 20, 256, 335);
    CreateChild(SkillWnd, NWS_VISIBLE | 0x20);
    BackTex = FindTexture(TEXT("L2UI_CH3.SkillWnd.skill_back"));
    return 1;
}
NW_HOOK_METHOD(0x100900c0, NCGMMagicSkillWnd, OnCreate)

// 0x1008aa60 (vslot 38)
INT NCGMMagicSkillWnd::OnDestroy()
{
    guard(NCGMMagicSkillWnd::OnDestroy);
    if (SkillWnd)
    {
        RemoveChildWindow(SkillWnd->Entry);
        delete SkillWnd;
        SkillWnd = NULL;
    }
    NCWnd::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1008aa60, NCGMMagicSkillWnd, OnDestroy)

// 0x100efc40 (vslot 48) - the folded `jmp NCWnd::ShowWindow` (NCCandidateBox.cpp hooks it)
void NCGMMagicSkillWnd::ShowWindow()
{
    NCWnd::ShowWindow();
}

// 0x1008abb0 (vslot 49)
void NCGMMagicSkillWnd::HideWindow()
{
    ClearSkills();
    NCWnd::HideWindow();
}
NW_HOOK_METHOD(0x1008abb0, NCGMMagicSkillWnd, HideWindow)

// 0x1008ab10 (vslot 62) - the background under the title bar
INT NCGMMagicSkillWnd::OnPaint(NCGDevice* Device)
{
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    Device->BeginPaint(ScreenX, ScreenY);
    if (BackTex)
        Device->DrawTexture(0, 20, 256, 335, 0, 0, 256, 335, BackTex, 0xFF, 1);
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
}
NW_HOOK_METHOD(0x1008ab10, NCGMMagicSkillWnd, OnPaint)

// 0x100891c0
void NCGMMagicSkillWnd::ClearSkills()
{
    SkillWnd->ClearLists();
}
NW_HOOK_METHOD(0x100891c0, NCGMMagicSkillWnd, ClearSkills)

NW_CHECK_VTABLE(NCGMMagicSkillWnd, 0x101b5120, 74)
