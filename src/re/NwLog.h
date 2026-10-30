#pragma once
// NWindowDecomp.log / NWindowDecomp.ini, both next to NWindowDecomp.dll.
// Plain Win32 only: used from DllMain, before the engine is usable.

#include <windows.h>

namespace nw {

void LogOpen(HMODULE self);
void Log(const char* fmt, ...);

struct Config
{
    bool hooksEnabled = true;  // [Hooks] Enabled=0 keeps the whole stock DLL
    char disable[16384] = {};   // [Hooks] Disable= list of stock addresses / Class::Method / Class::*
    bool debugMarker = false;  // [Debug] Marker=1: proof marks drawn by our code (re/NwDebug.h)
    bool traceHooks = true;    // [Debug] Trace=0: no entry trace; 1 (default): every hook enters through a stub that
                               // notes it in a ring of the last 256 calls, written to the log by the crash loggers
    char logCalls[8192] = {};  // [Debug] LogCalls= patterns (stock address, Class::Method, Class::*, or a prefix ending
                               // in *): every call of the matching functions, hooked or not, is written to the log
                               // as `[call] va name this= ret= args=` (names: NWindowDecomp.names beside the dll)
    DWORD logMax = 300;        // [Debug] LogMax=: calls logged per function (a paint handler would flood the log)
    // [Debug] Watch*: a hardware write watchpoint (DR0..DR3) on the first dword of an object, to find who corrupts it.
    //   WatchFn=10036ce0   stock VA of a function listed in LogCalls (its `this` is the object to watch)
    //   WatchRet=1012b05f  only when the call comes from these stock return addresses (empty: every call)
    //   WatchMax=40        writes logged in all
    // Every write to the dword is logged as `[watch] ...` with the writer's address and the code addresses on its stack.
    DWORD watchFn = 0;
    DWORD watchRet[8] = {};
    int watchRetCount = 0;
    DWORD watchMax = 40;
    bool logFlush = true;      // [Log] Flush=0: no FlushFileBuffers per line (the lines still
                               // reach the file if the game crashes; only a crash of Windows
                               // could lose them). tools/difftest.py, where a flush per line
                               // costs up to 8 ms when the disk is busy
};

const Config& GetConfig();
void LoadConfig(HMODULE self);
bool IsDisabledByConfig(DWORD va, const char* name);
// [Debug] Watch*: called by the call logger on every logged call (NwHook.cpp); arms a watchpoint when it matches
void WatchMaybeArm(DWORD fnVa, DWORD self, DWORD retStockVa);

}  // namespace nw
