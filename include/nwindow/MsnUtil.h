#pragma once
// The messenger's small helpers (C3 only; names ours): the client description the protocol sends (the platform, the system
// version and the processor), the character set conversions of the protocol's text, and the address canonicalizer. Free
// functions of the stock image (cdecl; the ones that give a string fill the FString they are given and return it).

#include "nwindow/NCWnd.h"

FString* MsnPlatformName(FString* Out);                                      // 0x10143D70 "win32", "win" or "winnt"
FString* MsnSystemVersion(FString* Out);                                     // 0x10143DF0 "major.minor" of the system
FString* MsnProcessorName(FString* Out);                                     // 0x10143E50 "i386", "mips", "alpha" or "ppc"
INT MsnWideToAnsi(const TCHAR* Wide, char* Out);                             // 0x10144220 into at most 0x1040 bytes (the ANSI code page)
INT MsnAnsiToWide(const char* Ansi, TCHAR* Out);                             // 0x10144240 into at most 0x1040 characters
INT MsnUrlCanonicalize(const TCHAR* Url, TCHAR* Out);                        // 0x101442D0 shlwapi's UrlCanonicalizeW into 0x104 characters
void MsnDecipherProduct(TCHAR* ProductID, TCHAR* ProductKey);                  // 0x10169010 (name ours) the product id and key deciphered (Blowfish) into two buffers of 0x1040 characters
INT __cdecl NUtf8Encode(const WCHAR* Src, char* Dest);                       // 0x10168C60 (NChatMsg.cpp) the characters as UTF-8 (code page 65001, the terminator included) into 0x1040 bytes
FString* __cdecl UtilGetLanguageID(FString* Result);                         // 0x101685F0 (Util.cpp) the language of the system as "0x0409", built in the string Result points at
void __stdcall MsnShiftText(TCHAR* Text, INT Count);                         // 0x10165E60 (SwitchboardSession.cpp) the first Count characters taken off the front of the text
