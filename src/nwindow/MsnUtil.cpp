/*=============================================================================
    MsnUtil.cpp - the messenger's small helpers (C4 addresses): the client description (0x10168730 / 0x101687B0 / 0x10168810), the
    character set conversions (0x10168BE0 / 0x10168C00), the address canonicalizer (0x10168C90), the product key (0x10169010) and the
    tables of the windows (UE2's TMap instances). The protocol object's thin calls (0x1014F1C0, 0x1014F300, 0x1014F550, 0x1014F5C0) are
    Msn's members now (MsnLogin.cpp).
=============================================================================*/
#include "nwindow/MsnUtil.h"
#include "nwindow/MsnWnd.h"
#include "nwindow/NChatTypes.h"
#include "nwindow/MsnProtoOps.h"

#include <shlwapi.h>
#pragma comment(lib, "shlwapi.lib")

/*-----------------------------------------------------------------------------
    The client description.
-----------------------------------------------------------------------------*/

// 0x10168730 (cdecl) - the platform of the system as the protocol names it
FString* MsnPlatformName(FString* Out)
{
    TCHAR Name[32];
    OSVERSIONINFOA Info;
    Name[0] = 0;
    Info.dwOSVersionInfoSize = sizeof(Info);
#pragma warning(suppress : 4996)  // deprecated in current SDKs; the stock code calls it
    GetVersionExA(&Info);
    if (Info.dwPlatformId == VER_PLATFORM_WIN32s)
        appStrcpy(Name, TEXT("win32"));
    else if (Info.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS)
        appStrcpy(Name, TEXT("win"));
    else if (Info.dwPlatformId == VER_PLATFORM_WIN32_NT)
        appStrcpy(Name, TEXT("winnt"));
    new (Out) FString(Name);
    return Out;
}
NW_HOOK_FUNC(0x10168730, MsnPlatformName)

// 0x101687b0 (cdecl) - "major.minor" of the system
FString* MsnSystemVersion(FString* Out)
{
    TCHAR Text[32];
    OSVERSIONINFOA Info;
    Text[0] = 0;
    Info.dwOSVersionInfoSize = sizeof(Info);
#pragma warning(suppress : 4996)
    GetVersionExA(&Info);
    appSprintf(Text, TEXT("%d.%d"), Info.dwMajorVersion, Info.dwMinorVersion);
    new (Out) FString(Text);
    return Out;
}
NW_HOOK_FUNC(0x101687b0, MsnSystemVersion)

// 0x10168810 (cdecl) - the processor family as the protocol names it
FString* MsnProcessorName(FString* Out)
{
    TCHAR Name[32];
    SYSTEM_INFO Info;
    Name[0] = 0;
    Info.wProcessorArchitecture = 0;
    GetSystemInfo(&Info);
    if (Info.wProcessorArchitecture == 0)
        appStrcpy(Name, TEXT("i386"));
    else if (Info.wProcessorArchitecture == 1)
        appStrcpy(Name, TEXT("mips"));
    else if (Info.wProcessorArchitecture == 2)
        appStrcpy(Name, TEXT("alpha"));
    else if (Info.wProcessorArchitecture == 3)
        appStrcpy(Name, TEXT("ppc"));
    new (Out) FString(Name);
    return Out;
}
NW_HOOK_FUNC(0x10168810, MsnProcessorName)

/*-----------------------------------------------------------------------------
    Conversions.
-----------------------------------------------------------------------------*/

// 0x10168be0 (cdecl)
INT MsnWideToAnsi(const TCHAR* Wide, char* Out)
{
    return WideCharToMultiByte(CP_ACP, 0, Wide, -1, Out, 0x1040, NULL, NULL);
}
NW_HOOK_FUNC(0x10168be0, MsnWideToAnsi)

// 0x10168c00 (cdecl)
INT MsnAnsiToWide(const char* Ansi, TCHAR* Out)
{
    return MultiByteToWideChar(CP_ACP, 0, Ansi, -1, Out, 0x1040);
}
NW_HOOK_FUNC(0x10168c00, MsnAnsiToWide)

// 0x10168c90 (cdecl)
INT MsnUrlCanonicalize(const TCHAR* Url, TCHAR* Out)
{
    DWORD Length = 0x104;
    return UrlCanonicalizeW(Url, Out, &Length, 0x28001000);
}
NW_HOOK_FUNC(0x10168c90, MsnUrlCanonicalize)

// 0x10169010 (cdecl (ProductID, ProductKey), both out buffers of 0x1040 characters) - the messenger's product id and product key,
// kept enciphered (16 bytes each) in the image, deciphered with the Blowfish key "[;'.]94-31==-%&@!^+]" (21 bytes with the
// terminator; the key schedule is made again for each) and read as ANSI text. The stock reads the two blocks from its data
// (0x101AB8B4, 0x101AB8C4)
void MsnDecipherProduct(TCHAR* ProductID, TCHAR* ProductKey)
{
    static const BYTE EncryptedID[16] = {0x2e, 0xdf, 0xa0, 0xc5, 0xfb, 0x77, 0x11, 0x57,
                                         0xc5, 0xca, 0x20, 0x35, 0x09, 0x02, 0x10, 0xc5};
    static const BYTE EncryptedKey[16] = {0x6b, 0x42, 0x85, 0x43, 0xe3, 0x74, 0x24, 0x1c,
                                          0x1c, 0xb3, 0xb5, 0x2e, 0x0c, 0xcc, 0x44, 0x10};
    FBlowFish Blow;
    BYTE Key[0x15];
    BYTE Buffer[0x104];
    memcpy(Key, "[;'.]94-31==-%&@!^+]", 0x15);
    appMemset(Buffer, 0, 0x104);
    memcpy(Buffer, EncryptedID, 16);
    Blow.InitializeBlowfish(Key, 0x15);
    Blow.BlowfishDecrypt(Buffer, 0x10);
    MsnAnsiToWide((const char*)Buffer, ProductID);
    appMemset(Buffer, 0, 0x104);
    memcpy(Buffer, EncryptedKey, 16);
    Blow.InitializeBlowfish(Key, 0x15);
    Blow.BlowfishDecrypt(Buffer, 0x10);
    MsnAnsiToWide((const char*)Buffer, ProductKey);
}
NW_HOOK_FUNC(0x10169010, MsnDecipherProduct)

/*-----------------------------------------------------------------------------
    The tables (UE2's TMap of the messenger's windows, the pairs in an array chained through a hash of the key).
-----------------------------------------------------------------------------*/

// 0x10119b20 (cdecl) - the hash of a text (case-insensitive CRC): the C++ port in NChatTypes.cpp, so that no ported code enters the
// stock function (it is not hooked: regcheck reads its partial cx as a register argument)
DWORD NStrihash(const TCHAR* Data);
static FORCEINLINE INT MsnHashText(const TCHAR* Text)
{
    return (INT)NStrihash(Text);
}

// The out-of-line instances of UE2's TArray the stock calls as functions of their own (the compiler would inline the template into every
// caller): the name pairs' Remove(Index, Count) (0x1013FEF0: NChatNameArray::Remove, hooked in NChatTypes.cpp; the windows' pairs of a name
// and a window are the same 0x14 bytes) and the room for one more 12-byte pair at the end (0x10148F30)
__declspec(noinline) static void MsnRemoveNamePairs(void* Array, INT Index, INT Count)
{
    ((NChatNameArray*)Array)->Remove(Index, Count);
}
__declspec(noinline) static void* __cdecl MsnNewWndIntPair(size_t Size, TArray<NMsnWndIntPair>& Array)
{
    return operator new(Size, Array);
}

// 0x101514f0 (ret 4) - the window entered by this name
NCWnd* NMsnWndStrMap::FindWnd(const FString* Key)
{
    INT Index = Hash[MsnHashText(**Key) & (HashCount - 1)];
    while (Index != -1)
    {
        NMsnWndStrPair& Pair = Data[Index];
        if (Pair.Key == *Key)
            return Pair.Wnd;
        Index = Pair.HashNext;
    }
    return NULL;
}
NW_HOOK_METHOD(0x101514f0, NMsnWndStrMap, FindWnd)

// 0x1015ad40 (ret 8) - the window entered by this name becomes Value (a new entry, made by the protocol lane's 0x10134840, when
// there is none); the place of the window in the table is given back
NCWnd** NMsnWndStrMap::SetWnd(const TCHAR* Key, NCWnd** Value)
{
    INT Index = Hash[MsnHashText(Key) & (HashCount - 1)];
    while (Index != -1)
    {
        NMsnWndStrPair& Pair = Data[Index];
        if (Pair.Key == Key)
        {
            Pair.Wnd = *Value;
            return &Data[Index].Wnd;
        }
        Index = Pair.HashNext;
    }
    return ((NMsnWndStrMapOps*)this)->AddWnd(Key, Value);
}
NW_HOOK_METHOD(0x1015ad40, NMsnWndStrMap, SetWnd)

// 0x10151770 (ret 8) - the window entered by this number becomes Value (a new pair at the end of the array, chained into the
// hash, the hash doubled when the pairs outgrow it, when there is none); the place of the window in the table is given back
NCWnd** NMsnWndIntMap::SetWnd(const INT* Key, NCWnd** Value)
{
    INT Number = *Key;
    INT Index = Hash[Number & (HashCount - 1)];
    while (Index != -1)
    {
        NMsnWndIntPair& Pair = Data[Index];
        if (Pair.Key == Number)
        {
            Pair.Wnd = *Value;
            return &Data[Index].Wnd;
        }
        Index = Pair.HashNext;
    }
    NMsnWndIntPair* Pair = (NMsnWndIntPair*)MsnNewWndIntPair(0xC, Pairs());   // room for one more at the end
    if (Pair)
    {
        Pair->Key = *Key;
        Pair->Wnd = *Value;
    }
    INT Slot = Pair->Key & (HashCount - 1);
    Pair->HashNext = Hash[Slot];
    Hash[Slot] = Num - 1;
    if (HashCount * 2 + 8 < Num)
    {
        HashCount = HashCount * 2;
        ((NMsnIntMap*)this)->Rehash();
    }
    return &Pair->Wnd;
}
NW_HOOK_METHOD(0x10151770, NMsnWndIntMap, SetWnd)

// 0x1015be40 (ret 8) - the entries of this name that hold this window go, from the last to the first (the removal is the chat
// lane's 0x10136400); when some went, the hash is made smaller while it is more than twice the pairs plus 8 and rebuilt; the
// number removed comes back
INT NMsnWndStrMap::RemoveWnd(const TCHAR* Key, NCWnd** Value)
{
    INT Removed = 0;
    for (INT i = Num - 1; i >= 0; i--)
    {
        if (Data[i].Key == Key && Data[i].Wnd == *Value)
        {
            MsnRemoveNamePairs(this, i, 1);
            Removed++;
        }
    }
    if (Removed)
    {
        if (HashCount > Num * 2 + 8)
        {
            do
                HashCount = HashCount / 2;
            while (HashCount > Num * 2 + 8);
        }
        ((NChatNameMap*)this)->Rehash();
    }
    return Removed;
}
NW_HOOK_METHOD(0x1015be40, NMsnWndStrMap, RemoveWnd)

// 0x1015bed0 - all the pairs removed (through the chat lane's 0x10136400, which frees each name), then the array's destructor
void NMsnWndStrMap::FreePairs()
{
    MsnRemoveNamePairs(this, 0, Num);
    ((FArray*)this)->~FArray();
    Data = NULL;                     // (nothing reads it again; the store keeps this a function that returns, as the stock one does)
}
NW_HOOK_METHOD(0x1015bed0, NMsnWndStrMap, FreePairs)

// 0x1015e0d0 - the hash freed, then the pairs
void NMsnWndStrMap::Free()
{
    if (Hash)
        appFree(Hash);
    Hash = NULL;
    HashCount = 0;
    FreePairs();
    Num = 0;
}
NW_HOOK_METHOD(0x1015e0d0, NMsnWndStrMap, Free)

// 0x1015ae90 - the array of pairs of numbers emptied with UnTemplate.h's checks, then freed (the pairs have nothing to free)
void NMsnWndIntMap::FreePairs()
{
    typedef TArray<NMsnWndIntPair> PairArray;
    Pairs().~PairArray();
}
NW_HOOK_METHOD(0x1015ae90, NMsnWndIntMap, FreePairs)

// 0x1015bf20 - the hash freed, then the pairs
void NMsnWndIntMap::Free()
{
    if (Hash)
        appFree(Hash);
    Hash = NULL;
    HashCount = 0;
    FreePairs();
}
NW_HOOK_METHOD(0x1015bf20, NMsnWndIntMap, Free)

