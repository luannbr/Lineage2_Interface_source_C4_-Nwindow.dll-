#pragma once
// NCHtmlButton / NCHtmlPushButton - the buttons of the HTML pages (UClass "CHtmlButton" 0x220,
// "CHtmlPushButton" 0x228). They paint like NCButton but clip to the window they live in, so a
// page scrolled under the viewer's edge does not show them: NCHtmlButton to the enclosing
// NCHtmlViewer, NCHtmlPushButton to its first ancestor that is not a control (the scroll
// buttons of an HTML edit box). A click on an NCHtmlButton reports Command to the viewer.

#include "nwindow/NCButton.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCHtmlButton : public NCButton
{
    NW_DECLARE_CLASS(NCHtmlButton, NCButton, 0, 0x102CD7D8)

    const TCHAR* Command;      // +0x2B4 (name ours) sent on a click (NWM_COMMAND lParam): the
                               //   <button> action

    NCHtmlButton();                                                             // 0x1004ac30
    void DestroyBody();                                                         // 0x1004ac80 (name ours) the stock destructor: the vtable, ConditionalDestroy, NCButton's destructor (0x10003140)
    NCHtmlButton(const TCHAR* NormalTexName, const TCHAR* PushedTexName, const TCHAR* InCommand,
                 UBOOL bInAcceptKey);                                           // 0x100308e0
    NCHtmlButton(const TCHAR* NormalTexName, const TCHAR* PushedTexName);       // 0x10030910 (C1 only: no C2 match) (C1 only: no C2 match) (C1 only: no C2 match)
    // ~NCHtmlButton                                                            // 0x1004ac80

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);             // 25 0x100e9f60 (shared)
    virtual INT OnPaint(NCGDevice* Device);                                     // 62 0x10047d00
    virtual INT OnLButtonUp(WPARAM wParam, LPARAM lParam);                      // 69 0x10047410
};
NW_ASSERT_OFFSET(NCHtmlButton, Command, 0x2B4);
NW_ASSERT_SIZE(NCHtmlButton, 0x2B8);

class NCHtmlPushButton : public NCPushButton
{
    NW_DECLARE_CLASS(NCHtmlPushButton, NCPushButton, 0, 0x102CC848)

    NCHtmlPushButton();                                                         // 0x1004ad30
    void DestroyBody();                                                         // 0x1004ad80 (name ours) the vtable, ConditionalDestroy, NCPushButton's destructor (0x100038F0)
    NCHtmlPushButton(const TCHAR* NormalTexName, const TCHAR* PushedTexName);   // 0x10030cc0 (C1 only: no C2 match) (C1 only: no C2 match) (C1 only: no C2 match)
    // ~NCHtmlPushButton                                                        // 0x1004ad80

    virtual INT OnPaint(NCGDevice* Device);                                     // 62 0x10047e00
};
NW_ASSERT_SIZE(NCHtmlPushButton, 0x2C0);

#pragma pack(pop)