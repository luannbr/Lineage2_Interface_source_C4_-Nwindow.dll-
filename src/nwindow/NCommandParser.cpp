/*=============================================================================
    NCommandParser.cpp - chat command recognition. Stock: 0x100338e0 .. 0x1003397c.
=============================================================================*/
#include "nwindow/NCommandParser.h"
#include "nwindow/NConsoleWnd.h"
#include "nwindow/NStl.h"

#include <wchar.h>

#if NW_FEATURES
// phase 6: ManufactureCmd (NWindow_C1 src/inject/ManufactureCmd.cpp)
#include "features/ManufactureCmd.h"
#endif

// 0x1004b8e0 (cdecl; name ours) - NCActionLinker's message: "msgid;argument" becomes the text of system message msgid
// with its first "$s1" replaced by the argument (what follows the last ';'); no ';' leaves the argument empty
// (the stock reads an uninitialised buffer there). The result is written over the code.
void ExpandSystemMsgCode(TCHAR* Message)
{
    INT Len = appStrlen(Message);
    INT MsgID = appAtoi(Message);
    if (MsgID <= 0)
        return;
    TCHAR Text[256];
    appStrcpy(Text, *GL2GameData.SystemMsg[MsgID].Message);
    if (Len <= 0)
        return;
    TCHAR Arg[130];   // the stock frame has 4 spare bytes under its std::wstring; the frame check wants ours as big (+ cookie)
    Arg[0] = 0;
    INT Semi = Len - 2;
    while (Semi >= 0 && Message[Semi] != ';')
        Semi--;
    if (Semi >= 0)
    {
        appStrncpy(Arg, Message + Semi + 1, Len - Semi);
        Arg[Len - Semi - 1] = 0;
    }
    NStlWString Wide;
    Wide.Assign(Text, wcslen(Text));
    INT Pos = (INT)Wide.Find(TEXT("$s1"), 0, wcslen(TEXT("$s1")));
    if (Pos >= 0 && Pos < appStrlen(Text))
    {
        Wide.Replace(Pos, 3, Arg, wcslen(Arg));
        appStrncpy(Message, Wide.CStr(), appStrlen(Text) + appStrlen(Arg) - 2);
    }
    else
        appStrcpy(Message, Text);
}
NW_HOOK_FUNC(0x1004b8e0, ExpandSystemMsgCode)

// 0x1004bb70
NCommandParser::NCommandParser()
{
    SetSperator(GCommandSeparators);
}
NW_HOOK_CTOR(0x1004bb70, NCommandParser)

// 0x1004bbd0 - a line starting with '/' (or the full-width slash U+FF0F; C1 compared with the Korean code
// page's 0xA3AF) is a command, looked up by its first token; anything else is chat
INT NCommandParser::GetCommand(TCHAR* Line)
{
    if (!Line)
        return NCMD_None;
    if (Line[0] != *GCommandPrefix[0] && Line[0] != *GCommandPrefix[1])
        return NCMD_Chat;
#if NW_FEATURES
    // phase 6: ManufactureCmd (NWindow_C1 src/inject/ManufactureCmd.cpp): /dwarvenmanufacture,
    // which C1's command table lacks, sends RequestActionUse(37). SetString starts the parser
    // over, so the stock line below gets the same first token again.
    if (nwf::ManufactureCmd_OnCommand(SetString(Line)))
        return NCMD_None;
#endif
    return GL2GameData.GetCommandType(SetString(Line));
}
NW_HOOK_METHOD(0x1004bbd0, NCommandParser, GetCommand)

// 0x1004bc30 (C2 only; name ours) - a token that is one of the table's words (compared without case) is replaced by
// the name it stands for: %target (the selected actor), %self (the player), %pet, %party1..8. 0 when the token is
// absent, is not one of them or there is nobody (nothing is copied then, except by the party words, which copy first
// and ask afterwards: the stock does that, whatever the first call returned). The stock copies the pet's record (45
// words in C4) out of the temporary the console returns; the name is read from the copy
INT NCommandParser::ExpandToken(TCHAR* Token)
{
    if (!Token)
        return 0;
    INT Which = 0;
    do
    {
        if (appStricmp(GCommandTokenNames[Which], Token) == 0)
            break;
        Which++;
    } while (Which < NCMD_TOKEN_COUNT);
    if (Which > 10)
        return 0;
    const TCHAR* Name;
    switch (Which)
    {
    case 0:
    {
        User* Target = GConsole->GetSelectedActor();
        if (!Target)
            return 0;
        Name = (const TCHAR*)((BYTE*)Target + 0x1C);        // User::GetName, inlined
        break;
    }
    case 1:
    {
        User* Me = GConsole->GetMyUser();
        if (!Me)
            return 0;
        Name = (const TCHAR*)((BYTE*)Me + 0x1C);
        break;
    }
    case 2:
    {
        // the stock's frame holds the console's temporary and the copy the name is read from, with 8 bytes between
        // them (tools/port/check_frames.py counts the frame: the pad keeps ours from being the smaller one)
        volatile INT FramePad[3];
        FramePad[0] = 0;
        PetInfo Returned = GConsole->GetPetInfo();
        PetInfo Pet;
        appMemcpy(&Pet, &Returned, sizeof(PetInfo));
        if (appStrlen(Pet.Name) == 0)
            return 0;
        appStrcpy(Token, Pet.Name);
        return 1;
    }
    default:
    {
        INT Member = Which - 3;
        appStrcpy(Token, GConsole->GetPartyMemberName(Member));
        return GConsole->GetPartyMemberName(Member) != NULL;
    }
    }
    appStrcpy(Token, Name);
    return 1;
}
NW_HOOK_METHOD(0x1004bc30, NCommandParser, ExpandToken)
