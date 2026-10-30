/*=============================================================================
    NCRadarWnd.cpp - the radar. Stock: 0x100a1c90 .. 0x100a1db5, 0x100a1df0 .. 0x100a1e4f.
=============================================================================*/
#include "nwindow/NCRadarWnd.h"
#include "nwindow/NCGDevice.h"

// 0x100ef9f0
NCRadarWnd::NCRadarWnd()
{
}
NW_HOOK_CTOR(0x100ef9f0, NCRadarWnd)

// 0x100efa50
NW_HOOK_DTOR(0x100efa50, NCRadarWnd)

// stock body shared with NControl's (0x10004e90)
INT NCRadarWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return NCWnd::WindowProc(Msg, wParam, lParam) != 0;
}

// 0x100ef780 (vslot 37) - NCWnd's OnCreate and 1 (one body for several classes)
INT NCRadarWnd::OnCreate()
{
    NCWnd::OnCreate();
    return 1;
}
NW_HOOK_METHOD(0x100ef780, NCRadarWnd, OnCreate)

// 0x100ef890 (vslot 38)
INT NCRadarWnd::OnDestroy()
{
    guard(NCRadarWnd::OnDestroy);
    Targets.Empty();
    NCWnd::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100ef890, NCRadarWnd, OnDestroy)

// 0x100de5e0 (vslot 56) - the radar coordinates are the world's. C3: the canvas draws the radar in three steps (the
// back, one mark per target, the rest: UCanvas::DrawRadarBack / DrawRadarTarget / DrawRadarEtc through the device's
// 0x100ef790, 0x10010db0 and 0x10010da0; the device header has the C2 pair only, so the last two go by address)
INT NCRadarWnd::OnPaint(NCGDevice* Device)
{
    guard(NCRadarWnd::OnPaint);
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    Device->DrawRadar();
    for (INT i = 0; i < Targets.Num(); i++)
    {
        FMinimapTarget T = Targets(i);
        Device->DrawRadarTarget(T.X, T.Y);   // 0x10013000
    }
    Device->DrawRadarEtc();                  // 0x10012ff0
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100ef790, NCRadarWnd, OnPaint)

// 0x100ef9d0 - the target list emptied (the base's ClearTargets address 0x10048300 is NCPledgeWnd::Reset here)
void NCRadarWnd::ClearTargets()
{
    Targets.Empty();
}
NW_HOOK_METHOD(0x100ef9d0, NCRadarWnd, ClearTargets)

// the stock body is NCMinimapWnd's (0x10079230)
void NCRadarWnd::AddTarget(INT X, INT Y, INT Z)
{
    INT i = Targets.Add();
    Targets(i).X = X;
    Targets(i).Y = Y;
    Targets(i).Z = Z;
}

// the stock body is NCMinimapWnd's (0x10079270), with its skip after a removed entry
void NCRadarWnd::RemoveTarget(INT X, INT Y, INT Z)
{
    for (INT i = 0; i < Targets.Num(); i++)
    {
        FMinimapTarget T = Targets(i);
        if (T.X == X && T.Y == Y && T.Z == Z)
            Targets.Remove(i, 1);
    }
}

NW_CHECK_VTABLE(NCRadarWnd, 0x101c6bc8, 74)
