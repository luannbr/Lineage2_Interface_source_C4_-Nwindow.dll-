#pragma once
// NCIME - base of the language IMEs (UClass "CIME", 0x41F8), hidden controls owned by
// NCIMEManager. Each keeps its own input context and a copy of every composition string
// (ImmGetCompositionStringW), which the edit boxes read. WindowProc turns the IME window
// messages into virtual calls; the base handlers only follow the composition state and the
// IME properties. Activate/Deactivate run when an edit box gains or loses the focus.
//
// Private messages between the IMEs, NCIMEManager and the edit boxes (names ours). The IMEs send
// them to their parent (NControl::SendToParent), the manager passes them on to the edit box:
//   0xF0000012 composition changed (wParam: last composed character, 0 when none, or 1)
//   0xF0000013 result string ready          0xF0000017 composition ended (Japanese, Chinese)
//   0xF0000015 caret position wanted: wParam = the window to answer with 0xF0000016 (X, Y)
//   0xF0000018 composition attribute on/off 0xF0000019 cursor position (wParam)
//   0xF000001A reading string changed (Chinese, Taiwanese)

#include "nwindow/NControl.h"
#include "nwindow/NCCandidateBox.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

enum
{
    NIM_COMPOSITION = (INT)0xF0000012,
    NIM_RESULT = (INT)0xF0000013,
    NIM_14 = (INT)0xF0000014,
    NIM_GETCARET = (INT)0xF0000015,
    NIM_CARET = (INT)0xF0000016,
    NIM_ENDCOMPOSITION = (INT)0xF0000017,
    NIM_ATTRIBUTE = (INT)0xF0000018,
    NIM_CURSORPOS = (INT)0xF0000019,
    NIM_READING = (INT)0xF000001A,
};

// (C2 kept the input context of the active IME in a global, GActiveIMC, for NConsoleWnd::GetIMEType: the C4 stock has no such
// global, GetIMEType gets the context from its caller; the declaration with a C3 placeholder address was removed, nothing used it)

class NCIME : public NControl
{
    NW_DECLARE_CLASS(NCIME, NControl, 0, 0x102C0528)

    // C2: not placed (no evidence for where it went): HIMC hIMC;                       // +0x1D4 own input context (ImmCreateContext)
    UBOOL bNotComposing;             // +0x1F4 cleared between WM_IME_START/ENDCOMPOSITION
    INT Mode;                        // +0x1F8 (Korean: 1 starts in native mode)
    UBOOL bActive;                   // +0x1FC between Activate and Deactivate
    INT Unknown1B0;                  // +0x200   // C2: offset derived, no evidence   // C2: offset derived, no evidence   // C2: offset derived, no evidence
    INT CursorPos;                   // +0x204 GCS_CURSORPOS   // C2: offset derived, no evidence   // C2: offset derived, no evidence   // C2: offset derived, no evidence
    INT Unknown1B8;                  // +0x208   // C2: offset derived, no evidence   // C2: offset derived, no evidence   // C2: offset derived, no evidence
    DWORD Property;                  // +0x20C IGP_PROPERTY of the current IME
    DWORD Conversion;                // +0x210 conversion / sentence mode kept while inactive
    DWORD Sentence;                  // +0x214
    UBOOL OpenStatus;                // +0x218 ImmGetOpenStatus kept (Japanese, Taiwanese, Chinese)
    UINT CodePage;                   // +0x21C of the keyboard layout (non-Unicode systems)
    TCHAR CompStr[1024];             // +0x220 GCS_COMPSTR
    TCHAR CompReadStr[1024];         // +0xA20 GCS_COMPREADSTR   // C2: offset derived, no evidence
    TCHAR ResultStr[1024];           // +0x1220 GCS_RESULTSTR
    TCHAR Unknown19D0[1024];         // +0x1A20   // C2: offset derived, no evidence   // C2: offset derived, no evidence   // C2: offset derived, no evidence
    TCHAR ReadingStr[1024];          // +0x2220 the reading string (Taiwanese, Chinese: NIM_READING)
    BYTE CompAttr[1024];             // +0x2A20 GCS_COMPATTR   // C2: offset derived, no evidence   // C2: offset derived, no evidence   // C2: offset derived, no evidence
    BYTE CompReadAttr[1024];         // +0x2E20 GCS_COMPREADATTR   // C2: offset derived, no evidence   // C2: offset derived, no evidence   // C2: offset derived, no evidence
    DWORD CompClause[256];           // +0x3220 GCS_COMPCLAUSE   // C2: offset derived, no evidence   // C2: offset derived, no evidence   // C2: offset derived, no evidence
    DWORD CompReadClause[256];       // +0x3620 GCS_COMPREADCLAUSE   // C2: offset derived, no evidence   // C2: offset derived, no evidence   // C2: offset derived, no evidence
    DWORD ResultClause[256];         // +0x3A20 GCS_RESULTCLAUSE   // C2: offset derived, no evidence   // C2: offset derived, no evidence   // C2: offset derived, no evidence
    DWORD Unknown3DD0[256];          // +0x3E20   // C2: offset derived, no evidence   // C2: offset derived, no evidence   // C2: offset derived, no evidence
    INT CompStrLen;                  // +0x4220 characters   // C2: offset derived, no evidence   // C2: offset derived, no evidence   // C2: offset derived, no evidence
    INT CompAttrLen;                 // +0x4224   // C2: offset derived, no evidence   // C2: offset derived, no evidence   // C2: offset derived, no evidence
    INT CompClauseLen;               // +0x4228 entries   // C2: offset derived, no evidence   // C2: offset derived, no evidence   // C2: offset derived, no evidence
    INT CompReadStrLen;              // +0x422C   // C2: offset derived, no evidence   // C2: offset derived, no evidence   // C2: offset derived, no evidence
    INT CompReadAttrLen;             // +0x4230   // C2: offset derived, no evidence   // C2: offset derived, no evidence   // C2: offset derived, no evidence
    INT CompReadClauseLen;           // +0x4234   // C2: offset derived, no evidence   // C2: offset derived, no evidence   // C2: offset derived, no evidence
    INT ResultStrLen;                // +0x4238   // C2: offset derived, no evidence   // C2: offset derived, no evidence   // C2: offset derived, no evidence
    INT ResultClauseLen;             // +0x423C   // C2: offset derived, no evidence   // C2: offset derived, no evidence   // C2: offset derived, no evidence
    INT Unknown41F0;                 // +0x4240   // C2: offset derived, no evidence   // C2: offset derived, no evidence   // C2: offset derived, no evidence
    INT Unknown41F4;                 // +0x4244   // C2: offset derived, no evidence   // C2: offset derived, no evidence   // C2: offset derived, no evidence

    NCIME(INT InMode = 0);                                           // 0x1001d360
    // ~NCIME                                                        // 0x1001d200

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x100163c0
    virtual INT OnCreate();                                          // 37 0x100178e0
    virtual INT OnDestroy();                                         // 38 0x10016520
    // names ours
    virtual void Activate();                                         // 75 0x100165d0
    virtual void Deactivate();                                       // 76 0x10016640
    virtual INT OnImeNotify(WPARAM wParam, LPARAM lParam) { return 1; }          // 77 0x100e6a50
    virtual INT OnImeStartComposition(WPARAM wParam, LPARAM lParam);             // 78 0x10001860
    virtual INT OnImeEndComposition(WPARAM wParam, LPARAM lParam);               // 79 0x10001830
    virtual INT OnImeSetContext(WPARAM wParam, LPARAM lParam);                   // 80 0x10016570
    virtual INT OnImeComposition(WPARAM wParam, LPARAM lParam) { return 1; }     // 81 0x100e6a50
    virtual INT OnInputLangChange(WPARAM wParam, LPARAM lParam);                 // 82 0x10016550
    virtual INT OnImeControl(WPARAM wParam, LPARAM lParam) { return 1; }         // 83 0x100e6a50 (C1: 71)
    virtual INT OnImeCompositionFull(WPARAM wParam, LPARAM lParam) { return 1; } // 84 0x100e6a50 (C1: 72)
    virtual INT OnImeChar(WPARAM wParam, LPARAM lParam) { return 1; }            // 85 0x100e6a50 (C1: 73)

    // names ours
    TCHAR* GetResultStr();                                           // 0x100165a0
    TCHAR* GetCompStr();                                             // 0x100165b0
    TCHAR* GetReadingStr();                                          // 0x100165c0
    UBOOL IsNotComposing();                                          // 0x100166e0
    __declspec(noinline) UBOOL IsRestrictedMode();                                        // 0x10016530 Mode 2 or 6 (the stock callers call it)
    UBOOL IsUnicodeOS();                                             // 0x100166f0 NT or Win98+
    // the Taiwanese and Chinese IMEs' setup (names ours)
    void UpdateCodePage();                                           // 0x10016750
    static void LoadImmFunctions();                                  // 0x10016b80

    // C3: the stock function (0x10028920) is ImmIsUIMessageW(GViewportHWnd, ...); NCObject.h still has the C2
    // address (0x10025240) and an empty body, so the IMEs call the stock one (hides NCObject's)
    BOOL ImeIsUIMessage(UINT Msg, WPARAM wParam, LPARAM lParam)
    {
#if NW_HOOK_MODE
        return ::nw::StockCallAt<BOOL>(0x1002B400, this, Msg, wParam, lParam);
#else
        return ImmIsUIMessageW(GViewportHWnd, Msg, wParam, lParam);
#endif
    }

    // the test every WM_IME_COMPOSITION handler makes before each string (inlined): Chinese,
    // Taiwanese and Japanese IMEs on a system without Unicode IME calls convert from CodePage
    UBOOL IsAnsiIME()
    {
        return (GLanguageType == 2 || GLanguageType == 3 || GLanguageType == 4) && !IsUnicodeOS();
    }
};
// NW_ASSERT_OFFSET(NCIME, hIMC, 0x1D4);   // C2: field not mapped
NW_ASSERT_OFFSET(NCIME, CodePage, 0x21C);
NW_ASSERT_OFFSET(NCIME, CompAttr, 0x2A20);
NW_ASSERT_OFFSET(NCIME, ResultClause, 0x3A20);
NW_ASSERT_OFFSET(NCIME, CompStrLen, 0x4220);
NW_ASSERT_SIZE(NCIME, 0x4248);

// imm32.dll's input context locks, found by NCIME::LoadImmFunctions: the reading string of the
// Taiwanese and Chinese IMEs without the GetReadingString API is read in the IME's private data
// (names ours)
typedef BYTE*(WINAPI* NImmLockIMC)(HIMC);
typedef BOOL(WINAPI* NImmUnlockIMC)(HIMC);
typedef LPVOID(WINAPI* NImmLockIMCC)(HIMCC);
typedef BOOL(WINAPI* NImmUnlockIMCC)(HIMCC);
#if NW_HOOK_MODE
#define GImmLockIMC NW_GLOBAL(NImmLockIMC, 0x102BF050)
#define GImmUnlockIMC NW_GLOBAL(NImmUnlockIMC, 0x102BF054)
#define GImmLockIMCC NW_GLOBAL(NImmLockIMCC, 0x102BF058)
#define GImmUnlockIMCC NW_GLOBAL(NImmUnlockIMCC, 0x102BF05C)
#else
extern NImmLockIMC GImmLockIMC;
extern NImmUnlockIMC GImmUnlockIMC;
extern NImmLockIMCC GImmLockIMCC;
extern NImmUnlockIMCC GImmUnlockIMCC;
#endif

// an IME's GetReadingString export (names ours)
typedef UINT(WINAPI* NGetReadingString)(HIMC, UINT, LPWSTR, PINT, BOOL*, PUINT);

// INPUTCONTEXT::hPrivate (immdev.h, not included)
inline HIMCC IMCPrivate(BYTE* IC) { return *(HIMCC*)(IC + 0x124); }

// OSVERSIONINFO cache of NCIME::IsUnicodeOS (function statics in the stock code)
#if NW_HOOK_MODE
#define GIMEOSChecked NW_GLOBAL(UBOOL, 0x102C14C0)
#define GIMEUnicodeOS NW_GLOBAL(UBOOL, 0x102C14BC)
#else
extern UBOOL GIMEOSChecked;
extern UBOOL GIMEUnicodeOS;
#endif

#pragma pack(pop)