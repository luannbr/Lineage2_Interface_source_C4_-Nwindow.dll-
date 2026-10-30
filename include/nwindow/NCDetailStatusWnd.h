#pragma once
// NCDetailStatusWnd - the character tab of the status window (UClass "CDetailStatusWnd",
// 0xD4; Alt+T): title and name, clan, level and class, HP / MP / exp / weight bars, the combat
// values, the six stats, karma and PvP, all read from the player's User at each paint.
// NCGMDetailStatusWnd derives from it.
//
// NCMainWnd +0x94 is this window (tab 0, created at 0, 65, 240 x 300).

#include "nwindow/NCWnd.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCDetailStatusWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCDetailStatusWnd, NCWnd, 0, 0x1030BAD0)

    User* Player;                    // +0xE8 the player (each paint)   // C2: offset derived, no evidence
    UTexture* BackTex;               // +0xEC "L2UI_CH3.PlayerStatusWnd.myinfo_back"
    UTexture* HPTex;                 // +0xF0 "L2UI_CH3.PlayerStatusWnd.ps_hpbar"   // C2: offset derived, no evidence
    UTexture* MPTex;                 // +0xF4 "L2UI_CH3.PlayerStatusWnd.ps_mpbar"   // C2: offset derived, no evidence
    UTexture* EXPTex;                // +0xF8 "L2UI_CH3.PlayerStatusWnd.ps_expbar"   // C2: offset derived, no evidence
    BYTE _UnknownDC[0xE0 - 0xDC];    // +0xFC not set by C3's OnCreate (C2: SPTex)   // C2: offset derived, no evidence
    UTexture* WeightTex[5];          // +0x100 "L2UI_CH3.PlayerStatusWnd.ps_weightBar1" .. 5: up to 50%, 66.6%, 80%, 100% of   // C2: offset derived, no evidence
                                     //   the weight limit, above (C2 had four at +0xE0 and the name end pieces after them)
    BYTE _UnknownF4[0xFC - 0xF4];    // +0x114 (C2: NameRightTex, CrestTex; C3 does not touch them in this class)   // C2: offset derived, no evidence
    UTexture* CPTex;                 // +0x11C "L2UI_CH3.PlayerStatusWnd.ps_cpbar" (new in C3)   // C2: offset derived, no evidence
    INT _Unknown120;                 // +0x120 (not touched by C4's code either)
    UTexture* HeroTex;               // +0x124 "L2UI_CH3.PlayerStatusWnd.myinfo_heroicon" (new in C4; OnCreate, OnPaint)
    UTexture* NobleTex;              // +0x128 "L2UI_CH3.PlayerStatusWnd.myinfo_nobleicon" (new in C4)
    INT UnknownCC[17];               // +0x12C .. +0x144 cleared by the constructor (no other stock code touches them) except for
                                     //   six that OnPaint reads as the stats' bonuses, shown "N(+B)": see BONUS_* (names ours;
                                     //   the enum's indexes are the C2 ones, (offset - 0xCC) / 4, and still index this array)
    enum { BONUS_Int = (0xE4 - 0xCC) / 4, BONUS_Str = (0xEC - 0xCC) / 4, BONUS_Con = (0xF4 - 0xCC) / 4,
           BONUS_Men = (0xFC - 0xCC) / 4, BONUS_Dex = (0x104 - 0xCC) / 4, BONUS_Wit = (0x10C - 0xCC) / 4 };
    BYTE _Unknown148[0x154 - 0x148]; // +0x170 (up to sizeof)   // C2: offset derived, no evidence

    NCDetailStatusWnd();                                             // 0x10117850
    // ~NCDetailStatusWnd                                            // 0x10073b50 (C1 only: no C2 match) (C1 only: no C2 match)

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10148f10 (folded)
    virtual INT OnCreate();                                          // 37 0x1010def0
    virtual INT OnDestroy();                                         // 38 0x1010dff0 (folded)
    virtual void ShowWindow();                                       // 48 0x100efc40 (folded)
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x10110c90
};
NW_ASSERT_OFFSET(NCDetailStatusWnd, Player, 0xE8);
NW_ASSERT_OFFSET(NCDetailStatusWnd, WeightTex, 0x100);
NW_ASSERT_OFFSET(NCDetailStatusWnd, CPTex, 0x11C);
NW_ASSERT_OFFSET(NCDetailStatusWnd, HeroTex, 0x124);
NW_ASSERT_OFFSET(NCDetailStatusWnd, NobleTex, 0x128);
NW_ASSERT_OFFSET(NCDetailStatusWnd, UnknownCC, 0x12C);
NW_ASSERT_SIZE(NCDetailStatusWnd, 0x17C);

// NCGMDetailStatusWnd - a GM's view of another player's status (UClass "CGMDetailStatusWnd",
// 0x2DC): the character tab under a title bar, laid out a little lower, drawn from the User it
// keeps (the console fills it with the server's answer). HP and MP in the common font with a
// shadow, exp in percent at the right, a white name (no karma color) with the title before it
// in one string, the crest looked up by its texture name.
//
// Desktop (NCConsole) +0x120 is this window.
class NCGMDetailStatusWnd : public NCDetailStatusWnd
{
    NW_DECLARE_CLASS(NCGMDetailStatusWnd, NCDetailStatusWnd, 0, 0x102D7F68)

    User Target;                     // +0x17C (0x1F8 in C2) the GM's answer; Engine's constructor builds it

    NCGMDetailStatusWnd();                                           // 0x10090230
    // ~NCGMDetailStatusWnd                                          // 0x1007e8d0 (C1 only: no C2 match)

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10021e60 (folded)
    virtual INT OnCreate();                                          // 37 0x1008d9a0
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x1008abc0

    void SetTarget(User InTarget);                                   // 0x100891d0 (name ours) the
                                                                     //   console's, by value
};
NW_ASSERT_OFFSET(NCGMDetailStatusWnd, Target, 0x17C);
NW_ASSERT_SIZE(NCGMDetailStatusWnd, 0x3C4);

// NWM_COMMAND id of the GM window's title bar
enum { NID_GMDETAIL_FRAME = 0xF0F118 };

#pragma pack(pop)