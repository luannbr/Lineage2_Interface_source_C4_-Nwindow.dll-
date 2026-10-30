#pragma once
// NConsoleFns.h - stage 5 (sa-verify): the ONE definition of the console's small anonymous methods that the stock keeps out of line and
// that many windows call (names ours; the bodies are NLeftHelpers.cpp's and NTemplates.cpp's, hooked at the C4 addresses below).
//
// They used to be declared by hand, with the members each lane's files called, in NConsoleWndExt.h, NWinBStock.h, NLeftHelpers.h,
// MsnUiStock.h and in two .cpp files (the same struct name five times, so that no two of those headers could meet in one file and
// tools/port/vt_audit.py had to skip two of them). The members are the union of what any file calls. The structs add no data and no
// virtuals, so they are not classes: `this` is the console (or is not read at all by the stock bodies), and the windows reach the
// methods through a cast of GConsole:
//     ((NConsoleLeftFns*)GConsole)->GetClassGroup(ClassID)
// A member added here must be defined (and hooked at its stock address) in the file named in its comment.
//
// This header is neutral: it needs only the console's class (NConsoleWnd.h), so the messenger's neutral stand-ins (MsnUiStock.h), the
// console's views (NConsoleWndExt.h) and the windows' files can all include it.

#include "nwindow/NConsoleWnd.h"
#include "nwindow/NCIcon.h"

class L2ParamStack;

// NLeftHelpers.cpp: the console's small functions nobody named
struct NConsoleLeftFns : public NConsoleWnd
{
    INT GetClassGroup(INT ClassID);                                  // 0x1006b970 (ret 4; `this` is not read)
    const TCHAR* GetPartyRoomAreaName(INT Area);                     // 0x1006dfa0 (ret 4; `this` is not read)
    const TCHAR* GetLootRuleName(INT Rule);                          // 0x1006e140 (ret 4; `this` is not read)
    BYTE NpcShowsBars(INT NpcID);                                    // 0x1007d850 (ret 4)
    INT GetEtcItemStackKind(INT ItemID);                             // 0x1007c360 (ret 4)
    UBOOL IsDwarf();                                                 // 0x100753d0
    INT GetTrackedQuestID();                                         // 0x100b90f0 (the desktop's quest tab's tracked quest)
    INT GetEnchantedPAtk(INT WeaponType, INT BodyPart, INT CrystalGrade, INT Enchant, INT BaseAtk);   // 0x100741b0 (ret 0x14)
    INT GetEnchantedMAtk(INT WeaponType, INT BodyPart, INT CrystalGrade, INT Enchant, INT BaseAtk);   // 0x10074250 (ret 0x14)
    void RemovePledgeCrest();                                        // 0x10068490
};

// NTemplates.cpp: the party matching requests (and the hand-over of the party master): the engine's network handler gets the parameters
// (slots 278 .. 284)
struct NConsoleForwardFns : public NConsoleWnd
{
    void RequestListPartyWating(L2ParamStack& Params);               // 0x1006df40 (ret 4)
    void RequestManagePartyRoom(L2ParamStack& Params);               // 0x1006df50 (ret 4)
    void RequestJoinPartyRoom(L2ParamStack& Params);                 // 0x1006df60 (ret 4)
    void RequestOustFromPartyRoom(L2ParamStack& Params);             // 0x1006df70 (ret 4)
    void RequestDismissPartyRoom(L2ParamStack& Params);              // 0x1006df80 (ret 4)
    void RequestWithdrawPartyRoom(L2ParamStack& Params);             // 0x1006df90 (ret 4)
    void RequestHandOverPartyMaster(L2ParamStack& Params);           // 0x1006db80 (ret 4)
};

// The console's windows whose bodies the console lane wrote on a function-holder struct (NConsoleWndMisc.cpp / NConsoleWndAnon.cpp hook the
// addresses) and that other windows' files forward to: `this` is the window (or the console). Declared here once, for both sides.
struct NItemInfo;
struct NVIPNeededItem;

// NCPartyRoomWnd.cpp: the icon of a minimized window starts to flash (sub_1001DDB0, name ours; the stock code calls it with the window's Icon):
// when the icon is shown, a flag at +0x220 and a repeating timer (500 ms, id 0xFFF001, kept at +0x228) are set. NCIcon's header is not
// the party room's, so the method is declared on a view of it
struct NCIconFlashView : public NCIcon
{
    __declspec(noinline) void StartFlash();                          // 0x1001ddb0
};

// NConsoleWndMisc.cpp: the desktop tells the messenger window that the player leaves; the chat window toggles
struct NCConsoleExt
{
    __declspec(noinline) INT LogoutMessenger();                               // 0x1005bc40
    __declspec(noinline) void ToggleChatWnd();                                // 0x1005bc50
};

// NConsoleWndMisc.cpp: the package window's grids (NCDeliverWnd.cpp forwards NCDeliverWnd::AddInventoryItem / NCDeliverItemWnd::AddItem here)
struct NCDeliverWndExt
{
    __declspec(noinline) UBOOL AddInventoryItem(NItemInfo* Info);             // 0x100859f0 (ret 4)
};
struct NCDeliverItemWndExt
{
    __declspec(noinline) UBOOL AddItem(NItemInfo* Info);                      // 0x100856b0 (ret 4)
};

// NConsoleWndAnon.cpp: the multisell window (the VIP shop of the headers; NCVIPShopWnd.cpp forwards to it)
struct NCVIPShopWndExt
{
    __declspec(noinline) void Refresh();                                // 0x100a2ab0 (the list built again)
    __declspec(noinline) void AddNeededItem(INT ProductID, const NVIPNeededItem* Src);   // 0x100a2b40 (ret 8)
    __declspec(noinline) void AddRecord(INT Slot, INT List, const NItemInfo* InfoRaw);   // 0x100a3210 (ret 0xC)
    __declspec(noinline) void Open(INT ListID);                         // 0x100a4020 (ret 4)
};

// NTemplates.cpp: FL2SystemMsgData (the engine's record of systemmsg-*.dat, 0x30 bytes): the out-of-line copy constructor and destructor of its
// three strings (+4, +0x18, +0x24; the header of include/ue2 knows only two of them)
struct NSystemMsgFns : public FL2SystemMsgData
{
    FL2SystemMsgData* CopyFrom(const FL2SystemMsgData* Src);         // 0x10069eb0 (the copy constructor, ret 4)
    void DestroyStrings();                                           // 0x10069f30 (the destructor)
};
