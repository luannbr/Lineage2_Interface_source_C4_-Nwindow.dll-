/*=============================================================================
    NCFishViewportWnd.cpp - the fishing window (C4 only). Stock: 0x10120d10 (constructor), 0x10120e30 (destructor), 0x10120af0
    (OnCreate), 0x1011ffa0 (OnDestroy), 0x101204a0 (OnPaint).
=============================================================================*/
#include "nwindow/NCFishViewportWnd.h"
#include "nwindow/NCFrameCtrl.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NConsoleWnd.h"

// NWM_COMMAND id of the frame bar (names ours)
enum { NID_FISHING_FRAME = 0xB0000FF };
// the two hints' texts: SystemMsg[1665] (GL2GameData + 0x85AFC) and SystemMsg[1664] (+ 0x85ACC)
enum { SYSMSG_FishHint = 1665, SYSMSG_FishHintAlt = 1664 };

// 0x10120d10 - NCViewportWnd's constructor, the numbers and the two texts cleared
NCFishViewportWnd::NCFishViewportWnd()
{
    FishMaxLife = 0;
    FishLife = 0;
    Unknown11C = 0;
    TimeLeft = 0;
    bFighting = 0;
    EffectAlpha = 0;
    bEffect = 0;
    Unknown130 = 0;
    Unknown134 = 0;
    Fade = 0.0f;
    Message = TEXT("");
    Delta = 0;
    DeltaText = TEXT("");
    bHint = 0;
    HintTimer = 0.0f;
    Unknown164 = 0;
    bHintAlt = 0;
    bMessageRed = 0;
}
NW_HOOK_CTOR(0x10120d10, NCFishViewportWnd)

// 0x10120e30
NW_HOOK_DTOR(0x10120e30, NCFishViewportWnd)

// 0x10021e60 (vslot 25) - the stock body is the plain forward to NCWnd's, shared with NCButton's (hooked there): as a plain 0 / 1
INT NCFishViewportWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return NCWnd::WindowProc(Msg, wParam, lParam) != 0;
}

// 0x10120af0 (vslot 37) - the viewport's OnCreate, the seven pictures, the frame bar (256 x 20; its id is not the window's FrameID),
// and the place of the picture inside the window: 2 px in, 20 down, 252 x 254
INT NCFishViewportWnd::OnCreate()
{
    NCViewportWnd::OnCreate();
    BackTex = FindTexture(TEXT("L2UI_CH3.FishingWnd.fishing_back"));
    ClockTex = FindTexture(TEXT("L2UI_CH3.FishingWnd.fishing_clockicon"));
    LifeTex = FindTexture(TEXT("L2UI_CH3.FishingWnd.fishing_bar1"));
    LifeBackTex = FindTexture(TEXT("L2UI_CH3.FishingWnd.fishing_bar2"));
    EffectTex = FindTexture(TEXT("L2UI_CH3.FishingWnd.fishing_effect"));
    HintTex[0] = FindTexture(TEXT("Icon.skill_i.skill1313"));
    HintTex[1] = FindTexture(TEXT("Icon.skill_i.skill1314"));
    FrameCtrl = new NCFrameCtrl(1, 0, 0);
    FrameCtrl->ID = NID_FISHING_FRAME;
    FrameCtrl->SetPosition(0, 0, 256, 20);
    CreateChild(FrameCtrl, NWS_VISIBLE);
    OffsetX = 2;
    OffsetY = 20;
    ViewWidth = 252.0f;
    ViewHeight = 254.0f;
    return 1;
}
NW_HOOK_METHOD(0x10120af0, NCFishViewportWnd, OnCreate)

// 0x1011ffa0 (vslot 38, guard)
INT NCFishViewportWnd::OnDestroy()
{
    guard(NCFishViewportWnd::OnDestroy);
    NCViewportWnd::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1011ffa0, NCFishViewportWnd, OnDestroy)

// 0x101204a0 (vslot 62, guard; the db says the class is NCFishViewportwnd) - the back picture, the scene, then over it: the line
// and the damage (faded by Fade), while fighting the clock with its number, the life bar (115 px at most, in the two pictures) and the
// effect flash, and the blinking hint (icon and text)
INT NCFishViewportWnd::OnPaint(NCGDevice* Device)
{
    guard(NCFishViewportWnd::OnPaint);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    Device->BeginPaint(ScreenX, ScreenY);
    if (BackTex)
        Device->DrawTexture(0, 20, 256, 256, 0, 0, 256, 256, BackTex, 0xFF, 1);
    Device->EndPaint();
    Device->ReleaseClip();
    NCViewportWnd::DrawViewport(Device);   // the stock calls it directly, not through the vtable

    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    Device->BeginPaint(ScreenX, ScreenY);
    SIZE Size;
    NCGDevice::GetTextExtent(&Size, *DeltaText, 0, 0, 0, 0, 0);
    if (Fade > 0.0f)
    {
        BYTE Alpha = 0xFF;
        if (Fade <= 1.0f && Fade > 0.0f && Fade <= 1.0f)
            Alpha = (BYTE)(INT)(Fade * 255.0f);
        DWORD Color = (DWORD)Alpha << 24;
        if (bMessageRed)
            Device->DrawText(13, 256, Color | 0xFF0000, *Message, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
        else
            Device->DrawText(13, 256, Color | 0xDCDCDC, *Message, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
        if (Delta > 0)
            Device->DrawText(0xF2 - Size.cx, 256, Color | 0xFF0000, *DeltaText, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
        else if (Delta < 0)
            Device->DrawText(0xF2 - Size.cx, 256, Color | 0xDCDCDC, *DeltaText, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
    }
    if (bFighting)
    {
        if (ClockTex)
        {
            Device->DrawTexture(0x6E, 0xD7, 12, 16, 0, 0, 12, 16, ClockTex, 0xFF, 1);
            TCHAR Time[16];
            appSprintf(Time, TEXT("%d"), TimeLeft);
            Device->DrawSpecialDigit(2, 0x7D, 0xD8, 0xFFDCDCDC, Time, 0xFF);
        }
        if (FishMaxLife != 0)
        {
            INT Left = FishLife * 0x73 / FishMaxLife;
            Device->DrawTexture(0xD, 0xEC, Left, 11, 0, 0, 8, 11, LifeTex, 0xFF, 1);
            Device->DrawTexture(Left + 0xD, 0xEC, 0xE6 - Left, 11, 0, 0, 8, 11, LifeBackTex, 0xFF, 1);
        }
        if (bEffect)
            Device->DrawTexture(-0x16, 0xDD, 0x129, 0x29, 0, 0, 0x129, 0x29, EffectTex, (BYTE)EffectAlpha, 1);
    }
    if (bHint && ((HintTimer >= 0.0f && HintTimer < 0.1f) || (HintTimer >= 0.2f && HintTimer < 0.3f) || HintTimer >= 0.4f))
    {
        const TCHAR* Text;
        if (!bHintAlt)
        {
            Device->DrawTexture(13, 33, 32, 32, 0, 0, 32, 32, HintTex[0], 0xFF, 1);
            Text = *GL2GameData.SystemMsg[SYSMSG_FishHint].Message;
        }
        else
        {
            Device->DrawTexture(13, 33, 32, 32, 0, 0, 32, 32, HintTex[1], 0xFF, 1);
            Text = *GL2GameData.SystemMsg[SYSMSG_FishHintAlt].Message;
        }
        Device->DrawText(0x35, 0x21, 0xFFE7DCB6, Text, (DWORD)(INT)(Width - 63.0f), 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
    }
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x101204a0, NCFishViewportWnd, OnPaint)

NW_CHECK_VTABLE(NCFishViewportWnd, 0x101d0e80, 75)
