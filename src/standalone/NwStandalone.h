/*=============================================================================
    NwStandalone.h - the hook-mode macros for the standalone build (NW_HOOK_MODE=0).

    The same source builds NWindow.dll on its own (phase 5, docs/notas/Standalone.md):
      * the NW_HOOK_* registrations are empty and NW_CHECK_VTABLE only keeps the
        nwvt_<Class> name of our vtable;
      * what still reaches the stock image - NW_STOCK, NW_GLOBAL, nw::Addr,
        nw::StockCall - has no definition here, so the link fails and names each
        one (_nw_stock_<va>, nw::StockData<va>::Bytes, nw::Addr, nw::StockCall<...>):
        the list of what is left to write (tools/standalone_report.py);
      * nw::Field and nw::VirtualCall keep working (no stock code behind them).
    Included by NWindow.h after the UE2 headers, in place of re/NwHook.h.
=============================================================================*/
#pragma once

#include <windows.h>
#include <stdio.h>

#if NW_HOOK_MODE
#error "NwStandalone.h is for the standalone build only (NW_HOOK_MODE=0)"
#endif

/*-----------------------------------------------------------------------------
    What IMPLEMENT_CLASS (src/standalone/Classes.gen.cpp) needs from Core.
-----------------------------------------------------------------------------*/

enum ENativeConstructor { EC_NativeConstructor };

// Core.dll's UClass, opaque: only its native constructor (the one IMPLEMENT_CLASS calls) and
// its destructor (registered with atexit for every PrivateStaticClass). 0x520 bytes in C1/C2/C3
// (the spacing of the stock PrivateStaticClass objects); 0x530 in C4: UObject grew by 0xC, Core's
// constructor (1013 6480) writes up to +0x528, the registration of the classes in Core pushes 0x52C
// and the stock C4 objects are 0x530 apart (analysis/classes.json static_class, 192 of 341 pairs).
// A UClass declared 0x520 big makes Core's constructor zero the first 0xC bytes (vtable, Index,
// HashNext) of the object that follows it in memory: 176 of the 341 class objects came out with a
// null vtable pointer (sa-init).
class UClass : public UField
{
public:
    // ??0UClass@@QAE@W4ENativeConstructor@@KKPAV0@1VFGuid@@PBG33KP6AXPAX@ZP8UObject@@AEXXZ@Z
    CORE_API UClass(ENativeConstructor, DWORD InSize, DWORD InClassFlags, UClass* InSuperClass,
                    UClass* InWithinClass, FGuid InGuid, const TCHAR* InNameString,
                    const TCHAR* InPackageName, const TCHAR* InClassConfigName, DWORD InFlags,
                    void (*InClassConstructor)(void*), void (UObject::*InClassStaticConstructor)());
    CORE_API virtual ~UClass();

private:
    BYTE Opaque[0x530 - sizeof(UField)];
};
NW_ASSERT_SIZE(UClass, 0x530);

// IMPLEMENT_PACKAGE(NWindow) (src/standalone/NWindowPackage.cpp): the package name Core
// registers the classes under. Exported by name through src/standalone/NWindow.def.
extern "C" TCHAR GPackage[];

/*-----------------------------------------------------------------------------
    The hook macros: nothing to register.
-----------------------------------------------------------------------------*/

#define NW_CAT_(a, b) a##b
#define NW_CAT(a, b) NW_CAT_(a, b)
#define NW_UNPAREN(...) __VA_ARGS__

#define NW_HOOK_METHOD_SIG(va, Class, Method, Sig)
#define NW_HOOK_METHOD(va, Class, Method)
#define NW_HOOK_VARARGS(va, Class, Method, Sig)
#define NW_HOOK_FUNC(va, Func)
#define NW_HOOK_CTOR(va, Class)
#define NW_HOOK_CTOR_ARGS(va, Class, Params, Args)
#define NW_HOOK_DTOR(va, Class)

// our vtable stays reachable as nwvt_<Class> (NConsoleWndWindows.cpp's NW_HOOK_VARARGS_SLOT
// reads slots from it); there is no stock vtable to compare with
#define NW_CHECK_VTABLE(Class, StockVA, Slots)                                                  \
    __pragma(comment(linker, "/alternatename:_nwvt_" #Class "=??_7" #Class "@@6B@"))            \
    extern "C" const void* const nwvt_##Class[];
#define NW_CHECK_VTABLE_FOR(Class, Base, StockVA, Slots)                                        \
    __pragma(comment(linker, "/alternatename:_nwvt_" #Class "=??_7" #Class "@@6B" #Base "@@@")) \
    extern "C" const void* const nwvt_##Class[];

namespace nw {

// the registration object NW_HOOK_VARARGS_SLOT builds: nothing to register
struct HookReg
{
    HookReg(DWORD, void*, const char*, const char*) {}
};

}  // namespace nw

/*-----------------------------------------------------------------------------
    What still reaches the stock image: declared, never defined.
-----------------------------------------------------------------------------*/

#define NW_NAKED __declspec(naked)

// a body still in the stock image: the link reports "_nw_stock_<va>"
#define NW_STOCK(va) { __pragma(comment(linker, "/include:_nw_stock_" #va)) __asm int 3 }
#define NW_STOCK_AT(va) NW_STOCK(va)

// A virtual whose stock slot holds a function we have under another name (a placeholder NewVirtual<N>, an override the stock linker
// folded into its base's body): the placeholder's symbol becomes an alias of that function at link time (/alternatename applies only to
// a symbol nothing else defines), so the vtable slot holds the very address the stock slot holds, with no thunk between. Both names
// are decorated (take them from the map: build\standalone\Release\NWindow.map). Standalone only: the hook build never expands it.
#define NW_SLOT_IS(Placeholder, Target) __pragma(comment(linker, "/alternatename:" Placeholder "=" Target))

namespace nw {

// a stock global still reached by address: the link reports nw::StockData<va>::Bytes (va in
// decimal)
template <DWORD VA> struct StockData
{
    static BYTE Bytes[];
};

// a stock table still read by address, and thiscall into stock code: one unresolved symbol
// each (nw::Addr, nw::StockCall<...>); tools/standalone_report.py lists the call sites
void* Addr(DWORD va);
template <class R, class... A> R StockCall(DWORD va, const void* self, A... a);
// the same for an address that is already this client's (the C3/C4 sources use this one): one unresolved symbol each
template <class R, class... A> R StockCallAt(DWORD va, const void* self, A... a);

// hook-mode debugging aids: nothing in the standalone ([Debug] LogCalls does not exist there)
inline void TraceCall(const char*, const void*, int = 0, int = 0, int = 0) {}
// the stock FILE* belongs to the stock image's C runtime; in the standalone the stream is ours
inline size_t StockFread(void* Buffer, size_t Size, size_t Count, void* File)
{
    return fread(Buffer, Size, Count, (FILE*)File);
}

// Field of an object whose class has no declaration here yet.
template <class T> inline T& Field(const void* obj, DWORD offset)
{
    return *reinterpret_cast<T*>(reinterpret_cast<BYTE*>(const_cast<void*>(obj)) + offset);
}

// thiscall through vtable slot `slot` of an object whose class does not declare its virtuals.
template <class R, class... A> inline R VirtualCall(const void* self, int slot, A... a)
{
    typedef R(__fastcall * Fn)(const void*, void*, A...);
    return (*reinterpret_cast<Fn* const*>(self))[slot](self, 0, a...);
}

}  // namespace nw

#define NW_GLOBAL(Type, va) (*reinterpret_cast<Type*>(::nw::StockData<(va)>::Bytes))
