/*=============================================================================
    NConsoleWndActions.cpp - what the console does for the keyboard and the pages: the
    shortcut slots and the actions of the action window used from the keys, the console
    commands of the chat, the HTML pages' "bypass" commands, and the actors under the mouse
    (an item on the ground, a pawn).
=============================================================================*/
#include "nwindow/NConsoleWnd.h"
#include "nwindow/NCActionItemWnd.h"
#include "nwindow/NCChatListBox.h"
#include "nwindow/NCChatWnd.h"
#include "nwindow/NCComboBox.h"
#include "nwindow/NCConsole.h"
#include "nwindow/NCMatchWnd.h"
#include "nwindow/NCDialogBox.h"
#include "nwindow/NDialogInfo.h"
#include "nwindow/NCHtmlViewer.h"
#include "nwindow/NCShortCutWnd.h"
#include "nwindow/NConsoleWndExt.h"

#include <wchar.h>

// the dialog boxes a shortcut item closes (NDialogInfo::ID, the inventory's)
enum { DIALOG_DROP_ITEM = 4, DIALOG_DESTROY_ITEM = 5, DIALOG_CRYSTALLIZE_ITEM = 6 };

// the actor under the mouse
static FORCEINLINE AActor* HoveredActor(UEngine* Engine)
{
    return static_cast<UGameEngine*>(Engine)->MouseHitActor;
}

// a system message line in the chat window's first list
static FORCEINLINE void ChatSystemMessage(NConsoleWnd* Console, INT MsgID)
{
    Console->GetDesktop()->ChatWnd->Lists[CHATLIST_All]->AddString(
        *GL2GameData.SystemMsg[MsgID].Message, 0xFFB09B79, -1);
}

// 0x10072df0 - a console command (the chat's "/" commands the client does not know) run by
// the first viewport, its output to the log
void NConsoleWnd::ExecCommand(TCHAR* Command)
{
    UClient* Client = Engine->Client;
    if (Client && Client->Viewports.Num())
        static_cast<FExec*>(Client->Viewports(0))->Exec(Command, *GLog);
}
NW_HOOK_METHOD(0x10072df0, NConsoleWnd, ExecCommand)

// 0x1006a890 - the item on the ground under the mouse, NULL for anything else
AActor* NConsoleWnd::GetPickupUnderMouse()
{
    if (!Engine)
        return NULL;
    AActor* Actor = HoveredActor(Engine);
    if (Actor && Actor->IsA(AL2Pickup::StaticClass()))
        return Actor;
    return NULL;
}
NW_HOOK_METHOD(0x1006a890, NConsoleWnd, GetPickupUnderMouse)

// 0x1006e890
APawn* CastToPawn(UObject* Object)
{
    if (Object && Object->IsA(APawn::StaticClass()))
        return (APawn*)Object;
    return NULL;
}
NW_HOOK_FUNC(0x1006e890, CastToPawn)

// the dialog boxes of the shortcut items (NDialogInfo::ID; names ours): an item with a warning (it asks first)
// and an item with a message of its own (NItemEntry::GetUnknown19AC)
enum { DIALOG_CONFIRM_ITEM = 0x21, DIALOG_ITEM_MESSAGE = 0x45 };

// 0x1007e8d0 (ret 0xC) - an action used from a shortcut key (UseShortCut) or the action window: the requests of the action, the social
// motions by the action table's parameter; the others go to the server with Ctrl (force) and Shift (don't move). C4: the attack falls back to
// the selected door or other static actor; the invitation sends the name and the party item rule (PartyItemRule, +0x3C6C); 0x37 starts or stops the
// replay recording (new), 0x38 asks to join the command channel of the selected player (new, 0x32 is the hand over of the party's master);
// 0x28 (recommend) is not followed by the plain request; the party matching (11) is a request of its own and sets two combo boxes of the window
// (+0x164) to the first line. The actions that no case names (including 0x25, the recipe shop's) go to the server
void NConsoleWnd::UseAction(INT ActionID, INT bForce, INT bShift)
{
    switch (ActionID)
    {
    case 2:
    {
        User* Target = GetSelectedActor();
        AActor* Static = GetSelectedStaticActor();
        User* Me = GetMyUser();
        if (Me)
        {
            if (Target)
                RequestAttack(Target->ObjectID, Me->Pawn->Location, bForce, bShift);
            else if (Static)
                RequestAttack(nw::Field<INT>(Static, 0x60), Me->Pawn->Location, bForce, bShift);
        }
        break;
    }
    case 3:
    {
        User* Target = GetSelectedActor();
        if (Target && Target->Pawn)
            RequestTrade(Target->Pawn->CreatureID);
        break;
    }
    case 4:
    {
        User* Target = GetSelectedActor();
        if (!Target || !Target->Pawn)
            RequestTargetNext(200, -1);
        else
            RequestTargetNext(200, Target->Pawn->CreatureID);
        break;
    }
    case 5:
        RequestPickUp(200, -1);
        break;
    case 0x12:
        PetPickUp(200, -1);
        break;
    case 6:
        RequestAssist();
        break;
    case 7:
    {
        User* Target = GetSelectedActor();
        if (!Target || Target->Unknown8)
            ChatSystemMessage(this, 185);   // "Select user to invite to your party."
        else
        {
            L2ParamStack Params(10);
            Params.PushBack((BYTE*)Target + 0x1C);                          // the name
            Params.PushBack((void*)(INT_PTR)PartyItemRule);                 // the party item rule (the second party value)
            RequestPartyInvite(Params);
        }
        break;
    }
    case 8:
    {
        L2ParamStack Params(10);
        RequestPartyLeave(Params);
        break;
    }
    case 9:
    {
        User* Target = GConsole->GetSelectedActor();
        if (!Target || Target->Unknown8)
            ChatSystemMessage(this, 187);   // "Select user to expel."
        else
        {
            L2ParamStack Params(10);
            Params.PushBack(Target->GetName());
            RequestPartyDismiss(Params);
        }
        break;
    }
    case 0xA:
        RequestVendor();
        break;
    case 0x1C:
        RequestBuy();
        break;
    case 0xB:
    {
        // the list of the party matching: everyone (1), any level (-1), any place (1); then the window's two selection boxes go to the
        // first line
        L2ParamStack Params(10);
        Params.PushBack((void*)1);
        Params.PushBack((void*)-1);
        Params.PushBack((void*)1);
        ((NConsoleForwardFns*)this)->RequestListPartyWating(Params);
        nw::Field<NCComboBox*>(nw::Field<NCWnd*>(GConsole->Desktop, 0x164), 0xEC)->SetSelected(1);
        nw::Field<NCComboBox*>(nw::Field<NCWnd*>(GConsole->Desktop, 0x164), 0xF0)->SetSelected(1);
        break;
    }
    case 0xC:
    case 0xD:
    case 0xE:
    case 0x18:
    case 0x19:
    case 0x1A:
    case 0x1D:
    case 0x1E:
    case 0x1F:
    case 0x21:
    case 0x22:
    case 0x23:
        if (ActionID > 0 && ActionID < GL2GameData.ActionTable.Num())
        {
            INT Social = *(INT*)((BYTE*)GL2GameData.ActionTable(ActionID) + 8);   // the social action of the entry
            if (Social > 0)
                RequestSocialAction(Social);
        }
        break;
    case 0x28:
    {
        // the recommendation of a player (UL2NetHandler vslot 268, +0x430)
        User* Target = GetSelectedActor();
        if (!Target)
            ShowSystemMessage(0xF2, 0xFFB09B79);
        else if (Target->Unknown8 == 0)
        {
            if (Engine && Engine->NetHandler)
                nw::VirtualCall<void>(Engine->NetHandler, 268, Target->Pawn->CreatureID);
        }
        else
            ShowSystemMessage(0x90, 0xFFB09B79);
        break;
    }
    case 0x32:
    {
        User* Target = GConsole->GetSelectedActor();
        if (!Target || Target->Unknown8)
            ChatSystemMessage(this, 1400);
        else
        {
            L2ParamStack Params(10);
            Params.PushBack((BYTE*)Target + 0x1C);                          // the name
            ((NConsoleForwardFns*)this)->RequestHandOverPartyMaster(Params);
        }
        break;
    }
    case 0x37:
        if (Engine->ReplayManager)
            Engine->ReplayManager->OnSaveStartStop();
        break;
    case 0x38:
    {
        User* Target = GConsole->GetSelectedActor();
        if (!Target || Target->Unknown8)
            GConsole->ShowSystemMessage(0x32, 0xFFB09B79);
        else
        {
            L2ParamStack Params(10);
            Params.PushBack((BYTE*)Target + 0x1C);                          // the name
            GConsole->RequestExAskJoinMPCC(Params);
        }
        break;
    }
    default:
    {
        if (nw::VirtualCall<UBOOL>(this, 319))                              // Ctrl
            bForce = nw::VirtualCall<UBOOL>(this, 319);
        if (Engine->Client->Viewports(0)->Input->KeyDown(IK_Shift))
            bShift = Engine->Client->Viewports(0)->Input->KeyDown(IK_Shift);
        L2ParamStack Params(10);
        Params.PushBack((void*)(INT_PTR)ActionID);
        Params.PushBack((void*)(INT_PTR)bForce);
        Params.PushBack((void*)(INT_PTR)bShift);
        RequestActionUse(Params);
        break;
    }
    }
}
NW_HOOK_METHOD(0x1007e8d0, NConsoleWnd, UseAction)

// 0x100802f0 (ret 0xC) - a slot of the shortcut bar used from a key (HookShortCutKey, UseShortCutSlot): a skill with Ctrl (force) and Shift
// (don't move), an action, a macro, a recipe (new in C4: its make info is asked) or an item of the player's inventory or the pet's (an
// item with a warning, or with a message of its own, asks first). The slots are 0x1CC0 bytes from the bar's +0x200; this client's type is
// at +0x1B18 (1 item, 2 skill, 3 action, 4 macro, 5 recipe), the object id at +0x1B1C, the id at +0x1B20, the warning flag at +0x1BA4, the
// owner of an item at +0x1C98. The warning only counts for the dwarven classes (sub_100753D0)
void NConsoleWnd::UseShortCut(INT Slot, INT bForce, INT bShift)
{
    BYTE* Item = nw::Field<BYTE*>(nw::Field<NCWnd*>(Desktop, 0x15C), 0x200) + Slot * 0x1CC0;
    if (!Item)
        return;
    if (nw::VirtualCall<UBOOL>(this, 319))                          // Ctrl
        bForce = nw::VirtualCall<UBOOL>(this, 319);
    if (Engine->Client->Viewports(0)->Input->KeyDown(IK_Shift))
        bShift = Engine->Client->Viewports(0)->Input->KeyDown(IK_Shift);
    INT Type = nw::Field<INT>(Item, 0x1B18);
    if (Type == 1)
    {
        if (nw::Field<INT>(Item, 0x1BA4) != 0 && ((NConsoleLeftFns*)this)->IsDwarf())
        {
            TCHAR Name[64];
            appSprintf(Name, TEXT("%s"), ((NItemEntry*)Item)->GetName());
            NDialogInfo Info;
            Info.ID = DIALOG_CONFIRM_ITEM;
            Info.bMainBox = 1;
            Info.bInput = 0;
            Info.Owner = nw::Field<NWndEntry*>(nw::Field<NCWnd*>(Desktop, 0x15C), 0x74);
            Info.SetSystemMessage(0x31E);
            Info.SetButtons(Name, NULL);
            Info.SetParams(nw::Field<INT>(Item, 0x1B1C), -1);
            GConsole->ShowDialog(&Info);
        }
        else if (((NItemEntry*)Item)->GetUnknown19AC() > 0)
        {
            NDialogInfo Info;
            Info.ID = DIALOG_ITEM_MESSAGE;
            Info.bMainBox = 1;
            Info.bInput = 0;
            Info.Owner = nw::Field<NWndEntry*>(nw::Field<NCWnd*>(Desktop, 0x15C), 0x74);
            Info.SetSystemMessage(((NItemEntry*)Item)->GetUnknown19AC());
            Info.SetParams(nw::Field<INT>(Item, 0x1B1C), -1);
            GConsole->ShowDialog(&Info);
        }
        else if (nw::Field<INT>(Item, 0x1C98) == 1)
            RequestUseItem(nw::Field<INT>(Item, 0x1B1C));
        else if (nw::Field<INT>(Item, 0x1C98) == 2)
            RequestPetUseItem(nw::Field<INT>(Item, 0x1B1C));
    }
    else if (Type == 2)
    {
        L2ParamStack Params(10);
        Params.PushBack((void*)(INT_PTR)nw::Field<INT>(Item, 0x1B20));
        Params.PushBack((void*)(INT_PTR)bForce);
        Params.PushBack((void*)(INT_PTR)bShift);
        RequestSkillList(Params);
    }
    else if (Type == 3)
        UseAction(nw::Field<INT>(Item, 0x1B20), bForce, bShift);
    else if (Type == 4)
        StartMacro(nw::Field<INT>(Item, 0x1B20));
    else if (Type == 5)
        RequestRecipeItemMakeInfo(nw::Field<INT>(Item, 0x1B1C));
}
NW_HOOK_METHOD(0x100802f0, NConsoleWnd, UseShortCut)

// 0x10072560 (vslot 402) - an HTML page's command: "bypass [-h] ..." sends its tokens
// ("$name": the value of the page's control of that name) joined by spaces to the server
// (the tutorial's pages their own way); the result is 2 when an "-h" came first (the page
// closes), else 1, and 0 for a bypass without tokens (nothing sent). Any other command is
// the desktop's.
INT NConsoleWnd::RunHtmlCommand(TCHAR* Command, NCHtmlViewer* Viewer)
{
    INT Result = 0;
    const TCHAR* Str = Command;
    if (!ParseCommand(&Str, TEXT("BYPASS")))
        return GetDesktop()->OnHtmlCommand(Command, Viewer);
    TCHAR Bypass[1024] = {0};
    L2ParamStack Params(10);
    TCHAR Token[256];
    if (ParseToken(Str, Token, 256, 1))
    {
        do
        {
            if (Result != 2 && !wcscmp(Token, TEXT("-h")))
                Result = 2;
            else if (Token[0] == '$')
            {
                if (Viewer)
                {
                    // virtual 72 of the viewer (+0x120): GetControlValue by slot, as the stock does
                    TCHAR* Value = nw::VirtualCall<TCHAR*>(Viewer, 0x138 / 4, &Token[1]);
                    if (Value)
                    {
                        if (!Bypass[0])
                            appSprintf(Bypass, TEXT("%s"), Value);
                        else
                            appSprintf(Bypass, TEXT("%s %s"), Bypass, Value);
                    }
                }
            }
            else if (!Bypass[0])
                appSprintf(Bypass, TEXT("%s"), Token);
            else
                appSprintf(Bypass, TEXT("%s %s"), Bypass, Token);
        } while (ParseToken(Str, Token, 256, 1));
        Params.PushBack(Bypass);
        if (Viewer->IsA(NCTutorialHtmlViewer::StaticClass())   /* NClasses.gen.h has no C4 address for it */)
            RequestTutorialPassCmdToServer(Params);
        else
            RequestBypassToServer(Params);
        if (!Result)
            Result = 1;
    }
    return Result;
}
NW_HOOK_METHOD(0x10072560, NConsoleWnd, RunHtmlCommand)
