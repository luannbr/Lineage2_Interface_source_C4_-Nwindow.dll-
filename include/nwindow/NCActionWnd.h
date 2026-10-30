#pragma once
// NCActionWnd - the action window (UClass "CActionWnd", 0xA8; a page of the character window,
// desktop +0x94 +0x98): tabs Basic (the action icons, NCActionItemListWnd) and Clan
// (NCPledgeWnd). The Clan tab is disabled without a clan; shown with one, the window asks the
// server for the member list.

#include "nwindow/NCWnd.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCActionItemListWnd;
class NCPledgeWnd;
class NCTabControl;

enum { NID_ACTION_TABS = 0xF00A };

class NCActionWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCActionWnd, NCWnd, 0, 0x102CDD10)

    UTexture* BackTex;                     // +0xE8 "sek.cbui304" (loaded, never drawn)   // C2: offset derived, no evidence
    NCActionItemListWnd* ActionList;       // +0xEC
    NCPledgeWnd* PledgeWnd;                // +0xF0
    // C2: not placed (no evidence for where it went): NCTabControl* TabCtrl;                 // +0xA4
    // C2: not placed (no evidence for where it went): NCWnd* UnusedA4;                       // +0xA8 destroyed, never created   // C2: offset derived, no evidence

    NCActionWnd();                                                   // 0x10050990
    // ~NCActionWnd                                                  // 0x10042b60 (C1 only: no C2 match) (C1 only: no C2 match)

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10148f10 (folded)
    virtual INT OnCreate();                                          // 37 0x10050a30
    virtual INT OnDestroy();                                         // 38 0x1004eac0 (folded)
    virtual void ShowWindow();                                       // 48 0x10050c80
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x1004ea20

    // names ours
    void ResetPledgeWnd();                                           // 0x10036870 (C1 only: no C2 match) (C1 only: no C2 match) (C1 only: no C2 match) (the console)
    void ReloadActions();                                            // 0x10043e90 (C1 only: no C2 match) texts reloaded
};
NW_ASSERT_OFFSET(NCActionWnd, BackTex, 0xE8);
// NW_ASSERT_OFFSET(NCActionWnd, TabCtrl, 0xA4);   // C2: field not mapped
NW_ASSERT_SIZE(NCActionWnd, 0xF4);

#pragma pack(pop)