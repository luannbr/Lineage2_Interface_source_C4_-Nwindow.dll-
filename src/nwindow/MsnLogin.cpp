/*=============================================================================
    MsnLogin.cpp - the manager's log-in and the commands the windows send (C4 addresses): the sign-in (0x10150120) that makes the dispatch
    connection (0x10150080, its server 0x1014FF30, its constructor 0x10164C20), the manager's thin calls that hand a string or a flag to the
    notification connection (MsnUtil.cpp had them: 0x1014F1C0, 0x1014F300, 0x1014F550, 0x1014F5C0) and the commands of the notification
    connection that the window functions call: OUT (0x10161DF0), CHG (0x10163EC0), REA (0x10163F90) and REM / ADD of the block list (0x10164290).
    The classes are the protocol lane's (Msn.h, DispatchProcess.h, NotificationProcess.h); the methods here are members of theirs.
    Wave 5: the answer to SYN (0x10164D60) and the destructors of the pending-names map (0x101649F0, 0x10165240).
=============================================================================*/
#include "nwindow/DispatchProcess.h"
#include "nwindow/NotificationProcess.h"
#include "nwindow/MsnCommand.h"
#include "nwindow/MsnWnd.h"
#include "nwindow/MsnUtil.h"
#include "nwindow/MsnProtoOps.h"

/*-----------------------------------------------------------------------------
    The dispatch connection.
-----------------------------------------------------------------------------*/

// 0x1014ff30 (cdecl; the address in the hidden argument is filled and given back) - the dispatch server: messenger.hotmail.com, 1863
NMsnServer* MsnDispatchServer(NMsnServer* Out)
{
    NMsnServer Server(FString(TEXT("messenger.hotmail.com")), 0x747);
    new ((void*)Out) NMsnServer(Server);
    return Out;
}
NW_HOOK_FUNC(0x1014ff30, MsnDispatchServer)

/*-----------------------------------------------------------------------------
    The manager's sign-in.
-----------------------------------------------------------------------------*/

// 0x10150120 (ret 0x18) - the account and the password are kept, the dispatch connection is made; both given by value and freed here
void Msn::SignIn(NMsnStringArg InAccount, NMsnStringArg InPassword)
{
    Account = InAccount.Str();
    Password = InPassword.Str();
    StartDispatch();
    InAccount.Str().~FString();
    InPassword.Str().~FString();
}
NW_HOOK_VARARGS(0x10150120, Msn, SignIn, void (Msn::*)(NMsnStringArg, NMsnStringArg))

// 0x10150080 - the dispatch connection (a 0x64-byte object made with the dispatch server and the transaction id 1) is made and told
// to connect (virtual 8); when that fails the window is told the error 930
void Msn::StartDispatch()
{
    DispatchProcess* Process = NULL;
    void* Mem = appMalloc(0x64, TEXT("new"));
    if (Mem)
    {
        BYTE ServerBytes[sizeof(NMsnServer)];   // made by MsnDispatchServer, not by a constructor of ours
        MsnDispatchServer((NMsnServer*)ServerBytes);
#if NW_HOOK_MODE
        Process = ((DispatchProcess*)Mem)->ConstructDispatch(this, *(NMsnServer*)ServerBytes, 1);
#else
        Process = new (Mem) DispatchProcess(this, *(NMsnServer*)ServerBytes, 1);
#endif
        ((NMsnServer*)ServerBytes)->~NMsnServer();
    }
    Dispatch = Process;
    if (!Process->Connect())
        Wnd->ShowError(FString(TEXT("930")));
}
NW_HOOK_METHOD(0x10150080, Msn, StartDispatch)

/*-----------------------------------------------------------------------------
    The manager's thin calls (it holds the notification connection at +0xC; the call goes to it when there is one). The strings are
    given by value and freed here, the stock way.
-----------------------------------------------------------------------------*/

// 0x1014f1c0 - the session signs out
void Msn::SignOut()
{
    if (Notification)
        Notification->SendSignOut();
}
NW_HOOK_METHOD(0x1014f1c0, Msn, SignOut)

// 0x1014f300 (ret 0x10) - the account is blocked (bBlock set) or unblocked
void Msn::Block(NMsnStringArg Account, INT bBlock)
{
    if (Notification)
        Notification->SendBlock(Account.Str(), bBlock);
    Account.Str().~FString();
}
NW_HOOK_VARARGS(0x1014f300, Msn, Block, void (Msn::*)(NMsnStringArg, INT))

// 0x1014f550 (ret 0xC) - the presence code ("NLN", "BSY" ...) goes to the session
void Msn::SetStatus(NMsnStringArg Code)
{
    if (Notification)
        Notification->SendStatus(Code.Str());
    Code.Str().~FString();
}
NW_HOOK_VARARGS(0x1014f550, Msn, SetStatus, void (Msn::*)(NMsnStringArg))

// 0x1014f5c0 (ret 0xC) - the name shown goes to the session
void Msn::SetDisplayName(NMsnStringArg Name)
{
    if (Notification)
        Notification->SendDisplayName(Name.Str());
    Name.Str().~FString();
}
NW_HOOK_VARARGS(0x1014f5c0, Msn, SetDisplayName, void (Msn::*)(NMsnStringArg))

/*-----------------------------------------------------------------------------
    The commands of the notification connection.
-----------------------------------------------------------------------------*/

// 0x10161df0 - "OUT"
void NotificationProcess::SendSignOut()
{
    NMsnLocalCommand Command(FString(TEXT("OUT")));
    Send(Command);
}
NW_HOOK_METHOD(0x10161df0, NotificationProcess, SendSignOut)

// 0x10163ec0 (ret 0xC) - the status code is kept and "CHG trid code" sent (request type 7)
void NotificationProcess::SendStatus(FString Code)
{
    Status = Code;
    NMsnLocalCommand Command(FString(TEXT("CHG")));
    Command.TrID = NextTrID;
    Command.AddArg(Code);
    RegisterPending(7);
    Send(Command);
}
NW_HOOK_VARARGS(0x10163ec0, NotificationProcess, SendStatus, void (NotificationProcess::*)(FString))

// 0x10163f90 (ret 0xC) - "REA trid account name": the user's account and the new name made safe for the wire (0x10168C90) (request type 8)
void NotificationProcess::SendDisplayName(FString Name)
{
    NMsnLocalCommand Command(FString(TEXT("REA")));
    Command.TrID = NextTrID;
    NMsnFriend* Me = Owner->GetMe();
    Command.AddArg(FString(Me->Account));
    TCHAR Encoded[260];
    appMemset(Encoded, 0, 0x208);
    MsnUrlCanonicalize(*Name, Encoded);
    Command.AddArg(FString(Encoded));
    RegisterPending(8);
    Send(Command);
}
NW_HOOK_VARARGS(0x10163f90, NotificationProcess, SendDisplayName, void (NotificationProcess::*)(FString))

// 0x10164290 (ret 0x10) - when the user's forward list has the contact: "REM trid AL|BL account" (AL when blocking: it leaves the
// allow list, else the block list) and "ADD trid BL|AL account name" (the other list), both of request type 12
void NotificationProcess::SendBlock(FString Mail, INT bBlock)
{
    NMsnContacts* Contacts = Owner->GetContacts();
    FriendList* Forward = (FriendList*)MsnFirstDword(Contacts);   // the forward list (0x1016EC20), [Contacts + 0] (0x10151140 is the allow list's getter)
    if (Forward)
    {
        NMsnFriend* Contact = Forward->FindFriend(FString(Mail));
        if (Contact)
        {
            NMsnLocalCommand Remove(FString(TEXT("REM")));
            Remove.TrID = NextTrID;
            Remove.AddArg(FString(bBlock ? TEXT("AL") : TEXT("BL")));
            Remove.AddArg(FString(Mail));
            RegisterPending(0xC);
            Send(Remove);
            NMsnLocalCommand Add(FString(TEXT("ADD")));
            Add.TrID = NextTrID;
            Add.AddArg(FString(bBlock ? TEXT("BL") : TEXT("AL")));
            Add.AddArg(FString(Mail));
            Add.AddArg(FString(Contact->Name));
            RegisterPending(0xC);
            Send(Add);
        }
    }
}
NW_HOOK_VARARGS(0x10164290, NotificationProcess, SendBlock, void (NotificationProcess::*)(FString, INT))

// 0x10164d60 (ret 0x3C) - the answer to the request type 6 ("SYN trid serial"): when the serial is not the one the connection keeps,
// the four contact lists are emptied (0x10151FE0 each; the stock gets the lists through the four getters of the contact store,
// [+0] 0x1016EC20, [+4] 0x10151140, [+8] 0x10005B90, [+0xC] 0x10151150, folded with other getters of the image: here the fields are
// read directly), the serial is kept and the user's own status code (the manager's 0x10160F90) is sent again (CHG)
void NotificationProcess::HandleSYNStart(MsnLine Line)
{
    INT Serial = Line.GetArgInt(0);
    if (SyncSerial != Serial)
    {
        NMsnContacts* Contacts = Owner->GetContacts();
        ((NMsnListInit*)Contacts->FL)->DeleteAll();
        ((NMsnListInit*)Contacts->AL)->DeleteAll();
        ((NMsnListInit*)Contacts->BL)->DeleteAll();
        ((NMsnListInit*)Contacts->RL)->DeleteAll();
        SyncSerial = Serial;
        SendStatus(Owner->GetUnknown3C());
    }
}
typedef void (NotificationProcess::*SigNPHandleSYNStart)(MsnLine);
NW_HOOK_VARARGS(0x10164d60, NotificationProcess, HandleSYNStart, SigNPHandleSYNStart)

/*-----------------------------------------------------------------------------
    The table of the pending names (TMap<INT, FString>, NotificationProcess +0x84).
-----------------------------------------------------------------------------*/

// 0x101649f0 - the pairs removed (0x1013E9A0: the strings destroyed), then the array itself
void NMsnIntStrMap::FreePairs()
{
    RemovePairs(0, Pairs.Num());
    ((FArray*)this)->~FArray();
    *(void**)this = NULL;            // (nothing reads the array again; the store keeps this a function that returns, as the stock one does)
}
NW_HOOK_METHOD(0x101649f0, NMsnIntStrMap, FreePairs)

// 0x10165240 - the hash freed and forgotten, then the pairs (the stock clears the hash first; the pairs' destruction does not read it,
// and a store after the call keeps this a function that returns instead of a tail jump)
void NMsnIntStrMap::Free()
{
    if (Hash)
        appFree(Hash);
    FreePairs();
    Hash = NULL;
    HashCount = 0;
}
NW_HOOK_METHOD(0x10165240, NMsnIntStrMap, Free)
