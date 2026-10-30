/*=============================================================================
    MSNSocket.cpp - the messenger's connection base (C3 only). Stock: 0x10141070 .. 0x101412D0 (the socket object),
    0x1012CCD0 (the request table's Remove), 0x1013C220 (OnConnect), 0x1013C5A0 (Connect), 0x1013C5C0 (OnError),
    0x1013EA40 / 0x1013EA60 (the request table), 0x1013EA70 (the window message of the socket). The constructor
    (0x10140090) and the destructor (0x10140160) set the stock vtable and stay stock, with the derived classes'.

    Winsock goes through the stock image's own import thunks (NMsnProtocol.h): the hook DLL does not import ws2_32.
=============================================================================*/
#include "nwindow/MSNSocket.h"
#include "nwindow/Msn1Pending.h"
#include "nwindow/NConsoleWnd.h"
#include "nwindow/MsnUtil.h"
#include "nwindow/NChatMsg.h"

#if !NW_HOOK_MODE
#define _WINSOCK_DEPRECATED_NO_WARNINGS   // the stock calls WSAAsyncSelect, inet_addr, gethostbyname
#include <winsock2.h>   // the standalone imports ws2_32 itself
#endif

enum
{
    MSN_WM_SOCKET = 0x507,         // the window message WSAAsyncSelect posts for read / close events (0x506 for the connect)
    MSN_FD_EVENTS = 0x25           // FD_READ | FD_OOB | FD_CLOSE
};

/*-----------------------------------------------------------------------------
    NMsnIntMap: UE2's TMap<INT, INT>.
-----------------------------------------------------------------------------*/

// the stock's out-of-line "room for one more pair at the end" of the 12-byte pair arrays (0x10148F30; the size is not read) and the pair
// array's Remove(Index, 1) (0x101495B0), each a function of its own that the callers do not inline
__declspec(noinline) static void* __cdecl MsnNewIntPair(size_t Size, TArray<NMsnIntPair>& Array)
{
    return operator new(Size, Array);
}
__declspec(noinline) static void MsnRemoveIntPair(TArray<NMsnIntPair>& Array, INT Index)
{
    Array.Remove(Index, 1);
}

// 0x101514a0 (ret 4) - TMapBase::FindRef: the value stored under the key, 0 when there is none (the stock's callers pass the address of the key).
// Hooked in NConsoleWndAnon.cpp as NItemMapByKey::FindByKey: the same code serves every map of this layout
INT NMsnIntMap::FindRef(const INT& Key)
{
    for (INT i = Hash[Key & (HashCount - 1)]; i != -1; i = Pairs(i).HashNext)
        if (Pairs(i).Key == Key)
            return Pairs(i).Value;
    return 0;
}

// 0x10151770 (ret 8) - TMapBase::Set: the value of the pair of that key replaced, or a new pair at the end of the array, chained into the hash
// (the hash doubled when the pairs outgrow it). Hooked in MsnUtil.cpp as NMsnWndIntMap::SetWnd: the same code serves every map of this layout
void NMsnIntMap::Set(const INT& Key, const INT& Value)
{
    INT Number = Key;
    INT Index = Hash[Number & (HashCount - 1)];
    while (Index != -1)
    {
        NMsnIntPair& Found = Pairs(Index);
        if (Found.Key == Number)
        {
            Found.Value = Value;
            return;
        }
        Index = Found.HashNext;
    }
    NMsnIntPair* Pair = (NMsnIntPair*)MsnNewIntPair(0xC, Pairs);
    if (Pair)
    {
        Pair->Key = Key;
        Pair->Value = Value;
    }
    INT Slot = Pair->Key & (HashCount - 1);
    Pair->HashNext = Hash[Slot];
    Hash[Slot] = Pairs.Num() - 1;
    if (HashCount * 2 + 8 < Pairs.Num())
    {
        HashCount = HashCount * 2;
        Rehash();
    }
}

// 0x10089d40 - UE2's TMapBase::Rehash for numbers (the pairs {chain, key, value} of 12 bytes; the key is its own hash): a new table of
// HashCount heads, every pair chained into the head of its hash, the old table freed. Nineteen callers of every map of that layout
__declspec(noinline) void NMsnIntMap::Rehash()
{
    INT* NewHash = new (TEXT("HashMapHash")) INT[HashCount];
    for (INT i = 0; i < HashCount; i++)
        NewHash[i] = -1;
    for (INT i = 0; i < Pairs.Num(); i++)
    {
        NMsnIntPair& Pair = Pairs(i);
        INT iHash = Pair.Key & (HashCount - 1);
        Pair.HashNext = NewHash[iHash];
        NewHash[iHash] = i;
    }
    if (Hash)
        appFree(Hash);
    Hash = NewHash;
}
NW_HOOK_METHOD(0x10089d40, NMsnIntMap, Rehash)

// 0x101495b0 (ret 8) - TArray<T>::Remove(Index, Count) of the 12-byte pairs: UnTemplate.h's three checks, then FArray::Remove
typedef TArray<NMsnIntPair> TMsnIntPairArray;
NW_HOOK_METHOD(0x101495b0, TMsnIntPairArray, Remove)

// 0x10151830 (ret 4) - TMap::Remove(Key): every pair of that key goes (from the end); when any went, the hash shrinks while it
// is wider than the pairs need (twice the count plus 8) and is rebuilt; the number removed comes back
INT NMsnIntMap::Remove(const INT& Key)
{
    INT Removed = 0;
    for (INT i = Pairs.Num() - 1; i >= 0; i--)
    {
        if (Pairs(i).Key == Key)
        {
            MsnRemoveIntPair(Pairs, i);   // the pair array's Remove(i, 1)
            Removed++;
        }
    }
    if (Removed)
    {
        while (HashCount > Pairs.Num() * 2 + 8)
            HashCount /= 2;
        Rehash();
    }
    return Removed;
}
NW_HOOK_METHOD(0x10151830, NMsnIntMap, Remove)

/*-----------------------------------------------------------------------------
    The winsock calls, through the stock image's own import thunks (a `jmp [IAT]` each; the C4 image has them at 0x10170B02 .. 0x10170B44, in
    the order WSAGetLastError, connect, htons, gethostbyname, inet_addr, socket, WSAStartup, recv, send, WSAAsyncSelect, htonl, closesocket;
    read off the stock calls of NMsnSock::Connect 0x10165ae0, ReadLine 0x10165c30 and MSNSocket::OnConnect 0x10160c40 and the IAT names).
    NMsnProtocol.h (msn2's) has MsnWSAGetLastError and MsnRecv at the C3 order's addresses (0x10170B14, 0x10170B3E), which are
    gethostbyname and htonl in C4: this file uses its own.
-----------------------------------------------------------------------------*/
namespace
{
INT MsnLastError()                                           // WSAGetLastError, 0x10170B02
{
#if NW_HOOK_MODE
    typedef INT(__stdcall * Fn)();
    return ((Fn)::nw::Addr(0x10170B02))();
#else
    return WSAGetLastError();
#endif
}
INT MsnRecvBytes(UINT Socket, char* Buffer, INT Length, INT Flags)   // recv, 0x10170B2C
{
#if NW_HOOK_MODE
    typedef INT(__stdcall * Fn)(UINT, char*, INT, INT);
    return ((Fn)::nw::Addr(0x10170B2C))(Socket, Buffer, Length, Flags);
#else
    return recv(Socket, Buffer, Length, Flags);
#endif
}
}

/*-----------------------------------------------------------------------------
    NMsnSock.
-----------------------------------------------------------------------------*/

// 0x10165a90 - the handle cleared, the two strings empty
NMsnSock::NMsnSock()
{
    hSock = 0;
}
NW_HOOK_CTOR(0x10165a90, NMsnSock)

// 0x10166260 - the socket closed, then the two strings go
NMsnSock::~NMsnSock()
{
    Close();
}
NW_HOOK_DTOR(0x10166260, NMsnSock)

// 0x10165c30 - one line taken from the socket a byte at a time (recv of 1) up to and including the LF, read as UTF-8
// (0x10144260) into Line; the byte count. 0 when the peer closed (recv gave 0: Line is left alone); a recv error (-1) ends the
// line where it stands, as an LF would
INT NMsnSock::ReadLine()
{
    char Ascii[0x1040];
    TCHAR Wide[0x1040];
    char Ch;
    INT Count = 0;
    appMemset(Ascii, 0, 0x1040);
    appMemset(Wide, 0, 0x2080);
    INT Got = MsnRecvBytes(hSock, &Ch, 1, 0);
    while (Got != -1)
    {
        if (Got == 0)
            return 0;
        Ascii[Count++] = Ch;
        Ascii[Count] = 0;
        if (Ch == '\n')
            break;
        Got = MsnRecvBytes(hSock, &Ch, 1, 0);
    }
    MsnUtf8ToWide(Ascii, Wide);
    Line = Wide;
    return Count;
}
NW_HOOK_METHOD(0x10165c30, NMsnSock, ReadLine)

namespace
{
// the winsock calls only 0x101410C0 makes, through the stock image's import thunks (stdcall)
UINT MsnSocket(INT Family, INT Type, INT Protocol)           // socket, 0x10170B20
{
#if NW_HOOK_MODE
    typedef UINT(__stdcall * Fn)(INT, INT, INT);
    return ((Fn)::nw::Addr(0x10170B20))(Family, Type, Protocol);
#else
    return socket(Family, Type, Protocol);
#endif
}
DWORD MsnInetAddr(const char* Text)                          // inet_addr, 0x10170B1A
{
#if NW_HOOK_MODE
    typedef DWORD(__stdcall * Fn)(const char*);
    return ((Fn)::nw::Addr(0x10170B1A))(Text);
#else
    return inet_addr(Text);
#endif
}
BYTE* MsnGetHostByName(const char* Name)                     // gethostbyname, 0x10170B14: a hostent
{
#if NW_HOOK_MODE
    typedef BYTE*(__stdcall * Fn)(const char*);
    return ((Fn)::nw::Addr(0x10170B14))(Name);
#else
    return (BYTE*)gethostbyname(Name);
#endif
}
WORD MsnHtons(WORD Value)                                    // htons, 0x10170B0E
{
#if NW_HOOK_MODE
    typedef WORD(__stdcall * Fn)(WORD);
    return ((Fn)::nw::Addr(0x10170B0E))(Value);
#else
    return htons(Value);
#endif
}
INT MsnConnect(UINT Socket, const void* Address, INT Length) // connect, 0x10170B08
{
#if NW_HOOK_MODE
    typedef INT(__stdcall * Fn)(UINT, const void*, INT);
    return ((Fn)::nw::Addr(0x10170B08))(Socket, Address, Length);
#else
    return connect(Socket, (const sockaddr*)Address, Length);
#endif
}
}

// 0x10165ae0 (ret 8) - a TCP socket asked for the connect event (window message 0x506 to the game window); the host as a dotted
// address or looked up by name (the first address of the hostent into the sockaddr); the connect made. 1 when it is under way
// (WSAEWOULDBLOCK, 0x2733, is the usual answer), 0 when the socket, the select, the lookup or the connect failed (the socket is
// closed only after a refused connect)
INT NMsnSock::Connect(const TCHAR* Host, INT Port)
{
    char Ascii[0x104];
    hSock = MsnSocket(2, 1, 6);
    if (hSock == (UINT)-1)
        return 0;
    if (MsnWSAAsyncSelect(hSock, GViewportHWnd, 0x506, 0x10) != 0)
    {
        MsnLastError();
        return 0;
    }
    appMemset(Ascii, 0, 0x104);
    MsnWideToAnsi(Host, Ascii);
    DWORD Address = MsnInetAddr(Ascii);
    if (Address != (DWORD)-1)
        *(DWORD*)&SockAddr[4] = Address;
    else
    {
        BYTE* Ent = MsnGetHostByName(Ascii);
        HostEnt = Ent;
        if (!Ent)
            return 0;
        INT Length = *(SWORD*)(Ent + 0xA);                       // h_length
        memcpy(&SockAddr[4], **(BYTE***)(Ent + 0xC), Length);    // *h_addr_list
    }
    *(WORD*)&SockAddr[0] = 2;                                    // AF_INET
    *(WORD*)&SockAddr[2] = MsnHtons((WORD)Port);
    FString::Printf(TEXT("Connect: Host-%s, PortNo-%d"), Host, Port);
    if (MsnConnect(hSock, SockAddr, 0x10) == -1 && MsnLastError() != 0x2733)
    {
        MsnCloseSocket(hSock);
        return 0;
    }
    FString::Printf(TEXT("Connect: Succeeded hSock-%d"), hSock);
    return 1;
}
NW_HOOK_METHOD(0x10165ae0, NMsnSock, Connect)

// 0x10165d80 - the log line of the close (built and dropped), then the handle closed and cleared
void NMsnSock::Close()
{
    FString::Printf(TEXT("End: CloseSocket hSock-%d"), hSock);
    if (hSock)
    {
        MsnCloseSocket(hSock);
        hSock = 0;
    }
}
NW_HOOK_METHOD(0x10165d80, NMsnSock, Close)

// 0x10165cf0 (ret 4) - Count bytes taken from the socket (one recv; the buffer is cleared first and ended after Count
// bytes), read as UTF-8 (0x10144260) into Line
INT NMsnSock::ReceiveBytes(INT Count)
{
    char Ascii[0x1040];
    TCHAR Wide[0x1040];
    appMemset(Ascii, 0, 0x1040);
    appMemset(Wide, 0, 0x2080);
    MsnRecvBytes(hSock, Ascii, Count, 0);
    Ascii[Count] = 0;
    MsnUtf8ToWide(Ascii, Wide);
    Line = Wide;
    return 0;
}
NW_HOOK_METHOD(0x10165cf0, NMsnSock, ReceiveBytes)

/*-----------------------------------------------------------------------------
    MSNSocket.
-----------------------------------------------------------------------------*/

// 0x10160c40 (ret 8, vslot 9) - WSAAsyncSelect said the connect is done: an error in the high word of lParam is the answer;
// else the socket is asked for read / out-of-band / close events as window message 0x507 to the game window, and the connection
// is announced (OnConnected). A failing WSAAsyncSelect gives its WSAGetLastError
INT MSNSocket::OnConnect(WPARAM wParam, LPARAM lParam)
{
    INT Error = (lParam >> 16) & 0xFFFF;
    if (Error != 0)
        return Error;
    if (MsnWSAAsyncSelect(Sock.hSock, GViewportHWnd, MSN_WM_SOCKET, MSN_FD_EVENTS) != 0)
        return MsnLastError();
    OnConnected();
    return 0;
}
NW_HOOK_METHOD(0x10160c40, MSNSocket, OnConnect)

// 0x10160fc0 (vslot 8) - connect to the server of the object
INT MSNSocket::Connect()
{
    return Sock.Connect(*Server.Host, Server.Port);
}
NW_HOOK_METHOD(0x10160fc0, MSNSocket, Connect)

// 0x10160fe0 (ret 0x3C, vslot 4) - an error line (by value): its verb, the error number, goes to the manager's error dialog
void MSNSocket::OnErrorImpl(MsnLine Line)
{
    Owner->NotifyError(Line.Verb);
}
typedef void (MSNSocket::*SigMSNSocketOnError)(MsnLine);
NW_HOOK_VARARGS(0x10160fe0, MSNSocket, OnErrorImpl, SigMSNSocketOnError)

// 0x10163460 (ret 4) - the request type is kept under the transaction id of the next command
void MSNSocket::RegisterPending(INT RequestType)
{
    Pending.Set(NextTrID, RequestType);
}
NW_HOOK_METHOD(0x10163460, MSNSocket, RegisterPending)

// 0x10163480 (ret 4) - the request of that transaction answered: out of the table
void MSNSocket::RemovePending(INT TrID)
{
    Pending.Remove(TrID);
}
NW_HOOK_METHOD(0x10163480, MSNSocket, RemovePending)

// 0x10163490 (ret 8, vslot 10) - the window message of the socket: the event in the low word of lParam, its error in the high word. A close
// (0x20) is announced (OnClosed), with an error too. A read: one line is taken from the socket and parsed; an error number goes to OnError,
// an unprompted verb to OnAsync, and any other line is the answer to the request of its transaction (OnReply, after IsRequestKept said
// whether the request stays in the table) or, when nothing waits for it, OnCommand. Every handler gets its own copy of the line.
// C4: on Windows 98 (GRunningOS 1) the engine's net handler refreshes the WSAAsyncSelect first (slot 54); the stock measures the line
// (MsnCommand::GetLength 0x1014C0C0, the result unused) and reads its two flags through the getters 0x10088580 (bError, +0x38) and 0x1014BBD0
// (bAsync, +0x34), which it does not inline (here the fields are read directly)
void MSNSocket::OnSocketMessage(WPARAM wParam, LPARAM lParam)
{
    if (((lParam >> 16) & 0xFFFF) != 0)
    {
        if ((lParam & 0xFFFF) == 0x20)
            OnClosed();
        return;
    }
    INT Event = lParam & 0xFFFF;
    if (Event == 1)
    {
        if (GRunningOS == 1)
            GConsole->Engine->NetHandler->RefreshWSAAsyncSelect();
        MsnLine Line;
        if (Sock.ReadLine() > 0)
        {
            if (!Line.Parse(Sock.Line))
                return;
            Line.GetLength();
            if (Line.bError)
                OnError(Line);
            else if (Line.bAsync)
                OnAsync(Line);
            else
            {
                INT Request = Pending.FindRef(Line.TrID);
                if (Request)
                {
                    if (!IsRequestKept(Request))
                        RemovePending(Line.TrID);
                    OnReply(Request, Line);
                }
                else
                    OnCommand(Line);
            }
        }
    }
    else if (Event == 0x20)
        OnClosed();
}
NW_HOOK_METHOD(0x10163490, MSNSocket, OnSocketMessage)

/*-----------------------------------------------------------------------------
    The handshake the dispatch and notification connections share.
-----------------------------------------------------------------------------*/

// 0x1013bdc0 (ret 0x1C, not ours) - the command's text (CRLF ended) in the server's encoding sent over the socket, the transaction
// counter up. The command goes by value (a copy; the stock function releases it)
void MSNSocket::Send(const MsnCommand& Command)
{
    SendLine(NMsnLocalCommand(Command));
}

void MSNSocket::SendPayload(const MsnCommand& Command)
{
    SendRaw(NMsnLocalCommand(Command));
}

// the functions that make the strings of the CVR line (cdecl; the result is the hidden argument, the string they build there)
static FString MsnCallString(FString* (__cdecl *Fn)(FString*))
{
    FString Result;
    Fn(&Result);
    return Result;
}

// 0x10163680 (the slot 0 of the dispatch and notification classes) - "VER trid MSNP8 CVR0" (request type 1)
void MSNSocket::SendVER()
{
    NMsnLocalCommand Command(FString(TEXT("VER")));
    Command.TrID = NextTrID;
    Command.AddArg(FString(TEXT("MSNP8")));
    Command.AddArg(FString(TEXT("CVR0")));
    RegisterPending(1);
    Send(Command);
}
NW_HOOK_METHOD(0x10163680, MSNSocket, SendVER)

// 0x10163750 (ret 0x3C) - the VER was answered: "CVR trid locale os version cpu ncsoft 1.0.0 l2chronicle3-kr account" (type 2).
// The locale (Util::GetLanguageID 0x10143C30) and the three system names (0x10143D70, 0x10143DF0, 0x10143E50) are the dlg / msn2
// lanes' functions
void MSNSocket::SendCVR(MsnLine Reply)
{
    NMsnLocalCommand Command(FString(TEXT("CVR")));
    Command.TrID = NextTrID;
    Command.AddArg(MsnCallString(UtilGetLanguageID));
    Command.AddArg(MsnCallString(MsnPlatformName));
    Command.AddArg(MsnCallString(MsnSystemVersion));
    Command.AddArg(MsnCallString(MsnProcessorName));
    Command.AddArg(FString(TEXT("ncsoft")));
    Command.AddArg(FString(TEXT("1.0.0")));
    Command.AddArg(FString(TEXT("l2chronicle3-kr")));
    Command.AddArg(Owner->GetAccount());
    RegisterPending(2);
    Send(Command);
}
typedef void (MSNSocket::*SigMSNSocketSendCVR)(MsnLine);
NW_HOOK_VARARGS(0x10163750, MSNSocket, SendCVR, SigMSNSocketSendCVR)

// 0x101638d0 (ret 0x3C) - the CVR was answered: "USR trid TWN I account" (type 3, the dispatch connection). Not in the db's map; the C4
// code is the notification connection's twin (USR, TWN, I, the account, request type 3)
void MSNSocket::SendUSRInitDS(MsnLine Reply)
{
    NMsnLocalCommand Command(FString(TEXT("USR")));
    Command.TrID = NextTrID;
    Command.AddArg(FString(TEXT("TWN")));
    Command.AddArg(FString(TEXT("I")));
    Command.AddArg(Owner->GetAccount());
    RegisterPending(3);
    Send(Command);
}
typedef void (MSNSocket::*SigMSNSocketSendUSRInitDS)(MsnLine);
NW_HOOK_VARARGS(0x101638d0, MSNSocket, SendUSRInitDS, SigMSNSocketSendUSRInitDS)

// 0x101639d0 (ret 0x3C) - the same on the notification connection (type 4)
void MSNSocket::SendUSRInitNS(MsnLine Reply)
{
    NMsnLocalCommand Command(FString(TEXT("USR")));
    Command.TrID = NextTrID;
    Command.AddArg(FString(TEXT("TWN")));
    Command.AddArg(FString(TEXT("I")));
    Command.AddArg(Owner->GetAccount());
    RegisterPending(4);
    Send(Command);
}
typedef void (MSNSocket::*SigMSNSocketSendUSRInitNS)(MsnLine);
NW_HOOK_VARARGS(0x101639d0, MSNSocket, SendUSRInitNS, SigMSNSocketSendUSRInitNS)

/*-----------------------------------------------------------------------------
    Construction and destruction.
-----------------------------------------------------------------------------*/

// 0x10164a40 - the hash freed and cleared (the count too), then the pair array's destructor
NMsnIntMap::~NMsnIntMap()
{
    if (Hash)
        appFree(Hash);
    Hash = NULL;
    HashCount = 0;
}
NW_HOOK_DTOR(0x10164a40, NMsnIntMap)

// 0x10163330 - the destructor of the array of the request table's pairs (the instance for 12-byte pairs): Remove(0, Num) with
// UnTemplate.h's checks, then ~FArray
typedef TArray<NMsnIntPair> TMsnIntPairArray;
NW_HOOK_DTOR(0x10163330, TMsnIntPairArray)

#if NW_HOOK_MODE
// 0x10164ab0 (ret 0x18) - the socket object, the stock vtable (0x101DD840), the default server (messenger.hotmail.com, 1863) then the
// one given, the request table (a hash of 8 heads), the manager and the first transaction id
MSNSocket* MSNSocket::Construct(Msn* InOwner, NMsnServer InServer, INT InTrID)
{
    new (&Sock) NMsnSock();
    *(void**)this = ::nw::Addr(0x101DD840);
    new (&Server) NMsnServer();
    new (&Pending.Pairs) TArray<NMsnIntPair>();
    Pending.Hash = NULL;
    Pending.HashCount = 8;
    Pending.Rehash();
    Owner = InOwner;
    Server.Host = InServer.Host;
    Server.Port = InServer.Port;
    NextTrID = InTrID;
    return this;
}
typedef MSNSocket* (MSNSocket::*SigMSNSocketConstruct)(Msn*, NMsnServer, INT);
NW_HOOK_VARARGS(0x10164ab0, MSNSocket, Construct, SigMSNSocketConstruct)
#else
// the standalone's constructor: the compiler writes the vtable and makes the socket (NMsnSock) and the default server (messenger.hotmail.com,
// 1863) before the body, which gives the server its host and port, the request table its hash (8 heads) and the manager and the first
// transaction id their places: the same as the stock's body above
MSNSocket::MSNSocket(Msn* InOwner, const NMsnServer& InServer, INT InTrID) : Owner(InOwner), NextTrID(InTrID)
{
    Server.Host = InServer.Host;
    Server.Port = InServer.Port;
    Pending.Hash = NULL;
    Pending.HashCount = 8;
    Pending.Rehash();
}
#endif

// 0x10164b80 - the base's destruction: the stock vtable, the manager forgotten, the request table emptied (a hash of 8 heads) and
// destroyed, the server's host, the socket
void MSNSocket::DestroyBase()
{
#if NW_HOOK_MODE
    *(void**)this = ::nw::Addr(0x101DD840);
#endif
    if (Owner)
        Owner = NULL;
    Pending.Pairs.Empty();
    Pending.HashCount = 8;
    Pending.Rehash();
    Pending.~NMsnIntMap();
    Server.Host.~FString();
    Sock.~NMsnSock();
}
NW_HOOK_METHOD(0x10164b80, MSNSocket, DestroyBase)

// 0x10153c70 (ret 0x3C, vslot 2 of the base, the dispatch and the switchboard classes) - a line with no transaction that nobody waits for
// is ignored: the argument is destroyed
void MSNSocket::OnCommandIgnore(MsnLine Line)
{
}
typedef void (MSNSocket::*SigMSNSocketOnCommandIgnore)(MsnLine);
NW_HOOK_VARARGS(0x10153c70, MSNSocket, OnCommandIgnore, SigMSNSocketOnCommandIgnore)

// 0x10164f20 (ret 0x3C, vslot 3 of the base and the dispatch classes) - an unprompted line: MSG ("MSG account name length") has its
// payload (length bytes) read from the socket and cut into lines at each CRLF (a line keeps its CRLF; a blank line is "\r\n"; the
// rest without a CRLF is the last line), which the message parses (0x1012A270); the parsed message goes to OnMessage (virtual 6).
// Any other verb is ignored
void MSNSocket::OnAsyncImpl(MsnLine Line)
{
    if (Line.Verb == TEXT("MSG"))
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
        OnMessage(&Message);
    }
}
typedef void (MSNSocket::*SigMSNSocketOnAsync)(MsnLine);
// Waits for msn2's ~NMsnMessage (0x1014efe0, NChatMsg.cpp): the stock runs until the tree has that lane (Msn1Pending.h: MSN1_AFTER_MSN2 is 1 by
// itself once include/nwindow/Msn2Pending.h exists; proved in the merged tree)
NW_HOOK_VARARGS(0x10164f20, MSNSocket, OnAsyncImpl, SigMSNSocketOnAsync)

// 0x10164b70 (ret 0x40, slot 5 of the base's vtable 0x101AB138) - the answer to a request that nobody handles is ignored: the line given
// by value is destroyed (0x10129D80) and the request type dropped
void MSNSocket::OnReplyIgnore(INT RequestType, MsnLine Line)
{
}
typedef void (MSNSocket::*SigMSNSocketOnReplyIgnore)(INT, MsnLine);
NW_HOOK_VARARGS(0x10164b70, MSNSocket, OnReplyIgnore, SigMSNSocketOnReplyIgnore)

#if !NW_HOOK_MODE
/*-----------------------------------------------------------------------------
    The vtable of the base (the stock's 0x101DD840, 11 slots). The standalone build gets it from the compiler: each slot is one of the
    functions above, the ones with a line by value through the non-virtual implementations the hooks sit on. The derived classes override
    the slots that differ (DispatchProcess.cpp, NotificationProcess.cpp, SwitchboardSession.cpp).
-----------------------------------------------------------------------------*/

// slot 0 and slot 1 (0x10043B50: a lone ret, shared with every empty function of the image)
void MSNSocket::OnConnected()
{
}
void MSNSocket::OnClosed()
{
}
// slot 2 (0x10153C70)
void MSNSocket::OnCommand(MsnLine Line)
{
    OnCommandIgnore(Line);
}
// slot 3 (0x10164F20)
void MSNSocket::OnAsync(MsnLine Line)
{
    OnAsyncImpl(Line);
}
// slot 4 (0x10160FE0)
void MSNSocket::OnError(MsnLine Line)
{
    OnErrorImpl(Line);
}
// slot 5 (0x10164B70)
void MSNSocket::OnReply(INT RequestType, MsnLine Line)
{
    OnReplyIgnore(RequestType, Line);
}
// slot 6 (0x100023C0: ret 4) and slot 7 (0x100023D0: xor eax, eax; ret 4)
void MSNSocket::OnMessage(NMsnMessage* Message)
{
}
INT MSNSocket::IsRequestKept(INT RequestType)
{
    return 0;
}
#endif
