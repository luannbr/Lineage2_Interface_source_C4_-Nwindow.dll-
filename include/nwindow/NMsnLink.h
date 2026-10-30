#pragma once
// NMsnLink - the part of the messenger's protocol object (stock class "Msn", 0x58 bytes, made by MsnWnd::OnCreate and kept
// at MsnWnd +0xC8; the msn1 lane owns the class) that the messenger dialogs talk to (C3 only; name ours). It holds the
// session (the connection to the server) at +0x0C, and the data store (the groups and the contacts) at +0x50; its small
// request functions hand a command to the session, which writes it on the wire ("ADD", "REM", "ADG", "RMG", "REG"
// lines of the MSN protocol), and do nothing while there is no session.
//
// The dialogs call it with texts (an FString by value, which the callee destroys) and with the 0x44-byte contact record
// (NMsnBuddy, by value). The bodies are in NMsnLink.cpp.

#include "nwindow/NMsnBuddy.h"
#include "nwindow/NMsnGroup.h"
#include "nwindow/NMsnSession.h"
#pragma pack(push, 4)

// The data store at Msn +0x50 (name ours; the msn1 lane's class): +0x10 holds the pointer to the array of groups
struct NMsnStore
{
    BYTE Unknown00[0x10];
    TArray<NMsnGroup*>* Groups;     // +0x10

    __declspec(noinline) TArray<NMsnGroup*>* GetGroups();   // 0x1012C6C0 (name ours; a getter the linker folded with others')
};
NW_ASSERT_OFFSET(NMsnStore, Groups, 0x10);

struct NMsnLink
{
    BYTE Unknown00[0x0C];
    NMsnSession* Session;       // +0x0C
    BYTE Unknown10[0x50 - 0x10];
    NMsnStore* Store;           // +0x50 the data store (0x1014FC10 reads it: a getter the linker folded with others')

    __declspec(noinline) NMsnStore* GetStore();                          // 0x1014FC10 (4 bytes: mov eax, [ecx + 0x50]; hooked in NLeftHelpers.cpp as NFoldedDwordAt50::Get)

    // names ours
    void RequestAddFriend(FString Mail, INT bAllow);                     // 0x1012A7C0 (ret 0x10) the contact on the forward and the allow lists
    void RequestRemoveFriend(FString Mail, INT bBlock);                  // 0x1012A8A0 (ret 0x10) off the lists, on the block list when bBlock
    void RequestMoveFriend(NMsnBuddy Buddy, INT FromGroup, INT ToGroup); // 0x1012A910 (ret 0x4C)
    void RequestAddGroup(FString Name);                                  // 0x1012A990 (ret 0xC)
    void RequestRemoveGroup(INT GroupId);                                // 0x1012AA00 (ret 4)
    void RequestRenameGroup(INT GroupId, FString Name);                  // 0x1012AA10 (ret 0x10)
};
NW_ASSERT_OFFSET(NMsnLink, Session, 0x0C);
NW_ASSERT_OFFSET(NMsnLink, Store, 0x50);

#pragma pack(pop)
