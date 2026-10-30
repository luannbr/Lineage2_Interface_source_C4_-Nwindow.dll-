/*=============================================================================
    NCBroadcastObserverWnd.cpp - the match broadcast screen of the observer (C3 only; C3 addresses). Stock:
    0x10117BE0 .. 0x10119A0D: the Notify... functions, ShowPartyWnd, LockPosition, SwitchMsgMode, the selection
    functions, SetWindowSize, InitPosition, ShowWindow, HideWindow, OnDestroy, OnTimer, OnCreate, the constructor
    0x10119860 and the destructor 0x10119990. SetTeamState (0x10117D80) and ToggleFlag (0x10118510) are in
    NConsoleWndMisc.cpp (the `core` lane's file); the anonymous pieces 0x10118040 / 0x10118050 (NotifyUserInfo's loops)
    and 0x10119328 (NotifySpellInfoStart's) are parts of those functions.
=============================================================================*/
#include "nwindow/NCBroadcastObserverWnd.h"
#include "nwindow/NCBroadcastGMWnd.h"
#include "nwindow/NCBroadcastObserverMsgWnd.h"
#include "nwindow/NCBroadcastObserverPartyListWnd.h"
#include "nwindow/NCBroadcastObserverPlayerWnd.h"
#include "nwindow/NCBroadcastObserverTopWnd.h"
#include "nwindow/NConsoleWnd.h"

enum
{
    TIMER_ObserverSecond = 0xFEFAF001,   // counts the spell times down (name ours)
    OBS_Teams = 2,
    OBS_Slots = 9,
    OBS_StyleDragable = 0x400000,        // NWS_DRAGABLE
    OBS_EffectTeam0 = 4,                 // the appendix visual effects the roster marks (values from the calls; names unknown)
    OBS_EffectTeam1 = 8,
    OBS_EffectSelected0 = 0x10,
    OBS_EffectSelected1 = 0x20,
};

// The fields of APawn the observer sets (past the end of the declared class; names ours): bit 0 of the flags at +0x1744
// marks the followed player, +0x1748 his team, +0x174C his slot (C4: the C3 numbers were +0x16C0 / +0x16C4 / +0x16C8; the
// stock NotifyCharInfo 0x1012E850 stores the slot at [pawn + 0x174C] and the team at [pawn + 0x1748], HideWindow
// 0x1012ED70 clears bit 0 of [pawn + 0x1744])
static FORCEINLINE DWORD& PawnFlags(APawn* Pawn)
{
    return ::nw::Field<DWORD>(Pawn, 0x1744);
}
static FORCEINLINE INT& PawnTeam(APawn* Pawn)
{
    return ::nw::Field<INT>(Pawn, 0x1748);
}
static FORCEINLINE INT& PawnSlot(APawn* Pawn)
{
    return ::nw::Field<INT>(Pawn, 0x174C);
}

// A child window goes (repeated inline in the stock OnDestroy)
template <class T> static FORCEINLINE void DestroyChild(NCWnd* Parent, T*& Child)
{
    if (Child)
    {
        Parent->RemoveChildWindow(Child->Entry);
        delete Child;
        Child = NULL;
    }
}

// 0x10117dc0 .. : the stock team constructor (0x10125E40: the name and the nine players; NCBroadcastGMWnd.cpp's NBroadcastGMTeam, the same
// folded function) and destructor (0x10127BA0)
NBroadcastTeams::NBroadcastTeams()
{
    new ((void*)Raw) NBroadcastGMTeam();
    new ((void*)(Raw + 0x2E0)) NBroadcastGMTeam();
}
NBroadcastTeams::~NBroadcastTeams()
{
    ((NBroadcastGMTeam*)(Raw + 0x2E0))->~NBroadcastGMTeam();
    ((NBroadcastGMTeam*)Raw)->~NBroadcastGMTeam();
}

// 0x1012f5d0 (guard: the constructor's own name) - no children yet, nothing followed (-1), the party rows show the
// class; the one second timer starts
NCBroadcastObserverWnd::NCBroadcastObserverWnd()
{
    guard(NCBroadcastObserverWnd::NCBroadcastObserverWnd);
    TopWnd = NULL;
    for (INT i = 0; i < OBS_Teams; i++)
    {
        PlayerWnd[i] = NULL;
        PartyListWnd[i] = NULL;
    }
    MsgWnd = NULL;
    FollowTeamID = -1;
    UpdateTeam = -1;
    SelectedSlot = -1;
    SelectedTeam = -1;
    PartyShowMode = 1;
    TimerID = SetTimer(TIMER_ObserverSecond, 1000, 0);
    unguard;
}
NW_HOOK_CTOR(0x1012f5d0, NCBroadcastObserverWnd)

// 0x1012f700 - the two rosters go (in reverse), then NCWnd's
NW_HOOK_DTOR(0x1012f700, NCBroadcastObserverWnd)


// 0x10148f10 (vslot 25) - NControl::WindowProc's body, which the stock vtables of these windows share (the C3 address was
// 0x10119f60): NCWnd's, normalized to 0 or 1
INT NCBroadcastObserverWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return NCWnd::WindowProc(Msg, wParam, lParam) != 0;
}

// 0x10003360 (vslot 56) - a body shared by many classes: returns 1
INT NCBroadcastObserverWnd::OnPaint(NCGDevice* Device)
{
    return 1;
}

// 0x1012e540 (anonymous, name ours; ret 4) - a team's score (0 for a team out of range)
INT NCBroadcastObserverWnd::GetTeamScore(INT Team)
{
    if (Team < 0 || Team >= OBS_Teams)
        return 0;
    return Teams[Team].Score;
}
NW_HOOK_METHOD(0x1012e540, NCBroadcastObserverWnd, GetTeamScore)

// 0x1012e620 (anonymous, name ours; ret 8) - the roster line of a team's slot; NULL when out of range or free (-1)
NBroadcastUser* NCBroadcastObserverWnd::GetUser(INT Team, INT Slot)
{
    if (Team < 0 || Team >= OBS_Teams || Slot < 0 || Slot >= OBS_Slots)
        return NULL;
    NBroadcastUser* User = &Teams[Team].Users[Slot];
    return User->ID == -1 ? NULL : User;
}
NW_HOOK_METHOD(0x1012e620, NCBroadcastObserverWnd, GetUser)

// 0x1012e7e0 (anonymous, name ours; ret 0xC) - the roster line of an object id and where it is (either place may be NULL)
NBroadcastUser* NCBroadcastObserverWnd::FindUser(INT UserID, INT* OutTeam, INT* OutSlot)
{
    for (INT Team = 0; Team < OBS_Teams; Team++)
    {
        for (INT Slot = 0; Slot < OBS_Slots; Slot++)
        {
            if (Teams[Team].Users[Slot].ID == UserID)
            {
                if (OutTeam)
                    *OutTeam = Team;
                if (OutSlot)
                    *OutSlot = Slot;
                return &Teams[Team].Users[Slot];
            }
        }
    }
    return NULL;
}
NW_HOOK_METHOD(0x1012e7e0, NCBroadcastObserverWnd, FindUser)

// 0x1012e570 (guard: NCBroadcastObserverWnd::GetTeamName; ret 8: the string comes back by the hidden pointer) - a team's
// name, an empty string for a team out of 0..1
FString NCBroadcastObserverWnd::GetTeamName(INT Team)
{
    guard(NCBroadcastObserverWnd::GetTeamName);
    if (Team >= 0 && Team < OBS_Teams)
        return Teams[Team].Name;
    return FString(TEXT(""));
    unguard;
}
NW_HOOK_METHOD(0x1012e570, NCBroadcastObserverWnd, GetTeamName)

// 0x1012e4a0 (guard: NCBroadcastObserverWnd::GetSelectedUserIndex; ret 4) - the slot the team's player window shows
// (-1 for a team out of 0..1)
INT NCBroadcastObserverWnd::GetSelectedUserIndex(INT Team)
{
    guard(NCBroadcastObserverWnd::GetSelectedUserIndex);
    if (Team >= 0 && Team < OBS_Teams)
        return PlayerWnd[Team]->UserSlot;
    return -1;
    unguard;
}
NW_HOOK_METHOD(0x1012e4a0, NCBroadcastObserverWnd, GetSelectedUserIndex)

// 0x1012e670 (guard: NCBroadcastObserverWnd::IsSelected; ret 8) - whether the team's player window shows the slot
INT NCBroadcastObserverWnd::IsSelected(INT Team, INT Slot)
{
    guard(NCBroadcastObserverWnd::IsSelected);
    if (Team >= 0 && Team < OBS_Teams && Slot >= 0 && Slot < OBS_Slots)
        return PlayerWnd[Team]->UserSlot == Slot;
    return 0;
    unguard;
}
NW_HOOK_METHOD(0x1012e670, NCBroadcastObserverWnd, IsSelected)

// 0x1012e330 (guard: NCBroadcastObserverWnd::SetSelecteUser; ret 8) - the team's player window shows another slot: the
// pawn of the one it showed loses its mark (and its selection effect, 0x10 for the first team, 0x20 for the second),
// the new one gets both
void NCBroadcastObserverWnd::SetSelecteUser(INT Team, INT Slot)
{
    guard(NCBroadcastObserverWnd::SetSelecteUser);
    if (Team >= 0 && Team < OBS_Teams)
    {
        INT Old = PlayerWnd[Team]->UserSlot;
        if (Old != -1)
        {
            User* OldUser = GConsole->Engine->NetHandler->GetUser(Teams[Team].Users[Old].ID);
            if (OldUser)
            {
                PawnFlags(OldUser->Pawn) &= ~1;
                if (Team == 0)
                    OldUser->Pawn->DeleteAppendixEffect((AppendixVisualEffectBitType)OBS_EffectSelected0);
                else if (Team == 1)
                    OldUser->Pawn->DeleteAppendixEffect((AppendixVisualEffectBitType)OBS_EffectSelected1);
            }
        }
        PlayerWnd[Team]->SetUserIndex(Slot);
        User* NewUser = GConsole->Engine->NetHandler->GetUser(Teams[Team].Users[Slot].ID);
        if (NewUser)
        {
            PawnFlags(NewUser->Pawn) |= 1;
            if (Team == 0)
                NewUser->Pawn->AddAppendixEffect((AppendixVisualEffectBitType)OBS_EffectSelected0);
            else if (Team == 1)
                NewUser->Pawn->AddAppendixEffect((AppendixVisualEffectBitType)OBS_EffectSelected1);
        }
    }
    unguard;
}
NW_HOOK_METHOD(0x1012e330, NCBroadcastObserverWnd, SetSelecteUser)

// 0x1012db00 (anonymous, name ours; ret 0xC, no guard) - for the followed team the roster being sent is team 0 (State 1) or
// team 1 (State 2); the third value is not read. The console's packet handler (NConsoleWndMisc.cpp) calls it by address
void NCBroadcastObserverWnd::SetTeamState(INT TeamID, INT State, INT Unused)
{
    if (FollowTeamID == TeamID)
    {
        BYTE Mode = (BYTE)State;
        if (Mode == 1)
            UpdateTeam = 0;
        else if (Mode == 2)
            UpdateTeam = 1;
    }
}
NW_HOOK_METHOD(0x1012db00, NCBroadcastObserverWnd, SetTeamState)

// 0x1012d960 (guard: NCBroadcastObserverWnd::NotifyStartObserver; ret 0x1C) - the match starts: the followed team and the
// two names are kept, the scores are 0 and every roster slot is free
void NCBroadcastObserverWnd::NotifyStartObserver(INT TeamID, FString Team0Name, FString Team1Name)
{
    guard(NCBroadcastObserverWnd::NotifyStartObserver);
    FollowTeamID = TeamID;
    Teams[0].Name = Team0Name;
    Teams[1].Name = Team1Name;
    Teams[0].Score = 0;
    Teams[1].Score = 0;
    for (INT Team = 0; Team < OBS_Teams; Team++)
    {
        for (INT Slot = 0; Slot < OBS_Slots; Slot++)
            Teams[Team].Users[Slot].ID = -1;
    }
    unguard;
}
NW_HOOK_METHOD(0x1012d960, NCBroadcastObserverWnd, NotifyStartObserver)

// 0x1012da60 (guard: NCBroadcastObserverWnd::NotifyScore; ret 0xC) - the scores of the followed team's match, the score
// bar slides in
void NCBroadcastObserverWnd::NotifyScore(INT TeamID, INT Score0, INT Score1)
{
    guard(NCBroadcastObserverWnd::NotifyScore);
    if (FollowTeamID == TeamID)
    {
        Teams[0].Score = Score0;
        Teams[1].Score = Score1;
        TopWnd->ShowWindow();
    }
    unguard;
}
NW_HOOK_METHOD(0x1012da60, NCBroadcastObserverWnd, NotifyScore)

// 0x1012db40 (guard: NCBroadcastObserverWnd::NotifyTeamInfo; ret 0x3C) - one roster line of the team being sent
// (UpdateTeam): the object id, the name and the figures (the tenth value is not read; the first is kept twice)
void NCBroadcastObserverWnd::NotifyTeamInfo(INT Slot, INT UserID, FString Name, INT P1, INT P2, INT P3, INT P4, INT P5, INT P6, INT P7, INT P8, INT P9, INT P10)
{
    guard(NCBroadcastObserverWnd::NotifyTeamInfo);
    if (UpdateTeam == 0 || UpdateTeam == 1)
    {
        if (Slot >= 0 && Slot < OBS_Slots)
        {
            NBroadcastUser* User = &Teams[UpdateTeam].Users[Slot];
            User->ID = UserID;
            User->Name = Name;
            User->MaxHP = P2;
            User->MP = P3;
            User->MaxMP = P4;
            User->CP = P5;
            User->MaxCP = P6;
            User->Level = P7;
            User->HP = P1;
            User->ClassID = P8;
            User->Unknown30 = P9;
            User->Unknown34 = P1;
        }
    }
    unguard;
}
NW_HOOK_METHOD(0x1012db40, NCBroadcastObserverWnd, NotifyTeamInfo)

// 0x1012dc40 (guard: NCBroadcastObserverWnd::NotifyTeamInfoEnd) - the roster of the team being sent is complete: its
// party list shows the rows with a player and every pawn gets its slot, its team and the team's effect (4 / 8)
void NCBroadcastObserverWnd::NotifyTeamInfoEnd()
{
    guard(NCBroadcastObserverWnd::NotifyTeamInfoEnd);
    if (UpdateTeam == 0 || UpdateTeam == 1)
    {
        PartyListWnd[UpdateTeam]->RefreshShowStatus();
        for (INT Slot = 0; Slot < OBS_Slots; Slot++)
        {
            User* Player = GConsole->Engine->NetHandler->GetUser(Teams[UpdateTeam].Users[Slot].ID);
            if (Player && Player->Pawn)
            {
                PawnSlot(Player->Pawn) = Slot;
                if (UpdateTeam == 0)
                {
                    PawnTeam(Player->Pawn) = 0;
                    Player->Pawn->AddAppendixEffect((AppendixVisualEffectBitType)OBS_EffectTeam0);
                }
                else
                {
                    PawnTeam(Player->Pawn) = 1;
                    Player->Pawn->AddAppendixEffect((AppendixVisualEffectBitType)OBS_EffectTeam1);
                }
            }
        }
        UpdateTeam = -1;
    }
    unguard;
}
NW_HOOK_METHOD(0x1012dc40, NCBroadcastObserverWnd, NotifyTeamInfoEnd)

// 0x1012dd90 (guard: NCBroadcastObserverWnd::NotifyUserInfo; ret 0x30) - new figures for the roster line of an object id
// (the stock function jumps to its loops, 0x1012ddc0 / 0x1012ddd0)
void NCBroadcastObserverWnd::NotifyUserInfo(INT UserID, FString Name, INT P1, INT P2, INT P3, INT P4, INT P5, INT P6, INT P7, INT P8)
{
    guard(NCBroadcastObserverWnd::NotifyUserInfo);
    for (INT Team = 0; Team < OBS_Teams; Team++)
    {
        for (INT Slot = 0; Slot < OBS_Slots; Slot++)
        {
            NBroadcastUser* User = &Teams[Team].Users[Slot];
            if (User->ID == UserID)
            {
                User->Name = Name;
                User->HP = P1;
                User->MaxHP = P2;
                User->MP = P3;
                User->MaxMP = P4;
                User->CP = P5;
                User->MaxCP = P6;
                User->Level = P7;
                User->ClassID = P8;
                return;
            }
        }
    }
    unguard;
}
NW_HOOK_METHOD(0x1012dd90, NCBroadcastObserverWnd, NotifyUserInfo)

// 0x1012f030 (guard: NCBroadcastObserverWnd::NotifySpellInfoStart; ret 0xC; the second half is 0x1012f078) - the effects
// of one player are about to be sent: the selection is cleared, and for the followed team's match the player's lists
// of effects are emptied and remembered as the one being filled
void NCBroadcastObserverWnd::NotifySpellInfoStart(INT TeamID, INT UserID, INT Unused)
{
    guard(NCBroadcastObserverWnd::NotifySpellInfoStart);
    SelectedTeam = -1;
    SelectedSlot = -1;
    if (FollowTeamID == TeamID)
    {
        for (INT Team = 0; Team < OBS_Teams; Team++)
        {
            for (INT Slot = 0; Slot < OBS_Slots; Slot++)
            {
                if (Teams[Team].Users[Slot].ID == UserID)
                {
                    SelectedTeam = Team;
                    SelectedSlot = Slot;
                    Teams[Team].Users[Slot].SpellIDs.Empty();
                    Teams[SelectedTeam].Users[SelectedSlot].SpellTimes.Empty();
                    return;
                }
            }
        }
    }
    unguard;
}
NW_HOOK_METHOD(0x1012f030, NCBroadcastObserverWnd, NotifySpellInfoStart)

// 0x1012f150 (guard: NCBroadcastObserverWnd::NotifySpellInfo; ret 0x10) - one effect on the player being filled: its
// skill id and its remaining seconds (the other two values are not read)
void NCBroadcastObserverWnd::NotifySpellInfo(INT Unused1, INT SpellID, INT Unused2, INT RemainTime)
{
    guard(NCBroadcastObserverWnd::NotifySpellInfo);
    if (SelectedTeam != -1 && SelectedSlot != -1)
    {
        NBroadcastUser* User = &Teams[SelectedTeam].Users[SelectedSlot];
        User->SpellIDs(User->SpellIDs.Add()) = SpellID;
        User = &Teams[SelectedTeam].Users[SelectedSlot];
        User->SpellTimes(User->SpellTimes.Add()) = RemainTime;
    }
    unguard;
}
NW_HOOK_METHOD(0x1012f150, NCBroadcastObserverWnd, NotifySpellInfo)

// 0x1012dec0 (guard: NCBroadcastObserverWnd::NotifySpellInfoEnd) - the player's effects are complete: his player window
// reads them, and nobody is being filled
void NCBroadcastObserverWnd::NotifySpellInfoEnd()
{
    guard(NCBroadcastObserverWnd::NotifySpellInfoEnd);
    if (SelectedTeam != -1 && SelectedSlot != -1)
    {
        if (PlayerWnd[SelectedTeam])
            PlayerWnd[SelectedTeam]->RefreshBuffWnd();
        SelectedTeam = -1;
        SelectedSlot = -1;
    }
    unguard;
}
NW_HOOK_METHOD(0x1012dec0, NCBroadcastObserverWnd, NotifySpellInfoEnd)

// 0x1012df60 (guard: NCBroadcastObserverWnd::NotifyTeamUnlocked; ret 8) - a team's roster is freed (Which 1 the first
// team's, 2 the second's, in the followed match): every pawn loses its mark and its effects, the slots are free and
// the party list is redone
void NCBroadcastObserverWnd::NotifyTeamUnlocked(INT TeamID, INT Which)
{
    guard(NCBroadcastObserverWnd::NotifyTeamUnlocked);
    if (FollowTeamID == TeamID)
    {
        if ((BYTE)Which == 1)
        {
            for (INT Slot = 0; Slot < OBS_Slots; Slot++)
            {
                User* Player = GConsole->Engine->NetHandler->GetUser(Teams[0].Users[Slot].ID);
                if (Player && Player->Pawn)
                {
                    PawnFlags(Player->Pawn) &= ~1;
                    Player->Pawn->DeleteAllAppendixEffect();
                }
                Teams[0].Users[Slot].ID = -1;
            }
            PartyListWnd[0]->RefreshShowStatus();
        }
        else if ((BYTE)Which == 2)
        {
            for (INT Slot = 0; Slot < OBS_Slots; Slot++)
            {
                User* Player = GConsole->Engine->NetHandler->GetUser(Teams[1].Users[Slot].ID);
                if (Player && Player->Pawn)
                {
                    PawnFlags(Player->Pawn) &= ~1;
                    Player->Pawn->DeleteAllAppendixEffect();
                }
                Teams[1].Users[Slot].ID = -1;
            }
            PartyListWnd[1]->RefreshShowStatus();
        }
    }
    unguard;
}
NW_HOOK_METHOD(0x1012df60, NCBroadcastObserverWnd, NotifyTeamUnlocked)

// 0x1012e0b0 (guard: NCBroadcastObserverWnd::ShowPartyWnd) - the two party lists toggle together
void NCBroadcastObserverWnd::ShowPartyWnd()
{
    guard(NCBroadcastObserverWnd::ShowPartyWnd);
    if (PartyListWnd[0]->IsShowWindow())
    {
        PartyListWnd[0]->HideWindow();
        PartyListWnd[1]->HideWindow();
    }
    else
    {
        PartyListWnd[0]->ShowWindow();
        PartyListWnd[1]->ShowWindow();
    }
    unguard;
}
NW_HOOK_METHOD(0x1012e0b0, NCBroadcastObserverWnd, ShowPartyWnd)

// 0x1012e170 (guard: NCBroadcastObserverWnd::LockPosition) - the player windows and the party lists toggle between
// draggable and fixed (the first player window tells which they are now)
void NCBroadcastObserverWnd::LockPosition()
{
    guard(NCBroadcastObserverWnd::LockPosition);
    if (PlayerWnd[0])
    {
        if (PlayerWnd[0]->IsDragable())
        {
            for (INT i = 0; i < OBS_Teams; i++)
            {
                if (PlayerWnd[i])
                    PlayerWnd[i]->SetStyle(PlayerWnd[i]->NCWnd::GetStyle() & ~OBS_StyleDragable);
                if (PartyListWnd[i])
                    PartyListWnd[i]->SetStyle(PartyListWnd[i]->NCWnd::GetStyle() & ~OBS_StyleDragable);
            }
        }
        else
        {
            for (INT i = 0; i < OBS_Teams; i++)
            {
                if (PlayerWnd[i])
                    PlayerWnd[i]->SetStyle(PlayerWnd[i]->NCWnd::GetStyle() | OBS_StyleDragable);
                if (PartyListWnd[i])
                    PartyListWnd[i]->SetStyle(PartyListWnd[i]->NCWnd::GetStyle() | OBS_StyleDragable);
            }
        }
    }
    unguard;
}
NW_HOOK_METHOD(0x1012e170, NCBroadcastObserverWnd, LockPosition)

// 0x1012e2b0 (guard: NCBroadcastObserverWnd::SwitchMsgMode) - the message log shows its next view
void NCBroadcastObserverWnd::SwitchMsgMode()
{
    guard(NCBroadcastObserverWnd::SwitchMsgMode);
    if (MsgWnd)
        MsgWnd->SwitchMode();
    unguard;
}
NW_HOOK_METHOD(0x1012e2b0, NCBroadcastObserverWnd, SwitchMsgMode)

// 0x1012e850 (guard: NCBroadcastObserverWnd::NotifyCharInfo; ret 4) - a character the console learned of: when he is on a
// roster his pawn gets his slot, his team and the team's effect
void NCBroadcastObserverWnd::NotifyCharInfo(User* Info)
{
    guard(NCBroadcastObserverWnd::NotifyCharInfo);
    if (Info)
    {
        INT Team = -1;
        INT Slot = -1;
        if (FindUser(Info->ObjectID, &Team, &Slot))
        {
            if (Info->Pawn)
            {
                PawnSlot(Info->Pawn) = Slot;
                if (Team == 0)
                {
                    PawnTeam(Info->Pawn) = 0;
                    Info->Pawn->AddAppendixEffect((AppendixVisualEffectBitType)OBS_EffectTeam0);
                }
                else
                {
                    PawnTeam(Info->Pawn) = 1;
                    Info->Pawn->AddAppendixEffect((AppendixVisualEffectBitType)OBS_EffectTeam1);
                }
            }
        }
    }
    unguard;
}
NW_HOOK_METHOD(0x1012e850, NCBroadcastObserverWnd, NotifyCharInfo)

// 0x1012e930 (guard: NCBroadcastObserverWnd::NotifyMagicSkillUse; ret 0xC) - a skill was used by a player on another: a
// line in the message log (the two names, their places on the rosters and the skill's name from the skill table, level 1)
void NCBroadcastObserverWnd::NotifyMagicSkillUse(INT CasterID, INT TargetID, INT SkillID)
{
    guard(NCBroadcastObserverWnd::NotifyMagicSkillUse);
    INT CasterTeam = -1;
    INT CasterSlot = -1;
    NBroadcastUser* Caster = FindUser(CasterID, &CasterTeam, &CasterSlot);
    if (Caster)
    {
        INT TargetTeam = -1;
        INT TargetSlot = -1;
        NBroadcastUser* Target = FindUser(TargetID, &TargetTeam, &TargetSlot);
        if (Target && SkillID >= 0 && SkillID <= 1999)
        {
            FL2MagicSkillData* Skill = GL2GameData.GetMSData(SkillID, 1);
            if (Skill)
                MsgWnd->AddSkillMsg(Caster->Name, CasterTeam, CasterSlot, Target->Name, TargetTeam, TargetSlot, Skill->Name);
        }
    }
    unguard;
}
// (wave A switched this hook off while AddSkillMsg 0x10129B80 was an unported stub; it is ported in NCBroadcastObserverMsgWnd.cpp)
NW_HOOK_METHOD(0x1012e930, NCBroadcastObserverWnd, NotifyMagicSkillUse)

// 0x1012ea60 (guard: NCBroadcastObserverWnd::SetUpTarget) - the console's target, when he is on a roster, becomes the
// followed player of his team
void NCBroadcastObserverWnd::SetUpTarget()
{
    guard(NCBroadcastObserverWnd::SetUpTarget);
    User* Target = GConsole->GetSelectedActor();
    if (Target)
    {
        INT Team = -1;
        INT Slot = -1;
        if (FindUser(Target->ObjectID, &Team, &Slot))
            SetSelecteUser(Team, Slot);
    }
    unguard;
}
NW_HOOK_METHOD(0x1012ea60, NCBroadcastObserverWnd, SetUpTarget)

// 0x1012eb10 (guard: NCBroadcastObserverWnd::InitPosition) - the first player window back at the left edge (y 98), the
// second at the right (520 from the edge), the party lists at the left and 280 from the right edge (y 340)
void NCBroadcastObserverWnd::InitPosition()
{
    guard(NCBroadcastObserverWnd::InitPosition);
    if (PlayerWnd[0])
        PlayerWnd[0]->MoveWindow(-PlayerWnd[0]->ScreenX, 0x62 - PlayerWnd[0]->ScreenY);
    if (PlayerWnd[1])
        PlayerWnd[1]->MoveWindow((INT)(Width - 520.0f - PlayerWnd[1]->ScreenX), 0x62 - PlayerWnd[1]->ScreenY);
    for (INT i = 0; i < OBS_Teams; i++)
    {
        INT X = 0;
        if (i == 1)
            X = (INT)(Width - 280.0f);
        if (PartyListWnd[i])
            PartyListWnd[i]->MoveWindow(X - PartyListWnd[i]->ScreenX, 0x154 - PartyListWnd[i]->ScreenY);
    }
    unguard;
}
NW_HOOK_METHOD(0x1012eb10, NCBroadcastObserverWnd, InitPosition)

// 0x1012e720 (vslot 43, guard: NCBroadcastObserverWnd::SetWindowSize; ret 8) - the score bar takes the width less 120, the
// message log the width (the heights stay: 113 and 236)
void NCBroadcastObserverWnd::SetWindowSize(FLOAT W, FLOAT H)
{
    guard(NCBroadcastObserverWnd::SetWindowSize);
    NCWnd::SetWindowSize(W, H);
    if (TopWnd)
        TopWnd->SetWindowSize(W - 120.0f, 113.0f);
    if (MsgWnd)
        MsgWnd->SetWindowSize(W, 236.0f);
    unguard;
}
NW_HOOK_METHOD(0x1012e720, NCBroadcastObserverWnd, SetWindowSize)

// 0x1012ec40 (vslot 48, guard: NCBroadcastObserverWnd::ShowWindow) - NCWnd's; the player's controller in the broadcast
// observer mode, every pawn of the rosters marked (the effect of its team) and the followed player of each team selected
// again. C4: the match mark (GIsL2Mark) is no longer touched here (the console keeps it: NConsoleWnd::SaveL2Mark)
void NCBroadcastObserverWnd::ShowWindow()
{
    guard(NCBroadcastObserverWnd::ShowWindow);
    NCWnd::ShowWindow();
    ALineagePlayerController* Controller = CastToLineagePlayerController(GConsole->Engine->Client->Viewports(0)->Actor);
    if (Controller)
        Controller->SetBroadcastObserverMode(1);
    for (INT Team = 0; Team < OBS_Teams; Team++)
    {
        for (INT Slot = 0; Slot < OBS_Slots; Slot++)
        {
            User* Player = GConsole->Engine->NetHandler->GetUser(Teams[Team].Users[Slot].ID);
            if (Player && Player->Pawn)
            {
                if (Team == 0)
                    Player->Pawn->AddAppendixEffect((AppendixVisualEffectBitType)OBS_EffectTeam0);
                else
                    Player->Pawn->AddAppendixEffect((AppendixVisualEffectBitType)OBS_EffectTeam1);
            }
        }
        SetSelecteUser(Team, GetSelectedUserIndex(Team));
    }
    unguard;
}
NW_HOOK_METHOD(0x1012ec40, NCBroadcastObserverWnd, ShowWindow)

// 0x1012ed70 (vslot 49, guard: NCBroadcastObserverWnd::HideWindow) - the opposite: the controller out of the observer
// mode, every pawn of the rosters loses its mark and its effects (C4: no GIsL2Mark here either)
void NCBroadcastObserverWnd::HideWindow()
{
    guard(NCBroadcastObserverWnd::HideWindow);
    NCWnd::HideWindow();
    ALineagePlayerController* Controller = CastToLineagePlayerController(GConsole->Engine->Client->Viewports(0)->Actor);
    if (Controller)
        Controller->SetBroadcastObserverMode(0);
    for (INT Team = 0; Team < OBS_Teams; Team++)
    {
        for (INT Slot = 0; Slot < OBS_Slots; Slot++)
        {
            User* Player = GConsole->Engine->NetHandler->GetUser(Teams[Team].Users[Slot].ID);
            if (Player && Player->Pawn)
            {
                PawnFlags(Player->Pawn) &= ~1;
                Player->Pawn->DeleteAllAppendixEffect();
            }
        }
    }
    unguard;
}
NW_HOOK_METHOD(0x1012ed70, NCBroadcastObserverWnd, HideWindow)

// 0x1012efd0 (vslot 36) - every second the remaining seconds of every effect on every player go down by one
INT NCBroadcastObserverWnd::OnTimer(INT ID)
{
    if (ID == TIMER_ObserverSecond)
    {
        for (INT Team = 0; Team < OBS_Teams; Team++)
        {
            for (INT Slot = 0; Slot < OBS_Slots; Slot++)
            {
                NBroadcastUser* User = &Teams[Team].Users[Slot];
                for (INT i = 0; i < User->SpellTimes.Num(); i++)
                    User->SpellTimes(i)--;
            }
        }
    }
    return 1;
}
NW_HOOK_METHOD(0x1012efd0, NCBroadcastObserverWnd, OnTimer)

// 0x1012ee80 (vslot 38, guard: NCBroadcastObserverWnd::OnDestroy) - the score bar, the two player windows, the two party
// lists and the message log go, then NCWnd's
INT NCBroadcastObserverWnd::OnDestroy()
{
    guard(NCBroadcastObserverWnd::OnDestroy);
    DestroyChild(this, TopWnd);
    for (INT i = 0; i < OBS_Teams; i++)
        DestroyChild(this, PlayerWnd[i]);
    for (INT i = 0; i < OBS_Teams; i++)
        DestroyChild(this, PartyListWnd[i]);
    DestroyChild(this, MsgWnd);
    NCWnd::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1012ee80, NCBroadcastObserverWnd, OnDestroy)

// 0x1012f240 (vslot 37, guard: NCBroadcastObserverWnd::OnCreate) - the score bar (from x 60, as wide as the window less
// 120), the two player windows (520 x 224 at y 98; the second 520 from the right edge), the two party lists (280 x 630 at
// y 340; the second 280 from the right edge), both draggable, and the message log (the width, 236 high) at the bottom
INT NCBroadcastObserverWnd::OnCreate()
{
    guard(NCBroadcastObserverWnd::OnCreate);
    NCWnd::OnCreate();
    TopWnd = new NCBroadcastObserverTopWnd(this);
    TopWnd->SetPosition(60, 0, (INT)(Width - 120.0f), 0x71);
    CreateChild(TopWnd, NWS_VISIBLE);
    for (INT Team = 0; Team < OBS_Teams; Team++)
    {
        PlayerWnd[Team] = new NCBroadcastObserverPlayerWnd(this, Team);
        CreateChild(PlayerWnd[Team], NWS_VISIBLE | OBS_StyleDragable);
        if (Team == 0)
            PlayerWnd[0]->SetPosition(0, 0x62, 0x208, 0xE0);
        else if (Team == 1)
            PlayerWnd[1]->SetPosition((INT)(Width - 520.0f), 0x62, 0x208, 0xE0);
    }
    for (INT Team = 0; Team < OBS_Teams; Team++)
    {
        INT X = 0;
        if (Team == 1)
            X = (INT)(Width - 280.0f);
        PartyListWnd[Team] = new NCBroadcastObserverPartyListWnd(Team, this);
        PartyListWnd[Team]->SetPosition(X, 0x154, 0x118, 0x276);
        CreateChild(PartyListWnd[Team], NWS_VISIBLE | OBS_StyleDragable);
    }
    MsgWnd = new NCBroadcastObserverMsgWnd;
    MsgWnd->SetPosition(0, 0x3CB, (INT)Width, 0xEC);
    CreateChild(MsgWnd, NWS_VISIBLE);
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1012f240, NCBroadcastObserverWnd, OnCreate)

NW_CHECK_VTABLE(NCBroadcastObserverWnd, 0x101d57b8, 74)
