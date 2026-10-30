/*=============================================================================
    Util.cpp - the messenger's static helper class `Util` (stock 0x101685c0 .. 0x10168dd0; Util::IsLetter and Util::MsnStrcmp are
    hooked in NCoreHelpers.cpp, where the stub of this function's address sits too: it jumps into the hook).
=============================================================================*/
#include "nwindow/NWindow.h"

// 0x101685f0 (cdecl, the result by a hidden pointer, returned) - the language id the messenger sends to the servers ("0x0409"):
// the user's language (the low word of GetUserDefaultLCID) when it is one of the messenger's languages, else 0x0409 (English
// US); Chinese (Hong Kong, 0xC04) counts as Taiwan (0x0404). C4 list (the evidence: the stock's compare tree and its byte table at
// 0x10168708): 0x401, 0x404 .. 0x409, 0x40B .. 0x416, 0x419, 0x41B, 0x41D, 0x41F, 0x424, 0x804, 0x816 and 0xC0A (Chinese 0x404 for
// 0xC04)
static WORD UtilMessengerLanguage(WORD Lang)
{
    switch (Lang)
    {
    case 0x401:
    case 0x404:
    case 0x405:
    case 0x406:
    case 0x407:
    case 0x408:
    case 0x409:
    case 0x40B:
    case 0x40C:
    case 0x40D:
    case 0x40E:
    case 0x410:
    case 0x411:
    case 0x412:
    case 0x413:
    case 0x414:
    case 0x415:
    case 0x416:
    case 0x419:
    case 0x41B:
    case 0x41D:
    case 0x41F:
    case 0x424:
    case 0x804:
    case 0x816:
    case 0xC0A:
        return Lang;
    case 0xC04:
        return 0x404;
    }
    return 0x409;
}

FString* __cdecl UtilGetLanguageID(FString* Result)
{
    guard(Util::GetLanguageID);
    WORD Lang = UtilMessengerLanguage((WORD)GetUserDefaultLCID());
    FString Text = FString::Printf(TEXT("0x%04x"), (DWORD)Lang);
    new (Result) FString(Text);
    return Result;
    unguard;
}
NW_HOOK_FUNC(0x101685f0, UtilGetLanguageID)
