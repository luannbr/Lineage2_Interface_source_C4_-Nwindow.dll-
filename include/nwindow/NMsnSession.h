#pragma once
// NMsnSession - the connection object of the messenger (name ours; it is the notification connection, NotificationProcess, seen by the
// dialogs) as far as the requests of the messenger dialogs need it: each request builds a command of the MSN protocol (a name such as
// "ADD" or "REM", the transaction number at +0x4C and the parameters), registers the transaction and sends the command.
//
// The bodies are in NMsnSession.cpp, which sees the protocol headers; this header is included next to the dialogs' own records
// (NMsnGroup.h, NMsnBuddy.h) and so declares only what they need.

#include "nwindow/NMsnBuddy.h"
#pragma pack(push, 4)

struct NMsnCommand;   // the command the requests build (NMsnCommand.h: MsnCommand's local form)

struct NMsnSession
{
    BYTE Unknown00[0x4C];
    INT TransactionId;               // +0x4C the number of the command (every command carries it)

    // names ours: the commands the dialogs send. The stock functions take their texts and the record by value (the callee
    // destroys them). Out of line (NMsnSession.cpp): the dialogs' link (NMsnLink.cpp) calls them
    void AddFriend(FString Mail, INT bAllow);                         // 0x101640C0 (ret 0x10) "ADD FL|BL mail mail", "ADD AL|RL mail mail"
    void RemoveFriend(FString Mail, INT bBlock);                      // 0x10164E20 (ret 0x10) "REM FL mail", and with bBlock the block list
    void MoveFriend(NMsnBuddy Buddy, INT FromGroup, INT ToGroup);     // 0x10164480 (ret 0x4C) "ADD FL mail name group", "REM FL mail group"
    void AddGroup(FString Name);                                      // 0x10164640 (ret 0xC) "ADG name 0"
    __declspec(noinline) void RemoveGroup(INT GroupId);               // 0x10164750 (ret 4) "RMG group"
    void RenameGroup(INT GroupId, FString Name);                      // 0x101647F0 (ret 0x10) "REG group name 0"

    // the connection's own functions the commands use (MSNSocket / NotificationProcess, NMsnSession.cpp calls them on the same object)
    void RegisterTransaction(INT Code);                               // 0x10163460 (ret 4) the pending transaction (this number, Code) noted in the table at +0x50
    void SendCommand(const NMsnCommand& Command);                     // 0x101607E0 (ret 0x1C) a copy of the command goes by value; the callee releases it
    void BlockFriend(const FString& Mail, INT bBlock);                // 0x10164290 (ret 0x10)
};
NW_ASSERT_OFFSET(NMsnSession, TransactionId, 0x4C);

#pragma pack(pop)
