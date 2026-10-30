#pragma once
// The std::wstring and std::vector instances of the stock build (VC 7.1 STL), rewritten without
// the STL: the same layouts and the same algorithms, member for member, each one hooked over
// its stock copy (src/nwindow/NStl.cpp). The allocator is the global operator new of UE2, so
// memory comes from GMalloc (appMalloc(n, "new") / appFree, inline as in the stock code).
// Names ours; std's are given in the comments. Used by the HTML page parser (NHtmlLayout.h).
//
// Not reproduced: the stock code wraps its allocations and element copies in try/catch blocks
// that free what was built and rethrow. GMalloc does not return failures (appErrorf) and the
// element copies cannot throw otherwise, so those blocks never run. The string range and length
// errors stay the stock library's (they throw std::out_of_range / std::length_error); the
// vectors' is thrown through NStdLib.h.

#include "nwindow/NWindow.h"

/*-----------------------------------------------------------------------------
    std::wstring.
-----------------------------------------------------------------------------*/

// std::allocator<wchar_t> (empty)
struct NStlCharAllocator
{
    TCHAR* Allocate(UINT Count, const void* Hint);                // 0x10027c90 allocate
};

// A std::wstring (0x1C bytes): up to 7 characters in place, longer ones in a block of Res + 1.
class NStlWString
{
public:
    static const UINT NPos = 0xFFFFFFFF;

    NStlCharAllocator Alloc;   // +0x00
    union
    {
        TCHAR Buf[8];          // +0x04 while Res < 8
        TCHAR* Ptr;
    };
    UINT Size;                 // +0x14 characters
    UINT Res;                  // +0x18 capacity

    // inlined in the stock code (the destructor's out-of-line copy is 0x100286d0, _Tidy(true))
    NStlWString() : Size(0), Res(7) { Buf[0] = 0; }
    NStlWString(const NStlWString& Right) : Size(0), Res(7)
    {
        Buf[0] = 0;
        Assign(Right, 0, NPos);
    }
    ~NStlWString()
    {
        if (Res >= 8)
            appFree(Ptr);
        Res = 7;
        Size = 0;
        Buf[0] = 0;
    }
    NStlWString& operator=(const NStlWString& Right) { return Assign(Right, 0, NPos); }
    TCHAR* CStr() { return Res >= 8 ? Ptr : Buf; }
    const TCHAR* CStr() const { return Res >= 8 ? Ptr : Buf; }

    UINT Find(const TCHAR* Str, UINT Off, UINT Count) const;      // 0x10027bb0 find, NPos = none
    void Copy(UINT NewSize, UINT OldLen);                         // 0x10028820 _Copy
    bool Grow(UINT NewSize, bool bTrim = false);                  // 0x1002a000 _Grow
    NStlWString& Replace(UINT Off, UINT N0, const NStlWString& Right, UINT ROff,
                         UINT Count);                             // 0x1002a0d0 replace
    NStlWString& Erase(UINT Off, UINT Count);                     // 0x1002a8a0 erase
    TCHAR** Erase(TCHAR** Result, TCHAR* First, TCHAR* Last);     // 0x1002a970 erase(it, it)
    NStlWString& Replace(UINT Off, UINT N0, const TCHAR* Str, UINT Count); // 0x1002a9e0 replace
    NStlWString& Assign(const NStlWString& Right, UINT ROff, UINT Count); // 0x1002abd0 assign
    NStlWString& Assign(const TCHAR* Str, UINT Count);            // 0x1002ac90 assign
    NStlWString& Assign(const TCHAR* Str);                        // 0x1002ad40 operator=
    NStlWString& Append(const NStlWString& Right, UINT ROff, UINT Count); // 0x1002cbf0 append
    NStlWString& Append(const TCHAR* Str, UINT Count);            // 0x1002ccf0 append
    NStlWString& Insert(UINT Off, const NStlWString& Right, UINT ROff, UINT Count); // 0x1002ce10
    __declspec(noinline) NStlWString& Insert(UINT Off, const TCHAR* Str, UINT Count);  // 0x100372b0 insert (the stock's Insert(Off, Str) calls it)
    NStlWString& Insert(UINT Off, const TCHAR* Str);              // 0x100375e0 (C2) the same with the length of Str
    NStlWString& Insert(UINT Off, UINT Count, TCHAR Ch);          // 0x1002d1b0 insert
    NStlWString* Substr(NStlWString* Result, UINT Off, UINT Count) const; // 0x1002d3e0 substr

    // the errors (<xstring>'s, NStdLib's NStdStringXran / NStdStringXlen)
    void Xran() const;                                            // 0x100bd4fb _Xran
    void Xlen() const;                                            // 0x100bd553 _Xlen

private:
    bool Inside(const TCHAR* Str) { return !(Str < CStr() || CStr() + Size <= Str); }
    void Eos(UINT NewSize) { CStr()[Size = NewSize] = 0; }
};
NW_ASSERT_OFFSET(NStlWString, Size, 0x14);
NW_ASSERT_SIZE(NStlWString, 0x1C);

/*-----------------------------------------------------------------------------
    std::vector. Instances: _WORD and NHtmlTag* (trivial elements), NStlWString.
-----------------------------------------------------------------------------*/

// std::allocator<T> (empty)
template <class T> struct NStlAllocator
{
    void Destroy(T* Ptr) { Ptr->~T(); }                           // 0x1002d330 (NStlWString)
};
typedef NStlAllocator<NStlWString> NStlWStringAllocator;

// The algorithms the vectors use (free function templates of the stock code: iterators by
// value, the allocator by reference, an empty iterator category last; never inlined there).
template <class T> __declspec(noinline) void NStlFill(T* First, T* Last, const T& Val)
{
    for (; First != Last; ++First)
        *First = Val;
}
template <class T>
__declspec(noinline) T* NStlUninitCopy(T* First, T* Last, T* Dest, NStlAllocator<T>& Al, INT Cat)
{
    for (; First != Last; ++Dest, ++First)
        if (Dest)
            ::new (static_cast<void*>(Dest)) T(*First);
    return Dest;
}
template <class T>
__declspec(noinline) void NStlUninitFillN(T* First, UINT Count, const T& Val, NStlAllocator<T>& Al,
                                          INT Cat)
{
    for (; Count > 0; --Count, ++First)
        if (First)
            ::new (static_cast<void*>(First)) T(Val);
}
template <class T>
__declspec(noinline) void NStlDestroyRange(T* First, T* Last, NStlAllocator<T>& Al, INT Cat)
{
    for (; First != Last; ++First)
        Al.Destroy(First);
}
template <class T> T** NStlCopyBackwardOpt(T** Result, T* First, T* Last, T* Dest, INT Cat)
{
    while (First != Last)
        *--Dest = *--Last;
    *Result = Dest;
    return Result;
}
template <class T> T** NStlCopyBackward(T** Result, T* First, T* Last, T* Dest)
{
    return NStlCopyBackwardOpt(Result, First, Last, Dest, 0);
}

// the length error of the vectors (throws std::length_error "vector<T> too long")
struct NStlVectorBase
{
    void Xlen() const;                                            // 0x1002e050 _Xlen
};

// A std::vector<T> (0x10 bytes).
template <class T> struct NStlVector : NStlVectorBase
{
    NStlAllocator<T> Alloc;    // +0x00
    T* First;                  // +0x04
    T* Last;                   // +0x08
    T* End;                    // +0x0C

    NStlVector() : First(NULL), Last(NULL), End(NULL) {}
    ~NStlVector() { Tidy(); }
    UINT Size() const { return First ? Last - First : 0; }
    UINT Capacity() const { return First ? End - First : 0; }
    static UINT MaxSize() { return 0xFFFFFFFF / sizeof(T); }

    void Tidy()
    {
        if (First)
        {
            Destroy(First, Last);
            appFree(First);
        }
        First = NULL;
        Last = NULL;
        End = NULL;
    }
    void Destroy(T* F, T* L) { NStlDestroyRange(F, L, Alloc, 0); }
    T* Ucopy(T* F, T* L, T* Ptr) { return NStlUninitCopy(F, L, Ptr, Alloc, 0); }
    T* Ufill(T* Ptr, UINT Count, const T& Val)
    {
        NStlUninitFillN(Ptr, Count, Val, Alloc, 0);
        return Ptr + Count;
    }
    // insert Count copies of Val at Where (never inlined in the stock code)
    __declspec(noinline) void InsertN(T* Where, UINT Count, const T& Val)
    {
        T Tmp = Val;               // in case Val is in the sequence
        UINT Cap = Capacity();
        if (Count == 0)
            ;
        else if (MaxSize() - Size() < Count)
            Xlen();
        else if (Cap < Size() + Count)
        {
            // not enough room: grow by half, or to the size needed
            Cap = MaxSize() - Cap / 2 < Cap ? 0 : Cap + Cap / 2;
            if (Cap < Size() + Count)
                Cap = Size() + Count;
            T* NewVec = (T*)appMalloc(Cap * sizeof(T), TEXT("new"));
            T* Ptr = Ucopy(First, Where, NewVec);
            Ptr = Ufill(Ptr, Count, Tmp);
            Ucopy(Where, Last, Ptr);
            Count += Size();
            if (First)
            {
                Destroy(First, Last);
                appFree(First);
            }
            End = NewVec + Cap;
            Last = NewVec + Count;
            First = NewVec;
        }
        else if ((UINT)(Last - Where) < Count)
        {
            // the new elements go past the end
            Ucopy(Where, Last, Where + Count);
            Ufill(Last, Count - (Last - Where), Tmp);
            Last += Count;
            NStlFill(Where, Last - Count, Tmp);
        }
        else
        {
            T* OldEnd = Last;
            Last = Ucopy(OldEnd - Count, OldEnd, Last);
            T* Dest;
            NStlCopyBackward(&Dest, Where, OldEnd - Count, OldEnd);
            NStlFill(Where, Where + Count, Tmp);
        }
    }
    T** Insert(T** Result, T* Where, const T& Val)
    {
        UINT Off = Size() == 0 ? 0 : Where - First;
        InsertN(Where, 1, Val);
        *Result = First + Off;
        return Result;
    }
    void PushBack(const T& Val)
    {
        if (Size() < Capacity())
            Last = Ufill(Last, 1, Val);
        else
        {
            T* It;
            Insert(&It, Last, Val);
        }
    }
};
typedef NStlVector<_WORD> NStlWordVector;
typedef NStlVector<NStlWString> NStlWStringVector;
NW_ASSERT_OFFSET(NStlWordVector, First, 0x04);
NW_ASSERT_SIZE(NStlWordVector, 0x10);
NW_ASSERT_SIZE(NStlWStringVector, 0x10);
