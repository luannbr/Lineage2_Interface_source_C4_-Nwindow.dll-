#pragma once
// NMsnBuddy - the 0x44-byte contact record of the messenger (name ours, there is no UClass): a number, a
// second one that travels with it (the group the contact is in, going by MoveGroupDialog), and five texts (the account,
// the shown name, ...). RemoveFriendDialog and MoveGroupDialog embed one, AddedConfirmDialog keeps pointers to them.
//
// It is the dialogs' view of the protocol layer's NMsnFriend (NMsnProtocol.h: the same record, the same layout; that header cannot be
// included next to the dialogs' NMsnGroup.h). Stock code (0x1013E9A0 constructor, 0x10150F20 copy constructor, 0x10150FF0 destructor, in
// NChatTypes.cpp / NMsnProtocol.cpp): the five FStrings at +0x08, +0x14, +0x20, +0x2C and +0x38 are made in that order and destroyed in
// the reverse one; the copy constructor makes them empty, copies +4 and assigns the five texts (+0 is not touched by any of them). The
// three are functions of their own in the stock (every user calls them out of line), so they are out of line here (NMsnBuddy.cpp).

#include "nwindow/NWindow.h"
#pragma pack(push, 4)

// 0x10168CC0 (cdecl, NMsnGroup.cpp): the text URL-decoded into Out (a buffer of 260 characters)
BOOL __cdecl DecodeMsnUrl(const TCHAR* Url, TCHAR* Out);

struct NMsnBuddy
{
    INT Word00;       // +0x00 number (never set by the constructors)
    INT Word04;       // +0x04 the second number
    FString Str08;    // +0x08
    FString Str14;    // +0x14
    FString Str20;    // +0x20
    FString Str2C;    // +0x2C
    FString Str38;    // +0x38

    __declspec(noinline) NMsnBuddy();
    __declspec(noinline) NMsnBuddy(const NMsnBuddy& Other);
    __declspec(noinline) ~NMsnBuddy();

    INT& Number() { return Word00; }
    INT& Param() { return Word04; }                               // the group the contact is in (MoveGroupDialog)
    FString& Text08() { return Str08; }
    FString& Text14() { return Str14; }
    FString& Text20() { return Str20; }                           // the account (AddedConfirmDialog's $s2)
    FString& Text2C() { return Str2C; }                           // the shown name as the server sent it (URL-encoded)
    FString& Text38() { return Str38; }                           // GetDecodedName's result

    // 0x101510D0 (ret 4; the msn1 lane's NMsnFriend::GetDisplayName): +0x2C URL-decoded into +0x38, and a copy of that built in Out
    // (the hidden result pointer of an FString return, which the stock callers point at the argument they are making)
    FString* GetDecodedName(FString* Out);
};
NW_ASSERT_OFFSET(NMsnBuddy, Str08, 0x08);
NW_ASSERT_OFFSET(NMsnBuddy, Str20, 0x20);
NW_ASSERT_OFFSET(NMsnBuddy, Str38, 0x38);
NW_ASSERT_SIZE(NMsnBuddy, 0x44);

#pragma pack(pop)
