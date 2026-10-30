/*=============================================================================
    MsnData.cpp - the manager's data (C3 only; C3 addresses): the constructor and the destructor of the manager (Msn: 0x1012C180,
    0x1012BF80), of its contact lists (0x1012D760 .. 0x1012D970), of one list (0x1012D6F0, 0x1012D400), of the group list
    (0x1012D7C0, 0x1012D180, 0x1012D510) and of the tables' pair arrays (0x1012CDD0, 0x1012D4A0, 0x1012B7B0). The classes are
    the protocol lane's (Msn.h, FriendList.h); these are the functions of theirs the messenger window's lane owns. The
    constructors are written as methods of a type derived from the class (NMsnManager, NMsnLists ...), which adds nothing, so
    that they can be hooked with the object as `this` and give it back.
=============================================================================*/
#include "nwindow/Msn.h"
#include "nwindow/FriendList.h"
#include "nwindow/NCWnd.h"
#include "nwindow/SwitchboardSession.h"
#include "nwindow/MsnProtoOps.h"

/*-----------------------------------------------------------------------------
    The tables' pair arrays (UE2's TArray<pair> destructor with UnTemplate.h's checks, three instances).
-----------------------------------------------------------------------------*/

typedef TArray<NMsnIntPair> NMsnIntPairArray;
typedef TArray<NDataMap<SwitchboardSession>::NPair> NMsnSessionPairArray;

// 0x10151930 - the group table's pairs
NW_HOOK_DTOR(0x10151930, NMsnIntPairArray)
// 0x10150270 - the session table's pairs
NW_HOOK_DTOR(0x10150270, NMsnSessionPairArray)

// A pointer to an object made with GMalloc->Malloc(.., "new") and freed with GMalloc->Free (repeated inline in the stock code)
static FORCEINLINE void* NewBlock(DWORD Size)
{
    return appMalloc(Size, TEXT("new"));
}

// The destructor of an array of pointers (0x101480C0) and of the session table's pair array (0x10150270) are functions of their own in the
// stock (the AddedConfirmDialog.cpp / above hooks are those instances); the compiler would inline the template into every caller
typedef TArray<void*> NMsnPtrArray;
__declspec(noinline) static void MsnFreePtrArray(void* Array)
{
    ((NMsnPtrArray*)Array)->~NMsnPtrArray();
}
__declspec(noinline) static void MsnFreeSessionPairs(NMsnSessionPairArray& Pairs)
{
    Pairs.~NMsnSessionPairArray();
}

/*-----------------------------------------------------------------------------
    One contact list.
-----------------------------------------------------------------------------*/

// 0x101522d0 (ret 4) - no contact, no entry in the table by account (its hash is made), and the list's bit
NMsnListInit* NMsnListInit::Construct(INT ListBit)
{
    INT* Array = (INT*)&Friends;
    Array[0] = 0;
    Array[1] = 0;
    Array[2] = 0;
    INT* Map = (INT*)&ByAccount;
    Map[0] = 0;
    Map[1] = 0;
    Map[2] = 0;
    ByAccount.Hash = NULL;
    ByAccount.HashCount = 8;
    ByAccount.Rehash();
    Unknown00 = ListBit;
    return this;
}
NW_HOOK_VARARGS(0x101522d0, NMsnListInit, Construct, NMsnListInit* (NMsnListInit::*)(INT))

// 0x10151fe0 - every contact is destroyed and freed, the array emptied; the keys of the table by account are destroyed, its pairs
// emptied and the hash rebuilt (empty)
void NMsnListInit::DeleteAll()
{
    for (INT i = 0; i < Friends.Num(); i++)
    {
        NMsnFriend* Contact = Friends(i);
        if (Contact)
        {
            Contact->~NMsnFriend();
            appFree(Contact);
        }
    }
    ((FArray*)&Friends)->Empty(4);
    for (INT i = 0; i < ByAccount.Pairs.Num(); i++)
        ((NChatNamePair*)&ByAccount.Pairs(i))->~NChatNamePair();   // the key goes (0x1006E880)
    ((FArray*)&ByAccount.Pairs)->Empty(sizeof(NMsnStrPair));
    ByAccount.HashCount = 8;
    ByAccount.Rehash();
}
NW_HOOK_METHOD(0x10151fe0, NMsnListInit, DeleteAll)

// 0x10152340 - the contacts go, then the table by account, then the array
void NMsnListInit::Destroy()
{
    DeleteAll();
    ((NChatNameMap*)&ByAccount)->~NChatNameMap();
    MsnFreePtrArray(&Friends);
    volatile INT KeepReturn = 0;     // (a store after the call keeps ours a call and a return, as the stock one is)
    (void)KeepReturn;
}
NW_HOOK_METHOD(0x10152340, NMsnListInit, Destroy)

/*-----------------------------------------------------------------------------
    The group list.
-----------------------------------------------------------------------------*/

// 0x101523a0 - no group, an empty table by id
NMsnGroupsInit* NMsnGroupsInit::Construct()
{
    INT* Array = (INT*)&Groups;
    Array[0] = 0;
    Array[1] = 0;
    Array[2] = 0;
    INT* Map = (INT*)&ById;
    Map[0] = 0;
    Map[1] = 0;
    Map[2] = 0;
    ById.Hash = NULL;
    ById.HashCount = 8;
    ById.Rehash();
    return this;
}
NW_HOOK_VARARGS(0x101523a0, NMsnGroupsInit, Construct, NMsnGroupsInit* (NMsnGroupsInit::*)())

// 0x10151ce0 - every group is destroyed and freed, the array emptied, the table by id emptied and its hash rebuilt (empty)
void NMsnGroupsInit::DeleteAll()
{
    for (INT i = 0; i < Groups.Num(); i++)
    {
        NMsnGroup* Group = Groups(i);
        if (Group)
        {
            Group->~NMsnGroup();
            appFree(Group);
        }
    }
    ((FArray*)&Groups)->Empty(4);
    ((FArray*)&ById.Pairs)->Empty(sizeof(NMsnIntPair));
    ById.HashCount = 8;
    ById.Rehash();
    volatile INT KeepReturn = 0;     // (the stock jumps into the rehash and returns from there: a store after the call keeps ours a call and a return)
    (void)KeepReturn;
}
NW_HOOK_METHOD(0x10151ce0, NMsnGroupsInit, DeleteAll)

// 0x101520f0 - the groups go, then the table by id (0x1012D4A0), then the array
void NMsnGroupsInit::Destroy()
{
    DeleteAll();
    ((NMsnIntTableInit*)&ById)->Free();
    MsnFreePtrArray(&Groups);
    volatile INT KeepReturn = 0;     // (a store after the call keeps ours a call and a return, as the stock one is)
    (void)KeepReturn;
}
NW_HOOK_METHOD(0x101520f0, NMsnGroupsInit, Destroy)

// 0x10152080 - the table by id: its hash freed and forgotten, then the pairs (0x10151930)
void NMsnIntTableInit::Free()
{
    if (Hash)
        appFree(Hash);
    Hash = NULL;
    HashCount = 0;
    typedef TArray<NMsnIntPair> PairArray;
    Pairs.~PairArray();
}
NW_HOOK_METHOD(0x10152080, NMsnIntTableInit, Free)

/*-----------------------------------------------------------------------------
    The four lists and the groups.
-----------------------------------------------------------------------------*/

// 0x10152410 - the lists of the four bits (forward 1, allow 2, block 4, reverse 8) and the group list
NMsnContactsInit* NMsnContactsInit::Construct()
{
    void* Mem = NewBlock(0x24);
    FL = Mem ? ((NMsnListInit*)Mem)->Construct(MSNLIST_FL) : NULL;
    Mem = NewBlock(0x24);
    AL = Mem ? ((NMsnListInit*)Mem)->Construct(MSNLIST_AL) : NULL;
    Mem = NewBlock(0x24);
    BL = Mem ? ((NMsnListInit*)Mem)->Construct(MSNLIST_BL) : NULL;
    Mem = NewBlock(0x24);
    RL = Mem ? ((NMsnListInit*)Mem)->Construct(MSNLIST_RL) : NULL;
    Mem = NewBlock(0x20);
    Groups = Mem ? ((NMsnGroupsInit*)Mem)->Construct() : NULL;
    return this;
}
NW_HOOK_VARARGS(0x10152410, NMsnContactsInit, Construct, NMsnContactsInit* (NMsnContactsInit::*)())

// 0x10152550 - each list destroyed (0x1012D760), freed and forgotten, then the group list (0x1012D510)
void NMsnContactsInit::Destroy()
{
    if (FL)
    {
        ((NMsnListInit*)FL)->Destroy();
        appFree(FL);
        FL = NULL;
    }
    if (AL)
    {
        ((NMsnListInit*)AL)->Destroy();
        appFree(AL);
        AL = NULL;
    }
    if (BL)
    {
        ((NMsnListInit*)BL)->Destroy();
        appFree(BL);
        BL = NULL;
    }
    if (RL)
    {
        ((NMsnListInit*)RL)->Destroy();
        appFree(RL);
        RL = NULL;
    }
    if (Groups)
    {
        ((NMsnGroupsInit*)Groups)->Destroy();
        appFree(Groups);
        Groups = NULL;
    }
}
NW_HOOK_METHOD(0x10152550, NMsnContactsInit, Destroy)

/*-----------------------------------------------------------------------------
    The manager.
-----------------------------------------------------------------------------*/

// 0x10150c40 (ret 4) - the manager of that window: its vtable (the stock's 0x101DBEE4, hook build; the standalone build's constructor below
// runs this after the compiler wrote ours), an empty table of sessions, three empty strings, no connection, not signed in, the contact
// lists (0x10152410) and the user's record (0x1013E9A0) made, the third string "NLN"
Msn* Msn::Construct(MsnWnd* Window)
{
#if NW_HOOK_MODE
    *(void**)this = ::nw::Addr(0x101DBEE4);
#endif
    INT* Map = (INT*)&Sessions;
    Map[0] = 0;
    Map[1] = 0;
    Map[2] = 0;
    Sessions.Hash = NULL;
    Sessions.HashCount = 8;
    ((NMsnIntMap*)&Sessions)->Rehash();
    new ((void*)&Account) FString;
    new ((void*)&Password) FString;
    new ((void*)&Unknown3C) FString;
    Dispatch = NULL;
    Notification = NULL;
    ((FArray*)&Sessions.Pairs)->Empty(sizeof(NDataMap<SwitchboardSession>::NPair));
    Sessions.HashCount = 8;
    ((NMsnIntMap*)&Sessions)->Rehash();
    bSignedIn = 0;
    void* Mem = NewBlock(0x14);
    Contacts = Mem ? ((NMsnContactsInit*)Mem)->Construct() : NULL;
    Mem = NewBlock(0x44);
    Me = Mem ? (NMsnFriend*)new (Mem) NChatUser : NULL;
    Unknown3C = TEXT("NLN");
    Wnd = Window;
    Unknown48 = 1;
    return this;
}
NW_HOOK_VARARGS(0x10150c40, Msn, Construct, Msn* (Msn::*)(MsnWnd*))

#if !NW_HOOK_MODE
// the standalone's constructor: the compiler writes the vtable (two slots, OnSocketConnect and OnSocketData), then the body above
Msn::Msn(MsnWnd* Window)
{
    Construct(Window);
}
#endif

// 0x10150a40 - the contact lists and the user's record destroyed and freed, every session destroyed (0x10153C80) and freed, the
// table of sessions emptied, the window forgotten, the strings, then the table's hash and pairs
void Msn::Destroy()
{
#if NW_HOOK_MODE
    *(void**)this = ::nw::Addr(0x101DBEE4);
#endif
    if (Contacts)
    {
        ((NMsnContactsInit*)Contacts)->Destroy();
        appFree(Contacts);
    }
    if (Me)
    {
        Me->~NMsnFriend();
        appFree(Me);
    }
    for (INT i = 0; i < Sessions.Pairs.Num(); i++)
    {
        SwitchboardSession* Session = Sessions.Pairs(i).Value;
        if (Session)
        {
            Session->Destroy();
            appFree(Session);
        }
    }
    ((FArray*)&Sessions.Pairs)->Empty(sizeof(NDataMap<SwitchboardSession>::NPair));
    Sessions.HashCount = 8;
    ((NMsnIntMap*)&Sessions)->Rehash();
    if (Wnd)
        Wnd = NULL;
    Unknown3C.~FString();
    Password.~FString();
    Account.~FString();
    if (Sessions.Hash)
        appFree(Sessions.Hash);
    Sessions.Hash = NULL;
    Sessions.HashCount = 0;
    MsnFreeSessionPairs(Sessions.Pairs);
}
NW_HOOK_METHOD(0x10150a40, Msn, Destroy)
