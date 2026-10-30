#pragma once
// NCTaiwanIME - the IME of the Taiwanese client (UClass "CTaiwanIME", 0x620C). The composition
// is read as the Korean IME reads it, but only with a Taiwanese IME (see IsTaiwanIME); the
// candidate list goes into an NCCandidateBox as the Japanese IME's does, laid out by the IME
// type the console reports. The reading string (ReadingStr, NIM_READING) comes from the IME's
// GetReadingString export or, for the old Microsoft IMEs without it, from the IME's private
// data (IMN_PRIVATE).

#include "nwindow/NCIME.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

// the IME's GetReadingString (NCTaiwanIME::LoadReadingString; name ours)
#if NW_HOOK_MODE
#define GTaiwanGetReadingString NW_GLOBAL(NGetReadingString, 0x102BF048)
#else
extern NGetReadingString GTaiwanGetReadingString;
#endif

class NCTaiwanIME : public NCIME
{
    NW_DECLARE_CLASS(NCTaiwanIME, NCIME, 0, 0x102C0F88)

    TArray<NImeText> Clauses;        // +0x4248 as NCKoreanIME's
    CANDIDATELIST* CandList;         // +0x4254 copy of the IME's list (ImmGetCandidateListW)
    NCCandidateBox* CandidateBox;    // +0x4258
    BYTE Unknown420C[0x2000];        // +0x425C not used   // C2: offset derived, no evidence   // C2: offset derived, no evidence   // C2: offset derived, no evidence

    NCTaiwanIME(INT InMode = 0);                                     // 0x10018120 (C1 only: no C2 match) (C1 only: no C2 match)
    // ~NCTaiwanIME                                                  // 0x10018190 (C1 only: no C2 match) (C1 only: no C2 match)

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10016a40
    virtual INT OnCreate();                                          // 37 0x1001a2b0
    virtual INT OnDestroy();                                         // 38 0x1001a380 (shared)
    virtual void ShowWindow();                                       // 48 0x10017020 (shared)
    virtual void Deactivate();                                       // 76 0x10017060
    virtual INT OnImeNotify(WPARAM wParam, LPARAM lParam);           // 77 0x1001c950
    virtual INT OnImeSetContext(WPARAM wParam, LPARAM lParam);       // 80 0x100e6a50
    virtual INT OnImeComposition(WPARAM wParam, LPARAM lParam);      // 81 0x1001a460
    virtual INT OnInputLangChange(WPARAM wParam, LPARAM lParam);     // 82 0x10017040 (its own body here, not the shared stub)
    // name ours: the edit box's answer to NIM_GETCARET
    virtual INT OnCaret(INT X, INT Y);                               // 86 0x10017b30

    // names ours
    __declspec(noinline) void FreeCandList();                        // 0x100169f0 (shared by the three IMEs; the stock callers call it)
    __declspec(noinline) void FillCandidateBox();                    // 0x100179c0 (shared with the Japanese IME; the stock ReadCandList calls it)
    void ReadCandList();                                             // 0x1001b2b0
    void ReadReadingString();                                        // 0x10016d40
    UBOOL IsTaiwanIME();                                             // 0x10016f80 (this unused)
    static void LoadReadingString();                                 // 0x10016b20
    static DWORD GetImeId();                                         // 0x10016bd0 the old IMEs'
                                                                     //   version, 0 for the others
};
NW_ASSERT_OFFSET(NCTaiwanIME, Clauses, 0x4248);
NW_ASSERT_OFFSET(NCTaiwanIME, CandidateBox, 0x4258);
NW_ASSERT_SIZE(NCTaiwanIME, 0x625C);

#pragma pack(pop)