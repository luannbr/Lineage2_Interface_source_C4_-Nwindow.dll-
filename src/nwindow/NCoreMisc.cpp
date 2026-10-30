/*=============================================================================
    NCoreMisc.cpp - the small pieces of the engine's headers that the stock NWindow compiled out of line (one copy
    each, called from the windows): appSeconds, appMemcpy, the std algorithm instance that fills with a value, the
    copy and destructor of a system message line, and the constructor of an array of textures. Names are UE2's or the
    library's; the instances are the C2 build's.
=============================================================================*/
#include "nwindow/NConsoleWnd.h"
#include <intrin.h>
#include <string.h>

// 0x1001e860 - UE2's appSeconds (UnVcWin32.h): the time stamp counter in seconds, plus 2^24 "to make bugs apparent where
// the value is passed to a FLOAT". The pawn viewer's OnStart calls this copy; the other users expand it inline (the
// stock's out-of-line copy at 0x10016620 belongs to the Core build's)
static DOUBLE NwAppSeconds()
{
    unsigned __int64 Cycles = __rdtsc();
    DWORD L = (DWORD)Cycles;
    DWORD H = (DWORD)(Cycles >> 32);
    return ((DOUBLE)H * 4294967296.0 + (DOUBLE)L) * GSecondsPerCycle + 16777216.0;
}
NW_HOOK_FUNC(0x1001e860, NwAppSeconds)


// (the uninitialized fill of dwords, 0x10043e60, is ported in NToolTip.cpp)

// 0x100021f0 - the global operator new, out of line: GMalloc->Malloc(Size, "new") (cdecl); the stock code calls it for the
// objects it makes itself (the nProtect library's among them)
static void* __cdecl NwOperatorNew(size_t Size)
{
    return operator new(Size);
}
NW_HOOK_FUNC(0x100021f0, NwOperatorNew)


