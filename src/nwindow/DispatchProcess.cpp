/*=============================================================================
    DispatchProcess.cpp - the messenger's first connection (C4). Stock: 0x10160C80 (the close), 0x10161040 (ProcessXFR), 0x10164C20 (the
    constructor), 0x10164CA0 (the destructor), 0x10164CB0 (the answers). The constructor and the destructor set the vtable 0x101DD86C: the hook
    build writes it by hand, the standalone build gets it from the compiler (the constructor below, the three overrides at the end).
=============================================================================*/
#include "nwindow/DispatchProcess.h"

// 0x10160c80 (vslot 1) - the dispatch connection closed: the manager deletes it
void DispatchProcess::OnClosedImpl()
{
    Owner->NotifyDispatchProcessEnd();
}
NW_HOOK_METHOD(0x10160c80, DispatchProcess, OnClosedImpl)

// 0x10161040 (ret 0x3C, guard name) - "XFR trid NS host:port ...": the notification server of argument 1 is the manager's new
// connection
void DispatchProcess::ProcessXFR(MsnLine Line)
{
    guard(DispatchProcess::ProcessXFR);
    NMsnServer Server = Line.GetArgServer(1);
    Owner->StartNotification(Server);
    unguard;
}
typedef void (DispatchProcess::*SigDispatchProcessXFR)(MsnLine);
NW_HOOK_VARARGS(0x10161040, DispatchProcess, ProcessXFR, SigDispatchProcessXFR)

// 0x10164cb0 (ret 0x40, vslot 5) - the answer to a request of the dispatch connection, by the request type (the first argument): the VER
// (1) is answered by the CVR, the CVR (2) by "USR TWN I", and the USR (3) carries the XFR; each handler gets its own copy of the line
void DispatchProcess::OnReplyImpl(INT RequestType, MsnLine Line)
{
    if (RequestType == 1)
        SendCVR(Line);
    else if (RequestType == 2)
        SendUSRInitDS(Line);
    else if (RequestType == 3)
        ProcessXFR(Line);
}
typedef void (DispatchProcess::*SigDispatchOnReply)(INT, MsnLine);
NW_HOOK_VARARGS(0x10164cb0, DispatchProcess, OnReplyImpl, SigDispatchOnReply)

// 0x10164ca0 - the vtable of the dispatch class stored (hook build), then the base's destruction
void DispatchProcess::Destroy()
{
#if NW_HOOK_MODE
    *(void**)this = ::nw::Addr(0x101DD86C);
#endif
    DestroyBase();
}
NW_HOOK_METHOD(0x10164ca0, DispatchProcess, Destroy)

#if NW_HOOK_MODE
// 0x10164c20 (ret 0x18) - the base's construction (0x10164AB0: the owner, a copy of the server, the first transaction id), the vtable of the
// dispatch class, and the server given by value destroyed
__declspec(safebuffers) DispatchProcess* DispatchProcess::ConstructDispatch(Msn* InOwner, NMsnServer InServer, INT InTrID)
{
    MSNSocket::Construct(InOwner, InServer, InTrID);
    *(void**)this = ::nw::Addr(0x101DD86C);
    return this;
}
NW_HOOK_VARARGS(0x10164c20, DispatchProcess, ConstructDispatch, DispatchProcess* (DispatchProcess::*)(Msn*, NMsnServer, INT))
#else
// the standalone's constructor: the base's (the compiler writes the vtable of this class after it)
DispatchProcess::DispatchProcess(Msn* InOwner, const NMsnServer& InServer, INT InTrID) : MSNSocket(InOwner, InServer, InTrID)
{
}

// the vtable's overrides (0x101DD86C): slot 0 (0x10163680) is SendVER, slot 1 (0x10160C80) the close, slot 5 (0x10164CB0) the answers
void DispatchProcess::OnConnected()
{
    SendVER();
}
void DispatchProcess::OnClosed()
{
    OnClosedImpl();
}
void DispatchProcess::OnReply(INT RequestType, MsnLine Line)
{
    OnReplyImpl(RequestType, Line);
}
#endif
