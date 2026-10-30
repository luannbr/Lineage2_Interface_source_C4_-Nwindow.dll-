#pragma once
// The messenger's data records as the conversation windows (ChatWnd, L2ChatWnd) keep them. Names ours; the layouts come
// from the stock constructors, copy functions, destructors and the hash maps' Rehash instantiations (0x10119B20 ..
// 0x1012CA90). The Msn / SwitchboardSession objects themselves are the Msn lanes' business: the windows only hold
// pointers to them.

#include "nwindow/NWindow.h"
#pragma pack(push, 4)

// 0x10119B20 (cdecl): UE2's appStrihash (the CRC of the upper-cased characters over GCRCTable), linked statically into
// NWindow; the string maps hash their keys with it
DWORD NStrihash(const TCHAR* Data);

// 0x10168CC0 (cdecl, NMsnGroup.cpp): the text URL-decoded into Out (a buffer of 260 characters), with the escapes and nothing else touched
// (InternetCanonicalizeUrlW, ICU_DECODE | ICU_NO_ENCODE); what the import answers comes back
BOOL __cdecl DecodeMsnUrl(const TCHAR* Url, TCHAR* Out);

// A TArray of pointers whose destructor the stock keeps out of line (0x1011C140: UnTemplate.h's checks, the engine's
// FArray::Remove of all elements, ~FArray): the waiting messages of ChatWnd
struct NChatPtrArray : public TArray<void*>
{
    __declspec(noinline) ~NChatPtrArray();
};

/*-----------------------------------------------------------------------------
    NChatUser - a person of the messenger (0x44; the "User" the stock names show): the status code and the account are
    what the windows read.
-----------------------------------------------------------------------------*/
struct NChatUser
{
    INT Unknown00;           // +0x00 (the copy constructor leaves it alone)
    INT Unknown04;           // +0x04
    FString Name08;          // +0x08
    FString Status;          // +0x14 the presence code: "NLN" online, "BSY", "IDL", "AWY", "LUN", "PHN", "BRB", "HDN", "FLN"
    FString Account;         // +0x20 the account (the e-mail) the windows compare
    FString Nick;            // +0x2C the display name as the server sends it (URL-encoded)
    FString NickDecoded;     // +0x38 the same, decoded by GetDecodedName

    __declspec(noinline) NChatUser();                                // 0x1011A140
    __declspec(noinline) NChatUser(const NChatUser& Src);            // 0x1012C460 (ret 4) every field but Unknown00
    __declspec(noinline) ~NChatUser();                               // 0x1012C530
    // 0x1012C610 (ret 4; the string comes back through a hidden pointer): Nick decoded (InternetCanonicalizeUrl,
    // ICU_DECODE | ICU_NO_ENCODE) into NickDecoded, and a copy of it
    __declspec(noinline) FString GetDecodedName();
};
NW_ASSERT_OFFSET(NChatUser, Status, 0x14);
NW_ASSERT_OFFSET(NChatUser, Account, 0x20);
NW_ASSERT_OFFSET(NChatUser, NickDecoded, 0x38);
NW_ASSERT_SIZE(NChatUser, 0x44);

/*-----------------------------------------------------------------------------
    The people of a conversation (ChatWnd +0x910): a TMap<INT, NChatPeer> in UE2's layout. A pair is 0x54 bytes: the chain
    link, the key and the value: 8 bytes of unknown data, a timer id (the person's typing notice is cleared by it) and the user.
-----------------------------------------------------------------------------*/
struct NChatPeer
{
    INT Unknown00;           // +0x00
    INT TimerID;             // +0x04 killed with the person
    NChatUser User;          // +0x08

    // 0x1011A240 (ret 4): a copy of the person's user (the stock iterates the pairs and takes copies)
    __declspec(noinline) NChatUser GetUser();
};
NW_ASSERT_OFFSET(NChatPeer, User, 0x08);
NW_ASSERT_SIZE(NChatPeer, 0x4C);

struct NChatPeerPair
{
    INT HashNext;            // +0x00
    INT Key;                 // +0x04 the id the typing timer (ChatWnd::OnTimer) is compared with
    NChatPeer Value;         // +0x08

    // 0x10119FB0 (the stock function is `add ecx, 0x10; jmp ~NChatUser`)
    __declspec(noinline) ~NChatPeerPair();
};
NW_ASSERT_OFFSET(NChatPeerPair, Value, 0x08);
NW_ASSERT_SIZE(NChatPeerPair, 0x54);

typedef TArray<NChatPeerPair> NChatPeerArray;      // destructor 0x1011D3D0, Remove 0x1011C3E0

struct NChatPeerMap
{
    NChatPeerArray Pairs;            // +0x00
    INT* Hash;                       // +0x0C
    INT HashCount;                   // +0x10

    NChatPeerMap() : Hash(NULL), HashCount(8) { Rehash(); }          // the stock constructors' inline code
    __declspec(noinline) ~NChatPeerMap();                            // 0x1011ED50: the table freed, the pairs destroyed (0x1011D3D0)
    void Rehash();                                                   // 0x1011B390 UE2's TMapBase::Rehash (INT keys)
    // 0x1011D310 (ret 4; the key by reference): every pair of that key goes; returns how many
    INT RemoveKey(const INT& InKey);
    // what a TMap iterator's destructor does after a removal (0x1011C1D0 is its out-of-line copy)
    void Relax()
    {
        while (Pairs.Num() * 2 + 8 < HashCount)
            HashCount = HashCount / 2;
        Rehash();
    }
};
NW_ASSERT_SIZE(NChatPeerMap, 0x14);

// A TMap iterator (UE2's TMapBase::TIterator) over the people: the map twice (the map, its pair array: the same address), the
// pair being looked at and whether something was removed; its destructor relaxes the map after a removal
struct NChatPeerIter
{
    NChatPeerMap* Map;               // +0x00
    NChatPeerMap* Pairs;             // +0x04
    INT Index;                       // +0x08
    INT Removed;                     // +0x0C

    NChatPeerIter(NChatPeerMap& InMap) : Map(&InMap), Pairs(&InMap), Index(0), Removed(0) {}
    __declspec(noinline) ~NChatPeerIter();                           // 0x1011C1D0
};
NW_ASSERT_SIZE(NChatPeerIter, 0x10);

/*-----------------------------------------------------------------------------
    ChatWnd +0x924: a TMap<FString, NChatUser*> (the people by account). A pair is 0x14 bytes.
-----------------------------------------------------------------------------*/
struct NChatNamePair
{
    INT HashNext;            // +0x00
    FString Key;             // +0x04 the account
    NChatUser* Value;        // +0x10

    __declspec(noinline) NChatNamePair(const NChatNamePair& Src);    // 0x10119F80 (ret 4)
    __declspec(noinline) ~NChatNamePair();                           // 0x1012C7F0 (a jump into ~FString at +4)
};
NW_ASSERT_SIZE(NChatNamePair, 0x14);

typedef TArray<NChatNamePair> NChatNameArray;      // destructor 0x10140EB0, Remove 0x1013FEF0, Empty 0x10140F00

// 0x101561D0 (cdecl): UE2's `new(Array) T` for the name pairs: a pair added at the end, its address (the size is not read)
void* __cdecl NChatNameNew(size_t Size, TArray<NChatNamePair>& Array);

// 0x10141120 (ret 4): the name pairs' array assignment: unless it is the same array, Empty(the other's count) and a copy of each pair added at the
// end (a member of a helper type so that a hook takes `this` in ecx; the array and the helper are the same object)
struct NChatNameArrayAssigner : public NChatNameArray
{
    NChatNameArray& Assign(const NChatNameArray& Other);
};

struct NChatNameMap
{
    NChatNameArray Pairs;            // +0x00
    INT* Hash;                       // +0x0C
    INT HashCount;                   // +0x10

    NChatNameMap() : Hash(NULL), HashCount(8) { Rehash(); }
    __declspec(noinline) ~NChatNameMap();                            // 0x1011D420
    void Rehash();                                                   // 0x1012CA90 (FString keys, hashed with NStrihash)
    void Relax()
    {
        while (Pairs.Num() * 2 + 8 < HashCount)
            HashCount = HashCount / 2;
        Rehash();
    }
};
NW_ASSERT_SIZE(NChatNameMap, 0x14);

// the name map's iterator (the destructor 0x101362F0 relaxes the map: halve the table while it is too big, rebuild it)
struct NChatNameIter
{
    NChatNameMap* Map;               // +0x00
    NChatNameMap* Pairs;             // +0x04
    INT Index;                       // +0x08
    INT Removed;                     // +0x0C

    NChatNameIter(NChatNameMap& InMap) : Map(&InMap), Pairs(&InMap), Index(0), Removed(0) {}
    __declspec(noinline) ~NChatNameIter();                           // 0x101362F0
};
NW_ASSERT_SIZE(NChatNameIter, 0x10);

#pragma pack(pop)
