/*=============================================================================
    NCHeroTowerContentWnd.cpp - the hero tower's content window (C4 only). Stock: 0x1005fb00 (constructor), 0x1005fb50 (destructor),
    0x1013b5f0 (WindowProc), 0x1013b660 (OnDestroy), 0x1013b730 (OnPaint), 0x1013b830 (OnCreate).
=============================================================================*/
#include "nwindow/NCHeroTowerContentWnd.h"
#include "nwindow/NCButton.h"
#include "nwindow/NCFrameCtrl.h"
#include "nwindow/NCGDevice.h"

// NWM_COMMAND ids of the two buttons (names ours)
enum
{
    NID_HEROCONTENT_FRAME = 0xCFDDCEA0,
    NID_HEROCONTENT_FIRST = 0xCFDDCEB0,
    NID_HEROCONTENT_SECOND = 0xCFDDCEB1,
};

// 0x1005fb00 - NCWnd's constructor; the fields are left as allocated
NCHeroTowerContentWnd::NCHeroTowerContentWnd()
{
}
NW_HOOK_CTOR(0x1005fb00, NCHeroTowerContentWnd)

// 0x1005fb50
NW_HOOK_DTOR(0x1005fb50, NCHeroTowerContentWnd)

// 0x1013b830 (vslot 37, guard: the stock's string is NCManorInfoSettingWnd's, copied) - the title bar (as wide as the window, 20 px
// high), the background piece and the two buttons at the foot (SysString 1 each)
INT NCHeroTowerContentWnd::OnCreate()
{
    guard(NCManorInfoSettingWnd::OnCreate);
    NCWnd::OnCreate();
    FrameCtrl = new NCFrameCtrl(1, 0, 0);
    FrameID = NID_HEROCONTENT_FRAME;
    FrameCtrl->ID = NID_HEROCONTENT_FRAME;
    FrameCtrl->SetPosition(0, 0, (INT)Width, 20);
    CreateChild(FrameCtrl, NWS_VISIBLE);
    BackTex = FindTexture(TEXT("L2UI_CH3.HeroTower.herotower2_back"));
    FirstButton = new NCButton(0, 0);
    FirstButton->ID = NID_HEROCONTENT_FIRST;
    FirstButton->SetPosition(0x4E, 0x174, 0, 0);
    CreateChild(FirstButton, NWS_VISIBLE);
    FirstButton->SetTextID(1);
    SecondButton = new NCButton(0, 0);
    SecondButton->ID = NID_HEROCONTENT_SECOND;
    SecondButton->SetPosition(0x9E, 0x174, 0, 0);
    CreateChild(SecondButton, NWS_VISIBLE);
    SecondButton->SetTextID(1);
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1013b830, NCHeroTowerContentWnd, OnCreate)

// 0x1013b5f0 (vslot 25) - both buttons call one function of the stock image that answers 1 (0x100f7850, folded with the other
// empty handlers; here two empty methods, kept out of line like the stock's), so a click is taken and nothing happens
INT NCHeroTowerContentWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCWnd::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND)
    {
        if (wParam == NID_HEROCONTENT_FIRST)
            return OnFirstBtnClick() != 0;
        if (wParam == NID_HEROCONTENT_SECOND)
            return OnSecondBtnClick() != 0;
    }
    return 0;
}
NW_HOOK_METHOD(0x1013b5f0, NCHeroTowerContentWnd, WindowProc)

// 0x1013b660 (vslot 38, guard) - the two buttons go, then NCWnd's OnDestroy
INT NCHeroTowerContentWnd::OnDestroy()
{
    guard(NCHeroTowerContentWnd::OnDestroy);
    if (FirstButton)
    {
        RemoveChildWindow(FirstButton->Entry);
        delete FirstButton;
        FirstButton = NULL;
    }
    if (SecondButton)
    {
        RemoveChildWindow(SecondButton->Entry);
        delete SecondButton;
        SecondButton = NULL;
    }
    NCWnd::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1013b660, NCHeroTowerContentWnd, OnDestroy)

// 0x1013b730 (vslot 62, guard) - the background piece from y = 20 down, as wide as the window and 20 less high
INT NCHeroTowerContentWnd::OnPaint(NCGDevice* Device)
{
    guard(NCHeroTowerContentWnd::OnPaint);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    Device->BeginPaint(ScreenX, ScreenY);
    if (BackTex)
        Device->DrawTexture(0, 0x14, (INT)Width, (INT)(Height - 20.0f), 0, 0, (INT)Width, (INT)(Height - 20.0f), BackTex, 0xFF, 1);
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1013b730, NCHeroTowerContentWnd, OnPaint)

NW_CHECK_VTABLE(NCHeroTowerContentWnd, 0x101ae770, 74)
