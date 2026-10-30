/*=============================================================================
    NCOlympiadTargetWnd.cpp - the opponent's status window of the olympiad observer (C4 only). Stock: 0x1005fd60 (constructor),
    0x1005fde0 (destructor), 0x1013db80 .. 0x1013dd00+ (Initialize, OnOlympiadUserInfo, OnDestroy), 0x1013ddb0 (OnPaint),
    0x1013e000 (OnCreate).
=============================================================================*/
#include "nwindow/NCOlympiadTargetWnd.h"
#include "nwindow/NCFrameCtrl.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NCNameCtrl.h"
#include "nwindow/NConsoleWnd.h"

// 0x1005fd60 - NCWnd's constructor, the name empty; the other fields are left as allocated
NCOlympiadTargetWnd::NCOlympiadTargetWnd()
{
}
NW_HOOK_CTOR(0x1005fd60, NCOlympiadTargetWnd)

// 0x1005fde0 - ConditionalDestroy, the name's destructor, NCWnd's
NW_HOOK_DTOR(0x1005fde0, NCOlympiadTargetWnd)

// NWM_COMMAND id of the frame bar (names ours)
enum { NID_OLYMPIAD_TARGET_FRAME = 0xAFDDCEA0 };

// 0x1013e000 (vslot 37, guard: the stock's string is NCManorInfoSettingWnd's, copied) - the frame bar (12 px wide, as high as the
// window), the pieces of the skin and the four bar pictures, the name control (12 px in, 12 less wide than the window); nobody yet
INT NCOlympiadTargetWnd::OnCreate()
{
    guard(NCManorInfoSettingWnd::OnCreate);
    NCWnd::OnCreate();
    FrameCtrl = new NCFrameCtrl(1, 1, 1);
    FrameID = NID_OLYMPIAD_TARGET_FRAME;
    FrameCtrl->ID = NID_OLYMPIAD_TARGET_FRAME;
    FrameCtrl->SetPosition(0, 0, 12, (INT)Height);
    CreateChild(FrameCtrl, NWS_VISIBLE);
    for (INT i = 0; i < 3; i++)
        BackTex[i] = FindTexture(*FString::Printf(TEXT("L2UI_CH3.SmallWnd.SmallWindow_Back%d"), i + 1));
    CpBarTex = FindTexture(TEXT("L2UI_CH3.SmallWnd.CpBar"));
    HpBarTex = FindTexture(TEXT("L2UI_CH3.SmallWnd.HpBar"));
    CpBackTex = FindTexture(TEXT("L2UI_CH3.SmallWnd.CpBar_back"));
    HpBackTex = FindTexture(TEXT("L2UI_CH3.SmallWnd.HpBar_back"));
    NameCtrl = new NCNameCtrl();
    NameCtrl->SetPosition(12, 6, (INT)(Width - 12.0f), 14);
    CreateChild(NameCtrl, 6);
    Name = TEXT("");
    MaxHP = 0;
    HP = 0;
    MaxCP = 0;
    CP = 0;
    ObjectID = 0;
    Unknown128 = 0;
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1013e000, NCOlympiadTargetWnd, OnCreate)

// 0x1013db80 (guard) - nobody yet: the name empty, the numbers 0
void NCOlympiadTargetWnd::Initialize()
{
    guard(NCOlympiadTargetWnd::Initialize);
    Name = TEXT("");
    MaxHP = 0;
    HP = 0;
    MaxCP = 0;
    CP = 0;
    ObjectID = 0;
    Unknown128 = 0;
    unguard;
}
NW_HOOK_METHOD(0x1013db80, NCOlympiadTargetWnd, Initialize)

// 0x1013dc40 (guard) - ExOlympiadUserInfo's values in the order they come: the object id, the name, the class (dropped), HP, max HP,
// CP, max CP
void NCOlympiadTargetWnd::OnOlympiadUserInfo(L2ParamStack& Params)
{
    guard(NCOlympiadTargetWnd::OnOlympiadUserInfo);
    ObjectID = (INT)Params.Top();
    Name = (const TCHAR*)Params.Top();
    Params.Top();
    HP = (INT)Params.Top();
    MaxHP = (INT)Params.Top();
    CP = (INT)Params.Top();
    MaxCP = (INT)Params.Top();
    unguard;
}
NW_HOOK_METHOD(0x1013dc40, NCOlympiadTargetWnd, OnOlympiadUserInfo)

// 0x1013dd00 (vslot 38, guard) - the name control goes, then NCWnd's OnDestroy
INT NCOlympiadTargetWnd::OnDestroy()
{
    guard(NCOlympiadTargetWnd::OnDestroy);
    if (NameCtrl)
    {
        RemoveChildWindow(NameCtrl->Entry);
        delete NameCtrl;
        NameCtrl = NULL;
    }
    NCWnd::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1013dd00, NCOlympiadTargetWnd, OnDestroy)

// 0x1013ddb0 (vslot 62, guard) - the three pieces of the skin (the middle one stretched between the two ends), the empty bars and
// the filled part of each (150 pixels at most), the name
INT NCOlympiadTargetWnd::OnPaint(NCGDevice* Device)
{
    guard(NCOlympiadTargetWnd::OnPaint);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    Device->BeginPaint(ScreenX, ScreenY);
    if (BackTex[0])
        Device->DrawTexture(0xC, 0, 0x10, (INT)Height, 0, 0, 0x10, 0x2E, BackTex[0], 0xFF, 1);
    if (BackTex[1])
        Device->DrawTexture(0x1C, 0, (INT)(Width - 32.0f - 12.0f), (INT)Height, 0, 0, 0x10, 0x2E, BackTex[1], 0xFF, 1);
    if (BackTex[2])
        Device->DrawTexture((INT)(Width - 16.0f), 0, 0x10, (INT)Height, 0, 0, 0x10, 0x2E, BackTex[2], 0xFF, 1);
    Device->DrawTexture(0x10, 0x1A, 0x96, 6, 0, 0, 8, 6, CpBackTex, 0xFF, 1);
    Device->DrawTexture(0x10, 0x21, 0x96, 6, 0, 0, 8, 6, HpBackTex, 0xFF, 1);
    if (MaxCP != 0)
        Device->DrawTexture(0x10, 0x1A, (CP * 0x96) / MaxCP, 6, 0, 0, 8, 6, CpBarTex, 0xFF, 1);
    if (MaxHP != 0)
        Device->DrawTexture(0x10, 0x21, (HP * 0x96) / MaxHP, 6, 0, 0, 8, 6, HpBarTex, 0xFF, 1);
    Device->DrawText(0x19, 6, 0xFFDCDCDC, *Name, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1013ddb0, NCOlympiadTargetWnd, OnPaint)

NW_CHECK_VTABLE(NCOlympiadTargetWnd, 0x101aeae8, 74)
