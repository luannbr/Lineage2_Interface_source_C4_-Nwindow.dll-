#pragma once
// NSSQHelpers - the small stock helpers the Seven Signs windows call (all of them anonymous in the db): four texts the
// windows pick by a number (the console is passed in ecx and ignored, so they are __fastcall free functions here) and
// the request the windows send to the server. Names ours. The addresses are the C4 image's (0x1006C630 .. 0x1006C7C0, read
// with `python tools/disasm.py`; the C3 ones were 0x10060D30 .. 0x10060EC0). The four texts are hooked in NSSQNameFns.cpp
// (SSQSealNameC4 ... at the same addresses): the definitions in NSSQHelpers.cpp are stubs that enter the stock code through
// those hooks, for the callers in the Seven Signs windows.

#include "nwindow/NConsoleWnd.h"

// 0x1006C630 (ret 4, 79 B): the name of a seal (1, 2, 3: sysstrings 816 .. 818; GL2GameData + 0x9871C + 12 * N), the "-" line
// (sysstring 27, +0x98860) for any other number
const TCHAR* __fastcall SSQSealName(void* Console, void* Unused, INT Seal);
// 0x1006C680 (ret 4, 61 B): the cabal (1: sysstring 815, 2: 814), the "-" line for any other number
const TCHAR* __fastcall SSQCabalName(void* Console, void* Unused, INT Cabal);
// 0x1006C6C0 (ret 4, 143 B, a jump table at 0x1006C750): the event the quest-like row stands for (1 .. 5: sysstrings 819, 820,
// 821, 844, 822)
const TCHAR* __fastcall SSQEventName(void* Console, void* Unused, INT Event);
// 0x1006C770 (ret 4, 79 B): the text of a seal's state (1, 2, 3: system messages 1178, 1179, 1180; others 27: GL2GameData +
// 0x722C8 + 0x30 * N + 4)
const TCHAR* __fastcall SSQSealStateName(void* Console, void* Unused, INT State);

// 0x1006C7C0 (ret 4, 14 B: `mov eax, [ecx + 0x54]; mov ecx, [eax + 0x64]; jmp [[ecx] + 0x438]`): the console's request for the
// Seven Signs status of a page (2 the event list, 3 the seals, 1 and 4 the status window's two pages): the engine's net handler
// (UL2NetHandler::RequestSSQStatus, vslot 270)
struct NConsoleSSQ : public NConsoleWnd
{
    void RequestSSQStatus(INT Page);
};
