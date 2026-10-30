#pragma once
// The messenger protocol layer's helper types (new header of the sa-msn-proto lane). The stock keeps many small functions of the protocol
// layer out of line (constructors and destructors of the contact lists, the windows' notices of the sessions ...); ours are written as methods
// of types derived from the class they belong to, adding nothing (so that a hook can take the object as `this`). The types used to be
// declared inside the .cpp that defines them; the files that call across (Msn.cpp, MsnLogin.cpp, MsnUtil.cpp, the windows' files) need the
// declarations, so they are here. Every function is the stock one at the address named; the bodies are in MsnData.cpp, MsnUtil.cpp and
// SwitchboardSession.cpp.
//
// MsnWndOps.h has the heavier half (the types derived from the chat, task and alarm windows).

#include "nwindow/Msn.h"
#include "nwindow/FriendList.h"
#include "nwindow/MsnWnd.h"
#include "nwindow/SwitchboardSession.h"

class ChatWnd;

/*-----------------------------------------------------------------------------
    The contact data (MsnData.cpp): the four lists, the groups and the tables they keep.
-----------------------------------------------------------------------------*/

struct NMsnListInit : public FriendList
{
    // (each a function of its own in the stock, so each is kept out of line: the compiler would inline these into their callers otherwise)
    __declspec(noinline) NMsnListInit* Construct(INT ListBit);       // 0x101522D0 (ret 4): no contact, no entry by account, the list's bit
    __declspec(noinline) void DeleteAll();                           // 0x10151FE0: every contact destroyed and freed, the tables emptied
    __declspec(noinline) void Destroy();                             // 0x10152340: the contacts go, then the table by account, then the array
};

struct NMsnGroupsInit : public NMsnGroupList
{
    __declspec(noinline) NMsnGroupsInit* Construct();                // 0x101523A0: no group, an empty table by id
    __declspec(noinline) void DeleteAll();                           // 0x10151CE0: every group destroyed and freed, the tables emptied
    __declspec(noinline) void Destroy();                             // 0x101520F0: the groups go, then the table by id, then the array
};

struct NMsnIntTableInit : public NMsnIntMap
{
    __declspec(noinline) void Free();                                // 0x10152080: the hash freed and forgotten, then the pairs
};

struct NMsnContactsInit : public NMsnContacts
{
    __declspec(noinline) NMsnContactsInit* Construct();              // 0x10152410: the four lists and the group list made
    __declspec(noinline) void Destroy();                             // 0x10152550: each list destroyed, freed and forgotten, then the group list
};

/*-----------------------------------------------------------------------------
    The messenger window's tables of chat windows (MsnWnd.h's NMsnWndStrMap, SwitchboardSession.cpp / MsnUtil.cpp).
-----------------------------------------------------------------------------*/

struct NMsnWndStrMapOps : public NMsnWndStrMap
{
    __declspec(noinline) NCWnd** AddWnd(const TCHAR* InKey, NCWnd* const* InValue);          // 0x10151560 (ret 8): a new pair at the end (hooked in NConsoleWndAnon.cpp as NMsnStrMapOps::Add)
    void MultiFind(const FString* InKey, TArray<NCWnd*>* Out);          // 0x10159410 (ret 8): every window entered under that name is added to the array
    __declspec(noinline) INT RemoveKey(const TCHAR* Key);                                    // 0x10151E60 (ret 4): every pair of that key removed (hooked in NConsoleWndAnon.cpp as NMsnStrMapOps::RemoveKey)
};

// the messenger's string map (UE2's TMap<FString, T*>: NMsnStrMap, FriendList.h): the stock keeps one out-of-line copy of the pair
// insertion and of the removal by key, which the windows' functions call (defined in NConsoleWndAnon.cpp; `this` the map, NMsnWndStrMap or
// NMsnStrMap). Stage 5 (sa-verify): the one definition (it used to be declared in MsnUiStock.h and NConsoleWndExt.h as well)
struct NMsnStrMapOps : public NMsnStrMap
{
    __declspec(noinline) void** Add(const TCHAR* Key, void** Value);         // 0x10151560 (ret 8)
    __declspec(noinline) INT RemoveKey(const TCHAR* Key);                    // 0x10151E60 (ret 4)
};

/*-----------------------------------------------------------------------------
    The session's senders the conversation window uses (ChatWnd.cpp defines them; the session's own are SwitchboardSession.cpp's).
-----------------------------------------------------------------------------*/

struct NChatSbSender : public SwitchboardSession
{
    void SendTyping(NMsnMessageArg Message);    // 0x10152FE0 (ret 0x58)
    void SendText(NMsnMessageArg Message);      // 0x101530D0 (ret 0x58)
    void SendInvite(FString Account);           // 0x101532B0 (ret 0xC)
};

/*-----------------------------------------------------------------------------
    The messenger window (MsnWnd) as the protocol object's notices reach it (SwitchboardSession.cpp).
-----------------------------------------------------------------------------*/

struct NMsnWndOps : public MsnWnd
{
    __declspec(noinline) ChatWnd* FindFreeChat();                                                 // 0x1015B920
    __declspec(noinline) ChatWnd* FindOneToOneChat(FString Account);                              // 0x1015B820 (ret 0xC)
    __declspec(noinline) void OnSessionTyping(SwitchboardSession* Session, NMsnFriend Friend);    // 0x1015AA10 (ret 0x48)
    __declspec(noinline) void ShowTaskWnd();                                                      // 0x1015AA90
    __declspec(noinline) void GiveListsToTree();                                                  // 0x10155BE0
    __declspec(noinline) void OnSessionJoin(SwitchboardSession* Session, NMsnFriend Friend);      // 0x1015B980 (ret 0x48)
    __declspec(noinline) void OnSessionLeave(SwitchboardSession* Session, NMsnFriend Friend);     // 0x1015FE70 (ret 0x48)
    __declspec(noinline) void OnSessionMessage(SwitchboardSession* Session, NMsnFriend Friend, NMsnMessage Message);       // 0x10160580 (ret 0xA0)
    __declspec(noinline) void OnSessionInvite(SwitchboardSession* Session, NMsnFriend Friend, NMsnMessage Message);        // 0x101606B0 (ret 0xA0)
    __declspec(noinline) void OnSessionNewTalk(SwitchboardSession* Session, NMsnFriend Friend, NMsnMessage Message);       // 0x1015FA50 (ret 0xA0)
};
