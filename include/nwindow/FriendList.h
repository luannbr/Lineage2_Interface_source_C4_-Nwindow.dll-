#pragma once
// FriendList: one of the messenger's four contact lists (forward, allow, block, reverse), a sorted array of contacts and a map
// by account (stock 0x1012C920 .. 0x1012D570, source file "MSN\MsnFriend.cpp"). Created by the manager (Msn); the
// constructor and destructor (0x1012D1xx / 0x1012D400) are msn2's.

#include "nwindow/NMsnProtocol.h"

// a pair of the map by account: UE2's TMap<FString, T*> keeps its pairs in an array and chains them by index (name ours)
struct NMsnStrPair
{
    INT HashNext;            // +0x00 the next pair of the chain, -1 at the end
    FString Key;             // +0x04
    void* Value;             // +0x10
};
NW_ASSERT_SIZE(NMsnStrPair, 0x14);

// UE2's TMap<FString, void*> as the stock lays it out: the pairs, then the hash (a power of 2 of chain heads)
struct NMsnStrMap
{
    TArray<NMsnStrPair> Pairs;   // +0x00
    INT* Hash;                   // +0x0C
    INT HashCount;               // +0x10

    void* Find(const FString& Key);                              // 0x101347D0 (ret 4, other lane): the value, NULL when there is none
    void* Set(const TCHAR* Key, void** Value);                   // 0x101361B0 (ret 8, other lane): the value stored under the key
    INT Remove(const TCHAR* Key);                                // 0x101374E0 (ret 4): every pair of that key removed, the number removed
    __declspec(noinline) void Rehash();                          // 0x1012CA90: the chains rebuilt in a new table of HashCount heads
};
NW_ASSERT_SIZE(NMsnStrMap, 0x14);

class FriendList
{
public:
    INT Unknown00;                   // +0x00
    TArray<NMsnFriend*> Friends;     // +0x04 sorted by CompareFriend (online first, then by name)
    NMsnStrMap ByAccount;            // +0x10 TMap<FString, NMsnFriend*> by Account

    static INT __cdecl CompareFriend(const void* A, const void* B);   // 0x1012C920 (the comparator of appQsort)
    void Sort();                                                  // 0x1012CBA0: appQsort of Friends
    INT AddFriend(NMsnFriend Friend);                             // 0x1012CE60 (ret 0x44, guard name): the index in Friends
    NMsnFriend* FindFriend(FString Account);                      // 0x1012CB40 (ret 0xC, other lane's tree): the contact of that account or NULL
    void RemoveFriend(FString Account);                           // 0x1012D350 (ret 0xC): the contact of that account removed and freed, then the map's pairs of it
};
NW_ASSERT_OFFSET(FriendList, Friends, 0x04);
NW_ASSERT_OFFSET(FriendList, ByAccount, 0x10);

// the groups of the contacts (names ours): the array in the order they came, and a map by id
class NMsnGroupList
{
public:
    TArray<NMsnGroup*> Groups;       // +0x00
    NMsnIntMap ById;                 // +0x0C TMap<INT, NMsnGroup*> by the group id (the layout of a pair map of integers)

    void AddGroup(NMsnGroup Group);                              // 0x1012D030 (ret 0x1C): a known id gets the new name, else a copy is entered in both
    void RemoveGroup(INT GroupID);                               // 0x1012D130 (ret 4): out of the map and of the array (the record itself is not freed)
    NMsnGroup* FindGroup(INT GroupID);                           // 0x1012CBC0 (ret 4)
};
NW_ASSERT_OFFSET(NMsnGroupList, ById, 0x0C);

// the four lists and the groups of a manager (Msn +0x50; name ours)
struct NMsnContacts
{
    FriendList* FL;                  // +0x00 forward list: the contacts of the user
    FriendList* AL;                  // +0x04 allow list
    FriendList* BL;                  // +0x08 block list
    FriendList* RL;                  // +0x0C reverse list
    NMsnGroupList* Groups;           // +0x10

    void DestroyLists();                                          // 0x1012D570 (name ours): the destructors of the four lists (0x1012D400) and of the groups (0x1012D180)
    FriendList* GetList(FString ListName);                        // 0x1012C710 (ret 0xC): "FL" / "AL" / "BL" / "RL", NULL for any other
};
NW_ASSERT_OFFSET(NMsnContacts, Groups, 0x10);
