#pragma once
// NCBroadcastObserverWnd - the screen of the match broadcast, seen by the observer (UClass "CBroadcastObserverWnd",
// 0x6B8; C3 only; the desktop NCConsole +0x1B4, shown with the console's UI mode 1): the score bar at the top
// (NCBroadcastObserverTopWnd), the two teams' player windows (NCBroadcastObserverPlayerWnd, 520 x 224 at the sides)
// and party lists (NCBroadcastObserverPartyListWnd: nine rows of NCBroadcastObserverPartyWnd each) and the skill
// message log at the bottom (NCBroadcastObserverMsgWnd). The server's match packets reach it through the
// NConsoleWnd::Notify... virtuals (src/nwindow/NConsoleWndMisc.cpp calls these by address); it keeps the two
// teams' rosters (NBroadcastTeam, nine NBroadcastUser each) and marks the followed player in the 3D world with
// appendix visual effects of his pawn.
//
// Layout and virtuals from the stock constructor 0x10119860, OnCreate 0x101194F0 and the Notify... functions
// (0x10117BE0 .. 0x10119400). The window and its children share a few guard strings (copied between the classes).

#include "nwindow/NCWnd.h"
#pragma pack(push, 4)

class NCBroadcastObserverTopWnd;
class NCBroadcastObserverPlayerWnd;
class NCBroadcastObserverPartyListWnd;
class NCBroadcastObserverMsgWnd;

// One player of a team (0x50 bytes; the stock constructor is 0x1010FF10, the destructor 0x1010FFC0): the server's
// roster line (NotifyTeamInfo, updated by NotifyUserInfo) and the effects on him (NotifySpellInfo).
struct NBroadcastUser
{
    INT ID;                  // +0x00 the object id; -1 for a free slot
    FString Name;            // +0x04
    INT HP;                  // +0x10 (an HP of 0 draws the dead frame)
    INT MaxHP;               // +0x14
    INT MP;                  // +0x18
    INT MaxMP;               // +0x1C
    INT CP;                  // +0x20
    INT MaxCP;               // +0x24
    INT Level;               // +0x28 (names ours from here: "Lv%d %s" of the player window)
    INT ClassID;             // +0x2C (NConsoleWnd::GetClassNameString)
    INT Unknown30;           // +0x30 the ninth value of NotifyTeamInfo
    INT Unknown34;           // +0x34 a second copy of the first value (HP)
    TArray<INT> SpellIDs;    // +0x38 the skills working on him (NotifySpellInfo), cleared by NotifySpellInfoStart
    TArray<INT> SpellTimes;  // +0x44 their remaining seconds (the observer's timer counts them down)
};
NW_ASSERT_OFFSET(NBroadcastUser, Name, 0x04);
NW_ASSERT_OFFSET(NBroadcastUser, HP, 0x10);
NW_ASSERT_OFFSET(NBroadcastUser, ClassID, 0x2C);
NW_ASSERT_OFFSET(NBroadcastUser, SpellIDs, 0x38);
NW_ASSERT_OFFSET(NBroadcastUser, SpellTimes, 0x44);
NW_ASSERT_SIZE(NBroadcastUser, 0x50);

// One team (0x2E0 bytes; the stock constructor is 0x10110020, the destructor 0x10111DC0): its score and name, and nine
// player slots
struct NBroadcastTeam
{
    INT Score;               // +0x00
    FString Name;            // +0x04
    NBroadcastUser Users[9]; // +0x10
};
NW_ASSERT_OFFSET(NBroadcastTeam, Users, 0x10);
NW_ASSERT_SIZE(NBroadcastTeam, 0x2E0);

// The two teams of the window, built and destroyed by the stock functions (an `eh_vector_constructor_iterator` of the
// teams' constructor in the stock window's constructor; here the members' own)
struct NBroadcastTeams
{
    BYTE Raw[2 * 0x2E0];

    NBroadcastTeams();       // the stock constructor of each team, in order
    ~NBroadcastTeams();      // the stock destructor of each, in reverse
    NBroadcastTeam& operator[](INT Index) { return *(NBroadcastTeam*)(Raw + Index * 0x2E0); }
};
NW_ASSERT_SIZE(NBroadcastTeams, 0x5C0);

class NCBroadcastObserverWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCBroadcastObserverWnd, NCWnd, 0, 0x10314760)

    NCBroadcastObserverTopWnd* TopWnd;           // +0xE8 (names ours) 0x71 high from x = 60, the score bar
    NCBroadcastObserverPlayerWnd* PlayerWnd[2];  // +0xEC the player windows (style 0x400002: draggable), 520 wide
    NCBroadcastObserverPartyListWnd* PartyListWnd[2]; // +0xF4 the party lists (draggable), 280 wide
    NCBroadcastObserverMsgWnd* MsgWnd;           // +0xFC the skill message log at the bottom
    INT FollowTeamID;                            // +0x100 the team the server says is followed (NotifyStartObserver; -1)
    INT UpdateTeam;                              // +0x104 0 / 1 while a team's list is being sent (NotifyTeamInfo... End); -1
    INT SelectedSlot;                            // +0x108 the player whose spells are being sent (NotifySpellInfoStart); -1
    INT SelectedTeam;                            // +0x10C his team; -1
    INT PartyShowMode;                           // +0x110 1 the party rows show the class, 0 the name (1 from the constructor;
                                                 //   ToggleFlag 0x10118510, in NConsoleWndMisc.cpp, flips it)
    NBroadcastTeams Teams;                       // +0x114 the two rosters
    INT TimerID;                                 // +0x6D4 the one second timer (0xFEFAF001) that counts the spell times down

    NCBroadcastObserverWnd();                                        // 0x1012f5d0
    // ~NCBroadcastObserverWnd                                       // 0x1012f700

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10148f10 (NControl's body, folded)
    virtual INT OnTimer(INT TimerID);                                // 36 0x1012efd0
    virtual INT OnCreate();                                          // 37 0x1012f240
    virtual INT OnDestroy();                                         // 38 0x1012ee80
    virtual void SetWindowSize(FLOAT W, FLOAT H);                    // 47 0x1012e720 the top bar and the message log follow
    virtual void ShowWindow();                                       // 48 0x1012ec40
    virtual void HideWindow();                                       // 49 0x1012ed70
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x100f76f0 (folded: returns 1)

    // The match packets (guard strings; argument names ours)
    void NotifyStartObserver(INT TeamID, FString Team0Name, FString Team1Name); // 0x1012d960 (ret 0x1C) the rosters emptied
    void NotifyScore(INT TeamID, INT Score0, INT Score1);            // 0x1012da60 (ret 0xC) the scores, the top bar shown
    void SetTeamState(INT TeamID, INT State, INT Unused);            // 0x1012db00 (ret 0xC; name ours, no guard) the followed team's list state:
                                                                     //   1 sets UpdateTeam to 0, 2 to 1 (NConsoleWndMisc.cpp calls it by address)
    void NotifyTeamInfo(INT Slot, INT UserID, FString Name, INT P1, INT P2, INT P3, INT P4, INT P5, INT P6, INT P7,
                        INT P8, INT P9, INT P10);                    // 0x10117DC0 (ret 0x3C) one roster line
    void NotifyTeamInfoEnd();                                        // 0x1012dc40 the roster is complete: the pawns marked
    void NotifyUserInfo(INT UserID, FString Name, INT P1, INT P2, INT P3, INT P4, INT P5, INT P6, INT P7,
                        INT P8);                                     // 0x10118010 (ret 0x30; the loops are in the pieces 0x10118040 / 0x10118050)
    void NotifySpellInfoEnd();                                       // 0x1012dec0
    void NotifyTeamUnlocked(INT TeamID, INT Which);                  // 0x1012df60 (ret 8) a team's roster freed
    void ShowPartyWnd();                                             // 0x1012e0b0 the party lists shown / hidden
    void LockPosition();                                             // 0x1012e170 the draggable style of the children toggled
    void SwitchMsgMode();                                            // 0x1012e2b0
    void SetSelecteUser(INT Team, INT Slot);                         // 0x1012e330 (ret 8; the name is the stock's) the followed player
    INT GetSelectedUserIndex(INT Team);                              // 0x1012e4a0 (ret 4)
    FString GetTeamName(INT Team);                                   // 0x1012e570 (ret 8: the FString comes back by the hidden pointer)
    INT IsSelected(INT Team, INT Slot);                              // 0x1012e670 (ret 8)
    void NotifyCharInfo(User* Info);                                 // 0x1012e850 (ret 4) a character's record: his pawn is marked if he is on a roster
    void NotifyMagicSkillUse(INT CasterID, INT TargetID, INT SkillID); // 0x1012e930 (ret 0xC) a line in the message log
    void SetUpTarget();                                              // 0x1012ea60 the console's target becomes the followed player
    void InitPosition();                                             // 0x1012eb10 the player windows and party lists back at the sides
    void NotifySpellInfoStart(INT TeamID, INT UserID, INT Unused);   // 0x1012f030 (ret 0xC; the pieces 0x101192E0 + 0x10119328)
    void NotifySpellInfo(INT Unused1, INT SpellID, INT Unused2, INT RemainTime); // 0x1012f150 (ret 0x10)

    // anonymous functions of the class (names ours)
    INT GetTeamScore(INT Team);                                      // 0x1012e540 (ret 4) 0 for a team out of range
    NBroadcastUser* GetUser(INT Team, INT Slot);                     // 0x1012e620 (ret 8) NULL for a free slot
    NBroadcastUser* FindUser(INT UserID, INT* OutTeam, INT* OutSlot); // 0x1012e7e0 (ret 0xC) the roster line of an object id
};
NW_ASSERT_OFFSET(NCBroadcastObserverWnd, TopWnd, 0xE8);
NW_ASSERT_OFFSET(NCBroadcastObserverWnd, PlayerWnd, 0xEC);
NW_ASSERT_OFFSET(NCBroadcastObserverWnd, PartyListWnd, 0xF4);
NW_ASSERT_OFFSET(NCBroadcastObserverWnd, MsgWnd, 0xFC);
NW_ASSERT_OFFSET(NCBroadcastObserverWnd, FollowTeamID, 0x100);
NW_ASSERT_OFFSET(NCBroadcastObserverWnd, UpdateTeam, 0x104);
NW_ASSERT_OFFSET(NCBroadcastObserverWnd, SelectedSlot, 0x108);
NW_ASSERT_OFFSET(NCBroadcastObserverWnd, SelectedTeam, 0x10C);
NW_ASSERT_OFFSET(NCBroadcastObserverWnd, PartyShowMode, 0x110);
NW_ASSERT_OFFSET(NCBroadcastObserverWnd, Teams, 0x114);
NW_ASSERT_OFFSET(NCBroadcastObserverWnd, TimerID, 0x6D4);
NW_ASSERT_SIZE(NCBroadcastObserverWnd, 0x6D8);

#pragma pack(pop)
