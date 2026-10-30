/*=============================================================================
    NMacroRun.cpp - running a macro: the console's tick (NMacroEntry::Tick, 0x1009E8F0) runs the line the macro is at
    (NMacroEntry::ExecuteLine, 0x1009B8B0) and waits for it to end. The data of NMacroEntry is NMacroEntry.cpp's.
=============================================================================*/
#include "nwindow/NMacroEntry.h"
#include "nwindow/NCMacroListWnd.h"
#include "nwindow/NCChatWnd.h"
#include "nwindow/NCConsole.h"
#include "nwindow/NCShortCutWnd.h"
#include "nwindow/NConsoleWnd.h"
#include "nwindow/NDialogInfo.h"
#include "nwindow/NCrt.h"
#include "nwindow/NItemC4.h"

// system message 0x53B: a macro cannot be run from a macro (C3 only); the stock shows it through a dialog of the
// console (0x1006B980, ret 4: the console lane's anonymous function that fills an NDialogInfo with the message)
enum { MSG_MacroInMacro = 0x53B };

// C3: twelve slots to a page (C2 ten); the number of slots of the bar
enum { SHORTCUT_SlotsPerPage = 12, SHORTCUT_MaxSlots = 120 };

// the slot of the shortcut bar at Index (the desktop keeps the bar at +0x138, its slots at +0x1E0; the kind of a slot is
// the entry's +0x1910 and its id +0x1918). The stock reads the bar without a check in the lines that follow a check
static FORCEINLINE NShortCutItem* ShortCutSlot(INT Index)
{
    return &GConsole->GetDesktop()->ShortCutWnd->Items[Index];
}

// the kind of a slot of the bar (SHORTCUT_*) and the id it holds: the entry's fields at +0x1B18 / +0x1B20 in this client
static FORCEINLINE INT ShortCutKind(NShortCutItem* Slot)
{
    return AsC4((NItemEntry*)Slot)->Unknown1910;
}
static FORCEINLINE INT ShortCutID(NShortCutItem* Slot)
{
    return AsC4((NItemEntry*)Slot)->ID;
}

// the player's pawn (NConsoleWnd::GetMyPawn, 0x1006A290: the viewport's actor)
static APawn* GetMyPawn()
{
    return GConsole->GetMyPawn();
}

// a macro in a macro: every macro stops and the player is told (the console's dialog)
static void RefuseMacroInMacro()
{
    // 0x10075990 (ShowNoticeDialog, NCGaraDialogBox.cpp, where it is static; the stock ignores its this and reads GConsole): a notice box
    // (dialog 0x1F, the desktop's main box) with one system message
    NDialogInfo Info;
    Info.bInput = 0;
    Info.ID = 0x1F;
    Info.bMainBox = 1;
    Info.SetSystemMessage((INT)MSG_MacroInMacro);
    GConsole->ShowDialog(&Info);
}

// the index in the pawn's table of social actions (APawn::GetSocialAnimDuration) of an action by its number; the stock code repeats this
// switch for an action line and for the action of a shortcut. -1: not a social action. Action 0 sits or stands: the other of what the
// pawn does (the controller's byte at +0x41D in this client; C3 +0x40D, C2 +0x409)
static INT ActionAnimIndex(APawn* Pawn, INT Action)
{
    INT Index = -1;
    switch (Action)
    {
        case 0:
        {
            BYTE State = nw::Field<BYTE>(Pawn->Controller, 0x41D);
            if (State == 0)
                Index = 1;
            else if (State == 1)
                Index = 0;
            break;
        }
        case 0xC: Index = 2; break;
        case 0xD: Index = 3; break;
        case 0xE: Index = 4; break;
        case 0x18: Index = 6; break;
        case 0x19: Index = 5; break;
        case 0x1A: Index = 7; break;
        case 0x1D: Index = 8; break;
        case 0x1E: Index = 9; break;
        case 0x1F: Index = 10; break;
        case 0x21: Index = 11; break;
        case 0x22: Index = 12; break;
        case 0x23: Index = 13; break;
    }
    return Index;
}

// the same for the social commands NCommandParser::GetCommand knows (/sit, /stand, /socialhello ...)
static INT CommandAnimIndex(APawn* Pawn, INT Command)
{
    INT Index = -1;
    switch (Command)
    {
        case 4: Index = 0; break;
        case 5: Index = 1; break;
        case 0x15: Index = 2; break;
        case 0x16: Index = 3; break;
        case 0x17: Index = 4; break;
        case 0x2E: Index = 5; break;
        case 0x2F: Index = 6; break;
        case 0x30: Index = 7; break;
        case 0x31: Index = 8; break;
        case 0x32: Index = 9; break;
        case 0x33: Index = 10; break;
        case 0x36: Index = 11; break;
        case 0x37: Index = 12; break;
        case 0x38: Index = 13; break;
        case 0x39:
        {
            BYTE State = nw::Field<BYTE>(Pawn->Controller, 0x41D);
            if (State == 0)
                Index = 1;
            else if (State == 1)
                Index = 0;
            break;
        }
    }
    return Index;
}

// 0x100a7f80 (ret 4) - runs line Index of the macro: a skill is used (with Ctrl and Shift as the requests take them), an action (its social
// animation sets how long the line waits), an item, a shortcut (a skill or an action one only: a macro in a macro is refused, -1, with
// the dialog 0x53B), a command (typed into the chat as if the player had: the social commands wait for the animation, the lines of
// plain chat after one another wait a fifth of a second a character; C3: /useshortcut of a macro's slot stops every macro) or a
// delay. Returns -1 when the line cannot run (no such line, no pawn)
INT NMacroEntry::ExecuteLine(INT Index)
{
    if (Index >= NumLines)
        return -1;
    NMacroLine* Line = &Lines[Index];
    switch (Line->Type)
    {
        case NMLT_Skill:
        {
            L2ParamStack Params(10);
            Params.PushBack((void*)(INT_PTR)Line->Param08);
            Params.PushBack((void*)(INT_PTR)::nw::VirtualCall<UBOOL>(GConsole, 319));   // Ctrl (IsAttackKeyDown)
            Params.PushBack((void*)(INT_PTR)GConsole->Engine->Client->Viewports(0)->Input->KeyDown(0x10));
            GConsole->RequestSkillList(Params);   // 0x1005D040 (the header's name for the C3 address of the skill request)
            break;
        }
        case NMLT_Action:
        {
            APawn* Pawn = GetMyPawn();
            if (Pawn == NULL)
                return -1;
            FLOAT Seconds = Pawn->GetSocialAnimDuration(ActionAnimIndex(Pawn, Line->Param08));
            if (Seconds > 0.0f)
                Line->Duration = Seconds;
            break;
        }
        case NMLT_Item:
            GConsole->RequestUseItem(Line->Param04);
            break;
        case NMLT_ShortCut:
        {
            INT Slot = Line->Param08 + Line->Param0C * SHORTCUT_SlotsPerPage;
            INT Type = ShortCutKind(ShortCutSlot(Slot));
            if (Type == SHORTCUT_Macro)
            {
                RefuseMacroInMacro();
                return -1;
            }
            if (Type == SHORTCUT_Action)
            {
                NCConsole* Desktop = GConsole->GetDesktop();
                if (Desktop != NULL && Desktop->ShortCutWnd != NULL && Desktop->ShortCutWnd->Items != NULL)
                {
                    APawn* Pawn = GetMyPawn();
                    if (Pawn == NULL)
                        return -1;
                    FLOAT Seconds = Pawn->GetSocialAnimDuration(ActionAnimIndex(Pawn, ShortCutID(ShortCutSlot(Slot))));
                    if (Seconds > 0.0f)
                        Line->Duration = Seconds;
                }
            }
            GConsole->UseShortCut(Slot, 0, 0);   // NConsoleWnd: use the shortcut (0x10076020)
            break;
        }
        case NMLT_Command:
        {
            TCHAR* Text = Line->Text;
            if (Text != NULL)
            {
                if (GConsole != NULL && GConsole->Desktop != NULL && GConsole->GetDesktop()->ChatWnd != NULL)
                    GConsole->GetDesktop()->ChatWnd->ProcessChatLine(Text);   // NCChatWnd: the line as typed (0x10050360)
                INT Command = Parser.GetCommand(Text);
                if (Command == 0x1D || Command == 0x1C)
                {
                    // C3: the two numbers "page slot"; a slot that holds a macro stops every macro (and says why)
                    INT Page = 0;
                    INT Slot = 0;
                    TCHAR* Token = Parser.GetNextToken();
                    if (Token != NULL)
                        Page = NWtol(Token);
                    Token = Parser.GetNextToken();
                    if (Token != NULL)
                        Slot = NWtol(Token);
                    if (Page > 0 && Page < 11 && Slot > 0 && Slot < SHORTCUT_SlotsPerPage + 1)
                    {
                        INT Record = Slot - (SHORTCUT_SlotsPerPage + 1) + Page * SHORTCUT_SlotsPerPage;
                        if (Record < SHORTCUT_MaxSlots)
                        {
                            NShortCutItem* Item = ShortCutSlot(Record);
                            if (Item != NULL && ShortCutKind(Item) == SHORTCUT_Macro)
                            {
                                for (INT i = 0; i < GConsole->GetMacroCount(); i++)
                                    GConsole->GetMacro(i)->bRunning = 0;
                                RefuseMacroInMacro();
                                break;
                            }
                        }
                    }
                }
                if (Command == 4 || Command == 5 || Command == 0xE || Command == 0x13 || Command == 0x15 || Command == 0x16
                    || Command == 0x17 || Command == 0x27 || (Command >= 0x2E && Command <= 0x33) || (Command >= 0x36 && Command <= 0x39))
                {
                    APawn* Pawn = GetMyPawn();
                    if (Pawn == NULL)
                        return -1;
                    FLOAT Seconds = Pawn->GetSocialAnimDuration(CommandAnimIndex(Pawn, Command));
                    if (Seconds > 0.0f)
                        Line->Duration = Seconds;
                }
                else if (Command == NCMD_Chat && Index + 1 < NumLines && Lines[Index + 1].Type == NMLT_Command
                         && Parser.GetCommand(Lines[Index + 1].Text) == Command)
                    Line->Duration = (FLOAT)appStrlen(Text) * 0.2f;
            }
            break;
        }
        case NMLT_Delay:
            Line->Duration = (FLOAT)Line->Param0C;
            break;
    }
    return 1;
}
NW_HOOK_METHOD(0x100a7f80, NMacroEntry, ExecuteLine)

// 0x100aaf60 (ret 4) - the console's tick for a running macro: DeltaTime seconds. A macro that is not running or is past its last line
// stops. The first tick starts line 0; after that the line's clock runs, and the line ends when its Duration has passed, or, when it
// has none (0), as soon as what it waits for is done: a skill line or /useskill, /pickup and the shortcut of a skill wait for the console's
// events (NCMacroListWnd::OnSkillCoolTime / OnPickUpDone set the Duration); /target, /targetnext and /assist wait until the selected actor
// is the one asked for (then OnTargetDone sets a thousandth of a second); every other line ends at once. The next line then starts.
// C3: a /useshortcut or a shortcut line that points at a macro's slot stops the macro (dialog 0x53B).
void NMacroEntry::Tick(FLOAT DeltaTime)
{
    TCHAR Buffer[300];
    appMemset(Buffer, 0, 600);
    INT Cur = CurLine;
    if (Cur >= NumLines || bRunning == 0)
    {
        bRunning = 0;
        CurLine = 0;
        return;
    }
    if (Cur == 0 && Lines[0].StartTime == 0.0f)
    {
        Lines[0].StartTime = DeltaTime;
        Lines[CurLine].Clock = DeltaTime;
        ExecuteLine(0);
        return;
    }
    Lines[Cur].Clock = DeltaTime + Lines[Cur].Clock;
    NMacroLine* Line = &Lines[CurLine];
    UBOOL bEnds = 1;
    if (Line->Type == NMLT_Skill)
        bEnds = 0;
    else if (Line->Type == NMLT_Command)
    {
        INT Command = Parser.GetCommand(Line->Text);
        if (Command == 0x18 || Command == 0x19 || Command == 0x1A || Command == 0xE)
            bEnds = 0;
        else if (Command == 0xC || Command == 0xD || Command == 0xF)
        {
            User* Selected = GConsole->GetSelectedActor();
            INT Wanted = 0;
            UBOOL bAsked = 0;
            if (Command == 0xC)
            {
                TCHAR* Token = Parser.GetNextToken();
                Parser.ExpandToken(Token);
                appSprintf(Buffer, TEXT("%s"), Token);
                Token = Parser.GetNextToken();
                Parser.ExpandToken(Token);
                while (Token != NULL)
                {
                    appSprintf(Buffer, TEXT("%s %s"), Buffer, Token);
                    Token = Parser.GetNextToken();
                    Parser.ExpandToken(Token);
                }
                User* Named = GConsole->FindUserByName(Buffer);
                if (Named != NULL && Named->Pawn != NULL)
                    Wanted = Named->ObjectID;
                bAsked = 1;
            }
            else if (Command == 0xD)
            {
                if (Selected != NULL)
                {
                    User* Next = nw::VirtualCall<User*>(GConsole->Engine->NetHandler, 250, 200.0f,
                                                         nw::Field<INT>(Selected->Pawn, 0x60));
                    if (Next != NULL && Next->Pawn != NULL)
                        Wanted = Next->ObjectID;
                    bAsked = 1;
                }
            }
            else if (Selected != NULL)
            {
                if (Selected->Pawn != NULL)
                {
                    BYTE* Controller = (BYTE*)Selected->Pawn->Controller;
                    if (Controller != NULL && nw::Field<BYTE*>(Controller, 0x400) != NULL)
                    {
                        INT TargetID = nw::Field<INT>(nw::Field<BYTE*>(Controller, 0x400), 0x60);
                        User* Target = GConsole->FindUserByID(TargetID);
                        if (Target != NULL)
                            Wanted = Target->ObjectID;
                        else
                        {
                            BYTE* Record = nw::VirtualCall<BYTE*>(GConsole->Engine->NetHandler, 38, TargetID);
                            if (Record != NULL)
                                Wanted = nw::Field<INT>(Record, 0x8);
                        }
                    }
                }
                bAsked = 1;
            }
            if (!bAsked)
                bEnds = 0;
            else if (Selected != NULL && Selected->Pawn != NULL && Selected->ObjectID == Wanted)
                ((NCMacroListWnd*)GConsole->GetDesktop()->NewWndF8)->OnTargetDone();
            else
                bEnds = 0;
        }
        else if (Command == 0x1D || Command == 0x1C)
        {
            INT Page = 0;
            INT Slot = 0;
            TCHAR* Token = Parser.GetNextToken();
            if (Token != NULL)
                Page = NWtol(Token);
            Token = Parser.GetNextToken();
            if (Token != NULL)
                Slot = NWtol(Token);
            if (Page > 0 && Page <= 10 && Slot > 0 && Slot <= SHORTCUT_SlotsPerPage)
            {
                INT Record = (Slot + Page * SHORTCUT_SlotsPerPage) - (SHORTCUT_SlotsPerPage + 1);
                if (Record < SHORTCUT_MaxSlots)
                {
                    INT Kind = ShortCutKind(ShortCutSlot(Record));
                    if (Kind == SHORTCUT_Skill)
                        bEnds = 0;
                    else if (Kind == SHORTCUT_Macro)
                    {
                        bRunning = 0;
                        CurLine = 0;
                        RefuseMacroInMacro();
                        return;
                    }
                }
            }
        }
    }
    else if (Line->Type == NMLT_ShortCut)
    {
        INT Record = Line->Param08 + Line->Param0C * SHORTCUT_SlotsPerPage;
        if (GConsole != NULL && GConsole->Desktop != NULL && GConsole->GetDesktop()->ShortCutWnd != NULL
            && GConsole->GetDesktop()->ShortCutWnd->Items != NULL && Record < SHORTCUT_MaxSlots)
        {
            INT Kind = ShortCutKind(ShortCutSlot(Record));
            if (Kind == SHORTCUT_Skill)
                bEnds = 0;
            else if (Kind == SHORTCUT_Macro)
            {
                bRunning = 0;
                CurLine = 0;
                RefuseMacroInMacro();
                return;
            }
        }
    }
    Line = &Lines[CurLine];
    if ((Line->Duration > 0.0f && Line->Duration < Line->Clock - Line->StartTime) || (bEnds && Line->Duration == 0.0f))
    {
        Line->StartTime = 0;
        Lines[CurLine].Clock = 0;
        Lines[CurLine].Duration = 0;
        CurLine++;
        if (CurLine < NumLines)
        {
            Lines[CurLine].StartTime = DeltaTime;
            Lines[CurLine].Clock = DeltaTime;
            ExecuteLine(CurLine);
        }
    }
}
NW_HOOK_METHOD(0x100aaf60, NMacroEntry, Tick)
