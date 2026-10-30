#pragma once
// NCSSQMainBoard - the Seven Signs window (UClass "CSSQMainBoard", 0xE0; new in C3): three tabs (the status, the event
// list and the seal list: NCSSQStatusWnd, NCSSQMainEventWnd, NCSSQSealStatusListWnd, each 256 x 353 under a 20 pixel
// title bar) on the "SSQ_back" panel. The first visit of the event and seal tabs asks the console for their lists
// (once, until the window is hidden).

#include "nwindow/NCWnd.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCSSQMainEventWnd;
class NCSSQSealStatusListWnd;
class NCSSQStatusWnd;
class NCTabControl;

class NCSSQMainBoard : public NCWnd
{
    NW_DECLARE_CLASS(NCSSQMainBoard, NCWnd, 0, 0x10302E70)

    UTexture* BackTex;               // +0xE8 "L2UI_CH3.SSQWnd.SSQ_back"
    NCTabControl* TabCtrl;           // +0xEC id 0xFFFA0FF (names ours)
    NCSSQStatusWnd* StatusWnd;       // +0xF0
    NCSSQMainEventWnd* EventWnd;     // +0xF4
    NCSSQSealStatusListWnd* SealWnd; // +0xF8
    BYTE bEventAsked;                // +0xFC the event list was asked for (name ours)
    BYTE bSealAsked;                 // +0xFD the seal list was asked for (name ours)

    NCSSQMainBoard();                                                // 0x100fc6c0
    // ~NCSSQMainBoard                                               // 0x100fc710

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x100fb800
    virtual INT OnCreate();                                          // 37 0x100fc760
    virtual INT OnDestroy();                                         // 38 0x100fbec0 (db: NCMainWnd::OnDestroy)
    virtual void ShowWindow();                                       // 48 0x100fb860
    virtual void HideWindow();                                       // 49 0x100fb890
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x100fbff0
    virtual INT OnTab(INT Index);                                    // 74 0x100fb8d0 (name ours)
};
NW_ASSERT_OFFSET(NCSSQMainBoard, TabCtrl, 0xEC);
NW_ASSERT_OFFSET(NCSSQMainBoard, SealWnd, 0xF8);
NW_ASSERT_OFFSET(NCSSQMainBoard, bEventAsked, 0xFC);
NW_ASSERT_SIZE(NCSSQMainBoard, 0x100);

#pragma pack(pop)
