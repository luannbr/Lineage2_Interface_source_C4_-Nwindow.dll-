#pragma once
// NCKoreanIME - the IME of Korean and English clients (UClass "CKoreanIME", 0x4204). Keeps the
// Hangul/alphanumeric mode across focus changes: while the chat line has the focus the mode is
// saved on Deactivate and restored on Activate. A composition with clause information is split
// into Clauses (Flag = target clause) for the edit box.

#include "nwindow/NCIME.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCKoreanIME : public NCIME
{
    NW_DECLARE_CLASS(NCKoreanIME, NCIME, 0, 0x102BFAC0)

    TArray<NImeText> Clauses;        // +0x4248   // C2: offset derived, no evidence

    NCKoreanIME(INT InMode = 0);                                     // 0x10017ee0 (C1 only: no C2 match) (C1 only: no C2 match)
    // ~NCKoreanIME                                                  // 0x10017f50 (C1 only: no C2 match) (C1 only: no C2 match)

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10016790
    virtual INT OnDestroy();                                         // 38 0x10016810
    virtual void Deactivate();                                       // 76 0x100168a0
    virtual INT OnImeNotify(WPARAM wParam, LPARAM lParam);           // 77 0x10016820
    virtual INT OnImeComposition(WPARAM wParam, LPARAM lParam);      // 81 0x10018160
};
NW_ASSERT_OFFSET(NCKoreanIME, Clauses, 0x4248);
NW_ASSERT_SIZE(NCKoreanIME, 0x4254);

#pragma pack(pop)