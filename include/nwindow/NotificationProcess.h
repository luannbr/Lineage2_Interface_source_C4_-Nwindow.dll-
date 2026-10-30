#pragma once
// NotificationProcess: the connection to the notification server (names ours: the stock has no string for the class;
// vtable 0x101AB190, 0x98 bytes, made by Msn::StartNotification with the constructor 0x10140890). After the same VER / CVR /
// USR TWN I steps as the dispatch connection it logs in to the Passport (USR TWN S, with the ticket the Passport server gives for
// the challenge), asks for the lists (SYN) and then follows the contact traffic: ILN / ADD / REM / LST / ADG / RMG / REG / CHG /
// REA / CHL and the switchboard requests (RNG, XFR SB).
//
// Request types kept in Pending (RegisterPending): 1 VER, 2 CVR, 3 USR TWN I (dispatch), 4 USR TWN I (notification), 5 USR TWN S,
// 6 SYN; the user's commands (sent from the windows' functions of the other lanes) use the rest. The answer handlers below get
// the line by value (ret 0x3C) and are called from the vtable slot 5 (0x10140DF0, core lane's) by request type.

#include "nwindow/MSNSocket.h"
#include "nwindow/FriendList.h"

#include "nwindow/Msn2Pending.h"

// the login: the account and the password, and the HTTP exchange with the Passport (names ours; 0x18 bytes, made by 0x101413A0)
struct NMsnPassport
{
    FString Account;                                         // +0x00
    FString Password;                                        // +0x0C

    NMsnPassport(const FString& InAccount, const FString& InPassword);   // 0x10165DC0 (ret 0x18: both strings by value)
    const TCHAR* FetchChallenge(const TCHAR* Url);          // 0x101418A0 (ret 4, core lane's): the login server's URL for the nexus
    const TCHAR* Login(const TCHAR* Challenge, const TCHAR* Argument);   // 0x10141480 (ret 8, core lane's): the ticket for the challenge, NULL when it failed
    __declspec(noinline) ~NMsnPassport();                    // 0x1013C270 (the two strings in reverse; out of line: NotificationProcess::Destroy calls it)
};
NW_ASSERT_SIZE(NMsnPassport, 0x18);

// UE2's TMap<INT, FString>: pairs {chain, key, string} (name ours; 0x14 bytes)
struct NMsnIntStrPair
{
    INT HashNext;                // +0x00
    INT Key;                     // +0x04
    FString Value;               // +0x08
};
NW_ASSERT_SIZE(NMsnIntStrPair, 0x14);

struct NMsnIntStrMap
{
    TArray<NMsnIntStrPair> Pairs;   // +0x00
    INT* Hash;                      // +0x0C
    INT HashCount;                  // +0x10

    __declspec(noinline) FString FindRef(const INT& Key);    // 0x1013E1C0 (ret 8: the hidden result, the key): the string under the key, an empty one when there is none
    __declspec(noinline) void Rehash();                      // 0x1013E230
    void RemovePairs(INT Index, INT Count);                  // 0x101633C0 (ret 8): the pairs' array Remove with the strings destroyed
    __declspec(noinline) void FreePairs();                   // 0x101649F0: the pairs removed (the strings destroyed), then the array itself (MsnLogin.cpp)
    void Free();                                             // 0x10165240: the hash freed and forgotten, then the pairs (MsnLogin.cpp)
};
NW_ASSERT_SIZE(NMsnIntStrMap, 0x14);

class NotificationProcess : public MSNSocket
{
public:
    FString Unknown64;                   // +0x64
    NMsnPassport* Passport;              // +0x70 the login being made (heap, 0x18 bytes)
    FString Status;                      // +0x74 the user's presence status as the server last said it (CHG); the constructor writes ""
    INT SyncSerial;                      // +0x80 the version of the contact lists (the serial the SYN / ADD / REM answers carry)
    NMsnIntStrMap Names;                 // +0x84 (rehashed by the constructor)

    // Hook build: the stock constructor's body (0x101652B0, ret 0x18: the base, the stock vtable 0x101DD898 written by hand, the strings and
    // the map). Standalone build: the real constructor (the compiler writes the vtable, whose slots are the overrides below)
    NotificationProcess* ConstructNS(Msn* InOwner, NMsnServer InServer, INT InTrID);
#if !NW_HOOK_MODE
    NotificationProcess(Msn* InOwner, const NMsnServer& InServer, INT InTrID);
#endif
    FString GetStatus();                         // 0x10160E70 (ret 4; name ours): a copy of Status

    // The destructor's body (the object has the stock vtable from the stock constructor; a C++ destructor here would store ours)
    void Destroy();                              // 0x10165390 (name ours): the vtable, the login object freed, the map, the strings, then MSNSocket::DestroyBase

    // the vtable's overrides (the stock's slots 0x101DD898: 0 = SendVER, 1 OnClosedImpl 0x10160CE0, 2 OnCommandImpl 0x10161110, 3 OnAsyncImpl
    // 0x10165440, 5 OnReplyImpl 0x10165810, 6 OnMessageImpl 0x101619E0; 4 and 7 .. 10 are the base's)
    virtual void OnConnected();
    virtual void OnClosed();
    virtual void OnCommand(MsnLine Line);
    virtual void OnAsync(MsnLine Line);
    virtual void OnReply(INT RequestType, MsnLine Line);
    virtual void OnMessage(NMsnMessage* Message);

    // the commands of the notification connection the windows send (MsnLogin.cpp; the strings by value, freed by the callee)
    __declspec(noinline) void SendSignOut();                          // 0x10161DF0: "OUT"
    __declspec(noinline) void SendStatus(FString Code);               // 0x10163EC0 (ret 0xC): the status code is kept and "CHG trid code" sent (request type 7)
    __declspec(noinline) void SendDisplayName(FString Name);          // 0x10163F90 (ret 0xC): "REA trid account name" (request type 8)
    __declspec(noinline) void SendBlock(FString Mail, INT bBlock);    // 0x10164290 (ret 0x10): the contact moved between the allow and the block list
    __declspec(noinline) void HandleSYNStart(MsnLine Line);           // 0x10164D60 (ret 0x3C): the answer to request type 6 ("SYN trid serial")

    // vtable slots (0 is MSNSocket::SendVER, 4 and 8-10 are the base's)
    void OnClosedImpl();                         // 0x1013C2C0 (slot 1)
    void OnAsyncImpl(MsnLine Line);              // 0x10140A20 (slot 3, ret 0x3C): the base's MSG handler, then NLN / FLN / RNG / LSG / LST / OUT
    // NOTE the stock order: the request type is the FIRST argument, the line the second (the stock caller pushes the type last)
    void OnReplyImpl(INT RequestType, MsnLine Line);   // 0x10140DF0 (slot 5, ret 0x40): the answer to the request of that type, below
    void OnCommandImpl(MsnLine Line);            // 0x1013C6F0 (slot 2, ret 0x3C): ILN / ADD / REM / CHL of the server
    void OnMessageImpl(NMsnMessage* Message);    // 0x1013CFC0 (slot 6, ret 4): a MSG payload (the unread mail count, a new mail)

    // answers by request type (called by the slot 5 at 0x10140DF0)
    void HandleUSRReply(MsnLine Line);           // 0x1013F0B0 (ret 0x3C): the answer to USR TWN I: an XFR to another server or the login
    void HandleLoginReply(MsnLine Line);         // 0x1013F300 (ret 0x3C): the answer to USR TWN S: SYN, and the user's record
    void HandleSYNReply(MsnLine Line);           // 0x1013C480 (ret 0x3C): the serial of the lists
    void HandleCHG(MsnLine Line);                // 0x1013D120 (ret 0x3C)
    void HandleREA(MsnLine Line);                // 0x1013D270 (ret 0x3C)
    void HandleADD(MsnLine Line);                // 0x1013D450 (ret 0x3C)
    void HandleMoveReply(MsnLine Line);          // 0x1013DA30 (ret 0x3C): the answer with the group (five arguments or more): the contact's group changes
    void HandleREM(MsnLine Line);                // 0x1013D6C0 (ret 0x3C)
    void HandleAsyncListChange(MsnLine Line);    // 0x1013D7F0 (ret 0x3C): ADD / REM of the lists without a transaction
    void HandleADG(MsnLine Line);                // 0x1013DC20 (ret 0x3C)
    void HandleRMG(MsnLine Line);                // 0x1013DDA0 (ret 0x3C)
    void HandleREG(MsnLine Line);                // 0x1013DE40 (ret 0x3C)

    // unprompted lines (called by the slot 3 at 0x10140A20)
    void HandleLST(MsnLine Line);                // 0x1013E2C0 (ret 0x3C): "LST account name lists [group]": the contact in each list the bits name
    void HandleRNG(MsnLine Line);                // 0x1013DF70 (ret 0x3C): "RNG session host:port CKI cookie ..": an invitation: a session that will answer (ANS)
    void HandleXFRSB(MsnLine Line);              // 0x1013E670 (ret 0x3C): "XFR trid SB host:port CKI cookie": the switchboard for a chat the user asked for
    void HandleAsyncGroup(MsnLine Line);         // 0x1013C2F0 (ret 0x3C): "ADG group name": a group entered in the group list

    // one line of the LST answer for a list (FriendList::UpdateFriend with the contact)
    void ApplyList(FriendList* List, FString Account, NMsnFriend Friend, INT GroupID);   // 0x1013D1B0 (ret 0x58)
};
NW_ASSERT_OFFSET(NotificationProcess, Passport, 0x70);
NW_ASSERT_OFFSET(NotificationProcess, Status, 0x74);
NW_ASSERT_OFFSET(NotificationProcess, SyncSerial, 0x80);
NW_ASSERT_OFFSET(NotificationProcess, Names, 0x84);
NW_ASSERT_SIZE(NotificationProcess, 0x98);
