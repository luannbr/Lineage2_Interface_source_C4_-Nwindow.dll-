#pragma once
// NCJapaneseIME - the IME of the Japanese client (UClass "CJapaneseIME", 0x620C). The
// composition is read as the Korean IME reads it (clauses included) but only with a Japanese
// keyboard layout, and the cursor comes from the IME only while some characters are still
// being typed (attribute 0). The IME's candidate list is copied (CandList) and shown in an
// NCCandidateBox, 9 a page, placed at the caret the edit box reports (NIM_CARET).

#include "nwindow/NCIME.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCJapaneseIME : public NCIME
{
    NW_DECLARE_CLASS(NCJapaneseIME, NCIME, 0, 0x102BF060)

    TArray<NImeText> Clauses;        // +0x4248 as NCKoreanIME's
    CANDIDATELIST* CandList;         // +0x4254 copy of the IME's list (ImmGetCandidateListW)
    NCCandidateBox* CandidateBox;    // +0x4258
    BYTE Unknown420C[0x2000];        // +0x425C not used   // C2: offset derived, no evidence   // C2: offset derived, no evidence   // C2: offset derived, no evidence

    NCJapaneseIME(INT InMode = 0);                                   // 0x10017fb0 (C1 only: no C2 match) (C1 only: no C2 match)
    // ~NCJapaneseIME                                                // 0x10018020 (C1 only: no C2 match) (C1 only: no C2 match)

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x100168d0 (shared)
    virtual INT OnCreate();                                          // 37 0x10019040
    virtual INT OnDestroy();                                         // 38 0x10019120
    virtual void ShowWindow();                                       // 48 0x10017020
    virtual void Deactivate();                                       // 76 0x1001a230
    virtual INT OnImeNotify(WPARAM wParam, LPARAM lParam);           // 77 0x1001c880
    virtual INT OnImeEndComposition(WPARAM wParam, LPARAM lParam);   // 79 0x10016a20
    virtual INT OnImeSetContext(WPARAM wParam, LPARAM lParam) { return 1; } // 80 0x100e6a50 (folded)
    virtual INT OnImeComposition(WPARAM wParam, LPARAM lParam);      // 81 0x100192b0
    virtual INT OnInputLangChange(WPARAM wParam, LPARAM lParam) { return 1; } // 82 0x100e6a50 (folded)
    // name ours: the edit box's answer to NIM_GETCARET
    virtual INT OnCaret(INT X, INT Y);                               // 86 0x10017930

    // names ours
    __declspec(noinline) void FreeCandList();                        // 0x100169f0 (shared by the three IMEs; the stock callers call it)
    void FillCandidateBox();                                         // 0x100179c0 (shared)
    void ReadCandList();                                             // 0x10019200
    UBOOL IsJapaneseLayout();                                        // 0x100169d0 (this unused)
};
NW_ASSERT_OFFSET(NCJapaneseIME, Clauses, 0x4248);
NW_ASSERT_OFFSET(NCJapaneseIME, CandidateBox, 0x4258);
NW_ASSERT_SIZE(NCJapaneseIME, 0x625C);

#pragma pack(pop)