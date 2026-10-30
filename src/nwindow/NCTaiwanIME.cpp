/*=============================================================================
    NCTaiwanIME.cpp - IME of the Taiwanese client. Stock: 0x1000e6f0 .. 0x1000e7d9,
    0x1000e7e0 .. 0x1000e832, 0x1000e890 .. 0x1000ed5a (up to OnImeSetContext's end),
    0x1000ed90 .. 0x1000ee6a, 0x1000f890 .. 0x1000f953, 0x1000fad0 .. 0x1000fbea,
    0x10012310 .. 0x100131ff, 0x10013210 .. 0x100132fe, 0x10014860 .. 0x10014b57 (OnDestroy,
    ShowWindow and the candidate box filling are the Japanese IME's bodies).
=============================================================================*/
#include "nwindow/NCTaiwanIME.h"
#include "nwindow/NCChatWnd.h"
#include "nwindow/NCConsole.h"
#include "nwindow/NCEditBox.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NCIMEManager.h"
#include "nwindow/NConsoleWnd.h"

#include <stdlib.h>

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

// the IME versions GetImeId knows (names ours)
enum
{
    IMEID_CHT_VER42 = 0x40002,         // New Phonetic / ChangJie, IME 98
    IMEID_CHT_VER43 = 0x40003,         // IME 98a
    IMEID_CHT_VER44 = 0x40004,         // New ChangJie, IME 98b
    IMEID_CHT_VER50 = 0x50000,
    IMEID_CHT_VER51 = 0x50001,         // IME 2002
    IMEID_CHT_VER52 = 0x50002,
    IMEID_CHT_VER60 = 0x60000,
};

// 0x1001d570 - CandList and the candidate box are left as allocated
NCTaiwanIME::NCTaiwanIME(INT InMode)
    : NCIME(0)
{
    Mode = InMode;
}
NW_HOOK_CTOR_ARGS(0x1001d570, NCTaiwanIME, (INT A), (A))

// 0x1001d5e0
NW_HOOK_DTOR(0x1001d5e0, NCTaiwanIME)


// 0x10017550 - the class's operator new (NW_DECLARE_CLASS), out of line for the manager's
// OnCreate
static void* NewTaiwanIME(size_t Size, UObject* Outer, FName Name, DWORD SetFlags)
{
    return NCTaiwanIME::operator new(Size, Outer, Name, SetFlags);
}
NW_HOOK_FUNC(0x10017550, NewTaiwanIME)

// 0x10016a40 (vslot 25) - as the Japanese IME's, without the end of the composition
INT NCTaiwanIME::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCIME::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == WM_IME_NOTIFY)
        return OnImeNotify(wParam, lParam) != 0;
    if (Msg == WM_IME_COMPOSITION)
        return OnImeComposition(wParam, lParam) != 0;
    if (Msg == WM_INPUTLANGCHANGE)
        return OnInputLangChange(wParam, lParam) != 0;
    if (Msg == WM_IME_SETCONTEXT)
        return OnImeSetContext(wParam, lParam) != 0;
    if (Msg == NIM_CARET)
        return OnCaret((INT)wParam, (INT)lParam) != 0;
    return 0;
}
NW_HOOK_METHOD(0x10016a40, NCTaiwanIME, WindowProc)

// 0x10016b20 - GTaiwanGetReadingString from the current IME's file, NULL without it (the
// library is not freed)
void NCTaiwanIME::LoadReadingString()
{
    ANSICHAR FileName[MAX_PATH];
    HKL Layout = GetKeyboardLayout(0);
    GTaiwanGetReadingString = NULL;
    if (ImmGetIMEFileNameA(Layout, FileName, MAX_PATH) > 0)
    {
        HMODULE IME = LoadLibraryA(FileName);
        if (IME)
            GTaiwanGetReadingString = (NGetReadingString)GetProcAddress(IME, "GetReadingString");
    }
}
NW_HOOK_FUNC(0x10016b20, NCTaiwanIME::LoadReadingString)

// 0x10016bd0 - the version of the old Microsoft Traditional Chinese IMEs, which keep the reading string in their private data:
// their layouts (E008 and E009 with the language 0404), their file (unless the IME has GetReadingString, one of TINTLGNT,
// CINTLGNT or MSTCIPHA), a known version for a Chinese layout (any version with GetReadingString); 0 otherwise. As the Chinese
// IME's (NCChineseIME::GetImeId): the version buffer comes from the engine's allocator (appMalloc, no NULL test) and is freed by
// the out-of-line operator delete when its version is returned, by appFree when not
DWORD NCTaiwanIME::GetImeId()
{
    ANSICHAR FileName[MAX_PATH];
    HKL Layout = GetKeyboardLayout(0);
    DWORD Lang = (DWORD)(UINT_PTR)Layout & 0x3FF;
    if (((UINT_PTR)Layout != 0xE0080404 && (UINT_PTR)Layout != 0xE0090404) || !ImmGetIMEFileNameA(Layout, FileName, MAX_PATH - 1))
        return 0;
    if (!GTaiwanGetReadingString && lstrcmpiA(FileName, "TINTLGNT.IME") && lstrcmpiA(FileName, "CINTLGNT.IME")
        && lstrcmpiA(FileName, "MSTCIPHA.IME"))
        return 0;
    DWORD Handle;
    DWORD Size = GetFileVersionInfoSizeA(FileName, &Handle);
    if (!Size)
        return 0;
    void* Buffer = appMalloc(Size, TEXT("new"));
    if (GetFileVersionInfoA(FileName, Handle, Size, Buffer))
    {
        VS_FIXEDFILEINFO* Info;
        UINT InfoSize;
        if (VerQueryValueA(Buffer, "\\", (LPVOID*)&Info, &InfoSize))
        {
            DWORD Version = Info->dwFileVersionMS & 0xFF00FF;
            if (GTaiwanGetReadingString
                || (Lang == LANG_CHINESE
                    && (Version == IMEID_CHT_VER42 || Version == IMEID_CHT_VER43 || Version == IMEID_CHT_VER44
                        || Version == IMEID_CHT_VER50 || Version == IMEID_CHT_VER51 || Version == IMEID_CHT_VER52
                        || Version == IMEID_CHT_VER60)))
            {
                appFree(Buffer);   // the stock calls its out-of-line delete (0x10016880, StockOperatorDelete: appFree, no NULL test)
                return Version;
            }
        }
    }
    appFree(Buffer);
    return 0;
}
NW_HOOK_FUNC(0x10016bd0, NCTaiwanIME::GetImeId)

// 0x10016d40 - the reading string of the IME's own window, read through its GetReadingString export, or for the old IMEs
// without it from their private data: 4.2 .. 4.4 in Unicode, 5.0 in ANSI (the lengths are in bytes there), 5.1 and 5.2 in
// Unicode; the version comes from GetImeId. The string goes to ReadingStr (NIM_READING) with its length and error index. As
// the Chinese IME's (NCChineseIME::ReadReadingString; the Taiwanese IME does not clamp the error index): the viewport's input
// context (ImeGetContext), released before the string is sent; the IME's reading string comes in a buffer of the game's
// allocator (twice the length it answered), freed at the end, and a failed allocation is not tested
void NCTaiwanIME::ReadReadingString()
{
    DWORD Id = GetImeId();
    if (!Id)
        return;
    WCHAR Local[262];
    INT Err = 0;
    bool bUnicode = false;
    UINT Len = 0;
    WCHAR* Str = NULL;
    WCHAR* Allocated = NULL;
    HIMC hIMC = ImeGetContext();
    if (GTaiwanGetReadingString)
    {
        BOOL bVertical;
        UINT MaxUiLen;
        Len = GTaiwanGetReadingString(hIMC, 0, NULL, &Err, &bVertical, &MaxUiLen);
        if (Len)
        {
            Str = Allocated = (WCHAR*)appMalloc(Len * 2, TEXT("new"));
            Len = GTaiwanGetReadingString(hIMC, Len, Str, &Err, &bVertical, &MaxUiLen);
        }
        bUnicode = true;
    }
    else
    {
        BYTE* IC = GImmLockIMC(hIMC);
        BYTE* p;
        if (Id > IMEID_CHT_VER50)
        {
            if (Id >= IMEID_CHT_VER51 && Id <= IMEID_CHT_VER52)
            {
                p = *(BYTE**)((BYTE*)GImmLockIMCC(IMCPrivate(IC)) + 4);
                if (p)
                {
                    p = *(BYTE**)(p + 0x18);
                    if (p)
                    {
                        Len = *(DWORD*)(p + 0x60);
                        Err = *(DWORD*)(p + 0x64);
                        Str = (WCHAR*)(p + 0x40);
                        bUnicode = true;
                    }
                }
            }
        }
        else if (Id == IMEID_CHT_VER50)
        {
            p = *(BYTE**)((BYTE*)GImmLockIMCC(IMCPrivate(IC)) + 0xC);
            if (p)
            {
                p = *(BYTE**)(p + 0x20);
                if (p)
                {
                    Len = *(DWORD*)(p + 0x50) >> 1;
                    Err = *(DWORD*)(p + 0x54) >> 1;
                    MultiByteToWideChar(CodePage, 0, (ANSICHAR*)(p + 0x40), -1, Local, Len);
                }
            }
        }
        else if (Id >= IMEID_CHT_VER42 && Id <= IMEID_CHT_VER44)
        {
            p = *(BYTE**)((BYTE*)GImmLockIMCC(IMCPrivate(IC)) + 0x18);
            if (p)
            {
                Len = *(DWORD*)(p + 0x9C);
                Err = *(DWORD*)(p + 0xA0);
                Str = (WCHAR*)(p + 0x38);
                bUnicode = true;
            }
        }
        GImmUnlockIMCC(IMCPrivate(IC));
        GImmUnlockIMC(hIMC);
    }
    ImeReleaseContext(hIMC);
    if (Len > 0)
    {
        ReadingStr[Len] = 0;
        for (INT i = 0; i < (INT)Len; i++)
            ReadingStr[i] = bUnicode ? Str[i] : Local[i];
        SendToParent(NIM_READING, Len, Err);
    }
    if (Allocated)
        appFree(Allocated);
}
NW_HOOK_METHOD(0x10016d40, NCTaiwanIME, ReadReadingString)

// 0x10017040 (vslot 82) - the keyboard layout changed: the new IME's reading string API, imm32's input context locks and the
// code page (the other IMEs' slot is a shared stub that only answers 1)
INT NCTaiwanIME::OnInputLangChange(WPARAM wParam, LPARAM lParam)
{
    LoadReadingString();
    LoadImmFunctions();
    UpdateCodePage();
    return 1;
}
NW_HOOK_METHOD(0x10017040, NCTaiwanIME, OnInputLangChange)

// 0x10016f80 - a Traditional Chinese layout with a Taiwanese IME: layouts E002, E006, E008 and
// E020, or the files TINTLGNT, CINTLGNT and MSTCIPHA (this is not used)
UBOOL NCTaiwanIME::IsTaiwanIME()
{
    ANSICHAR FileName[260];
    HKL Layout = GetKeyboardLayout(0);
    ImmGetIMEFileNameA(Layout, FileName, 259);
    if (((UINT_PTR)Layout & 0x3FF) != LANG_CHINESE)
        return 0;
    WORD Device = HIWORD((UINT_PTR)Layout);
    if (Device == 0xE002 || Device == 0xE006 || Device == 0xE008 || Device == 0xE020
        || !lstrcmpiA(FileName, "TINTLGNT.IME") || !lstrcmpiA(FileName, "CINTLGNT.IME")
        || !lstrcmpiA(FileName, "MSTCIPHA.IME"))
        return 1;
    return 0;
}
NW_HOOK_METHOD(0x10016f80, NCTaiwanIME, IsTaiwanIME)

// 0x100e6a50: the shared "take it" default (ret 8): 1
#if NW_HOOK_MODE
NW_NAKED INT NCTaiwanIME::OnImeSetContext(WPARAM wParam, LPARAM lParam) NW_STOCK(0x100e6a50)
#else
INT NCTaiwanIME::OnImeSetContext(WPARAM wParam, LPARAM lParam)
{
    return 1;
}
#endif

// 0x10017060 (vslot 76) - the base IME's, then the composition cancelled and the candidate list closed on the viewport's
// input context; the candidates go and their box is hidden when it was shown (the clauses are kept, unlike the Japanese and
// Chinese IMEs')
void NCTaiwanIME::Deactivate()
{
    NCIME::Deactivate();
    HIMC hIMC = ImeGetContext();
    ImmNotifyIME(hIMC, NI_COMPOSITIONSTR, CPS_CANCEL, 0);
    ImmNotifyIME(hIMC, NI_CLOSECANDIDATE, 0, 0);
    ImeReleaseContext(hIMC);
    FreeCandList();
    if (CandidateBox->IsShowWindow())
        CandidateBox->HideWindow();
}
NW_HOOK_METHOD(0x10017060, NCTaiwanIME, Deactivate)

// 0x1001a2b0 (vslot 37) - the candidate box, hidden, then the reading string API, imm32's input context locks and the code
// page (the Chinese IME's body without its composition window; SetPosition has three more arguments in C4, left at their
// defaults)
INT NCTaiwanIME::OnCreate()
{
    NCIME::OnCreate();
    CandidateBox = new NCCandidateBox(9, 15);
    CandidateBox->SetPosition(0, 0, 100, 135);
    CreateChild(CandidateBox, NWS_VISIBLE);
    CandidateBox->HideWindow();
    LoadReadingString();
    LoadImmFunctions();
    UpdateCodePage();
    return 1;
}
NW_HOOK_METHOD(0x1001a2b0, NCTaiwanIME, OnCreate)

// 0x10017b30 (vslot 86) - shown and placed only when hidden: moved back to the IME's origin,
// then by (X, -Y) with the layout and page of the IME type (6: 1, 9 a page; 4: 2, 9 a page;
// 5 and 7: 3, 10 a page); other types leave it at the origin
INT NCTaiwanIME::OnCaret(INT X, INT Y)
{
    if (!CandidateBox->IsShowWindow())
    {
        INT Pos[2];
        INT* P = CandidateBox->GetLocalPos(Pos);
        CandidateBox->MoveWindow(-P[0], -P[1]);
        if (GConsole->GetIMEType() == 6)
        {
            CandidateBox->MoveWindow(X, -Y);
            CandidateBox->SetMode(1);
            CandidateBox->PageSize = 9;
        }
        else if (GConsole->GetIMEType() == 4)
        {
            CandidateBox->MoveWindow(X, -Y);
            CandidateBox->SetMode(2);
            CandidateBox->PageSize = 9;
        }
        else if (GConsole->GetIMEType() == 5 || GConsole->GetIMEType() == 7)
        {
            CandidateBox->MoveWindow(X, -Y);
            CandidateBox->SetMode(3);
            CandidateBox->PageSize = 10;
        }
        CandidateBox->ShowWindow();
    }
    return 1;
}
NW_HOOK_METHOD(0x10017b30, NCTaiwanIME, OnCaret)

// stock body shared with the Japanese IME's (0x1000e680)
void NCTaiwanIME::ShowWindow()
{
    NCWnd::ShowWindow();
    CandidateBox->HideWindow();
}

// stock body shared with the Japanese IME's (0x1000e6a0)
void NCTaiwanIME::FreeCandList()
{
    if (CandList)
    {
        CANDIDATELIST* List = CandList;
        CandList = NULL;
        appFree(List);
    }
}

// stock body shared with the Japanese IME's (0x100159b0; the stock code is one function, hooked there)
void NCTaiwanIME::FillCandidateBox()
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

// 0x1001a380 (vslot 38) - NCWnd::OnDestroy is not called. C4: no guard, and the IME keeps no input context of its own
INT NCTaiwanIME::OnDestroy()
{
    Clauses.Empty();
    if (CandList)
        appFree(CandList);
    CandList = NULL;
    DestroyChild(this, CandidateBox);
    return 1;
}
NW_HOOK_METHOD(0x1001a380, NCTaiwanIME, OnDestroy)

// 0x1001a460 (vslot 81) - NCJapaneseIME::OnImeComposition's copy (see there), with its own guard names, nothing read unless
// the IME is Taiwanese (IsTaiwanIME) and the cursor position never looked for in the attributes: with an ANSI IME it is half
// the byte length of the last conversion (the clause data's when it was read, else the last other string's), else the IME's
// answer. C4: the input context is the viewport's, asked for after the Taiwanese IME test (ImeGetContext) and released at the end
INT NCTaiwanIME::OnImeComposition(WPARAM wParam, LPARAM lParam)
{
    guard(NCTaiwanIME::OnImeComposition);
    TCHAR Clause[1024];
    NImeText Item;
    WCHAR Wide[0x1000];
    ANSICHAR Ansi[0x801];
    UBOOL bClauses = 0;
    INT Bytes;
    UINT Len = 0;
    UINT CursorLen;
    ImeIsUIMessage(WM_IME_SETCONTEXT, 1, 0);
    ImeIsUIMessage(WM_IME_CONTROL, IMC_CLOSESTATUSWINDOW, 0);
    // the CJK clients with another IME: nothing (and the context is not released)
    if ((GLanguageType == 2 || GLanguageType == 3 || GLanguageType == 4) && !IsTaiwanIME())
        return 1;
    HIMC hIMC = ImeGetContext();

    if (lParam & GCS_RESULTSTR)
    {
        if (lParam & GCS_RESULTCLAUSE)
        {
            if (IsAnsiIME())
            {
                guard(NCTaiwanIME::OnImeComposition::GCS_RESULTCLAUSE::NotUnicode);
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
                guard(NCTaiwanIME::OnImeComposition::GCS_RESULTCLAUSE::Unicode);
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
            guard(NCTaiwanIME::OnImeComposition::GCS_RESULTSTR::NotUnicode);
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
            guard(NCTaiwanIME::OnImeComposition::GCS_RESULTSTR::Unicode);
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
            guard(NCTaiwanIME::OnImeComposition::GCS_COMPSTR::NotUnicode);
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
            guard(NCTaiwanIME::OnImeComposition::GCS_COMPSTR::Unicode);
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
            guard(NCTaiwanIME::OnImeComposition::GCS_COMPATTR::NotUnicode);
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
            guard(NCTaiwanIME::OnImeComposition::GCS_COMPATTR::Unicode);
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

    CursorLen = Len;
    if (lParam & GCS_COMPCLAUSE)
    {
        if (IsAnsiIME())
        {
            guard(NCTaiwanIME::OnImeComposition::GCS_COMPCLAUSE::NotUnicode);
            Bytes = ImmGetCompositionStringW(hIMC, GCS_COMPCLAUSE, Ansi, 0x800);
            Ansi[Bytes] = 0;
            CursorLen = MultiByteToWideChar(CodePage, 0, Ansi, -1, Wide, 0x1000);
            if (CursorLen)
                CursorLen = CursorLen * 2 - 2;
            CompClauseLen = CursorLen >> 1;
            appMemset(CompClause, 0, sizeof(CompClause));
            appMemcpy(CompClause, Wide, CompClauseLen);
            CompClause[CompClauseLen] = 0;
            unguard;
        }
        else
        {
            guard(NCTaiwanIME::OnImeComposition::GCS_COMPCLAUSE::Unicode);
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
            CursorPos = CursorLen >> 1;
        else
            CursorPos = ImmGetCompositionStringW(hIMC, GCS_CURSORPOS, NULL, 0);
        SendToParent(NIM_CURSORPOS, CursorPos, 0);
    }

    if (lParam & GCS_COMPREADSTR)
    {
        if (IsAnsiIME())
        {
            guard(NCTaiwanIME::OnImeComposition::GCS_COMPREADSTR::NotUnicode);
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
            guard(NCTaiwanIME::OnImeComposition::GCS_COMPREADSTR::Unicode);
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
            guard(NCTaiwanIME::OnImeComposition::GCS_COMPREADCLAUSE::NotUnicode);
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
            guard(NCTaiwanIME::OnImeComposition::GCS_COMPREADCLAUSE::Unicode);
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
            guard(NCTaiwanIME::OnImeComposition::GCS_COMPREADATTR::NotUnicode);
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
            guard(NCTaiwanIME::OnImeComposition::GCS_COMPREADATTR::Unicode);
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
                // TImeTextArray::AddItem (0x10004d30, ret 4: the item by pointer, copied by the callee): out of line here, the
                // other IMEs' stock code has it inline
                Clauses.AddItem(Item);
            }
        }
    }
    ImeReleaseContext(hIMC);
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1001a460, NCTaiwanIME, OnImeComposition)

// 0x1001b2b0 - the Japanese IME's (see there) with the IME type 7 hiding the candidate box when the IME has no list (the
// context is released in any case); the IME is told to start its page where the selection's page of 9 starts, unless it
// already does
void NCTaiwanIME::ReadCandList()
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
    else if (GConsole->GetIMEType() == 7)
        CandidateBox->HideWindow();
    ImeReleaseContext(hIMC);
}
NW_HOOK_METHOD(0x1001b2b0, NCTaiwanIME, ReadCandList)

// 0x1001c950 (vslot 77) - answers 1 (handled) for everything but a change of the conversion mode, which is 0, and
// the private notifications: only while active with a Taiwanese IME (else 1). The candidate list is read when it opens
// (inside a guard of its own) or changes and closed with its box. A change of the conversion mode in a restricted field
// (Mode 2 and 6: NCIME's test) clears it again (0). IMN_PRIVATE: the reading string is read while no candidate is shown,
// then the notifications the old IMEs send for their own reading window (4.2 .. 4.4: 1 and 2; 5.0 .. 5.2 and 6.0: 16, 17,
// 26, 27, 28) are answered 1, every other 0
INT NCTaiwanIME::OnImeNotify(WPARAM wParam, LPARAM lParam)
{
    guard(NCTaiwanIME::OnImeNotify);
    if (!bActive || !IsTaiwanIME())
        return 1;
    switch (wParam)
    {
    case IMN_SETCONVERSIONMODE:
    {
        HIMC hIMC = ImeGetContext();
        if (IsRestrictedMode())
        {
            DWORD Conversion, Sentence;
            ImmGetConversionStatus(hIMC, &Conversion, &Sentence);
            if (Conversion)
                ImmSetConversionStatus(hIMC, 0, 0);
        }
        ImeReleaseContext(hIMC);
        return 0;
    }
    case IMN_OPENCANDIDATE:
    {
        guard(NCTaiwanIME::OnImeNotify::IMN_OPENCANDIDATE);
        ReadCandList();
        unguard;
        return 1;
    }
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
    case IMN_PRIVATE:
    {
        if (!CandidateBox->IsShowWindow())
            ReadReadingString();
        DWORD Id = GetImeId();
        if ((Id >= IMEID_CHT_VER50 && Id <= IMEID_CHT_VER52) || Id == IMEID_CHT_VER60)
        {
            if (lParam == 16 || lParam == 17 || lParam == 26 || lParam == 27 || lParam == 28)
                return 1;
        }
        else if (Id >= IMEID_CHT_VER42 && Id <= IMEID_CHT_VER44)
        {
            if (lParam == 1 || lParam == 2)
                return 1;
        }
        return 0;
    }
    }
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1001c950, NCTaiwanIME, OnImeNotify)

NW_CHECK_VTABLE(NCTaiwanIME, 0x101a4d40, 87)
