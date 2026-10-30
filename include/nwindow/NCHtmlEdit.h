#pragma once
// NCHtmlEdit / NCJHtmlEdit - the <edit> boxes of the HTML pages (UClass "CHtmlEdit" and
// "CJHtmlEdit", 0x23C each, both straight from NCEditBox). Enter sends EnterCommand to the
// viewer instead of NCEditBox's codes; the value is the text. NCHtmlEdit draws "NWindow.chatback"
// stretched instead of the edit frame, clipped to the viewer's height, and a click scrolls it
// into view; NCJHtmlEdit paints like NCEditBox (nothing in NWindow creates one).

#include "nwindow/NCEditBox.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCHtmlEdit : public NCEditBox
{
    NW_DECLARE_CLASS(NCHtmlEdit, NCEditBox, 0, 0x102CCD78)

    TCHAR* EnterCommand;             // +0x2D4 (name ours) sent on Enter, NULL = NCEditBox's keys

    NCHtmlEdit(INT InExtraWidth = 0, INT InType = 0);                        // 0x1003dc50 (C1 only: no C2 match) (C1 only: no C2 match)
    // ~NCHtmlEdit                                                   // 0x1004a590

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x1011ea70
    virtual INT OnPaint(NCGDevice* Device);                          // C2: slot gone (was 50)
    virtual INT OnKeyDown(WPARAM wParam, LPARAM lParam);             // 65 0x10047600
    virtual INT OnLButtonDown(WPARAM wParam, LPARAM lParam);         // 68 0x10047f00
    virtual const TCHAR* GetValue();                                 // 74 0x100475f0
};
NW_ASSERT_OFFSET(NCHtmlEdit, EnterCommand, 0x2D4);
NW_ASSERT_SIZE(NCHtmlEdit, 0x2D8);

class NCJHtmlEdit : public NCEditBox
{
    NW_DECLARE_CLASS(NCJHtmlEdit, NCEditBox, 0, 0x102CAE40)

    TCHAR* EnterCommand;             // +0x2D4 (set to NULL by the constructor, as NCHtmlEdit's)   // C2: offset derived, no evidence

    NCJHtmlEdit(INT Unused1 = 0, INT Unused2 = 1, INT Unused3 = 1);              // 0x1003dd60 (C1 only: no C2 match) (C1 only: no C2 match) (ret 0xC: three arguments, none read)
    // ~NCJHtmlEdit                                                  // 0x1004a6b0

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x1011ea70 (shared)
    virtual INT OnKeyDown(WPARAM wParam, LPARAM lParam);             // 65 0x10047600 (shared)
    virtual const TCHAR* GetValue();                                 // 74 0x100475f0 (shared)
};
NW_ASSERT_OFFSET(NCJHtmlEdit, EnterCommand, 0x2D4);
NW_ASSERT_SIZE(NCJHtmlEdit, 0x2D8);

#pragma pack(pop)