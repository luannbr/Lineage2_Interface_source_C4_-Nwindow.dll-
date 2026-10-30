/*=============================================================================
    NCManorInfoUpWnd.cpp - the tab bar of the manor information window (new in C4).
    Stock (C4): 0x10136EF0 .. 0x10137590 (OnDestroy, OnPaint, GetManorInfoDownWnd, OnCreate, OnTabClick, WindowProc),
    0x101374F0 (constructor), 0x10137540 (destructor).
=============================================================================*/
#include "nwindow/NCManorInfoUpWnd.h"
#include "nwindow/NCManorInfoWnd.h"
#include "nwindow/NCManorSeedInfoWnd.h"
#include "nwindow/NCManorCropInfoWnd.h"
#include "nwindow/NClasses.gen.h"
#include "nwindow/NGameData.h"
#include "nwindow/NString.h"

// the file the stock code reports a missing page from
#define MANORUP_SRC "Src\\NManorInfoUpWnd.cpp"

// sysstring labels of the three tabs
enum
{
    STR_SeedInfo = 0x459,
    STR_CropInfo = 0x45A,
    STR_DefaultInfo = 0x45B,
};

// 0x101374f0 - NCWnd's constructor and the vtable
NCManorInfoUpWnd::NCManorInfoUpWnd()
{
}
NW_HOOK_CTOR(0x101374f0, NCManorInfoUpWnd)

// 0x10137540 - ConditionalDestroy, then NCWnd's destructor
NW_HOOK_DTOR(0x10137540, NCManorInfoUpWnd)

// 0x10137590 (vslot 25) - the base's answer first; then the tab control's command (id 0xFF000001) goes to OnTabClick with
// the second word
INT NCManorInfoUpWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCWnd::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND && wParam == (WPARAM)NID_MANORINFOUP_TABS)
        return OnTabClick(lParam) != 0;
    return 0;
}
NW_HOOK_METHOD(0x10137590, NCManorInfoUpWnd, WindowProc)

// 0x10136ef0 (vslot 38) - the tab control goes, then the base's
INT NCManorInfoUpWnd::OnDestroy()
{
    guard(NCManorInfoUpWnd::OnDestroy);
    ManorDestroyChild(this, TabCtrl);
    NCWnd::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10136ef0, NCManorInfoUpWnd, OnDestroy)

// 0x10136fa0 (vslot 62) - the back picture over the whole window
INT NCManorInfoUpWnd::OnPaint(NCGDevice* Device)
{
    guard(NCManorInfoUpWnd::OnPaint);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    Device->BeginPaint(ScreenX, ScreenY);
    if (BackTex)
        Device->DrawTexture(0, 0, (INT)Width, (INT)Height, 0, 0, (INT)Width, (INT)Height, BackTex, 0xFF, 1);
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10136fa0, NCManorInfoUpWnd, OnPaint)

// 0x10137160 (ret 4) - the page of a tab: the information window this one belongs to (its parent, checked), and its
// page 0..2
NCWnd* NCManorInfoUpWnd::GetManorInfoDownWnd(INT Index)
{
    guard(NCManorInfoUpWnd::GetManorInfoDownWnd);
    NCManorInfoWnd* Info = NManorCastChecked(Entry->Parent->Wnd);
    return Info->GetPage(Index);
    unguard;
}
NW_HOOK_METHOD(0x10137160, NCManorInfoUpWnd, GetManorInfoDownWnd)

// 0x101371f0 (vslot 37) - the tab control (three tabs: seed, crop and default information), then the back picture
INT NCManorInfoUpWnd::OnCreate()
{
    guard(NCManorInfoUpWnd::OnCreate);
    NCWnd::OnCreate();
    TabCtrl = new NCTabControl();
    TabCtrl->ID = NID_MANORINFOUP_TABS;
    TabCtrl->SetPosition(0xC, 5, (INT)Width, (INT)Height);
    CreateChild(TabCtrl, NWS_VISIBLE);
    for (INT i = 0; i < 3; i++)
    {
        NCWnd* pManorInfoDownWnd = GetManorInfoDownWnd(i);
        NW_CHECK_AT(pManorInfoDownWnd, MANORUP_SRC, 30);
        INT TextID = -1;
        switch (i)
        {
        case 0:
            TextID = STR_SeedInfo;
            break;
        case 1:
            TextID = STR_CropInfo;
            break;
        case 2:
            TextID = STR_DefaultInfo;
            break;
        }
        TabCtrl->AddTab(TextID, pManorInfoDownWnd, 0x5E, 0x17, TEXT("L2UI_CH3.Manor.manor1_tab2"),
                        TEXT("L2UI_CH3.Manor.manor1_tab1"), 1, -1, 0);
    }
    BackTex = FindTexture(TEXT("L2UI_CH3.Manor.manor1_back1"));
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x101371f0, NCManorInfoUpWnd, OnCreate)

// 0x101373a0 (ret 4) - a tab was clicked: the server is asked for its page ("manor_menu_select", ask 3 seeds, 4 crops, 5 the
// manor's defaults); the seed and crop pages ask for the manor and the period their combo boxes show
INT NCManorInfoUpWnd::OnTabClick(INT Tab)
{
    guard(NCManorInfoUpWnd::OnTabClick);
    TCHAR Bypass[1024];
    switch (Tab)
    {
    case 0:
    {
        NCManorSeedInfoWnd* Seed = NManorCastSeedInfoWnd(GetManorInfoDownWnd(0));
        if (Seed == NULL)
            return 1;
        INT Time = Seed->GetTimeSelected();   // the period combo box's choice
        appSprintf(Bypass, TEXT("manor_menu_select?ask=3&state=%d&time=%d"), Seed->GetSelectedManorID(), Time);
        break;
    }
    case 1:
    {
        NCManorCropInfoWnd* Crop = NManorCastCropInfoWnd(GetManorInfoDownWnd(1));
        if (Crop == NULL)
            return 1;
        INT Time = reinterpret_cast<NCManorSeedInfoWnd*>(Crop)->GetTimeSelected();   // the same function (0x1013AC10): the window layouts agree
        appSprintf(Bypass, TEXT("manor_menu_select?ask=4&state=%d&time=%d"), Crop->GetSelectedManorID(), Time);
        break;
    }
    case 2:
        appSprintf(Bypass, TEXT("manor_menu_select?ask=5&state=-1&time=0"));
        break;
    default:
        return 1;
    }
    L2ParamStack Params(10);
    Params.PushBack((void*)Bypass);
    GConsole->RequestBypassToServer(Params);
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x101373a0, NCManorInfoUpWnd, OnTabClick)

NW_CHECK_VTABLE(NCManorInfoUpWnd, 0x101d7788, 74)
