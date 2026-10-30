/*=============================================================================
    SwitchboardSession.cpp - one conversation of the messenger (C3 only). Stock: 0x1012DCE0 .. 0x1012F930 (the senders, the
    answers, the room). The constructor (0x1012EF60) and the destructor (0x1012F090) write the stock vtable by hand.

    Wave 5 added, further down: the TMap / TArray instances the chat, task and window tables use (NTaskMap, the people map, the
    windows by name, copies), and the window-side functions the manager's session notices reach (Task / TaskBox / TaskWnd, ChatWnd's
    typing notice, SetSession, join / leave notices, the 4-argument constructor, the MsnWnd session events, the person tree's
    Rebuild, the alarm box). They are methods of types derived from the windows' classes (adding nothing), hooked with the window
    as `this`. 0x10119B20 (appStrihash) is not hooked (regcheck reads its partial cx as a register argument); every caller of it is
    ported and uses NChatTypes.cpp's NStrihash, so the stock copy is dead (db/morto.csv).
=============================================================================*/
#if !NW_HOOK_MODE
#define _WINSOCK_DEPRECATED_NO_WARNINGS   // the stock calls WSAAsyncSelect, inet_addr, gethostbyname
#include <winsock2.h>   // the standalone imports ws2_32 itself
#endif
#include "nwindow/SwitchboardSession.h"
#include "nwindow/NChatTypes.h"
#include "nwindow/NChatMsg.h"
#include "nwindow/TaskWnd.h"
#include "nwindow/TaskBox.h"
#include "nwindow/Task.h"
#include "nwindow/MsnWnd.h"
#include "nwindow/NConsoleWnd.h"
#include "nwindow/NString.h"
#include "nwindow/ChatWnd.h"
#include "nwindow/ChatBox.h"
#include "nwindow/InviteWnd.h"
#include "nwindow/NCEditBox.h"
#include "nwindow/NCTree.h"
#include "nwindow/MsnTreeCtrl.h"
#include "nwindow/AlarmWnd.h"
#include "nwindow/MsnWndOps.h"

/*-----------------------------------------------------------------------------
    The vtable's slots.
-----------------------------------------------------------------------------*/

// WSAGetLastError through the stock image's import thunk. C4 stage 4: NMsnProtocol.h's MsnWSAGetLastError names 0x10170B14, which is the
// thunk of gethostbyname in this image (the C3 -> C4 map of the eleven identical `jmp [IAT]` thunks is ambiguous); the thunk of
// WSAGetLastError is 0x10170B02 (the stock OnConnect calls it at 0x10153429). The same header's MsnRecv names 0x10170B3E (htonl), recv is
// 0x10170B2C: reported, the header is shared
static FORCEINLINE INT SbWSAGetLastError()
{
#if NW_HOOK_MODE
    typedef INT(__stdcall * Fn)();
    return ((Fn)::nw::Addr(0x10170B02))();
#else
    return WSAGetLastError();
#endif
}

// 0x10153400 (ret 8, vslot 9) - the connect is done: an error in the high word of lParam is the answer; else the socket
// asked for read / out-of-band / close events as window message 0x507; a chat the user started says USR, an invited one ANS
INT SwitchboardSession::OnConnect(WPARAM wParam, LPARAM lParam)
{
    INT Error = (lParam >> 16) & 0xFFFF;
    if (Error != 0)
        return Error;
    if (MsnWSAAsyncSelect(Sock.hSock, GViewportHWnd, 0x507, 0x25) != 0)
        return SbWSAGetLastError();
    if (bInvited == 0)
        SendUSR();
    else
        SendANS();
    return 0;
}
NW_HOOK_METHOD(0x10153400, SwitchboardSession, OnConnect)

// 0x101528d0 (ret 4, vslot 7) - only the answer to ANS stays in the table of requests
INT SwitchboardSession::IsRequestKept(INT RequestType)
{
    return RequestType == 0x14;
}
NW_HOOK_METHOD(0x101528d0, SwitchboardSession, IsRequestKept)

// 0x101528c0 (vslot 1) - the peer closed: the manager drops this session (found by its socket)
void SwitchboardSession::OnClosedImpl()
{
    Owner->RemoveSession(Sock.hSock);
}
NW_HOOK_METHOD(0x101528c0, SwitchboardSession, OnClosedImpl)

// 0x10154520 (ret 0x3C, vslot 3, guard name) - a line without a transaction: MSG (the payload follows), JOI (a member joined)
// or BYE (a member left); each handler gets its own copy of the line
void SwitchboardSession::OnAsyncImpl(MsnLine Line)
{
    guard(SwitchboardSession::ProcessNotifyMessage);
    if (Line.Verb == TEXT("MSG"))
        ProcessMSG(Line);
    else if (Line.Verb == TEXT("JOI"))
        ProcessJOI(Line);
    else if (Line.Verb == TEXT("BYE"))
        ProcessBYE(Line);
    unguard;
}
typedef void (SwitchboardSession::*SigSBOnAsync)(MsnLine);
NW_HOOK_VARARGS(0x10154520, SwitchboardSession, OnAsyncImpl, SigSBOnAsync)

// 0x10153e60 (ret 0x40, vslot 5) - the answer to a request: USR (0x12) goes on to CAL, CAL (0x13) and ANS (0x14) to theirs
void SwitchboardSession::OnReplyImpl(INT RequestType, MsnLine Line)
{
    if (RequestType == 0x12)
        SendCAL(Line);
    else if (RequestType == 0x13)
        HandleCALReply(Line);
    else if (RequestType == 0x14)
        HandleANSReply(Line);
}
typedef void (SwitchboardSession::*SigSBOnReply)(INT, MsnLine);
NW_HOOK_VARARGS(0x10153e60, SwitchboardSession, OnReplyImpl, SigSBOnReply)

/*-----------------------------------------------------------------------------
    The senders.
-----------------------------------------------------------------------------*/

// 0x10152da0 - "USR trid account cookie": the user starts the chat (request type 0x12)
void SwitchboardSession::SendUSR()
{
    NMsnLocalCommand Command(FString(TEXT("USR")));
    Command.TrID = NextTrID;
    Command.AddArg(Owner->GetAccount());
    Command.AddArg(Cookie);
    RegisterPending(0x12);
    Send(Command);
}
NW_HOOK_METHOD(0x10152da0, SwitchboardSession, SendUSR)

// 0x10152cc0 - "ANS trid account cookie sessionID": the user answers an invitation (request type 0x14)
void SwitchboardSession::SendANS()
{
    NMsnLocalCommand Command(FString(TEXT("ANS")));
    Command.TrID = NextTrID;
    Command.AddArg(Owner->GetAccount());
    Command.AddArg(Cookie);
    Command.AddArg(SessionID);
    RegisterPending(0x14);
    Send(Command);
}
NW_HOOK_METHOD(0x10152cc0, SwitchboardSession, SendANS)

// 0x10152f20 (ret 0x3C) - the USR was answered: "CAL trid callee" invites the other user (request type 0x13)
void SwitchboardSession::SendCAL(MsnLine Reply)
{
    NMsnLocalCommand Command(FString(TEXT("CAL")));
    Command.TrID = NextTrID;
    Command.AddArg(Callee);
    RegisterPending(0x13);
    Send(Command);
}
typedef void (SwitchboardSession::*SigSBSendCAL)(MsnLine);
NW_HOOK_VARARGS(0x10152f20, SwitchboardSession, SendCAL, SigSBSendCAL)

// 0x10152e70 (guard name) - "OUT": leave the session (no transaction)
void SwitchboardSession::Close()
{
    guard(SwitchboardSession::Close);
    NMsnLocalCommand Command(FString(TEXT("OUT")));
    Send(Command);
    unguard;
}
NW_HOOK_METHOD(0x10152e70, SwitchboardSession, Close)

/*-----------------------------------------------------------------------------
    The answers.
-----------------------------------------------------------------------------*/

// 0x10153760 (ret 0x3C) - the CAL was answered: argument 1 is the id of the session; the contact the user invited (found by
// the manager in the forward list) enters the room, and the manager tells the window the session started
void SwitchboardSession::HandleCALReply(MsnLine Line)
{
    FString Id = Line.GetArg(1);
    SessionID = Id;
    AddFriend(*Owner->FindFriend(Callee));
    Owner->NotifySessionStarted(Id);
}
typedef void (SwitchboardSession::*SigSBHandleCALReply)(MsnLine);
NW_HOOK_VARARGS(0x10153760, SwitchboardSession, HandleCALReply, SigSBHandleCALReply)

// 0x10153980 (ret 0x3C) - the answer to ANS: IRO lines list the people already in the room (argument 2 the account, 3 the
// name): each enters it; the "ANS OK" line tells the manager the session started
void SwitchboardSession::HandleANSReply(MsnLine Line)
{
    if (Line.Verb == TEXT("IRO"))
    {
        FString Account = Line.GetArg(2);
        FString Name = Line.GetArg(3);
        NMsnFriend Member(Account, Name);
        AddFriend(Member);
    }
    else if (Line.Verb == TEXT("ANS"))
    {
        FString Status = Line.GetArg(0);
        if (Status == TEXT("OK"))
            Owner->NotifySessionStarted(SessionID);
    }
}
typedef void (SwitchboardSession::*SigSBHandleANSReply)(MsnLine);
NW_HOOK_VARARGS(0x10153980, SwitchboardSession, HandleANSReply, SigSBHandleANSReply)

// 0x10153830 (ret 0x3C, guard name) - JOI: argument 0 the account, 1 the name of a member that joined: it enters the room and
// the manager tells the window
void SwitchboardSession::ProcessJOI(MsnLine Line)
{
    guard(SwitchboardSession::ProcessJOI);
    FString Account = Line.GetArg(0);
    FString Name = Line.GetArg(1);
    NMsnFriend Member(Account, Name);
    AddFriend(Member);
    Owner->NotifySessionJoin(SessionID, Member);
    unguard;
}
typedef void (SwitchboardSession::*SigSBProcessJOI)(MsnLine);
NW_HOOK_VARARGS(0x10153830, SwitchboardSession, ProcessJOI, SigSBProcessJOI)

// 0x10153d30 (ret 0x3C, guard name) - BYE: argument 0 is the account of a member that left: its record is taken out of the room
// and the manager tells the window
void SwitchboardSession::ProcessBYE(MsnLine Line)
{
    guard(SwitchboardSession::ProcessBYE);
    FString Account = Line.GetArg(0);
    NMsnFriend Member;
    GetMember(Account, &Member);
    RemoveMember(Account);
    Owner->NotifySessionLeave(SessionID, Member);
    unguard;
}
typedef void (SwitchboardSession::*SigSBProcessBYE)(MsnLine);
NW_HOOK_VARARGS(0x10153d30, SwitchboardSession, ProcessBYE, SigSBProcessBYE)

/*-----------------------------------------------------------------------------
    The room.
-----------------------------------------------------------------------------*/

// 0x101534e0 (ret 0x44, guard name) - a member enters the room: the manager's contact of that account (forward list) or, when it
// has none, a copy of the record made online (NLN); kept in Members under the account and as LastFriend
void SwitchboardSession::AddFriend(NMsnFriend Friend)
{
    guard(SwitchboardSession::AddFriend);
    NMsnFriend* Known = Owner->FindFriend(Friend.Account);
    if (!Known)
    {
        Known = new NMsnFriend(Friend);
        Known->SetStatus(FString(TEXT("NLN")));
    }
    Members.Set(*Friend.Account, (void**)&Known);
    LastFriend = Known;
    unguard;
}
typedef void (SwitchboardSession::*SigSBAddFriend)(NMsnFriend);
NW_HOOK_VARARGS(0x101534e0, SwitchboardSession, AddFriend, SigSBAddFriend)

// 0x101535f0 (ret 4: the hidden result, guard name) - the display names of the members one after the other, each followed by a
// comma, then without the last comma
FString SwitchboardSession::GetSessionTitle()
{
    guard(SwitchboardSession::GetSessionTitle);
    FString Title;
    for (INT i = 0; i < Members.Pairs.Num(); i++)
    {
        NMsnFriend* Member = (NMsnFriend*)Members.Pairs(i).Value;
        if (Member)
        {
            Title += Member->GetDisplayName();
            Title += TEXT(",");
        }
    }
    Title = Title.Left(Title.Len() - 1);
    return FString(Title);
    unguard;
}
NW_HOOK_METHOD(0x101535f0, SwitchboardSession, GetSessionTitle)

// 0x10153450 (ret 0x10) - the record of the member of that account (by value) copied to Out, when there is one
void SwitchboardSession::GetMember(FString Account, NMsnFriend* Out)
{
    FString Key(*Account);
    NMsnFriend* Member = (NMsnFriend*)Members.Find(Key);
    if (Member)
        *Out = *Member;
}
typedef void (SwitchboardSession::*SigSBGetMember)(FString, NMsnFriend*);
NW_HOOK_VARARGS(0x10153450, SwitchboardSession, GetMember, SigSBGetMember)

// 0x10153b00 (ret 0xC) - the member of that account (by value) leaves the map
void SwitchboardSession::RemoveMember(FString Account)
{
    Members.Remove(*Account);
}
typedef void (SwitchboardSession::*SigSBRemoveMember)(FString);
NW_HOOK_VARARGS(0x10153b00, SwitchboardSession, RemoveMember, SigSBRemoveMember)

// 0x10152ba0 (ret 0xC) - a new cookie (by value)
void SwitchboardSession::SetCookie(FString NewCookie)
{
    Cookie = NewCookie;
}
typedef void (SwitchboardSession::*SigSBSetCookie)(FString);
NW_HOOK_VARARGS(0x10152ba0, SwitchboardSession, SetCookie, SigSBSetCookie)

/*-----------------------------------------------------------------------------
    The messages of the room.
-----------------------------------------------------------------------------*/

// 0x101531c0 (ret 0x58) - "MSG trid N" with the message (by value) for the stock's sender of messages (0x1013C040: the headers and
// the text of the message follow the line). The stock first makes the message (its own copy) a cancel of the invitation (Kind 3,
// stored into the argument at the entry: `mov [esp + 0x48], 3`), which the payload builder turns into the CANCEL text of its cookie
void SwitchboardSession::SendMSG(NMsnMessage Message)
{
    Message.Kind = NMSN_CancelInvite;
    NMsnLocalCommand Command(FString(TEXT("MSG")));
    Command.TrID = NextTrID;
    Command.AddArg(FString(TEXT("N")));
    SendMessageBody(Command, MsnMessageArgOf(Message));
}
typedef void (SwitchboardSession::*SigSBSendMSG)(NMsnMessage);
NW_HOOK_VARARGS(0x101531c0, SwitchboardSession, SendMSG, SigSBSendMSG)

// 0x101528e0 (ret 0x94) - a text message (type 2): argument 0 the account and 1 the name of the sender; the manager gets the session
// id, the sender and the message
void SwitchboardSession::HandleMsgText(MsnLine Line, NMsnMessage Message)
{
    FString Account = Line.GetArg(0);
    FString Name = Line.GetArg(1);
    NMsnFriend Sender(Account, Name);
    Owner->NotifySessionMessage(SessionID, Sender, Message);
}
typedef void (SwitchboardSession::*SigSBHandleMsgText)(MsnLine, NMsnMessage);
NW_HOOK_VARARGS(0x101528e0, SwitchboardSession, HandleMsgText, SigSBHandleMsgText)

// 0x10152a50 (ret 0x94) - a control message (type 1, the typing notice): the manager gets the session id and the sender
void SwitchboardSession::HandleMsgControl(MsnLine Line, NMsnMessage Message)
{
    FString Account = Line.GetArg(0);
    FString Name = Line.GetArg(1);
    NMsnFriend Sender(Account, Name);
    Owner->NotifySessionEvent(SessionID, Sender);
}
typedef void (SwitchboardSession::*SigSBHandleMsgControl)(MsnLine, NMsnMessage);
NW_HOOK_VARARGS(0x10152a50, SwitchboardSession, HandleMsgControl, SigSBHandleMsgControl)

// 0x10153f10 (ret 0x94) - an invitation (type 3): a message that carries only its Invitation-Cookie is sent back, then the manager
// gets the session id, the sender and the invitation
void SwitchboardSession::HandleMsgInvite(MsnLine Line, NMsnMessage Message)
{
    NMsnMessage Reply;
    Reply.Cookie = Message.Cookie;
    SendMSG(Reply);
    FString Account = Line.GetArg(0);
    FString Name = Line.GetArg(1);
    NMsnFriend Sender(Account, Name);
    Owner->NotifySessionMessage2(SessionID, Sender, Message);
}
typedef void (SwitchboardSession::*SigSBHandleMsgInvite)(MsnLine, NMsnMessage);
NW_HOOK_VARARGS(0x10153f10, SwitchboardSession, HandleMsgInvite, SigSBHandleMsgInvite)

// 0x10154120 (ret 0x3C) - "MSG account name length": the payload (length bytes) is read from the socket and cut into lines at
// each CRLF (a line keeps its CRLF; a blank line is "\r\n"; the rest without a CRLF is the last line); the message parses
// them (0x1012A270) and the handler of its type (1 control, 2 text, 3 invitation) gets the line and the message
void SwitchboardSession::ProcessMSG(MsnLine Line)
{
    NMsnMessage Message;
    INT Length = Line.GetArgInt(2);
    TArray<FString*> Lines;
    FString Unused;
    Sock.ReceiveBytes(Length);
    FString& Text = Sock.Line;
    INT Cr = Text.InStr(TEXT("\r\n"), 0);
    while (Cr != -1)
    {
        FString* Part;
        if (Cr == 0)
        {
            Part = new FString(TEXT("\r\n"));
            Lines(Lines.Add(1)) = Part;
            Text = Text.Right(Text.Len() - 2);
        }
        else
        {
            Part = new FString(Text.Left(Cr + 2));
            Lines(Lines.Add(1)) = Part;
            Text = Text.Right(Text.Len() - Cr - 2);
        }
        Cr = Text.InStr(TEXT("\r\n"), 0);
    }
    if (Text.Len() > 0)
    {
        FString* Rest = new FString(Text);
        Lines(Lines.Add(1)) = Rest;
    }
    Message.Parse(Lines);
    for (INT i = 0; i < Lines.Num(); i++)
        if (Lines(i))
            delete Lines(i);
    Lines.Empty();
    if (Message.Kind == 1)
        HandleMsgControl(Line, Message);
    else if (Message.Kind == 2)
        HandleMsgText(Line, Message);
    else if (Message.Kind == 3)
        HandleMsgInvite(Line, Message);
}
typedef void (SwitchboardSession::*SigSBProcessMSG)(MsnLine);
NW_HOOK_VARARGS(0x10154120, SwitchboardSession, ProcessMSG, SigSBProcessMSG)

/*-----------------------------------------------------------------------------
    Construction and destruction.
-----------------------------------------------------------------------------*/

#if NW_HOOK_MODE
// 0x10153b60 (ret 0x20) - the base with the first transaction id, the stock vtable 0x101DC0C8, the strings, the map of members (a hash
// of 8 heads, rehashed), the session id, not invited, no last friend
SwitchboardSession* SwitchboardSession::ConstructSession(Msn* InOwner, NMsnServer InServer, FString InSessionID)
{
    MSNSocket::Construct(InOwner, InServer, 1);
    *(void**)this = ::nw::Addr(0x101DC0C8);
    new (&SessionID) FString();
    new (&Cookie) FString();
    new (&Members.Pairs) TArray<NMsnStrPair>();
    Members.Hash = NULL;
    Members.HashCount = 8;
    Members.Rehash();
    new (&Callee) FString();
    SessionID = InSessionID;
    bInvited = 0;
    LastFriend = NULL;
    return this;
}
typedef SwitchboardSession* (SwitchboardSession::*SigSBConstruct)(Msn*, NMsnServer, FString);
NW_HOOK_VARARGS(0x10153b60, SwitchboardSession, ConstructSession, SigSBConstruct)
#else
// the standalone's constructor: the base with the first transaction id (the compiler writes the vtable and makes the three strings), the map
// of members (a hash of 8 heads, rehashed), the session id, not invited, no last friend
SwitchboardSession::SwitchboardSession(Msn* InOwner, const NMsnServer& InServer, const FString& InSessionID) : MSNSocket(InOwner, InServer, 1)
{
    Members.Hash = NULL;
    Members.HashCount = 8;
    Members.Rehash();
    SessionID = InSessionID;
    bInvited = 0;
    LastFriend = NULL;
}

// the vtable's overrides (0x101DC0C8; the others are MSNSocket's): slot 1 (0x101528C0) the close, slot 3 (0x10154520) the unprompted lines,
// slot 5 (0x10153E60) the answers
void SwitchboardSession::OnClosed()
{
    OnClosedImpl();
}
void SwitchboardSession::OnAsync(MsnLine Line)
{
    OnAsyncImpl(Line);
}
void SwitchboardSession::OnReply(INT RequestType, MsnLine Line)
{
    OnReplyImpl(RequestType, Line);
}
#endif

// The out-of-line instances of UE2's TArray the stock calls as functions of their own (the compiler would inline the template into every
// caller): the name pairs' Empty(Slack) (0x10140F00: NChatNameArray::Empty, hooked in NChatTypes.cpp), the pointer arrays' destructor
// (0x101480C0) and Remove(Index, 1) (0x1001F3F0), the 12-byte pairs' and list items' Remove(Index, 1) (0x101495B0) and the rooms for one more
// element at the end of the arrays of pointers (0x10152C00), of name pairs (0x101561D0, NChatNameNew) and of 12-byte pairs (0x10148F30)
typedef TArray<void*> NMsnPtrArray;
__declspec(noinline) static void MsnEmptyNamePairs(void* Array, INT Slack)
{
    ((NChatNameArray*)Array)->Empty(Slack);
}
__declspec(noinline) static void MsnRemoveNamePair(void* Array, INT Index)
{
    ((NChatNameArray*)Array)->Remove(Index, 1);
}
__declspec(noinline) static void MsnFreePtrArray(void* Array)
{
    ((NMsnPtrArray*)Array)->~NMsnPtrArray();
}
__declspec(noinline) static void MsnRemovePtr(NMsnPtrArray& Array, INT Index)
{
    Array.Remove(Index, 1);
}
template <class A> __declspec(noinline) static void MsnRemoveFirst(A& Array)
{
    Array.Remove(0, 1);
}
__declspec(noinline) static void* __cdecl MsnNewPtrElement(size_t Size, NMsnPtrArray& Array)
{
    return operator new(Size, Array);
}
__declspec(noinline) static void* __cdecl MsnNewTaskPair(size_t Size, TArray<NTaskPair>& Array)
{
    return operator new(Size, Array);
}

// 0x10153c80 - the stock vtable, the members' pairs emptied (0x1011C390: the pairs destroyed) and the hash of 8 heads rebuilt, no last
// friend, the callee string, the map destroyed (0x1011D420), the cookie and the session id, then the base's destruction
void SwitchboardSession::Destroy()
{
#if NW_HOOK_MODE
    *(void**)this = ::nw::Addr(0x101DC0C8);
#endif
    MsnEmptyNamePairs(&Members, 0);
    Members.HashCount = 8;
    Members.Rehash();
    LastFriend = NULL;
    Callee.~FString();
    ((NChatNameMap*)&Members)->~NChatNameMap();
    Cookie.~FString();
    SessionID.~FString();
    DestroyBase();
}
NW_HOOK_METHOD(0x10153c80, SwitchboardSession, Destroy)

// 0x10152b90 (4 bytes) - the address of the session id; 0x1012e010 (4 bytes) - the contact entered last
NW_HOOK_METHOD(0x10152b90, SwitchboardSession, GetSessionIDRef)
NW_HOOK_METHOD(0x10152bf0, SwitchboardSession, GetLastFriend)

/*-----------------------------------------------------------------------------
    The tables the messenger's windows keep (UE2's TMap / TArray instances: the stock has one out-of-line copy of each, called by
    the window functions further down). The classes are the chat and window lanes' headers; the functions below are written as
    methods of types derived from them (adding nothing) so that they can be hooked with the table as `this`.
-----------------------------------------------------------------------------*/

// 0x10119b20 (cdecl; NChatTypes.cpp's NStrihash is this function) - UE2's appStrihash, the hash of the string maps' keys. Not hooked:
// regcheck reads the stock's `mov cx, [esi]` followed by CH as a register argument (the wave 4 chat lane found the same)

// the stock's out-of-line "new element at the end of the array" of each pair type (cdecl: the size, the array; they give the new
// element). 0x1011a010 ignores its size and adds a pair of 0x54 bytes; 0x10131560 a pointer; 0x10131580 a pair of 0x14 bytes
static FORCEINLINE void* NewPointerElement(void* Array) { return MsnNewPtrElement(4, *(NMsnPtrArray*)Array); }
static FORCEINLINE void* NewNamePairElement(void* Array) { return NChatNameNew(0x14, *(TArray<NChatNamePair>*)Array); }

// 0x1013e890 (cdecl) - a new pair of the people map (0x54 bytes) at the end of its array
__declspec(noinline) static NChatPeerPair* __cdecl NewPeerPairElement(INT /*the size: unused*/, FArray* Array)
{
    INT Index = Array->Add(1, sizeof(NChatPeerPair));
    return (NChatPeerPair*)Array->GetData() + Index;
}
NW_HOOK_FUNC(0x1013e890, NewPeerPairElement)

/*-----------------------------------------------------------------------------
    TMap<INT, NChatPeer>: the people of a conversation (ChatWnd +0x910).
-----------------------------------------------------------------------------*/

struct NMsnPeerPairInit : public NChatPeerPair
{
    __declspec(noinline) NMsnPeerPairInit* Construct(const INT* InKey, const NChatPeer* InValue);   // 0x1011B420 (ret 8)
};

// 0x1013ff90 (ret 8) - the pair made from a key and a person: the key, the two words, a copy of the user (the chain link is left)
NMsnPeerPairInit* NMsnPeerPairInit::Construct(const INT* InKey, const NChatPeer* InValue)
{
    Key = *InKey;
    Value.Unknown00 = InValue->Unknown00;
    Value.TimerID = InValue->TimerID;
    new (&Value.User) NChatUser(InValue->User);
    return this;
}
NW_HOOK_METHOD(0x1013ff90, NMsnPeerPairInit, Construct)

struct NMsnPeerMapSet : public NChatPeerMap
{
    NChatPeer* SetPeer(const INT* InKey, const NChatPeer* InValue);   // 0x1011D240 (ret 8)
};

// 0x10141db0 (ret 8) - TMap::Set: the person already under the key is overwritten (the two words, the user assigned); else a new
// pair is made at the end of the array (0x1011B420), chained into the hash, and the hash doubled when the pairs outgrow it
// (0x1011B390). The place of the person comes back
NChatPeer* NMsnPeerMapSet::SetPeer(const INT* InKey, const NChatPeer* InValue)
{
    INT Index = Hash[*InKey & (HashCount - 1)];
    while (Index != -1)
    {
        NChatPeerPair& Found = Pairs(Index);
        if (Found.Key == *InKey)
        {
            Found.Value.Unknown00 = InValue->Unknown00;
            Found.Value.TimerID = InValue->TimerID;
            *(NMsnFriend*)&Found.Value.User = *(const NMsnFriend*)&InValue->User;
            return &Pairs(Index).Value;
        }
        Index = Found.HashNext;
    }
    NChatPeerPair* Pair = NewPeerPairElement(0x54, &Pairs);
    if (Pair)
        Pair = (NChatPeerPair*)((NMsnPeerPairInit*)Pair)->Construct(InKey, InValue);
    INT Slot = Pair->Key & (HashCount - 1);
    Pair->HashNext = Hash[Slot];
    Hash[Slot] = Pairs.Num() - 1;
    if (HashCount * 2 + 8 < Pairs.Num())
    {
        HashCount = HashCount * 2;
        Rehash();
    }
    return &Pair->Value;
}
NW_HOOK_METHOD(0x10141db0, NMsnPeerMapSet, SetPeer)

struct NMsnChatPeerInit : public NChatPeer
{
    NMsnChatPeerInit* Construct(INT InUnknown, INT InTimerID, NChatUser InUser);   // 0x1011A1C0 (ret 0x4C)
};

// 0x1013ea20 (ret 0x4C) - a person: an empty user (0x1011A140), the two words and the user given by value assigned to it (0x1012C5B0);
// the copy that came by value is destroyed at the end (0x1012C530)
NMsnChatPeerInit* NMsnChatPeerInit::Construct(INT InUnknown, INT InTimerID, NChatUser InUser)
{
    new (&User) NChatUser();
    TimerID = InTimerID;
    Unknown00 = InUnknown;
    *(NMsnFriend*)&User = *(const NMsnFriend*)&InUser;
    return this;
}
typedef NMsnChatPeerInit* (NMsnChatPeerInit::*SigChatPeerInit)(INT, INT, NChatUser);
NW_HOOK_VARARGS(0x1013ea20, NMsnChatPeerInit, Construct, SigChatPeerInit)

/*-----------------------------------------------------------------------------
    TMap<NCWnd*, NCWnd*>: the task windows' map (TaskWnd / L2TaskWnd +0xC8: a conversation window -> its Task).
-----------------------------------------------------------------------------*/

// the hash of a key that is an object: its index (+4), 0 for none
static FORCEINLINE INT ObjectHash(NCWnd* Key)
{
    return Key ? ::nw::Field<INT>(Key, 4) : 0;
}

// 0x101494e0 (ret 4) - the Task of that conversation window (the key by address), NULL for none
NCWnd* NMsnTaskMapOps::FindTask(NCWnd* const* InKey)
{
    NCWnd* Wnd = *InKey;
    INT Index = Hash[ObjectHash(Wnd) & (HashCount - 1)];
    while (Index != -1)
    {
        NTaskPair& Pair = Pairs(Index);
        if (Pair.Key == Wnd)
            return Pair.Value;
        Index = Pair.HashNext;
    }
    return NULL;
}
NW_HOOK_METHOD(0x101494e0, NMsnTaskMapOps, FindTask)

// 0x10149530 (ret 8) - a new pair (0x1011A030 makes room at the end of the array) with the key and the value, chained into the
// hash; the hash doubled (Rehash 0x10124A60) when the pairs outgrow it. The place of the value comes back
NCWnd** NMsnTaskMapOps::AddTask(NCWnd* const* InKey, NCWnd* const* InValue)
{
    NTaskPair* Pair = (NTaskPair*)MsnNewTaskPair(0xC, Pairs);
    if (Pair)
    {
        Pair->Key = *InKey;
        Pair->Value = *InValue;
    }
    INT Slot = ObjectHash(Pair->Key) & (HashCount - 1);
    Pair->HashNext = Hash[Slot];
    Hash[Slot] = Pairs.Num() - 1;
    if (HashCount * 2 + 8 < Pairs.Num())
    {
        HashCount = HashCount * 2;
        Rehash();
    }
    return &Pair->Value;
}
NW_HOOK_METHOD(0x10149530, NMsnTaskMapOps, AddTask)

// 0x10140d80 (ret 8) - TMap::Set: the value of the pair of that key replaced, or a new pair (AddTask)
NCWnd** NMsnTaskMapOps::SetTask(NCWnd* const* InKey, NCWnd* const* InValue)
{
    NCWnd* Wnd = *InKey;
    INT Index = Hash[ObjectHash(Wnd) & (HashCount - 1)];
    while (Index != -1)
    {
        NTaskPair& Pair = Pairs(Index);
        if (Pair.Key == Wnd)
        {
            Pairs(Index).Value = *InValue;
            return &Pairs(Index).Value;
        }
        Index = Pair.HashNext;
    }
    return AddTask(InKey, InValue);
}
NW_HOOK_METHOD(0x10140d80, NMsnTaskMapOps, SetTask)

// 0x10140c80 (ret 8) - the Task of that conversation window (the map at +0xC8, found by the window) keeps the session at +0xC8
void NMsnTaskWndOps::SetTaskSession(void* InSession, NCWnd* ChatWindow)
{
    NCWnd* Entry = ((NMsnTaskMapOps&)Tasks).FindTask(&ChatWindow);
    if (Entry)
        ((Task*)Entry)->Unknown_C8 = (INT)InSession;
}
NW_HOOK_METHOD(0x10140c80, NMsnTaskWndOps, SetTaskSession)

/*-----------------------------------------------------------------------------
    TMap<FString, T*>: the windows by name (MsnWnd +0xCC, +0xE0), the people by account (ChatWnd +0x924).
-----------------------------------------------------------------------------*/

// 0x101561b0 (ret 8) - the pair of a name and a window: the key built from the text, the window (the chain link is left)
NMsnWndStrPairInit* NMsnWndStrPairInit::Construct(const TCHAR* InKey, NCWnd* const* InValue)
{
    new (&Key) FString(InKey);
    Wnd = *InValue;
    return this;
}
NW_HOOK_METHOD(0x101561b0, NMsnWndStrPairInit, Construct)

// 0x10151560 (ret 8) - TMapBase::Add: a new pair at the end of the array (0x101561D0) made from the text and the window (0x101561B0), linked
// into the hash chain of the key's hash; the table doubles and is rebuilt (0x1013FC50) when it is not more than twice the number of pairs
// plus 8. The address of the window comes back. Hooked in NConsoleWndAnon.cpp as NMsnStrMapOps::Add (the same code serves every map of
// this layout); this is the copy the windows' tables call
NCWnd** NMsnWndStrMapOps::AddWnd(const TCHAR* InKey, NCWnd* const* InValue)
{
    NMsnWndStrPair* Pair = (NMsnWndStrPair*)NewNamePairElement(this);
    if (Pair)
        Pair = ((NMsnWndStrPairInit*)Pair)->Construct(InKey, InValue);
    UINT HashValue = NStrihash(*Pair->Key);
    INT Index = HashValue & (HashCount - 1);
    Pair->HashNext = Hash[Index];
    Hash[Index] = Num - 1;
    if (HashCount * 2 + 8 < Num)
    {
        HashCount = HashCount * 2;
        ((NChatNameMap*)this)->Rehash();
    }
    return &Pair->Wnd;
}

// 0x10151e60 (ret 4) - every pair of that key removed (0x1013FEF0), from the last to the first; when some went, the hash is made smaller
// while it is more than twice the pairs plus 8, and rebuilt; the number removed comes back. Hooked in NConsoleWndAnon.cpp as
// NMsnStrMapOps::RemoveKey (the same code); this is the copy the windows' tables call
INT NMsnWndStrMapOps::RemoveKey(const TCHAR* Key)
{
    INT Removed = 0;
    for (INT i = Num - 1; i >= 0; i--)
    {
        if (Data[i].Key == Key)
        {
            MsnRemoveNamePair(this, i);
            Removed++;
        }
    }
    if (Removed)
    {
        if (HashCount > Num * 2 + 8)
        {
            do
                HashCount = HashCount / 2;
            while (HashCount > Num * 2 + 8);
        }
        ((NChatNameMap*)this)->Rehash();
    }
    return Removed;
}

// 0x10159410 (ret 8) - TMultiMap::MultiFind: every window entered under that name is added to the array (0x10131560)
void NMsnWndStrMapOps::MultiFind(const FString* InKey, TArray<NCWnd*>* Out)
{
    INT Index = Hash[NStrihash(**InKey) & (HashCount - 1)];
    while (Index != -1)
    {
        if (Data[Index].Key == *InKey)
        {
            NCWnd** Element = (NCWnd**)NewPointerElement(Out);
            if (Element)
                *Element = Data[Index].Wnd;
        }
        Index = Data[Index].HashNext;
    }
}
NW_HOOK_METHOD(0x10159410, NMsnWndStrMapOps, MultiFind)

/*-----------------------------------------------------------------------------
    Copies of arrays and maps (the copy constructors of the stock's instances).
-----------------------------------------------------------------------------*/

// 0x10167130 (ret 4) - a copy of an array of pointers: room for the other's count, then each pointer added at the end
__declspec(noinline) NMsnPtrArrayCopy::NMsnPtrArrayCopy(const TArray<void*>& Src) : FArray(Src.Num(), sizeof(void*))
{
    ArrayNum = 0;
    for (INT i = 0; i < Src.Num(); i++)
    {
        void** Element = (void**)NewPointerElement(this);
        if (Element)
            *Element = Src(i);
    }
}
NW_HOOK_CTOR_ARGS(0x10167130, NMsnPtrArrayCopy, (const TArray<void*>& Src), (Src))

// 0x1013fd00 (ret 4) - a copy of the array of the pairs of a map by name: room for the other's count, then each pair added at the
// end (0x10131580) as a copy of the chain link, the key and the value
struct NMsnNameArrayCopy : public FArray
{
    __declspec(noinline) NMsnNameArrayCopy(const NChatNameArray& Src) : FArray(Src.Num(), sizeof(NChatNamePair))
    {
        ArrayNum = 0;
        for (INT i = 0; i < Src.Num(); i++)
        {
            NChatNamePair* Pair = (NChatNamePair*)NewNamePairElement(this);
            if (Pair)
            {
                const NChatNamePair& From = Src(i);
                Pair->HashNext = From.HashNext;
                new (&Pair->Key) FString(From.Key);
                Pair->Value = From.Value;
            }
        }
    }
};
NW_HOOK_CTOR_ARGS(0x1013fd00, NMsnNameArrayCopy, (const NChatNameArray& Src), (Src))

// 0x10141d50 (ret 4) - a copy of a map by name: the pairs copied (0x1011B2D0), no hash yet, the other's hash size, the hash built
NChatNameMap* NMsnNameMapCopier::CopyFrom(const NChatNameMap* Src)
{
    new (&Pairs) NMsnNameArrayCopy(Src->Pairs);
    Hash = NULL;
    HashCount = Src->HashCount;
    Rehash();
    return this;
}
NW_HOOK_METHOD(0x10141d50, NMsnNameMapCopier, CopyFrom)

struct NMsnMessageAssign : public NMsnMessage
{
    NMsnMessage* Assign(const NMsnMessage* Src);   // 0x101399C0 (ret 4)
};

// 0x1015e330 (ret 4) - a message assigned field by field: the kind, the three strings, the headers (the array assigned by 0x10129AD0,
// the hash size taken and the hash rebuilt), the font, the effects and the colour
NMsnMessage* NMsnMessageAssign::Assign(const NMsnMessage* Src)
{
    Kind = Src->Kind;
    Body = Src->Body;
    Unknown10 = Src->Unknown10;
    Cookie = Src->Cookie;
    ((NMsnHeaderArrayAssigner&)Headers.Pairs).Assign(Src->Headers.Pairs);
    Headers.HashCount = Src->Headers.HashCount;
    Headers.Rehash();
    FontName = Src->FontName;
    Effects = Src->Effects;
    Color = Src->Color;
    return this;
}
NW_HOOK_METHOD(0x1015e330, NMsnMessageAssign, Assign)

// 0x10165e60 (stdcall, ret 8) - the first Count characters of the text taken out: the rest (and its end) moved down
void __stdcall MsnShiftText(TCHAR* Text, INT Count)
{
    TCHAR* From = Text + Count;
    TCHAR Ch = *From;
    if (Ch)
    {
        TCHAR* To = Text;
        do
        {
            *To = Ch;
            From++;
            Ch = *From;
            To++;
        } while (Ch);
    }
    From[-Count] = 0;
}
NW_HOOK_FUNC(0x10165e60, MsnShiftText)

struct NMsnSessionMapFree : public NDataMap<SwitchboardSession>
{
    void Free();                                                             // 0x1012BF10
};

// 0x101509d0 - the manager's table of sessions (TMap<INT, SwitchboardSession*>, Msn +0x10): the hash freed and forgotten, then the
// pairs' array destroyed (0x1012B7B0)
void NMsnSessionMapFree::Free()
{
    if (Hash)
        appFree(Hash);
    typedef TArray<NDataMap<SwitchboardSession>::NPair> SessionPairArray;
    Pairs.~SessionPairArray();       // 0x1012B7B0
    Hash = NULL;                     // (the stock clears these two before the call; nothing in the call reads them, and a store
    HashCount = 0;                   //  after it keeps this a function that returns instead of a tail jump)
}
NW_HOOK_METHOD(0x101509d0, NMsnSessionMapFree, Free)

/*-----------------------------------------------------------------------------
    The task list (TaskWnd / TaskBox / Task) of the conversations. The window functions below are written as methods of types
    derived from the windows' classes (adding nothing), so that they can be hooked with the window as `this`.
-----------------------------------------------------------------------------*/

// 0x1011e760 (ret 0xC) - the constructor of a Task made for a conversation: the base window's, then the session, the conversation window
// and the alert flag in place of the cleared fields (the blink and the timer start empty)
struct NMsnTaskMaker : public Task
{
    __declspec(noinline) Task* MakeFor(INT InSession, NCWnd* InChat, INT bInAlert);   // 0x1011E760 (ret 0xC): `this` is raw memory
};

// 0x101432f0 (ret 0xC) - the same function, at its C4 address: the stock inlines Task::Task() and stores the three words
Task* NMsnTaskMaker::MakeFor(INT InSession, NCWnd* InChat, INT bInAlert)
{
    Task* Self = ::new ((void*)this) Task();
    Self->Unknown_C8 = InSession;
    Self->ChatWindow = InChat;
    Self->bAlert = bInAlert;
    return Self;
}
NW_HOOK_METHOD(0x101432f0, NMsnTaskMaker, MakeFor)

struct NMsnTaskBoxOps : public TaskBox
{
    void AddTask(UTexture* Icon, const TCHAR* Text, DWORD Param, INT Color);   // 0x1011C000 (ret 0x10)
};

// 0x10140b70 (ret 0x10) - a line for Text (nothing for NULL) with the Param, the icon and the color; when the box is full the oldest
// line goes
void NMsnTaskBoxOps::AddTask(UTexture* Icon, const TCHAR* Text, DWORD Param, INT Color)
{
    if (Text)
    {
        NString* Str = new NString(Text);
        Str->Color = Color;
        if (NumLines < MaxLines)
        {
            NumLines++;
            if (NumLines > VisibleLines)
                TopIndex++;
        }
        else
        {
            NString* Oldest = Items(0).Text;
            if (Oldest)
            {
                delete Oldest;
                Items(0).Text = NULL;
            }
            MsnRemoveFirst(Items);   // TListItemArray::Remove(0, 1)
        }
        INT i = Items.Add();
        Items(i).Text = Str;
        Items(i).Param = Param;
        Items(i).Unknown8 = (INT)Icon;
    }
}
NW_HOOK_METHOD(0x10140b70, NMsnTaskBoxOps, AddTask)

// 0x10141c30 (ret 4) - the Task whose session (at +0xC8) is that one, NULL for none
Task* NMsnTaskWndOps::FindTaskOfSession(void* InSession)
{
    for (INT i = 0; i < Tasks.Pairs.Num(); i++)
        if (((Task*)Tasks.Pairs(i).Value)->Unknown_C8 == (INT)InSession)
            return (Task*)Tasks.Pairs(i).Value;
    return NULL;
}
NW_HOOK_METHOD(0x10141c30, NMsnTaskWndOps, FindTaskOfSession)

// C2 CANDIDATE: the C2 function is not the same code as the C1 one; kept only while it matches the stock one (verify_data)
// 0x101435c0 (ret 0xC) - the conversation of that session goes into the list: when there is no Task of the session, one is made for the
// conversation window (a child of this window, kept in the map by it) and a line with the session's title and the messenger's icon is
// added; when there is one, its line is retitled and the alert flag is set unless the conversation window is shown
void NMsnTaskWndOps::AddTask(SwitchboardSession* InSession, NCWnd* ChatWindow, INT bAlert)
{
    Task* Existing = FindTaskOfSession(InSession);
    if (!Existing)
    {
        void* Mem = UObject::StaticAllocateObject(Task::StaticClass());
        Task* NewTask = Mem ? ((NMsnTaskMaker*)Mem)->MakeFor((INT)InSession, ChatWindow, bAlert) : NULL;
        NewTask->SetPosition(0, 0, 0, 0);
        CreateChild(NewTask, 1);
        NCWnd* Value = NewTask;
        ((NMsnTaskMapOps&)Tasks).SetTask(&ChatWindow, &Value);
        FString Title = InSession->GetSessionTitle();
        ((NMsnTaskBoxOps*)List)->AddTask(FindTexture(TEXT("L2UI_CH3.Msn.msnicon6")), *Title, (DWORD)NewTask, 0xFFDCDCDC);
    }
    else
    {
        {
            FString Title = InSession->GetSessionTitle();
            List->UpdateString(*Title, Existing);
        }
        if (!ChatWindow->IsShowWindow())
            Existing->bAlert = bAlert;
    }
}
NW_HOOK_METHOD(0x101435c0, NMsnTaskWndOps, AddTask)

// an array of pointers to windows, its destructor out of line in the stock (0x10136370: the checks of UnTemplate.h, Remove of all, ~FArray)
typedef TArray<NCWnd*> NMsnWndPtrArray;
NW_HOOK_DTOR(0x1015ae00, NMsnWndPtrArray)

/*-----------------------------------------------------------------------------
    ChatWnd: the people of the talk, the session, the notices of people coming and going.
-----------------------------------------------------------------------------*/

// the messenger's notice of a message to the console (0x10061190: a jump to the console's virtual 264; ret 0xC)
static FORCEINLINE void NMsnNotifyConsole(const TCHAR* Account, const TCHAR* Text, INT Flag)
{
    GConsole->RequestSendMsnChatLog((TCHAR*)Account, (TCHAR*)Text, Flag);
}

// 0x101561f0 (ret 0xC) - the string at +0x938 replaced
void NMsnChatOps::SetPeerAccount(FString Value)
{
    Unknown938 = Value;
}
typedef void (NMsnChatOps::*SigChatSetPeerAccount)(FString);
NW_HOOK_VARARGS(0x101561f0, NMsnChatOps, SetPeerAccount, SigChatSetPeerAccount)

// 0x101430c0 (ret 0x44) - a person is typing: the people already entered under that account (compared with each person's user) lose their
// timer and their entry; the person is entered under a new timer id (counted up from +0x9EC) with a timer of 5 seconds that will clear it
void NMsnChatOps::OnPeerTyping(NChatUser Who)
{
    for (INT i = 0; i < Peers.Pairs.Num(); i++)
    {
        UBOOL bSame;
        {
            NChatUser Known = Peers.Pairs(i).Value.GetUser();
            bSame = Known.Account == Who.Account;
        }
        if (bSame)
        {
            KillTimer(Peers.Pairs(i).Value.TimerID);
            Peers.RemoveKey(Peers.Pairs(i).Key);
        }
    }
    Unknown9EC = Unknown9EC + 1;
    INT Handle = SetTimer(Unknown9EC, 5000, 0);
    BYTE Storage[sizeof(NChatPeer)];
    NChatPeer* Peer = (NChatPeer*)((NMsnChatPeerInit*)Storage)->Construct(Unknown9EC, Handle, Who);
    ((NMsnPeerMapSet&)Peers).SetPeer(&Unknown9EC, Peer);
    Peer->User.~NChatUser();
}
typedef void (NMsnChatOps::*SigChatOnPeerTyping)(NChatUser);
NW_HOOK_VARARGS(0x101430c0, NMsnChatOps, OnPeerTyping, SigChatOnPeerTyping)

// 0x101419f0 (ret 4) - the conversation gets its session: the other party's record (the session's last contact) is taken, the messages that
// waited for the session are sent (0x1012E4D0) one by one, freed, and each person of the names is notified with the edit box's text
// (every other waiting message is skipped: the stock removes the one it has just sent and counts on); the invite window gets the session too
void NMsnChatOps::SetSession(SwitchboardSession* Session)
{
    if (Session)
    {
        Switchboard = Session;
        *(NMsnFriend*)&UserB = *Session->GetLastFriend();
        for (INT i = 0; i < PendingMessages.Num(); i++)
        {
            NMsnMessage* Pending = (NMsnMessage*)PendingMessages(i);
            if (Pending)
            {
                ((NChatSbSender*)Session)->SendText(MsnMessageArgOf(*Pending));
                Pending->~NMsnMessage();
                appFree(Pending);
                for (INT j = 0; j < Names.Pairs.Num(); j++)
                {
                    NChatUser* User = Names.Pairs(j).Value;
                    if (User)
                        NMsnNotifyConsole(*User->Account, EditBox->CommitText(), 0);
                }
            }
            MsnRemovePtr(PendingMessages, i);   // TArray<void*>::Remove(i, 1)
        }
        InviteWindow->Switchboard = Session;
    }
}
NW_HOOK_METHOD(0x101419f0, NMsnChatOps, SetSession)

// 0x101408f0 (ret 0x44) - "somebody joined": SystemMsg 1348 with $s1 the person's decoded name, shown in the talk with the messenger's icon
void NMsnChatOps::ShowJoinNotice(NChatUser User)
{
    FString Text(*GL2GameData.SystemMsg[1348].Message);
    Text = Text.Substitute(FString(TEXT("$s1")), User.GetDecodedName());
    TCHAR Line[260];
    appSprintf(Line, TEXT("%s"), *Text);
    TextBox->AddWithIcon((INT)FindTexture(TEXT("L2UI_CH3.Msn.msnicon1")), Line, 0xFF8CB48C);
}
typedef void (NMsnChatOps::*SigChatShowNotice)(NChatUser);
NW_HOOK_VARARGS(0x101408f0, NMsnChatOps, ShowJoinNotice, SigChatShowNotice)

// 0x10140a30 (ret 0x44) - "somebody left": SystemMsg 1349 with the icon msnicon7 and the color 0xFFE37171
void NMsnChatOps::ShowLeaveNotice(NChatUser User)
{
    FString Text(*GL2GameData.SystemMsg[1349].Message);
    Text = Text.Substitute(FString(TEXT("$s1")), User.GetDecodedName());
    TCHAR Line[260];
    appSprintf(Line, TEXT("%s"), *Text);
    TextBox->AddWithIcon((INT)FindTexture(TEXT("L2UI_CH3.Msn.msnicon7")), Line, 0xFFE37171);
}
NW_HOOK_VARARGS(0x10140a30, NMsnChatOps, ShowLeaveNotice, SigChatShowNotice)

// 0x10144300 (ret 0x9C) - the answer to an invitation: SystemMsg 1347 as a text message in Arial (color 8000), sent through the
// session when there is one (0x1012E4D0, the message by value) and shown in the talk under the player's own name; the person and the
// invitation that came are not read
void NMsnChatOps::OnInvitation(NChatUser From, NMsnMessage Msg)
{
    NMsnMessage Reply;
    TCHAR Text[520];
    appStrcpy(Text, *GL2GameData.SystemMsg[1347].Message);
    Reply.Body = FString(Text);
    Reply.FontName = FString(TEXT("Arial"));
    Reply.Color = 8000;
    Reply.Effects = FString(TEXT("l"));
    if (Switchboard)
    {
        ((NChatSbSender*)Switchboard)->SendText(MsnMessageArgOf(Reply));
        TextBox->AddMessage(Text, (NChatUser*)((::Msn*)Msn)->Me, 0xFF8CB48C);
    }
}
typedef void (NMsnChatOps::*SigChatOnInvitation)(NChatUser, NMsnMessage);
NW_HOOK_VARARGS(0x10144300, NMsnChatOps, OnInvitation, SigChatOnInvitation)

struct NMsnChatMaker : public ChatWnd
{
    __declspec(noinline) ChatWnd* MakeFor(INT InMessenger, INT InMsn, SwitchboardSession* InSession, NMsnPersonRec Me);   // 0x1011F1C0 (ret 0x50): `this` is raw memory
};

// 0x10143d50 (ret 0x50) - a conversation window made for a session: the empty window (the stock's constructor body, but the flag of
// the hidden state is left clear as the allocation left it), the messenger window, the Msn object and the session in their fields, the
// player's own record as the first user and the session's last contact as the second, and the setting that hides the warning read
ChatWnd* NMsnChatMaker::MakeFor(INT InMessenger, INT InMsn, SwitchboardSession* InSession, NMsnPersonRec Me)
{
    ChatWnd* Chat = ::new ((void*)this) ChatWnd();
    Chat->bHidden = 0;
    Chat->Messenger = (void*)InMessenger;
    Chat->Msn = (void*)InMsn;
    Chat->Switchboard = InSession;
    *(NMsnFriend*)&Chat->UserA = *(const NMsnFriend*)&Me;
    *(NMsnFriend*)&Chat->UserB = *InSession->GetLastFriend();
    if (!GConfig->GetBool(TEXT("URL"), TEXT("MSNHideWarnMsg"), (UBOOL&)GMsnHideWarnMsg, NULL))
        GMsnHideWarnMsg = 0;
    MsnPersonFree(&Me);
    return Chat;
}
typedef ChatWnd* (NMsnChatMaker::*SigChatMakeFor)(INT, INT, SwitchboardSession*, NMsnPersonRec);
NW_HOOK_VARARGS(0x10143d50, NMsnChatMaker, MakeFor, SigChatMakeFor)

/*-----------------------------------------------------------------------------
    The alarm box and the person tree.
-----------------------------------------------------------------------------*/

struct NMsnAlarmOps : public AlarmWnd
{
    __declspec(noinline) void SetMessage(NMsnPersonRec Who, NMsnMessage Msg, NCWnd* InTarget);   // 0x1013AC90 (ret 0xA0)
};

// 0x1015f670 (ret 0xA0) - the alarm of type 1: a shown box is hidden, the person and the message are kept, the window a click shows is set
void NMsnAlarmOps::SetMessage(NMsnPersonRec Who, NMsnMessage Msg, NCWnd* InTarget)
{
    if (IsShowWindow())
        HideWindow();
    AlarmType = 1;
    Person.Assign(&Who);
    ((NMsnMessageAssign&)Message).Assign(&Msg);
    Target = InTarget;
    MsnPersonFree(&Who);
}
typedef void (NMsnAlarmOps::*SigAlarmSetMessage)(NMsnPersonRec, NMsnMessage, NCWnd*);
NW_HOOK_VARARGS(0x1015f670, NMsnAlarmOps, SetMessage, SigAlarmSetMessage)

// the stock's copy of an array of pointers, made and unmade by its out-of-line instances (the copy constructor at CopyVA, 0x10123570 the destructor)
struct NMsnTempPtrArray
{
    BYTE Raw[0xC];
    NMsnTempPtrArray(const void* Src) { new ((void*)Raw) NMsnPtrArrayCopy(*(const TArray<void*>*)Src); }   // the copy constructors 0x101466B0 / 0x10167130 (one code)
    ~NMsnTempPtrArray() { MsnFreePtrArray(Raw); }
    INT Num() const { return ((const INT*)Raw)[1]; }
    void* At(INT i) const { return ((void* const*)((const INT*)Raw)[0])[i]; }
};

struct NMsnTreeCtrlOps : public MsnTreeCtrl
{
    void ResetRoot();                                            // 0x10142170
    __declspec(noinline) void Rebuild();                         // 0x101433D0
};

// 0x10166b20 - the tree starts again from an empty root
void NMsnTreeCtrlOps::ResetRoot()
{
    NTVItemData Data;
    ((NCTree*)MsnTreeWnd)->AddItem(&Data, NULL);
}
NW_HOOK_METHOD(0x10166b20, NMsnTreeCtrlOps, ResetRoot)

// 0x10167d90 - the tree built again: the control's slot 78 (Clear; C3 slot 72), a new root, a group entry for each group of the list of groups
// (0x10121B20 copies the array) and a person entry for each person of the forward list (0x10142780 copies it), the root opened, and the
// tree told where the lists are (C4: +0x21C the groups, +0x220 the forward list, +0x224 the block list; C3 +0x1FC, +0x200, +0x204)
void NMsnTreeCtrlOps::Rebuild()
{
    ::nw::VirtualCall<void>(this, 78);
    ResetRoot();
    NMsnTempPtrArray Groups((const void*)Unknown118);
    for (INT i = 0; i < Groups.Num(); i++)
    {
        NMsnGroupArg* Group = (NMsnGroupArg*)Groups.At(i);
        if (Group)
            AddGroupBody(this, NULL, MsnGroupCopy(*Group));
    }
    NMsnTempPtrArray People((BYTE*)FriendBook + 4);
    for (INT i = 0; i < People.Num(); i++)
    {
        NMsnPersonRec* Person = (NMsnPersonRec*)People.At(i);
        if (Person)
            AddFriendBody(this, NULL, MsnPersonCopy(*Person));
    }
    NCTree* Tree = (NCTree*)MsnTreeWnd;
    if (Tree->Root)
        Tree->Root->Expand();
    ::nw::Field<void*>(MsnTreeWnd, 0x21C) = (void*)Unknown118;
    ::nw::Field<void*>(MsnTreeWnd, 0x220) = FriendBook;
    ::nw::Field<void*>(MsnTreeWnd, 0x224) = (void*)Unknown120;
}
NW_HOOK_METHOD(0x10167d90, NMsnTreeCtrlOps, Rebuild)

/*-----------------------------------------------------------------------------
    MsnWnd: the chat windows by account and by session, the task list, the lists for the tree.
-----------------------------------------------------------------------------*/

// an empty array of windows that the stock's out-of-line destructor (0x10136370) takes down
struct NMsnTempWndArray
{
    NCWnd** Data;
    INT Num;
    INT Max;
    NMsnTempWndArray() : Data(NULL), Num(0), Max(0) {}
    ~NMsnTempWndArray() { MsnFreePtrArray(this); }
};

// ChatWnd::ReceiveInstantMessage (0x10144A30, ret 0x9C): the person (0x44) and the message (0x58) by value, both destroyed by the callee. Called
// by address, with a copy of each in the argument slots as the stock caller makes, so that this file does not depend on how the chat lane
// declares the message argument (msn1 declares it NMsnMessageArg, plain bytes) nor on the body being a stub
static FORCEINLINE void SbReceiveInstantMessage(ChatWnd* Chat, const NChatUser& From, const NMsnMessage& Msg)
{
    Chat->ReceiveInstantMessage(From, MsnMessageArgOf(Msg));
}


// 0x1015b920 - the first chat window of the table by number (+0xF4) whose flag at +0x8F8 is clear, NULL for none
ChatWnd* NMsnWndOps::FindFreeChat()
{
    for (INT i = 0; i < InviteWnds.Num; i++)
    {
        ChatWnd* Chat = (ChatWnd*)InviteWnds.Data[i].Wnd;
        if (Chat && Chat->Unknown8F8 == 0)
            return Chat;
    }
    return NULL;
}
NW_HOOK_METHOD(0x1015b920, NMsnWndOps, FindFreeChat)

// 0x1015b820 (ret 0xC) - the chat windows entered under that account (the table by name at +0xCC, MultiFind) and, of them, the first
// that is not a talk of several people (the flag at +0x8F4 is clear); NULL for none
ChatWnd* NMsnWndOps::FindOneToOneChat(FString Account)
{
    NMsnTempWndArray Found;
    {
        FString Key(*Account);
        ((NMsnWndStrMapOps&)ChatWnds).MultiFind(&Key, (TArray<NCWnd*>*)&Found);
    }
    for (INT i = 0; i < Found.Num; i++)
    {
        ChatWnd* Chat = (ChatWnd*)Found.Data[i];
        if (Chat && Chat->Unknown8F4 == 0)
            return Chat;
    }
    return NULL;
}
typedef ChatWnd* (NMsnWndOps::*SigWndFindOneToOneChat)(FString);
NW_HOOK_VARARGS(0x1015b820, NMsnWndOps, FindOneToOneChat, SigWndFindOneToOneChat)

// 0x1015aa10 (ret 0x48) - the typing notice of a session's person: the chat window of the session (the table by session id at +0xE0)
// is told (0x1011E530)
void NMsnWndOps::OnSessionTyping(SwitchboardSession* Session, NMsnFriend Friend)
{
    if (Session)
    {
        NCWnd* Chat = ((NMsnWndStrMapOps&)ChatNames).FindWnd(Session->GetSessionIDRef());
        if (Chat)
            ((NMsnChatOps*)Chat)->OnPeerTyping(*(const NChatUser*)&Friend);
    }
}
typedef void (NMsnWndOps::*SigWndOnSessionTyping)(SwitchboardSession*, NMsnFriend);
NW_HOOK_VARARGS(0x1015aa10, NMsnWndOps, OnSessionTyping, SigWndOnSessionTyping)

// 0x1015aa90 - the task list window (kept at +0x160) is made when there is none, as a child of the console's desktop in the lower right
// corner (172 x 132), then shown
void NMsnWndOps::ShowTaskWnd()
{
    if (!L2FriendWindow)
    {
        TaskWnd* Window = new TaskWnd;
        L2FriendWindow = Window;
        Window->SetPosition(GSEKScreenX - 0xAC, GSEKScreenY - 0x11A, 0xAC, 0x84);
        GConsole->Desktop->CreateChild(Window, 0x4002);
    }
    L2FriendWindow->ShowWindow();
}
NW_HOOK_METHOD(0x1015aa90, NMsnWndOps, ShowTaskWnd)

// 0x10155be0 - the person tree is given the lists of the manager (the user's own record, the groups, the forward and the block lists)
// and built again (the stock jumps to 0x101433D0)
void NMsnWndOps::GiveListsToTree()
{
    NMsnTreeCtrlOps* Tree = (NMsnTreeCtrlOps*)TreeCtrl;
    Tree->Unknown114 = (INT)MsnGetSelf(MsnObj);
    Tree->Unknown118 = (INT)MsnObj->GetContacts()->Groups;
    // C4 stage 4: the stock calls the getters of [Lists + 0] (the forward list, 0x1016EC20) and [Lists + 8] (the block list, 0x10005B90);
    // MsnListFriends / MsnListBlocked of MsnWnd.h name 0x10151140 ([+4], the allow list) and 0x10151160 ([+0x10], the groups) and are not used here
    Tree->FriendBook = MsnFirstDword(MsnGetLists(MsnObj));   // 0x1016EC20
    Tree->Unknown120 = (INT)MsnObj->GetContacts()->BL;
    ((NMsnTreeCtrlOps*)TreeCtrl)->Rebuild();
}
NW_HOOK_METHOD(0x10155be0, NMsnWndOps, GiveListsToTree)

// 0x1015b980 (ret 0x48) - a person joined a session: when no chat window is entered under the session id, a one-to-one chat with that
// account (FindOneToOneChat) gets the session (entered under the session id, roster made again, its Task told the session and updated);
// when there is one, it is told the person joined (the notice, the roster, the Task) and sends its warning
void NMsnWndOps::OnSessionJoin(SwitchboardSession* Session, NMsnFriend Friend)
{
    if (Session)
    {
        FString Account(Friend.Account);
        ChatWnd* Chat;
        UBOOL bMissing;
        {
            FString Id(**Session->GetSessionIDRef());
            Chat = (ChatWnd*)((NMsnWndStrMapOps&)ChatNames).FindWnd(&Id);
            bMissing = Chat == NULL;
        }
        if (bMissing)
        {
            Chat = FindOneToOneChat(FString(Account));
            if (Chat)
            {
                NCWnd* Value = Chat;
                ((NMsnWndStrMap&)ChatNames).SetWnd(**Session->GetSessionIDRef(), &Value);
                ((NMsnChatOps*)Chat)->SetSession(Session);
                Chat->SetRoster();
                if (L2FriendWindow)
                {
                    ((NMsnTaskWndOps*)L2FriendWindow)->SetTaskSession(Session, Chat);
                    ((NMsnTaskWndOps*)L2FriendWindow)->AddTask(Session, Chat, 0);
                }
            }
        }
        else
        {
            ((NMsnChatOps*)Chat)->ShowJoinNotice(*(const NChatUser*)&Friend);
            Chat->SetRoster();
            if (L2FriendWindow)
                ((NMsnTaskWndOps*)L2FriendWindow)->AddTask(Session, Chat, 0);
            Chat->SendWarning();
        }
    }
}
typedef void (NMsnWndOps::*SigWndOnSessionJoinLeave)(SwitchboardSession*, NMsnFriend);
NW_HOOK_VARARGS(0x1015b980, NMsnWndOps, OnSessionJoin, SigWndOnSessionJoinLeave)

// a copy of a session's table of members, made and unmade by the stock's out-of-line instances (0x1011D1E0 the copy, 0x1011D420 the destructor)
struct NMsnTempNameMap
{
    BYTE Raw[sizeof(NChatNameMap)];
    NMsnTempNameMap(const void* Src) { ((NMsnNameMapCopier*)Raw)->CopyFrom((const NChatNameMap*)Src); }
    ~NMsnTempNameMap() { ((NChatNameMap*)Raw)->~NChatNameMap(); }
    NChatNameMap& Map() { return *(NChatNameMap*)Raw; }
};

// 0x1015fe70 (ret 0x48) - a person left a session. With a chat window entered under the session id: a copy of the session's members is
// taken; when the chat's peer account (+0x938) is the one that left and members remain, the chat is entered under the account of the first
// remaining member instead (and takes it as its peer account); when members remain the leave notice, the roster and the Task are updated,
// and when none remains the chat and its invite window forget the session and the session id leaves the table
void NMsnWndOps::OnSessionLeave(SwitchboardSession* Session, NMsnFriend Friend)
{
    if (Session)
    {
        NMsnWndStrMapOps& Names = (NMsnWndStrMapOps&)ChatNames;
        ChatWnd* Chat;
        UBOOL bFound;
        {
            FString Id(**Session->GetSessionIDRef());
            Chat = (ChatWnd*)Names.FindWnd(&Id);
            bFound = Chat != NULL;
        }
        if (bFound)
        {
            NMsnTempNameMap Members(&Session->Members);
            if (Chat->Unknown938 == Friend.Account)
            {
                if (Members.Map().Pairs.Num() > 0)
                {
                    ((NMsnWndStrMap&)ChatWnds).RemoveWnd(*Chat->Unknown938, (NCWnd**)&Chat);
                    NChatNameIter It(Members.Map());
                    ((NMsnWndStrMapOps&)ChatWnds).AddWnd(*Members.Map().Pairs(0).Value->Account, (NCWnd* const*)&Chat);
                    ((NMsnChatOps*)Chat)->SetPeerAccount(FString(*Members.Map().Pairs(0).Value->Account));
                }
            }
            if (Members.Map().Pairs.Num() > 0)
            {
                ((NMsnChatOps*)Chat)->ShowLeaveNotice(*(const NChatUser*)&Friend);
                Chat->SetRoster();
                if (L2FriendWindow)
                    ((NMsnTaskWndOps*)L2FriendWindow)->AddTask(Session, Chat, 0);
            }
            else
            {
                ChatWnd* Known;
                UBOOL bKnown;
                {
                    FString Id(**Session->GetSessionIDRef());
                    Known = (ChatWnd*)Names.FindWnd(&Id);
                    bKnown = Known != NULL;
                }
                if (bKnown)
                {
                    Known->Switchboard = NULL;
                    Known->InviteWindow->Switchboard = NULL;
                }
                ((NMsnWndStrMapOps&)ChatNames).RemoveKey(**Session->GetSessionIDRef());
            }
        }
    }
}
NW_HOOK_VARARGS(0x1015fe70, NMsnWndOps, OnSessionLeave, SigWndOnSessionJoinLeave)

// 0x10160580 (ret 0xA0) - a text message in a session: the chat window entered under the session id receives it (and when it is not
// shown its Task starts to alert); without one the message starts a talk (0x1013B070)
void NMsnWndOps::OnSessionMessage(SwitchboardSession* Session, NMsnFriend Friend, NMsnMessage Message)
{
    if (Session)
    {
        ChatWnd* Chat = (ChatWnd*)((NMsnWndStrMapOps&)ChatNames).FindWnd(Session->GetSessionIDRef());
        if (Chat)
        {
            SbReceiveInstantMessage(Chat, *(const NChatUser*)&Friend, Message);
            if (!Chat->IsShowWindow())
            {
                if (L2FriendWindow)
                {
                    Task* Entry = ((NMsnTaskWndOps*)L2FriendWindow)->FindTaskOfSession(Session);
                    if (Entry)
                        Entry->bAlert = 1;
                }
            }
        }
        else
            OnSessionNewTalk(Session, Friend, Message);
    }
}
typedef void (NMsnWndOps::*SigWndOnSessionMessage)(SwitchboardSession*, NMsnFriend, NMsnMessage);
NW_HOOK_VARARGS(0x10160580, NMsnWndOps, OnSessionMessage, SigWndOnSessionMessage)

// 0x101606b0 (ret 0xA0) - an invitation in a session: the same, with the chat window's answer to an invitation (0x1011F770)
void NMsnWndOps::OnSessionInvite(SwitchboardSession* Session, NMsnFriend Friend, NMsnMessage Message)
{
    if (Session)
    {
        ChatWnd* Chat = (ChatWnd*)((NMsnWndStrMapOps&)ChatNames).FindWnd(Session->GetSessionIDRef());
        if (Chat)
        {
            ((NMsnChatOps*)Chat)->OnInvitation(*(const NChatUser*)&Friend, Message);
            if (!Chat->IsShowWindow())
            {
                if (L2FriendWindow)
                {
                    Task* Entry = ((NMsnTaskWndOps*)L2FriendWindow)->FindTaskOfSession(Session);
                    if (Entry)
                        Entry->bAlert = 1;
                }
            }
        }
        else
            OnSessionNewTalk(Session, Friend, Message);
    }
}
NW_HOOK_VARARGS(0x101606b0, NMsnWndOps, OnSessionInvite, SigWndOnSessionMessage)

// 0x1015fa50 (ret 0xA0) - a message from a session that has no chat window yet: the task list is shown; a one-to-one chat with the sender
// that has no session and a session of one member takes this session (title, peer account, Task); else a free chat window is reused or a
// new one made (4-argument constructor 0x1011F1C0) at a random place of the desktop (x 200 .. 399, y 150 .. 299, 256 x 401), given the
// session, entered under the account, titled, hidden, added to the task list as alerting and announced by the alarm box when the setting
// asks. The chat is then entered under the session id and receives the message (kind 2) or the invitation (kind 3)
void NMsnWndOps::OnSessionNewTalk(SwitchboardSession* Session, NMsnFriend Friend, NMsnMessage Message)
{
    if (Session)
    {
        ShowTaskWnd();
        FString Account(Friend.Account);
        ChatWnd* Chat = FindOneToOneChat(FString(Account));
        if (Chat && Session->Members.Pairs.Num() <= 1)
        {
            {
                FString Name = Friend.GetDisplayName();
                Chat->SetTitle(*Name);
            }
            ((NMsnChatOps*)Chat)->SetPeerAccount(FString(Account));
            Chat->Unknown8F8 = 1;
            ((NMsnChatOps*)Chat)->SetSession(Session);
            if (L2FriendWindow)
                ((NMsnTaskWndOps*)L2FriendWindow)->SetTaskSession(Session, Chat);
            if (!Chat->IsShowWindow())
            {
                if (L2FriendWindow)
                {
                    Task* Entry = ((NMsnTaskWndOps*)L2FriendWindow)->FindTaskOfSession(Session);
                    if (Entry)
                        Entry->bAlert = 1;
                }
            }
        }
        else
        {
            Chat = FindFreeChat();
            if (!Chat)
            {
                void* Mem = UObject::StaticAllocateObject(ChatWnd::StaticClass());
                Chat = Mem ? ((NMsnChatMaker*)Mem)->MakeFor((INT)this, (INT)MsnObj, Session, MsnPersonCopy(Me)) : NULL;
                INT X = appRand() % 200 + 200;
                INT Y = appRand() % 150 + 150;
                Chat->SetPosition(X, Y, 0x100, 0x191);
                Chat->AttachEntry(GConsole->Desktop->AddChildWindow(Chat, 0x2002));
                Chat->SendWarning();
            }
            else
                ((NMsnChatOps*)Chat)->SetSession(Session);
            ((NMsnWndStrMapOps&)ChatWnds).AddWnd(*Account, (NCWnd* const*)&Chat);
            {
                FString Name = Friend.GetDisplayName();
                Chat->SetTitle(*Name);
            }
            ((NMsnChatOps*)Chat)->SetPeerAccount(FString(Account));
            Chat->Unknown8F8 = 1;
            Chat->SetRoster();
            Chat->HideWindow();
            if (L2FriendWindow)
                ((NMsnTaskWndOps*)L2FriendWindow)->AddTask(Session, Chat, 1);
            if (GMsnMessageAlarm)
            {
                ((NMsnAlarmOps*)AlarmWindow)->SetMessage(MsnPersonCopy(*(const NMsnPersonRec*)&Friend), Message, Chat);
                AlarmWindow->ShowWindow();
            }
        }
        ((NMsnWndStrMap&)ChatNames).SetWnd(**Session->GetSessionIDRef(), (NCWnd**)&Chat);
        if (Message.Kind == 2)
            SbReceiveInstantMessage(Chat, *(const NChatUser*)&Friend, Message);
        else if (Message.Kind == 3)
            ((NMsnChatOps*)Chat)->OnInvitation(*(const NChatUser*)&Friend, Message);
    }
}
NW_HOOK_VARARGS(0x1015fa50, NMsnWndOps, OnSessionNewTalk, SigWndOnSessionMessage)
