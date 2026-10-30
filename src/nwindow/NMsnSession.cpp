/*=============================================================================
    NMsnSession.cpp - the commands of the messenger dialogs (C4 addresses). Stock: 0x101640C0 (AddFriend), 0x10164480 (MoveFriend),
    0x10164640 .. 0x10164750 .. 0x101647F0 (AddGroup, RemoveGroup, RenameGroup) and 0x10164E20 (RemoveFriend). Each builds MSN commands (the
    name, the transaction number, the parameters), notes the transaction and sends the command. `this` is the notification connection
    (NotificationProcess): the pieces it uses are MSNSocket's and NotificationProcess's own functions, called on the same object.
=============================================================================*/
#include "nwindow/NMsnSession.h"
#include "nwindow/NMsnCommand.h"
#include "nwindow/NotificationProcess.h"
#include "nwindow/MsnUtil.h"

// 0x10168c90 (cdecl, MsnUtil.cpp) - Url escaped into Out (a buffer of 260 characters): UrlCanonicalizeW with
// URL_ESCAPE_UNSAFE | URL_DONT_SIMPLIFY | URL_ESCAPE_PERCENT
static FORCEINLINE void EncodeMsnUrl(const TCHAR* Url, TCHAR* Out)
{
    MsnUrlCanonicalize(Url, Out);
}

// 0x10163460 (ret 4): the connection notes the pending transaction
__forceinline void NMsnSession::RegisterTransaction(INT Code)
{
    ((NotificationProcess*)this)->RegisterPending(Code);
}

// 0x101607e0 (ret 0x1C): the command goes to the connection's sender as a copy, which the sender releases
__forceinline void NMsnSession::SendCommand(const NMsnCommand& Command)
{
    ((NotificationProcess*)this)->SendLine(NMsnLocalCommand(Command));
}

// 0x10164290 (ret 0x10): the block / unblock of a contact, the connection's SendBlock (the text by value)
__forceinline void NMsnSession::BlockFriend(const FString& Mail, INT bBlock)
{
    ((NotificationProcess*)this)->SendBlock(Mail, bBlock);
}

// 0x101640c0 (ret 0x10) - "ADD FL|BL mail mail" (the contact on the forward list, or on the block list), then "ADD AL|RL
// mail mail" (the allow list, or the reverse list)
void NMsnSession::AddFriend(FString Mail, INT bAllow)
{
    NMsnCommand Forward(TEXT("ADD"));
    Forward.TrId() = TransactionId;
    Forward.AddParam(bAllow ? TEXT("FL") : TEXT("BL"));
    Forward.AddParam(Mail);
    Forward.AddParam(Mail);
    RegisterTransaction(10);
    SendCommand(Forward);

    NMsnCommand Allow(TEXT("ADD"));
    Allow.TrId() = TransactionId;
    Allow.AddParam(bAllow ? TEXT("AL") : TEXT("RL"));
    Allow.AddParam(Mail);
    Allow.AddParam(Mail);
    RegisterTransaction(10);
    SendCommand(Allow);
}
NW_HOOK_VARARGS(0x101640c0, NMsnSession, AddFriend, void (NMsnSession::*)(FString, INT))

// 0x10164e20 (ret 0x10) - "REM FL mail", and with bBlock the contact goes on the block list (0x10164290)
void NMsnSession::RemoveFriend(FString Mail, INT bBlock)
{
    NMsnCommand Command(TEXT("REM"));
    Command.TrId() = TransactionId;
    Command.AddParam(TEXT("FL"));
    Command.AddParam(Mail);
    RegisterTransaction(11);
    SendCommand(Command);
    if (bBlock)
        BlockFriend(Mail, 1);
}
NW_HOOK_VARARGS(0x10164e20, NMsnSession, RemoveFriend, void (NMsnSession::*)(FString, INT))

// 0x10164480 (ret 0x4c) - "ADD FL account name ToGroup", then "REM FL account FromGroup" (the contact record's account is
// at +0x20 and its shown name at +0x2C)
void NMsnSession::MoveFriend(NMsnBuddy Buddy, INT FromGroup, INT ToGroup)
{
    NMsnCommand Add(TEXT("ADD"));
    Add.TrId() = TransactionId;
    Add.AddParam(TEXT("FL"));
    Add.AddParam(Buddy.Text20());
    Add.AddParam(Buddy.Text2C());
    Add.AddParam(ToGroup);
    RegisterTransaction(13);
    SendCommand(Add);

    NMsnCommand Remove(TEXT("REM"));
    Remove.TrId() = TransactionId;
    Remove.AddParam(TEXT("FL"));
    Remove.AddParam(Buddy.Text20());
    Remove.AddParam(FromGroup);
    RegisterTransaction(14);
    SendCommand(Remove);
}
NW_HOOK_VARARGS(0x10164480, NMsnSession, MoveFriend, void (NMsnSession::*)(NMsnBuddy, INT, INT))

// 0x10164640 (ret 0xc) - "ADG name 0" with the name URL-escaped
void NMsnSession::AddGroup(FString Name)
{
    NMsnCommand Command(TEXT("ADG"));
    Command.TrId() = TransactionId;
    TCHAR Escaped[260];
    appMemset(Escaped, 0, 0x208);
    EncodeMsnUrl(*Name, Escaped);
    Command.AddParam(Escaped);
    Command.AddParam(0);
    RegisterTransaction(15);
    SendCommand(Command);
}
NW_HOOK_VARARGS(0x10164640, NMsnSession, AddGroup, void (NMsnSession::*)(FString))

// 0x10164750 (ret 4) - "RMG group"
void NMsnSession::RemoveGroup(INT GroupId)
{
    NMsnCommand Command(TEXT("RMG"));
    Command.TrId() = TransactionId;
    Command.AddParam(GroupId);
    RegisterTransaction(16);
    SendCommand(Command);
}
NW_HOOK_METHOD(0x10164750, NMsnSession, RemoveGroup)

// 0x101647f0 (ret 0x10) - "REG group name 0" with the name URL-escaped
void NMsnSession::RenameGroup(INT GroupId, FString Name)
{
    NMsnCommand Command(TEXT("REG"));
    Command.TrId() = TransactionId;
    Command.AddParam(GroupId);
    TCHAR Escaped[260];
    appMemset(Escaped, 0, 0x208);
    EncodeMsnUrl(*Name, Escaped);
    Command.AddParam(Escaped);
    Command.AddParam(0);
    RegisterTransaction(17);
    SendCommand(Command);
}
NW_HOOK_VARARGS(0x101647f0, NMsnSession, RenameGroup, void (NMsnSession::*)(INT, FString))
