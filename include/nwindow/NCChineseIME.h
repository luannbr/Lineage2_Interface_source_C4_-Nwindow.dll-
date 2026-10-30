#pragma once
// NCChineseIME - the IME of the Chinese client (UClass "CChineseIME", 0x6224). The composition
// is read as the Japanese IME reads it, but only with a Chinese IME (see IsChineseIME); the
// candidate list goes into an NCCandidateBox (9 a page, 10 for the IME type 9). The IME types
// 11 and 12 (NConsoleWnd::GetIMEType; 11 is never returned) also show the letters typed in an
// NCCompositeBox, rebuilt from the keyboard state on IMN_PRIVATE, and 12 takes a candidate with
// the digit keys. The reading string (ReadingStr, NIM_READING) comes from the IME's
// GetReadingString export or, for the old Microsoft Pinyin IMEs without it, from the IME's
// private data (IMN_PRIVATE).

#include "nwindow/NCIME.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCCompositeBox;

// the IME's GetReadingString (NCChineseIME::LoadReadingString; name ours), and
// NCChineseIME::IsNT's function statics in the stock code (names ours)
#if NW_HOOK_MODE
#define GChineseGetReadingString NW_GLOBAL(NGetReadingString, 0x102BF04C)
#define GChineseOSVersion NW_GLOBAL(OSVERSIONINFOA, 0x102C14C8)
#define GChineseOSVersionRead NW_GLOBAL(bool, 0x102C155C)
#define GChineseNT NW_GLOBAL(bool, 0x102C155D)
#define GChineseNTChecked NW_GLOBAL(bool, 0x102C155E)
#else
extern NGetReadingString GChineseGetReadingString;
extern OSVERSIONINFOA GChineseOSVersion;
extern bool GChineseOSVersionRead;
extern bool GChineseNT;
extern bool GChineseNTChecked;
#endif

class NCChineseIME : public NCIME
{
    NW_DECLARE_CLASS(NCChineseIME, NCIME, 0, 0x102BF590)

    TArray<NImeText> Clauses;        // +0x4248 as NCKoreanIME's
    CANDIDATELIST* CandList;         // +0x4254 copy of the IME's list (ImmGetCandidateListW)
    NCCandidateBox* CandidateBox;    // +0x4258
    NCCompositeBox* CompositeBox;    // +0x425C letters typed (IME types 11 and 12)
    FString LastComp;                // +0x4260 the composition before the last one (IME type 10;
                                     //   the edit boxes move the box's caret by it)
    BYTE Unknown421C[0x2000];        // +0x426C not used   // C2: offset derived, no evidence   // C2: offset derived, no evidence   // C2: offset derived, no evidence
    UBOOL bCandOpened;               // +0x626C IME type 11: a candidate list opened since the
                                     //   last key (its changes are read only then)
    UBOOL bOpenResultSent;           // +0x6270 IME type 12: an opening candidate list sent a
                                     //   result (the next opening sends none)

    NCChineseIME(INT InMode = 0);                                    // 0x1001d640
    // ~NCChineseIME                                                 // 0x1001d6f0

    // WindowProc: the stock body is the Japanese IME's (0x1000ee70)
    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x100168d0
    virtual INT OnCreate();                                          // 37 0x1001b390
    virtual INT OnDestroy();                                         // 38 0x1001b510
    virtual void ShowWindow();                                       // 48 0x100171d0
    // Activate (slot 75, 0x100165d0) is NCIME's own: the stock vtable of this class points at it
    virtual void Deactivate();                                       // 76 0x1001c750
    virtual INT OnImeNotify(WPARAM wParam, LPARAM lParam);           // 77 0x1001cb90
    virtual INT OnImeEndComposition(WPARAM wParam, LPARAM lParam);   // 79 0x10017340
    virtual INT OnImeSetContext(WPARAM wParam, LPARAM lParam) { return 1; }   // 80 0x100e6a50 (folded)
    virtual INT OnImeComposition(WPARAM wParam, LPARAM lParam);      // 81 0x1001b720
    virtual INT OnInputLangChange(WPARAM wParam, LPARAM lParam) { return 1; } // 82 0x100e6a50 (folded)
    // name ours: the edit box's answer to NIM_GETCARET
    virtual INT OnCaret(INT X, INT Y);                               // 86 0x10018060

    // names ours
    __declspec(noinline) void FreeCandList();                        // 0x100169f0 (shared by the three IMEs; the stock callers call it)
    void FillCandidateBox();                                         // 0x10017ed0
    void ReadCandList();                                             // 0x1001b620
    void ReadReadingString();                                        // 0x10017c70
    UBOOL IsChineseIME();                                            // 0x10017120 (this unused)
    static void LoadReadingString();                                 // 0x100170c0
    static DWORD GetImeId();                                         // 0x10017200 the old IMEs'
                                                                     //   version, 0 for the others
    static bool IsNT();                                              // 0x100175b0
};
NW_ASSERT_OFFSET(NCChineseIME, Clauses, 0x4248);
NW_ASSERT_OFFSET(NCChineseIME, CompositeBox, 0x425C);
NW_ASSERT_OFFSET(NCChineseIME, LastComp, 0x4260);
NW_ASSERT_OFFSET(NCChineseIME, bCandOpened, 0x626C);
NW_ASSERT_SIZE(NCChineseIME, 0x6274);

#pragma pack(pop)