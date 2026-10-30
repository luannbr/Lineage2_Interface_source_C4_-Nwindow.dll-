// hook_selftest.cpp - one hook on the C2 image, to prove the installer on it while the game source
// is empty (cmake -DNWD_HOOK_SELFTEST=ON, tools/cycle.py --hook-selftest; never in the default build).
//
// NCWnd::IsEnabled, stock 0x10001B70 (`mov eax, [ecx+0x5C]; ret`, vslot 26): a thiscall getter with no
// stock call in it. The hook must show up as `ok` in NWindowDecomp.log, and tools/verify_hooks.py must
// find the same `ret` (0 bytes popped), no vtable slot and no import in ours and in the stock one.
#include "re/NwHook.h"

static INT __fastcall SelfTestIsEnabled(void* Self, void*)
{
    return *(INT*)((BYTE*)Self + 0x5C);
}
NW_HOOK_FUNC(0x10001B70, SelfTestIsEnabled)
