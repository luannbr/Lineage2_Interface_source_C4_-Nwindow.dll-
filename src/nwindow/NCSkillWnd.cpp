/*=============================================================================
    NCSkillWnd.cpp - the skill list and its GM copy (C3 addresses). Stock: 0x100a1130, 0x100a1150, 0x100a1aa0,
    0x100a23b0 .. 0x100a2470 .. 0x100a26a0, 0x1007e2a0 (WindowProc is NCButton's body 0x100127a0, OnPaint
    NCGMActionWnd's 0x10081e00), NCGMSkillWnd's 0x1007f780, 0x100849d0 .. 0x10084cc8.
=============================================================================*/
#include "nwindow/NCSkillWnd.h"
#include "nwindow/NCSkillItemWnd.h"
#include "nwindow/NCTabControl.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NItemC4.h"
#include "nwindow/NWndCreate.h"

/*-----------------------------------------------------------------------------
    NCSkillWnd
-----------------------------------------------------------------------------*/

// 0x100aec30 - NControl's constructor is inlined in the stock one (NCWnd's 0x10036ce0, then +0xE8 / +0xEC / +0xF0 (-1) / +0xF4 set
// as NControl's do); only the tab control (+0x1FC) is cleared here. The two lists (+0x1F4 / +0x1F8) are made by OnCreate
NCSkillWnd::NCSkillWnd()
{
    TabCtrl = NULL;
}
NW_HOOK_CTOR(0x100aec30, NCSkillWnd)

// 0x100aeca0
NW_HOOK_DTOR(0x100aeca0, NCSkillWnd)

// 0x100127a0 (vslot 25) - NCButton's body, shared (hooked there)
#if NW_HOOK_MODE
NW_NAKED INT NCSkillWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam) NW_STOCK(0x10021e60)
#else
INT NCSkillWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return NControl::WindowProc(Msg, wParam, lParam) != 0;   // 0x10021e60
}
#endif

// a list of the window (repeated inline in the stock OnCreate): 6 x 8 icons of 32, 5 apart across and 3 down. The grid
// is made like the other children: allocated, then the constructor 0x100AEA70 (ret 0x18)
static FORCEINLINE NCSkillItemWnd* CreateSkillList(NCWnd* Parent)
{
    NCSkillItemWnd* List = new NCSkillItemWnd(6, 8, 80, 32, 5, 3);
    List->SetPosition(9, 40, 239, 280);
    Parent->CreateChild(List, NWS_VISIBLE | 0x20);
    return List;
}

// 0x100aecf0 (vslot 37) - the tabs "Active" / "Passive" (sysstring 120 / 121); the first tab is the one shown
INT NCSkillWnd::OnCreate()
{
    NControl::OnCreate();
    ActiveList = CreateSkillList(this);
    PassiveList = CreateSkillList(this);
    TabCtrl = NewTabControl();
    TabCtrl->ID = NID_SKILL_TABS;
    TabCtrl->SetPosition(12, 8, 189, 23);
    CreateChild(TabCtrl, NWS_VISIBLE);
    AddTabC4(TabCtrl, 0x78, ActiveList, 94, 23, TEXT("L2UI_CH3.skillwnd.skill_tab2"),
             TEXT("L2UI_CH3.skillwnd.skill_tab1"), 1, -1, 0);
    AddTabC4(TabCtrl, 0x79, PassiveList, 94, 23, TEXT("L2UI_CH3.skillwnd.skill_tab2"),
             TEXT("L2UI_CH3.skillwnd.skill_tab1"), 0, -1, 0);
    return 1;
}
NW_HOOK_METHOD(0x100aecf0, NCSkillWnd, OnCreate)

// 0x100ae220 (vslot 38) - also the GM window's (folded in the stock code)
INT NCSkillWnd::OnDestroy()
{
    guard(NCSkillWnd::OnDestroy);
    DestroyChild(this, ActiveList);
    DestroyChild(this, PassiveList);
    DestroyChild(this, TabCtrl);
    NControl::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100ae220, NCSkillWnd, OnDestroy)

// 0x100ad870 (vslot 48) - also NCGMSkillWnd's
void NCSkillWnd::ShowWindow()
{
    NCWnd::ShowWindow();
    TabCtrl->ShowCurrentPage();
}
NW_HOOK_METHOD(0x100ad870, NCSkillWnd, ShowWindow)

// 0x10081e00 (vslot 56) - NCGMActionWnd's body, shared (ported there)
#if NW_HOOK_MODE
NW_NAKED INT NCSkillWnd::OnPaint(NCGDevice* Device) NW_STOCK(0x100ae1c0)
#else
INT NCSkillWnd::OnPaint(NCGDevice* Device)
{
    // 0x100ae1c0: draws nothing but the clip and the origin
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    Device->BeginPaint(ScreenX, ScreenY);
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
}
#endif

// 0x10089180 - into the passive list when NItemInfo::Param is 1; also NCGMSkillWnd's (the linker folded them)
INT NCSkillWnd::AddSkill(NItemInfo* Info)
{
    if (AsC4(Info)->Param == 1)
        PassiveList->AddSkill(Info);
    else
        ActiveList->AddSkill(Info);
    return 1;
}
NW_HOOK_METHOD(0x10089180, NCSkillWnd, AddSkill)

// 0x10089160 - both lists empty (also NCGMSkillWnd's)
void NCSkillWnd::ClearLists()
{
    ActiveList->Clear();
    PassiveList->Clear();
}
NW_HOOK_METHOD(0x10089160, NCSkillWnd, ClearLists)

NW_CHECK_VTABLE(NCSkillWnd, 0x101bafe0, 75)

/*-----------------------------------------------------------------------------
    NCGMSkillWnd
-----------------------------------------------------------------------------*/

// 0x1008fd10 - the same body as NCSkillWnd's constructor (NControl's inlined, the tab control cleared), this class' vtable
NCGMSkillWnd::NCGMSkillWnd()
{
    TabCtrl = NULL;
}
NW_HOOK_CTOR(0x1008fd10, NCGMSkillWnd)

// 0x1008fd80
NW_HOOK_DTOR(0x1008fd80, NCGMSkillWnd)

// 0x100127a0 (vslot 25) - NCButton's body, shared (hooked there)
#if NW_HOOK_MODE
NW_NAKED INT NCGMSkillWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam) NW_STOCK(0x10021e60)
#else
INT NCGMSkillWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return NControl::WindowProc(Msg, wParam, lParam) != 0;   // 0x10021e60
}
#endif

// a list of the GM window (repeated inline in the stock OnCreate): 6 x 7 icons; the constructor is 0x1008FCD0
static FORCEINLINE NCGMSkillItemWnd* CreateGMSkillList(NCWnd* Parent)
{
    NCGMSkillItemWnd* List = new NCGMSkillItemWnd(6, 7, 80, 32, 5, 3);
    List->SetPosition(9, 40, 239, 280);
    Parent->CreateChild(List, NWS_VISIBLE | 0x20);
    return List;
}

// 0x1008fdd0 (vslot 37) - NCSkillWnd's OnCreate with the GM grids and its own tab id
INT NCGMSkillWnd::OnCreate()
{
    NControl::OnCreate();
    ActiveList = CreateGMSkillList(this);
    PassiveList = CreateGMSkillList(this);
    TabCtrl = NewTabControl();
    TabCtrl->ID = NID_GMSKILL_TABS;
    TabCtrl->SetPosition(12, 8, 189, 23);
    CreateChild(TabCtrl, NWS_VISIBLE);
    AddTabC4(TabCtrl, 0x78, ActiveList, 94, 23, TEXT("L2UI_CH3.skillwnd.skill_tab2"),
             TEXT("L2UI_CH3.skillwnd.skill_tab1"), 1, -1, 0);
    AddTabC4(TabCtrl, 0x79, PassiveList, 94, 23, TEXT("L2UI_CH3.skillwnd.skill_tab2"),
             TEXT("L2UI_CH3.skillwnd.skill_tab1"), 0, -1, 0);
    return 1;
}
NW_HOOK_METHOD(0x1008fdd0, NCGMSkillWnd, OnCreate)

// 0x1008a960 (vslot 38) - NCSkillWnd's (0x100a1aa0) body under its own guard
INT NCGMSkillWnd::OnDestroy()
{
    guard(NCGMSkillWnd::OnDestroy);
    DestroyChild(this, ActiveList);
    DestroyChild(this, PassiveList);
    DestroyChild(this, TabCtrl);
    NControl::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1008a960, NCGMSkillWnd, OnDestroy)

// 0x100a1130 (vslot 44) - NCSkillWnd's body, shared (hooked there)
#if NW_HOOK_MODE
NW_NAKED void NCGMSkillWnd::ShowWindow() NW_STOCK(0x100ad870)
#else
void NCGMSkillWnd::ShowWindow()
{
    NCWnd::ShowWindow();   // 0x100ad870
    TabCtrl->ShowCurrentPage();
}
#endif

// 0x10081e00 (vslot 56) - NCGMActionWnd's body, shared (ported there)
#if NW_HOOK_MODE
NW_NAKED INT NCGMSkillWnd::OnPaint(NCGDevice* Device) NW_STOCK(0x100ae1c0)
#else
INT NCGMSkillWnd::OnPaint(NCGDevice* Device)
{
    // 0x100ae1c0: draws nothing but the clip and the origin
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    Device->BeginPaint(ScreenX, ScreenY);
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
}
#endif

// the stock code folded this with NCSkillWnd::AddSkill (0x1007e2a0)
INT NCGMSkillWnd::AddSkill(NItemInfo* Info)
{
    if (AsC4(Info)->Param == 1)
        PassiveList->AddSkill(Info);
    else
        ActiveList->AddSkill(Info);
    return 1;
}

// the stock code folded this with NCSkillWnd::ClearLists (0x100a1150)
void NCGMSkillWnd::ClearLists()
{
    ActiveList->Clear();
    PassiveList->Clear();
}

NW_CHECK_VTABLE(NCGMSkillWnd, 0x101b4f78, 75)
