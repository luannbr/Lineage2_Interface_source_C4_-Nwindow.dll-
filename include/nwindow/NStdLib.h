#pragma once
// The pieces of the C++ standard library the stock module was built with (MSVC 7.1, the
// Dinkumware library, its allocations going through UE2's global new/delete, GMalloc with the
// tag "new"), reproduced with plain structs: no STL in our code. Names follow <xstring>,
// <stdexcept> and <deque> (Myres, Mysize, Map...). In hook mode the std::exception base is the
// stock C runtime's (library code, called in place); the standalone build has its own.

#include "nwindow/NWindow.h"

/*-----------------------------------------------------------------------------
    std::string (basic_string<char>): 15 characters in the object, more on the heap.
-----------------------------------------------------------------------------*/

struct NStdString
{
    BYTE Alval;                      // +0x00 the (empty) allocator
    union
    {
        ANSICHAR Buf[16];            // +0x04 while Myres < 16
        ANSICHAR* Ptr;               // +0x04 GMalloc, Myres + 1 bytes
    } Bx;
    DWORD Mysize;                    // +0x14 characters
    DWORD Myres;                     // +0x18 room, without the terminator

    enum { BUF_SIZE = 16, ALLOC_MASK = 15 };
    enum { NPOS = 0xFFFFFFFF, MAX_SIZE = 0xFFFFFFFE };

    // _Tidy(): empty, in the object (inline in the stock code)
    NStdString() { Myres = BUF_SIZE - 1; Eos(0); }
    NStdString(const ANSICHAR* Ptr);                                         // 0x1000d9d0
    ~NStdString();                                                           // 0x1000d410

    ANSICHAR* Myptr() { return Myres >= BUF_SIZE ? Bx.Ptr : Bx.Buf; }
    const ANSICHAR* Myptr() const { return Myres >= BUF_SIZE ? Bx.Ptr : Bx.Buf; }
    // _Eos: the size and the terminator
    void Eos(DWORD NewSize) { Myptr()[Mysize = NewSize] = 0; }
    // _Tidy(Built): the heap buffer freed when Built, then empty
    void Tidy(bool bBuilt = false)
    {
        if (bBuilt && Myres >= BUF_SIZE)
            appFree(Bx.Ptr);
        Myres = BUF_SIZE - 1;
        Eos(0);
    }
    // _Inside: Ptr points into the string
    bool Inside(const ANSICHAR* Ptr) const
    {
        if (Ptr < Myptr() || Myptr() + Mysize <= Ptr)
            return false;
        return true;
    }

    void Copy(DWORD NewSize, DWORD OldLen);                                  // 0x1000d2d0
    bool Grow(DWORD NewSize, bool bTrim = false);                            // 0x1000d440
    NStdString& Erase(DWORD Off = 0, DWORD Count = NPOS);                    // 0x1000d5e0
    NStdString& Assign(const NStdString& Right, DWORD Roff, DWORD Count);    // 0x1000d810
    NStdString& Assign(const ANSICHAR* Ptr, DWORD Num);                      // 0x1000d8c0
};
NW_ASSERT_SIZE(NStdString, 0x1C);

/*-----------------------------------------------------------------------------
    std::exception, std::logic_error, std::length_error, std::out_of_range. Every one has
    two virtuals: vslot 0 the scalar deleting destructor, vslot 1 what().

    Hook mode: the objects keep the stock vtables (their what() and destructors are the
    hooked ones below) and are thrown with the stock throw information, so that the stock
    code can catch and destroy them; std::exception's constructors and destructor, and
    <xstring>'s _Xlen / _Xran, are the stock C runtime's (library code, called in place).
    Standalone: the same classes with our own vtables, std::exception written after the
    VC 7.0 runtime's (exception.cpp), thrown with throw (our throw information).
-----------------------------------------------------------------------------*/

#if NW_HOOK_MODE

// the stock vtables and the throw information of std::length_error (names ours)
#define GStdLogicErrorVtbl ((const void*)::nw::Addr(0x101DE200))
#define GStdLengthErrorVtbl ((const void*)::nw::Addr(0x101DE20C))
#define GStdLengthErrorThrowInfo ((void*)::nw::Addr(0x101E529C))

// the C runtime's std::exception: its constructors and destructor are library code
struct NStdException
{
    const void* Vtbl;                // +0x00
    const char* WhatStr;             // +0x04 _m_what
    INT bDoFree;                     // +0x08 _m_doFree: WhatStr was copied

    NStdException() { ::nw::StockCall<void>(0x10171893, this); }
    NStdException(const NStdException& Other) { ::nw::StockCall<void>(0x101718A4, this, &Other); }
    ~NStdException() { ::nw::StockCall<void>(0x101718EE, this); }
};

#else

// the C runtime's std::exception (0x100be11d .. 0x100be1b6 in the stock image: ::exception,
// the VC 7.0 library puts it outside std)
struct NStdException
{
    // +0x00 the vtable
    const char* WhatStr;             // +0x04 _m_what
    INT bDoFree;                     // +0x08 _m_doFree: WhatStr was copied

    NStdException() : WhatStr(NULL), bDoFree(0) {}                           // 0x100be11d
    NStdException(const NStdException& Other);                              // 0x100be12e
    virtual ~NStdException();                                                // 0x100be178 (vslot 0)
    virtual const char* What() const;                                        // 0x100be18e (vslot 1)
};
NW_ASSERT_OFFSET(NStdException, WhatStr, 0x4);

#endif
NW_ASSERT_SIZE(NStdException, 0xC);

// (standalone: the destructor and What override std::exception's virtuals)
struct NStdLogicError : NStdException
{
    NStdString Str;                  // +0x0C _Str: the message

    NStdLogicError(const NStdString& Message);                               // 0x1000d970
    NStdLogicError(const NStdLogicError& Other);                             // 0x1000dad0
    ~NStdLogicError();                                                       // 0x1000d520
    const char* What() const;                                                // 0x1000d5a0 (vslot 1)
};
NW_ASSERT_SIZE(NStdLogicError, 0x28);

struct NStdLengthError : NStdLogicError
{
    // inline where the stock code throws one
    NStdLengthError(const NStdString& Message) : NStdLogicError(Message)
    {
#if NW_HOOK_MODE
        Vtbl = GStdLengthErrorVtbl;
#endif
    }
    NStdLengthError(const NStdLengthError& Other);                           // 0x1000db40
    ~NStdLengthError();                                                      // 0x1000da40
};

#if !NW_HOOK_MODE
// std::out_of_range (hook mode: only the stock _Xran builds one); constructors and destructor
// inline, as in <stdexcept> (stock copies 0x100bd53b and 0x100bd4f0)
struct NStdOutOfRange : NStdLogicError
{
    NStdOutOfRange(const NStdString& Message) : NStdLogicError(Message) {}
};
#endif

// throws a length_error with Message the way the stock code does (_THROW: the object in the
// thrower's frame, the stock throw information in hook mode; name ours)
__declspec(noreturn) void NStdThrowLengthError(const ANSICHAR* Message);

// <xstring>'s _String_base::_Xlen and _Xran, shared by std::string and std::wstring: throw
// length_error("string too long") and out_of_range("invalid string position") (stock
// 0x100bd553 and 0x100bd4fb, library code; names ours)
__declspec(noreturn) void NStdStringXlen();
__declspec(noreturn) void NStdStringXran();

/*-----------------------------------------------------------------------------
    std::deque<POINT>: blocks of 2 points found through a map of block pointers that grows
    by half (at least 8).
-----------------------------------------------------------------------------*/

struct NPointDeque
{
    BYTE Alval;                      // +0x00 the (empty) allocator
    POINT** Map;                     // +0x04 the blocks, NULL until used
    DWORD Mapsize;                   // +0x08 entries of Map
    DWORD Myoff;                     // +0x0C place of the first point
    DWORD Mysize;                    // +0x10 points

    enum { DEQUESIZ = 2, DEQUEMAPSIZ = 8, MAX_BLOCKS = 0xFFFFFFF };

    NPointDeque() : Map(NULL), Mapsize(0), Myoff(0), Mysize(0) {}
    ~NPointDeque();                                                          // 0x1000d510

    void Tidy();                                                             // 0x1000d260
    void TidyThunk();                                                        // 0x10032BB0 (name ours: the stock is a `jmp Tidy`)
    void PushBack(const POINT& Val);                                         // 0x1000dca0
    void GrowMap(DWORD Count);                                               // 0x1000db60
    void Xlen() const;                                                       // 0x1000da50

    // back() and pop_back() (inline in the stock code)
    POINT& Back()
    {
        INT Off = Myoff + Mysize - 1;
        INT Block = Off / DEQUESIZ;
        INT Index = Off - Block * DEQUESIZ;
        if (Mapsize <= (DWORD)Block)
            Block -= Mapsize;
        return Map[Block][Index];
    }
    void PopBack()
    {
        if (Mysize != 0 && --Mysize == 0)
            Myoff = 0;
    }
};
NW_ASSERT_SIZE(NPointDeque, 0x14);
