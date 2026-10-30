#pragma once
// A message (NMsnMessage, 0x58 bytes, NChatMsg.h) handed to or received from the stock-convention functions BY VALUE, without the C++
// destructor of NMsnMessage being run by the compiler (new file of the msn1 lane).
//
// Why: a function that takes a message by value in the stock frees it itself, with the out-of-line destructor (0x1014EFE0) at a point of
// its own; the callers (and the hooks of the other lane that call these functions by address) make the argument with the out-of-line copy
// constructor (0x1014F090) in the argument slot. The helpers below are those two calls (our NMsnMessage::~NMsnMessage and the copy
// constructor, both out of line in NChatMsg.cpp), so a callee declared with NMsnMessageArg frees its argument exactly where the stock does
// and the compiler runs no destructor of its own on it.

#include "nwindow/NChatMsg.h"

// The bytes of a message in an argument slot: what a by-value parameter looks like to the callee (it frees it with MsnMessageFree)
struct NMsnMessageArg
{
    BYTE Raw[0x58];
    NMsnMessage& Msg() { return *(NMsnMessage*)Raw; }
};
NW_ASSERT_SIZE(NMsnMessageArg, 0x58);

// The destructor of a message (0x1014EFE0): the headers emptied, the strings destroyed
FORCEINLINE void MsnMessageFree(void* Message)
{
    ((NMsnMessage*)Message)->~NMsnMessage();
}

// A copy of a message made by the copy constructor (0x1014F090), as the stock callers make the argument of a callee that frees it
FORCEINLINE NMsnMessageArg MsnMessageArgOf(const NMsnMessage& Source)
{
    NMsnMessageArg Copy;
    new ((void*)Copy.Raw) NMsnMessage(Source);
    return Copy;
}

// A message a function owns: made by the constructor (NChatMsg.cpp, stock 0x101437D0) and freed on every way out by the destructor
struct NMsnMessageLocal
{
    BYTE Raw[sizeof(NMsnMessage)];

    NMsnMessageLocal() { new ((void*)Raw) NMsnMessage; }
    ~NMsnMessageLocal() { MsnMessageFree(Raw); }
    NMsnMessage* operator->() { return (NMsnMessage*)Raw; }
    NMsnMessage& Ref() { return *(NMsnMessage*)Raw; }
};

// The people's map iterator of a conversation (NChatPeerIter, NChatTypes.h) whose destructor is the out-of-line function 0x10140D40 (the
// map relaxed after a removal): the same four fields, the destructor of NChatPeerIter called as a function of its own
struct NChatPeerIterLocal
{
    NChatPeerMap* Map;               // +0x00
    NChatPeerMap* Pairs;             // +0x04
    INT Index;                       // +0x08
    INT Removed;                     // +0x0C

    NChatPeerIterLocal(NChatPeerMap& InMap) : Map(&InMap), Pairs(&InMap), Index(0), Removed(0) {}
    __declspec(noinline) ~NChatPeerIterLocal() { ((NChatPeerIter*)this)->~NChatPeerIter(); }
};
NW_ASSERT_SIZE(NChatPeerIterLocal, 0x10);
