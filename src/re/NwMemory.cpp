// Global operator new/delete of NWindowDecomp.dll go through the engine allocator, like
// the stock module (its operator new calls GMalloc->Malloc(Size, L"new")): objects are
// created in one module and freed in another.
#include <windows.h>
#include <stddef.h>

// Core.dll's allocator (GMalloc, ?GMalloc@@3PAVFMalloc@@A). Only the first three virtuals - Malloc,
// Realloc, Free: the same slots in the C1 and C2 Core - are used here, so they are declared
// locally: this file is part of the hook runtime (src/re) and must not depend on include/ue2
// (whose declarations are rewritten for C2). It has to stay a `class` named FMalloc: that is what
// the decorated import name says. With /Zc:wchar_t- the TCHAR below is `unsigned short`.
class FMalloc
{
public:
    virtual void* Malloc(DWORD Count, const wchar_t* Tag) = 0;
    virtual void* Realloc(void* Original, DWORD Count, const wchar_t* Tag) = 0;
    virtual void Free(void* Original) = 0;
};
extern __declspec(dllimport) FMalloc* GMalloc;

static inline void* appMalloc(DWORD Count, const wchar_t* Tag) { return GMalloc->Malloc(Count, Tag); }
static inline void appFree(void* Original) { GMalloc->Free(Original); }

void* operator new(size_t Size) { return appMalloc((DWORD)Size, L"new"); }
void* operator new[](size_t Size) { return appMalloc((DWORD)Size, L"new"); }

void operator delete(void* Ptr) noexcept
{
    if (Ptr)
        appFree(Ptr);
}
void operator delete[](void* Ptr) noexcept
{
    if (Ptr)
        appFree(Ptr);
}
void operator delete(void* Ptr, size_t) noexcept
{
    if (Ptr)
        appFree(Ptr);
}
void operator delete[](void* Ptr, size_t) noexcept
{
    if (Ptr)
        appFree(Ptr);
}
