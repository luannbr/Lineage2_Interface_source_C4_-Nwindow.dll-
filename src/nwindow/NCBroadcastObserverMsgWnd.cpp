/*=============================================================================
    NCBroadcastObserverMsgWnd.cpp - the skill message log of the broadcast observer screen (C3 only; C3 addresses).
    Stock: AddSkillMsg 0x10113DB0, SwitchMode 0x10114080, OnCreate 0x101140D0, OnDestroy 0x10114160, DrawSkillText
    0x101141E0, FindOldestLine 0x10114570, Clear 0x10114620, the line destructor 0x10114660, OnPaint 0x101146C0, OnTimer
    0x101148E0, the constructor 0x101149E0, the destructor 0x10114C10 and the line constructor 0x10113D50.
=============================================================================*/
#include "nwindow/NCBroadcastObserverMsgWnd.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NConsoleWnd.h"

enum
{
    TIMER_MsgSecond = 0xFEFFF001,   // the one second timer (name ours)
    MSG_Lines = 6,
    MSG_LineHeight = 0x1E,
    MSG_HoldSeconds = 10,
};

// An FString's three words, passed by value where the stock passes the string to a "%s" of FString::Printf (it copies
// them to the stack without the copy constructor)
struct NRawFString
{
    DWORD Data, Num, Max;
};

// 0x10129b20 - the three strings constructed (the numbers are left as allocated)
NBroadcastMsgLine::NBroadcastMsgLine()
{
}
NW_HOOK_CTOR(0x10129b20, NBroadcastMsgLine)

// 0x1012a430 - the three strings go in reverse
NBroadcastMsgLine::~NBroadcastMsgLine()
{
}
NW_HOOK_DTOR(0x1012a430, NBroadcastMsgLine)

// 0x1012a3f0 (anonymous) - a free line: the strings emptied, the numbers -1
void NBroadcastMsgLine::Clear()
{
    Caster = TEXT("");
    Target = TEXT("");
    Skill = TEXT("");
    CasterTeam = -1;
    TargetTeam = -1;
    CasterSlot = -1;
    TargetSlot = -1;
}
NW_HOOK_METHOD(0x1012a3f0, NBroadcastMsgLine, Clear)

// 0x1012a7b0 (guard: the constructor's own name) - the six lines of each list are free, nothing counts down, the
// one second timer starts
NCBroadcastObserverMsgWnd::NCBroadcastObserverMsgWnd()
{
    guard(NCBroadcastObserverMsgWnd::NCBroadcastObserverMsgWnd);
    BackTex = NULL;
    NextAll = 0;
    NextTeam0 = 0;
    NextTeam1 = 0;
    TimerID = 0;
    Mode = 0;
    for (INT i = 0; i < MSG_Lines; i++)
    {
        AllLines[i].Caster = TEXT("");
        AllLines[i].Target = TEXT("");
        AllLines[i].Skill = TEXT("");
        AllLines[i].CasterTeam = -1;
        AllLines[i].TargetTeam = -1;
        AllLines[i].CasterSlot = -1;
        AllLines[i].TargetSlot = -1;
        Team0Lines[i].Caster = TEXT("");
        Team0Lines[i].Target = TEXT("");
        Team0Lines[i].Skill = TEXT("");
        Team0Lines[i].CasterTeam = -1;
        Team0Lines[i].TargetTeam = -1;
        Team0Lines[i].CasterSlot = -1;
        Team0Lines[i].TargetSlot = -1;
        Team1Lines[i].Caster = TEXT("");
        Team1Lines[i].Target = TEXT("");
        Team1Lines[i].Skill = TEXT("");
        Team1Lines[i].CasterTeam = -1;
        Team1Lines[i].TargetTeam = -1;
        Team1Lines[i].CasterSlot = -1;
        Team1Lines[i].TargetSlot = -1;
    }
    Countdown = -1;
    TimerID = SetTimer(TIMER_MsgSecond, 1000, 0);
    unguard;
}
NW_HOOK_CTOR(0x1012a7b0, NCBroadcastObserverMsgWnd)

// 0x1012a9e0 - the three lists of lines go in reverse
NW_HOOK_DTOR(0x1012a9e0, NCBroadcastObserverMsgWnd)

// 0x10148f10 (vslot 25) - NControl::WindowProc's body, which the stock vtables of these windows share (the C3 address was
// 0x10119f60): NCWnd's, normalized to 0 or 1
INT NCBroadcastObserverMsgWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return NCWnd::WindowProc(Msg, wParam, lParam) != 0;
}

// 0x10129ea0 (vslot 37; the guard string is NCBroadcastObserverWnd::OnCreate, copied from the parent window) - the
// background texture
INT NCBroadcastObserverMsgWnd::OnCreate()
{
    guard(NCBroadcastObserverWnd::OnCreate);
    NCWnd::OnCreate();
    BackTex = FindTexture(TEXT("L2UI_CH3.BroadcastObs.br_msg3_back"));
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10129ea0, NCBroadcastObserverMsgWnd, OnCreate)

// 0x10129f30 (vslot 38; the guard string is NCBroadcastObserverWnd::OnDestroy) - nothing of its own to free
INT NCBroadcastObserverMsgWnd::OnDestroy()
{
    guard(NCBroadcastObserverWnd::OnDestroy);
    NCWnd::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10129f30, NCBroadcastObserverMsgWnd, OnDestroy)

// 0x10129e50 (anonymous, name ours) - the next view of the log: all lines -> the two teams -> hidden -> all lines
void NCBroadcastObserverMsgWnd::SwitchMode()
{
    switch (Mode)
    {
    case 0:
        Mode = 1;
        Countdown = MSG_HoldSeconds;
        break;
    case 1:
        Mode = 2;
        Countdown = -1;
        break;
    case 2:
        Mode = 0;
        Countdown = MSG_HoldSeconds;
        break;
    }
}
NW_HOOK_METHOD(0x10129e50, NCBroadcastObserverMsgWnd, SwitchMode)

// 0x1012a340 (anonymous, name ours; ret 4) - the oldest line in use of a list (all lines for -1, the first team's for 0,
// the second's for 1): the six slots are looked at from the one the next line goes to; NULL when all are free or the
// list is none of these
NBroadcastMsgLine* NCBroadcastObserverMsgWnd::FindOldestLine(INT Which)
{
    if (Which == -1)
    {
        for (INT i = 0, Index = NextAll; i < MSG_Lines; i++, Index++)
        {
            NBroadcastMsgLine* Line = &AllLines[Index % MSG_Lines];
            if (Line->CasterTeam != -1)
                return Line;
        }
    }
    else if (Which == 0)
    {
        for (INT i = 0, Index = NextTeam0; i < MSG_Lines; i++, Index++)
        {
            NBroadcastMsgLine* Line = &Team0Lines[Index % MSG_Lines];
            if (Line->CasterTeam != -1)
                return Line;
        }
        return NULL;
    }
    else if (Which == 1)
    {
        for (INT i = 0, Index = NextTeam1; i < MSG_Lines; i++, Index++)
        {
            NBroadcastMsgLine* Line = &Team1Lines[Index % MSG_Lines];
            if (Line->CasterTeam != -1)
                return Line;
        }
    }
    return NULL;
}
NW_HOOK_METHOD(0x1012a340, NCBroadcastObserverMsgWnd, FindOldestLine)

// 0x1012a6b0 (vslot 36, guard: NCBroadcastObserverMsgWnd::OnTimer; ret 4) - every second the countdown runs down; at
// zero the oldest line (of each team's list in Mode 1) is freed and the countdown starts over at ten seconds, or stops
// (-1) when nothing is left. Mode 2 shows nothing and frees nothing.
INT NCBroadcastObserverMsgWnd::OnTimer(INT ID)
{
    guard(NCBroadcastObserverMsgWnd::OnTimer);
    if (ID == TIMER_MsgSecond)
    {
        if (Countdown > 0)
            Countdown--;
        else if (Countdown == 0)
        {
            if (Mode == 0)
            {
                NBroadcastMsgLine* Line = FindOldestLine(-1);
                if (Line)
                {
                    Line->Clear();
                    Countdown = MSG_HoldSeconds;
                }
                else
                    Countdown = -1;
            }
            else if (Mode == 1)
            {
                NBroadcastMsgLine* First = FindOldestLine(0);
                NBroadcastMsgLine* Second = FindOldestLine(1);
                if (First)
                {
                    First->Clear();
                    if (Second)
                        Second->Clear();
                    Countdown = MSG_HoldSeconds;
                }
                else if (Second)
                {
                    Second->Clear();
                    Countdown = MSG_HoldSeconds;
                }
                else
                    Countdown = -1;
            }
        }
    }
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1012a6b0, NCBroadcastObserverMsgWnd, OnTimer)

// 0x10129b80 (guard: NCBroadcastObserverMsgWnd::AddSkillMsg; ret 0x34) - the line goes into the all-lines ring, and into
// the ring of its caster's team (the one for team 0 or 1); the countdown starts over at ten seconds
void NCBroadcastObserverMsgWnd::AddSkillMsg(FString Caster, INT CasterTeam, INT CasterSlot, FString Target, INT TargetTeam, INT TargetSlot, FString Skill)
{
    guard(NCBroadcastObserverMsgWnd::AddSkillMsg);
    AllLines[NextAll].Caster = Caster;
    AllLines[NextAll].Target = Target;
    AllLines[NextAll].Skill = Skill;
    AllLines[NextAll].CasterTeam = CasterTeam;
    AllLines[NextAll].TargetTeam = TargetTeam;
    AllLines[NextAll].CasterSlot = CasterSlot;
    AllLines[NextAll].TargetSlot = TargetSlot;
    NextAll = (NextAll + 1) % MSG_Lines;
    if (CasterTeam == 0)
    {
        Team0Lines[NextTeam0].Caster = Caster;
        Team0Lines[NextTeam0].Target = Target;
        Team0Lines[NextTeam0].Skill = Skill;
        Team0Lines[NextTeam0].CasterTeam = 0;
        Team0Lines[NextTeam0].TargetTeam = TargetTeam;
        Team0Lines[NextTeam0].CasterSlot = CasterSlot;
        Team0Lines[NextTeam0].TargetSlot = TargetSlot;
        NextTeam0 = (NextTeam0 + 1) % MSG_Lines;
    }
    else if (CasterTeam == 1)
    {
        Team1Lines[NextTeam1].Caster = Caster;
        Team1Lines[NextTeam1].Target = Target;
        Team1Lines[NextTeam1].Skill = Skill;
        Team1Lines[NextTeam1].CasterTeam = 1;
        Team1Lines[NextTeam1].TargetTeam = TargetTeam;
        Team1Lines[NextTeam1].CasterSlot = CasterSlot;
        Team1Lines[NextTeam1].TargetSlot = TargetSlot;
        NextTeam1 = (NextTeam1 + 1) % MSG_Lines;
    }
    Countdown = MSG_HoldSeconds;
    unguard;
}
NW_HOOK_METHOD(0x10129b80, NCBroadcastObserverMsgWnd, AddSkillMsg)

// 0x10129fb0 (guard: NCBroadcastObserverMsgWnd::DrawSkillText; ret 0x14) - see the declaration
void NCBroadcastObserverMsgWnd::DrawSkillText(NBroadcastMsgLine* Line, INT X, INT Y, INT bNames, NCGDevice* Device)
{
    guard(NCBroadcastObserverMsgWnd::DrawSkillText);
    FString Text;
    if (bNames)
        Text = FString::Printf(TEXT("%d%s>"), Line->CasterSlot + 1, *(NRawFString*)&Line->Caster);
    else
        Text = FString::Printf(TEXT("%d>"), Line->CasterSlot + 1);
    SIZE Size;
    NCGDevice::GetTextExtent(&Size, *Text, 0, 0, 0, 0, 0, (L2FontType)6);
    if (Line->CasterTeam == 0)
        Device->DrawText(X, Y, 0xFFFF3737, *Text, 0, 0, 0, (L2FontType)6, 0, 0, 0, 0, 0, 0, 0);
    else if (Line->CasterTeam == 1)
        Device->DrawText(X, Y, 0xFFDCDCDC, *Text, 0, 0, 0, (L2FontType)6, 0, 0, 0, 0, 0, 0, 0);
    X += Size.cx;

    FString Number;
    if (bNames)
        Number = FString::Printf(TEXT("%d%s"), Line->TargetSlot + 1, *(NRawFString*)&Line->Target);
    else
        Number = FString::Printf(TEXT("%d"), Line->TargetSlot + 1);
    NCGDevice::GetTextExtent(&Size, *Number, 0, 0, 0, 0, 0, (L2FontType)6);
    if (Line->TargetTeam == 0)
        Device->DrawText(X, Y, 0xFFFF3737, *Number, 0, 0, 0, (L2FontType)6, 0, 0, 0, 0, 0, 0, 0);
    else if (Line->TargetTeam == 1)
        Device->DrawText(X, Y, 0xFFDCDCDC, *Number, 0, 0, 0, (L2FontType)6, 0, 0, 0, 0, 0, 0, 0);
    X += Size.cx;

    FString Colon(TEXT(": "));
    NCGDevice::GetTextExtent(&Size, *Colon, 0, 0, 0, 0, 0, (L2FontType)6);
    Device->DrawText(X, Y, 0xFFFF3737, *Colon, 0, 0, 0, (L2FontType)6, 0, 0, 0, 0, 0, 0, 0);
    X += Size.cx;
    NCGDevice::GetTextExtent(&Size, *Colon, 0, 0, 0, 0, 0, (L2FontType)6);
    Device->DrawText(X, Y, 0xFFFFD23C, *Line->Skill, 0, 0, 0, (L2FontType)6, 0, 0, 0, 0, 0, 0, 0);
    unguard;
}
NW_HOOK_METHOD(0x10129fb0, NCBroadcastObserverMsgWnd, DrawSkillText)

// 0x1012a490 (vslot 62; the guard string is NCBroadcastObserverWnd::OnPaint) - the background, then the used lines of the
// Mode's list(s), 30 pixels apart from the oldest: all lines with the caster's and target's names (Mode 0), the first
// team's at the left and the second team's from the middle of the window (Mode 1, numbers only)
INT NCBroadcastObserverMsgWnd::OnPaint(NCGDevice* Device)
{
    guard(NCBroadcastObserverWnd::OnPaint);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    Device->BeginPaint(ScreenX, ScreenY);
    Device->DrawTexture(0, 0, (INT)Width, 0xEC, 0, 0, 0x20, 0x100, BackTex, 0xFF, 1);
    if (Mode == 0)
    {
        for (INT i = 0; i < MSG_Lines; i++)
        {
            NBroadcastMsgLine* Line = &AllLines[(NextAll + i) % MSG_Lines];
            if (!(Line->Caster == TEXT("")))
                DrawSkillText(Line, 0x14, i * MSG_LineHeight, 1, Device);
        }
    }
    else if (Mode == 1)
    {
        for (INT i = 0; i < MSG_Lines; i++)
        {
            NBroadcastMsgLine* Line = &Team0Lines[(NextTeam0 + i) % MSG_Lines];
            if (!(Line->Caster == TEXT("")))
                DrawSkillText(Line, 0x14, i * MSG_LineHeight, 0, Device);
        }
        for (INT i = 0; i < MSG_Lines; i++)
        {
            NBroadcastMsgLine* Line = &Team1Lines[(NextTeam1 + i) % MSG_Lines];
            if (!(Line->Caster == TEXT("")))
                DrawSkillText(Line, (INT)(Width * 0.5f + ScreenX + 20.0f), i * MSG_LineHeight, 0, Device);
        }
    }
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1012a490, NCBroadcastObserverMsgWnd, OnPaint)

NW_CHECK_VTABLE(NCBroadcastObserverMsgWnd, 0x101d3260, 74)
