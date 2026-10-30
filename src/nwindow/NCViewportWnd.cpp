/*=============================================================================
    NCViewportWnd.cpp - a window that has the canvas render a camera view into a rectangle of it. Stock (C4):
    0x1011ff20, 0x101203a0, 0x10120c60, 0x10120ce0 (the device's helper 0x10013470 is listed with the class).
=============================================================================*/
#include "nwindow/NCViewportWnd.h"
#include "nwindow/NCGDevice.h"

// 0x10120ce0 - the window's base, the vtable, and the picture's rectangle emptied
NCViewportWnd::NCViewportWnd()
{
    OffsetY = 0;
    OffsetX = 0;
    ViewWidth = 0.0f;
    ViewHeight = 0.0f;
}
NW_HOOK_CTOR(0x10120ce0, NCViewportWnd)

// 0x10120c60 - the vtable, ConditionalDestroy, NCWnd
NW_HOOK_DTOR(0x10120c60, NCViewportWnd)

// 0x10119f60-style shared body: NControl's, shared (hooked there)
#if NW_HOOK_MODE
NW_NAKED INT NCViewportWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam) NW_STOCK(0x10148f10)
#else
INT NCViewportWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return NCWnd::WindowProc(Msg, wParam, lParam) != 0;   // 0x10148f10
}
#endif

// 0x100ef780 (vslot 37) - NCWnd's OnCreate and 1 (one body for several classes, hooked in NCRadarWnd.cpp)
#if NW_HOOK_MODE
NW_NAKED INT NCViewportWnd::OnCreate() NW_STOCK(0x100ef780)
#else
INT NCViewportWnd::OnCreate()
{
    NCWnd::OnCreate();   // 0x100ef780
    return 1;
}
#endif

// 0x1011ff20 (vslot 38)
INT NCViewportWnd::OnDestroy()
{
    guard(NCViewportWnd::OnDestroy);
    NCWnd::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1011ff20, NCViewportWnd, OnDestroy)

// 0x100f76f0 (vslot 62) - `return 1`, shared (hooked in NCScrollWnd.cpp)
#if NW_HOOK_MODE
NW_NAKED INT NCViewportWnd::OnPaint(NCGDevice* Device) NW_STOCK(0x100f76f0)
#else
INT NCViewportWnd::OnPaint(NCGDevice* Device)
{
    return 1;   // 0x100f76f0: mov eax, 1 / ret 4
}
#endif

// 0x10013470 - the device's helper of DrawViewport (anonymous in the stock image, a member of NCGDevice that the header does
// not carry): the canvas draws its camera scene at the origin plus (X, Y), W x H (UCanvas::DrawCameraSceneNode, slot 60)
// (a thiscall on the device: declared on a stand-in class so that the hook forwarder passes `this`; NCGDevice.h is not this
// lane's)
struct NGDeviceCameraDraw
{
    void DrawCameraScene(INT P1, INT X, INT Y, INT W, INT H)
    {
        NCGDevice* Device = reinterpret_cast<NCGDevice*>(this);
        Device->Canvas->DrawCameraSceneNode(P1, Device->OriginX + X, Device->OriginY + Y, W, H);
    }
};
NW_HOOK_METHOD(0x10013470, NGDeviceCameraDraw, DrawCameraScene)

// 0x101203a0 (vslot 74) - clipped to the rectangle, the canvas draws its scene there
INT NCViewportWnd::DrawViewport(NCGDevice* Device)
{
    guard(NCViewportWnd::DrawViewport);
    Device->SetClip(ScreenX + OffsetX, ScreenY + OffsetY, (INT)ViewWidth, (INT)ViewHeight);
    Device->BeginPaint(ScreenX, ScreenY);
    reinterpret_cast<NGDeviceCameraDraw*>(Device)->DrawCameraScene(0, OffsetX, OffsetY, (INT)ViewWidth, (INT)ViewHeight);
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x101203a0, NCViewportWnd, DrawViewport)

NW_CHECK_VTABLE(NCViewportWnd, 0x101d0d50, 75)
