#pragma once
// NCBroadcastGMWnd - the GM's control window of the broadcast ("event match") tools (UClass "CBroadcastGMWnd", 0x738; new in C3: the
// desktop NCConsole makes it at +0x1AC, NConsoleWnd shows it and feeds it the server's replies). A GM sets two parties (their
// names, leaders and locks) up, creates a game, starts / pauses / ends it, sets the score, sends announcements and
// fireworks, opens the fence window (NCBroadcastGMFenceWnd) and a page of command links read from an option file; every
// button sends one "eventmatch ..." command through the console's virtual 192 (0x1005EEB0: a command line as an FString).
// The server answers with the Notify* calls (NConsoleWnd::OnEventMatch*), which enable / disable the buttons and fill the
// two team lists.
//
// Layout and virtuals from the stock constructor 0x10112820, destructor 0x101129C0, OnCreate 0x10110080, OnDestroy 0x1010E6C0,
// OnPaint 0x1010EAB0, OnTimer 0x1010EED0 and the handlers 0x1010CB70 .. 0x101120C0 (names ours unless a guard string says it).

#include "nwindow/NCWnd.h"
#pragma pack(push, 4)

class NCButton;
class NCEditBox;
class NCListCtrl;
class L2ParamStack;

// A pair of numbers read from the option file (names ours). A buff is (BuffID, BuffLv). A skill exception is stored as
// (the last BuffID read, ExpSkillID): the stock read leaves the first word stale and the readers (the "A%d" / "D%d" lists
// of OnStartBroadcastGameBtnClick) use the second one
struct NBroadcastGMPair
{
    INT First;                       // +0x00
    INT Second;                      // +0x04
};
NW_ASSERT_SIZE(NBroadcastGMPair, 0x8);

// A player of a team as the server lists it (0x50 bytes; the constructor 0x1010FF10 and the destructor 0x1010FFC0 are the
// stock out-of-line copies, hooked in NCBroadcastGMWnd.cpp). Filled by NotifyTeamInfo, shown by NotifyTeamInfoEnd.
struct NBroadcastGMMember
{
    INT ID;                          // +0x00 object id the server gave (-1: the slot is free)
    FString Name;                    // +0x04
    INT Value10;                     // +0x10 the first number of the packet (also copied to +0x34)
    INT Value14;                     // +0x14 .. +0x30: the next eight numbers of the packet
    INT Value18;                     // +0x18
    INT Value1C;                     // +0x1C
    INT Value20;                     // +0x20
    INT Value24;                     // +0x24
    INT Level;                       // +0x28 shown in the list's second cell (name ours)
    INT ClassID;                     // +0x2C shown as the class name (NConsoleWnd::GetClassNameString) (name ours)
    INT Value30;                     // +0x30
    INT Value34;                     // +0x34 a copy of Value10
    TArray<INT> ListA;               // +0x38 emptied by the constructor, nothing in this window's code fills it
    TArray<INT> ListB;               // +0x44

    NBroadcastGMMember();            // 0x1010FF10
    ~NBroadcastGMMember();           // 0x1010FFC0
};
NW_ASSERT_OFFSET(NBroadcastGMMember, Name, 0x4);
NW_ASSERT_OFFSET(NBroadcastGMMember, Level, 0x28);
NW_ASSERT_OFFSET(NBroadcastGMMember, ListA, 0x38);
NW_ASSERT_OFFSET(NBroadcastGMMember, ListB, 0x44);
NW_ASSERT_SIZE(NBroadcastGMMember, 0x50);

// A team (0x2E0 bytes; constructor 0x10110020 and destructor 0x10111DC0): up to nine players
struct NBroadcastGMTeam
{
    INT Unknown00;                   // +0x00 never touched
    FString Name;                    // +0x04
    NBroadcastGMMember Members[9];   // +0x10

    NBroadcastGMTeam();              // 0x10110020
    ~NBroadcastGMTeam();             // 0x10111DC0
};
NW_ASSERT_OFFSET(NBroadcastGMTeam, Members, 0x10);
NW_ASSERT_SIZE(NBroadcastGMTeam, 0x2E0);

// What the option file lists (ReadBroadcastOptionFile): the teleport delay, which skills / items the GM may use ("DefaultAllow"
// with the exceptions), and the buffs to cast when a game starts. 0x30 bytes (its inline destructor is 0x10111D60); the three
// numbers are left alone by the constructor, the read sets them.
struct NBroadcastGMOption
{
    INT TeleportDelay;               // +0x00 "Etc" / "TeleportDelay"
    INT SkillDefaultAllow;           // +0x04 "Skill" / "DefaultAllow"
    INT ItemDefaultAllow;            // +0x08 "Item" / "DefaultAllow"
    TArray<NBroadcastGMPair> ExpSkills;   // +0x0C the exceptions to the skill default
    TArray<INT> ExpItems;            // +0x18 the exceptions to the item default
    TArray<NBroadcastGMPair> Buffs;  // +0x24 "Buff" / "BuffID%d", "BuffLv%d"
};
NW_ASSERT_OFFSET(NBroadcastGMOption, ExpSkills, 0xC);
NW_ASSERT_OFFSET(NBroadcastGMOption, ExpItems, 0x18);
NW_ASSERT_OFFSET(NBroadcastGMOption, Buffs, 0x24);
NW_ASSERT_SIZE(NBroadcastGMOption, 0x30);

class NCBroadcastGMWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCBroadcastGMWnd, NCWnd, 0, 0x10311880)

    UTexture* BackTex;               // +0xE8 "L2UI_CH3.Minimap.MapBack" (OnCreate)
    NCEditBox* PartyAEdit;           // +0xEC the party A name (id 0xFFFFE001) (names ours for every control)
    NCEditBox* PartyBEdit;           // +0xF0 the party B name (0xFFFFE002)
    NCEditBox* ScoreAEdit;           // +0xF4 the score of A, one digit (0xFFFFE009)
    NCEditBox* ScoreBEdit;           // +0xF8 the score of B (0xFFFFE00A)
    NCEditBox* OptionFileEdit;       // +0xFC the option file to read when the game starts (0xFFFFE003)
    NCEditBox* CommandFileEdit;      // +0x100 the command file the command page lists (0xFFFFE004)
    NCEditBox* LeaderAEdit;          // +0x104 the party A leader's name (0xFFFFE005)
    NCEditBox* LeaderBEdit;          // +0x108 the party B leader's name (0xFFFFE006)
    NCEditBox* AnnounceEdit;         // +0x10C the announcement text, 50 characters (0xFFFFE008; C3: 25)
    NCButton* CommandButton;         // +0x110 id 0xFFFFD003 shows the command page
    NCButton* SetLeaderAButton;      // +0x114 0xFFFFD004
    NCButton* LockAButton;           // +0x118 0xFFFFD005
    NCButton* SetLeaderBButton;      // +0x11C 0xFFFFD006
    NCButton* LockBButton;           // +0x120 0xFFFFD007
    NCButton* AnnounceButton;        // +0x124 0xFFFFD009
    NCButton* StartButton;           // +0x128 0xFFFFD00A
    NCButton* CreateButton;          // +0x12C 0xFFFFD00B creates / removes the game
    NCButton* PauseButton;           // +0x130 0xFFFFD00C pauses / resumes
    NCButton* CounterButton;         // +0x134 0xFFFFD00D sets the score
    NCButton* EndMsgButton;          // +0x138 0xFFFFD00E sends the "game ended" announcement
    NCButton* FenceButton;           // +0x13C 0xFFFFD00F opens the fence window / removes the fence
    NCButton* FireworkAButton;       // +0x140 0xFFFFD010 fireworks for the party A
    NCButton* FireworkBButton;       // +0x144 0xFFFFD011
    NCListCtrl* TeamAList;           // +0x148 id 0xFFFFC001 (the constructor leaves both pointers alone)
    NCListCtrl* TeamBList;           // +0x14C 0xFFFFC002
    FString GameID;                  // +0x150 the id of the created game as text ("" without one)
    NBroadcastGMOption Option;       // +0x15C the option file's lists
    NBroadcastGMTeam Teams[2];       // +0x18C the two teams (A, B)
    INT TeamFillIndex;               // +0x74C the team NotifyTeamInfo fills (0 A, 1 B, -1 none)
    INT CountDownStep;               // +0x750 the countdown's step (4 .. 0; -1 none)
    INT CountDownTimer;              // +0x754 the timer id of the countdown (0: none)

    NCBroadcastGMWnd();                                              // 0x101285f0
    // ~NCBroadcastGMWnd                                             // 0x10128790

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10128430
    virtual INT OnTimer(INT TimerID);                                // 36 0x10124e10 the countdown (0xFFFFA001)
    virtual INT OnCreate();                                          // 37 0x10125ea0
    virtual INT OnDestroy();                                         // 38 0x10124530
    virtual void Minimize();                                         // 54 0x101243b0 (the db says MinimizeWindow)
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x101249f0

    // the buttons (WindowProc: NWM_COMMAND with the id)
    INT OnShowBroadcastCommandWndBtnClick();                         // 0x10127c00
    INT OnSetBroadcastPartyALeaderBtnClick();                        // 0x10122ab0
    INT OnLockBroadcastPartyABtnClick();                             // 0x10125190
    INT OnSetBroadcastPartyBLeaderBtnClick();                        // 0x10122b50
    INT OnLockBroadcastPartyBBtnClick();                             // 0x101253b0
    INT OnSendBroadcastAnnounceBtnClick();                           // 0x101255d0
    INT OnStartBroadcastGameBtnClick();                              // 0x10127ea0
    INT OnCreateBroadcastGameBtnClick();                             // 0x101256b0
    INT OnPauseBroadcastGameBtnClick();                              // 0x101258b0
    INT OnSetGameCounterBtnClick();                                  // 0x10125a30
    INT OnSendGameEndMsgBtnClick();                                  // 0x10125b20
    INT OnControlFenceBtnClick();                                    // 0x10125be0
    INT OnTeamAFirecrackerBtnClick();                                // 0x10122bf0
    INT OnTeamBFirecrackerBtnClick();                                // 0x10122cd0

    // the server's replies (NConsoleWnd::OnEventMatch*; the console calls them by address)
    FString GetBroadcastGameID();                                    // 0x10122db0
    void NotifyCreateBroadcastGame(FString InGameID);                // 0x10122e40
    void NotifyRemoveBroadcastGame();                                // 0x10122fc0
    void NotifyLockPartyA();                                         // 0x10123180
    void NotifyLockPartyB();                                         // 0x10123260
    void NotifyUnlockPartyA();                                       // 0x10123340
    void NotifyUnlockPartyB();                                       // 0x10123400 (the stock guard string says NotifyLockPartyB)
    void NotifyStartBroadcastGame();                                 // 0x101234c0
    void NotifyEndBroadcastGame();                                   // 0x101235f0
    void NotifyPauseBroadcastGame();                                 // 0x101236d0
    void NotifyResumeBroadcastGame();                                // 0x10123760
    void NotifyTeamInfoStart(INT InGameID, INT Team, INT Unused);    // 0x101237f0
    void NotifyTeamInfo(INT Index, INT ID, FString Name, INT P1, INT P2, INT P3, INT P4, INT P5, INT P6, INT P7,
                        INT P8, INT P9, INT P10);                    // 0x1010DA90
    void NotifyTeamInfoEnd();                                        // 0x10123a00
    void NotifyFenceInfo(FString X, FString Y, FString Z, FString Width, FString Height);   // 0x10124230
    void StartCountDown();                                           // 0x10124450 the timer and the first announcement
    // the console's entries for a game found when the window is opened (NConsoleWnd 382 / 383; names ours): they take the
    // next values of the packet with L2ParamStack::Top (ret 4 each)
    void NotifyGameInfo(L2ParamStack* Params);                       // 0x10123bc0 the game's state, parties, locks and fence
    void NotifyTeamMember(L2ParamStack* Params);                     // 0x10123f80 a player of a party (his row and his leader box)
    UBOOL ReadBroadcastOptionFile(FString FileName);                 // 0x10127490 the option file (GConfig), 0 on a bad one
};
NW_ASSERT_OFFSET(NCBroadcastGMWnd, BackTex, 0xE8);
NW_ASSERT_OFFSET(NCBroadcastGMWnd, PartyAEdit, 0xEC);
NW_ASSERT_OFFSET(NCBroadcastGMWnd, AnnounceEdit, 0x10C);
NW_ASSERT_OFFSET(NCBroadcastGMWnd, CommandButton, 0x110);
NW_ASSERT_OFFSET(NCBroadcastGMWnd, FireworkBButton, 0x144);
NW_ASSERT_OFFSET(NCBroadcastGMWnd, TeamAList, 0x148);
NW_ASSERT_OFFSET(NCBroadcastGMWnd, GameID, 0x150);
NW_ASSERT_OFFSET(NCBroadcastGMWnd, Option, 0x15C);
NW_ASSERT_OFFSET(NCBroadcastGMWnd, Teams, 0x18C);
NW_ASSERT_OFFSET(NCBroadcastGMWnd, TeamFillIndex, 0x74C);
NW_ASSERT_OFFSET(NCBroadcastGMWnd, CountDownTimer, 0x754);
NW_ASSERT_SIZE(NCBroadcastGMWnd, 0x758);

#pragma pack(pop)
