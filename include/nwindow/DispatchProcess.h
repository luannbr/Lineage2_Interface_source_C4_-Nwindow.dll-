#pragma once
// DispatchProcess: the first connection of the messenger (vtable 0x101DD86C, 0x64 bytes, no members of its own): to
// the dispatch server (messenger.hotmail.com:1863) it says VER and CVR, then USR ... TWN I, and the server answers with an XFR
// to the notification server it should use (ProcessXFR). Then the manager drops it (Msn::NotifyDispatchProcessEnd).

#include "nwindow/MSNSocket.h"

class DispatchProcess : public MSNSocket
{
public:
    // Hook build: the stock constructor's body (0x10164C20, ret 0x18: the base's Construct, then the stock vtable 0x101DD86C written by hand).
    // The destructor's body: the vtable, then the base's destruction
    DispatchProcess* ConstructDispatch(Msn* InOwner, NMsnServer InServer, INT InTrID);
    void Destroy();                              // 0x10164CA0

    // the vtable's overrides (the stock's slots: 0 = SendVER, 1 = OnClosedImpl, 5 = OnReplyImpl; the others are the base's)
    virtual void OnConnected();                  //  0 0x10163680 (MSNSocket::SendVER)
    virtual void OnClosed();                     //  1 0x10160C80 (OnClosedImpl)
    virtual void OnReply(INT RequestType, MsnLine Line);   //  5 0x10164CB0 (OnReplyImpl)
#if !NW_HOOK_MODE
    // the real constructor of the standalone build (the compiler writes the vtable); the hook build makes the object with the base's Construct
    // and the stock vtable (MsnLogin.cpp)
    DispatchProcess(Msn* InOwner, const NMsnServer& InServer, INT InTrID);
#endif

    // the slot 1 of the vtable (the peer closed): the manager deletes this connection
    void OnClosedImpl();                         // 0x10160C80 (jump to Msn::NotifyDispatchProcessEnd of the owner)

    // the answer to the USR: "XFR trid NS host:port ...": a notification connection to that server
    void ProcessXFR(MsnLine Line);               // 0x1013C620 (ret 0x3C, guard name)

    // the slot 5 of the vtable (an answer by request type; the request type is the FIRST argument, the line the second): 1 (VER) goes
    // to SendCVR, 2 (CVR) to SendUSRInitDS, 3 (USR TWN I) to ProcessXFR
    void OnReplyImpl(INT RequestType, MsnLine Line);   // 0x10140290 (ret 0x40)
};
NW_ASSERT_SIZE(DispatchProcess, 0x64);
