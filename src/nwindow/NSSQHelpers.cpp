/*=============================================================================
    NSSQHelpers.cpp - the Seven Signs windows' text pickers and request. C4 stock: 0x1006C630 .. 0x1006C7CD (the C3 image
    had them at 0x10060D30 .. 0x10060ECF).
=============================================================================*/
#include "nwindow/NSSQHelpers.h"
#include "nwindow/NGameData.h"

// sysstring line of "nothing" used by the pickers' default
enum { STR_Dash = 27 };

// The four text pickers are ported in NSSQNameFns.cpp (hooks SSQSealNameC4 / SSQCabalNameC4 / SSQEventNameC4 / SSQSealStateNameC4
// at the addresses below, proved by verify / verify_data / the jump table of 0x1006C750 read against the C4 image). What stays
// here are the declarations the Seven Signs windows call: each enters the stock address, which that hook has taken over.

// The standalone has no hook to enter: these are the same pickers (NSSQNameFns.cpp keeps the hook-mode copies, static there).

// 0x1006c630 (ret 4): the name of a seal (1, 2, 3: sysstrings 816 .. 818), the "-" line for any other number
#if NW_HOOK_MODE
NW_NAKED const TCHAR* __fastcall SSQSealName(void* Console, void* Unused, INT Seal) NW_STOCK(0x1006c630)
#else
const TCHAR* __fastcall SSQSealName(void* Console, void* Unused, INT Seal)
{
    switch (Seal)
    {
    case 1:
        return *GL2GameData.SysString[816];
    case 2:
        return *GL2GameData.SysString[817];
    case 3:
        return *GL2GameData.SysString[818];
    default:
        return *GL2GameData.SysString[STR_Dash];
    }
}
#endif

// 0x1006c680 (ret 4): the cabal (1: sysstring 815, 2: 814), the "-" line for any other number
#if NW_HOOK_MODE
NW_NAKED const TCHAR* __fastcall SSQCabalName(void* Console, void* Unused, INT Cabal) NW_STOCK(0x1006c680)
#else
const TCHAR* __fastcall SSQCabalName(void* Console, void* Unused, INT Cabal)
{
    switch (Cabal)
    {
    case 1:
        return *GL2GameData.SysString[815];
    case 2:
        return *GL2GameData.SysString[814];
    default:
        return *GL2GameData.SysString[STR_Dash];
    }
}
#endif

// 0x1006c6c0 (ret 4): the event the quest-like row stands for (1 .. 5: sysstrings 819, 820, 821, 844, 822)
#if NW_HOOK_MODE
NW_NAKED const TCHAR* __fastcall SSQEventName(void* Console, void* Unused, INT Event) NW_STOCK(0x1006c6c0)
#else
const TCHAR* __fastcall SSQEventName(void* Console, void* Unused, INT Event)
{
    switch (Event)
    {
    case 1:
        return *GL2GameData.SysString[819];
    case 2:
        return *GL2GameData.SysString[820];
    case 3:
        return *GL2GameData.SysString[821];
    case 4:
        return *GL2GameData.SysString[844];
    case 5:
        return *GL2GameData.SysString[822];
    default:
        return *GL2GameData.SysString[STR_Dash];
    }
}
#endif

// 0x1006c770 (ret 4): the text of a seal's state (1, 2, 3: system messages 1178, 1179, 1180; others 27)
#if NW_HOOK_MODE
NW_NAKED const TCHAR* __fastcall SSQSealStateName(void* Console, void* Unused, INT State) NW_STOCK(0x1006c770)
#else
const TCHAR* __fastcall SSQSealStateName(void* Console, void* Unused, INT State)
{
    switch (State)
    {
    case 1:
        return *GL2GameData.SystemMsg[1178].Message;
    case 2:
        return *GL2GameData.SystemMsg[1179].Message;
    case 3:
        return *GL2GameData.SystemMsg[1180].Message;
    default:
        return *GL2GameData.SystemMsg[27].Message;
    }
}
#endif

// 0x1006c7c0
void NConsoleSSQ::RequestSSQStatus(INT Page)
{
    Engine->NetHandler->RequestSSQStatus(Page);
}
NW_HOOK_METHOD(0x1006c7c0, NConsoleSSQ, RequestSSQStatus)
