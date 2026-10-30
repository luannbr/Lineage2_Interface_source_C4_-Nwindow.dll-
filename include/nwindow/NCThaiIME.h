#pragma once
// NCThaiIME - the IME of the Thai client (UClass "CThaiIME", 0x61FC; new in C2, GLanguageType 5). It only keeps
// the input context of the game window: no composition is read (the Thai text is typed with the keyboard layout,
// not composed), so the base class's string buffers stay empty and the edit box tests the characters itself
// (NCEditBox::IsThaiInputValid). The class is as big as the Taiwanese IME's (0x620C minus one pointer) but nothing
// in its code touches the fields past NCIME's.

#include "nwindow/NCIME.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCThaiIME : public NCIME
{
    NW_DECLARE_CLASS(NCThaiIME, NCIME, 0, 0x102C0A58)

    BYTE Unknown41FC[0x2000];        // +0x4248 not used by any function of this class (allocated zeroed)   // C2: offset derived, no evidence   // C2: offset derived, no evidence

    NCThaiIME(INT InMode = 0);                                       // 0x10013790 (C1 only: no C2 match) (C1 only: no C2 match)
    // ~NCThaiIME                                                    // 0x10015b00 (C1 only: no C2 match)

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10017400
    virtual void ShowWindow();                                       // 48 0x100efc40 (shared)
    virtual void Deactivate();                                       // 76 0x10017510
    virtual INT OnImeNotify(WPARAM wParam, LPARAM lParam);           // 77 0x100174a0
    virtual INT OnImeSetContext(WPARAM wParam, LPARAM lParam) { return 1; }      // 80 0x100e6a50 (folded)
    virtual INT OnInputLangChange(WPARAM wParam, LPARAM lParam) { return 1; }    // 82 0x100e6a50 (folded)
};
NW_ASSERT_OFFSET(NCThaiIME, Unknown41FC, 0x4248);
NW_ASSERT_SIZE(NCThaiIME, 0x6248);

#pragma pack(pop)
