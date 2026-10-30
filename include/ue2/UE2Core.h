/*=============================================================================
    UE2Core.h - the part of Core.dll (Lineage II C4 build, 27/05/2006) that NWindow uses.

    Written against the real binaries, not copied from a later SDK: the Interlude
    headers found on disk have a different UObject (0x34 bytes, C1 and C2 are 0x28).
    C2 vs the C1 build this file started from (tools/ue2_vtable_diff.py): UObject has one more
    virtual (ClearL2Game, slot 22), FFileManager one more (CreateMemReader, slot 1), FMalloc six
    more at the end; FArchive, FOutputDevice, FExec are the same.
    Rules for this file:
      * every function marked CORE_API is imported from Core.dll; its declaration
        must produce exactly the decorated name Core.dll exports (the link fails
        otherwise - third_party/ue_import/Core.def lists them all);
      * every virtual list follows the vtable read from the DLL
        (tools/engine_vtables.py);
      * every field has its offset asserted.
=============================================================================*/
#pragma once

#include <windows.h>
#include <stddef.h>
#include <string.h>
#include <malloc.h>
#include <new>

// winuser.h macros that would rename engine / NWindow methods of the same name
#undef DrawText
#undef DrawState
#undef GetObject
#undef SendMessage
#undef PostMessage
#undef CreateWindow
#undef GetMessage
#undef LoadString
#undef LoadImage
#undef SetWindowText
#undef GetWindowText
#undef GetClassName
#undef FindText
#undef MessageBox
#undef DefWindowProc

/*-----------------------------------------------------------------------------
    Types (VC7.0 Win32: TCHAR is unsigned short, build with /Zc:wchar_t-).
-----------------------------------------------------------------------------*/

static_assert(sizeof(wchar_t) == 2 && (wchar_t)-1 > 0, "build with /Zc:wchar_t-");

typedef unsigned char       BYTE;
typedef unsigned short      _WORD;
typedef unsigned long       DWORD;
typedef unsigned __int64    QWORD;
typedef signed char         SBYTE;
typedef signed short        SWORD;
typedef signed int          INT;
typedef signed __int64      SQWORD;
typedef char                ANSICHAR;
typedef wchar_t             UNICHAR;
typedef float               FLOAT;
typedef double              DOUBLE;
typedef unsigned long       BITFIELD;
typedef int                 UBOOL;      // ?GIsNative@@3HA: int

#ifndef TEXT
#define TEXT(s) L##s
#endif

#define CORE_API    __declspec(dllimport)
#define ENGINE_API  __declspec(dllimport)
#define DLL_EXPORT  __declspec(dllexport)
#define VARARGS     __cdecl
#define STDCALL     __stdcall
#define FORCEINLINE __forceinline

#undef MAXINT
#define MAXINT      0x7fffffff
#define ARRAY_COUNT(a) (sizeof(a) / sizeof((a)[0]))

// Field offset check used next to every decompiled layout.
#define NW_ASSERT_OFFSET(Type, Field, Off) \
    static_assert(offsetof(Type, Field) == (Off), #Type "::" #Field " must be at " #Off)
#define NW_ASSERT_SIZE(Type, Size) static_assert(sizeof(Type) == (Size), "sizeof(" #Type ") must be " #Size)

class UObject;
class UClass;
class UPackage;
class UFunction;
class UProperty;
class UPackageMap;
class FArchive;
class FOutputDevice;
class FGuid;
struct FFrame;
struct FScriptDelegate;
struct FStateFrame;
class ULinkerLoad;

/*-----------------------------------------------------------------------------
    Assertions and exception frames (UnBuild.h / Core.h).
-----------------------------------------------------------------------------*/

CORE_API void appFailAssert(const ANSICHAR* Expr, const ANSICHAR* File, INT Line);
CORE_API void VARARGS appUnwindf(const TCHAR* Fmt, ...);

#define check(expr) { if (!(expr)) appFailAssert(#expr, __FILE__, __LINE__); }
// check() of code the stock build expanded from UE2's headers: the fatal error names the
// original's file and line, and the expression is written as there (#expr is its text)
#define NW_CHECK_AT(expr, File, Line) { if (!(expr)) appFailAssert(#expr, File, Line); }
#define NW_UNTEMPLATE_H "e:\\work\\2004_06_07\\warfare2110\\core\\inc\\UnTemplate.h"

// guard/unguard exactly as the stock code has them (catch(TCHAR*) rethrow, catch(...)
// records the function in the crash history). Needs /EHa for catch(...) to see GPFs.
#define guard(func) { static const TCHAR __FUNC_NAME__[] = TEXT(#func); try {
#define unguard } catch (TCHAR* Err) { throw Err; } catch (...) { appUnwindf(TEXT("%s"), __FUNC_NAME__); throw; } }
#define unguardf(msg) } catch (TCHAR* Err) { throw Err; } catch (...) { appUnwindf(TEXT("%s"), __FUNC_NAME__); appUnwindf msg; throw; } }

/*-----------------------------------------------------------------------------
    Memory (FMalloc vtable: 13 slots, the first 7 pure in Core, the 6 statistics getters after
    them exported; C1 had only the 7).
-----------------------------------------------------------------------------*/

class FMalloc
{
public:
    virtual void* Malloc(DWORD Count, const TCHAR* Tag) = 0;
    virtual void* Realloc(void* Original, DWORD Count, const TCHAR* Tag) = 0;
    virtual void Free(void* Original) = 0;
    virtual void DumpAllocs() = 0;
    virtual void HeapCheck() = 0;
    virtual void Init(UBOOL Reset) = 0;
    virtual void Exit() = 0;
    CORE_API virtual INT GetOsPeak();           // 7 (new in C2)
    CORE_API virtual INT GetOsCurrent();        // 8
    CORE_API virtual INT GetUsedPeak();         // 9
    CORE_API virtual INT GetUsedCurrent();      // 10
    CORE_API virtual INT GetCurrentAllocs();    // 11
    CORE_API virtual INT GetTotalAllocs();      // 12
};

CORE_API extern FMalloc* GMalloc;

// Core's SSE flag (Core.def: `GIsSSE DATA`, an UNDECORATED C symbol, so extern "C"): FConvexVolume::BoxCheck / SphereCheck pick their SSE or their x87 path by it (EngineGeometry.cpp)
extern "C" CORE_API INT GIsSSE;

inline void* appMalloc(DWORD Count, const TCHAR* Tag) { return GMalloc->Malloc(Count, Tag); }
inline void* appRealloc(void* Original, DWORD Count, const TCHAR* Tag) { return GMalloc->Realloc(Original, Count, Tag); }
inline void appFree(void* Original) { GMalloc->Free(Original); }

// UE2's stack allocator (UnMem.h): only the members NWindow uses (FMemMark pops what a
// collision query left on GMem).
// C4 (Core.dll 27/05/2006): the stack is per thread. GMem (FMemStack) hands out the calling thread's FMemStackFrame through the private
// GetThreadSafeMemStack(), the frame has the old FMemStack's four members (Top, End, DefaultChunkSize, TopChunk) and FreeChunks, and
// FMemMark::Pop is out of line (Core exports it; the C4 NWindow imports it). The mark itself is inline in the NWindow: its constructor
// stores Top and TopChunk of the frame it gets and the frame (0xC bytes: Top, Frame, SavedChunk), as the compiled
// FMemMark(GMem) of the C4 NWindow does.
struct FTaggedMemory;                   // (C3: FMemStack::FTaggedMemory)

class FMemStackFrame
{
public:
    BYTE* Top;                          // +0x00 top of the current chunk
    BYTE* End;                          // +0x04
    INT DefaultChunkSize;               // +0x08
    FTaggedMemory* TopChunk;            // +0x0C

    CORE_API void FreeChunks(FTaggedMemory* NewTopChunk);    // ?FreeChunks@FMemStackFrame@@QAEXPAUFTaggedMemory@@@Z
};

class FMemStack
{
private:
    CORE_API FMemStackFrame& GetThreadSafeMemStack();        // ?GetThreadSafeMemStack@FMemStack@@AAEAAVFMemStackFrame@@XZ
    friend class FMemMark;
};

CORE_API extern FMemStack GMem;

class FMemMark
{
public:
    FMemMark(FMemStack& InMem)
    {
        FMemStackFrame& F = InMem.GetThreadSafeMemStack();
        Top = F.Top;
        Frame = &F;
        SavedChunk = F.TopChunk;
    }
    CORE_API void Pop();                // ?Pop@FMemMark@@QAEXXZ

private:
    BYTE* Top;                          // +0x00
    FMemStackFrame* Frame;              // +0x04
    FTaggedMemory* SavedChunk;          // +0x08
};
NW_ASSERT_SIZE(FMemMark, 0xC);

// Global operator new/delete go through GMalloc (src/re/NwMemory.cpp), as in every UE2
// module: objects cross module boundaries and must come from one heap.
inline void* operator new(size_t Size, const TCHAR* Tag) { return appMalloc((DWORD)Size, Tag); }
inline void* operator new[](size_t Size, const TCHAR* Tag) { return appMalloc((DWORD)Size, Tag); }
inline void operator delete(void* Ptr, const TCHAR*) { appFree(Ptr); }
inline void operator delete[](void* Ptr, const TCHAR*) { appFree(Ptr); }

/*-----------------------------------------------------------------------------
    String / memory helpers exported by Core.
-----------------------------------------------------------------------------*/

CORE_API TCHAR* appStrcpy(TCHAR* Dest, const TCHAR* Src);
CORE_API TCHAR* appStrcat(TCHAR* Dest, const TCHAR* Src);
CORE_API TCHAR* appStrncpy(TCHAR* Dest, const TCHAR* Src, INT Max);
CORE_API INT appStrlen(const TCHAR* String);
CORE_API INT appStrcmp(const TCHAR* String1, const TCHAR* String2);
CORE_API INT appStricmp(const TCHAR* String1, const TCHAR* String2);   // the same without case (the data loaders' word tables)
CORE_API TCHAR* appStrchr(const TCHAR* String, INT Char);                  // ?appStrchr@@YAPAGPBGH@Z (EngineUrl.cpp: FURL's text parsing)
CORE_API INT appStrnicmp(const TCHAR* A, const TCHAR* B, INT Count);       // ?appStrnicmp@@YAHPBG0H@Z: appStricmp over at most Count characters (FURL)
CORE_API TCHAR* appStrstr(const TCHAR* String, const TCHAR* Find);
CORE_API TCHAR* appStristr(const TCHAR* String, const TCHAR* Find);
CORE_API TCHAR* appStrrchr(const TCHAR* String, INT Char);         // ?appStrrchr@@YAPAGPBGH@Z (new in C2): the last Char
CORE_API INT appAtoi(const TCHAR* Str);
CORE_API FLOAT appAtof(const TCHAR* Str);
CORE_API TCHAR* appItoa(INT Num);                       // L2: returns a TCHAR*, not an FString
CORE_API INT VARARGS appSprintf(TCHAR* Dest, const TCHAR* Fmt, ...);
CORE_API const INT VARARGS appMsgf(INT Type, const TCHAR* Fmt, ...);   // a message box (the stock data loaders: script errors)
CORE_API INT appRand();                                     // rand() % RAND_MAX
CORE_API void appRandInit(INT Seed);                        // ?appRandInit@@YAXH@Z (new in C2): srand(Seed)
CORE_API extern DWORD GCRCTable[];                          // the CRC-32 table appStrihash walks
CORE_API void* appMemmove(void* Dest, const void* Src, INT Count);
CORE_API INT winGetSizeANSI(const TCHAR* InUCh);
CORE_API ANSICHAR* winToANSI(ANSICHAR* ACh, const TCHAR* InUCh, INT Count);
#define appAlloca(size) ((size) == 0 ? 0 : _alloca(((size) + 7) & ~7))
#define TCHAR_TO_ANSI(str) winToANSI((ANSICHAR*)appAlloca(winGetSizeANSI(str)), str, winGetSizeANSI(str))
// inline (SSE asm) in the headers NWindow was built with: the stock image carries its own
// out-of-line copy at 0x1000e320
inline void appMemcpy(void* Dest, const void* Src, INT Count) { memcpy(Dest, Src, Count); }
typedef INT (__cdecl* FQsortCompare)(const void* A, const void* B);
CORE_API void appQsort(void* Base, INT Num, INT Size, FQsortCompare Compare);   // new in C3

// The cipher of the data files, a class of Core.dll in C3 (in C2 FL2GameData held its 0x1048 bytes inline and the
// Engine did the work): the Engine's FL2GameData::BlowFish member is one of these (key schedule: 18 + 4 * 256 dwords)
class FBlowFish
{
public:
    CORE_API FBlowFish();
    CORE_API SWORD InitializeBlowfish(BYTE* const Key, SWORD KeyLength);   // (argument names ours)
    CORE_API void BlowfishDecrypt(BYTE* Data, INT Length);
private:
    BYTE _Schedule[0x1048];
};
NW_ASSERT_SIZE(FBlowFish, 0x1048);
CORE_API void appMemset(void* Dest, INT C, INT Count);
CORE_API INT appFloor(FLOAT F);
CORE_API DOUBLE appSin(DOUBLE Value);
CORE_API DOUBLE appCos(DOUBLE Value);
CORE_API DOUBLE appSqrt(DOUBLE Value);   // ?appSqrt@@YANN@Z: `fld qword [esp+4]; fsqrt; ret` (the camera shake's Mgr::Init calls it, EngineViewShake.cpp)
CORE_API FLOAT appFrand();
CORE_API void appSystemTime(INT& Year, INT& Month, INT& DayOfWeek, INT& Day, INT& Hour, INT& Min,
                            INT& Sec, INT& MSec);
CORE_API void appRequestExit(UBOOL Force);
// L2's local time (5 fields; order from NCServerSelectWnd's use)
CORE_API void appLocalTime(INT& Year, INT& Month, INT& Day, INT& Hour, INT& Min);
// a server time (seconds) split the same way (order from systemmsg 713, "yyyy/mm/dd hh:mm")
CORE_API void appConvertIntToTime(INT Time, INT& Year, INT& Month, INT& Day, INT& Hour, INT& Min);

/*-----------------------------------------------------------------------------
    FArray / TArray (UnTemplate.h). Layout: Data, ArrayNum, ArrayMax.
-----------------------------------------------------------------------------*/

enum { INDEX_NONE = -1 };   // UE2: what a search that finds nothing returns

class FArray
{
public:
    void* GetData() { return Data; }
    const void* GetData() const { return Data; }
    INT Num() const { return ArrayNum; }
    UBOOL IsValidIndex(INT i) const { return i >= 0 && i < ArrayNum; }

    CORE_API INT Add(INT Count, INT ElementSize);
    CORE_API INT AddZeroed(INT ElementSize, INT Count = 1);
    CORE_API void Insert(INT Index, INT Count, INT ElementSize);
    CORE_API void Remove(INT Index, INT Count, INT ElementSize);
    CORE_API ~FArray();
    // exported too, but inline in the headers NWindow was built with
    void Empty(INT ElementSize, INT Slack = 0)
    {
        ArrayNum = 0;
        ArrayMax = Slack;
        Realloc(ElementSize);
    }

protected:
    FArray() : Data(NULL), ArrayNum(0), ArrayMax(0) {}
    FArray(INT InNum, INT ElementSize) : Data(NULL), ArrayNum(InNum), ArrayMax(InNum)
    {
        Realloc(ElementSize);
    }
    CORE_API void Realloc(INT ElementSize);

    void* Data;
    INT ArrayNum;
    INT ArrayMax;
};
NW_ASSERT_SIZE(FArray, 0xC);

template <class T> class TArray;
template <class T> void* operator new(size_t Size, TArray<T>& Array);
template <class TK, class TI> class TMap;   // only named (engine virtuals' parameters)

template <class T> class TArray : public FArray
{
public:
    typedef T ElementType;
    TArray() {}
    TArray(const TArray& Other) : FArray(Other.ArrayNum, sizeof(T))
    {
        ArrayNum = 0;
        for (INT i = 0; i < Other.ArrayNum; i++)
            new (*this) T(Other(i));
    }
    ~TArray() { Remove(0, ArrayNum); }
    TArray& operator=(const TArray& Other)
    {
        if (this != &Other)
        {
            Empty(Other.ArrayNum);
            for (INT i = 0; i < Other.ArrayNum; i++)
                new (*this) T(Other(i));
        }
        return *this;
    }

    T& operator()(INT i) { return ((T*)Data)[i]; }
    const T& operator()(INT i) const { return ((T*)Data)[i]; }
    T& Last(INT c = 0) { return ((T*)Data)[ArrayNum - c - 1]; }

    INT Add(INT n = 1) { return FArray::Add(n, sizeof(T)); }
    INT AddZeroed(INT n = 1) { return FArray::AddZeroed(sizeof(T), n); }
    void Insert(INT Index, INT Count = 1) { FArray::Insert(Index, Count, sizeof(T)); }
    // the stock build keeps UE2's check()s here (UnTemplate.h 515-517)
    void Remove(INT Index, INT Count = 1)
    {
        NW_CHECK_AT(Index>=0, NW_UNTEMPLATE_H, 515);
        NW_CHECK_AT(Index<=ArrayNum, NW_UNTEMPLATE_H, 516);
        NW_CHECK_AT(Index+Count<=ArrayNum, NW_UNTEMPLATE_H, 517);
        for (INT i = Index; i < Index + Count; i++)
            (&(*this)(i))->~T();
        FArray::Remove(Index, Count, sizeof(T));
    }
    // UnTemplate.h 398 (the stock code inlines it; the HTML alignment stacks' out-of-line copy is
    // 0x10029fc0)
    FORCEINLINE T Pop()
    {
        NW_CHECK_AT(ArrayNum>0, NW_UNTEMPLATE_H, 398);
        T Result = ((T*)Data)[ArrayNum - 1];
        Remove(ArrayNum - 1);
        return Result;
    }
    INT AddItem(const T& Item)
    {
        INT Index = Add();
        new (&(*this)(Index)) T(Item);
        return Index;
    }
    INT FindItemIndex(const T& Item) const
    {
        for (INT Index = 0; Index < ArrayNum; Index++)
            if ((*this)(Index) == Item)
                return Index;
        return INDEX_NONE;
    }
    // UE2's RemoveItem: every element equal to Item goes, the number removed comes back
    INT RemoveItem(const T& Item)
    {
        INT OriginalNum = ArrayNum;
        for (INT Index = 0; Index < ArrayNum; Index++)
            if ((*this)(Index) == Item)
                Remove(Index--);
        return OriginalNum - ArrayNum;
    }
    void Empty(INT Slack = 0)
    {
        for (INT i = 0; i < ArrayNum; i++)
            (&(*this)(i))->~T();
        ArrayNum = 0;
        ArrayMax = Slack;
        Realloc(sizeof(T));
    }
};

// new(Array) T(...): constructs a new last element in place
template <class T> void* operator new(size_t Size, TArray<T>& Array)
{
    INT Index = Array.FArray::Add(1, sizeof(T));
    return &Array(Index);
}
template <class T> void operator delete(void*, TArray<T>&) {}

/*-----------------------------------------------------------------------------
    FString (TArray<TCHAR>, null terminated when not empty).
-----------------------------------------------------------------------------*/

class FString : protected TArray<TCHAR>
{
public:
    CORE_API FString();
    CORE_API FString(const FString& Other);
    CORE_API FString(const TCHAR* In);
    CORE_API ~FString();
    CORE_API FString& operator=(const TCHAR* Other);
    CORE_API FString& operator=(const FString& Other);
    CORE_API FString& operator+=(const TCHAR* Str);
    CORE_API FString& operator+=(const FString& Str);
    CORE_API FString operator+(const FString& Str);          // non-const in this build
    CORE_API FString operator+(const TCHAR* Str);            // ??HFString@@QAE?AV0@PBG@Z (new in C2)
    CORE_API const TCHAR* operator*() const;
    CORE_API UBOOL operator==(const TCHAR* Other) const;
    CORE_API UBOOL operator==(const FString& Other) const;  // case-insensitive (appStricmp)
    CORE_API UBOOL operator!=(const TCHAR* Other) const;    // ??9FString@@QBEHPBG@Z (new in C3)
    CORE_API UBOOL operator!=(const FString& Other) const;  // ??9FString@@QBEHABV0@@Z (new in C3)
    CORE_API FString Locs() const;                          // ?Locs@FString@@QBE?AV1@XZ (new in C3: the localized text)
    CORE_API FString Substitute(FString Find, FString Replace);   // ?Substitute@FString@@QAE?AV1@V1@0@Z (new in C3; names ours)
    CORE_API INT InStr(const TCHAR* SubStr, UBOOL Right = 0) const;
    CORE_API static FString VARARGS Printf(const TCHAR* Fmt, ...);   // ?Printf@FString@@SA?AV1@PBGZZ: the formatted text by value (FURL::String's port)
    CORE_API FString Left(INT Count) const;
    CORE_API FString Right(INT Count) const;
    CORE_API FString Mid(INT Start, INT Count = MAXINT) const;
    CORE_API void Empty();
    CORE_API void Delete(INT Index, INT Count);

    using TArray<TCHAR>::Num;  // characters + terminator (0 when empty)
    INT Len() const { return Num() ? Num() - 1 : 0; }
    // the raw buffer, NULL when empty (unlike operator*, which gives TEXT(""))
    const TCHAR* GetCharData() const { return (const TCHAR*)GetData(); }
};
NW_ASSERT_SIZE(FString, 0xC);

struct FStringNoInit : public FString       // a struct in this build (??0FStringNoInit@@QAE@ABU0@@Z)
{
    CORE_API FStringNoInit(const FStringNoInit& Other);
    CORE_API ~FStringNoInit();
    CORE_API FStringNoInit& operator=(const TCHAR* Other);   // the one the data loaders use (??4FStringNoInit@@QAEAAU0@PBG@Z)
};

/*-----------------------------------------------------------------------------
    FName.
-----------------------------------------------------------------------------*/

enum EName { NAME_None = 0 };
enum EFindName { FNAME_Find, FNAME_Add, FNAME_Intrinsic };

class FName
{
public:
    FName() {}
    FName(EName N) : Index(N) {}
    CORE_API FName(const TCHAR* Name, EFindName FindType);
    CORE_API const TCHAR* operator*() const;
    INT GetIndex() const { return Index; }
    UBOOL operator==(const FName& Other) const { return Index == Other.Index; }
    UBOOL operator!=(const FName& Other) const { return Index != Other.Index; }

private:
    INT Index;
};
NW_ASSERT_SIZE(FName, 4);

/*-----------------------------------------------------------------------------
    The memory cache (UnCache.h): GCache, an Engine variable. It holds data derived from the level
    (lightmaps, static mesh collision, crests), not UI textures; UEngine::Flush empties it. Only
    what is needed is declared: the object is 0x10058 bytes or more, only its address is used.
-----------------------------------------------------------------------------*/

class CORE_API FMemCache
{
public:
    void Flush(QWORD Id, DWORD Mask, INT Item);     // (0, 0xFFFFFFFF, 0): every item
private:
    BYTE _Opaque[1];
};

/*-----------------------------------------------------------------------------
    Output devices.
-----------------------------------------------------------------------------*/

class FOutputDevice
{
public:
    virtual void Serialize(const TCHAR* V, EName Event) = 0;
    CORE_API virtual void Flush();
    CORE_API virtual void Release();
    CORE_API void VARARGS Logf(const TCHAR* Fmt, ...);
    CORE_API void VARARGS Logf2(const TCHAR* Fmt, ...);     // L2 addition
    // the overload with the category (?Logf2@FOutputDevice@@QAAXW4EName@@PBGZZ): the polygon code passes the raw name index 0x2FF (NAME_Warning in UE2, INFERRED; only the index is read in the stock code)
    CORE_API void VARARGS Logf2(EName Event, const TCHAR* Fmt, ...);
};

CORE_API extern FOutputDevice* GLog;
CORE_API extern FOutputDevice* GNull;

class FOutputDeviceError : public FOutputDevice
{
};
CORE_API extern FOutputDeviceError* GError;

// localized error text of a key (Core's Localize family)
CORE_API const TCHAR* LocalizeError(const TCHAR* Key, const TCHAR* Package = TEXT("Core"),
                                    const TCHAR* LangExt = NULL);

/*-----------------------------------------------------------------------------
    Archives and files. Only called through pointers the engine hands out, never derived
    from here, so only the virtual order matters (??_7FArchive@@6B@: 23 slots).
-----------------------------------------------------------------------------*/

class FLazyLoader;
class FRoughLoader;   // C3: FArchive::AttachRoughLoader / DetachRoughLoader (layout not read)

class FArchive
{
public:
    CORE_API virtual ~FArchive();                                    // 0
    CORE_API virtual void Serialize(void* V, INT Length);            // 1
    CORE_API virtual void SerializeBits(void* V, INT LengthBits);    // 2
    CORE_API virtual void SerializeInt(DWORD& Value, DWORD Max);     // 3
    CORE_API virtual void Preload(UObject* Object);                  // 4
    CORE_API virtual void CountBytes(DWORD InNum, DWORD InMax);      // 5
    CORE_API virtual FArchive& operator<<(FName& N);                 // 7 (MSVC reverses overloads)
    CORE_API virtual FArchive& operator<<(UObject*& Res);            // 6
    CORE_API virtual INT MapName(FName* Name);                       // 8
    CORE_API virtual INT MapObject(UObject* Object);                 // 9
    CORE_API virtual INT Tell();                                     // 10
    CORE_API virtual INT TotalSize();                                // 11
    CORE_API virtual UBOOL AtEnd();                                  // 12
    CORE_API virtual UBOOL AtStopper();                              // 13
    CORE_API virtual void SetStopper(INT InStopper);                 // 14
    CORE_API virtual void Seek(INT InPos);                           // 15
    CORE_API virtual void AttachLazyLoader(FLazyLoader* LazyLoader); // 16
    CORE_API virtual void DetachLazyLoader(FLazyLoader* LazyLoader); // 17
    CORE_API virtual void AttachRoughLoader(FRoughLoader* RoughLoader); // 18 (new in C3; argument name ours)
    CORE_API virtual void DetachRoughLoader(FRoughLoader* RoughLoader); // 19 (new in C3)
    CORE_API virtual void Precache(INT HintCount);                   // 20 (C2: 18)
    CORE_API virtual void Flush();                                   // 21 (C2: 19)
    CORE_API virtual UBOOL Close();                                  // 22 (C2: 20)
    CORE_API virtual UBOOL GetError();                               // 23 (C2: 21)
    CORE_API virtual INT GetCurrentEncryptVersion();                 // 24 (L2 addition; C2: 22)
    CORE_API FArchive& ByteOrderSerialize(void* V, INT Length);      // ?ByteOrderSerialize@FArchive@@QAEAAV1@PAXH@Z: NOT virtual (the vtable does not change); FURL's operator<<
};
CORE_API FArchive& operator<<(FArchive& Ar, FString& S);             // ??6@YAAAVFArchive@@AAV0@AAVFString@@@Z (FURL's operator<<)

enum EFileWrite
{
    FILEWRITE_NoFail            = 0x01,
    FILEWRITE_NoReplaceExisting = 0x02,
    FILEWRITE_EvenIfReadOnly    = 0x04,
    FILEWRITE_Unbuffered        = 0x08,
    FILEWRITE_Append            = 0x10,
    FILEWRITE_AllowRead         = 0x20,
};

// ??_7FFileManager@@6B@: 16 slots in C2 (C1: 15), 2..14 pure (the platform file manager lives in
// Core); only the ones NWindow calls are declared
class FFileManager
{
public:
    CORE_API virtual void Init(UBOOL Startup);                                              // 0
    CORE_API virtual FArchive* CreateMemReader(void* Data, INT Size);                       // 1 (new in C2)
    virtual FArchive* CreateFileReader(const TCHAR* Filename, DWORD ReadFlags = 0,
                                       FOutputDevice* Error = GNull) = 0;                  // 2
    virtual FArchive* CreateFileWriter(const TCHAR* Filename, DWORD WriteFlags = 0,
                                       FOutputDevice* Error = GNull) = 0;                  // 3
    virtual INT FileSize(const TCHAR* Filename) = 0;                                        // 4
};

CORE_API extern FFileManager* GFileManager;

/*-----------------------------------------------------------------------------
    UObject. C1, C2 and C3 keep the stock UE2 layout (0x28 bytes); the C4 Core.dll/Engine.dll (27/05/2006)
    add three dwords at +0x28..+0x30 (0x34 bytes: every UObject-derived class moves by 0xC, NCObject
    included). The C2 vtable has the three
    FUnknown slots, the destructor and 21 UObject virtuals (25 slots: ClearL2Game, new in C2, sits
    at slot 22, before Rename and NetDirty, which moved to 23 and 24; every engine class derived
    from UObject has one more slot than in C1).
-----------------------------------------------------------------------------*/

// EObjectFlags (only the ones NWindow touches so far)
enum
{
    RF_Public     = 0x00000004,
    RF_Transient  = 0x00004000,
    RF_Standalone = 0x00080000,
    RF_Native     = 0x04000000,
};

enum EGotoState { GOTOSTATE_NotFound, GOTOSTATE_Success, GOTOSTATE_Preempted };

#define ANY_PACKAGE ((UObject*)-1)   // UPackage* in the SDK; UObject* avoids needing UPackage here

class FGuid
{
public:
    DWORD A, B, C, D;
};

class UObject
{
public:
    // FUnknown
    CORE_API virtual DWORD STDCALL QueryInterface(const FGuid& RefIID, void** InterfacePtr);
    CORE_API virtual DWORD STDCALL AddRef();
    CORE_API virtual DWORD STDCALL Release();
    // UObject
    CORE_API virtual ~UObject();
    CORE_API virtual void ProcessEvent(UFunction* Function, void* Parms, void* Result = NULL);
    CORE_API virtual void ProcessDelegate(FName DelegateName, FScriptDelegate* Delegate, void* Parms, void* Result = NULL);
    CORE_API virtual void ProcessState(FLOAT DeltaSeconds);
    CORE_API virtual UBOOL ProcessRemoteFunction(UFunction* Function, void* Parms, FFrame* Stack);
    CORE_API virtual void Modify();
    CORE_API virtual void PostLoad();
    CORE_API virtual void Destroy();
    CORE_API virtual void Serialize(FArchive& Ar);
    CORE_API virtual UBOOL IsPendingKill();
    CORE_API virtual EGotoState GotoState(FName State);
    CORE_API virtual INT GotoLabel(FName Label);
    CORE_API virtual void InitExecution();
    CORE_API virtual void ShutdownAfterError();
    CORE_API virtual void PostEditChange();
    CORE_API virtual void CallFunction(FFrame& TheStack, void* const Result, UFunction* Function);
    CORE_API virtual UBOOL ScriptConsoleExec(const TCHAR* Cmd, FOutputDevice& Ar, UObject* Executor);
    CORE_API virtual void Register();
    CORE_API virtual void LanguageChange();
    CORE_API virtual void ClearL2Game();                                  // 22 (new in C2)
    CORE_API virtual void Rename(const TCHAR* NewName = NULL, UObject* NewOuter = NULL);
    CORE_API virtual void NetDirty(UProperty* Property);

    CORE_API UObject();
    CORE_API const TCHAR* GetName() const;                      // ?GetName@UObject@@QBEPBGXZ (new in C3 imports)
    CORE_API const TCHAR* GetFullName(TCHAR* Result) const;     // ?GetFullName@UObject@@QBEPBGPAG@Z
    CORE_API UBOOL ConditionalDestroy();
    CORE_API static void operator delete(void* Object, size_t Size);

    CORE_API static UObject* StaticFindObject(UClass* Class, UObject* InOuter, const TCHAR* Name, UBOOL ExactClass = 0);
    CORE_API static UObject* StaticLoadObject(UClass* Class, UObject* InOuter, const TCHAR* Name, const TCHAR* Filename,
                                              DWORD LoadFlags, UPackageMap* Sandbox);
    CORE_API static UClass* StaticLoadClass(UClass* BaseClass, UObject* InOuter, const TCHAR* Name, const TCHAR* Filename,
                                            DWORD LoadFlags, UPackageMap* Sandbox);
    CORE_API static UPackage* GetTransientPackage();
    CORE_API static const TCHAR* StaticConfigName();
    CORE_API static UBOOL StaticExec(const TCHAR* Cmd, FOutputDevice& Ar);  // "OBJ ..." and the like
    CORE_API static void CollectGarbage(DWORD KeepFlags);
    CORE_API void StaticConstructor();   // the static constructor IMPLEMENT_CLASS registers
    // new in C2 (the C2 NWindow imports it; argument names after UE2's): allocates, then constructs
    CORE_API static UObject* StaticConstructObject(UClass* Class, UObject* InOuter = (UObject*)GetTransientPackage(),
                                                   FName Name = NAME_None, DWORD SetFlags = 0, UObject* Template = NULL,
                                                   FOutputDevice* Error = GError, UObject* SubobjectRoot = NULL);
    CORE_API static UObject* StaticAllocateObject(UClass* Class, UObject* InOuter = (UObject*)GetTransientPackage(),
                                                  FName Name = NAME_None, DWORD SetFlags = 0, UObject* Template = NULL,
                                                  FOutputDevice* Error = GError, UObject* Ptr = NULL,
                                                  UObject* SubobjectRoot = NULL);
    // Not called by the stock code. Phase 6 (src/features/CombatLog.cpp): the combat log keeps
    // its windows through OBJ GARBAGE (GObjRoot) and tells a collected one by these.
    CORE_API void AddToRoot();
    CORE_API UBOOL IsValid();

    inline UBOOL IsA(UClass* SomeBaseClass) const;

    static UClass* StaticClass() { return &PrivateStaticClass; }

    DWORD GetFlags() const { return ObjectFlags; }
    void SetFlags(DWORD NewFlags) { ObjectFlags |= NewFlags; }
    void ClearFlags(DWORD NewFlags) { ObjectFlags &= ~NewFlags; }
    FName GetFName() const { return Name; }
    UClass* GetClass() const { return Class; }
    UObject* GetOuter() const { return Outer; }

protected:
    INT          Index;          // +0x04
    UObject*     HashNext;       // +0x08
    FStateFrame* StateFrame;     // +0x0C
    ULinkerLoad* _Linker;        // +0x10
    INT          _LinkerIndex;   // +0x14
    UObject*     Outer;          // +0x18
    DWORD        ObjectFlags;    // +0x1C
    FName        Name;           // +0x20
    UClass*      Class;          // +0x24
    // C4: three dwords the 27/05/2006 Core.dll added (names ours). UObject::UObject(ENativeConstructor, ...) and the
    // (EStaticConstructor) one write -1 at +0x28 and 0 at +0x2C; ~UObject indexes a second global object table
    // (Core 0x102D52F0) with +0x30, the way it indexes GObjObjects with Index (+0x04)
    INT          _L2Field28;     // +0x28 -1 until the object is registered
    INT          _L2Field2C;     // +0x2C 0
    INT          _L2Index30;     // +0x30 index in the second global object table (~UObject)

private:
    CORE_API static UClass PrivateStaticClass;
};
NW_ASSERT_SIZE(UObject, 0x34);   // C4 (C1, C2, C3: 0x28)

// Only the link used by UObject::IsA (C1: SuperField right after UObject).
class UField : public UObject
{
public:
    UField* SuperField;  // +0x34 (C1-C3: +0x28; Core's UObject::IsA reads [Class + 0x34])
    UField* Next;        // +0x38
};
NW_ASSERT_OFFSET(UField, SuperField, 0x34);

// UE2 inline: walks the class chain through UField::SuperField (UClass is a UField).
inline UBOOL UObject::IsA(UClass* SomeBaseClass) const
{
    for (UField* TempClass = reinterpret_cast<UField*>(Class); TempClass; TempClass = TempClass->SuperField)
        if (TempClass == reinterpret_cast<UField*>(SomeBaseClass))
            return 1;
    return SomeBaseClass == NULL;
}

// UE2's Cast (UnObjBas.h): Src when it is a T, else NULL
template <class T> T* Cast(UObject* Src)
{
    return Src && Src->IsA(T::StaticClass()) ? (T*)Src : NULL;
}

// UE2's CastChecked: the same, but a failed cast is logged to GError ("Cast of %s to %s failed") and the pointer
// comes back as it is (stock: one out-of-line instance per class, 0x10030ba0 for NCWnd)
template <class T> T* CastChecked(UObject* Src)
{
    if (!Src || !Src->IsA(T::StaticClass()))
        GError->Logf(TEXT("Cast of %s to %s failed"), Src ? Src->GetFullName(NULL) : TEXT("NULL"),
                     reinterpret_cast<UObject*>(T::StaticClass())->GetName());   // UClass is an UObject; it may be incomplete here
    return (T*)Src;
}

// ??_7FExec@@6B@ (1 slot)
class FExec
{
public:
    virtual UBOOL Exec(const TCHAR* Cmd, FOutputDevice& Ar) = 0;
};
CORE_API extern FExec* GExec;  // where console commands go first (set by the launcher)

// ??_7USubsystem@@6BUObject@@@: UObject 25 + Tick (C1: 24 + Tick); FExec at +0x34 (C1-C3: +0x28)
class USubsystem : public UObject, public FExec
{
public:
    CORE_API virtual void Tick(FLOAT DeltaTime);   // 25 (C1: 24)
};

/*-----------------------------------------------------------------------------
    FColor, configuration, parsing.
-----------------------------------------------------------------------------*/

class FColor
{
public:
    BYTE B, G, R, A;
    DWORD DWColor() const { return *(const DWORD*)this; }
};
NW_ASSERT_SIZE(FColor, 4);

// Interface only (the implementation lives in the executable; no vtable export). Slots from
// UE2 (UT2003); the calls NWindow makes use 0..5. MSVC reverses overloaded virtuals: the
// TCHAR* GetString is slot 4.
class FConfigCache
{
public:
    virtual UBOOL GetBool(const TCHAR* Section, const TCHAR* Key, UBOOL& Value,
                          const TCHAR* Filename = NULL) = 0;                                   // 0
    virtual UBOOL GetInt(const TCHAR* Section, const TCHAR* Key, INT& Value,
                         const TCHAR* Filename = NULL) = 0;                                    // 1
    virtual UBOOL GetFloat(const TCHAR* Section, const TCHAR* Key, FLOAT& Value,
                           const TCHAR* Filename = NULL) = 0;                                  // 2
    virtual UBOOL GetString(const TCHAR* Section, const TCHAR* Key, TCHAR* Value, INT Size,
                            const TCHAR* Filename = NULL) = 0;                                 // 4
    virtual UBOOL GetString(const TCHAR* Section, const TCHAR* Key, class FString& Str,
                            const TCHAR* Filename = NULL) = 0;                                 // 3
    virtual const TCHAR* GetStr(const TCHAR* Section, const TCHAR* Key,
                                const TCHAR* Filename = NULL) = 0;                             // 5
    virtual UBOOL GetSection(const TCHAR* Section, TCHAR* Value, INT Size,
                             const TCHAR* Filename = NULL) = 0;                                // 6
    virtual void* GetSectionPrivate(const TCHAR* Section, UBOOL Force, UBOOL Const,
                                    const TCHAR* Filename = NULL) = 0;                         // 7 TMultiMap*
    virtual void EmptySection(const TCHAR* Section, const TCHAR* Filename = NULL) = 0;         // 8
    virtual void SetBool(const TCHAR* Section, const TCHAR* Key, UBOOL Value,
                         const TCHAR* Filename = NULL) = 0;                                    // 9
    virtual void SetInt(const TCHAR* Section, const TCHAR* Key, INT Value,
                        const TCHAR* Filename = NULL) = 0;                                     // 10
    virtual void SetFloat(const TCHAR* Section, const TCHAR* Key, FLOAT Value,
                          const TCHAR* Filename = NULL) = 0;                                   // 11
    virtual void SetString(const TCHAR* Section, const TCHAR* Key, const TCHAR* Value,
                           const TCHAR* Filename = NULL) = 0;                                  // 12
};

CORE_API extern FConfigCache* GConfig;
CORE_API UBOOL Parse(const TCHAR* Stream, const TCHAR* Match, BYTE& Value);
CORE_API UBOOL ParseCommand(const TCHAR** Stream, const TCHAR* Match);  // skips Match and blanks
CORE_API UBOOL ParseToken(const TCHAR*& Str, TCHAR* Result, INT MaxLen, UBOOL UseEscape);

/*-----------------------------------------------------------------------------
    Globals exported by Core (L2 additions included).
-----------------------------------------------------------------------------*/

CORE_API extern UBOOL GIsNative;
CORE_API extern TCHAR GCountryCode[];                         // the language suffix of the data files (-e: English)
CORE_API extern UBOOL GIsRequestingExit; // appRequestExit set it: the main loop ends
CORE_API extern UBOOL GIsStickyWindow;   // option: windows snap to screen edges and each other
CORE_API extern INT GSEKScreenX;         // client area size
CORE_API extern INT GSEKScreenY;
CORE_API extern UBOOL GL2UseColorCursor;  // option: color mouse cursors
CORE_API extern INT GColorBits;
CORE_API extern UBOOL GUnicodeOS;
CORE_API extern INT GLanguageType;      // 0 Korean, 1 English, 2 Japanese, 3 Taiwanese, 4 Chinese
CORE_API extern INT GIsL2NetLogin;      // (Core.def: ?GIsL2NetLogin@@3HA) the Engine's User::GetArrowItemID reads it: 0, the arrow of the item in the left
                                        //   hand is not looked up
CORE_API extern UBOOL GIsEnterChatting; // the chat input line has the focus
CORE_API extern UBOOL GIsOldChat;       // option: the chat tabs put their prefix (#, @...) in the line
CORE_API extern INT GL2TextCapture;     // chat log to a file: 1 start asked, 2 capturing, 3 stop asked
// C3 additions (Core.dll data the C3 NWindow imports; names are the exports', meanings by use)
enum ERunningOS : INT { OS_Unknown = 0 };   // the values are not read from the code yet (only the variable is imported)
CORE_API extern ERunningOS GRunningOS;
CORE_API extern TCHAR* GMachineOS;          // the machine report the client sends (messenger / bug report)
CORE_API extern TCHAR* GMachineCPU;
CORE_API extern TCHAR* GMachineVideo;
CORE_API extern INT GIsL2Mark;
CORE_API extern INT GIsBenchmarking;        // the benchmark mode (FL2ReplayManager's BM data)
CORE_API extern INT GIsArrowMode;
CORE_API extern INT GIsTransparencyMode;
CORE_API extern INT GIsOnlyMSN;             // messenger-only start
CORE_API extern INT GL2ReplayMode;          // replay recording / playback
CORE_API extern INT GL2ScreenShotQuality;   // options
CORE_API extern INT GL2Antialiasing;
CORE_API extern INT GL2GlowEffect;           // (the C4 NWindow does not import it any more; Core.dll still exports it)
// C4 additions (Core.dll data the C4 NWindow imports; names are the exports', meanings by use / not read yet)
CORE_API extern INT GL2SystemMsgWnd;                  // options of the system message windows (NCSystemMsgWnd...)
CORE_API extern INT GL2SystemMsgWndDamage;
CORE_API extern INT GL2SystemMsgWndExpendableItem;
CORE_API extern INT GL2ShowZoneTitle;                 // option: the zone name banner
CORE_API extern INT GL2PostProcessType;               // option: the post processing (bloom / HDR) kind
CORE_API extern INT GL2FFEffectUse;                   // option: force feedback effects
CORE_API extern INT GL2DisableGCLobby;                // the GameGuard check is off in the lobby
CORE_API extern INT GIsTrueFontUse;                   // the TrueType text path (UCanvas::DrawTrueFont*) is on
// the messenger's settings (NCMSNWnd's replacement in C3: Msn*, L2FriendWnd...; one INT each)
CORE_API extern INT GMsnLoginAlarm;
CORE_API extern INT GMsnMessageAlarm;
CORE_API extern INT GMsnNewEMailAlarm;
CORE_API extern INT GMsnSoundAlarm;
CORE_API extern INT GMsnDontDisturb;
CORE_API extern INT GMsnHideAll;
CORE_API extern INT GMsnHideWarnMsg;
// game options (Option.ini [Game], NCGameOptionWnd)
CORE_API extern UBOOL GL2ShowMyName;
CORE_API extern UBOOL GL2ShowNPCName;
CORE_API extern UBOOL GL2ShowPledgeMemberName;
CORE_API extern UBOOL GL2ShowPartyMemberName;
CORE_API extern UBOOL GL2ShowOtherPCName;
CORE_API extern INT GL2PartyLooting;       // item distribution of a party this player leads
CORE_API extern UBOOL GL2UseAutoTrackingPawn;
CORE_API extern UBOOL GL2UsingJoystick;
CORE_API extern UBOOL GL2CanUseJoystick;   // a joystick was found
// video options (Option.ini [Video] [ClippingRange] [L2WaterEffect], NCVideoOptionWnd)
CORE_API extern INT GRefreshRate;
CORE_API extern UBOOL GL2KeepMinFrameRate;   // "Lower Detail": low textures, details fixed
CORE_API extern UBOOL GL2UseTrilinear;
CORE_API extern INT GL2TextureDetail;        // 0 high .. 2 low
CORE_API extern UBOOL GL2UseReducePawnLOD;
CORE_API extern UBOOL GL2UseSkipAnim;
CORE_API extern FLOAT GL2OriginPawnCR;       // clipping ranges
CORE_API extern FLOAT GL2OriginTerrainCR;
CORE_API extern FLOAT GL2OriginStaticMeshCR;
CORE_API extern FLOAT GL2OriginStaticMeshLodCR;
CORE_API extern FLOAT GL2ActorCR;
CORE_API extern FLOAT GL2FogStart;           // env.int [FOG] of the terrain range
CORE_API extern FLOAT GL2FogEnd;
CORE_API extern UBOOL GL2RenderDeco;         // ground detail
CORE_API extern UBOOL GIsL2Projector;        // pawn shadows
CORE_API extern UBOOL GIsL2WaterEffect;      // reflections
CORE_API extern INT GIsL2WaterEffectType;    // 1 all, 2 sky
CORE_API extern INT GL2RenderActorLimitOpt;  // 1 .. 6 (6: no limit)
CORE_API extern INT GL2RenderActorLimit;     // 50 .. 10000
CORE_API extern DOUBLE GSecondsPerCycle;
CORE_API extern TCHAR GCountryName[];  // FontInfo.ini section of the client's country
CORE_API extern INT GL2TestServer;      // test server client: no new characters
CORE_API extern UBOOL GIsOnlyL2Console;  // the desktop adds a page showing test.html
// new in C2 (the C2 NWindow imports them; what they switch is not mapped yet): ?GIsL2Seamless@@3HA, ?GEnableEngSelection@@3HA,
// ?GL2UseKeyCrypt@@3HA, ?GL2ThaiHtmlMode@@3HA (all int)
CORE_API extern INT GIsL2Seamless;
CORE_API extern INT GEnableEngSelection;
CORE_API extern INT GL2UseKeyCrypt;
CORE_API extern INT GL2ThaiHtmlMode;
CORE_API extern UBOOL GIsTestHtml;       // the desktop adds NCTestHtmlViewer
