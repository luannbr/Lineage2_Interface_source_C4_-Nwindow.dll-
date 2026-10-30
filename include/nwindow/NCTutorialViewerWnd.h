#pragma once
// NCTutorialViewerWnd - the tutorial window (UClass "CTutorialViewerWnd", 0xA0): the NPC dialog
// layout around an NCTutorialHtmlViewer, pages given as text.
// NCTutorialQuestionWnd - the blinking question mark that opens a tutorial page (UClass
// "CTutorialQuestionWnd", 0xA8): a click asks the server for the page of its question and
// hides it.

#include "nwindow/NCWnd.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCTutorialHtmlViewer;

class NCTutorialViewerWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCTutorialViewerWnd, NCWnd, 0, 0x1030F408)

    // C3 layout (OnCreate 0x1010a4d0, OnPaint 0x10109d40): three background pieces instead of the single texture
    NCTutorialHtmlViewer* HtmlViewer; // +0xE8
    UTexture* TopTex;                // +0xEC "l2ui_ch3.tutorialwnd.tutorial_back1" 12 pixels, 20 below the top
    UTexture* MidTex;                // +0xF0 "l2ui_ch3.tutorialwnd.tutorial_back2" repeated 12 pixels at a time
    UTexture* BottomTex;             // +0xF4 "l2ui_ch3.tutorialwnd.tutorial_back3" 9 pixels, at the bottom

    NCTutorialViewerWnd();                                           // 0x1011f980
    // ~NCTutorialViewerWnd                                          // 0x100de770 (C1 only: no C2 match) (C1 only: no C2 match)

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x1011e960 (shared)
    virtual INT OnCreate();                                          // 37 0x1011f430
    virtual INT OnDestroy();                                         // 38 0x1011ecc0 (shared)
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x1011ed70
    virtual INT OnClose(WPARAM wParam, LPARAM lParam);               // 74 0x1003b600 (shared)

    void ShowPage(TCHAR* Text);                                      // 0x1011e9c0 (name ours)
    void FitToPage(INT ContentHeight);                               // 0x1011e9e0 (name ours) size to the page it holds
};
NW_ASSERT_OFFSET(NCTutorialViewerWnd, MidTex, 0xF0);
NW_ASSERT_OFFSET(NCTutorialViewerWnd, BottomTex, 0xF4);
NW_ASSERT_SIZE(NCTutorialViewerWnd, 0xF8);

// C3: class NCTutorialQuestionWnd gone (no counterpart): class NCTutorialQuestionWnd : public NCWnd
// C3: class NCTutorialQuestionWnd gone (no counterpart): {
// C3: class NCTutorialQuestionWnd gone (no counterpart):     NW_DECLARE_CLASS(NCTutorialQuestionWnd, NCWnd, 0, 0x102B6918)

// C3: class NCTutorialQuestionWnd gone (no counterpart):     UTexture* HelpTex;               // +0xC8 "L2UI.etcWnd.TutorialHelp"
// C3: class NCTutorialQuestionWnd gone (no counterpart):     UTexture* HelpClickTex;          // +0xCC "L2UI.etcWnd.TutorialHelp_click"
// C3: class NCTutorialQuestionWnd gone (no counterpart):     INT QuestionID;                  // +0xD0 (name ours) sent when clicked
// C3: class NCTutorialQuestionWnd gone (no counterpart):     UBOOL bLit;                      // +0xD4 HelpTex, else HelpClickTex: the timer blinks it,
// C3: class NCTutorialQuestionWnd gone (no counterpart):                                      //   a press holds it off
// C3: class NCTutorialQuestionWnd gone (no counterpart):     INT TimerID;                     // +0xD8 (0xFA0D06, 500 ms)

// C3: class NCTutorialQuestionWnd gone (no counterpart):     NCTutorialQuestionWnd();                                         // 0x1010aa90
// C3: class NCTutorialQuestionWnd gone (no counterpart):     // ~NCTutorialQuestionWnd                                        // 0x100deb20 (C1 only: no C2 match)

// C3: class NCTutorialQuestionWnd gone (no counterpart):     virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10119f60 (shared)
// C3: class NCTutorialQuestionWnd gone (no counterpart):     virtual INT OnTimer(INT InTimerID);                              // 36 0x10109a10
// C3: class NCTutorialQuestionWnd gone (no counterpart):     virtual INT OnCreate();                                          // 37 0x10109940
// C3: class NCTutorialQuestionWnd gone (no counterpart):     virtual INT OnPaint(NCGDevice* Device);                          // 56 0x10109f50
// C3: class NCTutorialQuestionWnd gone (no counterpart):     virtual INT OnLButtonDown(WPARAM wParam, LPARAM lParam);         // 62 0x10109990
// C3: class NCTutorialQuestionWnd gone (no counterpart):     virtual INT OnLButtonUp(WPARAM wParam, LPARAM lParam);           // 63 0x101099c0
// C3: class NCTutorialQuestionWnd gone (no counterpart): };
// C3: class NCTutorialQuestionWnd gone (no counterpart): NW_ASSERT_OFFSET(NCTutorialQuestionWnd, QuestionID, 0xD0);
// C3: class NCTutorialQuestionWnd gone (no counterpart): NW_ASSERT_SIZE(NCTutorialQuestionWnd, 0xDC);

#pragma pack(pop)