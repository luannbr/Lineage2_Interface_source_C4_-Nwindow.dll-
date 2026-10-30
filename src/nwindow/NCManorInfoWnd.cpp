/*=============================================================================
    NCManorInfoWnd.cpp - the manor information window (new in C4): a title bar, the tab bar and the three pages.
    Stock (C4): 0x10137600 .. 0x10137A20 (GetPage, OnDestroy, OnShowSeedInfo, OnShowCropInfo, OnShowManorDefaultInfo,
    OnCreate), 0x1005F430 (constructor), 0x1005F480 (destructor).
=============================================================================*/
#include "nwindow/NCManorInfoWnd.h"
#include "nwindow/NCManorInfoUpWnd.h"
#include "nwindow/NCManorSeedInfoWnd.h"
#include "nwindow/NCManorCropInfoWnd.h"
#include "nwindow/NCManorDefaultInfoWnd.h"
#include "nwindow/NClasses.gen.h"
#include "nwindow/NGameData.h"
#include "nwindow/NString.h"

// the id of the title bar (name ours)
enum
{
    NID_MANORINFO_FRAME = (INT)0xFFF00001,
};

// 0x10137100 (cdecl, anonymous in the db) - CastChecked<NCManorInfoWnd>: the window comes back as it is; a window of another
// class is logged ("Cast of %s to %s failed")
NCManorInfoWnd* __cdecl NManorCastChecked(UObject* Src)
{
    return CastChecked<NCManorInfoWnd>(Src);
}
NW_HOOK_FUNC(0x10137100, NManorCastChecked)

// 0x101370a0 (cdecl, anonymous in the db) - Cast<NCManorSeedInfoWnd>: the window when it is of that class or a derived one,
// else NULL
NCManorSeedInfoWnd* __cdecl NManorCastSeedInfoWnd(UObject* Src)
{
    return Cast<NCManorSeedInfoWnd>(Src);
}
NW_HOOK_FUNC(0x101370a0, NManorCastSeedInfoWnd)

// 0x101370d0 (cdecl, anonymous in the db) - Cast<NCManorCropInfoWnd>
NCManorCropInfoWnd* __cdecl NManorCastCropInfoWnd(UObject* Src)
{
    return Cast<NCManorCropInfoWnd>(Src);
}
NW_HOOK_FUNC(0x101370d0, NManorCastCropInfoWnd)

// 0x10137700 (cdecl, anonymous in the db) - Cast<NCManorDefaultInfoWnd>
NCManorDefaultInfoWnd* __cdecl NManorCastDefaultInfoWnd(UObject* Src)
{
    return Cast<NCManorDefaultInfoWnd>(Src);
}
NW_HOOK_FUNC(0x10137700, NManorCastDefaultInfoWnd)

// the second half of CreateChild, the way the stock code has it inline after a SetPosition (the window already added to this
// one): its entry, the position made absolute, WM_CREATE
static FORCEINLINE void ManorAttachChild(NCWnd* Child, NWndEntry* E)
{
    Child->Entry = E;
    if (E->Parent)
    {
        NCWnd* Parent = E->Parent->Wnd;
        Child->ScreenX += Parent->ScreenX;
        Child->ScreenY += Parent->ScreenY;
    }
    Child->WindowProc(WM_CREATE, 0, 0);
}

// 0x1005f430 - NCWnd's constructor and the vtable
NCManorInfoWnd::NCManorInfoWnd()
{
}
NW_HOOK_CTOR(0x1005f430, NCManorInfoWnd)

// 0x1005f480 - ConditionalDestroy, then NCWnd's destructor
NW_HOOK_DTOR(0x1005f480, NCManorInfoWnd)

// 0x10137600 (ret 4) - page 0..2 (the seed, crop and default information), NULL for any other index
NCWnd* NCManorInfoWnd::GetPage(INT Index)
{
    if (Index < 0 || Index > 2)
        return NULL;
    return Pages[Index];
}
NW_HOOK_METHOD(0x10137600, NCManorInfoWnd, GetPage)

// 0x10137620 (vslot 38) - the tab bar and the three pages go, then the base's
INT NCManorInfoWnd::OnDestroy()
{
    guard(NCManorInfoWnd::OnDestroy);
    ManorDestroyChild(this, UpWnd);
    for (INT i = 0; i < 3; i++)
        ManorDestroyChild(this, Pages[i]);
    NCWnd::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10137620, NCManorInfoWnd, OnDestroy)

// 0x10137730 (ret 4) - the packet goes to the seed page; the window comes up when it was hidden (the page then notes the
// manor to paint, SetPaintManor); the first tab is chosen without telling the tab bar's owner
void NCManorInfoWnd::OnShowSeedInfo(L2ParamStack* Params)
{
    guard(NCManorInfoWnd::OnShowSeedInfo);
    NCManorSeedInfoWnd* Seed = NManorCastSeedInfoWnd(Pages[0]);
    if (Seed)
        Seed->OnShowSeedInfo(Params);
    if (!IsShowWindow())
    {
        Seed->SetPaintManor();
        ShowWindow();
    }
    UpWnd->TabCtrl->SelectTab(0, 0);
    unguard;
}
NW_HOOK_METHOD(0x10137730, NCManorInfoWnd, OnShowSeedInfo)

// 0x10137800 (ret 4) - the same for the crop page and the second tab
void NCManorInfoWnd::OnShowCropInfo(L2ParamStack* Params)
{
    guard(NCManorInfoWnd::OnShowCropInfo);
    NCManorCropInfoWnd* Crop = NManorCastCropInfoWnd(Pages[1]);
    if (Crop)
        Crop->OnShowCropInfo(Params);
    if (!IsShowWindow())
        ShowWindow();
    UpWnd->TabCtrl->SelectTab(1, 0);
    unguard;
}
NW_HOOK_METHOD(0x10137800, NCManorInfoWnd, OnShowCropInfo)

// 0x101378c0 (ret 4) - the same for the default page and the third tab
void NCManorInfoWnd::OnShowManorDefaultInfo(L2ParamStack* Params)
{
    guard(NCManorInfoWnd::OnShowManorDefaultInfo);
    NCManorDefaultInfoWnd* Info = NManorCastDefaultInfoWnd(Pages[2]);
    if (Info)
        Info->OnShowManorDefaultInfo(Params);
    if (!IsShowWindow())
        ShowWindow();
    UpWnd->TabCtrl->SelectTab(2, 0);
    unguard;
}
NW_HOOK_METHOD(0x101378c0, NCManorInfoWnd, OnShowManorDefaultInfo)

// 0x10137a20 (vslot 37) - the title bar (id 0xFFF00001); the three pages (made here, then placed at (0, 48) and told they
// are created); the tab bar at (0, 20) last
INT NCManorInfoWnd::OnCreate()
{
    guard(NCManorInfoWnd::OnCreate);
    NCWnd::OnCreate();
    FrameCtrl = ManorNewFrameCtrl(1, 0, 0);
    FrameID = NID_MANORINFO_FRAME;
    FrameCtrl->ID = NID_MANORINFO_FRAME;
    FrameCtrl->SetPosition(0, 0, (INT)Width, 0x14);
    CreateChild(FrameCtrl, NWS_VISIBLE);

    for (INT i = 0; i < 3; i++)
    {
        switch (i)
        {
        case 0:
            Pages[0] = new NCManorSeedInfoWnd();
            break;
        case 1:
            Pages[1] = new NCManorCropInfoWnd();
            break;
        case 2:
            Pages[2] = new NCManorDefaultInfoWnd();
            break;
        }
        NWndEntry* E = AddChildWindow(Pages[i], NWS_VISIBLE);
        Pages[i]->SetPosition(0, 0x30, (INT)Width, 0x116);
        ManorAttachChild(Pages[i], E);
    }

    UpWnd = new NCManorInfoUpWnd();
    NWndEntry* E = AddChildWindow(UpWnd, NWS_VISIBLE);
    UpWnd->SetPosition(0, 0x14, (INT)Width, 0x1C);
    ManorAttachChild(UpWnd, E);
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10137a20, NCManorInfoWnd, OnCreate)

NW_CHECK_VTABLE(NCManorInfoWnd, 0x101adcd0, 74)
