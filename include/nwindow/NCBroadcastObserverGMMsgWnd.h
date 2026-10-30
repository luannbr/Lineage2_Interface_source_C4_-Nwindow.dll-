#pragma once
// NCBroadcastObserverGMMsgWnd - the GM's message line of the broadcast observer screen (UClass
// "CBroadcastObserverGMMsgWnd", 0xE4; C3 only): a three-piece banner (left end, stretched middle, right end of
// 32 x 128) with the text centered on it, shown for five seconds. SetMsg gives it the text, ShowWindow starts the
// timer that hides it again. Layout and virtuals from the stock constructor 0x10113BE0, OnCreate 0x101137F0,
// OnDestroy 0x101138D0, ShowWindow 0x101136B0, SetMsg 0x10113760, OnTimer 0x10113950 and OnPaint 0x101139F0.

#include "nwindow/NCWnd.h"
#pragma pack(push, 4)

class NCBroadcastObserverGMMsgWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCBroadcastObserverGMMsgWnd, NCWnd, 0, 0x103122E8)

    UTexture* PieceTex[3];           // +0xE8 "L2UI_CH3.BroadcastObs.br_msg2_back1" .. "3": left end, middle, right end (names ours)
    FString Msg;                     // +0xF4 the text (SetMsg); OnCreate starts it as SystemMsg 1431
    INT TimerID;                     // +0x100 the five second timer (0xB0000, ShowWindow) that hides the banner

    NCBroadcastObserverGMMsgWnd();                                   // 0x101299b0
    // ~NCBroadcastObserverGMMsgWnd                                  // 0x10129aa0

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10148f10 (NControl's body, folded)
    virtual INT OnTimer(INT TimerID);                                // 36 0x10129730
    virtual INT OnCreate();                                          // 37 0x101295d0
    virtual INT OnDestroy();                                         // 38 0x101296b0
    virtual void ShowWindow();                                       // 48 0x10129490 NCWnd's, then the five second timer
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x101297d0

    void SetMsg(FString NewMsg);                                     // 0x10129540 (ret 0xC; guard: SetMsg) the text
};
NW_ASSERT_OFFSET(NCBroadcastObserverGMMsgWnd, PieceTex, 0xE8);
NW_ASSERT_OFFSET(NCBroadcastObserverGMMsgWnd, Msg, 0xF4);
NW_ASSERT_OFFSET(NCBroadcastObserverGMMsgWnd, TimerID, 0x100);
NW_ASSERT_SIZE(NCBroadcastObserverGMMsgWnd, 0x104);

#pragma pack(pop)
