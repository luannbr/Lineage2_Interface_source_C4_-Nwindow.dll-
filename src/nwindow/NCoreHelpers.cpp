/*=============================================================================
    NCoreHelpers.cpp - small anonymous stock functions that belong to no class file (wave 4, lane core): the
    helpers the console's messenger / broadcast / replay code calls and the records the GM tools build. Names ours.
=============================================================================*/
#include "nwindow/NWindow.h"
#include "nwindow/NControl.h"
#include "nwindow/MsnUiStock.h"   // NCoreSkinControl (declared once there, stage 5 sa-verify)

/*-----------------------------------------------------------------------------
    The messenger windows' small control (a picture, a text and a flag after NControl's fields; the class is the
    messenger lanes').
-----------------------------------------------------------------------------*/

// The control of the messenger's and the broadcast GM window's title bars (MessengerWnd::OnPaint / MinimizeWindow,
// NCBroadcastGMWnd::MinimizeWindow): its fields from +0x1D4 (the picture; the text at +0x1E0; the flag at +0x1F4). The
// function is its setter; a stand-in class (NCoreSkinControl, MsnUiStock.h) so that it is hooked as a method (thiscall)

// 0x1001db90 - the picture found by its name (when given), the text kept (when given) and the flag stored. C4: the fields
// moved by 0x20 (the picture +0x1F4, the text +0x200, the flag +0x214; evidence: the stores of this function) and FindTexture
// takes a second argument (1)
void NCoreSkinControl::SetSkin(const TCHAR* TexName, const TCHAR* Text, ANSICHAR Flag)
{
    if (TexName)
        nw::Field<UTexture*>(this, 0x1F4) = FindTexture(TexName, 1);
    if (Text)
        nw::Field<FString>(this, 0x200) = Text;
    nw::Field<INT>(this, 0x214) = (BYTE)Flag;
}
NW_HOOK_METHOD(0x1001db90, NCoreSkinControl, SetSkin)

/*-----------------------------------------------------------------------------
    Util: the messenger's static helpers.
-----------------------------------------------------------------------------*/

// (Util::GetLanguageID, 0x101685f0, is ported in Util.cpp)

// 0x101685c0 (cdecl) - whether the character is a Latin letter (A-Z or a-z)
static __declspec(noinline) INT __cdecl NUtilIsLetter(TCHAR C)
{
    return (C >= 'A' && C <= 'Z') || (C >= 'a' && C <= 'z');
}
NW_HOOK_FUNC(0x101685c0, NUtilIsLetter)

// 0x10168dd0 (cdecl; both strings by value, the function destroys them) - Util::MsnStrcmp: the localized texts compared
// character by character; a character that is no letter comes before a letter, otherwise the difference of the characters
// decides; when one text is a prefix of the other the longer is greater
INT __cdecl NUtilMsnStrcmp(FString A, FString B)      // (sa-msn-ui: not static, MsnTree.cpp calls it; declared in MsnUiStock.h)
{
    FString X = A.Locs();
    FString Y = B.Locs();
    INT LenX = X.Len();
    INT LenY = Y.Len();
    INT Count = LenX > LenY ? LenY : LenX;
    for (INT i = 0; i < Count; i++)
    {
        INT LetterX = NUtilIsLetter(X.GetCharData()[i]);
        INT LetterY = NUtilIsLetter(Y.GetCharData()[i]);
        if (!LetterX)
        {
            if (LetterY)
                return -1;
        }
        else if (!LetterY)
            return 1;
        INT Difference = X.GetCharData()[i] - Y.GetCharData()[i];
        if (Difference)
            return Difference;
    }
    if (LenX > LenY)
        return 1;
    if (LenX < LenY)
        return -1;
    return 0;
}
NW_HOOK_FUNC(0x10168dd0, NUtilMsnStrcmp)

// 0x101692e0 (cdecl; the names by value) - Util::MsnCompareFriend: a friend who is on (the number after the name is not 0)
// comes before one who is not; otherwise the names decide (MsnStrcmp)
INT __cdecl NUtilMsnCompareFriend(FString NameA, INT OnlineA, FString NameB, INT OnlineB)      // (sa-msn-ui: not static, FriendList.cpp and MsnTree.cpp call it; declared in MsnUiStock.h)
{
    if (OnlineA)
    {
        if (!OnlineB)
            return -1;
    }
    else if (OnlineB)
        return 1;
    return NUtilMsnStrcmp(NameA, NameB);
}
NW_HOOK_FUNC(0x101692e0, NUtilMsnCompareFriend)

/*-----------------------------------------------------------------------------
    NSpawnTestParam.
-----------------------------------------------------------------------------*/

// The record the NPC viewer's spawn test builds on its stack (NCNpcView_CharacterSelectWnd::OnSpawnTest, a GM tool):
// 0x30 bytes, the fields at 0 except the scale, the id and the -1. The only user is the stock OnSpawnTest.
struct NSpawnTestParam
{
    INT Unknown00;                   // +0x00 (the second argument)
    INT Zero[8];                     // +0x04 .. +0x20 (0)
    FLOAT Scale;                     // +0x24 (the first argument; the caller passes 1.0)
    INT Unknown28;                   // +0x28 (-1)
    INT Unknown2C;                   // +0x2C (0)

    NSpawnTestParam(FLOAT InScale, INT InUnknown00);
};
NW_ASSERT_SIZE(NSpawnTestParam, 0x30);

// 0x100bf490 (ret 8)
NSpawnTestParam::NSpawnTestParam(FLOAT InScale, INT InUnknown00)
{
    Unknown00 = InUnknown00;
    Zero[0] = 0;
    Zero[1] = 0;
    Zero[2] = 0;
    Zero[3] = 0;
    Zero[4] = 0;
    Zero[5] = 0;
    Zero[6] = 0;
    Zero[7] = 0;
    Scale = InScale;
    Unknown28 = -1;
    Unknown2C = 0;
}
NW_HOOK_CTOR_ARGS(0x100bf490, NSpawnTestParam, (FLOAT A, INT B), (A, B))
