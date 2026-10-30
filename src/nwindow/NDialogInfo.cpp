/*=============================================================================
    NDialogInfo.cpp - dialog box parameters. Stock: 0x10006a30 .. 0x10006c36 (the setters),
    0x10006e20 (the destructor) and 0x100075e0 (the constructor).
=============================================================================*/
#include "nwindow/NDialogInfo.h"

// 0x100085c0 - the eight strings empty in the order of the members, the defaults (ID -1, Type 3, main box) and the
// numbers cleared; Location is left as it is
NDialogInfo::NDialogInfo()
{
    Owner = NULL;
    bInput = 0;
    Flag18 = 0;
    Param1 = 0;
    Param2 = 0;
    Unknown1624 = 0;
    Unknown1628 = 0;
    bProgress = 0;
    Unknown74 = 0;
    bLargeAmount = 0;
    ID = -1;
    Type = 3;
    bMainBox = 1;
}
NW_HOOK_CTOR(0x100085c0, NDialogInfo)

// 0x10007d50 - the strings go in the reverse order of the members
NDialogInfo::~NDialogInfo()
{
}
NW_HOOK_DTOR(0x10007d50, NDialogInfo)

// 0x10007ad0 - NULL empties it (C1 emptied for "" too)
void NDialogInfo::SetText(const TCHAR* InText)
{
    if (InText)
        Text = InText;
    else
        Text.Empty();
}
NW_HOOK_METHOD(0x10007ad0, NDialogInfo, SetText)

// 0x100079b0 (ret 4) - the record's two other strings come along (the sounds the box plays: Message2, and the third text
// of the C4 record, Message3)
void NDialogInfo::SetSystemMessage(INT MsgID)
{
    if (*GL2GameData.SystemMsg[MsgID].Message)
    {
        Text = *GL2GameData.SystemMsg[MsgID].Message;
        Text88 = FString(*GL2GameData.SystemMsg[MsgID].Message2);
        Text94 = FString(*GL2GameData.SystemMsg[MsgID].Message3);
    }
    else
        Text.Empty();
}
NW_HOOK_METHOD(0x100079b0, NDialogInfo, SetSystemMessage)

// 0x10007960 (ret 8) - empty or NULL empties it
void NDialogInfo::SetButtons(const TCHAR* InButton1, const TCHAR* InButton2)
{
    if (InButton1 && *InButton1)
        Button1 = InButton1;
    else
        Button1.Empty();
    if (InButton2 && *InButton2)
        Button2 = InButton2;
    else
        Button2.Empty();
}
NW_HOOK_METHOD(0x10007960, NDialogInfo, SetButtons)

// 0x10007af0 (ret 4)
void NDialogInfo::SetParamText(const TCHAR* InText)
{
    if (InText && *InText)
        ParamText = InText;
    else
        ParamText.Empty();
}
NW_HOOK_METHOD(0x10007af0, NDialogInfo, SetParamText)

// 0x10007b20 (ret 8)
void NDialogInfo::SetParams(INT InParam1, INT InParam2)
{
    Param1 = InParam1;
    Param2 = InParam2;
}
NW_HOOK_METHOD(0x10007b20, NDialogInfo, SetParams)

// 0x10007b40 (ret 4)
void NDialogInfo::SetText60(const TCHAR* InText)
{
    if (InText && *InText)
        Text60 = InText;
    else
        Text60.Empty();
}
NW_HOOK_METHOD(0x10007b40, NDialogInfo, SetText60)
