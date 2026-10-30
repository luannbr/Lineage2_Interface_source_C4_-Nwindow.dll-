#pragma once
/*=============================================================================
    NConsoleWndExt.h - the function-holder views of the console's files (NCConsole.cpp, NConsoleWndMisc.cpp,
    NConsoleWndAnon.cpp, NConsoleWndEvents.cpp).

    Each struct below carries functions of the stock image that no class header declares (anonymous in the db, names
    ours): `this` is the window (or the object) the stock function works on, the member is hooked at the stock address
    (NW_HOOK_METHOD in the .cpp that defines it). They are declared here, not in the .cpp, because the callers sit in
    other files (stage 5: a typed call replaces the stock call by address, and the standalone build has no other way to
    name them).

    These are NOT classes: never create one of these objects, only cast a window pointer to it for one call.
=============================================================================*/

#include "nwindow/NCWnd.h"
#include "nwindow/NConsoleWnd.h"
#include "nwindow/NConsoleFns.h"   // NConsoleLeftFns / NConsoleForwardFns (stage 5, sa-verify: one definition for all the headers)
#include "nwindow/NCIcon.h"
#include "nwindow/MsnWnd.h"
#include "nwindow/FriendList.h"
#include "nwindow/L2TaskWnd.h"
#include "nwindow/NCListBox.h"
#include "nwindow/MsnUiStock.h"    // NMsnShellWnd, NL2FriendArg, NChatTaskListBox (declared once there, stage 5 sa-verify)
#include "nwindow/MsnProtoOps.h"   // NMsnStrMapOps
#include "nwindow/MsnWndOps.h"     // NMsnWndStrPairInit, NChatL2TaskWnd
#include "nwindow/NCQuestGaraTreeWnd.h"
#include "nwindow/NCQuestGaraTreeCtrl.h"
#include "nwindow/NCGaraTVItem.h"
#include "nwindow/NCVIPShopWnd.h"
#include "nwindow/NCVIPShopItemListBox.h"
#include "nwindow/NCZoneTitleWnd.h"

struct FAbnormalStatus;
struct NItemInfo;
struct NViewItemData;
struct NMsnMainPanel;
struct NPledgeInfoExt;
struct NTrainEntryExt;

/*-----------------------------------------------------------------------------
    Defined in NConsoleWndMisc.cpp.
-----------------------------------------------------------------------------*/

// NCAbnormalStatusWnd: both lists and the extra icon forgotten (NCConsole::Reset). The window's other small functions (the second list's
// AddStatus2, SetExtraStatus, ClearStatus2, ClearExtraStatus) are its own methods (NCAbnormalStatusWnd.h)
struct NCAbnormalStatusWndExt
{
    __declspec(noinline) void ClearAll();                                // 0x10116a80
};

// the window at +0x170 of the desktop loses its child's flag (+0x1B4); this window's two flag bytes (+0xFC, +0xFD) are cleared
// (defined in NConsoleWndAnon.cpp)
struct NCFlagWndExt
{
    __declspec(noinline) void ClearFlags();                              // 0x100ec100
};

// The messenger window (CMessengerWnd, 0xD8 bytes; include/nwindow/MessengerWnd.h is another lane's): the MSN page (+0xE8)
// and the Lineage II friends page (+0xEC, the main panel above)
struct NMsnMessengerView : public NCWnd
{
    __declspec(noinline) void HideAll();   // 0x1015dfc0 (name ours)
    __declspec(noinline) INT SignOut();    // 0x10160550 (name ours)
};

// The quest tree window (the main window's quest tab and the GM's) and its tree control, with the functions the db has as anonymous
// (their headers belong to other lanes: views of the same layout with the members added; the hooks are installed on the members so
// that the stock address is entered with `this` in ecx)
struct NCQuestGaraTreeWndExt : public NCQuestGaraTreeWnd
{
    __declspec(noinline) void MarkSelectedQuest();                // 0x100eb840 (name ours)
    __declspec(noinline) FString GetTargetName();                 // 0x100EA340
    __declspec(noinline) FVector GetTargetPos();                  // 0x100EA370
    __declspec(noinline) void SetTargetPos(INT X, INT Y, INT Z);  // 0x100ea1b0
    __declspec(noinline) void ResetTree();                        // 0x100eb820
    void AddQuest(INT QuestID, INT Step);                         // 0x100ebf90 (ret 8)
};
struct NCQuestGaraTreeCtrlExt : public NCQuestGaraTreeCtrl
{
    NCGaraTVItem* AddQuestStep(INT QuestID, INT Step, NCGaraTVItem* Node, INT bMore);   // 0x100ebbe0 (ret 0x10)
};

/*-----------------------------------------------------------------------------
    Wave 3: the small anonymous methods that the console's functions and the shortcut keys call. Each is a member of a
    window class that is another lane's (or has no header). Defined in NConsoleWndMisc.cpp.
-----------------------------------------------------------------------------*/

struct NControlExt
{
    __declspec(noinline) void EndMouseDrag();                                 // 0x100116b0
};
struct NPledgeInfoExt
{
    __declspec(noinline) NPledgeInfoExt* Clear();                             // 0x1004ce80
};
// (NCConsoleExt, NCDeliverWndExt, NCDeliverItemWndExt, NCVIPShopWndExt: NConsoleFns.h)
struct NCVideoOptionWndExt
{
    __declspec(noinline) void ToggleLowerDetail();                            // 0x100c6d00
};
struct NCGMQuestWndExt
{
    __declspec(noinline) void AddQuestItem(INT ItemID, INT Count);            // 0x1008da50
};
struct NCGodWndExt
{
    __declspec(noinline) INT GetFlag();                                       // 0x10090ef0
};
struct NCLobbyWndExt
{
    __declspec(noinline) void ClearSlots();                                   // 0x1009b6d0
};
struct NCRadarWndExt
{
    __declspec(noinline) void RemoveTarget(INT X, INT Y, INT Z);              // 0x100ef970
};
struct NCRecipeBookWndExt
{
    __declspec(noinline) void SetField16C(INT Value);                         // 0x100f3760
};
struct NCLoginServerWndExt
{
    __declspec(noinline) void SetKey(const void* Key);                        // 0x10100370
};
struct NCZoneTitleWndExt
{
    __declspec(noinline) void SetTexts(const TCHAR* Text1, const TCHAR* Text2, const TCHAR* Text3);   // 0x101211f0
};
struct NCBroadcastObserverWndExt2
{
    __declspec(noinline) INT ToggleFlag();                                    // 0x1012e290
};
struct NCQuestTreeWndExt
{
    __declspec(noinline) void MakeRoot();                                     // 0x100DB380 (C3 address)
};
struct NCSSQEventTreeWndExt
{
    __declspec(noinline) void MakeRoot();                                     // 0x100fdab0
};
struct NCSSQListTreeWndExt
{
    __declspec(noinline) void MakeRoot();                                     // 0x100ff4c0
};

/*-----------------------------------------------------------------------------
    Defined in NConsoleWndAnon.cpp.
-----------------------------------------------------------------------------*/

// the multisell window (the VIP shop of the headers) and its list child
struct NCVIPShopListExt
{
    __declspec(noinline) UBOOL AddEntry(NItemInfo* Info);               // 0x100a22b0 (ret 4)
};

// the party matching list's rows (a fixed table of children at +0x100, their count at +0xF4; each has the room's id at +0x364)
struct NCPartyRowExt
{
    __declspec(noinline) void ReadInfo(L2ParamStack& Params);                // 0x101104e0 (ret 4)
};
struct NCPartyRowListExt
{
    __declspec(noinline) void UpdateRow(INT RoomID, L2ParamStack& Params);   // 0x101107c0 (ret 8)
};

// the command channel window (the desktop's +0x264) keeps its list in the child at +0xF0
struct NCChannelListExt
{
    __declspec(noinline) void ReadInfo(L2ParamStack& Params);                // 0x100511e0 (ret 4)
};
struct NCChannelWndExt
{
    __declspec(noinline) void SetInfo(L2ParamStack& Params);                 // 0x10051420 (ret 4)
    __declspec(noinline) void AddMember(L2ParamStack& Params);               // 0x10051c00 (ret 4)
};
// the channel window's list child (its list is at +0xEC): a row of two cells, the name and a number
struct NCChannelRowsExt
{
    __declspec(noinline) void AddRow(L2ParamStack& Params);                  // 0x100517f0 (ret 4)
};

// (NMsnStrMapOps, the messenger's string map operations defined in NConsoleWndAnon.cpp, is declared once in MsnProtoOps.h)

// the trainer's entry (NTrainEntryC4, 0x14A8 bytes)
struct NTrainEntryExt
{
    __declspec(noinline) NTrainEntryExt* Construct();                        // 0x1006e8c0
};

/*-----------------------------------------------------------------------------
    More views defined in NConsoleWndAnon.cpp, NConsoleWndMisc.cpp and NConsoleWndEvents.cpp.
-----------------------------------------------------------------------------*/

// NConsoleWndAnon.cpp
// 0x101514a0 (ret 4) - the item table's search by the key the caller points at: the record under it, NULL when there is none
struct NItemMapByKey
{
    __declspec(noinline) NViewItemData* FindByKey(const INT* Key);
};
// 0x100ea530 (name ours) - the list child (+0xE8) emptied (a tail call)
struct NCListOwnerExt
{
    __declspec(noinline) void ClearList();
};
// 0x101029e0 / 0x100e3370 (ret 4; names ours) - a window's number at +0x118 / +0x11C
struct NCNumber118Ext
{
    __declspec(noinline) void Set(INT Value);
};
struct NCNumber11CExt
{
    __declspec(noinline) void Set(INT Value);
};
// 0x10117d40 (ret 4; name ours) - the mode of a window (+0x11C) and the system string of its title
struct NCModeTitleWndExt
{
    __declspec(noinline) void SetMode(INT Mode);
};
// 0x1011c250 / 0x1011c550 (ret 4; the trainer's windows) - a required item of the skill
struct NCTrainInfoWndExt2
{
    __declspec(noinline) void AddRequiredItem(NItemInfo* InfoRaw);
};
struct NCTrainWndExt2
{
    __declspec(noinline) void AddRequiredItem(NItemInfo* InfoRaw);
};
// 0x100e7600 (ret 4) - a store window with two states (+0x11C)
struct NCStoreModeWndExt
{
    __declspec(noinline) void SetFlag(BYTE bFlag);
};
// 0x10099210 (name ours) - the enchant window's list of items to choose from rebuilt
struct NCEnchantWndExt
{
    __declspec(noinline) void RebuildItems();
};
// 0x100bcd30 (ret 4; name ours) - the 0x80-byte elements of an array (each unmade by ~NToolTipInfo) taken out and the array given Slack room
struct NRaw80ArrayExt : public FArray
{
    __declspec(noinline) void EmptyElements(INT Slack);
};
// the icon of a party member's class (a stdcall function, ret 4; name ours; NConsoleWndAnon.cpp): by the class's group, "party_styleicon..."
const TCHAR* __stdcall GetPartyStyleIconName(INT ClassID);             // 0x10074c70

// NConsoleWndMisc.cpp: the zone title's stand-in for the method the stock keeps at 0x1010b4a0 (the console's Tick calls it by address)
struct NCZoneTitleWndTick : public NCZoneTitleWnd
{
    __declspec(noinline) void Tick(FLOAT DeltaTime);   // 0x1010B4A0 (ret 4)
};

// (NL2FriendArg and NMsnShellWnd, the messenger window (the desktop's +0x18C) as the console reaches it, are defined in MsnUiStock.h)

// NConsoleWndEvents.cpp: the tutorial's question mark (the desktop's +0x17C; no header): a window of its own with its question number and three timers
struct NTutorialMarkWnd : public NCWnd
{
    __declspec(noinline) void StartTimers();           // 0x10002210 (name ours)
    __declspec(noinline) void SetQuestion(INT ID);     // 0x10002280 (name ours)
};

/*-----------------------------------------------------------------------------
    Defined in other lanes' .cpp files (a local struct there, no header): the same declaration as theirs, so that the
    typed call links to their definition (the member's decorated name is the same). Do not change the members.
-----------------------------------------------------------------------------*/

// (NSystemMsgFns, NConsoleLeftFns and NConsoleForwardFns, the console's small functions nobody named, are defined in NConsoleFns.h)

// (NCIconFlashView, the icon of a minimized window starts to flash (NCPartyRoomWnd.cpp), is declared in NConsoleFns.h)

// (NMsnWndStrPairInit, NChatL2TaskWnd: MsnWndOps.h; NChatTaskListBox: MsnUiStock.h)

// (NL2Book, the friend book of L2FriendWnd.cpp, is defined in MsnUiStock.h)

// NConsoleWndC4.cpp (defined there; declared here for NConsoleWndInput.cpp's tick): the window at the console's +0x3C08 (class
// 0x103103A0, constructor 0x10120D10; it shows a line of text with a number that blinks and fades), and the Olympiad windows the
// desktop keeps at +0x214 / +0x218 (the players) and +0x224 (the target)
struct NCTimerNoticeWndExt
{
    __declspec(noinline) void Clear();                                                     // 0x10120020
    __declspec(noinline) void Reset(INT A, INT B, INT bFlag);                              // 0x10120090 (ret 0xC)
    __declspec(noinline) void SetText(INT StringID, INT Number);                           // 0x10120100 (ret 8)
    __declspec(noinline) void SetNumberText(INT MessageID, INT Number, INT Value);         // 0x10120170 (ret 0xC)
    __declspec(noinline) void Flash();                                                     // 0x101201F0
    __declspec(noinline) void Tick(FLOAT DeltaTime);                                       // 0x10120210 (ret 4)
    __declspec(noinline) void RestartTimer();                                              // 0x10120380
    __declspec(noinline) void Update(INT A, INT B, INT bFlash, INT Flag2, INT Kind, INT Extra);   // 0x10120990 (ret 0x18)
};

struct NOlympiadPlayerWndExt
{
    INT GetPlayerID();   // 0x1013D0F0
};

struct NOlympiadTargetWndExt
{
    INT GetSide();           // 0x1013DC30
    void SetSide(INT Side);  // 0x1013DC20 (ret 4)
};


// (NConsoleForwardFns, the console's party requests, is defined in NConsoleFns.h)
