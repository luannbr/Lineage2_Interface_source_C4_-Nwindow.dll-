#pragma once
// NGameGuard - the game's calls into nProtect GameGuard's client library (NPGameLib, linked statically into the stock
// image at 0x10169570 .. 0x1016EA00: the NPGameLibApi / NPGameLib / NPGSys functions of a third party, no UI; the entry
// points the game calls are the NPGameLibApi ones, and the console's InitGameMon_part 0x1006BD60 wraps the start). Name ours.
//
// Hook mode: the library is still the stock code and is called in place (it is the only thing of it that hook mode still
// runs). Standalone: the library is NOT linked (docs/notas/C4_Standalone.md section 0; the C2 decision, docs/notas/C2_Fase5.md,
// "nProtect e NKeyCrypt no standalone"), the entry points do nothing, as the library itself while it has not been started
// (its object pointer 0x10326E20 is 0: InitNPGameMon, CheckNPGameMon and the query return 0 at once, SetHwnd returns).
// The static initializer that makes the game's GameMon object (CreateGameMonObject 0x1006EB60, reached through PreInitWrapper
// 0x10064AB0 and the library's PreInit; the pointer it stores is the variable at 0x102D2C0C, GGameGuardRunning) has no
// counterpart either, so that variable stays 0 and every GameGuard path of the game (the console's tick, the server's query,
// InitGameMon_part) is skipped by its own test; the wrappers below are the documented no-ops for the places that would call
// the library past that test. FormatGameMonMessage 0x1006E560 (the text of a GameMon message code) is only called by the
// library, so nothing of the game reaches it. The client the project reconstructs (the binary-patched one) still answers the
// login server's query: its SendAuthQuery (0x1016ba60, a `je` patched to nops) hands it to GameMon, whose answer comes back
// through the stock message handler (0x1006e560) and goes out as AuthGameGuard (NetHandler ResponseAuthGameGuard). The C4 login
// needs that packet after the Init, so the standalone answers the query itself, with zeros (NConsoleWnd::GameGuardQueryX,
// src/nwindow/NConsoleWndC2.cpp).
//
// The calls are cdecl, except StartGameMon (thiscall, like the stock InitGameMon_part). (The names are unique on purpose: check_literals
// follows a call by its simple name, and a plain `Start` took this header's literal into an unrelated hooked function.)

#include "nwindow/NWindow.h"

namespace NGameGuard
{

// the library's answer to a good start (the stock keeps it at 0x101DE1E8, GGameGuardOK, defined in
// src/standalone/NwGameGuard.cpp): the standalone Init and Check answer it, so that a caller that compares the answer goes on
// and reports no error
enum { InitSuccess = 0x755 };

#if NW_HOOK_MODE

// NPGameLibApi::InitNPGameMon 0x10169570 - the start-up answer (0x755 = success)
FORCEINLINE INT Init()
{
    typedef INT(__cdecl * Fn)();
    return ((Fn)nw::Addr(0x10169570))();
}

// NPGameLibApi::SetHwndToGameMon 0x10169590 - the game window, after a good start
FORCEINLINE void SetHwnd(INT hWnd)
{
    typedef void(__cdecl * Fn)(INT);
    ((Fn)nw::Addr(0x10169590))(hWnd);
}

// NPGameLibApi::SendAuthQuery 0x10169780 - the server's authentication query (four dwords) to GameMon
FORCEINLINE INT SendAuthQuery(INT* Query)
{
    typedef INT(__cdecl * Fn)(INT*);
    return ((Fn)nw::Addr(0x10169780))(Query);
}

// NPGameLibApi::CheckNPGameMon 0x10169760 - the periodic check of the GameMon process (0x755 = all well). The stock
// caller gives it ecx = 0, which the function does not read (it loads the library's object pointer itself)
FORCEINLINE INT Check()
{
    return ::nw::StockCallAt<INT>(0x10169760, NULL);
}

// NConsoleWnd::InitGameMon_part 0x1006BD60 (thiscall, ret 4) - the game's side of the start-up, called by Initialize with the
// network login: with the GameMon object made, InitNPGameMon and then SetHwndToGameMon with the game window
FORCEINLINE void StartGameMon(void* Console, INT hWnd)
{
    ::nw::StockCallAt<void>(0x1006BD60, Console, hWnd);
}

#else

// no library: documented no-ops (see above)
FORCEINLINE INT Init() { return InitSuccess; }
FORCEINLINE void SetHwnd(INT) {}
FORCEINLINE INT SendAuthQuery(INT*) { return 0; }
FORCEINLINE INT Check() { return InitSuccess; }
FORCEINLINE void StartGameMon(void*, INT) {}

#endif

}  // namespace NGameGuard
