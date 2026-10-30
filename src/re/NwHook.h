/*=============================================================================
    NwHook.h - hook mode runtime.

    NWindowDecomp.dll is imported by the stock NWindow.dll, so its DllMain runs
    after the stock image is mapped but before any stock code executes. Every
    function reimplemented in src/nwindow registers itself with one of the
    NW_HOOK_* macros below; at attach time the stock entry point gets a
    `jmp ours`, so stock callers, vtables and Engine all reach the new code.

    Addresses in the source are stock virtual addresses (ImageBase 0x10000000).
    At run time they are rebased with g_nwBase (the stock DLL may be relocated).
=============================================================================*/
#pragma once

#include <windows.h>

extern "C" DWORD g_nwBase;  // where the stock NWindow.dll is mapped (0 before startup)

namespace nw {

constexpr DWORD kStockBase = 0x10000000;

// Stock virtual address -> pointer into the loaded stock image.
inline void* Addr(DWORD va) { return reinterpret_cast<void*>(g_nwBase + (va - kStockBase)); }

enum HookState
{
    HOOK_PENDING = 0,
    HOOK_INSTALLED = 1,
    HOOK_DISABLED = 2,       // turned off in NWindowDecomp.ini
    HOOK_ERR_UNKNOWN = -1,   // va is not a function entry of the stock image
    HOOK_ERR_BYTES = -2,     // stock bytes differ (other build, or patched by someone else)
    HOOK_ERR_SPACE = -3,     // function + padding shorter than the 5-byte jump
    HOOK_ERR_PROTECT = -4,
    HOOK_ERR_DUPLICATE = -5, // two reimplementations for the same va
};

struct HookEntry
{
    DWORD va;
    void* fn;
    const char* name;
    const char* file;
    HookEntry* next;
    int state;
    BYTE* trampoline;  // stock code relocated so it can still be called (NULL if impossible)
};

struct HookReg
{
    HookEntry e;
    HookReg(DWORD va, void* fn, const char* name, const char* file);
};

HookEntry* FirstHook();

// The last 256 hooked functions entered (HookEntry pointers, newest at g_traceSeq & 255): each hook is entered through
// a small stub that notes it here before jumping to ours ([Debug] Trace in the ini). The crash loggers print them.
extern "C" DWORD g_nwTraceRing[256];
extern "C" volatile DWORD g_nwTraceSeq;
void DumpTrace(const char* tag, int count);

// A vtable compiled here, registered to be compared with the stock one (NW_CHECK_VTABLE).
struct VtableCheck
{
    const char* name;
    const void* const* ours;
    DWORD stockVA;
    int slots;
    VtableCheck* next;
};
struct VtableReg
{
    VtableCheck v;
    VtableReg(const char* name, const void* const* ours, DWORD stockVA, int slots);
};
VtableCheck* FirstVtable();
int InstallHooks();                  // returns the number of hooks installed
int InstallCallLogs(HMODULE self);   // [Debug] LogCalls: logging detours on stock functions that are not hooks
// [Debug] LogCalls for the calls between OUR functions (they never pass through the stock entry that carries the
// logging stub): a ported function that matters for a bug hunt starts with `::nw::TraceCall("Class::Method", this, a, b, c)`.
// Cheap when LogCalls is empty (one byte test).
void TraceCall(const char* name, const void* self, int a0 = 0, int a1 = 0, int a2 = 0);
const HookEntry* FindHook(DWORD va);

// Callable stock implementation of `va`: the trampoline when hooked, else the stock entry.
void* Stock(DWORD va);

namespace detail {

// Code address of a member function pointer. With multiple inheritance MSVC makes it
// {code, this adjustment}: the forwarders are members of the most derived class, reached
// through its primary base, so the adjustment must be 0 and the code comes first.
template <class F> inline void* CodeOf(F f)
{
    static_assert(sizeof(F) == sizeof(void*) || sizeof(F) == 2 * sizeof(void*),
                  "member pointer with virtual inheritance");
    union { F f; void* p[2]; } u;
    u.f = f;
    return u.p[0];
}

template <class T> struct Member;
template <class R, class C, class... A> struct Member<R (C::*)(A...)>
{
    template <class W> static void* Fwd() { return CodeOf(&W::template Fwd<R, A...>); }
};
template <class R, class C, class... A> struct Member<R (C::*)(A...) const>
{
    template <class W> static void* Fwd() { return CodeOf(&W::template FwdC<R, A...>); }
};

}  // namespace detail
}  // namespace nw

#define NW_CAT_(a, b) a##b
#define NW_CAT(a, b) NW_CAT_(a, b)

/*-----------------------------------------------------------------------------
    NW_HOOK_METHOD(va, Class, Method)
    Installs Class::Method over the stock function at va. Works for virtual and
    non-virtual methods alike: the registered code is a non-virtual forwarder
    making a qualified call, so a virtual method never goes back through the
    stock vtable (whose slot still points at va and would loop).
    NW_HOOK_METHOD_SIG: same, for overloaded methods (Sig = R (Class::*)(A...)).
-----------------------------------------------------------------------------*/
#define NW_HOOK_METHOD_SIG(va, Class, Method, Sig)                                                \
    namespace {                                                                                   \
    struct NW_CAT(NwFwd_, __LINE__) : Class                                                       \
    {                                                                                             \
        template <class R, class... A> R Fwd(A... a) { return this->Class::Method(a...); }        \
        template <class R, class... A> R FwdC(A... a) const { return this->Class::Method(a...); } \
    };                                                                                            \
    ::nw::HookReg NW_CAT(nwHook_, __LINE__)(                                                      \
        va, ::nw::detail::Member<Sig>::template Fwd<NW_CAT(NwFwd_, __LINE__)>(),                   \
        #Class "::" #Method, __FILE__);                                                           \
    }
#define NW_HOOK_METHOD(va, Class, Method) NW_HOOK_METHOD_SIG(va, Class, Method, decltype(&Class::Method))

// Variadic member (cdecl, `this` on the stack): no forwarder can pass "...", the method itself
// is installed - so it must not be virtual. Sig = R (VARARGS Class::*)(A..., ...).
#define NW_HOOK_VARARGS(va, Class, Method, Sig)                                                   \
    namespace {                                                                                   \
    ::nw::HookReg NW_CAT(nwHook_, __LINE__)(va, ::nw::detail::CodeOf(static_cast<Sig>(&Class::Method)), \
                                            #Class "::" #Method, __FILE__);                       \
    }

// Free or static function (same calling convention as the stock one).
#define NW_HOOK_FUNC(va, Func) \
    namespace { ::nw::HookReg NW_CAT(nwHook_, __LINE__)(va, (void*)&Func, #Func, __FILE__); }

// Default constructor / destructor (no C++ way to take their address: a __fastcall
// DEFAULT-initialises (`new (p) Class`, not `Class()`): value-initialisation of a class without a
// user-provided constructor would zero the whole object first (wave 3: NCPrivateBuyFrameCtrl).
// wrapper has the thiscall register layout - ecx = this - and runs ours in place).
// Never hook a `scalar deleting destructor`: the stock one calls the hooked destructor.
#define NW_HOOK_CTOR(va, Class)                                                                  \
    namespace {                                                                                  \
    struct NW_CAT(NwCtor_, __LINE__)                                                             \
    {                                                                                            \
        static Class* __fastcall Run(Class* self) { return ::new (static_cast<void*>(self)) Class; } \
    };                                                                                           \
    ::nw::HookReg NW_CAT(nwHook_, __LINE__)(va, (void*)&NW_CAT(NwCtor_, __LINE__)::Run,          \
                                            #Class "::" #Class, __FILE__);                       \
    }
// Constructor with arguments: Params is the parenthesized parameter list, Args the call.
//   NW_HOOK_CTOR_ARGS(0x10001a20, NCButton, (const TCHAR* A, const TCHAR* B, UBOOL C), (A, B, C))
// __fastcall pops the stack arguments like the thiscall constructor (edx is a dummy).
#define NW_HOOK_CTOR_ARGS(va, Class, Params, Args)                                               \
    namespace {                                                                                  \
    struct NW_CAT(NwCtor_, __LINE__)                                                             \
    {                                                                                            \
        static Class* __fastcall Run(Class* self, void*, NW_UNPAREN Params)                      \
        {                                                                                        \
            return ::new (static_cast<void*>(self)) Class Args;                                  \
        }                                                                                        \
    };                                                                                           \
    ::nw::HookReg NW_CAT(nwHook_, __LINE__)(va, (void*)&NW_CAT(NwCtor_, __LINE__)::Run,          \
                                            #Class "::" #Class #Params, __FILE__);               \
    }
#define NW_UNPAREN(...) __VA_ARGS__
#define NW_HOOK_DTOR(va, Class)                                                                  \
    namespace {                                                                                  \
    struct NW_CAT(NwDtor_, __LINE__)                                                             \
    {                                                                                            \
        static void __fastcall Run(Class* self) { self->Class::~Class(); }                       \
    };                                                                                           \
    ::nw::HookReg NW_CAT(nwHook_, __LINE__)(va, (void*)&NW_CAT(NwDtor_, __LINE__)::Run,          \
                                            #Class "::~" #Class, __FILE__);                      \
    }

/*-----------------------------------------------------------------------------
    NW_CHECK_VTABLE(Class, StockVtableVA, Slots)
    Logs our compiled vtable of Class next to the stock one (tools/verify_hooks.py compares
    them slot by slot). Needs the vtable to be emitted here: Class must have its
    constructor or destructor instantiated in this module.
-----------------------------------------------------------------------------*/
#define NW_CHECK_VTABLE(Class, StockVA, Slots)                                                        __pragma(comment(linker, "/alternatename:_nwvt_" #Class "=??_7" #Class "@@6B@"))                 extern "C" const void* const nwvt_##Class[];                                                      namespace { ::nw::VtableReg NW_CAT(nwVt_, __LINE__)(#Class, nwvt_##Class, StockVA, Slots); }
// Same for a class with several vtables (multiple inheritance): Base names the primary one,
// e.g. NW_CHECK_VTABLE_FOR(NL2Font, UObject, ...) checks ??_7NL2Font@@6BUObject@@@.
#define NW_CHECK_VTABLE_FOR(Class, Base, StockVA, Slots)                                               __pragma(comment(linker, "/alternatename:_nwvt_" #Class "=??_7" #Class "@@6B" #Base "@@@"))       extern "C" const void* const nwvt_##Class[];                                                      namespace { ::nw::VtableReg NW_CAT(nwVt_, __LINE__)(#Class, nwvt_##Class, StockVA, Slots); }

/*-----------------------------------------------------------------------------
    NW_STOCK(va): body of a function that is declared but not decompiled yet.
    Jumps into the stock code with every register and the stack untouched
    (eax is free in cdecl/stdcall/thiscall). Use on NW_NAKED definitions:

        NW_NAKED void NCWnd::Invalidate() NW_STOCK(0x10023450)

    Hook mode only: the standalone build must not contain any.
-----------------------------------------------------------------------------*/
#define NW_NAKED __declspec(naked)
#define NW_STOCK(va) \
    { __asm mov eax, g_nwBase __asm add eax, (va) - 0x10000000 __asm jmp eax }
// The same for an address that is already this client's (generated text: tools/port/gen.py maps the plain
// NW_STOCK( of a copied intro through the address map and leaves this one alone).
#define NW_STOCK_AT(va) NW_STOCK(va)

// Stock global variable at va (hook mode: lives in the stock image).
#define NW_GLOBAL(Type, va) (*reinterpret_cast<Type*>(::nw::Addr(va)))

namespace nw {

// Scaffolding for code that touches classes not decompiled yet. Both disappear as the
// owning classes get real declarations; the standalone build has neither.

// thiscall into the stock image (__fastcall with a dummy edx has the thiscall layout).
template <class R, class... A> inline R StockCall(DWORD va, const void* self, A... a)
{
    typedef R(__fastcall * Fn)(const void*, void*, A...);
    return reinterpret_cast<Fn>(Addr(va))(self, 0, a...);
}

// The same for an address that is already this client's: tools/port/gen.py maps the address of a plain
// StockCall<...>( through its address map (the base client's) and leaves this one alone.
template <class R, class... A> inline R StockCallAt(DWORD va, const void* self, A... a)
{
    typedef R(__fastcall * Fn)(const void*, void*, A...);
    return reinterpret_cast<Fn>(Addr(va))(self, 0, a...);
}

// The stock image links its own C runtime statically, with its own FILE table and file locks. A FILE* that
// stock code opened (NL2Font::Load's fopen) is only valid for THAT runtime's fread/fseek/fclose: ours would
// take the lock of a stream it never saw and fault inside ntdll. Code that gets a stock FILE* reads it here.
// (Heap is not a problem: operator new/delete go through GMalloc, src/re/NwMemory.cpp.)
inline size_t StockFread(void* Buffer, size_t Size, size_t Count, void* File)
{
    typedef size_t(__cdecl * Fn)(void*, size_t, size_t, void*);
    return reinterpret_cast<Fn>(Addr(0x1017247A))(Buffer, Size, Count, File);   // _fread (C3 image: 0x1014DAC0)
}

// Field of an object whose class has no declaration here yet.
template <class T> inline T& Field(const void* obj, DWORD offset)
{
    return *reinterpret_cast<T*>(reinterpret_cast<BYTE*>(const_cast<void*>(obj)) + offset);
}

// thiscall through vtable slot `slot` of an object whose class does not declare its virtuals
// yet (NConsoleWnd's 285).
template <class R, class... A> inline R VirtualCall(const void* self, int slot, A... a)
{
    typedef R(__fastcall * Fn)(const void*, void*, A...);
    return (*reinterpret_cast<Fn* const*>(self))[slot](self, 0, a...);
}

}  // namespace nw
