/*=============================================================================
    NMapBase24.cpp - the out-of-line instances of the engine's hash map (4 byte keys, 16 byte values) that the minimap's party
    map uses (C4 addresses): Rehash 0x100BA050 and the pairs array's destructor 0x100BB860. The sub_100BB820 between them
    (Relax) stays stock: it reads the first pair where the map's own counters should be.
=============================================================================*/
#include "nwindow/NMapBase24.h"

// 0x100ba050 - a new hash table of HashCount entries, every pair chained into the entry of its key (HashCount is a power of
// two), then the old table is freed
void NMapBase24::Rehash()
{
    INT* NewHash = (INT*)GMalloc->Malloc(HashCount << 2, TEXT("HashMapHash"));
    for (INT i = 0; i < HashCount; i++)
        NewHash[i] = -1;
    for (INT i = 0; i < Pairs.Num(); i++)
    {
        DWORD Index = (DWORD)Pairs(i).Key & (HashCount - 1);
        Pairs(i).HashNext = NewHash[Index];
        NewHash[Index] = i;
    }
    if (Hash)
        GMalloc->Free(Hash);
    Hash = NewHash;
}
NW_HOOK_METHOD(0x100ba050, NMapBase24, Rehash)

// 0x100bb860 - the pairs go (no destructor to run for a pair), then the array
NW_HOOK_DTOR(0x100bb860, TMapPair24Array)
