/*=============================================================================
    NCInvenWeightWnd.cpp - the weight bar of the inventory window (C4 only). Stock: 0x10099840 (constructor), 0x100998b0 (destructor),
    0x10093d40 (OnCreate), 0x10093e20 (OnDestroy), 0x10095630 (OnPaint), 0x10098a50 (OnDrawToolTip).
=============================================================================*/
#include "nwindow/NCInvenWeightWnd.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NConsoleWnd.h"

#include <stdio.h>
#include <wchar.h>

// 0x10099840 - NCWnd's constructor; the textures are left as allocated
NCInvenWeightWnd::NCInvenWeightWnd()
{
}
NW_HOOK_CTOR(0x10099840, NCInvenWeightWnd)

// 0x100998b0
NW_HOOK_DTOR(0x100998b0, NCInvenWeightWnd)

// 0x10093d40 (vslot 37, guard)
INT NCInvenWeightWnd::OnCreate()
{
    guard(NCInvenWeightWnd::OnCreate);
    NCWnd::OnCreate();
    WeightTex[0] = FindTexture(TEXT("L2UI_CH3.PlayerStatusWnd.PS_weightbar1"));
    WeightTex[1] = FindTexture(TEXT("L2UI_CH3.PlayerStatusWnd.PS_weightbar2"));
    WeightTex[2] = FindTexture(TEXT("L2UI_CH3.PlayerStatusWnd.PS_weightbar3"));
    WeightTex[3] = FindTexture(TEXT("L2UI_CH3.PlayerStatusWnd.PS_weightbar4"));
    WeightTex[4] = FindTexture(TEXT("L2UI_CH3.PlayerStatusWnd.PS_weightbar5"));
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10093d40, NCInvenWeightWnd, OnCreate)

// 0x10093e20 (vslot 38, guard)
INT NCInvenWeightWnd::OnDestroy()
{
    guard(NCInvenWeightWnd::OnDestroy);
    NCWnd::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10093e20, NCInvenWeightWnd, OnDestroy)

// 0x10095630 (vslot 62, guard) - the bar (weight * 85 / limit pixels, in the color of the percentage) and the percentage centered on
// it; with no limit or a bar of no pixel only the percentage, 0.00
INT NCInvenWeightWnd::OnPaint(NCGDevice* Device)
{
    guard(NCInvenWeightWnd::OnPaint);
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    User* Me = GConsole->GetMyUser();
    if (Me)
    {
        FLOAT Percent = 0.0f;
        INT Max = Me->MaxLoad;
        if (Max != 0)
        {
            INT Load = Me->Load;
            INT BarW = Load * 85 / Max;
            if (BarW > 0)
            {
                // the first test uses the unrounded quotient, the others the stored float
                DOUBLE Quotient = (DOUBLE)Load / Max * 100.0;
                Percent = (FLOAT)Quotient;
                UTexture* Tex = NULL;
                if (Quotient <= 50.0f)
                    Tex = WeightTex[0];
                else if (Percent <= 66.6)
                    Tex = WeightTex[1];
                else if (Percent > 66.6 && Percent <= 80.0f)
                    Tex = WeightTex[2];
                else if (Percent > 80.0f && Percent <= 100.0f)
                    Tex = WeightTex[3];
                else if (Percent > 100.0f)
                    Tex = WeightTex[4];
                if (Tex)
                    Device->DrawTexture(0, 0, BarW, 12, 0, 0, BarW, 12, Tex, 0xFF, 1);
            }
        }
        TCHAR Text[128];
        SIZE Size;
        _swprintf(Text, TEXT("%.2f%s"), (DOUBLE)Percent, TEXT("%"));
        NCGDevice::GetTextExtent(&Size, Text, 0, 0, 0, 0, 0);
        Device->DrawSpecialDigit(1, (INT)(Width * 0.5f - Size.cx / 2), 2, 0xFFDCDCDC, Text);
    }
    Device->ReleaseClip();
    Device->EndPaint();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10095630, NCInvenWeightWnd, OnPaint)

// 0x10098a50 (vslot 63, guard; the db says OnPostPaint) - the tip "[carried]/[limit]" in the tool tip skin (3, 3 off the window's rectangle)
INT NCInvenWeightWnd::OnDrawToolTip(NCGDevice* Device)
{
    guard(NCInvenWeightWnd::OnPostPaint);
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    User* Me = GConsole->GetMyUser();
    if (Me)
    {
        NToolTipInfo Tip;
        Tip.SetText(*FString::Printf(TEXT("[%d]/[%d]"), Me->Load, Me->MaxLoad), 1);   // 0x10032460
        if (ToolTipSkin)
        {
            NToolTipRect Wnd;
            Wnd.X = ScreenX;
            Wnd.Y = ScreenY;
            Wnd.W = Width;
            Wnd.H = Height;
            ToolTipSkin->Draw(Device, Tip, 3, 3, Wnd);
        }
    }
    Device->ReleaseClip();
    Device->EndPaint();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10098a50, NCInvenWeightWnd, OnDrawToolTip)

NW_CHECK_VTABLE(NCInvenWeightWnd, 0x101b6ac0, 74)
