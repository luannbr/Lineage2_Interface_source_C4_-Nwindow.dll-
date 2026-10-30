/*=============================================================================
    NStdLib.cpp - the stock module's standard library pieces: std::string, logic_error /
    length_error, the deque of NCGDevice's origins, UE2's operator delete. Stock:
    0x1000d260 .. 0x1000d2c3, 0x1000d2d0 .. 0x1000d503, 0x1000d510 .. 0x1000d592,
    0x1000d5a0 .. 0x1000d5ad, 0x1000d5e0 .. 0x1000d692, 0x1000d810 .. 0x1000d95c,
    0x1000d970 .. 0x1000d9ff, 0x1000da40 .. 0x1000dd25, 0x1000e520 .. 0x1000e531 (the scalar
    deleting destructors are the compiler's).
=============================================================================*/
#include "nwindow/NStdLib.h"
#include "nwindow/NCPawnViewrWnd.h"

#include <new>
#include <stdlib.h>
#include <string.h>

#if NW_HOOK_MODE
// the stock C runtime's _CxxThrowException, and <xstring>'s _String_base::_Xlen / _Xran
// (library code: length_error "string too long", out_of_range "invalid string position")
// (not declared noreturn here: the compiler would end its callers with an int3, which tools/port/check_breakers.py
// takes for an NW_UNPORTED() body; the thrower ends in a loop it never reaches)
NW_NAKED static void __stdcall StockThrow(void* Object, void* ThrowInfo)
    NW_STOCK(0x10171169)
NW_NAKED void NStdStringXlen() NW_STOCK(0x10170BC9)
NW_NAKED void NStdStringXran() NW_STOCK(0x10170B71)
#endif

/*-----------------------------------------------------------------------------
    NStdString.
-----------------------------------------------------------------------------*/

// 0x10014550
NStdString::NStdString(const ANSICHAR* Ptr)
{
    Myres = BUF_SIZE - 1;
    Eos(0);
    Assign(Ptr, (DWORD)strlen(Ptr));
}
NW_HOOK_CTOR_ARGS(0x10014550, NStdString, (const ANSICHAR* A), (A))

// 0x10013f90
NStdString::~NStdString()
{
    Tidy(true);
}
NW_HOOK_DTOR(0x10013f90, NStdString)

// 0x10013670 (ret 8; the db calls it `new`, and its second half 0x100113af a function of its own) - basic_string::_Copy of the C3
// build: a new buffer for NewSize characters (rounded up to 16n - 1, but NewSize itself when that wraps), OldLen of the old text
// copied into it, the old heap buffer freed, then the object takes the new one. Unlike the C2 build there is no second try without
// the rounding when the allocation fails
void NStdString::Copy(DWORD NewSize, DWORD OldLen)
{
    DWORD NewRes = NewSize | ALLOC_MASK;
    if (NewRes > MAX_SIZE)
        NewRes = NewSize;
    ANSICHAR* NewPtr = (ANSICHAR*)appMalloc(NewRes + 1, TEXT("new"));
    if (OldLen > 0)
        memcpy(NewPtr, Myptr(), OldLen);
    if (Myres >= BUF_SIZE)
        appFree(Bx.Ptr);
    Bx.Buf[0] = 0;
    Bx.Ptr = NewPtr;
    Myres = NewRes;
    Eos(OldLen);
}
NW_HOOK_METHOD(0x10013670, NStdString, Copy)

// 0x10013fc0 - room for NewSize characters: a new buffer (the old text kept unless bTrim),
// or with bTrim and a short size back into the object; true when there is something to copy
bool NStdString::Grow(DWORD NewSize, bool bTrim)
{
    if (NewSize > MAX_SIZE)
        NStdStringXlen();
    if (Myres < NewSize)
        Copy(NewSize, bTrim ? 0 : Mysize);
    else if (bTrim && NewSize < BUF_SIZE)
        Tidy(true);
    else if (NewSize == 0)
        Eos(0);
    return NewSize > 0;
}
NW_HOOK_METHOD(0x10013fc0, NStdString, Grow)

// 0x10014150
NStdString& NStdString::Erase(DWORD Off, DWORD Count)
{
    if (Mysize < Off)
        NStdStringXran();
    if (Mysize - Off < Count)
        Count = Mysize - Off;
    if (Count > 0)
    {
        memmove(Myptr() + Off, Myptr() + Off + Count, Mysize - Off - Count);
        DWORD NewSize = Mysize - Count;
        if (Grow(NewSize))
            Eos(NewSize);
    }
    return *this;
}
NW_HOOK_METHOD(0x10014150, NStdString, Erase)

// 0x10014350 - Count characters of Right from Roff (the string's own: cut in place)
NStdString& NStdString::Assign(const NStdString& Right, DWORD Roff, DWORD Count)
{
    if (Right.Mysize < Roff)
        NStdStringXran();
    DWORD Num = Right.Mysize - Roff;
    if (Count < Num)
        Num = Count;
    if (this == &Right)
    {
        Erase(Roff + Num);
        Erase(0, Roff);
    }
    else if (Grow(Num, true))
    {
        appMemcpy(Myptr(), Right.Myptr() + Roff, Num);
        Eos(Num);
    }
    return *this;
}
NW_HOOK_METHOD_SIG(0x10014350, NStdString, Assign,
                   NStdString& (NStdString::*)(const NStdString&, DWORD, DWORD))

// 0x10014400
NStdString& NStdString::Assign(const ANSICHAR* Ptr, DWORD Num)
{
    if (Inside(Ptr))
        return Assign(*this, (DWORD)(Ptr - Myptr()), Num);
    if (Grow(Num, true))
    {
        appMemcpy(Myptr(), Ptr, Num);
        Eos(Num);
    }
    return *this;
}
NW_HOOK_METHOD_SIG(0x10014400, NStdString, Assign, NStdString& (NStdString::*)(const ANSICHAR*, DWORD))

/*-----------------------------------------------------------------------------
    std::exception (standalone only: in hook mode it is the stock C runtime's).
-----------------------------------------------------------------------------*/

#if !NW_HOOK_MODE
// 0x100be12e - a copied message is copied again (malloc; NULL when that fails)
NStdException::NStdException(const NStdException& Other)
{
    bDoFree = Other.bDoFree;
    if (bDoFree)
    {
        WhatStr = (const char*)malloc(strlen(Other.WhatStr) + 1);
        if (WhatStr)
            strcpy((char*)WhatStr, Other.WhatStr);
    }
    else
        WhatStr = Other.WhatStr;
}

// 0x100be178
NStdException::~NStdException()
{
    if (bDoFree)
        free((void*)WhatStr);
}

// 0x100be18e
const char* NStdException::What() const
{
    return WhatStr ? WhatStr : "Unknown exception";
}
#endif

/*-----------------------------------------------------------------------------
    logic_error, length_error, out_of_range. In hook mode the constructors and
    destructors put the stock vtables in (the compiler puts ours in the standalone
    build).
-----------------------------------------------------------------------------*/

// 0x100144b0 (ret 4) - the exception base, then the message copied in
NStdLogicError::NStdLogicError(const NStdString& Message)
{
#if NW_HOOK_MODE
    Vtbl = GStdLogicErrorVtbl;
#endif
    Str.Assign(Message, 0, NStdString::NPOS);
}
NW_HOOK_CTOR_ARGS(0x100144b0, NStdLogicError, (const NStdString& A), (A))

// 0x10014610
NStdLogicError::NStdLogicError(const NStdLogicError& Other)
    : NStdException(Other)
{
#if NW_HOOK_MODE
    Vtbl = GStdLogicErrorVtbl;
#endif
    Str.Assign(Other.Str, 0, NStdString::NPOS);
}
NW_HOOK_CTOR_ARGS(0x10014610, NStdLogicError, (const NStdLogicError& A), (A))

// 0x10014090 - the message string gives its heap buffer back, then the exception base goes
NStdLogicError::~NStdLogicError()
{
#if NW_HOOK_MODE
    Vtbl = GStdLogicErrorVtbl;
#endif
}
NW_HOOK_DTOR(0x10014090, NStdLogicError)

// 0x10014110 (vslot 1) - what()
const char* NStdLogicError::What() const
{
    return Str.Myptr();
}
NW_HOOK_METHOD(0x10014110, NStdLogicError, What)

// 0x10014680
NStdLengthError::NStdLengthError(const NStdLengthError& Other)
    : NStdLogicError(Other)
{
#if NW_HOOK_MODE
    Vtbl = GStdLengthErrorVtbl;
#endif
}
NW_HOOK_CTOR_ARGS(0x10014680, NStdLengthError, (const NStdLengthError& A), (A))

// 0x10014540 - its own vtable, then the logic error's destructor (a tail jump in the stock code)
NStdLengthError::~NStdLengthError()
{
#if NW_HOOK_MODE
    Vtbl = GStdLengthErrorVtbl;
#endif
}
NW_HOOK_DTOR(0x10014540, NStdLengthError)

// _THROW(length_error, Message): the exception object stays in this frame for the handler (not
// destroyed by the unwinding: the runtime destroys it through the throw information); the
// message string is a temporary the unwinding destroys
void NStdThrowLengthError(const ANSICHAR* Message)
{
    NStdString Text(Message);
#if NW_HOOK_MODE
    __declspec(align(4)) BYTE Storage[sizeof(NStdLengthError)];
    NStdLengthError* Error = new (Storage) NStdLengthError(Text);
    StockThrow(Error, GStdLengthErrorThrowInfo);
    for (;;)
    {
    }
#else
    throw NStdLengthError(Text);
#endif
}

#if !NW_HOOK_MODE
// 0x100bd553 - _String_base::_Xlen
void NStdStringXlen()
{
    NStdThrowLengthError("string too long");
}

// 0x100bd4fb - _String_base::_Xran
void NStdStringXran()
{
    NStdString Text("invalid string position");
    throw NStdOutOfRange(Text);
}
#endif

/*-----------------------------------------------------------------------------
    NPointDeque.
-----------------------------------------------------------------------------*/

// the map's block pointers moved and cleared (_Uninitialized_copy / _Uninitialized_fill_n,
// stock template code at 0x1002c990 / 0x1002ca50)
static FORCEINLINE POINT** CopyBlocks(POINT** First, POINT** Last, POINT** Dest)
{
    for (; First != Last; ++First, ++Dest)
        *Dest = *First;
    return Dest;
}
static FORCEINLINE void ClearBlocks(POINT** Dest, DWORD Count)
{
    for (; Count > 0; --Count, ++Dest)
        *Dest = NULL;
}

// C1 0x1000d510: the C2 build inlines this destructor into its owners, so no C2 function is hooked; the
// out-of-line copy is what our own owners (NCGDevice, NHtmlAnalyzer) call
NPointDeque::~NPointDeque()
{
    Tidy();
}

// 0x10037eb0 (hooked in NToolTip.cpp on a stand-in, NPointDequeTidyExt::TidyBlocks) - std::deque<POINT>::_Tidy: the points go (nothing to
// destroy), the blocks and the map are freed, the map empty
#if NW_HOOK_MODE
NW_NAKED void NPointDeque::Tidy() NW_STOCK(0x10037eb0)
#else
void NPointDeque::Tidy()
{
    while (Mysize != 0)
        PopBack();
    for (DWORD i = Mapsize; i > 0; i--)
        appFree(Map[i - 1]);
    appFree(Map);
    Mapsize = 0;
    Map = NULL;
}
#endif

// 0x10037f20 - a 5-byte `jmp Tidy` the linker left in the NHtml objects' range; only the unwind funclets of the stock NCGDevice
// (0x10011f60) and NHtmlAnalyzer (0x10032c90) functions jump to it
void NPointDeque::TidyThunk()
{
    Tidy();
}
NW_HOOK_METHOD(0x10037f20, NPointDeque, TidyThunk)

// 0x100146a0 - Count more block pointers (half the map again, at least 8, when that fits),
// the first block's pointer keeping its place in the new map
void NPointDeque::GrowMap(DWORD Count)
{
    if (MAX_BLOCKS - Mapsize < Count)
        Xlen();
    DWORD Inc = Mapsize / 2;
    if (Inc < DEQUEMAPSIZ)
        Inc = DEQUEMAPSIZ;
    if (Count < Inc && Mapsize <= MAX_BLOCKS - Inc)
        Count = Inc;
    DWORD Myboff = Myoff / DEQUESIZ;
    POINT** NewMap = (POINT**)appMalloc((Mapsize + Count) * sizeof(POINT*), TEXT("new"));
    POINT** Myptr = CopyBlocks(Map + Myboff, Map + Mapsize, NewMap + Myboff);
    if (Myboff <= Count)
    {
        Myptr = CopyBlocks(Map, Map + Myboff, Myptr);
        ClearBlocks(Myptr, Count - Myboff);
        ClearBlocks(NewMap, Myboff);
    }
    else
    {
        CopyBlocks(Map, Map + Count, Myptr);
        Myptr = CopyBlocks(Map + Count, Map + Myboff, NewMap);
        ClearBlocks(Myptr, Count);
    }
    appFree(Map);
    Map = NewMap;
    Mapsize += Count;
}
NW_HOOK_METHOD(0x100146a0, NPointDeque, GrowMap)

// 0x100147e0
void NPointDeque::PushBack(const POINT& Val)
{
    if ((Myoff + Mysize) % DEQUESIZ == 0 && Mapsize <= (Mysize + DEQUESIZ) / DEQUESIZ)
        GrowMap(1);
    DWORD Newoff = Myoff + Mysize;
    DWORD Block = Newoff / DEQUESIZ;
    if (Mapsize <= Block)
        Block -= Mapsize;
    if (Map[Block] == NULL)
        Map[Block] = (POINT*)appMalloc(DEQUESIZ * sizeof(POINT), TEXT("new"));
    POINT* Slot = Map[Block] + Newoff % DEQUESIZ;
    if (Slot)
        *Slot = Val;
    ++Mysize;
}
NW_HOOK_METHOD(0x100147e0, NPointDeque, PushBack)

// 0x10014590 - _Xlen: length_error("deque<T> too long")
void NPointDeque::Xlen() const
{
    NStdThrowLengthError("deque<T> too long");
}
NW_HOOK_METHOD(0x10014590, NPointDeque, Xlen)

/*-----------------------------------------------------------------------------
    std::vector<NSimPawn> (the pawn viewer's simulator).
-----------------------------------------------------------------------------*/

// 0x100d7690 - the pawns copied into raw storage (the allocator and the iterator tag are not
// used)
NSimPawn* NSimPawnVector::UninitializedCopy(NSimPawn* First, NSimPawn* Last, NSimPawn* Dest, void* Al,
                                            INT Tag)
{
    for (; First != Last; ++First, ++Dest)
    {
        if (Dest)
            *Dest = *First;
    }
    return Dest;
}
NW_HOOK_FUNC(0x100d7690, NSimPawnVector::UninitializedCopy)

// 0x100d94b0 - _Xlen: length_error("vector<T> too long")
void NSimPawnVector::Xlen() const
{
    NStdThrowLengthError("vector<T> too long");
}
NW_HOOK_METHOD(0x100d94b0, NSimPawnVector, Xlen)

// 0x100d9530 - storage for Count pawns: the old ones copied over (the new storage freed if the
// copy fails), the old storage freed
void NSimPawnVector::Reserve(DWORD Count)
{
    if (Count > 0x1FFFFFFF)
        Xlen();
    else if ((First ? (DWORD)(End - First) : 0) < Count)
    {
        NSimPawn* Ptr = (NSimPawn*)appMalloc(Count * sizeof(NSimPawn), TEXT("new"));
        try
        {
            UninitializedCopy(First, Last, Ptr, this, 0);
        }
        catch (...)
        {
            appFree(Ptr);
            throw;
        }
        DWORD Size = First ? (DWORD)(Last - First) : 0;
        if (First)
            appFree(First);
        End = Ptr + Count;
        Last = Ptr + Size;
        First = Ptr;
    }
}
NW_HOOK_METHOD(0x100d9530, NSimPawnVector, Reserve)

/*-----------------------------------------------------------------------------
    UE2's operator delete (UnVcWin32.h), out of line for the C runtime's code.
-----------------------------------------------------------------------------*/

// 0x10016880 - no NULL test
static void StockOperatorDelete(void* Ptr)
{
    appFree(Ptr);
}
NW_HOOK_FUNC(0x10016880, StockOperatorDelete)
