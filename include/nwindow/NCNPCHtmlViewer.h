#pragma once
// NCNPCHtmlViewer - the NPC dialog window (UClass "CNPCHtmlViewer", 0x9C): a title bar, the
// "NPCHtml_BACK" background stretched, and an NCHtmlViewer that closes the window when a page
// command asks for it (NWM_HTML_CLOSE).

#include "nwindow/NCWnd.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCHtmlViewer;

class NCNPCHtmlViewer : public NCWnd
{
    NW_DECLARE_CLASS(NCNPCHtmlViewer, NCWnd, 0, 0x102C9448)

    NCHtmlViewer* HtmlViewer;        // +0xE8
    UTexture* BackTex;               // +0xEC "L2UI.etcwnd.NPCHtml_BACK"

    NCNPCHtmlViewer();                                               // 0x1003f4f0
    // ~NCNPCHtmlViewer                                              // 0x10030270 (C1 only: no C2 match) (C1 only: no C2 match)

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x1011e960
    virtual INT OnCreate();                                          // 37 0x1003f510
    virtual INT OnDestroy();                                         // 38 0x1003c790 (shared)
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x1003c840
    virtual INT OnClose(WPARAM wParam, LPARAM lParam);               // 74 0x1003b600 (name ours)

    void LoadFile(const TCHAR* FileName);                            // 0x100423b0 (name ours)
    void ShowPage(TCHAR* Text);                                      // 0x1011e9c0 (folded with
                                                                     //   NCTutorialViewerWnd's)
};
NW_ASSERT_OFFSET(NCNPCHtmlViewer, HtmlViewer, 0xE8);
NW_ASSERT_SIZE(NCNPCHtmlViewer, 0xF0);

#pragma pack(pop)