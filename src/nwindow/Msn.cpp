/*=============================================================================
    Msn.cpp - the messenger's manager (C3 only). Stock: 0x1012AB60 .. 0x1012C0E0. The notifications to the window take
    their records by value (the callee destroys them, like every by-value argument of the stock build); the window's
    functions are msn2's and are called by address.
=============================================================================*/
#include "nwindow/Msn.h"
#include "nwindow/Msn1Pending.h"
#include "nwindow/NConsoleWnd.h"
#include "nwindow/DispatchProcess.h"
#include "nwindow/NotificationProcess.h"
#include "nwindow/SwitchboardSession.h"
#include "nwindow/MsnWnd.h"
#include "nwindow/MsnTreeCtrl.h"
#include "nwindow/MsnTree.h"
#include "nwindow/MsnProtoOps.h"

// 0x1014f8f0 (ret 0x44, guard name) - a friend came: the window gets a copy of the record
void Msn::NotifyAddFriend(NMsnFriend Friend)
{
    guard(Msn::NotifyAddFriend);
    if (Wnd)
        Wnd->NotifyAddFriend(MsnPersonCopy(*(const NMsnPersonRec*)&Friend));
    unguard;
}
typedef void (Msn::*SigMsnNotifyAddFriend)(NMsnFriend);
NW_HOOK_VARARGS(0x1014f8f0, Msn, NotifyAddFriend, SigMsnNotifyAddFriend)

// 0x1014f9a0 (ret 0xC, guard name) - a friend went (the account by value): the window gets a copy of the account
void Msn::NotifyRemoveFriend(FString Account)
{
    if (Wnd)
        Wnd->NotifyRemoveFriend(MsnStringCopy(Account));
}
typedef void (Msn::*SigMsnNotifyRemoveFriend)(FString);
NW_HOOK_VARARGS(0x1014f9a0, Msn, NotifyRemoveFriend, SigMsnNotifyRemoveFriend)

// 0x1014fa50 (ret 0x1C, guard name) - a group came: the window gets a copy of the record
void Msn::NotifyAddGroup(NMsnGroup Group)
{
    guard(Msn::NotifyAddGroup);
    if (Wnd)
        Wnd->NotifyAddGroup(MsnGroupCopy(*(const NMsnGroupArg*)&Group));
    unguard;
}
typedef void (Msn::*SigMsnNotifyAddGroup)(NMsnGroup);
NW_HOOK_VARARGS(0x1014fa50, Msn, NotifyAddGroup, SigMsnNotifyAddGroup)

// 0x1014fca0 (ret 4, guard name) - the messenger's sounds, when the option is on: 0 a friend came online, 1 a message
// came, 2 new e-mail (the interface sound of that name is played by the console)
void Msn::PlayAlarmSound(INT Kind)
{
    guard(Msn::PlayAlarmSound);
    if (GMsnSoundAlarm)
    {
        if (Kind == 0)
            GConsole->PlaySoundName(TEXT("MessengerSound.online"));
        else if (Kind == 1)
            GConsole->PlaySoundName(TEXT("MessengerSound.type"));
        else if (Kind == 2)
            GConsole->PlaySoundName(TEXT("MessengerSound.newemail"));
    }
    unguard;
}
NW_HOOK_METHOD(0x1014fca0, Msn, PlayAlarmSound)

// 0x101501a0 (guard name) - the dispatch connection is over: its destructor (0x10140280), its memory, the pointer cleared
void Msn::NotifyDispatchProcessEnd()
{
    if (Dispatch)
    {
        Dispatch->Destroy();
        appFree(Dispatch);
        Dispatch = NULL;
    }
}
NW_HOOK_METHOD(0x101501a0, Msn, NotifyDispatchProcessEnd)

// 0x1014f630 (ret 0xC; name ours) - new e-mail text for the window's 0x10130FF0 (a copy: the window destroys it)
void Msn::NotifyNewMail(FString Text)
{
    Wnd->OnNewMail(MsnStringCopy(Text));
}
typedef void (Msn::*SigMsnNotifyNewMail)(FString);
NW_HOOK_VARARGS(0x1014f630, Msn, NotifyNewMail, SigMsnNotifyNewMail)

// 0x1014f880 (ret 0xC; name ours) - an error number (the string by value) for the window's dialog (0x10133BE0)
void Msn::NotifyError(FString Code)
{
    if (Wnd)
        Wnd->ShowError(Code);
}
typedef void (Msn::*SigMsnNotifyError)(FString);
NW_HOOK_VARARGS(0x1014f880, Msn, NotifyError, SigMsnNotifyError)

// 0x1014f6a0 (ret 0x44; name ours) - a friend's record for the window's 0x10137120 after the forward list was sorted
void Msn::NotifyFriendOnline(NMsnFriend Friend)
{
    Contacts->FL->Sort();
    Wnd->OnFriendPresence(MsnPersonCopy(*(const NMsnPersonRec*)&Friend));
}
typedef void (Msn::*SigMsnNotifyFriendOnline)(NMsnFriend);
NW_HOOK_VARARGS(0x1014f6a0, Msn, NotifyFriendOnline, SigMsnNotifyFriendOnline)

// 0x1014f710 (ret 0x44; name ours) - the same for the window's 0x10137290
void Msn::NotifyFriendStatus(NMsnFriend Friend)
{
    Contacts->FL->Sort();
    Wnd->RefreshFriend(MsnPersonCopy(*(const NMsnPersonRec*)&Friend));
}
typedef void (Msn::*SigMsnNotifyFriendStatus)(NMsnFriend);
NW_HOOK_VARARGS(0x1014f710, Msn, NotifyFriendStatus, SigMsnNotifyFriendStatus)

// 0x1014f780 (ret 0x44; name ours) - the same for the window's 0x10133AF0
void Msn::NotifyFriendName(NMsnFriend Friend)
{
    Contacts->FL->Sort();
    Wnd->RefreshGroupOf(MsnPersonCopy(*(const NMsnPersonRec*)&Friend));
}
typedef void (Msn::*SigMsnNotifyFriendName)(NMsnFriend);
NW_HOOK_VARARGS(0x1014f780, Msn, NotifyFriendName, SigMsnNotifyFriendName)

// 0x1014f7f0 (ret 0x44; name ours) - a friend's record for the window's 0x10133B60 (no sort)
void Msn::NotifyFriendBlocked(NMsnFriend Friend)
{
    Wnd->OnFriendRequest(MsnPersonCopy(*(const NMsnPersonRec*)&Friend));
}
typedef void (Msn::*SigMsnNotifyFriendBlocked)(NMsnFriend);
NW_HOOK_VARARGS(0x1014f7f0, Msn, NotifyFriendBlocked, SigMsnNotifyFriendBlocked)

// 0x1014fb00 (ret 0x44; name ours) - the user's own record came: signed in, the record kept, the window's 0x10131280 gets a copy
void Msn::NotifySignedIn(NMsnFriend Self)
{
    bSignedIn = 1;
    *Me = Self;
    if (Wnd)
        Wnd->OnSignedIn(MsnPersonCopy(*(const NMsnPersonRec*)&Self));
}
typedef void (Msn::*SigMsnNotifySignedIn)(NMsnFriend);
NW_HOOK_VARARGS(0x1014fb00, Msn, NotifySignedIn, SigMsnNotifySignedIn)

// 0x1014fb80 (ret 0x44; name ours) - the user's record changed: kept, the window's 0x10131360 gets a copy
void Msn::NotifyMyInfo(NMsnFriend Self)
{
    *Me = Self;
    if (Wnd)
        Wnd->SetMe(MsnPersonCopy(*(const NMsnPersonRec*)&Self));
}
typedef void (Msn::*SigMsnNotifyMyInfo)(NMsnFriend);
NW_HOOK_VARARGS(0x1014fb80, Msn, NotifyMyInfo, SigMsnNotifyMyInfo)

// 0x1014fc30 (ret 0xC; name ours) - the contact of that account in the forward list (the account by value), NULL when there is none
NMsnFriend* Msn::FindFriend(FString Account)
{
    return Contacts->FL->FindFriend(Account);
}
typedef NMsnFriend* (Msn::*SigMsnFindFriend)(FString);
NW_HOOK_VARARGS(0x1014fc30, Msn, FindFriend, SigMsnFindFriend)

// 0x1014f850 (name ours; the db's C2-mapped "MSNSocket::CloseSocket" is wrong) - with a window: the forward list sorted and the
// window's 0x10130F70 (the tree rebuilt from the lists)
void Msn::SortFriends()
{
    if (Wnd)
    {
        Contacts->FL->Sort();
        ((NMsnWndOps*)Wnd)->GiveListsToTree();
    }
}
NW_HOOK_METHOD(0x1014f850, Msn, SortFriends)

// 0x1014fef0 (name ours) - the same as SortFriends, unless the person tree of the window's tree control has its root entry already
// (C4: Wnd +0x17C is the control, its +0x144 the MsnTree, whose +0x1F4 is the root entry)
void Msn::RefreshFriends()
{
    if (Wnd)
    {
        if (Wnd->TreeCtrl->MsnTreeWnd->Root == NULL)
        {
            Contacts->FL->Sort();
            ((NMsnWndOps*)Wnd)->GiveListsToTree();
        }
    }
}
NW_HOOK_METHOD(0x1014fef0, Msn, RefreshFriends)

// 0x10152c90 (ret 4; the result is the hidden argument; name ours) - a copy of the account of the login
FString Msn::GetAccount()
{
    return Account;
}
NW_HOOK_METHOD(0x10152c90, Msn, GetAccount)

// 0x10160f60 (ret 4; name ours) - a copy of the password
FString Msn::GetPassword()
{
    return Password;
}
NW_HOOK_METHOD(0x10160f60, Msn, GetPassword)

// 0x10160f90 (ret 4; name ours) - a copy of the string at +0x3C
FString Msn::GetUnknown3C()
{
    return Unknown3C;
}
NW_HOOK_METHOD(0x10160f90, Msn, GetUnknown3C)

// 0x10150840 (guard name) - the end of every switchboard session: each is shown to the window (0x10138460), destroyed (the
// destructor 0x1012F090) and freed, then the table is emptied (twelve-byte pairs, a hash of 8)
void Msn::NotifySwitchboardSessionEnd()
{
    for (INT i = 0; i < Sessions.Pairs.Num(); i++)
    {
        SwitchboardSession* Session = Sessions.Pairs(i).Value;
        if (Session)
        {
            if (Wnd)
                Wnd->OnSessionRemoved(Session);
            Session->Destroy();
            appFree(Session);
        }
    }
    Sessions.Pairs.Empty();
    Sessions.HashCount = 8;
    ((NMsnIntMap*)&Sessions)->Rehash();   // the table's Rehash
}
NW_HOOK_METHOD(0x10150840, Msn, NotifySwitchboardSessionEnd)

// 0x10150ba0 (ret 0xC; name ours) - the notification connection ended (the reason by value): the sessions go first, then the
// connection (destructor 0x10140970, freed), the signed-in flag is cleared and the window gets the reason (0x101387F0)
void Msn::NotifyNotificationProcessEnd(FString Reason)
{
    NotifySwitchboardSessionEnd();
    if (Notification)
    {
        NotificationProcess* Process = Notification;
        Process->Destroy();
        appFree(Process);
        Notification = NULL;
        bSignedIn = 0;
        Wnd->OnSignedOut(MsnStringCopy(Reason));
    }
}
typedef void (Msn::*SigMsnNotifyNotificationProcessEnd)(FString);
NW_HOOK_VARARGS(0x10150ba0, Msn, NotifyNotificationProcessEnd, SigMsnNotifyNotificationProcessEnd)

// 0x1014fd50 (ret 0x10; name ours) - a notification connection (0x98 bytes, constructed by 0x10140890 with the manager, the
// server and the first transaction id 1) to that server, started; error 930 to the window when it cannot connect
void Msn::StartNotification(NMsnServer Server)
{
    NotificationProcess* Process = NULL;
    void* Memory = ::operator new(0x98);
    if (Memory)
    {
#if NW_HOOK_MODE
        Process = ((NotificationProcess*)Memory)->ConstructNS(this, Server, 1);
#else
        Process = new (Memory) NotificationProcess(this, Server, 1);
#endif
    }
    Notification = Process;
    if (!Process->Connect())
        NotifyError(FString(TEXT("930")));
}
typedef void (Msn::*SigMsnStartNotification)(NMsnServer);
NW_HOOK_VARARGS(0x1014fd50, Msn, StartNotification, SigMsnStartNotification)

// 0x10150300 (ret 4; name ours) - the session connects (virtual 8) and enters the map under its socket
void Msn::AddSession(SwitchboardSession* Session)
{
    Session->Connect();
    INT Socket = Session->Sock.hSock;
    INT Value = (INT)Session;
    ((NMsnIntMap*)&Sessions)->Set(Socket, Value);
}
NW_HOOK_METHOD(0x10150300, Msn, AddSession)

// 0x10150970 (ret 4; name ours) - the session of that socket: shown to the window (even a missing one), destroyed and freed
// when there is one; every pair of the socket leaves the map
void Msn::RemoveSession(INT Socket)
{
    SwitchboardSession* Session = Sessions.FindRef(Socket);
    if (Wnd)
        Wnd->OnSessionRemoved(Session);
    if (Session)
    {
        Session->Destroy();
        appFree(Session);
    }
    ((NMsnIntMap*)&Sessions)->Remove(Socket);
}
NW_HOOK_METHOD(0x10150970, Msn, RemoveSession)

// 0x10150330 (ret 0x50; name ours) - every session whose id is that one: the window's 0x10136F90 gets the session and a copy of
// the record
void Msn::NotifySessionJoin(FString SessionID, NMsnFriend Friend)
{
    for (INT i = 0; i < Sessions.Pairs.Num(); i++)
    {
        SwitchboardSession* Session = Sessions.Pairs(i).Value;
        if (*Session->GetSessionIDRef() == SessionID)
            ((NMsnWndOps*)Wnd)->OnSessionJoin(Session, Friend);
    }
}
typedef void (Msn::*SigMsnNotifySessionJoin)(FString, NMsnFriend);
NW_HOOK_VARARGS(0x10150330, Msn, NotifySessionJoin, SigMsnNotifySessionJoin)

// 0x10150400 (ret 0x50; name ours) - the same for the window's 0x1013B480
void Msn::NotifySessionLeave(FString SessionID, NMsnFriend Friend)
{
    for (INT i = 0; i < Sessions.Pairs.Num(); i++)
    {
        SwitchboardSession* Session = Sessions.Pairs(i).Value;
        if (*Session->GetSessionIDRef() == SessionID)
            ((NMsnWndOps*)Wnd)->OnSessionLeave(Session, Friend);
    }
}
typedef void (Msn::*SigMsnNotifySessionLeave)(FString, NMsnFriend);
NW_HOOK_VARARGS(0x10150400, Msn, NotifySessionLeave, SigMsnNotifySessionLeave)

// 0x101505d0 (ret 0x50; name ours) - the same for the window's 0x10135E90
void Msn::NotifySessionEvent(FString SessionID, NMsnFriend Friend)
{
    for (INT i = 0; i < Sessions.Pairs.Num(); i++)
    {
        SwitchboardSession* Session = Sessions.Pairs(i).Value;
        if (*Session->GetSessionIDRef() == SessionID)
            ((NMsnWndOps*)Wnd)->OnSessionTyping(Session, Friend);
    }
}
typedef void (Msn::*SigMsnNotifySessionEvent)(FString, NMsnFriend);
NW_HOOK_VARARGS(0x101505d0, Msn, NotifySessionEvent, SigMsnNotifySessionEvent)

// 0x101504d0 (ret 0xA8; name ours) - the same with a message (the window's 0x1013BB60 gets the session, a copy of the record and
// a copy of the message)
void Msn::NotifySessionMessage(FString SessionID, NMsnFriend Friend, NMsnMessage Message)
{
    for (INT i = 0; i < Sessions.Pairs.Num(); i++)
    {
        SwitchboardSession* Session = Sessions.Pairs(i).Value;
        if (*Session->GetSessionIDRef() == SessionID)
            ((NMsnWndOps*)Wnd)->OnSessionMessage(Session, Friend, Message);
    }
}
typedef void (Msn::*SigMsnNotifySessionMessage)(FString, NMsnFriend, NMsnMessage);
// Waits for msn2's ~NMsnMessage (0x1014efe0, NChatMsg.cpp): the stock runs until the tree has that lane (Msn1Pending.h: MSN1_AFTER_MSN2 is 1 by
// itself once include/nwindow/Msn2Pending.h exists; proved in the merged tree)
NW_HOOK_VARARGS(0x101504d0, Msn, NotifySessionMessage, SigMsnNotifySessionMessage)

// 0x101506a0 (ret 0xA8; name ours) - the same for the window's 0x1013BC90
void Msn::NotifySessionMessage2(FString SessionID, NMsnFriend Friend, NMsnMessage Message)
{
    for (INT i = 0; i < Sessions.Pairs.Num(); i++)
    {
        SwitchboardSession* Session = Sessions.Pairs(i).Value;
        if (*Session->GetSessionIDRef() == SessionID)
            ((NMsnWndOps*)Wnd)->OnSessionInvite(Session, Friend, Message);
    }
}
typedef void (Msn::*SigMsnNotifySessionMessage2)(FString, NMsnFriend, NMsnMessage);
// Waits for msn2's ~NMsnMessage (0x1014efe0, NChatMsg.cpp): the stock runs until the tree has that lane (Msn1Pending.h: MSN1_AFTER_MSN2 is 1 by
// itself once include/nwindow/Msn2Pending.h exists; proved in the merged tree)
NW_HOOK_VARARGS(0x101506a0, Msn, NotifySessionMessage2, SigMsnNotifySessionMessage2)

// 0x101507a0 (ret 0xC; name ours) - the session of that id (by value) started: the window's 0x10138420 gets it
void Msn::NotifySessionStarted(FString SessionID)
{
    for (INT i = 0; i < Sessions.Pairs.Num(); i++)
    {
        SwitchboardSession* Session = Sessions.Pairs(i).Value;
        if (*Session->GetSessionIDRef() == SessionID)
            Wnd->ShowChatOfSession(Session);
    }
}
typedef void (Msn::*SigMsnNotifySessionStarted)(FString);
NW_HOOK_VARARGS(0x101507a0, Msn, NotifySessionStarted, SigMsnNotifySessionStarted)

// 0x1014fc00 (ret 0xC; name ours) - nothing: a function taking a string by value only to destroy it (the unknown answers of
// the notification connection go through it)
void Msn::DiscardString(FString Text)
{
}
typedef void (Msn::*SigMsnDiscardString)(FString);
NW_HOOK_VARARGS(0x1014fc00, Msn, DiscardString, SigMsnDiscardString)

// 0x1014fe20 (ret 0x1C; name ours) - a switchboard session for that server and session id (both by value): 0xA4 bytes, constructed
// by 0x1012EF60 with the manager, the server and the id
SwitchboardSession* Msn::CreateSession(NMsnServer Server, FString SessionID)
{
    SwitchboardSession* Session = NULL;
    void* Memory = ::operator new(0xA4);
    if (Memory)
    {
#if NW_HOOK_MODE
        Session = ((SwitchboardSession*)Memory)->ConstructSession(this, Server, SessionID);
#else
        Session = new (Memory) SwitchboardSession(this, Server, SessionID);
#endif
    }
    return Session;
}
typedef SwitchboardSession* (Msn::*SigMsnCreateSession)(NMsnServer, FString);
NW_HOOK_VARARGS(0x1014fe20, Msn, CreateSession, SigMsnCreateSession)

// 0x1014ffc0 (ret 8; name ours) - WSAAsyncSelect's connect message: the connection of that socket (the dispatch one, the notification one,
// else a session of the map) gets OnConnect
void Msn::OnSocketConnect(INT Socket, LPARAM lParam)
{
    MSNSocket* Target = NULL;
    if (Dispatch && Dispatch->Sock.hSock == (UINT)Socket)
        Target = Dispatch;
    else if (Notification && Notification->Sock.hSock == (UINT)Socket)
        Target = Notification;
    else if (Sessions.Pairs.Num() > 0)
        Target = Sessions.FindRef(Socket);
    if (Target)
        Target->OnConnect(Socket, lParam);
}
NW_HOOK_METHOD(0x1014ffc0, Msn, OnSocketConnect)

// 0x10150020 (ret 8; name ours) - the data / close message of a socket, the same way (OnSocketMessage)
void Msn::OnSocketData(INT Socket, LPARAM lParam)
{
    MSNSocket* Target = NULL;
    if (Dispatch && Dispatch->Sock.hSock == (UINT)Socket)
        Target = Dispatch;
    else if (Notification && Notification->Sock.hSock == (UINT)Socket)
        Target = Notification;
    else if (Sessions.Pairs.Num() > 0)
        Target = Sessions.FindRef(Socket);
    if (Target)
        Target->OnSocketMessage(Socket, lParam);
}
NW_HOOK_METHOD(0x10150020, Msn, OnSocketData)

// 0x1014f1d0 (ret 4; the result is the hidden argument; name ours) - the presence status the notification connection has (0x1013C450),
// an empty string without a connection
FString Msn::GetStatus()
{
    if (Notification)
        return Notification->GetStatus();
    return FString(TEXT(""));
}
NW_HOOK_METHOD(0x1014f1d0, Msn, GetStatus)
