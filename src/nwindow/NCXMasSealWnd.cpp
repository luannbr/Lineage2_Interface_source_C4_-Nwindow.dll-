/*=============================================================================
    NCXMasSealWnd.cpp - the Christmas seal window (C2 only). Stock (C3): 0x1010b020 .. 0x1010b2c0.
=============================================================================*/
#include "nwindow/NCXMasSealWnd.h"
#include "nwindow/NCFrameCtrl.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NGameData.h"

enum { NID_XMASSEAL_FRAME = 0xB000FA };   // the title bar's id (name ours)

// 0x10121140 - the first and the last member only; the back texture comes with OnCreate
NCXMasSealWnd::NCXMasSealWnd()
{
    Unknown98 = 0;
    SealTex = NULL;
}
NW_HOOK_CTOR(0x10121140, NCXMasSealWnd)

// 0x10121190
NW_HOOK_DTOR(0x10121190, NCXMasSealWnd)

// 0x10121020 (vslot 37) - the back texture, then the title bar (334 x 20; C4: SetPosition has the screen pin, FindTexture
// the fallback flag: the header's defaults)
INT NCXMasSealWnd::OnCreate()
{
    NCWnd::OnCreate();
    BackTex = FindTexture(TEXT("L2UI.MinimapWnd.MinimapBack"));

    FrameCtrl = new NCFrameCtrl(1, 0, 0);
    FrameCtrl->ID = NID_XMASSEAL_FRAME;
    FrameCtrl->SetPosition(0, 0, 334, 20);
    CreateChild(FrameCtrl, NWS_VISIBLE);
    return 1;
}
NW_HOOK_METHOD(0x10121020, NCXMasSealWnd, OnCreate)

// 0x10120ed0 (vslot 38) - guarded; nothing of its own to free
INT NCXMasSealWnd::OnDestroy()
{
    guard(NCXMasSealWnd::OnDestroy);
    NCWnd::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10120ed0, NCXMasSealWnd, OnDestroy)

// 0x10120f50 (vslot 62) - the panel below the title bar, the seal inside it
INT NCXMasSealWnd::OnPaint(NCGDevice* Device)
{
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    Device->BeginPaint(ScreenX, ScreenY);
    if (BackTex)
        Device->DrawTexture(0, 20, 334, 333, 0, 0, 334, 333, BackTex, 0xFF, 1);
    if (SealTex)
        Device->DrawTexture(3, 21, 328, 329, 0, 0, 328, 329, SealTex, 0xFF, 1);
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
}
NW_HOOK_METHOD(0x10120f50, NCXMasSealWnd, OnPaint)

// 0x101210f0 (name ours) - the item the console shows as the seal (Unknown98 keeps its id): the picture is the texture of
// the name at +0x94 of its item record (C4; C3 +0x7C; the stock reads the record without a check)
void NCXMasSealWnd::SetItem(INT ItemID)
{
    Unknown98 = ItemID;
    NViewItemData* Data = GL2GameData.ItemTable->FindRef(Unknown98);
    SealTex = FindTexture(*nw::Field<L2FName>(Data, 0x94));
}
NW_HOOK_METHOD(0x101210f0, NCXMasSealWnd, SetItem)

NW_CHECK_VTABLE(NCXMasSealWnd, 0x101d1020, 74)
