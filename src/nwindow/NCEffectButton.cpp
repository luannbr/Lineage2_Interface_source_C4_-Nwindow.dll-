/*=============================================================================
    NCEffectButton.cpp - the glowing button of the desktop's left corner (C4 only). Stock: 0x10003b50 (constructor), 0x10003bb0
    (destructor), 0x100020c0 (OnCreate), 0x10002120 (OnTimer), 0x10002700 (OnPaint), 0x10002830 (the click).
=============================================================================*/
#include "nwindow/NCEffectButton.h"
#include "nwindow/NCBoardWnd.h"
#include "nwindow/NCConsole.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NCMainWnd.h"
#include "nwindow/NCQuestGaraTreeCtrl.h"
#include "nwindow/NCQuestGaraTreeWnd.h"
#include "nwindow/NCTabControl.h"
#include "nwindow/NConsoleWnd.h"

// timers (names ours; StartTimers starts them)
enum
{
    TIMER_EFFECT_BLINK = 0xF0E0F0,   // the over face on and off
    TIMER_EFFECT_FADE = 0xF0E0F1,    // the glow's alpha, 15 per tick
    TIMER_EFFECT_GROW = 0xF0E0F2,    // the ring, 5 percent per tick
};
// what the click does (names ours)
enum
{
    EFFECT_TutorialQuestion = 0,
    EFFECT_Quest = 1,
    EFFECT_Mail = 2,
};

// 0x10003b50 - NCButton's constructor, the blink period 500 ms and the tutorial's question mark as the kind
NCEffectButton::NCEffectButton()
{
    BlinkInterval = 500;
    Kind = EFFECT_TutorialQuestion;
}
NW_HOOK_CTOR(0x10003b50, NCEffectButton)

// 0x10003c00 (ret 0x18; anonymous in the db) - NCButton's four-argument constructor (the three textures and the flags), the kind and the blink
// period given, and the blink flag cleared with a byte store (the stock one; the three bytes after it stay as allocated)
NCEffectButton::NCEffectButton(INT InKind, const TCHAR* NormalTexName, const TCHAR* PushedTexName, const TCHAR* OverTexName,
                               INT InBlinkInterval, INT InFlags)
    : NCButton(NormalTexName, PushedTexName, OverTexName, InFlags)
{
    Kind = InKind;
    BlinkInterval = InBlinkInterval;
    *(BYTE*)&bBlinkOn = 0;
}
NW_HOOK_CTOR_ARGS(0x10003c00, NCEffectButton, (INT A, const TCHAR* B, const TCHAR* C, const TCHAR* D, INT E, INT F), (A, B, C, D, E, F))

// 0x10003bb0
NW_HOOK_DTOR(0x10003bb0, NCEffectButton)

// 0x100020c0 (vslot 37) - NControl's OnCreate, the two glow textures, the glow off
INT NCEffectButton::OnCreate()
{
    NControl::OnCreate();
    GlowTex[0] = FindTexture(TEXT("l2ui_ch3.etc.glowbtn1"));
    GlowTex[1] = FindTexture(TEXT("l2ui_ch3.etc.glowbtn2"));
    bFadeIn = 0;
    Alpha = 0;
    bGlowing = 0;
    RingSize = 50;
    return 1;
}
NW_HOOK_METHOD(0x100020c0, NCEffectButton, OnCreate)

// 0x10002120 (vslot 36, ret 4) - the blink flips; the alpha goes up by 15 to 255, then down to nothing, where both timers end and the
// next start fades in again; the ring grows by 5 percent
INT NCEffectButton::OnTimer(INT TimerID)
{
    if (TimerID == TIMER_EFFECT_BLINK)
    {
        bBlinkOn = !bBlinkOn;
        return 1;
    }
    if (TimerID == TIMER_EFFECT_FADE)
    {
        if (bFadeIn)
        {
            Alpha += 15;
            if (Alpha > 255)
            {
                Alpha = 255;
                bFadeIn = 0;
            }
            return 1;
        }
        Alpha -= 15;
        if (Alpha < 0)
        {
            Alpha = 0;
            bFadeIn = 1;
            KillTimer(TIMER_EFFECT_FADE);
            KillTimer(TIMER_EFFECT_GROW);
            bGlowing = 0;
        }
        return 1;
    }
    if (TimerID == TIMER_EFFECT_GROW)
        RingSize += 5;
    return 1;
}
NW_HOOK_METHOD(0x10002120, NCEffectButton, OnTimer)

// 0x10002700 (vslot 62) - the button's face, then the glow (a 128 x 128 picture around the button, the ring growing from the middle
// of it) while it runs, else the over face while the blink is on
INT NCEffectButton::OnPaint(NCGDevice* Device)
{
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    DrawButton(Device);
    if (bGlowing)
    {
        Device->DrawTexture(-0x30, -0x30, 0x80, 0x80, 0, 0, 0x80, 0x80, GlowTex[0], (BYTE)Alpha, 0);
        INT Size = RingSize * 0x80 / 100;
        INT Pos = 0x10 - Size / 2;
        Device->DrawTexture(Pos, Pos, Size, Size, 0, 0, 0x80, 0x80, GlowTex[1], (BYTE)Alpha, 0);
    }
    else if (bBlinkOn)
    {
        Device->DrawTexture(0, 0, 0x20, 0x20, 0, 0, 0x20, 0x20, OverTex, 0xFF, 1);
    }
    Device->ReleaseClip();
    Device->EndPaint();
    return 1;
}
NW_HOOK_METHOD(0x10002700, NCEffectButton, OnPaint)

// 0x10002830 (vslot 69, ret 8; the db says OnLButtonDown) - the button's own release, then by Kind: the tutorial's question mark is
// asked for; the character window's quest tab opens with the quest selected in its tree; the board opens on its mail tab. The button
// hides
INT NCEffectButton::OnLButtonUp(WPARAM wParam, LPARAM lParam)
{
    NCButton::OnLButtonUp(wParam, lParam);
    switch (Kind)
    {
    case EFFECT_TutorialQuestion:
        GConsole->RequestTutorialQuestionMark(Param);
        break;
    case EFFECT_Quest:
        GConsole->GetDesktop()->ToggleCharacter(4);
        {
            NCQuestGaraTreeWnd* QuestWnd = (NCQuestGaraTreeWnd*)GConsole->GetDesktop()->MainWnd->QuestWnd;
            if (QuestWnd->TreeCtrl)
            {
                QuestWnd->TreeCtrl->SelectedID = Param;
                HideWindow();
                return 1;
            }
        }
        break;
    case EFFECT_Mail:
        ((NCBoardWnd*)GConsole->GetDesktop()->BoardWnd)->RunTabCommand(5);
        GConsole->GetDesktop()->BoardWnd->ShowWindow();
        ((NCBoardWnd*)GConsole->GetDesktop()->BoardWnd)->TabCtrl->SetTab(5, 0);
        HideWindow();
        return 1;
    }
    HideWindow();
    return 1;
}
NW_HOOK_METHOD(0x10002830, NCEffectButton, OnLButtonUp)

NW_CHECK_VTABLE(NCEffectButton, 0x1019f3c0, 75)
