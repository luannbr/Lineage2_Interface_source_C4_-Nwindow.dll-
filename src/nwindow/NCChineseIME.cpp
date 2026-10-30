/*=============================================================================
    NCChineseIME.cpp - IME of the Chinese client. Stock: 0x1000ef80 .. 0x1000f087,
    0x1000f090 .. 0x1000f0b4, 0x1000f0c0 .. 0x1000f2ad, 0x1000f390 .. 0x1000f3fb,
    0x1000fbf0 .. 0x10010206, 0x10013300 .. 0x100134be, 0x100134c0 .. 0x10014637,
    0x10014b60 .. 0x1001522e (the window procedure and FreeCandList are the Japanese IME's
    bodies).
=============================================================================*/
#include "nwindow/NCChineseIME.h"
#include "nwindow/NCChatWnd.h"
#include "nwindow/NCCompositeBox.h"
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
    IMEID_CHS_VER41 = 0x40001,         // Microsoft Pinyin 1.5
    IMEID_CHS_VER42 = 0x40002,         // Microsoft Pinyin 2
    IMEID_CHS_VER53 = 0x50003,         // Microsoft Pinyin 3
};

// 0x1001d640 - CandList and the boxes are left as allocated
NCChineseIME::NCChineseIME(INT InMode)
    : NCIME(0)
{
    Mode = InMode;
}
NW_HOOK_CTOR_ARGS(0x1001d640, NCChineseIME, (INT A), (A))

// 0x1001d6f0
NW_HOOK_DTOR(0x1001d6f0, NCChineseIME)

// 0x10017580 - the class's operator new (NW_DECLARE_CLASS), out of line for the manager's
// OnCreate
static void* NewChineseIME(size_t Size, UObject* Outer, FName Name, DWORD SetFlags)
{
    return NCChineseIME::operator new(Size, Outer, Name, SetFlags);
}
NW_HOOK_FUNC(0x10017580, NewChineseIME)

// stock body shared with the Japanese IME's (0x1000ee70)
INT NCChineseIME::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
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

// 0x100170c0 - GChineseGetReadingString from the current IME's file, NULL without it (the
// library is not freed)
void NCChineseIME::LoadReadingString()
{
    ANSICHAR FileName[MAX_PATH];
    HKL Layout = GetKeyboardLayout(0);
    GChineseGetReadingString = NULL;
    if (ImmGetIMEFileNameA(Layout, FileName, MAX_PATH) > 0)
    {
        HMODULE IME = LoadLibraryA(FileName);
        if (IME)
            GChineseGetReadingString = (NGetReadingString)GetProcAddress(IME, "GetReadingString");
    }
}
NW_HOOK_FUNC(0x100170c0, NCChineseIME::LoadReadingString)

// 0x10017120 - a Chinese layout with a Simplified Chinese IME: layouts E00E, E001, E004 and
// E025, or the files PINTLGNT, winpy, winabc and PWBX32 (this is not used)
UBOOL NCChineseIME::IsChineseIME()
{
    ANSICHAR FileName[260];
    HKL Layout = GetKeyboardLayout(0);
    ImmGetIMEFileNameA(Layout, FileName, 259);
    if (((UINT_PTR)Layout & 0x3FF) != LANG_CHINESE)
        return 0;
    WORD Device = HIWORD((UINT_PTR)Layout);
    if (Device == 0xE00E || Device == 0xE001 || Device == 0xE004 || Device == 0xE025
        || !lstrcmpiA(FileName, "PINTLGNT.IME") || !lstrcmpiA(FileName, "winpy.ime")
        || !lstrcmpiA(FileName, "winabc.ime") || !lstrcmpiA(FileName, "PWBX32.IME"))
        return 1;
    return 0;
}
NW_HOOK_METHOD(0x10017120, NCChineseIME, IsChineseIME)

// stock body shared with the Japanese IME's (0x1000e6a0)
void NCChineseIME::FreeCandList()
{
    if (CandList)
    {
        CANDIDATELIST* List = CandList;
        CandList = NULL;
        appFree(List);
    }
}

// 0x100171d0 (vslot 48) - both boxes hidden
void NCChineseIME::ShowWindow()
{
    NCWnd::ShowWindow();
    CandidateBox->HideWindow();
    CompositeBox->HideWindow();
}
NW_HOOK_METHOD(0x100171d0, NCChineseIME, ShowWindow)

// 0x10017200 - the version of the old Microsoft Pinyin IMEs, which keep the reading string in their private data: their
// layout (E00E), their file (unless the IME has GetReadingString, PINTLGNT or MSSCIPYA), a known version for a Chinese
// layout (any version with GetReadingString); 0 otherwise. C4: the version buffer comes from the engine's allocator
// (appMalloc, no NULL test) and is freed by the out-of-line operator delete when its version is returned, by appFree
// when not
DWORD NCChineseIME::GetImeId()
{
    ANSICHAR FileName[MAX_PATH];
    HKL Layout = GetKeyboardLayout(0);
    DWORD Lang = (DWORD)(UINT_PTR)Layout & 0x3FF;
    if ((UINT_PTR)Layout != 0xE00E0804 || ImmGetIMEFileNameA(Layout, FileName, MAX_PATH - 1) <= 0)
        return 0;
    if (!GChineseGetReadingString && lstrcmpiA(FileName, "PINTLGNT.IME") && lstrcmpiA(FileName, "MSSCIPYA.IME"))
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
            if (GChineseGetReadingString
                || (Lang == LANG_CHINESE
                    && (Version == IMEID_CHS_VER41 || Version == IMEID_CHS_VER42
                        || Version == IMEID_CHS_VER53)))
            {
                appFree(Buffer);   // the stock calls its out-of-line delete (0x10016880: GMalloc->Free) here
                return Version;
            }
        }
    }
    appFree(Buffer);
    return 0;
}
NW_HOOK_FUNC(0x10017200, NCChineseIME::GetImeId)

// 0x10017340 (vslot 79) - the IME types 11 and 12 also empty the composition window and hide
// the candidates
INT NCChineseIME::OnImeEndComposition(WPARAM wParam, LPARAM lParam)
{
    bNotComposing = 1;
    if (GConsole->GetIMEType() == 11 || GConsole->GetIMEType() == 12)
    {
        FString Text;
        Text.Empty();
        CompositeBox->SetText(*Text);
        CandidateBox->HideWindow();
    }
    SendToParent(NIM_ENDCOMPOSITION, 0, 0);
    return 1;
}
NW_HOOK_METHOD(0x10017340, NCChineseIME, OnImeEndComposition)

// 0x100175b0 - the system is NT (the version is read once)
bool NCChineseIME::IsNT()
{
    if (!GChineseNTChecked)
    {
        if (!GChineseOSVersionRead)
        {
            GChineseOSVersion.dwOSVersionInfoSize = sizeof(OSVERSIONINFOA);
#pragma warning(suppress : 4996)  // deprecated in current SDKs; the stock code calls it
            GetVersionExA(&GChineseOSVersion);
            GChineseOSVersionRead = true;
        }
        OSVERSIONINFOA Info = GChineseOSVersion;
        if (Info.dwPlatformId == VER_PLATFORM_WIN32_NT)
            GChineseNT = true;
        GChineseNTChecked = true;
    }
    return GChineseNT;
}
NW_HOOK_FUNC(0x100175b0, NCChineseIME::IsNT)

// the composition window: the class's five-argument constructor (0x10005160)
static NCCompositeBox* NewCompositeBox(INT W, INT H, INT X, INT Y, INT bShowCaret)
{
    return new NCCompositeBox(W, H, X, Y, bShowCaret);
}

// 0x1001b390 (vslot 37) - the candidate box and the composition window, hidden, then the reading string API, imm32's input
// context locks and the code page (C3's body: SetPosition has three more arguments, left at their defaults)
INT NCChineseIME::OnCreate()
{
    NCIME::OnCreate();
    CandidateBox = new NCCandidateBox(9, 15);
    CandidateBox->SetPosition(0, 0, 100, 135);
    CreateChild(CandidateBox, NWS_VISIBLE);
    CandidateBox->HideWindow();
    LastComp.Empty();
    bCandOpened = 0;
    bOpenResultSent = 0;
    CompositeBox = NewCompositeBox(100, 15, 0, 0, 0);
    CompositeBox->SetPosition(0, 0, 100, 15);
    CreateChild(CompositeBox, NWS_VISIBLE);
    CompositeBox->HideWindow();
    LoadReadingString();
    LoadImmFunctions();
    UpdateCodePage();
    return 1;
}
NW_HOOK_METHOD(0x1001b390, NCChineseIME, OnCreate)

// 0x10017c70 - the Taiwanese IME's (see there) with the Pinyin IMEs' private data: 1.5 and 3 in Unicode, 2 in Unicode on NT
// only (else ANSI, with twice as much room before the lengths). C4: the viewport's input context (ImeGetContext), released
// before the string is sent; the IME's reading string comes in a buffer of the game's allocator (twice the length it
// answered), freed at the end, and a failed allocation is not tested
void NCChineseIME::ReadReadingString()
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
    if (GChineseGetReadingString)
    {
        BOOL bVertical;
        UINT MaxUiLen;
        Len = GChineseGetReadingString(hIMC, 0, NULL, &Err, &bVertical, &MaxUiLen);
        if (Len)
        {
            Str = Allocated = (WCHAR*)appMalloc(Len * 2, TEXT("new"));
            Len = GChineseGetReadingString(hIMC, Len, Str, &Err, &bVertical, &MaxUiLen);
        }
        bUnicode = true;
    }
    else
    {
        BYTE* IC = GImmLockIMC(hIMC);
        BYTE* p;
        switch (Id)
        {
        case IMEID_CHS_VER41:
            p = *(BYTE**)((BYTE*)GImmLockIMCC(IMCPrivate(IC)) + 0x20);
            if (!p)
                break;
            Len = *(DWORD*)(p + 0x9C);
            Err = *(DWORD*)(p + 0xA0);
            if ((UINT)Err >= Len)
                Err = Len;
            Str = (WCHAR*)(p + 0x38);
            bUnicode = true;
            break;
        case IMEID_CHS_VER42:
        {
            INT CharSize = IsNT() ? 2 : 1;
            p = *(BYTE**)((BYTE*)GImmLockIMCC(IMCPrivate(IC)) + 0x20);
            if (!p)
                break;
            Len = *(DWORD*)(p + 0x40 + 16 * CharSize);
            Err = *(DWORD*)(p + 0x44 + 16 * CharSize);
            bUnicode = IsNT();
            if (bUnicode)
                Str = (WCHAR*)(p + 0x40);
            else
                MultiByteToWideChar(CodePage, 0, (ANSICHAR*)(p + 0x40), -1, Local, Len);
            break;
        }
        case IMEID_CHS_VER53:
            p = *(BYTE**)((BYTE*)GImmLockIMCC(IMCPrivate(IC)) + 4);
            if (!p)
                break;
            p = *(BYTE**)(p + 0x18);
            if (!p)
                break;
            Len = *(DWORD*)(p + 0x60);
            Err = *(DWORD*)(p + 0x64);
            Str = (WCHAR*)(p + 0x40);
            bUnicode = true;
            break;
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
NW_HOOK_METHOD(0x10017c70, NCChineseIME, ReadReadingString)

// 0x10017ed0 - the Japanese IME's (see there) with 10 candidates a page for the IME type 9
// (the box 150 px high then)
void NCChineseIME::FillCandidateBox()
{
    SIZE Size;
    SIZE ItemSize;
    CandidateBox->Clear();
    INT PageSize = GConsole->GetIMEType() == 9 ? 10 : 9;
    DWORD i = CandList->dwPageStart;
    TCHAR* Text = (TCHAR*)((BYTE*)CandList + CandList->dwOffset[i]);
    CandidateBox->AddItem(Text, (INT)0xFFDCDCDC);
    if (i == CandList->dwSelection)
        CandidateBox->Selected = i % PageSize;
    NCGDevice::GetTextExtent(&Size, Text, 0, 0, 0, 0, 0, L2FONT_Normal);
    for (i = CandList->dwPageStart + 1; i < CandList->dwCount; i++)
    {
        if (i >= CandList->dwPageSize + CandList->dwPageStart)
            break;
        Text = (TCHAR*)((BYTE*)CandList + CandList->dwOffset[i]);
        CandidateBox->AddItem(Text, (INT)0xFFDCDCDC);
        NCGDevice::GetTextExtent(&ItemSize, Text, 0, 0, 0, 0, 0, L2FONT_Normal);
        if (i == CandList->dwSelection)
            CandidateBox->Selected = i % PageSize;
        if (ItemSize.cx > Size.cx)
            Size.cx = ItemSize.cx;
    }
    CandidateBox->SetScroll(CandList->dwCount, CandList->dwSelection);
    CandidateBox->PageSize = CandList->dwPageSize;
    CandidateBox->CallSetSize(Size.cx + 20, PageSize * 15);
    SendToParent(NIM_GETCARET, (WPARAM)Entry, 0);
}
NW_HOOK_METHOD(0x10017ed0, NCChineseIME, FillCandidateBox)

// C2 CANDIDATE: the C2 function is not the same code as the C1 one; kept only while it matches the stock one (verify_data)
// 0x10018060 (vslot 86) - moved to the caret when hidden, then laid out by the IME type (9 and
// 12: layout 5, 10 a page, 100x150; else 0, 9 a page, 100x135) and shown. The SetWindowRect
// puts the box back at the IME's origin, as in the stock code, so the caret is not followed.
INT NCChineseIME::OnCaret(INT X, INT Y)
{
    if (!CandidateBox->IsShowWindow())
    {
        INT Pos[2];
        INT* P = CandidateBox->GetLocalPos(Pos);
        CandidateBox->MoveWindow(-P[0], -P[1]);
        CandidateBox->MoveWindow(X, Y);
    }
    if (GConsole->GetIMEType() == 9 || GConsole->GetIMEType() == 12)
    {
        CandidateBox->SetMode(5);
        CandidateBox->PageSize = 10;
        CandidateBox->SetPosition(0, 0, 100, 150);
    }
    else
    {
        CandidateBox->SetMode(0);
        CandidateBox->PageSize = 9;
        CandidateBox->SetPosition(0, 0, 100, 135);
    }
    CandidateBox->ShowWindow();
    return 1;
}
NW_HOOK_METHOD(0x10018060, NCChineseIME, OnCaret)

// (slot 75, 0x100165d0, is NCIME::Activate itself: the stock vtable of this class points at it, so the class has no override)

// 0x1001b510 (vslot 38) - NCWnd::OnDestroy is not called. C4: no guard, and the input context is not destroyed here any more
// (the IME keeps none: ImeGetContext asks for the viewport's each time)
INT NCChineseIME::OnDestroy()
{
    Clauses.Empty();
    if (CandList)
        appFree(CandList);
    CandList = NULL;
    DestroyChild(this, CandidateBox);
    DestroyChild(this, CompositeBox);
    return 1;
}
NW_HOOK_METHOD(0x1001b510, NCChineseIME, OnDestroy)

// 0x1001b620 - the Japanese IME's (see there) with the IME type's page (10 for the type 9), the page start always sent to the
// IME and kept in CandList for the type 10; a selection past the list stops early (C4: the context is released then too)
void NCChineseIME::ReadCandList()
{
    HIMC hIMC = ImeGetContext();
    DWORD Size = ImmGetCandidateListW(hIMC, 0, NULL, 0);
    FreeCandList();
    if (Size)
    {
        CandList = (CANDIDATELIST*)appMalloc(Size, TEXT("new"));
        CandList->dwSize = Size;
        ImmGetCandidateListW(hIMC, 0, CandList, Size);
        if (CandList->dwSelection < CandList->dwCount)
        {
            DWORD PageStart = 0;
            DWORD Step;
            if (GConsole->GetIMEType() == 9)
                Step = CandList->dwPageSize == 10 || !CandList->dwPageSize ? CandList->dwPageSize : 10;
            else
                Step = CandList->dwPageSize == 9 || !CandList->dwPageSize ? CandList->dwPageSize : 9;
            if (CandList->dwSelection > 0 && CandList->dwPageSize && Step <= CandList->dwSelection)
            {
                do
                    PageStart += Step;
                while (Step + PageStart <= CandList->dwSelection);
            }
            if (GConsole->GetIMEType() == 10)
                CandList->dwPageStart = PageStart;
            ImmNotifyIME(hIMC, NI_SETCANDIDATE_PAGESTART, 0, PageStart);
            FillCandidateBox();
        }
    }
    ImeReleaseContext(hIMC);
}
NW_HOOK_METHOD(0x1001b620, NCChineseIME, ReadCandList)

// 0x1001b720 (vslot 81) - NCJapaneseIME::OnImeComposition's copy (see there), with its own guard names, nothing read unless
// the IME is Chinese (GetImeId is called first, its result unused), and for the IME type 10: LastComp keeps the composition
// string before each change and the clause data is not read. The clauses are rebuilt without telling the parent anything
// more. C4: the input context is the viewport's, asked for after the Chinese IME test (ImeGetContext) and released at the end
INT NCChineseIME::OnImeComposition(WPARAM wParam, LPARAM lParam)
{
    guard(NCChineseIME::OnImeComposition);
    TCHAR Clause[1024];
    NImeText Item;
    WCHAR Wide[0x1000];
    ANSICHAR Ansi[0x801];
    UBOOL bClauses = 0;
    INT Bytes;
    UINT Len = 0;
    ImeIsUIMessage(WM_IME_SETCONTEXT, 1, 0);
    ImeIsUIMessage(WM_IME_CONTROL, IMC_CLOSESTATUSWINDOW, 0);
    GetImeId();
    // the CJK clients with another IME: nothing (and the context is not released)
    if ((GLanguageType == 2 || GLanguageType == 3 || GLanguageType == 4) && !IsChineseIME())
        return 1;
    HIMC hIMC = ImeGetContext();

    if (lParam & GCS_RESULTSTR)
    {
        if (lParam & GCS_RESULTCLAUSE)
        {
            if (IsAnsiIME())
            {
                guard(NCChineseIME::OnImeComposition::GCS_RESULTCLAUSE::NotUnicode);
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
                guard(NCChineseIME::OnImeComposition::GCS_RESULTCLAUSE::Unicode);
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
            guard(NCChineseIME::OnImeComposition::GCS_RESULTSTR::NotUnicode);
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
            guard(NCChineseIME::OnImeComposition::GCS_RESULTSTR::Unicode);
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
            guard(NCChineseIME::OnImeComposition::GCS_COMPSTR::NotUnicode);
            Bytes = ImmGetCompositionStringW(hIMC, GCS_COMPSTR, Ansi, 0x800);
            Ansi[Bytes] = 0;
            Len = MultiByteToWideChar(CodePage, 0, Ansi, -1, Wide, 0x1000);
            if (Len)
                Len = Len * 2 - 2;
            CompStrLen = Len >> 1;
            if (GConsole->GetIMEType() == 10)
                LastComp = FString(CompStr);
            appMemset(CompStr, 0, sizeof(CompStr));
            CompStr[0] = 0;
            appStrcpy(CompStr, Wide);
            CompStr[CompStrLen] = 0;
            unguard;
        }
        else
        {
            guard(NCChineseIME::OnImeComposition::GCS_COMPSTR::Unicode);
            if (GConsole->GetIMEType() == 10)
                LastComp = FString(CompStr);
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
        if (GConsole->GetIMEType() == 10)
            LastComp = FString(CompStr);
    }

    if (lParam & GCS_COMPATTR)
    {
        if (IsAnsiIME())
        {
            guard(NCChineseIME::OnImeComposition::GCS_COMPATTR::NotUnicode);
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
            guard(NCChineseIME::OnImeComposition::GCS_COMPATTR::Unicode);
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

    if (GConsole->GetIMEType() != 10)
    {
        if (lParam & GCS_COMPCLAUSE)
        {
            if (IsAnsiIME())
            {
                guard(NCChineseIME::OnImeComposition::GCS_COMPCLAUSE::NotUnicode);
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
                guard(NCChineseIME::OnImeComposition::GCS_COMPCLAUSE::Unicode);
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
            guard(NCChineseIME::OnImeComposition::GCS_COMPREADSTR::NotUnicode);
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
            guard(NCChineseIME::OnImeComposition::GCS_COMPREADSTR::Unicode);
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

    if (GConsole->GetIMEType() != 10)
    {
        if (lParam & GCS_COMPREADCLAUSE)
        {
            if (IsAnsiIME())
            {
                guard(NCChineseIME::OnImeComposition::GCS_COMPREADCLAUSE::NotUnicode);
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
                guard(NCChineseIME::OnImeComposition::GCS_COMPREADCLAUSE::Unicode);
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
    }

    if (lParam & GCS_COMPREADATTR)
    {
        if (IsAnsiIME())
        {
            guard(NCChineseIME::OnImeComposition::GCS_COMPREADATTR::NotUnicode);
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
            guard(NCChineseIME::OnImeComposition::GCS_COMPREADATTR::Unicode);
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

    // one entry per clause of the composition (none without clause data)
    if (bClauses)
    {
        Clauses.Empty();
        if (CompStrLen && CompClauseLen && CompAttrLen)
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
NW_HOOK_METHOD(0x1001b720, NCChineseIME, OnImeComposition)

// 0x1001c750 (vslot 76) - the base IME's, then the composition and the candidates cancelled in the viewport's context; both
// boxes are hidden when the candidates were shown. C4: the open status and the context association are the base's now
void NCChineseIME::Deactivate()
{
    NCIME::Deactivate();
    HIMC hIMC = ImeGetContext();
    ImmNotifyIME(hIMC, NI_COMPOSITIONSTR, CPS_CANCEL, 0);
    ImmNotifyIME(hIMC, NI_CLOSECANDIDATE, 0, 0);
    ImeReleaseContext(hIMC);
    FreeCandList();
    Clauses.Empty();
    if (CandidateBox->IsShowWindow())
    {
        CandidateBox->HideWindow();
        CompositeBox->HideWindow();
    }
}
NW_HOOK_METHOD(0x1001c750, NCChineseIME, Deactivate)

// 0x1001cb90 (vslot 77) - only while active with a Chinese IME. C4 answers 1 (handled) for the candidate notifications; the
// conversion mode change (IMN_SETCONVERSIONMODE alone: the open status change is not handled any more) puts the mode back
// to 0 when the IME is in mode 6 or 2 (NCIME's test, the anonymous 0x10016530) and the mode is not 0; the candidate list is
// read when it opens (the IME type 11 follows its changes only from then on, the type 12 sends the first character of the
// page's second candidate as the result on every other opening) and closed with both boxes (always for the types 11 and 12).
// IMN_PRIVATE: for the types 11 and 12 the keys held build the composition window's text (letters added in lower case,
// Backspace, Space empties it) and the type 12 takes the candidate of the digit held (1..9, 0 for the tenth) as the result,
// without its first character; then the reading string is read while no candidate is shown (always for the type 8), and
// the private notifications the old IMEs send for their own reading window (4.x: 1 and 2; 5.3: 16, 17, 26, 27, 28) are
// answered 1, every other 0 (C3 answered the opposite)
INT NCChineseIME::OnImeNotify(WPARAM wParam, LPARAM lParam)
{
    if (!bActive || !IsChineseIME())
        return 0;
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
        if (GConsole->GetIMEType() == 11)
            bCandOpened = 1;
        ReadCandList();
        if (GConsole->GetIMEType() == 12)
        {
            if (!bOpenResultSent)
            {
                bOpenResultSent = 1;
                appMemset(ResultStr, 0, sizeof(ResultStr));
                ResultStr[0] = 0;
                if (CandList)
                {
                    appMemcpy(ResultStr, (BYTE*)CandList + CandList->dwOffset[CandList->dwPageStart + 1], 2);
                    ResultStr[1] = 0;
                    SendToParent(NIM_RESULT, 0, 0);
                }
            }
            else
                bOpenResultSent = 0;
        }
        return 1;
    case IMN_CHANGECANDIDATE:
        if (GConsole->GetIMEType() != 11 || bCandOpened)
            ReadCandList();
        return 1;
    case IMN_CLOSECANDIDATE:
        if (lParam == 1 || GConsole->GetIMEType() == 11 || GConsole->GetIMEType() == 12)
        {
            FreeCandList();
            CandidateBox->HideWindow();
            CompositeBox->HideWindow();
            bOpenResultSent = 0;
            bCandOpened = 0;
        }
        return 1;
    case IMN_PRIVATE:
        if (IsRestrictedMode())
            return 0;
        if (GConsole->GetIMEType() == 11 || GConsole->GetIMEType() == 12)
        {
            BYTE KeyState[256];
            GetKeyboardState(KeyState);
            FString Text;
            for (INT Key = 'A'; Key < 0x5B; Key++)
            {
                if (KeyState[Key] & 0x80)
                {
                    TCHAR Letter[2];
                    Letter[0] = (TCHAR)(Key + ('a' - 'A'));
                    Letter[1] = 0;
                    Text = CompositeBox->Text + FString(Letter);   // GetText() is a copy of it
                    CompositeBox->SetText(*Text);
                }
            }
            if (KeyState[VK_BACK] & 0x80)
            {
                Text = CompositeBox->Text;
                if (Text.Len())
                {
                    Text.Delete(Text.Len() - 1, 1);
                    CompositeBox->SetText(*Text);
                }
            }
            if (KeyState[VK_SPACE] & 0x80)
            {
                FString None;
                None.Empty();
                CompositeBox->SetText(*None);
            }
            if (bCandOpened)
                bCandOpened = 0;
            if (GConsole->GetIMEType() == 12 && CandList)
            {
                appMemset(ResultStr, 0, sizeof(ResultStr));
                DWORD Start = CandList->dwPageStart;
                BYTE* List = (BYTE*)CandList;
                UBOOL bPicked = 0;
                TCHAR* Cand = NULL;
                // the last digit held wins, 0 over all (tested one by one in the stock code)
                for (INT Digit = 1; Digit <= 9; Digit++)
                {
                    if (KeyState['0' + Digit] & 0x80)
                    {
                        Cand = (TCHAR*)(List + CandList->dwOffset[Start + Digit - 1]);
                        bPicked = 1;
                    }
                }
                if (KeyState['0'] & 0x80)
                {
                    Cand = (TCHAR*)(List + CandList->dwOffset[Start + 9]);
                    bPicked = 1;
                }
                if (bPicked && Cand)
                {
                    INT CandLen = appStrlen(Cand);
                    appMemcpy(ResultStr, Cand + 1, CandLen * 2 - 2);
                    ResultStr[CandLen - 1] = 0;
                    FreeCandList();
                    Clauses.Empty();
                    bOpenResultSent = 0;
                    SendToParent(NIM_RESULT, 0, 0);
                    SendToParent(NIM_ATTRIBUTE, 1, 1);
                }
            }
        }
        if (!CandidateBox->IsShowWindow() || GConsole->GetIMEType() == 8)
            ReadReadingString();
        switch (GetImeId())
        {
        case IMEID_CHS_VER41:
        case IMEID_CHS_VER42:
            if (lParam == 1 || lParam == 2)
                return 1;
            break;
        case IMEID_CHS_VER53:
            if (lParam == 16 || lParam == 17 || lParam == 26 || lParam == 27 || lParam == 28)
                return 1;
            break;
        }
        return 0;
    }
    return 0;
}
NW_HOOK_METHOD(0x1001cb90, NCChineseIME, OnImeNotify)

NW_CHECK_VTABLE(NCChineseIME, 0x101a4ea0, 87)
