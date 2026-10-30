#pragma once
// NCMapCoverWnd - the world map window (UClass "CMapCoverWnd", 0x108; new in C3; opened by NCConsole's map button):
// the frame around an NCMinimapWnd (the map itself, 328 x 328 pixels under a title bar) with a button that shows the
// mark the console reports, a button that centers the map on the player and (C4) a button that walks through the
// positions of the party members (the combo box that picked the continent in C3 is gone). The window is created
// minimized into an icon like the others of the main window (NCWnd::Minimize with the menu icon 0x1BF).

#include "nwindow/NCWnd.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCButton;
class NCComboBox;
class NCMinimapWnd;

class NCMapCoverWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCMapCoverWnd, NCWnd, 0, 0x102E8DD0)

    NCMinimapWnd* Map;               // +0xE8 the map (child, 3, 21)
    UTexture* BackTex;               // +0xEC "L2UI_CH3.Minimap.MapBack" (the constructor clears it, 0x14 pixels down)
    INT PartyLocShown;               // +0xF0 (-1) the party position the last click showed (name ours); OnTimer puts it back to -1
    NCButton* MarkButton;            // +0xF4 id 0xB000F5, SysString 886 (name ours): shows the console's mark
    NCButton* MyPosButton;           // +0xF8 id 0xB00F00, SysString 887 (name ours): the player's place
    NCButton* PartyLocButton;        // +0xFC id 0xB00F01, SysString 1223 (C4, name ours): the next party member's place
    INT PartyLocNext;                // +0x100 (-1) the index the next click starts from (name ours; OnPartyLocBtnClick)
    INT PartyLocTimer;               // +0x104 the timer 0xB01000 set by a click (2000 ms), 0 when none (name ours)

    NCMapCoverWnd();                                                 // 0x100bbd80
    // ~NCMapCoverWnd                                                // 0x100bbde0

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x100b87e0
    virtual void KillFocus();                                        // 30 0x100b9010 (NCMinimapWnd's body, folded: the
                                                                     //   cursor lock goes, then NCWnd's)
    virtual INT OnCreate();                                          // 37 0x100bd420
    virtual INT OnDestroy();                                         // 38 0x100b91c0
    virtual void ShowWindow();                                       // 48 0x100bba20
    virtual void Minimize();                                         // 54 0x100b8880 (the db says MinimizeWindow)
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x100b9120
    // names ours: the three controls' commands
    virtual INT OnShowMark();                                        // 74 0x100bb9e0 (0xB000F5)
    virtual INT OnShowMyPosition();                                  // 75 0x100bb8f0 (0xB00F00)
    virtual INT OnPartyLocBtnClick();                                // 76 0x100bcdb0 (0xB00F01; C4)
    virtual INT OnTimer(INT TimerID) ;  // 36 0x100b88e0 (the stock vtable of this class overrides it; defined in NNewVirtuals.gen.cpp)
};
NW_ASSERT_OFFSET(NCMapCoverWnd, Map, 0xE8);
NW_ASSERT_OFFSET(NCMapCoverWnd, BackTex, 0xEC);
NW_ASSERT_OFFSET(NCMapCoverWnd, PartyLocShown, 0xF0);
NW_ASSERT_OFFSET(NCMapCoverWnd, PartyLocButton, 0xFC);
NW_ASSERT_OFFSET(NCMapCoverWnd, PartyLocNext, 0x100);
NW_ASSERT_OFFSET(NCMapCoverWnd, PartyLocTimer, 0x104);
NW_ASSERT_SIZE(NCMapCoverWnd, 0x108);

#pragma pack(pop)
