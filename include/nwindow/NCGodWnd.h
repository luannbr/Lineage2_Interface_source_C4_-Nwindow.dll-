#pragma once
// NCGodWnd - the screen-sized top window of the game screen (UClass "CGodWnd", 0xDC; C3 only: the console keeps it at
// +0x3950, NConsoleWnd::GodWnd, made last by Initialize): the restart menu, the message line at the bottom, the options
// window and the warning box live in it, so the console shows and hides them through its methods (ShowMessage,
// ShowOptionWnd, ShowRestartMenu, ...).
//
// Layout and virtuals from the stock constructor 0x100861C0, OnCreate 0x10085DD0, OnDestroy 0x100859E0, DefaultPosition
// 0x10085B00, SetWindowSize 0x10085C80 and the small methods 0x10085620 .. 0x10085D20.

#include "nwindow/NCWnd.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCLoginWnd;
class NCGaraDialogBox;
class NCMessageWnd;
class NCOptionWnd;
class NCWarningDialogBox;

class NCGodWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCGodWnd, NCWnd, 0, 0x102D8F08)

    NCWnd* UnknownC8;                // +0xE8 (name ours; 0 from the constructor) passed to the options window's constructor
    NCLoginWnd* LoginWnd;            // +0xEC (C4; the C3 header had the restart menu here) the login screen, as wide as the
                                     //   window and 52 less high, shown; made only when neither GIsOnlyL2Console nor GIsTestHtml
    NCMessageWnd* MessageWnd;        // +0xF0 52 high at the bottom, anchored (8)
    NCOptionWnd* OptionWnd;          // +0xF4 256 x 480, style 0x811002, title SysString 146 (hidden)
    NCWarningDialogBox* WarningBox;  // +0xF8 256 x 108 in the middle of the screen (hidden)
    NCGaraDialogBox* GaraDialog;     // +0xFC (C4) 256 x 108 above the middle, anchored (5), hidden, with the buttons 140 / 141;
                                     //   made with the login screen

    NCGodWnd();                                                      // 0x10091590
    // ~NCGodWnd                                                     // 0x10091620

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10148f10 (folded)
    virtual INT OnCreate();                                          // 37 0x100910a0
    virtual INT OnDestroy();                                         // 38 0x10090ca0 (NCWnd's is not called)
    virtual void SetWindowSize(FLOAT W, FLOAT H);                    // 47 0x10090f20 the message line follows the width

    // names from the stock guard strings (the console calls them by address)
    void ReceiveServerList(INT Param);                               // 0x10090970 the message line emptied, the options hidden
    void RestartBegin();                                             // 0x10090a80 the options and the restart menu hidden
    void ShowOptionWnd();                                            // 0x10090b00 toggles the options window (sounds 5 / 6)
    void ShowMessage(const TCHAR* Text);                             // 0x10090bd0 the line's text (when not empty), shown
    void DefaultPosition();                                          // 0x10090df0 the windows back in their places
    void ShowWarningMessage(const TCHAR* Text, INT bExit);           // 0x10090ff0 the box raised modal with the text
};
NW_ASSERT_OFFSET(NCGodWnd, LoginWnd, 0xEC);
NW_ASSERT_OFFSET(NCGodWnd, GaraDialog, 0xFC);
NW_ASSERT_OFFSET(NCGodWnd, OptionWnd, 0xF4);
NW_ASSERT_OFFSET(NCGodWnd, WarningBox, 0xF8);
NW_ASSERT_SIZE(NCGodWnd, 0x100);

#pragma pack(pop)
