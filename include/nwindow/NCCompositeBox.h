#pragma once
// NCCompositeBox - the IME composition window (UClass "CCompositeBox", 0xB0): the string
// being composed, drawn one character at a time from FirstChar until about 100 px, with a
// "|" caret before the character at CaretPos while bShowCaret is set. Filled by the IMEs.

#include "nwindow/NCWnd.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCCompositeBox : public NCWnd
{
    NW_DECLARE_CLASS(NCCompositeBox, NCWnd, 0, 0x102B77B8)

    UTexture* BackTex;         // +0xE8 NWindow.BlackTexture
    FString Text;              // +0xEC composition string
    INT FirstChar;             // +0xF8 first character drawn
    INT CaretPos;              // +0xFC
    UBOOL bShowCaret;          // +0x100

    NCCompositeBox();                                                // 0x10005050
    // the Chinese IME's (ret 0x14): size and place given (BackTex is left as allocated)
    NCCompositeBox(INT InW, INT InH, INT InX, INT InY, UBOOL bInShowCaret); // 0x10005160
    // ~NCCompositeBox                                               // 0x10003b60 (C1 only: no C2 match) (C1 only: no C2 match)

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10148f10 (NControl's)
    virtual INT OnCreate();                                          // 37 0x10004070
    virtual INT OnDestroy();                                         // 38 0x10004110 (shared)
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x10004810

    // names ours; the text scrolls so that the caret stays within 97 px
    void CaretLeft();                                                // 0x100040a0
    void CaretRight();                                               // 0x10004a10
    void SetText(const TCHAR* InText);                               // 0x10004b80
    FString GetText();                                               // 0x100040d0 a copy
    void SetShowCaret(UBOOL bShow);                                  // 0x10004100
};
NW_ASSERT_OFFSET(NCCompositeBox, Text, 0xEC);
NW_ASSERT_SIZE(NCCompositeBox, 0x104);

#pragma pack(pop)