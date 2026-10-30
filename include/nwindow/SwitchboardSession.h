#pragma once
// SwitchboardSession: one conversation of the messenger (C3 only; vtable 0x101A9AB0, 0xA4 bytes; made by Msn::sub_1012B350 with
// the constructor 0x1012EF60). A connection to a switchboard server: it says USR (the user started the chat) or ANS (the user was
// invited, RNG), invites with CAL, then carries the MSG / JOI / BYE traffic of the people in the room. The manager keeps the
// sessions in a map by socket; the window (ChatWnd, chat lane's) shows them.
//
// Request types kept in Pending (RegisterPending): 0x12 USR, 0x13 CAL, 0x14 ANS.

#include "nwindow/MSNSocket.h"
#include "nwindow/FriendList.h"
#include "nwindow/Msn2Pending.h"

class SwitchboardSession : public MSNSocket
{
public:
    FString SessionID;               // +0x64 the id of the switchboard session (the ANS / CAL answers give it)
    FString Cookie;                  // +0x70 the cookie of the invitation (CKI; USR / ANS send it)
    NMsnFriend* LastFriend;          // +0x7C the contact AddFriend entered last (name ours)
    NMsnStrMap Members;              // +0x80 the people in the room: TMap<FString, NMsnFriend*> by account (rehashed by the constructor)
    INT bInvited;                    // +0x94 1 when the user was invited (ANS), 0 when the user started the chat (USR)
    FString Callee;                  // +0x98 the account CAL invites

    __declspec(noinline) FString* GetSessionIDRef() { return &SessionID; }   // 0x1012DFB0 (4 bytes: lea eax, [ecx + 0x64]): the stock's callers call it
    __declspec(noinline) NMsnFriend* GetLastFriend() { return LastFriend; }  // 0x1012E010 (4 bytes: mov eax, [ecx + 0x7C])

    // Hook build: the stock constructor's body (0x10153B60, ret 0x20: the base with transaction 1, the stock vtable 0x101DC0C8 written by
    // hand). Standalone build: the real constructor (the compiler writes the vtable). The destructor's body: the vtable, the members map
    // emptied, the strings, the base's destruction
    SwitchboardSession* ConstructSession(Msn* InOwner, NMsnServer InServer, FString InSessionID);
#if !NW_HOOK_MODE
    SwitchboardSession(Msn* InOwner, const NMsnServer& InServer, const FString& InSessionID);
#endif
    void Destroy();                                              // 0x10153C80

    // the vtable (0x101DC0C8: the base's slots 0, 2, 4, 6, 8, 10 are stock's 0x10043B50, 0x10153C70, 0x10160FE0, 0x100023C0, 0x10160FC0, 0x10163490)
    virtual INT OnConnect(WPARAM wParam, LPARAM lParam);         //  9 0x10153400 (ret 8): WSAAsyncSelect, then USR or ANS by bInvited
    virtual INT IsRequestKept(INT RequestType);                  //  7 0x101528D0 (ret 4): the ANS request (0x14) stays in Pending
    virtual void OnClosed();                                     //  1 0x101528C0 (OnClosedImpl)
    virtual void OnAsync(MsnLine Line);                          //  3 0x10154520 (OnAsyncImpl)
    virtual void OnReply(INT RequestType, MsnLine Line);         //  5 0x10153E60 (OnReplyImpl)
    void OnClosedImpl();                                         //  1 0x101528C0: the manager drops this session (by the socket)
    void OnAsyncImpl(MsnLine Line);                              //  3 0x10154520 (ret 0x3C, guard name ProcessNotifyMessage): MSG / JOI / BYE
    void OnReplyImpl(INT RequestType, MsnLine Line);             //  5 0x10153E60 (ret 0x40, the request type FIRST): USR -> CAL, CAL -> its answer, ANS -> its answer

    // the senders (a command built and sent as the stock's locals: NMsnLocalCommand)
    void SendUSR();                                              // 0x1012E1A0: "USR trid account cookie", type 0x12
    void SendANS();                                              // 0x1012E0C0: "ANS trid account cookie sessionID", type 0x14
    void SendCAL(MsnLine Reply);                                 // 0x1012E320 (ret 0x3C): "CAL trid callee", type 0x13
    void Close();                                                // 0x1012E270 (guard name): "OUT", no transaction

    // the answers
    void HandleCALReply(MsnLine Line);                           // 0x1012EB60 (ret 0x3C): the session id (argument 1), the callee entered, the manager told
    void HandleANSReply(MsnLine Line);                           // 0x1012ED80 (ret 0x3C): IRO (a member already there) or ANS OK (the manager told)
    void ProcessJOI(MsnLine Line);                               // 0x1012EC30 (ret 0x3C, guard name): a member joined
    void ProcessBYE(MsnLine Line);                               // 0x1012F140 (ret 0x3C, guard name): a member left

    // the messages (the payload of a MSG line is read from the socket and parsed)
    void ProcessMSG(MsnLine Line);                               // 0x1012F530 (ret 0x3C)
    void HandleMsgText(MsnLine Line, NMsnMessage Message);       // 0x1012DD00 (ret 0x94): a text message
    void HandleMsgControl(MsnLine Line, NMsnMessage Message);    // 0x1012DE70 (ret 0x94): a control message (typing)
    void HandleMsgInvite(MsnLine Line, NMsnMessage Message);     // 0x1012F320 (ret 0x94): an invitation
    void SendMSG(NMsnMessage Message);                           // 0x1012E5C0 (ret 0x58)

    // the room
    void AddFriend(NMsnFriend Friend);                           // 0x1012E8E0 (ret 0x44, guard name): the contact known to the manager (else a copy of the record, online), in Members
    FString GetSessionTitle();                                   // 0x1012E9F0 (ret 4, guard name): the members' display names joined with ","
    void GetMember(FString Account, NMsnFriend* Out);            // 0x1012E850 (ret 0x10): the record of that member copied to Out, when there is one
    void RemoveMember(FString Account);                          // 0x1012EF00 (ret 0xC)
    void SetCookie(FString NewCookie);                           // 0x1012DFC0 (ret 0xC)
};
NW_ASSERT_OFFSET(SwitchboardSession, SessionID, 0x64);
NW_ASSERT_OFFSET(SwitchboardSession, Cookie, 0x70);
NW_ASSERT_OFFSET(SwitchboardSession, LastFriend, 0x7C);
NW_ASSERT_OFFSET(SwitchboardSession, Members, 0x80);
NW_ASSERT_OFFSET(SwitchboardSession, bInvited, 0x94);
NW_ASSERT_OFFSET(SwitchboardSession, Callee, 0x98);
NW_ASSERT_SIZE(SwitchboardSession, 0xA4);
