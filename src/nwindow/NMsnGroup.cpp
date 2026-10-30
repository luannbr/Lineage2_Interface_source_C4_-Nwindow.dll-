/*=============================================================================
    NMsnGroup.cpp - a contact group of the messenger (C3 only). Stock: GetName 0x1012DC80, GetId 0x1012DCD0, and the
    URL decoder 0x10144300 they (and the contact record's GetDecodedName) share.
=============================================================================*/
#include <windows.h>
#include <wininet.h>
#include "nwindow/NMsnGroup.h"
#include "nwindow/NMsnLink.h"

// wininet is an import of the stock image; the hook DLL does not link it otherwise
#pragma comment(lib, "wininet.lib")

// 0x10168cc0 (cdecl, name ours) - Url decoded into Out (a buffer of 260 characters) with the escapes and nothing else
// touched (ICU_DECODE | ICU_NO_ENCODE); what the import answers comes back
__declspec(noinline) BOOL __cdecl DecodeMsnUrl(const TCHAR* Url, TCHAR* Out)
{
    DWORD Length = 0x104;
    return InternetCanonicalizeUrlW(Url, Out, &Length, ICU_DECODE | ICU_NO_ENCODE);
}
NW_HOOK_FUNC(0x10168cc0, DecodeMsnUrl)

// 0x10152860 (name ours) - the name as the server sent it (Name) decoded into a buffer of 260 characters (cleared first) and kept in the
// second string (EncodedName), which comes back
FString* NMsnGroup::GetName()
{
    TCHAR Decoded[260];
    appMemset(Decoded, 0, 0x208);
    DecodeMsnUrl(*Name, Decoded);
    EncodedName = Decoded;
    return &EncodedName;
}
NW_HOOK_METHOD(0x10152860, NMsnGroup, GetName)

// 0x101528b0 (name ours)
INT NMsnGroup::GetId()
{
    return GroupID;
}
NW_HOOK_METHOD(0x101528b0, NMsnGroup, GetId)

// 0x10151160 (`mov eax, [ecx + 0x10]`, 4 bytes; a getter the linker folded with others': hooked in NCItemWnd.cpp as NItemEntry::ReadDword10, the
// same code) - the array of groups of the store
TArray<NMsnGroup*>* NMsnStore::GetGroups()
{
    return Groups;
}
