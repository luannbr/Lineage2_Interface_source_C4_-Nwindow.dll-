/*=============================================================================
    NCJapaneseIME.cpp - IME of the Japanese client. Stock: 0x1000e660 .. 0x1000e6ed,
    0x1000ee70 .. 0x1000ef7a, 0x1000f760 .. 0x1000f88c, 0x1000f960 .. 0x1000fac1,
    0x10011120 .. 0x1001220f, 0x10012210 .. 0x10012309, 0x10014710 .. 0x1001485f (the
    window procedure, OnDestroy, ShowWindow and the candidate list helpers are shared with the
    Taiwanese and Chinese IMEs, hooked here).
=============================================================================*/
#include "nwindow/NCJapaneseIME.h"
#include "nwindow/NCChatWnd.h"
#include "nwindow/NCConsole.h"
#include "nwindow/NCEditBox.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NCIMEManager.h"
#include "nwindow/NConsoleWnd.h"

// a child window goes (repeated inline in the stock OnDestroys)
template <class T> static FORCEINLINE void DestroyChild(NCWnd* Parent, T*& Child)
{
    if (Child)
    {
        Parent->RemoveChildWindow(Child->Entry);
        delete Child;
        Child = NULL;
    }
}

// 0x1001d4a0 - CandList and the candidate box are left as allocated
NCJapaneseIME::NCJapaneseIME(INT InMode)
    : NCIME(0)
{
    Mode = InMode;
}
NW_HOOK_CTOR_ARGS(0x1001d4a0, NCJapaneseIME, (INT A), (A))

// 0x1001d510
NW_HOOK_DTOR(0x1001d510, NCJapaneseIME)


// 0x100168d0 (vslot 25) - also the Chinese IME's: NCIME's messages again after NCIME, and the
// caret the edit box sends back
INT NCJapaneseIME::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCIME::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == WM_IME_NOTIFY)
        return OnImeNotify(wParam, lParam) != 0;
    if (Msg == WM_IME_COMPOSITION)
        return OnImeComposition(wParam, lParam) != 0;
    if (Msg == WM_IME_ENDCOMPOSITION)
        return OnImeEndComposition(wParam, lParam) != 0;
    if (Msg == WM_IME_SETCONTEXT)
        return OnImeSetContext(wParam, lParam) != 0;
    if (Msg == WM_INPUTLANGCHANGE)
        return OnInputLangChange(wParam, lParam) != 0;
    if (Msg == NIM_CARET)
        return OnCaret((INT)wParam, (INT)lParam) != 0;
    return 0;
}
NW_HOOK_METHOD(0x100168d0, NCJapaneseIME, WindowProc)

// 0x100169d0 - the keyboard layout is Japanese (this is not used)
UBOOL NCJapaneseIME::IsJapaneseLayout()
{
    return PRIMARYLANGID(LOWORD(GetKeyboardLayout(0))) == LANG_JAPANESE;
}
NW_HOOK_METHOD(0x100169d0, NCJapaneseIME, IsJapaneseLayout)

// 0x10017020 (vslot 48) - also the Taiwanese IME's
void NCJapaneseIME::ShowWindow()
{
    NCWnd::ShowWindow();
    CandidateBox->HideWindow();
}
NW_HOOK_METHOD(0x10017020, NCJapaneseIME, ShowWindow)

// 0x100169f0 - also the Taiwanese and Chinese IMEs'
void NCJapaneseIME::FreeCandList()
{
    if (CandList)
    {
        CANDIDATELIST* List = CandList;
        CandList = NULL;
        appFree(List);
    }
}
NW_HOOK_METHOD(0x100169f0, NCJapaneseIME, FreeCandList)

// 0x10016a20 (vslot 79)
INT NCJapaneseIME::OnImeEndComposition(WPARAM wParam, LPARAM lParam)
{
    bNotComposing = 1;
    SendToParent(NIM_ENDCOMPOSITION, 0, 0);
    return 1;
}
NW_HOOK_METHOD(0x10016a20, NCJapaneseIME, OnImeEndComposition)

// 0x10019040 (vslot 37) - C4: NCIME's OnCreate, then the Japanese defaults (native and full shape conversion, roman
// sentence mode, IME closed) and the candidate box, hidden
INT NCJapaneseIME::OnCreate()
{
    NCIME::OnCreate();
    Conversion = IME_CMODE_NATIVE | IME_CMODE_FULLSHAPE;
    Sentence = IME_SMODE_PHRASEPREDICT;
    OpenStatus = 0;
    CandidateBox = new NCCandidateBox(9, 15);
    CandidateBox->SetPosition(0, 0, 100, 135);
    CreateChild(CandidateBox, NWS_VISIBLE);
    CandidateBox->HideWindow();
    return 1;
}
NW_HOOK_METHOD(0x10019040, NCJapaneseIME, OnCreate)

// 0x10017930 (vslot 86) - the candidate box shown at the caret (moved only when it was hidden)
INT NCJapaneseIME::OnCaret(INT X, INT Y)
{
    if (!CandidateBox->IsShowWindow())
    {
        INT Pos[2];
        INT* P = CandidateBox->GetLocalPos(Pos);
        CandidateBox->MoveWindow(-P[0], -P[1]);
        CandidateBox->MoveWindow(X, Y);
    }
    CandidateBox->SetMode(0);
    CandidateBox->ShowWindow();
    return 1;
}
NW_HOOK_METHOD(0x10017930, NCJapaneseIME, OnCaret)

// 0x100179c0 - also the Taiwanese IME's: the page of CandList (the selection among them, 9 a
// page) into the candidate box, sized to its widest candidate, then the edit box is asked for
// its caret
void NCJapaneseIME::FillCandidateBox()
{
    SIZE Size;
    SIZE ItemSize;
    CandidateBox->Clear();
    DWORD i = CandList->dwPageStart;
    TCHAR* Text = (TCHAR*)((BYTE*)CandList + CandList->dwOffset[i]);
    CandidateBox->AddItem(Text, (INT)0xFFDCDCDC);
    if (i == CandList->dwSelection)
        CandidateBox->Selected = i % 9;
    NCGDevice::GetTextExtent(&Size, Text, 0, 0, 0, 0, 0, L2FONT_Normal);
    for (i = CandList->dwPageStart + 1; i < CandList->dwCount; i++)
    {
        if (i >= CandList->dwPageSize + CandList->dwPageStart)
            break;
        Text = (TCHAR*)((BYTE*)CandList + CandList->dwOffset[i]);
        CandidateBox->AddItem(Text, (INT)0xFFDCDCDC);
        NCGDevice::GetTextExtent(&ItemSize, Text, 0, 0, 0, 0, 0, L2FONT_Normal);
        if (i == CandList->dwSelection)
            CandidateBox->Selected = i % 9;
        if (ItemSize.cx > Size.cx)
            Size.cx = ItemSize.cx;
    }
    CandidateBox->SetScroll(CandList->dwCount, CandList->dwSelection);
    CandidateBox->PageSize = CandList->dwPageSize;
    CandidateBox->CallSetSize(Size.cx + 20, 135);
    SendToParent(NIM_GETCARET, (WPARAM)Entry, 0);
}
NW_HOOK_METHOD(0x100179c0, NCJapaneseIME, FillCandidateBox)

// (Activate 0x100165d0 is NCIME's: the stock slot points there)

// 0x10019120 (vslot 38) - also the Taiwanese IME's (NCWnd::OnDestroy is not called); C4: no context to destroy
INT NCJapaneseIME::OnDestroy()
{
    Clauses.Empty();
    if (CandList)
        appFree(CandList);
    CandList = NULL;
    DestroyChild(this, CandidateBox);
    return 1;
}
NW_HOOK_METHOD(0x10019120, NCJapaneseIME, OnDestroy)

// 0x10019200 - the IME's candidate list copied, the IME told to start its page where the selection's page of 9 starts,
// the box filled; the context is released in any case
void NCJapaneseIME::ReadCandList()
{
    HIMC hIMC = ImeGetContext();
    DWORD Size = ImmGetCandidateListW(hIMC, 0, NULL, 0);
    FreeCandList();
    if (Size)
    {
        CandList = (CANDIDATELIST*)appMalloc(Size, TEXT("new"));
        CandList->dwSize = Size;
        ImmGetCandidateListW(hIMC, 0, CandList, Size);
        DWORD Step = CandList->dwPageSize == 9 || !CandList->dwPageSize ? CandList->dwPageSize : 9;
        DWORD PageStart = 0;
        if (CandList->dwSelection > 0 && CandList->dwPageSize && Step <= CandList->dwSelection)
        {
            do
                PageStart += Step;
            while (Step + PageStart <= CandList->dwSelection);
        }
        if (CandList->dwPageStart != PageStart)
            ImmNotifyIME(hIMC, NI_SETCANDIDATE_PAGESTART, 0, PageStart);
        FillCandidateBox();
    }
    ImeReleaseContext(hIMC);
}
NW_HOOK_METHOD(0x10019200, NCJapaneseIME, ReadCandList)

// 0x100192b0 (vslot 74) - NCKoreanIME::OnImeComposition's copy (see there), with its own guard
// names, nothing read unless the keyboard layout is Japanese, and the cursor position: from
// the IME only while a character still has attribute 0 (being typed), else the end of the
// composition. The stock code's guards around the cursor position compiled to nothing.
INT NCJapaneseIME::OnImeComposition(WPARAM wParam, LPARAM lParam)
{
    guard(NCJapaneseIME::OnImeComposition);
    TCHAR Clause[1024];
    NImeText Item;
    WCHAR Wide[0x1000];
    ANSICHAR Ansi[0x801];
    UBOOL bClauses = 0;
    INT Bytes;
    UINT Len = 0;
    ImeIsUIMessage(WM_IME_SETCONTEXT, 1, 0);
    ImeIsUIMessage(WM_IME_CONTROL, IMC_CLOSESTATUSWINDOW, 0);
    // the CJK clients with another layout: nothing (and the context is not released)
    if ((GLanguageType == 2 || GLanguageType == 3 || GLanguageType == 4) && !IsJapaneseLayout())
        return 1;
    HIMC hIMC = ImeGetContext();

    if (lParam & GCS_RESULTSTR)
    {
        if (lParam & GCS_RESULTCLAUSE)
        {
            if (IsAnsiIME())
            {
                guard(NCJapaneseIME::OnImeComposition::GCS_RESULTCLAUSE::NotUnicode);
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
                guard(NCJapaneseIME::OnImeComposition::GCS_RESULTCLAUSE::Unicode);
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
            guard(NCJapaneseIME::OnImeComposition::GCS_RESULTSTR::NotUnicode);
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
            guard(NCJapaneseIME::OnImeComposition::GCS_RESULTSTR::Unicode);
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
            guard(NCJapaneseIME::OnImeComposition::GCS_COMPSTR::NotUnicode);
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
            guard(NCJapaneseIME::OnImeComposition::GCS_COMPSTR::Unicode);
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
            guard(NCJapaneseIME::OnImeComposition::GCS_COMPATTR::NotUnicode);
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
            guard(NCJapaneseIME::OnImeComposition::GCS_COMPATTR::Unicode);
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
            guard(NCJapaneseIME::OnImeComposition::GCS_COMPCLAUSE::NotUnicode);
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
            guard(NCJapaneseIME::OnImeComposition::GCS_COMPCLAUSE::Unicode);
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
        UBOOL bTyping = 0;
        if (IsAnsiIME())
        {
            for (INT i = 0; i < CompAttrLen; i++)
                if (CompAttr[i] == ATTR_INPUT)
                    bTyping = 1;
            if (bTyping)
                CursorPos = Len >> 1;
            else
                CursorPos = CompStrLen;
        }
        else
        {
            for (INT i = 0; i < CompAttrLen; i++)
                if (CompAttr[i] == ATTR_INPUT)
                    bTyping = 1;
            if (bTyping)
                CursorPos = ImmGetCompositionStringW(hIMC, GCS_CURSORPOS, NULL, 0);
            else
                CursorPos = CompStrLen;
        }
        SendToParent(NIM_CURSORPOS, CursorPos, 0);
    }

    if (lParam & GCS_COMPREADSTR)
    {
        if (IsAnsiIME())
        {
            guard(NCJapaneseIME::OnImeComposition::GCS_COMPREADSTR::NotUnicode);
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
            guard(NCJapaneseIME::OnImeComposition::GCS_COMPREADSTR::Unicode);
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
            guard(NCJapaneseIME::OnImeComposition::GCS_COMPREADCLAUSE::NotUnicode);
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
            guard(NCJapaneseIME::OnImeComposition::GCS_COMPREADCLAUSE::Unicode);
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
            guard(NCJapaneseIME::OnImeComposition::GCS_COMPREADATTR::NotUnicode);
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
            guard(NCJapaneseIME::OnImeComposition::GCS_COMPREADATTR::Unicode);
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
NW_HOOK_METHOD(0x100192b0, NCJapaneseIME, OnImeComposition)

// 0x1001a230 (vslot 76) - NCIME's, then the composition cancelled and the candidate list closed on the game window's
// context; the candidates go and the box is hidden
void NCJapaneseIME::Deactivate()
{
    NCIME::Deactivate();
    HIMC hIMC = ImeGetContext();
    ImmNotifyIME(hIMC, NI_COMPOSITIONSTR, CPS_CANCEL, 0);
    ImmNotifyIME(hIMC, NI_CLOSECANDIDATE, 0, 0);
    ImeReleaseContext(hIMC);
    FreeCandList();
    Clauses.Empty();
    if (CandidateBox->IsShowWindow())
        CandidateBox->HideWindow();
}
NW_HOOK_METHOD(0x1001a230, NCJapaneseIME, Deactivate)

// 0x1001c880 (vslot 77) - handled only while active. A change of the conversion mode in a restricted field (Mode 2
// and 6) clears it again (0); the candidate list is read when it opens or changes and closed with its box (1)
INT NCJapaneseIME::OnImeNotify(WPARAM wParam, LPARAM lParam)
{
    if (!bActive)
        return 0;
    switch (wParam)
    {
    case IMN_SETCONVERSIONMODE:
    {
        HIMC hIMC = ImeGetContext();
        if (IsRestrictedMode())
        {
            DWORD Conv, Sent;
            ImmGetConversionStatus(hIMC, &Conv, &Sent);
            if (Conv)
                ImmSetConversionStatus(hIMC, 0, 0);
        }
        ImeReleaseContext(hIMC);
        return 0;
    }
    case IMN_OPENCANDIDATE:
    case IMN_CHANGECANDIDATE:
        ReadCandList();
        return 1;
    case IMN_CLOSECANDIDATE:
        if (lParam == 1)
        {
            FreeCandList();
            CandidateBox->HideWindow();
        }
        return 1;
    }
    return 0;
}
NW_HOOK_METHOD(0x1001c880, NCJapaneseIME, OnImeNotify)

NW_CHECK_VTABLE(NCJapaneseIME, 0x101a4be0, 87)
