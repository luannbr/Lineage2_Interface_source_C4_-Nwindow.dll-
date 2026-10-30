#pragma once
// NCBroadcastObserverMsgWnd - the skill message log at the bottom of the broadcast observer screen (UClass
// "CBroadcastObserverMsgWnd", 0x48C; C3 only; NCBroadcastObserverWnd +0xDC): the last six lines of "who used which
// skill on whom", in three views (all lines, the first team's, the second team's). The lines die one by one ten
// seconds after the last new one (a one second timer), and a shortcut cycles the views (Mode).
// Layout and virtuals from the stock constructor 0x101149E0, OnCreate 0x101140D0, OnDestroy 0x10114160, OnPaint
// 0x101146C0, OnTimer 0x101148E0, AddSkillMsg 0x10113DB0, DrawSkillText 0x101141E0 and the anonymous helpers
// 0x10113D50 .. 0x10114660.

#include "nwindow/NCWnd.h"
#pragma pack(push, 4)

// One line of the log (0x34 bytes; the constructor is 0x10113D50, the destructor 0x10114660): the caster, the target and
// the skill, and the positions of the two in the rosters. A line is free when CasterTeam is -1.
struct NBroadcastMsgLine
{
    FString Caster;          // +0x00 (names ours) the caster's name
    FString Target;          // +0x0C the target's name
    FString Skill;           // +0x18 the skill's name
    INT CasterTeam;          // +0x24 0 / 1 (the color of the caster's number); -1 for a free line
    INT TargetTeam;          // +0x28 0 / 1 (the color of the target's number)
    INT CasterSlot;          // +0x2C the caster's roster slot (drawn +1)
    INT TargetSlot;          // +0x30 the target's roster slot (drawn +1)

    NBroadcastMsgLine();     // 0x10113D50 the three strings constructed; the numbers are left (the window sets them)
    ~NBroadcastMsgLine();    // 0x10114660
    void Clear();            // 0x10114620 (anonymous, name ours) the strings emptied, the numbers -1
};
NW_ASSERT_OFFSET(NBroadcastMsgLine, Target, 0x0C);
NW_ASSERT_OFFSET(NBroadcastMsgLine, Skill, 0x18);
NW_ASSERT_OFFSET(NBroadcastMsgLine, CasterTeam, 0x24);
NW_ASSERT_OFFSET(NBroadcastMsgLine, TargetSlot, 0x30);
NW_ASSERT_SIZE(NBroadcastMsgLine, 0x34);

class NCBroadcastObserverMsgWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCBroadcastObserverMsgWnd, NCWnd, 0, 0x10312820)

    UTexture* BackTex;                    // +0xE8 "L2UI_CH3.BroadcastObs.br_msg3_back" (32 x 256, stretched)
    INT NextAll;                          // +0xEC (names ours) the ring index the next line of each list goes to
    INT NextTeam0;                        // +0xF0
    INT NextTeam1;                        // +0xF4
    INT TimerID;                          // +0xF8 the one second timer (0xFEFFF001)
    NBroadcastMsgLine AllLines[6];        // +0xFC every line (Mode 0)
    NBroadcastMsgLine Team0Lines[6];      // +0x234 the first team's casters' lines (Mode 1, left column)
    NBroadcastMsgLine Team1Lines[6];      // +0x36C the second team's (Mode 1, right column)
    INT Countdown;                        // +0x4A4 seconds until the oldest line goes (-1: nothing to count)
    INT Mode;                             // +0x4A8 0 all lines, 1 the two teams side by side, 2 nothing

    NCBroadcastObserverMsgWnd();                                     // 0x1012a7b0
    // ~NCBroadcastObserverMsgWnd                                    // 0x1012a9e0

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10148f10 (NControl's body, folded)
    virtual INT OnTimer(INT TimerID);                                // 36 0x1012a6b0
    virtual INT OnCreate();                                          // 37 0x10129ea0 (guard: NCBroadcastObserverWnd::OnCreate)
    virtual INT OnDestroy();                                         // 38 0x10129f30 (guard: NCBroadcastObserverWnd::OnDestroy)
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x1012a490 (guard: NCBroadcastObserverWnd::OnPaint)

    // A new line (ret 0x34; the FStrings come by value): into the all-lines ring and, by CasterTeam, the team's
    void AddSkillMsg(FString Caster, INT CasterTeam, INT CasterSlot, FString Target, INT TargetTeam, INT TargetSlot,
                     FString Skill);                                 // 0x10113DB0
    // One line at (X, Y) (ret 0x14): "<slot>>" (with the caster's name after it when bNames) in the caster team's color
    // (red for 0, white for 1), the same for the target, ": " and the skill in yellow, all in font 6
    void DrawSkillText(NBroadcastMsgLine* Line, INT X, INT Y, INT bNames, NCGDevice* Device); // 0x10129fb0
    void SwitchMode();                                               // 0x10129e50 (anonymous, name ours) 0 -> 1 -> 2 -> 0
    NBroadcastMsgLine* FindOldestLine(INT Which);                    // 0x1012a340 (anonymous, name ours; ret 4) the oldest
                                                                     //   used line of the list (-1 all, 0 / 1 a team's)
};
NW_ASSERT_OFFSET(NCBroadcastObserverMsgWnd, BackTex, 0xE8);
NW_ASSERT_OFFSET(NCBroadcastObserverMsgWnd, TimerID, 0xF8);
NW_ASSERT_OFFSET(NCBroadcastObserverMsgWnd, AllLines, 0xFC);
NW_ASSERT_OFFSET(NCBroadcastObserverMsgWnd, Team0Lines, 0x234);
NW_ASSERT_OFFSET(NCBroadcastObserverMsgWnd, Team1Lines, 0x36C);
NW_ASSERT_OFFSET(NCBroadcastObserverMsgWnd, Countdown, 0x4A4);
NW_ASSERT_OFFSET(NCBroadcastObserverMsgWnd, Mode, 0x4A8);
NW_ASSERT_SIZE(NCBroadcastObserverMsgWnd, 0x4AC);

#pragma pack(pop)
