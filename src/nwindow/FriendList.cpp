/*=============================================================================
    FriendList.cpp - the messenger's contact lists (C3 only, source file "MSN\MsnFriend.cpp" in the assert). Stock:
    0x1012C710 (list by name), 0x1012C920 (CompareFriend), 0x1012CA90 (the map's Rehash), 0x1012CB40 (FindFriend),
    0x1012CBA0 (Sort), 0x1012CE60 (AddFriend), 0x1012D350 (RemoveFriend), 0x101374E0 (the map's Remove). The list's
    constructor and destructor and the group list (0x1012D030, 0x1012D130, 0x1012D300, 0x1012D570) are other functions.
=============================================================================*/
#include "nwindow/FriendList.h"
#include "nwindow/MsnUiStock.h"
#include "nwindow/MsnProtoOps.h"   // NMsnStrMapOps, NMsnListInit, NMsnGroupsInit (stage 5, sa-verify: declared there once)

/*-----------------------------------------------------------------------------
    NMsnStrMap: UE2's TMap<FString, T*> (UnTemplate.h).
-----------------------------------------------------------------------------*/

// 0x101347d0 (ret 4, not ours) - the value of the pair of that key, NULL when there is none
void* NMsnStrMap::Find(const FString& Key)
{
    return ((NMsnWndStrMap*)this)->FindWnd(&Key);                // 0x101514F0: the same table, the windows' name
}

// 0x101361b0 (ret 8, not ours) - the pair of that key gets that value (a pair is made when there is none)
void* NMsnStrMap::Set(const TCHAR* Key, void** Value)
{
    return ((NMsnWndStrMap*)this)->SetWnd(Key, (NCWnd**)Value);  // 0x1015AD40
}

// the case-insensitive CRC of a text (stock 0x1013e3c0): msn2's NChatTypes.cpp has the C++ port, which is not hooked (regcheck reads the stock's
// `mov cx, [esi]` followed by a read of ecx as a register argument); the ported callers use that copy
DWORD NStrihash(const TCHAR* Data);

// 0x1013fc50 - UE2's TMapBase::Rehash for FString keys (appStrihash of the characters): a new table of HashCount heads, every pair chained
// into the head of its hash, the old table freed
void NMsnStrMap::Rehash()
{
    INT* NewHash = new (TEXT("HashMapHash")) INT[HashCount];
    for (INT i = 0; i < HashCount; i++)
        NewHash[i] = -1;
    for (INT i = 0; i < Pairs.Num(); i++)
    {
        NMsnStrPair& Pair = Pairs(i);
        INT iHash = NStrihash(*Pair.Key) & (HashCount - 1);
        Pair.HashNext = NewHash[iHash];
        NewHash[iHash] = i;
    }
    if (Hash)
        appFree(Hash);
    Hash = NewHash;
}
NW_HOOK_METHOD(0x1013fc50, NMsnStrMap, Rehash)

// The messenger window's name table (MsnWnd.h) has this layout and this rehash: the stock has the one function, 0x1013fc50
void NMsnWndStrMap::Rehash()
{
    ((NMsnStrMap*)this)->Rehash();
}

// 0x10151ee0 - the destructor of the array of pairs of a string map (the instance for 0x14-byte pairs): the pairs removed from the first
// to the last (their keys destroyed), then the array
typedef TArray<NMsnStrPair> TMsnStrPairArray;
NW_HOOK_DTOR(0x10151ee0, TMsnStrPairArray)

// 0x10151e60  NOT PORTED (not in eligible.csv (category D or not a game function)): the stock body is NMsnStrMapOps::RemoveKey (NConsoleWndAnon.cpp's
// hook of this address), which the standalone calls
#if NW_HOOK_MODE
NW_NAKED INT NMsnStrMap::Remove(const TCHAR* Key) NW_STOCK(0x10151e60)
#else
INT NMsnStrMap::Remove(const TCHAR* Key)
{
    return ((NMsnStrMapOps*)this)->RemoveKey(Key);
}
#endif

/*-----------------------------------------------------------------------------
    FriendList.
-----------------------------------------------------------------------------*/

// 0x101513b0 (guard name; cdecl, the comparator of appQsort) - two contacts ordered by Util::MsnCompareFriend (0x10144920)
// of their display names and their online flags (the status is not "FLN"); the second contact is prepared first
INT __cdecl FriendList::CompareFriend(const void* A, const void* B)
{
    guard(FriendList::CompareFriend);
    NMsnFriend* FriendB = *(NMsnFriend* const*)B;
    NMsnFriend* FriendA = *(NMsnFriend* const*)A;
    // the arguments are pushed right to left: the second contact's status and name first, as the stock does
    return NUtilMsnCompareFriend(FriendA->GetDisplayName(), FriendA->Status != TEXT("FLN"),      // Util::MsnCompareFriend 0x101692e0
                                 FriendB->GetDisplayName(), FriendB->Status != TEXT("FLN"));
    unguard;
}
NW_HOOK_FUNC(0x101513b0, FriendList::CompareFriend)

// 0x10151640 - the contacts sorted
void FriendList::Sort()
{
    appQsort(Friends.GetData(), Friends.Num(), 4, CompareFriend);
}
NW_HOOK_METHOD(0x10151640, FriendList, Sort)

// 0x101515e0 (ret 0xC) - the contact of that account, NULL when there is none (the string comes by value)
NMsnFriend* FriendList::FindFriend(FString Account)
{
    return (NMsnFriend*)ByAccount.Find(Account);
}
typedef NMsnFriend* (FriendList::*SigFriendListFindFriend)(FString);
NW_HOOK_VARARGS(0x101515e0, FriendList, FindFriend, SigFriendListFindFriend)

// 0x101519c0 (ret 0x44, guard name) - a known contact (by account) takes the new name and status, an unknown one is copied to
// the heap, entered in the map and inserted in order (before the first contact that does not order below it, else at
// the end); the index of the contact comes back
INT FriendList::AddFriend(NMsnFriend Friend)
{
    INT Index = -1;
    NMsnFriend* Known = (NMsnFriend*)ByAccount.Find(Friend.Account);
    if (Known)
    {
        Known->SetName(Friend.Name);
        Known->SetStatus(Friend.Status);
        for (INT i = 0; i < Friends.Num(); i++)
        {
            if (Friends(i) == Known)
            {
                Index = i;
                break;
            }
        }
    }
    else
    {
        NMsnFriend* Added = new NMsnFriend(Friend);
        ByAccount.Set(*Friend.Account, (void**)&Added);
        for (INT i = 0; i < Friends.Num(); i++)
        {
            NMsnFriend* Other = Friends(i);
            if (CompareFriend(&Other, &Added) >= 0)
            {
                Friends.Insert(i, 1);
                Friends(i) = Added;
                Index = i;
                break;
            }
        }
        if (Index == -1)
        {
            Index = Friends.Add(1);
            Friends(Index) = Added;
        }
    }
    if (Index == -1)
        appFailAssert("-1 != nIndex", "MSN\\MsnFriend.cpp", 122);
    return Index;
}
typedef INT (FriendList::*SigFriendListAddFriend)(NMsnFriend);
NW_HOOK_VARARGS(0x101519c0, FriendList, AddFriend, SigFriendListAddFriend)

// 0x10151f30 (ret 0xC) - the contact of that account freed and taken out of the array (the first one), then the map's pairs
// of the account removed
void FriendList::RemoveFriend(FString Account)
{
    for (INT i = 0; i < Friends.Num(); i++)
    {
        NMsnFriend* Contact = Friends(i);
        if (Contact && Contact->Account == Account)
        {
            delete Contact;
#if NW_HOOK_MODE
            ::nw::StockCallAt<void>(0x1001F3F0, &Friends, i, 1);   // TPtrArray::Remove(i, 1)
#else
            Friends.Remove(i, 1);                                  // TPtrArray::Remove(i, 1): the standalone compiles the template
#endif
            break;
        }
    }
    ByAccount.Remove(*Account);
}
typedef void (FriendList::*SigFriendListRemoveFriend)(FString);
NW_HOOK_VARARGS(0x10151f30, FriendList, RemoveFriend, SigFriendListRemoveFriend)

/*-----------------------------------------------------------------------------
    NMsnContacts.
-----------------------------------------------------------------------------*/

// 0x101511b0 (ret 0xC) - the list of that name ("FL", "AL", "BL", "RL"), NULL for any other name (the name comes by value)
FriendList* NMsnContacts::GetList(FString ListName)
{
    if (ListName == TEXT("FL"))
        return FL;
    if (ListName == TEXT("AL"))
        return AL;
    if (ListName == TEXT("BL"))
        return BL;
    if (ListName == TEXT("RL"))
        return RL;
    return NULL;
}
typedef FriendList* (NMsnContacts::*SigContactsGetList)(FString);
NW_HOOK_VARARGS(0x101511b0, NMsnContacts, GetList, SigContactsGetList)

/*-----------------------------------------------------------------------------
    NMsnGroupList.
-----------------------------------------------------------------------------*/

// 0x10151660 (ret 4) - the group of that id, NULL when there is none
NMsnGroup* NMsnGroupList::FindGroup(INT GroupID)
{
    return (NMsnGroup*)ById.FindRef(GroupID);
}
NW_HOOK_METHOD(0x10151660, NMsnGroupList, FindGroup)

// 0x10151b90 (ret 0x1C) - a group of a known id takes the new name (a copy of the string); an unknown one is copied to the heap and
// entered in the map by its id and at the end of the array
void NMsnGroupList::AddGroup(NMsnGroup Group)
{
    NMsnGroup* Known = (NMsnGroup*)ById.FindRef(Group.GroupID);
    if (Known)
    {
        Known->SetName(Group.Name);       // NMsnGroup::SetName 0x101527B0 (ret 0xC, the name by value)
    }
    else
    {
        NMsnGroup* Added = new NMsnGroup(Group);
        INT Key = Group.GroupID;
        INT Value = (INT)Added;
        ById.Set(Key, Value);
        INT Index = Groups.Add(1);
        Groups(Index) = Added;
    }
}
typedef void (NMsnGroupList::*SigGroupListAddGroup)(NMsnGroup);
NW_HOOK_VARARGS(0x10151b90, NMsnGroupList, AddGroup, SigGroupListAddGroup)

// 0x10151c90 (ret 4) - the id leaves the map; the first group of that id leaves the array (its record is not freed)
void NMsnGroupList::RemoveGroup(INT GroupID)
{
    ById.Remove(GroupID);
    for (INT i = 0; i < Groups.Num(); i++)
    {
        NMsnGroup* Group = Groups(i);
        if (Group && Group->GroupID == GroupID)
        {
#if NW_HOOK_MODE
            ::nw::StockCallAt<void>(0x1001f3f0, &Groups, i, 1);   // TPtrArray::Remove(i, 1)
#else
            Groups.Remove(i, 1);                                  // TPtrArray::Remove(i, 1): the standalone compiles the template
#endif
            return;
        }
    }
}
NW_HOOK_METHOD(0x10151c90, NMsnGroupList, RemoveGroup)

// 0x10152150 (name ours) - the destructors of the four lists (0x1012D400, the msn2 lane's) and of the group list (0x1012D180); the
// memory is the caller's
void NMsnContacts::DestroyLists()
{
    ((NMsnListInit*)FL)->DeleteAll();         // 0x10151fe0
    ((NMsnListInit*)AL)->DeleteAll();
    ((NMsnListInit*)BL)->DeleteAll();
    ((NMsnListInit*)RL)->DeleteAll();
    ((NMsnGroupsInit*)Groups)->DeleteAll();   // 0x10151ce0
}
NW_HOOK_METHOD(0x10152150, NMsnContacts, DestroyLists)

