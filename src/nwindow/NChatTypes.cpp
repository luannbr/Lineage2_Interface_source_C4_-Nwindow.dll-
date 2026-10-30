/*=============================================================================
    NChatTypes.cpp - the messenger records the conversation windows keep (C3 only; C3 addresses). Stock: appStrihash
    0x10119b20, the name pair's copy 0x10119f80, the peer pair's destructor 0x10119fb0, the user 0x1011a140 / 0x1012c460 /
    0x1012c530 / 0x1012c610, the maps' Rehash 0x1011b390 / 0x1012ca90, RemoveKey 0x1011d310, the arrays' Remove 0x1011c3e0,
    the maps' destructors 0x1011d420 / 0x1011ed50 and the pair destructor 0x1012c7f0.
=============================================================================*/
#include "nwindow/NChatTypes.h"

// 0x10119b20 (cdecl) - UE2's appStrihash: each character upper-cased (a..z only) and run twice (low byte, high byte)
// through the CRC table
DWORD NStrihash(const TCHAR* Data)
{
    DWORD Hash = 0;
    while (*Data)
    {
        TCHAR Ch = *Data++;
        if (Ch >= 'a' && Ch <= 'z')
            Ch = (TCHAR)(Ch - 0x20);
        WORD B = Ch;
        Hash = ((Hash >> 8) & 0x00FFFFFF) ^ GCRCTable[(Hash ^ B) & 0x000000FF];
        B = Ch >> 8;
        Hash = ((Hash >> 8) & 0x00FFFFFF) ^ GCRCTable[(Hash ^ B) & 0x000000FF];
    }
    return Hash;
}
// (not hooked: regcheck reads the stock's `mov cx, [esi]` followed by a read of ecx as a register argument. The stock
// function has nine callers (NMsnHeaderMap::Rehash / Lookup, NMsnStrStrMap::Add / Set, TMapFStringWnd::Rehash,
// NMsnWndStrMapOps::MultiFind / AddWnd, NMsnWndStrMap::FindWnd / SetWnd): all are ported, hooked and call this copy, so the
// stock one is dead in hook mode and listed in db/morto.csv)

// 0x10140cb0 - the base array's destructor, out of line (Remove of every element, then ~FArray)
NChatPtrArray::~NChatPtrArray()
{
}
NW_HOOK_DTOR(0x10140cb0, NChatPtrArray)

/*-----------------------------------------------------------------------------
    NChatUser.
-----------------------------------------------------------------------------*/

// 0x1013e9a0 - the five strings empty (the two numbers are left as they were)
NChatUser::NChatUser()
{
}
NW_HOOK_CTOR(0x1013e9a0, NChatUser)

// 0x1012c460 (ret 4) - the strings made empty first, then every field but the first copied (the second number, then the
// status, +8, the account, the nick and the decoded nick)
NChatUser::NChatUser(const NChatUser& Src)
{
    Unknown04 = Src.Unknown04;
    Status = Src.Status;
    Name08 = Src.Name08;
    Account = Src.Account;
    Nick = Src.Nick;
    NickDecoded = Src.NickDecoded;
}
// (not hooked here: the same record is NMsnFriend, whose copy constructor is hooked at that address in NMsnProtocol.cpp)

// 0x1012c530 - the five strings in reverse
NChatUser::~NChatUser()
{
}
// (not hooked here: the same record is NMsnFriend, whose destructor is hooked at that address in NMsnProtocol.cpp)

// 0x1012c610 (ret 4) - Nick URL-decoded into a buffer of 260 characters (InternetCanonicalizeUrlW through the stock's wrapper
// 0x10144300, cdecl (source, destination)), kept in NickDecoded and returned
FString NChatUser::GetDecodedName()
{
    TCHAR Buffer[260];
    appMemset(Buffer, 0, sizeof(Buffer));
    DecodeMsnUrl(*Nick, Buffer);
    NickDecoded = Buffer;
    return NickDecoded;
}
// (not hooked here: the same record is NMsnFriend, whose GetDisplayName is hooked at that address in NMsnProtocol.cpp)

/*-----------------------------------------------------------------------------
    The people of a conversation: pair, map and the arrays' out-of-line functions.
-----------------------------------------------------------------------------*/

// 0x1013eaa0 (ret 4; the user comes back through a hidden pointer) - a copy of the person's user
NChatUser NChatPeer::GetUser()
{
    return User;
}
NW_HOOK_METHOD(0x1013eaa0, NChatPeer, GetUser)

// 0x1013e830 (`add ecx, 0x10` then a jump into ~NChatUser)
NChatPeerPair::~NChatPeerPair()
{
}
NW_HOOK_DTOR(0x1013e830, NChatPeerPair)

// 0x10140f50 (ret 8) - the stock's out-of-line TArray<NChatPeerPair>::Remove(Index, Count) (UnTemplate.h's three checks, the
// pairs' destructors, the engine's FArray::Remove), and the array's destructor (0x1011d3d0: Remove of every pair, ~FArray)
NW_HOOK_METHOD(0x10140f50, NChatPeerArray, Remove)
NW_HOOK_DTOR(0x10141f40, NChatPeerArray)

// 0x10140d40 - after a removal the map is relaxed
NChatPeerIter::~NChatPeerIter()
{
    if (Removed)
        Map->Relax();
}
NW_HOOK_DTOR(0x10140d40, NChatPeerIter)

// 0x1013fdc0 - UE2's TMapBase::Rehash: a table of HashCount entries, the chains rebuilt by the pairs' keys, the old table freed
void NChatPeerMap::Rehash()
{
    INT* NewHash = new (TEXT("HashMapHash")) INT[HashCount];
    for (INT i = 0; i < HashCount; i++)
        NewHash[i] = -1;
    for (INT i = 0; i < Pairs.Num(); i++)
    {
        NChatPeerPair& Pair = Pairs(i);
        INT iHash = Pair.Key & (HashCount - 1);
        Pair.HashNext = NewHash[iHash];
        NewHash[iHash] = i;
    }
    if (Hash)
        appFree(Hash);
    Hash = NewHash;
}
NW_HOOK_METHOD(0x1013fdc0, NChatPeerMap, Rehash)

// the stock's out-of-line instances of TArray<NChatNamePair> / TArray<NChatPeerPair> that it calls as functions of their own (0x10140F50 the
// people's Remove(Index, 1), 0x10140F00 the names' Empty(Slack)); the compiler would inline the templates into every caller
__declspec(noinline) static void NChatPeerRemove(NChatPeerArray& Array, INT Index)
{
    Array.Remove(Index, 1);
}
__declspec(noinline) static void NChatNameEmpty(NChatNameArray& Array, INT Slack)
{
    Array.Empty(Slack);
}

// 0x10141e80 (ret 4) - UE2's TMapBase::Remove(Key): from the last pair down, those of that key go (through the array's
// out-of-line Remove); if any did, the table is halved while it is too big and rebuilt
INT NChatPeerMap::RemoveKey(const INT& InKey)
{
    INT Count = 0;
    for (INT i = Pairs.Num() - 1; i >= 0; i--)
    {
        if (Pairs(i).Key == InKey)
        {
            NChatPeerRemove(Pairs, i);
            Count++;
        }
    }
    if (Count)
        Relax();
    return Count;
}
NW_HOOK_METHOD(0x10141e80, NChatPeerMap, RemoveKey)

// 0x101438d0 - the table freed and forgotten, then the pairs (0x1011d3d0 removes them all)
NChatPeerMap::~NChatPeerMap()
{
    if (Hash)
        appFree(Hash);
    Hash = NULL;
    HashCount = 0;
}
NW_HOOK_DTOR(0x101438d0, NChatPeerMap)

/*-----------------------------------------------------------------------------
    The people by account: pair and map.
-----------------------------------------------------------------------------*/

// 0x1013e800 (ret 4) - the link and the pointer copied, the account copied as a string
NChatNamePair::NChatNamePair(const NChatNamePair& Src)
    : HashNext(Src.HashNext), Key(Src.Key), Value(Src.Value)
{
}
NW_HOOK_CTOR_ARGS(0x1013e800, NChatNamePair, (const NChatNamePair& A), (A))

// 0x1013fef0 (ret 8) - the out-of-line TArray<NChatNamePair>::Remove(Index, Count) (UnTemplate.h's checks, ~FString of each key, then
// FArray::Remove); 0x10140eb0 is the array's destructor (Remove of every pair, ~FArray)
NW_HOOK_METHOD(0x1013fef0, NChatNameArray, Remove)
NW_HOOK_DTOR(0x10140eb0, NChatNameArray)

// 0x10140f00 (ret 4) - Empty(Slack): the pairs' destructors (0x1006e880), the count 0, room for Slack
NW_HOOK_METHOD(0x10140f00, NChatNameArray, Empty)


// 0x1015adc0 - after a removal the map is relaxed
NChatNameIter::~NChatNameIter()
{
    if (Removed)
        Map->Relax();
}
NW_HOOK_DTOR(0x1015adc0, NChatNameIter)

// 0x101561d0 (cdecl) - UE2's `new(Array) T` for the name pairs: a pair added at the end, its address
__declspec(noinline) void* __cdecl NChatNameNew(size_t Size, TArray<NChatNamePair>& Array)
{
    return operator new(Size, Array);
}
NW_HOOK_FUNC(0x101561d0, NChatNameNew)

// 0x10141120 (ret 4) - the array's assignment: unless it is the same array, Empty(the other's count) (the instance above, called
// out of line) and a copy of each pair added at the end through the instance of `new(Array)` above. (A member of a helper type
// so that the hook takes `this` in ecx; the array and the helper are the same object.)
NChatNameArray& NChatNameArrayAssigner::Assign(const NChatNameArray& Other)
{
    if (this != &Other)
    {
        NChatNameEmpty(*this, Other.Num());
        for (INT i = 0; i < Other.Num(); i++)
            new (NChatNameNew(sizeof(NChatNamePair), *this)) NChatNamePair(Other(i));
    }
    return *this;
}
NW_HOOK_METHOD(0x10141120, NChatNameArrayAssigner, Assign)

// 0x1006e880 - the key goes (`add ecx, 4` then a jump into ~FString)
NChatNamePair::~NChatNamePair()
{
}
NW_HOOK_DTOR(0x1006e880, NChatNamePair)

// 0x1012ca90 - UE2's TMapBase::Rehash for FString keys (appStrihash of the characters)
void NChatNameMap::Rehash()
{
    INT* NewHash = new (TEXT("HashMapHash")) INT[HashCount];
    for (INT i = 0; i < HashCount; i++)
        NewHash[i] = -1;
    for (INT i = 0; i < Pairs.Num(); i++)
    {
        NChatNamePair& Pair = Pairs(i);
        INT iHash = NStrihash(*Pair.Key) & (HashCount - 1);
        Pair.HashNext = NewHash[iHash];
        NewHash[iHash] = i;
    }
    if (Hash)
        appFree(Hash);
    Hash = NewHash;
}
// (not hooked here: the contact lists' NMsnStrMap::Rehash is hooked at that address in FriendList.cpp, the same function)

// 0x10141f90 - the table freed and forgotten, then the pairs (0x1011c340 removes them all)
NChatNameMap::~NChatNameMap()
{
    if (Hash)
        appFree(Hash);
    Hash = NULL;
    HashCount = 0;
}
NW_HOOK_DTOR(0x10141f90, NChatNameMap)
