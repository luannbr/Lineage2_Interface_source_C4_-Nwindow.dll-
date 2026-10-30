#pragma once
// Msn: the messenger's manager (not an object of the engine: a plain heap object made by the messenger window's OnCreate). It has a
// vtable of two slots (the stock's 0x101DBEE4: OnSocketConnect 0x1014FFC0, OnSocketData 0x10150020). It owns the connections and the contact
// data and tells the window what happens. Stock: 0x1014F1C0 .. 0x10150C40 (the methods below).

#include "nwindow/NMsnProtocol.h"
#include "nwindow/NGameData.h"
#include "nwindow/FriendList.h"

class MsnWnd;                 // msn2's window (stock 0x10130F70 ..)
struct NMsnStringArg;         // a string given by value to a function that frees it (MsnWnd.h)
class DispatchProcess;
class NotificationProcess;
class SwitchboardSession;

class Msn
{
public:
    // +0x00 the vtable (two slots: the window messages of the sockets, below)
    MsnWnd* Wnd;                                      // +0x04 the window of the list (NULL before it exists)
    DispatchProcess* Dispatch;                        // +0x08 the dispatch server connection (deleted at its end)
    NotificationProcess* Notification;                // +0x0C the notification server connection
    NDataMap<SwitchboardSession> Sessions;            // +0x10 the switchboard sessions by their socket (TMap<INT, SwitchboardSession*>)
    FString Account;                                  // +0x24 the e-mail account of the login
    FString Password;                                 // +0x30
    FString Unknown3C;                                // +0x3C
    INT Unknown48;                                    // +0x48
    INT bSignedIn;                                    // +0x4C 1 once the user's data came (NotifySignedIn)
    NMsnContacts* Contacts;                           // +0x50 the four lists and the groups
    NMsnFriend* Me;                                   // +0x54 the user's own record
    // The stock constructor's (0x10150C40, ret 4) and destructor's (0x10150A40) bodies. Hook build: Construct writes the stock vtable
    // (0x101DBEE4) by hand, as it cannot let the compiler store ours. Standalone build: the real constructor below (placement new on a block of
    // 0x58 bytes: the compiler writes our vtable), Destroy runs before the block is freed
    Msn* Construct(MsnWnd* Window);
    void Destroy();
#if !NW_HOOK_MODE
    Msn(MsnWnd* Window);
#endif

    // what the window asks of the manager (MsnLogin.cpp; the strings go by value as plain bytes, freed by the callee)
    __declspec(noinline) void SignIn(NMsnStringArg InAccount, NMsnStringArg InPassword);   // 0x10150120 (ret 0x18): the account and the password kept, the dispatch connection made
    __declspec(noinline) void StartDispatch();                                           // 0x10150080: the dispatch connection made and told to connect (error 930 to the window when it cannot)
    void SignOut();                                                                      // 0x1014F1C0: the notification connection signs out
    __declspec(safebuffers) void Block(NMsnStringArg Account, INT bBlock);               // 0x1014F300 (ret 0x10): the account blocked (bBlock) or unblocked
    __declspec(safebuffers) void SetStatus(NMsnStringArg Code);                          // 0x1014F550 (ret 0xC): the presence code goes to the notification connection
    __declspec(safebuffers) void SetDisplayName(NMsnStringArg Name);                     // 0x1014F5C0 (ret 0xC): the name shown goes to the notification connection

    // the notifications to the window (names ours except the guard ones)
    void NotifyAddFriend(NMsnFriend Friend);          // 0x1012ae20 (ret 0x44, guard name)
    void NotifyRemoveFriend(FString Account);         // 0x1012aed0 (ret 0xC, guard name)
    void NotifyAddGroup(NMsnGroup Group);             // 0x1012af80 (ret 0x1C, guard name)
    void PlayAlarmSound(INT Kind);                    // 0x1012b1d0 (ret 4, guard name): 0 online, 1 type, 2 new e-mail
    void NotifyDispatchProcessEnd();                  // 0x1012b720 (guard name): the dispatch connection deleted
    void NotifySwitchboardSessionEnd();               // 0x1012bd80 (guard name): every session deleted
    void NotifyNotificationProcessEnd(FString Reason);  // 0x1012c0e0 (ret 0xC; name ours): every session deleted, the notification connection too, the window told
    void StartNotification(NMsnServer Server);        // 0x1012b280 (ret 0x10; name ours): a notification connection to that server, connecting (error 930 when it cannot)
    FString GetStatus();                              // 0x1012a700 (ret 4; name ours): the notification connection's status, "" when there is none
    FString GetAccount();                             // 0x1012e090 (ret 4; name ours): a copy of Account
    FString GetPassword();                            // 0x1013c540 (ret 4; name ours): a copy of Password
    FString GetUnknown3C();                           // 0x1013c570 (ret 4; name ours)
    // the window messages of the sockets (the stock's 0x1012B540 / 0x1012B5A0, ret 8): the connection with that socket gets them
    virtual void OnSocketConnect(INT Socket, LPARAM lParam);   // slot 0, 0x1014ffc0 (name ours): the dispatch, the notification or the session of that socket: virtual 9
    virtual void OnSocketData(INT Socket, LPARAM lParam);      // slot 1, 0x10150020 (name ours): the same with virtual 10
    // the sessions
    SwitchboardSession* CreateSession(NMsnServer Server, FString SessionID);   // 0x1012b350 (ret 0x1C; name ours): a session (0xA4 bytes, constructed by 0x1012EF60) for that server and session id
    void AddSession(SwitchboardSession* Session);     // 0x1012b840 (ret 4; name ours): the session connecting, entered in the map by its socket
    void RemoveSession(INT Socket);                   // 0x1012beb0 (ret 4; name ours): the session of that socket shown to the window (0x10138460), destroyed (0x1012F090), freed, out of the map
    void NotifySessionJoin(FString SessionID, NMsnFriend Friend);       // 0x1012b870 (ret 0x50; name ours): the window's 0x10136F90 for the session of that id
    void NotifySessionLeave(FString SessionID, NMsnFriend Friend);      // 0x1012b940 (ret 0x50; name ours): the window's 0x1013B480
    void NotifySessionEvent(FString SessionID, NMsnFriend Friend);      // 0x1012bb10 (ret 0x50; name ours): the window's 0x10135E90
    void NotifySessionMessage(FString SessionID, NMsnFriend Friend, NMsnMessage Message);   // 0x1012ba10 (ret 0xA8; name ours): the window's 0x1013BB60
    void NotifySessionMessage2(FString SessionID, NMsnFriend Friend, NMsnMessage Message);  // 0x1012bbe0 (ret 0xA8; name ours): the window's 0x1013BC90
    void NotifySessionStarted(FString SessionID);     // 0x1012bce0 (ret 0xC; name ours): the window's 0x10138420 for the session of that id
    void DiscardString(FString Text);                 // 0x1012b130 (ret 0xC; name ours): nothing (the string by value is destroyed)
    void NotifyNewMail(FString Text);                 // 0x1012ab60 (ret 0xC)
    void NotifyError(FString Code);                   // 0x1012adb0 (ret 0xC): the window's dialog for an error number
    void NotifyFriendOnline(NMsnFriend Friend);       // 0x1012abd0 (ret 0x44): the list sorted, then the window's 0x10137120
    void NotifyFriendStatus(NMsnFriend Friend);       // 0x1012ac40 (ret 0x44): the list sorted, then the window's 0x10137290
    void NotifyFriendName(NMsnFriend Friend);         // 0x1012acb0 (ret 0x44): the list sorted, then the window's 0x10133AF0
    void NotifyFriendBlocked(NMsnFriend Friend);      // 0x1012ad20 (ret 0x44): the window's 0x10133B60
    void NotifySignedIn(NMsnFriend Self);             // 0x1012b030 (ret 0x44): the user's record taken, the window's 0x10131280
    void NotifyMyInfo(NMsnFriend Self);               // 0x1012b0b0 (ret 0x44): the user's record taken, the window's 0x10131360
    NMsnFriend* FindFriend(FString Account);          // 0x1012b160 (ret 0xC): in the forward list
    void SortFriends();                               // 0x1012ad80: the forward list sorted and the window's tree refreshed (the db's "MSNSocket::CloseSocket")
    void RefreshFriends();                            // 0x1012b420: the same unless the window is in its busy state
    // 0x1012b140 (4 bytes, folded with other classes' getters and hooked in NCWnd.cpp) returns Contacts, 0x1012b150 (chat lane's)
    // returns Me: the stock's callers outside this class call them, ours calls the out-of-line copy of the first
    __declspec(noinline) NMsnContacts* GetContacts() { return Contacts; }
    // 0x1014FC20 (4 bytes: mov eax, [ecx + 0x54]; folded with other classes' getters and hooked in ChatWnd.cpp as NChatMsnRequester::GetMe): the user's
    // own record. The stock's callers outside this class call it, so ours calls the out-of-line copy
    __declspec(noinline) NMsnFriend* GetMe() { return Me; }
};
NW_ASSERT_SIZE(Msn, 0x58);
NW_ASSERT_OFFSET(Msn, Wnd, 0x04);
NW_ASSERT_OFFSET(Msn, Sessions, 0x10);
NW_ASSERT_OFFSET(Msn, Account, 0x24);
NW_ASSERT_OFFSET(Msn, Password, 0x30);
NW_ASSERT_OFFSET(Msn, Unknown3C, 0x3C);
NW_ASSERT_OFFSET(Msn, bSignedIn, 0x4C);
NW_ASSERT_OFFSET(Msn, Contacts, 0x50);
NW_ASSERT_OFFSET(Msn, Me, 0x54);
