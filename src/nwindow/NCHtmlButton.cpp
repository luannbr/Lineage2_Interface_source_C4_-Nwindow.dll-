/*=============================================================================
    NCHtmlButton.cpp - NCHtmlButton and NCHtmlPushButton, the buttons of the HTML pages.
    Stock: 0x1002fee0 .. 0x1002ff16, 0x10030840 .. 0x10030fb9 (WindowProc is a body the linker
    folded with the other buttons').
=============================================================================*/
#include "nwindow/NCHtmlButton.h"
#include "nwindow/NCHtmlViewer.h"
#include "nwindow/NCGDevice.h"

/*-----------------------------------------------------------------------------
    NCHtmlButton.
-----------------------------------------------------------------------------*/

// 0x1004ac30 - NCButton's, nothing more
NCHtmlButton::NCHtmlButton()
{
}
NW_HOOK_CTOR(0x1004ac30, NCHtmlButton)

// 0x1004ac80 - the vtable, ConditionalDestroy, then NCButton's destructor (0x10003140, the stock one). Hook build only: the
// hook replaces the stock destructor body by this; the standalone's compiler writes our vtable and runs ConditionalDestroy and the
// base destructor in the destructor it generates for the class
#if NW_HOOK_MODE
void NCHtmlButton::DestroyBody()
{
    *(void**)this = ::nw::Addr(0x101A9FE8);
    ConditionalDestroy();
    ::nw::StockCallAt<void>(0x10003140, this);
    __asm nop   // keeps the call from becoming a tail jump (the stock function ends with call + ret)
}
NW_HOOK_METHOD(0x1004ac80, NCHtmlButton, DestroyBody)
#endif


// 0x1004acd0
NCHtmlButton::NCHtmlButton(const TCHAR* NormalTexName, const TCHAR* PushedTexName,
                           const TCHAR* InCommand, UBOOL bInAcceptKey)
    : NCButton(NormalTexName, PushedTexName, bInAcceptKey)
{
    Command = InCommand;
}
NW_HOOK_CTOR_ARGS(0x1004acd0, NCHtmlButton,
                  (const TCHAR* A, const TCHAR* B, const TCHAR* C, UBOOL D), (A, B, C, D))

// 0x1004ad00 - the textures, Enter and Space do not click it; Command is left as allocated
NCHtmlButton::NCHtmlButton(const TCHAR* NormalTexName, const TCHAR* PushedTexName)
    : NCButton(NormalTexName, PushedTexName, 0)
{
}
NW_HOOK_CTOR_ARGS(0x1004ad00, NCHtmlButton, (const TCHAR* A, const TCHAR* B), (A, B))


// 0x1003a790 (vslot 25) - the stock body is NCExButton's (hooked in NCButton.cpp); C2: no enabled test
INT NCHtmlButton::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return NCButton::WindowProc(Msg, wParam, lParam) != 0;
}

// 0x10047d00 (vslot 62) - NCButton::DrawButton (0x10001000, a function of its own in C4; C3 had it inline) clipped to the
// enclosing NCHtmlViewer (no clip when there is none); guarded
INT NCHtmlButton::OnPaint(NCGDevice* Device)
{
    guard(NCHtmlButton::OnPaint);
    Device->BeginPaint(ScreenX, ScreenY);
    NCWnd* Owner = EnclosingHtmlViewer(this);
    if (Owner)
        Device->SetClip(Owner->ScreenX, Owner->ScreenY, (INT)Owner->Width, (INT)Owner->Height);
    DrawButton(Device);   // 0x10001000
    Device->ReleaseClip();
    Device->EndPaint();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10047d00, NCHtmlButton, OnPaint)

// 0x10047410 (vslot 69) - a release inside reports Command, even when disabled, without sound. C4: the field the base
// header calls bReleased is the button's state here (0 normal, 1 pushed, 2 disabled: DrawButton 0x10001000 switches on it)
INT NCHtmlButton::OnLButtonUp(WPARAM wParam, LPARAM lParam)
{
    if (IsInWindow(LOWORD(lParam), HIWORD(lParam)))
        NotifyParent((LPARAM)Command);
    bReleased = 0;
    return 1;
}
NW_HOOK_METHOD(0x10047410, NCHtmlButton, OnLButtonUp)

NW_CHECK_VTABLE(NCHtmlButton, 0x101a9fe8, 75)

/*-----------------------------------------------------------------------------
    NCHtmlPushButton.
-----------------------------------------------------------------------------*/

// 0x1004ad30  NOT PORTED (does not compile against the C2 headers: verify_data: disps stock-only {} ours-only {294})
// 0x1004ad30 - NCPushButton's default constructor and the vtable
NCHtmlPushButton::NCHtmlPushButton()
{
}
NW_HOOK_CTOR(0x1004ad30, NCHtmlPushButton)


// 0x1004add0 - NCPushButton's default constructor, then the textures
NCHtmlPushButton::NCHtmlPushButton(const TCHAR* NormalTexName, const TCHAR* PushedTexName)
{
    SetNormalTex(NormalTexName);
    SetPushedTex(PushedTexName);
}
NW_HOOK_CTOR_ARGS(0x1004add0, NCHtmlPushButton, (const TCHAR* A, const TCHAR* B), (A, B))

// 0x1004ad80 - the vtable, ConditionalDestroy, then NCPushButton's destructor (0x100038F0). Hook build only, as the button's
#if NW_HOOK_MODE
void NCHtmlPushButton::DestroyBody()
{
    *(void**)this = ::nw::Addr(0x101AA118);
    ConditionalDestroy();
    ::nw::StockCallAt<void>(0x100038F0, this);
    __asm nop
}
NW_HOOK_METHOD(0x1004ad80, NCHtmlPushButton, DestroyBody)
#endif


// 0x10047e00 (vslot 62) - the same, clipped to the first ancestor that is not a control (the scroll buttons of an HTML
// edit box); guarded
INT NCHtmlPushButton::OnPaint(NCGDevice* Device)
{
    guard(NCHtmlPushButton::OnPaint);
    Device->BeginPaint(ScreenX, ScreenY);
    NCWnd* Owner = FirstNonControlParent(this);
    if (Owner)
        Device->SetClip(Owner->ScreenX, Owner->ScreenY, (INT)Owner->Width, (INT)Owner->Height);
    DrawButton(Device);   // 0x10001000
    Device->ReleaseClip();
    Device->EndPaint();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10047e00, NCHtmlPushButton, OnPaint)

NW_CHECK_VTABLE(NCHtmlPushButton, 0x101aa118, 75)
