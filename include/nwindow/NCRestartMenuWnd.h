#pragma once
// NCRestartMenuWnd - the menu that opens when the character dies (UClass "CRestartMenuWnd",
// 0xC0): a button per place to restart at (nearest village, hideaway, castle, siege
// headquarters, fixed). The server says which of them apply (SetRestartPoints); the others
// hide when the menu shows. A click sends the choice (RequestRestartPoint) and closes it.

#include "nwindow/NCWnd.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCButton;

// Buttons[] and what the server gets for each (names ours)
enum
{
    RESTART_Village,
    RESTART_Hideaway,
    RESTART_Castle,
    RESTART_SiegeHQ,
    RESTART_Fixed,
    RESTART_MAX
};

// NWM_COMMAND ids of the buttons
enum
{
    NID_RESTART_VILLAGE = 0xF0F012,
    NID_RESTART_HIDEAWAY = 0xF0F013,
    NID_RESTART_CASTLE = 0xF0F014,
    NID_RESTART_SIEGEHQ = 0xF0F015,
    NID_RESTART_FIXED = 0xF0F020,
};

class NCRestartMenuWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCRestartMenuWnd, NCWnd, 0, 0x102E7900)

    UTexture* BackTex;                     // +0xE8 "L2UI.EtcWnd.RestartBack"
    NCButton* Buttons[RESTART_MAX];        // +0xEC RESTART_*
    UBOOL bAvailable[RESTART_MAX];         // +0x100 (1) from the server (SetRestartPoints)
    UBOOL bPointsSet;                      // +0x114 (name ours, C4) SetRestartPoints sets it, a click clears it (the ctor starts it clear)

    NCRestartMenuWnd();                                              // 0x100b81d0
    // ~NCRestartMenuWnd                                             // 0x100971b0 (C1 only: no C2 match) (C1 only: no C2 match)

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x100b6120
    virtual INT OnCreate();                                          // 37 0x100b7490
    virtual INT OnDestroy();                                         // 38 0x100b6b40
    virtual void ShowWindow();                                       // 48 0x100b6c90
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x100b6aa0
    // names ours
    virtual INT OnVillage();                                         // 74 0x100b62c0
    virtual INT OnHideaway();                                        // 75 0x100b6350
    virtual INT OnCastle();                                          // 76 0x100b63e0
    virtual INT OnSiegeHQ();                                         // 77 0x100b6470
    virtual INT OnFixed();                                           // 78 0x100b6500

    // names ours
    void HideUnavailable();                                          // 0x100b6200
    void SetRestartPoints(UBOOL bVillage, UBOOL bHideaway, UBOOL bCastle, UBOOL bSiegeHQ,
                          UBOOL bFixed);                             // 0x10076930 (the console,
                                                                     //   when the player dies)
};
NW_ASSERT_OFFSET(NCRestartMenuWnd, BackTex, 0xE8);
NW_ASSERT_OFFSET(NCRestartMenuWnd, Buttons, 0xEC);
NW_ASSERT_OFFSET(NCRestartMenuWnd, bAvailable, 0x100);
NW_ASSERT_SIZE(NCRestartMenuWnd, 0x118);

#pragma pack(pop)