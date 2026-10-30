/*=============================================================================
    NCManorInfoDownWnd.cpp - the base of the manor information pages (new in C4): the back picture and a list.
    Stock (C4): 0x101334B0 (constructor), 0x10133430 (destructor), 0x101367B0 .. 0x10136960 (OnDestroy, OnPaint, OnCreate).
=============================================================================*/
#include "nwindow/NCManorInfoDownWnd.h"
#include "nwindow/NClasses.gen.h"
#include "nwindow/NGameData.h"
#include "nwindow/NString.h"

// 0x101334b0 - NCWnd's constructor and the vtable
NCManorInfoDownWnd::NCManorInfoDownWnd()
{
}
NW_HOOK_CTOR(0x101334b0, NCManorInfoDownWnd)

// 0x10133430 - ConditionalDestroy, then NCWnd's destructor
NW_HOOK_DTOR(0x10133430, NCManorInfoDownWnd)

// 0x101367b0 (vslot 38) - the list goes, then the base's
INT NCManorInfoDownWnd::OnDestroy()
{
    guard(NCManorInfoDownWnd::OnDestroy);
    ManorDestroyChild(this, ListCtrl);
    NCWnd::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x101367b0, NCManorInfoDownWnd, OnDestroy)

// 0x10136860 (vslot 62) - the back picture over the whole window
INT NCManorInfoDownWnd::OnPaint(NCGDevice* Device)
{
    guard(NCManorInfoDownWnd::OnPaint);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    Device->BeginPaint(ScreenX, ScreenY);
    if (BackTex)
        Device->DrawTexture(0, 0, (INT)Width, (INT)Height, 0, 0, (INT)Width, (INT)Height, BackTex, 0xFF, 1);
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10136860, NCManorInfoDownWnd, OnPaint)

// 0x10136960 (vslot 37) - the back picture and the list (1000 rows, 10 shown, 3 columns to begin with; the text starts 3
// pixels down)
INT NCManorInfoDownWnd::OnCreate()
{
    guard(NCManorInfoDownWnd::OnCreate);
    NCWnd::OnCreate();
    BackTex = FindTexture(TEXT("L2UI_CH3.Manor.manor1_back2"));

    ListCtrl = ManorNewListCtrl(0x3E8, 10, 3, 0x10, 0, 0x12);
    ListCtrl->ID = NID_MANORINFODOWN_LIST;
    ListCtrl->SetPosition(7, 0x2A, 0x24A, 0xC7);
    CreateChild(ListCtrl, 0x22);
    ListCtrl->SetTextOffset(0, 3);
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10136960, NCManorInfoDownWnd, OnCreate)

// 0x100023c0 (vslot 74) - the shared stub of the extra slot (ret 4: it takes a row, as NCManorInfoSettingWnd's OnListCtrl, and
// does nothing); the standalone has its own empty body
#if NW_HOOK_MODE
NW_NAKED INT NCManorInfoDownWnd::NewVirtual74(INT Row) NW_STOCK_AT(0x100023c0)
#else
INT NCManorInfoDownWnd::NewVirtual74(INT Row)
{
    return 0;
}
#endif

NW_CHECK_VTABLE(NCManorInfoDownWnd, 0x101d65d0, 75)
