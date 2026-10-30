/*=============================================================================
    NCBroadcastGMWnd.cpp - the GM's control window of the broadcast ("event match") tools (C3 only; C3 addresses). Stock:
    0x1010CB70 .. 0x1010E5E0 (the handlers and the server's replies), 0x1010E6C0 OnDestroy, 0x1010EAB0 OnPaint, 0x1010EED0
    OnTimer, 0x1010F250 .. 0x1010FD10 (more handlers), 0x10110080 OnCreate, 0x10111660 ReadBroadcastOptionFile, 0x10111E20 and
    0x101120C0 (the command page and the start button), 0x10112660 WindowProc, the constructor 0x10112820 and the
    destructor 0x101129C0. The scalar deleting destructor (0x10112990) and InternalConstructor (0x10112A50) stay stock.
    The small anonymous functions among them are the compiler's copies of the members' constructors and destructors and of
    two TArray templates.
=============================================================================*/
#include "nwindow/NCBroadcastGMWnd.h"
#include "nwindow/NCBroadcastGMFenceWnd.h"
#include "nwindow/NCButton.h"
#include "nwindow/NCConsole.h"
#include "nwindow/NCEditBox.h"
#include "nwindow/NCFrameCtrl.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NCListCtrl.h"
#include "nwindow/NCNPCHtmlViewer.h"
#include "nwindow/NConsoleWnd.h"
#include "nwindow/NGameData.h"
#include "nwindow/MsnUiStock.h"   // NCoreSkinControl

// NWM_COMMAND ids of the controls (names ours) and the title bar's id
enum
{
    NID_BGM_FRAME = 0xFFFFF001,
    NID_BGM_PARTYA = 0xFFFFE001,
    NID_BGM_PARTYB = 0xFFFFE002,
    NID_BGM_OPTIONFILE = 0xFFFFE003,
    NID_BGM_COMMANDFILE = 0xFFFFE004,
    NID_BGM_LEADERA = 0xFFFFE005,
    NID_BGM_LEADERB = 0xFFFFE006,
    NID_BGM_ANNOUNCE = 0xFFFFE008,
    NID_BGM_SCOREA = 0xFFFFE009,
    NID_BGM_SCOREB = 0xFFFFE00A,
    NID_BGM_SHOWCOMMANDS = 0xFFFFD003,
    NID_BGM_SETLEADERA = 0xFFFFD004,
    NID_BGM_LOCKA = 0xFFFFD005,
    NID_BGM_SETLEADERB = 0xFFFFD006,
    NID_BGM_LOCKB = 0xFFFFD007,
    NID_BGM_SENDANNOUNCE = 0xFFFFD009,
    NID_BGM_START = 0xFFFFD00A,
    NID_BGM_CREATE = 0xFFFFD00B,
    NID_BGM_PAUSE = 0xFFFFD00C,
    NID_BGM_SETSCORE = 0xFFFFD00D,
    NID_BGM_ENDMSG = 0xFFFFD00E,
    NID_BGM_FENCE = 0xFFFFD00F,
    NID_BGM_FIREA = 0xFFFFD010,
    NID_BGM_FIREB = 0xFFFFD011,
    NID_BGM_LISTA = 0xFFFFC001,
    NID_BGM_LISTB = 0xFFFFC002,
};

enum { BGM_COUNTDOWN_TIMER = 0xFFFFA001 };   // the countdown's timer id (OnTimer)

// sysstring lines (GL2GameData + 0x65A8C + 12 * N; names ours: the texts are only known by position)
enum
{
    STR_BGM_ColumnName = 872,         // the team lists' columns
    STR_BGM_ColumnLevel = 537,
    STR_BGM_ColumnClass = 391,
    STR_BGM_Create = 1068,            // the create / remove button
    STR_BGM_Remove = 1069,
    STR_BGM_SetLeader = 1070,
    STR_BGM_Lock = 1071,
    STR_BGM_Locked = 1072,
    STR_BGM_Pause = 1073,
    STR_BGM_Resume = 1074,
    STR_BGM_Start = 1075,
    STR_BGM_End = 1076,
    STR_BGM_SetScore = 1077,
    STR_BGM_Announce = 1078,
    STR_BGM_Commands = 1079,
    STR_BGM_EndMsg = 1080,
    STR_BGM_FenceOn = 1081,
    STR_BGM_FenceOff = 1082,
    STR_BGM_FireworkA = 1083,
    STR_BGM_FireworkB = 1084,
    STR_BGM_LabelPartyA = 1085,       // OnPaint's labels
    STR_BGM_LabelPartyB = 1086,
    STR_BGM_LabelLeaderA = 1087,
    STR_BGM_LabelLeaderB = 1088,
    STR_BGM_LabelOptionFile = 1089,
    STR_BGM_LabelCommandFile = 1090,
    STR_BGM_LabelListA = 1091,
    STR_BGM_LabelListB = 1092,
    STR_BGM_LabelScore = 1093,
    STR_BGM_LabelScoreA = 1094,
    STR_BGM_LabelScoreB = 1095,
    STR_BGM_LabelAnnounce = 1096,
    STR_BGM_Locking = 1097,           // the texts of a button waiting for the server's answer
    STR_BGM_Unlocking = 1098,
    STR_BGM_Creating = 1099,
    STR_BGM_Pausing = 1100,
    STR_BGM_Resuming = 1101,
    STR_BGM_Starting = 1102,
    STR_BGM_Ending = 1103,
    STR_BGM_MinimizedTip = 1104,
    STR_BGM_LabelGameID = 1107,
};

// systemmsg lines (GL2GameData + 0x3F638 + 0x30 * N + 4): the warnings
enum
{
    MSG_BGM_NoOptionFile = 1415,      // the command file box is empty (OnShowBroadcastCommandWndBtnClick)
    MSG_BGM_NoLeaderA = 1416,
    MSG_BGM_NoLeaderB = 1417,
    MSG_BGM_NoPartyAName = 1418,
    MSG_BGM_NoPartyBName = 1419,
    MSG_BGM_SamePartyName = 1420,
    MSG_BGM_NoOptionFileName = 1421,
    MSG_BGM_NoBuffCount = 1422,
    MSG_BGM_NoBuffID = 1423,          // with the number of the entry
    MSG_BGM_NoBuffLevel = 1424,
    MSG_BGM_NoSkillDefault = 1425,    // also used for the item default
    MSG_BGM_NoSkillCount = 1426,
    MSG_BGM_NoSkillID = 1427,
    MSG_BGM_NoItemCount = 1428,
    MSG_BGM_NoItemID = 1429,
    MSG_BGM_NoTeleportDelay = 1430,
};

// a warning on the console's dialog (virtual 28)
static FORCEINLINE void BgmWarn(INT MsgIndex)
{
    GConsole->ShowWarningDialog((INT)(INT_PTR)*GL2GameData.SystemMsg[MsgIndex].Message, 0);
}

// A command line to the server: the console's virtual 193 (SendBypassBuildCmd, 0x1006A020, ret 0xC; the C3 slot was 192,
// 0x1005EEB0) takes it as an FString by value (it destroys it); a macro so that a Printf result is built in the
// argument slot as the stock code does
#define BGM_SEND(Command) \
    ((void(__fastcall*)(NConsoleWnd*, void*, FString))(*(void***)GConsole)[193])(GConsole, 0, Command)

// a child window goes (repeated inline in the stock OnDestroy)
template <class T> static FORCEINLINE void DestroyChild(NCWnd* Parent, T*& Child)
{
    if (Child)
    {
        Parent->RemoveChildWindow(Child->Entry);
        delete Child;
        Child = NULL;
    }
}

/*-----------------------------------------------------------------------------
    The members of the window (the compiler's out-of-line copies of their constructors and destructors)
-----------------------------------------------------------------------------*/

// 0x10125d30 - a free slot: id -1, empty name, the numbers cleared and both lists emptied
NBroadcastGMMember::NBroadcastGMMember()
{
    ID = -1;
    Name = TEXT("");
    Value10 = 0;
    Value14 = 0;
    Value18 = 0;
    Value1C = 0;
    Value20 = 0;
    Value24 = 0;
    Level = 0;
    ClassID = 0;
    Value30 = 0;
    Value34 = 0;
    ListA.Empty();
    ListB.Empty();
}
NW_HOOK_CTOR(0x10125d30, NBroadcastGMMember)

// 0x10125de0 - the lists and the name go (in reverse order)
NBroadcastGMMember::~NBroadcastGMMember()
{
}
NW_HOOK_DTOR(0x10125de0, NBroadcastGMMember)

// 0x10125e40 - the name, then the nine players
NBroadcastGMTeam::NBroadcastGMTeam()
{
}
NW_HOOK_CTOR(0x10125e40, NBroadcastGMTeam)

// 0x10127ba0
NBroadcastGMTeam::~NBroadcastGMTeam()
{
}
NW_HOOK_DTOR(0x10127ba0, NBroadcastGMTeam)

/*-----------------------------------------------------------------------------
    NCBroadcastGMWnd
-----------------------------------------------------------------------------*/

// 0x101285f0 - NCWnd's constructor, the name, the option arrays, the teams, the control pointers cleared one by one
// (the two lists are left alone), the game id emptied and the countdown state
NCBroadcastGMWnd::NCBroadcastGMWnd()
{
    BackTex = NULL;
    PartyAEdit = NULL;
    PartyBEdit = NULL;
    OptionFileEdit = NULL;
    CommandFileEdit = NULL;
    LeaderAEdit = NULL;
    LeaderBEdit = NULL;
    ScoreAEdit = NULL;
    ScoreBEdit = NULL;
    AnnounceEdit = NULL;
    CommandButton = NULL;
    SetLeaderAButton = NULL;
    LockAButton = NULL;
    SetLeaderBButton = NULL;
    LockBButton = NULL;
    AnnounceButton = NULL;
    StartButton = NULL;
    CreateButton = NULL;
    PauseButton = NULL;
    CounterButton = NULL;
    EndMsgButton = NULL;
    FenceButton = NULL;
    FireworkAButton = NULL;
    FireworkBButton = NULL;
    GameID = TEXT("");
    CountDownTimer = 0;
    TeamFillIndex = -1;
    CountDownStep = -1;
}
NW_HOOK_CTOR(0x101285f0, NCBroadcastGMWnd)

// 0x10128790
NW_HOOK_DTOR(0x10128790, NCBroadcastGMWnd)


// 0x10128430 (vslot 25) - NCWnd's, then the buttons by id
INT NCBroadcastGMWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCWnd::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND)
    {
        if (wParam == NID_BGM_SHOWCOMMANDS)
            return OnShowBroadcastCommandWndBtnClick() != 0;
        if (wParam == NID_BGM_SETLEADERA)
            return OnSetBroadcastPartyALeaderBtnClick() != 0;
        if (wParam == NID_BGM_LOCKA)
            return OnLockBroadcastPartyABtnClick() != 0;
        if (wParam == NID_BGM_SETLEADERB)
            return OnSetBroadcastPartyBLeaderBtnClick() != 0;
        if (wParam == NID_BGM_LOCKB)
            return OnLockBroadcastPartyBBtnClick() != 0;
        if (wParam == NID_BGM_SENDANNOUNCE)
            return OnSendBroadcastAnnounceBtnClick() != 0;
        if (wParam == NID_BGM_START)
            return OnStartBroadcastGameBtnClick() != 0;
        if (wParam == NID_BGM_CREATE)
            return OnCreateBroadcastGameBtnClick() != 0;
        if (wParam == NID_BGM_PAUSE)
            return OnPauseBroadcastGameBtnClick() != 0;
        if (wParam == NID_BGM_SETSCORE)
            return OnSetGameCounterBtnClick() != 0;
        if (wParam == NID_BGM_ENDMSG)
            return OnSendGameEndMsgBtnClick() != 0;
        if (wParam == NID_BGM_FENCE)
            return OnControlFenceBtnClick() != 0;
        if (wParam == NID_BGM_FIREA)
            return OnTeamAFirecrackerBtnClick() != 0;
        if (wParam == NID_BGM_FIREB)
            return OnTeamBFirecrackerBtnClick() != 0;
    }
    return 0;
}
NW_HOOK_METHOD(0x10128430, NCBroadcastGMWnd, WindowProc)

// 0x10122ab0 (guard) - the leader's box takes the name of the player the GM has targeted (nothing without a target)
INT NCBroadcastGMWnd::OnSetBroadcastPartyALeaderBtnClick()
{
    guard(NCBroadcastGMWnd::OnSetBroadcastPartyALeaderBtnClick);
    LeaderAEdit->ClearText();
    User* Target = GConsole->GetSelectedActor();
    if (Target)
        LeaderAEdit->InsertText((TCHAR*)((BYTE*)Target + 0x1C), 1);
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10122ab0, NCBroadcastGMWnd, OnSetBroadcastPartyALeaderBtnClick)

// 0x10122b50 (guard)
INT NCBroadcastGMWnd::OnSetBroadcastPartyBLeaderBtnClick()
{
    guard(NCBroadcastGMWnd::OnSetBroadcastPartyBLeaderBtnClick);
    LeaderBEdit->ClearText();
    User* Target = GConsole->GetSelectedActor();
    if (Target)
        LeaderBEdit->InsertText((TCHAR*)((BYTE*)Target + 0x1C), 1);
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10122b50, NCBroadcastGMWnd, OnSetBroadcastPartyBLeaderBtnClick)

// 0x10122bf0 (guard) - a firework for every player of the party A with a slot taken. The stock passes the name's FString
// object itself to Printf (its three words; the first is the text pointer %s reads)
INT NCBroadcastGMWnd::OnTeamAFirecrackerBtnClick()
{
    guard(NCBroadcastGMWnd::OnTeamAFirecrackerBtnClick);
    for (INT i = 0; i < 9; i++)
    {
        if (Teams[0].Members[i].ID != -1)
        {
            const DWORD* Raw = (const DWORD*)&Teams[0].Members[i].Name;
            BGM_SEND(FString::Printf(TEXT("eventmatch firecracker %s"), Raw[0], Raw[1], Raw[2]));
        }
    }
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10122bf0, NCBroadcastGMWnd, OnTeamAFirecrackerBtnClick)

// 0x10122cd0 (guard) - the same for the party B
INT NCBroadcastGMWnd::OnTeamBFirecrackerBtnClick()
{
    guard(NCBroadcastGMWnd::OnTeamBFirecrackerBtnClick);
    for (INT i = 0; i < 9; i++)
    {
        if (Teams[1].Members[i].ID != -1)
        {
            const DWORD* Raw = (const DWORD*)&Teams[1].Members[i].Name;
            BGM_SEND(FString::Printf(TEXT("eventmatch firecracker %s"), Raw[0], Raw[1], Raw[2]));
        }
    }
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10122cd0, NCBroadcastGMWnd, OnTeamBFirecrackerBtnClick)

// 0x10122db0 (guard; ret 4) - a copy of the game id
FString NCBroadcastGMWnd::GetBroadcastGameID()
{
    guard(NCBroadcastGMWnd::GetBroadcastGameID);
    return GameID;
    unguard;
}
typedef FString (NCBroadcastGMWnd::*SigGetBroadcastGameID)();
NW_HOOK_VARARGS(0x10122db0, NCBroadcastGMWnd, GetBroadcastGameID, SigGetBroadcastGameID)

// 0x10122e40 (guard; ret 0xC) - the create button waited for the answer: the game id is kept, the button now removes
// the game and the other buttons come alive
void NCBroadcastGMWnd::NotifyCreateBroadcastGame(FString InGameID)
{
    guard(NCBroadcastGMWnd::NotifyCreateBroadcastGame);
    if (!(CreateButton->Text != *GL2GameData.SysString[STR_BGM_Creating]))
    {
        GameID = InGameID;
        CreateButton->SetTextID(STR_BGM_Remove);
        CreateButton->EnableWindow(1);
        SetLeaderAButton->EnableWindow(1);
        SetLeaderBButton->EnableWindow(1);
        LockAButton->EnableWindow(1);
        LockBButton->EnableWindow(1);
        AnnounceButton->EnableWindow(1);
        EndMsgButton->EnableWindow(1);
        CounterButton->EnableWindow(1);
        FenceButton->EnableWindow(1);
        FireworkAButton->EnableWindow(1);
        FireworkBButton->EnableWindow(1);
    }
    unguard;
}
typedef void (NCBroadcastGMWnd::*SigNotifyCreateBroadcastGame)(FString);
NW_HOOK_VARARGS(0x10122e40, NCBroadcastGMWnd, NotifyCreateBroadcastGame, SigNotifyCreateBroadcastGame)

// 0x10122fc0 (guard) - the game is gone: no id, only the create button on, every label back and both lists empty
void NCBroadcastGMWnd::NotifyRemoveBroadcastGame()
{
    guard(NCBroadcastGMWnd::NotifyRemoveBroadcastGame);
    GameID = TEXT("");
    CreateButton->EnableWindow(1);
    SetLeaderAButton->EnableWindow(0);
    SetLeaderBButton->EnableWindow(0);
    LockAButton->EnableWindow(0);
    LockBButton->EnableWindow(0);
    StartButton->EnableWindow(0);
    AnnounceButton->EnableWindow(0);
    EndMsgButton->EnableWindow(0);
    CounterButton->EnableWindow(0);
    PauseButton->EnableWindow(0);
    FenceButton->EnableWindow(0);
    FireworkAButton->EnableWindow(0);
    FireworkBButton->EnableWindow(0);
    CreateButton->SetTextID(STR_BGM_Create);
    LockAButton->SetTextID(STR_BGM_Lock);
    LockBButton->SetTextID(STR_BGM_Lock);
    StartButton->SetTextID(STR_BGM_Start);
    FenceButton->SetTextID(STR_BGM_FenceOn);
    TeamAList->ClearRows();
    TeamBList->ClearRows();
    unguard;
}
NW_HOOK_METHOD(0x10122fc0, NCBroadcastGMWnd, NotifyRemoveBroadcastGame)

// 0x10123180 (guard) - the party A is locked: its lock button shows "Locked", the leader button goes; with the party B
// locked too the game can start
void NCBroadcastGMWnd::NotifyLockPartyA()
{
    guard(NCBroadcastGMWnd::NotifyLockPartyA);
    LockAButton->EnableWindow(1);
    LockAButton->SetTextID(STR_BGM_Locked);
    SetLeaderAButton->EnableWindow(0);
    if (LockBButton->Text == *GL2GameData.SysString[STR_BGM_Locked])
        StartButton->EnableWindow(1);
    unguard;
}
NW_HOOK_METHOD(0x10123180, NCBroadcastGMWnd, NotifyLockPartyA)

// 0x10123260 (guard)
void NCBroadcastGMWnd::NotifyLockPartyB()
{
    guard(NCBroadcastGMWnd::NotifyLockPartyB);
    LockBButton->EnableWindow(1);
    LockBButton->SetTextID(STR_BGM_Locked);
    SetLeaderBButton->EnableWindow(0);
    if (LockAButton->Text == *GL2GameData.SysString[STR_BGM_Locked])
        StartButton->EnableWindow(1);
    unguard;
}
NW_HOOK_METHOD(0x10123260, NCBroadcastGMWnd, NotifyLockPartyB)

// 0x10123340 (guard) - the party A is open again: the game cannot start, the leader can be changed, the list is emptied
void NCBroadcastGMWnd::NotifyUnlockPartyA()
{
    guard(NCBroadcastGMWnd::NotifyUnlockPartyA);
    LockAButton->EnableWindow(1);
    StartButton->EnableWindow(0);
    SetLeaderAButton->EnableWindow(1);
    LockAButton->SetTextID(STR_BGM_Lock);
    TeamAList->ClearRows();
    unguard;
}
NW_HOOK_METHOD(0x10123340, NCBroadcastGMWnd, NotifyUnlockPartyA)

// 0x10123400 (guard string "NotifyLockPartyB" in the stock)
void NCBroadcastGMWnd::NotifyUnlockPartyB()
{
    guard(NCBroadcastGMWnd::NotifyLockPartyB);
    LockBButton->EnableWindow(1);
    StartButton->EnableWindow(0);
    SetLeaderBButton->EnableWindow(1);
    LockBButton->SetTextID(STR_BGM_Lock);
    TeamBList->ClearRows();
    unguard;
}
NW_HOOK_METHOD(0x10123400, NCBroadcastGMWnd, NotifyUnlockPartyB)

// 0x101234c0 (guard) - the game runs: the locks and the leader buttons go, pause and the end button come, and the server
// is told "msg 2"
void NCBroadcastGMWnd::NotifyStartBroadcastGame()
{
    guard(NCBroadcastGMWnd::NotifyStartBroadcastGame);
    SetLeaderAButton->EnableWindow(0);
    SetLeaderBButton->EnableWindow(0);
    LockAButton->EnableWindow(0);
    LockBButton->EnableWindow(0);
    PauseButton->EnableWindow(1);
    StartButton->EnableWindow(1);
    StartButton->SetTextID(STR_BGM_End);
    BGM_SEND(FString::Printf(TEXT("eventmatch msg %s %d NULL"), *GetBroadcastGameID(), 2));
    unguard;
}
NW_HOOK_METHOD(0x101234c0, NCBroadcastGMWnd, NotifyStartBroadcastGame)

// 0x101235f0 (guard) - the game is over: the party buttons are back, pause goes, the start button reads "Start"
void NCBroadcastGMWnd::NotifyEndBroadcastGame()
{
    guard(NCBroadcastGMWnd::NotifyEndBroadcastGame);
    SetLeaderAButton->EnableWindow(1);
    SetLeaderBButton->EnableWindow(1);
    LockAButton->EnableWindow(1);
    LockBButton->EnableWindow(1);
    PauseButton->EnableWindow(0);
    StartButton->EnableWindow(1);
    StartButton->SetTextID(STR_BGM_Start);
    unguard;
}
NW_HOOK_METHOD(0x101235f0, NCBroadcastGMWnd, NotifyEndBroadcastGame)

// 0x101236d0 (guard) - paused: the pause button now resumes
void NCBroadcastGMWnd::NotifyPauseBroadcastGame()
{
    guard(NCBroadcastGMWnd::NotifyPauseBroadcastGame);
    PauseButton->EnableWindow(1);
    PauseButton->SetTextID(STR_BGM_Resume);
    unguard;
}
NW_HOOK_METHOD(0x101236d0, NCBroadcastGMWnd, NotifyPauseBroadcastGame)

// 0x10123760 (guard) - resumed
void NCBroadcastGMWnd::NotifyResumeBroadcastGame()
{
    guard(NCBroadcastGMWnd::NotifyResumeBroadcastGame);
    PauseButton->EnableWindow(1);
    PauseButton->SetTextID(STR_BGM_Pause);
    unguard;
}
NW_HOOK_METHOD(0x10123760, NCBroadcastGMWnd, NotifyResumeBroadcastGame)

// 0x101237f0 (guard; ret 0xC) - a team list begins for this game: Team 1 is the party A, 2 the party B; its nine slots are
// freed and NotifyTeamInfo fills them. The third argument is not used
void NCBroadcastGMWnd::NotifyTeamInfoStart(INT InGameID, INT Team, INT Unused)
{
    guard(NCBroadcastGMWnd::NotifyTeamInfoStart);
    UBOOL bOtherGame;
    {
        FString Current = GetBroadcastGameID();
        bOtherGame = FString::Printf(TEXT("%d"), InGameID) != Current;
    }
    if (!bOtherGame)
    {
        if ((BYTE)Team == 1)
        {
            TeamFillIndex = 0;
            for (INT i = 0; i < 9; i++)
                Teams[0].Members[i].ID = -1;
        }
        else if ((BYTE)Team == 2)
        {
            TeamFillIndex = 1;
            for (INT i = 0; i < 9; i++)
                Teams[1].Members[i].ID = -1;
        }
    }
    unguard;
}
NW_HOOK_METHOD(0x101237f0, NCBroadcastGMWnd, NotifyTeamInfoStart)

// 0x10123900 (guard; ret 0x3C) - a player of the team being filled (slot 0 .. 8): his id, name and nine numbers (the tenth
// is not used; the first is kept twice)
void NCBroadcastGMWnd::NotifyTeamInfo(INT Index, INT InID, FString InName, INT P1, INT P2, INT P3, INT P4, INT P5,
                                      INT P6, INT P7, INT P8, INT P9, INT P10)
{
    guard(NCBroadcastGMWnd::NotifyTeamInfo);
    if ((TeamFillIndex == 0 || TeamFillIndex == 1) && Index >= 0 && Index < 9)
    {
        NBroadcastGMMember& Member = Teams[TeamFillIndex].Members[Index];
        Member.ID = InID;
        Member.Name = InName;
        Member.Value14 = P2;
        Member.Value18 = P3;
        Member.Value1C = P4;
        Member.Value20 = P5;
        Member.Value24 = P6;
        Member.Level = P7;
        Member.ClassID = P8;
        Member.Value30 = P9;
        Member.Value10 = P1;
        Member.Value34 = P1;
    }
    unguard;
}
typedef void (NCBroadcastGMWnd::*SigNotifyTeamInfo)(INT, INT, FString, INT, INT, INT, INT, INT, INT, INT, INT, INT, INT);
NW_HOOK_VARARGS(0x10123900, NCBroadcastGMWnd, NotifyTeamInfo, SigNotifyTeamInfo)

// 0x10123a00 (guard) - the list ends: the team's list is emptied, its lock button refreshed and the rows made from the
// slots taken (name, level, class); the team being filled is forgotten
void NCBroadcastGMWnd::NotifyTeamInfoEnd()
{
    guard(NCBroadcastGMWnd::NotifyTeamInfoEnd);
    if (TeamFillIndex == 0 || TeamFillIndex == 1)
    {
        if (TeamFillIndex == 0)
        {
            TeamAList->ClearRows();
            NotifyLockPartyA();
        }
        else if (TeamFillIndex == 1)
        {
            TeamBList->ClearRows();
            NotifyLockPartyB();
        }
        for (INT i = 0; i < 9; i++)
        {
            NBroadcastGMMember& Member = Teams[TeamFillIndex].Members[i];
            if (Member.ID == -1)
                break;
            NListRow* Row = new NListRow(3);
            Row->Cells[0].SetText(*Member.Name);
            Row->Cells[1].SetText(*FString::Printf(TEXT("%d"), Member.Level));
            Row->Cells[2].SetText(GConsole->GetClassNameString(Member.ClassID));
            if (TeamFillIndex == 0)
                TeamAList->AddRow(Row);
            else if (TeamFillIndex == 1)
                TeamBList->AddRow(Row);
        }
        TeamFillIndex = -1;
    }
    unguard;
}
NW_HOOK_METHOD(0x10123a00, NCBroadcastGMWnd, NotifyTeamInfoEnd)

// 0x10124230 (guard; ret 0x3C) - the fence's five texts: "eventmatch create 1 <party A> <party B> <x> <y> <z> <w> <h>";
// the create button then waits for the answer
void NCBroadcastGMWnd::NotifyFenceInfo(FString X, FString Y, FString Z, FString Width, FString Height)
{
    guard(NCBroadcastGMWnd::NotifyFenceInfo);
    BGM_SEND(FString::Printf(TEXT("eventmatch create 1 %s %s %s %s %s %s %s"), *PartyAEdit->Left, *PartyBEdit->Left, *X, *Y,
                             *Z, *Width, *Height));
    CreateButton->EnableWindow(0);
    CreateButton->SetTextID(STR_BGM_Creating);
    unguard;
}
typedef void (NCBroadcastGMWnd::*SigNotifyFenceInfo)(FString, FString, FString, FString, FString);
NW_HOOK_VARARGS(0x10124230, NCBroadcastGMWnd, NotifyFenceInfo, SigNotifyFenceInfo)

// 0x1001db90 (NCoreHelpers.cpp; ret 0xc) - the setter of the skin of the messenger's and the broadcast GM window's title-bar control
// (a stand-in class of that file, the same object; declared in MsnUiStock.h)

// 0x101243b0 (vslot 54, guard) - the icon of the minimized window gets its look (NCoreSkinControl::SetSkin of the
// icon, 0x1001DB90 in C4 (the C3 code called the function at its C3 address 0x1001BDD0), ret 0xC), then NCWnd's Minimize
void NCBroadcastGMWnd::Minimize()
{
    guard(NCBroadcastGMWnd::MinimizeWindow);
    if (Icon)
        ((NCoreSkinControl*)Icon)->SetSkin(TEXT("L2UI_CH3.MenuIcon.Menuicon4"),
                                           (const TCHAR*)*GL2GameData.SysString[STR_BGM_MinimizedTip], 1);
    NCWnd::Minimize();
    unguard;
}
NW_HOOK_METHOD(0x101243b0, NCBroadcastGMWnd, Minimize)

// 0x10124450 (guard) - the countdown: a timer of one second, step 4, "msg 8" to the server
void NCBroadcastGMWnd::StartCountDown()
{
    guard(NCBroadcastGMWnd::StartCountDown);
    CountDownTimer = SetTimer(BGM_COUNTDOWN_TIMER, 1000, 0);
    CountDownStep = 4;
    BGM_SEND(FString::Printf(TEXT("eventmatch msg %s %d NULL"), *GetBroadcastGameID(), 8));
    unguard;
}
NW_HOOK_METHOD(0x10124450, NCBroadcastGMWnd, StartCountDown)

// 0x10123bc0 (ret 4; no guard; name ours) - a game was found when the window opened (NConsoleWnd 382): everything is switched
// on as after a creation, then the packet sets the rest. It takes, in this order: the game id, the state (1 not started,
// 2 running, 3 paused), the fence (0 / 1 off, 2 on), the party A name, its lock (0 open, 1 locked), the party B name and its
// lock. The teams' lists are emptied
void NCBroadcastGMWnd::NotifyGameInfo(L2ParamStack* Params)
{
    CreateButton->SetTextID(STR_BGM_Remove);
    CreateButton->EnableWindow(1);
    SetLeaderAButton->EnableWindow(1);
    SetLeaderBButton->EnableWindow(1);
    LockAButton->EnableWindow(1);
    LockBButton->EnableWindow(1);
    AnnounceButton->EnableWindow(1);
    EndMsgButton->EnableWindow(1);
    CounterButton->EnableWindow(1);
    FenceButton->EnableWindow(1);
    FireworkAButton->EnableWindow(1);
    FireworkBButton->EnableWindow(1);
    GameID = FString::Printf(TEXT("%d"), Params->Top());
    BYTE State = (BYTE)(INT)Params->Top();
    BYTE FenceState = (BYTE)(INT)Params->Top();
    FString NameA((const TCHAR*)Params->Top());
    BYTE LockA = (BYTE)(INT)Params->Top();
    FString NameB((const TCHAR*)Params->Top());
    BYTE LockB = (BYTE)(INT)Params->Top();
    if (State == 1)
    {
        StartButton->SetTextID(STR_BGM_Start);
        PauseButton->SetTextID(STR_BGM_Pause);
        PauseButton->EnableWindow(0);
        if (LockA == State && LockB == State)
            StartButton->EnableWindow(1);
        else
            StartButton->EnableWindow(0);
        LockAButton->EnableWindow(1);
        LockBButton->EnableWindow(1);
    }
    else if (State == 2 || State == 3)
    {
        StartButton->SetTextID(STR_BGM_End);
        PauseButton->SetTextID(State == 2 ? STR_BGM_Pause : STR_BGM_Resume);
        StartButton->EnableWindow(1);
        PauseButton->EnableWindow(1);
        LockAButton->EnableWindow(0);
        LockBButton->EnableWindow(0);
    }
    if (FenceState == 0 || FenceState == 1)
        FenceButton->SetTextID(STR_BGM_FenceOn);
    else if (FenceState == 2)
        FenceButton->SetTextID(STR_BGM_FenceOff);
    if (LockA == 0)
        LockAButton->SetTextID(STR_BGM_Lock);
    else if (LockA == 1)
        LockAButton->SetTextID(STR_BGM_Locked);
    if (LockB == 0)
        LockBButton->SetTextID(STR_BGM_Lock);
    else if (LockB == 1)
        LockBButton->SetTextID(STR_BGM_Locked);
    PartyAEdit->ClearText();
    PartyBEdit->ClearText();
    if (NameA.Num() != 0 && NameA.Num() - 1 > 0)
        PartyAEdit->InsertText(*NameA, 1);
    if (NameB.Num() != 0 && NameB.Num() - 1 > 0)
        PartyBEdit->InsertText(*NameB, 1);
    TeamAList->ClearRows();
    TeamBList->ClearRows();
    for (INT i = 0; i < 9; i++)
        Teams[0].Members[i].ID = -1;
    for (INT i = 0; i < 9; i++)
        Teams[1].Members[i].ID = -1;
}
NW_HOOK_METHOD(0x10123bc0, NCBroadcastGMWnd, NotifyGameInfo)

// 0x10123f80 (ret 4; no guard; name ours) - a player of a party arrives when the window opened (NConsoleWnd 383). The packet
// is taken in this order: the game id (another game's packet is left unread), the slot, the party (1 A, 2 B), whether he
// is the leader, his id, his name, his class and his level. The slot is kept as it is (no range check), a row of the
// party's list shown and, for the leader, the party's leader box takes the name
void NCBroadcastGMWnd::NotifyTeamMember(L2ParamStack* Params)
{
    INT PacketGame = (INT)Params->Top();
    UBOOL bOtherGame;
    {
        FString Current = GetBroadcastGameID();
        bOtherGame = FString::Printf(TEXT("%d"), PacketGame) != Current;
    }
    if (bOtherGame)
        return;
    INT Slot = (INT)Params->Top();
    signed char Team = (signed char)((signed char)(INT)Params->Top() - 1);
    BYTE bLeader = (BYTE)(INT)Params->Top();
    INT MemberID = (INT)Params->Top();
    FString Name((const TCHAR*)Params->Top());
    INT ClassID = (INT)Params->Top();
    INT Level = (INT)Params->Top();
    NBroadcastGMMember& Member = Teams[(INT)Team].Members[Slot];
    Member.ID = MemberID;
    Member.Name = Name;
    Member.Value10 = 0;
    Member.Value14 = 0;
    Member.Value18 = 0;
    Member.Value1C = 0;
    Member.Value20 = 0;
    Member.Value24 = 0;
    Member.Level = Level;
    Member.ClassID = ClassID;
    Member.Value30 = 0;
    Member.Value34 = 0;
    NListRow* Row = new NListRow(3);
    Row->Cells[0].SetText(*Member.Name);
    Row->Cells[1].SetText(*FString::Printf(TEXT("%d"), Member.Level));
    Row->Cells[2].SetText(GConsole->GetClassNameString(Member.ClassID));
    if (Team == 0)
        TeamAList->AddRow(Row);
    else if (Team == 1)
        TeamBList->AddRow(Row);
    if (Team == 0)
    {
        if (bLeader == 1)
        {
            LeaderAEdit->ClearText();
            LeaderAEdit->InsertText(*Member.Name, 1);
        }
    }
    else if (Team == 1 && bLeader == 1)
    {
        LeaderBEdit->ClearText();
        LeaderBEdit->InsertText(*Member.Name, 1);
    }
}
NW_HOOK_METHOD(0x10123f80, NCBroadcastGMWnd, NotifyTeamMember)

// 0x10124530 (vslot 38, guard) - the twenty five children go in the order of their creation boxes first, then the
// buttons, then the two lists (C4: the C3 code left the last three buttons and the lists alone); NCWnd's OnDestroy follows
INT NCBroadcastGMWnd::OnDestroy()
{
    guard(NCBroadcastGMWnd::OnDestroy);
    DestroyChild(this, PartyAEdit);
    DestroyChild(this, PartyBEdit);
    DestroyChild(this, ScoreAEdit);
    DestroyChild(this, ScoreBEdit);
    DestroyChild(this, OptionFileEdit);
    DestroyChild(this, CommandFileEdit);
    DestroyChild(this, LeaderAEdit);
    DestroyChild(this, LeaderBEdit);
    DestroyChild(this, AnnounceEdit);
    DestroyChild(this, CommandButton);
    DestroyChild(this, SetLeaderAButton);
    DestroyChild(this, LockAButton);
    DestroyChild(this, SetLeaderBButton);
    DestroyChild(this, LockBButton);
    DestroyChild(this, AnnounceButton);
    DestroyChild(this, StartButton);
    DestroyChild(this, CreateButton);
    DestroyChild(this, PauseButton);
    DestroyChild(this, CounterButton);
    DestroyChild(this, EndMsgButton);
    DestroyChild(this, FenceButton);
    DestroyChild(this, FireworkAButton);
    DestroyChild(this, FireworkBButton);
    DestroyChild(this, TeamAList);
    DestroyChild(this, TeamBList);
    NCWnd::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10124530, NCBroadcastGMWnd, OnDestroy)

// 0x101249f0 (vslot 56, guard) - the panel under the title bar (20 down, the window's width, its height less 20) and the
// labels of the boxes, the game id ("<label>: <id>") and the score line
INT NCBroadcastGMWnd::OnPaint(NCGDevice* Device)
{
    guard(NCBroadcastGMWnd::OnPaint);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    Device->BeginPaint(ScreenX, ScreenY);
    if (BackTex)
        Device->DrawTexture(0, 20, (INT)Width, (INT)(Height - 20.0f), 0, 0, 0x100, 0x6C, BackTex, 0xFF, 1);
    Device->DrawText(0x19, 0x23, 0xFFDCDCDC, *GL2GameData.SysString[STR_BGM_LabelPartyA], 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0,
                     0, 0, 0);
    Device->DrawText(0x109, 0x23, 0xFFDCDCDC, *GL2GameData.SysString[STR_BGM_LabelPartyB], 0, 0, 0, L2FONT_Normal, 0, 0, 0,
                     0, 0, 0, 0);
    Device->DrawText(0x1EF, 0x37, 0xFFDCDCDC,
                     *FString::Printf(TEXT("%s: %s"), *GL2GameData.SysString[STR_BGM_LabelGameID], *GetBroadcastGameID()),
                     0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
    Device->DrawText(0x19, 0x5A, 0xFFDCDCDC, *GL2GameData.SysString[STR_BGM_LabelLeaderA], 0, 0, 0, L2FONT_Normal, 0, 0, 0,
                     0, 0, 0, 0);
    Device->DrawText(0x109, 0x5A, 0xFFDCDCDC, *GL2GameData.SysString[STR_BGM_LabelLeaderB], 0, 0, 0, L2FONT_Normal, 0, 0, 0,
                     0, 0, 0, 0);
    Device->DrawText(0x19, 0x96, 0xFFDCDCDC, *GL2GameData.SysString[STR_BGM_LabelOptionFile], 0, 0, 0, L2FONT_Normal, 0, 0,
                     0, 0, 0, 0, 0);
    Device->DrawText(0x15E, 0x96, 0xFFDCDCDC, *GL2GameData.SysString[STR_BGM_LabelCommandFile], 0, 0, 0, L2FONT_Normal, 0,
                     0, 0, 0, 0, 0, 0);
    Device->DrawText(0x19, 0xEB, 0xFFDCDCDC, *GL2GameData.SysString[STR_BGM_LabelListA], 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0,
                     0, 0, 0);
    Device->DrawText(0x163, 0xEB, 0xFFDCDCDC, *GL2GameData.SysString[STR_BGM_LabelListB], 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0,
                     0, 0, 0);
    Device->DrawText(0x19, 0x1A4, 0xFFDCDCDC, *GL2GameData.SysString[STR_BGM_LabelScore], 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0,
                     0, 0, 0);
    Device->DrawText(0x50, 0x1A4, 0xFFDCDCDC, *GL2GameData.SysString[STR_BGM_LabelScoreA], 0, 0, 0, L2FONT_Normal, 0, 0, 0,
                     0, 0, 0, 0);
    Device->DrawText(0x8C, 0x1A4, 0xFFDCDCDC, TEXT(":"), 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
    Device->DrawText(0x96, 0x1A4, 0xFFDCDCDC, *GL2GameData.SysString[STR_BGM_LabelScoreB], 0, 0, 0, L2FONT_Normal, 0, 0, 0,
                     0, 0, 0, 0);
    Device->DrawText(0x19, 0x1C2, 0xFFDCDCDC, *FString::Printf(TEXT("%s:"), *GL2GameData.SysString[STR_BGM_LabelAnnounce]),
                     0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x101249f0, NCBroadcastGMWnd, OnPaint)

// 0x10124e10 (vslot 36, guard) - the countdown's timer: step 4 .. 1 sends "msg 7 / 6 / 5 / 4" and goes down; step 0 sends
// "msg 2", the buffs ("useskill <id> <buff> <level> ...") and "start", marks the game as started and stops the timer, like
// any step out of range
INT NCBroadcastGMWnd::OnTimer(INT TimerID)
{
    guard(NCBroadcastGMWnd::OnTimer);
    if (TimerID == (INT)BGM_COUNTDOWN_TIMER)
    {
        switch (CountDownStep)
        {
        case 4:
            BGM_SEND(FString::Printf(TEXT("eventmatch msg %s %d NULL"), *GetBroadcastGameID(), 7));
            CountDownStep = 3;
            break;
        case 3:
            BGM_SEND(FString::Printf(TEXT("eventmatch msg %s %d NULL"), *GetBroadcastGameID(), 6));
            CountDownStep = 2;
            break;
        case 2:
            BGM_SEND(FString::Printf(TEXT("eventmatch msg %s %d NULL"), *GetBroadcastGameID(), 5));
            CountDownStep = 1;
            break;
        case 1:
            BGM_SEND(FString::Printf(TEXT("eventmatch msg %s %d NULL"), *GetBroadcastGameID(), 4));
            CountDownStep = 0;
            break;
        case 0:
        {
            BGM_SEND(FString::Printf(TEXT("eventmatch msg %s %d NULL"), *GetBroadcastGameID(), 2));
            FString Command = FString::Printf(TEXT("eventmatch useskill %s"), *GetBroadcastGameID());
            for (INT i = 0; i < Option.Buffs.Num(); i++)
                Command += FString::Printf(TEXT(" %d %d"), Option.Buffs(i).First, Option.Buffs(i).Second);
            BGM_SEND(Command);
            BGM_SEND(FString::Printf(TEXT("eventmatch start %s"), *GetBroadcastGameID()));
            NotifyStartBroadcastGame();
        }
            // falls into the default: the timer stops
        default:
            if (CountDownTimer)
            {
                KillTimer(CountDownTimer);
                CountDownTimer = 0;
            }
            CountDownStep = -1;
            break;
        }
    }
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10124e10, NCBroadcastGMWnd, OnTimer)

// 0x10125190 (guard) - the lock button of the party A. "Lock": the leader's name is needed (a message without it); the
// server gets the leader and the lock and the button waits for the answer. Otherwise the party is unlocked the same way
INT NCBroadcastGMWnd::OnLockBroadcastPartyABtnClick()
{
    guard(NCBroadcastGMWnd::OnLockBroadcastPartyABtnClick);
    if (LockAButton->Text == *GL2GameData.SysString[STR_BGM_Lock])
    {
        if (LeaderAEdit->Left != TEXT(""))
        {
            BGM_SEND(FString::Printf(TEXT("eventmatch leader %s 1 %s"), *GetBroadcastGameID(), *LeaderAEdit->Left));
            BGM_SEND(FString::Printf(TEXT("eventmatch lock %s 1"), *GetBroadcastGameID()));
            LockAButton->EnableWindow(0);
            LockAButton->SetTextID(STR_BGM_Locking);
        }
        else
            BgmWarn(MSG_BGM_NoLeaderA);
    }
    else
    {
        BGM_SEND(FString::Printf(TEXT("eventmatch unlock %s 1"), *GetBroadcastGameID()));
        LockAButton->EnableWindow(0);
        LockAButton->SetTextID(STR_BGM_Unlocking);
        NotifyUnlockPartyA();
    }
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10125190, NCBroadcastGMWnd, OnLockBroadcastPartyABtnClick)

// 0x101253b0 (guard) - the same for the party B
INT NCBroadcastGMWnd::OnLockBroadcastPartyBBtnClick()
{
    guard(NCBroadcastGMWnd::OnLockBroadcastPartyBBtnClick);
    if (LockBButton->Text == *GL2GameData.SysString[STR_BGM_Lock])
    {
        if (LeaderBEdit->Left != TEXT(""))
        {
            BGM_SEND(FString::Printf(TEXT("eventmatch leader %s 2 %s"), *GetBroadcastGameID(), *LeaderBEdit->Left));
            BGM_SEND(FString::Printf(TEXT("eventmatch lock %s 2"), *GetBroadcastGameID()));
            LockBButton->EnableWindow(0);
            LockBButton->SetTextID(STR_BGM_Locking);
        }
        else
            BgmWarn(MSG_BGM_NoLeaderB);
    }
    else
    {
        BGM_SEND(FString::Printf(TEXT("eventmatch unlock %s 2"), *GetBroadcastGameID()));
        LockBButton->EnableWindow(0);
        LockBButton->SetTextID(STR_BGM_Unlocking);
        NotifyUnlockPartyB();
    }
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x101253b0, NCBroadcastGMWnd, OnLockBroadcastPartyBBtnClick)

// 0x101255d0 (guard) - the announcement typed goes to the server: "msg <id> 0 "<text>""
INT NCBroadcastGMWnd::OnSendBroadcastAnnounceBtnClick()
{
    guard(NCBroadcastGMWnd::OnSendBroadcastAnnounceBtnClick);
    BGM_SEND(FString::Printf(TEXT("eventmatch msg %s %d \"%s\""), *GetBroadcastGameID(), 0, *AnnounceEdit->Left));
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x101255d0, NCBroadcastGMWnd, OnSendBroadcastAnnounceBtnClick)

// 0x101256b0 (guard) - the create button. "Create": both party names must be typed and differ (a message otherwise), then the
// fence window comes up modal (its OK sends the command). "Remove": the game is removed
INT NCBroadcastGMWnd::OnCreateBroadcastGameBtnClick()
{
    guard(NCBroadcastGMWnd::OnCreateBroadcastGameBtnClick);
    if (CreateButton->Text == *GL2GameData.SysString[STR_BGM_Create])
    {
        if (PartyAEdit->Left == TEXT(""))
            BgmWarn(MSG_BGM_NoPartyAName);
        else if (PartyBEdit->Left == TEXT(""))
            BgmWarn(MSG_BGM_NoPartyBName);
        else if (PartyAEdit->Left == PartyBEdit->Left)
            BgmWarn(MSG_BGM_SamePartyName);
        else
            static_cast<NCBroadcastGMFenceWnd*>(GDesktop->BroadcastGMFenceWnd)->NewVirtual75();   // DoModal: virtual in C4 (slot 75, 0x100083F0); a direct call in C3
    }
    else if (CreateButton->Text == *GL2GameData.SysString[STR_BGM_Remove])
    {
        BGM_SEND(FString::Printf(TEXT("eventmatch remove %s"), *GetBroadcastGameID()));
        CreateButton->EnableWindow(0);
        CreateButton->SetTextID(STR_BGM_Unlocking);
        NotifyRemoveBroadcastGame();
    }
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x101256b0, NCBroadcastGMWnd, OnCreateBroadcastGameBtnClick)

// 0x101258b0 (guard) - the pause button: "Pause" pauses the game, otherwise it starts it again; either way it waits for the
// answer
INT NCBroadcastGMWnd::OnPauseBroadcastGameBtnClick()
{
    guard(NCBroadcastGMWnd::OnPauseBroadcastGameBtnClick);
    if (PauseButton->Text == *GL2GameData.SysString[STR_BGM_Pause])
    {
        BGM_SEND(FString::Printf(TEXT("eventmatch pause %s"), *GetBroadcastGameID()));
        PauseButton->EnableWindow(0);
        PauseButton->SetTextID(STR_BGM_Pausing);
        NotifyPauseBroadcastGame();
    }
    else
    {
        BGM_SEND(FString::Printf(TEXT("eventmatch start %s"), *GetBroadcastGameID()));
        PauseButton->EnableWindow(0);
        PauseButton->SetTextID(STR_BGM_Resuming);
        NotifyResumeBroadcastGame();
    }
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x101258b0, NCBroadcastGMWnd, OnPauseBroadcastGameBtnClick)

// 0x10125a30 (guard) - the score: "score <id> <A> <B>" from the two small boxes
INT NCBroadcastGMWnd::OnSetGameCounterBtnClick()
{
    guard(NCBroadcastGMWnd::OnSetGameCounterBtnClick);
    BGM_SEND(FString::Printf(TEXT("eventmatch score %s %s %s"), *GetBroadcastGameID(), *ScoreAEdit->Left,
                             *ScoreBEdit->Left));
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10125a30, NCBroadcastGMWnd, OnSetGameCounterBtnClick)

// 0x10125b20 (guard) - the "game ended" announcement (msg 3)
INT NCBroadcastGMWnd::OnSendGameEndMsgBtnClick()
{
    guard(NCBroadcastGMWnd::OnSendGameEndMsgBtnClick);
    BGM_SEND(FString::Printf(TEXT("eventmatch msg %s %d NULL"), *GetBroadcastGameID(), 3));
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10125b20, NCBroadcastGMWnd, OnSendGameEndMsgBtnClick)

// 0x10125be0 (guard) - the fence button toggles its text and sends "fence <id> 2" (it was on) or "fence <id> 1"
INT NCBroadcastGMWnd::OnControlFenceBtnClick()
{
    guard(NCBroadcastGMWnd::OnControlFenceBtnClick);
    if (FenceButton->Text == *GL2GameData.SysString[STR_BGM_FenceOn])
    {
        FenceButton->SetTextID(STR_BGM_FenceOff);
        BGM_SEND(FString::Printf(TEXT("eventmatch fence %s %d"), *GetBroadcastGameID(), 2));
    }
    else if (FenceButton->Text == *GL2GameData.SysString[STR_BGM_FenceOff])
    {
        FenceButton->SetTextID(STR_BGM_FenceOn);
        BGM_SEND(FString::Printf(TEXT("eventmatch fence %s %d"), *GetBroadcastGameID(), 1));
    }
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10125be0, NCBroadcastGMWnd, OnControlFenceBtnClick)

// a button of the window (repeated in the stock OnCreate): 76 x 23 at (X, Y), the two "btn1" textures, the text of a
// sysstring line, and switched off when bOff
static FORCEINLINE NCButton* MakeButton(NCWnd* Parent, INT ID, INT X, INT Y, INT TextIndex, UBOOL bOff)
{
    NCButton* Button = new NCButton(TEXT("L2UI_ch3.button.btn1_normal"), TEXT("L2UI_ch3.button.btn1_normalon"), 0);
    Button->ID = ID;
    Button->SetPosition(X, Y, 0x4C, 0x17);
    Parent->CreateChild(Button, NWS_VISIBLE);
    Button->SetTextID(TextIndex);
    if (bOff)
        Button->EnableWindow(0);
    return Button;
}

// a text box of the window (repeated in the stock OnCreate): W x 16 at (X, Y), MaxLength characters when not 0
static FORCEINLINE NCEditBox* MakeEdit(NCWnd* Parent, INT ID, INT X, INT Y, INT W, INT MaxLength)
{
    NCEditBox* Edit = new NCEditBox(0, 0, 1, 0x108, 0);
    Edit->ID = ID;
    Edit->SetPosition(X, Y, W, 0x10);
    Parent->CreateChild(Edit, NWS_VISIBLE);
    if (MaxLength)
        Edit->SetMaxLength(MaxLength);
    return Edit;
}

// a team list of the window: 9 rows of 3 columns, 300 x 150 at (X, 250)
static FORCEINLINE NCListCtrl* MakeList(NCWnd* Parent, INT ID, INT X)
{
    NCListCtrl* List = new NCListCtrl(9, 9, 3, 0x10, 1);
    List->ID = ID;
    List->SetPosition(X, 0xFA, 300, 0x96);
    Parent->CreateChild(List, NWS_VISIBLE);
    List->SetTextOffset(0, 3);
    return List;
}

// 0x10125ea0 (vslot 37, guard) - the panel texture, the title bar (20 high), the nine text boxes, the fourteen buttons (all
// off but "create" and the command page) and the two team lists with their three columns (name, level, class).
// C4: the button texts and the column titles are sysstring numbers (SetTextID, NListColumnInfo::SetTitleID: 0x10005BA0)
INT NCBroadcastGMWnd::OnCreate()
{
    guard(NCBroadcastGMWnd::OnCreate);
    NListColumnInfo Column;   // one record for the six columns
    NCWnd::OnCreate();
    BackTex = FindTexture(TEXT("L2UI_CH3.Minimap.MapBack"));
    FrameCtrl = new NCFrameCtrl(1, 0, 0);
    FrameID = NID_BGM_FRAME;
    FrameCtrl->ID = NID_BGM_FRAME;
    FrameCtrl->SetPosition(0, 0, (INT)Width, 20);
    CreateChild(FrameCtrl, NWS_VISIBLE);

    PartyAEdit = MakeEdit(this, NID_BGM_PARTYA, 0x19, 0x32, 0x96, 10);
    PartyBEdit = MakeEdit(this, NID_BGM_PARTYB, 0x109, 0x32, 0x96, 10);
    LeaderAEdit = MakeEdit(this, NID_BGM_LEADERA, 0x19, 0x69, 0x96, 0);
    LeaderBEdit = MakeEdit(this, NID_BGM_LEADERB, 0x109, 0x69, 0x96, 0);
    OptionFileEdit = MakeEdit(this, NID_BGM_OPTIONFILE, 0x19, 0xA5, 0xFA, 0);
    CommandFileEdit = MakeEdit(this, NID_BGM_COMMANDFILE, 0x15E, 0xA5, 0xFA, 0);
    ScoreAEdit = MakeEdit(this, NID_BGM_SCOREA, 100, 0x1A2, 0x19, 1);
    ScoreBEdit = MakeEdit(this, NID_BGM_SCOREB, 0xAA, 0x1A2, 0x19, 1);
    AnnounceEdit = MakeEdit(this, NID_BGM_ANNOUNCE, 0x50, 0x1C0, 500, 0x32);   // C4: 50 characters (C3: 25)

    CreateButton = MakeButton(this, NID_BGM_CREATE, 0x1EF, 0x1E, STR_BGM_Create, 0);
    SetLeaderAButton = MakeButton(this, NID_BGM_SETLEADERA, 0xB1, 0x5A, STR_BGM_SetLeader, 1);
    LockAButton = MakeButton(this, NID_BGM_LOCKA, 0xB1, 0x72, STR_BGM_Lock, 1);
    SetLeaderBButton = MakeButton(this, NID_BGM_SETLEADERB, 0x1A1, 0x5A, STR_BGM_SetLeader, 1);
    LockBButton = MakeButton(this, NID_BGM_LOCKB, 0x1A1, 0x72, STR_BGM_Lock, 1);
    PauseButton = MakeButton(this, NID_BGM_PAUSE, 0x1EF, 0xBE, STR_BGM_Pause, 1);
    StartButton = MakeButton(this, NID_BGM_START, 0x244, 0xBE, STR_BGM_Start, 1);
    CounterButton = MakeButton(this, NID_BGM_SETSCORE, 200, 0x19F, STR_BGM_SetScore, 1);
    AnnounceButton = MakeButton(this, NID_BGM_SENDANNOUNCE, 0x24E, 0x1BD, STR_BGM_Announce, 1);
    CommandButton = MakeButton(this, NID_BGM_SHOWCOMMANDS, 0x19, 0x1D6, STR_BGM_Commands, 0);
    EndMsgButton = MakeButton(this, NID_BGM_ENDMSG, 0x6E, 0x1D6, STR_BGM_EndMsg, 1);
    FenceButton = MakeButton(this, NID_BGM_FENCE, 0x226, 0x66, STR_BGM_FenceOn, 1);
    FireworkAButton = MakeButton(this, NID_BGM_FIREA, 0xC3, 0x1D6, STR_BGM_FireworkA, 1);
    FireworkBButton = MakeButton(this, NID_BGM_FIREB, 0x118, 0x1D6, STR_BGM_FireworkB, 1);

    TeamAList = MakeList(this, NID_BGM_LISTA, 0x19);
    TeamBList = MakeList(this, NID_BGM_LISTB, 0x163);

    Column.Width = 0;
    Column.Height = 0x13;
    Column.SetNormalTex(TEXT("L2UI_CH3.ListCtrl.tab1"), 0);
    Column.SetPushedTex(TEXT("L2UI_CH3.ListCtrl.tab1_down"), 0);
    Column.SetNormalTex(TEXT("L2UI_CH3.ListCtrl.tab2"), 1);
    Column.SetPushedTex(TEXT("L2UI_CH3.ListCtrl.tab2_down"), 1);
    Column.SetNormalTex(TEXT("L2UI_CH3.ListCtrl.tab3"), 2);
    Column.SetPushedTex(TEXT("L2UI_CH3.ListCtrl.tab3_down"), 2);
    Column.Width = 0x78;
    Column.SetTitleID(STR_BGM_ColumnName);
    TeamAList->AddColumn(0, &Column);
    TeamBList->AddColumn(0, &Column);
    Column.Width = 0x3C;
    Column.SetTitleID(STR_BGM_ColumnLevel);
    TeamAList->AddColumn(1, &Column);
    TeamBList->AddColumn(1, &Column);
    Column.Width = 0x78;
    Column.SetTitleID(STR_BGM_ColumnClass);
    TeamAList->AddColumn(2, &Column);
    TeamBList->AddColumn(2, &Column);
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10125ea0, NCBroadcastGMWnd, OnCreate)

// 0x10127490 (guard; ret 0xC) - the option file (GConfig): the numbers cleared and the lists emptied, the file loaded
// afresh, then "Buff" (BuffCnt, BuffID%d / BuffLv%d), "Skill" (DefaultAllow, ExpSkillCnt, ExpSkillID%d), "Item" (DefaultAllow,
// ExpItemCnt, ExpItemID%d) and "Etc" (TeleportDelay); the file is dropped from the cache at the end. Any missing key is a
// message (a numbered one for the lists' entries) and 0. A skill exception is stored with the last BuffID read as its
// first word, as the stock does
UBOOL NCBroadcastGMWnd::ReadBroadcastOptionFile(FString FileName)
{
    guard(NCBroadcastGMWnd::ReadBroadcastOptionFile);
    TCHAR Message[1024];
    INT First = 0;
    INT Second = 0;
    Option.TeleportDelay = 0;
    Option.SkillDefaultAllow = 0;
    Option.ItemDefaultAllow = 0;
    Option.ExpSkills.Empty();
    Option.ExpItems.Empty();
    Option.Buffs.Empty();
    ::nw::VirtualCall<void>(GConfig, 13, 1, *FileName);   // Flush: read the file again
    INT BuffCount = -1;
    if (!GConfig->GetInt(TEXT("Buff"), TEXT("BuffCnt"), BuffCount, *FileName))
    {
        BgmWarn(MSG_BGM_NoBuffCount);
        return 0;
    }
    INT i;
    for (i = 0; i < BuffCount; i++)
    {
        if (!GConfig->GetInt(TEXT("Buff"), *FString::Printf(TEXT("BuffID%d"), i), First, *FileName))
        {
            GConsole->GetNumberMessage(MSG_BGM_NoBuffID, i, Message);
            GConsole->ShowWarningDialog((INT)(INT_PTR)Message, 0);
            return 0;
        }
        if (!GConfig->GetInt(TEXT("Buff"), *FString::Printf(TEXT("BuffLv%d"), i), Second, *FileName))
        {
            GConsole->GetNumberMessage(MSG_BGM_NoBuffLevel, i, Message);
            GConsole->ShowWarningDialog((INT)(INT_PTR)Message, 0);
            return 0;
        }
        INT Slot = Option.Buffs.Add();
        Option.Buffs(Slot).First = First;
        Option.Buffs(Slot).Second = Second;
    }
    if (!GConfig->GetBool(TEXT("Skill"), TEXT("DefaultAllow"), (UBOOL&)Option.SkillDefaultAllow, *FileName))
    {
        BgmWarn(MSG_BGM_NoSkillDefault);
        return 0;
    }
    INT SkillCount = -1;
    if (!GConfig->GetInt(TEXT("Skill"), TEXT("ExpSkillCnt"), SkillCount, *FileName))
    {
        BgmWarn(MSG_BGM_NoSkillCount);
        return 0;
    }
    for (i = 0; i < SkillCount; i++)
    {
        if (!GConfig->GetInt(TEXT("Skill"), *FString::Printf(TEXT("ExpSkillID%d"), i), Second, *FileName))
        {
            GConsole->GetNumberMessage(MSG_BGM_NoSkillID, i, Message);
            GConsole->ShowWarningDialog((INT)(INT_PTR)Message, 0);
            return 0;
        }
        INT Slot = Option.ExpSkills.Add();
        Option.ExpSkills(Slot).First = First;
        Option.ExpSkills(Slot).Second = Second;
    }
    if (!GConfig->GetBool(TEXT("Item"), TEXT("DefaultAllow"), (UBOOL&)Option.ItemDefaultAllow, *FileName))
    {
        BgmWarn(MSG_BGM_NoSkillDefault);
        return 0;
    }
    INT ItemCount = 0;
    if (!GConfig->GetInt(TEXT("Item"), TEXT("ExpItemCnt"), ItemCount, *FileName))
    {
        BgmWarn(MSG_BGM_NoItemCount);
        return 0;
    }
    for (i = 0; i < ItemCount; i++)
    {
        if (!GConfig->GetInt(TEXT("Item"), *FString::Printf(TEXT("ExpItemID%d"), i), Second, *FileName))
        {
            GConsole->GetNumberMessage(MSG_BGM_NoItemID, i, Message);
            GConsole->ShowWarningDialog((INT)(INT_PTR)Message, 0);
            return 0;
        }
        INT Slot = Option.ExpItems.Add();
        Option.ExpItems(Slot) = Second;
    }
    if (!GConfig->GetInt(TEXT("Etc"), TEXT("TeleportDelay"), Option.TeleportDelay, *FileName))
    {
        BgmWarn(MSG_BGM_NoTeleportDelay);
        return 0;
    }
    ::nw::VirtualCall<void>(GConfig, 13, 1, *FileName);
    return 1;
    unguard;
}
typedef UBOOL (NCBroadcastGMWnd::*SigReadBroadcastOptionFile)(FString);
NW_HOOK_VARARGS(0x10127490, NCBroadcastGMWnd, ReadBroadcastOptionFile, SigReadBroadcastOptionFile)

// 0x10127c00 (guard) - the command page: the commands "Cmd0 .. Cmd<n-1>" of the command file ("Cmd" / "CmdCnt") become
// links ("<a cmd = "..">..</a><br1>") in an HTML page shown in the console's NPC window; without a file name a message
INT NCBroadcastGMWnd::OnShowBroadcastCommandWndBtnClick()
{
    guard(NCBroadcastGMWnd::OnShowBroadcastCommandWndBtnClick);
    FString File(CommandFileEdit->Left);
    if (File == TEXT(""))
    {
        BgmWarn(MSG_BGM_NoOptionFile);
        return 1;
    }
    INT CommandCount = 0;
    GConfig->GetInt(TEXT("Cmd"), TEXT("CmdCnt"), CommandCount, *File);
    TArray<FString> Commands;
    for (INT i = 0; i < CommandCount; i++)
    {
        FString Command;
        GConfig->GetString(TEXT("Cmd"), *FString::Printf(TEXT("Cmd%d"), i), Command, *File);
        new (Commands) FString(Command);
    }
    ::nw::VirtualCall<void>(GConfig, 13, 1, *File);
    FString Page;
    Page = TEXT("<html><body>");
    for (INT j = 0; j < Commands.Num(); j++)
    {
        Page += TEXT("<a cmd = \"");
        Page += Commands(j);
        Page += TEXT("\">");
        Page += Commands(j);
        Page += TEXT("</a><br1>");
    }
    Page += TEXT("</body></html>");
    GDesktop->NpcHtmlWnd->ShowPage((TCHAR*)*Page);   // 0x1011E9C0 (the C3 code called the function at its C3 address 0x10109890)
    GDesktop->NpcHtmlWnd->ShowWindow();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10127c00, NCBroadcastGMWnd, OnShowBroadcastCommandWndBtnClick)

// 0x10127ea0 (guard) - the start button. "Start": the option file named in the box is read (nothing without a name); the item
// and skill rules go to the server ("item_rule <id> allow_all D<n>.." or "deny_all A<n>..", the same for skills), the
// countdown starts and the button waits. Otherwise the game is ended: "dispelall", "end" and "msg 1"
INT NCBroadcastGMWnd::OnStartBroadcastGameBtnClick()
{
    guard(NCBroadcastGMWnd::OnStartBroadcastGameBtnClick);
    if (StartButton->Text == *GL2GameData.SysString[STR_BGM_Start])
    {
        FString OptionFile(OptionFileEdit->Left);
        FString LeaderA(LeaderAEdit->Left);   // copied and never used
        FString LeaderB(LeaderBEdit->Left);
        if (OptionFile == TEXT(""))
            BgmWarn(MSG_BGM_NoOptionFileName);
        else if (ReadBroadcastOptionFile(OptionFile))
        {
            FString Rules;
            if (Option.ItemDefaultAllow)
            {
                Rules = FString::Printf(TEXT("eventmatch item_rule %s allow_all"), *GetBroadcastGameID());
                for (INT i = 0; i < Option.ExpItems.Num(); i++)
                    Rules += FString::Printf(TEXT(" D%d"), Option.ExpItems(i));
            }
            else
            {
                Rules = FString::Printf(TEXT("eventmatch item_rule %s deny_all"), *GetBroadcastGameID());
                for (INT i = 0; i < Option.ExpItems.Num(); i++)
                    Rules += FString::Printf(TEXT(" A%d"), Option.ExpItems(i));
            }
            BGM_SEND(Rules);
            FString SkillRules;
            if (Option.SkillDefaultAllow)
            {
                SkillRules = FString::Printf(TEXT("eventmatch skill_rule %s allow_all"), *GetBroadcastGameID());
                for (INT i = 0; i < Option.ExpSkills.Num(); i++)
                    SkillRules += FString::Printf(TEXT(" D%d"), Option.ExpSkills(i).Second);
            }
            else
            {
                // (a stock slip: the header goes to the item rules' string, which was sent already, so only the
                // " A<n>" entries reach the server here)
                Rules = FString::Printf(TEXT("eventmatch skill_rule %s deny_all"), *GetBroadcastGameID());
                for (INT i = 0; i < Option.ExpSkills.Num(); i++)
                    SkillRules += FString::Printf(TEXT(" A%d"), Option.ExpSkills(i).Second);
            }
            BGM_SEND(SkillRules);
            StartCountDown();
            StartButton->EnableWindow(0);
            StartButton->SetTextID(STR_BGM_Starting);
        }
    }
    else
    {
        BGM_SEND(FString::Printf(TEXT("eventmatch dispelall %s"), *GetBroadcastGameID()));
        BGM_SEND(FString::Printf(TEXT("eventmatch end %s"), *GetBroadcastGameID()));
        BGM_SEND(FString::Printf(TEXT("eventmatch msg %s %d NULL"), *GetBroadcastGameID(), 1));
        StartButton->SetTextID(STR_BGM_Ending);
        NotifyEndBroadcastGame();
    }
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10127ea0, NCBroadcastGMWnd, OnStartBroadcastGameBtnClick)

/*-----------------------------------------------------------------------------
    The compiler's out-of-line template instances this window's code (and its destructor) calls
-----------------------------------------------------------------------------*/

typedef TArray<FString> TBgmStringArray;
typedef TArray<INT> TBgmIntArray;
typedef TArray<NBroadcastGMPair> TBgmPairArray;



// 0x10127a20 - a list of 4 byte elements (the players' two lists, ExpItems)
NW_HOOK_DTOR(0x10127a20, TBgmIntArray)

// 0x10127ab0 - a list of 8 byte elements (ExpSkills, Buffs)
NW_HOOK_DTOR(0x10127ab0, TBgmPairArray)

// 0x10127b40 - the option record: Buffs, ExpItems, ExpSkills go in this order
NW_HOOK_DTOR(0x10127b40, NBroadcastGMOption)

// 0x100f7b80 (ret 8) - TArray<FString>::Remove (the command list of the command page, the replay files' list; the three
// checks are UnTemplate.h 515 .. 517)
NW_HOOK_METHOD(0x100f7b80, TBgmStringArray, Remove)

// 0x100f7c20 - ~TArray<FString>: all the strings go, then the array
NW_HOOK_DTOR(0x100f7c20, TBgmStringArray)

// 0x10148f30 (cdecl, the Size is not read) - operator new(size_t, TArray<FString>&): room for one more string, its address
static void* NewStringArrayElement(size_t Size, TBgmStringArray& Array)
{
    INT Index = Array.Add();
    return &Array(Index);
}
NW_HOOK_FUNC(0x10148f30, NewStringArrayElement)

NW_CHECK_VTABLE(NCBroadcastGMWnd, 0x101d2838, 74)
