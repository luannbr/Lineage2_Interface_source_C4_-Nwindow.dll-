/*=============================================================================
    NCThaiIME.cpp - IME of the Thai client (new in C2). Stock: 0x100125a0 .. 0x100126b0 +
    0x100126b0 .. 0x10012773, 0x10013790 .. 0x1001382f (constructor, destructor); ShowWindow is the
    shared 0x1007b460 body.
=============================================================================*/
#include "nwindow/NCThaiIME.h"
#include "nwindow/NCChatWnd.h"
#include "nwindow/NCConsole.h"
#include "nwindow/NCEditBox.h"
#include "nwindow/NCIMEManager.h"
#include "nwindow/NConsoleWnd.h"

// 0x1001d770
NCThaiIME::NCThaiIME(INT InMode)
    : NCIME(0)
{
    Mode = InMode;
}
NW_HOOK_CTOR_ARGS(0x1001d770, NCThaiIME, (INT A), (A))

// 0x1001d7c0
NW_HOOK_DTOR(0x1001d7c0, NCThaiIME)

// 0x10017620 - the class's operator new (NW_DECLARE_CLASS), out of line for the manager's OnCreate (cdecl: the size is
// not read; the object is allocated in Outer under Name with SetFlags, as the Chinese IME's at 0x100155c0)
static void* NewThaiIME(size_t Size, UObject* Outer, FName Name, DWORD SetFlags)
{
    return NCThaiIME::operator new(Size, Outer, Name, SetFlags);
}
NW_HOOK_FUNC(0x10017620, NewThaiIME)


// 0x10017400 (vslot 25) - NCIME's messages again after NCIME: the notify, the context and the layout change
INT NCThaiIME::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCIME::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == WM_IME_NOTIFY)
        return OnImeNotify(wParam, lParam) != 0;
    if (Msg == WM_IME_SETCONTEXT)
        return OnImeSetContext(wParam, lParam) != 0;
    if (Msg == WM_INPUTLANGCHANGE)
        return OnInputLangChange(wParam, lParam) != 0;
    return 0;
}
NW_HOOK_METHOD(0x10017400, NCThaiIME, WindowProc)

// (Activate 0x100165d0, OnCreate 0x100178e0 and OnDestroy 0x10016520 are NCIME's: the stock slots point there)

// 0x1007b460 (vslot 43) - one body for every class that only calls NCWnd's (hooked with NCEditBox's)
void NCThaiIME::ShowWindow()
{
    NCWnd::ShowWindow();
}

// 0x100174a0 (vslot 77) - handled only while active: a change of the open status (8) closes the IME again in the
// restricted fields (Mode 2 and 6) and clears their conversion mode; every other notification is ignored (0)
INT NCThaiIME::OnImeNotify(WPARAM wParam, LPARAM lParam)
{
    if (!bActive)
        return 0;
    if (wParam == IMN_SETOPENSTATUS)
    {
        HIMC hIMC = ImeGetContext();
        if (Mode == 6 || Mode == 2)
        {
            if (ImmGetOpenStatus(hIMC))
                ImmSetOpenStatus(hIMC, 0);
            ImmSetConversionStatus(hIMC, 0, 0);
        }
        ImeReleaseContext(hIMC);
        return 1;
    }
    return 0;
}
NW_HOOK_METHOD(0x100174a0, NCThaiIME, OnImeNotify)

// 0x10017510 (vslot 76) - NCIME's, then the composition cancelled and the candidate window closed on the game
// window's context
void NCThaiIME::Deactivate()
{
    NCIME::Deactivate();
    HIMC hIMC = ImeGetContext();
    ImmNotifyIME(hIMC, NI_COMPOSITIONSTR, CPS_CANCEL, 0);
    ImmNotifyIME(hIMC, NI_CLOSECANDIDATE, 0, 0);
    ImeReleaseContext(hIMC);
}
NW_HOOK_METHOD(0x10017510, NCThaiIME, Deactivate)

NW_CHECK_VTABLE(NCThaiIME, 0x101a5000, 86)
