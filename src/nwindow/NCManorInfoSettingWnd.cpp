/*=============================================================================
    NCManorInfoSettingWnd.cpp - the base of the lord's tables (new in C4): a title bar, the back picture and a list.
    Stock (C4): 0x1005F550 (constructor), 0x1005F4D0 (destructor), 0x10136AD0 .. 0x10136CE0 (WindowProc, OnDestroy, OnPaint,
    OnCreate).
=============================================================================*/
#include "nwindow/NCManorInfoSettingWnd.h"
#include "nwindow/NClasses.gen.h"
#include "nwindow/NGameData.h"
#include "nwindow/NString.h"

// 0x1005f550 - NCWnd's constructor and the vtable
NCManorInfoSettingWnd::NCManorInfoSettingWnd()
{
}
NW_HOOK_CTOR(0x1005f550, NCManorInfoSettingWnd)

// 0x1005f4d0 - ConditionalDestroy, then NCWnd's destructor
NW_HOOK_DTOR(0x1005f4d0, NCManorInfoSettingWnd)

// 0x10136ad0 (vslot 25) - the base's answer first; then the list's double click (its command, id 0xD0000011) goes to
// OnListCtrl with the second word
INT NCManorInfoSettingWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCWnd::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND && wParam == (WPARAM)NID_MANORINFOSETTING_LIST)
        return OnListCtrl(lParam) != 0;
    return 0;
}
NW_HOOK_METHOD(0x10136ad0, NCManorInfoSettingWnd, WindowProc)

// 0x10136b30 (vslot 38) - the list goes, then the base's
INT NCManorInfoSettingWnd::OnDestroy()
{
    guard(NCManorInfoSettingWnd::OnDestroy);
    ManorDestroyChild(this, ListCtrl);
    NCWnd::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10136b30, NCManorInfoSettingWnd, OnDestroy)

// 0x10136be0 (vslot 62) - the back picture from y = 20 to the bottom (the title bar is the frame control's)
INT NCManorInfoSettingWnd::OnPaint(NCGDevice* Device)
{
    guard(NCManorInfoSettingWnd::OnPaint);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    Device->BeginPaint(ScreenX, ScreenY);
    if (BackTex)
        Device->DrawTexture(0, 0x14, (INT)Width, (INT)(Height - 20.0f), 0, 0, (INT)Width, (INT)(Height - 20.0f), BackTex, 0xFF, 1);
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10136be0, NCManorInfoSettingWnd, OnPaint)

// 0x10136ce0 (vslot 37) - the title bar (id 0xD0000001), the back picture and the list (1000 rows, 10 shown, 3 columns
// to begin with; the text starts 3 pixels down)
INT NCManorInfoSettingWnd::OnCreate()
{
    guard(NCManorInfoSettingWnd::OnCreate);
    NCWnd::OnCreate();
    FrameCtrl = ManorNewFrameCtrl(1, 0, 0);
    FrameID = NID_MANORINFOSETTING_FRAME;
    FrameCtrl->ID = NID_MANORINFOSETTING_FRAME;
    FrameCtrl->SetPosition(0, 0, (INT)Width, 0x14);
    CreateChild(FrameCtrl, NWS_VISIBLE);

    BackTex = FindTexture(TEXT("L2UI_CH3.Manor.manor2_back"));

    ListCtrl = ManorNewListCtrl(0x3E8, 10, 3, 0x10, 0, 0x12);
    ListCtrl->ID = NID_MANORINFOSETTING_LIST;
    ListCtrl->SetPosition(7, 0x49, 0x24A, 0xC7);
    CreateChild(ListCtrl, 0x22);
    ListCtrl->SetTextOffset(0, 3);
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10136ce0, NCManorInfoSettingWnd, OnCreate)

// 0x100f76f0 (vslot 74) - the shared stub of the extra slot (returns 1, ret 4); the standalone has its own body
#if NW_HOOK_MODE
NW_NAKED INT NCManorInfoSettingWnd::OnListCtrl(INT Row) NW_STOCK_AT(0x100f76f0)
#else
INT NCManorInfoSettingWnd::OnListCtrl(INT Row)
{
    return 1;
}
#endif

NW_CHECK_VTABLE(NCManorInfoSettingWnd, 0x101addf8, 75)
