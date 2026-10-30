#pragma once
// MSNSocket: the base of the messenger's connections (C3 only; a plain heap object with a vtable, vtable 0x101AB138 in the
// stock image, 0x64 bytes). A TCP socket (NMsnSock, at +4), the server it talks to, the counter of the transactions it
// sends and the table of the requests waiting for their answer (transaction id -> request type). The derived classes are
// DispatchProcess (vtable 0x101AB164), NotificationProcess (0x101AB190, names ours) and SwitchboardSession (0x101A9AB0).
//
// The window-message part: WSAAsyncSelect(socket, the game window, 0x506 for the connect, 0x507 for the data and the
// close); Msn passes those window messages to the socket that owns the handle (Msn::sub_1012B540 / sub_1012B5A0).

#include "nwindow/MsnCommand.h"
#include "nwindow/MsnStockArgs.h"
#include "nwindow/Msn.h"

// The socket itself, the first member of MSNSocket (+4; 0x34 bytes; name ours)
struct NMsnSock
{
    UINT hSock;                  // +0x00 the SOCKET, 0 when closed
    INT Unknown04;
    BYTE SockAddr[16];           // +0x08 the sockaddr_in of the server (family at +8, port at +0xA, address at +0xC)
    void* HostEnt;               // +0x18 the hostent of the name lookup
    FString Line;                // +0x1C the last text received (ReadLine, ReceiveBytes)
    FString Text;                // +0x28

    __declspec(noinline) NMsnSock();                                              // 0x10141070
    __declspec(noinline) ~NMsnSock();                                              // 0x10141840: Close, then the two strings
    __declspec(noinline) void Close();                           // 0x10141360: closesocket and the handle cleared
    INT ReceiveBytes(INT Count);                                 // 0x101412D0 (ret 4): Count bytes read (as UTF-8) into Line
    INT ReadLine();                                              // 0x10141210 (other lane): one line (up to LF) read into Line, the byte count (0 when the peer closed)
    INT Connect(const TCHAR* Host, INT Port);                    // 0x101410C0 (ret 8, other lane): 1 when the connect is under way
};
NW_ASSERT_OFFSET(NMsnSock, SockAddr, 0x08);
NW_ASSERT_OFFSET(NMsnSock, Line, 0x1C);
NW_ASSERT_OFFSET(NMsnSock, Text, 0x28);
NW_ASSERT_SIZE(NMsnSock, 0x34);

class MSNSocket
{
public:
    NMsnSock Sock;               // +0x04 (the vtable is at +0)
    Msn* Owner;                  // +0x38 the manager
    NMsnServer Server;           // +0x3C the server to connect to
    INT NextTrID;                // +0x4C the transaction id of the next command (Send counts it up)
    NMsnIntMap Pending;          // +0x50 transaction id -> request type, for the requests sent and not answered yet

    // The vtable (11 slots in every class; the signatures from the callers in OnSocketMessage). The ones taking a MsnLine by value
    // are implemented by the non-virtual ...Impl functions below: the stock vtable is what the objects have.
    virtual void OnConnected();                                  //  0 the connection is up (the dispatch and notification classes send VER)
    virtual void OnClosed();                                     //  1 the peer closed the connection
    virtual void OnCommand(MsnLine Line);                        //  2 a line with a transaction nobody waits for (ret 0x3C)
    virtual void OnAsync(MsnLine Line);                          //  3 a line of the unprompted verbs (ret 0x3C)
    virtual void OnError(MsnLine Line);                          //  4 an error number (ret 0x3C)
    virtual void OnReply(INT RequestType, MsnLine Line);         //  5 the answer to a request (ret 0x40; the REQUEST TYPE IS THE FIRST ARGUMENT: the stock pushes it last, see 0x1013EA70)
    virtual void OnMessage(NMsnMessage* Message);                //  6 a parsed MSG payload (ret 4)
    virtual INT IsRequestKept(INT RequestType);                  //  7 non-zero: the request stays in Pending (ret 4)
    virtual INT Connect();                                       //  8
    virtual INT OnConnect(WPARAM wParam, LPARAM lParam);         //  9 the connect finished (ret 8)
    virtual void OnSocketMessage(WPARAM wParam, LPARAM lParam);  // 10 data or close (ret 8)

    // Construction and destruction. Hook build: the stock objects get their vtable by the stock constructor, which we cannot compile (our vtable
    // would be stored), so the stock's constructor and destructor bodies are these plain functions: Construct writes the stock vtable address
    // by hand (0x101DD840). Standalone build: the constructor below is the real one (the compiler writes our vtable, whose slots are the
    // virtuals above and the classes' overrides); the objects are made by placement new on a block of the right size
    MSNSocket* Construct(Msn* InOwner, NMsnServer InServer, INT InTrID);   // 0x10164AB0 (ret 0x18): vtable 0x101DD840, the server and the transaction counter
    void DestroyBase();                                          // 0x10164B80: vtable 0x101DD840, the table of requests emptied and freed, the server's host, the socket
#if !NW_HOOK_MODE
    MSNSocket(Msn* InOwner, const NMsnServer& InServer, INT InTrID);
#endif

    // implementations of the slots with a line by value (hooked at the stock addresses; not virtual)
    void OnCommandIgnore(MsnLine Line);                          // 0x1012F070 (slot 2 of the base, the dispatch and the switchboard classes, ret 0x3C): nothing, the line is destroyed
    void OnReplyIgnore(INT RequestType, MsnLine Line);           // 0x10140150 (slot 5 of the base, ret 0x40): nothing, the line is destroyed
    void OnErrorImpl(MsnLine Line);                              // 0x1013C5C0 (slot 4, ret 0x3C): the window's error dialog for the error number
    void OnAsyncImpl(MsnLine Line);                              // 0x10140500 (slot 3 of the base and the dispatch classes, ret 0x3C): MSG, its payload read and parsed, goes to OnMessage

    void Send(const MsnCommand& Command);                        // the command's text sent (SendLine on a copy), the transaction counter up
    void SendPayload(const MsnCommand& Command);                 // the same without the CRLF (a payload follows the line): SendRaw on a copy
    // the senders (MsnSend.cpp): the command goes by value and the callee releases it (NMsnLocalCommand's destructor is Release: the argument
    // strings are not freed, as in the stock)
    __declspec(safebuffers) void SendLine(NMsnLocalCommand Command);       // 0x101607E0 (ret 0x1C): the wire text and CRLF in UTF-8; a short write reads the socket's error
    __declspec(safebuffers) void SendRaw(NMsnLocalCommand Command);        // 0x10160950 (ret 0x1C): the wire text in the ANSI code page, no CRLF
    __declspec(safebuffers) void SendMessageBody(NMsnLocalCommand Command, NMsnMessageArg Message);   // 0x10160A60 (ret 0x74): the command, then the message's payload
    void SendVER();                                              // 0x1013EC60 (the slot 0 of the dispatch and notification classes): "VER trid MSNP8 CVR0", request type 1
    void SendCVR(MsnLine Reply);                                 // 0x1013EE30 (ret 0x3C): "CVR trid locale os version cpu ncsoft 1.0.0 l2chronicle3-kr account", type 2
    void SendUSRInitDS(MsnLine Reply);                           // 0x1013ED30 (ret 0x3C): "USR trid TWN I account", type 3 (dispatch)
    void SendUSRInitNS(MsnLine Reply);                           // 0x1013EFB0 (ret 0x3C): the same, type 4 (notification)
    __declspec(noinline) void RegisterPending(INT RequestType);                       // 0x1013EA40 (ret 4): the request type kept under the next transaction id
    __declspec(noinline) void RemovePending(INT TrID);                                // 0x1013EA60 (ret 4)
};
NW_ASSERT_OFFSET(MSNSocket, Sock, 0x04);
NW_ASSERT_OFFSET(MSNSocket, Owner, 0x38);
NW_ASSERT_OFFSET(MSNSocket, Server, 0x3C);
NW_ASSERT_OFFSET(MSNSocket, NextTrID, 0x4C);
NW_ASSERT_OFFSET(MSNSocket, Pending, 0x50);
NW_ASSERT_SIZE(MSNSocket, 0x64);
