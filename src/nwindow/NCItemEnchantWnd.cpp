/*=============================================================================
    NCItemEnchantWnd.cpp - the window that asks which item an enchant scroll is for, and its grid (new in C4).
    Stock (C4): 0x10093FC0 .. 0x10095F70 (WindowProc, OnOK, OnCancel, HideWindow, the grid's OnLButtonDown, OnPaint,
    OnDestroy), 0x1009A0E0 .. 0x1009A526 (constructors, destructors, OnCreate).
=============================================================================*/
#include "nwindow/NCItemEnchantWnd.h"
#include "nwindow/NCButton.h"
#include "nwindow/NCConsole.h"
#include "nwindow/NCFrameCtrl.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NConsoleWnd.h"
#include "nwindow/NItemC4.h"

// ids of the two buttons (name ours)
enum
{
    NID_ENCHANT_OK = 0xF0F012,
    NID_ENCHANT_CANCEL = 0xF0F013,
};

// sysstring labels
enum
{
    STR_OK = 140,
    STR_Cancel = 141,
};

// the window's frame: a title bar of 256 x 20 (NCFrameCtrl's constructor, three arguments, ret 0xc)
static NCFrameCtrl* NewFrameCtrl(INT A, INT B, INT C)
{
    return new NCFrameCtrl(A, B, C);
}

// a button of the standard style (NCButton's constructor 0x100034E0: style 0 = "btn1_normal" textures at their own size,
// then whether Enter / Space click it)
static NCButton* NewStyleButton(INT Style, UBOOL bAcceptKey)
{
    return new NCButton(Style, bAcceptKey);
}

// a child window goes (repeated inline in the stock OnDestroy)
template <class T> static FORCEINLINE void DestroyChild(NCWnd* Parent, T*& Child)
{
    if (Child)
    {
        Parent->RemoveChildWindow(Child->Entry);
        delete Child;
        Child = NULL;
    }
}

/*-----------------------------------------------------------------------------
    NCItemEnchantItemWnd.
-----------------------------------------------------------------------------*/

// 0x1009a0e0
NCItemEnchantItemWnd::NCItemEnchantItemWnd()
{
}
NW_HOOK_CTOR(0x1009a0e0, NCItemEnchantItemWnd)

// 0x1009a180 (ret 0x18; anonymous in the db) - the grid of the six arguments (columns, rows, most items, icon size, gaps)
NCItemEnchantItemWnd::NCItemEnchantItemWnd(INT InColumns, INT InVisibleRows, INT InMaxItems, INT InCellSize, INT InGapX,
                                           INT InGapY)
    : NCItemWnd(InColumns, InVisibleRows, InMaxItems, InCellSize, InGapX, InGapY)
{
}
NW_HOOK_CTOR_ARGS(0x1009a180, NCItemEnchantItemWnd, (INT A, INT B, INT C, INT D, INT E, INT F), (A, B, C, D, E, F))

// 0x1009a130
NW_HOOK_DTOR(0x1009a130, NCItemEnchantItemWnd)

// 0x100ad790 (vslot 25) - NCSkillItemWnd's body, shared (hooked there): NCItemWnd's answer as a plain 0 / 1
INT NCItemEnchantItemWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return NCItemWnd::WindowProc(Msg, wParam, lParam) != 0;
}

// 0x10095d40 (vslot 68) - selects the item under the mouse; nothing is picked up and no sound plays
INT NCItemEnchantItemWnd::OnLButtonDown(WPARAM wParam, LPARAM lParam)
{
    INT Index = HitTest(LOWORD(lParam), HIWORD(lParam), 0, 0);
    Selected = Index;
    HoverIndex = Index;
    if (Index >= 0 && Index < Items.Num())
    {
        UnselectAll();
        Items(Selected)->Select();
        bDragging = 0;
    }
    return 1;
}
NW_HOOK_METHOD(0x10095d40, NCItemEnchantItemWnd, OnLButtonDown)

NW_CHECK_VTABLE(NCItemEnchantItemWnd, 0x101b6d88, 81)

/*-----------------------------------------------------------------------------
    NCItemEnchantWnd.
-----------------------------------------------------------------------------*/

// 0x1009a1c0
NCItemEnchantWnd::NCItemEnchantWnd()
{
}
NW_HOOK_CTOR(0x1009a1c0, NCItemEnchantWnd)

// 0x1009a210
NW_HOOK_DTOR(0x1009a210, NCItemEnchantWnd)

// 0x10093fc0 (vslot 25) - the base's answer first; then the two buttons' commands
INT NCItemEnchantWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCWnd::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND)
    {
        if (wParam == NID_ENCHANT_OK)
            return OnOK() != 0;
        if (wParam == NID_ENCHANT_CANCEL)
            return OnCancel() != 0;
    }
    return 0;
}
NW_HOOK_METHOD(0x10093fc0, NCItemEnchantWnd, WindowProc)

// 0x10095f70 (vslot 74) - the selected item (if any) is the one: its object id goes to the console
INT NCItemEnchantWnd::OnOK()
{
    if (ItemWnd->GetSelected() >= 0)
    {
        NItemEntry* Item = ItemWnd->Items(ItemWnd->GetSelected());
        if (Item)
            GConsole->RequestEnchantItem(AsC4(Item)->ObjectID);
    }
    return 1;
}
NW_HOOK_METHOD(0x10095f70, NCItemEnchantWnd, OnOK)

// 0x10094040 (vslot 75) - none (-1), and the window closes
INT NCItemEnchantWnd::OnCancel()
{
    GConsole->RequestEnchantItem(-1);
    HideWindow();
    return 1;
}
NW_HOOK_METHOD(0x10094040, NCItemEnchantWnd, OnCancel)

// 0x10094070 (vslot 49) - the grid is emptied first (by the shared body 0x100fe540: it loads the pointer at +0xE8 and
// jumps to that object's slot 78, Clear; NCSSQSealStatusListWnd::ClearList in the db)
void NCItemEnchantWnd::HideWindow()
{
    ItemWnd->Clear();   // 0x100fe540
    NCWnd::HideWindow();
}
NW_HOOK_METHOD(0x10094070, NCItemEnchantWnd, HideWindow)

// 0x10095ed0 (vslot 38) - the base's, then the grid and the two buttons
INT NCItemEnchantWnd::OnDestroy()
{
    NCWnd::OnDestroy();
    DestroyChild(this, ItemWnd);
    DestroyChild(this, OKButton);
    DestroyChild(this, CancelButton);
    return 1;
}
NW_HOOK_METHOD(0x10095ed0, NCItemEnchantWnd, OnDestroy)

// 0x10095db0 (vslot 62) - the back texture under the title bar (20 pixels), the title (a text of the game data, 11, 32)
INT NCItemEnchantWnd::OnPaint(NCGDevice* Device)
{
    guard(NCItemEnchantWnd::OnPaint);
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    INT W = (INT)Width;
    INT H = (INT)(Height - 20.0f);
    Device->DrawTexture(0, 0x14, W, H, 0, 0, W, H, BackTex, 0xFF, 1);
    Device->DrawText(0xB, 0x20, 0xFFDCDCDC, *::nw::Field<FString>(&GL2GameData, 0x9C058), 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0,
                     0, 0, 0);
    Device->ReleaseClip();
    Device->EndPaint();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10095db0, NCItemEnchantWnd, OnPaint)

// 0x1009a260 (vslot 37) - the title bar (256 x 20), the grid (9, 48; 240 x 318, 6 x 9 cells of 32 pixels, 250 at most, gaps
// 5 and 3), OK (51, 372; 76 x 23) and Cancel (131, 372): the buttons are the standard style, ids 0xF0F012 / 0xF0F013
// (the stock code does not call NCWnd::OnCreate)
INT NCItemEnchantWnd::OnCreate()
{
    FrameCtrl = NewFrameCtrl(1, 0, 0);
    FrameCtrl->SetPosition(0, 0, 0x100, 0x14);
    CreateChild(FrameCtrl, NWS_VISIBLE | NWS_NOCLOSE);

    ItemWnd = new NCItemEnchantItemWnd(6, 9, 0xFA, 0x20, 5, 3);
    ItemWnd->SetPosition(9, 0x30, 0xF0, 0x13E);
    CreateChild(ItemWnd, NWS_VISIBLE | NWS_UNKNOWN20);

    OKButton = NewStyleButton(0, 0);
    OKButton->ID = NID_ENCHANT_OK;
    OKButton->SetPosition(0x33, 0x174, 0x4C, 0x17);
    CreateChild(OKButton, NWS_VISIBLE);
    OKButton->SetTextID(STR_OK);

    CancelButton = NewStyleButton(0, 0);
    CancelButton->ID = NID_ENCHANT_CANCEL;
    CancelButton->SetPosition(0x83, 0x174, 0x4C, 0x17);
    CreateChild(CancelButton, NWS_VISIBLE);
    CancelButton->SetTextID(STR_Cancel);

    BackTex = FindTexture(TEXT("L2UI_CH3.InventoryWnd.itemenchant_back"));
    return 1;
}
NW_HOOK_METHOD(0x1009a260, NCItemEnchantWnd, OnCreate)

NW_CHECK_VTABLE(NCItemEnchantWnd, 0x101b6ed0, 76)
