/*=============================================================================
    NCIME.cpp - base of the language IMEs. Stock: 0x1000dee0 .. 0x1000e22f,
    0x1000e840 .. 0x1000e88f, 0x1000f2b0 .. 0x1000f2ff, 0x1000f6f0 .. 0x1000f758.
=============================================================================*/
#include "nwindow/NCIME.h"
#include "nwindow/NCrt.h"

#include <stdlib.h>

// (no GActiveIMC global in C4: see NCIME.h)

// 0x1001d360 - NControl's constructor is inlined in the stock code
NCIME::NCIME(INT InMode)
{
    Property = 0;
    OpenStatus = 0;
    Conversion = 0;
    Sentence = 0;
    CodePage = 0;
    bActive = 0;
    Mode = InMode;
    bNotComposing = 1;
}
NW_HOOK_CTOR_ARGS(0x1001d360, NCIME, (INT A), (A))

// 0x1001d200
NW_HOOK_DTOR(0x1001d200, NCIME)

// 0x100163c0 (vslot 25)
INT NCIME::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NControl::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == WM_IME_NOTIFY)
        return OnImeNotify(wParam, lParam) != 0;
    if (Msg == WM_IME_SETCONTEXT)
        return OnImeSetContext(wParam, lParam) != 0;
    if (Msg == WM_IME_STARTCOMPOSITION)
        return OnImeStartComposition(wParam, lParam) != 0;
    if (Msg == WM_IME_ENDCOMPOSITION)
        return OnImeEndComposition(wParam, lParam) != 0;
    if (Msg == WM_IME_CONTROL)
        return OnImeControl(wParam, lParam) != 0;
    if (Msg == WM_INPUTLANGCHANGE)
        return OnInputLangChange(wParam, lParam) != 0;
    if (Msg == WM_IME_COMPOSITIONFULL)
        return OnImeCompositionFull(wParam, lParam) != 0;
    if (Msg == WM_IME_COMPOSITION)
        return OnImeComposition(wParam, lParam) != 0;
    if (Msg == WM_IME_CHAR)
        return OnImeChar(wParam, lParam) != 0;
    return 0;
}
NW_HOOK_METHOD(0x100163c0, NCIME, WindowProc)

// 0x100178e0 (vslot 37) - the IME's default window stays hidden; C4: no input context of its own any more (the IME
// works on the game window's, see ImeGetContext), the fields that want a real keyboard (Mode 2 alphanumeric and 6
// password) keep the conversion alone and every other one starts in native mode with the IME open.
// Shared by the Korean and Thai IMEs (the stock slot is this function).
INT NCIME::OnCreate()
{
    NControl::OnCreate();
    HWND IMEWnd = ImmGetDefaultIMEWnd(GViewportHWnd);
    if (IMEWnd)
        ::ShowWindow(IMEWnd, SW_HIDE);
    if (Mode != 6 && Mode != 2)
    {
        Conversion |= IME_CMODE_NATIVE;
        OpenStatus = 1;
    }
    return 1;
}
NW_HOOK_METHOD(0x100178e0, NCIME, OnCreate)

// 0x10016520 (vslot 38) - NControl's OnDestroy, then 1 (shared by the Thai IME; the Korean one calls it)
INT NCIME::OnDestroy()
{
    NControl::OnDestroy();
    return 1;
}
NW_HOOK_METHOD(0x10016520, NCIME, OnDestroy)

// 0x10016530 (name ours) - the field wants plain input: Mode 2 (alphanumeric) or 6 (password). Called out of line by the
// Japanese, Taiwanese and Chinese OnImeNotify; the other IME methods repeat the test inline.
UBOOL NCIME::IsRestrictedMode()
{
    return Mode == 6 || Mode == 2;
}
NW_HOOK_METHOD(0x10016530, NCIME, IsRestrictedMode)

// 0x100165d0 (vslot 75) - C4: the IME works on the context of the game window. The restricted fields (Mode 2 and 6)
// start with the conversion cleared, every other one gets the open status and the conversion mode it kept. Shared by
// the Korean, Japanese and Thai IMEs.
void NCIME::Activate()
{
    bActive = 1;
    HIMC hIMC = ImeGetContext();
    if (Mode != 6 && Mode != 2)
    {
        ImmSetOpenStatus(hIMC, OpenStatus);
        ImmSetConversionStatus(hIMC, Conversion, Sentence);
        ImeReleaseContext(hIMC);
        return;
    }
    ImmSetConversionStatus(hIMC, 0, 0);
    ImeReleaseContext(hIMC);
}
NW_HOOK_METHOD(0x100165d0, NCIME, Activate)

// 0x10016640 (vslot 76) - the restricted fields give back the status they kept; for every other one the status of
// the game window's context is kept (and cleared, with the chat line open) before the IME is released
void NCIME::Deactivate()
{
    HIMC hIMC;
    if (Mode == 6 || Mode == 2)
    {
        hIMC = ImeGetContext();
        ImmSetConversionStatus(hIMC, Conversion, Sentence);
        ImmSetOpenStatus(hIMC, OpenStatus);
    }
    else
    {
        hIMC = ImeGetContext();
        ImmGetConversionStatus(hIMC, &Conversion, &Sentence);
        OpenStatus = ImmGetOpenStatus(hIMC);
        if (GIsEnterChatting)
        {
            ImmSetConversionStatus(hIMC, 0, 0);
            ImmSetOpenStatus(hIMC, 0);
        }
    }
    ImeReleaseContext(hIMC);
    bActive = 0;
    bNotComposing = 1;
}
NW_HOOK_METHOD(0x10016640, NCIME, Deactivate)

// 0x10001860 (vslot 78)
INT NCIME::OnImeStartComposition(WPARAM wParam, LPARAM lParam)
{
    bNotComposing = 0;
    return 1;
}
NW_HOOK_METHOD(0x10001860, NCIME, OnImeStartComposition)

// 0x10001830 (vslot 79)
INT NCIME::OnImeEndComposition(WPARAM wParam, LPARAM lParam)
{
    bNotComposing = 1;
    return 1;
}
NW_HOOK_METHOD(0x10001830, NCIME, OnImeEndComposition)

// 0x10016570 (vslot 80) - C3: no longer by the IME properties; the IME is asked to hide its own windows
// whenever the context is activated showing the composition (ISC_SHOWUICOMPOSITIONWINDOW, the sign bit) or
// the candidate window (ISC_SHOWUICANDIDATEWINDOW): the message goes again without any flag
INT NCIME::OnImeSetContext(WPARAM wParam, LPARAM lParam)
{
    if (wParam == 1)
    {
        if ((INT)lParam < 0 || (lParam & ISC_SHOWUICANDIDATEWINDOW))
            ImeIsUIMessage(WM_IME_SETCONTEXT, 1, 0);
    }
    return 1;
}
NW_HOOK_METHOD(0x10016570, NCIME, OnImeSetContext)

// 0x10016550 (vslot 82)
INT NCIME::OnInputLangChange(WPARAM wParam, LPARAM lParam)
{
    Property = ImeGetProperty(IGP_PROPERTY);
    return 1;
}
NW_HOOK_METHOD(0x10016550, NCIME, OnInputLangChange)

// 0x100165a0
TCHAR* NCIME::GetResultStr()
{
    return ResultStr;
}
NW_HOOK_METHOD(0x100165a0, NCIME, GetResultStr)

// 0x100165b0
TCHAR* NCIME::GetCompStr()
{
    return CompStr;
}
NW_HOOK_METHOD(0x100165b0, NCIME, GetCompStr)

// 0x100165c0
TCHAR* NCIME::GetReadingStr()
{
    return ReadingStr;
}
NW_HOOK_METHOD(0x100165c0, NCIME, GetReadingStr)

// 0x100166e0
UBOOL NCIME::IsNotComposing()
{
    return bNotComposing;
}
NW_HOOK_METHOD(0x100166e0, NCIME, IsNotComposing)

// 0x100166f0 - NT, or Windows 4.x from 98 on; asked once
UBOOL NCIME::IsUnicodeOS()
{
    if (!GIMEOSChecked)
    {
        OSVERSIONINFOW Info;
        Info.dwOSVersionInfoSize = sizeof(Info);
#pragma warning(suppress : 4996)  // deprecated in current SDKs; the stock code calls it
        GetVersionExW(&Info);
        if (Info.dwPlatformId == VER_PLATFORM_WIN32_NT || Info.dwMajorVersion > 4 ||
            (Info.dwMajorVersion == 4 && Info.dwMinorVersion > 0))
            GIMEUnicodeOS = 1;
        GIMEOSChecked = 1;
    }
    return GIMEUnicodeOS;
}
NW_HOOK_METHOD(0x100166f0, NCIME, IsUnicodeOS)

// 0x10016750 - CodePage becomes the default ANSI code page of the keyboard layout's language
void NCIME::UpdateCodePage()
{
    ANSICHAR Buf[12];
    if (GetLocaleInfoA(LOWORD(GetKeyboardLayout(0)), LOCALE_IDEFAULTANSICODEPAGE, Buf, 10))
        CodePage = NAtol(Buf);
}
NW_HOOK_METHOD(0x10016750, NCIME, UpdateCodePage)

// 0x10016b80 (static) - imm32.dll's four input context locks (the library is not freed)
void NCIME::LoadImmFunctions()
{
    HMODULE Imm = LoadLibraryA("imm32.dll");
    if (Imm)
    {
        GImmLockIMC = (NImmLockIMC)GetProcAddress(Imm, "ImmLockIMC");
        GImmUnlockIMC = (NImmUnlockIMC)GetProcAddress(Imm, "ImmUnlockIMC");
        GImmLockIMCC = (NImmLockIMCC)GetProcAddress(Imm, "ImmLockIMCC");
        GImmUnlockIMCC = (NImmUnlockIMCC)GetProcAddress(Imm, "ImmUnlockIMCC");
    }
}
NW_HOOK_FUNC(0x10016b80, NCIME::LoadImmFunctions)

NW_CHECK_VTABLE(NCIME, 0x101a47d8, 86)
