#pragma once
// NCOptionWnd - the options window (UClass "COptionWnd", 0xB8; system menu > Options): three
// tab pages (video, audio, game) and OK / Cancel / Apply. OK and Apply make every page apply
// its settings and mark the first run done (Option.ini [FirstRun]); OK and Cancel close it.

#include "nwindow/NCWnd.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCButton;
class NCTabControl;
class NCVideoOptionWnd;
class NCAudioOptionWnd;
class NCGameOptionWnd;

// NWM_COMMAND ids of the window's controls
enum
{
    NID_OPTION_FRAME = 0xF001,
    NID_OPTION_OK = 0xF002,
    NID_OPTION_CANCEL = 0xF003,
    NID_OPTION_APPLY = 0xF004,
    NID_OPTION_TABS = 0xF006,
};

class NCOptionWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCOptionWnd, NCWnd, 0, 0x102EC7C0)

    UTexture* BackTex;                     // +0xE8 C3: "L2UI_ch3.SSQWnd.ssq_back" (the strip under the title bar)
    UTexture* OptionBackTex;               // +0xEC "L2ui_ch3.option1_back" (the page's backdrop; name ours)
    NCVideoOptionWnd* VideoWnd;            // +0xF0
    NCAudioOptionWnd* AudioWnd;            // +0xF4
    NCGameOptionWnd* GameWnd;              // +0xF8
    NCButton* OKButton;                    // +0xFC
    NCButton* CancelButton;                // +0x100
    NCButton* ApplyButton;                 // +0x104
    NCTabControl* TabCtrl;                 // +0x108
    NCWnd* UnusedB4;                       // +0x10C destroyed, never created   // C2: offset derived, no evidence
    INT bRestricted;                       // +0x110 (name ours) the constructor's argument: OnCreate disables the game page's
                                           //   tab while it is set (NCGodWnd makes the window with it)

    NCOptionWnd(INT InRestricted = 0);                               // 0x100cb4e0 (ret 4)
    // ~NCOptionWnd                                                  // 0x100a4420 (C1 only: no C2 match) (C1 only: no C2 match)

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x100c4aa0
    virtual INT OnCreate();                                          // 37 0x100cb590
    virtual INT OnDestroy();                                         // 38 0x100c8e30
    virtual void ShowWindow();                                       // 48 0x100c9980
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x100c8d00
    // names ours
    virtual INT OnOK();                                              // 74 0x100c9010
    virtual INT OnCancel();                                          // 75 0x10084ee0 (folded)
    virtual INT OnApply();                                           // 76 0x100c9070
};
NW_ASSERT_OFFSET(NCOptionWnd, BackTex, 0xE8);
NW_ASSERT_OFFSET(NCOptionWnd, OKButton, 0xFC);
NW_ASSERT_OFFSET(NCOptionWnd, TabCtrl, 0x108);
NW_ASSERT_SIZE(NCOptionWnd, 0x114);

#pragma pack(pop)