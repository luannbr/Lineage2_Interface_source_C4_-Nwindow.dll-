/*=============================================================================
    NHtmlAnalyzer.cpp - an empty scroll window. Stock: 0x100240a0 .. 0x1002428d; WindowProc,
    OnCreate and OnDestroy are bodies shared with other classes.
=============================================================================*/
#include "nwindow/NHtmlAnalyzer.h"

// 0x10037f30 - the deques' constructors are inlined
NHtmlAnalyzer::NHtmlAnalyzer()
{
    UnknownCC = 0;
    UnknownD0 = 0;
    UnknownD4 = 0;
    Unknown100 = 0;
    Unknown104 = 0;
    Unknown108 = 5;
}
NW_HOOK_CTOR(0x10037f30, NHtmlAnalyzer)

// 0x10038000
NW_HOOK_DTOR(0x10038000, NHtmlAnalyzer)

// stock body shared with NCItemWnd's (0x1006bb90)
INT NHtmlAnalyzer::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return NCScrollWnd::WindowProc(Msg, wParam, lParam) != 0;
}

// stock body shared with NCNoticeInfoWnd's (0x10079480)
INT NHtmlAnalyzer::OnCreate()
{
    NCScrollWnd::OnCreate();
    return 1;
}

// 0x10037e20 (vslot 38) - the stock body (the base's OnDestroy, 1) is shared by the classes of this kind
INT NHtmlAnalyzer::OnDestroy()
{
    NCScrollWnd::OnDestroy();
    return 1;
}
NW_HOOK_METHOD(0x10037e20, NHtmlAnalyzer, OnDestroy)

// 0x10037e30 (vslot 62)
INT NHtmlAnalyzer::OnPaint(NCGDevice* Device)
{
    guard(NHtmlAnalyzer::OnPaint);
    NCScrollWnd::OnPaint(Device);
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10037e30, NHtmlAnalyzer, OnPaint)

NW_CHECK_VTABLE(NHtmlAnalyzer, 0x101a74f8, 78)
