/*=============================================================================
    NConsoleWndC4.cpp - the console's functions that only the C4 client has: the unnamed slots of its vtable (209 .. 210, 317, 318,
    369, 407 .. 447 and the like) and the small helpers around them. The engine class (UL2ConsoleWnd) declares those slots as
    NewVirtualN() (include/ue2/gen/UL2ConsoleWnd.vt.inc) and NNewVirtuals.gen.cpp gives them naked bodies that jump into the stock
    code, so the real signatures live here: a non-virtual method named after the slot (C4VirtualN, the guard string's name when
    the stock function has one), hooked at the slot's stock function. Offsets from the C4 disassembly; the windows behind them
    are reached by offset (their classes are not decompiled).
=============================================================================*/
#include "nwindow/NConsoleWnd.h"
#include "nwindow/NCConsole.h"
#include "nwindow/NCWnd.h"
#include "nwindow/NDialogInfo.h"
#include "nwindow/NCDialogBox.h"
#include "nwindow/NCAbnormalStatusWnd.h"
#include "nwindow/NCListCtrl.h"
#include "nwindow/NCCommandChannelWnd.h"
#include "nwindow/NCCommandInfoWnd.h"
#include "nwindow/NCGodWnd.h"
#include "nwindow/NCLobbyWnd.h"
#include "nwindow/NCOlympiadPlayerWnd.h"
#include "nwindow/NCOlympiadTargetWnd.h"
#include "nwindow/NCQuestListInfoWnd.h"
#include "nwindow/NConsoleWndExt.h"

// the desktop's window at an offset (as NConsoleWndC2.cpp's)
static FORCEINLINE NCWnd* DeskWndC4(NConsoleWnd* Console, DWORD Offset)
{
    return nw::Field<NCWnd*>(Console->Desktop, Offset);
}

/*-----------------------------------------------------------------------------
    A window of the console at +0x3C0C and one at +0x3C08 (new in C4: the field of the console that C3's header did not have).
-----------------------------------------------------------------------------*/

// 0x10066260 (vslot 317) - the window at +0x3C0C shown, when there is one
void NConsoleWnd::C4Virtual317()
{
    NCWnd* Wnd = nw::Field<NCWnd*>(this, 0x3C0C);
    if (Wnd)
        Wnd->ShowWindow();
}
NW_HOOK_METHOD(0x10066260, NConsoleWnd, C4Virtual317)

// 0x10066280 (vslot 318) - and hidden
void NConsoleWnd::C4Virtual318()
{
    NCWnd* Wnd = nw::Field<NCWnd*>(this, 0x3C0C);
    if (Wnd)
        Wnd->HideWindow();
}
NW_HOOK_METHOD(0x10066280, NConsoleWnd, C4Virtual318)

// 0x1006d920 (vslot 417) - the window at +0x3C08 (the warning dialog box) shown when the value is 0
void NConsoleWnd::C4Virtual417(INT Value)
{
    if (!Value)
        nw::Field<NCWnd*>(this, 0x3C08)->ShowWindow();
}
NW_HOOK_METHOD(0x1006d920, NConsoleWnd, C4Virtual417)

// 0x1006d940 (vslot 418) - hidden when the value is 0; -1 asks the slot again with 0 (the stock jumps through the vtable)
void NConsoleWnd::C4Virtual418(INT Value)
{
    if (Value == -1)
    {
        nw::VirtualCall<void>(this, 0x688 / 4, 0);
        return;
    }
    if (!Value)
        nw::Field<NCWnd*>(this, 0x3C08)->HideWindow();
}
NW_HOOK_METHOD(0x1006d940, NConsoleWnd, C4Virtual418)

/*-----------------------------------------------------------------------------
    The window at +0x3C08 (class 0x103103A0, constructor 0x10120D10: new in C4; name of the view ours). It shows a line of text with a number
    (a count, a time) that blinks and fades. Its fields: +0x114 / +0x118 / +0x11C / +0x120 the numbers it follows, +0x124 active, +0x128 the
    alpha of the blink, +0x12C blinking, +0x130 the blink's time, +0x134 and +0x154 two timers in seconds (floats), +0x138 the text, +0x144 and
    +0x148 the number and its text, +0x158 the number is above zero, +0x15C running, +0x160 the time it ran, +0x164 / +0x165 two flags.
-----------------------------------------------------------------------------*/

// (struct NCTimerNoticeWndExt is declared in include/nwindow/NConsoleWndExt.h: NConsoleWndInput.cpp's tick calls Tick)

// 0x10120020 - everything back to zero, the text empty
void NCTimerNoticeWndExt::Clear()
{
    nw::Field<INT>(this, 0x114) = 0;
    nw::Field<INT>(this, 0x118) = 0;
    nw::Field<INT>(this, 0x11C) = 0;
    nw::Field<INT>(this, 0x120) = 0;
    nw::Field<INT>(this, 0x124) = 0;
    nw::Field<INT>(this, 0x134) = 0;
    nw::Field<INT>(this, 0x154) = 0;
    nw::Field<INT>(this, 0x144) = 0;
    nw::Field<FString>(this, 0x138) = TEXT("");
    nw::Field<INT>(this, 0x15C) = 0;
    nw::Field<INT>(this, 0x160) = 0;
    nw::Field<BYTE>(this, 0x164) = 0;
    nw::Field<BYTE>(this, 0x165) = 0;
    nw::Field<INT>(this, 0x158) = 0;
}
NW_HOOK_METHOD(0x10120020, NCTimerNoticeWndExt, Clear)

// 0x10120090 (ret 0xC) - started over for two numbers (the first twice, the second twice) and a flag byte (+0x164)
void NCTimerNoticeWndExt::Reset(INT A, INT B, INT bFlag)
{
    nw::Field<INT>(this, 0x114) = A;
    nw::Field<INT>(this, 0x118) = A;
    nw::Field<INT>(this, 0x120) = B;
    nw::Field<INT>(this, 0x11C) = B;
    nw::Field<INT>(this, 0x124) = 0;
    nw::Field<INT>(this, 0x154) = 0;
    nw::Field<INT>(this, 0x144) = 0;
    nw::Field<FString>(this, 0x138) = TEXT("");
    nw::Field<INT>(this, 0x15C) = 0;
    nw::Field<INT>(this, 0x160) = 0;
    nw::Field<BYTE>(this, 0x165) = 0;
    nw::Field<INT>(this, 0x158) = 0;
    nw::Field<BYTE>(this, 0x164) = (BYTE)bFlag;
}
NW_HOOK_METHOD(0x10120090, NCTimerNoticeWndExt, Reset)

// 0x10120100 (ret 8) - the text is a system string, the number its text too; the second timer runs for two seconds and the number counts as
// positive when it is above zero
void NCTimerNoticeWndExt::SetText(INT StringID, INT Number)
{
    nw::Field<FString>(this, 0x138) = *GL2GameData.SysString[StringID];
    nw::Field<INT>(this, 0x144) = Number;
    nw::Field<FString>(this, 0x148) = appItoa(Number);
    nw::Field<FLOAT>(this, 0x154) = 2.0f;
    nw::Field<INT>(this, 0x158) = Number > 0;
}
NW_HOOK_METHOD(0x10120100, NCTimerNoticeWndExt, SetText)

// 0x10120170 (ret 0xC) - the same with the text of a system message that takes a number (NConsoleWnd::GetNumberMessage)
void NCTimerNoticeWndExt::SetNumberText(INT MessageID, INT Number, INT Value)
{
    TCHAR Text[256];
    GConsole->GetNumberMessage(MessageID, Value, Text);
    nw::Field<FString>(this, 0x138) = Text;
    nw::Field<INT>(this, 0x144) = Number;
    nw::Field<FString>(this, 0x148) = appItoa(Number);
    nw::Field<FLOAT>(this, 0x154) = 2.0f;
    nw::Field<INT>(this, 0x158) = 1;
}
NW_HOOK_METHOD(0x10120170, NCTimerNoticeWndExt, SetNumberText)

// 0x101201f0 - the blink starts: opaque, from time 0
void NCTimerNoticeWndExt::Flash()
{
    nw::Field<INT>(this, 0x12C) = 1;
    nw::Field<INT>(this, 0x128) = 0xFF;
    nw::Field<INT>(this, 0x130) = 0;
}
NW_HOOK_METHOD(0x101201f0, NCTimerNoticeWndExt, Flash)

// 0x10120380 - the first timer set to two seconds; an active window blinks
void NCTimerNoticeWndExt::RestartTimer()
{
    nw::Field<FLOAT>(this, 0x134) = 2.0f;
    if (nw::Field<INT>(this, 0x124))
        Flash();
}
NW_HOOK_METHOD(0x10120380, NCTimerNoticeWndExt, RestartTimer)

// 0x10120210 (ret 4; the console's tick calls it while the window is shown) - the blink: opaque to 0.04 s, clear to 0.08 s, then a fade out
// until 0.2 s (its alpha 255 - (t - 0.05) * 10000 to 0.095 s, 105 - (t - 0.08) * 500 after) and the end of the blink; the time it runs adds up;
// the two timers run down to zero, the first one hides the console's warning box (its slot 418) when it ends
void NCTimerNoticeWndExt::Tick(FLOAT DeltaTime)
{
    if (nw::Field<INT>(this, 0x12C))
    {
        FLOAT Time = DeltaTime + nw::Field<FLOAT>(this, 0x130);
        nw::Field<FLOAT>(this, 0x130) = Time;
        if (Time <= 0.04f)
            nw::Field<INT>(this, 0x128) = 0xFF;
        else if (Time <= 0.08f)
            nw::Field<INT>(this, 0x128) = 0;
        else if (Time <= 0.095f)
            nw::Field<INT>(this, 0x128) = (INT)(255.0f - (Time - 0.05f) * 10000.0f);
        else if (Time <= 0.2f)
            nw::Field<INT>(this, 0x128) = (INT)(105.0f - (Time - 0.08f) * 500.0f);
        else
        {
            nw::Field<INT>(this, 0x12C) = 0;
            nw::Field<INT>(this, 0x130) = 0;
            nw::Field<INT>(this, 0x128) = 0;
        }
    }
    if (nw::Field<INT>(this, 0x15C))
        nw::Field<FLOAT>(this, 0x160) = DeltaTime + nw::Field<FLOAT>(this, 0x160);
    if (0.0f < nw::Field<FLOAT>(this, 0x154))
    {
        FLOAT Left = nw::Field<FLOAT>(this, 0x154) - DeltaTime;
        nw::Field<FLOAT>(this, 0x154) = Left;
        if (Left < 0.0f)
            nw::Field<FLOAT>(this, 0x154) = 0.0f;
    }
    if (0.0f < nw::Field<FLOAT>(this, 0x134))
    {
        FLOAT Left = nw::Field<FLOAT>(this, 0x134) - DeltaTime;
        nw::Field<FLOAT>(this, 0x134) = Left;
        if (Left <= 0.0f)
            nw::VirtualCall<void>(GConsole, 418, 0);
    }
}
NW_HOOK_METHOD(0x10120210, NCTimerNoticeWndExt, Tick)

// 0x10120990 (ret 0x18; the console's slot 421 passes its six values) - an update of the numbers it follows: Flag2 (+0x165) changes restart
// the running time; Kind 4 keeps the numbers as they were (the others store A in +0x118 and B in +0x120); it turns active when +0x11C less
// +0x120 is 3 (and runs unless the flag +0x164 is set); Kind 3 / 4 set the text (system strings 1261, 1264; Kind 4 stops it running);
// then, active: bFlash starts the blink, Kind 1 shows the change since the last (A less +0x118): from zero up the string 1258, below zero the
// string 1672 with Extra above zero (as a number message) or 1256 without; Kind 2: below zero 1671 / 1257, from zero up 1259
void NCTimerNoticeWndExt::Update(INT A, INT B, INT bFlash, INT Flag2, INT Kind, INT Extra)
{
    INT Delta = A - nw::Field<INT>(this, 0x118);
    if (nw::Field<BYTE>(this, 0x165) != (BYTE)Flag2)
    {
        nw::Field<BYTE>(this, 0x165) = (BYTE)Flag2;
        nw::Field<INT>(this, 0x160) = 0;
    }
    if ((BYTE)Kind != 4)
    {
        nw::Field<INT>(this, 0x118) = A;
        nw::Field<INT>(this, 0x120) = B;
    }
    if (nw::Field<INT>(this, 0x124) == 0 && nw::Field<INT>(this, 0x11C) - nw::Field<INT>(this, 0x120) == 3)
    {
        nw::Field<INT>(this, 0x124) = 1;
        if (nw::Field<BYTE>(this, 0x164) == 0)
            nw::Field<INT>(this, 0x15C) = 1;
    }
    if ((BYTE)Kind == 3)
        SetText(0x4ED, 0);
    else if ((BYTE)Kind == 4)
    {
        nw::Field<INT>(this, 0x15C) = 0;
        SetText(0x4F0, 0);
    }
    if (nw::Field<INT>(this, 0x124))
    {
        if ((BYTE)bFlash)
            Flash();
        if ((BYTE)Kind == 1)
        {
            if (Delta < 0)
            {
                if (Extra > 0)
                    SetNumberText(0x688, Delta, Extra);
                else
                    SetText(0x4E8, Delta);
            }
            else
                SetText(0x4EA, Delta);
        }
        else if ((BYTE)Kind == 2)
        {
            if (Delta < 0)
            {
                if (Extra > 0)
                    SetNumberText(0x687, Delta, Extra);
                else
                    SetText(0x4E9, Delta);
            }
            else
                SetText(0x4EB, Delta);
        }
    }
}
NW_HOOK_METHOD(0x10120990, NCTimerNoticeWndExt, Update)

// 0x1006d970 (vslot 419) - the +0x3C08 window's 0x10120020
void NConsoleWnd::C4Virtual419()
{
    NCWnd* Wnd = nw::Field<NCWnd*>(this, 0x3C08);
    if (Wnd)
        ((NCTimerNoticeWndExt*)Wnd)->Clear();
}
NW_HOOK_METHOD(0x1006d970, NConsoleWnd, C4Virtual419)

// 0x1006d980 (vslot 420) - the +0x3C08 window's 0x10120090 with three values
void NConsoleWnd::C4Virtual420(INT A, INT B, INT C)
{
    NCWnd* Wnd = nw::Field<NCWnd*>(this, 0x3C08);
    if (Wnd)
        ((NCTimerNoticeWndExt*)Wnd)->Reset(A, B, C);
}
NW_HOOK_METHOD(0x1006d980, NConsoleWnd, C4Virtual420)

// 0x1006d9a0 (vslot 421) - the +0x3C08 window's 0x10120990 with six values
void NConsoleWnd::C4Virtual421(INT A, INT B, INT C, INT D, INT E, INT F)
{
    NCWnd* Wnd = nw::Field<NCWnd*>(this, 0x3C08);
    if (Wnd)
        ((NCTimerNoticeWndExt*)Wnd)->Update(A, B, C, D, E, F);
}
NW_HOOK_METHOD(0x1006d9a0, NConsoleWnd, C4Virtual421)

// 0x1006d9c0 (vslot 422) - the +0x3C08 window's 0x10120380
void NConsoleWnd::C4Virtual422()
{
    NCWnd* Wnd = nw::Field<NCWnd*>(this, 0x3C08);
    if (Wnd)
        ((NCTimerNoticeWndExt*)Wnd)->RestartTimer();
}
NW_HOOK_METHOD(0x1006d9c0, NConsoleWnd, C4Virtual422)

// 0x1006d9d0 (vslot 423) - the desktop's interface toggled (ToggleInterface(0, bShow)); hiding it, the god window's windows
// +0xEC .. +0xFC (the login screens) are hidden too
void NConsoleWnd::C4Virtual423(INT bShow)
{
    GetDesktop()->ToggleInterface(false, bShow != 0);
    if (!bShow && GodWnd)
    {
        for (DWORD Offset = 0xEC; Offset <= 0xFC; Offset += 4)
        {
            NCWnd* Wnd = nw::Field<NCWnd*>(GodWnd, Offset);
            if (Wnd)
                Wnd->HideWindow();
        }
    }
}
NW_HOOK_METHOD(0x1006d9d0, NConsoleWnd, C4Virtual423)

// 0x1006c490 (vslot 369) - the desktop hidden, the god window and its windows +0xF4, +0xF8 and +0xFC (no null checks)
void NConsoleWnd::C4Virtual369()
{
    if (Desktop)
        Desktop->HideWindow();
    if (GodWnd)
    {
        GodWnd->ShowWindow();
        nw::Field<NCWnd*>(GodWnd, 0xF4)->HideWindow();
        nw::Field<NCWnd*>(GodWnd, 0xF8)->HideWindow();
        nw::Field<NCWnd*>(GodWnd, 0xFC)->HideWindow();
    }
}
NW_HOOK_METHOD(0x1006c490, NConsoleWnd, C4Virtual369)

/*-----------------------------------------------------------------------------
    The game-card lobby's numbers (new in C4): +0x3C8C .. +0x3CA0.
-----------------------------------------------------------------------------*/

// 0x1006d710 (vslot 410) - the card lobby is off (GL2DisableGCLobby), and the numbers it was given are kept
void NConsoleWnd::C4Virtual410(INT A, INT B, INT C, INT D)
{
    GL2DisableGCLobby = 1;
    bGMChange = 1;
    GMStep = 1;
    GMValue = A;
    nw::Field<INT>(this, 0x3C98) = B;
    nw::Field<INT>(this, 0x3C9C) = C;
    nw::Field<INT>(this, 0x3CA0) = D;
}
NW_HOOK_METHOD(0x1006d710, NConsoleWnd, C4Virtual410)

// 0x1006d7f0 (vslot 442) - the three floats at +0x3C98 as integers
void NConsoleWnd::C4Virtual442(INT* A, INT* B, INT* C)
{
    *A = (INT)GMDest[0];
    *B = (INT)GMDest[1];
    *C = (INT)GMDest[2];
}
NW_HOOK_METHOD(0x1006d7f0, NConsoleWnd, C4Virtual442)

// 0x1006d830 (vslot 444)
void NConsoleWnd::C4Virtual444()
{
    GMStep = 0;
    bGMChange = 0;
}
NW_HOOK_METHOD(0x1006d830, NConsoleWnd, C4Virtual444)

// 0x1006d840 (vslot 445)
void NConsoleWnd::C4Virtual445(INT Value)
{
    GMValue = Value;
}
NW_HOOK_METHOD(0x1006d840, NConsoleWnd, C4Virtual445)

// 0x1006d850 (vslot 446)
void NConsoleWnd::C4Virtual446()
{
    GMStep--;
}
NW_HOOK_METHOD(0x1006d850, NConsoleWnd, C4Virtual446)

// 0x1006d860 (vslot 447)
void NConsoleWnd::C4Virtual447()
{
    GMStep++;
}
NW_HOOK_METHOD(0x1006d860, NConsoleWnd, C4Virtual447)

// 0x1006d750 (guard name; vslot 441) - the GM's server change goes to its next state (only from the one before it); logged
void NConsoleWnd::SetGMChangeServerState(INT State)
{
    guard(NConsoleWnd::SetGMChangeServerState);
    if (GMStep == State - 1)
    {
        GMStep = State;
        if (GLog)
            GLog->Logf2(TEXT("GMChangeServer is going to next state %d"), State);
    }
    unguard;
}
NW_HOOK_METHOD(0x1006d750, NConsoleWnd, SetGMChangeServerState)

/*-----------------------------------------------------------------------------
    Windows of the desktop that C4 added (+0x180, +0x260, +0x264 ...).
-----------------------------------------------------------------------------*/

// 0x10067830 (vslot 209) - the desktop's window at +0x260: two of its functions, then shown (the argument is not read)
void NConsoleWnd::C4Virtual209(INT Unused)
{
    ((NCListOwnerExt*)DeskWndC4(this, 0x260))->ClearList();
    static_cast<NCQuestListInfoWnd*>(DeskWndC4(this, 0x260))->FillList();
    DeskWndC4(this, 0x260)->ShowWindow();
}
NW_HOOK_METHOD(0x10067830, NConsoleWnd, C4Virtual209)

// 0x10067870 (vslot 210) - the packet's value goes to the desktop's window at +0x180 (shown first), and the global console's
// desktop is told (0x1005CF40)
void NConsoleWnd::C4Virtual210(L2ParamStack& Params)
{
    INT Value = (INT)Params.Top();
    DeskWndC4(this, 0x180)->ShowWindow();
    ((NTutorialMarkWnd*)DeskWndC4(this, 0x180))->SetQuestion(Value);
    GConsole->GetDesktop()->DockAboveChat();
}
NW_HOOK_METHOD(0x10067870, NConsoleWnd, C4Virtual210)

// 0x10067ee0 (vslot 407, UGameEngine::OnExMultiPartyCommandChannelInfo) - the command channel window (desktop +0x264): its
// list emptied (the child at +0xF0 is cleared by 0x10051220), then the packet's value
void NConsoleWnd::C4Virtual407(INT Value)
{
    static_cast<NCCommandInfoWnd*>(nw::Field<NCWnd*>(DeskWndC4(this, 0x264), 0xF0))->ClearInfo();
    ((NCChannelWndExt*)DeskWndC4(this, 0x264))->SetInfo(*(L2ParamStack*)Value);
}
NW_HOOK_METHOD(0x10067ee0, NConsoleWnd, C4Virtual407)

// 0x10067f20 (vslot 408, ...InfoAddMember) - the same window's 0x10051C00 (a tail call)
void NConsoleWnd::C4Virtual408(INT Value)
{
    ((NCChannelWndExt*)DeskWndC4(this, 0x264))->AddMember(*(L2ParamStack*)Value);
}
NW_HOOK_METHOD(0x10067f20, NConsoleWnd, C4Virtual408)

// 0x10070000 (vslot 85) - seven values of the packet: five of them go to fields of the store, private shop, private buy and
// the +0x198 window (the first and the third are dropped)
void NConsoleWnd::C4Virtual085(L2ParamStack& Params)
{
    Params.Top();
    INT V2 = (INT)Params.Top();
    Params.Top();
    INT V4 = (INT)Params.Top();
    INT V5 = (INT)Params.Top();
    INT V6 = (INT)Params.Top();
    INT V7 = (INT)Params.Top();
    nw::Field<INT>(GetDesktop()->StoreWnd, 0x120) = V2;
    nw::Field<INT>(GetDesktop()->PrivateShopWnd, 0x130) = V4;
    nw::Field<INT>(GetDesktop()->PrivateBuyWnd, 0x128) = V5;
    nw::Field<INT>(DeskWndC4(this, 0x198), 0x19C) = V6;
    nw::Field<INT>(DeskWndC4(this, 0x198), 0x198) = V7;
}
NW_HOOK_METHOD(0x10070000, NConsoleWnd, C4Virtual085)

// 0x1006a350 (guard name) - the god window shows the options window
void NConsoleWnd::ShowOptionWnd()
{
    guard(NConsoleWnd::ShowOptionWnd);
    if (GodWnd)
        static_cast<NCGodWnd*>(GodWnd)->ShowOptionWnd();
    unguard;
}
NW_HOOK_METHOD(0x1006a350, NConsoleWnd, ShowOptionWnd)

/*-----------------------------------------------------------------------------
    The Olympiad, the mail and the command channel (packets new in C4; the names are the stock guard strings).
-----------------------------------------------------------------------------*/

// 0x1005c490 (anonymous in the stock image) - the first viewport's player controller; the stock functions inline it too
ALineagePlayerController* NConsoleWnd::GetPlayerController()
{
    return CastToLineagePlayerController(Engine->Client->Viewports(0)->Actor);
}
NW_HOOK_METHOD(0x1005c490, NConsoleWnd, GetPlayerController)

// 0x10067d40 (guard name; vslot 220) - the Olympiad player's info (side 1 or 2): in the observer's UI mode (2) the player
// windows at desktop +0x214 / +0x218 take the packet; in the others the target window (+0x224) when its id is the side
void NConsoleWnd::OnOlympiadUserInfo(L2ParamStack& Params)
{
    guard(NConsoleWnd::OnOlympiadUserInfo);
    INT Side = (INT)Params.Top();
    if (Desktop)
    {
        if (UIMode == 2)
        {
            NCWnd* Wnd;
            if ((char)Side == 1)
                Wnd = DeskWndC4(this, 0x214);
            else if ((char)Side == 2)
                Wnd = DeskWndC4(this, 0x218);
            else
                return;
            if (Wnd)
                static_cast<NCOlympiadPlayerWnd*>(Wnd)->OnOlympiadUserInfo(Params);
        }
        else
        {
            NCWnd* Target = DeskWndC4(this, 0x224);
            if (Target)
            {
                INT TargetSide = ((NOlympiadTargetWndExt*)Target)->GetSide();   // its side (+0x128)
                if ((char)Side == TargetSide)
                    static_cast<NCOlympiadTargetWnd*>(DeskWndC4(this, 0x224))->OnOlympiadUserInfo(Params);
            }
        }
    }
    unguard;
}
NW_HOOK_METHOD(0x10067d40, NConsoleWnd, OnOlympiadUserInfo)

// 0x10067e30 (guard name; vslot 223) - the match is over: the two player windows (+0x214, +0x218) and their buff windows
// (+0x21C, +0x220) are cleared, then the target window (+0x224)
void NConsoleWnd::OnOlympiadMatchEnd()
{
    guard(NConsoleWnd::OnOlympiadMatchEnd);
    for (INT i = 0; i < 2; i++)
    {
        static_cast<NCOlympiadPlayerWnd*>(DeskWndC4(this, 0x214 + i * 4))->Initialize();
        static_cast<NCAbnormalStatusWnd*>(DeskWndC4(this, 0x21C + i * 4))->ClearStatus();
    }
    static_cast<NCOlympiadTargetWnd*>(DeskWndC4(this, 0x224))->Initialize();
    unguard;
}
NW_HOOK_METHOD(0x10067e30, NConsoleWnd, OnOlympiadMatchEnd)

// 0x1006a0f0 (guard name; vslot 224) - mail arrived: the window at desktop +0x184 shown and told 0 (0x10002280), the global
// console's desktop is told (0x1005CF40)
void NConsoleWnd::OnExMailArrivedPacket()
{
    guard(NConsoleWnd::OnExMailArrivedPacket);
    DeskWndC4(this, 0x184)->ShowWindow();
    ((NTutorialMarkWnd*)DeskWndC4(this, 0x184))->SetQuestion(0);
    GConsole->GetDesktop()->DockAboveChat();
    unguard;
}
NW_HOOK_METHOD(0x1006a0f0, NConsoleWnd, OnExMailArrivedPacket)

// 0x100702a0 (guard name; vslot 225) - the command channel window (desktop +0x264) shown, its flag at +0x100 set
void NConsoleWnd::OnExOpenMPCC(INT Unused)
{
    guard(NConsoleWnd::OnExOpenMPCC);
    DeskWndC4(this, 0x264)->ShowWindow();
    nw::Field<INT>(DeskWndC4(this, 0x264), 0x100) = 1;
    unguard;
}
NW_HOOK_METHOD(0x100702a0, NConsoleWnd, OnExOpenMPCC)

// 0x10070340 (guard name; vslot 226) - hidden, its list emptied (0x10051C20) and the flag cleared
void NConsoleWnd::OnExCloseMPCC(INT Unused)
{
    guard(NConsoleWnd::OnExCloseMPCC);
    DeskWndC4(this, 0x264)->HideWindow();
    static_cast<NCCommandChannelWnd*>(DeskWndC4(this, 0x264))->Reset();
    nw::Field<INT>(DeskWndC4(this, 0x264), 0x100) = 0;
    unguard;
}
NW_HOOK_METHOD(0x10070340, NConsoleWnd, OnExCloseMPCC)

// 0x10079640 (guard name; vslot 227) - someone asks to join the command channel: the dialog 0x52 (system message 1529) with
// the name on its button, unless the packet's name is missing
void NConsoleWnd::OnExAskJoinMPCC(L2ParamStack& Params)
{
    guard(NConsoleWnd::OnExAskJoinMPCC);
    TCHAR* Name = (TCHAR*)Params.Top();
    if (Name)
    {
        NDialogInfo Info;
        Info.ID = 0x52;
        Info.bMainBox = 0;
        Info.bInput = 0;
        Info.SetSystemMessage(0x5F9);
        Info.SetButtons(Name, NULL);
        Info.bProgress = 1;
        ShowDialog(&Info);
    }
    unguard;
}
NW_HOOK_METHOD(0x10079640, NConsoleWnd, OnExAskJoinMPCC)

// 0x1006b6b0 (vslot 246) - asks the server for the skills' cool times: NetHandler slot 192 (RequestSkillCoolTime, no argument; C3 passed a
// parameter stack on)
void NConsoleWnd::C4Virtual246()
{
    nw::VirtualCall<void>(Engine->NetHandler, 0x300 / 4);
}
NW_HOOK_METHOD(0x1006b6b0, NConsoleWnd, C4Virtual246)

/*-----------------------------------------------------------------------------
    The manor and the Olympiad: the requests the windows send (each a guard-named wrapper of one NetHandler slot).
-----------------------------------------------------------------------------*/

// 0x1006db90 (guard name) - NetHandler slot 285: the manors' list (asked for when the game starts)
void NConsoleWnd::RequestManorList()
{
    guard(NConsoleWnd::RequestManorList);
    Engine->NetHandler->RequestManorList();
    unguard;
}
NW_HOOK_METHOD(0x1006db90, NConsoleWnd, RequestManorList)

// 0x1006dc10 (guard name) - slot 286: the crops to sell to a castle
void NConsoleWnd::RequestProcureCropList(L2ParamStack& Params)
{
    guard(NConsoleWnd::RequestProcureCropList);
    Engine->NetHandler->RequestProcureCropList(&Params);
    unguard;
}
NW_HOOK_METHOD(0x1006dc10, NConsoleWnd, RequestProcureCropList)

// 0x1006dc90 (guard name) - slot 287: the seed the castle sets
void NConsoleWnd::RequestSetSeed(L2ParamStack& Params)
{
    guard(NConsoleWnd::RequestSetSeed);
    Engine->NetHandler->RequestSetSeed(&Params);
    unguard;
}
NW_HOOK_METHOD(0x1006dc90, NConsoleWnd, RequestSetSeed)

// 0x1006dd10 (guard name) - slot 288: the crop the castle sets
void NConsoleWnd::RequestSetCrop(L2ParamStack& Params)
{
    guard(NConsoleWnd::RequestSetCrop);
    Engine->NetHandler->RequestSetCrop(&Params);
    unguard;
}
NW_HOOK_METHOD(0x1006dd10, NConsoleWnd, RequestSetCrop)

// 0x1006dd90 (guard name) - slot 289: the hero's words (the text by value)
void NConsoleWnd::RequestWriteHeroWords(FString Words)
{
    guard(NConsoleWnd::RequestWriteHeroWords);
    Engine->NetHandler->RequestWriteHeroWords(Words);
    unguard;
}
typedef void (NConsoleWnd::*SigRequestWriteHeroWords)(FString);
// installed as the member itself (no forwarder): the by-value FString a forwarder would copy again is the stock's own
NW_HOOK_VARARGS(0x1006dd90, NConsoleWnd, RequestWriteHeroWords, SigRequestWriteHeroWords)

// 0x1006de40 (guard name) - slot 290: the observer of an Olympiad match leaves
void NConsoleWnd::RequestOlympiadObserverEnd()
{
    guard(NConsoleWnd::RequestOlympiadObserverEnd);
    Engine->NetHandler->RequestOlympiadObserverEnd();
    unguard;
}
NW_HOOK_METHOD(0x1006de40, NConsoleWnd, RequestOlympiadObserverEnd)

// 0x1006dec0 (guard name) - slot 291: the list of the running Olympiad matches
void NConsoleWnd::RequestOlympiadMatchList()
{
    guard(NConsoleWnd::RequestOlympiadMatchList);
    Engine->NetHandler->RequestOlympiadMatchList();
    unguard;
}
NW_HOOK_METHOD(0x1006dec0, NConsoleWnd, RequestOlympiadMatchList)

/*-----------------------------------------------------------------------------
    The observer mode, the Olympiad windows and a few NetHandler wrappers of this client.
-----------------------------------------------------------------------------*/

// 0x1006bd00 (name ours) - the observer mode keeps the value of GIsL2Mark and clears it
void NConsoleWnd::SaveL2Mark(INT Value)
{
    SavedL2Mark = Value;
    bL2MarkSaved = 1;
    GIsL2Mark = 0;
}
NW_HOOK_METHOD(0x1006bd00, NConsoleWnd, SaveL2Mark)

// 0x1006bd30 (name ours) - and gives it back, once
void NConsoleWnd::RestoreL2Mark()
{
    if (bL2MarkSaved)
    {
        GIsL2Mark = SavedL2Mark;
        SavedL2Mark = 0;
        bL2MarkSaved = 0;
    }
}
NW_HOOK_METHOD(0x1006bd30, NConsoleWnd, RestoreL2Mark)

// the Olympiad player windows (the desktop's +0x214 and +0x218): the player's id (+0x12C) as the stock's anonymous getter
// (struct NOlympiadPlayerWndExt is declared in include/nwindow/NConsoleWndExt.h)
INT NOlympiadPlayerWndExt::GetPlayerID()
{
    return nw::Field<INT>(this, 0x12C);
}
NW_HOOK_METHOD(0x1013d0f0, NOlympiadPlayerWndExt, GetPlayerID)

// the Olympiad target window (the desktop's +0x224): the side it shows (+0x128), read and set (struct NOlympiadTargetWndExt is
// declared in include/nwindow/NConsoleWndExt.h)
INT NOlympiadTargetWndExt::GetSide()
{
    return nw::Field<INT>(this, 0x128);
}
NW_HOOK_METHOD(0x1013dc30, NOlympiadTargetWndExt, GetSide)
void NOlympiadTargetWndExt::SetSide(INT Side)
{
    nw::Field<INT>(this, 0x128) = Side;
}
NW_HOOK_METHOD(0x1013dc20, NOlympiadTargetWndExt, SetSide)

// 0x1006d510 (guard name; vslot 395) - an attack is shown to the Olympiad player windows (+0x214, +0x218) whose player is the
// attacker or the target
void NConsoleWnd::OnAttack(User* A, User* B, INT P3, INT P4, INT P5)
{
    guard(NConsoleWnd::OnAttack);
    if (Desktop)
    {
        for (INT i = 0; i < 2; i++)
        {
            NCWnd* Player = nw::Field<NCWnd*>(Desktop, 0x214 + i * 4);
            if (Player)
            {
                INT ID = ((NOlympiadPlayerWndExt*)Player)->GetPlayerID();
                if (ID == nw::Field<INT>(A, 0x18) || ID == nw::Field<INT>(B, 0x18))
                    static_cast<NCOlympiadPlayerWnd*>(nw::Field<NCWnd*>(Desktop, 0x214 + i * 4))->OnAttack(A, B, P3, P4, P5);
            }
        }
    }
    unguard;
}
NW_HOOK_METHOD(0x1006d510, NConsoleWnd, OnAttack)

// 0x100650f0 (vslot 363) - the logout (NetHandler slot 160), then the replay recording stops when there is one
void NConsoleWnd::C4Virtual363()
{
    Engine->NetHandler->SendLogOutPacket();
    FL2ReplayManager* Replay = Engine->ReplayManager;
    if (Replay)
        Replay->OnSaveStop();
}
NW_HOOK_METHOD(0x100650f0, NConsoleWnd, C4Virtual363)

// 0x10065500 (name ours) - the restart request (NetHandler slot 172), then the replay recording stops when there is one
void NConsoleWnd::RequestRestartNow(L2ParamStack& Params)
{
    Engine->NetHandler->RequestRestart(Params);
    FL2ReplayManager* Replay = Engine->ReplayManager;
    if (Replay)
        Replay->OnSaveStop();
}
NW_HOOK_METHOD(0x10065500, NConsoleWnd, RequestRestartNow)

// the wrappers of the skill enchanting and of the command channel (the stock jumps into the NetHandler's slot with its own stack)
// 0x10067fe0 (vslot 436) - slot 187
void NConsoleWnd::RequestExEnchantSkillInfo(L2ParamStack& Params)
{
    Engine->NetHandler->RequestExEnchantSkillInfo(Params);
}
NW_HOOK_METHOD(0x10067fe0, NConsoleWnd, RequestExEnchantSkillInfo)

// 0x10067ff0 (vslot 437) - slot 188
void NConsoleWnd::RequestExEnchantSkill(L2ParamStack& Params)
{
    Engine->NetHandler->RequestExEnchantSkill(Params);
}
NW_HOOK_METHOD(0x10067ff0, NConsoleWnd, RequestExEnchantSkill)

// 0x10067f40 (name ours) - slot 189
void NConsoleWnd::RequestExAskJoinMPCC(L2ParamStack& Params)
{
    Engine->NetHandler->RequestExAskJoinMPCC(Params);
}
NW_HOOK_METHOD(0x10067f40, NConsoleWnd, RequestExAskJoinMPCC)

// 0x10067f50 (name ours) - slot 190
void NConsoleWnd::RequestExAcceptJoinMPCC(L2ParamStack& Params)
{
    Engine->NetHandler->RequestExAcceptJoinMPCC(Params);
}
NW_HOOK_METHOD(0x10067f50, NConsoleWnd, RequestExAcceptJoinMPCC)

// 0x10067f60 (name ours) - slot 191
void NConsoleWnd::RequestExOustFromMPCC(L2ParamStack& Params)
{
    Engine->NetHandler->RequestExOustFromMPCC(Params);
}
NW_HOOK_METHOD(0x10067f60, NConsoleWnd, RequestExOustFromMPCC)

// 0x100826a0 (vslot 416; the Engine calls it through the slot, Engine.dll 0x1046B677) - the "special server" flag of the server
// the player logged in to (+0x3C68: NCServerSelectWnd::OnOK stores the row's flag 8 there). C4 evidence: the stock is
// `mov eax, [ecx + 0x3C68]; ret`. (Wave B named it GetPartyItemDistribution: the party item rule is the field at +0x3C6C, which
// NCGameOptionWnd::Apply / InitItems write)
INT NConsoleWnd::GetSpecialServer()
{
    return SpecialServer;
}
NW_HOOK_METHOD(0x100826a0, NConsoleWnd, GetSpecialServer)

// 0x1006ca80 (name ours; the db says UL2ConsoleWnd::RequestSendL2FriendSay; 14 bytes, ret 8) - the whisper of the L2 friend list goes
// to the NetHandler's slot 272. The stock is `mov eax,[ecx+0x54]; mov ecx,[eax+0x64]; mov edx,[ecx]; jmp [edx+0x440]`: Engine, its
// NetHandler, the slot, with the caller's two arguments left on the stack
void NConsoleWnd::RequestSendL2FriendSay(TCHAR* Name, TCHAR* Text)
{
    Engine->NetHandler->RequestSendL2FriendSay(Name, Text);
}
NW_HOOK_METHOD(0x1006ca80, NConsoleWnd, RequestSendL2FriendSay)

// 0x1006ca90 (name ours; the db says UL2ConsoleWnd::RequestSendMsnChatLog; 14 bytes, ret 0xC) - the same for the messenger chat log
// (NetHandler slot 273, +0x444)
void NConsoleWnd::RequestSendMsnChatLog(TCHAR* Account, TCHAR* Text, INT Flag)
{
    Engine->NetHandler->RequestSendMsnChatLog(Account, Text, Flag);
}
NW_HOOK_METHOD(0x1006ca90, NConsoleWnd, RequestSendMsnChatLog)

/*-----------------------------------------------------------------------------
    The manors and the Olympiad windows.
-----------------------------------------------------------------------------*/

// 0x1006b290 (guard name) - the name of a manor (castle region) 1 .. 7: system strings 1049, 1052, 1053, 1057, 1060, 1059 and
// 1248; an empty text for another number
const TCHAR* NConsoleWnd::GetManorName(INT Manor)
{
    guard(NConsoleWnd::GetManorName);
    switch (Manor)
    {
    case 1: return *GL2GameData.SysString[1049];
    case 2: return *GL2GameData.SysString[1052];
    case 3: return *GL2GameData.SysString[1053];
    case 4: return *GL2GameData.SysString[1057];
    case 5: return *GL2GameData.SysString[1060];
    case 6: return *GL2GameData.SysString[1059];
    case 7: return *GL2GameData.SysString[1248];
    }
    return TEXT("");
    unguard;
}
NW_HOOK_METHOD(0x1006b290, NConsoleWnd, GetManorName)

// 0x10080050 (guard name OnSendManorList; vslot 211) - the manors the server knows: the list emptied, each id and name appended,
// and the window at +0x3C2C (a dialog box when it is one: its slot 75 refreshes it; else it is shown)
void NConsoleWnd::C4Virtual211(L2ParamStack& Params)
{
    guard(NConsoleWnd::OnSendManorList);
    ManorList.Empty(0);
    INT Count = (INT)Params.Top();
    for (INT i = 0; i < Count; i++)
    {
        INT ID = (INT)Params.Top();
        TCHAR* Name = (TCHAR*)Params.Top();
        ManorList.AddZeroed();
        ManorList(i).ID = ID;
        ManorList(i).Name = Name;
    }
    if (WordWrapperWnd)
    {
        UClass* Wanted = NCDialogBox::StaticClass();
        UClass* Class = nw::Field<UClass*>(WordWrapperWnd, 0x24);
        while (Class && Class != Wanted)
            Class = nw::Field<UClass*>(Class, 0x34);
        if (Class)
            nw::VirtualCall<void>(WordWrapperWnd, 75);
        else
            WordWrapperWnd->ShowWindow();
    }
    unguard;
}
NW_HOOK_METHOD(0x10080050, NConsoleWnd, C4Virtual211)

// 0x10080180 (guard name; vslot 222) - the Olympiad mode: 0 ends it (in the Olympiad UI mode, 2, the normal windows come back
// and the controller is told 0; else the target window is hidden), 1 and 2 show the target window with its side (2, 1), 3 starts
// the Olympiad UI mode and tells the controller 1
void NConsoleWnd::OnOlympiadMode(INT Mode)
{
    guard(NConsoleWnd::OnOlympiadMode);
    switch ((char)Mode)
    {
    case 0:
        if (UIMode == 2)
        {
            SwitchUIMode(0);
            ALineagePlayerController* Controller = GConsole->GetPlayerController();
            if (Controller)
                nw::VirtualCall<void>(Controller, 204, 0);
        }
        else
            nw::Field<NCWnd*>(Desktop, 0x224)->HideWindow();
        break;
    case 1:
        nw::Field<NCWnd*>(Desktop, 0x224)->ShowWindow();
        ((NOlympiadTargetWndExt*)nw::Field<NCWnd*>(Desktop, 0x224))->SetSide(2);
        break;
    case 2:
        nw::Field<NCWnd*>(Desktop, 0x224)->ShowWindow();
        ((NOlympiadTargetWndExt*)nw::Field<NCWnd*>(Desktop, 0x224))->SetSide(1);
        break;
    case 3:
    {
        SwitchUIMode(2);
        ALineagePlayerController* Controller = CastToLineagePlayerController(GConsole->Engine->Client->Viewports(0)->Actor);
        if (Controller)
            nw::VirtualCall<void>(Controller, 204, 1);
        break;
    }
    }
    unguard;
}
NW_HOOK_METHOD(0x10080180, NConsoleWnd, OnOlympiadMode)

// 0x100700b0 (guard name; vslot 221) - the effects on an Olympiad player (side, count, then the skill id, level and seconds each):
// the buff window of the player (+0x21C, +0x220) emptied and filled with them
void NConsoleWnd::OnOlympiadSpelledInfo(L2ParamStack& Params)
{
    guard(NConsoleWnd::OnOlympiadSpelledInfo);
    if (Desktop)
    {
        INT Side = (INT)Params.Top();
        NCAbnormalStatusWnd* Buffs;
        NCWnd* First = nw::Field<NCWnd*>(Desktop, 0x214);
        if (First && ((NOlympiadPlayerWndExt*)First)->GetPlayerID() == Side)
        {
            Buffs = nw::Field<NCAbnormalStatusWnd*>(Desktop, 0x21C);
            if (!Buffs)
                return;
        }
        else
        {
            NCWnd* Second = nw::Field<NCWnd*>(Desktop, 0x218);
            if (!Second || ((NOlympiadPlayerWndExt*)Second)->GetPlayerID() != Side)
                return;
            Buffs = nw::Field<NCAbnormalStatusWnd*>(Desktop, 0x220);
            if (!Buffs)
                return;
        }
        Buffs->ClearStatus();
        INT Count = (INT)Params.Top();
        for (INT i = 0; i < Count; i++)
        {
            FAbnormalStatus Status;
            INT SkillID = (INT)Params.Top();
            INT Level = (SWORD)(INT)Params.Top();
            Status.RemainTime = (INT)Params.Top();
            Status.SkillID = SkillID;
            Status.Level = Level;
            if (GConsole->GetSkillIconName(SkillID, Level))
                appStrcpy(Status.IconName, GConsole->GetSkillIconName(SkillID, Level));
            if (GConsole->GetSkillName(SkillID, Level))
                appStrcpy(Status.Name, GConsole->GetSkillName(SkillID, Level));
            if (GConsole->GetSkillDesc(SkillID, Level))
                appStrcpy(Status.Desc, GConsole->GetSkillDesc(SkillID, Level));
            Buffs->AddStatus(&Status);
        }
    }
    unguard;
}
NW_HOOK_METHOD(0x100700b0, NConsoleWnd, OnOlympiadSpelledInfo)

/*-----------------------------------------------------------------------------
    The GM's server change (vslots 441 .. 447; the tick calls 443 while bGMChange is set).
-----------------------------------------------------------------------------*/

// the step the tick last acted on (a global: the tick acts once per step)
#if NW_HOOK_MODE
#define GGMChangeLastStep   NW_GLOBAL(INT, 0x102D2C1C)
#else
extern INT GGMChangeLastStep;           // src/standalone/NwGlobals.cpp (tools/gen_globals.py)
#endif

// 0x10075410 (vslot 443, only this client has it) - each new step (GMStep, +0x3C90) of the change does its part once: 1 asks for step 2
// (SetGMChangeServerState, slot 441); 2 asks the server to restart (RequestRestartNow); 3 goes back to the login screen and logs in again
// with the account and the password the network handler keeps; 4 asks for the server list (slot 424); 5 (with the server number in
// GMValue, +0x3C8C) looks in the god window's server list (GodWnd +0xEC, +0xF8, +0xE8, +0xEC) for the row of that number and logs in to
// that server: the loading screen follows the row's text (the string 462 the screen 10002, 461 the screen 10003, else 10001) and the
// request takes the number, the server and the row's text as a number; 6 starts the lobby (when it exists), moves to step 7 and
// clears the change; 7 only clears it
void NConsoleWnd::C4Virtual443()
{
    if (bGMChange && GGMChangeLastStep != GMStep)
    {
        GGMChangeLastStep = GMStep;
        switch (GMStep)
        {
        case 1:
            nw::VirtualCall<void>(this, 441, 2);
            break;
        case 2:
        {
            L2ParamStack Params(10);
            RequestRestartNow(Params);
            break;
        }
        case 3:
        {
            ReturnToLogin();
            TCHAR Account[18];
            TCHAR Password[18];
            appStrcpy(Account, Engine->NetHandler->GetUserAccount());
            appStrcpy(Password, Engine->NetHandler->GetUserPassword());
            RequestAuthLogin(Account, Password);
            break;
        }
        case 4:
            NewVirtual424();
            break;
        case 5:
            if (GMValue > 0)
            {
                NCListCtrl* List = nw::Field<NCListCtrl*>(nw::Field<NCWnd*>(nw::Field<NCWnd*>(nw::Field<NCWnd*>(GodWnd, 0xEC), 0xF8), 0xE8), 0xEC);
                for (INT i = 0; i < List->GetRowCount(); i++)
                {
                    List->SetSelected(i);
                    NListRow* Row = List->GetSelectedRow();
                    if (Row && GMValue == Row->Param1)
                    {
                        const TCHAR* Text = (const TCHAR*)((BYTE*)Row->Cells + 0x6174);
                        INT Screen;
                        if (!appStrcmp(Text, *GL2GameData.SysString[462]))
                            Screen = 0x2712;
                        else if (!appStrcmp(Text, *GL2GameData.SysString[461]))
                            Screen = 0x2713;
                        else
                            Screen = 0x2711;
                        GConsole->LoadingScreenID = Screen;
                        L2ParamStack Params(10);
                        INT Server = Row->Param2;
                        INT Number = appAtoi(Row->Text);
                        Params.PushBack((void*)Row->Param1);
                        Params.PushBack((void*)Server);
                        Params.PushBack((void*)Number);
                        RequestServerLogin(Params);
                    }
                }
            }
            break;
        case 6:
            if (LobbyWnd)
                LobbyWnd->OnStart();
            nw::VirtualCall<void>(this, 441, 7);
            nw::VirtualCall<void>(this, 444);
            break;
        case 7:
            nw::VirtualCall<void>(this, 444);
            break;
        }
    }
}
NW_HOOK_METHOD(0x10075410, NConsoleWnd, C4Virtual443)
