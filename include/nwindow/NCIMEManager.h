#pragma once
// NCIMEManager - the IME of an edit box (UClass "CIMEManager", 0x1BC in C2): a hidden child that
// creates the IME of the client language (GLanguageType 0/1 Korean, 2 Japanese, 3 Taiwanese,
// 4 Chinese, 5 Thai (C2)), forwards Activate/Deactivate to it and passes the IME's private messages
// (NIM_*) on to the edit box. The getters read the IME's strings.

#include "nwindow/NCIME.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCIMEManager : public NControl
{
    NW_DECLARE_CLASS(NCIMEManager, NControl, 0, 0x102BFFF8)

    NCIME* KoreanIME;                // +0x1F4 NCKoreanIME (GLanguageType 0 and 1)
    NCIME* JapaneseIME;              // +0x1F8 NCJapaneseIME (2)
    NCIME* TaiwanIME;                // +0x1FC NCTaiwanIME (3)
    NCIME* ChineseIME;               // +0x200 NCChineseIME (4)
    NCIME* ThaiIME;                  // +0x204 new in C2: NCThaiIME (5, NCThaiIME.h, class CThaiIME 0x61FC).
                                     //   Activate, Deactivate and IsNotComposing know it; the getters (GetCompStr,
                                     //   GetResultStr, GetReadingStr) do not
    INT Mode;                        // +0x208 passed to the IME   // C2: offset derived, no evidence

    NCIMEManager(INT InMode = 0);                                    // 0x1001d280
    // ~NCIMEManager                                                 // 0x1001d310

    // WindowProc: the stock body is NCEditBox's (0x10007f80): NIM_* messages to 65..72
    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10015fe0
    virtual INT vfn033() { return 1; }                               // 34 0x100f7850 (folded)
    virtual INT OnCreate();                                          // 37 0x1001d880
    virtual INT OnDestroy();                                         // 38 0x10017650
    // names ours
    virtual void Activate();                                         // 75 0x10016220
    virtual void Deactivate();                                       // 76 0x10016280
    virtual INT OnComposition(WPARAM wParam, LPARAM lParam);         // 77 0x100162e0
    virtual INT OnResult(WPARAM wParam, LPARAM lParam);              // 78 0x10016300
    virtual INT OnAttribute(WPARAM wParam, LPARAM lParam);           // 79 0x10016320
    virtual INT OnReading(WPARAM wParam, LPARAM lParam);             // 80 0x10016340
    virtual INT On14(WPARAM wParam, LPARAM lParam) { return 1; }     // 81 0x100e6a50 (folded)
    virtual INT OnGetCaret(WPARAM wParam, LPARAM lParam);            // 82 0x10016360
    virtual INT OnEndComposition(WPARAM wParam, LPARAM lParam);      // 83 0x10016380
    virtual INT OnCursorPos(WPARAM wParam, LPARAM lParam);           // 84 0x100163a0

    // names ours; NULL (-1 for IsNotComposing) without an IME
    TCHAR* GetCompStr();                                             // 0x100177e0
    TCHAR* GetResultStr();                                           // 0x10017820
    TCHAR* GetReadingStr();                                          // 0x10017860
    UBOOL IsNotComposing();                                          // 0x100178a0

    // new in C2 (names ours), members of this class by their calling convention (the edit boxes call them with
    // ecx = IMEManager; they do not read it): the class of a Thai character (GLanguageType 5 only, else 0): 1
    // space, 2 consonant, 3 sara am, 4 vowel above/below, 5 vowel before/after, 6 symbol, 7 digit, 8 tone mark,
    // 9 thanthakhat
    INT GetThaiCharType(UINT Ch);                                    // 0x10016120
    // the length of Text in cursor steps: its Thai clusters (GLanguageType 5), else its characters
    INT GetTextLength(FString Text);                                 // 0x1001c7e0

    // the first IME created (inlined in the getters; the Thai one is not looked at)
    NCIME* GetIME()
    {
        if (KoreanIME)
            return KoreanIME;
        if (JapaneseIME)
            return JapaneseIME;
        if (TaiwanIME)
            return TaiwanIME;
        return ChineseIME;
    }
};
NW_ASSERT_OFFSET(NCIMEManager, KoreanIME, 0x1F4);
NW_ASSERT_OFFSET(NCIMEManager, ThaiIME, 0x204);
NW_ASSERT_OFFSET(NCIMEManager, Mode, 0x208);
NW_ASSERT_SIZE(NCIMEManager, 0x20C);

#pragma pack(pop)