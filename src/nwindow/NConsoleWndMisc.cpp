/*=============================================================================
    NConsoleWndMisc.cpp - the small services of the console that the windows call: the player's
    pawn and place, the macros the macro window keeps (the list, a macro by its id), the tattoo
    slots by the class, the additional name of an item, the party window's names, a few tail calls
    into the window manager and the network handler. Every one of them was an anonymous function of
    the C2 image (names ours).
=============================================================================*/
#include "nwindow/NConsoleWnd.h"
#include "nwindow/NConsoleWndC4.h"
#include "nwindow/NConsoleWndExt.h"
#include "nwindow/NCOlympiadPlayerWnd.h"
#include "nwindow/NCBroadcastGMWnd.h"
#include "nwindow/NCBroadcastObserverGMMsgWnd.h"
#include "nwindow/NCPartyRoomWnd.h"
#include "nwindow/NCStoreWnd.h"
#include "nwindow/NCStatusBarCtrl.h"
#include "nwindow/NCSSQMainEventWnd.h"
#include "nwindow/NCSSQMainEventGaraTreeCtrl.h"
#include "nwindow/NCSSQSealStatusListWnd.h"
#include "nwindow/NCSSQSealStatusGaraTreeCtrl.h"
#include "nwindow/NCSSQStatusWnd.h"
#include "nwindow/NCVideoOptionWnd.h"
#include "nwindow/NCQuestWnd.h"
#include "nwindow/NItemEntryC4.h"
#include "nwindow/L2Friend.h"
#include "nwindow/L2ChatWnd.h"
#include "nwindow/L2ChatBox.h"
#include "nwindow/L2FriendWnd.h"
#include "nwindow/L2TaskWnd.h"
#include "nwindow/Msn.h"
#include "nwindow/NMsnTree.h"
#include "nwindow/TaskWnd.h"
#include "nwindow/NCItemWnd.h"
#include "nwindow/NCConsole.h"
#include "nwindow/NCEquipItemWnd.h"
#include "nwindow/NCDeliverWnd.h"
#include "nwindow/NCVIPShopWnd.h"
#include "nwindow/NGameData.h"
#include "nwindow/NItemInfo.h"
#include "nwindow/NCAbnormalStatusWnd.h"
#include "nwindow/NCBroadcastObserverWnd.h"
#include "nwindow/NCChatWnd.h"
#include "nwindow/NCMenuWnd.h"
#include "nwindow/NCRadarWnd.h"
#include "nwindow/NCSystemMenuWnd.h"
#include "nwindow/NCInventoryWnd.h"
#include "nwindow/NCMacroListWnd.h"
#include "nwindow/NCMainWnd.h"
#include "nwindow/NCMatchWnd.h"
#include "nwindow/NCMinimapWnd.h"
#include "nwindow/NCLoadingWnd.h"
#include "nwindow/NCTrainWnd.h"
#include "nwindow/NCPartyWnd.h"
#include "nwindow/NCShortCutWnd.h"
#include "nwindow/NCStatusWnd.h"
#include "nwindow/NCTree.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NCZoneTitleWnd.h"
#include "nwindow/NCVirtualWndMain.h"
#include "nwindow/NGameGuard.h"
#include "nwindow/NMacroEntry.h"
#include "nwindow/NDialogInfo.h"
#include "nwindow/NCDialogBox.h"
#include <intrin.h>

// (the nProtect GameGuard library's answer to a good start, 0x755, is GGameGuardOK 0x101DE1E8: NConsoleWndInput.cpp, NwGameGuard.cpp)
// An L2Friend passed by value, as the stock callee gets it: six words that the callee owns (it destroys the name). Plain
// words, so that a hook does not copy the string; NMsnFriendOf reads them as the record
// NL2FriendArg (NConsoleWndExt.h)

struct NMsnMainPanel;

// The messenger window (the desktop's +0x18C): its main panel is the child at +0xCC
// NMsnShellWnd (NConsoleWndExt.h)

// the player's windows of the console that the macro functions go through
static FORCEINLINE NCMacroListWnd* MacroWnd(NConsoleWnd* Console)
{
    return static_cast<NCMacroListWnd*>(static_cast<NCConsole*>(Console->Desktop)->NewWndF8);
}

// 0x10073eb0 - the player's pawn: the first viewport's controller's pawn. The stock reads through without a check
// (the macro executor asks it only once a viewport exists)
APawn* NConsoleWnd::GetMyPawn()
{
    return Engine->Client->Viewports(0)->Actor->Pawn;
}
NW_HOOK_METHOD(0x10073eb0, NConsoleWnd, GetMyPawn)

// 0x1006eb10 - where the player's pawn stands: (0, 0, 0) without a controller or a pawn. (C2: anonymous; its C1 twin,
// GetMyLocation, was 0x10038540)
FVector NConsoleWnd::GetMyLocation()
{
    FVector Result(0.f, 0.f, 0.f);
    APlayerController* Controller = Engine->Client->Viewports(0)->Actor;
    if (Controller && Controller->Pawn)
        Result = Controller->Pawn->Location;
    return Result;
}
NW_HOOK_METHOD(0x1006eb10, NConsoleWnd, GetMyLocation)

/*-----------------------------------------------------------------------------
    The macros (the macro window's list: TArray<NMacroEntry*>, +0xC4 of the window).
-----------------------------------------------------------------------------*/

// 0x1006abc0 - how many macros there are: those of the macro window (the desktop's +0x150, its list at +0x12C), 0 without the window
// (the stock callers keep ecx across it: HookShortCutKey and Tick are ported with it)
INT NConsoleWnd::GetMacroCount()
{
    if (Desktop && nw::Field<NCWnd*>(Desktop, 0x150))
        return nw::Field<INT>(nw::Field<NCWnd*>(Desktop, 0x150), 0x130);
    return 0;
}
NW_HOOK_METHOD(0x1006abc0, NConsoleWnd, GetMacroCount)

// 0x10074060 - the macro at Index; NULL out of range
NMacroEntry* NConsoleWnd::GetMacro(INT Index)
{
    if (GetMacroCount() > Index && Index >= 0)
        return MacroWnd(this)->Macros(Index);
    return NULL;
}
NW_HOOK_METHOD(0x10074060, NConsoleWnd, GetMacro)

// 0x10074090 - whether a macro of the list is running (HookShortCutKey: a key press stops it first)
UBOOL NConsoleWnd::IsAnyMacroRunning()
{
    INT i = 0;
    if (GetMacroCount() > 0)
    {
        do
        {
            if (GetMacro(i) && GetMacro(i)->bRunning)
                break;
            i++;
        } while (i < GetMacroCount());
    }
    return i < GetMacroCount();
}
NW_HOOK_METHOD(0x10074090, NConsoleWnd, IsAnyMacroRunning)

// 0x100740f0 - the name of the macro with the server's id MacroID, NULL when the list has none
TCHAR* NConsoleWnd::GetMacroName(INT MacroID)
{
    if (GetMacroCount())
    {
        INT i = 0;
        if (GetMacroCount() > 0)
        {
            do
            {
                if (GetMacro(i) && GetMacro(i)->ID == MacroID)
                    return GetMacro(i)->Name;
                i++;
            } while (i < GetMacroCount());
        }
    }
    return NULL;
}
NW_HOOK_METHOD(0x100740f0, NConsoleWnd, GetMacroName)

// 0x10074150 - the icon number of the macro with the id, -1 when the list has none
INT NConsoleWnd::GetMacroIconType(INT MacroID)
{
    if (GetMacroCount())
    {
        INT i = 0;
        if (GetMacroCount() > 0)
        {
            do
            {
                if (GetMacro(i) && GetMacro(i)->ID == MacroID)
                    return GetMacro(i)->IconType;
                i++;
            } while (i < GetMacroCount());
        }
    }
    return -1;
}
NW_HOOK_METHOD(0x10074150, NConsoleWnd, GetMacroIconType)

// 0x10074e00 - a macro slot of the shortcut bar is used: the macro with that id starts running (its flag; the tick
// runs its lines)
void NConsoleWnd::StartMacro(INT MacroID)
{
    INT i = 0;
    if (GetMacroCount() > 0)
    {
        do
        {
            if (GetMacro(i) && GetMacro(i)->ID == MacroID)
            {
                GetMacro(i)->bRunning = 1;
                return;
            }
            i++;
        } while (i < GetMacroCount());
    }
}
NW_HOOK_METHOD(0x10074e00, NConsoleWnd, StartMacro)

// 0x1006ab80 - a macro object (made by ReceiveMacroListInfo) goes into the macro window's list; 0 when there is no
// object or no window
UBOOL NConsoleWnd::AddMacro(NMacroEntry* Macro)
{
    if (Macro && Desktop && static_cast<NCConsole*>(Desktop)->NewWndF8)
    {
        MacroWnd(this)->AddMacro(Macro);
        return 1;
    }
    return 0;
}
NW_HOOK_METHOD(0x1006ab80, NConsoleWnd, AddMacro)

/*-----------------------------------------------------------------------------
    The player's class.
-----------------------------------------------------------------------------*/

// 0x10074450 - how many tattoo slots the player has: by the profession tier of its class (the 58 classes: the
// base class 1, the first profession 2, the second 3); -1 without a player or for an id past the table
INT NConsoleWnd::GetHennaSlotCount()
{
    User* Me = GetMyUser();
    // C3: the stock reads the class id at User +0x200 (C2: +0x54); include/ue2's User puts PKCount there and ClassID at
    // +0x54 (the coordinator's header)
    INT ClassID = Me ? nw::Field<INT>(Me, 0x210) : 0;
    if (Me && (UINT)ClassID <= 0x39)
    {
        switch (ClassID)
        {
        case 0: case 10: case 18: case 25: case 31: case 38: case 44: case 49: case 53:
            return 1;
        case 1: case 4: case 7: case 11: case 15: case 19: case 22: case 26: case 29: case 32:
        case 35: case 39: case 42: case 45: case 47: case 50: case 54: case 56:
            return 2;
        case 2: case 3: case 5: case 6: case 8: case 9: case 12: case 13: case 14: case 16: case 17:
        case 20: case 21: case 23: case 24: case 27: case 28: case 30: case 33: case 34: case 36:
        case 37: case 40: case 41: case 43: case 46: case 48: case 51: case 52: case 55: case 57:
            return 3;
        }
    }
    if (Me && ClassID >= 0x58 && ClassID <= 0x76)   // the third professions of this client
        return 3;
    return -1;
}
NW_HOOK_METHOD(0x10074450, NConsoleWnd, GetHennaSlotCount)

/*-----------------------------------------------------------------------------
    Items, the party and the window manager.
-----------------------------------------------------------------------------*/

// 0x1006c040 - the item has an additional name (the data holds "NAME_None" when it has not)
UBOOL NConsoleWnd::HasItemAdditionalName(INT ItemID)
{
    TCHAR* Name = GL2GameData.GetItemAdditionalName(ItemID);
    if (Name && appStrcmp(Name, TEXT("NAME_None")))
        return 1;
    return 0;
}
NW_HOOK_METHOD(0x1006c040, NConsoleWnd, HasItemAdditionalName)

// 0x1006c2e0 - the item's additional name. The stock compares it with "NAME_None" and drops the result: it returns the
// name either way
const TCHAR* NConsoleWnd::GetItemAdditionalName(INT ItemID)
{
    TCHAR* Name = GL2GameData.GetItemAdditionalName(ItemID);
    if (Name)
        appStrcmp(Name, TEXT("NAME_None"));
    return Name;
}
NW_HOOK_METHOD(0x1006c2e0, NConsoleWnd, GetItemAdditionalName)

// 0x10065af0 - the name of the Index-th member of the party window (the %party1.. words of a /command, ExpandToken);
// NULL out of range or without the window
TCHAR* NConsoleWnd::GetPartyMemberName(INT Index)
{
    if (Desktop)
    {
        NCPartyWnd* Party = static_cast<NCConsole*>(Desktop)->PartyWnd;
        if (Party && Party->Members)
        {
            if (Index < Party->NumMembers && Index >= 0)   // C3: NCPartyWnd.h has NumMembers at 0xD8, the stock reads +0xD4
                return Party->Members[Index]->Name;
        }
    }
    return NULL;
}
NW_HOOK_METHOD(0x10065af0, NConsoleWnd, GetPartyMemberName)

// 0x1006a2e0 - the mouse position of the game window, from the window manager (a tail call into
// NCVirtualWndMain::GetMousePos, 0x1002e390; NCWnd::Snap asks it)
UBOOL NConsoleWnd::GetMousePos(INT* X, INT* Y)
{
    return WndMain->GetMousePos(X, Y);
}
NW_HOOK_METHOD(0x1006a2e0, NConsoleWnd, GetMousePos)

// 0x10069be0 - the GM's snoop window was closed: NetHandler slot 251 (a tail call)
void NConsoleWnd::RequestGMSnoopEnd(INT SnoopID)
{
    Engine->NetHandler->GMSnoopEnd(SnoopID);
}
NW_HOOK_METHOD(0x10069be0, NConsoleWnd, RequestGMSnoopEnd)

// 0x1006a2a0 - a line into the desktop's chat window, with sub type 5 and the given color (the /setcl help)
void NConsoleWnd::ShowChatText(const TCHAR* Text, DWORD Color)
{
    if (Desktop)
        static_cast<NCConsole*>(Desktop)->AddSystemText(Text, 5, Color);
}
NW_HOOK_METHOD(0x1006a2a0, NConsoleWnd, ShowChatText)

// 0x10069df0 - the text of system message MsgID with one number as its parameter, written to Out instead of shown
// (the security card prompt of the login box): the number is added as a parameter of type 1 (slot 190), then the message is
// ended (slot 191) with the text asked for
void NConsoleWnd::GetNumberMessage(INT MsgID, INT Number, TCHAR* Out)
{
    L2ParamStack Params(10);
    Params.PushBack((void*)1);
    Params.PushBack((void*)(INT_PTR)Number);
    nw::VirtualCall<void>(this, 190, &Params);
    L2ParamStack Message(10);
    Message.PushBack((void*)(INT_PTR)MsgID);
    nw::VirtualCall<void>(this, 191, &Message, 1, Out, 0);
}
NW_HOOK_METHOD(0x10069df0, NConsoleWnd, GetNumberMessage)

// 0x10065060 (guard name) - the card key for the login (NetHandler slot 68, RequestSCCheck)
UBOOL NConsoleWnd::RequestAuthCardKeyLogin(INT CardID, INT Key)
{
    guard(NConsoleWnd::RequestAuthCardKeyLogin);
    return Engine->NetHandler->RequestSCCheck(CardID, Key) != 0;
    unguard;
}
NW_HOOK_METHOD(0x10065060, NConsoleWnd, RequestAuthCardKeyLogin)

// 0x1007a130 - the "/useshortcut" kind of command 0x50: the item of the shortcut slot (the records of 0x1CC0 bytes from the
// array at +0x200 of the shortcut window), when it is a weapon or armor that the player does not wear yet (the equipment window's
// records are searched by their id +0x1B20), is put on (RequestUseItem with the object id +0x1B1C). The slot Index counts the pages
void NConsoleWnd::UseShortCutEquip(INT Index)
{
    BYTE* Item = nw::Field<BYTE*>(static_cast<NCConsole*>(Desktop)->ShortCutWnd, 0x200) + Index * 0x1CC0;
    if (Item && nw::Field<INT>(Item, 0x1B14) == 1)
    {
        NCWnd* Equip = nw::Field<NCWnd*>(static_cast<NCConsole*>(Desktop)->InventoryWnd, 0x114);
        INT Num = nw::Field<INT>(Equip, 0xF4);
        for (INT i = 0; i < Num; i++)
        {
            BYTE* Other = nw::Field<BYTE**>(Equip, 0xF0)[i];
            if (Other && nw::Field<INT>(Other, 0x1B20) == nw::Field<INT>(Item, 0x1B20))
                return;
        }
        NViewItemDataC4* Data = (NViewItemDataC4*)NItemTable::Find((INT*)(Item + 0x1B20));
        if (Data && (Data->Class == ITEMCLASS_C4_Weapon || Data->Class == ITEMCLASS_C4_Armor))
            RequestUseItem(nw::Field<INT>(Item, 0x1B1C));
    }
}
NW_HOOK_METHOD(0x1007a130, NConsoleWnd, UseShortCutEquip)

// 0x1005c450 - Cast<ALineagePlayerController>, out of line
ALineagePlayerController* CastToLineagePlayerController(UObject* Object)
{
    if (Object && Object->IsA(ALineagePlayerController::StaticClass()))
        return (ALineagePlayerController*)Object;
    return NULL;
}
NW_HOOK_FUNC(0x1005c450, CastToLineagePlayerController)

// 0x10060620  NOT PORTED (no C4 counterpart; name ours: C1's game side of the nProtect GameGuard start-up. C4 has it as the
// GIsL2NetLogin branch of NConsoleWnd::Initialize, NConsoleWnd::InitGameMon_part 0x1006BD60, which Initialize calls through
// NGameGuard::StartGameMon). Nothing calls this one; the standalone has the documented no-op of NGameGuard.h (the library is not linked)
void NConsoleWnd::StartGameGuard(INT hWnd)
{
#if NW_HOOK_MODE
    NW_UNPORTED_NORET();
#else
    NGameGuard::StartGameMon(this, hWnd);
#endif
}

// 0x1006b460 - the allocator's Free (slot 302, a virtual of the console that does nothing else; the stock linker folded the
// global operator delete into it, so NString and the other stock code call it as a function): GMalloc->Free(Ptr)
void NConsoleWnd::Free(void* Ptr)
{
    appFree(Ptr);
}
NW_HOOK_METHOD(0x1006b460, NConsoleWnd, Free)

/*-----------------------------------------------------------------------------
    The virtuals only C3 has that forward to a window of the desktop (names ours; each takes the
    stock's arguments and returns what it returns). The functions called by address are the ones
    this port has no class for.
-----------------------------------------------------------------------------*/

// 0x10068350 (C3 virtual 166) - the buff window's 0x10102BA0 (name unknown)
void NConsoleWnd::C3Virtual166()
{
    GetDesktop()->AbnormalStatusWnd->ClearExtraStatus();
}
NW_HOOK_METHOD(0x10068350, NConsoleWnd, C3Virtual166)

// 0x10068370 (C3 virtual 168) - the buff window's 0x10102B30 (name unknown)
void NConsoleWnd::C3Virtual168()
{
    GetDesktop()->AbnormalStatusWnd->ClearStatus2();
}
NW_HOOK_METHOD(0x10068370, NConsoleWnd, C3Virtual168)

// 0x10066040 (vslot 203) - the window of the desktop's +0x26C hidden and its flag at +0x114 cleared
void NConsoleWnd::C3Virtual202()
{
    nw::Field<NCWnd*>(Desktop, 0x26C)->HideWindow();
    nw::Field<INT>(nw::Field<NCWnd*>(Desktop, 0x26C), 0x114) = 0;
}
NW_HOOK_METHOD(0x10066040, NConsoleWnd, C3Virtual202)

// 0x10066070 (vslot 204) - and shown
void NConsoleWnd::C3Virtual203()
{
    nw::Field<NCWnd*>(Desktop, 0x26C)->ShowWindow();
}
NW_HOOK_METHOD(0x10066070, NConsoleWnd, C3Virtual203)

// 0x100660a0 (C3 virtual 206) - the loading screen shown
void NConsoleWnd::C3Virtual206()
{
    LoadingWnd->ShowWindow();
}
NW_HOOK_METHOD(0x100660a0, NConsoleWnd, C3Virtual206)

// 0x100695b0 (C3 virtual 270) - the desktop's ToggleMinimap
void NConsoleWnd::C3Virtual270(INT Mode)
{
    GetDesktop()->ToggleMinimap(Mode);
}
NW_HOOK_METHOD(0x100695b0, NConsoleWnd, C3Virtual270)

// 0x1006ca40 (C3 virtual 291) - the messenger window's 0x10139670 (name unknown)
void NConsoleWnd::C3Virtual291()
{
    ((NMsnShellWnd*)GetDesktop()->MessengerWnd)->PanelCall139370();
}
NW_HOOK_METHOD(0x1006ca40, NConsoleWnd, C3Virtual291)

// 0x1006ca60 (C3 virtual 293) - the messenger window's 0x10131530 (name unknown)
void NConsoleWnd::C3Virtual293()
{
    ((NMsnShellWnd*)GetDesktop()->MessengerWnd)->ReloadFriends();
}
NW_HOOK_METHOD(0x1006ca60, NConsoleWnd, C3Virtual293)

// C3 only (0x10060bc0 is not a function of this client and nothing calls this; the desktop hidden): an inert body
void NConsoleWnd::C3Virtual346()
{
}

// C3 only (0x10060be0 is not a function of this client and nothing calls this; the desktop shown): an inert body
void NConsoleWnd::C3Virtual347()
{
}

// 0x100753b0 (C3 virtual 349) - the petition window's +0xFC child (C4; C3: +0xDC of the window the C3 header called PetitionWnd) enabled
void NConsoleWnd::C3Virtual349()
{
    nw::Field<NCWnd*>(PetitionWnd, 0xFC)->EnableWindow(1);
}
NW_HOOK_METHOD(0x100753b0, NConsoleWnd, C3Virtual349)

// 0x100663f0 (C3 virtual 350) - the zone title window's 0x1010B320 with the three values
void NConsoleWnd::C3Virtual350(INT A, INT B, INT C)
{
    if (Desktop && GetDesktop()->ZoneTitleWnd)
        ((NCZoneTitleWndExt*)GetDesktop()->ZoneTitleWnd)->SetTexts((const TCHAR*)A, (const TCHAR*)B, (const TCHAR*)C);
}
NW_HOOK_METHOD(0x100663f0, NConsoleWnd, C3Virtual350)

// 0x100826b0 (C3 virtual 351) - a flag of the character window's quest page (+0xFC)
UBOOL NConsoleWnd::C3Virtual351()
{
    if (Desktop && GetDesktop()->MainWnd && GetDesktop()->MainWnd->QuestWnd)
        return nw::Field<BYTE>(GetDesktop()->MainWnd->QuestWnd, 0xFC) != 0;
    return 0;
}
NW_HOOK_METHOD(0x100826b0, NConsoleWnd, C3Virtual351)

// 0x100826e0 (C3 virtual 352) - the NCGodWnd's 0x10085C50 as a flag
UBOOL NConsoleWnd::C3Virtual352()
{
    if (GodWnd)
        return ((NCGodWndExt*)GodWnd)->GetFlag() != 0;
    return 0;
}
NW_HOOK_METHOD(0x100826e0, NConsoleWnd, C3Virtual352)

// 0x10082700 (C3 virtual 353) - a flag of the character window's quest page (+0xFD)
UBOOL NConsoleWnd::C3Virtual353()
{
    if (Desktop && GetDesktop()->MainWnd && GetDesktop()->MainWnd->QuestWnd)
        return nw::Field<BYTE>(GetDesktop()->MainWnd->QuestWnd, 0xFD) != 0;
    return 0;
}
NW_HOOK_METHOD(0x10082700, NConsoleWnd, C3Virtual353)

// 0x10082730 (C3 virtual 354) - sets the quest page's flag at +0xDD
void NConsoleWnd::C3Virtual354(INT bValue)
{
    if (Desktop && GetDesktop()->MainWnd && GetDesktop()->MainWnd->QuestWnd)
        nw::Field<BYTE>(GetDesktop()->MainWnd->QuestWnd, 0xFD) = (bValue != 0);
}
NW_HOOK_METHOD(0x10082730, NConsoleWnd, C3Virtual354)

// 0x10082880 (C3 virtual 357) - the quest page's 0x100DA3A0 with three values
void NConsoleWnd::C3Virtual357(INT A, INT B, INT C)
{
    if (Desktop && GetDesktop()->MainWnd && GetDesktop()->MainWnd->QuestWnd)
        ((NCQuestGaraTreeWndExt*)GetDesktop()->MainWnd->QuestWnd)->SetTargetPos(A, B, C);
}
NW_HOOK_METHOD(0x10082880, NConsoleWnd, C3Virtual357)

// 0x1006da80 (C3 virtual 374) - the party matching window's 0x100A5B10
void NConsoleWnd::C3Virtual374(L2ParamStack& Params)
{
    GetDesktop()->MatchWnd->SetList(Params);
}
NW_HOOK_METHOD(0x1006da80, NConsoleWnd, C3Virtual374)

// 0x1006daa0 (C3 virtual 375) - the party matching window shown, focused and refreshed (0x100A4DE0)
void NConsoleWnd::C3Virtual375()
{
    GetDesktop()->MatchWnd->ShowWindow();
    GetDesktop()->MatchWnd->SetFocus();
    GetDesktop()->MatchWnd->ClearList();
}
NW_HOOK_METHOD(0x1006daa0, NConsoleWnd, C3Virtual375)

// 0x1006dae0 (C3 virtual 376) - the party room window's 0x100A5450
void NConsoleWnd::C3Virtual376(L2ParamStack& Params)
{
    static_cast<NCPartyRoomWnd*>(GetDesktop()->PartyRoomWnd)->SetRoomInfo(Params);
}
NW_HOOK_METHOD(0x1006dae0, NConsoleWnd, C3Virtual376)

// 0x1006db00 (C3 virtual 377) - the party room window's 0x100A6EA0
void NConsoleWnd::C3Virtual377(L2ParamStack& Params)
{
    static_cast<NCPartyRoomWnd*>(GetDesktop()->PartyRoomWnd)->SetMembers(Params);
}
NW_HOOK_METHOD(0x1006db00, NConsoleWnd, C3Virtual377)

// 0x1006db60 (C3 virtual 379) - the party room window's 0x100A75B0
void NConsoleWnd::C3Virtual379(L2ParamStack& Params)
{
    static_cast<NCPartyRoomWnd*>(GetDesktop()->PartyRoomWnd)->UpdateMember(Params);
}
NW_HOOK_METHOD(0x1006db60, NConsoleWnd, C3Virtual379)

// 0x1006e490 (C3 virtual 382) - the broadcast GM window shown and given the value (0x1010DD50)
void NConsoleWnd::C3Virtual382(INT A)
{
    GetDesktop()->BroadcastGMWnd->ShowWindow();
    static_cast<NCBroadcastGMWnd*>(GetDesktop()->BroadcastGMWnd)->NotifyGameInfo((L2ParamStack*)A);
}
NW_HOOK_METHOD(0x1006e490, NConsoleWnd, C3Virtual382)

// 0x1006e4d0 (C3 virtual 383) - the broadcast GM window's 0x1010E110
void NConsoleWnd::C3Virtual383(L2ParamStack& Params)
{
    static_cast<NCBroadcastGMWnd*>(GetDesktop()->BroadcastGMWnd)->NotifyTeamMember(&Params);
}
NW_HOOK_METHOD(0x1006e4d0, NConsoleWnd, C3Virtual383)

// 0x10066120 (C3 virtual 385) - the desktop and the replay window hidden
void NConsoleWnd::C3Virtual385()
{
    Desktop->HideWindow();
    ReplayWnd->HideWindow();
}
NW_HOOK_METHOD(0x10066120, NConsoleWnd, C3Virtual385)

// 0x10066140 (C3 virtual 386) - the replay window shown
void NConsoleWnd::C3Virtual386()
{
    ReplayWnd->ShowWindow();
}
NW_HOOK_METHOD(0x10066140, NConsoleWnd, C3Virtual386)

// 0x10082690 (C3 virtual 389) - the loading picture's id
INT NConsoleWnd::C3Virtual389()
{
    return LoadingScreenID;
}
NW_HOOK_METHOD(0x10082690, NConsoleWnd, C3Virtual389)

/*-----------------------------------------------------------------------------
    Wave 3: the C3-only virtuals and helpers that act on a window or the network handler (names ours unless a guard
    string gave them). The windows the console's own header has no class for are reached by their offsets.
-----------------------------------------------------------------------------*/

// 0x10066dc0 (guard name) - the try-on shop's list sent to the server: NetHandler slot 96 (the stock returns its answer)
INT NConsoleWnd::RequestPreviewItem(L2ParamStack& Params)
{
    guard(NConsoleWnd::RequestPreviewItem);
    return Engine->NetHandler->RequestPreviewItem(Params);
    unguard;
}
NW_HOOK_METHOD(0x10066dc0, NConsoleWnd, RequestPreviewItem)

// 0x1006c410 (guard name) - the answer of a confirmation dialog the server asked for: NetHandler slot 260
void NConsoleWnd::SendConfirmDlg(L2ParamStack& Params)
{
    guard(NConsoleWnd::SendConfirmDlg);
    Engine->NetHandler->ConfirmDlg(Params);
    unguard;
}
NW_HOOK_METHOD(0x1006c410, NConsoleWnd, SendConfirmDlg)

// 0x1006a020 (guard name; C3 virtual 192) - a builder command (a GM's "//" line) sent as a one-text parameter stack through
// the console's BuilderCommand (slot 193, called on the global console as the stock does). The text arrives by value
void NConsoleWnd::SendBypassBuildCmd(FString Command)
{
    guard(NConsoleWnd::SendBypassBuildCmd);
    L2ParamStack Params(10);
    Params.PushBack((void*)*Command);
    ::nw::VirtualCall<void>(GConsole, 0x308 / 4, &Params);   // C4: slot 194 (the header has the builder command at +0x304)
    unguard;
}
typedef void (NConsoleWnd::*SigSendBypassBuildCmd)(FString);
// installed as the member itself (no forwarder): the by-value FString a forwarder would copy again is the stock's own
NW_HOOK_VARARGS(0x1006a020, NConsoleWnd, SendBypassBuildCmd, SigSendBypassBuildCmd)

// The windows below are other lanes' classes (or have no header yet); these views carry the functions of theirs that this
// port reaches: the member's `this` is the window, hooked as a member at the stock address (names ours; db/names.csv)
// NCQuestGaraTreeWndExt (NConsoleWndExt.h)

// 0x100660b0 (vslot 208; ret 0xC) - the loading screen's picture by texture name: the window's SetTextureName (0x1009A700, the system lane's,
// it stays stock) takes a copy of the text; the original is this function's own and dies with it
void NConsoleWnd::SetLoadingTextureName(FString Name)
{
    LoadingWnd->SetTextureName(Name);
}
typedef void (NConsoleWnd::*SigSetLoadingTextureName)(FString);
NW_HOOK_VARARGS(0x100660b0, NConsoleWnd, SetLoadingTextureName, SigSetLoadingTextureName)

// 0x100eb840 (name ours) - the quest page of the character window: the tree's first item of the quest the list has
// selected (+0x13C of the child at +0xEC) is marked, and so is its last child (flags at +0x20C8 / +0x20CC: the quest is
// tracked on the map); the raw offsets are the quest tree classes' (NCQuestGaraTreeWnd, not placed in a header yet)
// C3 +0xCC / +0x114 / +0x1D4 / +0x2088 / +0x208C / +0x1A08 / +0x20A8 / +0x20AC; here +0x20 or +0x40 more
void NCQuestGaraTreeWndExt::MarkSelectedQuest()
{
    BYTE* Ctrl = nw::Field<BYTE*>(this, 0xEC);
    BYTE* Tree = nw::Field<BYTE*>(nw::Field<BYTE*>(Ctrl, 0x134), 0x1F4);
    if (!Tree)
        return;
    for (INT i = 0; i < nw::Field<INT>(Tree, 0x20AC); i++)
    {
        BYTE* Item = nw::Field<BYTE**>(Tree, 0x20A8)[i];
        if (nw::Field<INT>(nw::Field<BYTE*>(this, 0xEC), 0x13C) == nw::Field<INT>(Item, 0x1A28))
        {
            nw::Field<INT>(Item, 0x20C8) = 1;
            nw::Field<INT>(Item, 0x20CC) = 1;
            BYTE* Last = nw::Field<BYTE**>(Item, 0x20A8)[nw::Field<INT>(Item, 0x20AC) - 1];
            nw::Field<INT>(Last, 0x20C8) = 1;
            nw::Field<INT>(Last, 0x20CC) = 1;
        }
    }
}
NW_HOOK_METHOD(0x100eb840, NCQuestGaraTreeWndExt, MarkSelectedQuest)

// 0x10067300 (guard name; C3 virtual 171) - the quest list is complete: the quest page marks the selected quest
void NConsoleWnd::EndQuestList()
{
    guard(NConsoleWnd::EndQuestList);
    ((NCQuestGaraTreeWndExt*)GetDesktop()->MainWnd->QuestWnd)->MarkSelectedQuest();
    unguard;
}
NW_HOOK_METHOD(0x10067300, NConsoleWnd, EndQuestList)

// (NCPartyWnd::ClearMemberBuffs 0x10115980 and AddMemberBuff 0x10115920 are real methods of NCPartyWnd.cpp, hooked there: the stand-in
// struct NCPartyWndExt with its two stock jumps that stood here is gone, ReceiveBuffList calls the methods)

// 0x10068b40 (C3 virtual 114) - the buffs of one of the party's windows follow: the kind (0 a party member by its object id,
// 1 the pet, 2 the summoned creature), the member's id, the count and, C4, two values per buff (the skill id and a second
// one the buff windows take: C3 had the skill id alone). The member's buffs are cleared first. The pet's window is
// called with NCPartyStatusWnd's functions (0x101155E0, 0x101147F0: the same arrays at +0x368 / +0x374)
void NConsoleWnd::ReceiveBuffList(L2ParamStack& Params)
{
    INT Kind = (INT)Params.Top();
    INT MemberID = (INT)Params.Top();
    INT Count = (INT)Params.Top();
    if (Kind == 0)
    {
        ((NCPartyWnd*)GetDesktop()->PartyWnd)->ClearMemberBuffs(MemberID);
        for (INT i = 0; i < Count; i++)
        {
            INT SkillID = (INT)Params.Top();
            INT Param = (INT)Params.Top();
            ((NCPartyWnd*)GetDesktop()->PartyWnd)->AddMemberBuff(MemberID, SkillID, Param);
        }
    }
    else if (Kind == 1)
    {
        ((NCPartyStatusWnd*)GetDesktop()->PetStatusWnd)->ClearBuffs();   // 0x101155E0: the party member window's, shared with the pet's
        for (INT i = 0; i < Count; i++)
        {
            INT SkillID = (INT)Params.Top();
            INT Param = (INT)Params.Top();
            ((NCPartyStatusWnd*)GetDesktop()->PetStatusWnd)->AddBuff(SkillID, Param);   // 0x101147F0: shared with the pet's
        }
    }
    else if (Kind == 2)
    {
        GetDesktop()->SummonedStatusWnd->ClearBuffs();
        for (INT i = 0; i < Count; i++)
        {
            INT SkillID = (INT)Params.Top();
            INT Param = (INT)Params.Top();
            GetDesktop()->SummonedStatusWnd->AddBuff(SkillID, Param);
        }
    }
}
NW_HOOK_METHOD(0x10068b40, NConsoleWnd, ReceiveBuffList)

// 0x100697e0 (C3 virtual 272) - the map cover's minimap (+0xC8 of the window at desktop +0x14C) shows a place, while the
// cover is shown (the last argument, 0: no follow)
void NConsoleWnd::ShowMapPosition(INT X, INT Y, INT Z)
{
    NCWnd* Cover = Desktop ? GetDesktop()->MapCoverWnd : NULL;
    if (Cover && nw::Field<NCMinimapWnd*>(Cover, 0xE8) && Cover->IsShowWindow())
        nw::Field<NCMinimapWnd*>(GetDesktop()->MapCoverWnd, 0xE8)->ShowPosition(X, Y, Z, 0);
}
NW_HOOK_METHOD(0x100697e0, NConsoleWnd, ShowMapPosition)

// 0x10072c70 (C3 virtual 271) - the same, on the place of the player's pawn (the stock truncates each coordinate with
// _ftol); (0, 0, 0) without a controller or a pawn
void NConsoleWnd::ShowMyMapPosition()
{
    FVector Loc(0.f, 0.f, 0.f);
    APlayerController* Controller = Engine->Client->Viewports(0)->Actor;
    if (Controller && Controller->Pawn)
        Loc = Controller->Pawn->Location;
    INT X = (INT)Loc.X;
    INT Y = (INT)Loc.Y;
    NCWnd* Cover = Desktop ? GetDesktop()->MapCoverWnd : NULL;
    if (Cover && nw::Field<NCMinimapWnd*>(Cover, 0xE8) && Cover->IsShowWindow())
        nw::Field<NCMinimapWnd*>(GetDesktop()->MapCoverWnd, 0xE8)->ShowPosition(X, Y, (INT)Loc.Z, 0);
}
NW_HOOK_METHOD(0x10072c70, NConsoleWnd, ShowMyMapPosition)

/*-----------------------------------------------------------------------------
    Wave 3 (2): the match broadcast of the GM's and the observer's windows (the desktop's +0x1AC .. +0x1BC: all of their
    own functions stay stock and are called by address), the buff lists of the C3 buff window, the Seven Signs board, the
    shortcut bar's auto shots, the quest page's target, and the small forwarders of the C3-only slots. Names ours; the
    guard strings are the stock's (copied where they are another function's or have a typo).
-----------------------------------------------------------------------------*/

// (NCBroadcastObserverWnd::SetTeamState, 0x1012DB00, is a real method of NCBroadcastObserverWnd.cpp now, hooked there: the stand-in
// struct and its stock jump that stood here are gone, OnEventMatchTeamInfoStart calls the method)

// 0x1006cae0 (C3 virtual 360; guard name) - a message of the match: kind 0 is the GM's text for the observer's message
// window, kinds 1..8 select the line of the special message window (the kind goes through the setter 0x10116B20 the
// private stores use too); the window is shown. The kind is a byte in a dword
void NConsoleWnd::OnEventMatchMessage(INT Kind, FString Message)
{
    guard(NConsoleWnd::OnEventMatchMessage);
    if (Desktop)
    {
        INT K = (signed char)Kind;
        if (K == 0)
        {
            NCWnd* Wnd = GetDesktop()->BroadcastObserverGMMsgWnd;
            if (Wnd)
            {
                static_cast<NCBroadcastObserverGMMsgWnd*>(Wnd)->SetMsg(Message);   // NCBroadcastObserverGMMsgWnd::SetMsg
                Wnd->ShowWindow();
            }
        }
        else if (K > 0 && K <= 8)
        {
            NCWnd* Wnd = GetDesktop()->BroadcastObserverSpecialMsgWnd;
            if (Wnd)
            {
                ((NCStoreWnd*)Wnd)->SetAdena(K);   // 0x101029D0: the shared setter of the dword at +0x114
                Wnd->ShowWindow();
            }
        }
    }
    unguard;
}
typedef void (NConsoleWnd::*SigOnEventMatchMessage)(INT, FString);
NW_HOOK_VARARGS(0x1006cae0, NConsoleWnd, OnEventMatchMessage, SigOnEventMatchMessage)

// 0x1006cbf0 (C3 virtual 361; guard name) - the score to the observer's window (NCBroadcastObserverWnd::NotifyScore)
void NConsoleWnd::OnEventMatchScore(INT A, INT B, INT C)
{
    guard(NConsoleWnd::OnEventMatchScore);
    if (Desktop && GetDesktop()->BroadcastObserverWnd)
        static_cast<NCBroadcastObserverWnd*>(GetDesktop()->BroadcastObserverWnd)->NotifyScore(A, B, C);
    unguard;
}
NW_HOOK_METHOD(0x1006cbf0, NConsoleWnd, OnEventMatchScore)

// 0x1006cc90 (C3 virtual 362; the guard string is "NConsoelWnd::..." in the stock) - the teams' list begins: to the GM's
// window while it is shown (NotifyTeamInfoStart 0x1010D980) and to the observer's window when the console is in the
// observer's UI mode (UIMode 1)
void NConsoleWnd::OnEventMatchTeamInfoStart(INT A, INT B, INT C)
{
    guard(NConsoelWnd::OnEventMatchTeamInfoStart);
    if (Desktop)
    {
        NCWnd* GM = GetDesktop()->BroadcastGMWnd;
        if (GM && GM->IsShowWindow())
            static_cast<NCBroadcastGMWnd*>(GM)->NotifyTeamInfoStart(A, B, C);
        NCWnd* Observer = GetDesktop()->BroadcastObserverWnd;
        if (Observer && UIMode == 1)
            ((NCBroadcastObserverWnd*)Observer)->SetTeamState(A, B, C);
    }
    unguard;
}
NW_HOOK_METHOD(0x1006cc90, NConsoleWnd, OnEventMatchTeamInfoStart)

// 0x1006cd70 (C3 virtual 363; guard name) - a team of the list (its two numbers, its name and ten more numbers), to the
// GM's window (0x1010DA90) and the observer's (0x10117DC0), each while it is shown
void NConsoleWnd::OnEventMatchTeamInfo(INT A, INT B, FString Name, INT P1, INT P2, INT P3, INT P4, INT P5, INT P6,
                                       INT P7, INT P8, INT P9, INT P10)
{
    guard(NConsoleWnd::OnEventMatchTeamInfo);
    if (Desktop)
    {
        NCWnd* GM = GetDesktop()->BroadcastGMWnd;
        if (GM && GM->IsShowWindow())
            static_cast<NCBroadcastGMWnd*>(GM)->NotifyTeamInfo(A, B, Name, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10);
        NCWnd* Observer = GetDesktop()->BroadcastObserverWnd;
        if (Observer && Observer->IsShowWindow())
            static_cast<NCBroadcastObserverWnd*>(Observer)->NotifyTeamInfo(A, B, Name, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10);
    }
    unguard;
}
typedef void (NConsoleWnd::*SigOnEventMatchTeamInfo)(INT, INT, FString, INT, INT, INT, INT, INT, INT, INT, INT, INT, INT);
NW_HOOK_VARARGS(0x1006cd70, NConsoleWnd, OnEventMatchTeamInfo, SigOnEventMatchTeamInfo)

// 0x1006cee0 (C3 virtual 364; "NConsoelWnd" in the stock's guard) - the teams' list ends (0x1010DB90, 0x10117EC0)
void NConsoleWnd::OnEventMatchTeamInfoEnd()
{
    guard(NConsoelWnd::OnEventMatchTeamInfoEnd);
    if (Desktop)
    {
        NCWnd* GM = GetDesktop()->BroadcastGMWnd;
        if (GM && GM->IsShowWindow())
            static_cast<NCBroadcastGMWnd*>(GM)->NotifyTeamInfoEnd();
        NCWnd* Observer = GetDesktop()->BroadcastObserverWnd;
        if (Observer && Observer->IsShowWindow())
            static_cast<NCBroadcastObserverWnd*>(Observer)->NotifyTeamInfoEnd();
    }
    unguard;
}
NW_HOOK_METHOD(0x1006cee0, NConsoleWnd, OnEventMatchTeamInfoEnd)

// 0x1006cfb0 (C3 virtual 365; guard name) - a game was created: its id as text to the GM's window (0x1010CF00)
void NConsoleWnd::OnEventMatchCreate(INT GameID)
{
    guard(NConsoleWnd::OnEventMatchCreate);
    if (Desktop && GetDesktop()->BroadcastGMWnd)
        static_cast<NCBroadcastGMWnd*>(GetDesktop()->BroadcastGMWnd)->NotifyCreateBroadcastGame(FString::Printf(TEXT("%d"), GameID));
    unguard;
}
NW_HOOK_METHOD(0x1006cfb0, NConsoleWnd, OnEventMatchCreate)

// 0x1006d060 (C3 virtual 366; guard "OnEventMatchUserInfo") - a player of the match (a number, a name, eight numbers) to
// the observer's window (NotifyUserInfo 0x10118010)
void NConsoleWnd::OnEventMatchUserInfo(INT A, FString Name, INT P1, INT P2, INT P3, INT P4, INT P5, INT P6, INT P7, INT P8)
{
    guard(NConsoleWnd::OnEventMatchUserInfo);
    if (Desktop && GetDesktop()->BroadcastObserverWnd)
        static_cast<NCBroadcastObserverWnd*>(GetDesktop()->BroadcastObserverWnd)->NotifyUserInfo(A, Name, P1, P2, P3, P4, P5, P6, P7, P8);
    unguard;
}
typedef void (NConsoleWnd::*SigOnEventMatchUserInfo)(INT, FString, INT, INT, INT, INT, INT, INT, INT, INT);
NW_HOOK_VARARGS(0x1006d060, NConsoleWnd, OnEventMatchUserInfo, SigOnEventMatchUserInfo)

// 0x1006d140 (C3 virtual 367; the stock's guard string is the previous function's) - the spells' list of a player begins
// (NotifySpellInfoStart 0x101192E0)
void NConsoleWnd::OnEventMatchSpellInfoStart(INT A, INT B, INT C)
{
    guard(NConsoleWnd::OnEventMatchUserInfo);
    if (Desktop && GetDesktop()->BroadcastObserverWnd)
        static_cast<NCBroadcastObserverWnd*>(GetDesktop()->BroadcastObserverWnd)->NotifySpellInfoStart(A, B, C);
    unguard;
}
NW_HOOK_METHOD(0x1006d140, NConsoleWnd, OnEventMatchSpellInfoStart)

// 0x1006d1e0 (C3 virtual 368; the same guard string) - a spell of the list (NotifySpellInfo 0x10119400)
void NConsoleWnd::OnEventMatchSpellInfo(INT A, INT B, INT C, INT D)
{
    guard(NConsoleWnd::OnEventMatchUserInfo);
    if (Desktop && GetDesktop()->BroadcastObserverWnd)
        static_cast<NCBroadcastObserverWnd*>(GetDesktop()->BroadcastObserverWnd)->NotifySpellInfo(A, B, C, D);
    unguard;
}
NW_HOOK_METHOD(0x1006d1e0, NConsoleWnd, OnEventMatchSpellInfo)

// 0x1006d280 (C3 virtual 369; the same guard string) - the spells' list ends (NotifySpellInfoEnd 0x10118140)
void NConsoleWnd::OnEventMatchSpellInfoEnd()
{
    guard(NConsoleWnd::OnEventMatchUserInfo);
    if (Desktop && GetDesktop()->BroadcastObserverWnd)
        static_cast<NCBroadcastObserverWnd*>(GetDesktop()->BroadcastObserverWnd)->NotifySpellInfoEnd();
    unguard;
}
NW_HOOK_METHOD(0x1006d280, NConsoleWnd, OnEventMatchSpellInfoEnd)

// 0x1006d310 (C3 virtual 370; guard name) - a team unlocked (NotifyTeamUnlocked 0x101181E0)
void NConsoleWnd::OnEventMatchTeamUnlocked(INT A, INT B)
{
    guard(NConsoleWnd::OnEventMatchTeamUnlocked);
    if (Desktop && GetDesktop()->BroadcastObserverWnd)
        static_cast<NCBroadcastObserverWnd*>(GetDesktop()->BroadcastObserverWnd)->NotifyTeamUnlocked(A, B);
    unguard;
}
NW_HOOK_METHOD(0x1006d310, NConsoleWnd, OnEventMatchTeamUnlocked)

// 0x1006d3a0 (C3 virtual 371; guard name) - the GM's broadcast window shown
void NConsoleWnd::OnEventMatchGMTest()
{
    guard(NConsoleWnd::OnEventMatchGMTest);
    if (Desktop && GetDesktop()->BroadcastGMWnd)
        GetDesktop()->BroadcastGMWnd->ShowWindow();
    unguard;
}
NW_HOOK_METHOD(0x1006d3a0, NConsoleWnd, OnEventMatchGMTest)

// 0x1006d430 (guard name; vslot 394) - a skill cast is shown to the observer window (the desktop's +0x1E0) and to the Olympiad player
// windows (+0x214, +0x218) whose player is the caster
void NConsoleWnd::OnReceiveMagicSkillUse(User* Caster, User* Target, INT Param)
{
    guard(NConsoleWnd::OnReceiveMagicSkillUse);
    if (Desktop)
    {
        NCWnd* Observer = nw::Field<NCWnd*>(Desktop, 0x1E0);
        if (Observer)
            static_cast<NCBroadcastObserverWnd*>(Observer)->NotifyMagicSkillUse(nw::Field<INT>(Caster, 0x18), nw::Field<INT>(Target, 0x18), Param);   // NCBroadcastObserverWnd::NotifyMagicSkillUse
        for (INT i = 0; i < 2; i++)
        {
            NCWnd* Player = nw::Field<NCWnd*>(Desktop, 0x214 + i * 4);
            if (Player && static_cast<NCOlympiadPlayerWnd*>(Player)->GetPlayerID() == nw::Field<INT>(Caster, 0x18))
                static_cast<NCOlympiadPlayerWnd*>(nw::Field<NCWnd*>(Desktop, 0x214 + i * 4))->NotifyMagicSkillUse(Param);   // NCOlympiadPlayerWnd::NotifyMagicSkillUse
        }
    }
    unguard;
}
NW_HOOK_METHOD(0x1006d430, NConsoleWnd, OnReceiveMagicSkillUse)

// 0x1006d5e0 (C3 virtual 373; guard name) - a character's info, only while the console is in the observer's UI mode
// (UIMode 1), to the observer's window (NotifyCharInfo 0x10118AE0)
void NConsoleWnd::OnCharInfo(INT Info)
{
    guard(NConsoleWnd::OnCharInfo);
    if (UIMode == 1 && Desktop && GetDesktop()->BroadcastObserverWnd)
        static_cast<NCBroadcastObserverWnd*>(GetDesktop()->BroadcastObserverWnd)->NotifyCharInfo((User*)Info);
    unguard;
}
NW_HOOK_METHOD(0x1006d5e0, NConsoleWnd, OnCharInfo)

// 0x1006d680 (C3 virtual 384; guard name) - a command line as if it was typed in the chat (NCChatWnd::ProcessChatLine)
void NConsoleWnd::ExecuteCommand(TCHAR* Line)
{
    guard(NConsoleWnd::ExecuteCommand);
    if (Desktop && GetDesktop()->ChatWnd)
        GetDesktop()->ChatWnd->ProcessChatLine(Line);
    unguard;
}
NW_HOOK_METHOD(0x1006d680, NConsoleWnd, ExecuteCommand)

/*-----------------------------------------------------------------------------
    The C3 buff window's second list and its extra icon (the desktop's +0x108: the console's slots 166 .. 169).
-----------------------------------------------------------------------------*/

// NCAbnormalStatusWndExt (NConsoleWndExt.h): NCAbnormalStatusWnd's own functions this port reaches, the stock's small ones that no header names

// (the second list's AddStatus2, the extra icon's SetExtraStatus, ClearStatus2 and ClearExtraStatus are NCAbnormalStatusWnd's own methods, hooked in
// NCAbnormalStatusWnd.cpp; this file used to carry unhooked copies of them, which stage 5 removed: the console calls the window's methods)


// 0x10116a80 (name ours) - the buff window forgets everything (NCConsole::Reset): the first list, the extra icon, the
// second list (the last one as a tail jump)
void NCAbnormalStatusWndExt::ClearAll()
{
    NCAbnormalStatusWnd* This = (NCAbnormalStatusWnd*)this;
    This->ClearStatus();
    This->ClearExtraStatus();
    This->ClearStatus2();
}
NW_HOOK_METHOD(0x10116a80, NCAbnormalStatusWndExt, ClearAll)

// 0x10071580 (C3 virtual 169) - a buff of the second list: the skill's id and level (the time is not sent)
INT NConsoleWnd::AddAbnormalStatus2(L2ParamStack& Params)
{
    FAbnormalStatus Status;
    Status.SkillID = (INT)Params.Top();
    Status.Level = (INT)Params.Top();
    Status.RemainTime = 0;
    if (GetSkillIconName(Status.SkillID, Status.Level))
        appStrcpy(Status.IconName, GetSkillIconName(Status.SkillID, Status.Level));
    if (GetSkillName(Status.SkillID, Status.Level))
        appStrcpy(Status.Name, GetSkillName(Status.SkillID, Status.Level));
    if (GetSkillDesc(Status.SkillID, Status.Level))
        appStrcpy(Status.Desc, GetSkillDesc(Status.SkillID, Status.Level));
    return GetDesktop()->AbnormalStatusWnd->AddStatus2(&Status);
}
NW_HOOK_METHOD(0x10071580, NConsoleWnd, AddAbnormalStatus2)

// 0x100716d0 (C3 virtual 167) - the extra icon: the skill's id, its level and the seconds left
INT NConsoleWnd::SetExtraAbnormalStatus(L2ParamStack& Params)
{
    FAbnormalStatus Status;
    Status.SkillID = (INT)Params.Top();
    Status.Level = (INT)Params.Top();
    Status.RemainTime = (INT)Params.Top();
    if (GetSkillIconName(Status.SkillID, Status.Level))
        appStrcpy(Status.IconName, GetSkillIconName(Status.SkillID, Status.Level));
    if (GetSkillName(Status.SkillID, Status.Level))
        appStrcpy(Status.Name, GetSkillName(Status.SkillID, Status.Level));
    if (GetSkillDesc(Status.SkillID, Status.Level))
        appStrcpy(Status.Desc, GetSkillDesc(Status.SkillID, Status.Level));
    return GetDesktop()->AbnormalStatusWnd->SetExtraStatus(&Status);
}
NW_HOOK_METHOD(0x100716d0, NConsoleWnd, SetExtraAbnormalStatus)

/*-----------------------------------------------------------------------------
    The quest page's target (the quest the player follows: its name at +0xE4 and a place at +0xF0 of the quest page), the
    HP bar's animation, the shortcut bar's auto shots and a few dialogs.
-----------------------------------------------------------------------------*/

// 0x100ea140 / 0x100ea180 / 0x100ea1b0 (names ours) - NCQuestGaraTreeWnd's target: the name (a copy of the FString at
// +0x104) and the three words at +0x110 (a place), read and written
FString NCQuestGaraTreeWndExt::GetTargetName()
{
    return nw::Field<FString>(this, 0x104);
}
NW_HOOK_METHOD(0x100ea140, NCQuestGaraTreeWndExt, GetTargetName)

FVector NCQuestGaraTreeWndExt::GetTargetPos()
{
    return nw::Field<FVector>(this, 0x110);
}
NW_HOOK_METHOD(0x100ea180, NCQuestGaraTreeWndExt, GetTargetPos)

void NCQuestGaraTreeWndExt::SetTargetPos(INT X, INT Y, INT Z)
{
    nw::Field<INT>(this, 0x110) = X;
    nw::Field<INT>(this, 0x114) = Y;
    nw::Field<INT>(this, 0x118) = Z;
}
NW_HOOK_METHOD(0x100ea1b0, NCQuestGaraTreeWndExt, SetTargetPos)

// 0x1006c590 (C3 virtual 345) - the followed quest's target: its name and its place into the caller's records
void NConsoleWnd::GetQuestTarget(FString* Name, FVector* Pos)
{
    NCQuestGaraTreeWndExt* Quest = (NCQuestGaraTreeWndExt*)GetDesktop()->MainWnd->QuestWnd;
    *Name = Quest->GetTargetName();
    *Pos = Quest->GetTargetPos();
}
NW_HOOK_METHOD(0x1006c590, NConsoleWnd, GetQuestTarget)

// 0x10082760 (C3 virtual 355) - the followed quest's target name (empty without the quest page)
FString NConsoleWnd::GetQuestTargetName()
{
    FString Name;
    if (Desktop && GetDesktop()->MainWnd && GetDesktop()->MainWnd->QuestWnd)
        Name = ((NCQuestGaraTreeWndExt*)GetDesktop()->MainWnd->QuestWnd)->GetTargetName();
    return Name;
}
NW_HOOK_METHOD(0x10082760, NConsoleWnd, GetQuestTargetName)

// 0x10082830 (C3 virtual 356) - the followed quest's target place ((0, 0, 0) without the quest page)
FVector NConsoleWnd::GetQuestTargetPos()
{
    if (Desktop && GetDesktop()->MainWnd && GetDesktop()->MainWnd->QuestWnd)
        return ((NCQuestGaraTreeWndExt*)GetDesktop()->MainWnd->QuestWnd)->GetTargetPos();
    FVector Zero;
    nw::Field<INT>(&Zero, 0) = 0;
    nw::Field<INT>(&Zero, 4) = 0;
    nw::Field<INT>(&Zero, 8) = 0;
    return Zero;
}
NW_HOOK_METHOD(0x10082830, NConsoleWnd, GetQuestTargetPos)

// UE2's appSeconds (UnVcWin32.h): the time stamp counter in seconds, plus 2^24 (the stock expands it inline)
static FORCEINLINE DOUBLE NwAppSeconds()
{
    unsigned __int64 Cycles = __rdtsc();
    DWORD L = (DWORD)Cycles;
    DWORD H = (DWORD)(Cycles >> 32);
    return ((DOUBLE)L + 4294967296.0 * (DOUBLE)H) * GSecondsPerCycle + 16777216.0;
}

// 0x1006cab0 (C3 virtual 358) - the HP bar of the player's status window animates (the first value is not read)
void NConsoleWnd::StartHPBarAnim(INT Unused, INT Value, INT Max, DOUBLE Time)
{
    static_cast<NCStatusBarCtrl*>(GetDesktop()->PlayerStatusWnd->HPBar)->StartAnim(Value, Max, Time);
}
NW_HOOK_METHOD(0x1006cab0, NConsoleWnd, StartHPBarAnim)

// 0x1006e1e0 (C3 virtual 380) - the auto use of a shot (its id, on / off) is set on every shortcut slot that holds it (the stock walks the 120
// slots of the bar: 12 pages of 10); C4: with no id and off, every slot that is on is turned off
void NConsoleWnd::SetShotAutoUse(L2ParamStack& Params)
{
    INT ItemID = (INT)Params.Top();
    INT bOn = (INT)Params.Top();
    for (INT i = 0; i < 120; i++)
    {
        BYTE* Slot = nw::Field<BYTE*>(GetDesktop()->ShortCutWnd, 0x200) + i * 0x1CC0;
        if (nw::Field<INT>(Slot, 0x1B20) == ItemID || (ItemID == 0 && bOn == 0 && nw::Field<INT>(Slot, 0x1CA4)))
            nw::Field<INT>(Slot, 0x1CA4) = (bOn != 0);
    }
}
NW_HOOK_METHOD(0x1006e1e0, NConsoleWnd, SetShotAutoUse)

// 0x1006e840 (name ours) - TArray<INT>::FindItem (the stock's instance for the shortcut code): the index of the first
// element equal to Item in Index (the count when none), 1 when found
struct NIntArrayExt
{
    __declspec(noinline) UBOOL FindItem(const INT* Item, INT* Index);   // 0x100626A0
};
UBOOL NIntArrayExt::FindItem(const INT* Item, INT* Index)
{
    TArray<INT>* This = (TArray<INT>*)this;
    *Index = 0;
    for (; *Index < This->Num(); (*Index)++)
    {
        if ((*This)((*Index)) == *Item)
            return 1;
    }
    return 0;
}
NW_HOOK_METHOD(0x1006e840, NIntArrayExt, FindItem)

// 0x1007f030 (C3 virtual 381) - the shots of the auto list are used for the weapon the player wears (the first record of the paperdoll (the
// inventory's +0x114, its records at +0xF0) with the type +0x1B14 0; its crystal grade at +0x1B64: 0 none .. 5 S). With bFlag 0 every listed
// shot whose grade is the weapon's is used (D: 0x5B7, C: 0x5B8, B: 0x5B9, A: 0x5BA, S: 0x5BB; no grade: 0x72B and 0x169D); with bFlag set the
// first of the grade's list that the auto list holds is used (no grade: 0x9CD, 0xF6B, 0x169E; D: 0x9CE, 0xF6C; C: 0x9CF, 0xF6D; B: 0x9D0, 0xF6E;
// A: 0x9D1, 0xF6F; S: 0x9D2, 0xF70). The shot is used by its object id. (As the stock, the last record looked at is the weapon when no
// record has the type)
void NConsoleWnd::UseAutoShots(INT bFlag)
{
    if (!Desktop)
        return;
    NCWnd* Equip = nw::Field<NCWnd*>(nw::Field<NCWnd*>(Desktop, 0x120), 0x114);
    INT n = nw::Field<INT>(Equip, 0xF4);
    if (n <= 0)
        return;
    INT w = 0;
    BYTE* Weapon = NULL;
    for (; w < n; w++)
    {
        Weapon = nw::Field<BYTE**>(Equip, 0xF0)[w];
        if (Weapon && nw::Field<INT>(Weapon, 0x1B14) == 0)
            break;
    }
    if (!Weapon)
        return;
    TArray<INT> Ids;
    TArray<INT> Objects;
    for (INT s = 0; s < 120; s++)
    {
        BYTE* Slot = nw::Field<BYTE*>(nw::Field<NCWnd*>(Desktop, 0x15C), 0x200) + s * 0x1CC0;
        if (Slot && nw::Field<INT>(Slot, 0x1CA4))
        {
            Ids.AddItem(nw::Field<INT>(Slot, 0x1B20));
            Objects.AddItem(nw::Field<INT>(Slot, 0x1B1C));
        }
    }
    INT Grade = nw::Field<INT>(Weapon, 0x1B64);
    if (bFlag == 0)
    {
        for (INT i = 0; i < Ids.Num(); i++)
        {
            INT Id = Ids(i);
            UBOOL bUse = 0;
            if (Id > 0x72B)
                bUse = (Id == 0x169D) && Grade == 0;
            else if (Id == 0x72B)
                bUse = Grade == 0;
            else if ((UINT)(Id - 0x5B7) <= 4)
                bUse = Grade == Id - 0x5B7 + 1;
            if (bUse)
                RequestUseItem(Objects(i));
        }
    }
    else
    {
        static const INT Lists[6][3] =
        {
            { 0x9CD, 0xF6B, 0x169E },
            { 0x9CE, 0xF6C, 0 },
            { 0x9CF, 0xF6D, 0 },
            { 0x9D0, 0xF6E, 0 },
            { 0x9D1, 0xF6F, 0 },
            { 0x9D2, 0xF70, 0 }
        };
        INT Index = -1;
        if ((UINT)Grade <= 5)
        {
            for (INT k = 0; k < 3; k++)
            {
                INT Id = Lists[Grade][k];
                if (!Id)
                    break;
                if (((NIntArrayExt*)&Ids)->FindItem(&Id, &Index))
                {
                    if (Index >= 0)
                        RequestUseItem(Objects(Index));
                    break;
                }
            }
        }
    }
}
NW_HOOK_METHOD(0x1007f030, NConsoleWnd, UseAutoShots)

// 0x1006db20 (C3 virtual 378) - the party room window closes (the child at +0xCC is told through its slot 71); the
// parameter stack is not read
void NConsoleWnd::ClosePartyRoom(L2ParamStack& Params)
{
    GetDesktop()->PartyRoomWnd->HideWindow();
    nw::VirtualCall<void>(nw::Field<NCWnd*>(GetDesktop()->PartyRoomWnd, 0xEC), 0x134 / 4);
}
NW_HOOK_METHOD(0x1006db20, NConsoleWnd, ClosePartyRoom)

// 0x1006ed50 (C3 virtual 306) - a message of the messenger library for its window (the grandchild of the desktop's
// messenger window, +0xC8 twice): 0x506 and 0x507 go to its slots 0 and 1 with the two words
void NConsoleWnd::DispatchMessengerMsg(INT hWnd, UINT Msg, INT wParam, INT lParam)
{
    if (!Desktop || !GetDesktop()->MessengerWnd)
        return;
    NCWnd* Child = nw::Field<NCWnd*>(GetDesktop()->MessengerWnd, 0xE8);
    if (!Child)
        return;
    NCWnd* Target = nw::Field<NCWnd*>(Child, 0xE8);
    if (!Target)
        return;
    if (Msg == 0x506)
        nw::VirtualCall<void>(Target, 0, wParam, lParam);
    else if (Msg == 0x507)
        nw::VirtualCall<void>(Target, 1, wParam, lParam);
}
NW_HOOK_METHOD(0x1006ed50, NConsoleWnd, DispatchMessengerMsg)

// 0x1006c390 (guard name ConfirmDlg; vslot 367) - the C4 console's EndSystemMessageParam (slot 191) with the packet's parameter stack
// and (0, 0, 1)
void NConsoleWnd::ShowMessageDialog46(L2ParamStack& Params)
{
    guard(NConsoleWnd::ConfirmDlg);
    nw::VirtualCall<void>(this, 191, &Params, 0, 0, 1);
    unguard;
}
NW_HOOK_METHOD(0x1006c390, NConsoleWnd, ShowMessageDialog46)

// the windows that an end dialog hides (the buffs, the chat, the shortcuts, the player's status, the menu bar and the radar; the
// match's end also the observer's window and the zone title; the desktop itself is shown first)
static FORCEINLINE void HideGameWindows(NConsoleWnd* Console, UBOOL bWithObserver)
{
    NCConsole* Desk = Console->GetDesktop();
    Desk->AbnormalStatusWnd->HideWindow();
    Desk->ChatWnd->HideWindow();
    Desk->ShortCutWnd->HideWindow();
    Desk->PlayerStatusWnd->HideWindow();
    Desk->MenuWnd->HideWindow();
    Desk->RadarWnd->HideWindow();
    if (bWithObserver)
    {
        Desk->NewWnd198->HideWindow();
        Desk->ZoneTitleWnd->HideWindow();
    }
}

// 0x10075700 (C3 virtual 387) - the end of a match: the desktop is shown, the game's windows hidden, and a dialog of the
// kind 0x1C (the system menu window answers: its entry is the owner) asks with the system message 1337
void NConsoleWnd::ShowMatchEndDialog()
{
    GetDesktop()->ShowWindow();
    HideGameWindows(this, 1);
    BYTE InfoMem[sizeof(NDialogInfo)];
    NDialogInfo* Info = ::new ((void*)InfoMem) NDialogInfo;
    Info->bInput = 0;
    Info->ID = 0x1C;
    Info->bMainBox = 1;
    Info->Owner = GetDesktop()->SystemMenuWnd->Entry;
    Info->SetSystemMessage(1337);
    GConsole->ShowDialog(Info);
    Info->~NDialogInfo();
}
NW_HOOK_METHOD(0x10075700, NConsoleWnd, ShowMatchEndDialog)

// 0x10075850 (C3 virtual 388) - a text in a dialog of the kind 0x50 (owned by the system menu window); with bHide set the
// desktop is shown and the game's windows are hidden first (all but the observer's window)
void NConsoleWnd::ShowMatchTextDialog(const TCHAR* Text, INT bHide)
{
    if (bHide)
    {
        GetDesktop()->ShowWindow();
        HideGameWindows(this, 0);
    }
    BYTE InfoMem[sizeof(NDialogInfo)];
    NDialogInfo* Info = ::new ((void*)InfoMem) NDialogInfo;
    Info->ID = 0x50;
    Info->bMainBox = 1;
    Info->bInput = 0;
    Info->Owner = GetDesktop()->SystemMenuWnd->Entry;
    Info->SetText(Text);
    GConsole->ShowDialog(Info);
    Info->~NDialogInfo();
}
NW_HOOK_METHOD(0x10075850, NConsoleWnd, ShowMatchTextDialog)

/*-----------------------------------------------------------------------------
    The Seven Signs board (NCSSQMainBoard, the desktop's +0x1F4): its status page (+0xD0), event page (+0xD4) and list
    page (+0xD8) are fed by the console's slots 286 .. 290. The pages' tree builders (0x100EB1D0, 0x100ECCA0 and the root
    makers 0x100EB150, 0x100ECC20) stay the stock's, called by address. Names ours.
-----------------------------------------------------------------------------*/

struct NCSSQStatusPageExt
{
    __declspec(noinline) void SetStatus(const INT* Record);          // 0x100E8E60 the sixteen words of a status packet
    __declspec(noinline) void ClearLists();                          // 0x100E98E0 the three lists at +0x104, +0x110, +0x11C of +0x108
    __declspec(noinline) void ReadLists(L2ParamStack& Params);       // 0x100E9800 the lists from the packet
};
struct NCSSQEventPageExt
{
    __declspec(noinline) INT AddEvent(const void* Record);           // 0x100EB570 the tree builder of the child at +0xCC
    __declspec(noinline) void Reset();                               // 0x100EB580 the page's slot 72, then a new root
};
struct NCSSQListPageExt
{
    __declspec(noinline) INT AddEntry(const void* Record);           // 0x100ECFC0 the tree builder of the child at +0xC8
    __declspec(noinline) void Reset();                               // 0x100ECFD0 the child's slot 72 (0x100EBB50), then a new root
};

// 0x100fb5f0 (name ours) - the page keeps the packet's words (the record's order is the packet's: a few of them sit
// out of line)
void NCSSQStatusPageExt::SetStatus(const INT* R)
{
    nw::Field<INT>(this, 0xE8) = R[0x00 / 4];
    nw::Field<INT>(this, 0x118) = R[0x30 / 4];
    nw::Field<INT>(this, 0xEC) = R[0x04 / 4];
    nw::Field<INT>(this, 0xF0) = R[0x08 / 4];
    nw::Field<INT>(this, 0xF4) = R[0x0C / 4];
    nw::Field<INT>(this, 0x124) = R[0x3C / 4];
    nw::Field<INT>(this, 0xF8) = R[0x10 / 4];
    nw::Field<INT>(this, 0xFC) = R[0x14 / 4];
    nw::Field<INT>(this, 0x100) = R[0x18 / 4];
    nw::Field<INT>(this, 0x104) = R[0x1C / 4];
    nw::Field<INT>(this, 0x108) = R[0x20 / 4];
    nw::Field<INT>(this, 0x10C) = R[0x24 / 4];
    nw::Field<INT>(this, 0x110) = R[0x28 / 4];
    nw::Field<INT>(this, 0x114) = R[0x2C / 4];
    nw::Field<INT>(this, 0x11C) = R[0x34 / 4];
    nw::Field<INT>(this, 0x120) = R[0x38 / 4];
}
NW_HOOK_METHOD(0x100fb5f0, NCSSQStatusPageExt, SetStatus)

// 0x100fc170 (name ours) - the page forgets its numbers and the three lists of the object at +0x108
void NCSSQStatusPageExt::ClearLists()
{
    nw::Field<INT>(this, 0x12C) = 0;
    nw::Field<INT>(this, 0x130) = 0;
    BYTE* Lists = nw::Field<BYTE*>(this, 0x128);
    nw::Field<TArray<INT> >(Lists, 0x124).Empty();
    nw::Field<TArray<INT> >(Lists, 0x130).Empty();
    nw::Field<TArray<INT> >(Lists, 0x13C).Empty();
}
NW_HOOK_METHOD(0x100fc170, NCSSQStatusPageExt, ClearLists)

// 0x100fc090 (name ours) - a packet of the lists: a value that is dropped, the one the page keeps at +0x10C, the count
// (+0x110) and three values each, each into its list
void NCSSQStatusPageExt::ReadLists(L2ParamStack& Params)
{
    Params.Top();
    nw::Field<INT>(this, 0x12C) = (INT)Params.Top();
    INT Count = (INT)Params.Top();
    nw::Field<INT>(this, 0x130) = Count;
    for (INT i = 0; i < Count; i++)
    {
        BYTE* Lists = nw::Field<BYTE*>(this, 0x128);
        INT A = (INT)Params.Top();
        nw::Field<TArray<INT> >(Lists, 0x124).AddItem(A);
        Lists = nw::Field<BYTE*>(this, 0x128);
        INT B = (INT)Params.Top();
        nw::Field<TArray<INT> >(Lists, 0x130).AddItem(B);
        Lists = nw::Field<BYTE*>(this, 0x128);
        INT C = (INT)Params.Top();
        nw::Field<TArray<INT> >(Lists, 0x13C).AddItem(C);
    }
}
NW_HOOK_METHOD(0x100fc090, NCSSQStatusPageExt, ReadLists)

// 0x100fdf10 / 0x100eb580 / 0x100ecfc0 / 0x100ecfd0 (names ours) - the event and the list pages hand their work to a child
INT NCSSQEventPageExt::AddEvent(const void* Record)
{
    return (INT)nw::Field<NCSSQMainEventGaraTreeCtrl*>(this, 0xEC)->AddEvent((const NSSQEventRecord*)Record);
}
NW_HOOK_METHOD(0x100fdf10, NCSSQEventPageExt, AddEvent)

void NCSSQEventPageExt::Reset()
{
    ((NCSSQMainEventWnd*)this)->ClearList();   // 0x100FCC30: the tree control's slot 78
    ((NCSSQEventTreeWndExt*)nw::Field<void*>(this, 0xEC))->MakeRoot();
}
NW_HOOK_METHOD(0x100fdf20, NCSSQEventPageExt, Reset)

INT NCSSQListPageExt::AddEntry(const void* Record)
{
    return (INT)nw::Field<NCSSQSealStatusGaraTreeCtrl*>(this, 0xE8)->AddEntry((const NSSQSealRecord*)Record);
}
NW_HOOK_METHOD(0x100ff860, NCSSQListPageExt, AddEntry)

void NCSSQListPageExt::Reset()
{
    ((NCSSQSealStatusListWnd*)this)->ClearList();
    ((NCSSQListTreeWndExt*)nw::Field<void*>(this, 0xE8))->MakeRoot();
}
NW_HOOK_METHOD(0x100ff870, NCSSQListPageExt, Reset)

// 0x1006c810 (C3 virtual 286) - the board's status: sixteen words (the record is filled in the packet's order, the page
// keeps them by their places), the board is shown with the focus
void NConsoleWnd::ReceiveSSQStatus(L2ParamStack& Params)
{
    INT R[16];
    R[0x00 / 4] = (INT)Params.Top();
    R[0x30 / 4] = (INT)Params.Top();
    R[0x34 / 4] = (INT)Params.Top();
    R[0x38 / 4] = (INT)Params.Top();
    R[0x04 / 4] = (INT)Params.Top();
    R[0x08 / 4] = (INT)Params.Top();
    R[0x0C / 4] = (INT)Params.Top();
    R[0x3C / 4] = (INT)Params.Top();
    R[0x10 / 4] = (INT)Params.Top();
    R[0x18 / 4] = (INT)Params.Top();
    R[0x14 / 4] = (INT)Params.Top();
    R[0x1C / 4] = (INT)Params.Top();
    R[0x20 / 4] = (INT)Params.Top();
    R[0x24 / 4] = (INT)Params.Top();
    R[0x28 / 4] = (INT)Params.Top();
    R[0x2C / 4] = (INT)Params.Top();
    GetDesktop()->SSQMainBoard->HideWindow();
    nw::Field<NCSSQStatusWnd*>(GetDesktop()->SSQMainBoard, 0xF0)->ResetStatus();
    ((NCSSQStatusPageExt*)nw::Field<void*>(GetDesktop()->SSQMainBoard, 0xF0))->SetStatus(R);
    GetDesktop()->SSQMainBoard->ShowWindow();
    GetDesktop()->SSQMainBoard->SetFocus();
}
NW_HOOK_METHOD(0x1006c810, NConsoleWnd, ReceiveSSQStatus)

// 0x1006c7d0 (C3 virtual 287) - the status page's lists: emptied, then read from the packet
void NConsoleWnd::ReceiveSSQLists(L2ParamStack& Params)
{
    NCSSQStatusPageExt* Page = (NCSSQStatusPageExt*)nw::Field<void*>(GetDesktop()->SSQMainBoard, 0xF0);
    Page->ClearLists();
    ((NCSSQStatusPageExt*)nw::Field<void*>(GetDesktop()->SSQMainBoard, 0xF0))->ReadLists(Params);
}
NW_HOOK_METHOD(0x1006c7d0, NConsoleWnd, ReceiveSSQLists)

// 0x1006c920 (C3 virtual 288) - the event page starts over: its slot 72 is told (0x100EA340), then it asks for a new
// root (Reset also tells the slot again, as the stock does)
void NConsoleWnd::ResetSSQEvents()
{
    nw::Field<NCSSQMainEventWnd*>(GetDesktop()->SSQMainBoard, 0xF4)->ClearList();
    ((NCSSQEventPageExt*)nw::Field<void*>(GetDesktop()->SSQMainBoard, 0xF4))->Reset();
}
NW_HOOK_METHOD(0x1006c920, NConsoleWnd, ResetSSQEvents)

// a period of the event page (name ours; 0xF0 bytes: six words and two arrays of nine texts)
struct NSSQEventPacket
{
    INT Id;                 // +0x00 the packet's first value (restored after each entry)
    INT Period;             // +0x04 the packet's third (restored after each entry)
    INT Value8;             // +0x08
    INT ValueC;             // +0x0C
    INT Value10;            // +0x10
    INT Value14;            // +0x14
    FString Texts1[9];      // +0x18 filled from the second list of the entry
    FString Texts2[9];      // +0x84 filled from the first list of the entry

    __declspec(noinline) NSSQEventPacket();          // 0x10061060 the two arrays of FStrings (the numbers are left)
    __declspec(noinline) ~NSSQEventPacket();         // 0x100610d0 the second array, then the first
};
NW_ASSERT_SIZE(NSSQEventPacket, 0xF0);

NSSQEventPacket::NSSQEventPacket()
{
}
NW_HOOK_CTOR(0x1006c960, NSSQEventPacket)

NSSQEventPacket::~NSSQEventPacket()
{
}
NW_HOOK_DTOR(0x1006c9d0, NSSQEventPacket)

// 0x10074e80 (C3 virtual 289) - the event page's entries: an id, a count of periods, and for each a number, a count of
// entries and for each three numbers and two lists of texts (up to nine kept: the stock does not check the counts),
// handed one by one to the page's builder; the texts are emptied after each
void NConsoleWnd::ReceiveSSQEvents(L2ParamStack& Params)
{
    NSSQEventPacket Record;
    INT Id = (INT)Params.Top();
    Record.Id = Id;
    INT Periods = (INT)Params.Top();
    for (INT p = 0; p < Periods; p++)
    {
        INT Period = (INT)Params.Top();
        Record.Period = Period;
        INT Entries = (INT)Params.Top();
        for (INT e = 0; e < Entries; e++)
        {
            Record.Value8 = (INT)Params.Top();
            Record.ValueC = (INT)Params.Top();
            Record.Value14 = (INT)Params.Top();
            INT Count2 = (INT)Params.Top();
            for (INT i = 0; i < Count2; i++)
            {
                TCHAR* Text = (TCHAR*)Params.Top();
                if (Text && i < 9)
                    Record.Texts2[i] = Text;
            }
            Record.Value10 = (INT)Params.Top();
            INT Count1 = (INT)Params.Top();
            for (INT i = 0; i < Count1; i++)
            {
                TCHAR* Text = (TCHAR*)Params.Top();
                if (Text && i < 9)
                    Record.Texts1[i] = Text;
            }
            ((NCSSQEventPageExt*)nw::Field<void*>(GetDesktop()->SSQMainBoard, 0xF4))->AddEvent(&Record);
            for (INT i = 0; i < 9; i++)
            {
                Record.Texts1[i].Empty();
                Record.Texts2[i].Empty();
            }
            Record.Id = Id;
            Record.Period = Period;
        }
    }
}
NW_HOOK_METHOD(0x10074e80, NConsoleWnd, ReceiveSSQEvents)

// 0x10075040 (C3 virtual 290) - the list page starts over (its child's slot 72, then the page's own: Reset), then its
// entries: three numbers, a count and four numbers each, handed one by one to the page's builder
void NConsoleWnd::ReceiveSSQList(L2ParamStack& Params)
{
    nw::Field<NCSSQSealStatusListWnd*>(GetDesktop()->SSQMainBoard, 0xF8)->ClearList();
    ((NCSSQListPageExt*)nw::Field<void*>(GetDesktop()->SSQMainBoard, 0xF8))->Reset();
    INT R[7];
    R[0] = 0;
    R[1] = 0;
    R[2] = 0;
    R[3] = 0;
    R[4] = 0;
    R[5] = 0;
    R[6] = 0;
    R[0] = (INT)Params.Top();
    R[3] = (INT)Params.Top();
    R[4] = (INT)Params.Top();
    INT Count = (INT)Params.Top();
    for (INT i = 0; i < Count; i++)
    {
        R[1] = (INT)Params.Top();
        R[2] = (INT)Params.Top();
        R[6] = (INT)Params.Top();
        R[5] = (INT)Params.Top();
        ((NCSSQListPageExt*)nw::Field<void*>(GetDesktop()->SSQMainBoard, 0xF8))->AddEntry(R);
    }
}
NW_HOOK_METHOD(0x10075040, NConsoleWnd, ReceiveSSQList)

/*-----------------------------------------------------------------------------
    Wave 3 (3): the small anonymous methods that the console's functions and the shortcut keys call. Each is a member of a
    window class that is another lane's (or has no header): a small view struct carries it, hooked at the stock address.
-----------------------------------------------------------------------------*/

struct NIntPair
{
    INT A;
    INT B;
};

// NControlExt .. NCSSQListTreeWndExt: the view structs of the small anonymous methods below are in NConsoleWndExt.h

// 0x100116b0 (name ours) - NControl, the one whose mouse press the console follows (the object at the console's +0x39E4,
// asked on every tick): when it still holds the drag (+0x234) the position of the mouse relative to its window (and its
// press offset at +0x228 / +0x22C) goes to its parent as a message 3 and the drag ends
void NControlExt::EndMouseDrag()
{
    if (nw::Field<INT>(this, 0x268))
    {
        INT X, Y;
        GConsole->GetMousePos(&X, &Y);
        WORD DY = (WORD)Y;
        DY = (WORD)(DY - nw::Field<WORD>(this, 0x260));
        WORD DX = (WORD)X;
        DX = (WORD)(DX - nw::Field<WORD>(this, 0x25C));
        DY = (WORD)(DY - nw::Field<WORD>(this, 0x48));
        DX = (WORD)(DX - nw::Field<WORD>(this, 0x44));
        ((NControl*)this)->SendToParent(3, 0, (INT)(((DWORD)DY << 16) | DX));
        nw::Field<INT>(this, 0x268) = 0;
    }
}
NW_HOOK_METHOD(0x100116b0, NControlExt, EndMouseDrag)

// 0x1004ce80 (name ours) - the pledge's info record (0x638 bytes: the name, the master's name, the crest, the ally's and the
// numbers) is cleared field by field; it returns itself
NPledgeInfoExt* NPledgeInfoExt::Clear()
{
    nw::Field<WORD>(this, 0x000) = 0;
    nw::Field<WORD>(this, 0x208) = 0;
    nw::Field<WORD>(this, 0x41C) = 0;
    nw::Field<INT>(this, 0x200) = 0;
    nw::Field<INT>(this, 0x204) = 0;
    nw::Field<INT>(this, 0x624) = 0;
    nw::Field<INT>(this, 0x408) = 0;
    nw::Field<INT>(this, 0x40C) = 0;
    nw::Field<INT>(this, 0x410) = 0;
    nw::Field<INT>(this, 0x414) = 0;
    nw::Field<INT>(this, 0x418) = 0;
    nw::Field<INT>(this, 0x61C) = 0;
    nw::Field<INT>(this, 0x620) = 0;
    nw::Field<INT>(this, 0x62C) = 0;
    nw::Field<INT>(this, 0x630) = 0;
    nw::Field<INT>(this, 0x634) = 0;
    return this;
}
NW_HOOK_METHOD(0x1004ce80, NPledgeInfoExt, Clear)

// 0x1005bc40 (name ours) - the desktop tells the messenger window (+0x18C) that the player leaves (0x1013BB30, the
// messenger's own: it stays stock)
INT NCConsoleExt::LogoutMessenger()
{
    return static_cast<NMsnMessengerView*>(((NCConsole*)this)->MessengerWnd)->SignOut();
}
NW_HOOK_METHOD(0x1005bc40, NCConsoleExt, LogoutMessenger)

// 0x1005bc50 (name ours) - the chat window is hidden when shown, else shown and focused (the shortcut key's)
void NCConsoleExt::ToggleChatWnd()
{
    NCConsole* Desk = (NCConsole*)this;
    if (Desk->ChatWnd->IsShowWindow())
        Desk->ChatWnd->HideWindow();
    else
    {
        Desk->ChatWnd->ShowWindow();
        Desk->ChatWnd->SetFocus();
    }
}
NW_HOOK_METHOD(0x1005bc50, NCConsoleExt, ToggleChatWnd)

// 0x1006c530 (name ours; a shortcut key's) - the buff icons of the party's window (+0xDC), the pet's (+0x428) and the
// summoned creature's (+0x360) are switched on / off together
void NConsoleWnd::ToggleBuffIcons()
{
    NCConsole* Desk = GetDesktop();
    nw::Field<INT>(Desk->PartyWnd, 0xFC) = nw::Field<INT>(Desk->PartyWnd, 0xFC) == 0;
    nw::Field<INT>(Desk->PetStatusWnd, 0x464) = nw::Field<INT>(Desk->PetStatusWnd, 0x464) == 0;
    nw::Field<INT>(Desk->SummonedStatusWnd, 0x390) = nw::Field<INT>(Desk->SummonedStatusWnd, 0x390) == 0;
}
NW_HOOK_METHOD(0x1006c530, NConsoleWnd, ToggleBuffIcons)

// 0x100c6d00 (name ours; a shortcut key's) - NCVideoOptionWnd: the "lower detail" check box (+0xF8, its state at +0x1F0)
// is flipped with a system message (0x503 on, 0x504 off, colored 0xFFB09B79), the window's slot 70 and Apply follow
void NCVideoOptionWndExt::ToggleLowerDetail()
{
    BYTE* Box = nw::Field<BYTE*>(this, 0x118);
    INT MsgID;
    if (nw::Field<INT>(Box, 0x294))
    {
        nw::Field<INT>(Box, 0x294) = 0;
        MsgID = 0x504;
    }
    else
    {
        nw::Field<INT>(Box, 0x294) = 1;
        MsgID = 0x503;
    }
    GConsole->ShowSystemMessage(MsgID, 0xFFB09B79);
    nw::VirtualCall<void>(this, 0x134 / 4);
    ((NCVideoOptionWnd*)this)->Apply();   // NCVideoOptionWnd::Apply
}
NW_HOOK_METHOD(0x100c6d00, NCVideoOptionWndExt, ToggleLowerDetail)

// 0x1012e290 (name ours; a shortcut key's) - a flag of the observer's window (+0xF0) flipped
INT NCBroadcastObserverWndExt2::ToggleFlag()
{
    INT New = nw::Field<INT>(this, 0x110) == 0;
    nw::Field<INT>(this, 0x110) = New;
    return New;
}
NW_HOOK_METHOD(0x1012e290, NCBroadcastObserverWndExt2, ToggleFlag)

// 0x100856b0 (name ours, ret 4) - NCDeliverItemWnd: an item of the package window's grid (its entries at +0x12C, the count at +0x130, the room at +0x150);
// 0 when it is full. The icon by its texture's name (the black texture when missing), the entry made from the record by the stock's constructor
// (0x1001F960, 0x1C40 bytes) with its count as the trade count (a signed 64-bit at +0x1B54), its tool tip (+0x1BC0, filled for type 2) and the
// scroll range
UBOOL NCDeliverItemWndExt::AddItem(NItemInfo* InfoRaw)
{
    NConsoleItemInfoC4* Info = (NConsoleItemInfoC4*)InfoRaw;
    if (nw::Field<INT>(this, 0x130) >= nw::Field<INT>(this, 0x150))
        return 0;
    if (Info->IconName)
        Info->Icon = ((NCWnd*)this)->FindTexture((const TCHAR*)Info->IconName, 1);               // NCObject::FindTexture
    if (!Info->Icon)
        Info->Icon = ((NCWnd*)this)->FindTexture(TEXT("NWindow.BlackTexture"), 1);
    BYTE* Mem = (BYTE*)GMalloc->Malloc(0x1C40, TEXT("new"));
    NItemEntry* Entry = Mem ? ::new ((void*)Mem) NItemEntry(InfoRaw) : NULL;            // NItemEntry::NItemEntry(NItemInfo*)
    nw::Field<INT>(Entry, 0x1B54) = Info->Count;
    nw::Field<INT>(Entry, 0x1B58) = Info->Count >> 31;
    EntryToolTip(Entry)->Fill(Entry, 2, 0);                         // NToolTipInfo::Fill
    nw::Field<TArray<BYTE*> >(this, 0x12C).AddItem((BYTE*)Entry);
    ((NCItemWnd*)this)->UpdateScrollRange();                                                // NCItemWnd::UpdateScrollRange
    return 1;
}
NW_HOOK_METHOD(0x100856b0, NCDeliverItemWndExt, AddItem)

// 0x100859f0 (name ours) - NCDeliverWnd: an item of the player's inventory (the grid on top, +0xC8)
UBOOL NCDeliverWndExt::AddInventoryItem(NItemInfo* Info)
{
    return nw::Field<NCDeliverItemWndExt*>(this, 0xE8)->AddItem(Info);   // C4: the grid at +0xE8 (C3 +0xC8)
}
NW_HOOK_METHOD(0x100859f0, NCDeliverWndExt, AddInventoryItem)

// 0x1008da50 (name ours) - the GM's quest window keeps an item's id and count (a list of pairs at +0x180)
void NCGMQuestWndExt::AddQuestItem(INT ItemID, INT Count)
{
    TArray<NIntPair>& Pairs = nw::Field<TArray<NIntPair> >(this, 0x1A4);
    INT i = Pairs.Add();
    Pairs(i).A = ItemID;
    Pairs(i).B = Count;
}
NW_HOOK_METHOD(0x1008da50, NCGMQuestWndExt, AddQuestItem)

// 0x10090ef0 (name ours) - NCGodWnd: a flag three windows down (+0xD4, +0xD8, +0x104: the options window's check box
// at +0x1F0), 0 when one of them is missing
INT NCGodWndExt::GetFlag()
{
    BYTE* A = nw::Field<BYTE*>(this, 0xF4);
    if (!A)
        return 0;
    BYTE* B = nw::Field<BYTE*>(A, 0xF8);
    if (!B)
        return 0;
    BYTE* C = nw::Field<BYTE*>(B, 0x124);
    if (!C)
        return 0;
    return nw::Field<INT>(C, 0x294);
}
NW_HOOK_METHOD(0x10090ef0, NCGodWndExt, GetFlag)

// 0x1009b6d0 (name ours) - NCLobbyWnd: the first word of each of the character slots (0x230 bytes each from +0x230, as
// many as +0x1030 says) is cleared
void NCLobbyWndExt::ClearSlots()
{
    BYTE* Slot = (BYTE*)this + 0x258;
    for (INT i = 0; i < nw::Field<INT>(this, 0x10F8); i++)
    {
        nw::Field<INT>(Slot, 0) = 0;
        Slot += 0x248;
    }
}
NW_HOOK_METHOD(0x1009b6d0, NCLobbyWndExt, ClearSlots)

// (the C1 NCVIPShopWnd::AddRecord(const NVIPShopEntry*) had no counterpart in this client and nothing called it: removed)

// 0x100ef970 (name ours) - NCRadarWnd: the target at that place is removed (the stock goes on to the element after the one
// that took its place)
void NCRadarWndExt::RemoveTarget(INT X, INT Y, INT Z)
{
    NCRadarWnd* This = (NCRadarWnd*)this;
    for (INT i = 0; i < This->Targets.Num(); i++)
    {
        FMinimapTarget& T = This->Targets(i);
        if (T.X == X && T.Y == Y && T.Z == Z)
            This->Targets.Remove(i, 1);
    }
}
NW_HOOK_METHOD(0x100ef970, NCRadarWndExt, RemoveTarget)

// 0x100f3760 (name ours) - NCRecipeBookWnd: the dword at +0x16C (the book's id of the list that follows)
void NCRecipeBookWndExt::SetField16C(INT Value)
{
    nw::Field<INT>(this, 0x190) = Value;
}
NW_HOOK_METHOD(0x100f3760, NCRecipeBookWndExt, SetField16C)

// 0x10100370 (name ours) - the server list's child: a 16-byte key (a session key of the login) at +0xFC, cleared when none
void NCLoginServerWndExt::SetKey(const void* Key)
{
    if (Key)
        appMemcpy((BYTE*)this + 0x11C, Key, 0x10);
    else
        appMemset((BYTE*)this + 0x11C, 0, 0x10);
}
NW_HOOK_METHOD(0x10100370, NCLoginServerWndExt, SetKey)

// 0x101211f0 (name ours) - NCZoneTitleWnd: its three texts (+0xE8, +0xF4, +0x100) are emptied, then set from the ones given; nothing happens when
// the option of the zone title (GL2ShowZoneTitle) is off
void NCZoneTitleWndExt::SetTexts(const TCHAR* Text1, const TCHAR* Text2, const TCHAR* Text3)
{
    if (GL2ShowZoneTitle)
    {
        FString& T1 = nw::Field<FString>(this, 0xE8);
        FString& T2 = nw::Field<FString>(this, 0xF4);
        FString& T3 = nw::Field<FString>(this, 0x100);
        T1.Empty();
        T2.Empty();
        T3.Empty();
        if (Text1)
            T1 = Text1;
        if (Text2)
            T2 = Text2;
        if (Text3)
            T3 = Text3;
    }
}
NW_HOOK_METHOD(0x101211f0, NCZoneTitleWndExt, SetTexts)

// 0x10121350 (cdecl, the result in st0) - the value held between the two limits (UE2's Clamp for floats)
static __declspec(noinline) FLOAT __cdecl NZoneTitleClamp(FLOAT Value, FLOAT Min, FLOAT Max)
{
    if (Value >= Min)
    {
        if (Value >= Max)
            return Max;
        return Value;
    }
    return Min;
}
NW_HOOK_FUNC(0x10121350, NZoneTitleClamp)

// the zone title's stand-in for the method the stock keeps at 0x1010b4a0 (the console's Tick calls it by address)
// NCZoneTitleWndTick (NConsoleWndExt.h)

// 0x10121380 (ret 4) - while the first line has text: a hidden window is placed (centered by the width of the longest line, a fifth of the screen
// down, as high as the first line) and shown, the time is added; past 8 seconds the lines are emptied, the time is reset and the window hidden.
// The cream text fades in over the first 2 seconds (alpha 127.5 a second) and out after the 4th (by 127.5 a second); the shadow in by 85 and
// out by 170
void NCZoneTitleWndTick::Tick(FLOAT DeltaTime)
{
    if (Line1.Len() <= 0)
        return;
    if (!IsShowWindow())
    {
        SIZE Size;
        NCGDevice::GetTextExtent(&Size, *Line1, 0, 0, 0, 0, 0, (L2FontType)4);
        SetPosition(GSEKScreenX / 2 - GetTitleWidth() / 2, GSEKScreenY / 5, GetTitleWidth(), Size.cy);
        ShowWindow();
    }
    DeltaTime = DeltaTime + Elapsed;
    Elapsed = DeltaTime;
    if (!(DeltaTime > 8.0f))
    {
        if (DeltaTime < 2.0f)
        {
            FLOAT Value = DeltaTime * 127.5f;
            if (Value < 0.0f)
                TextAlpha = (BYTE)(INT)0.0f;
            else if (Value < 255.0f)
                TextAlpha = (BYTE)(INT)Value;
            else
                TextAlpha = (BYTE)(INT)255.0f;
        }
        else if (DeltaTime > 4.0f)
            TextAlpha = (BYTE)(INT)NZoneTitleClamp(255.0f - (DeltaTime - 4.0f) * 127.5f, 0.0f, 255.0f);
        if (DeltaTime < 2.0f)
        {
            FLOAT Value = DeltaTime * 85.0f;
            if (Value < 0.0f)
                ShadowAlpha = (BYTE)(INT)0.0f;
            else if (Value < 255.0f)
                ShadowAlpha = (BYTE)(INT)Value;
            else
                ShadowAlpha = (BYTE)(INT)255.0f;
        }
        else if (DeltaTime > 4.0f)
            ShadowAlpha = (BYTE)(INT)NZoneTitleClamp(255.0f - (DeltaTime - 4.0f) * 170.0f, 0.0f, 255.0f);
        return;
    }
    Line1.Empty();
    Line2.Empty();
    Line3.Empty();
    Elapsed = 0.0f;
    if (IsShowWindow())
        HideWindow();
}
NW_HOOK_METHOD(0x10121380, NCZoneTitleWndTick, Tick)

// the root item of a tree window (the quest page's tree and the Seven Signs board's two): a default tree item (0x1EB0
// bytes, the stock's constructor and destructor) with its second word cleared is added at the top of the tree (the
// control at +0x134, its slot 75), and the result kept at +0x138 (C3: +0x114, slot 69, +0x118)
static FORCEINLINE void MakeTreeRoot(BYTE* Wnd)
{
    BYTE Item[0x1EB0];
    ::new ((void*)Item) NGaraTVItemData;
    nw::Field<INT>(Item, 4) = 0;
    INT Root = nw::VirtualCall<INT>(nw::Field<void*>(Wnd, 0x134), 75, (void*)Item, (INT)0);
    nw::Field<INT>(Wnd, 0x138) = Root;
    ((NGaraTVItemData*)Item)->~NGaraTVItemData();
}

void NCQuestTreeWndExt::MakeRoot()
{
    MakeTreeRoot((BYTE*)this);
}
NW_HOOK_METHOD(0x100eb5d0, NCQuestTreeWndExt, MakeRoot)

void NCSSQEventTreeWndExt::MakeRoot()
{
    MakeTreeRoot((BYTE*)this);
}
NW_HOOK_METHOD(0x100fdab0, NCSSQEventTreeWndExt, MakeRoot)

void NCSSQListTreeWndExt::MakeRoot()
{
    MakeTreeRoot((BYTE*)this);
}
NW_HOOK_METHOD(0x100ff4c0, NCSSQListTreeWndExt, MakeRoot)

// 0x100eb820 (name ours) - the quest page: its slot 68 (the layout), then a new root in its tree window (+0xCC)
void NCQuestGaraTreeWndExt::ResetTree()
{
    nw::VirtualCall<void>(this, 74);
    ((NCQuestTreeWndExt*)nw::Field<void*>(this, 0xEC))->MakeRoot();
}
NW_HOOK_METHOD(0x100eb820, NCQuestGaraTreeWndExt, ResetTree)

/*-----------------------------------------------------------------------------
    The messenger's friend events (the console's slots 292, 294, 295, 296): a friend record (L2Friend: the name and three
    values, its small setters 0x101265A0 / 0x101265B0 / 0x101265C0 / SetName 0x101265D0 are the messenger's own and stay
    stock) is built and handed by value to the messenger window (the desktop's +0x18C).
-----------------------------------------------------------------------------*/

// L2Friend as the stock lays it out (name ours: 0x18 bytes)
struct NL2FriendRec
{
    FString Name;      // +0x00
    INT Unknown0C;     // +0x0C (set by 0x101265B0)
    INT Unknown10;     // +0x10 (set by 0x101265A0)
    INT Unknown14;     // +0x14 (set by 0x101265C0)

    // the three one-field setters (the stock ones are anonymous; the messenger's code calls them too)
    void SetUnknown10(INT Value);   // 0x101265A0 (ret 4)
    void SetUnknown0C(INT Value);   // 0x101265B0 (ret 4)
    void SetUnknown14(INT Value);   // 0x101265C0 (ret 4)
};
NW_ASSERT_SIZE(NL2FriendRec, 0x18);

// 0x1014b070 - the +0x10 value
void NL2FriendRec::SetUnknown10(INT Value)
{
    Unknown10 = Value;
}
NW_HOOK_METHOD(0x1014b070, NL2FriendRec, SetUnknown10)

// 0x1014b080 - the +0xC value
void NL2FriendRec::SetUnknown0C(INT Value)
{
    Unknown0C = Value;
}
NW_HOOK_METHOD(0x1014b080, NL2FriendRec, SetUnknown0C)

// 0x1014b090 - the +0x14 value
void NL2FriendRec::SetUnknown14(INT Value)
{
    Unknown14 = Value;
}
NW_HOOK_METHOD(0x1014b090, NL2FriendRec, SetUnknown14)

// the record goes to a function of the messenger window by value: L2Friend's copy constructor (0x10126500) builds the
// argument (which the callee destroys), the six words of it are the argument list
static FORCEINLINE void SendFriendToMessenger(NConsoleWnd* Console, void (NMsnShellWnd::*Function)(NL2FriendArg), NL2FriendRec& Rec)
{
    NL2FriendArg Copy;
    ::new ((void*)&Copy) L2Friend(*(const L2Friend*)&Rec);
    (((NMsnShellWnd*)Console->GetDesktop()->MessengerWnd)->*Function)(Copy);
}

// 0x10075110 (C3 virtual 292) - a friend of the messenger (the +0x10 value, the name, the +0xC value, the +0x14 value) to the
// window's 0x10134610
void NConsoleWnd::OnMessengerFriendAdd(INT A, TCHAR* Name, INT B, INT C)
{
    NL2FriendRec Rec;
    Rec.SetUnknown10(A);
    ((L2Friend*)&Rec)->SetName(Name);
    Rec.SetUnknown0C(B);
    Rec.SetUnknown14(C);
    SendFriendToMessenger(this, &NMsnShellWnd::FriendAdd, Rec);
}
NW_HOOK_METHOD(0x10075110, NConsoleWnd, OnMessengerFriendAdd)

// 0x100751c0 (C3 virtual 294) - the same with a kind first: 1 to the window's 0x10134610, 2 to 0x101346F0, 3 to 0x10134680
// (the other kinds only build the record)
void NConsoleWnd::OnMessengerFriendEvent(INT Kind, INT A, TCHAR* Name, INT B, INT C)
{
    NL2FriendRec Rec;
    Rec.SetUnknown10(A);
    ((L2Friend*)&Rec)->SetName(Name);
    Rec.SetUnknown0C(B);
    Rec.SetUnknown14(C);
    if (Kind == 1)
        SendFriendToMessenger(this, &NMsnShellWnd::FriendAdd, Rec);
    else if (Kind == 2)
        SendFriendToMessenger(this, &NMsnShellWnd::FriendUpdate, Rec);
    else if (Kind == 3)
        SendFriendToMessenger(this, &NMsnShellWnd::FriendRemove, Rec);
}
NW_HOOK_METHOD(0x100751c0, NConsoleWnd, OnMessengerFriendEvent)

// 0x100752d0 (C3 virtual 295) - a friend's status (the +0xC value, the name, the +0x10 value) to the window's 0x101346F0
void NConsoleWnd::OnMessengerFriendStatus(INT B, TCHAR* Name, INT A)
{
    NL2FriendRec Rec;
    Rec.SetUnknown0C(B);
    ((L2Friend*)&Rec)->SetName(Name);
    Rec.SetUnknown10(A);
    SendFriendToMessenger(this, &NMsnShellWnd::FriendUpdate, Rec);
}
NW_HOOK_METHOD(0x100752d0, NConsoleWnd, OnMessengerFriendStatus)

// 0x10075370 (C3 virtual 296) - a message of a friend: the text is looked up for bad words (the stock does not read the
// answer), then both texts go to the messenger window's 0x10139680 when they are there
void NConsoleWnd::OnMessengerChat(INT A, TCHAR* Name, TCHAR* Text)
{
    HasBadWord(Text);   // HasBadWord (hooked there)
    if (Name && Text)
        ((NMsnShellWnd*)GetDesktop()->MessengerWnd)->PanelChat(A, Name, Text);
}
NW_HOOK_METHOD(0x10075370, NConsoleWnd, OnMessengerChat)

/*-----------------------------------------------------------------------------
    The messenger's friend list and the friends window's tree (the anonymous functions the events above end in). The classes
    are the messenger lanes'; what is declared here is what these functions read (names ours). The records are the L2Friend
    of NL2FriendRec.
-----------------------------------------------------------------------------*/

static FORCEINLINE NL2FriendRec& NMsnFriendOf(NL2FriendArg& Arg)
{
    return *(NL2FriendRec*)&Arg;
}

// A string passed by value to a stock function that is only declared here (a naked stub cannot take an FString): its three words,
// built in raw memory so that nothing destroys it here - the callee does, as the stock caller expects
struct NStrArg
{
    DWORD W[3];
};
NW_ASSERT_SIZE(NStrArg, 0xC);

static FORCEINLINE NStrArg NMsnStrArg(const TCHAR* Text)
{
    BYTE Mem[sizeof(FString)];
    ::new ((void*)Mem) FString(Text);
    return *(NStrArg*)Mem;
}

// a string's copy for a callee that destroys its argument, made with FString's copy constructor (the words of NewChat's by-value
// strings)
static FORCEINLINE NStrArg NMsnStrCopy(const FString& Text)
{
    BYTE Mem[sizeof(FString)];
    ::new ((void*)Mem) FString(Text);
    return *(NStrArg*)Mem;
}

// the record's copy for a callee that destroys its argument: L2Friend's copy constructor (0x10126500), as the stock does
static FORCEINLINE void NMsnFriendCopy(NL2FriendArg& Copy, NL2FriendRec& Src)
{
    ::new ((void*)&Copy) L2Friend(*(const L2Friend*)&Src);
}

// The friend list: the records (+0x00), and a UE2 TMap (+0x0C) from the name to the record (TMapBase: the pairs of 0x14 bytes -
// the chain, the key, the value -, the hash and its size)
struct NMsnFriendPair
{
    INT HashNext;          // +0x00
    FString Name;          // +0x04
    NL2FriendRec* Rec;     // +0x10
};
NW_ASSERT_SIZE(NMsnFriendPair, 0x14);

struct NMsnFriendMap
{
    TArray<NMsnFriendPair> Pairs;   // +0x00
    INT* Hash;                      // +0x0C
    INT HashCount;                  // +0x10
};
NW_ASSERT_SIZE(NMsnFriendMap, 0x14);

// The friends are sorted with the online ones first (the +0xC value of the record, non zero), by name within each (appStrcmp)
// 0x10151340 - the comparator appQsort gets (cdecl; the arguments point to the list's elements)
static INT __cdecl NMsnCompareFriends(const void* PA, const void* PB)
{
    NL2FriendRec* A = *(NL2FriendRec* const*)PA;
    NL2FriendRec* B = *(NL2FriendRec* const*)PB;
    if (A->Unknown0C != 0 && B->Unknown0C == 0)
        return -1;
    if (A->Unknown0C == 0 && B->Unknown0C != 0)
        return 1;
    return appStrcmp(*A->Name, *B->Name);
}
NW_HOOK_FUNC(0x10151340, NMsnCompareFriends)

struct NMsnFriendList
{
    TArray<NL2FriendRec*> Friends;   // +0x00
    NMsnFriendMap Map;               // +0x0C

    __declspec(noinline) void Sort();     // 0x1012C9F0
    void Update(NL2FriendArg Arg);        // 0x1012CC30 (ret 0x18)
    void AddOrUpdate(NL2FriendArg Arg);   // 0x1012D1F0 (ret 0x18)
    void Remove(NL2FriendArg Arg);        // 0x1012D5A0 (ret 0x18)
    void Clear();                         // 0x1012D660
};
NW_ASSERT_SIZE(NMsnFriendList, 0x20);

// 0x10151480 - the records sorted
void NMsnFriendList::Sort()
{
    appQsort(Friends.GetData(), Friends.Num(), sizeof(NL2FriendRec*), NMsnCompareFriends);
}
NW_HOOK_METHOD(0x10151480, NMsnFriendList, Sort)

// 0x101516d0 - the record of the same name (the list's Find, 0x1012CBD0, takes the name by value) takes the three values
void NMsnFriendList::Update(NL2FriendArg Arg)
{
    NL2FriendRec& Rec = NMsnFriendOf(Arg);
    NL2FriendRec* Found = (NL2FriendRec*)((NL2Book*)this)->Find(MsnStringCopy(Rec.Name));
    if (Found)
    {
        Found->SetUnknown0C(Rec.Unknown0C);
        Found->SetUnknown10(Rec.Unknown10);
        Found->SetUnknown14(Rec.Unknown14);
    }
    Rec.Name.~FString();
}
NW_HOOK_METHOD(0x101516d0, NMsnFriendList, Update)

// 0x10151d50 - the same, and a name the list does not know is added: a new record made by L2Friend's copy constructor
// (0x10126500), the map told (0x101361B0: the name and the record's address) and the record appended
void NMsnFriendList::AddOrUpdate(NL2FriendArg Arg)
{
    NL2FriendRec& Rec = NMsnFriendOf(Arg);
    NL2FriendRec* Found = (NL2FriendRec*)((NMsnWndStrMap*)&Map)->FindWnd(&Rec.Name);
    if (Found)
    {
        Found->SetUnknown0C(Rec.Unknown0C);
        Found->SetUnknown10(Rec.Unknown10);
        Found->SetUnknown14(Rec.Unknown14);
    }
    else
    {
        NL2FriendRec* New = (NL2FriendRec*)appMalloc(sizeof(NL2FriendRec), TEXT("new"));
        if (New)
            ::new ((void*)New) L2Friend(*(const L2Friend*)&Rec);
        ((NMsnWndStrMap*)&Map)->SetWnd(*Rec.Name, (NCWnd**)&New);
        INT Index = Friends.Add();
        ((NL2FriendRec**)Friends.GetData())[Index] = New;
    }
    Rec.Name.~FString();
}
NW_HOOK_METHOD(0x10151d50, NMsnFriendList, AddOrUpdate)

// 0x10152180 - the record of the same name (compared without case) goes (its name, then its memory), then the map forgets
// the name (0x101374E0)
void NMsnFriendList::Remove(NL2FriendArg Arg)
{
    NL2FriendRec& Rec = NMsnFriendOf(Arg);
    for (INT i = 0; i < Friends.Num(); i++)
    {
        NL2FriendRec* Other = Friends(i);
        if (Other && Other->Name == Rec.Name)
        {
            Other->Name.~FString();
            appFree(Other);
            Friends.Remove(i, 1);   // TPtrArray::Remove (the template's out-of-line copy)
            break;
        }
    }
    ((NMsnStrMapOps*)&Map)->RemoveKey(*Rec.Name);
    Rec.Name.~FString();
}
NW_HOOK_METHOD(0x10152180, NMsnFriendList, Remove)

// 0x10152240 - every record and its memory go, the array emptied; the map's pairs lose their names, the pairs are emptied
// (of 0x14 bytes), the hash goes back to 8 entries and is rebuilt (0x1012CA90)
void NMsnFriendList::Clear()
{
    for (INT i = 0; i < Friends.Num(); i++)
    {
        NL2FriendRec* Other = Friends(i);
        if (Other)
        {
            Other->Name.~FString();
            appFree(Other);
        }
    }
    Friends.Empty();
    for (INT i = 0; i < Map.Pairs.Num(); i++)
        Map.Pairs(i).Name.~FString();
    Map.Pairs.FArray::Empty(sizeof(NMsnFriendPair));
    Map.HashCount = 8;
    ((NMsnStrMap*)&Map)->Rehash();
}
NW_HOOK_METHOD(0x10152240, NMsnFriendList, Clear)

// Two stock tree walks (stdcall, one argument): the first child of an entry, else the entry that follows it among its
// parent's children (0x1002E600); the entry that follows it among its parent's children, NULL for the last (0x1002E5A0)
static NCTVItem* __stdcall NMsnFirstChildOrNext(NCTVItem* Item)
{
    return (NCTVItem*)((NMsnTreeNode*)Item)->GetNextNode((NMsnTreeNode*)Item);   // 0x10032F20 (the function ignores `this`)
}
static NCTVItem* __stdcall NMsnNextSibling(NCTVItem* Item)
{
    return (NCTVItem*)((NMsnTreeNode*)Item)->GetNextSibling((NMsnTreeNode*)Item);   // 0x10032EC0 (the function ignores `this`)
}

// A copy of the friend list's array, made by the stock instance of the pointer array's copy constructor (0x10142780, ret 4)
// and unmade by its destructor (0x1012CD40, hooked as TPtrArray's)
struct NMsnRecordArray
{
    NL2FriendRec** Data;
    INT Num;
    INT Max;

    void CopyOf(const NMsnFriendList* List);   // 0x10142780
    void Destroy();                            // 0x1012CD40
};
// the pointer arrays' copy constructor (0x10167130: room for the other's count, then each pointer added at the end) and destructor
// (0x101518A0, the instance of TArray<void*>): the template's own
void NMsnRecordArray::CopyOf(const NMsnFriendList* List)
{
    ::new ((void*)this) TArray<void*>(*(const TArray<void*>*)&List->Friends);
}
void NMsnRecordArray::Destroy()
{
    ((TArray<void*>*)this)->~TArray<void*>();
}

// The friends window of the messenger (the main panel's +0xF4): the friend list it shows (+0x114) and its tree view (+0x118;
// the root has two entries, the groups of the online (1) and the offline (0) friends, with the friends under them)
struct NMsnFriendsWnd : public NCWnd
{
    __declspec(noinline) void Rebuild();                         // 0x101437E0
    __declspec(noinline) void AddGroups();                       // 0x10143080
    __declspec(noinline) void AddFriendNode(DWORD W0, DWORD W1, DWORD W2, DWORD W3, DWORD W4, DWORD W5);   // 0x101431F0 (ret 0x18: the record
                                                                 //   by value, as six words, so that the hook entry is the function itself)
    __declspec(noinline) void Refresh();                         // 0x101436F0
    __declspec(noinline) NCTVItem* FindGroup(INT GroupID);       // 0x10142700 (ret 4)
    __declspec(noinline) void ResetRoot();                       // 0x10142740

    NMsnFriendList* List() { return nw::Field<NMsnFriendList*>(this, 0x134); }
    NCTree* Tree() { return nw::Field<NCTree*>(this, 0x138); }
};

// 0x101670f0 - the tree starts again from an empty root
void NMsnFriendsWnd::ResetRoot()
{
    NTVItemData Data;
    Tree()->AddItem(&Data, NULL);
}
NW_HOOK_METHOD(0x101670f0, NMsnFriendsWnd, ResetRoot)

// 0x101670b0 - the group entry of the number (the +4 of its record), NULL when there is none
NCTVItem* NMsnFriendsWnd::FindGroup(INT GroupID)
{
    NCTVItem* Item = NMsnFirstChildOrNext(Tree()->Root);
    if (!Item)
        return NULL;
    while (Item->Data.Unknown04 != GroupID)
    {
        Item = NMsnNextSibling(Item);
        if (!Item)
            return NULL;
    }
    return Item;
}
NW_HOOK_METHOD(0x101670b0, NMsnFriendsWnd, FindGroup)

// 0x10167a40 - the two groups under the root (when the tree has one): the online ones (sysstring 1006, group 1) and the offline
// ones (1007, group 0), each with the plus and minus pictures of the quest tree and the black one as its second picture
void NMsnFriendsWnd::AddGroups()
{
    NTVItemData Data;
    UTexture* Plus = FindTexture(TEXT("L2UI_CH3.QUESTWND.QuestWndPlusBtn"));
    UTexture* Minus = FindTexture(TEXT("L2UI_CH3.QUESTWND.QuestWndMinusBtn"));
    Data.ExpandedTex = Minus;
    Data.CollapsedTex = Plus;
    Data.Icon2Tex = FindTexture(TEXT("NWindow.BlackTexture"));
    appStrcpy(Data.Text, *GL2GameData.SysString[1006]);
    Data.Unknown04 = 1;
    Data.Unknown2848 = 2;
    Data.bCenterOnIcon2 = 0;
    Data.Icon2Box.X = 0;
    Data.Icon2Box.Y = 0;
    Data.Icon2Box.W = 0.0f;
    Data.Icon2Box.H = 15.0f;
    Data.IconBox.X = 0;
    Data.IconBox.Y = 0;
    Data.IconBox.W = 15.0f;
    Data.IconBox.H = 15.0f;
    Data.TextBox.X = 0;
    Data.TextBox.Y = 0;
    Data.TextBox.W = 220.0f;
    Data.TextBox.H = 14.0f;
    if (Tree()->Root)
        Tree()->AddItem(&Data, Tree()->Root);
    appStrcpy(Data.Text, *GL2GameData.SysString[1007]);
    Data.Unknown04 = 0;
    if (Tree()->Root)
        Tree()->AddItem(&Data, Tree()->Root);
}
NW_HOOK_METHOD(0x10167a40, NMsnFriendsWnd, AddGroups)

// 0x10167bb0 (ret 0x18) - a friend under the group of its +0xC value (online or not): the name as the text, the +0x10 and +0xC
// values in the entry's +4 and +8, the text centered on the second picture (3 in the +0x2848 the tree uses)
void NMsnFriendsWnd::AddFriendNode(DWORD W0, DWORD W1, DWORD W2, DWORD W3, DWORD W4, DWORD W5)
{
    NL2FriendArg Arg;
    Arg.W[0] = W0;
    Arg.W[1] = W1;
    Arg.W[2] = W2;
    Arg.W[3] = W3;
    Arg.W[4] = W4;
    Arg.W[5] = W5;
    NL2FriendRec& Rec = NMsnFriendOf(Arg);
    NTVItemData Data;
    appStrcpy(Data.Text, *Rec.Name);
    Data.Unknown04 = Rec.Unknown10;
    Data.Unknown08 = Rec.Unknown0C;
    Data.Unknown2848 = 3;
    Data.bCenterOnIcon2 = 1;
    Data.Icon2Box.X = 0;
    Data.Icon2Box.Y = 0;
    Data.Icon2Box.W = 0.0f;
    Data.Icon2Box.H = 15.0f;
    Data.TextBox.X = 0;
    Data.TextBox.Y = 0;
    Data.TextBox.W = 220.0f;
    Data.TextBox.H = 14.0f;
    NCTVItem* Group = FindGroup(Rec.Unknown0C);
    if (Group)
        Tree()->AddItem(&Data, Group);
    Rec.Name.~FString();
}
NW_HOOK_METHOD(0x10167bb0, NMsnFriendsWnd, AddFriendNode)

// 0x101680b0 - the friends of every group made again from the list: the group's entries deleted, then each record of a copy
// of the list that belongs to the group (by its +0xC value) added under it
void NMsnFriendsWnd::Refresh()
{
    NCTVItem* Group = NMsnFirstChildOrNext(Tree()->Root);
    while (Group)
    {
        Group->DeleteAllChildren();
        NMsnRecordArray Copy;
        Copy.CopyOf(List());
        for (INT i = 0; i < Copy.Num; i++)
        {
            NL2FriendRec* Rec = Copy.Data[i];
            if (Rec && Group == FindGroup(Rec->Unknown0C))
            {
                NL2FriendArg Arg;
                NMsnFriendCopy(Arg, *Rec);
                AddFriendNode(Arg.W[0], Arg.W[1], Arg.W[2], Arg.W[3], Arg.W[4], Arg.W[5]);
            }
        }
        Group = NMsnNextSibling(Group);
        Copy.Destroy();
    }
}
NW_HOOK_METHOD(0x101680b0, NMsnFriendsWnd, Refresh)

// 0x101681a0 - the tree built again: the window's slot 72, a new root, the two groups, every friend of a copy of the list under
// its group, and the root opened
void NMsnFriendsWnd::Rebuild()
{
    ::nw::VirtualCall<void>(this, 78);
    ResetRoot();
    AddGroups();
    NMsnRecordArray Copy;
    Copy.CopyOf(List());
    for (INT i = 0; i < Copy.Num; i++)
    {
        NL2FriendRec* Rec = Copy.Data[i];
        if (Rec)
        {
            NL2FriendArg Arg;
            NMsnFriendCopy(Arg, *Rec);
            AddFriendNode(Arg.W[0], Arg.W[1], Arg.W[2], Arg.W[3], Arg.W[4], Arg.W[5]);
        }
    }
    if (Tree()->Root)
        Tree()->Root->Expand();
    Copy.Destroy();
}
NW_HOOK_METHOD(0x101681a0, NMsnFriendsWnd, Rebuild)

// The windows of the chats. A chat window is kept by the friend's name in a map of pairs of 0x14 bytes (the chain, the name, the
// window); the windows of the chats that were closed are kept in a set (a map of 0xC byte pairs, the window as key and as value)
struct NMsnChatPair
{
    INT HashNext;      // +0x00
    FString Name;      // +0x04
    NCWnd* Wnd;        // +0x10
};
NW_ASSERT_SIZE(NMsnChatPair, 0x14);

struct NMsnChatMap
{
    TArray<NMsnChatPair> Pairs;   // +0x00
    INT* Hash;                    // +0x0C
    INT HashCount;                // +0x10
};
NW_ASSERT_SIZE(NMsnChatMap, 0x14);

struct NMsnWndPair
{
    INT HashNext;      // +0x00
    INT Key;           // +0x04 the window's address
    NCWnd* Wnd;        // +0x08
};
NW_ASSERT_SIZE(NMsnWndPair, 0xC);

struct NMsnWndSet
{
    TArray<NMsnWndPair> Pairs;    // +0x00
    INT* Hash;                    // +0x0C
    INT HashCount;                // +0x10

    NCWnd** Set(const INT* Key, NCWnd* const* Value);   // 0x10136230 (ret 8) the value under the key, made when there is none
};
NW_ASSERT_SIZE(NMsnWndSet, 0x14);
NCWnd** NMsnWndSet::Set(const INT* Key, NCWnd* const* Value)
{
    return ((NMsnWndIntMap*)this)->SetWnd(Key, (NCWnd**)Value);   // 0x10151770
}

// the chat window's text view (its +0xE4): a line added (0x101250C0, ret 0xC; not ported here)
struct NMsnChatView
{
    void AddLine(const TCHAR* Who, const TCHAR* Text, DWORD Color);
};
void NMsnChatView::AddLine(const TCHAR* Who, const TCHAR* Text, DWORD Color)
{
    ((L2ChatBox*)this)->AddMessage(Who, Text, Color);   // 0x10149B70
}

// A chat window: a reset (0x101242D0: its +0xD8 cleared, two of its parts told through their slots 71 and 70) and the lines
// the system code of a message adds to the view at +0xE4
struct NMsnChatWnd : public NCWnd
{
    void Reset();   // 0x101242D0
    __declspec(noinline) void OnCode(INT Code, DWORD A0, DWORD A1, DWORD A2, DWORD B0, DWORD B1, DWORD B2);   // 0x10125230 (ret 0x1C)
    void OnCodeStr(INT Code, DWORD A0, DWORD A1, DWORD A2, DWORD B0, DWORD B1, DWORD B2);   // the same entry, called with the texts as strings (their words)
};
void NMsnChatWnd::Reset()
{
    ((L2ChatWnd*)this)->ClearAll();   // 0x10148DF0
}
void NMsnChatWnd::OnCodeStr(INT Code, DWORD A0, DWORD A1, DWORD A2, DWORD B0, DWORD B1, DWORD B2)
{
    OnCode(Code, A0, A1, A2, B0, B1, B2);   // 0x10149CE0: the same entry, the words are the strings (which OnCode destroys)
}

// 0x10149ce0 (ret 0x1C; the two texts by value, as six words, which the function destroys) - code 0 puts the two texts in
// the view (a line of the friend) in the normal color; 0x91, 0x93, 0x26C and 0x437 put a text of the game data
// (GL2GameData +0x8210C, +0x820AC, +0x8213C, +0x820DC) in red (0xFFE37171) with no name; any other code adds nothing
void NMsnChatWnd::OnCode(INT Code, DWORD A0, DWORD A1, DWORD A2, DWORD B0, DWORD B1, DWORD B2)
{
    DWORD Words[6] = { A0, A1, A2, B0, B1, B2 };
    FString& TextA = *(FString*)&Words[0];
    FString& TextB = *(FString*)&Words[3];
    NMsnChatView* View = nw::Field<NMsnChatView*>(this, 0x104);
    switch (Code)
    {
    case 0:
        View->AddLine(*TextA, *TextB, 0xFFDCDCDC);
        break;
    case 0x91:
        View->AddLine(NULL, *nw::Field<FString>(&GL2GameData, 0x8210C), 0xFFE37171);
        break;
    case 0x93:
        View->AddLine(NULL, *nw::Field<FString>(&GL2GameData, 0x820AC), 0xFFE37171);
        break;
    case 0x26C:
        View->AddLine(NULL, *nw::Field<FString>(&GL2GameData, 0x8213C), 0xFFE37171);
        break;
    case 0x437:
        View->AddLine(NULL, *nw::Field<FString>(&GL2GameData, 0x820DC), 0xFFE37171);
        break;
    }
    TextA.~FString();
    TextB.~FString();
}
NW_HOOK_METHOD(0x10149ce0, NMsnChatWnd, OnCode)

// the layout object of the chat list (its +0xDC): a chat item taken out of it
struct NMsnChatLayout
{
    void Remove(NCWnd* Item);   // 0x10124FC0 (ret 4)
};
void NMsnChatLayout::Remove(NCWnd* Item)
{
    ((NChatTaskListBox*)this)->RemoveTask(Item);   // 0x10149A70
}

// The chat list (the main panel's +0xF8): its items (a map of 0xC byte pairs at +0xC8, the item at +8 of each) and their layout
struct NMsnChatListWnd : public NCWnd
{
    __declspec(noinline) void DestroyItems();   // 0x101256A0
    void* FindItem(DWORD N0, DWORD N1, DWORD N2);   // 0x101255D0 (ret 0xC, not ported here) the item of a friend
};
void* NMsnChatListWnd::FindItem(DWORD N0, DWORD N1, DWORD N2)
{
    DWORD Words[3] = { N0, N1, N2 };
    FString& Name = *(FString*)Words;   // the caller's copy, which this function destroys (as the stock callee did)
    void* Found = ((L2TaskWnd*)this)->FindTask(Name);   // 0x1014A080: FindTask takes its own copy
    Name.~FString();
    return Found;
}

// 0x1014a150 - every item taken out of the layout, removed from the window and deleted (its scalar deleting destructor), the map
// emptied and its hash rebuilt (0x10124A60) with 8 entries
void NMsnChatListWnd::DestroyItems()
{
    NMsnWndSet& Map = *(NMsnWndSet*)((BYTE*)this + 0xE8);
    for (INT i = 0; i < Map.Pairs.Num(); i++)
    {
        NCWnd* Item = Map.Pairs(i).Wnd;
        if (Item)
        {
            nw::Field<NMsnChatLayout*>(this, 0xFC)->Remove(Item);
            RemoveChildWindow(Item->Entry);
            ::nw::VirtualCall<void>(Item, 3, 1);
        }
    }
    Map.Pairs.FArray::Empty(sizeof(NMsnWndPair));
    Map.HashCount = 8;
    ((NTaskMap*)&Map)->Rehash();
}
NW_HOOK_METHOD(0x1014a150, NMsnChatListWnd, DestroyItems)

// The messenger's main panel (the window's +0xCC): the friend list (+0xC8), the chat windows by name (+0xCC) and the closed
// ones (+0xE0), the friends window (+0xF4) and the chat list (+0xF8)
struct NMsnMainPanel : public NCWnd
{
    void AddFriend(NL2FriendArg Arg);      // 0x10133E70 (ret 0x18)
    void RemoveFriend(NL2FriendArg Arg);   // 0x10133EF0 (ret 0x18)
    void UpdateFriend(NL2FriendArg Arg);   // 0x10133F70 (ret 0x18)
    __declspec(noinline) void ReloadFriends();   // 0x10131420
    __declspec(noinline) void CloseAll();        // 0x10139370
    __declspec(noinline) void HideAll();         // 0x10138B50 (name ours)
    void OnChat(INT A, TCHAR* Name, TCHAR* Text);      // 0x10139230 (ret 0xC)
    void NewChat(INT A, DWORD N0, DWORD N1, DWORD N2, DWORD T0, DWORD T1, DWORD T2);   // 0x10138F50 (ret 0x1C; the two texts by value, as words)

    NMsnFriendList* List() { return nw::Field<NMsnFriendList*>(this, 0xE8); }
    NMsnChatMap& Chats() { return *(NMsnChatMap*)((BYTE*)this + 0xEC); }
    NMsnWndSet& Parked() { return *(NMsnWndSet*)((BYTE*)this + 0x100); }
    NMsnFriendsWnd* FriendsWnd() { return nw::Field<NMsnFriendsWnd*>(this, 0x114); }
    NMsnChatListWnd* ChatList() { return nw::Field<NMsnChatListWnd*>(this, 0x118); }
};

// every chat window reset and hidden and put in the closed set, the map of the chats emptied (the names destroyed, the hash
// rebuilt with 8 entries: 0x1012CA90): the first part of CloseAll (the stock has it inline in the panel's hide-all function too)
static FORCEINLINE void NMsnParkChats(NMsnMainPanel* Panel)
{
    NMsnChatMap& Chats = Panel->Chats();
    for (INT i = 0; i < Chats.Pairs.Num(); i++)
    {
        NCWnd* Wnd = Chats.Pairs(i).Wnd;
        if (Wnd)
        {
            NCWnd* Key = Wnd;
            NCWnd* Value = Wnd;
            ((NMsnChatWnd*)Wnd)->Reset();
            Panel->Parked().Set((const INT*)&Key, &Value);
            Wnd->HideWindow();
        }
    }
    for (INT i = 0; i < Chats.Pairs.Num(); i++)
        Chats.Pairs(i).Name.~FString();
    Chats.Pairs.FArray::Empty(sizeof(NMsnChatPair));
    Chats.HashCount = 8;
    ((NMsnStrMap*)&Chats)->Rehash();
}

// 0x1015dce0 - the chats parked and hidden, the chat list's items deleted and the list hidden, the friends window's slot 72
// and the friend list emptied
void NMsnMainPanel::CloseAll()
{
    NMsnParkChats(this);
    if (ChatList())
    {
        ChatList()->DestroyItems();
        ChatList()->HideWindow();
    }
    ::nw::VirtualCall<void>(FriendsWnd(), 78);
    List()->Clear();
}
NW_HOOK_METHOD(0x1015dce0, NMsnMainPanel, CloseAll)

// 0x1015d4e0 (name ours) - the messenger window's hide: the chats parked and hidden as CloseAll starts, every window of the closed
// set hidden (the ones parked before too), the chat list's items deleted and the list hidden. The friends window and the friend list
// stay as they are
void NMsnMainPanel::HideAll()
{
    NMsnParkChats(this);
    NMsnWndSet& Closed = Parked();
    for (INT i = 0; i < Closed.Pairs.Num(); i++)
    {
        NCWnd* Wnd = Closed.Pairs(i).Wnd;
        if (Wnd)
            Wnd->HideWindow();
    }
    if (ChatList())
    {
        ChatList()->DestroyItems();
        ChatList()->HideWindow();
    }
}
NW_HOOK_METHOD(0x1015d4e0, NMsnMainPanel, HideAll)

// The messenger window's first page, the MsnWnd (include/nwindow/MsnWnd.h is another lane's; its children by their offsets): the
// open chat windows by account (+0xCC, pairs of 0x14 bytes with the window at +0x10), the windows by a number (+0xF4, pairs of 0xC
// bytes with the window at +0x8), the Lineage II friends window (+0x160, may be none) and the twelve windows from +0x164 on
// (option, alarm, status, the dialogs: always there)
struct NMsnPageView : public NCWnd
{
    __declspec(noinline) void HideAll();   // 0x10136CA0 (name ours)
};

// 0x1015b690 (name ours) - every child window of the messenger page hidden
void NMsnPageView::HideAll()
{
    NMsnChatMap& Chats = *(NMsnChatMap*)((BYTE*)this + 0xEC);
    for (INT i = 0; i < Chats.Pairs.Num(); i++)
    {
        NCWnd* Wnd = Chats.Pairs(i).Wnd;
        if (Wnd)
            Wnd->HideWindow();
    }
    NMsnWndSet& Numbered = *(NMsnWndSet*)((BYTE*)this + 0x114);
    for (INT i = 0; i < Numbered.Pairs.Num(); i++)
    {
        NCWnd* Wnd = Numbered.Pairs(i).Wnd;
        if (Wnd)
            Wnd->HideWindow();
    }
    NCWnd* L2Friends = nw::Field<NCWnd*>(this, 0x180);
    if (L2Friends)
        L2Friends->HideWindow();
    for (DWORD Offset = 0x184; Offset <= 0x1B0; Offset += 4)
        nw::Field<NCWnd*>(this, Offset)->HideWindow();
}
NW_HOOK_METHOD(0x1015b690, NMsnPageView, HideAll)

// NMsnMessengerView (NConsoleWndExt.h): the messenger window (CMessengerWnd, 0xD8 bytes; include/nwindow/MessengerWnd.h is another lane's)

// 0x1015dfc0 (name ours; NCConsole::ShowWindow's) - both pages' children hidden
void NMsnMessengerView::HideAll()
{
    nw::Field<NMsnPageView*>(this, 0xE8)->HideAll();
    nw::Field<NMsnMainPanel*>(this, 0xEC)->HideAll();
}
NW_HOOK_METHOD(0x1015dfc0, NMsnMessengerView, HideAll)

// 0x1013bb30 (name ours; NCConsole's +0x18C, the desktop's logout) - the sign-out button of the MSN page (MsnWnd::OnSignOutButton,
// 0x1013AA20), whose answer comes back
INT NMsnMessengerView::SignOut()
{
    return nw::Field<MsnWnd*>(this, 0xE8)->OnSignOutButton();
}
NW_HOOK_METHOD(0x10160550, NMsnMessengerView, SignOut)

// 0x1015dba0 (ret 0xC) - a message of a friend (A, the friend's name, the text): the chat window kept under the name (the map's
// Find, 0x101347D0) shows it through its OnCode (the code A with the name and the text); when that window is hidden and the
// chat list has the friend's item, the item is marked (+0xD8 = 1). A friend without a chat window gets one (0x10138F50, with the
// same three values)
void NMsnMainPanel::OnChat(INT A, TCHAR* Name, TCHAR* Text)
{
    NMsnChatWnd* Chat;
    {
        FString Key(Name);
        Chat = (NMsnChatWnd*)((NMsnWndStrMap*)&Chats())->FindWnd(&Key);
    }
    if (Chat)
    {
        NStrArg TextArg = NMsnStrArg(Text);
        NStrArg NameArg = NMsnStrArg(Name);
        Chat->OnCodeStr(A, NameArg.W[0], NameArg.W[1], NameArg.W[2], TextArg.W[0], TextArg.W[1], TextArg.W[2]);
        if (!Chat->IsShowWindow() && ChatList())
        {
            NStrArg ItemName = NMsnStrArg(Name);
            void* Item = ChatList()->FindItem(ItemName.W[0], ItemName.W[1], ItemName.W[2]);
            if (Item)
                nw::Field<INT>(Item, 0xF8) = 1;
        }
    }
    else
    {
        NStrArg TextArg = NMsnStrArg(Text);
        NStrArg NameArg = NMsnStrArg(Name);
        NewChat(A, NameArg.W[0], NameArg.W[1], NameArg.W[2], TextArg.W[0], TextArg.W[1], TextArg.W[2]);
    }
}
NW_HOOK_METHOD(0x1015dba0, NMsnMainPanel, OnChat)

// 0x1015d8d0 (ret 0x1C; the name and the text by value, as six words, which the function destroys) - a message (code A) of a friend
// whose chat window is not entered by his name yet. The task window is made and shown first. A friend without a window gets one:
// a free one of the number table, or a new L2ChatWnd (256 x 401 at a random place, a child of the desktop); it is entered by the name,
// titled, given the name, marked (+0xD8 = 1) and left HIDDEN, and the task window lists it. A friend with a window has it titled
// and given the name again, marked, and when it is not shown the task window's entry of the friend is marked too. Then the
// window's OnCode shows the message
void NMsnMainPanel::NewChat(INT A, DWORD N0, DWORD N1, DWORD N2, DWORD T0, DWORD T1, DWORD T2)
{
    DWORD Words[6] = { N0, N1, N2, T0, T1, T2 };
    FString& Name = *(FString*)&Words[0];
    FString& Text = *(FString*)&Words[3];
    ((L2FriendWnd*)this)->ShowTaskWnd();   // L2FriendWnd::ShowTaskWnd
    NCWnd* Chat;
    {
        FString Key(*Name);
        Chat = ((NMsnWndStrMap*)&Chats())->FindWnd(&Key);   // the table of names
    }
    if (!Chat)
    {
        Chat = ((L2FriendWnd*)this)->FindFreeChatWnd();   // L2FriendWnd::FindFreeChatWnd
        if (!Chat)
        {
            Chat = new L2ChatWnd(this, Name);   // L2ChatWnd (0x1014A5D0, the name by value: a copy)
            INT X = appRand() % 200 + 200;
            INT Y = appRand() % 150 + 150;
            Chat->SetPosition(X, Y, 0x100, 0x191);
            GConsole->Desktop->CreateChild(Chat, 0x2002);
        }
        ((NMsnWndStrMap*)&Chats())->SetWnd(*Name, &Chat);   // entered by the name
        ::nw::VirtualCall<void>(Chat, 56, 0x3E5);   // the title: system string 997
        ((L2ChatWnd*)Chat)->SetFriendName(Name);   // the chat window keeps the name (a copy)
        nw::Field<INT>(Chat, 0xF8) = 1;
        Chat->HideWindow();
        if (ChatList())
        {
            ((NChatL2TaskWnd*)ChatList())->AddTask(Chat, Name, 1);   // the task window lists it (a copy of the name)
        }
    }
    else
    {
        ::nw::VirtualCall<void>(Chat, 56, 0x3E5);   // the title: system string 997
        ((L2ChatWnd*)Chat)->SetFriendName(Name);
        nw::Field<INT>(Chat, 0xF8) = 1;
        if (!Chat->IsShowWindow() && ChatList())
        {
            NStrArg Listed = NMsnStrCopy(Name);
            void* Item = ChatList()->FindItem(Listed.W[0], Listed.W[1], Listed.W[2]);
            if (Item)
                nw::Field<INT>(Item, 0xF8) = 1;
        }
    }
    NStrArg NameArg = NMsnStrCopy(Name);
    NStrArg TextArg = NMsnStrCopy(Text);
    ((NMsnChatWnd*)Chat)->OnCodeStr(A, NameArg.W[0], NameArg.W[1], NameArg.W[2], TextArg.W[0], TextArg.W[1], TextArg.W[2]);
    Name.~FString();
    Text.~FString();
}
NW_HOOK_METHOD(0x1015d8d0, NMsnMainPanel, NewChat)


// 0x10158b10 - the friend (a copy, which the list destroys) into the list, the list sorted, the friends window's tree made
// again; 0x10133ef0 the same with the friend taken out of the list; 0x10133f70 with its values updated
void NMsnMainPanel::AddFriend(NL2FriendArg Arg)
{
    NL2FriendArg Copy;
    NMsnFriendCopy(Copy, NMsnFriendOf(Arg));
    List()->AddOrUpdate(Copy);
    List()->Sort();
    FriendsWnd()->Refresh();
    NMsnFriendOf(Arg).Name.~FString();
}
NW_HOOK_METHOD(0x10158b10, NMsnMainPanel, AddFriend)

void NMsnMainPanel::RemoveFriend(NL2FriendArg Arg)
{
    NL2FriendArg Copy;
    NMsnFriendCopy(Copy, NMsnFriendOf(Arg));
    List()->Remove(Copy);
    List()->Sort();
    FriendsWnd()->Refresh();
    NMsnFriendOf(Arg).Name.~FString();
}
NW_HOOK_METHOD(0x10158b90, NMsnMainPanel, RemoveFriend)

void NMsnMainPanel::UpdateFriend(NL2FriendArg Arg)
{
    NL2FriendArg Copy;
    NMsnFriendCopy(Copy, NMsnFriendOf(Arg));
    List()->Update(Copy);
    List()->Sort();
    FriendsWnd()->Refresh();
    NMsnFriendOf(Arg).Name.~FString();
}
NW_HOOK_METHOD(0x10158c10, NMsnMainPanel, UpdateFriend)

// 0x10156090 - the friends window shows the panel's list, sorted, and builds its tree (a tail jump into Rebuild)
void NMsnMainPanel::ReloadFriends()
{
    nw::Field<NMsnFriendList*>(FriendsWnd(), 0x134) = List();
    List()->Sort();
    FriendsWnd()->Rebuild();
}
NW_HOOK_METHOD(0x10156090, NMsnMainPanel, ReloadFriends)

// the object at +0xC8 of the shell's child at +0xC8, that holds a name: its text is given through 0x1012A700 (ret 4, the string by
// the hidden pointer; not ported here)
struct NMsnNameHolder
{
    FString* GetText(FString* Result);
};
FString* NMsnNameHolder::GetText(FString* Result)
{
    ::new ((void*)Result) FString(((Msn*)this)->GetStatus());   // 0x1014F1D0 (ret 4, the string by the hidden pointer)
    return Result;
}


// 0x101592c0 / 0x10134680 / 0x101346f0 - the friend (a copy, which the panel destroys) to the main panel
void NMsnShellWnd::FriendAdd(NL2FriendArg Arg)
{
    NL2FriendArg Copy;
    NMsnFriendCopy(Copy, NMsnFriendOf(Arg));
    MainPanel()->AddFriend(Copy);
    NMsnFriendOf(Arg).Name.~FString();
}
NW_HOOK_METHOD(0x101592c0, NMsnShellWnd, FriendAdd)

void NMsnShellWnd::FriendRemove(NL2FriendArg Arg)
{
    NL2FriendArg Copy;
    NMsnFriendCopy(Copy, NMsnFriendOf(Arg));
    MainPanel()->RemoveFriend(Copy);
    NMsnFriendOf(Arg).Name.~FString();
}
NW_HOOK_METHOD(0x10159330, NMsnShellWnd, FriendRemove)

void NMsnShellWnd::FriendUpdate(NL2FriendArg Arg)
{
    NL2FriendArg Copy;
    NMsnFriendCopy(Copy, NMsnFriendOf(Arg));
    MainPanel()->UpdateFriend(Copy);
    NMsnFriendOf(Arg).Name.~FString();
}
NW_HOOK_METHOD(0x101593a0, NMsnShellWnd, FriendUpdate)

// 0x10159280 (ret 4; the string through the hidden pointer, which it returns) - the name the shell's first child holds: its
// holder's text, empty when there is no holder (NCChatWnd::OnPaint draws it)
FString* NMsnShellWnd::GetPeerName(FString* Result)
{
    NMsnNameHolder* Holder = nw::Field<NMsnNameHolder*>(nw::Field<void*>(this, 0xE8), 0xE8);
    if (Holder)
    {
        Holder->GetText(Result);
        return Result;
    }
    ::new ((void*)Result) FString(TEXT(""));
    return Result;
}
NW_HOOK_METHOD(0x10159280, NMsnShellWnd, GetPeerName)

// 0x101561a0 - the main panel's ReloadFriends (a tail jump); 0x10139670 its 0x10139370; 0x10139680 its 0x10139230 with the
// three arguments
void NMsnShellWnd::ReloadFriends()
{
    MainPanel()->ReloadFriends();
}
NW_HOOK_METHOD(0x101561a0, NMsnShellWnd, ReloadFriends)

void NMsnShellWnd::PanelCall139370()
{
    MainPanel()->CloseAll();
}
NW_HOOK_METHOD(0x1015dfe0, NMsnShellWnd, PanelCall139370)


void NMsnShellWnd::PanelChat(INT A, TCHAR* Name, TCHAR* Text)
{
    MainPanel()->OnChat(A, Name, Text);
}
NW_HOOK_METHOD(0x1015dff0, NMsnShellWnd, PanelChat)
