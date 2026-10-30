/*=============================================================================
    NwGameGuard.cpp - the stock globals of the nProtect GameGuard glue, for the standalone build.

    The statically linked nProtect library is not part of the standalone NWindow.dll (include/nwindow/NGameGuard.h);
    these objects belong to the game's side of the glue, which stays. Hook mode reaches them in the stock image
    (NW_GLOBAL, NConsoleWndWindows.cpp / NConsoleWndInput.cpp / NConsoleWndC2.cpp); here they are ours, with the stock
    initial values. They are not in NwGlobals.cpp (tools/gen_globals.py skips them: OTHER_OWNER).
=============================================================================*/
#include "nwindow/NWindow.h"

#if NW_HOOK_MODE
#error "NwGameGuard.cpp is for the standalone build only"
#endif

class NConsoleWnd;

// 0x102D2C0C .bss (name ours); NConsoleWndInput.cpp, NConsoleWndC2.cpp - the stock keeps here the pointer to the game's
// GameMon object, made by a static initializer (CreateGameMonObject 0x1006EB60, 0x10191F30) and destroyed at exit
// (0x1019B7B0); non-zero while the GameGuard runs, and every GameGuard path of the game tests it (the console's tick,
// the server's query GameGuardQueryX, InitGameMon_part). The standalone has no such object: it stays 0, so those paths
// are skipped (NGameGuard.h)
INT GGameGuardRunning = 0;

// 0x102D2C10 .bss (name ours); NConsoleWndWindows.cpp - the console, set at the end of Initialize. The stock reads it
// in FormatGameMonMessage 0x1006E560 (the text of a GameMon message, only called by the library); the standalone has no reader
NConsoleWnd* GGameGuardConsole = NULL;

// 0x102D2C20 .bss (name ours); NConsoleWndInput.cpp - the seconds since the game guard's last check: Tick adds the
// frame time to it (only while GGameGuardRunning is set, so never here), and the check it would trigger past 10 seconds
// never runs in the patched client (the packager's patch jumps over it)
FLOAT GGameGuardTimer = 0.0f;

// 0x101DE1E8 .rdata (name ours); NConsoleWndInput.cpp - the library's answer to a good start and to a good check
// (InitNPGameMon / CheckNPGameMon compare with it): 0x755, read-only data of the stock image; NGameGuard::InitSuccess
INT GGameGuardOK = 0x755;
