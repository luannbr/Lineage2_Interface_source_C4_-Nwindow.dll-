/*=============================================================================
    NTownMapWnd.cpp - the town map window (C2 only). Stock: 0x10098e80 .. 0x10098ec2,
    0x10098fe0 .. 0x100991af and 0x100998a0 .. 0x10099b6f.
=============================================================================*/
#include "nwindow/NTownMapWnd.h"
#include "nwindow/NCFrameCtrl.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NConsoleWnd.h"

enum { NID_TOWNMAP_FRAME = 0xB000F0 };   // the title bar's id (name ours)

// 0x100bcb50 - the textures and the drag cleared, the view 328 x 328 of a 512 x 512 picture (the mark's place is left)
NTownMapWnd::NTownMapWnd()
{
    BackTex = NULL;
    MapTex = NULL;
    IconTex = NULL;
    DragX = 0;
    DragY = 0;
    ScrollX = 0;
    ScrollY = 0;
    bDragging = 0;
    ViewW = 0x148;
    ViewH = 0x148;
    MapW = 0x200;
    MapH = 0x200;
}
NW_HOOK_CTOR(0x100bcb50, NTownMapWnd)


// 0x100b9010 (vslot 30; 16 bytes; the db calls it NCMinimapWnd::KillFocus, but NCMinimapWnd's own slot 30 is NCWnd's 0x100347A0: this one is
// in the vtables of NTownMapWnd and NCMapCoverWnd only, see classes.json) - a drag in progress lets the cursor go, then NCWnd's body.
// NCMapCoverWnd::KillFocus (a stub that jumps to this address) reaches the hook
void NTownMapWnd::KillFocus()
{
    UnlockCursor();
    NCWnd::KillFocus();
}
NW_HOOK_METHOD(0x100b9010, NTownMapWnd, KillFocus)

// 0x100b8cc0 (vslot 38) - guarded; NCWnd's OnDestroy and nothing else
INT NTownMapWnd::OnDestroy()
{
    guard(NTownMapWnd::OnDestroy);
    NCWnd::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100b8cc0, NTownMapWnd, OnDestroy)

// C2 CANDIDATE: the C2 function is not the same code as the C1 one; kept only while it matches the stock one (verify_data)
// 0x100b8d40 (name ours; the console's ShowTownMap calls it) - the town's picture by name and the player's place
// in it (a place outside the picture counts as its corner), the view centred on the place and then kept inside
// the picture. A NULL name changes nothing.
void NTownMapWnd::SetMap(const TCHAR* TexName, INT X, INT Y)
{
    if (!TexName)
        return;
    MapTex = FindTexture(TexName);
    if (X >= 0 && X <= MapW)
        IconX = X;
    else
        IconX = 0;
    if (Y >= 0 && Y <= MapH)
        IconY = Y;
    else
        IconY = 0;
    ScrollX = IconX - ViewW / 2;
    ScrollY = IconY - ViewH / 2;
    if (ScrollX < 0)
        ScrollX = 0;
    else if (ScrollX > MapW - ViewW)
        ScrollX = MapW - ViewW;
    if (ScrollY < 0)
        ScrollY = 0;
    else if (ScrollY > MapH - ViewH)
        ScrollY = MapH - ViewH;
}
NW_HOOK_METHOD(0x100b8d40, NTownMapWnd, SetMap)

// 0x100b8e20 (vslot 68) - the point relative to the view's corner (3, 21), kept at least there: a click inside the
// picture starts a drag (the cursor is locked)
INT NTownMapWnd::OnLButtonDown(WPARAM wParam, LPARAM lParam)
{
    INT X = (SWORD)LOWORD(lParam) - ScreenX - 3;
    INT Y = (SWORD)HIWORD(lParam) - ScreenY - 0x15;
    if (X <= 3)
        X = 3;
    if (Y <= 0x15)
        Y = 0x15;
    if (X >= 4 && X <= (INT)Width && Y >= 0x16 && Y <= (INT)Height)
    {
        DragX = X;
        DragY = Y;
        bDragging = 1;
        SetCursor(3, NULL);
        GConsole->LockCursor();
    }
    return 1;
}
NW_HOOK_METHOD(0x100b8e20, NTownMapWnd, OnLButtonDown)

// 0x100b8eb0 (vslot 69) - the drag ends and the console lets the cursor go
INT NTownMapWnd::OnLButtonUp(WPARAM wParam, LPARAM lParam)
{
    GConsole->UnlockCursor();
    bDragging = 0;
    return 1;
}
NW_HOOK_METHOD(0x100b8eb0, NTownMapWnd, OnLButtonUp)

// 0x100b8ee0 (vslot 67) - with the focus and a drag in progress the picture follows the mouse (the view's corner is
// moved by what the point did since the last one) and stays inside the picture
INT NTownMapWnd::OnMouseMove(WPARAM wParam, LPARAM lParam)
{
    if (!IsFocused())
        return 1;
    INT X = (SWORD)LOWORD(lParam) - ScreenX - 3;
    INT Y = (SWORD)HIWORD(lParam) - ScreenY - 0x15;
    if (X <= 3)
        X = 3;
    if (Y <= 0x15)
        Y = 0x15;
    if (bDragging && X >= 4 && X <= (INT)Width && Y >= 0x16 && Y <= (INT)Height)
    {
        ScrollX += DragX - X;
        ScrollY += DragY - Y;
        DragX = X;
        DragY = Y;
        if (ScrollX < 0)
            ScrollX = 0;
        else if (ScrollX > MapW - ViewW)
            ScrollX = MapW - ViewW;
        if (ScrollY < 0)
        {
            ScrollY = 0;
            return 1;
        }
        if (ScrollY > MapH - ViewH)
            ScrollY = MapH - ViewH;
    }
    return 1;
}
NW_HOOK_METHOD(0x100b8ee0, NTownMapWnd, OnMouseMove)

// 0x100bb3c0 (vslot 37) - a title bar (334 x 21, the stock frame control with (1, 0, 0)), the minimap's frame texture and the mark's
// (looked up with the second argument in C4)
INT NTownMapWnd::OnCreate()
{
    NCWnd::OnCreate();
    FrameCtrl = new NCFrameCtrl(1, 0, 0);
    FrameCtrl->ID = NID_TOWNMAP_FRAME;
    FrameCtrl->SetPosition(0, 0, 0x14E, 0x15);
    CreateChild(FrameCtrl, NWS_VISIBLE);
    BackTex = FindTexture(TEXT("L2UI.MinimapWnd.MinimapBack"), 1);
    IconTex = FindTexture(TEXT("town_map.icon_i00"), 1);
    return 1;
}
NW_HOOK_METHOD(0x100bb3c0, NTownMapWnd, OnCreate)

// 0x100b9de0 (vslot 62) - the clip is 3 px short of the window's height; the picture's part at (3, 21), the mark
// at its place in the part, the frame texture (334 x 333) over them; the origin goes on after the clip
INT NTownMapWnd::OnPaint(NCGDevice* Device)
{
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)(Height - 3.0f));
    Device->BeginPaint(ScreenX, ScreenY);
    if (MapTex)
        Device->DrawTexture(3, 0x15, ViewW, ViewH, ScrollX, ScrollY, ViewW, ViewH, MapTex, 0xFF, 1);
    if (IconTex)
        Device->DrawTexture(IconX - ScrollX + 3, IconY - ScrollY + 0x15, 0x20, 0x20, 0, 0, 0x20, 0x20, IconTex, 0xFF, 1);
    if (BackTex)
        Device->DrawTexture(0, 0x15, 0x14E, 0x14D, 0, 0, 0x14E, 0x14D, BackTex, 0xFF, 1);
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
}
NW_HOOK_METHOD(0x100b9de0, NTownMapWnd, OnPaint)

// 0x100bcbf0 - the vtable, ConditionalDestroy, NCWnd
NW_HOOK_DTOR(0x100bcbf0, NTownMapWnd)

NW_CHECK_VTABLE(NTownMapWnd, 0x101bdc10, 74)
