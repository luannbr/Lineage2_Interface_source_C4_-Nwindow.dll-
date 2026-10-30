#pragma once
// NMapBase24 - the engine's hash map with 4 byte keys and 16 byte values as the minimap's party map holds it (TMap<INT, PartyInfo>,
// UnTemplate.h's TMapBase): its pairs in a TArray (0x18 bytes each: the hash chain index, the key - which is its own hash - and the
// value), then the hash table (HashCount indices of the first pair of each chain, a power of two). Only the out-of-line instances
// the stock keeps are here: Rehash 0x100BA050 and the destructor of the pairs array 0x100BB860. NMinimapPartyMap
// (NCMinimapWnd.cpp) is the same layout without the methods.

#include "nwindow/NCWnd.h"
#pragma pack(push, 4)

struct NMapPair24
{
    INT HashNext;                    // +0x00 the next pair of the chain (-1: the end)
    INT Key;                         // +0x04 the key, used as its own hash
    BYTE Value[16];                  // +0x08 id, x, y, z of a party member
};
NW_ASSERT_SIZE(NMapPair24, 0x18);

typedef TArray<NMapPair24> TMapPair24Array;

struct NMapBase24
{
    TMapPair24Array Pairs;           // +0x00
    INT* Hash;                       // +0x0C (allocated with the tag "HashMapHash")
    INT HashCount;                   // +0x10

    void Rehash();                   // 0x100ba050 the hash table made again from the pairs
};
NW_ASSERT_OFFSET(NMapBase24, Hash, 0xC);
NW_ASSERT_SIZE(NMapBase24, 0x14);

#pragma pack(pop)
