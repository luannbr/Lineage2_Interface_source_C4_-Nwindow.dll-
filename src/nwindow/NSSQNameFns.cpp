/*=============================================================================
    NSSQNameFns.cpp - the Seven Signs text pickers of NSSQHelpers.cpp (C4 stock: 0x1006c630 .. 0x1006c7bc), ported here because
    NSSQHelpers.cpp is in no lane of stage 4 (the stubs stay there: the callers' calls reach these hooks through the stock
    addresses). `this` (the console, ecx) is not read: __fastcall free functions with a dummy edx, one argument, ret 4.
=============================================================================*/
#include "nwindow/NWindow.h"
#include "nwindow/NGameData.h"

// sysstring line of "nothing" used by the pickers' default
enum { STR_DashLine = 27 };

// 0x1006c630 (ret 4): the name of a seal (1, 2, 3: sysstrings 816 .. 818), the "-" line for any other number
static const TCHAR* __fastcall SSQSealNameC4(void* Console, void* Unused, INT Seal)
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
        return *GL2GameData.SysString[STR_DashLine];
    }
}
NW_HOOK_FUNC(0x1006c630, SSQSealNameC4)

// 0x1006c680 (ret 4): the cabal (1: sysstring 815, 2: 814), the "-" line for any other number
static const TCHAR* __fastcall SSQCabalNameC4(void* Console, void* Unused, INT Cabal)
{
    switch (Cabal)
    {
    case 1:
        return *GL2GameData.SysString[815];
    case 2:
        return *GL2GameData.SysString[814];
    default:
        return *GL2GameData.SysString[STR_DashLine];
    }
}
NW_HOOK_FUNC(0x1006c680, SSQCabalNameC4)

// 0x1006c6c0 (ret 4): the event the quest-like row stands for (1 .. 5: sysstrings 819, 820, 821, 844, 822)
static const TCHAR* __fastcall SSQEventNameC4(void* Console, void* Unused, INT Event)
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
        return *GL2GameData.SysString[STR_DashLine];
    }
}
NW_HOOK_FUNC(0x1006c6c0, SSQEventNameC4)

// 0x1006c770 (ret 4): the text of a seal's state (1, 2, 3: system messages 1178, 1179, 1180; others 27)
static const TCHAR* __fastcall SSQSealStateNameC4(void* Console, void* Unused, INT State)
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
NW_HOOK_FUNC(0x1006c770, SSQSealStateNameC4)
