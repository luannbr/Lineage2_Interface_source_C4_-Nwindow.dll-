/*=============================================================================
    NCKoreanIME.cpp - IME of Korean and English clients. Stock: 0x1000e230 .. 0x1000e31f,
    0x1000e540 .. 0x1000e65c, 0x10010230 .. 0x1001111b, 0x10014640 .. 0x1001470f.
=============================================================================*/
#include "nwindow/NCKoreanIME.h"
#include "nwindow/NCChatWnd.h"
#include "nwindow/NCConsole.h"
#include "nwindow/NCEditBox.h"
#include "nwindow/NCIMEManager.h"
#include "nwindow/NConsoleWnd.h"

// 0x1001d3d0 - CandList and the clauses are left as allocated
NCKoreanIME::NCKoreanIME(INT InMode)
    : NCIME(0)
{
    Mode = InMode;
}
NW_HOOK_CTOR_ARGS(0x1001d3d0, NCKoreanIME, (INT A), (A))

// 0x1001d440
NW_HOOK_DTOR(0x1001d440, NCKoreanIME)


// 0x10016790 (vslot 25) - dispatches notify and composition again after NCIME
INT NCKoreanIME::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCIME::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == WM_IME_NOTIFY)
        return OnImeNotify(wParam, lParam) != 0;
    if (Msg == WM_IME_COMPOSITION)
        return OnImeComposition(wParam, lParam) != 0;
    return 0;
}
NW_HOOK_METHOD(0x10016790, NCKoreanIME, WindowProc)

// (OnCreate 0x100178e0 and Activate 0x100165d0 are NCIME's: the stock slots point there)

// 0x10016810 (vslot 38) - C4: no context of its own to destroy, NCIME's OnDestroy and 1
INT NCKoreanIME::OnDestroy()
{
    NCIME::OnDestroy();
    return 1;
}
NW_HOOK_METHOD(0x10016810, NCKoreanIME, OnDestroy)

// 0x100168a0 (vslot 76) - NCIME's, then the composition cancelled on the game window's context
void NCKoreanIME::Deactivate()
{
    NCIME::Deactivate();
    HIMC hIMC = ImeGetContext();
    ImmNotifyIME(hIMC, NI_COMPOSITIONSTR, CPS_CANCEL, 0);
    ImeReleaseContext(hIMC);
}
NW_HOOK_METHOD(0x100168a0, NCKoreanIME, Deactivate)

// 0x10016820 (vslot 77) - handled only while active: after a change of the conversion mode the restricted fields
// (Mode 2 and 6) get it cleared again; 0 for every notification
INT NCKoreanIME::OnImeNotify(WPARAM wParam, LPARAM lParam)
{
    if (bActive && wParam == IMN_SETCONVERSIONMODE)
    {
        HIMC hIMC = ImeGetContext();
        if (Mode == 6 || Mode == 2)
        {
            DWORD Conv, Sent;
            ImmGetConversionStatus(hIMC, &Conv, &Sent);
            if (Conv)
                ImmSetConversionStatus(hIMC, 0, 0);
        }
        ImeReleaseContext(hIMC);
    }
    return 0;
}
NW_HOOK_METHOD(0x10016820, NCKoreanIME, OnImeNotify)

// 0x10018160 (vslot 74) - copies every string lParam announces (GCS_*) and tells the parent.
// Each copy has its own guard, as in the stock code. When the IME must be asked in ANSI (see
// IsAnsiIME) the strings are converted from CodePage and the cursor position comes from the
// last conversion (stock behavior); clause and attribute lengths keep the stock units. The
// stock conversion buffer holds 0x800 characters although 0x1000 are allowed; ours is larger.
INT NCKoreanIME::OnImeComposition(WPARAM wParam, LPARAM lParam)
{
    guard(NCKoreanIME::OnImeComposition);
    TCHAR Clause[1024];
    NImeText Item;
    WCHAR Wide[0x1000];
    ANSICHAR Ansi[0x801];
    UBOOL bClauses = 0;
    INT Bytes;
    UINT Len = 0;
    ImeIsUIMessage(WM_IME_SETCONTEXT, 1, 0);
    ImeIsUIMessage(WM_IME_CONTROL, IMC_CLOSESTATUSWINDOW, 0);
    HIMC hIMC = ImeGetContext();

    if (lParam & GCS_RESULTSTR)
    {
        if (lParam & GCS_RESULTCLAUSE)
        {
            if (IsAnsiIME())
            {
                guard(NCKoreanIME::OnImeComposition::GCS_RESULTCLAUSE::NotUnicode);
                Bytes = ImmGetCompositionStringW(hIMC, GCS_RESULTCLAUSE, Ansi, 0x800);
                Ansi[Bytes] = 0;
                Len = MultiByteToWideChar(CodePage, 0, Ansi, -1, Wide, 0x1000);
                if (Len)
                    Len = Len * 2 - 2;
                ResultClauseLen = Len >> 1;
                appMemset(ResultClause, 0, sizeof(ResultClause));
                appMemcpy(ResultClause, Wide, ResultClauseLen);
                ResultClause[ResultClauseLen] = 0;
                unguard;
            }
            else
            {
                guard(NCKoreanIME::OnImeComposition::GCS_RESULTCLAUSE::Unicode);
                appMemset(ResultClause, 0, sizeof(ResultClause));
                ResultClauseLen =
                    ImmGetCompositionStringW(hIMC, GCS_RESULTCLAUSE, ResultClause, sizeof(ResultClause));
                unguard;
            }
        }
        else
        {
            appMemset(ResultClause, 0, sizeof(ResultClause));
            ResultClauseLen = 0;
        }
        if (IsAnsiIME())
        {
            guard(NCKoreanIME::OnImeComposition::GCS_RESULTSTR::NotUnicode);
            Bytes = ImmGetCompositionStringW(hIMC, GCS_RESULTSTR, Ansi, 0x800);
            Ansi[Bytes] = 0;
            Len = MultiByteToWideChar(CodePage, 0, Ansi, -1, Wide, 0x1000);
            if (Len)
                Len = Len * 2 - 2;
            ResultStrLen = Len >> 1;
            appMemset(ResultStr, 0, sizeof(ResultStr));
            ResultStr[0] = 0;
            appStrcpy(ResultStr, Wide);
            ResultStr[ResultStrLen] = 0;
            unguard;
        }
        else
        {
            guard(NCKoreanIME::OnImeComposition::GCS_RESULTSTR::Unicode);
            appMemset(ResultStr, 0, sizeof(ResultStr));
            ResultStr[0] = 0;
            ResultStrLen =
                (DWORD)ImmGetCompositionStringW(hIMC, GCS_RESULTSTR, ResultStr, sizeof(ResultStr)) >> 1;
            ResultStr[ResultStrLen] = 0;
            unguard;
        }
        Clauses.Empty();
        SendToParent(NIM_RESULT, 0, 0);
        SendToParent(NIM_ATTRIBUTE, 1, 1);
    }
    else
    {
        ResultStrLen = 0;
        appMemset(ResultStr, 0, sizeof(ResultStr));
        ResultStr[0] = 0;
        appMemset(ResultClause, 0, sizeof(ResultClause));
        ResultClauseLen = 0;
    }

    if (lParam & GCS_COMPSTR)
    {
        if (IsAnsiIME())
        {
            guard(NCKoreanIME::OnImeComposition::GCS_COMPSTR::NotUnicode);
            Bytes = ImmGetCompositionStringW(hIMC, GCS_COMPSTR, Ansi, 0x800);
            Ansi[Bytes] = 0;
            Len = MultiByteToWideChar(CodePage, 0, Ansi, -1, Wide, 0x1000);
            if (Len)
                Len = Len * 2 - 2;
            CompStrLen = Len >> 1;
            appMemset(CompStr, 0, sizeof(CompStr));
            CompStr[0] = 0;
            appStrcpy(CompStr, Wide);
            CompStr[CompStrLen] = 0;
            unguard;
        }
        else
        {
            guard(NCKoreanIME::OnImeComposition::GCS_COMPSTR::Unicode);
            appMemset(CompStr, 0, sizeof(CompStr));
            CompStr[0] = 0;
            CompStrLen = (DWORD)ImmGetCompositionStringW(hIMC, GCS_COMPSTR, CompStr, sizeof(CompStr)) >> 1;
            CompStr[CompStrLen] = 0;
            unguard;
        }
        // the character being composed (read before the string when it is empty, as in the
        // stock code)
        SendToParent(NIM_COMPOSITION, CompStr[CompStrLen - 1], 0);
    }
    else
    {
        CompStrLen = 0;
        appMemset(CompStr, 0, sizeof(CompStr));
        CompStr[0] = 0;
        SendToParent(NIM_COMPOSITION, 0, 0);
    }

    if (lParam & GCS_COMPATTR)
    {
        if (IsAnsiIME())
        {
            guard(NCKoreanIME::OnImeComposition::GCS_COMPATTR::NotUnicode);
            Bytes = ImmGetCompositionStringW(hIMC, GCS_COMPATTR, Ansi, 0x800);
            Ansi[Bytes] = 0;
            Len = MultiByteToWideChar(CodePage, 0, Ansi, -1, Wide, 0x1000);
            if (Len)
                Len = Len * 2 - 2;
            CompAttrLen = Len >> 1;
            appMemset(CompAttr, 0, sizeof(CompAttr));
            appMemcpy(CompAttr, Wide, CompAttrLen);
            CompAttr[CompAttrLen] = 0;
            unguard;
        }
        else
        {
            guard(NCKoreanIME::OnImeComposition::GCS_COMPATTR::Unicode);
            appMemset(CompAttr, 0, sizeof(CompAttr));
            CompAttrLen = ImmGetCompositionStringW(hIMC, GCS_COMPATTR, CompAttr, sizeof(CompAttr));
            unguard;
        }
        if (!CompAttr[0])
        {
            SendToParent(NIM_ATTRIBUTE, 0, 0);
            Clauses.Empty();
            bClauses = 0;
        }
        else
            bClauses = 1;
    }
    else
    {
        appMemset(CompAttr, 0, sizeof(CompAttr));
        CompAttrLen = 0;
    }

    if (lParam & GCS_COMPCLAUSE)
    {
        if (IsAnsiIME())
        {
            guard(NCKoreanIME::OnImeComposition::GCS_COMPCLAUSE::NotUnicode);
            Bytes = ImmGetCompositionStringW(hIMC, GCS_COMPCLAUSE, Ansi, 0x800);
            Ansi[Bytes] = 0;
            Len = MultiByteToWideChar(CodePage, 0, Ansi, -1, Wide, 0x1000);
            if (Len)
                Len = Len * 2 - 2;
            CompClauseLen = Len >> 1;
            appMemset(CompClause, 0, sizeof(CompClause));
            appMemcpy(CompClause, Wide, CompClauseLen);
            CompClause[CompClauseLen] = 0;
            unguard;
        }
        else
        {
            guard(NCKoreanIME::OnImeComposition::GCS_COMPCLAUSE::Unicode);
            appMemset(CompClause, 0, sizeof(CompClause));
            CompClauseLen =
                (DWORD)ImmGetCompositionStringW(hIMC, GCS_COMPCLAUSE, CompClause, sizeof(CompClause)) >> 2;
            unguard;
        }
    }
    else
    {
        appMemset(CompClause, 0, sizeof(CompClause));
        CompClauseLen = 0;
    }

    if (lParam & GCS_CURSORPOS)
    {
        if (IsAnsiIME())
            CursorPos = Len >> 1;
        else
            CursorPos = ImmGetCompositionStringW(hIMC, GCS_CURSORPOS, NULL, 0);
        SendToParent(NIM_CURSORPOS, CursorPos, 0);
    }

    if (lParam & GCS_COMPREADSTR)
    {
        if (IsAnsiIME())
        {
            guard(NCKoreanIME::OnImeComposition::GCS_COMPREADSTR::NotUnicode);
            Bytes = ImmGetCompositionStringW(hIMC, GCS_COMPREADSTR, Ansi, 0x800);
            Ansi[Bytes] = 0;
            Len = MultiByteToWideChar(CodePage, 0, Ansi, -1, Wide, 0x1000);
            if (Len)
                Len = Len * 2 - 2;
            CompReadStrLen = Len >> 1;
            appMemset(CompReadStr, 0, sizeof(CompReadStr));
            CompReadStr[0] = 0;
            appStrcpy(CompReadStr, Wide);
            CompReadStr[CompReadStrLen] = 0;
            unguard;
        }
        else
        {
            guard(NCKoreanIME::OnImeComposition::GCS_COMPREADSTR::Unicode);
            appMemset(CompReadStr, 0, sizeof(CompReadStr));
            CompReadStr[0] = 0;
            Len = ImmGetCompositionStringW(hIMC, GCS_COMPREADSTR, CompReadStr, sizeof(CompReadStr));
            CompReadStrLen = Len >> 1;
            CompReadStr[Len >> 1] = 0;
            unguard;
        }
    }
    else
    {
        CompReadStrLen = 0;
        appMemset(CompReadStr, 0, sizeof(CompReadStr));
        CompReadStr[0] = 0;
    }

    if (lParam & GCS_COMPREADCLAUSE)
    {
        if (IsAnsiIME())
        {
            guard(NCKoreanIME::OnImeComposition::GCS_COMPREADCLAUSE::NotUnicode);
            Bytes = ImmGetCompositionStringW(hIMC, GCS_COMPREADCLAUSE, Ansi, 0x800);
            Ansi[Bytes] = 0;
            Len = MultiByteToWideChar(CodePage, 0, Ansi, -1, Wide, 0x1000);
            if (Len)
                Len = Len * 2 - 2;
            CompReadClauseLen = Len >> 1;
            appMemset(CompReadClause, 0, sizeof(CompReadClause));
            appMemcpy(CompReadClause, Wide, CompReadClauseLen);
            CompReadClause[CompReadClauseLen] = 0;
            unguard;
        }
        else
        {
            guard(NCKoreanIME::OnImeComposition::GCS_COMPREADCLAUSE::Unicode);
            appMemset(CompReadClause, 0, sizeof(CompReadClause));
            CompReadClauseLen = ImmGetCompositionStringW(hIMC, GCS_COMPREADCLAUSE, CompReadClause,
                                                         sizeof(CompReadClause));
            unguard;
        }
    }
    else
    {
        CompReadClauseLen = 0;
        appMemset(CompReadClause, 0, sizeof(CompReadClause));
    }

    if (lParam & GCS_COMPREADATTR)
    {
        if (IsAnsiIME())
        {
            guard(NCKoreanIME::OnImeComposition::GCS_COMPREADATTR::NotUnicode);
            Bytes = ImmGetCompositionStringW(hIMC, GCS_COMPREADATTR, Ansi, 0x800);
            Ansi[Bytes] = 0;
            Len = MultiByteToWideChar(CodePage, 0, Ansi, -1, Wide, 0x1000);
            if (Len)
                Len = Len * 2 - 2;
            CompReadAttrLen = Len >> 1;
            appMemset(CompReadAttr, 0, sizeof(CompReadAttr));
            appMemcpy(CompReadAttr, Wide, CompReadAttrLen);
            CompReadAttr[CompReadAttrLen] = 0;
            unguard;
        }
        else
        {
            guard(NCKoreanIME::OnImeComposition::GCS_COMPREADATTR::Unicode);
            appMemset(CompReadAttr, 0, sizeof(CompReadAttr));
            CompReadAttrLen =
                ImmGetCompositionStringW(hIMC, GCS_COMPREADATTR, CompReadAttr, sizeof(CompReadAttr));
            unguard;
        }
    }
    else
    {
        CompReadAttrLen = 0;
        appMemset(CompReadAttr, 0, sizeof(CompReadAttr));
    }

    // one entry per clause of the composition; without clause data the parent is told
    // whether there is a composition at all
    if (bClauses)
    {
        Clauses.Empty();
        if (CompStrLen == 0)
            SendToParent(NIM_COMPOSITION, 0, 0);
        else if (CompClauseLen == 0 || CompAttrLen == 0)
            SendToParent(NIM_COMPOSITION, 1, 0);
        else
        {
            CompClause[255] = 0;
            for (INT i = 1; CompClause[i] && CompClause[i - 1] < (DWORD)CompStrLen; i++)
            {
                INT Count = CompClause[i] - CompClause[i - 1];
                appMemcpy(Clause, &CompStr[CompClause[i - 1]], Count * 2);
                Clause[Count] = 0;
                Item.Text[0] = 0;
                Item.Flag = 0;
                if (CompAttr[CompClause[i - 1]] & 1)
                    Item.Flag = 1;
                if (Clause[0])
                    wcscpy(Item.Text, Clause);
                Clauses(Clauses.Add()) = Item;
            }
        }
    }
    ImeReleaseContext(hIMC);
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10018160, NCKoreanIME, OnImeComposition)

NW_CHECK_VTABLE(NCKoreanIME, 0x101a4a88, 86)
